#include "ui.h"
#include "bambu.h"

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>

// Palette
#define COL_BG      lv_color_hex(0x101418)
#define COL_CARD    lv_color_hex(0x1a2129)
#define COL_TEXT    lv_color_hex(0xe8eaed)
#define COL_MUTED   lv_color_hex(0x8a939c)
#define COL_ACCENT  lv_color_hex(0x00ae42)  // Bambu green
#define COL_WARN    lv_color_hex(0xe0a800)
#define COL_DANGER  lv_color_hex(0xd9453a)

static lv_obj_t* statusLabel;
static lv_obj_t* wifiLabel;
static lv_obj_t* taskLabel;
static lv_obj_t* percentLabel;
static lv_obj_t* layerLabel;
static lv_obj_t* timeLabel;
static lv_obj_t* progressBar;
static lv_obj_t* nozzleLabel;
static lv_obj_t* bedLabel;
static lv_obj_t* speedLabel;
static lv_obj_t* pauseBtn;
static lv_obj_t* pauseBtnLabel;
static lv_obj_t* stopBtn;
static lv_obj_t* lightBtn;
static lv_obj_t* lightBtnLabel;
static lv_obj_t* speedBtn;

static bool isState(const char* s) { return strcmp(printerState.gcodeState, s) == 0; }
static bool isPrinting() { return isState("RUNNING") || isState("PREPARE") || isState("SLICING"); }

// ---- button actions ----

static void onPauseClicked(lv_event_t*) {
  if (isState("PAUSE")) bambuResume();
  else if (isPrinting()) bambuPause();
}

static void onStopConfirm(lv_event_t* e) {
  lv_obj_t* mbox = lv_event_get_current_target(e);
  if (lv_msgbox_get_active_btn(mbox) == 0) bambuStop();
  lv_msgbox_close(mbox);
}

static void onStopClicked(lv_event_t*) {
  if (!isPrinting() && !isState("PAUSE")) return;
  static const char* btns[] = {"Stop", "Cancel", ""};
  lv_obj_t* mbox = lv_msgbox_create(nullptr, "Stop print?", "This cancels the current job.", btns, false);
  lv_obj_set_style_bg_color(mbox, COL_CARD, 0);
  lv_obj_set_style_text_color(mbox, COL_TEXT, 0);
  lv_obj_add_event_cb(mbox, onStopConfirm, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_center(mbox);
}

static void onLightClicked(lv_event_t*) {
  bambuSetLight(!printerState.chamberLight);
  // Optimistic: the next report confirms the real state.
  printerState.chamberLight = !printerState.chamberLight;
  printerState.dirty = true;
}

static void onSpeedClicked(lv_event_t*) {
  int next = printerState.speedLevel % 4 + 1;
  bambuSetSpeed(next);
  printerState.speedLevel = next;
  printerState.dirty = true;
}

// ---- layout ----

static lv_obj_t* makeButton(lv_obj_t* parent, const char* text, lv_event_cb_t cb,
                            lv_color_t bg, lv_obj_t** labelOut = nullptr) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_set_size(btn, 72, 44);
  lv_obj_set_style_bg_color(btn, bg, 0);
  lv_obj_set_style_radius(btn, 8, 0);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* label = lv_label_create(btn);
  lv_label_set_text(label, text);
  lv_obj_center(label);
  if (labelOut) *labelOut = label;
  return btn;
}

