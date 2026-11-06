# alchemy-emu — every Alchemy Lab firmware, unchanged, on the Mac

> **Your own firmware?** `make FW=custom FW_DIR=~/my-module` builds any Alchemy Lab
> firmware on its own copy of the SDK, and `make test-custom FW_DIR=...` tests it.
> **[AGENTS.md](AGENTS.md)** is the full guide, written so an AI coding agent
> (Claude Code, Codex, Cursor, ...) can do it for you. Paste this in your firmware's folder:
>
> *Clone https://github.com/timncox/alchemy-lab (branch `emu-custom`), read `emu/AGENTS.md`,
> and get this firmware running in the Alchemy Lab emulator. Then write emulator tests for its
> main controls and run them.*

Each firmware's own `*_alchemy.cpp`, its engine and the Alchemy SDK framework
(pages, Settings, presets, control loop, CV matrix, LED renderer, HostLink
core) are compiled **as they are**; only the board is replaced:

| Replaced | By |
|---|---|
| `AlchemyLabV2` (the board) | `include/alchemy/hw/alchemy_lab_v2.h` — pots, B1–B3, J3–J10 (CV in **and out**), the 102-LED chain are the emulator panel; J1/J2 are the SDK's own `TriggerJack`, fed the input blocks before the firmware's callback as the Lab's audio shim does |
| libDaisy (`daisy_seed.h`, `per/rng.h`, `hid/usb.h`, `util/CpuLoadMeter.h`, HAL) | `include/` — real time, ADC from the panel, QSPI = a flash image, the host's RNG |
| QSPI flash at 0x90760000 | `~/.alchemy-emu/<Firmware>-flash.bin` (presets survive restarts; `--flash none` = RAM) |
| SD card | a formatted 64 MB FAT32 RAM disk under the real FatFS; `--card dir` copies a folder onto it |
| Smack's 1U OLED | drawn on the panel; scripts read it (`expect screen`) |
| Launchpad Mini MK3, Launch Control XL, Haute42 | `src/emu_ctl.cpp` — emulated devices behind the firmware's own `lp::` / `xl::` / `pad::` API (`--usb launchpad`), drawn beside the panel (`src/emu_ui_ctl.cpp`) |
| The front USB-C as a USB device (libDaisy's `UsbHandle`, Phone+Audio's MIDI) | `src/emu_hostlink.cpp` — a TCP socket (`--hostlink <port>`) carrying the USB MIDI bytes; see "Phone mode and the iOS app" |
| SD picker, USB audio, HostLink's USB-CDC (Mac mode) | `src/emu_stubs.cpp`, `src/stubs_usbhost.cpp` — inert |

## Firmwares (`fw/*.mk` says where each one's source comes from)

| FW | Source |
|---|---|
| belt | belt-alchemy `hold` (HOLD + Hide and Seek) |
| belt-phone | belt-alchemy `claude/iphone-screen` (Phone / Phone+Audio USB modes for the iPhone app; not in `all-fw` / `test`) |
| mark | mark-alchemy `track-page` (= the card's 75196d5) |
| smack | smack-alchemy `worktree-smack-alchemy-port` (= the card's ff2a16c + a comment) |
| seq | seq-alchemy `worktree-core` (the sequencer; J1 clock / J2 reset in, J3–J10 out) |
| clouds elements marbles meld plaits warps | mi-alchemy `port` (= v0.1.0); each port's own `fw.mk`, `-DTEST` (stmlib's portable path) |

Host-only adjustments, all in `fw/` / `include/fw/`, none in firmware source:
Mark's pool 62 → 72 MiB (8-byte pointers outgrow the M7's 99.6 %-full pool);
Mark's engine without threads / dlopen / directories, as on the module
(`include/fw/mark_core_prefix.h` mirrors its `host_stubs.c`);
Elements' ELF asm-labelled sample arrays aliased for Mach-O; Elements'
`elements.smp` built by its own tool and put on the card.

## Build / play

    brew install sdl2               # once
    make FW=plaits                  # build/plaits/emu   (make all-fw = all nine)
    build/plaits/emu                # Mac mic in, default output out -- HEADPHONES
    build/elements/emu --card build/elements/card     # Elements needs its samples
    build/belt/emu --in voice.wav --record out.wav
    build/belt/emu --usb launchpad      # Launchpad mode, with the three controllers plugged in

`--usb launchpad` writes `usb.cfg` onto the card, so the firmware boots in
Launchpad mode as the module does, and plugs in the emulated Launchpad Mini
MK3, Launch Control XL and Haute42 (Belt, Mark and Smack only; the MI ports
have no controller support). Click the Launchpad's pads, top row and side
column; drag the XL's knobs and faders and click its buttons; click and hold
the Haute42's buttons. Their LEDs are what the firmware sets. What runs is
each firmware's own controller code (layouts, LED painting, the XL map, the
gamepad punches); what's replaced is the USB-MIDI / XInput driver under it.

