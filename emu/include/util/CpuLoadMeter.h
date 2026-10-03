/* util/CpuLoadMeter.h (emulator): load = the callback's wall time over the
 * block's duration, so the emulator reports how hard the Mac works, not the
 * M7. The firmware reads it for its B2 alarm and HostLink extras. */
#pragma once
#include <chrono>

namespace daisy {

class CpuLoadMeter
{
  public:
    void Init(float sample_rate, int block_size, float smoothing = 0.01f)
    {
        block_us_ = 1e6f * (float)block_size / sample_rate;
        smooth_   = smoothing;
        Reset();
    }
    void OnBlockStart() { t0_ = std::chrono::steady_clock::now(); }
    void OnBlockEnd()
    {
        const float us = std::chrono::duration<float, std::micro>(
                             std::chrono::steady_clock::now() - t0_).count();
        const float load = us / block_us_;
        avg_ += (load - avg_) * smooth_;
        if (load > max_) max_ = load;
        if (load < min_) min_ = load;
    }
    float GetAvgCpuLoad() const { return avg_; }
    float GetMaxCpuLoad() const { return max_; }
    float GetMinCpuLoad() const { return min_; }
    void  Reset() { avg_ = 0.f; max_ = 0.f; min_ = 1e9f; }

  private:
    std::chrono::steady_clock::time_point t0_;
    float block_us_ = 500.f, smooth_ = 0.01f;
    float avg_ = 0.f, max_ = 0.f, min_ = 0.f;
};

} // namespace daisy
