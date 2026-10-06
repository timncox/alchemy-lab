/*
 * What every firmware shares and the emulator does not model: the SD
 * firmware picker. (HostLink's USB-CDC transport: emu_hostlink.cpp. USB host
 * and USB audio: stubs_usbhost.cpp, for the firmwares that have them.)
 */
#include <atomic>
#include <string>
#include <cstdio>

#include "emu.h"

/* ---- SD picker (Tim's firmwares share one): no switching in the emulator.
 * Other firmware has no picker.h, and needs no stub. ---- */
#if __has_include("picker.h")
#include "picker.h"
namespace picker {
void Install(alchemy::Settings&, uint8_t, alchemy::SdCard&, alchemy::AlchemyLab&) {}
bool Busy() { return false; }
} // namespace picker
#endif


/* No screen unless the firmware links stubs_screen.cpp. */
__attribute__((weak)) bool emu::ScreenLines(std::string*, std::string*) { return false; }

/* Firmwares without controllers (the MI ports) link no emu_ctl.cpp. */
#define EMU_WEAK __attribute__((weak))
namespace emu {
EMU_WEAK void PadSetButtons(uint32_t) {}
namespace ctl {
EMU_WEAK void     Enable(bool) {}
EMU_WEAK bool     Enabled() { return false; }
EMU_WEAK void     LpEvent(int, int, int, bool) {}
EMU_WEAK uint8_t  LpGrid(int, int) { return 0; }
EMU_WEAK uint8_t  LpTop(int) { return 0; }
EMU_WEAK uint8_t  LpSide(int) { return 0; }
EMU_WEAK uint8_t  LpLogo() { return 0; }
EMU_WEAK void     XlKnob(int, int, int) {}
EMU_WEAK void     XlFader(int, int) {}
EMU_WEAK uint8_t  XlKnobValue(int, int) { return 0; }
EMU_WEAK uint8_t  XlFaderValue(int) { return 0; }
EMU_WEAK void     XlButton(int, int, bool) {}
EMU_WEAK uint8_t  XlKnobLed(int, int) { return 0; }
EMU_WEAK uint8_t  XlButtonLed(int, int) { return 0; }
EMU_WEAK void     PadSet(uint32_t, bool) {}
EMU_WEAK uint32_t PadMask() { return 0; }
EMU_WEAK void     KeysNote(int, int) {}
}}
