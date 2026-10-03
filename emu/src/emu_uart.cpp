/*
 * The rear header's USART1 TX (pin 8), as a log of the bytes a firmware sent.
 * A firmware's own UART driver is swapped for an emulator copy that calls
 * UartTx() (emu_midi_out.cpp for seq's chord out); scripts read the log with
 * `expect uart ...` (emu_script.cpp). Always built: a firmware with no UART
 * simply never claims it.
 */
#include <atomic>
#include <mutex>
#include <vector>

#include "emu.h"

namespace {
std::mutex           g_mu;
std::vector<uint8_t> g_log;
std::atomic<bool>    g_claimed{false};
std::atomic<bool>    g_built{false};
} // namespace

void emu::UartBuilt() { g_built = true; }
bool emu::UartHasDriver() { return g_built; }
void emu::UartClaim() { g_claimed = true; }
bool emu::UartClaimed() { return g_claimed; }

void emu::UartTx(uint8_t b)
{
    std::lock_guard<std::mutex> lk(g_mu);
    g_log.push_back(b);
}

std::vector<uint8_t> emu::UartLog()
{
    std::lock_guard<std::mutex> lk(g_mu);
    return g_log;
}
