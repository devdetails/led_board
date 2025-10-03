#pragma once

#include <stdint.h>
#include <vector>

#include "config.h"
#include "Image.h"
#include "Matrix16x16.h"

class AnimatedImage
{
public:
    void clearFrames();
    void setFrames(const std::vector<Image>& frames);

    void     setFrameDuration(uint32_t milliseconds);
    uint32_t getFrameDuration() const;

    void setLooping(bool enable);
    bool isLooping() const;

    void        reset();
    Matrix16x16 update(uint32_t nowMs);
    bool        isFinished() const;
    size_t      frameCount() const;

private:
    void showFrame(size_t index);

    Matrix16x16        matrix;
    std::vector<Image> frames;
    bool               looping            = DEFAULT_IMAGE_LOOPING;
    uint32_t           frameDurationMs    = DEFAULT_IMAGE_FRAME_DURATION_MS;
    uint32_t           lastFrameTimestamp = 0;
    size_t             currentIndex       = 0;
    bool               hasDisplayedFrame  = false;
};

