/* usbd_desc.h (emulator, belt-phone only): libDaisy's two descriptor tables
 * (defined in src/stubs_phone.cpp). */
#pragma once
#include "usbd_def.h"

#ifdef __cplusplus
extern "C" {
#endif
extern USBD_DescriptorsTypeDef FS_Desc;
extern USBD_DescriptorsTypeDef HS_Desc;
#ifdef __cplusplus
}
#endif
