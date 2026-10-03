# Mark (mark-alchemy), branch track-page = the card build 75196d5's sources.
FW_NAME   := Mark
FW_ROOT   ?= $(TIMOS)/mark-alchemy/.claude/worktrees/track-page
FW_MAIN   := $(FW_ROOT)/src/mark_alchemy.cpp
FW_CXX    := $(FW_ROOT)/src/usb_shared.cpp
FW_C      := $(FW_ROOT)/src/clock_adapter.c $(FW_ROOT)/src/versio_alloc.c \
             $(FW_ROOT)/src/mark_core_alchemy.c $(FW_ROOT)/src/punch_fx.c
FW_INCS   := -I$(FW_ROOT)/src -I$(FW_ROOT)/src/vendor
FW_DEFS   := -DMARK_VERSION=\"emu\" -DMARK_GIT_HASH=\"$(shell git -C $(FW_ROOT) rev-parse --short HEAD)\"
FW_STUBS  := src/stubs_usbhost.cpp src/emu_ctl.cpp
EMU_LABELS     := TRACK 1|TRACK 2|TRACK 3|TRACK 4|TRACK 5|TRACK SEL
EMU_LABELS_ALT := QUANTIZE|REC ->|DUB|PLAY|TEMPO|MASTER
FW_MAIN_FLAGS := -include include/fw/mark_prefix.h
