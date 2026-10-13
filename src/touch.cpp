#include "touch.h"

#include <Arduino.h>
#include <SPI.h>

#if defined(TOUCH_CS)
// ---------------------------------------------------------------------------
// Shared-bus backend: TFT_eSPI owns the XPT2046.
// ---------------------------------------------------------------------------
static TFT_eSPI* gTft;

void touchSetup(TFT_eSPI& tft) { gTft = &tft; }
void touchSetCal(const uint16_t cal[5]) { gTft->setTouch(const_cast<uint16_t*>(cal)); }
bool touchGet(uint16_t* x, uint16_t* y, uint16_t threshold) { return gTft->getTouch(x, y, threshold); }
void touchGetRaw(uint16_t* x, uint16_t* y, uint16_t* z) {
  gTft->getTouchRaw(x, y);
  *z = gTft->getTouchRawZ();
}
void touchCalibrate(TFT_eSPI& tft, uint16_t cal[5], uint32_t fg, uint32_t bg, uint8_t size) {
  tft.calibrateTouch(cal, fg, bg, size);
}

#elif defined(TOUCH_VSPI_CS)
// ---------------------------------------------------------------------------
// VSPI backend: direct XPT2046 reads on a bus the display does not use.
// ---------------------------------------------------------------------------
#ifndef TOUCH_VSPI_FREQ
#define TOUCH_VSPI_FREQ 2000000
#endif
static const int RAW_ERR = 20;  // max jitter between two samples of a valid touch

static SPIClass vspi(VSPI);
static TFT_eSPI* gTft;
static uint16_t calX0 = 1, calX1 = 1, calY0 = 1, calY1 = 1;
static bool calRotate = false, calInvX = false, calInvY = false;

static uint16_t xfer(uint8_t cmd) {
  vspi.transfer(cmd);
  return vspi.transfer16(0) >> 3;
}

static void select() {
  vspi.beginTransaction(SPISettings(TOUCH_VSPI_FREQ, MSBFIRST, SPI_MODE0));
  digitalWrite(TOUCH_VSPI_CS, LOW);
}
static void deselect() {
  digitalWrite(TOUCH_VSPI_CS, HIGH);
  vspi.endTransaction();
}

void touchSetup(TFT_eSPI& tft) {
  gTft = &tft;
  pinMode(TOUCH_VSPI_CS, OUTPUT);
  digitalWrite(TOUCH_VSPI_CS, HIGH);
#ifdef TOUCH_VSPI_IRQ
  pinMode(TOUCH_VSPI_IRQ, INPUT);
#endif
  vspi.begin(TOUCH_VSPI_CLK, TOUCH_VSPI_MISO, TOUCH_VSPI_MOSI, -1);
}

void touchSetCal(const uint16_t cal[5]) {
  calX0 = cal[0] ? cal[0] : 1;
  calX1 = cal[1] ? cal[1] : 1;
  calY0 = cal[2] ? cal[2] : 1;
  calY1 = cal[3] ? cal[3] : 1;
  calRotate = cal[4] & 0x01;
  calInvX = cal[4] & 0x02;
  calInvY = cal[4] & 0x04;
}

void touchGetRaw(uint16_t* x, uint16_t* y, uint16_t* z) {
  select();
  uint16_t z1 = xfer(0xB0) & 0x0FFF;
  uint16_t z2 = xfer(0xC0) & 0x0FFF;
  *x = xfer(0xD0) & 0x0FFF;
  *y = xfer(0x90) & 0x0FFF;
  deselect();
  // A pressed panel pulls z1 up and z2 down. A silent bus reads all zeros or
  // all ones, which the plain TFT_eSPI formula (4095 + z1 - z2) would report
  // as full pressure, so reject those explicitly.
  if (z1 < 10 || z2 > 4000) { *z = 0; return; }
  *z = 4095 + z1 - z2;
}

static uint16_t rawZ() {
  uint16_t x, y, z;
  touchGetRaw(&x, &y, &z);
  return z;
}

// Debounced raw read, mirrors TFT_eSPI::validTouch.
static bool validTouch(uint16_t* x, uint16_t* y, uint16_t threshold) {
  uint16_t z1 = 1, z2 = 0;
  while (z1 > z2) {  // wait until pressure stops increasing
    z2 = z1;
    z1 = rawZ();
    delay(1);
  }
  if (z1 <= threshold) return false;
  uint16_t xa, ya, xb, yb, z;
  touchGetRaw(&xa, &ya, &z);
  delay(1);
  if (rawZ() <= threshold) return false;
  delay(2);
  touchGetRaw(&xb, &yb, &z);
  if (abs((int)xa - (int)xb) > RAW_ERR) return false;
  if (abs((int)ya - (int)yb) > RAW_ERR) return false;
  *x = xa;
  *y = ya;
  return true;
}

