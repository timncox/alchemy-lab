// HostLink v1 host side (the Alchemy SDK's docs/hostlink-protocol.md), for
// any transport: WebSerial to a module, the browser emulator's push/pull, or
// a Unix socket to the native emulator (Node). Plain ES module, Uint8Array
// only, no Node or DOM APIs.
//
//   const link = new HostLink(transport)   // transport: { write(u8), onData(cb) }
//   const id   = await link.hello()
//   const desc = await link.descriptor()
//   const live = await link.getLive()      // Uint8Array, desc.size bytes
//   const v    = decodeState(desc, live)   // { fieldId: number }
//   await link.setLive(encodeState(desc, live, { "p0.1": 0.7 }))

export const CMD = {
  hello: 0x01, descriptor: 0x02,
  listSlots: 0x10, readSlot: 0x11, blobBegin: 0x12, blobData: 0x13, blobCommit: 0x14, eraseSlot: 0x15,
  getLive: 0x20, saveToSlot: 0x30, loadFromSlot: 0x31, reboot: 0x40,
};

export const STATUS = ["ok", "unsupported", "bad-args", "bad-state", "bad-crc", "bad-slot", "too-large",
  "schema-mismatch", "flash-fail", "busy", "frame-error", "no-card", "no-file", "exists", "locked",
  "full", "io"];

export const LIVE = 0xff;   // BLOB_BEGIN target: audition into the running module

export class HostLinkError extends Error {
  constructor(cmd, status) {
    super(`HostLink 0x${cmd.toString(16)}: ${STATUS[status] ?? `device error (${status})`}`);
    this.cmd = cmd;
    this.status = status;
  }
}

// ---- framing (§1) ----------------------------------------------------------

const CRC_TABLE = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) {
    let c = n;
    for (let k = 0; k < 8; k++) c = c & 1 ? 0xedb88320 ^ (c >>> 1) : c >>> 1;
    t[n] = c >>> 0;
  }
  return t;
})();

export function crc32(buf) {
  let c = 0xffffffff;
  for (let i = 0; i < buf.length; i++) c = CRC_TABLE[(c ^ buf[i]) & 0xff] ^ (c >>> 8);
  return (c ^ 0xffffffff) >>> 0;
}

export function cobsEncode(src) {
  const out = new Uint8Array(src.length + Math.ceil(src.length / 254) + 1);
  let o = 1, code = 1, codeAt = 0;
  for (let i = 0; i < src.length; i++) {
    if (src[i] === 0) { out[codeAt] = code; codeAt = o++; code = 1; continue; }
    out[o++] = src[i];
    if (++code === 0xff) { out[codeAt] = code; codeAt = o++; code = 1; }
  }
  out[codeAt] = code;
  return out.subarray(0, o);
}

export function cobsDecode(src) {
  const out = new Uint8Array(src.length);
  let i = 0, o = 0;
  while (i < src.length) {
    const code = src[i++];
    if (code === 0 || i + code - 1 > src.length) return null;
    for (let k = 1; k < code; k++) out[o++] = src[i++];
    if (code < 0xff && i < src.length) out[o++] = 0;
  }
  return out.subarray(0, o);
}

function frame(type, seq, body) {
  const dec = new Uint8Array(6 + body.length + 4);
  const dv = new DataView(dec.buffer);
  dec[0] = 1; dec[1] = type;
  dv.setUint16(2, seq, true); dv.setUint16(4, body.length, true);
  dec.set(body, 6);
  dv.setUint32(6 + body.length, crc32(dec.subarray(0, 6 + body.length)), true);
  const enc = cobsEncode(dec);
  const wire = new Uint8Array(enc.length + 1);
  wire.set(enc);
  return wire;   // trailing 0x00 delimiter
}

// ---- little-endian body helpers ------------------------------------------------

export class Writer {
  constructor() { this.b = []; }
  u8(v) { this.b.push(v & 0xff); return this; }
  u16(v) { this.b.push(v & 0xff, (v >>> 8) & 0xff); return this; }
  u32(v) { for (let k = 0; k < 4; k++) this.b.push((v >>> (8 * k)) & 0xff); return this; }
  bytes(u8) { for (const x of u8) this.b.push(x); return this; }
  str(s) { const e = new TextEncoder().encode(s); this.u8(e.length); return this.bytes(e); }
  done() { return Uint8Array.from(this.b); }
}

export class Reader {
  constructor(u8) { this.u = u8; this.dv = new DataView(u8.buffer, u8.byteOffset, u8.byteLength); this.i = 0; }
  u8() { return this.u[this.i++]; }
  u16() { const v = this.dv.getUint16(this.i, true); this.i += 2; return v; }
  u32() { const v = this.dv.getUint32(this.i, true); this.i += 4; return v; }
  bytes(n) { const v = this.u.subarray(this.i, this.i + n); this.i += n; return v; }
  str() { return new TextDecoder().decode(this.bytes(this.u8())); }
  rest() { return this.u.subarray(this.i); }
}

