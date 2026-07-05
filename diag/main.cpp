// Touch calibration: runs TFT_eSPI's guided corner calibration, shows the
// resulting calData values on screen, then lets you verify accuracy by
// drawing dots wherever you touch.
// Flash with: pio run -e diag -t upload
#include <Arduino.h>
#include <TFT_eSPI.h>

static TFT_eSPI tft;
static uint16_t calData[5];

void setup() {
  Serial.begin(115200);
  tft.init();
  tft.setRotation(1);
  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  tft.drawString("Tap the corner arrows as they appear", 60, 140, 2);
  delay(1500);

  tft.calibrateTouch(calData, TFT_GREEN, TFT_BLACK, 15);

  tft.fillScreen(TFT_BLACK);
  tft.setTextColor(TFT_GREEN, TFT_BLACK);
  tft.drawString("CALIBRATION DONE - report these:", 20, 20, 2);
  tft.setTextColor(TFT_WHITE, TFT_BLACK);
  char buf[64];
  snprintf(buf, sizeof(buf), "%u, %u, %u, %u, %u",
           calData[0], calData[1], calData[2], calData[3], calData[4]);
  tft.drawString(buf, 20, 60, 4);
  Serial.printf("[diag] calData = { %s }\n", buf);

  tft.setTouch(calData);
  tft.setTextColor(TFT_CYAN, TFT_BLACK);
  tft.drawString("Now touch anywhere to verify:", 20, 120, 2);
  tft.drawString("dots should appear under your finger", 20, 145, 2);
}

void loop() {
  uint16_t x, y;
  if (tft.getTouch(&x, &y)) {
    tft.fillCircle(x, y, 3, TFT_YELLOW);
  }
  delay(20);
}
