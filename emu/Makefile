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
FW  ?= belt
ifeq ($(filter $(FW),$(FWS)),)
$(error FW must be one of: $(FWS))
endif

TIMOS       ?= $(HOME)/tim-os
ALCHEMY_DIR ?= $(TIMOS)/belt-alchemy/lib/alchemy-sdk
LIBDAISY_DIR ?= $(TIMOS)/belt-alchemy/lib/libDaisy
FATFS       := $(LIBDAISY_DIR)/Middlewares/Third_Party/FatFs/src
BUILD       := build/$(FW)$(if $(SAN),-$(SAN))

ifneq ($(filter $(FW),clouds elements marbles meld plaits warps),)
include fw/mi.mk
else
include fw/$(FW).mk
endif

SDL_CFLAGS := $(shell sdl2-config --cflags)
SDL_LIBS   := $(shell sdl2-config --libs)

DEFS := -DALCHEMY_BOARD_V2 -DALCHEMY_HOSTLINK_MAX_BODY=2048 \
        -DLIBDAISY_FFCONF=\"$(abspath $(LIBDAISY_DIR))/src/sys/ffconf.h\" \
        '-DEMU_FW_NAME="$(FW_NAME)"' '-DEMU_LABELS="$(EMU_LABELS)"' '-DEMU_LABELS_ALT="$(EMU_LABELS_ALT)"' \
        $(FW_DEFS)

INCS := -Iinclude -Isrc $(FW_INCS) \
        -I$(ALCHEMY_DIR)/framework/include -I$(ALCHEMY_DIR)/hardware/include \
        -I$(ALCHEMY_DIR)/hardware/alchemy-lab/v2/include \
        -I$(ALCHEMY_DIR)/tests/host -Iinclude/fatfs_conf -I$(FATFS) -I$(ALCHEMY_DIR)

# SAN=thread / SAN=address: a sanitizer build in build/<fw>-<san>/
SANFLAGS := $(if $(SAN),-fsanitize=$(SAN) -fno-omit-frame-pointer)
CXXFLAGS := -std=gnu++17 -O2 -g -w $(DEFS) $(INCS) $(SDL_CFLAGS) $(SANFLAGS)
CFLAGS   := -std=gnu11 -O2 -g -w $(DEFS) $(INCS) $(FW_CFLAGS) $(SANFLAGS)

SDK_SRC := \
  framework/src/surface/control_loop.cpp framework/src/surface/cv_matrix.cpp \
  framework/src/surface/led_render.cpp framework/src/surface/pager.cpp \
  framework/src/surface/preset_gesture_ui.cpp framework/src/surface/settings.cpp \
  framework/src/surface/virtual_knob.cpp framework/src/surface/button_bank.cpp \
  framework/src/led/panel.cpp framework/src/led/ring_frame.cpp framework/src/led/perf_renderer.cpp \
  framework/src/control/param_lock_manager.cpp framework/src/control/musical_clock.cpp \
  framework/src/control/cv_router.cpp framework/src/control/cv_edge.cpp \
  framework/src/control/clock_follower.cpp framework/src/control/clock_pll.cpp \
  framework/src/host_link/host_link.cpp framework/src/host_link/descriptor.cpp \
  framework/src/host_link/describe.cpp framework/src/host_link/json_check.cpp \
  framework/src/host_link/host.cpp \
  framework/src/storage/sd_card.cpp framework/src/storage/fs_extension.cpp \
  hardware/alchemy-lab/v2/src/alchemy_lab_v2_layout.cpp \
  hardware/alchemy-lab/v2/src/v2_calibration.cpp \
  hardware/alchemy-lab/v2/src/trigger_jack.cpp \
  tests/host/ram_diskio.cpp
SDK_ANIMS := $(wildcard $(ALCHEMY_DIR)/framework/src/led/anims/*.cpp)

EMU_CXX := src/emu_board.cpp src/emu_stubs.cpp src/presets_emu.cpp src/emu_main.cpp \
           src/emu_ui.cpp src/emu_script.cpp src/emu_font.cpp src/emu_hostfs.cpp src/emu_palette.cpp src/emu_ui_ctl.cpp src/emu_uart.cpp $(FW_STUBS)

ALL_CXX := $(addprefix $(ALCHEMY_DIR)/,$(SDK_SRC)) $(SDK_ANIMS) $(EMU_CXX) $(FW_CXX)
ALL_C   := $(FW_C) $(FATFS)/ff.c $(FATFS)/option/ccsbcs.c
ALL_CC  := $(FW_CC)

objname = $(BUILD)/obj/$(subst /,_,$(subst ..,up,$(1))).o
OBJS := $(foreach f,$(ALL_CXX) $(ALL_C) $(ALL_CC),$(call objname,$(f))) $(BUILD)/obj/firmware.o
HDRS := $(wildcard include/*.h include/*/*.h include/*/*/*.h src/*.h)

# Which source tree and emulator switches this build is of: when either
# changes (FW_ROOT=<another worktree>), everything rebuilds -- a file's date
# alone would keep objects from the other tree.
BUILD_CONFIG := $(BUILD)/config.txt
BUILD_WANT   := $(FW_MAIN) $(filter -DEMU_%,$(FW_DEFS))
ifneq ($(shell cat $(BUILD_CONFIG) 2>/dev/null),$(strip $(BUILD_WANT)))
$(shell mkdir -p $(BUILD) && echo '$(strip $(BUILD_WANT))' > $(BUILD_CONFIG))
endif
HDRS += $(BUILD_CONFIG)

.DEFAULT_GOAL := all
all: $(BUILD)/emu

$(BUILD)/emu: $(OBJS)
	$(CXX) $(OBJS) $(SDL_LIBS) $(FW_LDFLAGS) $(SANFLAGS) -o $@

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