// ---- the link ----------------------------------------------------------------

export class HostLink {
  constructor(transport) {
    this.t = transport;
    this.seq = 1;
    this.rx = [];
    this.pending = null;   // stop-and-wait (§7): one request in flight
    this.queue = Promise.resolve();
    this.maxBody = 512;    // until HELLO says otherwise
    transport.onData((u8) => this.#onData(u8));
  }

  #onData(u8) {
    for (const x of u8) {
      if (x !== 0) { this.rx.push(x); continue; }
      const dec = this.rx.length ? cobsDecode(Uint8Array.from(this.rx)) : null;
      this.rx = [];
      if (!dec || dec.length < 10 || dec[0] !== 1) continue;
      const dv = new DataView(dec.buffer, dec.byteOffset, dec.byteLength);
      const len = dv.getUint16(4, true);
      if (dec.length !== 10 + len) continue;
      if (crc32(dec.subarray(0, 6 + len)) !== dv.getUint32(6 + len, true)) continue;
      const seq = dv.getUint16(2, true);
      const p = this.pending;
      if (p && seq === p.seq && (dec[1] === (p.type | 0x80) || dec[1] === 0xff)) {
        this.pending = null;
        clearTimeout(p.timer);
        p.resolve({ type: dec[1], body: dec.slice(6, 6 + len) });
      }
    }
  }

  // One request -> its response body after the status byte. Throws on a
  // non-OK status (unless allow lists it) or on timeout.
  request(type, body = new Uint8Array(0), { timeout = 2000, allow = [] } = {}) {
    const run = () => new Promise((resolve, reject) => {
      const seq = this.seq = (this.seq + 1) & 0xffff || 1;
      const timer = setTimeout(() => {
        this.pending = null;
        reject(new Error(`HostLink 0x${type.toString(16)}: no answer in ${timeout} ms`));
      }, timeout);
      this.pending = { seq, type, timer, resolve: ({ body: b }) => {
        const status = b[0];
        if (status !== 0 && !allow.includes(status)) reject(new HostLinkError(type, status));
        else resolve({ status, r: new Reader(b.subarray(1)) });
      } };
      Promise.resolve(this.t.write(frame(type, seq, body))).catch(reject);
    });
    const p = this.queue.then(run, run);
    this.queue = p.catch(() => {});
    return p;
  }

  async hello() {
    const { r } = await this.request(CMD.hello);
    const h = {
      proto: r.u8(), board: r.u8(), slotCount: r.u8(), bootSlot: r.u8(), uid: r.bytes(12),
      schemaHash: r.u32(), blobCapacity: r.u32(), descriptorLen: r.u32(), descriptorCrc: r.u32(),
      maxBody: r.u16(), liveSize: r.u16(),
      moduleId: r.str(), moduleName: r.str(), fw: r.str(), git: r.str(), sdk: r.str(),
    };
    this.maxBody = h.maxBody;
    this.id = h;
    return h;
  }

  get chunk() { return this.maxBody - 16; }

  async descriptor() {
    const h = this.id ?? await this.hello();
    if (!h.descriptorLen) throw new Error("this firmware has no descriptor");
    const out = new Uint8Array(h.descriptorLen);
    for (let off = 0; off < out.length;) {
      const { r } = await this.request(CMD.descriptor,
        new Writer().u32(off).u16(Math.min(this.chunk, out.length - off)).done());
      r.u32();
      const n = r.u16();
      if (!n) break;
      out.set(r.bytes(n), off);
      off += n;
    }
    if (crc32(out) !== h.descriptorCrc) throw new Error("descriptor CRC mismatch");
    return JSON.parse(new TextDecoder().decode(out));
  }

  // GET_LIVE: offset 0 snapshots the running state; later offsets read it.
  async getLive() {
    let out = null;
    for (let off = 0; ;) {
      const { r } = await this.request(CMD.getLive, new Writer().u32(off).u16(this.chunk).done());
      r.u8(); r.u32();
      const n = r.u16(), total = r.u16();
      r.u32();
      out ??= new Uint8Array(total);
      out.set(r.bytes(n), off);
      off += n;
      if (!n || off >= total) return out;
    }
  }

  // BLOB_* to a slot (0..15) or LIVE: the bytes must be this firmware's schema.
  async writeBlob(target, bytes) {
    const h = this.id ?? await this.hello();
    await this.request(CMD.blobBegin, new Writer().u8(target).u32(bytes.length).u32(h.schemaHash).done());
    const step = this.maxBody - 4;
    for (let off = 0; off < bytes.length; off += step)
      await this.request(CMD.blobData, new Writer().u32(off).bytes(bytes.subarray(off, off + step)).done());
    await this.request(CMD.blobCommit, new Writer().u32(crc32(bytes)).done(), { timeout: 10000 });
  }

