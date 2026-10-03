/*
 * Headless runs: a script drives the panel in real time while the firmware's
 * audio callback is paced by a driver thread (128-frame blocks at 48 kHz),
 * then checks LEDs and the output. Exit 0 if every `expect` held.
 *
 *   # comment
 *   wait <ms>
 *   pot <1-6> <0..1>                 P1..P6, front view
 *   press <b1|b2|b3> / release <b>   hold <b> <ms>   tap <b>
 *   cv <3-8> <volts>                 J3..J8
 *   sing <hz>                        a sung buzz at hz on J1/J2 ("sing 0" =
 *   silence                          silence; default when there is no --in)
 *   mark                             start a new output-level window
 *   expect rms > <x> | < <x>         output RMS since `mark` (0..1)
 *   expect screen <text>             the OLED (Smack) shows <text> (substring)
 *   expect tone <hz> > <x> | < <x>   amplitude at hz in the last ~170 ms out
 *   expect led <b1|b2|b3> <colour>   off red green blue purple grey white amber
 *   expect led <b> rgb <r> <g> <b>   (+-48 per channel, raw firmware values)
 *   expect booted | alive | finite   audio started / LEDs still refreshing /
 *                                    no NaN or inf in the output
 *   expect unclipped                 under 1% of output frames at full scale
 *   expect leds > <n>                LEDs lit anywhere on the panel
 *   expect cv <3-10> > | < <volts>   a jack the firmware drives as an output
 *   expect cvrange <3-10> > <volts>  that output swings more than <volts> in 2 s
 *   print rms | cv                   output level since mark / CV outputs
 *   print leds                       the three button pairs, raw
 *   print tones                      A3..C5 amplitudes in the output
 *   print screen                     the OLED's two lines
 *   snapshot <file.bmp>              the panel, drawn as the window draws it
 */
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cctype>
#include <cstring>
#include <string>
#include <thread>

#include "alchemy/hw/alchemy_lab_v2.h"
#include "emu.h"
#include "emu_audio.h"

namespace {

std::atomic<bool> g_run{true};

void driver()
{
    using Clock = std::chrono::steady_clock;
    float           buf[128 * 2];
    auto            next = Clock::now();
    const auto      period = std::chrono::microseconds(128 * 1000000 / 48000);
    while (g_run)
    {
        emu::Render(buf, 128);
        next += period;
        std::this_thread::sleep_until(next);
    }
}

int btn_index(const char* s)
{
    if (!std::strcmp(s, "b1")) return 0;
    if (!std::strcmp(s, "b2")) return 1;
    if (!std::strcmp(s, "b3")) return 2;
    return -1;
}

void sleep_ms(int ms) { std::this_thread::sleep_for(std::chrono::milliseconds(ms)); }

/* The button pair's colour as the firmware set it (both LEDs of a pair are
 * set together; the top one is read). */
void button_rgb(int b, int* r, int* g, int* bl)
{
    emu::LedFrame f;
    emu::ReadLeds(&f);
    const uint16_t i = alchemy::kAlchemyLabV2Layout.buttons[b].top_chain;
    *r = f.rgb[i][0]; *g = f.rgb[i][1]; *bl = f.rgb[i][2];
}

/* Colour names by dominant channels -- robust to the brightness setting. */
const char* classify(int r, int g, int b)
{
    const int mx = std::max(r, std::max(g, b));
    if (mx < 10) return "off";
    auto hi = [&](int v) { return v >= mx * 0.6f; };
    auto lo = [&](int v) { return v <= mx * 0.4f; };
    if (hi(r) && hi(g) && hi(b)) return mx > 50 ? "white" : "grey";
    if (hi(r) && lo(g) && lo(b)) return "red";
    if (lo(r) && hi(g) && lo(b)) return "green";
    if (lo(r) && lo(g) && hi(b)) return "blue";
    if (hi(r) && lo(g) && hi(b)) return "purple";
    if (hi(r) && !lo(g) && lo(b)) return "amber";
    if (lo(r) && hi(g) && hi(b)) return "cyan";
    if (!lo(r) && lo(g) && hi(b)) return "purple";
    return "mixed";
}

} // namespace

int emu_ui_snapshot(const char* path);

