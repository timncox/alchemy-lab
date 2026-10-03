# Your firmware, on the emulated Alchemy Lab:
#
#   make FW=custom FW_DIR=~/my-module
#
# Works out of the box for a firmware built the standard Daisy way (a Makefile
# with TARGET / CPP_SOURCES / ALCHEMY_DIR / LIBDAISY_DIR -- the shape of
# hermetic-modular/alchemy-template): its Makefile is asked for those
# variables (fw/print-vars.mk; no rule runs, so no ARM toolchain is needed).
#
# Anything else (a CMake project, an unusual layout) describes itself in
# <FW_DIR>/emu.mk instead -- read FIRST, so whatever it sets wins:
#
#   EMU_TARGET       := capicola                   # name (required without a Makefile)
#   EMU_CPP_SOURCES  := src/main.cpp src/audio/x.cpp   # the app's own .cpp files
#   EMU_C_SOURCES    := ...                        # and .c / .cc (EMU_CC_SOURCES)
#   EMU_C_INCLUDES   := -Isrc -Ilib                # the app's own include dirs
#   EMU_C_DEFS       := -DMY_FLAG=1                # its defines
#   EMU_ALCHEMY_DIR  := lib/alchemy-sdk            # where its SDK is (default)
#   EMU_LIBDAISY_DIR := lib/alchemy-sdk/vendor/libDaisy   # and libDaisy
#   EMU_LABELS       := GAIN|TONE|MIX|TIME|FDBK|WET   # the six knob names, P1..P6
#   EMU_LABELS_ALT   := ...                        # a second page's names (optional)
#   EMU_EXTRA_SRC    := emu/my_shim.cpp            # extra emulator-only sources
#   EMU_EXTRA_FLAGS  := -include emu/prefix.h      # extra flags for the main file
#
# Paths are relative to FW_DIR. Sources under the SDK or libDaisy are dropped
# from the app list: the emulator compiles the firmware's own SDK itself
# (minus the chips it replaces), so it always matches the SDK version the
# firmware was written against. See AGENTS.md.
ifeq ($(FW_DIR),)
$(error FW=custom needs FW_DIR=<path to your firmware repo>)
endif
override FW_DIR := $(abspath $(patsubst ~/%,$(HOME)/%,$(FW_DIR)))
ifeq ($(wildcard $(FW_DIR)),)
$(error FW_DIR $(FW_DIR) does not exist)
endif

-include $(FW_DIR)/emu.mk

ifeq ($(strip $(EMU_TARGET)),)
ifeq ($(wildcard $(FW_DIR)/Makefile),)
$(error $(FW_DIR) has no Makefile; describe the build in $(FW_DIR)/emu.mk (see fw/custom.mk, AGENTS.md))
endif
EMU_VARS_FILE := $(shell mkdir -p build && echo build/.vars-$(notdir $(FW_DIR)).mk)
$(shell $(MAKE) -s --no-print-directory -C $(FW_DIR) -f Makefile -f $(abspath fw/print-vars.mk) emu-print-vars > $(EMU_VARS_FILE) 2>/dev/null)
include $(EMU_VARS_FILE)
ifeq ($(strip $(EMU_TARGET)),)
$(error could not read TARGET from $(FW_DIR)/Makefile; describe the build in $(FW_DIR)/emu.mk (see AGENTS.md))
endif
endif

