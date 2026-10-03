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
#include "alchemy/hw/trigger_jack.h"
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
using EmuCv  = EmuCvJack;
using CvJack = EmuCvJack;   /* the SDK's name for it, which firmware uses too */

/* The V2's I2C chips, for firmware that drives them directly -- the SDK makes
 * them public members, as Daisy boards do. The expander keeps its output
 * byte (B3 itself is read through emu_buttons); the MCP4728 stages four codes
 * and LDAC puts them on J3..J6 through the board's calibration, as the SDK's
 * cv_jack.cpp would. Same names and calls as alchemy/hw/{pca9557,mcp4728}.h,
 * minus Init (the board inits them). */
class Pca9557
{
  public:
    bool    SetOutputBit(uint8_t bit, bool level)
    {
        shadow_ = level ? (uint8_t)(shadow_ | (1u << bit)) : (uint8_t)(shadow_ & ~(1u << bit));
        return true;
    }
    bool    ToggleOutputBit(uint8_t bit) { shadow_ ^= (uint8_t)(1u << bit); return true; }
    bool    WriteOutputs(uint8_t value)  { shadow_ = value; return true; }
    bool    ReadInputs(uint8_t& out)     { out = shadow_; return true; }
    bool    ReadInputBit(uint8_t bit, bool& level) { level = (shadow_ >> bit) & 1u; return true; }
    uint8_t OutputShadow() const { return shadow_; }
    bool    Ready() const { return true; }
  private:
    uint8_t shadow_ = 0u;
};

class Mcp4728
{
  public:
    bool WriteAll(uint16_t a, uint16_t b, uint16_t c, uint16_t d)
    {
        staged_[0] = a; staged_[1] = b; staged_[2] = c; staged_[3] = d;
        return true;
    }
    bool    PulseLdac(Pca9557& expander, uint8_t ldac_io);   /* emu_board.cpp */
    bool    Ready() const { return true; }
    uint8_t Address() const { return 0x60u; }
  private:
    uint16_t staged_[4] = {2048u, 2048u, 2048u, 2048u};
};

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

    Pca9557          expander;
    Mcp4728          dac;       /* J3..J6 */
    daisy::DacHandle stm_dac;   /* J7, J8 */

    /* J1, J2: the SDK's own TriggerJack, fed the input blocks before the
     * firmware's callback as the Lab's audio shim does (emu_board.cpp) */
    TriggerJack  triggers[kNumTriggerJacks];
    TriggerJack& j1 = triggers[0];
    TriggerJack& j2 = triggers[1];

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

    /* J1/J2 to the trigger detectors, then the firmware's callback: the
     * input half of the Lab's AudioShim (TriggerJack's friend, as there). */
    static void AudioShim(daisy::AudioHandle::InputBuffer in,
                          daisy::AudioHandle::OutputBuffer out, size_t n);

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
