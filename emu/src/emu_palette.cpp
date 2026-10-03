/* Controller LED colours as RGB, for drawing and for script checks.
 *
 * Launchpad Mini MK3 palette (approximate): 0 off, 1 dark grey, 2 grey,
 * 3 white; from 4 on, groups of four per hue (+0 pastel, +1 bright, +2 mid,
 * +3 dim): red, orange, yellow, lime, green, spring, mint, teal, cyan, sky,
 * blue, indigo, violet, magenta, pink -- the indices the firmwares use (5 red,
 * 9 orange, 13 yellow, 21 green, 37 cyan, 45 blue, 53 magenta and their +2
 * dims) land on the right hues.
 *
 * Launch Control XL: bicolour, red 0-3 and green 0-3 (launchpad.h xl::Col).
 */
#include <cstdint>

namespace emu {

void LpRgb(uint8_t idx, uint8_t* r, uint8_t* g, uint8_t* b)
{
    if (idx == 0) { *r = *g = *b = 0; return; }
    if (idx < 4)
    {
        const uint8_t v = idx == 1 ? 40 : idx == 2 ? 110 : 255;
        *r = *g = *b = v;
        return;
    }
    static const uint8_t hue[15][3] = {
        {255, 0, 0},   {255, 100, 0}, {255, 220, 0}, {150, 255, 0}, {0, 255, 0},
        {0, 255, 90},  {0, 255, 170}, {0, 220, 200}, {0, 200, 255}, {0, 130, 255},
        {0, 40, 255},  {80, 0, 255},  {160, 0, 255}, {255, 0, 220}, {255, 0, 110},
    };
    const int grp = ((idx - 4) / 4) % 15, lvl = (idx - 4) % 4;
    const float k = lvl == 1 ? 1.0f : lvl == 2 ? 0.45f : lvl == 3 ? 0.18f : 1.0f;
    for (int c = 0; c < 3; c++)
    {
        float v = hue[grp][c] * k;
        if (lvl == 0) v = v * 0.6f + 100.0f;   /* pastel */
        (c == 0 ? *r : c == 1 ? *g : *b) = (uint8_t)(v > 255.f ? 255.f : v);
    }
}

void XlRgb(uint8_t col, uint8_t* r, uint8_t* g, uint8_t* b)
{
    const int red = col & 3, green = (col >> 4) & 3;
    *r = (uint8_t)(red * 85);
    *g = (uint8_t)(green * 85);
    *b = 0;
}

} // namespace emu
