/*
 * The rear header's USART1, both directions.
 *
 * RX (Belt's rear MIDI in): the emulated per/uart.h. A script's bytes go to
 * the firmware's circular-listen callback in the DMA buffer it gave, a
 * buffer's worth at a time, as the DMA's idle-line interrupt delivers them on
 * the module (`uart <hex bytes>`, emu::UartInject).
 *
 * TX (pin 8, seq's chord out): a log of the bytes a firmware sent. A
 * firmware's own UART driver is swapped for an emulator copy that calls
 * UartTx() (emu_midi_out.cpp for seq's chord out); scripts read the log with
 * `expect uart ...` (emu_script.cpp). Always built: a firmware with no UART
 * simply never claims it.
 */
#include <atomic>
#include <cstring>
#include <mutex>
#include <vector>

#include "emu.h"
#include "per/uart.h"

/* ---- RX ---------------------------------------------------------------- */

namespace {
std::mutex g_rx_mu;
uint8_t*   g_buf  = nullptr;
size_t     g_size = 0;
void*      g_ctx  = nullptr;
daisy::UartHandler::CircularRxCallbackFunctionPtr g_cb = nullptr;
}

namespace daisy {

UartHandler::Result UartHandler::Init(const Config&) { return Result::OK; }

UartHandler::Result UartHandler::DmaListenStart(uint8_t* buff, size_t size,
                                                CircularRxCallbackFunctionPtr cb, void* ctx)
{
    std::lock_guard<std::mutex> lk(g_rx_mu);
    g_buf = buff; g_size = size; g_cb = cb; g_ctx = ctx;
    return (buff && size && cb) ? Result::OK : Result::ERR;
}

UartHandler::Result UartHandler::DmaListenStop()
{
    std::lock_guard<std::mutex> lk(g_rx_mu);
    g_cb = nullptr;
    return Result::OK;
}

bool UartHandler::IsListening() const
{
    std::lock_guard<std::mutex> lk(g_rx_mu);
    return g_cb != nullptr;
}

} // namespace daisy

size_t emu::UartInject(const uint8_t* data, size_t n)
{
    std::lock_guard<std::mutex> lk(g_rx_mu);
    if (!g_cb) return 0;
    size_t done = 0;
    while (done < n)
    {
        const size_t k = (n - done) < g_size ? (n - done) : g_size;
        std::memcpy(g_buf, data + done, k);
        g_cb(g_buf, k, g_ctx, daisy::UartHandler::Result::OK);
        done += k;
    }
    return done;
}

/* ---- TX ---------------------------------------------------------------- */

namespace {
std::mutex           g_tx_mu;
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
    std::lock_guard<std::mutex> lk(g_tx_mu);
    g_log.push_back(b);
}

std::vector<uint8_t> emu::UartLog()
{
    std::lock_guard<std::mutex> lk(g_tx_mu);
    return g_log;
}
