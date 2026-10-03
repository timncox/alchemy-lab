/*
 * The emulated Alchemy Lab: time, the panel's pots / buttons / CV, the LED
 * chain, the QSPI flash image (presets persist across runs), the SD card (a
 * formatted RAM disk) and the audio callback hand-off.
 */
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

#include "alchemy/hw/alchemy_lab_v2.h"
#include "ff.h"
#include "ram_diskio.h"
#include "emu.h"

namespace {

using Clock = std::chrono::steady_clock;
const Clock::time_point g_t0 = Clock::now();

emu::PanelState g_panel;
std::mutex      g_led_mu;
emu::LedFrame   g_leds = {};

daisy::AudioHandle::AudioCallback g_cb = nullptr;      /* what Render() calls */
daisy::AudioHandle::AudioCallback g_user_cb = nullptr; /* the firmware's */
alchemy::AlchemyLabV2*            g_board = nullptr;
std::atomic<uint32_t>             g_edges[8], g_edge_us[8];

size_t                            g_block = 24;

/* Flash image the preset store lives in; persisted to emu::FlashPath().
 * It must hold every slot: 16 slots x 2 ping-pong sides x 5 sectors x 4 KB
 * = 640 KB. (It was 256 KB: the home slots 12-14 then wrote past the end --
 * in Mark's binary straight into its engine pool, which crashed Mark.) */
constexpr size_t kFlashBytes = 1024u * 1024u;
static_assert(kFlashBytes >= 16u * 2u * alchemy::kPresetSectorSize * alchemy::kPresetSectorsPerSide,
              "the emulated flash must hold every preset slot");
alignas(4096) uint8_t g_flash[kFlashBytes];

} // namespace

namespace emu {

uint32_t NowMs()
{
    return (uint32_t)std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - g_t0).count();
}
uint32_t NowUs()
{
    return (uint32_t)std::chrono::duration_cast<std::chrono::microseconds>(Clock::now() - g_t0).count();
}
void SleepMs(uint32_t ms)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
}

PanelState& Panel() { return g_panel; }

/* CV volts -> the ADC code / raw value. 0 V = mid-scale; +5 V = full scale. */
static float volts_to_raw(float v)
{
    float r = 0.5f + v / 10.0f;
    return r < 0.f ? 0.f : (r > 1.f ? 1.f : r);
}

uint16_t AdcRaw(uint8_t ch)
{
    if (ch >= alchemy::kCvAdcOffset && ch < alchemy::kCvAdcOffset + alchemy::kNumCvInputs)
        return (uint16_t)(volts_to_raw(g_panel.cv_volts[ch - alchemy::kCvAdcOffset].load()) * 65535.0f);
    if (ch < alchemy::kNumPots) return (uint16_t)(g_panel.pot[ch].load() * 65535.0f);
    return 32768u;
}

void RebootRequested(const char* why)
{
    std::fprintf(stderr, "[emu] firmware asked to reboot (%s) -- exiting\n", why);
    std::fflush(stderr);
    std::_Exit(3);
}

void ReadLeds(LedFrame* out)
{
    std::lock_guard<std::mutex> lk(g_led_mu);
    *out = g_leds;
}

daisy::AudioHandle::AudioCallback AudioCb() { return g_cb; }
uint32_t                          ShowCount();
size_t                            BlockSize() { return g_block; }

uintptr_t PresetFlashBase() { return reinterpret_cast<uintptr_t>(g_flash); }

void LoadFlash()
{
    std::memset(g_flash, 0xFF, sizeof g_flash);
    if (FILE* f = std::fopen(FlashPath().c_str(), "rb"))
    {
        size_t n = std::fread(g_flash, 1, sizeof g_flash, f);
        (void)n;
        std::fclose(f);
    }
}

