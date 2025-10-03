#include <unity.h>
#include <config.h>

#include "Image.h"
#include "AnimatedImage.h"
#include "Matrix16x16.h"

MockBackend* gBackend = nullptr;
MockBackend  backend;

void setUp() 
{
    backend.reset();
    gBackend = &backend;
}
void tearDown() {}

void test_image_pixel_access()
{
    Image img;
    img.clear();
    img.setPixel(0, 0, true);
    img.setPixel(15, 15, true);
    TEST_ASSERT_TRUE(img.getPixel(0, 0));
    TEST_ASSERT_TRUE(img.getPixel(15, 15));
    TEST_ASSERT_FALSE(img.getPixel(1, 1));
}

void test_image_draw_to_matrix()
{
    Matrix16x16 matrix;

    Image img;
    img.clear();
    img.setPixel(3, 5, true);
    img.setPixel(7, 10, true);

    img.draw(matrix);

    TEST_ASSERT_TRUE(matrix.getPixel(3, 5));
    TEST_ASSERT_TRUE(matrix.getPixel(7, 10));
    TEST_ASSERT_FALSE(matrix.getPixel(0, 0));
}

void test_animated_image_sequence()
{
    backend.reset();

    Image frameA;
    frameA.clear();
    frameA.setPixel(0, 0, true);

    Image frameB;
    frameB.clear();
    frameB.setPixel(15, 15, true);

    AnimatedImage anim;
    anim.setLooping(false);
    anim.setFrameDuration(10);
    anim.setFrames({frameA, frameB});

    Matrix16x16 matrix = anim.update(0);
    TEST_ASSERT_TRUE(matrix.getPixel(0, 0));
    TEST_ASSERT_FALSE(matrix.getPixel(15, 15));

    matrix = anim.update(15);
    TEST_ASSERT_FALSE(matrix.getPixel(0, 0));
    TEST_ASSERT_TRUE(matrix.getPixel(15, 15));
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_image_pixel_access);
    RUN_TEST(test_image_draw_to_matrix);
    RUN_TEST(test_animated_image_sequence);
    return UNITY_END();
}

