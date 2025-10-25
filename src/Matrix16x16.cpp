#include "Matrix16x16.h"
#include <cstddef>

void Matrix16x16::clear()
{
    rows.fill(0x0);
}

void Matrix16x16::setAll(bool on)
{
    rows.fill(on ? 0xFFFF : 0x0000);
}

void Matrix16x16::setPixel(int x, int y, bool on)
{
    if (x < 0 || x >= LED_MATRIX_COLS || y < 0 || y >= LED_MATRIX_ROWS)
        return;

    uint16_t columnMask = getPixelColumnMask(x);

    if (on) rows[y] |=  columnMask;
    else    rows[y] &= ~columnMask;
}

bool Matrix16x16::getPixel(int x, int y) const
{
    if (x < 0 || x >= LED_MATRIX_COLS || y < 0 || y >= LED_MATRIX_ROWS)
        return false;

    return (rows[y] & getPixelColumnMask(x)) != 0;
}

uint16_t Matrix16x16::getPixelColumnMask(int x) const
{
    if (x < 0 || x >= LED_MATRIX_COLS)
        return 0u;

    return static_cast<uint16_t>(0x1u << (LED_MATRIX_COLS - x - 1));
}

uint16_t Matrix16x16::getPixelRowMask(int y) const
{
    if (y < 0 || y >= LED_MATRIX_ROWS)
        return 0u;

    return static_cast<uint16_t>(0x1u << (LED_MATRIX_ROWS - y - 1));
}

void Matrix16x16::setRowBits(int y, uint16_t bits)
{
    if (y < 0 || y >= LED_MATRIX_ROWS)
        return;

    const uint16_t mask = (LED_MATRIX_COLS >= 16) ? 0xFFFFu : static_cast<uint16_t>((1u << LED_MATRIX_COLS) - 1u);
    rows[y] = bits & mask;
}

uint16_t Matrix16x16::getRowBits(int y) const
{
    if (y < 0 || y >= LED_MATRIX_ROWS)
        return 0u;

    const uint16_t mask = (LED_MATRIX_COLS >= 16) ? 0xFFFFu : static_cast<uint16_t>((1u << LED_MATRIX_COLS) - 1u);
    return rows[y] & mask;
}

uint32_t Matrix16x16::composeRowWord(int row) const
{
    if (row < 0 || row >= LED_MATRIX_ROWS)
        return 0u;

    uint32_t rowWord = static_cast<uint32_t>(getPixelRowMask(row)) << LED_MATRIX_COLS;
    rowWord |= rows[row];

    // Matrix wiring is active-low for both rows and columns
    rowWord = ~rowWord;
    return rowWord;
}

void Matrix16x16::copyFrom(const Matrix16x16& other)
{
    rows = other.rows;
}

void Matrix16x16::merge(const Matrix16x16& other)
{
    for (std::size_t i = 0; i < rows.size(); ++i)
    {
        rows[i] |= other.rows[i];
    }
}


