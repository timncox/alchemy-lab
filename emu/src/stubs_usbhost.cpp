/*
 * The USB host (Launchpad Mini MK3 / Launch Control XL / gamepad) and the USB
 * audio interface of Belt, Mark and Smack: not connected / never started.
 * The gamepad's buttons can be held from a script (emu::PadSetButtons).
 */
#include <atomic>
#include <cstdio>
#include <cstring>

#include "launchpad.h"
#include "usb_audio.h"
#include "emu.h"

/* ---- Launchpad Mini MK3: not connected ---- */
namespace lp {
void    Init() {}
void    Poll(uint32_t) {}
bool    Connected() { return false; }
bool    PopEvent(Event*) { return false; }
void    SetGrid(uint8_t, uint8_t, uint8_t) {}
void    SetSide(uint8_t, uint8_t) {}
void    SetTop(uint8_t, uint8_t) {}
void    SetLogo(uint8_t) {}
void    ClearAll() {}
uint8_t Stage() { return 3; }   /* "running" */
int     Report(char* buf, int cap) { return cap > 0 ? std::snprintf(buf, (size_t)cap, "emulator\n") : 0; }
uint32_t RxCount() { return 0; }
Diag     Diagnostics() { return Diag{}; }
uint32_t TxCount() { return 0; }
} // namespace lp

/* ---- Launch Control XL: not connected ---- */
namespace xl {
bool    Connected() { return false; }
bool    Knob(uint8_t, uint8_t, uint8_t*) { return false; }
bool    Fader(uint8_t, uint8_t*) { return false; }
uint8_t KnobValue(uint8_t, uint8_t) { return 0; }
bool    PopButton(Button*) { return false; }
void    SetKnobLed(uint8_t, uint8_t, uint8_t) {}
void    SetButtonLed(uint8_t, uint8_t, uint8_t) {}
} // namespace xl

/* ---- Gamepad: the emulator panel holds its buttons ---- */
namespace {
std::atomic<uint32_t> g_pad{0};
}
void emu::PadSetButtons(uint32_t mask) { g_pad = mask; }
namespace pad {
bool     Connected() { return true; }
uint32_t Buttons() { return g_pad.load(); }
uint32_t ReportCount() { return 0; }
} // namespace pad

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

