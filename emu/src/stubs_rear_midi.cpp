/*
 * stubs_rear_midi.cpp -- Belt's rear MIDI jack (rear_midi.h, a UART on the
 * rear header) for the emulator: nothing is ever plugged in, so no bytes
 * and no notes arrive.
 */
#include <stdint.h>

#include "rear_midi.h"

namespace rearmidi
{
void     Init() {}
void     Enable(bool) {}
void     Poll(NoteFn) {}
uint32_t Bytes() { return 0u; }
uint32_t Dropped() { return 0u; }
} // namespace rearmidi
