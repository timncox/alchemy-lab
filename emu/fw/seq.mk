# seq (seq-alchemy), a generative sequencer for the Alchemy Lab. Default source:
# its worktree-core branch (core/seq.c + alchemy/src/seq_alchemy.cpp).
FW_NAME   := seq
FW_ROOT   ?= $(TIMOS)/seq-alchemy/.claude/worktrees/core
FW_MAIN   := $(FW_ROOT)/alchemy/src/seq_alchemy.cpp
FW_CXX    := $(FW_ROOT)/alchemy/src/usb_shared.cpp
FW_C      := $(FW_ROOT)/core/seq.c
FW_INCS   := -I$(FW_ROOT)/alchemy/src -I$(FW_ROOT)/core
FW_DEFS   := -DSEQ_VERSION=\"emu\" -DSEQ_GIT_HASH=\"$(shell git -C $(FW_ROOT) rev-parse --short HEAD)\"
FW_STUBS  := src/emu_ctl.cpp   # no USB audio mode in seq
EMU_LABELS     := DENSITY|LENGTH|SLIDE|JUMP|TONES|OCTAVE
EMU_LABELS_ALT := TEMPO|SWING|GLIDE|KEY|SCALE|CLOCK IN
# Chord out (the card's seq, 24b4e56 on): core/chord_midi.c, with the rear
# header's UART taken by a stub that drops the bytes.
ifneq ($(wildcard $(FW_ROOT)/core/chord_midi.c),)
FW_C     += $(FW_ROOT)/core/chord_midi.c
FW_STUBS += src/stubs_seq_midi_out.cpp
endif
# Phone mode (folder-picker + phone kit builds): the phone block compiles
# against the emulator's stand-ins for the USB device and the picker.
ifneq ($(wildcard $(FW_ROOT)/alchemy/src/phone_kit.h),)
FW_CXX   += $(FW_ROOT)/alchemy/src/midi_usb_transport.cpp $(FW_ROOT)/alchemy/src/phone_ext.cpp
FW_INCS  += -Iinclude/fw/usbd
FW_STUBS += src/stubs_phone.cpp
endif
