#pragma once
#include <Arduino.h>

enum class BambuConn : uint8_t {
  NO_WIFI,
  WIFI_CONNECTING,
  MQTT_CONNECTING,
  CONNECTED,
};

struct PrinterState {
  BambuConn conn = BambuConn::NO_WIFI;
  char gcodeState[16] = "";   // IDLE / RUNNING / PAUSE / FINISH / FAILED / PREPARE / SLICING
  char taskName[64]  = "";
  int  percent       = 0;
  int  remainingMin  = 0;
  int  layer         = 0;
  int  totalLayers   = 0;
  float nozzleTemp   = 0, nozzleTarget = 0;
  float bedTemp      = 0, bedTarget    = 0;
  int  speedLevel    = 2;     // 1=Silent 2=Standard 3=Sport 4=Ludicrous
  bool chamberLight  = false;
  bool dirty         = true;  // UI should redraw
};

extern PrinterState printerState;

void bambuSetup();
void bambuLoop();

void bambuPause();
void bambuResume();
void bambuStop();
void bambuSetLight(bool on);
void bambuSetSpeed(int level);  // 1..4

const char* bambuSpeedName(int level);
