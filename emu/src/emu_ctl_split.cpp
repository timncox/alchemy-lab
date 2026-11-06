/*
 * The emulated Launchpad Mini MK3, Launch Control XL and Haute42 for a
 * firmware whose controller layer is split in two (belt-phone: Belt's
 * claude/iphone-screen branch): launchpad_core.cpp, compiled unchanged, keeps
 * the event queues, knob tables, gamepad mask and LED framebuffers that both
 * backends share, plus the remote backend (HostLink CTRL, controllers on the
 * phone's hub); launchpad.cpp is the USB host backend. This file replaces
 * launchpad.cpp: the "USB host" is the emulator panel.
 *
 * Launchpad mode (--usb launchpad): what is clicked on the panel's
 * controllers is handed to the core's queues and tables in lp::Poll(), on
 * the firmware's thread (as the USB host's receive callback would), and the
 * panel draws the core's LED framebuffers. Phone modes: the emulated
 * controllers stay unplugged and undrawn (emu::ctl::Enable(false)); the
 * firmware's controllers are whatever the phone forwards over CTRL.
 *
 * Firmwares with the older, single-file controller layer use emu_ctl.cpp.
 */
#include <atomic>
#include <cstdio>
#include <cstring>
#include <deque>
#include <mutex>

#include "launchpad.h"
#include "launchpad_detail.h"
#include "emu.h"

namespace {

std::atomic<bool> g_on{false};
std::mutex        g_mu;   /* the two event queues: panel thread -> Poll() */

std::deque<lp::Event>  g_lp_q;
std::deque<xl::Button> g_xl_q;
std::atomic<uint8_t>   g_xl_knob[3][8];
std::atomic<bool>      g_xl_knob_dirty[3][8];
std::atomic<uint8_t>   g_xl_fader[8];
std::atomic<bool>      g_xl_fader_dirty[8];
std::atomic<uint32_t>  g_pad{0};
uint32_t               g_pad_sent = 0;   /* firmware thread */

} // namespace

/* ---------------------------------------------------------------- emu side */

void emu::ctl::Enable(bool on)
{
    g_on = on;
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 8; c++) g_xl_knob[r][c] = 64;
    for (int c = 0; c < 8; c++) g_xl_fader[c] = 0;
}
bool emu::ctl::Enabled() { return g_on.load(); }

void emu::ctl::LpEvent(int kind, int x, int y, bool down)
{
    std::lock_guard<std::mutex> lk(g_mu);
    g_lp_q.push_back(lp::Event{(lp::Kind)kind, (uint8_t)x, (uint8_t)y, down});
}
/* The LED framebuffers belong to the firmware (launchpad_core.cpp); the
 * panel reads single bytes of them, as the USB host's flush would. */
uint8_t emu::ctl::LpGrid(int x, int y) { return lpd::lp_want[y * 8 + x]; }
uint8_t emu::ctl::LpTop(int x)        { return lpd::lp_want[lpd::kLpTop0 + x]; }
uint8_t emu::ctl::LpSide(int y)       { return lpd::lp_want[lpd::kLpSide0 + y]; }
uint8_t emu::ctl::LpLogo()            { return lpd::lp_want[lpd::kLpLogo]; }

void emu::ctl::XlKnob(int row, int col, int v)
{
    g_xl_knob[row][col]       = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
    g_xl_knob_dirty[row][col] = true;
}
void emu::ctl::XlFader(int col, int v)
{
    g_xl_fader[col]       = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
    g_xl_fader_dirty[col] = true;
}
uint8_t emu::ctl::XlKnobValue(int row, int col) { return g_xl_knob[row][col].load(); }
uint8_t emu::ctl::XlFaderValue(int col)         { return g_xl_fader[col].load(); }
void emu::ctl::XlButton(int row, int col, bool down)
{
    std::lock_guard<std::mutex> lk(g_mu);
    g_xl_q.push_back(xl::Button{(uint8_t)row, (uint8_t)col, down});
}
uint8_t emu::ctl::XlKnobLed(int row, int col)   { return lpd::xl_want[row * 8 + col]; }
uint8_t emu::ctl::XlButtonLed(int row, int col) { return lpd::xl_want[lpd::kXlKnobs + row * 8 + col]; }

void     emu::ctl::PadSet(uint32_t bit, bool down) { if (down) g_pad |= bit; else g_pad &= ~bit; }
uint32_t emu::ctl::PadMask() { return g_pad.load(); }
void     emu::PadSetButtons(uint32_t mask) { g_pad = mask; }

/* ------------------------------------- firmware side: launchpad.cpp's half */

namespace lpd {
bool UsbLpConnected()  { return g_on.load(); }
bool UsbXlConnected()  { return g_on.load(); }
bool UsbPadConnected() { return g_on.load(); }
} // namespace lpd

namespace lp {
void Init() {}

/* The USB host's work, done from the panel: incoming controls into the
 * core's queues and tables (LEDs need no flush: the panel reads them). */
void Poll(uint32_t)
{
    if (!g_on.load()) return;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        for (const lp::Event& e : g_lp_q) lpd::LpPush(e.kind, e.x, e.y, e.down);
        g_lp_q.clear();
        for (const xl::Button& b : g_xl_q) lpd::XlPush(b.row, b.col, b.down);
        g_xl_q.clear();
    }
    for (int r = 0; r < 3; r++)
        for (int c = 0; c < 8; c++)
            if (g_xl_knob_dirty[r][c].exchange(false)) lpd::XlKnob(r * 8 + c, g_xl_knob[r][c].load());
    for (int c = 0; c < 8; c++)
        if (g_xl_fader_dirty[c].exchange(false)) lpd::XlFader(c, g_xl_fader[c].load());
    const uint32_t pad = g_pad.load();
    if (pad != g_pad_sent)
    {
        lpd::pad_buttons = pad;
        lpd::pad_reports++;
        g_pad_sent = pad;
    }
}

uint8_t  Stage() { return g_on.load() ? 3 : 0; }
Diag     Diagnostics() { return Diag{}; }
int      Report(char* buf, int cap) { return cap > 0 ? std::snprintf(buf, (size_t)cap, "emulator\n") : 0; }
uint32_t RxCount() { return 0; }
uint32_t TxCount() { return 0; }
} // namespace lp
