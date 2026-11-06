# Belt (belt-alchemy) from its iPhone branch, claude/iphone-screen: the Phone
# and Phone+Audio USB modes (USB MIDI carrying HostLink, the phone block
# 0x70-0x77, controllers forwarded from the phone). Serve it to the iPhone
# app with --usb phone --hostlink <port>; see "Phone mode and the iOS app"
# in README.md. Not part of all-fw / test.
#
#   make FW=belt-phone                         the worktree below
#   make FW=belt-phone FW_ROOT=~/elsewhere      any checkout of that branch
#
# Its own SDK and libDaisy submodules (the branch may pin a newer SDK).
FW_NAME   := Belt
FW_ROOT   ?= $(TIMOS)/belt-alchemy/.claude/worktrees/iphone-screen
override FW_ROOT := $(abspath $(patsubst ~/%,$(HOME)/%,$(FW_ROOT)))
ALCHEMY_DIR  := $(FW_ROOT)/lib/alchemy-sdk
LIBDAISY_DIR := $(FW_ROOT)/lib/libDaisy
FW_MAIN   := $(FW_ROOT)/src/belt_alchemy.cpp
# unchanged firmware sources: the USB-MIDI HostLink transport runs as it is
# over the emulated device (include/hid/usb.h -> src/emu_hostlink.cpp), the
# phone block, and the hardware-free half of the controller layer (the CTRL
# remote backend; its USB-host half is src/emu_ctl_split.cpp)
FW_CXX    := $(FW_ROOT)/src/usb_shared.cpp $(FW_ROOT)/src/midi_usb_transport.cpp \
             $(FW_ROOT)/src/phone_ext.cpp $(FW_ROOT)/src/launchpad_core.cpp
# usb_midi_keys.c: the keyboard filter launchpad_core.cpp runs the phone's
# notes (CTRL types 6/7) through, as a USB-MIDI keyboard's
FW_C      := $(FW_ROOT)/src/belt_core_alchemy.c $(FW_ROOT)/src/versio_alloc.c $(FW_ROOT)/src/punch_fx.c \
             $(FW_ROOT)/src/usb_midi_keys.c
FW_INCS   := -I$(FW_ROOT)/src -I$(FW_ROOT)/src/vendor -Iinclude/fw/usbd
FW_DEFS   := -DBELT_VERSION=\"emu\" -DBELT_GIT_HASH=\"$(shell git -C $(FW_ROOT) rev-parse --short HEAD)\"
FW_CFLAGS := -ffast-math -fno-finite-math-only
FW_STUBS  := src/stubs_usbhost.cpp src/stubs_phone.cpp src/emu_ctl_split.cpp
EMU_LABELS     := KEY|SCALE|RETUNE|AMOUNT|HARMONY|FORMANT
EMU_LABELS_ALT := VOICE 1|VOICE 2|VOICE 3|VOICE 4|DOUBLER|SPREAD
