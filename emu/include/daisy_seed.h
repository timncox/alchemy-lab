/**
 * @file daisy_seed.h (emulator)
 *
 * libDaisy's public surface as the Alchemy firmwares use it, backed by the
 * emulator instead of a Seed: real time, an ADC whose channels the emulator
 * panel drives, a QSPI that writes the emulator's flash image, and audio
 * started by the emulator's sound card. Grown from alchemy-sdk/stubs (the
 * SDK's host-test stub, where every body is a no-op).
 */

#pragma once

#include <cstddef>
#include <cstdint>
#include <cstring>

#include "daisy_core.h"
#include "stm32h7xx_hal.h"

/* Section placement attributes: plain globals on the host. */
#ifndef DSY_SDRAM_BSS
#define DSY_SDRAM_BSS
#endif
#ifndef DSY_QSPI_BSS
#define DSY_QSPI_BSS
#endif
#ifndef DSY_DTCMRAM
#define DSY_DTCMRAM
#endif

/* Cortex-M7 cache maintenance: nothing to do on the host. */
inline void SCB_InvalidateDCache_by_Addr(void*, int32_t) {}
inline void SCB_InvalidateDCache_by_Addr(volatile void*, int32_t) {}
inline void SCB_CleanDCache_by_Addr(void*, int32_t) {}
inline void SCB_CleanDCache_by_Addr(volatile void*, int32_t) {}
inline void SCB_DisableDCache() {}
inline void SCB_EnableDCache() {}

namespace emu {
/* Implemented in emu_board.cpp. */
uint32_t NowMs();
uint32_t NowUs();
void     SleepMs(uint32_t ms);
uint16_t AdcRaw(uint8_t ch);              /* 0..65535 */
void     RebootRequested(const char* why);
}

namespace daisy {

using Pin = uint32_t;

namespace seed {
    static constexpr Pin D0  =  0, D1  =  1, D2  =  2, D3  =  3;
    static constexpr Pin D4  =  4, D5  =  5, D6  =  6, D7  =  7;
    static constexpr Pin D8  =  8, D9  =  9, D10 = 10, D11 = 11;
    static constexpr Pin D12 = 12, D13 = 13, D14 = 14, D15 = 15;
    static constexpr Pin D16 = 16, D17 = 17, D18 = 18, D19 = 19;
    static constexpr Pin D20 = 20, D21 = 21, D22 = 22, D23 = 23;
    static constexpr Pin D24 = 24, D25 = 25, D26 = 26, D27 = 27;
    static constexpr Pin D28 = 28, D29 = 29, D30 = 30, D31 = 31;
    static constexpr Pin D32 = 32;
} // namespace seed

struct SaiHandle {
    struct Config {
        enum class SampleRate : uint8_t {
            SAI_8KHZ  = 0,
            SAI_16KHZ = 1,
            SAI_32KHZ = 2,
            SAI_48KHZ = 3,
            SAI_96KHZ = 4,
        };
    };
};

struct AudioHandle {
    using InputBuffer   = const float* const*;
    using OutputBuffer  = float**;
    using AudioCallback = void (*)(InputBuffer, OutputBuffer, size_t);
};

struct AdcChannelConfig {
    void InitSingle(Pin) {}
};

struct AdcHandle {
    void      Init(AdcChannelConfig*, uint8_t) {}
    void      Start()                          {}
    uint16_t* GetPtr(uint8_t)                  { return nullptr; }
    uint16_t  Get(uint8_t ch) const            { return emu::AdcRaw(ch); }
    float     GetFloat(uint8_t ch) const       { return emu::AdcRaw(ch) / 65535.f; }
};

/* Addresses are host pointers into the emulator's flash image (the preset
 * store's base is rebased onto it, see emu/src/presets_emu.cpp). */
struct QSPIHandle {
    enum class Result { OK, ERR };
    Result Erase(uintptr_t start, uintptr_t end)
    {
        if (end > start) std::memset(reinterpret_cast<void*>(start), 0xFF, end - start);
        Persist();
        return Result::OK;
    }
    Result Write(uintptr_t addr, uint32_t size, uint8_t* buf)
    {
        std::memcpy(reinterpret_cast<void*>(addr), buf, size);
        Persist();
        return Result::OK;
    }
    static void Persist();   /* emu_board.cpp: flash image -> file */
};

/* Functional, as in the SDK stub: Value() = raw/65535, flip/invert. */
struct AnalogControl {
    void Init(uint16_t* adcptr, float, bool flip, bool invert, float)
    {
        raw_ = adcptr; flip_ = flip; invert_ = invert; val_ = 0.f;
    }
    void Process()
    {
        if (!raw_) return;
        float t = static_cast<float>(*raw_) / 65535.f;
        if (flip_)   t = 1.f - t;
        if (invert_) t = 1.f - t;
        val_ = t;
    }
    float Value() const { return val_; }
    void  SetValue(float v) { val_ = v; }   /* emulator: the panel's pot */

  private:
    uint16_t* raw_    = nullptr;
    bool      flip_   = false;
    bool      invert_ = false;
    float     val_    = 0.f;
};

struct Switch {
    void  Init(Pin)            {}
    void  Debounce()           {}
    bool  RisingEdge()  const  { return false; }
    bool  FallingEdge() const  { return false; }
    bool  Pressed()     const  { return false; }
    float TimeHeldMs()  const  { return 0.f; }
};

struct System {
    enum class BootloaderMode { STM, DAISY, DAISY_SKIP_TIMEOUT, DAISY_INFINITE_TIMEOUT };
    enum class MemoryRegion { INTERNAL_FLASH, ITCMRAM, DTCMRAM, SRAM_D1, SRAM_D2, SRAM_D3, SDRAM, QSPI, INVALID_ADDRESS };
    static uint32_t GetNow()        { return emu::NowMs(); }
    static uint32_t GetUs()         { return emu::NowUs(); }
    static uint32_t GetTick()       { return emu::NowUs(); }
    static uint32_t GetTickFreq()   { return 1000000u; }
    static void     Delay(uint32_t ms)   { emu::SleepMs(ms); }
    static void     DelayUs(uint32_t us) { if (us >= 1000u) emu::SleepMs(us / 1000u); }
    static void     ResetToBootloader(BootloaderMode = BootloaderMode::STM)
    {
        emu::RebootRequested("reset to bootloader");
    }
    static MemoryRegion GetProgramMemoryRegion() { return MemoryRegion::SRAM_D1; }
};

struct DaisySeed {
    AdcHandle  adc;
    QSPIHandle qspi;

    void Configure() {}
    void Init(bool = false) {}

    void SetAudioBlockSize(uint32_t)                       {}
    void SetAudioSampleRate(SaiHandle::Config::SampleRate) {}
    void StartAudio(AudioHandle::AudioCallback)            {}

    float  AudioSampleRate()         { return 48000.f; }
    float  AudioCallbackRate() const { return 48000.f / 24.f; }
    size_t AudioBlockSize()          { return 24u; }
};

} // namespace daisy

using daisy::System;
