/* per/rng.h (emulator): the STM32's hardware RNG, from the host's. */
#pragma once
#include <cstdint>
#include <random>
#include "daisy_core.h"

namespace daisy {
class Random
{
  public:
    static void     Init() {}
    static void     DeInit() {}
    static uint32_t GetValue() { return gen()(); }
    static float    GetFloat(float min = 0.f, float max = 1.f)
    {
        return min + (max - min) * (float)(GetValue() >> 8) / 16777216.0f;
    }
    static bool IsReady() { return true; }
  private:
    static std::mt19937& gen() { static std::mt19937 g{0xA1C4E3u}; return g; }
};
} // namespace daisy
