/*
 * USB audio on the front USB-C, for a firmware whose port is a USB audio
 * device (TETHER 0.5.0): the far end, a computer or the FM-1 (script `midi
 * plug computer`), plays a stream to the firmware and listens to one back.
 * The module's USB stack is not emulated: the firmware's emulator shim
 * (EMU_EXTRA_SRC, e.g. TETHER's emu/tether_emu_midi.cpp) asks OutFrames for
 * the host's frames as its iso OUT packets would carry them, and hands what
 * its iso IN packets carried to InFrames. 16-bit stereo, L in the low half,
 * at the host's 44.1 kHz (the shim keeps the host's packet clock).
 */
#include <atomic>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <mutex>

#include "emu.h"

namespace
{
std::mutex          g_mu;
std::atomic<bool>   g_out_on{false}, g_in_on{false};
double              g_hz = 0.0, g_ph = 0.0, g_ppm = 0.0;
int                 g_chans = 3;     /* 1 left, 2 right, 3 both */
float               g_level = 0.5f;
constexpr size_t    kTap = 16384;
float               g_in_l[kTap], g_in_r[kTap];
size_t              g_in_w = 0;
std::atomic<uint32_t> g_in_frames{0}, g_out_frames{0};
} // namespace

namespace emu { namespace usbaudio {

void Out(double hz, int chans, float level, double ppm)
{
    std::lock_guard<std::mutex> lk(g_mu);
    g_hz    = hz;
    g_chans = chans;
    g_level = level;
    g_ppm   = ppm;
    g_out_on = true;
}
void OutOff() { g_out_on = false; }
bool OutOn() { return g_out_on.load(); }
double OutPpm()
{
    std::lock_guard<std::mutex> lk(g_mu);
    return g_ppm;
}

void OutFrames(int16_t* lr, int frames)
{
    std::lock_guard<std::mutex> lk(g_mu);
    for (int i = 0; i < frames; i++)
    {
        const double x = g_hz > 0.0 ? g_level * std::sin(2.0 * M_PI * g_ph) : 0.0;
        g_ph += g_hz / 44100.0;
        if (g_ph >= 1.0) g_ph -= 1.0;
        const int16_t s = (int16_t)std::lrint(x * 32767.0);
        lr[2 * i]     = (g_chans & 1) ? s : 0;
        lr[2 * i + 1] = (g_chans & 2) ? s : 0;
    }
    g_out_frames += (uint32_t)frames;
}

void In(bool on) { g_in_on = on; }
bool InOn() { return g_in_on.load(); }

void InFrames(const int16_t* lr, int frames)
{
    std::lock_guard<std::mutex> lk(g_mu);
    for (int i = 0; i < frames; i++)
    {
        g_in_l[g_in_w & (kTap - 1)] = lr[2 * i] / 32768.0f;
        g_in_r[g_in_w & (kTap - 1)] = lr[2 * i + 1] / 32768.0f;
        g_in_w++;
    }
    g_in_frames += (uint32_t)frames;
}

/* Amplitude at hz in the last 8192 frames the host heard (L/R average,
 * Goertzel, Hann-windowed), as ToneLevel measures the output. */
float InTone(float hz)
{
    std::lock_guard<std::mutex> lk(g_mu);
    const size_t n = 8192, end = g_in_w;
    if (end < n) return 0.0f;
    const double k = 2.0 * M_PI * hz / 44100.0, coeff = 2.0 * std::cos(k);
    double s1 = 0, s2 = 0, wsum = 0;
    for (size_t i = 0; i < n; i++)
    {
        const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (n - 1));
        const size_t at = (end - n + i) & (kTap - 1);
        const double x = 0.5 * (g_in_l[at] + g_in_r[at]) * w;
        const double s0 = x + coeff * s1 - s2;
        s2 = s1; s1 = s0; wsum += w;
    }
    const double pw = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return (float)(2.0 * std::sqrt(pw > 0 ? pw : 0) / wsum);
}

float InRms()
{
    std::lock_guard<std::mutex> lk(g_mu);
    const size_t n = 8192, end = g_in_w;
    if (end < n) return 0.0f;
    double a = 0;
    for (size_t i = 0; i < n; i++)
    {
        const size_t at = (end - n + i) & (kTap - 1);
        a += 0.5 * (g_in_l[at] * g_in_l[at] + g_in_r[at] * g_in_r[at]);
    }
    return (float)std::sqrt(a / n);
}

uint32_t InCount() { return g_in_frames.load(); }
uint32_t OutCount() { return g_out_frames.load(); }

} } // namespace emu::usbaudio
