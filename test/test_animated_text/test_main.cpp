#include <unity.h>

#include <cstdint>
#include <config.h>

#include "AnimatedText.h"

MockBackend* gBackend = nullptr;
MockBackend  backend;

namespace
{
int countPixels(const Matrix16x16& matrix)
{
    int count = 0;
    for (int y = 0; y < LED_MATRIX_ROWS; ++y)
    {
        for (int x = 0; x < LED_MATRIX_COLS; ++x)
        {
            if (matrix.getPixel(x, y))
            {
                ++count;
            }
        }
    }
    return count;
}
} // namespace

void setUp(void) 
{
    gBackend = &backend;
}

void tearDown(void)
{

}

void test_animated_text_draws_pixels()
{
    AnimatedText animator;
    animator.setFrameDuration(0);
    animator.setText("A");

    Matrix16x16 matrix = animator.update(0);
 
    TEST_ASSERT_EQUAL_CHAR('A', animator.currentChar());
    TEST_ASSERT_GREATER_THAN_INT(0, countPixels(matrix));
}

void test_animated_text_sequence()
{
    AnimatedText animator;
    animator.setAnimationMode(AnimatedText::AnimationMode::Hold);
    animator.setFrameDuration(10);
    animator.setLooping(false);
    animator.setText("A ");

    Matrix16x16 matrix = animator.update(0);
    int pixelsFirst = countPixels(matrix);
    TEST_ASSERT_EQUAL_CHAR('A', animator.currentChar());
    TEST_ASSERT_GREATER_THAN_INT(0, pixelsFirst);

    matrix = animator.update(5);
    TEST_ASSERT_EQUAL_CHAR('A', animator.currentChar());

    matrix = animator.update(15);
    TEST_ASSERT_EQUAL_CHAR(' ', animator.currentChar());
    TEST_ASSERT_EQUAL_INT(0, countPixels(matrix));
    TEST_ASSERT_TRUE(animator.isFinished());
}

void test_animated_text_loops()
{
    AnimatedText animator;
    animator.setAnimationMode(AnimatedText::AnimationMode::Hold);
    animator.setFrameDuration(10);
    animator.setLooping(true);
    animator.setText("AB");

    animator.update(0);
    TEST_ASSERT_EQUAL_CHAR('A', animator.currentChar());

    animator.update(15);
    TEST_ASSERT_EQUAL_CHAR('B', animator.currentChar());

    animator.update(30);
    TEST_ASSERT_EQUAL_CHAR('A', animator.currentChar());
    TEST_ASSERT_FALSE(animator.isFinished());
}

void test_animated_text_scroll_finishes()
{
    AnimatedText animator;
    animator.setAnimationMode(AnimatedText::AnimationMode::Scroll);
    animator.setFrameDuration(0);
    animator.setLooping(false);
    animator.setText("A");

    Matrix16x16 matrix = animator.update(0);
    TEST_ASSERT_EQUAL_CHAR('A', animator.currentChar());
    TEST_ASSERT_GREATER_THAN_INT(0, countPixels(matrix));

    for (int step = 1; step <= 16; ++step)
    {
        matrix = animator.update(step);
    }

    TEST_ASSERT_EQUAL_INT(0, countPixels(matrix));
    TEST_ASSERT_TRUE(animator.isFinished());
}

void test_animated_text_scroll_loops()
{
    AnimatedText animator;
    animator.setAnimationMode(AnimatedText::AnimationMode::Scroll);
    animator.setFrameDuration(0);
    animator.setLooping(true);
    animator.setText("AB");
    animator.update(0);

    TEST_ASSERT_EQUAL_CHAR('A', animator.currentChar());

    for (int step = 1; step <= 40; ++step)
    {
        animator.update(step);
    }

    TEST_ASSERT_FALSE(animator.isFinished());
    TEST_ASSERT_NOT_EQUAL('\0', animator.currentChar());
}

int main(int, char**)
{
    UNITY_BEGIN();
    RUN_TEST(test_animated_text_draws_pixels);
    RUN_TEST(test_animated_text_sequence);
    RUN_TEST(test_animated_text_loops);
    RUN_TEST(test_animated_text_scroll_finishes);
    RUN_TEST(test_animated_text_scroll_loops);
    return UNITY_END();
}
