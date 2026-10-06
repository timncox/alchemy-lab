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
| SD picker, USB audio | `src/emu_stubs.cpp`, `src/stubs_usbhost.cpp` — inert |
| HostLink's USB-CDC transport | `src/emu_hostlink.cpp` — a Unix socket (`--hostlink <path>`) natively, `emu_web_hl_*` in the browser |

## Firmwares (`fw/*.mk` says where each one's source comes from)

| FW | Source |
|---|---|
| belt | belt-alchemy `hold` (HOLD + Hide and Seek) |
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
script its command-line options. `uart <hex bytes>` feeds USART1 RX (the
rear header's pin 7) through `include/per/uart.h`, a shadow of libDaisy's
UartHandler whose circular-listen callback gets the bytes as the DMA
interrupt would.

### Belt with the chord sources

    make test-belt-chords       # build/belt-chords from belt-alchemy chord-sources, then tests/belt-chords

`tests/belt-chords` = the chord scripts (`chords_seq`: the internal chord
sequencer on the bar lines and the Launchpad CHORDS page; `chords_j3clock`:
J3 edges step the chords, J3 no longer punches HARD, sevenths; `chords_cv`:
J4-J6 V/oct chords, hysteresis, the calibration Learn against a sender off
by +30 / -45 / +10 mV, J3 sample and hold, a refused miswired input;
`chords_midi`: rear-header MIDI with running status, a clock byte inside a
message, velocity-0 offs, any channel, release on leaving the source) plus
every `tests/belt` script, symlinked, run against that build. `make test`
and the default belt build (the `hold` tree) are untouched;
`BELT_CHORDS_ROOT=` points at another tree.

## Not modelled

The M7's speed (CPU readings are the Mac's — check load on the module), the
SD picker, the controllers' USB drivers themselves (and real controllers
plugged into the Mac — next: CoreMIDI), USB audio mode.

## HostLink

The firmware's own `hostlink::Host` answers over `--hostlink <socket>` (native) or the page's
`emu_web_hl_push/pull` (browser, through a 4 KB exchange buffer at `emu_web_hl_buf()`); the bytes
are the wire bytes, COBS frames both ways. `web/hostlink.js` is the host side for any transport
(the emulator, WebSerial to a real module, a socket from Node), and `web/control/` is the page
built on it: every knob and setting from the descriptor, live, plus presets.
`node tests/hostlink/hostlink_test.mjs build/<fw>/emu ...` checks the whole round trip.

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
