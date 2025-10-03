#include <unity.h>
#include <config.h>

#include <cstdint>
#include <tuple>
#include <vector>

#include "Matrix16x16.h"
#include "ShiftRegisterChain.h"

MockBackend*       gBackend = nullptr;
MockBackend        backend;
ShiftRegisterChain chain;

void setUp(void) 
{
    gBackend = &backend;
    chain.begin();
}

void tearDown(void)
{
}

void test_set_get_pixel()
{
    Matrix16x16 matrix;
    matrix.clear();

    matrix.setPixel(0, 0, true);
    matrix.setPixel(15, 15, true);
    matrix.setPixel(19, 5, true);

    TEST_ASSERT_EQUAL(matrix.getPixel(0, 0),   true);
    TEST_ASSERT_EQUAL(matrix.getPixel(15, 15), true);
    TEST_ASSERT_EQUAL(matrix.getPixel(19, 15), false);
}

void test_shift_register_bitflow()
{
    backend.reset();
    chain.writeWord(0xA5A5A5A5u);

    TEST_ASSERT_EQUAL(32, backend.bitCount);
    TEST_ASSERT_EQUAL_HEX32(0xA5A5A5A5u, backend.latchedWord);
}

int main(int argc, char** argv)
{
    UNITY_BEGIN();
    RUN_TEST(test_shift_register_bitflow);
    return UNITY_END();
}
