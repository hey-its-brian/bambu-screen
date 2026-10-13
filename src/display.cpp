#include "display.h"
#include "config.h"
#include "touch.h"

#include <Arduino.h>
#include <SPI.h>
#include <TFT_eSPI.h>
#include <lvgl.h>

#ifndef DISPLAY_ROTATION
#define DISPLAY_ROTATION 1
#endif

// TFT_WIDTH/TFT_HEIGHT describe the panel in portrait; the odd rotations
// (1 and 3) are landscape, so swap them to get the logical screen size.
#if (DISPLAY_ROTATION & 1)
static const int SCREEN_W = TFT_HEIGHT;
static const int SCREEN_H = TFT_WIDTH;
#else
static const int SCREEN_W = TFT_WIDTH;
static const int SCREEN_H = TFT_HEIGHT;
#endif

static TFT_eSPI tft;

// Heap-allocated: 480*40 pixels is too large for the static DRAM segment.
static lv_disp_draw_buf_t drawBuf;
static const size_t DRAW_BUF_PIXELS = SCREEN_W * 40;

static void flushCb(lv_disp_drv_t* disp, const lv_area_t* area, lv_color_t* pixels) {
  uint32_t w = area->x2 - area->x1 + 1;
  uint32_t h = area->y2 - area->y1 + 1;
  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t*)&pixels->full, w * h, true);
  tft.endWrite();
  lv_disp_flush_ready(disp);
}

// Touch backend is selected at build time (see touch.h). Calibration comes
// from the diag build's corner calibration.
static void touchReadCb(lv_indev_drv_t* drv, lv_indev_data_t* data) {
  uint16_t x = 0, y = 0;
  if (touchGet(&x, &y, TOUCH_Z_THRESHOLD)) {
    data->point.x = constrain((int)x, 0, SCREEN_W - 1);
    data->point.y = constrain((int)y, 0, SCREEN_H - 1);
    data->state = LV_INDEV_STATE_PRESSED;
  } else {
    data->state = LV_INDEV_STATE_RELEASED;
  }
}

void displaySetup() {
  tft.init();
  tft.setRotation(DISPLAY_ROTATION);
  tft.fillScreen(TFT_BLACK);
  touchSetup(tft);
  uint16_t calData[5] = TOUCH_CAL_DATA;
  touchSetCal(calData);
  Serial.printf("[display] %dx%d, rotation %d\n", SCREEN_W, SCREEN_H, DISPLAY_ROTATION);

  lv_init();
  lv_color_t* buf1 = (lv_color_t*)malloc(DRAW_BUF_PIXELS * sizeof(lv_color_t));
  if (!buf1) {
    Serial.println("[display] FATAL: draw buffer allocation failed");
    while (true) delay(1000);
  }
  lv_disp_draw_buf_init(&drawBuf, buf1, nullptr, DRAW_BUF_PIXELS);

  static lv_disp_drv_t dispDrv;
  lv_disp_drv_init(&dispDrv);
  dispDrv.hor_res = SCREEN_W;
  dispDrv.ver_res = SCREEN_H;
  dispDrv.flush_cb = flushCb;
  dispDrv.draw_buf = &drawBuf;
  lv_disp_drv_register(&dispDrv);
  tft.setSwapBytes(true);

  static lv_indev_drv_t indevDrv;
  lv_indev_drv_init(&indevDrv);
  indevDrv.type = LV_INDEV_TYPE_POINTER;
  indevDrv.read_cb = touchReadCb;
  lv_indev_drv_register(&indevDrv);
}