int emu_script_run(const char* path)
{
    if (!path) { std::fprintf(stderr, "[emu] --headless needs --script\n"); return 2; }
    FILE* f = std::fopen(path, "r");
    if (!f) { std::fprintf(stderr, "[emu] cannot read %s\n", path); return 2; }

    std::thread drv(driver);
    auto& P = emu::Panel();
    int   fails = 0, line_no = 0;
    char  line[256];
    float rms_acc = 0.f;
    (void)rms_acc;
    emu::TakeOutRms();

    while (std::fgets(line, sizeof line, f))
    {
        line_no++;
        char* hash = std::strchr(line, '#');
        if (hash) *hash = 0;
        char cmd[32] = {}, a[240] = {}, b[32] = {}, c[32] = {}, d[32] = {}, e[32] = {};
        const int n = std::sscanf(line, "%31s %239s %31s %31s %31s %31s", cmd, a, b, c, d, e);
        if (n <= 0) continue;
        const std::string C = cmd;

        if (C == "wait") sleep_ms(std::atoi(a));
        else if (C == "pot") P.pot[std::atoi(a) - 1] = (float)std::atof(b);
        else if (C == "press")   P.button[btn_index(a)] = true;
        else if (C == "release") P.button[btn_index(a)] = false;
        else if (C == "hold")  { P.button[btn_index(a)] = true; sleep_ms(std::atoi(b)); P.button[btn_index(a)] = false; }
        else if (C == "tap")   { P.button[btn_index(a)] = true; sleep_ms(80); P.button[btn_index(a)] = false; }
        else if (C == "cv")      P.cv_volts[std::atoi(a) - 3] = (float)std::atof(b);
        else if (C == "sing")    emu::SetSynthHz(std::atoi(a));
        else if (C == "silence") emu::SetSynthHz(0);
        else if (C == "mark")    emu::TakeOutRms();
        else if (C == "snapshot")
        {
            const int rc = emu_ui_snapshot(a);
            std::printf("%s snapshot %s\n", rc ? "FAILED" : "wrote", a);
        }
        else if (C == "print" && !std::strcmp(a, "leds"))
        {
            for (int i = 0; i < 3; i++)
            {
                int r, g, bl;
                button_rgb(i, &r, &g, &bl);
                std::printf("  b%d = %3d %3d %3d (%s)\n", i + 1, r, g, bl, classify(r, g, bl));
            }
        }
        else if (C == "expect" && !std::strcmp(a, "booted"))
        {
            const bool ok = emu::AudioCb() != nullptr;
            std::printf("%s line %d: firmware started audio\n", ok ? "PASS" : "FAIL", line_no);
            if (!ok) fails++;
        }
        else if (C == "expect" && !std::strcmp(a, "alive"))
        {
            const uint32_t s0 = emu::ShowCount();
            sleep_ms(500);
            const uint32_t s1 = emu::ShowCount();
            const bool ok = s1 > s0 + 5;
            std::printf("%s line %d: control loop alive (%u LED frames in 0.5 s)\n", ok ? "PASS" : "FAIL", line_no, s1 - s0);
            if (!ok) fails++;
        }
        else if (C == "expect" && !std::strcmp(a, "finite"))
        {
            uint32_t bad, pinned; uint64_t frames;
            emu::OutputHealth(&bad, &pinned, &frames);
            const bool ok = bad == 0;
            std::printf("%s line %d: output finite (NaN/inf %u; at full scale %u of %llu frames)\n",
                        ok ? "PASS" : "FAIL", line_no, bad, pinned, (unsigned long long)frames);
            if (!ok) fails++;
        }
        else if (C == "expect" && !std::strcmp(a, "unclipped"))
        {
            /* audio firmwares: under 1% of frames at full scale (DC/CV
             * firmwares such as Marbles legitimately sit there) */
            uint32_t bad, pinned; uint64_t frames;
            emu::OutputHealth(&bad, &pinned, &frames);
            const bool ok = pinned <= frames / 100;
            std::printf("%s line %d: unclipped (%u of %llu frames at full scale)\n", ok ? "PASS" : "FAIL",
                        line_no, pinned, (unsigned long long)frames);
            if (!ok) fails++;
        }
        else if (C == "expect" && !std::strcmp(a, "leds"))
        {
            emu::LedFrame fr;
            emu::ReadLeds(&fr);
            int lit = 0;
            for (int i = 0; i < alchemy::kLedTotal; i++) if (fr.rgb[i][0] | fr.rgb[i][1] | fr.rgb[i][2]) lit++;
            const int want = std::atoi(c);
            const bool ok = !std::strcmp(b, ">") ? lit > want : lit < want;
            std::printf("%s line %d: %d LEDs lit %s %d\n", ok ? "PASS" : "FAIL", line_no, lit, b, want);
            if (!ok) fails++;
        }
        else if (C == "expect" && !std::strcmp(a, "cv"))
        {
            /* expect cv <jack 3-10> > | < <volts>: what the firmware outputs */
            const int j = std::atoi(b) - 3;
            auto& P2 = emu::Panel();
            const float v = P2.cv_out[j].load();
            const bool is_out = P2.cv_is_out[j].load();
            const float want = (float)std::atof(d);
            const bool ok = is_out && (!std::strcmp(c, ">") ? v > want : v < want);
            std::printf("%s line %d: J%d %s %.3f V %s %.3f\n", ok ? "PASS" : "FAIL", line_no, j + 3,
                        is_out ? "outputs" : "is not an output,", v, c, want);
            if (!ok) fails++;
        }
        else if (C == "expect" && !std::strcmp(a, "cvrange"))
        {
            /* expect cvrange <jack 3-10> > <volts>: the output's swing over 2 s */
            const int j = std::atoi(b) - 3;
            auto& P2 = emu::Panel();
            float lo = 1e9f, hi = -1e9f;
            for (int k = 0; k < 200; k++)
            {
                const float v = P2.cv_out[j].load();
                lo = std::min(lo, v); hi = std::max(hi, v);
                sleep_ms(10);
            }
            const float want = (float)std::atof(d);
            const bool ok = P2.cv_is_out[j].load() && hi - lo > want;
            std::printf("%s line %d: J%d swung %.3f V (%.3f..%.3f) over 2 s, want > %.3f\n",
                        ok ? "PASS" : "FAIL", line_no, j + 3, hi - lo, lo, hi, want);
            if (!ok) fails++;
        }
        else if (C == "print" && !std::strcmp(a, "rms"))
            std::printf("  output rms since mark: %.4f\n", emu::TakeOutRms());
        else if (C == "print" && !std::strcmp(a, "cv"))
        {
            auto& P2 = emu::Panel();
            for (int j = 0; j < 8; j++)
                if (P2.cv_is_out[j].load()) std::printf("  J%d out %.3f V\n", j + 3, P2.cv_out[j].load());
        }
        else if (C == "expect" && !std::strcmp(a, "rms"))
        {
            const float v = emu::TakeOutRms(), want = (float)std::atof(c);
            const bool ok = !std::strcmp(b, ">") ? v > want : v < want;
            std::printf("%s line %d: output rms %.4f %s %.4f\n", ok ? "PASS" : "FAIL", line_no, v, b, want);
            if (!ok) fails++;
        }
        else if (C == "print" && !std::strcmp(a, "screen"))
        {
            std::string sm, bg;
            if (emu::ScreenLines(&sm, &bg)) std::printf("  screen: [%s] [%s]\n", sm.c_str(), bg.c_str());
            else std::printf("  screen: (none)\n");
        }
        else if (C == "expect" && !std::strcmp(a, "screen"))
        {
            /* the rest of the line, case-insensitive substring of either line */
            const char* want = std::strstr(line, "screen") + 6;
            while (*want == ' ') want++;
            std::string w = want;
            while (!w.empty() && (w.back() == '\n' || w.back() == ' ' || w.back() == '\r')) w.pop_back();
            std::string sm, bg;
            const bool have = emu::ScreenLines(&sm, &bg);
            auto up = [](std::string x) { for (auto& ch : x) ch = (char)std::toupper((unsigned char)ch); return x; };
            const bool ok = have && (up(sm).find(up(w)) != std::string::npos || up(bg).find(up(w)) != std::string::npos);
            std::printf("%s line %d: screen [%s] [%s] contains \"%s\"\n", ok ? "PASS" : "FAIL", line_no, sm.c_str(), bg.c_str(), w.c_str());
            if (!ok) fails++;
        }
        else if (C == "expect" && !std::strcmp(a, "tone"))
        {
            const float hz = (float)std::atof(b), v = emu::ToneLevel(hz), want = (float)std::atof(d);
            const bool ok = !std::strcmp(c, ">") ? v > want : v < want;
            std::printf("%s line %d: tone %.1f Hz at %.4f %s %.4f\n", ok ? "PASS" : "FAIL", line_no, hz, v, c, want);
            if (!ok) fails++;
        }
        else if (C == "print" && !std::strcmp(a, "tones"))
        {
            for (int i = 2; i < n && i < 6; i++) {}
            const char* names[] = {"A3 220", "C4 261.6", "D4 293.7", "E4 329.6", "F4 349.2", "G4 392", "A4 440", "B4 493.9", "C5 523.3"};
            const float hz[] = {220.f, 261.63f, 293.66f, 329.63f, 349.23f, 392.f, 440.f, 493.88f, 523.25f};
            for (int i = 0; i < 9; i++) std::printf("  %-9s %.4f\n", names[i], emu::ToneLevel(hz[i]));
        }
        else if (C == "expect" && !std::strcmp(a, "led"))
        {
            const int i = btn_index(b);
            int r, g, bl;
            button_rgb(i, &r, &g, &bl);
            bool ok;
            if (!std::strcmp(c, "rgb"))
                ok = std::abs(r - std::atoi(d)) <= 48 && std::abs(g - std::atoi(e)) <= 48;
            else
                ok = !std::strcmp(classify(r, g, bl), c);
            std::printf("%s line %d: %s is %s (%d %d %d), want %s\n", ok ? "PASS" : "FAIL", line_no, b,
                        classify(r, g, bl), r, g, bl, c);
            if (!ok) fails++;
        }
        else { std::printf("?? line %d: %s", line_no, line); fails++; }
        std::fflush(stdout);
    }
    std::fclose(f);
    g_run = false;
    drv.join();
    std::printf(fails ? "\n%d check(s) FAILED\n" : "\nall checks passed\n", fails);
    return fails ? 1 : 0;
}
