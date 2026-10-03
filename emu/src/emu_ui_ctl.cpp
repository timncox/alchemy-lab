/*
 * The emulated controllers, drawn beside the panel when --usb launchpad
 * plugs them in: Launchpad Mini MK3 (click pads, top row, side column),
 * Launch Control XL (drag knobs and faders, click buttons) and Haute42
 * (click and hold its buttons). LEDs are what the firmware last set.
 */
#include <SDL.h>

#include <cmath>
#include <cstring>

#include "emu.h"

void emu_text(SDL_Renderer* r, int x, int y, int s, const char* t, SDL_Color c);
int  emu_text_width(const char* t, int s);

namespace {

constexpr int kCell = 40;          /* Launchpad pad pitch */
constexpr int kLpX = 30, kLpY = 70;
constexpr int kXlX = 450, kXlY = 70;
constexpr int kPadX = 30, kPadY = 520;

const SDL_Color kInk = {0xE8, 0xE0, 0xD0, 255};
const SDL_Color kDim = {0x90, 0x88, 0x7C, 255};

struct PadBtn { const char* name; uint32_t bit; int x, y; };
/* A leverless layout (GP2040-CE's default positions, roughly): directions
 * under the left hand, Up under the thumb, the eight face buttons in two
 * rows, L3 / R3 above. */
const PadBtn kPad[] = {
    {"LEFT", 1u << 2, 0, 40},   {"DOWN", 1u << 1, 50, 30},  {"RIGHT", 1u << 3, 100, 40},
    {"UP", 1u << 0, 150, 120},
    {"X", 1u << 14, 200, 30},   {"Y", 1u << 15, 250, 20},   {"RB", 1u << 9, 300, 20},  {"LB", 1u << 8, 350, 30},
    {"A", 1u << 12, 200, 80},   {"B", 1u << 13, 250, 70},   {"RT", 1u << 17, 300, 70}, {"LT", 1u << 16, 350, 80},
    {"L3", 1u << 6, 200, -30},  {"R3", 1u << 7, 250, -30},
};
constexpr int kPadN = (int)(sizeof kPad / sizeof kPad[0]);

int  g_x0 = 0;
int  g_drag_knob = -1, g_drag_fader = -1, g_drag_y = 0, g_drag_v0 = 0;
int  g_lp_down = -1;               /* kind*100 + index of the pad held by the mouse */
int  g_xl_btn = -1;
int  g_pad_btn = -1;

void fill_circle(SDL_Renderer* r, int cx, int cy, int rad)
{
    for (int dy = -rad; dy <= rad; dy++)
    {
        const int dx = (int)std::sqrt((float)(rad * rad - dy * dy));
        SDL_RenderDrawLine(r, cx - dx, cy + dy, cx + dx, cy + dy);
    }
}

void set_rgb(SDL_Renderer* r, uint8_t R, uint8_t G, uint8_t B, bool lit)
{
    if (!lit) { SDL_SetRenderDrawColor(r, 0x30, 0x2D, 0x2A, 255); return; }
    SDL_SetRenderDrawColor(r, R, G, B, 255);
}

SDL_Rect lp_rect(int kind, int i)    /* kind 0 grid (i = y*8+x), 1 side, 2 top, 3 logo */
{
    const int ox = g_x0 + kLpX, oy = kLpY;
    if (kind == 2) return {ox + i * kCell + 4, oy + 8, kCell - 8, kCell - 22};
    if (kind == 3) return {ox + 8 * kCell + 4, oy + 4, kCell - 8, kCell - 8};
    if (kind == 1) return {ox + 8 * kCell + 8, oy + kCell + i * kCell + 8, kCell - 16, kCell - 16};
    return {ox + (i % 8) * kCell + 2, oy + kCell + (i / 8) * kCell + 2, kCell - 4, kCell - 4};
}

void xl_knob_xy(int idx, int* x, int* y) { *x = g_x0 + kXlX + 20 + (idx % 8) * 50; *y = kXlY + 40 + (idx / 8) * 52; }
SDL_Rect xl_fader_track(int c) { return {g_x0 + kXlX + 14 + c * 50, kXlY + 210, 12, 150}; }
SDL_Rect xl_btn_rect(int row, int c) { return {g_x0 + kXlX + 4 + c * 50, kXlY + 380 + row * 40, 32, 28}; }

bool in_rect(int x, int y, const SDL_Rect& q) { return x >= q.x && x < q.x + q.w && y >= q.y && y < q.y + q.h; }

} // namespace

