# Relay (relay), MIDI to CV. The emulator has no MIDI input: Relay's MIDI mode
# boots and idles (include/fw/relay/hid), and its HostLink and Phone modes
# are what can be tested here.
FW_NAME   := Relay
FW_ROOT   ?= $(TIMOS)/relay
FW_MAIN   := $(FW_ROOT)/alchemy/src/relay_alchemy.cpp
FW_CXX    :=
FW_C      := $(FW_ROOT)/core/relay.c
FW_INCS   := -Iinclude/fw/relay -Iinclude/fw/usbd -I$(FW_ROOT)/alchemy/src -I$(FW_ROOT)/core
FW_DEFS   := -DRELAY_VERSION=\"emu\" -DRELAY_GIT_HASH=\"$(shell git -C $(FW_ROOT) rev-parse --short HEAD)\"
FW_STUBS  := src/stubs_phone.cpp
EMU_LABELS     := CHANNEL|VOICES|PRIORITY|GLIDE|BEND|CLOCK
EMU_LABELS_ALT := SRC A|CC A|SRC B|CC B|BASE|J10

# Phone mode (folder-picker + phone kit builds).
ifneq ($(wildcard $(FW_ROOT)/alchemy/src/phone_kit.h),)
FW_CXX   += $(FW_ROOT)/alchemy/src/usb_shared.cpp \
            $(FW_ROOT)/alchemy/src/midi_usb_transport.cpp $(FW_ROOT)/alchemy/src/phone_ext.cpp
endif