Knobs: drag or scroll (double-click = centre). B1–B3: click and hold, or keys
**1 2 3** (B2+B3 2 s = Settings). CV: drag a jack for volts, click GATE for
0 ↔ +5 V; jacks the firmware drives as outputs show their volts.

## Test

    make test                       # = make all-fw && ./run-tests.sh  (~1 min, parallel)
    ./run-tests.sh mark plaits      # just these

Every firmware runs `tests/<fw>/smoke.emu` (boots; survives every knob end to
end, every button, Settings in and out, every CV jack; LEDs keep refreshing;
output stays finite; snapshot in `build/<fw>/smoke.bmp`) plus its own:

| FW | Functional test |
|---|---|
| belt, mark, smack | **controllers_full**: every documented Launchpad / Launch Control XL / Haute42 function — Belt 94 checks (all 12 KEY pads, all 9 SCALE pads, HARD / MUTE tap + hold, HOLD, PLAY held notes sound, all 6 faders, voice / key / scale knobs, all buttons, all 13 punch buttons), Mark 44 (record / play / overdub / undo / redo / stop / ALL / clear per track from both controllers, levels, master, pan), Smack 194 (all 27 punch pads and every Haute42 button checked by the effect name on the OLED, PUNCH FX knob, LIVE, RE-ROLL, CLEAR, capture, all faders and SETUP knobs, the 8 lower punch buttons) |
| belt | HOLD Freeze (B2 0.6 s, sustains in silence, ignores a new note, fades), HOLD Lock via Settings + J8 gate, B2 mute; **controllers**: HOLD from Launchpad top 4 / XL upper 3 / Haute42 L3, the Launchpad KEY keyboard really changes key (C♯4 for A's +3rd in D), XL HARMONY fader |
| mark | record → play a loop, plays in silence; **Settings closed with B2 doesn't stop it** (75196d5 fix); B2 stops; **controllers**: Launchpad row 1 records / plays / stops track 1, XL fader 1 is its level, XL button 1 lit in its state |
| smack | capture loops in silence; OLED follows FX; **Settings closed with B2 doesn't punch**; **controllers**: Launchpad top 1 captures, a held row-1 pad punches (green, B1 white), a held Haute42 button punches (B2 white) |
| seq | **transport**: stopped at boot, B2 plays (kick / snare / hats / bass gates on J7 / J8 / J9 / J5, bass pitch moves inside 0–5 V), B2 stops; **clock**: J1 at 300 BPM takes over, B2 stops it against the clock and plays again, clock gone 2 s stops it, back starts it, J2 reset; **controllers**: Launchpad play / mute / step loop, XL drums row, XL stop, Haute42 hold-mute; **drums_file**: /seq/drums.txt from the card (tests/seq/card) |
| clouds | FREEZE holds the buffer in silence, B1 white; unfrozen it fades |
| elements | STRIKE up: B1 and a J3 gate ring the resonator, it decays |
| marbles | X1, X2 (J3, J4) wander and T3 (J7) gates on the internal clock |
| meld, warps | the carrier cross-modulated by J2 input |
| plaits | drones from boot (Auto), strike decays, a J3 trigger plays |

Script language: top of `src/emu_script.cpp` (`pot`, `press`/`hold`/`tap`,
`cv`, `sing`, `clock`/`pulse` (5 ms pulses on J1 / J2), `lp`, `xl`, `pad`, `expect edges` (rising edges an output made since `mark`), `expect led|lp|xl|ring|ringchanged|pan|
lpmoves|lpstill|rms|tone|cv|cvrange|screen|booted|alive|finite`, `snapshot`).
`make FW=mark SAN=thread` builds a ThreadSanitizer variant (build/mark-thread/). A `#!args --usb launchpad` line gives a
script its command-line options.

## Phone mode and the iOS app

Belt's iPhone branch has two more USB modes: **Phone** (the front USB-C is a
USB MIDI device carrying HostLink inside SysEx, for the Alchemy Screen app)
and **Phone+Audio** (the same plus a 2-in/2-out audio interface). In the
emulator the USB device under the firmware is a TCP socket: `--hostlink
<port>` listens on `127.0.0.1:<port>`, and a client there is the USB host.
The firmware's own `MidiUsbHostTransport`, `hlmidi.h` framing, phone block
(0x70–0x77) and HostLink core run unchanged; only the USB-MIDI device layer
is replaced (`include/hid/usb.h` → `src/emu_hostlink.cpp`).

