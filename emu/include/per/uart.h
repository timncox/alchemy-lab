/**
 * @file per/uart.h (emulator)
 *
 * libDaisy's UartHandler as the firmwares use it for MIDI on the rear
 * header (belt-alchemy src/rear_midi.cpp): Init() and the circular DMA
 * listen. There is no wire; a script's `uart <hex bytes>` hands the bytes to
 * the listen callback from the script thread, as the DMA's idle-line
 * interrupt would on the module. One listener (the emulated board has one
 * USART1).
 */
#pragma once

#include <cstddef>
#include <cstdint>

#include "daisy_seed.h"

namespace daisy {

class UartHandler
{
  public:
    struct Config
    {
        enum class Peripheral { USART_1, USART_2, USART_3, UART_4, UART_5, USART_6, UART_7, UART_8, LPUART_1 };
        enum class StopBits { BITS_0_5, BITS_1, BITS_1_5, BITS_2 };
        enum class Parity { NONE, EVEN, ODD };
        enum class Mode { RX, TX, TX_RX };
        enum class WordLength { BITS_7, BITS_8, BITS_9 };

        struct
        {
            Pin tx;
            Pin rx;
        } pin_config;

        Config()
        {
            stopbits   = StopBits::BITS_1;
            parity     = Parity::NONE;
            wordlength = WordLength::BITS_8;
            baudrate   = 31250;
        }

        Peripheral periph;
        StopBits   stopbits;
        Parity     parity;
        Mode       mode;
        WordLength wordlength;
        uint32_t   baudrate;
    };

    enum class Result { OK, ERR };

    typedef void (*CircularRxCallbackFunctionPtr)(uint8_t* data, size_t size,
                                                  void* context, Result result);

    Result Init(const Config& config);
    Result DmaListenStart(uint8_t* buff, size_t size,
                          CircularRxCallbackFunctionPtr cb, void* callback_context);
    Result DmaListenStop();
    bool   IsListening() const;
    Result PollTx(uint8_t*, size_t) { return Result::OK; }
};

} // namespace daisy

namespace emu {
/* Bytes arriving on the emulated USART1 RX (emu_uart.cpp). Returns how
 * many reached a listener (0 when nothing is listening). */
size_t UartInject(const uint8_t* data, size_t n);
}
