# 16×16 LED Board (ESP32-C6)

Firmware for the DIY 16 × 16 LED board kit (see photos in `docs/board_diy_kit.jpg`, `docs/board_assembled.jpg`, and the schematic in `docs/board_schematic.jpg`). It drives the matrix through four daisy-chained 74HC595 shift registers on an ESP32‑C6. The codebase offers a tiny framebuffer, animated text and image renderers, and a Wi‑Fi web UI that accepts ordinary image uploads and converts them in the browser to the panel’s native 1‑bit format.

## Features
- **Matrix driver** – `Matrix16x16` maintains per-row bitfields and streams them to the register chain.
- **Text animation** – `AnimatedText` renders ASCII strings (hold or scroll mode) with configurable frame timing.
- **Image playback** – `Image` packs a 16 × 16 bitmap into 32 bytes; `AnimatedImage` plays single frames or sequences with adjustable frame duration and looping.
- **Web interface** – ESP32-hosted page (WebServer + WiFi) that
  - edits text animations,
  - uploads arbitrary images (JPEG/PNG/BMP/TIFF, etc.), scales/thresholds them client-side, and pushes the resulting frames,
  - switches between text and image modes, and
  - surfaces current state via a JSON API.
- **Automated tests** – Native Unity suites cover shift-register bitflow, text scrolling, bitmap drawing, and image animation behaviour.

## Repository Layout
- `platformio.ini` – ESP32‑C6 Arduino environment and native test environment with shared build flags.
- `src/`
  - `ShiftRegisterChain.*` – 74HC595 bit-banging helper.
  - `Matrix16x16.*` – framebuffer + row helpers.
  - `AnimatedText.*`, `Image.*`, `AnimatedImage.*` – rendering helpers.
  - `WebInterface.*` – Wi‑Fi setup, HTTP API, and single-page UI.
  - `main.cpp` – minimal sketch wiring the pieces together.
- `test/` – Unity test suites (`test_shiftreg`, `test_running_light`, `test_animated_text`, `test_image`).

## Hardware Pins
Default pins (override via `platformio.ini` build flags or `src/ShiftRegisterChain.h`):

| Signal | Default pin | Notes |
| ------ | ----------- | ----- |
| `SR_PIN_DATA`  | 1  | 595 SER input |
| `SR_PIN_CLK`   | 3  | 595 SRCLK |
| `SR_PIN_LATCH` | 2  | 595 RCLK |
| `SR_PIN_OE`    | 0  | Active-low output enable. NOTE: not connected on the board |

Rows occupy the upper 16 bits of the shifted word, and both rows/columns default to active-high, MSB-first order. Adjust the `ROW_*` / `COL_*` build flags if your wiring differs. Logical coordinates use `(0,0)` at the top-left LED, `x` increases rightward, `y` downward.

### DIY Kit Notes
- To articulate the board with an ESP32 or other external controller, use the external pin header; do **not** populate the onboard DX156 microcontroller (U1) from the kit.
- The photos in `docs/` show the raw kit, an assembled example, and the wiring schematic for reference while soldering.

## Build & Test
```bash
# build firmware
pio run -e esp32c6_supermini

# flash (auto-detects port or honour platformio.ini upload_port)
pio run -e esp32c6_supermini -t upload

# serial monitor
pio device monitor -e esp32c6_supermini

# run native unit tests
pio test -e native
```

## Web Interface
1. Update Wi‑Fi credentials / hostname in `src/main.cpp` (`WIFI_SSID`, `WIFI_PASSWORD`, `WIFI_HOSTNAME`).
2. Flash the firmware and open a serial monitor to read the device IP once connected.
3. Browse to `http://<device-ip>/`.
4. **Text animation** – edit the string, choose hold/scroll, set the frame duration, press **Update Text**; use **Show Text** to force display mode.
5. **Image animation** – pick one or more images, adjust frame duration & looping, click **Upload & Replace Sequence**. The browser rescales to 16 × 16, converts to 1‑bit, and uploads the hex-encoded frames. Switch to image mode with **Show Images**.
6. The status banner reflects success or errors, and the image panel shows how many frames are loaded.

### REST Endpoints
- `GET /api/state` – JSON snapshot of current mode, text settings, and image stats.
- `POST /api/text` – Parameters `text`, `mode` (`hold`/`scroll`), `frameDuration` (ms).
- `POST /api/images` – Parameters `frames` (comma-separated 64-character hex blobs, one per frame), `frameDuration`, `loop` (0/1).
- `POST /api/mode` – Parameter `mode` (`text` or `image`).

## Development Notes
- Wi‑Fi secrets can be kept out of source control by creating `include/secrets.h` defining `WIFI_SSID` / `WIFI_PASSWORD` (and optionally overriding in `main.cpp`). The file is already referenced via include guards; add it to your own `.gitignore` if needed.
- `Image` stores 16 rows of 16 bits (32 bytes); use `rawRows()` for bulk operations.
- `AnimatedImage::setFrames` copies the vector – keep sequences modest to conserve RAM.
- The web UI uses `<canvas>` for scaling/thresholding, enabling offline conversion after the page loads.
- Native tests use `MockBackend` to emulate the shift-register signals, allowing logic verification without hardware.

## Possible Enhancements
- Persist last-used text/image sequences via NVS.
- Add idle cycles to displayTask for brightness control.
- Add playlists that mix text and image animations.
- Improve image import to semantically extract relevant edges (16x16@1bit is extremely limiting)
