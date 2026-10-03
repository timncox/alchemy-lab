/*
 * The SDK's real Presets, with the one hardware address rebased: the preset
 * store reads flash by dereferencing kPresetFlashBase (0x90760000, the
 * Seed's memory-mapped QSPI). Here it points at the emulator's flash image.
 * The layout header is included first so its own constexpr definition is
 * untouched; the macro only rewrites the use in presets.cpp.
 */
#include <cstdint>
#include "alchemy/hw/alchemy_lab_v2_layout.h"

namespace emu { uintptr_t PresetFlashBase(); }
#define kPresetFlashBase (::emu::PresetFlashBase())

#include "framework/src/surface/presets.cpp"   /* -I$(ALCHEMY_DIR) */