abs_fw = $(foreach f,$(1),$(if $(filter /%,$(f)),$(f),$(FW_DIR)/$(f)))
ALCHEMY_DIR  := $(call abs_fw,$(or $(EMU_ALCHEMY_DIR),lib/alchemy-sdk))
LIBDAISY_DIR := $(call abs_fw,$(or $(EMU_LIBDAISY_DIR),$(if $(wildcard $(FW_DIR)/lib/libDaisy),lib/libDaisy,$(patsubst $(FW_DIR)/%,%,$(ALCHEMY_DIR))/vendor/libDaisy)))
ifeq ($(wildcard $(ALCHEMY_DIR)/framework),)
$(error no Alchemy SDK at $(ALCHEMY_DIR) -- run "git submodule update --init --recursive" in $(FW_DIR), or set EMU_ALCHEMY_DIR in emu.mk)
endif
ifeq ($(wildcard $(LIBDAISY_DIR)/src),)
$(error no libDaisy at $(LIBDAISY_DIR) -- run "git submodule update --init --recursive" in $(FW_DIR), or set EMU_LIBDAISY_DIR in emu.mk)
endif

not_in_libs = $(filter-out $(ALCHEMY_DIR)/% $(LIBDAISY_DIR)/%,$(1))
APP_CPP := $(call not_in_libs,$(call abs_fw,$(EMU_CPP_SOURCES)))
APP_CC  := $(call not_in_libs,$(call abs_fw,$(EMU_CC_SOURCES)))
APP_C   := $(call not_in_libs,$(call abs_fw,$(EMU_C_SOURCES)))

# the translation unit with main() is the firmware's (it becomes firmware_main)
MAIN_RE := '^[[:space:]]*int[[:space:]]+main[[:space:]]*[(]'
FW_MAIN := $(firstword $(foreach f,$(APP_CPP),$(if $(shell grep -lE $(MAIN_RE) $(f) 2>/dev/null),$(f))))
ifeq ($(FW_MAIN),)
$(error no C++ source with "int main(" among: $(APP_CPP))
endif
FW_CXX := $(filter-out $(FW_MAIN),$(APP_CPP)) $(call abs_fw,$(EMU_EXTRA_SRC))
FW_CC  := $(APP_CC)
FW_C   := $(APP_C)
FW_MAIN_FLAGS := $(EMU_EXTRA_FLAGS)

# its include dirs, minus the SDK / libDaisy ones (the emulator's shadow
# headers must win over those); -include of a hardware header is dropped too
inc_dir = $(call abs_fw,$(patsubst -I%,%,$(1)))
FW_INCS := $(foreach i,$(filter -I%,$(EMU_C_INCLUDES)),$(if $(filter $(ALCHEMY_DIR)/% $(LIBDAISY_DIR)/%,$(call inc_dir,$(i))),,-I$(call inc_dir,$(i))))
# its defines, minus libDaisy's ARM/STM32 build defaults (those would switch
# the shadowed headers onto hardware paths)
DAISY_CORE_DEFS := -DALCHEMY_BOARD_V2 -DUSE_HAL_DRIVER -DSTM32H750xx -DSTM32H750IB -DCORE_CM7 \
                   -DARM_MATH_CM7 -DUSE_FULL_LL_DRIVER -DBOOT_APP -DFILEIO_ENABLE_FATFS_READER \
                   -DDAISY_FORCE_FULL_INIT -DVECT_TAB_SRAM
FW_DEFS := $(filter-out $(DAISY_CORE_DEFS) -DHSE_VALUE=%,$(EMU_C_DEFS))
FW_NAME := $(EMU_TARGET)
EMU_LABELS     ?= P1|P2|P3|P4|P5|P6
EMU_LABELS_ALT ?= $(EMU_LABELS)
# one build folder per firmware CHECKOUT (two repos may share a TARGET), and
# everything rebuilds when emu.mk changes (its labels/defines reach every file)
FW_KEY  := $(EMU_TARGET)-$(shell printf '%s' '$(FW_DIR)' | cksum | cut -c1-6 | tr -d ' ')
FW_DEPS := $(wildcard $(FW_DIR)/emu.mk) $(wildcard $(FW_DIR)/Makefile)
ifeq ($(WEB),1)
BUILD := build/web/custom/$(FW_KEY)
else
BUILD := build/custom/$(FW_KEY)$(if $(SAN),-$(SAN))
endif
FW_ROOT := $(FW_DIR)
