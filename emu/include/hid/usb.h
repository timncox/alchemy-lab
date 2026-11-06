/* hid/usb.h (emulator): libDaisy's USB device handle, as the firmwares name it.
 *
 * Behind it is one emulated device port (src/emu_hostlink.cpp): what the
 * firmware transmits (USB-MIDI event packets) goes to the --hostlink TCP
 * client as plain MIDI bytes, and what the client sends comes back to the
 * firmware's receive callback as USB-MIDI packets, on the firmware's own
 * thread (as the USB interrupt cuts into its main loop on the module).
 * Without --hostlink nothing is connected: transmits are accepted and
 * dropped and nothing is received, as inert as before. */
#pragma once
#include <cstddef>
#include <cstdint>

namespace emu {
namespace usbdev {
/* The firmware started (Init) or stopped the device. */
void Attach();
void Detach();
/* libDaisy's receive callback (UsbHandle), or usb_audio.h's (Phone+Audio). */
void SetRx(void (*fn)(uint8_t* buf, uint32_t* len));
void SetRxAudio(void (*fn)(const uint8_t* buf, uint32_t len));
/* USB-MIDI event packets out. False = busy, try again (the client is slow). */
bool Transmit(const uint8_t* packets, size_t len);
} // namespace usbdev
} // namespace emu

namespace daisy {
class UsbHandle
{
  public:
    enum UsbPeriph { FS_INTERNAL, FS_EXTERNAL, FS_BOTH };
    enum class Result { OK, ERR };
    typedef void (*ReceiveCallback)(uint8_t* buff, uint32_t* len);
    void   Init(UsbPeriph) { emu::usbdev::Attach(); }
    void   DeInit(UsbPeriph) { emu::usbdev::Detach(); }
    Result TransmitInternal(uint8_t* b, size_t n) { return emu::usbdev::Transmit(b, n) ? Result::OK : Result::ERR; }
    Result TransmitExternal(uint8_t* b, size_t n) { return emu::usbdev::Transmit(b, n) ? Result::OK : Result::ERR; }
    void   SetReceiveCallback(ReceiveCallback cb, UsbPeriph) { emu::usbdev::SetRx(cb); }
};
} // namespace daisy
