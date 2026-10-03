/*
 * seq's rear-header MIDI out (seq-alchemy alchemy/src/midi_out.h), emulated:
 * the same interface, but the bytes go to the emulator's UART log
 * (emu_uart.cpp) instead of USART1. Drain() lets out about what 31,250 baud
 * would (3 bytes per 1 ms poll), so a burst queues in the firmware's ring
 * as it does on the module.
 */
#include "midi_out.h"

#include "emu.h"

namespace {
bool g_ready = false;
struct Registered { Registered() { emu::UartBuilt(); } } g_registered;
} // namespace

namespace midiout
{

void Init()
{
    if(g_ready) return;
    g_ready = true;
    emu::UartClaim();
}

void Drain(chord_midi_ring_t* ring)
{
    if(!g_ready) return;
    uint8_t b;
    for(int i = 0; i < 3 && chord_midi_ring_pop(ring, &b); i++)
        emu::UartTx(b);
}

} // namespace midiout
