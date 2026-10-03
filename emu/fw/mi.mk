# The six Mutable Instruments ports (mi-alchemy), FW = clouds | elements |
# marbles | meld | plaits | warps. Port branch 42c5b50 = the v0.1.0 release.
#
# Each port's own src/<fw>/fw.mk is included as-is so the engine source list
# can't drift; its repo-relative paths are resolved against FW_ROOT (vpath for
# rule prerequisites, abs_mi for the lists).
FW_ROOT  ?= $(TIMOS)/mi-alchemy/.claude/worktrees/port
EURORACK := $(FW_ROOT)/vendor/eurorack
BUILD_DIR := $(BUILD)
CPP_SOURCES :=
CC_SOURCES := $(EURORACK)/stmlib/dsp/units.cc $(EURORACK)/stmlib/dsp/atan.cc $(EURORACK)/stmlib/utils/random.cc
C_INCLUDES :=
FW_CC_OPT  := -O3
include $(FW_ROOT)/src/$(FW)/fw.mk
VPATH := $(FW_ROOT)

abs_mi = $(foreach f,$(1),$(if $(filter /%,$(f)),$(f),$(if $(filter $(BUILD)/%,$(f)),$(f),$(FW_ROOT)/$(f))))
inc_mi = $(foreach i,$(1),$(if $(filter -I/%,$(i)),$(i),-I$(FW_ROOT)/$(patsubst -I%,%,$(i))))

FW_MAIN   := $(FW_ROOT)/src/$(FW)/$(FW)_alchemy.cpp
FW_CXX    := $(call abs_mi,$(CPP_SOURCES))   # picker: emu_stubs.cpp
FW_C      :=
FW_CC     := $(call abs_mi,$(CC_SOURCES))
FW_INCS   := -I$(FW_ROOT)/src/common -I$(FW_ROOT)/src/$(FW) -I$(FW_ROOT)/src/shim -I$(EURORACK) \
             $(call inc_mi,$(C_INCLUDES))
FW_DEFS   := -DTEST -DMI_VERSION=\"emu\" -DMI_GIT_HASH=\"$(shell git -C $(FW_ROOT) rev-parse --short HEAD)\"
FW_CC_FLAGS := $(FW_CC_OPT)
FW_STUBS  :=

FW_NAME_clouds   := Clouds
FW_NAME_elements := Elements
FW_NAME_marbles  := Marbles
FW_NAME_meld     := Meld
FW_NAME_plaits   := Plaits
FW_NAME_warps    := Warps
FW_NAME := $(FW_NAME_$(FW))

L_clouds   := POSITION|SIZE|PITCH|DENSITY|TEXTURE|BLEND
A_clouds   := MODE|QUALITY|SPREAD|FEEDBACK|REVERB|IN GAIN
L_elements := COARSE|FINE|GEOMETRY|BRIGHTNESS|DAMPING|POSITION
A_elements := BOW|BLOW|STRIKE|CONTOUR|FLOW|MALLET
L_marbles  := RATE|T BIAS|T JITTER|DEJA VU|X SPREAD|X BIAS
A_marbles  := X STEPS|LENGTH|T MODE|X MODE|X RANGE|SCALE
L_meld     := ALGORITHM|TIMBRE|LEVEL 1|LEVEL 2|MODE|CARRIER
A_meld     := IN GAIN|TUNE|CV 2|P4|P5|P6
L_plaits   := MODEL|FREQUENCY|HARMONICS|TIMBRE|MORPH|DECAY
A_plaits   := OCTAVE|FINE|FM|TIMBRE ATT|MORPH ATT|LPG COLOUR
L_warps    := ALGORITHM|TIMBRE|LEVEL 1|LEVEL 2|MODE|CARRIER
A_warps    := IN GAIN|TUNE|P3|P4|P5|P6
EMU_LABELS     := $(L_$(FW))
EMU_LABELS_ALT := $(A_$(FW))

# Elements: its fw.mk builds the card's sample file with a host tool, with
# repo-relative paths; the same recipe with absolute ones (this definition
# wins), and the file staged where the emulated card is filled from.
ifeq ($(FW),elements)
# elements_storage.cpp names the sample arrays with ELF asm labels
# ("_ZN8elements15smp_sample_dataE"); Mach-O C symbols carry one more leading
# underscore, so the engine's references want "__ZN8...". Alias them.
FW_LDFLAGS := -Wl,-alias,_ZN8elements15smp_sample_dataE,__ZN8elements15smp_sample_dataE \
              -Wl,-alias,_ZN8elements16smp_noise_sampleE,__ZN8elements16smp_noise_sampleE
$(BUILD)/elements_samples_tool: $(FW_ROOT)/tools/elements_samples.cpp $(FW_ROOT)/src/elements/elements_samples.cpp
	@mkdir -p $(dir $@)
	c++ -std=gnu++14 -O1 -DTEST -I$(EURORACK) -I$(FW_ROOT)/src/elements $(FW_ROOT)/tools/elements_samples.cpp $(FW_ROOT)/src/elements/elements_samples.cpp -o $@
$(BUILD)/card/mi/elements.smp: $(BUILD)/elements.smp
	@mkdir -p $(dir $@)
	cp $< $@
all: $(BUILD)/card/mi/elements.smp
endif
