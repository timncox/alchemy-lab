/*
 * The three USB controllers Belt, Mark and Smack drive in Launchpad mode,
 * emulated as devices behind the firmware's own controller API (launchpad.h,
 * shared by copy across the three): a Launchpad Mini MK3, a Launch Control
 * XL and a Haute42 (XInput gamepad). The firmware's mapping and LED code runs
 * unchanged; what is replaced is the USB-MIDI / XInput driver underneath.
 *
 * Connected only with `--usb launchpad` (emu_main writes usb.cfg onto the
 * card so the firmware boots in Launchpad mode, as the module does).
 */
#include <atomic>
#include <cstdio>
#include <cstring>
#include <array>
#include <deque>
#include <mutex>

#include "launchpad.h"
#include "emu.h"

namespace {

std::atomic<bool> g_on{false};
std::mutex        g_mu;

/* Launchpad Mini MK3 */
std::deque<lp::Event> g_lp_q;
std::atomic<uint8_t>  g_lp_grid[8][8];
std::atomic<uint8_t>  g_lp_top[8];
std::atomic<uint8_t>  g_lp_side[8];
std::atomic<uint8_t>  g_lp_logo{0};

/* Launch Control XL */
std::atomic<uint8_t> g_xl_knob[3][8];
std::atomic<bool>    g_xl_knob_dirty[3][8];
std::atomic<uint8_t> g_xl_fader[8];
std::atomic<bool>    g_xl_fader_dirty[8];
std::deque<xl::Button> g_xl_q;
std::atomic<uint8_t> g_xl_knob_led[3][8];
std::atomic<uint8_t> g_xl_btn_led[2][8];

/* Haute42 */
std::atomic<uint32_t> g_pad{0};

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
uint8_t emu::ctl::LpGrid(int x, int y) { return g_lp_grid[y][x].load(); }
uint8_t emu::ctl::LpTop(int x)        { return g_lp_top[x].load(); }
uint8_t emu::ctl::LpSide(int y)       { return g_lp_side[y].load(); }
uint8_t emu::ctl::LpLogo()            { return g_lp_logo.load(); }

void emu::ctl::XlKnob(int row, int col, int v)
{
    g_xl_knob[row][col] = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
    g_xl_knob_dirty[row][col] = true;
}
void emu::ctl::XlFader(int col, int v)
{
    g_xl_fader[col] = (uint8_t)(v < 0 ? 0 : v > 127 ? 127 : v);
    g_xl_fader_dirty[col] = true;
}
uint8_t emu::ctl::XlKnobValue(int row, int col) { return g_xl_knob[row][col].load(); }
uint8_t emu::ctl::XlFaderValue(int col)         { return g_xl_fader[col].load(); }
void emu::ctl::XlButton(int row, int col, bool down)
{
    std::lock_guard<std::mutex> lk(g_mu);
    g_xl_q.push_back(xl::Button{(uint8_t)row, (uint8_t)col, down});
}
uint8_t emu::ctl::XlKnobLed(int row, int col) { return g_xl_knob_led[row][col].load(); }
uint8_t emu::ctl::XlButtonLed(int row, int col) { return g_xl_btn_led[row][col].load(); }

void     emu::ctl::PadSet(uint32_t bit, bool down) { if (down) g_pad |= bit; else g_pad &= ~bit; }
uint32_t emu::ctl::PadMask() { return g_pad.load(); }
void     emu::PadSetButtons(uint32_t mask) { g_pad = mask; }

/* ------------------------------------------------------------ firmware side */

namespace lp {
void Init() {}
void Poll(uint32_t) {}
bool Connected() { return g_on.load(); }
bool PopEvent(Event* e)
{
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_lp_q.empty()) return false;
    *e = g_lp_q.front();
    g_lp_q.pop_front();
    return true;
}
void SetGrid(uint8_t x, uint8_t y, uint8_t col) { if (x < 8 && y < 8) g_lp_grid[y][x] = col; }
void SetSide(uint8_t y, uint8_t col) { if (y < 8) g_lp_side[y] = col; }
void SetTop(uint8_t x, uint8_t col)  { if (x < 8) g_lp_top[x] = col; }
void SetLogo(uint8_t col) { g_lp_logo = col; }
void ClearAll()
{
    for (int y = 0; y < 8; y++)
        for (int x = 0; x < 8; x++) g_lp_grid[y][x] = 0;
    for (int i = 0; i < 8; i++) { g_lp_top[i] = 0; g_lp_side[i] = 0; }
    g_lp_logo = 0;
}
uint8_t  Stage() { return g_on.load() ? 3 : 0; }
Diag     Diagnostics() { return Diag{}; }
int      Report(char* buf, int cap) { return cap > 0 ? std::snprintf(buf, (size_t)cap, "emulator\n") : 0; }
uint32_t RxCount() { return 0; }
uint32_t TxCount() { return 0; }
} // namespace lp

