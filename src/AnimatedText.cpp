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

void AnimatedText::setVerticalAlignment(VerticalAlignment alignment)
{
    if (verticalAlignment == alignment)
        return;

    verticalAlignment = alignment;
    reset();
}

AnimatedText::VerticalAlignment AnimatedText::getVerticalAlignment() const
{
    return verticalAlignment;
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

    const int glyphWidth = glyphPixelWidth();

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

        drawScrollFrame(scrollOffset);
        lastFrameTimestamp = nowMs;
        return;
    }

    if (frameDurationMs > 0 && (nowMs - lastFrameTimestamp) < frameDurationMs)
        return;

    lastFrameTimestamp = nowMs;
    scrollOffset += 1;

    if (scrollOffset >= glyphWidth)
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

    drawScrollFrame(scrollOffset);
}

void AnimatedText::drawGlyphAtOffset(const uint8_t* glyph, int offsetX)
{
    const int horizontalScale = this->horizontalScale();
    const int verticalScale   = this->verticalScale();
    const int verticalOffset  = (verticalAlignment == VerticalAlignment::LowerHalf)
                                    ? (LED_MATRIX_ROWS - 8 * verticalScale)
                                    : 0;

    for (int row = 0; row < 8; ++row)
    {
        for (int col = 0; col < 8; ++col)
        {
            if (!glyphPixelOn(glyph, col, row))
                continue;

            const int baseX = offsetX + col * horizontalScale;
            const int baseY = verticalOffset + row * verticalScale;

            for (int dy = 0; dy < verticalScale; ++dy)
            {
                for (int dx = 0; dx < horizontalScale; ++dx)
                {
                    matrix.setPixel(baseX + dx, baseY + dy, true);
                }
            }
        }
    }
}

void AnimatedText::drawScrollFrame(int offset)
{
    matrix.clear();

    if (message.empty() || displayedIndex < 0)
        return;

    const int glyphWidth = glyphPixelWidth();
    int       drawX      = -offset;

    size_t glyphIndex = static_cast<size_t>(displayedIndex);
    char   current    = message[glyphIndex];
    bool   hasGlyphs  = true;

    while (drawX < LED_MATRIX_COLS)
    {
        drawGlyphAtOffset(font8x8_basic[static_cast<uint8_t>(current)], drawX);
        drawX += glyphWidth;

        if (!hasGlyphs)
            continue;

        size_t nextGlyph = glyphIndex + 1;
        if (nextGlyph >= message.size())
        {
            if (!looping)
            {
                hasGlyphs = false;
                current   = ' ';
                continue;
            }
            nextGlyph = 0;
        }

        glyphIndex = nextGlyph;
        current    = message[glyphIndex];
    }
}

void AnimatedText::drawCharacter(char c)
{
    matrix.clear();
    drawGlyphAtOffset(font8x8_basic[static_cast<uint8_t>(c)], 0);
}

int AnimatedText::horizontalScale() const
{
    return (verticalAlignment == VerticalAlignment::Full) ? 2 : 1;
}

int AnimatedText::verticalScale() const
{
    return (verticalAlignment == VerticalAlignment::Full) ? 2 : 1;
}

int AnimatedText::glyphPixelWidth() const
{
    return horizontalScale() * 8;
}

