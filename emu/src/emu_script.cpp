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
 *   clock <1|2> <ms>                 a 5 ms pulse every <ms> on J1 / J2 (0 = off)
 *   pulse <1|2>                      one 5 ms pulse on J1 / J2
 *   uart <hex> <hex> ...             bytes into USART1 RX (rear header pin 7),
 *                                    e.g. `uart 90 3c 64` = note-on C4
 *   mark                             start a new output-level window (and edge count)
 *   expect edges <3-10> > | < | = <n>  rising edges an output made since mark
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
 *   lp tap|press|release <col> <row>  Launchpad grid, 1-based, rows top-down
 *   lp top|side <n> [press|release]  its top row / right column (default tap)
 *   xl knob <row 1-3> <col> <0-127>  Launch Control XL; xl fader <col> <v>;
 *   xl button <row 1-2> <col> [press|release]
 *   pad <a|b|x|y|lb|rb|lt|rt|up|down|left|right|l3|r3> [tap|press|release]
 *   expect lp <col> <row> <colour> | expect lp top|side <n> <colour>
 *                                    (<colour> a number = that palette index)
 *   expect ring <pot> > | < <n>      lit LEDs on that pot's ring
 *   ringsave <pot> / expect ringchanged <pot>   the ring's LEDs moved
 *   expect pan left|right|centre     the output's L/R balance (last 8192)
 *   expect lpmoves <row>             the Launchpad row animates (playhead)
 *   expect lpstill <row>             pads 3-8 of that row are dark
 *   expect xl button|knob <row> <col> <colour>
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
#include "per/uart.h"
#include "emu_audio.h"