namespace xl {
bool Connected() { return g_on.load(); }
bool Knob(uint8_t row, uint8_t col, uint8_t* v)
{
    if (row >= 3 || col >= 8 || !g_xl_knob_dirty[row][col].exchange(false)) return false;
    *v = g_xl_knob[row][col].load();
    return true;
}
bool Fader(uint8_t col, uint8_t* v)
{
    if (col >= 8 || !g_xl_fader_dirty[col].exchange(false)) return false;
    *v = g_xl_fader[col].load();
    return true;
}
uint8_t KnobValue(uint8_t row, uint8_t col) { return row < 3 && col < 8 ? g_xl_knob[row][col].load() : 0; }
uint8_t FaderValue(uint8_t col) { return col < 8 ? g_xl_fader[col].load() : 0; }
bool PopButton(Button* b)
{
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_xl_q.empty()) return false;
    *b = g_xl_q.front();
    g_xl_q.pop_front();
    return true;
}
void SetKnobLed(uint8_t row, uint8_t col, uint8_t colour) { if (row < 3 && col < 8) g_xl_knob_led[row][col] = colour; }
void SetButtonLed(uint8_t row, uint8_t col, uint8_t colour) { if (row < 2 && col < 8) g_xl_btn_led[row][col] = colour; }
} // namespace xl

namespace pad {
bool     Connected() { return g_on.load(); }
uint32_t Buttons() { return g_pad.load(); }
uint32_t ReportCount() { return 0; }
} // namespace pad

/* A generic USB-MIDI keyboard (belt-alchemy usb-keys, launchpad.h keys::):
 * plugged in with the other controllers (--usb launchpad); a script plays
 * it with `keys on|off <note> [vel]`. Firmwares whose launchpad.h has no
 * keys:: never call these. */
namespace {
std::deque<std::array<uint8_t, 3>> g_keys_q;
bool                               g_keys_down[128];
uint32_t                           g_keys_n = 0, g_keys_drop = 0;
}
namespace keys {
bool Connected() { return g_on.load(); }
bool Held()
{
    std::lock_guard<std::mutex> lk(g_mu);
    for (bool d : g_keys_down) if (d) return true;
    return false;
}
bool PopMsg(uint8_t msg[3])
{
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_keys_q.empty()) return false;
    std::memcpy(msg, g_keys_q.front().data(), 3);
    g_keys_q.pop_front();
    return true;
}
uint32_t MsgCount() { std::lock_guard<std::mutex> lk(g_mu); return g_keys_n; }
uint32_t DropCount() { std::lock_guard<std::mutex> lk(g_mu); return g_keys_drop; }
} // namespace keys

void emu::ctl::KeysNote(int note, int vel)
{
    if (note < 0 || note > 127) return;
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_on.load() || g_keys_q.size() >= 128) { g_keys_drop++; return; }
    g_keys_down[note] = vel > 0;
    g_keys_q.push_back({(uint8_t)(vel > 0 ? 0x90 : 0x80), (uint8_t)note, (uint8_t)(vel > 0 ? vel : 0)});
    g_keys_n++;
}
