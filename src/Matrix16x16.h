#pragma once

#include <stdint.h>
#include <array>

class Matrix16x16
{
public:
    void clear();
    void setAll(bool on);

    void setPixel(int x, int y, bool on = true);
    bool getPixel(int x, int y) const;

    uint16_t getPixelColumnMask(int x) const;
    uint16_t getPixelRowMask(int y) const;

    void     setRowBits(int y, uint16_t bits);
    uint16_t getRowBits(int y) const;

    uint32_t composeRowWord(int row) const;

    void copyFrom(const Matrix16x16& other);

private:
    std::array<uint16_t, LED_MATRIX_ROWS> rows{};
};
