# alchemy-emu — every Alchemy Lab firmware, unchanged, on the Mac

Each firmware's own `*_alchemy.cpp`, its engine and the Alchemy SDK framework
(pages, Settings, presets, control loop, CV matrix, LED renderer, HostLink
core) are compiled **as they are**; only the board is replaced:

| Replaced | By |
|---|---|
| `AlchemyLabV2` (the board) | `include/alchemy/hw/alchemy_lab_v2.h` — pots, B1–B3, J3–J10 (CV in **and out**), the 102-LED chain are the emulator panel |
| libDaisy (`daisy_seed.h`, `per/rng.h`, `hid/usb.h`, `util/CpuLoadMeter.h`, HAL) | `include/` — real time, ADC from the panel, QSPI = a flash image, the host's RNG |
| QSPI flash at 0x90760000 | `~/.alchemy-emu/<Firmware>-flash.bin` (presets survive restarts; `--flash none` = RAM) |
| SD card | a formatted 8 MB RAM disk under the real FatFS; `--card dir` copies a folder onto it |
| Smack's 1U OLED | drawn on the panel; scripts read it (`expect screen`) |
| Launchpad Mini MK3, Launch Control XL, Haute42 | `src/emu_ctl.cpp` — emulated devices behind the firmware's own `lp::` / `xl::` / `pad::` API (`--usb launchpad`), drawn beside the panel (`src/emu_ui_ctl.cpp`) |
| SD picker, USB audio, HostLink's USB | `src/emu_stubs.cpp`, `src/stubs_usbhost.cpp` — inert |

## Firmwares (`fw/*.mk` says where each one's source comes from)

| FW | Source |
|---|---|
| belt | belt-alchemy `hold` (HOLD + Hide and Seek) |
| mark | mark-alchemy `track-page` (= the card's 75196d5) |
| smack | smack-alchemy `worktree-smack-alchemy-port` (= the card's ff2a16c + a comment) |
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
| clouds | FREEZE holds the buffer in silence, B1 white; unfrozen it fades |
| elements | STRIKE up: B1 and a J3 gate ring the resonator, it decays |
| marbles | X1, X2 (J3, J4) wander and T3 (J7) gates on the internal clock |
| meld, warps | the carrier cross-modulated by J2 input |
| plaits | drones from boot (Auto), strike decays, a J3 trigger plays |

Script language: top of `src/emu_script.cpp` (`pot`, `press`/`hold`/`tap`,
`cv`, `sing`, `lp`, `xl`, `pad`, `expect led|lp|xl|ring|ringchanged|pan|
lpmoves|lpstill|rms|tone|cv|cvrange|screen|booted|alive|finite`, `snapshot`).
`make FW=mark SAN=thread` builds a ThreadSanitizer variant (build/mark-thread/). A `#!args --usb launchpad` line gives a
script its command-line options.

## Not modelled

The M7's speed (CPU readings are the Mac's — check load on the module), the
SD picker, the controllers' USB drivers themselves (and real controllers
plugged into the Mac — next: CoreMIDI), USB audio mode, HostLink.
