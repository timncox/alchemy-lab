#!/usr/bin/env python3
"""HostLink over the emulator's --hostlink socket, as the iPhone app speaks it.

    python3 tools/hostlink_client.py --port 5071            HELLO, PHONE_INFO, STATE
    python3 tools/hostlink_client.py --port 5071 --check    exit 1 unless all answer OK
    python3 tools/hostlink_client.py --port 5071 --notes 67 --at 6 --hold 4
        then play notes as the phone forwards a keyboard: CTRL (0x77) with
        present bit 3, note-ons at --at s after launch, held --hold s (CTRL
        polls keep the module's 1 s timeout away), then note-offs

The emulator (`--usb phone --hostlink 5071`) is the module's USB MIDI port:
the TCP stream is plain MIDI bytes both ways. Each HostLink frame
([u8 proto][u8 type][u16 seq][u16 len][body][u32 crc32], little-endian) is
COBS-encoded and ends with 0x00, exactly as over USB serial; that byte stream
is cut into runs of at most 140 bytes, and each run travels as one SysEx

    F0 7D 48 4C 01 <7-bit packed bytes> F7

where every group of up to 7 bytes is preceded by a byte holding their top
bits (bit i = top bit of byte i). That is the firmware's src/hlmidi.h and the
app's SysExPipe.swift; replies come back the same way. Standard library only.
"""
import argparse
import socket
import struct
import sys
import time
import zlib

PROTO = 1
HELLO, PHONE_INFO, STATE, CTRL = 0x01, 0x70, 0x76, 0x77
KEYS, LAUNCHPAD = 0x08, 0x01   # CTRL present bits (CONTROLLERS.md)
SYSEX_HEAD = bytes([0xF0, 0x7D, 0x48, 0x4C, 0x01])
MAX_RAW_PER_MESSAGE = 140
STATUS = {0: "OK", 1: "UNSUPPORTED", 2: "BAD_ARGS", 3: "BAD_STATE", 4: "BAD_CRC",
          5: "BAD_SLOT", 6: "TOO_LARGE", 7: "SCHEMA_MISMATCH", 8: "FLASH_FAIL",
          9: "BUSY", 10: "FRAME_ERROR", 11: "FS_NO_CARD", 12: "FS_NO_FILE"}


# ---- COBS ----
def cobs_encode(data: bytes) -> bytes:
    out, block = bytearray(), bytearray()
    for b in data:
        if b == 0:
            out.append(len(block) + 1)
            out += block
            block.clear()
        else:
            block.append(b)
            if len(block) == 254:
                out.append(255)
                out += block
                block.clear()
    out.append(len(block) + 1)
    out += block
    return bytes(out)


def cobs_decode(data: bytes) -> bytes:
    out, i = bytearray(), 0
    while i < len(data):
        code = data[i]
        if code == 0 or i + code > len(data):
            raise ValueError("bad COBS")
        out += data[i + 1:i + code]
        i += code
        if code != 0xFF and i < len(data):
            out.append(0)
    return bytes(out)


# ---- HostLink frames ----
def frame(cmd: int, seq: int, body: bytes = b"") -> bytes:
    head = struct.pack("<BBHH", PROTO, cmd, seq, len(body)) + body
    return head + struct.pack("<I", zlib.crc32(head) & 0xFFFFFFFF)


def parse_frame(dec: bytes):
    if len(dec) < 10:
        raise ValueError("short frame")
    proto, typ, seq, n = struct.unpack_from("<BBHH", dec)
    if proto != PROTO or len(dec) != 10 + n:
        raise ValueError("bad header")
    (crc,) = struct.unpack_from("<I", dec, 6 + n)
    if crc != zlib.crc32(dec[:6 + n]) & 0xFFFFFFFF:
        raise ValueError("bad CRC")
    return typ, seq, dec[6:6 + n]


