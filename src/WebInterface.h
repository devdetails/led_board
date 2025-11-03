#pragma once

#include <stdint.h>

#include <WebServer.h>
#include <vector>
#include <string>

#include "AnimatedImage.h"
#include "AnimatedText.h"

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

class WebInterface
{
public:
    WebInterface(AnimatedText& animatedTextTop,
                 AnimatedText& animatedTextBottom,
                 AnimatedImage& animatedImage);

    void begin();
    void handle();

    DisplayMode getDisplayMode() const;
    TextLayout  getTextLayout() const;
    uint16_t    getBrightnessDuty() const;
    uint16_t    getBrightnessScale() const;

private:
    static constexpr float    kBrightnessGamma      = 2.2f;
    static constexpr uint16_t kBrightnessFixedScale = 32;

    AnimatedText& mAnimatedTextTop;
    AnimatedText& mAnimatedTextBottom;
    AnimatedImage& mAnimatedImage;
    WebServer      mHttpServer;

    DisplayMode        mDisplayMode;
    TextLayout         mTextLayout;
    std::vector<Image> mImageFrames;
    uint16_t           mBrightnessDuty;
    uint8_t            mBrightnessPercent;

    void updateBrightnessFromPercent(uint8_t percent);
    String htmlEscape(const std::string& text) const;
    String jsonEscape(const std::string& text) const;
    AnimatedText::AnimationMode parseTextMode(const String& arg) const;
    const char*                 textLayoutToString(TextLayout layout) const;
    TextLayout                  parseTextLayout(const String& arg) const;
    bool                        decodeHexFrame(const String& hex, Image& out) const;
    String                      imageToHexString(const Image& image) const;
    String                      buildStateJsonPayload();
    String                      buildHtml();
    void                        sendJsonResponse(int code, bool ok, const String& message);
    void                        sendStateJson();
    void                        applyDisplayMode(DisplayMode mode);
    void                        handleRoot();
    void                        handleApiState();
    void                        handleApiText();
    void                        handleApiImages();
    void                        handleApiBrightness();
    void                        handleApiMode();
    void                        handleNotFound();
};
