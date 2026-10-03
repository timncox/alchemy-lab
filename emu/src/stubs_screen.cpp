/*
 * Smack's 1U OLED (screen.h): instead of the SSD1305 over the expansion
 * header, the two lines it would show are kept for the emulator panel (drawn
 * as an OLED strip) and for scripts (`print screen`, `expect screen <text>`).
 */
#include <cstdio>
#include <cstring>
#include <mutex>
#include <string>

#include "screen.h"
#include "emu.h"

namespace {
std::mutex  g_mu;
std::string g_name, g_small, g_big;
bool        g_on = false;
}

namespace screen {
void Init(const char* name)
{
    std::lock_guard<std::mutex> lk(g_mu);
    g_name = name ? name : "";
    g_on   = true;
}
void Show(const char* small_line, const char* big_line)
{
    std::lock_guard<std::mutex> lk(g_mu);
    g_small = small_line ? small_line : "";
    g_big   = big_line ? big_line : "";
}
void Poll(uint32_t) {}
} // namespace screen

bool emu::ScreenLines(std::string* small_line, std::string* big_line)
{
    std::lock_guard<std::mutex> lk(g_mu);
    if (!g_on) return false;
    *small_line = g_small.empty() && g_big.empty() ? g_name : g_small;
    *big_line   = g_big;
    return true;
}
