/*
 * The emulated USART1 (include/per/uart.h): a script's bytes go to the
 * firmware's circular-listen callback in the DMA buffer it gave, a buffer's
 * worth at a time, as the DMA's idle-line interrupt delivers them on the
 * module.
 */
#include <cstring>
#include <mutex>

#include "per/uart.h"

namespace {
std::mutex g_mu;
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
    std::lock_guard<std::mutex> lk(g_mu);
    g_buf = buff; g_size = size; g_cb = cb; g_ctx = ctx;
    return (buff && size && cb) ? Result::OK : Result::ERR;
}

UartHandler::Result UartHandler::DmaListenStop()
{
    std::lock_guard<std::mutex> lk(g_mu);
    g_cb = nullptr;
    return Result::OK;
}

bool UartHandler::IsListening() const
{
    std::lock_guard<std::mutex> lk(g_mu);
    return g_cb != nullptr;
}

} // namespace daisy

size_t emu::UartInject(const uint8_t* data, size_t n)
{
    std::lock_guard<std::mutex> lk(g_mu);
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
