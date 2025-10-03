#pragma once

#include <stdint.h>

#include "AnimatedText.h"
#include "AnimatedImage.h"

enum class DisplayMode
{
    Text,
    Image
};

void WebInterface_begin(AnimatedText& animatedText,
                        AnimatedImage& animatedImage,
                        const char* wifiSsid,
                        const char* wifiPassword,
                        const char* wifiHostname);

void        WebInterface_handle();
DisplayMode WebInterface_getDisplayMode();


