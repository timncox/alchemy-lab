/*
 * A USB-MIDI device on the front USB-C, for a firmware that hosts one
 * (the module's USB host is not emulated): a script's `midi` lines queue
 * USB-MIDI event packets here, and the firmware's own emulator shim
 * (EMU_EXTRA_SRC, e.g. TETHER's emu/tether_emu_midi.cpp) takes them with
 * emu::midi::Pop as its USB host backend would. Packets are
 * cable|CIN, status, data 1, data 2 -- byte 0 in the low byte.
 */
#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <deque>
#include <mutex>

#include "emu.h"

namespace
{
std::mutex            g_mu;
std::deque<uint32_t>  g_q;
std::atomic<bool>     g_plugged{false};
std::atomic<uint32_t> g_count{0};
struct Timed { uint64_t frame; uint8_t st, d1, d2; };
std::vector<Timed>    g_timed;   /* sorted by frame */
size_t                g_timed_at = 0;
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

bool LoadFile(const char* path, uint64_t start_frame)
{
    FILE* f = std::fopen(path, "r");
    if (!f) return false;
    std::vector<Timed> v;
    char line[160];
    while (std::fgets(line, sizeof line, f))
    {
        double ms; unsigned st, d1, d2;
        if (line[0] == '#' || std::sscanf(line, "%lf %x %x %x", &ms, &st, &d1, &d2) != 4) continue;
        v.push_back(Timed{start_frame + (uint64_t)(ms * 48.0 + 0.5), (uint8_t)st, (uint8_t)d1, (uint8_t)d2});
    }
    std::fclose(f);
    std::stable_sort(v.begin(), v.end(), [](const Timed& a, const Timed& b) { return a.frame < b.frame; });
    std::lock_guard<std::mutex> lk(g_mu);
    g_timed    = v;
    g_timed_at = 0;
    g_plugged  = true;
    return true;
}

void Due(uint64_t frame)
{
    std::lock_guard<std::mutex> lk(g_mu);
    while (g_timed_at < g_timed.size() && g_timed[g_timed_at].frame <= frame)
    {
        const Timed& t = g_timed[g_timed_at++];
        const uint8_t cin = t.st >= 0xF0 ? (t.st >= 0xF8 ? 0x0F : 0x03) : (uint8_t)(t.st >> 4);
        if (g_q.size() < 4096)
            g_q.push_back((uint32_t)cin | (uint32_t)t.st << 8 | (uint32_t)(t.d1 & 0x7F) << 16 | (uint32_t)(t.d2 & 0x7F) << 24);
    }
}

size_t Pending()
{
    std::lock_guard<std::mutex> lk(g_mu);
    return g_timed.size() - g_timed_at;
}

} } // namespace emu::midi

namespace emu { namespace cvlog {
static FILE* g_f = nullptr;
bool Open(const char* path) { Close(); g_f = std::fopen(path, "w"); return g_f != nullptr; }
void Close() { if (g_f) std::fclose(g_f); g_f = nullptr; }
float OutVolts(int idx);
void Block(uint64_t frame)
{
    if (!g_f) return;
    std::fprintf(g_f, "%llu", (unsigned long long)frame);
    for (int j = 0; j < 8; j++) std::fprintf(g_f, " %.4f", OutVolts(j));
    std::fputc('\n', g_f);
}
} } // namespace emu::cvlog
