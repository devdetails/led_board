#include "AnimatedText.h"

#include <stddef.h>

#include "font8x8_basic.h"

// Helper to iterate font bits; glyph data already matches the matrix orientation
static bool glyphPixelOn(const uint8_t glyph[8], int col, int row)
{
    if (row < 0 || row >= 8 || col < 0 || col >= 8)
        return false;

    uint8_t rowBits = glyph[row];
    return ((rowBits >> col) & 0x1u) != 0u;
}

void AnimatedText::setText(const std::string& text)
{
    message = text;
    reset();
}

void AnimatedText::setText(const char* text)
{
    message = text ? std::string(text) : std::string();
    reset();
}

const std::string& AnimatedText::getText() const
{
    return message;
}

void AnimatedText::setAnimationMode(AnimationMode newMode)
{
    if (mode == newMode)
        return;

    mode = newMode;
    reset();
}

AnimatedText::AnimationMode AnimatedText::getAnimationMode() const
{
    return mode;
}

void AnimatedText::setFrameDuration(uint32_t milliseconds)
{
    frameDurationMs = milliseconds;
}

uint32_t AnimatedText::getFrameDuration() const
{
    return frameDurationMs;
}

void AnimatedText::setLooping(bool enable)
{
    looping = enable;
    if (looping && !message.empty() && nextIndex == message.size())
    {
        nextIndex = 0;
    }
}

bool AnimatedText::isLooping() const
{
    return looping;
}

void AnimatedText::reset()
{
    nextIndex          =  0;
    displayedIndex     = -1;
    lastFrameTimestamp =  0;
    scrollOffset       =  0;
    matrix.clear();
}

Matrix16x16 AnimatedText::update(uint32_t nowMs)
{
    if (message.empty())
    {
        if (displayedIndex != -1)
        {
            displayedIndex = -1;
            nextIndex = 0;
            scrollOffset = 0;
            matrix.clear();
        }
        return matrix;
    }

    if (mode == AnimationMode::Scroll)
    {
        updateScroll(nowMs);
    }
    else
    {
        updateHold(nowMs);
    }

    return matrix;
}

bool AnimatedText::isFinished() const
{
    if (looping)
        return false;

    if (message.empty())
        return true;

    if (mode == AnimationMode::Scroll)
    {
        return (displayedIndex == -1) && (nextIndex >= message.size());
    }

    return (displayedIndex == static_cast<int>(message.size() - 1)) && (nextIndex == message.size());
}

char AnimatedText::currentChar() const
{
    if (displayedIndex < 0)
        return '\0';

    size_t idx = static_cast<size_t>(displayedIndex);
    if (idx >= message.size())
        return '\0';

    return message[idx];
}

void AnimatedText::updateHold(uint32_t nowMs)
{
    if (!looping && displayedIndex >= 0 && nextIndex >= message.size())
    {
        return;
    }

    bool shouldDraw = false;
    size_t indexToDraw = 0;

    if (displayedIndex == -1)
    {
        shouldDraw = true;
        indexToDraw = 0;
    }
    else if (frameDurationMs == 0 || (nowMs - lastFrameTimestamp) >= frameDurationMs)
    {
        if (nextIndex >= message.size())
        {
            if (!looping)
                return;

            nextIndex = 0;
        }
        indexToDraw = nextIndex;
        shouldDraw = true;
    }

    if (!shouldDraw)
        return;

    drawCharacter(message[indexToDraw]);
    displayedIndex = static_cast<int>(indexToDraw);
    lastFrameTimestamp = nowMs;

    size_t candidate = indexToDraw + 1;
    if (candidate >= message.size())
    {
        nextIndex = looping ? 0 : message.size();
    }
    else
    {
        nextIndex = candidate;
    }
}

void AnimatedText::updateScroll(uint32_t nowMs)
{
    if (!looping && displayedIndex == -1 && nextIndex >= message.size())
        return;

    if (displayedIndex == -1)
    {
        displayedIndex = 0;
        if (message.size() <= 1)
        {
            nextIndex = looping ? 0 : message.size();
        }
        else
        {
            nextIndex = 1;
        }
        scrollOffset = 0;

        char current = message[displayedIndex];
        char next = ' ';
        if (nextIndex < message.size())
            next = message[nextIndex];
        else if (looping && !message.empty())
            next = message[0];

        drawScrollFrame(current, next, scrollOffset);
        lastFrameTimestamp = nowMs;
        return;
    }

    if (frameDurationMs > 0 && (nowMs - lastFrameTimestamp) < frameDurationMs)
        return;

    lastFrameTimestamp = nowMs;
    scrollOffset += 1;

    if (scrollOffset >= 16)
    {
        scrollOffset = 0;

        if (nextIndex >= message.size())
        {
            if (!looping)
            {
                displayedIndex = -1;
                nextIndex = message.size();
                matrix.clear();
                return;
            }
            nextIndex = 0;
        }

        displayedIndex = static_cast<int>(nextIndex);

        size_t upcoming = static_cast<size_t>(displayedIndex) + 1;
        if (upcoming >= message.size())
        {
            nextIndex = looping ? 0 : message.size();
        }
        else
        {
            nextIndex = upcoming;
        }
    }

    char current = (displayedIndex >= 0) ? message[displayedIndex] : ' ';
    char next = ' ';
    if (nextIndex < message.size())
        next = message[nextIndex];
    else if (looping && !message.empty())
        next = message[0];

    drawScrollFrame(current, next, scrollOffset);
}

void AnimatedText::drawGlyphAtOffset(const uint8_t* glyph, int offsetX)
{
    for (int row = 0; row < 8; ++row)
    {
        for (int col = 0; col < 8; ++col)
        {
            if (!glyphPixelOn(glyph, col, row))
                continue;

            int baseX = offsetX + col * 2;
            int baseY = row * 2;

            matrix.setPixel(baseX,     baseY,     true);
            matrix.setPixel(baseX + 1, baseY,     true);
            matrix.setPixel(baseX,     baseY + 1, true);
            matrix.setPixel(baseX + 1, baseY + 1, true);
        }
    }
}

void AnimatedText::drawScrollFrame(char current, char next, int offset)
{
    matrix.clear();
    drawGlyphAtOffset(font8x8_basic[static_cast<uint8_t>(current)], -offset);
    drawGlyphAtOffset(font8x8_basic[static_cast<uint8_t>(next)], 16 - offset);
}

void AnimatedText::drawCharacter(char c)
{
    matrix.clear();
    drawGlyphAtOffset(font8x8_basic[static_cast<uint8_t>(c)], 0);
}