void FormatCard()
{
    /* FAT32 like the cards the firmwares expect; FAT32 needs >= 65,525
     * clusters, so 64 MB of 512-byte clusters */
    ramdisk::Reset(131072u);
    static uint8_t work[4096];
    const FRESULT fr = f_mkfs("0:", FM_FAT32 | FM_SFD, 512u, work, sizeof work);
    if (fr != FR_OK) std::fprintf(stderr, "[emu] card format failed (%d)\n", (int)fr);
}

/* Copy a host directory onto the emulated card (recursively), e.g. the
 * Elements sample file at mi/elements.smp. */
void ListHostDir(const std::string& path, std::vector<std::pair<std::string, bool>>& out);

static int copy_tree(const std::string& host, const std::string& card)
{
    int n = 0;
    std::vector<std::pair<std::string, bool>> ents;
    ListHostDir(host, ents);
    for (const auto& ent : ents)
    {
        const std::string hp = host + "/" + ent.first, cp = card + "/" + ent.first;
        if (ent.second)
        {
            f_mkdir(cp.c_str());
            n += copy_tree(hp, cp);
            continue;
        }
        FILE* in = std::fopen(hp.c_str(), "rb");
        FIL   out;
        if (!in) continue;
        if (f_open(&out, cp.c_str(), FA_WRITE | FA_CREATE_ALWAYS) == FR_OK)
        {
            static uint8_t buf[8192];
            size_t got;
            while ((got = std::fread(buf, 1, sizeof buf, in)) > 0)
            {
                UINT w = 0;
                f_write(&out, buf, (UINT)got, &w);
            }
            f_close(&out);
            n++;
        }
        std::fclose(in);
    }
    return n;
}

void FillCard(const std::string& host_dir)
{
    static FATFS fs;
    const FRESULT fr = f_mount(&fs, "0:", 1);
    if (fr != FR_OK) { std::fprintf(stderr, "[emu] card mount failed (%d)\n", (int)fr); return; }
    const int n = copy_tree(host_dir, "0:");
    f_mount(nullptr, "0:", 0);
    std::fprintf(stderr, "[emu] card: %d file(s) from %s\n", n, host_dir.c_str());
}

} // namespace emu

void daisy::QSPIHandle::Persist()
{
    if (emu::FlashPath().empty()) return;
    if (FILE* f = std::fopen(emu::FlashPath().c_str(), "wb"))
    {
        std::fwrite(g_flash, 1, sizeof g_flash, f);
        std::fclose(f);
    }
}

