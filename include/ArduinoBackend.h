#pragma once

#ifdef ARDUINO

class ArduinoBackend
{
public:
    struct
    {
        void pinMode(int pin, int mode)
        {
            ::pinMode(pin, mode);
        }

        void digitalWrite(int pin, bool level)
        {
            ::digitalWrite(pin, level ? HIGH : LOW);
        }
    } gpio;

    struct
    {
        void begin(int8_t sck = -1, int8_t miso = -1, int8_t mosi = -1, int8_t ss = -1)
        {
            SPI.begin(sck, miso, mosi, ss);
        }

        void beginTransaction(SPISettings settings)
        {
            SPI.beginTransaction(settings);
        }

        void endTransaction() 
        {
            SPI.endTransaction();
        }

        void writeBytes(const uint8_t *data, uint32_t size)
        {
            SPI.writeBytes(data, size);
        }
    } spi;

    uint64_t micros()
    {
        return ::micros();
    }

    void delayMicroseconds(uint32_t us)
    {
        ::delayMicroseconds(us);
    }
};
#endif
