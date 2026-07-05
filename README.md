# bambu-screen

Touchscreen status display and basic controller for a Bambu Lab P1S in LAN
Developer Mode. Runs on the ESP32-3248S035 (Sunton 3.5" CYD variant): 480x320
ST7796 display with XPT2046 resistive touch, classic ESP32-WROOM-32.

Talks directly to the printer's local MQTT broker over TLS on port 8883. No
cloud and no Home Assistant required.

## Features

- Live status: print state, job name, progress, layer count, time remaining
- Nozzle and bed temperatures (current/target)
- Pause / Resume and Stop (with confirmation dialog)
- Chamber light toggle
- Print speed cycling (Silent / Standard / Sport / Ludicrous)

## Hardware notes

- Display: ST7796 on the HSPI bus (MISO 12, MOSI 13, SCLK 14, CS 15, DC 2)
- Backlight: GPIO 27, active high
- Touch: XPT2046 sharing the display SPI bus, chip select on GPIO 33.
  TFT_eSPI arbitrates the bus between the two chips via `TOUCH_CS`.
- Pause/Stop buttons are only enabled while a print is running or paused.

## Setup

1. Enable LAN Developer Mode on the printer and note its IP address.
2. Get your LAN access code (printer screen > Settings > WLAN > Access Code)
   and serial number (printer screen > Settings > Device).
3. Create your config:

   ```sh
   cp include/config.example.h include/config.h
   # edit include/config.h with your WiFi and printer details
   ```

4. Build and flash with the board connected over USB-C:

   ```sh
   pio run -e cyd -t upload
   pio device monitor   # optional: logs at 115200 baud
   ```

## Touch calibration

Each resistive panel is slightly different. To recalibrate:

1. Flash the diagnostic build: `pio run -e diag -t upload`
2. Tap the green corner arrows as they appear.
3. Read the five calData values from the serial monitor (115200 baud) and put
   them in `TOUCH_CAL_DATA` in `include/config.h`.
4. Reflash the main firmware: `pio run -e cyd -t upload`

## Troubleshooting

- Blank screen but firmware boots: confirm the panel driver and backlight pin
  match your board variant. The diag build cycles full-screen colors, which
  makes panel problems obvious.
- Touches land on the wrong spot: rerun the touch calibration above.
- Stuck on "Finding printer...": verify the printer IP, that LAN Developer
  Mode is enabled, and that the access code is current.
- Upload fails with serial noise errors: lower `upload_speed` in
  `platformio.ini` to 115200, use a different USB-C cable, or hold BOOT while
  the upload starts.

## Project layout

| File | Role |
|---|---|
| `src/main.cpp` | Setup and main loop |
| `src/display.cpp` | TFT, touch, and LVGL driver glue |
| `src/ui.cpp` | Screen layout and refresh from printer state |
| `src/bambu.cpp` | MQTT client, report parsing, printer commands |
| `diag/main.cpp` | Hardware diagnostic and touch calibration build |
