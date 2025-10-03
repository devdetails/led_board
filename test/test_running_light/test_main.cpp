#include <unity.h>
#include <config.h>

#include "Matrix16x16.h"
#include "ShiftRegisterChain.h"

MockBackend*       gBackend = nullptr;
MockBackend        backend;
ShiftRegisterChain shiftRegister;

void setUp(void) 
{
    gBackend = &backend;
    shiftRegister.begin();
}

void tearDown(void)
{
}

constexpr  uint32_t reverseBits(uint32_t v) 
{
    v = (v >> 1)  & 0x55555555u | (v & 0x55555555u) << 1;
    v = (v >> 2)  & 0x33333333u | (v & 0x33333333u) << 2;
    v = (v >> 4)  & 0x0F0F0F0Fu | (v & 0x0F0F0F0Fu) << 4;
    v = (v >> 8)  & 0x00FF00FFu | (v & 0x00FF00FFu) << 8;
    v = (v >> 16) | (v << 16);
    return v;
}

#include <stdio.h>

void test_running_light_single_pixel()
{
    backend.reset();
    Matrix16x16 matrix;

    for (int position = 0; position < LED_MATRIX_ROWS * LED_MATRIX_COLS; ++position)
    {
        int y = (position / LED_MATRIX_COLS) % LED_MATRIX_ROWS;
        int x =  position % LED_MATRIX_COLS;

        matrix.clear();
        matrix.setPixel(x, y, true);

        uint32_t rowWord       = matrix.composeRowWord(y);
        uint32_t expectedValue = reverseBits(rowWord);

        shiftRegister.writeWord(rowWord);
        TEST_ASSERT_EQUAL(expectedValue, backend.latchedWord);
    }
}

void test_corners()
{
    constexpr uint32_t TL = reverseBits(~(0x1u << 31 | 0x1u << 15));
    constexpr uint32_t TR = reverseBits(~(0x1u << 31 | 0x1u <<  0));
    constexpr uint32_t BR = reverseBits(~(0x1u << 16 | 0x1u <<  0));
    constexpr uint32_t BL = reverseBits(~(0x1u << 16 | 0x1u << 15));

    backend.reset();
    Matrix16x16 matrix;

    matrix.clear();
    matrix.setPixel(0, 0, true);
    shiftRegister.writeWord(matrix.composeRowWord(0));
    TEST_ASSERT_EQUAL(TL, backend.latchedWord);

    matrix.clear();
    matrix.setPixel(15, 0, true);
    shiftRegister.writeWord(matrix.composeRowWord(0));
    TEST_ASSERT_EQUAL(TR, backend.latchedWord);

    matrix.clear();
    matrix.setPixel(15, 15, true);
    shiftRegister.writeWord(matrix.composeRowWord(15));
    TEST_ASSERT_EQUAL(BR, backend.latchedWord);

    matrix.clear();
    matrix.setPixel(0, 15, true);
    shiftRegister.writeWord(matrix.composeRowWord(15));
    TEST_ASSERT_EQUAL(BL, backend.latchedWord);
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_corners);
    RUN_TEST(test_running_light_single_pixel);
    return UNITY_END();
}