  setLive(bytes) { return this.writeBlob(LIVE, bytes); }

  async listSlots() {
    const { r } = await this.request(CMD.listSlots);
    const n = r.u8(), slots = [];
    for (let i = 0; i < n; i++) {
      const flags = r.u8();
      slots.push({ slot: i, valid: !!(flags & 1), ours: !!(flags & 2), seq: r.u32(), length: r.u16(), schemaHash: r.u32() });
    }
    return slots;
  }

  async readSlot(slot) {
    let out = null;
    for (let off = 0; ;) {
      const { r } = await this.request(CMD.readSlot, new Writer().u8(slot).u32(off).u16(this.chunk).done());
      r.u8(); r.u32();
      const n = r.u16(), total = r.u16();
      r.u32();
      out ??= new Uint8Array(total);
      out.set(r.bytes(n), off);
      off += n;
      if (!n || off >= total) return out;
    }
  }

  saveToSlot(slot) { return this.request(CMD.saveToSlot, Uint8Array.of(slot), { timeout: 10000 }); }
  loadFromSlot(slot) { return this.request(CMD.loadFromSlot, Uint8Array.of(slot)); }
  eraseSlot(slot) { return this.request(CMD.eraseSlot, Uint8Array.of(slot), { timeout: 10000 }); }
}

// ---- transports ----------------------------------------------------------------

// The browser emulator (make WEB=1): M is the module createEmu() resolved to,
// after callMain. Bytes cross through the firmware's exchange buffer; replies
// are pulled on a timer, since the firmware answers between its own sleeps.
export function emuTransport(M, { pollMs = 4 } = {}) {
  const buf = M._emu_web_hl_buf(), size = M._emu_web_hl_buf_size();
  let cb = null, timer = null;
  return {
    write(u8) {
      for (let off = 0; off < u8.length; off += size) {
        const part = u8.subarray(off, off + size);
        M.HEAPU8.set(part, buf);   // HEAPU8 re-read each time: memory can grow
        M._emu_web_hl_push(part.length);
      }
    },
    onData(fn) {
      cb = fn;
      timer ??= setInterval(() => {
        for (let n; (n = M._emu_web_hl_pull()) > 0;) cb(M.HEAPU8.slice(buf, buf + n));
      }, pollMs);
    },
    close() { clearInterval(timer); timer = null; M._emu_web_hl_close(); },
  };
}

// A real module over WebSerial (Chrome/Edge). The Lab's front USB-C enumerates
// as the libDaisy CDC identity, shared with other Daisy devices, so identify
// it with HELLO, not by USB ids (§7).
export async function serialTransport(port) {
  await port.open({ baudRate: 115200 });
  const writer = port.writable.getWriter();
  let cb = null, reading = true;
  const reader = port.readable.getReader();
  (async () => {
    try {
      while (reading) {
        const { value, done } = await reader.read();
        if (done) break;
        if (value && cb) cb(value);
      }
    } catch { /* unplugged */ }
  })();
  return {
    write: (u8) => writer.write(u8),
    onData(fn) { cb = fn; },
    async close() {
      reading = false;
      await reader.cancel().catch(() => {});
      reader.releaseLock();
      writer.releaseLock();
      await port.close().catch(() => {});
    },
  };
}

// ---- state <-> values (§5) ------------------------------------------------------

// Every field of every field-bearing component, with its absolute offset.
export function fieldsOf(desc) {
  const out = [];
  for (const c of desc.components ?? []) {
    for (const f of c.fields ?? []) out.push({ ...f, component: c.id, kind: c.kind, abs: c.off + f.off });
  }
  return out;
}

export function decodeState(desc, bytes) {
  const dv = new DataView(bytes.buffer, bytes.byteOffset, bytes.byteLength);
  const v = {};
  for (const f of fieldsOf(desc)) {
    if (f.type === "f32") v[f.id] = dv.getFloat32(f.abs, true);
    else if (f.type === "enum") v[f.id] = bytes[f.abs];
  }
  return v;
}

// A copy of bytes with the given fields replaced; everything else (opaque
// components, names) round-trips byte-exact.
export function encodeState(desc, bytes, changes) {
  const out = bytes.slice();
  const dv = new DataView(out.buffer);
  const byId = new Map(fieldsOf(desc).map((f) => [f.id, f]));
  for (const [id, val] of Object.entries(changes)) {
    const f = byId.get(id);
    if (!f) throw new Error(`no field ${id}`);
    if (f.type === "f32") dv.setFloat32(f.abs, Math.min(1, Math.max(0, val)), true);
    else if (f.type === "enum") out[f.abs] = Math.min((f.zones ?? 256) - 1, Math.max(0, Math.round(val)));
  }
  return out;
}
