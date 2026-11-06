/* usbd_def.h (emulator, belt-phone only): the corner of ST's USB device core
 * midi_usb_transport.cpp touches -- the descriptor table it repoints to name
 * the device. No USB stack behind it (the device is src/emu_hostlink.cpp). */
#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    USBD_SPEED_HIGH = 0U,
    USBD_SPEED_FULL = 1U,
    USBD_SPEED_LOW  = 2U,
} USBD_SpeedTypeDef;

typedef struct
{
    uint8_t* (*GetDeviceDescriptor)(USBD_SpeedTypeDef speed, uint16_t* length);
    uint8_t* (*GetLangIDStrDescriptor)(USBD_SpeedTypeDef speed, uint16_t* length);
    uint8_t* (*GetManufacturerStrDescriptor)(USBD_SpeedTypeDef speed, uint16_t* length);
    uint8_t* (*GetProductStrDescriptor)(USBD_SpeedTypeDef speed, uint16_t* length);
    uint8_t* (*GetSerialStrDescriptor)(USBD_SpeedTypeDef speed, uint16_t* length);
    uint8_t* (*GetConfigurationStrDescriptor)(USBD_SpeedTypeDef speed, uint16_t* length);
    uint8_t* (*GetInterfaceStrDescriptor)(USBD_SpeedTypeDef speed, uint16_t* length);
} USBD_DescriptorsTypeDef;

#ifdef __cplusplus
}
#endif
