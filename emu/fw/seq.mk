# seq (seq-alchemy), the sequencer that replaces the Oxi One. Default source:
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
EMU_LABELS     := DENSITY|LENGTH|SLIDE|JUMP|TONES|OCTAVE
EMU_LABELS_ALT := TEMPO|SWING|GLIDE|KEY|SCALE|CLOCK IN
