#pragma once

#include <stdint.h>

// hardware related stuff
constexpr int      PIN_SR_DATA  = 1;
constexpr int      PIN_SR_CLK   = 4;
constexpr int      PIN_SR_LATCH = 2;
constexpr int      PIN_SR_OE    = 0;

constexpr uint32_t SHIFTREG_SPI_FREQUENCY_HZ = 4'000'000;

// Animated text defaults
constexpr const char* DEFAULT_INITIAL_TEXT                  = "Hello World  ";
constexpr bool        DEFAULT_TEXT_LOOPING                  = true;
constexpr bool        DEFAULT_TEXT_ANIMATION_MODE           = 1; // 0: Hold, 1: Scroll
constexpr uint32_t    DEFAULT_TEXT_FRAME_DURATION_LOOP_MS   = 50;
constexpr uint32_t    DEFAULT_TEXT_FRAME_DURATION_HOLD_MS   = 500;

// Animated image defaults
constexpr uint32_t    DEFAULT_IMAGE_FRAME_DURATION_MS  = 200;
constexpr bool        DEFAULT_IMAGE_LOOPING            = true;


constexpr const char* WIFI_HOSTNAME = "led_panel";

// Arduino dependencies 
#ifdef ARDUINO
    #include <Arduino.h>
    #include <SPI.h>
    #include <ArduinoBackend.h>

    typedef ArduinoBackend Backend;
#else
	// subset of arduino symbols needed for compilation
    #define HIGH     1
    #define LOW      0
    #define OUTPUT 0x3

    #define SPI_LSBFIRST 0
    #define SPI_MSBFIRST 1

    #define SPI_MODE0 0
    #define SPI_MODE1 1
    #define SPI_MODE2 2
    #define SPI_MODE3 3

    struct SPISettings
    {
        uint32_t clock    = 1000000;
        uint8_t  bitOrder = SPI_MSBFIRST;
        uint8_t  dataMode = SPI_MODE0;
    };

    #include <MockBackend.h>

    typedef MockBackend Backend;
#endif
