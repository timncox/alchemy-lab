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
| SD picker, USB host (Launchpad / XL / gamepad), USB audio, HostLink's USB | `src/emu_stubs.cpp`, `src/stubs_usbhost.cpp` — inert |

## Firmwares (`fw/*.mk` says where each one's source comes from)

| FW | Source |
|---|---|
| belt | belt-alchemy `hold` (HOLD + Hide and Seek) |
| mark | mark-alchemy `track-page` (= the card's 75196d5) |
| smack | smack-alchemy `worktree-smack-alchemy-port` (= the card's ff2a16c + a comment) |
| clouds elements marbles meld plaits warps | mi-alchemy `port` (= v0.1.0); each port's own `fw.mk`, `-DTEST` (stmlib's portable path) |

Host-only adjustments, all in `fw/` / `include/fw/`, none in firmware source:
Mark's pool 62 → 72 MiB (8-byte pointers outgrow the M7's 99.6 %-full pool);
Elements' ELF asm-labelled sample arrays aliased for Mach-O; Elements'
`elements.smp` built by its own tool and put on the card.

## Build / play

    brew install sdl2               # once
    make FW=plaits                  # build/plaits/emu   (make all-fw = all nine)
    build/plaits/emu                # Mac mic in, default output out -- HEADPHONES
    build/elements/emu --card build/elements/card     # Elements needs its samples
    build/belt/emu --in voice.wav --record out.wav

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
| belt | HOLD Freeze (B2 0.6 s, sustains in silence, ignores a new note, fades), HOLD Lock via Settings + J8 gate, B2 mute |
| mark | record → play a loop, plays in silence; **Settings closed with B2 doesn't stop it** (75196d5 fix); B2 stops |
| smack | capture loops in silence; OLED follows FX; **Settings closed with B2 doesn't punch** |
| clouds | FREEZE holds the buffer in silence, B1 white; unfrozen it fades |
| elements | STRIKE up: B1 and a J3 gate ring the resonator, it decays |
| marbles | X1, X2 (J3, J4) wander and T3 (J7) gates on the internal clock |
| meld, warps | the carrier cross-modulated by J2 input |
| plaits | drones from boot (Auto), strike decays, a J3 trigger plays |

Script language: top of `src/emu_script.cpp` (`pot`, `press`/`hold`/`tap`,
`cv`, `sing`, `expect led|rms|tone|cv|cvrange|screen|booted|alive|finite`,
`snapshot`).

## Not modelled

The M7's speed (CPU readings are the Mac's — check load on the module), the
SD picker, Launchpad / Launch Control XL / Haute42 (next: CoreMIDI), USB
audio mode, HostLink.
