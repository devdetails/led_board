#include "Image.h"

#include "Matrix16x16.h"

namespace
{
constexpr uint16_t bitMaskForX(int x)
{
    return (x < 0 || x >= Image::kSize) ? 0u : static_cast<uint16_t>(1u << (Image::kSize - 1 - x));
}
}

Image::Image()
{
    clear();
}

void Image::clear()
{
    rows.fill(0);
}

void Image::setPixel(int x, int y, bool on)
{
    if (x < 0 || x >= kSize || y < 0 || y >= kSize)
        return;

    uint16_t mask = bitMaskForX(x);
    if (on)
    {
        rows[y] |= mask;
    }
    else
    {
        rows[y] &= static_cast<uint16_t>(~mask);
    }
}

bool Image::getPixel(int x, int y) const
{
    if (x < 0 || x >= kSize || y < 0 || y >= kSize)
        return false;

    uint16_t mask = bitMaskForX(x);
    return (rows[y] & mask) != 0;
}

void Image::setRow(int y, uint16_t bits)
{
    if (y < 0 || y >= kSize)
        return;

    const uint16_t mask = (kSize >= 16) ? 0xFFFF : static_cast<uint16_t>((1u << kSize) - 1u);
    rows[y] = bits & mask;
}

uint16_t Image::getRow(int y) const
{
    if (y < 0 || y >= kSize)
        return 0;

    const uint16_t mask = (kSize >= 16) ? 0xFFFF : static_cast<uint16_t>((1u << kSize) - 1u);
    return rows[y] & mask;
}

void Image::draw(Matrix16x16& matrix) const
{
    matrix.clear();
    for (int y = 0; y < kSize; ++y)
    {
        matrix.setRowBits(y, rows[y]);
    }
}

