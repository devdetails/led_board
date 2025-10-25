#pragma once

#include <stdint.h>

#include "AnimatedText.h"
#include "AnimatedImage.h"

enum class DisplayMode
{
    Text,
    Image
};

enum class TextLayout
{
    Dual,
    SingleTop,
    SingleBottom
};

void WebInterface_begin(AnimatedText& animatedTextTop,
                        AnimatedText& animatedTextBottom,
                        AnimatedImage& animatedImage,
                        const char* wifiSsid,
                        const char* wifiPassword,
                        const char* wifiHostname);

void        WebInterface_handle();
DisplayMode WebInterface_getDisplayMode();
uint16_t    WebInterface_getBrightnessDuty();
uint16_t    WebInterface_getBrightnessScale();
TextLayout  WebInterface_getTextLayout();


