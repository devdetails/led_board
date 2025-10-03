#pragma once

#include <array>
#include <stdint.h>

class Matrix16x16;

class Image
{
public:
    static constexpr int kSize = 16;
    static constexpr int kStrideBits = kSize;
    static constexpr int kRowBytes = kStrideBits / 8;
    static constexpr int kTotalBytes = kRowBytes * kSize;

    Image();

    void clear();
    void setPixel(int x, int y, bool on = true);
    bool getPixel(int x, int y) const;

    void     setRow(int y, uint16_t bits);
    uint16_t getRow(int y) const;

    void draw(Matrix16x16& matrix) const;

private:
    std::array<uint16_t, kSize> rows{};
};

