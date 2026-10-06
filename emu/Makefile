# alchemy-emu: any Alchemy Lab firmware, unchanged, on an emulated module.
#
#   make FW=belt            build/belt/emu      (FW = one of $(FWS))
#   make all-fw             every firmware
#   make test               every firmware's tests/<fw>/*.emu, headless
#
# Each firmware's sources come from its own repo (fw/<fw>.mk says where);
# the Alchemy SDK framework is compiled as-is. Only the board, libDaisy's
# surface and the USB-host / picker modules are replaced (include/, src/).

FWS := belt mark smack seq clouds elements marbles meld plaits warps
ifneq ($(filter test-custom,$(MAKECMDGOALS)),)
FW := custom
endif
FW  ?= belt
ifeq ($(filter $(FW),$(FWS) custom),)
$(error FW must be one of: $(FWS), or custom FW_DIR=<your firmware repo>)
endif

#   make FW=custom FW_DIR=~/my-module
#                           YOUR firmware (any Daisy-Makefile project on the
#                           Alchemy SDK, e.g. hermetic-modular/alchemy-template)
#                           -> build/custom/<TARGET>/emu; see AGENTS.md
#   make WEB=1 FW=plaits    build/web/plaits/emu.{mjs,wasm} for the browser
#                           (Emscripten; see src/emu_web notes in emu_main.cpp)

TIMOS       ?= $(HOME)/tim-os
ALCHEMY_DIR ?= $(TIMOS)/belt-alchemy/lib/alchemy-sdk
LIBDAISY_DIR ?= $(TIMOS)/belt-alchemy/lib/libDaisy
ifeq ($(WEB),1)
BUILD       := build/web/$(FW)
else
BUILD       := build/$(FW)$(if $(SAN),-$(SAN))
endif

ifneq ($(filter $(FW),clouds elements marbles meld plaits warps),)
include fw/mi.mk
else
include fw/$(FW).mk
endif
# after the firmware's .mk: a custom firmware brings its own libDaisy
FATFS       := $(LIBDAISY_DIR)/Middlewares/Third_Party/FatFs/src
# the SD card's RAM disk: the SDK's own host-test one, or (SDKs before v0.10
# have none) the same file, MIT, copied into src/sdk_fallback/
ifneq ($(wildcard $(ALCHEMY_DIR)/tests/host/ram_diskio.cpp),)
SDK_RAMDISK := tests/host/ram_diskio.cpp
RAMDISK_INC := -I$(ALCHEMY_DIR)/tests/host
else
EMU_RAMDISK := src/sdk_fallback/ram_diskio.cpp
RAMDISK_INC := -Isrc/sdk_fallback
endif

ifeq ($(WEB),1)
# One thread: the firmware's main() runs under Asyncify and yields at every
# sleep; SDL2's Emscripten port draws to a canvas and plays through Web Audio.
CXX        := em++
CC         := emcc
SDL_CFLAGS := -sUSE_SDL=2
FW_LDFLAGS :=   # Elements' Mach-O aliases: wasm symbols need none
WEB_CARD   := $(if $(filter elements,$(FW)),$(BUILD)/card/mi/elements.smp)
# a custom firmware's <FW_DIR>/emu-card/ (test samples...) rides along as /card;
# the page passes --card /card for it
WEB_CARD_DIR := $(if $(filter custom,$(FW)),$(wildcard $(FW_DIR)/emu-card))
SDL_LIBS   := -sUSE_SDL=2 -sASYNCIFY -sASYNCIFY_STACK_SIZE=262144 -sSTACK_SIZE=1048576 \
              -sINITIAL_MEMORY=268435456 -sALLOW_MEMORY_GROWTH -sMAXIMUM_MEMORY=2GB \
              -sMODULARIZE -sEXPORT_ES6 -sEXPORT_NAME=createEmu -sINVOKE_RUN=0 \
              -sENVIRONMENT=web -sFORCE_FILESYSTEM -sEXPORTED_RUNTIME_METHODS=callMain,FS \
              $(if $(WEB_CARD),--preload-file $(BUILD)/card@/card) \
              $(if $(WEB_CARD_DIR),--preload-file $(WEB_CARD_DIR)@/card)
EXE        := $(BUILD)/emu.mjs
else
SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)
EXE        := $(BUILD)/emu
endif

DEFS := -DALCHEMY_BOARD_V2 -DALCHEMY_HOSTLINK_MAX_BODY=2048 \
        -DLIBDAISY_FFCONF=\"$(abspath $(LIBDAISY_DIR))/src/sys/ffconf.h\" \
        '-DEMU_FW_NAME="$(FW_NAME)"' '-DEMU_LABELS="$(EMU_LABELS)"' '-DEMU_LABELS_ALT="$(EMU_LABELS_ALT)"' \
        $(FW_DEFS)

INCS := -Iinclude -Isrc $(FW_INCS) \
        -I$(ALCHEMY_DIR)/framework/include -I$(ALCHEMY_DIR)/hardware/include \
        -I$(ALCHEMY_DIR)/hardware/alchemy-lab/v2/include \
        $(RAMDISK_INC) -Iinclude/fatfs_conf -I$(FATFS) -I$(ALCHEMY_DIR)

