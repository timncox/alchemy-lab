# Smack (smack-alchemy), worktree smack-alchemy-port = the card build ff2a16c's
# sources (plus a comment-only header change).
FW_NAME   := Smack
FW_ROOT   ?= $(TIMOS)/smack-alchemy/.claude/worktrees/smack-alchemy-port
FW_MAIN   := $(FW_ROOT)/src/smack_alchemy.cpp
FW_CXX    := $(FW_ROOT)/src/usb_shared.cpp
FW_C      := $(FW_ROOT)/src/clock_adapter.c $(FW_ROOT)/src/versio_alloc.c \
             $(FW_ROOT)/src/smack_core_alchemy.c $(FW_ROOT)/src/readout.c
FW_INCS   := -I$(FW_ROOT)/src -I$(FW_ROOT)/src/vendor
FW_DEFS   := -DSMACK_VERSION=\"emu\" -DSMACK_GIT_HASH=\"$(shell git -C $(FW_ROOT) rev-parse --short HEAD)\"
FW_STUBS  := src/stubs_usbhost.cpp src/emu_ctl.cpp src/stubs_screen.cpp
EMU_LABELS     := FX|ORDER|LENGTH|SLICE|BLEND|DJ FILTER
EMU_LABELS_ALT := SEED|PITCH RANGE|CLOCK RATIO|PUNCH FX|MODE|TEMPO

# Phone mode (folder-picker + phone kit builds): the phone block compiles
# against the emulator's stand-ins for the USB device and the picker.
ifneq ($(wildcard $(FW_ROOT)/src/phone_kit.h),)
FW_CXX   += $(FW_ROOT)/src/midi_usb_transport.cpp $(FW_ROOT)/src/phone_ext.cpp
FW_INCS  += -Iinclude/fw/usbd
FW_STUBS += src/stubs_phone.cpp
endif
