# Running your Alchemy Lab firmware in the emulator — instructions for an AI coding agent

You are helping someone run **their own firmware for the Hermetic Modular Alchemy Lab (V2)** on
this emulator, on their own computer, and test it. Read this whole file before acting.

The emulator compiles the firmware's real source code, unchanged, plus the firmware's **own copy of
the Alchemy SDK** (so the SDK version always matches), for the host computer. Only the hardware is
replaced: knobs, buttons, LED rings, CV jacks, the codec, the SD card, the QSPI flash. It has a
window (drag knobs, click buttons, hear audio) and a headless script mode with assertions, which
is what you use to test.

**The prime rule: do not change the firmware to make it fit the emulator.** If something doesn't
compile or link, the gap is in the emulator; fix it here (see "When it doesn't build"). The only
files you may add to the user's repo are `emu.mk`, `emu-tests/*.emu` and `emu-card/` (all
described below). Never edit their SDK or libDaisy submodules either.

## 1. Setup (once)

No ARM toolchain is needed: this is a native build.

- **macOS:** `xcode-select --install` (clang, make, git) and `brew install sdl2`.
- **Linux (Debian/Ubuntu):** `sudo apt install build-essential git libsdl2-dev`. Not yet tested
  there; if something breaks, it's most likely a macOS-only assumption in the Makefile. Fix it
  and tell the user.
- **Windows:** use WSL2 with the Linux steps (untested).

Get the emulator:

```sh
git clone -b emu-custom https://github.com/timncox/alchemy-lab.git
cd alchemy-lab/emu
```

The user's firmware repo needs its submodules:
`git -C <firmware> submodule update --init --recursive`.

## 2. Build their firmware

```sh
make FW=custom FW_DIR=/path/to/their-firmware -j8
# -> build/custom/<TARGET>/emu
```

- **A standard Daisy Makefile** (the shape of `hermetic-modular/alchemy-template`: `TARGET`,
  `CPP_SOURCES`, `ALCHEMY_DIR`, `LIBDAISY_DIR`) works as it is. The emulator asks that Makefile
  for its variables (`fw/print-vars.mk`; no rule runs).
- **Anything else** (CMake, a hand-rolled build, unusual paths): write `<FW_DIR>/emu.mk`. It's
  read first, so whatever it sets wins. Read their CMakeLists or build script and copy across
  only the **app's own** sources, include dirs and defines. The SDK and libDaisy are compiled by
  the emulator itself. Example (a real CMake firmware, heavylight-industries/capicola):

  ```make
  EMU_TARGET       := capicola
  EMU_CPP_SOURCES  := src/main.cpp src/audio/audio_engine.cpp src/capicola_manual.cpp
  EMU_C_INCLUDES   := -Isrc -Isrc/hal -Isrc/audio -Ilib
  EMU_C_DEFS       := -DCAPICOLA_GIT_HASH=\"emu\"
  EMU_ALCHEMY_DIR  := lib/alchemy-sdk
  EMU_LIBDAISY_DIR := lib/alchemy-sdk/vendor/libDaisy
  ```

  The full list of variables (`EMU_LABELS` for the six knob names, `EMU_EXTRA_SRC`,
  `EMU_EXTRA_FLAGS`, ...) is at the top of `fw/custom.mk`.