void uiSetup() {
  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, COL_BG, 0);
  lv_obj_set_style_text_color(scr, COL_TEXT, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  // Header: connection/printer state + wifi
  statusLabel = lv_label_create(scr);
  lv_obj_set_style_text_font(statusLabel, &lv_font_montserrat_16, 0);
  lv_obj_align(statusLabel, LV_ALIGN_TOP_LEFT, 10, 8);
  lv_label_set_text(statusLabel, "Starting...");

  wifiLabel = lv_label_create(scr);
  lv_obj_align(wifiLabel, LV_ALIGN_TOP_RIGHT, -10, 9);
  lv_label_set_text(wifiLabel, LV_SYMBOL_WIFI);
  lv_obj_set_style_text_color(wifiLabel, COL_MUTED, 0);

  // Job name
  taskLabel = lv_label_create(scr);
  lv_obj_set_width(taskLabel, 300);
  lv_label_set_long_mode(taskLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_style_text_color(taskLabel, COL_MUTED, 0);
  lv_obj_align(taskLabel, LV_ALIGN_TOP_LEFT, 10, 34);
  lv_label_set_text(taskLabel, "");

  // Progress: big percent, layer + time to the right
  percentLabel = lv_label_create(scr);
  lv_obj_set_style_text_font(percentLabel, &lv_font_montserrat_32, 0);
  lv_obj_align(percentLabel, LV_ALIGN_TOP_LEFT, 10, 58);
  lv_label_set_text(percentLabel, "--%");

  layerLabel = lv_label_create(scr);
  lv_obj_set_style_text_color(layerLabel, COL_MUTED, 0);
  lv_obj_align(layerLabel, LV_ALIGN_TOP_RIGHT, -10, 58);
  lv_label_set_text(layerLabel, "");

  timeLabel = lv_label_create(scr);
  lv_obj_set_style_text_color(timeLabel, COL_MUTED, 0);
  lv_obj_align(timeLabel, LV_ALIGN_TOP_RIGHT, -10, 76);
  lv_label_set_text(timeLabel, "");

  progressBar = lv_bar_create(scr);
  lv_obj_set_size(progressBar, 300, 12);
  lv_obj_align(progressBar, LV_ALIGN_TOP_LEFT, 10, 100);
  lv_obj_set_style_bg_color(progressBar, COL_CARD, LV_PART_MAIN);
  lv_obj_set_style_bg_color(progressBar, COL_ACCENT, LV_PART_INDICATOR);
  lv_bar_set_value(progressBar, 0, LV_ANIM_OFF);

  // Temps + speed
  nozzleLabel = lv_label_create(scr);
  lv_obj_set_style_text_font(nozzleLabel, &lv_font_montserrat_16, 0);
  lv_obj_align(nozzleLabel, LV_ALIGN_TOP_LEFT, 10, 128);
  lv_label_set_text(nozzleLabel, "Noz --/--");

  bedLabel = lv_label_create(scr);
  lv_obj_set_style_text_font(bedLabel, &lv_font_montserrat_16, 0);
  lv_obj_align(bedLabel, LV_ALIGN_TOP_MID, 10, 128);
  lv_label_set_text(bedLabel, "Bed --/--");

  speedLabel = lv_label_create(scr);
  lv_obj_set_style_text_color(speedLabel, COL_MUTED, 0);
  lv_obj_align(speedLabel, LV_ALIGN_TOP_RIGHT, -10, 130);
  lv_label_set_text(speedLabel, "");

  // Button row
  lv_obj_t* row = lv_obj_create(scr);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, 320, 56);
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -8);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  pauseBtn = makeButton(row, LV_SYMBOL_PAUSE, onPauseClicked, COL_ACCENT, &pauseBtnLabel);
  stopBtn  = makeButton(row, LV_SYMBOL_STOP, onStopClicked, COL_DANGER);
  lightBtn = makeButton(row, LV_SYMBOL_EYE_OPEN, onLightClicked, COL_CARD, &lightBtnLabel);
  speedBtn = makeButton(row, LV_SYMBOL_CHARGE, onSpeedClicked, COL_CARD);
}

static void formatRemaining(char* buf, size_t n, int minutes) {
  if (minutes <= 0) { buf[0] = 0; return; }
  if (minutes >= 60) snprintf(buf, n, LV_SYMBOL_LOOP " %dh %02dm", minutes / 60, minutes % 60);
  else snprintf(buf, n, LV_SYMBOL_LOOP " %dm", minutes);
}

