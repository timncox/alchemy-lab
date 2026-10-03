/*
 * The emulator window: the Alchemy Lab's front panel, drawn from the LED
 * chain the firmware Show()s, with the six pots, B1-B3 and the CV jacks
 * J3-J8 as controls.
 *
 *   knobs      drag up/down, or scroll; double-click = centre
 *   buttons    click and hold, or hold the keys 1 / 2 / 3
 *   jacks      drag the slider for volts; click the GATE box for 0 <-> +5 V
 *   Esc / Q    quit
 */
#include <SDL.h>

#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string>

#include "alchemy/hw/alchemy_lab_v2.h"
#include "emu.h"
#include "emu_audio.h"

void emu_text(SDL_Renderer* r, int x, int y, int s, const char* t, SDL_Color c);
int  emu_text_width(const char* t, int s);

namespace {

constexpr int kW = 600, kH = 860;
constexpr int kColX[2] = {150, 450};
constexpr int kRowY[3] = {170, 350, 530};
constexpr int kBtnX    = 300;
constexpr int kKnobR   = 44;
constexpr int kRingR   = 62;

/* The firmware's main-page / B3-page knob names, front view (P1..P6), from
 * fw/<fw>.mk ("A|B|C|D|E|F"). Display only. */
char kPlayBuf[160], kSetupBuf[160];
const char* kPlay[6]  = {"P1", "P2", "P3", "P4", "P5", "P6"};
const char* kSetup[6] = {"P1", "P2", "P3", "P4", "P5", "P6"};
const char* kJack[6]  = {"J3", "J4", "J5", "J6", "J7", "J8"};

void split_labels(const char* src, char* buf, size_t cap, const char** out)
{
    std::snprintf(buf, cap, "%s", src);
    int n = 0;
    char* p = buf;
    while (n < 6 && *p)
    {
        out[n++] = p;
        char* bar = std::strchr(p, '|');
        if (!bar) break;
        *bar = 0;
        p = bar + 1;
    }
}

struct LabelInit
{
    LabelInit()
    {
        split_labels(EMU_LABELS, kPlayBuf, sizeof kPlayBuf, kPlay);
        split_labels(EMU_LABELS_ALT, kSetupBuf, sizeof kSetupBuf, kSetup);
    }
} g_label_init;

const SDL_Color kInk   = {0xE8, 0xE0, 0xD0, 255};
const SDL_Color kDim   = {0x90, 0x88, 0x7C, 255};
const SDL_Color kRust  = {0xFF, 0x80, 0x30, 255};

void fill_circle(SDL_Renderer* r, int cx, int cy, int rad)
{
    for (int dy = -rad; dy <= rad; dy++)
    {
        const int dx = (int)std::sqrt((float)(rad * rad - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void hour_xy(float hour, int cx, int cy, int rad, int* x, int* y)
{
    const float a = hour / 12.0f * 2.0f * (float)M_PI;   /* 0 = 12 o'clock, CW */
    *x = cx + (int)lroundf(std::sin(a) * rad);
    *y = cy - (int)lroundf(std::cos(a) * rad);
}

/* LEDs as the eye sees them: the firmware's values are PWM duty, so scale
 * them up and keep a little glow when off. */
SDL_Color led_colour(const uint8_t* rgb)
{
    auto boost = [](uint8_t v) { int b = (int)(std::sqrt(v / 255.0f) * 255.0f); return (Uint8)(b > 255 ? 255 : b); };
    return {boost(rgb[0]), boost(rgb[1]), boost(rgb[2]), 255};
}

struct Ui
{
    SDL_Window*   win = nullptr;
    SDL_Renderer* ren = nullptr;
    int           drag_pot = -1, drag_jack = -1;
    int           drag_y = 0;
    float         drag_v0 = 0.f;
    int           mouse_btn = -1;      /* button held by the mouse */
    bool          key_btn[3] = {};
    uint32_t      last_click = 0;
    int           last_click_pot = -1;
};

int pot_at(int x, int y)
{
    for (int p = 0; p < 6; p++)
    {
        const int dx = x - kColX[p % 2], dy = y - kRowY[p / 2];
        if (dx * dx + dy * dy <= (kKnobR + 12) * (kKnobR + 12)) return p;
    }
    return -1;
}

int btn_at(int x, int y)
{
    for (int b = 0; b < 3; b++)
    {
        const int dx = x - kBtnX, dy = y - kRowY[b];
        if (dx * dx + dy * dy <= 30 * 30) return b;
    }
    return -1;
}

/* Jack sliders: column i at x = 50 + i*90, track y 690..790. */
SDL_Rect jack_track(int i) { return {50 + i * 90 + 20, 690, 12, 100}; }
SDL_Rect jack_gate(int i)  { return {50 + i * 90 + 6, 800, 40, 18}; }

int jack_at(int x, int y, bool* gate)
{
    for (int i = 0; i < 6; i++)
    {
        SDL_Rect t = jack_track(i), g = jack_gate(i);
        if (x >= t.x - 14 && x <= t.x + t.w + 14 && y >= t.y - 6 && y <= t.y + t.h + 6) { *gate = false; return i; }
        if (x >= g.x && x <= g.x + g.w && y >= g.y && y <= g.y + g.h) { *gate = true; return i; }
    }
    return -1;
}

void set_jack_from_y(int i, int y)
{
    SDL_Rect t = jack_track(i);
    float v = 5.0f - 10.0f * (float)(y - t.y) / (float)t.h;
    if (v > 5.f) v = 5.f;
    if (v < -5.f) v = -5.f;
    if (std::fabs(v) < 0.25f) v = 0.f;   /* detent at 0 V */
    emu::Panel().cv_volts[i] = v;
}

void draw(Ui& u)
{
    SDL_Renderer* r = u.ren;
    auto& P = emu::Panel();
    emu::LedFrame f;
    emu::ReadLeds(&f);
    const alchemy::HardwareLayout& L = alchemy::kAlchemyLabV2Layout;

    SDL_SetRenderDrawColor(r, 0x1A, 0x18, 0x16, 255);
    SDL_RenderClear(r);
    SDL_SetRenderDrawColor(r, 0x26, 0x23, 0x20, 255);
    SDL_Rect plate = {20, 20, kW - 40, kH - 40};
    SDL_RenderFillRect(r, &plate);

    emu_text(r, 40, 40, 3, EMU_FW_NAME, kRust);
    emu_text(r, 40, 72, 1, "ALCHEMY LAB EMULATOR - THE FIRMWARE, UNCHANGED", kDim);

    const bool setup_held = P.button[2].load();
    for (int p = 0; p < 6; p++)
    {
        const int cx = kColX[p % 2], cy = kRowY[p / 2];
        const alchemy::LedRingLayout& ring = L.rings[p];
        for (int k = 0; k < ring.led_count; k++)
        {
            int x, y;
            hour_xy(ring.start_hour + ring.step_hours * (float)k, cx, cy, kRingR, &x, &y);
            SDL_Color c = led_colour(f.rgb[ring.chain_start + k]);
            SDL_SetRenderDrawColor(r, (Uint8)(0x20 + c.r * 0.88f), (Uint8)(0x1C + c.g * 0.88f), (Uint8)(0x18 + c.b * 0.88f), 255);
            fill_circle(r, x, y, 5);
        }
        SDL_SetRenderDrawColor(r, 0x10, 0x0F, 0x0E, 255);
        fill_circle(r, cx, cy, kKnobR);
        SDL_SetRenderDrawColor(r, 0x3A, 0x36, 0x32, 255);
        fill_circle(r, cx, cy, kKnobR - 4);
        /* pointer: 7 o'clock (0) to 5 o'clock (1) */
        const float v = P.pot[p].load();
        int px, py;
        hour_xy(7.0f + v * 10.0f, cx, cy, kKnobR - 10, &px, &py);
        SDL_SetRenderDrawColor(r, 0xF0, 0xE8, 0xD8, 255);
        SDL_RenderDrawLine(r, cx, cy, px, py);
        SDL_RenderDrawLine(r, cx + 1, cy, px + 1, py);
        const char* name = setup_held ? kSetup[p] : kPlay[p];
        emu_text(r, cx - emu_text_width(name, 2) / 2, cy + kRingR + 14, 2, name, setup_held ? SDL_Color{0xC0, 0x80, 0xFF, 255} : kInk);
        char pn[8];
        std::snprintf(pn, sizeof pn, "P%d", p + 1);
        emu_text(r, cx - kRingR - 14, cy - kRingR - 8, 1, pn, kDim);
    }

    for (int b = 0; b < 3; b++)
    {
        const int cx = kBtnX, cy = kRowY[b];
        const alchemy::LedButtonLayout& bl = L.buttons[b];
        SDL_Color top = led_colour(f.rgb[bl.top_chain]), bot = led_colour(f.rgb[bl.bottom_chain]);
        SDL_SetRenderDrawColor(r, (Uint8)(0x20 + top.r * 0.88f), (Uint8)(0x1C + top.g * 0.88f), (Uint8)(0x18 + top.b * 0.88f), 255);
        fill_circle(r, cx, cy - 38, 7);
        SDL_SetRenderDrawColor(r, (Uint8)(0x20 + bot.r * 0.88f), (Uint8)(0x1C + bot.g * 0.88f), (Uint8)(0x18 + bot.b * 0.88f), 255);
        fill_circle(r, cx, cy + 38, 7);
        const bool down = P.button[b].load();
        SDL_SetRenderDrawColor(r, down ? 0x60 : 0x48, down ? 0x5A : 0x44, down ? 0x54 : 0x40, 255);
        fill_circle(r, cx, cy, 24);
        SDL_SetRenderDrawColor(r, down ? 0x80 : 0x5C, down ? 0x78 : 0x56, down ? 0x70 : 0x50, 255);
        fill_circle(r, cx, cy, 19);
        char bn[4];
        std::snprintf(bn, sizeof bn, "B%d", b + 1);
        emu_text(r, cx - emu_text_width(bn, 2) / 2, cy - 7, 2, bn, kInk);
    }

    /* jacks */
    emu_text(r, 40, 660, 1, "CV IN  (DRAG = VOLTS, GATE = 0 / +5 V)", kDim);
    for (int i = 0; i < 6; i++)
    {
        SDL_Rect t = jack_track(i), g = jack_gate(i);
        SDL_SetRenderDrawColor(r, 0x14, 0x13, 0x12, 255);
        SDL_RenderFillRect(r, &t);
        const float v = P.cv_volts[i].load();
        const int   y = t.y + (int)((5.0f - v) / 10.0f * t.h);
        SDL_SetRenderDrawColor(r, 0x60, 0x58, 0x50, 255);
        SDL_RenderDrawLine(r, t.x - 6, t.y + t.h / 2, t.x + t.w + 6, t.y + t.h / 2);
        SDL_SetRenderDrawColor(r, v != 0.f ? 0xFF : 0xB0, v != 0.f ? 0x80 : 0xA8, v != 0.f ? 0x30 : 0xA0, 255);
        SDL_Rect cap = {t.x - 8, y - 4, t.w + 16, 8};
        SDL_RenderFillRect(r, &cap);
        SDL_SetRenderDrawColor(r, std::fabs(v - 5.f) < 0.01f ? 0xFF : 0x40, std::fabs(v - 5.f) < 0.01f ? 0x80 : 0x3C, std::fabs(v - 5.f) < 0.01f ? 0x30 : 0x38, 255);
        SDL_RenderFillRect(r, &g);
        emu_text(r, g.x + 8, g.y + 6, 1, "GATE", kInk);
        emu_text(r, t.x - 22, t.y - 16, 1, kJack[i], kDim);
        char vs[12];
        std::snprintf(vs, sizeof vs, "%+.1fV", v);
        emu_text(r, t.x + 18, y - 3, 1, vs, kInk);
    }

    /* the firmware's OLED, if it has one (Smack's 1U screen) */
    {
        std::string sm, bg;
        if (emu::ScreenLines(&sm, &bg))
        {
            SDL_Rect oled = {kW / 2 - 130, 80, 260, 42};
            SDL_SetRenderDrawColor(r, 0x02, 0x04, 0x08, 255);
            SDL_RenderFillRect(r, &oled);
            const SDL_Color c = {0x60, 0xC8, 0xFF, 255};
            emu_text(r, oled.x + 8, oled.y + 6, 1, sm.c_str(), c);
            emu_text(r, oled.x + 8, oled.y + 17, 3, bg.c_str(), c);
        }
    }

    /* meters */
    char st[96];
    std::snprintf(st, sizeof st, "IN %3d%%   OUT %3d%%", (int)(emu::InLevel() * 100), (int)(emu::OutLevel() * 100));
    emu_text(r, kW - 40 - emu_text_width(st, 2), 40, 2, st, kInk);
    emu_text(r, 40, kH - 34, 1, "DRAG KNOBS (DOUBLE-CLICK = CENTRE)   CLICK/HOLD B1-B3 OR KEYS 1 2 3   ESC = QUIT", kDim);

    SDL_RenderPresent(r);
}

void apply_buttons(Ui& u)
{
    auto& P = emu::Panel();
    for (int b = 0; b < 3; b++) P.button[b] = u.key_btn[b] || u.mouse_btn == b;
}

} // namespace

int emu_ui_run(bool want_mic)
{
    if (SDL_Init(SDL_INIT_VIDEO) != 0) { std::fprintf(stderr, "[emu] SDL: %s\n", SDL_GetError()); return 1; }
    if (!emu::StartSoundCard(want_mic)) std::fprintf(stderr, "[emu] running without sound\n");
    Ui u;
    u.win = SDL_CreateWindow(EMU_FW_NAME " - Alchemy Lab emulator", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                             kW, kH, SDL_WINDOW_ALLOW_HIGHDPI);
    u.ren = SDL_CreateRenderer(u.win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!u.win || !u.ren) { std::fprintf(stderr, "[emu] window: %s\n", SDL_GetError()); return 1; }
    SDL_RenderSetLogicalSize(u.ren, kW, kH);

    auto& P = emu::Panel();
    for (bool run = true; run;)
    {
        SDL_Event e;
        while (SDL_PollEvent(&e))
        {
            switch (e.type)
            {
            case SDL_QUIT: run = false; break;
            case SDL_KEYDOWN:
            case SDL_KEYUP:
            {
                const bool down = e.type == SDL_KEYDOWN;
                const SDL_Keycode k = e.key.keysym.sym;
                if (down && (k == SDLK_ESCAPE || k == SDLK_q)) run = false;
                if (k == SDLK_1) u.key_btn[0] = down;
                if (k == SDLK_2) u.key_btn[1] = down;
                if (k == SDLK_3) u.key_btn[2] = down;
                apply_buttons(u);
                break;
            }
            case SDL_MOUSEBUTTONDOWN:
            {
                const int x = e.button.x, y = e.button.y;
                const int p = pot_at(x, y);
                bool gate = false;
                const int j = jack_at(x, y, &gate);
                if (p >= 0)
                {
                    const uint32_t now = SDL_GetTicks();
                    if (p == u.last_click_pot && now - u.last_click < 350) P.pot[p] = 0.5f;
                    u.last_click = now; u.last_click_pot = p;
                    u.drag_pot = p; u.drag_y = y; u.drag_v0 = P.pot[p].load();
                }
                else if (btn_at(x, y) >= 0) { u.mouse_btn = btn_at(x, y); apply_buttons(u); }
                else if (j >= 0 && gate) P.cv_volts[j] = std::fabs(P.cv_volts[j].load() - 5.f) < 0.01f ? 0.f : 5.f;
                else if (j >= 0) { u.drag_jack = j; set_jack_from_y(j, y); }
                break;
            }
            case SDL_MOUSEBUTTONUP:
                u.drag_pot = -1; u.drag_jack = -1;
                if (u.mouse_btn >= 0) { u.mouse_btn = -1; apply_buttons(u); }
                break;
            case SDL_MOUSEMOTION:
                if (u.drag_pot >= 0)
                {
                    float v = u.drag_v0 + (float)(u.drag_y - e.motion.y) / 240.0f;
                    P.pot[u.drag_pot] = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
                }
                if (u.drag_jack >= 0) set_jack_from_y(u.drag_jack, e.motion.y);
                break;
            case SDL_MOUSEWHEEL:
            {
                int mx, my;
                SDL_GetMouseState(&mx, &my);
                float sx = 1.f, sy = 1.f;
                SDL_RenderGetScale(u.ren, &sx, &sy);
                const int p = pot_at((int)(mx / (sx > 0 ? sx : 1)), (int)(my / (sy > 0 ? sy : 1)));
                if (p >= 0)
                {
                    float v = P.pot[p].load() + e.wheel.y * 0.01f;
                    P.pot[p] = v < 0.f ? 0.f : (v > 1.f ? 1.f : v);
                }
                break;
            }
            default: break;
            }
        }
        draw(u);
    }
    SDL_Quit();
    return 0;
}

/* The panel as an image, drawn exactly as the window draws it (headless
 * scripts: `snapshot file.bmp`). */
int emu_ui_snapshot(const char* path)
{
    SDL_Surface* s = SDL_CreateRGBSurfaceWithFormat(0, kW, kH, 32, SDL_PIXELFORMAT_RGBA32);
    if (!s) return 1;
    Ui u;
    u.ren = SDL_CreateSoftwareRenderer(s);
    if (!u.ren) { SDL_FreeSurface(s); return 1; }
    draw(u);
    SDL_Surface* rgb = SDL_ConvertSurfaceFormat(s, SDL_PIXELFORMAT_RGB24, 0);
    const int rc = rgb ? SDL_SaveBMP(rgb, path) : -1;
    if (rgb) SDL_FreeSurface(rgb);
    SDL_DestroyRenderer(u.ren);
    SDL_FreeSurface(s);
    return rc == 0 ? 0 : 1;
}
