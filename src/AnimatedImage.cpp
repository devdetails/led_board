#include "AnimatedImage.h"

void AnimatedImage::setFrames(const std::vector<Image>& newFrames)
{
    frames = newFrames;
    reset();
}

void AnimatedImage::clearFrames()
{
    frames.clear();
    reset();
}

void AnimatedImage::setFrameDuration(uint32_t milliseconds)
{
    frameDurationMs = milliseconds;
}

uint32_t AnimatedImage::getFrameDuration() const
{
    return frameDurationMs;
}

void AnimatedImage::setLooping(bool enable)
{
    looping = enable;
}

bool AnimatedImage::isLooping() const
{
    return looping;
}

void AnimatedImage::reset()
{
    currentIndex = 0;
    lastFrameTimestamp = 0;
    hasDisplayedFrame = false;
    matrix.clear();
}

Matrix16x16 AnimatedImage::update(uint32_t nowMs)
{
    if (frames.empty())
    {
        if (hasDisplayedFrame)
        {
            matrix.clear();
            hasDisplayedFrame = false;
        }
        return matrix;
    }

    if (!hasDisplayedFrame)
    {
        showFrame(currentIndex);
        hasDisplayedFrame = true;
        lastFrameTimestamp = nowMs;
        return matrix;
    }

    if (frameDurationMs > 0 && (nowMs - lastFrameTimestamp) < frameDurationMs)
        return matrix;

    lastFrameTimestamp = nowMs;

    size_t next = currentIndex + 1;
    if (next >= frames.size())
    {
        if (!looping)
        {
            hasDisplayedFrame = true;
            return matrix;
        }
        next = 0;
    }

    currentIndex = next;
    showFrame(currentIndex);
    hasDisplayedFrame = true;

    return matrix;
}

bool AnimatedImage::isFinished() const
{
    if (looping)
        return false;

    return hasDisplayedFrame && currentIndex + 1 >= frames.size();
}

size_t AnimatedImage::frameCount() const
{
    return frames.size();
}

void AnimatedImage::showFrame(size_t index)
{
    if (index >= frames.size())
        return;

    frames[index].draw(matrix);
}

