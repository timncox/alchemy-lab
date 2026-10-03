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
