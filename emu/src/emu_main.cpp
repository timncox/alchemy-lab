/*
 * alchemy-emu: an Alchemy Lab firmware on an emulated module (build/<fw>/emu).
 *
 *   belt-emu                       window; Mac mic in, default output out
 *   belt-emu --in voice.wav        a WAV file (looped) instead of the mic
 *   belt-emu --record out.wav      also write what J9/J10 put out
 *   belt-emu --headless --script t.emu [--in x.wav] [--record y.wav]
 *                                  no window, no sound card: run a script of
 *                                  panel actions and checks, exit 0 / 1
 *   belt-emu --usb launchpad      boot in Launchpad mode with the emulated Launchpad
 *                                 Mini MK3, Launch Control XL and Haute42 plugged in
 *   belt-emu --card dir           copy a host folder onto the emulated SD card
 *   belt-emu --flash path.bin      where presets persist (default
 *                                  ~/.alchemy-emu/belt-flash.bin; "none" = RAM)
 *
 * Threads, as on the device: the firmware's main() (its control loop) runs on
 * its own thread; the audio callback runs on the sound card's thread (or a
 * real-time-paced driver thread when headless); the window / script is the
 * main thread and only touches the panel state and reads the LEDs.
 */
#include <SDL.h>

#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

#include "alchemy/hw/alchemy_lab_v2.h"
#include "emu.h"
#include "emu_audio.h"

namespace {
std::string g_flash_path;
}

const std::string& emu::FlashPath() { return g_flash_path; }
void               emu::SetFlashPath(const std::string& p) { g_flash_path = p; }

/* ---------------------------------------------------------------- audio -- */

