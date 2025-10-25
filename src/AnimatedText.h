#pragma once

#include <stdint.h>
#include <string>

#include "config.h"
#include "Matrix16x16.h"

class AnimatedText
{
public:
    enum class AnimationMode
    {
        Hold,
        Scroll
    };
    enum class VerticalAlignment
    {
        Full,
        UpperHalf,
        LowerHalf
    };

    void               setText(const std::string& text);
    void               setText(const char* text);
    const std::string& getText() const;

    void          setAnimationMode(AnimationMode newMode);
    AnimationMode getAnimationMode() const;

    void     setFrameDuration(uint32_t milliseconds);
    uint32_t getFrameDuration() const;

    void setLooping(bool enable);
    bool isLooping() const;

    void                setVerticalAlignment(VerticalAlignment alignment);
    VerticalAlignment   getVerticalAlignment() const;

    void        reset();
    Matrix16x16 update(uint32_t nowMs);
    bool        isFinished() const;
    char        currentChar() const;

private:
    void updateHold(uint32_t nowMs);
    void updateScroll(uint32_t nowMs);
    void drawGlyphAtOffset(const uint8_t* glyph, int offsetX);
    void drawScrollFrame(int offset);
    void drawCharacter(char c);
    int  horizontalScale() const;
    int  verticalScale() const;
    int  glyphPixelWidth() const;

    Matrix16x16   matrix;
    std::string   message              =  DEFAULT_INITIAL_TEXT;
    bool          looping              =  DEFAULT_TEXT_LOOPING;
    AnimationMode mode                 =  (AnimationMode)DEFAULT_TEXT_ANIMATION_MODE;
    VerticalAlignment verticalAlignment = VerticalAlignment::Full;
    uint32_t      frameDurationMs      =  DEFAULT_TEXT_ANIMATION_MODE==0 ? DEFAULT_TEXT_FRAME_DURATION_HOLD_MS : DEFAULT_TEXT_FRAME_DURATION_LOOP_MS;
    uint32_t      lastFrameTimestamp   =  0;
    size_t        nextIndex            =  0;
    int           displayedIndex       = -1;
    int           scrollOffset         =  0;
};