int emu_ctl_width() { return emu::ctl::Enabled() ? 880 : 0; }

void emu_ctl_draw(SDL_Renderer* r, int x0, int h)
{
    if (!emu::ctl::Enabled()) return;
    g_x0 = x0;
    SDL_SetRenderDrawColor(r, 0x1E, 0x1C, 0x1A, 255);
    SDL_Rect bg = {x0, 0, 880, h};
    SDL_RenderFillRect(r, &bg);

    /* Launchpad Mini MK3 */
    emu_text(r, x0 + kLpX, 40, 2, "LAUNCHPAD MINI MK3", kInk);
    SDL_SetRenderDrawColor(r, 0x0E, 0x0D, 0x0C, 255);
    SDL_Rect body = {x0 + kLpX - 8, kLpY - 4, 9 * kCell + 12, 9 * kCell + 12};
    SDL_RenderFillRect(r, &body);
    uint8_t R, G, B;
    for (int x = 0; x < 8; x++)
    {
        const uint8_t c = emu::ctl::LpTop(x);
        emu::LpRgb(c, &R, &G, &B);
        set_rgb(r, R, G, B, c != 0);
        SDL_Rect q = lp_rect(2, x);
        SDL_RenderFillRect(r, &q);
    }
    {
        const uint8_t c = emu::ctl::LpLogo();
        emu::LpRgb(c, &R, &G, &B);
        set_rgb(r, R, G, B, c != 0);
        SDL_Rect q = lp_rect(3, 0);
        SDL_RenderFillRect(r, &q);
    }
    for (int y = 0; y < 8; y++)
    {
        for (int x = 0; x < 8; x++)
        {
            const uint8_t c = emu::ctl::LpGrid(x, y);
            emu::LpRgb(c, &R, &G, &B);
            set_rgb(r, R, G, B, c != 0);
            SDL_Rect q = lp_rect(0, y * 8 + x);
            SDL_RenderFillRect(r, &q);
        }
        const uint8_t c = emu::ctl::LpSide(y);
        emu::LpRgb(c, &R, &G, &B);
        set_rgb(r, R, G, B, c != 0);
        SDL_Rect q = lp_rect(1, y);
        fill_circle(r, q.x + q.w / 2, q.y + q.h / 2, q.w / 2);
    }

    /* Haute42 */
    emu_text(r, x0 + kPadX, kPadY - 50, 2, "HAUTE42", kInk);
    const uint32_t held = emu::ctl::PadMask();
    for (int i = 0; i < kPadN; i++)
    {
        const int cx = x0 + kPadX + 20 + kPad[i].x, cy = kPadY + 40 + kPad[i].y;
        const bool on = held & kPad[i].bit;
        SDL_SetRenderDrawColor(r, on ? 0xFF : 0x50, on ? 0xFF : 0x4A, on ? 0xFF : 0x44, 255);
        fill_circle(r, cx, cy, 18);
        emu_text(r, cx - emu_text_width(kPad[i].name, 1) / 2, cy - 3, 1, kPad[i].name, on ? SDL_Color{0, 0, 0, 255} : kInk);
    }

    /* Launch Control XL */
    emu_text(r, x0 + kXlX, 40, 2, "LAUNCH CONTROL XL", kInk);
    for (int i = 0; i < 24; i++)
    {
        int cx, cy;
        xl_knob_xy(i, &cx, &cy);
        const uint8_t led = emu::ctl::XlKnobLed(i / 8, i % 8);
        emu::XlRgb(led, &R, &G, &B);
        set_rgb(r, R, G, B, (led & 0x33) != 0);
        fill_circle(r, cx, cy - 22, 3);
        SDL_SetRenderDrawColor(r, 0x10, 0x0F, 0x0E, 255);
        fill_circle(r, cx, cy, 16);
        SDL_SetRenderDrawColor(r, 0x46, 0x42, 0x3E, 255);
        fill_circle(r, cx, cy, 13);
        const float v = emu::ctl::XlKnobValue(i / 8, i % 8) / 127.0f;
        const float a = (7.0f + v * 10.0f) / 12.0f * 2.0f * (float)M_PI;
        SDL_SetRenderDrawColor(r, 0xF0, 0xE8, 0xD8, 255);
        SDL_RenderDrawLine(r, cx, cy, cx + (int)(std::sin(a) * 11), cy - (int)(std::cos(a) * 11));
    }
    for (int c = 0; c < 8; c++)
    {
        SDL_Rect t = xl_fader_track(c);
        SDL_SetRenderDrawColor(r, 0x10, 0x0F, 0x0E, 255);
        SDL_RenderFillRect(r, &t);
        const int y = t.y + t.h - (int)(emu::ctl::XlFaderValue(c) / 127.0f * t.h);
        SDL_SetRenderDrawColor(r, 0xB0, 0xA8, 0xA0, 255);
        SDL_Rect cap = {t.x - 8, y - 5, t.w + 16, 10};
        SDL_RenderFillRect(r, &cap);
        for (int row = 0; row < 2; row++)
        {
            const uint8_t led = emu::ctl::XlButtonLed(row, c);
            emu::XlRgb(led, &R, &G, &B);
            set_rgb(r, R, G, B, (led & 0x33) != 0);
            SDL_Rect q = xl_btn_rect(row, c);
            SDL_RenderFillRect(r, &q);
        }
    }
    emu_text(r, x0 + kXlX, kXlY + 470, 1, "KNOBS + FADERS: DRAG.  PADS + BUTTONS: CLICK AND HOLD.", kDim);
}

