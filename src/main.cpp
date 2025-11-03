#include <stdint.h>
#include "config.h"
#include "secrets.h"

#include <WiFi.h>

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/semphr.h>
#include <esp32-hal-timer.h>

#include "Matrix16x16.h"
#include "AnimatedText.h"
#include "AnimatedImage.h"
#include "WebInterface.h"
#include "ShiftRegisterChain.h"

Backend* gBackend = nullptr;

Backend            backend;
AnimatedText       animatedTextTop;
AnimatedText       animatedTextBottom;
AnimatedImage      animatedImage;
ShiftRegisterChain shiftChain;
WebInterface       webInterface(animatedTextTop, animatedTextBottom, animatedImage);

namespace
{
    hw_timer_t*       gWaitTimer      = nullptr;
    SemaphoreHandle_t gTimerSemaphore = nullptr;
    portMUX_TYPE      gTimerMux       = portMUX_INITIALIZER_UNLOCKED;

    void IRAM_ATTR waitTimerISR()
    {
        BaseType_t higherPriorityTaskWoken = pdFALSE;
        if (gTimerSemaphore != nullptr)
        {
            xSemaphoreGiveFromISR(gTimerSemaphore, &higherPriorityTaskWoken);
        }
        if (higherPriorityTaskWoken == pdTRUE)
        {
            portYIELD_FROM_ISR();
        }
    }
}

static struct FrameData
{
    Matrix16x16 matrix;
    uint16_t    brightnessDuty  = 0;
    uint16_t    brightnessScale = 0;
} frameData;

portMUX_TYPE frameDataLock = portMUX_INITIALIZER_UNLOCKED;

static void updateFrameData(const Matrix16x16& newFrame)
{
    const uint16_t brightnessDuty  = webInterface.getBrightnessDuty();
    const uint16_t brightnessScale = webInterface.getBrightnessScale();

    portENTER_CRITICAL(&frameDataLock);
    frameData.matrix          = newFrame;
    frameData.brightnessDuty  = brightnessDuty;
    frameData.brightnessScale = brightnessScale;
    portEXIT_CRITICAL(&frameDataLock);
}

static Matrix16x16 composeTextFrame(uint32_t nowMs)
{
    const TextLayout layout = webInterface.getTextLayout();
    static TextLayout appliedLayout = TextLayout::Dual;

    if (layout != appliedLayout)
    {
        switch (layout)
        {
            case TextLayout::Dual:
                animatedTextTop.setVerticalAlignment(AnimatedText::VerticalAlignment::UpperHalf);
                animatedTextBottom.setVerticalAlignment(AnimatedText::VerticalAlignment::LowerHalf);
                break;
            case TextLayout::SingleTop:
                animatedTextTop.setVerticalAlignment(AnimatedText::VerticalAlignment::Full);
                animatedTextBottom.setVerticalAlignment(AnimatedText::VerticalAlignment::LowerHalf);
                break;
            case TextLayout::SingleBottom:
                animatedTextTop.setVerticalAlignment(AnimatedText::VerticalAlignment::UpperHalf);
                animatedTextBottom.setVerticalAlignment(AnimatedText::VerticalAlignment::Full);
                break;
            default:
                break;
        }
        appliedLayout = layout;
    }

    Matrix16x16 topFrame    = animatedTextTop.update(nowMs);
    Matrix16x16 bottomFrame = animatedTextBottom.update(nowMs);

    switch (layout)
    {
        case TextLayout::Dual:
            topFrame.merge(bottomFrame);
            return topFrame;
        case TextLayout::SingleTop:
            return topFrame;
        case TextLayout::SingleBottom:
            return bottomFrame;
        default:
            topFrame.merge(bottomFrame);
            return topFrame;
    }
}

// pin display- and webserver tasks to different cores chips with more than one core
#if defined(portNUM_PROCESSORS) && (portNUM_PROCESSORS > 1)
constexpr BaseType_t kDisplayTaskCore = 1;
constexpr BaseType_t kWebTaskCore     = 0;
#else
constexpr BaseType_t kDisplayTaskCore = tskNO_AFFINITY;
constexpr BaseType_t kWebTaskCore     = tskNO_AFFINITY;
#endif

