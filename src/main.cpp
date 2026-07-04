#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>

#include "config.h"
#include "display.h"
#include "ui.h"
#include "bambu.h"

void setup() {
  Serial.begin(115200);
  Serial.println("\n[main] bambu-screen starting");

  displaySetup();
  uiSetup();

  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);  // keeps MQTT latency low
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  bambuSetup();
}

void loop() {
  static uint32_t lastWifiRetry = 0;
  if (WiFi.status() != WL_CONNECTED && millis() - lastWifiRetry > 15000) {
    lastWifiRetry = millis();
    WiFi.reconnect();
  }

  bambuLoop();
  uiUpdate();
  lv_timer_handler();
  delay(5);
}
