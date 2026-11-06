/* usbd_ctlreq.h (emulator, belt-phone only): ST's string-descriptor helper
 * (defined in src/stubs_phone.cpp). */
#pragma once
#include "usbd_def.h"

#ifdef __cplusplus
extern "C" {
#endif
void USBD_GetString(uint8_t* desc, uint8_t* unicode, uint16_t* len);
#ifdef __cplusplus
}
#endif