namespace emu {

static std::vector<float> g_wav;          /* mono input file, looped */
static size_t             g_wav_pos = 0;
static std::mutex         g_in_mu;
static std::vector<float> g_mic;          /* capture ring (mono) */
static size_t             g_mic_r = 0, g_mic_w = 0;
static std::atomic<float> g_in_level{0.f}, g_out_level{0.f};
static FILE*              g_rec = nullptr;
static uint32_t           g_rec_frames = 0;
static std::atomic<int>   g_synth_hz{0};  /* script: a sung tone instead */
static double             g_synth_ph = 0.0;
static std::atomic<float> g_out_rms_acc{0.f};
/* the last 16384 output samples (mono), for pitch checks */
static float              g_tap[16384];
static float              g_tap_l[16384], g_tap_r[16384];
static std::atomic<size_t> g_tap_w{0};
static std::atomic<uint32_t> g_out_rms_n{0};
static std::atomic<uint32_t> g_bad{0};        /* NaN / inf output samples */
static std::atomic<uint32_t> g_pinned{0};     /* samples at full scale */
static std::atomic<uint64_t> g_frames{0};

/* Amplitude of a sinusoid at hz in the last 8192 output samples (Goertzel,
 * Hann-windowed): about 0.5 for a full-scale-0.5 sine, ~0 when absent. */
float ToneLevel(float hz)
{
    const size_t n = 8192, end = g_tap_w.load();
    const double k = 2.0 * M_PI * hz / 48000.0, coeff = 2.0 * std::cos(k);
    double s1 = 0, s2 = 0, wsum = 0;
    for (size_t i = 0; i < n; i++)
    {
        const double w = 0.5 - 0.5 * std::cos(2.0 * M_PI * i / (n - 1));
        const double x = g_tap[(end - n + i) & 16383u] * w;
        const double s0 = x + coeff * s1 - s2;
        s2 = s1; s1 = s0; wsum += w;
    }
    const double pw = s1 * s1 + s2 * s2 - coeff * s1 * s2;
    return (float)(2.0 * std::sqrt(pw > 0 ? pw : 0) / wsum);
}

void OutputHealth(uint32_t* bad, uint32_t* pinned, uint64_t* frames)
{
    *bad = g_bad.load(); *pinned = g_pinned.load(); *frames = g_frames.load();
}

/* RMS of the left and right outputs over the last 8192 samples. */
void LrRms(float* l, float* r)
{
    const size_t n = 8192, end = g_tap_w.load();
    double a = 0, b = 0;
    for (size_t i = 0; i < n; i++)
    {
        const float x = g_tap_l[(end - n + i) & 16383u], y = g_tap_r[(end - n + i) & 16383u];
        a += x * x; b += y * y;
    }
    *l = (float)std::sqrt(a / n);
    *r = (float)std::sqrt(b / n);
}

float InLevel()  { return g_in_level.load(); }
float OutLevel() { return g_out_level.load(); }
void  SetSynthHz(int hz) { g_synth_hz = hz; }

/* Running output meter for script checks: mean square since the last read. */
float TakeOutRms()
{
    const uint32_t n = g_out_rms_n.exchange(0);
    const float    a = g_out_rms_acc.exchange(0.f);
    return n ? std::sqrt(a / (float)n) : 0.f;
}

static float next_input_sample()
{
    const int hz = g_synth_hz.load();
    if (hz > 0)
    {
        /* a band-limited buzz, the rough spectrum of a sung vowel (the same
         * stimulus as the engine's sim tests: a sine has no harmonics for
         * the re-pitched voices to sound with) */
        g_synth_ph += (double)hz / 48000.0;
        if (g_synth_ph >= 1.0) g_synth_ph -= 1.0;
        double x = 0.0, norm = 0.0;
        const int nh = std::min(30, (int)(18000 / hz));
        for (int k = 1; k <= nh; k++) { x += std::sin(g_synth_ph * 2.0 * M_PI * k) / k; norm += 1.0 / k; }
        return (float)(0.6 * x / norm);
    }
    if (!g_wav.empty())
    {
        const float s = g_wav[g_wav_pos++];
        if (g_wav_pos >= g_wav.size()) g_wav_pos = 0;
        return s;
    }
    std::lock_guard<std::mutex> lk(g_in_mu);
    if (g_mic.empty() || g_mic_r == g_mic_w) return 0.f;
    const float s = g_mic[g_mic_r];
    g_mic_r = (g_mic_r + 1) % g_mic.size();
    return s;
}

/* J1 / J2 pulses: a train every period (0 = off) plus one-shots, 5 ms at
 * full scale (+5 V), counted in samples so they land on the sample. */
static std::atomic<int32_t> g_clk_period[2];   /* samples, 0 = off */
static std::atomic<int32_t> g_pulse_req[2];
static int32_t              g_clk_phase[2], g_pulse_left[2];
void SetClock(int jack, float period_ms)
{
    if (jack < 0 || jack > 1) return;
    g_clk_period[jack] = period_ms > 0.f ? (int32_t)(period_ms * 48.0f) : 0;
    g_clk_phase[jack]  = 0;
}
void  Pulse(int jack) { if (jack >= 0 && jack < 2) g_pulse_req[jack] = 1; }
float PulseSample(int jack)
{
    const int32_t per = g_clk_period[jack].load();
    if (g_pulse_req[jack].exchange(0)) g_pulse_left[jack] = 240;
    if (per > 0)
    {
        if (g_clk_phase[jack] == 0) g_pulse_left[jack] = 240;
        g_clk_phase[jack] = (g_clk_phase[jack] + 1) % per;
    }
    if (g_pulse_left[jack] > 0) { g_pulse_left[jack]--; return 1.0f; }
    return 0.0f;
}

/* Feed `frames` stereo frames through the firmware, `out` interleaved. */
void Render(float* out, size_t frames)
{
    auto cb = AudioCb();
    const size_t bs = BlockSize();
    static std::vector<float> il, ir, ol, orr;
    if (il.size() < bs) { il.resize(bs); ir.resize(bs); ol.resize(bs); orr.resize(bs); }
    float in_pk = 0.f, out_pk = 0.f, acc = 0.f;
    size_t done = 0;
    while (done < frames)
    {
        const size_t n = std::min(bs, frames - done);
        for (size_t i = 0; i < n; i++)
        {
            const float s = next_input_sample();
            il[i] = s + PulseSample(0);
            ir[i] = s + PulseSample(1);
            in_pk = std::max(in_pk, std::fabs(s));
        }
        if (cb && n == bs)
        {
            const float* ins[2] = {il.data(), ir.data()};
            float*       outs[2] = {ol.data(), orr.data()};
            cb(ins, outs, bs);
        }
        else
        {
            for (size_t i = 0; i < n; i++) { ol[i] = 0.f; orr[i] = 0.f; }
        }
        for (size_t i = 0; i < n; i++)
        {
            out[(done + i) * 2]     = ol[i];
            out[(done + i) * 2 + 1] = orr[i];
            if (!std::isfinite(ol[i]) || !std::isfinite(orr[i])) { g_bad++; ol[i] = orr[i] = 0.f; out[(done + i) * 2] = out[(done + i) * 2 + 1] = 0.f; }
            if (std::fabs(ol[i]) >= 0.999f || std::fabs(orr[i]) >= 0.999f) g_pinned++;
            out_pk = std::max(out_pk, std::max(std::fabs(ol[i]), std::fabs(orr[i])));
            const float m = 0.5f * (ol[i] + orr[i]);
            acc += m * m;
            const size_t w = g_tap_w.load();
            g_tap[w & 16383u] = m;
            g_tap_l[w & 16383u] = ol[i];
            g_tap_r[w & 16383u] = orr[i];
            g_tap_w = w + 1;
        }
        done += n;
    }
    g_frames += frames;
    g_in_level  = std::max(in_pk, g_in_level.load() * 0.9f);
    g_out_level = std::max(out_pk, g_out_level.load() * 0.9f);
    g_out_rms_acc = g_out_rms_acc.load() + acc;
    g_out_rms_n   = g_out_rms_n.load() + (uint32_t)frames;
    if (g_rec)
    {
        for (size_t i = 0; i < frames * 2; i++)
        {
            float v = out[i] < -1.f ? -1.f : (out[i] > 1.f ? 1.f : out[i]);
            int16_t s = (int16_t)(v * 32767.f);
            std::fwrite(&s, 2, 1, g_rec);
        }
        g_rec_frames += (uint32_t)frames;
    }
}

static void wav_header(FILE* f, uint32_t frames)
{
    const uint32_t data = frames * 4u, rate = 48000u, byte_rate = rate * 4u;
    const uint16_t fmt = 1, ch = 2, align = 4, bits = 16;
    const uint32_t riff = 36u + data, sixteen = 16u;
    std::fseek(f, 0, SEEK_SET);
    std::fwrite("RIFF", 1, 4, f); std::fwrite(&riff, 4, 1, f); std::fwrite("WAVEfmt ", 1, 8, f);
    std::fwrite(&sixteen, 4, 1, f); std::fwrite(&fmt, 2, 1, f); std::fwrite(&ch, 2, 1, f);
    std::fwrite(&rate, 4, 1, f); std::fwrite(&byte_rate, 4, 1, f); std::fwrite(&align, 2, 1, f);
    std::fwrite(&bits, 2, 1, f); std::fwrite("data", 1, 4, f); std::fwrite(&data, 4, 1, f);
}

bool OpenRecord(const char* path)
{
    g_rec = std::fopen(path, "wb");
    if (!g_rec) return false;
    wav_header(g_rec, 0);
    return true;
}

void CloseRecord()
{
    if (!g_rec) return;
    wav_header(g_rec, g_rec_frames);
    std::fclose(g_rec);
    g_rec = nullptr;
}

/* 16-bit PCM WAV (any rate is assumed 48 kHz; mono or the left channel). */
bool LoadWav(const char* path)
{
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char     id[4];
    uint32_t sz;
    uint16_t ch = 1, bits = 16;
    std::fseek(f, 12, SEEK_SET);
    while (std::fread(id, 1, 4, f) == 4 && std::fread(&sz, 4, 1, f) == 1)
    {
        if (!std::memcmp(id, "fmt ", 4))
        {
            uint16_t fmt; uint32_t rate, br; uint16_t align;
            std::fread(&fmt, 2, 1, f); std::fread(&ch, 2, 1, f); std::fread(&rate, 4, 1, f);
            std::fread(&br, 4, 1, f); std::fread(&align, 2, 1, f); std::fread(&bits, 2, 1, f);
            std::fseek(f, (long)sz - 16, SEEK_CUR);
        }
        else if (!std::memcmp(id, "data", 4))
        {
            const size_t n = sz / (bits / 8) / ch;
            g_wav.resize(n);
            for (size_t i = 0; i < n; i++)
            {
                int16_t s = 0;
                std::fread(&s, 2, 1, f);
                if (ch > 1) std::fseek(f, (long)(ch - 1) * 2, SEEK_CUR);
                g_wav[i] = s / 32768.f;
            }
            break;
        }
        else std::fseek(f, (long)sz, SEEK_CUR);
    }
    std::fclose(f);
    return !g_wav.empty();
}

static void sdl_play(void*, Uint8* stream, int len)
{
    Render(reinterpret_cast<float*>(stream), (size_t)len / (2 * sizeof(float)));
}

static void sdl_capture(void*, Uint8* stream, int len)
{
    const float* in = reinterpret_cast<const float*>(stream);
    const size_t n  = (size_t)len / sizeof(float);
    std::lock_guard<std::mutex> lk(g_in_mu);
    for (size_t i = 0; i < n; i++)
    {
        g_mic[g_mic_w] = in[i];
        g_mic_w = (g_mic_w + 1) % g_mic.size();
        if (g_mic_w == g_mic_r) g_mic_r = (g_mic_r + 1) % g_mic.size();   /* overrun: drop oldest */
    }
}

bool StartSoundCard(bool want_mic)
{
    if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) return false;
    if (want_mic)
    {
        g_mic.assign(48000u, 0.f);   /* 1 s of slack between the two devices */
        SDL_AudioSpec cw = {}, ch = {};
        cw.freq = 48000; cw.format = AUDIO_F32SYS; cw.channels = 1; cw.samples = 256;
        cw.callback = sdl_capture;
        SDL_AudioDeviceID cap = SDL_OpenAudioDevice(nullptr, 1, &cw, &ch, 0);
        if (!cap) std::fprintf(stderr, "[emu] no microphone (%s) -- input is silence\n", SDL_GetError());
        else
        {
            std::fprintf(stderr, "[emu] mic: %s\n", SDL_GetAudioDeviceName(0, 1) ? SDL_GetAudioDeviceName(0, 1) : "default");
            SDL_PauseAudioDevice(cap, 0);
        }
    }
    SDL_AudioSpec want = {}, have = {};
    want.freq = 48000; want.format = AUDIO_F32SYS; want.channels = 2; want.samples = 256;
    want.callback = sdl_play;
    SDL_AudioDeviceID out = SDL_OpenAudioDevice(nullptr, 0, &want, &have, 0);
    if (!out)
    {
        std::fprintf(stderr, "[emu] no output device: %s\n", SDL_GetError());
        return false;
    }
    SDL_PauseAudioDevice(out, 0);
    return true;
}

} // namespace emu

