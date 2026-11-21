/*
 * A USB-MIDI device on the front USB-C, for a firmware that hosts one
 * (the module's USB host is not emulated): a script's `midi` lines queue
 * USB-MIDI event packets here, and the firmware's own emulator shim
 * (EMU_EXTRA_SRC, e.g. TETHER's emu/tether_emu_midi.cpp) takes them with
 * emu::midi::Pop as its USB host backend would. Packets are
 * cable|CIN, status, data 1, data 2 -- byte 0 in the low byte.
 */
#include <atomic>
#include <deque>
#include <mutex>

#include "emu.h"

namespace
{
std::mutex            g_mu;
std::deque<uint32_t>  g_q;
std::atomic<bool>     g_plugged{false};
std::atomic<uint32_t> g_count{0};
} // namespace

namespace emu { namespace midi {

void Send(uint8_t status, uint8_t d1, uint8_t d2)
{
    const uint8_t cin = status >= 0xF0 ? (status >= 0xF8 ? 0x0F : 0x03) : (uint8_t)(status >> 4);
    std::lock_guard<std::mutex> lk(g_mu);
    g_plugged = true;
    if (g_q.size() < 4096)
        g_q.push_back((uint32_t)cin | (uint32_t)status << 8 | (uint32_t)(d1 & 0x7F) << 16
                      | (uint32_t)(d2 & 0x7F) << 24);
}

void Plug(bool on)
{
    std::lock_guard<std::mutex> lk(g_mu);
    g_plugged = on;
    if (!on) g_q.clear();
}

bool Plugged() { return g_plugged.load(); }

bool Pop(uint32_t* pkt)
{
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_q.empty()) return false;
    *pkt = g_q.front();
    g_q.pop_front();
    g_count++;
    return true;
}

uint32_t Count() { return g_count.load(); }

} } // namespace emu::midi
