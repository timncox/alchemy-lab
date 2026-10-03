/*
 * What every firmware shares and the emulator does not model: the SD
 * firmware picker and HostLink's USB-CDC transport. (USB host and USB audio:
 * stubs_usbhost.cpp, for the firmwares that have them.)
 */
#include <atomic>
#include <string>
#include <cstdio>

#include "picker.h"
#include "emu.h"

/* ---- SD picker: no firmware switching in the emulator ---- */
namespace picker {
void Install(alchemy::Settings&, uint8_t, alchemy::SdCard&, alchemy::AlchemyLab&) {}
bool Busy() { return false; }
} // namespace picker

/* ---- HostLink's USB-CDC transport: no USB in the emulator ---- */
#include "alchemy/host_link/cdc_transport.h"
namespace alchemy { namespace hostlink {
CdcUsbTransport* CdcUsbTransport::s_instance = nullptr;
void   CdcUsbTransport::Init(daisy::UsbHandle&, daisy::UsbHandle::UsbPeriph, const char*) {}
size_t CdcUsbTransport::Read(uint8_t*, size_t) { return 0; }
size_t CdcUsbTransport::Write(const uint8_t*, size_t n) { return n; }
size_t CdcUsbTransport::WriteSpace() const { return kTxRing; }
void   CdcUsbTransport::Pump() {}
void   CdcUsbTransport::RxTrampoline(uint8_t*, uint32_t*) {}
void   CdcUsbTransport::OnRx(const uint8_t*, uint32_t) {}
}} // namespace alchemy::hostlink

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
}}
