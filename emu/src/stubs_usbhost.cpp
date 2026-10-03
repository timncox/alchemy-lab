/*
 * The USB audio interface of Belt, Mark and Smack: never started. (Their USB
 * host controllers are emulated in emu_ctl.cpp.)
 */
#include <atomic>
#include <cstdio>
#include <cstring>

#include "launchpad.h"
#include "usb_audio.h"
#include "emu.h"

/* Launchpad / Launch Control XL / gamepad: emu_ctl.cpp (emulated devices). */

/* ---- USB audio interface: never started ---- */
extern "C" {
void    UAC_Start(const char*) {}
void    UAC_Process(const float* const* in, float** out, size_t frames)
{
    for (size_t i = 0; i < frames; i++) { out[0][i] = in[0][i]; out[1][i] = in[1][i]; }
}
uint8_t UAC_State(void) { return 0; }
uint8_t UAC_RebootRequested(void) { return 0; }
UAC_Info UAC_GetInfo(void) { UAC_Info i; memset(&i, 0, sizeof i); return i; }
}

