/* hid/usb_midi.h (emulator, Relay only): a MIDI-over-USB handler that never
 * receives anything (see hid/midi.h). */
#pragma once
#include "hid/midi.h"

namespace daisy
{
struct MidiUsbTransport
{
    struct Config
    {
        enum Periph
        {
            INTERNAL,
            EXTERNAL,
            HOST,
        };
        Periph periph = INTERNAL;
    };
};
class MidiUsbHandler
{
  public:
    struct Config
    {
        MidiUsbTransport::Config transport_config;
    };
    void      Init(const Config&) {}
    void      StartReceive() {}
    void      Listen() {}
    bool      HasEvents() const { return false; }
    MidiEvent PopEvent() { return MidiEvent(); }
};
} // namespace daisy
