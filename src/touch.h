#pragma once
#include <stdint.h>
#include <TFT_eSPI.h>

// XPT2046 resistive touch with two backends, chosen by build flags:
//   TOUCH_CS          shared display bus, read through TFT_eSPI (4.0 inch CYD)
//   TOUCH_VSPI_CS     separate VSPI bus, read directly (2.8 / 3.2 inch CYD)
// Calibration data uses TFT_eSPI's five-value calData format for both.

void touchSetup(TFT_eSPI& tft);
void touchSetCal(const uint16_t cal[5]);
// Calibrated screen coordinates; true while pressed harder than threshold.
bool touchGet(uint16_t* x, uint16_t* y, uint16_t threshold);
// Raw ADC values (0..4095); z is a pressure estimate, 0 when not pressed.
void touchGetRaw(uint16_t* x, uint16_t* y, uint16_t* z);
// Guided four-corner calibration, same UI as TFT_eSPI::calibrateTouch.
void touchCalibrate(TFT_eSPI& tft, uint16_t cal[5], uint32_t fg, uint32_t bg, uint8_t size);