# ---- SysEx (hlmidi.h) ----
def pack7(raw: bytes) -> bytes:
    out = bytearray()
    for i in range(0, len(raw), 7):
        g = raw[i:i + 7]
        out.append(sum(((b >> 7) & 1) << k for k, b in enumerate(g)))
        out += bytes(b & 0x7F for b in g)
    return bytes(out)


def to_sysex(stream: bytes) -> bytes:
    """A HostLink byte stream as SysEx messages of up to 140 raw bytes each."""
    out = bytearray()
    for i in range(0, len(stream), MAX_RAW_PER_MESSAGE):
        out += SYSEX_HEAD + pack7(stream[i:i + MAX_RAW_PER_MESSAGE]) + b"\xF7"
    return bytes(out)


class SysexDecoder:
    """MIDI bytes in, HostLink bytes out; ignores anything that isn't ours."""
    IDLE, HEAD, DATA = 0, 1, 2

    def __init__(self):
        self.state, self.head, self.pos, self.top = self.IDLE, 0, 0, 0
        self.messages = 0

    def feed(self, data: bytes) -> bytes:
        out = bytearray()
        for b in data:
            if b >= 0xF8:
                continue                      # real-time: transparent
            if b == 0xF0:
                self.state, self.head = self.HEAD, 1
                continue
            if b & 0x80:                      # F7 or another status ends it
                if self.state == self.DATA and b == 0xF7:
                    self.messages += 1
                self.state = self.IDLE
                continue
            if self.state == self.HEAD:
                if b != SYSEX_HEAD[self.head]:
                    self.state = self.IDLE
                    continue
                self.head += 1
                if self.head == len(SYSEX_HEAD):
                    self.state, self.pos = self.DATA, 0
            elif self.state == self.DATA:
                if self.pos == 0:
                    self.top = b
                else:
                    out.append(b | (((self.top >> (self.pos - 1)) & 1) << 7))
                self.pos = (self.pos + 1) & 7
        return bytes(out)


class Link:
    def __init__(self, host: str, port: int, timeout: float):
        self.sock = socket.create_connection((host, port), timeout=timeout)
        self.sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
        self.timeout = timeout
        self.dec = SysexDecoder()
        self.buf = bytearray()   # HostLink bytes up to the next 0x00
        self.frames = []
        self.seq = 0

    def request(self, cmd: int, body: bytes = b""):
        self.seq = (self.seq + 1) & 0xFFFF
        seq = self.seq
        self.sock.sendall(to_sysex(cobs_encode(frame(cmd, seq, body)) + b"\x00"))
        deadline = time.monotonic() + self.timeout
        while time.monotonic() < deadline:
            for typ, s, b in self.frames:
                if s == seq and typ in (cmd | 0x80, 0xFF):
                    self.frames.remove((typ, s, b))
                    return typ, b
            self.sock.settimeout(max(0.01, deadline - time.monotonic()))
            try:
                data = self.sock.recv(4096)
            except socket.timeout:
                break
            if not data:
                raise ConnectionError("emulator closed the connection")
            for byte in self.dec.feed(data):
                if byte:
                    self.buf.append(byte)
                    continue
                if self.buf:
                    try:
                        self.frames.append(parse_frame(cobs_decode(bytes(self.buf))))
                    except ValueError as e:
                        print(f"  (dropped a frame: {e})", file=sys.stderr)
                self.buf.clear()
        raise TimeoutError(f"no answer to 0x{cmd:02X} in {self.timeout} s")


class Reader:
    def __init__(self, b: bytes):
        self.b, self.i = b, 0

    def take(self, fmt):
        v = struct.unpack_from("<" + fmt, self.b, self.i)
        self.i += struct.calcsize("<" + fmt)
        return v[0] if len(v) == 1 else v

    def str(self):
        n = self.take("B")
        s = self.b[self.i:self.i + n].decode("ascii", "replace")
        self.i += n
        return s

    def left(self):
        return len(self.b) - self.i


def status_of(typ, body):
    st = body[0] if body else 0xFF
    name = STATUS.get(st, f"status {st}")
    if typ == 0xFF:
        name = "ERR frame: " + name
    return st if typ != 0xFF else -1, name