/* ---------------------------------------------------------- firmware ---- */

static void firmware_thread() { firmware_main(); }

int emu_ui_run(bool want_mic);                  /* emu_ui.cpp */
int emu_script_run(const char* path);          /* emu_script.cpp */

int main(int argc, char** argv)
{
    bool        headless = false;
    const char* script = nullptr;
    const char* in_wav = nullptr;
    const char* rec = nullptr;
    const char* card = nullptr;
    const char* usb  = nullptr;
    std::string flash = std::string(std::getenv("HOME") ? std::getenv("HOME") : ".") + "/.alchemy-emu/" + std::string(EMU_FW_NAME) + "-flash.bin";
    for (int i = 1; i < argc; i++)
    {
        const std::string a = argv[i];
        if (a == "--headless") headless = true;
        else if (a == "--script" && i + 1 < argc) script = argv[++i];
        else if (a == "--in" && i + 1 < argc) in_wav = argv[++i];
        else if (a == "--record" && i + 1 < argc) rec = argv[++i];
        else if (a == "--flash" && i + 1 < argc) flash = argv[++i];
        else if (a == "--card" && i + 1 < argc) card = argv[++i];
        else if (a == "--usb" && i + 1 < argc) usb = argv[++i];
        else { std::fprintf(stderr, "usage: %s [--headless --script f] [--in wav] [--record wav] [--flash path|none]\n", argv[0]); return 2; }
    }
    if (flash == "none") flash.clear();
    else
    {
        const std::string dir = flash.substr(0, flash.find_last_of('/'));
        std::string cmd = "mkdir -p '" + dir + "'";
        if (std::system(cmd.c_str()) != 0) {}
    }
    emu::SetFlashPath(flash);
    emu::LoadFlash();
    emu::FormatCard();
    if (card) emu::FillCard(card);
    if (usb)
    {
        /* the card's shared USB setting, as the module reads it at boot;
         * launchpad also plugs in the emulated controllers */
        emu::WriteCardFile("0:/alchemy", "0:/alchemy/usb.cfg", (std::string(usb) + "\n").c_str());
        emu::ctl::Enable(usb[0] == 'l');
    }

    auto& P = emu::Panel();
    for (int i = 0; i < alchemy::kNumPots; i++) P.pot[i] = 0.5f;
    for (int i = 0; i < alchemy::kNumButtons; i++) P.button[i] = false;
    for (int i = 0; i < alchemy::kNumCvInputs; i++) P.cv_volts[i] = 0.f;

    if (in_wav && !emu::LoadWav(in_wav)) { std::fprintf(stderr, "[emu] cannot read %s\n", in_wav); return 2; }
    if (rec && !emu::OpenRecord(rec)) { std::fprintf(stderr, "[emu] cannot write %s\n", rec); return 2; }

    std::thread(firmware_thread).detach();
    /* the firmware reaches StartAudio() after its boot (presets, card) */
    for (int i = 0; i < 400 && !emu::AudioCb(); i++) std::this_thread::sleep_for(std::chrono::milliseconds(5));

    int rc;
    if (headless) rc = emu_script_run(script);
    else          rc = emu_ui_run(in_wav == nullptr);
    emu::CloseRecord();
    std::fflush(stdout);
    std::_Exit(rc);
}
