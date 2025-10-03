#include <stdint.h>
#include "config.h"
#include "secrets.h"

#include <freertos/FreeRTOS.h>
#include <freertos/task.h>

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

static struct FrameData
{
    Matrix16x16 matrix;
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

void displayTask(void* param)
{
    (void)param;
    int row = 0;
    const TickType_t delayTicks = pdMS_TO_TICKS(1);

    FrameData displayFrame;

    for (;;)
    {
        // sync latest frame data with web task thread
        if (row == 0)
        {
            portENTER_CRITICAL(&frameDataLock);
            displayFrame = frameData;
            portEXIT_CRITICAL(&frameDataLock);
        }

        // render current row
        uint32_t word = displayFrame.matrix.composeRowWord(row);
        shiftChain.writeWord(word);

        row = (row + 1) % LED_MATRIX_ROWS;

        vTaskDelay(delayTicks);
    }
}

void webTask(void* param)
{
    (void)param;
    for (;;)
    {
        WebInterface_handle();

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

        portENTER_CRITICAL(&frameDataLock);
        frameData.matrix = frameMatrix;
        portEXIT_CRITICAL(&frameDataLock);
        
        vTaskDelay(1);
    }
}

void setup()
{
    gBackend = &backend;

    Serial.begin(115200);
    delay(500);

    shiftChain.begin();

    WebInterface_begin(animatedText,
                       animatedImage,
                       WIFI_SSID,
                       WIFI_PASSWORD,
                       WIFI_HOSTNAME);

    animatedText.update(millis());

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