def show_hello(r: Reader):
    proto, board, slots, boot = r.take("BBBB")
    uid = r.b[r.i:r.i + 12]
    r.i += 12
    schema, cap, dlen, dcrc = r.take("IIII")
    max_body, live = r.take("HH")
    mid, name, ver, git, sdk = r.str(), r.str(), r.str(), r.str(), r.str()
    print(f"  module {name} ({mid}) fw {ver} git {git} sdk {sdk}")
    print(f"  board v{board}, {slots} slots, max_body {max_body}, mcu_uid {uid.hex()}")


def show_info(r: Reader):
    ver, flags = r.take("BB")
    leds = r.take("H")
    layout = r.str()
    unit = r.take("I") if ver >= 2 and r.left() >= 4 else None
    caps = [n for bit, n in enumerate(["SCREEN", "FIRMWARE", "PRESS", "STATE", "CTRL"]) if flags >> bit & 1]
    print(f"  phone block v{ver}, flags 0x{flags:02X} ({' '.join(caps) or '-'}), {leds} LEDs, layout {layout}")
    if unit is not None:
        print(f"  unit 0x{unit:08X}")


def show_state(r: Reader):
    frame_no, page, flags, tpage, tpot, tage, cpu, inl, outl = r.take("HBBBBHBBB")
    touch = "none" if tpot == 0xFF else f"page {tpage} pot {tpot}, {tage} ms ago"
    print(f"  frame {frame_no}, page {page}, flags 0x{flags:02X} (settings {'open' if flags & 1 else 'closed'})")
    print(f"  touch {touch}; cpu {cpu * 100 // 255}%, in {inl}/255, out {outl}/255")
    if r.left() < 2:
        return
    tag, tver = r.take("BB")
    if tag != ord("B"):
        print(f"  tail '{chr(tag)}' v{tver}, {r.left()} bytes")
        return
    key, scale = r.take("BB")
    harm = [r.take("B") for _ in range(4)]
    bflags = r.take("B")
    sung, cents = r.take("hh")
    last, n = r.take("HB")
    trace = [r.take("h") for _ in range(n)]
    names = ["voiced", "hard", "muted", "hard latched", "mute latched", "J3 gate"]
    on = [nm for bit, nm in enumerate(names) if bflags >> bit & 1]
    print(f"  Belt v{tver}: key {key}, scale {scale}, voices {harm}, {', '.join(on) or 'idle'}")
    print(f"  sung {'-' if sung == -32768 else sung} cents {cents}; trace last {last}, {n} new")


def ctrl(link, present, events=()):
    """One CTRL request; returns (present_seen, lp_leds) or None."""
    body = bytes([present, len(events)]) + b"".join(bytes(e) for e in events)
    typ, b = link.request(CTRL, body)
    st, name = status_of(typ, b)
    if st != 0 or len(b) < 3 + 81 + 40:
        print(f"CTRL (0x77): {name}")
        return None
    return b[2], b[3:3 + 81]


def voice_row(leds):
    """Launchpad row 7 (Belt's harmony voices 1-4): which are lit."""
    return "".join("x" if leds[6 * 8 + v] else "." for v in range(4))


