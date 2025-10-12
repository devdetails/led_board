#pragma once

#include <stdint.h>
#include "config.h"

class ShiftRegisterChain
{
public:
    void begin();
    void enableOutput(bool enable);
    void writeWord(uint32_t word);

    int getDataPin()  const;
    int getClockPin() const;
    int getLatchPin() const;
    int getOePin()    const;

private:
    SPISettings spiSettings { SHIFTREG_SPI_FREQUENCY_HZ, SPI_LSBFIRST, SPI_MODE0 };
    bool        spiInitialized = false;

    int dataPin  = PIN_SR_DATA; 
    int clockPin = PIN_SR_CLK;
    int latchPin = PIN_SR_LATCH;
    int oePin    = PIN_SR_OE;
};
