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

// ---- Touch calibration (raw XPT2046 range -> screen) ----
// If touches land offset from where you press, tweak these.
#define TOUCH_X_MIN 200
#define TOUCH_X_MAX 3700
#define TOUCH_Y_MIN 240
#define TOUCH_Y_MAX 3800
