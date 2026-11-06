/*
 * The front USB-C's device side, over TCP: `--hostlink <port>`.
 *
 * On the module, Phone mode makes the front USB-C a class-compliant USB MIDI
 * device carrying HostLink inside SysEx (the firmware's own
 * midi_usb_transport + hlmidi.h). Here the USB device under it is replaced:
 * hid/usb.h's UsbHandle (and usb_audio.h's MIDI for Phone+Audio) end in
 * this file, and a TCP client on 127.0.0.1:<port> is the USB host -- the
 * iPhone app in the Simulator, or tools/hostlink_client.py.
 *
 * On the wire: plain MIDI bytes, both ways, nothing else. The client sends
 * exactly the SysEx it would send to the module (F0 7D 48 4C 01 ... F7) and
 * reads the module's SysEx back the same way. The USB-MIDI 1.0 event
 * packets the firmware sees and sends are made and unmade here (cable 0;
 * SysEx as CIN 4..7, real-time and anything else as single bytes, CIN F).
 *
 * One client at a time; a new connection replaces the old one (and clears
 * whatever was still queued for it). Not connected, the firmware's
 * transmits are accepted and dropped, as a cable with no host would.
 *
 * Threads: Poll() runs on the main loop (window frames, script waits) and
 * only moves bytes between the socket and two locked queues. The firmware's
 * receive callback is called by Service(), from SleepMs on the firmware's
 * own thread -- the way the USB interrupt cuts into its main loop -- so the
 * firmware's lock-free receive ring keeps the single producer it was
 * written for. Transmit() is called by the firmware's Pump() on that thread.
 *
 * The browser build has no sockets: everything here is inert there.
 */
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <vector>

#ifndef __EMSCRIPTEN__
#include <arpa/inet.h>
#include <cerrno>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#include "hid/usb.h"
#include "emu.h"

namespace {

std::mutex g_mu;
/* raw MIDI bytes: from the client, not yet given to the firmware; and from
 * the firmware, not yet sent to the client. Capped: past the cap the socket
 * stops being read (TCP pushes back on the client) or Transmit says busy
 * (the firmware keeps its chunk and retries, as with a busy endpoint). */
std::vector<uint8_t> g_rx, g_tx;
constexpr size_t     kRxCap = 64u * 1024u, kTxCap = 64u * 1024u;

std::atomic<bool> g_attached{false};
std::atomic<bool> g_client{false};      /* a client is connected */
std::atomic<bool> g_rx_reset{false};    /* new client: the packer starts over */
std::atomic<void (*)(uint8_t*, uint32_t*)>       g_rx_cb{nullptr};
std::atomic<void (*)(const uint8_t*, uint32_t)>  g_rx_audio_cb{nullptr};

thread_local bool g_fw_thread = false;

/* USB-MIDI 1.0: data bytes each code index number carries. */
constexpr uint8_t kCinSize[16] = {0, 0, 2, 3, 3, 1, 2, 3, 3, 3, 3, 3, 2, 2, 3, 1};

/* --- raw MIDI -> USB-MIDI event packets (firmware thread only) --- */
struct Packer
{
    uint8_t sx[3];
    int     n      = 0;
    bool    in_sx  = false;

    template <typename Out>
    void Single(uint8_t b, Out&& out)
    {
        const uint8_t p[4] = {0x0Fu, b, 0u, 0u};
        out(p);
    }
    template <typename Out>
    void FlushPartial(Out&& out)   /* an abandoned SysEx: its bytes, one by one */
    {
        for (int i = 0; i < n; i++) Single(sx[i], out);
        n = 0;
        in_sx = false;
    }
    template <typename Out>
    void Byte(uint8_t b, Out&& out)
    {
        if (b >= 0xF8u) { Single(b, out); return; }          /* real-time, anywhere */
        if (b == 0xF0u)
        {
            if (in_sx) FlushPartial(out);
            in_sx = true;
            sx[0] = b;
            n     = 1;
            return;
        }
        if (in_sx && b == 0xF7u)
        {
            sx[n++] = b;
            const uint8_t p[4] = {(uint8_t)(0x04u + n), sx[0], n > 1 ? sx[1] : (uint8_t)0u,
                                  n > 2 ? sx[2] : (uint8_t)0u};
            out(p);
            n     = 0;
            in_sx = false;
            return;
        }
        if (in_sx && b < 0x80u)
        {
            sx[n++] = b;
            if (n == 3)
            {
                const uint8_t p[4] = {0x04u, sx[0], sx[1], sx[2]};
                out(p);
                n = 0;
            }
            return;
        }
        if (in_sx) FlushPartial(out);   /* another status ends it */
        Single(b, out);                 /* notes etc.: the firmware ignores them */
    }
};
Packer g_packer;

#ifndef __EMSCRIPTEN__
int g_listen = -1, g_conn = -1, g_port = 0;

void set_nonblock(int fd) { fcntl(fd, F_SETFL, fcntl(fd, F_GETFL, 0) | O_NONBLOCK); }

void drop_client(const char* why)
{
    if (g_conn < 0) return;
    close(g_conn);
    g_conn = -1;
    g_client = false;
    std::fprintf(stderr, "[emu] hostlink: client %s\n", why);
}
#endif

} // namespace

/* ------------------------------------------------------ firmware side ---- */

void emu::usbdev::Attach() { g_attached = true; }
void emu::usbdev::Detach() { g_attached = false; }
void emu::usbdev::SetRx(void (*fn)(uint8_t*, uint32_t*)) { g_rx_cb = fn; }
void emu::usbdev::SetRxAudio(void (*fn)(const uint8_t*, uint32_t)) { g_rx_audio_cb = fn; }
void emu::usbdev::MarkFirmwareThread() { g_fw_thread = true; }