static void waitWithHardwareTimer(uint32_t microseconds)
{
    if (microseconds == 0)
        return;

    if (gTimerSemaphore == nullptr)
    {
        gTimerSemaphore = xSemaphoreCreateBinary();
        configASSERT(gTimerSemaphore != nullptr);
    }

    if (gWaitTimer == nullptr)
    {
        constexpr uint8_t  kTimerNumber   = 0;
        constexpr uint16_t kTimerDivider  = 80; // 80 MHz / 80 = 1 tick per microsecond
        gWaitTimer = timerBegin(kTimerNumber, kTimerDivider, true);
        configASSERT(gWaitTimer != nullptr);
        timerAttachInterrupt(gWaitTimer, &waitTimerISR, true);
    }

    // ensure semaphore starts in the empty state before arming the timer
    xSemaphoreTake(gTimerSemaphore, 0);

    portENTER_CRITICAL(&gTimerMux);
    timerAlarmDisable(gWaitTimer);
    timerStop(gWaitTimer);
    timerWrite(gWaitTimer, 0);
    timerAlarmWrite(gWaitTimer, microseconds, false);
    timerAlarmEnable(gWaitTimer);
    timerStart(gWaitTimer);
    portEXIT_CRITICAL(&gTimerMux);

    // block until the hardware timer fires
    xSemaphoreTake(gTimerSemaphore, portMAX_DELAY);
}

void displayTask(void* param)
{
    (void)param;

    int row = 0;
    constexpr uint32_t kRowPeriodUs = 520; // 16 rows x 520us ~= 8 ms per frame (~120 Hz refresh)

    FrameData displayFrame;
    portENTER_CRITICAL(&frameDataLock);
    displayFrame = frameData;
    portEXIT_CRITICAL(&frameDataLock);

    uint32_t brightnessScale = displayFrame.brightnessScale;
    uint16_t brightnessDuty  = displayFrame.brightnessDuty;
    uint32_t rowOnTimeUs     = 0;
    uint32_t rowOffTimeUs    = kRowPeriodUs;

    auto updateBrightnessTiming = [&]() 
    {
        // zero brightness
        if (brightnessScale == 0 || brightnessDuty == 0)
        {
            rowOnTimeUs  = 0;
            rowOffTimeUs = kRowPeriodUs;
            return;
        }

        // full brightness
        if (brightnessDuty >= brightnessScale)
        {
            rowOnTimeUs  = kRowPeriodUs;
            rowOffTimeUs = 0;
            return;
        }

        rowOnTimeUs = static_cast<uint32_t>((static_cast<uint64_t>(kRowPeriodUs) * brightnessDuty) / brightnessScale);
        if (rowOnTimeUs >= kRowPeriodUs)
        {
            rowOnTimeUs  = kRowPeriodUs;
            rowOffTimeUs = 0;
        }
        else
        {
            rowOffTimeUs = kRowPeriodUs - rowOnTimeUs;
        }
    };

    updateBrightnessTiming();

    for (;;)
    {
        // sync latest frame data with web task thread
        if (row == 0)
        {
            vTaskDelay(pdMS_TO_TICKS(1));

            portENTER_CRITICAL(&frameDataLock);
            displayFrame = frameData;
            portEXIT_CRITICAL(&frameDataLock);

            brightnessScale = displayFrame.brightnessScale;
            brightnessDuty  = displayFrame.brightnessDuty;

            updateBrightnessTiming();
        }

        // render current row with per-row PWM timing
        const uint32_t rowWord = displayFrame.matrix.composeRowWord(row);

        uint32_t offDelayUs = rowOffTimeUs;
        if (rowOnTimeUs == 0)
        {
            offDelayUs = kRowPeriodUs;
        }
        else
        {
            shiftChain.writeWord(rowWord);
            waitWithHardwareTimer(rowOnTimeUs);
        }

        shiftChain.writeWord(~0u);
        if (offDelayUs > 0)
        {
            waitWithHardwareTimer(offDelayUs);
        }

        row = (row + 1) % LED_MATRIX_ROWS;
    }
}

void webTask(void* param)
{
    (void)param;
    for (;;)
    {
        webInterface.handle();

        const uint32_t now = millis();
        const DisplayMode mode = webInterface.getDisplayMode();

        Matrix16x16 frameMatrix;
        if (mode == DisplayMode::Text)
        {
            frameMatrix = composeTextFrame(now);
        }
        else
        {
            frameMatrix = animatedImage.update(now);
        }

        uint16_t latestBrightnessDuty  = webInterface.getBrightnessDuty();
        uint16_t latestBrightnessScale = webInterface.getBrightnessScale();

        portENTER_CRITICAL(&frameDataLock);
        frameData.matrix          = frameMatrix;
        frameData.brightnessDuty  = latestBrightnessDuty;
        frameData.brightnessScale = latestBrightnessScale;
        portEXIT_CRITICAL(&frameDataLock);
        
        vTaskDelay(pdMS_TO_TICKS(1));
    }
}

