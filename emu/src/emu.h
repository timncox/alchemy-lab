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
void               WriteCardFile(const char* dir, const char* path, const char* text);

/* The firmware's OLED lines, if it has a screen (stubs_screen.cpp; a weak
 * default in emu_stubs.cpp returns false). */
bool ScreenLines(std::string* small_line, std::string* big_line);

/* LED frames the firmware has Show()n (a stalled control loop stops it). */
uint32_t ShowCount();

/* Launchpad / XL / gamepad emulation hooks (emu_ctl.cpp). */
void PadSetButtons(uint32_t mask);
namespace ctl {
void    Enable(bool on);          /* --usb launchpad */
bool    Enabled();
/* Launchpad Mini MK3: kind 0 grid (x 0-7, y 0-7 top-down), 1 side (y), 2 top (x) */
void    LpEvent(int kind, int x, int y, bool down);
uint8_t LpGrid(int x, int y);
uint8_t LpTop(int x);
uint8_t LpSide(int y);
uint8_t LpLogo();
/* Launch Control XL: knobs 3 x 8, faders 8, buttons 2 x 8 */
void    XlKnob(int row, int col, int v);
void    XlFader(int col, int v);
uint8_t XlKnobValue(int row, int col);
uint8_t XlFaderValue(int col);
void    XlButton(int row, int col, bool down);
uint8_t XlKnobLed(int row, int col);
uint8_t XlButtonLed(int row, int col);
/* Haute42: XInput bits (launchpad.h pad::k*) */
void     PadSet(uint32_t bit, bool down);
uint32_t PadMask();
} // namespace ctl
void LpRgb(uint8_t idx, uint8_t* r, uint8_t* g, uint8_t* b);   /* emu_palette.cpp */
void XlRgb(uint8_t col, uint8_t* r, uint8_t* g, uint8_t* b);

} // namespace emu

/* The firmware's own main(), renamed at compile time (-Dmain=firmware_main). */
int firmware_main(void);
