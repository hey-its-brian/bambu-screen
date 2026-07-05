/* Copy this file to include/config.h and fill in your values.
 * config.h is gitignored. */
#pragma once

// ---- WiFi ----
#define WIFI_SSID     "your-wifi-ssid"
#define WIFI_PASSWORD "your-wifi-password"

// ---- Printer (P1S in LAN / Developer mode) ----
// IP address of the printer on your LAN (set a DHCP reservation for it).
#define PRINTER_IP "192.168.1.100"
// Serial number: printer screen -> Settings -> Device, or Bambu Studio device page.
#define PRINTER_SERIAL "01P00A000000000"
// LAN access code: printer screen -> Settings -> WLAN -> Access Code.
#define PRINTER_ACCESS_CODE "12345678"

// ---- Touch calibration ----
// Values from TFT_eSPI's calibrateTouch (the diag build runs it: pio run
// -e diag -t upload, tap the corner arrows, use the reported numbers).
#define TOUCH_CAL_DATA {297, 3492, 509, 3132, 7}
// Press pressure threshold; idle noise reads z < 25.
#define TOUCH_Z_THRESHOLD 200
