/* Emulator-internal interfaces shared by emu_board / emu_main / emu_ui. */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace emu {

/* Where the QSPI flash image (presets) is kept between runs. Empty = none. */
const std::string& FlashPath();
void               SetFlashPath(const std::string& p);
void               LoadFlash();
uintptr_t          PresetFlashBase();
void               FormatCard();
void               FillCard(const std::string& host_dir);

/* The firmware's OLED lines, if it has a screen (stubs_screen.cpp; a weak
 * default in emu_stubs.cpp returns false). */
bool ScreenLines(std::string* small_line, std::string* big_line);

/* LED frames the firmware has Show()n (a stalled control loop stops it). */
uint32_t ShowCount();

/* Launchpad / XL / gamepad emulation hooks (stubs_usbhost.cpp). */
void PadSetButtons(uint32_t mask);

} // namespace emu

/* The firmware's own main(), renamed at compile time (-Dmain=firmware_main). */
int firmware_main(void);