def play_notes(link, notes, at, hold, t0):
    """Keyboard over CTRL, with the Launchpad present so Belt paints its voice
    row (G_MASK: in Harmony mode a held note takes a voice). Returns failures."""
    bad = 0
    present = KEYS | LAUNCHPAD

    def poll_until(t_end, label, present=present):
        rows = []   # the voice row as it went, changes only; the last one counts
        seen = None
        while time.monotonic() < t_end:
            r = ctrl(link, present)
            if r:
                seen = r[0]
                if not rows or rows[-1] != voice_row(r[1]):
                    rows.append(voice_row(r[1]))
            time.sleep(0.1)
        print(f"  {label}: present_seen 0x{seen:02X}, voice row {' -> '.join(rows)}")
        return seen, rows[-1] if rows else None

    r = ctrl(link, present)
    if not r:
        return 1
    print(f"CTRL (0x77): OK, present 0x{present:02X} -> present_seen 0x{r[0]:02X}")
    if not r[0] & KEYS:
        print("  keyboard bit 3 not echoed: this firmware doesn't play notes")
        return 1
    _, before = poll_until(t0 + at, "before")
    r = ctrl(link, present, [(6, n, 100, 0) for n in notes])
    print(f"  t={time.monotonic() - t0:.1f} s note on {notes}")
    _, held = poll_until(time.monotonic() + hold, "held")
    ctrl(link, present, [(6, n, 0, 0) for n in notes])
    print(f"  t={time.monotonic() - t0:.1f} s note off {notes}")
    _, after = poll_until(time.monotonic() + 1.5, "released")
    if held == before or after != before:
        print("  FAIL: the voice row did not follow the held notes")
        bad += 1

    # the pedal holds released notes; dropping bit 3 lets go of everything
    ctrl(link, present, [(7, 1, 0, 0)] + [(6, n, 100, 0) for n in notes] + [(6, n, 0, 0) for n in notes])
    _, ped = poll_until(time.monotonic() + 1.0, "pedal down, keys pressed and let go")
    _, gone = poll_until(time.monotonic() + 1.0, "bit 3 cleared", LAUNCHPAD)
    if ped != held or gone != after:
        print("  FAIL: sustain / release on bit 3 clear")
        bad += 1
    return bad


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--host", default="127.0.0.1")
    ap.add_argument("--port", type=int, default=5071)
    ap.add_argument("--timeout", type=float, default=3.0)
    ap.add_argument("--wait", type=float, default=10.0,
                    help="seconds to keep trying to connect (the emulator may still be starting)")
    ap.add_argument("--check", action="store_true", help="exit 1 unless every answer is OK")
    ap.add_argument("--notes", default="", help="comma-separated MIDI notes to play over CTRL")
    ap.add_argument("--at", type=float, default=0.0, help="seconds after launch to press them")
    ap.add_argument("--hold", type=float, default=3.0, help="seconds to hold them")
    a = ap.parse_args()
    t0 = time.monotonic()

    end = time.monotonic() + a.wait
    while True:
        try:
            link = Link(a.host, a.port, a.timeout)
            break
        except OSError as e:
            if time.monotonic() > end:
                print(f"cannot connect to {a.host}:{a.port}: {e}", file=sys.stderr)
                return 2
            time.sleep(0.2)
    print(f"connected to {a.host}:{a.port}")

    bad = 0
    for cmd, label, body, show in [(HELLO, "HELLO", b"", show_hello),
                                   (PHONE_INFO, "PHONE_INFO", b"", show_info),
                                   (STATE, "STATE", b"", show_state)]:
        # the firmware may still be booting when we connect: HELLO retries
        tries = 3 if cmd == HELLO else 1
        typ = None
        for t in range(tries):
            try:
                typ, body_r = link.request(cmd, body)
                break
            except TimeoutError as e:
                if t == tries - 1:
                    print(f"{label} (0x{cmd:02X}): {e} (is the emulator in Phone mode?)")
            except (ConnectionError, OSError) as e:
                print(f"{label} (0x{cmd:02X}): {e}")
                return 1 if a.check else 0
        if typ is None:
            bad += 1
            continue
        st, name = status_of(typ, body_r)
        print(f"{label} (0x{cmd:02X}): {name}")
        if st != 0:
            bad += 1
            continue
        try:
            show(Reader(body_r[1:]))
        except struct.error:
            print("  (reply shorter than expected)")
            bad += 1
    if a.notes:
        bad += play_notes(link, [int(n) for n in a.notes.split(",")], a.at, a.hold, t0)
    print(f"{link.dec.messages} SysEx messages received")
    return 1 if a.check and bad else 0


if __name__ == "__main__":
    sys.exit(main())
