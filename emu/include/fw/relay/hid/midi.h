/* hid/midi.h (emulator, Relay only): just enough of libDaisy's MIDI types for
 * relay_alchemy.cpp to compile. The emulator has no MIDI input, so the
 * handler never has an event; Relay's MIDI mode boots and idles. */
#pragma once
#include <stdint.h>
#include <stddef.h>

namespace daisy
{
enum MidiMessageType
{
    NoteOff,
    NoteOn,
    PolyphonicKeyPressure,
    ControlChange,
    ProgramChange,
    ChannelPressure,
    PitchBend,
    SystemCommon,
    SystemRealTime,
    ChannelMode,
    MessageLast,
};
enum SystemCommonType
{
    SystemExclusive,
    MTCQuarterFrame,
    SongPositionPointer,
    SongSelect,
    SCUndefined0,
    SCUndefined1,
    TuneRequest,
    SysExEnd,
    SystemCommonLast,
};
enum SystemRealTimeType
{
    TimingClock,
    SRTUndefined0,
    Start,
    Continue,
    Stop,
    SRTUndefined1,
    ActiveSensing,
    Reset,
    SystemRealTimeLast,
};
struct MidiEvent
{
    MidiMessageType    type     = MessageLast;
    int                channel  = 0;
    uint8_t            data[2]  = {0, 0};
    SystemCommonType   sc_type  = SystemCommonLast;
    SystemRealTimeType srt_type = SystemRealTimeLast;
};
} // namespace daisy