namespace {

std::atomic<bool> g_run{true};
uint32_t          g_edge_mark[8];

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

uint32_t pad_bit(const char* n)
{
    static const struct { const char* n; uint32_t b; } k[] = {
        {"up", 1u << 0}, {"down", 1u << 1}, {"left", 1u << 2}, {"right", 1u << 3},
        {"start", 1u << 4}, {"back", 1u << 5}, {"l3", 1u << 6}, {"r3", 1u << 7},
        {"lb", 1u << 8}, {"rb", 1u << 9}, {"guide", 1u << 10}, {"a", 1u << 12},
        {"b", 1u << 13}, {"x", 1u << 14}, {"y", 1u << 15}, {"lt", 1u << 16}, {"rt", 1u << 17}};
    for (const auto& e : k) if (!std::strcmp(n, e.n)) return e.b;
    return 0;
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
        else if (C == "clock")   emu::SetClock(std::atoi(a) - 1, (float)std::atof(b));
        else if (C == "pulse")   emu::Pulse(std::atoi(a) - 1);
        else if (C == "uart")
        {
            /* every hex token after the command, one byte each */
            uint8_t bytes[64];
            size_t  nb = 0;
            const char* p = std::strstr(line, "uart") + 4;
            char* end = nullptr;
            for (long v; nb < sizeof bytes && (v = std::strtol(p, &end, 16), end != p); p = end)
                bytes[nb++] = (uint8_t)v;
            if (emu::UartInject(bytes, nb) != nb)
            {
                std::fprintf(stderr, "[emu] line %d: uart: nothing listening on USART1\n", line_no);
                fails++;
            }
        }
        else if (C == "mark")
        {
            emu::TakeOutRms();
            for (int j = 0; j < 8; j++) g_edge_mark[j] = emu::Edges(j);
        }
        else if (C == "expect" && !std::strcmp(a, "edges"))
        {
            /* expect edges <jack 3-10> > | < | = <n>: rising edges since mark */
            const int      j = std::atoi(b) - 3;
            const uint32_t got = emu::Edges(j) - g_edge_mark[j], want = (uint32_t)std::atoi(d);
            const bool ok = !std::strcmp(c, ">") ? got > want : !std::strcmp(c, "<") ? got < want : got == want;
            std::printf("%s line %d: J%d made %u rising edges since mark, want %s %u\n",
                        ok ? "PASS" : "FAIL", line_no, j + 3, got, c, want);
            if (!ok) fails++;
        }
        else if (C == "print" && !std::strcmp(a, "edges"))
        {
            for (int j = 0; j < 8; j++)
                std::printf("  J%d %u edges since mark\n", j + 3, emu::Edges(j) - g_edge_mark[j]);
        }
        else if (C == "snapshot")
        {
            /* the folder may not exist: a suite can run another build's
             * scripts (tests/belt-chords runs tests/belt's) */
            const std::string dir = std::string(a).substr(0, std::string(a).find_last_of('/'));
            if (dir != a) std::system(("mkdir -p '" + dir + "'").c_str());
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
        else if (C == "lp")
        {
            /* lp tap|press|release <col 1-8> <row 1-8, top-down>; lp top <n> [press|release] */
            if (!std::strcmp(a, "top") || !std::strcmp(a, "side"))
            {
                const int kind = !std::strcmp(a, "top") ? 2 : 1, n = std::atoi(b) - 1;
                const bool press = !std::strcmp(c, "press"), rel = !std::strcmp(c, "release");
                if (!rel) emu::ctl::LpEvent(kind, kind == 2 ? n : 0, kind == 1 ? n : 0, true);
                if (!press && !rel) sleep_ms(80);
                if (!press) emu::ctl::LpEvent(kind, kind == 2 ? n : 0, kind == 1 ? n : 0, false);
            }
            else
            {
                const int x = std::atoi(b) - 1, y = std::atoi(c) - 1;
                if (std::strcmp(a, "release")) emu::ctl::LpEvent(0, x, y, true);
                if (!std::strcmp(a, "tap")) sleep_ms(80);
                if (std::strcmp(a, "press")) emu::ctl::LpEvent(0, x, y, false);
            }
        }
        else if (C == "xl")
        {
            /* xl knob <row 1-3> <col 1-8> <0-127> | xl fader <col> <v> | xl button <row 1-2> <col> [press|release] */
            if (!std::strcmp(a, "knob")) emu::ctl::XlKnob(std::atoi(b) - 1, std::atoi(c) - 1, std::atoi(d));
            else if (!std::strcmp(a, "fader")) emu::ctl::XlFader(std::atoi(b) - 1, std::atoi(c));
            else if (!std::strcmp(a, "button"))
            {
                const int r = std::atoi(b) - 1, cl = std::atoi(c) - 1;
                const bool press = !std::strcmp(d, "press"), rel = !std::strcmp(d, "release");
                if (!rel) emu::ctl::XlButton(r, cl, true);
                if (!press && !rel) sleep_ms(80);
                if (!press) emu::ctl::XlButton(r, cl, false);
            }
        }
        else if (C == "pad")
        {
            /* pad <a|b|x|y|lb|rb|lt|rt|up|down|left|right|l3|r3> [tap|press|release] */
            const uint32_t bit = pad_bit(a);
            if (!bit) { std::printf("?? line %d: pad button %s\n", line_no, a); fails++; }
            else if (!std::strcmp(b, "press")) emu::ctl::PadSet(bit, true);
            else if (!std::strcmp(b, "release")) emu::ctl::PadSet(bit, false);
            else { emu::ctl::PadSet(bit, true); sleep_ms(80); emu::ctl::PadSet(bit, false); }
        }
        else if (C == "expect" && !std::strcmp(a, "lp"))
        {
            /* expect lp <col> <row> <colour> | expect lp top <n> <colour> */
            uint8_t idx, r, g, bl;
            const char* want;
            char where[32];
            if (!std::strcmp(b, "top")) { idx = emu::ctl::LpTop(std::atoi(c) - 1); want = d; std::snprintf(where, sizeof where, "top %s", c); }
            else if (!std::strcmp(b, "side")) { idx = emu::ctl::LpSide(std::atoi(c) - 1); want = d; std::snprintf(where, sizeof where, "side %s", c); }
            else { idx = emu::ctl::LpGrid(std::atoi(b) - 1, std::atoi(c) - 1); want = d; std::snprintf(where, sizeof where, "pad %s,%s", b, c); }
            emu::LpRgb(idx, &r, &g, &bl);
            const char* got = classify(r, g, bl);
            /* a number = that exact palette index (bright vs dim) */
            const bool ok = (want[0] >= '0' && want[0] <= '9') ? idx == (uint8_t)std::strtol(want, nullptr, 0)
                                                              : !std::strcmp(got, want);
            std::printf("%s line %d: Launchpad %s is %s (palette %u), want %s\n", ok ? "PASS" : "FAIL", line_no, where, got, idx, want);
            if (!ok) fails++;
        }
        else if (C == "expect" && !std::strcmp(a, "xl"))
        {
            /* expect xl button <row> <col> <colour> | expect xl knob <row> <col> <colour> */
            const int r0 = std::atoi(c) - 1, c0 = std::atoi(d) - 1;
            const uint8_t col = !std::strcmp(b, "button") ? emu::ctl::XlButtonLed(r0, c0) : emu::ctl::XlKnobLed(r0, c0);
            uint8_t r, g, bl;
            emu::XlRgb(col, &r, &g, &bl);
            const char* got = classify(r, g, bl);
            /* a number (0x..) = that exact LED byte: launchpad.h xl::Col(red, green) */
            const bool ok = (e[0] >= '0' && e[0] <= '9') ? col == (uint8_t)std::strtol(e, nullptr, 0)
                                                        : !std::strcmp(got, e);
            std::printf("%s line %d: XL %s %d,%d is %s (0x%02X), want %s\n", ok ? "PASS" : "FAIL", line_no, b, r0 + 1, c0 + 1, got, col, e);
            if (!ok) fails++;
        }
        else if (C == "expect" && !std::strcmp(a, "ring"))
        {
            /* expect ring <pot 1-6> > | < <n>: lit LEDs on that pot's ring */
            emu::LedFrame fr;
            emu::ReadLeds(&fr);
            const alchemy::LedRingLayout& ring = alchemy::kAlchemyLabV2Layout.rings[std::atoi(b) - 1];
            int lit = 0;
            for (int k = 0; k < ring.led_count; k++)
            {
                const uint8_t* px = fr.rgb[ring.chain_start + k];
                if (px[0] | px[1] | px[2]) lit++;
            }
            const int want = std::atoi(d);
            const bool ok = !std::strcmp(c, ">") ? lit > want : lit < want;
            std::printf("%s line %d: P%s ring has %d lit, want %s %d\n", ok ? "PASS" : "FAIL", line_no, b, lit, c, want);
            if (!ok) fails++;
        }
        else if (C == "ringsave" || (C == "expect" && !std::strcmp(a, "ringchanged")))
        {
            /* ringsave <pot>; expect ringchanged <pot>: the ring's LEDs differ */
            static uint64_t saved[6];
            const int pot = std::atoi(C == "ringsave" ? a : b) - 1;
            emu::LedFrame fr;
            emu::ReadLeds(&fr);
            const alchemy::LedRingLayout& ring = alchemy::kAlchemyLabV2Layout.rings[pot];
            uint64_t h = 1469598103934665603ull;
            for (int k = 0; k < ring.led_count; k++)
                for (int ch = 0; ch < 3; ch++) h = (h ^ fr.rgb[ring.chain_start + k][ch]) * 1099511628211ull;
            if (C == "ringsave") saved[pot] = h;
            else
            {
                const bool ok = h != saved[pot];
                std::printf("%s line %d: P%d ring %s\n", ok ? "PASS" : "FAIL", line_no, pot + 1, ok ? "changed" : "unchanged");
                if (!ok) fails++;
            }
        }
        else if (C == "expect" && !std::strcmp(a, "pan"))
        {
            /* expect pan left|right|centre: the output's L/R balance */
            float l, r;
            emu::LrRms(&l, &r);
            const float ratio = (l + 1e-6f) / (r + 1e-6f);
            bool ok;
            if (!std::strcmp(b, "left")) ok = ratio > 2.0f;
            else if (!std::strcmp(b, "right")) ok = ratio < 0.5f;
            else ok = ratio > 0.7f && ratio < 1.4f;
            std::printf("%s line %d: L %.4f R %.4f (L/R %.2f), want %s\n", ok ? "PASS" : "FAIL", line_no, l, r, ratio, b);
            if (!ok) fails++;
        }
        else if (C == "expect" && !std::strcmp(a, "lpmoves"))
        {
            /* expect lpmoves <row 1-8>: the row's pattern changes within 1.5 s (a playhead) */
            const int y = std::atoi(b) - 1;
            uint64_t first = 0;
            int changes = 0;
            for (int k = 0; k < 30; k++)
            {
                uint64_t pat = 0;
                for (int x = 0; x < 8; x++) pat = (pat << 8) | emu::ctl::LpGrid(x, y);
                if (k == 0) first = pat;
                else if (pat != first) { changes++; first = pat; }
                sleep_ms(50);
            }
            const bool ok = changes >= 1;
            std::printf("%s line %d: Launchpad row %s changed %d time(s) in 1.5 s\n", ok ? "PASS" : "FAIL", line_no, b, changes);
            if (!ok) fails++;
        }
        else if (C == "expect" && !std::strcmp(a, "lpstill"))
        {
            /* expect lpstill <row>: the row is dark (no playhead) */
            const int y = std::atoi(b) - 1;
            int lit = 0;
            for (int x = 2; x < 8; x++) if (emu::ctl::LpGrid(x, y)) lit++;
            const bool ok = lit == 0;
            std::printf("%s line %d: Launchpad row %s pads 3-8 lit: %d, want 0\n", ok ? "PASS" : "FAIL", line_no, b, lit);
            if (!ok) fails++;
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
