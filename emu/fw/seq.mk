# seq (seq-alchemy), a generative sequencer for the Alchemy Lab. Default source:
# its worktree-core branch (core/seq.c + alchemy/src/seq_alchemy.cpp).
FW_NAME   := seq
FW_ROOT   ?= $(TIMOS)/seq-alchemy/.claude/worktrees/core
FW_MAIN   := $(FW_ROOT)/alchemy/src/seq_alchemy.cpp
FW_CXX    := $(FW_ROOT)/alchemy/src/usb_shared.cpp
FW_C      := $(wildcard $(FW_ROOT)/core/*.c)
FW_INCS   := -I$(FW_ROOT)/alchemy/src -I$(FW_ROOT)/core
FW_DEFS   := -DSEQ_VERSION=\"emu\" -DSEQ_GIT_HASH=\"$(shell git -C $(FW_ROOT) rev-parse --short HEAD)\"
FW_STUBS  := src/emu_ctl.cpp   # no USB audio mode in seq
# Chord out (branch chord-out): the rear-header MIDI out, its bytes logged
# for `expect uart` instead of sent. Older trees have no midi_out.h.
ifneq ($(wildcard $(FW_ROOT)/alchemy/src/midi_out.h),)
FW_STUBS  += src/emu_midi_out.cpp
endif
# A tree that pre-scales J9/J10 for the v0.11 SDK's codec transfer
# (codec_volts in seq_alchemy.cpp), built against an SDK without the fix:
# model the real jack volts so `expect cv 9|10` reads what the jack does.
ifneq ($(shell grep -l codec_volts $(FW_MAIN) 2>/dev/null),)
ifeq ($(wildcard $(ALCHEMY_DIR)/hardware/alchemy-lab/v2/include/alchemy/hw/v2_codec_cv.h),)
FW_DEFS   += -DEMU_CODEC_V011_COMPENSATED
endif
endif
EMU_LABELS     := DENSITY|LENGTH|SLIDE|JUMP|TONES|OCTAVE
EMU_LABELS_ALT := TEMPO|SWING|GLIDE|KEY|SCALE|CLOCK IN
