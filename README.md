# 16x16 LED Board (ESP32-C6)

Drives a 16x16 LED matrix with four daisy-chained 74HC595 shift registers using an ESP32-C6 module. Firmware provides a running-light diagnostic pattern, and native unit tests emulate the shift register chain for later image-verification work.

## Layout
- platformio.ini - ESP32-C6 Arduino env plus native test env
- src/ - shift-register driver, matrix framebuffer, and running-light sketch
- test/ - Unity tests for register bitflow and running-light logic

## Pins
Default pinout can be overridden in platformio.ini (build_flags) or by editing src/ShiftRegisterChain.h:
- SR_PIN_DATA (SER) = 8
- SR_PIN_CLK  (SRCLK) = 9
- SR_PIN_LATCH(RCLK) = 10
- SR_PIN_OE   (active low) = 18

Row select bits occupy the upper 16 bits of the shifted word (ROW_BITS_HIGH=1) with active-high logic for both rows and columns. If your wiring requires inversion or swapped ordering, adjust the build flags accordingly.
Logical coordinates use `setPixel(x, y)` with `(0,0)` at the physical top-left LED, `x` increasing to the right, and `y` increasing toward the bottom.

## Usage
- Web UI exposes threshold and invert controls with live preview for image uploads.
- Display handling runs on core 1 while the web interface stays on core 0 with a shared mutex-protected bridge.
- Build + upload: pio run -e esp32c6_supermini then pio upload -e esp32c6_supermini
- Monitor: pio device monitor -e esp32c6_supermini
- Run tests: pio test -e native

## Next Steps
- Add web interface for bitmap upload and scrolling text
- Implement PWM brightness via OE pin
- Extend tests to validate image rendering
