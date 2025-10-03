#pragma once

#include <vector>
#include <cassert>
#include "config.h"

class MockBackend
{
public:
    MockBackend()
    {
        gpio.backend = this;
        spi.backend  = this;
    }

    struct
    {
        void pinMode(int pin, int mode) {}

        void digitalWrite(int pin, bool level)
        {
            assert(pin != backend->clockPin && pin != backend->dataPin); // data transfer is supposed to be handled via SPI

            if (pin == backend->latchPin)
            {
                // rising edge: cache bits
                if (!backend->latchLevel && level)
                {
                    backend->latchedWord = backend->shiftedBits;
                }
                
                // falling edge: restart bit count
                if (backend->latchedWord && !level)
                {
                    backend->bitCount = 0;
                }

                backend->latchLevel = level;
            }

        }

        MockBackend* backend = nullptr;
    } gpio;

    struct
    {
        void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1)  {}
        void beginTransaction(SPISettings settings)                                      {}
        void endTransaction()                                                            {}

        void writeBytes(const uint8_t *data, uint32_t size)
        {
            for (int32_t byte = 0; byte<size; byte++)
            {
                for (uint32_t bit = 0; bit < 8; bit++)
                {
                    backend->shiftedBits  = backend->shiftedBits << 1;
                    backend->shiftedBits |= (data[byte] & (1 << bit)) ? 0x1 : 0x0;
                    
                    ++backend->bitCount;
                }
            }
        }

        MockBackend* backend = nullptr;
    } spi;

    uint64_t micros()
    {
        return 0;
    }

    void delayMicroseconds(uint32_t /*us*/) {}

    void reset()
    {
        latchLevel  = false;
        shiftedBits = 0;
        bitCount    = 0;
        latchedWord = 0;
    }

    int      dataPin     = PIN_SR_DATA;
    int      clockPin    = PIN_SR_CLK;
    int      latchPin    = PIN_SR_LATCH;
    int      oePin       = PIN_SR_OE;
    bool     latchLevel  = false;
    uint32_t shiftedBits = 0;
    int      bitCount    = 0;
    uint32_t latchedWord = 0;
};