/* Mouse: true if the click landed on a controller. */
bool emu_ctl_mouse_down(int x, int y)
{
    if (!emu::ctl::Enabled()) return false;
    for (int i = 0; i < 8; i++)
        if (in_rect(x, y, lp_rect(2, i))) { g_lp_down = 200 + i; emu::ctl::LpEvent(2, i, 0, true); return true; }
    for (int i = 0; i < 8; i++)
        if (in_rect(x, y, lp_rect(1, i))) { g_lp_down = 100 + i; emu::ctl::LpEvent(1, 0, i, true); return true; }
    for (int i = 0; i < 64; i++)
        if (in_rect(x, y, lp_rect(0, i))) { g_lp_down = i; emu::ctl::LpEvent(0, i % 8, i / 8, true); return true; }
    for (int i = 0; i < 24; i++)
    {
        int cx, cy;
        xl_knob_xy(i, &cx, &cy);
        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= 18 * 18)
        {
            g_drag_knob = i; g_drag_y = y; g_drag_v0 = emu::ctl::XlKnobValue(i / 8, i % 8);
            return true;
        }
    }
    for (int c = 0; c < 8; c++)
    {
        SDL_Rect t = xl_fader_track(c);
        if (x >= t.x - 10 && x < t.x + t.w + 10 && y >= t.y - 6 && y < t.y + t.h + 6)
        {
            g_drag_fader = c;
            emu::ctl::XlFader(c, (int)((t.y + t.h - y) * 127 / t.h));
            return true;
        }
        for (int row = 0; row < 2; row++)
            if (in_rect(x, y, xl_btn_rect(row, c))) { g_xl_btn = row * 8 + c; emu::ctl::XlButton(row, c, true); return true; }
    }
    for (int i = 0; i < kPadN; i++)
    {
        const int cx = g_x0 + kPadX + 20 + kPad[i].x, cy = kPadY + 40 + kPad[i].y;
        if ((x - cx) * (x - cx) + (y - cy) * (y - cy) <= 18 * 18)
        {
            g_pad_btn = i;
            emu::ctl::PadSet(kPad[i].bit, true);
            return true;
        }
    }
    return false;
}

void emu_ctl_mouse_motion(int x, int y)
{
    (void)x;
    if (g_drag_knob >= 0)
        emu::ctl::XlKnob(g_drag_knob / 8, g_drag_knob % 8, g_drag_v0 + (g_drag_y - y));
    if (g_drag_fader >= 0)
    {
        SDL_Rect t = xl_fader_track(g_drag_fader);
        emu::ctl::XlFader(g_drag_fader, (int)((t.y + t.h - y) * 127 / t.h));
    }
}

void emu_ctl_mouse_up()
{
    if (g_lp_down >= 0)
    {
        if (g_lp_down >= 200)      emu::ctl::LpEvent(2, g_lp_down - 200, 0, false);
        else if (g_lp_down >= 100) emu::ctl::LpEvent(1, 0, g_lp_down - 100, false);
        else                       emu::ctl::LpEvent(0, g_lp_down % 8, g_lp_down / 8, false);
        g_lp_down = -1;
    }
    if (g_xl_btn >= 0) { emu::ctl::XlButton(g_xl_btn / 8, g_xl_btn % 8, false); g_xl_btn = -1; }
    if (g_pad_btn >= 0) { emu::ctl::PadSet(kPad[g_pad_btn].bit, false); g_pad_btn = -1; }
    g_drag_knob = -1;
    g_drag_fader = -1;
}