namespace alchemy {

void EmuButton::Sample(uint32_t now_ms)
{
    const bool p = g_panel.button[idx_].load();
    if (p && !pressed_) { rise_ = true; since_ = now_ms; }
    if (!p && pressed_) fall_ = true;
    pressed_ = p;
}

float EmuButton::TimeHeldMs() const
{
    return pressed_ ? (float)(emu::NowMs() - since_) : 0.0f;
}

void EmuStrip::SetPixel(uint16_t idx, uint8_t r, uint8_t g, uint8_t b)
{
    if (idx >= kLedTotal) return;
    px_[idx][0] = r; px_[idx][1] = g; px_[idx][2] = b;
}

void EmuStrip::Clear() { std::memset(px_, 0, sizeof px_); }

static std::atomic<uint32_t> g_shows{0};

void EmuStrip::Show()
{
    std::lock_guard<std::mutex> lk(g_led_mu);
    std::memcpy(g_leds.rgb, px_, sizeof px_);
    g_shows++;
}

float EmuCvJack::Volts() const
{
    if (g_panel.cv_is_out[idx_].load()) return g_panel.cv_out[idx_].load();
    return idx_ < kNumCvInputs ? g_panel.cv_volts[idx_].load() : 0.0f;
}
float EmuCvJack::Value() const { return emu::volts_to_raw(Volts()); }
static void out_volts(uint8_t idx, float v)
{
#ifdef EMU_CODEC_V011_COMPENSATED
    /* J9 / J10 as the jack sees them: the v0.11 SDK's sample = volts / 5,
     * through the Seed2 DFM's inverting DC output, -8.667 V per +1 sample
     * (alchemy-sdk 4eb0e4c). Set for a firmware that pre-scales its codec
     * volts for that (fw/seq.mk), so `expect cv 10` reads jack volts. */
    if (idx >= alchemy::kNumCvInputs) v = v / 5.0f * (-4.0f * 39.0f / 18.0f);
#endif
    if (g_panel.cv_out[idx].load() < 2.5f && v >= 2.5f)
    {
        g_edges[idx]++;
        g_edge_us[idx] = emu::NowUs();
    }
    g_panel.cv_out[idx] = v;
}
bool  EmuCvJack::SetVolts(float v)   { out_volts(idx_, v); return true; }
bool  EmuCvJack::StageVolts(float v) { out_volts(idx_, v); return true; }
bool  EmuCvJack::EnableCvOutput()    { g_panel.cv_is_out[idx_] = true; return true; }
bool  EmuCvJack::DisableCvOutput()   { g_panel.cv_is_out[idx_] = false; return true; }
bool  EmuCvJack::IsOutput() const    { return g_panel.cv_is_out[idx_].load(); }

void AlchemyLabV2::Init(daisy::SaiHandle::Config::SampleRate, uint32_t block_size)
{
    block_size_ = block_size;
    g_block     = block_size;
    for (uint8_t i = 0; i < kNumButtons; i++) emu_buttons[i].Bind(i);
    for (uint8_t i = 0; i < 8; i++) cv_jacks[i].Bind(i);
    leds.Init(strip, kAlchemyLabV2Layout);
    ProcessAllControls();
}

void AlchemyLabV2::ProcessAllControls()
{
    const uint32_t now = emu::NowMs();
    for (uint8_t i = 0; i < kNumPots; i++) pots[i].SetValue(g_panel.pot[i].load());
    for (uint8_t i = 0; i < kNumButtons; i++) emu_buttons[i].Sample(now);
}

void AlchemyLabV2::StartAudio(daisy::AudioHandle::AudioCallback cb)
{
    g_board   = this;
    g_user_cb = cb;
    g_cb      = &AlchemyLabV2::AudioShim;
}

/* The Lab's audio shim, as far as inputs go (alchemy_lab_v2.cpp AudioShim). */
void AlchemyLabV2::AudioShim(daisy::AudioHandle::InputBuffer in,
                             daisy::AudioHandle::OutputBuffer out, size_t n)
{
    g_board->triggers[0].ProcessBlock(in[kCodecInChJ1], n);
    g_board->triggers[1].ProcessBlock(in[kCodecInChJ2], n);
    if (g_user_cb) g_user_cb(in, out, n);
}

} // namespace alchemy

uint32_t emu::ShowCount() { return alchemy::g_shows.load(); }
uint32_t emu::Edges(int idx) { return idx >= 0 && idx < 8 ? g_edges[idx].load() : 0u; }
uint32_t emu::LastEdgeUs(int idx) { return idx >= 0 && idx < 8 ? g_edge_us[idx].load() : 0u; }

void emu::WriteCardFile(const char* dir, const char* path, const char* text)
{
    static FATFS fs;
    FRESULT fr = f_mount(&fs, "0:", 1);
    if (fr != FR_OK) { std::fprintf(stderr, "[emu] card mount failed (%d)\n", (int)fr); return; }
    f_mkdir(dir);
    FIL f;
    fr = f_open(&f, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr == FR_OK)
    {
        UINT w = 0;
        f_write(&f, text, (UINT)std::strlen(text), &w);
        f_close(&f);
        std::fprintf(stderr, "[emu] card: wrote %s\n", path);
    }
    else std::fprintf(stderr, "[emu] card: cannot write %s (%d)\n", path, (int)fr);
    f_mount(nullptr, "0:", 0);
}
