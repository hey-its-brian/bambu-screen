# bambu-screen

Touchscreen status display and basic controller for a Bambu Lab P1S in LAN
Developer Mode. Runs on the ESP32-2432S028R "Cheap Yellow Display" (2.8"
320x240 ILI9341 with XPT2046 resistive touch).

Talks directly to the printer's local MQTT broker over TLS on port 8883. No
cloud and no Home Assistant required.

## Features

- Live status: print state, job name, progress, layer count, time remaining
- Nozzle and bed temperatures (current/target)
- Pause / Resume and Stop (with confirmation dialog)
- Chamber light toggle
- Print speed cycling (Silent / Standard / Sport / Ludicrous)

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
   pio run -t upload
   pio device monitor   # optional: logs at 115200 baud
   ```

## Troubleshooting

- Colors inverted or washed out: CYD panels vary. In `platformio.ini` try
  removing `-D TFT_INVERSION_ON=1`, or swap `ILI9341_2_DRIVER` for
  `ILI9341_DRIVER`.
- Touch lands offset from where you press: adjust the `TOUCH_*` calibration
  constants in `include/config.h`.
- Stuck on "Finding printer...": verify the printer IP, that LAN Developer
  Mode is enabled, and that the access code is current.

## Project layout

| File | Role |
|---|---|
| `src/main.cpp` | Setup and main loop |
| `src/display.cpp` | TFT, touch, and LVGL driver glue |
| `src/ui.cpp` | Screen layout and refresh from printer state |
| `src/bambu.cpp` | MQTT client, report parsing, printer commands |