void uiUpdate() {
  if (!printerState.dirty) return;
  printerState.dirty = false;
  PrinterState& s = printerState;

  // Connection / printer state line
  switch (s.conn) {
    case BambuConn::NO_WIFI:
    case BambuConn::WIFI_CONNECTING:
      lv_label_set_text(statusLabel, "WiFi connecting...");
      lv_obj_set_style_text_color(statusLabel, COL_MUTED, 0);
      break;
    case BambuConn::MQTT_CONNECTING:
      lv_label_set_text(statusLabel, "Finding printer...");
      lv_obj_set_style_text_color(statusLabel, COL_MUTED, 0);
      break;
    case BambuConn::CONNECTED:
      if (isState("RUNNING")) {
        lv_label_set_text(statusLabel, "Printing");
        lv_obj_set_style_text_color(statusLabel, COL_ACCENT, 0);
      } else if (isState("PAUSE")) {
        lv_label_set_text(statusLabel, "Paused");
        lv_obj_set_style_text_color(statusLabel, COL_WARN, 0);
      } else if (isState("FINISH")) {
        lv_label_set_text(statusLabel, "Finished");
        lv_obj_set_style_text_color(statusLabel, COL_ACCENT, 0);
      } else if (isState("FAILED")) {
        lv_label_set_text(statusLabel, "Failed");
        lv_obj_set_style_text_color(statusLabel, COL_DANGER, 0);
      } else if (isState("PREPARE") || isState("SLICING")) {
        lv_label_set_text(statusLabel, "Preparing");
        lv_obj_set_style_text_color(statusLabel, COL_WARN, 0);
      } else {
        lv_label_set_text(statusLabel, "Idle");
        lv_obj_set_style_text_color(statusLabel, COL_TEXT, 0);
      }
      break;
  }

  lv_obj_set_style_text_color(wifiLabel,
      WiFi.status() == WL_CONNECTED ? COL_ACCENT : COL_MUTED, 0);

  bool active = isPrinting() || isState("PAUSE");

  lv_label_set_text(taskLabel, active || isState("FINISH") ? s.taskName : "");

  if (active || isState("FINISH")) {
    lv_label_set_text_fmt(percentLabel, "%d%%", s.percent);
    if (s.totalLayers > 0)
      lv_label_set_text_fmt(layerLabel, "Layer %d/%d", s.layer, s.totalLayers);
    else
      lv_label_set_text(layerLabel, "");
    char tbuf[24];
    formatRemaining(tbuf, sizeof(tbuf), active ? s.remainingMin : 0);
    lv_label_set_text(timeLabel, tbuf);
    lv_bar_set_value(progressBar, s.percent, LV_ANIM_OFF);
  } else {
    lv_label_set_text(percentLabel, "--%");
    lv_label_set_text(layerLabel, "");
    lv_label_set_text(timeLabel, "");
    lv_bar_set_value(progressBar, 0, LV_ANIM_OFF);
  }

  lv_label_set_text_fmt(nozzleLabel, "Noz %d/%d", (int)lroundf(s.nozzleTemp), (int)lroundf(s.nozzleTarget));
  lv_label_set_text_fmt(bedLabel, "Bed %d/%d", (int)lroundf(s.bedTemp), (int)lroundf(s.bedTarget));
  lv_label_set_text_fmt(speedLabel, LV_SYMBOL_CHARGE " %s", bambuSpeedName(s.speedLevel));

  // Pause button doubles as resume
  lv_label_set_text(pauseBtnLabel, isState("PAUSE") ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
  if (active) {
    lv_obj_clear_state(pauseBtn, LV_STATE_DISABLED);
    lv_obj_clear_state(stopBtn, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(pauseBtn, LV_STATE_DISABLED);
    lv_obj_add_state(stopBtn, LV_STATE_DISABLED);
  }

  lv_obj_set_style_bg_color(lightBtn, s.chamberLight ? COL_WARN : COL_CARD, 0);
}
