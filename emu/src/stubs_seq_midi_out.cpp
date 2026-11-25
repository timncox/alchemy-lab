/*
 * stubs_seq_midi_out.cpp -- seq's chord out on the rear header (midi_out.h,
 * a UART) for the emulator: the bytes are taken from the ring and dropped,
 * as if nothing were plugged in.
 */
#include "midi_out.h"

namespace midiout
{
void Init() {}
void Drain(chord_midi_ring_t* ring)
{
    uint8_t b;
    while(chord_midi_ring_pop(ring, &b)) {}
}
} // namespace midiout