void setup()
{
    gBackend = &backend;

    shiftChain.begin();

    Serial.begin(115200);
    delay(500);

    animatedTextTop.setVerticalAlignment(AnimatedText::VerticalAlignment::UpperHalf);
    animatedTextBottom.setVerticalAlignment(AnimatedText::VerticalAlignment::LowerHalf);

    animatedTextTop.setAnimationMode(AnimatedText::AnimationMode::Scroll);
    animatedTextTop.setLooping(true);
    animatedTextTop.setText("Connecting ");

    animatedTextBottom.setAnimationMode(AnimatedText::AnimationMode::Hold);
    animatedTextBottom.setLooping(true);
    animatedTextBottom.setText("...");

    updateFrameData(composeTextFrame(millis()));

    BaseType_t displayResult = xTaskCreatePinnedToCore(displayTask,
                                                       "displayTask",
                                                       4096,
                                                       nullptr,
                                                       2,
                                                       nullptr,
                                                       kDisplayTaskCore);

    constexpr uint32_t kConnectTimeoutMs      = 20000;
    constexpr uint32_t kDisplayUpdateInterval = 40;
    constexpr uint32_t kSerialDotInterval     = 500;

    const bool wifiCredentialsPresent = (WIFI_SSID != nullptr && WIFI_SSID[0] != '\0');
    bool       wifiConnected          = false;

    if (wifiCredentialsPresent)
    {
        WiFi.mode(WIFI_STA);
        WiFi.setAutoReconnect(true);

        if (WIFI_HOSTNAME != nullptr && WIFI_HOSTNAME[0] != '\0')
        {
            WiFi.setHostname(WIFI_HOSTNAME);
        }

        WiFi.begin(WIFI_SSID, WIFI_PASSWORD != nullptr ? WIFI_PASSWORD : "");
        updateFrameData(composeTextFrame(millis()));

        Serial.print(F("Connecting to WiFi"));

        uint32_t connectStart = millis();
        uint32_t lastDot      = connectStart;

        while (WiFi.status() != WL_CONNECTED && (millis() - connectStart) < kConnectTimeoutMs)
        {
            const uint32_t now = millis();

            if (now - lastDot >= kSerialDotInterval)
            {
                Serial.print('.');
                lastDot = now;
            }

            updateFrameData(composeTextFrame(now));
            delay(kDisplayUpdateInterval);
        }

        Serial.println();
        updateFrameData(composeTextFrame(millis()));

        wifiConnected = (WiFi.status() == WL_CONNECTED);
        if (wifiConnected)
        {
            Serial.print(F("Connected. IP address: "));
            Serial.println(WiFi.localIP());
        }
        else
        {
            Serial.println(F("WiFi connection failed (continuing offline)."));
        }
    }
    else
    {
        Serial.println(F("WiFi SSID not provided; running without network."));
    }

    webInterface.begin();

    updateFrameData(composeTextFrame(millis()));

    std::string topLine;
    std::string bottomLine;

    if (wifiCredentialsPresent)
    {
        const char* ssid = (WIFI_SSID != nullptr) ? WIFI_SSID : "";
        topLine = "SSID: ";
        topLine += ssid;
        topLine += ' ';

        if (wifiConnected && WiFi.isConnected())
        {
            const String ipString = WiFi.localIP().toString();
            bottomLine = "IP: ";
            bottomLine += ipString.c_str();
            bottomLine += ' ';
        }
        else
        {
            bottomLine = "IP: offline ";
        }
    }
    else
    {
        topLine    = "SSID: <none> ";
        bottomLine = "IP: offline ";
    }

    animatedTextTop.setAnimationMode(AnimatedText::AnimationMode::Scroll);
    animatedTextTop.setLooping(true);
    animatedTextTop.setText(topLine);

    animatedTextBottom.setAnimationMode(AnimatedText::AnimationMode::Scroll);
    animatedTextBottom.setLooping(true);
    animatedTextBottom.setText(bottomLine);

    updateFrameData(composeTextFrame(millis()));


    BaseType_t webResult = xTaskCreatePinnedToCore(webTask,
                                                   "webTask",
                                                   8192,
                                                   nullptr,
                                                   1,
                                                   nullptr,
                                                   kWebTaskCore);

    configASSERT(displayResult == pdPASS);
    configASSERT(webResult     == pdPASS);
}

void loop()
{
    vTaskDelay(pdMS_TO_TICKS(1000));
}