**On the socket: plain MIDI bytes, both ways.** The client writes exactly
the SysEx it would send the module over USB MIDI, `F0 7D 48 4C 01 <packed>
F7` (each COBS-framed HostLink frame, 0x00-delimited, cut into runs of up to
140 bytes, 7-bit packed: `hlmidi.h`, the app's `SysExPipe.swift`), and reads
the module's SysEx back the same way. No USB-MIDI 4-byte packets and no
extra framing: the emulator makes and unmakes those. One client at a time; a
new connection replaces the old one.

Once, on the Mac (Belt's iPhone branch, with its submodules, next to `hold`):

    git -C ~/tim-os/belt-alchemy worktree add .claude/worktrees/iphone-screen claude/iphone-screen
    git -C ~/tim-os/belt-alchemy/.claude/worktrees/iphone-screen submodule update --init --recursive
    make FW=belt-phone                  # build/belt-phone/emu  (FW_ROOT=<dir> for another checkout)

Two modules, each in its own Terminal window:

    build/belt-phone/emu --usb phone --hostlink 5071 --flash ~/.alchemy-emu/left.bin
    build/belt-phone/emu --usb phone --hostlink 5072 --flash ~/.alchemy-emu/right.bin --nomic

Each prints `HostLink on 127.0.0.1:<port>`. Then in the app (in the iOS
Simulator, which shares the Mac's 127.0.0.1): **Connect > Emulator**.

- **Two units.** The phone tells modules apart by `unit`, a hash of the
  MCU's 96-bit unique ID. The emulator's ID is "EMU0" "BELT" and an instance
  number, which is the `--hostlink` port unless `--uid <n>` says otherwise
  (1 without either), so 5071 and 5072 are two stable, distinct modules
  (HELLO's `mcu_uid` too). Give each its own `--flash`, or both share
  `~/.alchemy-emu/Belt-flash.bin` (presets, settings) and overwrite each other.
- **`--usb phone+audio`** boots Phone+Audio: the same HostLink over the
  composite device's MIDI interface; its audio isn't modelled (the phone's
  stream into Belt is silence, Belt's stream back goes nowhere; J9/J10 still
  play on the Mac).
- **Controllers** come from the phone in Phone modes (CTRL, 0x77), so the
  emulated Launchpad / XL / Haute42 aren't drawn; `--usb launchpad` still
  has them (for this firmware through its own `launchpad_core.cpp`, with
  `src/emu_ctl_split.cpp` as the USB host).
- **Not there:** firmware switching from the phone (FW_LIST lists nothing,
  FW_FLASH answers UNSUPPORTED), and a module never reboots into another mode:
  `--usb` sets it at launch, as the card's `usb.cfg` does at power-up. A
  window with `--hostlink` and no `--usb` boots in whatever mode its saved
  settings hold (the emulator says so).

Without the app: `tools/hostlink_client.py` (Python 3, standard library)
connects, sends HELLO, PHONE_INFO (0x70) and STATE (0x76) wrapped as the app
wraps them, and prints the decoded answers; `tools/hostlink-test.sh` boots a
headless emulator in Phone mode and runs it with `--check` (exit 0 = every
answer OK). A headless emulator serves the socket during its script's
`wait`s, so a script of one long `wait` is a module with no window.

    python3 tools/hostlink_client.py --port 5071
    tools/hostlink-test.sh build/belt-phone/emu 5079 phone+audio

## Not modelled

The M7's speed (CPU readings are the Mac's — check load on the module), the
SD picker, the controllers' USB drivers themselves (and real controllers
plugged into the Mac — next: CoreMIDI), USB audio mode, HostLink over
USB-CDC (Mac mode; Phone mode's USB MIDI is the `--hostlink` socket).

## In the browser

`make WEB=1 FW=<fw>` builds the same firmware with Emscripten into
`build/web/<fw>/emu.{mjs,wasm}` (Elements adds `emu.data`, its sample card).
`web/publish.sh <site>/docs/emulator` builds all nine and copies them, plus
the demo inputs from `web/make_demos.py`, into the homepage's emulator page
(timncox.github.io/alchemy-lab/emulator/).

The browser has one thread, so the threads above become one:

- the firmware's `main()` runs under **Asyncify**: every `System::Delay`
  (`emu::SleepMs`) unwinds back to the browser and resumes after the delay;
- the **audio callback** runs from SDL's Web Audio callback while the firmware
  is suspended -- the way the audio interrupt cuts into the main loop on the
  module;
- the **panel** is drawn by `emscripten_request_animation_frame_loop`, not
  `emscripten_set_main_loop` (Emscripten pauses that loop while any Asyncify
  sleep is pending, i.e. always);
- `SDL_HINT_EMSCRIPTEN_ASYNCIFY=0`: SDL's canvas present would otherwise call
  `emscripten_sleep(0)` from inside the panel frame, a second sleep on top of
  the firmware's, and the firmware's next rewind aborts ("unreachable").

`emu::CallbackScope` marks the audio and panel code; a `SleepMs` from inside it
returns at once instead of trying to unwind. Presets live in memory only;
`--nomic` keeps the browser from asking for the microphone. No scripts in the
browser build -- the tests run on the Mac build.
