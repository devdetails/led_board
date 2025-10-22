#include <stdint.h>
#include "config.h"
#include "secrets.h"

#include <WiFi.h>
#include <ArduinoOTA.h>

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
AnimatedText       animatedText;
AnimatedImage      animatedImage;
ShiftRegisterChain shiftChain;

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
        constexpr uint32_t kTimerFrequencyHz = 1'000'000; // 1 tick per microsecond
        gWaitTimer = timerBegin(kTimerFrequencyHz);
        timerAttachInterrupt(gWaitTimer, &waitTimerISR);
    }

    // ensure semaphore starts in the empty state before arming the timer
    xSemaphoreTake(gTimerSemaphore, 0);

    portENTER_CRITICAL(&gTimerMux);
    timerStop(gWaitTimer);
    timerWrite(gWaitTimer, 0);
    timerAlarm(gWaitTimer, microseconds, false, 0);
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
        WebInterface_handle();
        ArduinoOTA.handle();

        const uint32_t now = millis();
        const DisplayMode mode = WebInterface_getDisplayMode();

        Matrix16x16 frameMatrix;
        if (mode == DisplayMode::Text)
        {
            frameMatrix = animatedText.update(now);
        }
        else
        {
            frameMatrix = animatedImage.update(now);
        }

        uint16_t latestBrightnessDuty  = WebInterface_getBrightnessDuty();
        uint16_t latestBrightnessScale = WebInterface_getBrightnessScale();

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

    WebInterface_begin(animatedText,
                       animatedImage,
                       WIFI_SSID,
                       WIFI_PASSWORD,
                       WIFI_HOSTNAME);

    ArduinoOTA.setHostname(WIFI_HOSTNAME);
    ArduinoOTA.onStart([]() 
        {
            const bool updatingSketch = (ArduinoOTA.getCommand() == U_FLASH);
            Serial.println();
            Serial.print(F("[OTA] Start updating "));
            Serial.println(updatingSketch ? F("sketch") : F("filesystem"));
        });

    ArduinoOTA.onEnd([]() 
        {
            Serial.println();
            Serial.println(F("[OTA] Update finished"));
        });

    ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) 
        {
            const unsigned int percent = (total > 0) ? (progress * 100u / total) : 0u;
            Serial.printf("[OTA] Progress: %u%%\r", percent);
        });

    ArduinoOTA.onError([](ota_error_t error) 
        {
            Serial.printf("[OTA] Error[%u]: ", error);
            switch (error)
            {
                case OTA_AUTH_ERROR:    Serial.println(F("Auth failed")); break;
                case OTA_BEGIN_ERROR:   Serial.println(F("Begin failed")); break;
                case OTA_CONNECT_ERROR: Serial.println(F("Connect failed")); break;
                case OTA_RECEIVE_ERROR: Serial.println(F("Receive failed")); break;
                case OTA_END_ERROR:     Serial.println(F("End failed")); break;
                default:                Serial.println(F("Unknown error")); break;
            }
        });

    ArduinoOTA.begin();
    Serial.println(F("[OTA] Ready for updates"));

    if (WiFi.isConnected())
    {
        Serial.print(F("[OTA] Device reachable at http://"));
        Serial.println(WiFi.localIP());
    }
    else
    {
        Serial.println(F("[OTA] Waiting for WiFi connection to announce OTA service"));
    }

    uint32_t    now           = millis();
    Matrix16x16 initialMatrix = animatedText.update(now);

    portENTER_CRITICAL(&frameDataLock);
    frameData.matrix          = initialMatrix;
    frameData.brightnessDuty  = WebInterface_getBrightnessDuty();
    frameData.brightnessScale = WebInterface_getBrightnessScale();
    portEXIT_CRITICAL(&frameDataLock);

    BaseType_t displayResult = xTaskCreatePinnedToCore(displayTask,
                                                       "displayTask",
                                                       4096,
                                                       nullptr,
                                                       2,
                                                       nullptr,
                                                       kDisplayTaskCore);
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