# SAN=thread / SAN=address: a sanitizer build in build/<fw>-<san>/
SANFLAGS := $(if $(SAN),-fsanitize=$(SAN) -fno-omit-frame-pointer)
CXXFLAGS := -std=gnu++17 -O2 -g -w $(DEFS) $(INCS) $(SDL_CFLAGS) $(SANFLAGS)
CFLAGS   := -std=gnu11 -O2 -g -w $(DEFS) $(INCS) $(FW_CFLAGS) $(SANFLAGS)

# The SDK's framework, every file except the ones the emulator replaces: the
# USB-CDC HostLink transport (no USB here) and Presets (src/presets_emu.cpp
# compiles it with the flash rebased). The v2 board's own chip drivers are
# replaced by include/alchemy/hw/alchemy_lab_v2.h + src/emu_board.cpp; its
# layout, calibration and trigger-jack code compile as-is.
SDK_REPLACED := framework/src/host_link/cdc_transport.cpp framework/src/host_link/diagnostics.cpp \
                framework/src/surface/presets.cpp
SDK_FRAMEWORK := $(patsubst $(ALCHEMY_DIR)/%,%,$(filter-out $(ALCHEMY_DIR)/framework/src/led/anims/%,\
                   $(sort $(shell find $(ALCHEMY_DIR)/framework/src -name '*.cpp'))))
SDK_V2 := $(filter $(addprefix hardware/alchemy-lab/v2/src/,alchemy_lab_v2_layout.cpp v2_calibration.cpp trigger_jack.cpp),\
            $(patsubst $(ALCHEMY_DIR)/%,%,$(wildcard $(ALCHEMY_DIR)/hardware/alchemy-lab/v2/src/*.cpp)))
SDK_SRC := $(filter-out $(SDK_REPLACED),$(SDK_FRAMEWORK)) $(SDK_V2) $(SDK_RAMDISK)
SDK_ANIMS := $(wildcard $(ALCHEMY_DIR)/framework/src/led/anims/*.cpp)

EMU_CXX := src/emu_board.cpp src/emu_stubs.cpp src/presets_emu.cpp src/emu_main.cpp \
           src/emu_ui.cpp src/emu_script.cpp src/emu_font.cpp src/emu_hostfs.cpp src/emu_palette.cpp src/emu_ui_ctl.cpp $(EMU_RAMDISK) $(FW_STUBS)

ALL_CXX := $(addprefix $(ALCHEMY_DIR)/,$(SDK_SRC)) $(SDK_ANIMS) $(EMU_CXX) $(FW_CXX)
ALL_C   := $(FW_C) $(FATFS)/ff.c $(FATFS)/option/ccsbcs.c
ALL_CC  := $(FW_CC)

objname = $(BUILD)/obj/$(subst /,_,$(subst ..,up,$(1))).o
OBJS := $(foreach f,$(ALL_CXX) $(ALL_C) $(ALL_CC),$(call objname,$(f))) $(BUILD)/obj/firmware.o
HDRS := $(wildcard include/*.h include/*/*.h include/*/*/*.h src/*.h) $(FW_DEPS)

.DEFAULT_GOAL := all
all: $(EXE)

$(EXE): $(OBJS) $(WEB_CARD)
	$(CXX) $(OBJS) $(SDL_LIBS) $(FW_LDFLAGS) $(SANFLAGS) -O2 -o $@

# The firmware's own TU: its main() becomes firmware_main(), ELF sections go.
$(BUILD)/obj/firmware.o: $(FW_MAIN) $(HDRS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(FW_MAIN_FLAGS) -include include/emu_prefix.h -Dmain=firmware_main -c $< -o $@

define CXX_RULE
$(call objname,$(1)): $(1) $(HDRS)
	@mkdir -p $$(dir $$@)
	$$(CXX) $$(CXXFLAGS) -include include/emu_prefix.h -c $$< -o $$@
endef
# per-file extras from fw/<fw>.mk: EMUFLAGS_<file basename>
define C_RULE
$(call objname,$(1)): $(1) $(HDRS)
	@mkdir -p $$(dir $$@)
	$$(CC) $$(CFLAGS) -include include/emu_prefix.h $$(EMUFLAGS_$(notdir $(basename $(1)))) -c $$< -o $$@
endef
define CC_RULE
$(call objname,$(1)): $(1)
	@mkdir -p $$(dir $$@)
	$$(CXX) $$(CXXFLAGS) $$(FW_CC_FLAGS) -c $$< -o $$@
endef
$(foreach f,$(ALL_CXX),$(eval $(call CXX_RULE,$(f))))
$(foreach f,$(ALL_C),$(eval $(call C_RULE,$(f))))
$(foreach f,$(ALL_CC),$(eval $(call CC_RULE,$(f))))

all-fw:
	@for f in $(FWS); do $(MAKE) --no-print-directory FW=$$f || exit 1; done

# Every firmware's scripts, headless, in parallel (real time; see run-tests.sh).
test: all-fw
	./run-tests.sh

clean:
	rm -rf build

.PHONY: all all-fw test clean

# the firmware's source tree, for web/publish.sh's manifest
print-root:
	@echo $(FW_ROOT)
.PHONY: print-root

# Your firmware's tests: the generic smoke test, then every script in
# <FW_DIR>/emu-tests/*.emu (a "#!args ..." line in a script = its CLI options;
# <FW_DIR>/emu-card/ is copied onto the emulated SD card if it exists).
#   make test-custom FW_DIR=~/my-module
test-custom: all
	@echo "testing $(BUILD)/emu"
	@./run-custom-tests.sh "$(BUILD)/emu" "$(FW_DIR)"
.PHONY: test-custom