bool emu::usbdev::Transmit(const uint8_t* p, size_t len)
{
    if (!g_client.load()) return true;   /* no host on the cable: gone */
    std::lock_guard<std::mutex> lk(g_mu);
    if (g_tx.size() + len > kTxCap) return false;   /* busy: the firmware retries */
    for (size_t i = 0; i + 4u <= len; i += 4u)
    {
        const uint8_t n = kCinSize[p[i] & 0x0Fu];
        for (uint8_t k = 0; k < n; k++) g_tx.push_back(p[i + 1u + k]);
    }
    return true;
}

void emu::usbdev::Service()
{
    if (!g_fw_thread || !g_attached.load()) return;
    auto cb  = g_rx_cb.load();
    auto acb = g_rx_audio_cb.load();
    if (!cb && !acb) return;
    if (g_rx_reset.exchange(false)) g_packer = Packer{};

    /* At most 256 MIDI bytes per call (SleepMs runs about every millisecond
     * in a control loop): ~250 kB/s, far above what HostLink needs, and
     * well inside the firmware's receive ring between two of its reads. */
    uint8_t raw[256];
    size_t  n = 0;
    {
        std::lock_guard<std::mutex> lk(g_mu);
        if (g_rx.empty()) return;
        n = g_rx.size() < sizeof raw ? g_rx.size() : sizeof raw;
        std::memcpy(raw, g_rx.data(), n);
        g_rx.erase(g_rx.begin(), g_rx.begin() + (long)n);
    }
    /* Bulk transfers of up to 64 bytes (16 packets), as on full-speed USB. */
    uint8_t  buf[64];
    uint32_t len = 0;
    auto flush = [&]() {
        if (!len) return;
        uint32_t l = len;
        if (cb) cb(buf, &l);
        else acb(buf, l);
        len = 0;
    };
    auto out = [&](const uint8_t* p) {
        std::memcpy(buf + len, p, 4);
        len += 4;
        if (len == sizeof buf) flush();
    };
    for (size_t i = 0; i < n; i++) g_packer.Byte(raw[i], out);
    flush();
}

/* ---------------------------------------------------------- TCP side ---- */

#ifndef __EMSCRIPTEN__
bool emu::hostlink::Start(int port)
{
    g_listen = socket(AF_INET, SOCK_STREAM, 0);
    if (g_listen < 0) { std::perror("[emu] hostlink socket"); return false; }
    int one = 1;
    setsockopt(g_listen, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    sockaddr_in a = {};
    a.sin_family      = AF_INET;
    a.sin_port        = htons((uint16_t)port);
    a.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(g_listen, (sockaddr*)&a, sizeof a) != 0 || listen(g_listen, 1) != 0)
    {
        std::fprintf(stderr, "[emu] hostlink: cannot listen on 127.0.0.1:%d (%s)\n", port, std::strerror(errno));
        close(g_listen);
        g_listen = -1;
        return false;
    }
    set_nonblock(g_listen);
    g_port = port;
    std::printf("HostLink on 127.0.0.1:%d\n", port);
    std::fflush(stdout);
    return true;
}

void emu::hostlink::Poll()
{
    if (g_listen < 0) return;

    /* a new client replaces the old one */
    for (;;)
    {
        const int c = accept(g_listen, nullptr, nullptr);
        if (c < 0) break;
        drop_client("replaced");
        set_nonblock(c);
        int one = 1;
        setsockopt(c, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);
#ifdef SO_NOSIGPIPE
        setsockopt(c, SOL_SOCKET, SO_NOSIGPIPE, &one, sizeof one);
#endif
        {
            std::lock_guard<std::mutex> lk(g_mu);
            g_rx.clear();
            g_tx.clear();
        }
        g_rx_reset = true;
        g_conn     = c;
        g_client   = true;
        std::fprintf(stderr, "[emu] hostlink: client connected on :%d\n", g_port);
    }
    if (g_conn < 0) return;

    /* client -> firmware */
    for (;;)
    {
        {
            std::lock_guard<std::mutex> lk(g_mu);
            if (g_rx.size() >= kRxCap) break;   /* let TCP push back */
        }
        uint8_t buf[4096];
        const ssize_t r = recv(g_conn, buf, sizeof buf, 0);
        if (r > 0)
        {
            std::lock_guard<std::mutex> lk(g_mu);
            g_rx.insert(g_rx.end(), buf, buf + r);
            continue;
        }
        if (r == 0) { drop_client("disconnected"); return; }
        if (errno == EINTR) continue;
        if (errno != EAGAIN && errno != EWOULDBLOCK) { drop_client("lost"); return; }
        break;
    }

    /* firmware -> client */
    std::lock_guard<std::mutex> lk(g_mu);
    while (!g_tx.empty())
    {
#ifdef MSG_NOSIGNAL
        const ssize_t w = send(g_conn, g_tx.data(), g_tx.size(), MSG_NOSIGNAL);
#else
        const ssize_t w = send(g_conn, g_tx.data(), g_tx.size(), 0);
#endif
        if (w > 0) { g_tx.erase(g_tx.begin(), g_tx.begin() + w); continue; }
        if (w < 0 && errno == EINTR) continue;
        if (w < 0 && (errno == EAGAIN || errno == EWOULDBLOCK)) break;
        g_tx.clear();
        close(g_conn);
        g_conn   = -1;
        g_client = false;
        std::fprintf(stderr, "[emu] hostlink: client lost\n");
        break;
    }
}
#else
bool emu::hostlink::Start(int) { return false; }
void emu::hostlink::Poll() {}
#endif
