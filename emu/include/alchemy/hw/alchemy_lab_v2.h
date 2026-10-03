/**
 * @file alchemy/hw/alchemy_lab_v2.h (emulator)
 *
 * Shadows the SDK's AlchemyLabV2 with a board whose pots, buttons, CV jacks
 * and LED chain are the emulator's panel. Same class name and public members
 * the firmwares and the SDK framework use (control_loop.h forward-declares
 * `class AlchemyLabV2`), so belt_alchemy.cpp compiles unchanged.
 */
#pragma once

#include <atomic>

#include "daisy_seed.h"
#include "alchemy/hw/alchemy_lab_v2_layout.h"
#include "alchemy/hw/i_button.h"
#include "alchemy/hw/v2_calibration.h"
#include "alchemy/led/led_strip.h"
#include "alchemy/led/panel.h"

namespace emu {

/* The panel's state, written by the UI / script thread, read by firmware. */
struct PanelState
{
    std::atomic<float> pot[alchemy::kNumPots];          /* 0..1, front view */
    std::atomic<bool>  button[alchemy::kNumButtons];    /* B1..B3 held */
    std::atomic<float> cv_volts[alchemy::kNumCvInputs]; /* J3..J8, -5..+5 V */
    std::atomic<float> cv_out[8];        /* J3..J10 as outputs (firmware-driven) */
    std::atomic<bool>  cv_is_out[8];
};
PanelState& Panel();

/* The LED chain as the firmware last Show()ed it. */
struct LedFrame { uint8_t rgb[alchemy::kLedTotal][3]; };
void ReadLeds(LedFrame* out);

} // namespace emu

namespace alchemy {

/* A button that reads the emulator panel. The SDK's real Button debounces;
 * the panel is already clean, so Pressed() is the panel state, sampled once
 * per ProcessAllControls() like the hardware's debouncer would hold it. */
class EmuButton : public IButton
{
  public:
    void  Bind(uint8_t idx) { idx_ = idx; }
    void  Sample(uint32_t now_ms);
    bool  Pressed() const override { return pressed_; }
    bool  RisingEdge() override  { bool r = rise_; rise_ = false; return r; }
    bool  FallingEdge() override { bool f = fall_; fall_ = false; return f; }
    float TimeHeldMs() const override;

  private:
    uint8_t  idx_     = 0;
    bool     pressed_ = false;
    bool     rise_    = false;
    bool     fall_    = false;
    uint32_t since_   = 0;
};

class EmuStrip : public ILedStrip
{
  public:
    void     SetPixel(uint16_t idx, uint8_t r, uint8_t g, uint8_t b) override;
    void     Clear() override;
    void     Show() override;
    bool     Busy() const override { return false; }
    uint16_t NumLeds() const override { return kLedTotal; }
  private:
    uint8_t px_[kLedTotal][3] = {};
};

/* CV jack J3..J10 (index 0..7): an input from the panel (J3..J8) or, once
 * the firmware enables it, an output whose volts the panel shows. Value()
 * 0..1 with 0.5 = 0 V, the SDK's raw convention. */
class EmuCvJack
{
  public:
    void  Bind(uint8_t idx) { idx_ = idx; }
    float Value() const;
    float Volts() const;
    bool  SetVolts(float v);
    bool  StageVolts(float v);
    bool  EnableCvOutput();
    bool  DisableCvOutput();
    bool  IsOutput() const;
  private:
    uint8_t idx_ = 0;
};
using EmuCv = EmuCvJack;

class AlchemyLabV2
{
  public:
    daisy::DaisySeed     seed;
    daisy::AnalogControl pots[kNumPots];
    EmuButton            emu_buttons[kNumButtons];

    struct ButtonArray
    {
        IButton* slots[kNumButtons];
        IButton&       operator[](uint8_t i)       { return *slots[i]; }
        const IButton& operator[](uint8_t i) const { return *slots[i]; }
    };
    ButtonArray buttons {{ &emu_buttons[0], &emu_buttons[1], &emu_buttons[2] }};

    EmuStrip strip;
    LedPanel leds;

    EmuCvJack cv_jacks[8];   /* J3..J10 */
    EmuCvJack& j3 = cv_jacks[0];
    EmuCvJack& j4 = cv_jacks[1];
    EmuCvJack& j5 = cv_jacks[2];
    EmuCvJack& j6 = cv_jacks[3];
    EmuCvJack& j7 = cv_jacks[4];
    EmuCvJack& j8 = cv_jacks[5];
    EmuCvJack& j9 = cv_jacks[6];
    EmuCvJack& j10 = cv_jacks[7];

    struct CvProxy
    {
        EmuCvJack* slots[kNumCvInputs];
        EmuCvJack&       operator[](uint8_t i)       { return *slots[i]; }
        const EmuCvJack& operator[](uint8_t i) const { return *slots[i]; }
    };
    CvProxy cv {{ &cv_jacks[0], &cv_jacks[1], &cv_jacks[2], &cv_jacks[3], &cv_jacks[4], &cv_jacks[5] }};

    void Init(daisy::SaiHandle::Config::SampleRate sample_rate
                  = daisy::SaiHandle::Config::SampleRate::SAI_48KHZ,
              uint32_t block_size = kEngineBlockSamples);
    void ProcessAllControls();
    void StartAudio(daisy::AudioHandle::AudioCallback cb);
    bool FlushCvOutputs() { return true; }

    const HardwareLayout& Layout() const { return kAlchemyLabV2Layout; }
    const ArcGeometry&    Arc()    const { return kAlchemyLabV2ArcGeometry; }
    float                 SampleRate() const { return 48000.0f; }
    size_t                BlockSize()  const { return block_size_; }
    bool I2cReady()      const { return true; }
    bool ExpanderReady() const { return true; }
    bool Mcp4728Ready()  const { return true; }
    bool StmDacReady()   const { return true; }
    bool IsCalibrated()  const { return false; }
    const V2Calibration& Calibration() const { return cal_; }

  private:
    size_t        block_size_ = kEngineBlockSamples;
    V2Calibration cal_        = {};
};

} // namespace alchemy

namespace emu {
/* The firmware's audio callback, once StartAudio() has run (null before). */
daisy::AudioHandle::AudioCallback AudioCb();
size_t                            BlockSize();
}