bool touchGet(uint16_t* x, uint16_t* y, uint16_t threshold) {
  uint16_t rx, ry;
  if (!validTouch(&rx, &ry, threshold)) return false;
  const int w = gTft->width(), h = gTft->height();
  int xx, yy;
  if (!calRotate) {
    xx = ((int)rx - calX0) * w / calX1;
    yy = ((int)ry - calY0) * h / calY1;
  } else {
    xx = ((int)ry - calX0) * w / calX1;
    yy = ((int)rx - calY0) * h / calY1;
  }
  if (calInvX) xx = w - xx;
  if (calInvY) yy = h - yy;
  *x = constrain(xx, 0, w - 1);
  *y = constrain(yy, 0, h - 1);
  return true;
}

void touchCalibrate(TFT_eSPI& tft, uint16_t cal[5], uint32_t fg, uint32_t bg, uint8_t size) {
  const int w = tft.width(), h = tft.height();
  int32_t v[8] = {0};
  uint16_t xt, yt;
  for (int i = 0; i < 4; i++) {
    tft.fillRect(0, 0, size + 1, size + 1, bg);
    tft.fillRect(0, h - size - 1, size + 1, size + 1, bg);
    tft.fillRect(w - size - 1, 0, size + 1, size + 1, bg);
    tft.fillRect(w - size - 1, h - size - 1, size + 1, size + 1, bg);
    switch (i) {
      case 0:  // top left
        tft.drawLine(0, 0, 0, size, fg); tft.drawLine(0, 0, size, 0, fg); tft.drawLine(0, 0, size, size, fg);
        break;
      case 1:  // bottom left
        tft.drawLine(0, h - size - 1, 0, h - 1, fg); tft.drawLine(0, h - 1, size, h - 1, fg);
        tft.drawLine(size, h - size - 1, 0, h - 1, fg);
        break;
      case 2:  // top right
        tft.drawLine(w - size - 1, 0, w - 1, 0, fg); tft.drawLine(w - size - 1, size, w - 1, 0, fg);
        tft.drawLine(w - 1, size, w - 1, 0, fg);
        break;
      case 3:  // bottom right
        tft.drawLine(w - size - 1, h - size - 1, w - 1, h - 1, fg);
        tft.drawLine(w - 1, h - 1 - size, w - 1, h - 1, fg);
        tft.drawLine(w - 1 - size, h - 1, w - 1, h - 1, fg);
        break;
    }
    if (i > 0) delay(1000);  // let the user release the previous corner
    for (int j = 0; j < 8; j++) {
      while (!validTouch(&xt, &yt, 350 / 2)) {}
      v[i * 2] += xt;
      v[i * 2 + 1] += yt;
    }
    v[i * 2] /= 8;
    v[i * 2 + 1] /= 8;
  }
  tft.fillRect(0, 0, size + 1, size + 1, bg);
  tft.fillRect(0, h - size - 1, size + 1, size + 1, bg);
  tft.fillRect(w - size - 1, 0, size + 1, size + 1, bg);
  tft.fillRect(w - size - 1, h - size - 1, size + 1, size + 1, bg);

  // Between corner 0 and 1 only the screen y changed; if raw x moved more
  // than raw y, the touch axes are swapped relative to the display.
  int32_t x0, x1, y0, y1;
  bool rotate = abs(v[0] - v[2]) > abs(v[1] - v[3]);
  if (rotate) {
    x0 = (v[1] + v[3]) / 2; x1 = (v[5] + v[7]) / 2;
    y0 = (v[0] + v[4]) / 2; y1 = (v[2] + v[6]) / 2;
  } else {
    x0 = (v[0] + v[2]) / 2; x1 = (v[4] + v[6]) / 2;
    y0 = (v[1] + v[5]) / 2; y1 = (v[3] + v[7]) / 2;
  }
  bool invX = false, invY = false;
  if (x0 > x1) { int32_t t = x0; x0 = x1; x1 = t; invX = true; }
  if (y0 > y1) { int32_t t = y0; y0 = y1; y1 = t; invY = true; }
  x1 -= x0;
  y1 -= y0;
  cal[0] = x0 ? x0 : 1;
  cal[1] = x1 ? x1 : 1;
  cal[2] = y0 ? y0 : 1;
  cal[3] = y1 ? y1 : 1;
  cal[4] = rotate | (invX << 1) | (invY << 2);
  touchSetCal(cal);
}

#else
#error "Define TOUCH_CS (shared bus) or TOUCH_VSPI_CS (separate bus) in platformio.ini"
#endif
