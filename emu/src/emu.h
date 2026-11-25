/* Emulator-internal interfaces shared by emu_board / emu_main / emu_ui. */
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace emu {

/* Where the QSPI flash image (presets) is kept between runs. Empty = none. */
const std::string& FlashPath();
void               SetFlashPath(const std::string& p);
void               LoadFlash();
uintptr_t          PresetFlashBase();
void               FormatCard();
void               FillCard(const std::string& host_dir);
void               WriteCardFile(const char* dir, const char* path, const char* text);

/* The firmware's OLED lines, if it has a screen (stubs_screen.cpp; a weak
 * default in emu_stubs.cpp returns false). */
bool ScreenLines(std::string* small_line, std::string* big_line);

/* J1 / J2 (jack 0 / 1): a 5 ms pulse train every period_ms (0 = off) and
 * one-shot pulses, mixed into the inputs the firmware sees (emu_main.cpp). */
void  SetClock(int jack, float period_ms);
void  Pulse(int jack);
float PulseSample(int jack);   /* the next input sample's pulse, 0 or 1 */
/* Rising edges (through 2.5 V) a firmware output has made on J3..J10
 * (idx 0..7), and when the last one was (NowUs). */
uint32_t Edges(int idx);
uint32_t LastEdgeUs(int idx);

/* LED frames the firmware has Show()n (a stalled control loop stops it). */
uint32_t ShowCount();

/* Launchpad / XL / gamepad emulation hooks (emu_ctl.cpp). */
void PadSetButtons(uint32_t mask);
namespace ctl {
void    Enable(bool on);          /* --usb launchpad */
bool    Enabled();
/* Launchpad Mini MK3: kind 0 grid (x 0-7, y 0-7 top-down), 1 side (y), 2 top (x) */
void    LpEvent(int kind, int x, int y, bool down);
uint8_t LpGrid(int x, int y);
uint8_t LpTop(int x);
uint8_t LpSide(int y);
uint8_t LpLogo();
/* Launch Control XL: knobs 3 x 8, faders 8, buttons 2 x 8 */
void    XlKnob(int row, int col, int v);
void    XlFader(int col, int v);
uint8_t XlKnobValue(int row, int col);
uint8_t XlFaderValue(int col);
void    XlButton(int row, int col, bool down);
uint8_t XlKnobLed(int row, int col);
uint8_t XlButtonLed(int row, int col);
/* Haute42: XInput bits (launchpad.h pad::k*) */
void     PadSet(uint32_t bit, bool down);
uint32_t PadMask();
} // namespace ctl
/* The front USB-C's device side over TCP (emu_hostlink.cpp, --hostlink).
 * Start: listen on 127.0.0.1:port, print "HostLink on 127.0.0.1:<port>".
 * Poll: accept / read / write without blocking, from the main loop (the
 * window's frames, the script's waits). Service: hand what the client sent
 * to the firmware's receive callback; SleepMs calls it, and it acts only
 * on the firmware's thread (marked by MarkFirmwareThread). */
namespace hostlink {
bool Start(int port);
void Poll();
} // namespace hostlink
namespace usbdev {
void MarkFirmwareThread();
void Service();
} // namespace usbdev

/* A USB-MIDI device on the front USB-C (emu_midi.cpp): the script's `midi`
 * lines Send; a firmware's emulator shim Pops the packets (cable|CIN,
 * status, d1, d2, byte 0 low) as its USB host would hand them over. The
 * first Send plugs the device in; Plug(false) pulls it out. */
namespace midi {
void     Send(uint8_t status, uint8_t d1, uint8_t d2);
void     Plug(bool on);
bool     Plugged();
/* The far end is a computer, an iPhone or an iPad (script `midi plug
 * computer`): for a firmware whose port can be a USB device too; `midi plug`
 * is a keyboard (the firmware is the host). Messages arrive the same way. */
void     PlugComputer(bool on);
bool     Computer();
bool     Pop(uint32_t* pkt);
uint32_t Count();   /* packets the firmware has taken */
/* SysEx both ways (script `midi sysex`, `expect sysexout`): a whole message
 * (F0 .. F7) queued as packets; Out() takes the firmware's packets back to
 * the computer, and Last() is the newest whole SysEx it sent, F0 .. F7. */
void     SendSysex(const uint8_t* msg, int n);
void     Out(uint32_t pkt);
int      LastOut(uint8_t* buf, int cap);
uint32_t OutCount();   /* whole SysEx messages the firmware has sent */
/* Control changes the firmware sent back (any channel; script `expect ccout`,
 * `expect cccount`, `print ccout`): the newest value of a CC, -1 if none yet,
 * and how many of it so far. */
int      CcOut(int cc);
uint32_t CcCount(int cc);
/* A timed stream (a keyboard simulator's log, script `midifile`): packets
 * handed over in the audio block whose first frame reaches their time, so
 * the firmware sees them as a host polling the port would. Due() runs from
 * Render before each block; frame = frames rendered so far (48 kHz). */
bool     LoadFile(const char* path, uint64_t start_frame);
void     Due(uint64_t frame);
size_t   Pending();
} // namespace midi
/* USB audio on the front USB-C (emu_usbaudio.cpp), for a firmware whose port
 * is a USB audio device too: the far end (script `midi plug computer`)
 * streams a sine to it (script `usbaudio out`) and listens back (`usbaudio
 * in`, `expect usbin`). The firmware's shim keeps the host's 1 ms packet
 * clock: OutFrames gives the host's next frames, InFrames takes what an IN
 * packet carried; 16-bit stereo interleaved, L first, 44.1 kHz. */
namespace usbaudio {
void     Out(double hz, int chans, float level, double ppm);   /* chans 1 L, 2 R, 3 both */
void     OutOff();
bool     OutOn();
double   OutPpm();     /* the host's clock against the Lab's */
void     OutFrames(int16_t* lr, int frames);
void     In(bool on);
bool     InOn();
void     InFrames(const int16_t* lr, int frames);
float    InTone(float hz);   /* amplitude at hz in the last 8192 frames heard */
float    InRms();
uint32_t InCount();          /* frames heard so far */
uint32_t OutCount();         /* frames sent so far */
} // namespace usbaudio
/* What J3..J10 put out, once a block, to a file (script `cvlog`): one line
 * a block, the frame then the eight volts. Block() runs from Render after
 * the firmware's callback. */
namespace cvlog {
bool Open(const char* path);
void Close();
void Block(uint64_t frame);
} // namespace cvlog
uint64_t BlockFrames();   /* frames rendered in whole blocks so far */

void LpRgb(uint8_t idx, uint8_t* r, uint8_t* g, uint8_t* b);   /* emu_palette.cpp */
void XlRgb(uint8_t col, uint8_t* r, uint8_t* g, uint8_t* b);

} // namespace emu

/* The firmware's own main(), renamed at compile time (-Dmain=firmware_main). */
int firmware_main(void);
