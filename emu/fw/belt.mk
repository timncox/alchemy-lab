# Belt (belt-alchemy). Default source: the hold branch (HOLD + Hide and Seek).
# Another tree: make FW=belt FW_ROOT=/abs/path/to/belt-alchemy-worktree
FW_NAME   := Belt
FW_ROOT   ?= $(TIMOS)/belt-alchemy/.claude/worktrees/hold
FW_MAIN   := $(FW_ROOT)/src/belt_alchemy.cpp
FW_CXX    := $(FW_ROOT)/src/usb_shared.cpp
FW_C      := $(FW_ROOT)/src/belt_core_alchemy.c $(FW_ROOT)/src/versio_alloc.c $(FW_ROOT)/src/punch_fx.c
FW_INCS   := -I$(FW_ROOT)/src -I$(FW_ROOT)/src/vendor
# The chord sources (belt-alchemy chord-sources): the chord sequencer
# (seq-alchemy's core, vendored), the CV / chord rules and MIDI in on the
# rear header -- whose parser is libDaisy's own MidiParser, compiled here
# as it is. -idirafter: the emulator's shadow headers still win.
ifneq ($(wildcard $(FW_ROOT)/src/chord_src.c),)
FW_CXX    += $(FW_ROOT)/src/rear_midi.cpp
FW_C      += $(FW_ROOT)/src/chord_src.c $(FW_ROOT)/src/vendor/seq.c
FW_CC     := $(LIBDAISY_DIR)/src/hid/midi_parser.cpp
FW_INCS   += -idirafter $(LIBDAISY_DIR)/src
endif
FW_DEFS   := -DBELT_VERSION=\"emu\" -DBELT_GIT_HASH=\"$(shell git -C $(FW_ROOT) rev-parse --short HEAD)\"
FW_CFLAGS := -ffast-math -fno-finite-math-only
FW_STUBS  := src/stubs_usbhost.cpp src/emu_ctl.cpp
EMU_LABELS     := KEY|SCALE|RETUNE|AMOUNT|HARMONY|FORMANT
EMU_LABELS_ALT := VOICE 1|VOICE 2|VOICE 3|VOICE 4|DOUBLER|SPREAD
