/*
 * HostLink in the emulator: the SDK's CdcUsbTransport with emulator bodies,
 * so the firmware's own hostlink::Host (descriptor, presets, live state, SD
 * files, extension commands) answers a host exactly as it does over the
 * front USB-C.
 *
 *   native  --hostlink <path>   a Unix socket; one client at a time, served
 *                               from Pump() on the firmware's own thread
 *   web     emu_web_hl_push / emu_web_hl_pull, called by the page between
 *                               the firmware's sleeps (one thread)
 *
 * The bytes are the wire bytes (COBS frames, protocol §1) both ways; nothing
 * here parses them.
 */
#include <cstdio>
#include <cstring>
#include <deque>
#include <string>

#include "emu.h"
#include "alchemy/host_link/cdc_transport.h"

#ifdef __EMSCRIPTEN__
#include <emscripten.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#endif

namespace {
std::deque<uint8_t> g_in;    /* host -> module, not yet Read() */
std::deque<uint8_t> g_out;   /* module -> host, not yet sent    */
bool                g_open = false;   /* a host is listening: keep g_out */

#ifndef __EMSCRIPTEN__
int g_listen = -1;
int g_client = -1;

void DropClient()
{
    if (g_client >= 0) close(g_client);
    g_client = -1;
    g_open   = false;
    g_in.clear();
    g_out.clear();
}

void ServeSocket()
{
    if (g_listen < 0) return;
    if (g_client < 0)
    {
        const int c = accept(g_listen, nullptr, nullptr);
        if (c < 0) return;
        fcntl(c, F_SETFL, fcntl(c, F_GETFL) | O_NONBLOCK);
        int one = 1;
        setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
        g_client = c;
        g_open   = true;
        g_in.clear();
        g_out.clear();
    }
    uint8_t buf[1024];
    for (;;)
    {
        const ssize_t n = recv(g_client, buf, sizeof buf, 0);
        if (n > 0) { g_in.insert(g_in.end(), buf, buf + n); continue; }
        if (n == 0 || (errno != EAGAIN && errno != EWOULDBLOCK)) { DropClient(); return; }
        break;
    }
    while (!g_out.empty())
    {
        size_t k = 0;
        while (k < sizeof buf && k < g_out.size()) { buf[k] = g_out[k]; k++; }
        const ssize_t n = send(g_client, buf, k, 0);
        if (n > 0) { g_out.erase(g_out.begin(), g_out.begin() + n); continue; }
        if (n < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        DropClient();
        return;
    }
}
#endif
} // namespace

namespace emu {
bool HostLinkListen(const char* path)
{
#ifdef __EMSCRIPTEN__
    (void)path;
    return false;
#else
    sockaddr_un a{};
    if (std::strlen(path) >= sizeof a.sun_path) return false;
    a.sun_family = AF_UNIX;
    std::strcpy(a.sun_path, path);
    unlink(path);
    g_listen = socket(AF_UNIX, SOCK_STREAM, 0);
    if (g_listen < 0) return false;
    if (bind(g_listen, (sockaddr*)&a, sizeof a) != 0 || listen(g_listen, 1) != 0)
    {
        close(g_listen);
        g_listen = -1;
        return false;
    }
    fcntl(g_listen, F_SETFL, fcntl(g_listen, F_GETFL) | O_NONBLOCK);
    return true;
#endif
}
} // namespace emu

namespace alchemy { namespace hostlink {
CdcUsbTransport* CdcUsbTransport::s_instance = nullptr;

void CdcUsbTransport::Init(daisy::UsbHandle&, daisy::UsbHandle::UsbPeriph, const char*)
{
    s_instance = this;
}

size_t CdcUsbTransport::Read(uint8_t* dst, size_t max)
{
    size_t n = 0;
    while (n < max && !g_in.empty()) { dst[n++] = g_in.front(); g_in.pop_front(); }
    return n;
}

size_t CdcUsbTransport::Write(const uint8_t* src, size_t n)
{
    /* nobody listening: the bytes go nowhere, as on an unplugged port */
    if (g_open) g_out.insert(g_out.end(), src, src + n);
    return n;
}

size_t CdcUsbTransport::WriteSpace() const
{
    return g_out.size() >= kTxRing ? 0 : kTxRing - g_out.size();
}

void CdcUsbTransport::Pump()
{
#ifndef __EMSCRIPTEN__
    ServeSocket();
#endif
}

void CdcUsbTransport::RxTrampoline(uint8_t*, uint32_t*) {}
void CdcUsbTransport::OnRx(const uint8_t*, uint32_t) {}
}} // namespace alchemy::hostlink

#ifdef __EMSCRIPTEN__
/* The page is the host. It copies request bytes into the exchange buffer and
 * pushes them, and pulls response bytes out of the same buffer (JS reads
 * HEAPU8 at emu_web_hl_buf(); no malloc). The first push is the "port
 * opened" moment; emu_web_hl_close forgets anything half-sent. */
static uint8_t g_xbuf[4096];

extern "C" EMSCRIPTEN_KEEPALIVE uint8_t* emu_web_hl_buf() { return g_xbuf; }
extern "C" EMSCRIPTEN_KEEPALIVE int emu_web_hl_buf_size() { return (int)sizeof g_xbuf; }

extern "C" EMSCRIPTEN_KEEPALIVE void emu_web_hl_push(int n)
{
    if (n < 0) n = 0;
    if (n > (int)sizeof g_xbuf) n = (int)sizeof g_xbuf;
    g_open = true;
    g_in.insert(g_in.end(), g_xbuf, g_xbuf + n);
}

extern "C" EMSCRIPTEN_KEEPALIVE int emu_web_hl_pull()
{
    int n = 0;
    while (n < (int)sizeof g_xbuf && !g_out.empty()) { g_xbuf[n++] = g_out.front(); g_out.pop_front(); }
    return n;
}

extern "C" EMSCRIPTEN_KEEPALIVE void emu_web_hl_close()
{
    g_open = false;
    g_in.clear();
    g_out.clear();
}
#endif