- **Name the knobs.** Set `EMU_LABELS := A|B|C|D|E|F` in `emu.mk` (P1..P6, front view: left
  column top to bottom is P1, P3, P5; right column is P2, P4, P6), taken from the firmware's own
  page/parameter definitions. Otherwise the panel says P1..P6. `EMU_LABELS_ALT` is a second set
  the panel shows **while B3 is held** (Tim's firmwares keep a second page there). It does not
  follow a firmware's own page switching (e.g. B1 taps); for that, name the main page, or use
  names that fit every page.
- **Rebuilds:** editing `emu.mk` or the firmware's Makefile rebuilds everything; source edits
  rebuild what changed. Each firmware checkout gets its own folder,
  `build/custom/<TARGET>-<hash>/` (`make test-custom` prints it). `make clean` deletes all builds.
- Firmware is compiled with `-w`: its warnings are hidden, since the emulator isn't the place to
  fix them.

The file with `int main(` becomes the firmware entry point (it is compiled with
`-Dmain=firmware_main`); the emulator's own `main()` runs it on a thread.

## 3. Run it

```sh
build/custom/<TARGET>/emu                    # window; the Mac's mic in, speakers out
build/custom/<TARGET>/emu --in loop.wav      # a 16-bit WAV, looped, instead of the mic
build/custom/<TARGET>/emu --record out.wav   # also write what J9/J10 put out
build/custom/<TARGET>/emu --card <dir>       # copy a folder onto the emulated SD card
build/custom/<TARGET>/emu --flash none       # presets in RAM only (default: ~/.alchemy-emu/)
```

In the window: drag a knob up/down (double-click = centre, mouse wheel works), click/hold B1–B3
or press keys 1 2 3, drag the J3–J8 sliders for CV in (GATE = 0/+5 V). Jacks the firmware drives
as outputs turn cyan and show their volts.

## 4. Test it (this is the useful part)

```sh
make test-custom FW_DIR=/path/to/their-firmware
```

This runs `tests/generic/smoke.emu` (boots, survives every knob, button, Settings and CV change,
keeps refreshing LEDs, never outputs NaN), then every `<FW_DIR>/emu-tests/*.emu`. Logs are in
`build/logs/custom-*.txt`. Every script starts fresh: `--flash none`, so no presets or saved
state carry over between scripts, and every pot starts at 0.5. Write the firmware's own tests in `<FW_DIR>/emu-tests/`: read its
source to learn what each control is supposed to do, then assert it. Scripts run in real time.

```
# emu-tests/filter.emu: the filter knob closes the top end
wait 3000                    # let it boot
expect booted                # audio started
pot 1 1.0                    # P1 fully up
sing 220                     # a buzzy "sung" tone at 220 Hz into J1/J2 (sing 0 = silence)
wait 500
mark                         # start a new measurement window
wait 1000
expect rms > 0.05            # output level since mark (0..1)
expect tone 1760 > 0.01      # energy at 1760 Hz in the last ~170 ms
pot 1 0.0
wait 800
expect tone 1760 < 0.005
expect led b1 green          # off red green blue purple grey white amber, or: rgb R G B
expect ring 1 > 8            # LEDs lit on P1's ring
expect cv 3 > 2.5            # a jack the firmware drives as an output (J3..J10)
snapshot /tmp/panel.bmp      # the panel as a picture: convert it and look at it
```

Things that will trip you up:

- **Measurements are mono.** `rms`, `tone` and `print` measure the average of L and R. On a
  stereo or dual-mono firmware, cutting one channel only halves the number. Use `expect pan
  left|right|centre` for the balance: left means L/R > 2, right means L/R < 0.5, centre means
  0.7 to 1.4.
- **Pick thresholds from measurements, not guesses.** Run a probe script with `print tone 220
  440 880 3520` / `print rms` / `print cv` first, then set each `expect` with a margin.
- **Pot catch.** The Alchemy SDK's paged knobs (`VirtualKnob`, `Pager`) use pot catch: after a
  page switch, or when a stored value differs from where the pot is, moving the pot does nothing
  until it **crosses** the stored value (or comes within 0.005 of it). A single `pot` jump from
  one side to the other counts as a crossing. A script that switches page and then sets a pot to
  a value on the same side does nothing. Set pots explicitly, and cross over when you mean to.
- **Paths.** A relative path in a script (snapshot, `#!args --in ...`) is relative to the
  emulator's `emu/` folder. Write snapshots to `/tmp` or another absolute path so they don't
  land in the emulator tree.

The full command set (`press/release/hold/tap b1..b3`, `cv <3-8> <volts>`, `clock`/`pulse` on
J1/J2, `expect edges/cvrange/pan/unclipped/finite/alive/leds`, `print rms|cv|leds|tones`, `print tone <hz>...`,
`ringsave`/`expect ringchanged`, ...) is documented at the top of `src/emu_script.cpp`. Every
`expect` prints `PASS`/`FAIL` with the measured value, and the run exits 1 if any failed. A line
`#!args <options>` anywhere in a script passes those command-line options for it (e.g.
`#!args --in /abs/path/input.wav`; relative paths are relative to the emulator's `emu/` folder).

**Look at the panel.** `snapshot x.bmp`, then convert it (`sips -s format png x.bmp --out x.png`
on macOS, `convert` on Linux) and view the image. The LED rings and button colours are the
firmware's own output, so this is how you check LED feedback.

Put input files the tests need, and anything the firmware expects on its SD card, in
`<FW_DIR>/emu-card/` (copied onto the card for every test).

## 5. When it doesn't build

Read the first error. Almost every case is one of these:

| Symptom | What it means | Where to fix it |
|---|---|---|
| `no member named X in alchemy::AlchemyLabV2` | the firmware uses a board member the emulator's board lacks | `include/alchemy/hw/alchemy_lab_v2.h` (+ `src/emu_board.cpp`). Copy the **real** declaration from `<their SDK>/hardware/alchemy-lab/v2/include/alchemy/hw/alchemy_lab_v2.h` and emulate it. Examples already there: `expander`, `dac` (MCP4728, J3–J6), `stm_dac` (J7/J8), `CvJack`. |
| `no member named X in daisy::Y` / unknown libDaisy type | a libDaisy API the shims don't cover | `include/daisy_seed.h` or the shadow headers under `include/per/`, `include/hid/`, `include/util/`. Match libDaisy's real signature. |
| undefined symbol at link | a function with no emulator body | a stub in `src/emu_stubs.cpp` (use `__attribute__((weak))` if some firmware provides its own) |
| hardware the emulator can't model (USB host/device, a display, an external chip) | | a stub that does nothing sensible and says so in a comment; tell the user it isn't emulated |
| error inside the SDK's `framework/src` | a newer SDK file needs something new | if it's hardware (USB, chips), add it to `SDK_REPLACED` in `Makefile` and stub what it provided; otherwise extend the shim it needs |

Rules for emulator changes: keep the real API's names and signatures; emulate behaviour from the
SDK's own source, not from guesses (DAC codes become volts through the board calibration, as
`cv_jack.cpp` does, for example); comment why; never break the other firmwares. Afterwards run `make test-custom` again for theirs, and if Tim's
firmware repos are checked out next to this one (`~/tim-os/...`, see `fw/*.mk`) also `make test`
(his ten firmwares, 30+ scripts); if they aren't, re-test the change on the three public
third-party firmwares listed at the end of this file.
Good fixes are worth sending upstream as a pull request to `timncox/alchemy-lab` (branch
`emu-custom`).

## 6. What it can't tell you

- **CPU load.** The host is far faster than the module's 480 MHz M7, so a patch that runs here
  can still overrun on the module (on the module that usually shows as the firmware's CPU warning
  or crackling). Watch for it on hardware.
- **Analog behaviour.** The codec, the CV input filtering and DAC settling are idealised; the
  calibration is the SDK's design default (`V2CalDesignFallback`).
- **The bootloader, the USB-C port (HostLink, the web programmer, USB MIDI/audio) and the SD card
  picker** aren't emulated. HostLink compiles but goes nowhere.
- **Timing.** The firmware's main loop runs on a host thread with real sleeps, and the audio
  callback on the sound card's thread, so the main loop sees roughly the same rates as on the
  module but not cycle-exact ones.

## 7. In the browser (optional)

With Emscripten installed (`brew install emscripten`), `make WEB=1 FW=custom FW_DIR=...` builds
`build/web/custom/<TARGET>/emu.{mjs,wasm}`, the same firmware for a web page. See "In the
browser" in `README.md` for how it runs there; `web/` has the page code Tim's site uses.

## Reference

- `fw/custom.mk`: how a custom firmware is found and built; `fw/print-vars.mk`
- `include/`: the shadow board and libDaisy headers (the hardware boundary)
- `src/emu_board.cpp`: panel state, jacks, LEDs, flash and card, DAC emulation
- `src/emu_script.cpp`: the test-script language (full reference at the top)
- `README.md`: architecture, the native and browser builds, Tim's firmwares
- Tried so far on third-party firmware: `hermetic-modular/alchemy-template` (stereo EQ, SDK
  v0.12), `justifiedfalsebeliefs/alchemy-lab-lfos` (an older SDK; drives the DAC directly) and
  `heavylight-industries/capicola` (CMake, via emu.mk). All three build unchanged and pass the
  smoke test. An agent given only this file then wrote and passed 44 checks of the template's EQ
  bands, channels, pages, pot catch and CV.
