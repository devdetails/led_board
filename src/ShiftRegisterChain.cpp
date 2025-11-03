#include "ShiftRegisterChain.h"

extern Backend* gBackend;

void ShiftRegisterChain::begin()
{
    if (gBackend == nullptr)
        return;
        
    gBackend->gpio.digitalWrite(oePin, HIGH);
    gBackend->gpio.pinMode(latchPin, OUTPUT);
    gBackend->gpio.pinMode(oePin, OUTPUT);
    gBackend->gpio.digitalWrite(latchPin, HIGH);

    gBackend->spi.begin(clockPin, -1, dataPin, -1);
    spiInitialized = true;

    writeWord(~0);
    gBackend->gpio.digitalWrite(oePin, LOW);
}

void ShiftRegisterChain::enableOutput(bool enable)
{
    if (gBackend == nullptr)
        return;

    gBackend->gpio.digitalWrite(oePin, enable ? LOW : HIGH);
}

void ShiftRegisterChain::writeWord(uint32_t word)
{
    if (gBackend == nullptr || !spiInitialized)
        return;

    gBackend->gpio.digitalWrite(latchPin, LOW);
    gBackend->spi.beginTransaction(spiSettings);
    gBackend->spi.writeBytes(reinterpret_cast<uint8_t*>(&word), sizeof(word));
    gBackend->spi.endTransaction();
    gBackend->gpio.digitalWrite(latchPin, HIGH);
}

int ShiftRegisterChain::getDataPin() const
{
    return dataPin;
}

int ShiftRegisterChain::getClockPin() const
{
    return clockPin;
}

int ShiftRegisterChain::getLatchPin() const
{
    return latchPin;
}

int ShiftRegisterChain::getOePin() const
{
    return oePin;
}
