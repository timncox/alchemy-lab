/* hid/usb.h (emulator): the type the firmwares name; nothing behind it. */
#pragma once
#include <cstddef>
#include <cstdint>

namespace daisy {
class UsbHandle
{
  public:
    enum UsbPeriph { FS_INTERNAL, FS_EXTERNAL, FS_BOTH };
    enum class Result { OK, ERR };
    typedef void (*ReceiveCallback)(uint8_t* buff, uint32_t* len);
    void   Init(UsbPeriph) {}
    void   DeInit(UsbPeriph) {}
    Result TransmitInternal(uint8_t*, size_t) { return Result::OK; }
    Result TransmitExternal(uint8_t*, size_t) { return Result::OK; }
    void   SetReceiveCallback(ReceiveCallback, UsbPeriph) {}
};
} // namespace daisy
