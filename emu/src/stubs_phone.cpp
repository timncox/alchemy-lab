/*
 * Belt's Phone USB modes (fw/belt-phone.mk): what is left of the hardware
 * under them once the USB device is src/emu_hostlink.cpp.
 *
 *   - libDaisy's descriptor switch and tables, which midi_usb_transport.cpp
 *     sets to make the device USB MIDI named "Alchemy Lab";
 *   - Phone+Audio's composite device (usb_audio.h): its MIDI interface is the
 *     same emulated device; its audio is not modelled (the phone's stream is
 *     silence, Belt's stream back to the phone goes nowhere);
 *   - the picker's firmware switching from the phone (FW_LIST / FW_FLASH):
 *     the emulator flashes nothing, so it lists no firmware and refuses.
 */
#include <cstdio>
#include <cstring>

#include "hid/usb.h"
#include "usbd_def.h"
#include "usbd_desc.h"
#include "usbd_ctlreq.h"
#include "usb_audio.h"
#include "phone_ext.h"
#include "picker.h"
#include "alchemy/host_link/wire.h"

/* ---- libDaisy / ST USB device core ---- */
extern "C" {
uint8_t                 usbd_mode = 0;   /* 1 = MIDI (midi_usb_transport.cpp) */
USBD_DescriptorsTypeDef FS_Desc   = {};
USBD_DescriptorsTypeDef HS_Desc   = {};

/* ST's: an ASCII string as a USB string descriptor (UTF-16LE). */
void USBD_GetString(uint8_t* desc, uint8_t* unicode, uint16_t* len)
{
    if (!desc || !unicode || !len) return;
    const size_t n = std::strlen((const char*)desc);
    *len = (uint16_t)(n * 2u + 2u);
    unicode[0] = (uint8_t)*len;
    unicode[1] = 0x03u;   /* USB_DESC_TYPE_STRING */
    for (size_t i = 0; i < n; i++)
    {
        unicode[2 + 2 * i]     = desc[i];
        unicode[2 + 2 * i + 1] = 0u;
    }
}
} // extern "C"

/* ---- Phone+Audio: the composite device's MIDI interface ---- */
extern "C" {
void UAC_StartWithMidi(const char*) { emu::usbdev::Attach(); }
void UAC_MidiSetRx(UAC_MidiRxFn fn) { emu::usbdev::SetRxAudio(fn); }
uint8_t UAC_MidiTransmit(const uint8_t* buf, uint16_t len)
{
    return emu::usbdev::Transmit(buf, len) ? 1u : 0u;
}
/* No audio over USB in the emulator: the phone plays silence into Belt. */
void UAC_ReadHost(float** out, size_t frames)
{
    for (size_t i = 0; i < frames; i++) { out[0][i] = 0.f; out[1][i] = 0.f; }
}
void UAC_WriteHost(const float* const*, size_t) {}
}

/* ---- the picker, driven from the phone: nothing to flash here ---- */
namespace {
class NoFirmware : public phone::IFirmwareSwitcher
{
  public:
    int         List(uint32_t) override { return 0; }
    const char* Name(int) const override { return ""; }
    uint32_t    Size(int) const override { return 0; }
    uint8_t     Start(const char*, uint32_t) override
    {
        return (uint8_t)alchemy::hostlink::Status::Unsupported;
    }
    phone::FwState State() const override { return phone::kFwIdle; }
    float          Progress() const override { return 0.f; }
    const char*    Error() const override { return ""; }
};
NoFirmware g_no_firmware;
} // namespace

namespace picker {
phone::IFirmwareSwitcher& Remote() { return g_no_firmware; }
void Service(uint32_t) {}
} // namespace picker
