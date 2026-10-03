# Added after a firmware's own Makefile (make -f Makefile -f print-vars.mk) so
# the emulator can ask it what it builds instead of being told by hand. Only
# variables are read; no rule runs, so no ARM toolchain is needed.
.PHONY: emu-print-vars
emu-print-vars:
	@echo 'EMU_TARGET := $(TARGET)'
	@echo 'EMU_ALCHEMY_DIR := $(ALCHEMY_DIR)'
	@echo 'EMU_LIBDAISY_DIR := $(LIBDAISY_DIR)'
	@echo 'EMU_CPP_SOURCES := $(CPP_SOURCES)'
	@echo 'EMU_CC_SOURCES := $(CC_SOURCES)'
	@echo 'EMU_C_SOURCES := $(C_SOURCES)'
	@echo 'EMU_C_INCLUDES := $(C_INCLUDES)'
	@echo 'EMU_C_DEFS := $(C_DEFS)'
