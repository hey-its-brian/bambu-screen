#include "ui.h"
#include "bambu.h"

#include <Arduino.h>
#include <WiFi.h>
#include <lvgl.h>

// ---------------------------------------------------------------------------
// DECK//OS visual language, ported from tab5-cyberdeck deck_theme.c
// (NETRUNNER accent: cyan + magenta). "dim" is lifted a few steps above the
// Tab5 value so small labels stay readable on the panel color.
// ---------------------------------------------------------------------------
#define COL_BG       lv_color_hex(0x07070D)
#define COL_GRID     lv_color_hex(0x16182A)
#define COL_PANEL    lv_color_hex(0x0D0F1C)
#define COL_PANEL_HI lv_color_hex(0x171A30)
#define COL_LINE     lv_color_hex(0x2A3050)
#define COL_TEXT     lv_color_hex(0xD8E1F0)
#define COL_DIM      lv_color_hex(0x8A96B8)
#define COL_ACCENT   lv_color_hex(0x00F0FF)
#define COL_ACCENT2  lv_color_hex(0xFF2A6D)
#define COL_WARN     lv_color_hex(0xF3E600)
#define COL_OK       lv_color_hex(0x39FF14)
#define COL_DANGER   lv_color_hex(0xFF3B3B)

// Fonts generated with lv_font_conv (see src/fonts/). Orbitron carries only
// digits, % and - at the hero sizes to keep flash use down. The text fonts
// fall back to Montserrat for LV_SYMBOL glyphs (set in the generated files).
LV_FONT_DECLARE(deck_orbitron_44);
LV_FONT_DECLARE(deck_orbitron_32);
LV_FONT_DECLARE(deck_orbitron_16);
LV_FONT_DECLARE(deck_mono_20);
LV_FONT_DECLARE(deck_mono_16);

// ---- widgets ----
static lv_obj_t* header;
static lv_obj_t* headerRule;
static lv_obj_t* wordmark[3];
static lv_obj_t* wifiLabel;
static lv_obj_t* stateChip;
static lv_obj_t* stateChipLabel;

static lv_obj_t* jobPanel;
static lv_obj_t* jobRule;
static lv_obj_t* percentLabel;
static lv_obj_t* pctSign;
static lv_obj_t* taskLabel;
static lv_obj_t* layerLabel;
static lv_obj_t* timeLabel;
static lv_obj_t* progressBar;

static lv_obj_t* thermPanel;
static lv_obj_t* nozzleName;
static lv_obj_t* nozzleValue;
static lv_obj_t* nozzleBar;
static lv_obj_t* bedName;
static lv_obj_t* bedValue;
static lv_obj_t* bedBar;

static lv_obj_t* pauseBtn;
static lv_obj_t* pauseBtnLabel;
static lv_obj_t* stopBtn;
static lv_obj_t* lightBtn;
static lv_obj_t* lightBtnLabel;
static lv_obj_t* speedBtn;
static lv_obj_t* speedBtnLabel;

static bool isState(const char* s) { return strcmp(printerState.gcodeState, s) == 0; }
static bool isPrinting() { return isState("RUNNING") || isState("PREPARE") || isState("SLICING"); }

// ---- button actions ----

static void onPauseClicked(lv_event_t*) {
  Serial.println("[ui] pause/resume tapped");
  if (isState("PAUSE")) bambuResume();
  else if (isPrinting()) bambuPause();
}

static void onStopConfirm(lv_event_t* e) {
  lv_obj_t* mbox = lv_event_get_current_target(e);
  if (lv_msgbox_get_active_btn(mbox) == 0) bambuStop();
  lv_msgbox_close(mbox);
}

static void onStopClicked(lv_event_t*) {
  Serial.println("[ui] stop tapped");
  if (!isPrinting() && !isState("PAUSE")) return;
  static const char* btns[] = {"Stop", "Cancel", ""};
  lv_obj_t* mbox = lv_msgbox_create(nullptr, "Stop print?", "This cancels the current job.", btns, false);
  lv_obj_set_style_bg_color(mbox, COL_PANEL, 0);
  lv_obj_set_style_border_color(mbox, COL_DANGER, 0);
  lv_obj_set_style_border_width(mbox, 1, 0);
  lv_obj_set_style_radius(mbox, 2, 0);
  lv_obj_set_style_text_color(mbox, COL_TEXT, 0);
  lv_obj_set_style_text_font(mbox, &deck_mono_16, 0);
  lv_obj_add_event_cb(mbox, onStopConfirm, LV_EVENT_VALUE_CHANGED, nullptr);
  lv_obj_center(mbox);
}

static void onLightClicked(lv_event_t*) {
  Serial.println("[ui] light tapped");
  bambuSetLight(!printerState.chamberLight);
  // Optimistic: the next report confirms the real state.
  printerState.chamberLight = !printerState.chamberLight;
  printerState.dirty = true;
}

static void onSpeedClicked(lv_event_t*) {
  Serial.println("[ui] speed tapped");
  int next = printerState.speedLevel % 4 + 1;
  bambuSetSpeed(next);
  printerState.speedLevel = next;
  printerState.dirty = true;
}

// ---- layout ----

// Pixel positions per panel. WIDE is the 480x320 board, NARROW squeezes the
// same screen onto 320x240 with the hero number one size down.
struct Layout {
  lv_coord_t pad;            // outer margin
  lv_coord_t headerH;        // header strip height
  lv_coord_t jobY, jobH;     // job panel
  lv_coord_t heroX, heroY;   // hero percent (inside job panel)
  lv_coord_t metaX;          // task / layer / time column (inside job panel)
  lv_coord_t taskY, layerY, timeY;
  lv_coord_t barY, barH;     // progress bar (inside job panel)
  lv_coord_t thermY, thermH; // thermals panel
  lv_coord_t btnW, btnH, rowH, rowBottom;
  const lv_font_t* hero;
  const lv_font_t* body;
  const lv_font_t* small;
};

static const Layout LAYOUT_WIDE = {
  10, 34,
  42, 132,
  14, 16,
  158,
  18, 50, 74,
  102, 12,
  184, 56,
  104, 48, 60, 8,
  &deck_orbitron_44, &deck_mono_20, &deck_mono_16,
};

static const Layout LAYOUT_NARROW = {
  6, 28,
  32, 100,
  8, 12,
  112,
  10, 36, 58,
  78, 10,
  138, 46,
  70, 42, 50, 6,
  &deck_orbitron_32, &deck_mono_16, &deck_mono_16,
};

static const Layout* L;

// Accent that follows the printer state, like body[data-state] on the web UI.
static lv_color_t stateColor() {
  if (printerState.conn != BambuConn::CONNECTED) return COL_DIM;
  if (isState("RUNNING")) return COL_ACCENT;
  if (isState("PAUSE") || isState("PREPARE") || isState("SLICING")) return COL_WARN;
  if (isState("FINISH")) return COL_OK;
  if (isState("FAILED")) return COL_DANGER;
  return COL_DIM;
}

static lv_obj_t* makePanel(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w, lv_coord_t h,
                           lv_obj_t** ruleOut) {
  lv_obj_t* p = lv_obj_create(parent);
  lv_obj_remove_style_all(p);
  lv_obj_set_pos(p, x, y);
  lv_obj_set_size(p, w, h);
  lv_obj_set_style_bg_color(p, COL_PANEL, 0);
  lv_obj_set_style_bg_opa(p, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(p, COL_LINE, 0);
  lv_obj_set_style_border_width(p, 1, 0);
  lv_obj_set_style_radius(p, 2, 0);
  lv_obj_clear_flag(p, LV_OBJ_FLAG_SCROLLABLE);

  // 2px accent rule along the top edge, inset past the chamfer on the web
  lv_obj_t* rule = lv_obj_create(p);
  lv_obj_remove_style_all(rule);
  lv_obj_set_pos(rule, 10, 0);
  lv_obj_set_size(rule, w - 12, 2);
  lv_obj_set_style_bg_color(rule, COL_ACCENT, 0);
  lv_obj_set_style_bg_opa(rule, LV_OPA_COVER, 0);
  if (ruleOut) *ruleOut = rule;
  return p;
}

static lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, lv_color_t color,
                           lv_coord_t x, lv_coord_t y, const char* text) {
  lv_obj_t* l = lv_label_create(parent);
  lv_obj_set_style_text_font(l, font, 0);
  lv_obj_set_style_text_color(l, color, 0);
  lv_obj_set_pos(l, x, y);
  lv_label_set_text(l, text);
  return l;
}

static lv_obj_t* makeMeter(lv_obj_t* parent, lv_coord_t x, lv_coord_t y, lv_coord_t w) {
  lv_obj_t* bar = lv_bar_create(parent);
  lv_obj_set_pos(bar, x, y);
  lv_obj_set_size(bar, w, 6);
  lv_obj_set_style_radius(bar, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(bar, 0, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(bar, COL_PANEL_HI, LV_PART_MAIN);
  lv_obj_set_style_border_color(bar, COL_LINE, LV_PART_MAIN);
  lv_obj_set_style_border_width(bar, 1, LV_PART_MAIN);
  lv_obj_set_style_bg_color(bar, COL_ACCENT, LV_PART_INDICATOR);
  lv_bar_set_range(bar, 0, 100);
  lv_bar_set_value(bar, 0, LV_ANIM_OFF);
  return bar;
}

static lv_obj_t* makeButton(lv_obj_t* parent, const char* text, const lv_font_t* font,
                            lv_event_cb_t cb, lv_color_t edge, lv_obj_t** labelOut = nullptr) {
  lv_obj_t* btn = lv_btn_create(parent);
  lv_obj_remove_style_all(btn);
  lv_obj_set_size(btn, L->btnW, L->btnH);
  lv_obj_set_style_bg_color(btn, COL_PANEL_HI, 0);
  lv_obj_set_style_bg_opa(btn, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(btn, edge, 0);
  lv_obj_set_style_border_width(btn, 1, 0);
  lv_obj_set_style_radius(btn, 2, 0);
  lv_obj_set_style_text_color(btn, edge, 0);
  // pressed: fill with the edge color, dark glyph
  lv_obj_set_style_bg_color(btn, edge, LV_STATE_PRESSED);
  lv_obj_set_style_text_color(btn, COL_BG, LV_STATE_PRESSED);
  // disabled: everything drops to the line color
  lv_obj_set_style_border_color(btn, COL_LINE, LV_STATE_DISABLED);
  lv_obj_set_style_text_color(btn, COL_LINE, LV_STATE_DISABLED);
  lv_obj_set_style_bg_color(btn, COL_PANEL, LV_STATE_DISABLED);
  lv_obj_add_flag(btn, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_t* label = lv_label_create(btn);
  lv_obj_set_style_text_font(label, font, 0);
  lv_label_set_text(label, text);
  lv_obj_center(label);
  if (labelOut) *labelOut = label;
  return btn;
}

void uiSetup() {
  const lv_coord_t W = lv_disp_get_hor_res(nullptr);
  const lv_coord_t H = lv_disp_get_ver_res(nullptr);
  L = (H >= 300) ? &LAYOUT_WIDE : &LAYOUT_NARROW;
  Serial.printf("[ui] %dx%d, %s layout\n", W, H, L == &LAYOUT_WIDE ? "wide" : "narrow");

  lv_obj_t* scr = lv_scr_act();
  lv_obj_set_style_bg_color(scr, COL_BG, 0);
  lv_obj_set_style_text_color(scr, COL_TEXT, 0);
  lv_obj_set_style_text_font(scr, L->body, 0);
  lv_obj_clear_flag(scr, LV_OBJ_FLAG_SCROLLABLE);

  const lv_coord_t innerW = W - 2 * L->pad;

  // ---- header strip: wordmark left, wifi + state chip right ----
  header = lv_obj_create(scr);
  lv_obj_remove_style_all(header);
  lv_obj_set_pos(header, 0, 0);
  lv_obj_set_size(header, W, L->headerH);
  lv_obj_set_style_bg_color(header, COL_PANEL, 0);
  lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(header, COL_LINE, 0);
  lv_obj_set_style_border_width(header, 1, 0);
  lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);

  static const char* marks[3] = {"P1S", "//", "MONITOR"};
  lv_coord_t mx = L->pad + 2;
  for (int i = 0; i < 3; i++) {
    wordmark[i] = makeLabel(header, &deck_orbitron_16, i == 1 ? COL_ACCENT2 : COL_TEXT, mx, 0, marks[i]);
    lv_obj_align(wordmark[i], LV_ALIGN_LEFT_MID, mx, 0);
    lv_obj_update_layout(wordmark[i]);
    mx += lv_obj_get_width(wordmark[i]) + 7;
  }

  // State chip: outlined, text + border in the state color
  stateChip = lv_obj_create(header);
  lv_obj_remove_style_all(stateChip);
  lv_obj_set_style_bg_color(stateChip, COL_PANEL_HI, 0);
  lv_obj_set_style_bg_opa(stateChip, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(stateChip, 1, 0);
  lv_obj_set_style_radius(stateChip, 2, 0);
  lv_obj_set_style_pad_hor(stateChip, 8, 0);
  lv_obj_set_style_pad_ver(stateChip, 3, 0);
  lv_obj_set_size(stateChip, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_clear_flag(stateChip, LV_OBJ_FLAG_SCROLLABLE);
  stateChipLabel = makeLabel(stateChip, L->small, COL_DIM, 0, 0, "STARTING");
  lv_obj_align(stateChip, LV_ALIGN_RIGHT_MID, -L->pad, 0);

  wifiLabel = makeLabel(header, &lv_font_montserrat_16, COL_DIM, 0, 0, LV_SYMBOL_WIFI);
  lv_obj_update_layout(stateChip);
  lv_obj_align_to(wifiLabel, stateChip, LV_ALIGN_OUT_LEFT_MID, -10, 0);

  // accent rule under the header, fading out to the right
  headerRule = lv_obj_create(scr);
  lv_obj_remove_style_all(headerRule);
  lv_obj_set_pos(headerRule, 0, L->headerH - 1);
  lv_obj_set_size(headerRule, W, 2);
  lv_obj_set_style_bg_opa(headerRule, LV_OPA_COVER, 0);
  lv_obj_set_style_bg_grad_dir(headerRule, LV_GRAD_DIR_HOR, 0);
  lv_obj_set_style_bg_grad_color(headerRule, COL_BG, 0);
  lv_obj_set_style_bg_main_stop(headerRule, 90, 0);
  lv_obj_set_style_bg_grad_stop(headerRule, 255, 0);

  // ---- job panel: hero percent, task, layer, remaining, progress ----
  jobPanel = makePanel(scr, L->pad, L->jobY, innerW, L->jobH, &jobRule);

  percentLabel = makeLabel(jobPanel, L->hero, COL_ACCENT, L->heroX, L->heroY, "0");
  pctSign = makeLabel(jobPanel, &deck_orbitron_16, COL_DIM, 0, 0, "%");

  taskLabel = makeLabel(jobPanel, L->body, COL_TEXT, L->metaX, L->taskY, "No job loaded");
  lv_obj_set_width(taskLabel, innerW - L->metaX - 12);
  lv_label_set_long_mode(taskLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);

  layerLabel = makeLabel(jobPanel, L->small, COL_TEXT, L->metaX, L->layerY, "");
  lv_label_set_recolor(layerLabel, true);
  timeLabel = makeLabel(jobPanel, L->small, COL_TEXT, L->metaX, L->timeY, "");
  lv_label_set_recolor(timeLabel, true);

  progressBar = lv_bar_create(jobPanel);
  lv_obj_set_pos(progressBar, 12, L->barY);
  lv_obj_set_size(progressBar, innerW - 24, L->barH);
  lv_obj_set_style_radius(progressBar, 0, LV_PART_MAIN);
  lv_obj_set_style_radius(progressBar, 0, LV_PART_INDICATOR);
  lv_obj_set_style_bg_color(progressBar, COL_PANEL_HI, LV_PART_MAIN);
  lv_obj_set_style_border_color(progressBar, COL_LINE, LV_PART_MAIN);
  lv_obj_set_style_border_width(progressBar, 1, LV_PART_MAIN);
  lv_obj_set_style_bg_color(progressBar, COL_ACCENT, LV_PART_INDICATOR);
  lv_bar_set_value(progressBar, 0, LV_ANIM_OFF);

  // ---- thermals: two columns, name / value over a meter ----
  thermPanel = makePanel(scr, L->pad, L->thermY, innerW, L->thermH, nullptr);
  const lv_coord_t colW = (innerW - 36) / 2;
  const lv_coord_t c1 = 12, c2 = 12 + colW + 12;
  const lv_coord_t rowY = L == &LAYOUT_WIDE ? 10 : 7;
  const lv_coord_t meterY = L == &LAYOUT_WIDE ? 36 : 31;

  nozzleName = makeLabel(thermPanel, L->small, COL_DIM, c1, rowY, L == &LAYOUT_WIDE ? "Nozzle" : "Noz");
  nozzleValue = makeLabel(thermPanel, L->body, COL_TEXT, 0, 0, "--");
  lv_label_set_recolor(nozzleValue, true);
  lv_obj_align(nozzleValue, LV_ALIGN_TOP_LEFT, c1 + colW, rowY - 2);
  nozzleBar = makeMeter(thermPanel, c1, meterY, colW);

  bedName = makeLabel(thermPanel, L->small, COL_DIM, c2, rowY, "Bed");
  bedValue = makeLabel(thermPanel, L->body, COL_TEXT, 0, 0, "--");
  lv_label_set_recolor(bedValue, true);
  lv_obj_align(bedValue, LV_ALIGN_TOP_LEFT, c2 + colW, rowY - 2);
  bedBar = makeMeter(thermPanel, c2, meterY, colW);

  // ---- button row ----
  lv_obj_t* row = lv_obj_create(scr);
  lv_obj_remove_style_all(row);
  lv_obj_set_size(row, W, L->rowH);
  lv_obj_align(row, LV_ALIGN_BOTTOM_MID, 0, -L->rowBottom);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_EVENLY, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);

  pauseBtn = makeButton(row, LV_SYMBOL_PAUSE, &lv_font_montserrat_20, onPauseClicked, COL_ACCENT, &pauseBtnLabel);
  stopBtn  = makeButton(row, LV_SYMBOL_STOP, &lv_font_montserrat_20, onStopClicked, COL_DANGER);
  lightBtn = makeButton(row, LV_SYMBOL_EYE_OPEN, &lv_font_montserrat_20, onLightClicked, COL_LINE, &lightBtnLabel);
  speedBtn = makeButton(row, "STD", L->small, onSpeedClicked, COL_LINE, &speedBtnLabel);
  lv_obj_set_style_text_color(lightBtn, COL_TEXT, 0);
  lv_obj_set_style_text_color(speedBtn, COL_TEXT, 0);
}

static void formatRemaining(char* buf, size_t n, int minutes) {
  if (minutes <= 0) { buf[0] = 0; return; }
  if (minutes >= 60) snprintf(buf, n, "#8A96B8 Remaining# %dh %02dm", minutes / 60, minutes % 60);
  else snprintf(buf, n, "#8A96B8 Remaining# %dm", minutes);
}

// "actual / target" with the target dimmed; meter shows actual against a
// ceiling, green when on target, magenta when overshooting.
static void setTemp(lv_obj_t* value, lv_obj_t* bar, float actual, float target, int ceiling) {
  int a = (int)lroundf(actual), t = (int)lroundf(target);
  if (t > 0) lv_label_set_text_fmt(value, "%d#8A96B8  / %d#°", a, t);
  else       lv_label_set_text_fmt(value, "%d°", a);
  int pct = (int)(actual * 100.0f / ceiling);
  lv_bar_set_value(bar, constrain(pct, 0, 100), LV_ANIM_OFF);
  lv_color_t c = COL_ACCENT;
  if (t > 0) {
    if (abs(a - t) <= 2) c = COL_OK;
    else if (a > t + 2)  c = COL_ACCENT2;
  }
  lv_obj_set_style_bg_color(bar, c, LV_PART_INDICATOR);
}

static const char* speedShort(int level) {
  switch (level) {
    case 1: return "SILENT";
    case 2: return "STD";
    case 3: return "SPORT";
    case 4: return "LUDIC";
    default: return "?";
  }
}

void uiUpdate() {
  if (!printerState.dirty) return;
  printerState.dirty = false;
  PrinterState& s = printerState;

  // State chip text
  const char* chip = "IDLE";
  switch (s.conn) {
    case BambuConn::NO_WIFI:
    case BambuConn::WIFI_CONNECTING: chip = "WIFI..."; break;
    case BambuConn::MQTT_CONNECTING: chip = "FINDING PRINTER"; break;
    case BambuConn::CONNECTED:
      if (isState("RUNNING"))      chip = "PRINTING";
      else if (isState("PAUSE"))   chip = "PAUSED";
      else if (isState("FINISH"))  chip = "FINISHED";
      else if (isState("FAILED"))  chip = "FAILED";
      else if (isState("PREPARE") || isState("SLICING")) chip = "PREPARING";
      else                         chip = "IDLE";
      break;
  }
  lv_label_set_text(stateChipLabel, chip);

  // Everything tinted by state follows one color
  lv_color_t sc = stateColor();
  lv_obj_set_style_text_color(stateChipLabel, sc, 0);
  lv_obj_set_style_border_color(stateChip, sc, 0);
  lv_obj_set_style_bg_color(stateChip, lv_color_mix(sc, COL_PANEL, 40), 0);
  lv_obj_set_style_bg_color(headerRule, sc, 0);
  lv_obj_set_style_bg_color(jobRule, sc, 0);
  lv_obj_set_style_text_color(percentLabel, sc, 0);
  lv_obj_set_style_bg_color(progressBar, sc, LV_PART_INDICATOR);

  // keep the chip right-aligned as its text changes, wifi icon beside it
  lv_obj_update_layout(stateChip);
  lv_obj_align(stateChip, LV_ALIGN_RIGHT_MID, -L->pad, 0);
  lv_obj_align_to(wifiLabel, stateChip, LV_ALIGN_OUT_LEFT_MID, -10, 0);
  lv_obj_set_style_text_color(wifiLabel,
      WiFi.status() == WL_CONNECTED ? COL_ACCENT : COL_DIM, 0);

  bool active = isPrinting() || isState("PAUSE");
  bool showJob = active || isState("FINISH");

  if (showJob) {
    lv_obj_clear_flag(percentLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_clear_flag(pctSign, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text_fmt(percentLabel, "%d", s.percent);
    lv_obj_update_layout(percentLabel);
    lv_obj_align_to(pctSign, percentLabel, LV_ALIGN_OUT_RIGHT_BOTTOM, 4, -4);

    lv_label_set_text(taskLabel, s.taskName[0] ? s.taskName : "Untitled job");
    lv_obj_set_pos(taskLabel, L->metaX, L->taskY);
    if (s.totalLayers > 0)
      lv_label_set_text_fmt(layerLabel, "#8A96B8 Layer# %d / %d", s.layer, s.totalLayers);
    else
      lv_label_set_text(layerLabel, "");
    char tbuf[40];
    formatRemaining(tbuf, sizeof(tbuf), active ? s.remainingMin : 0);
    lv_label_set_text(timeLabel, tbuf);
    lv_bar_set_value(progressBar, s.percent, LV_ANIM_OFF);
  } else {
    // Standby: no hero number, the chip carries the state
    lv_obj_add_flag(percentLabel, LV_OBJ_FLAG_HIDDEN);
    lv_obj_add_flag(pctSign, LV_OBJ_FLAG_HIDDEN);
    lv_label_set_text(taskLabel, s.conn == BambuConn::CONNECTED ? "No job loaded" : "Waiting for printer");
    lv_obj_set_pos(taskLabel, L->heroX + 2, (L->barY - lv_font_get_line_height(L->body)) / 2);
    lv_label_set_text(layerLabel, "");
    lv_label_set_text(timeLabel, "");
    lv_bar_set_value(progressBar, 0, LV_ANIM_OFF);
  }

  setTemp(nozzleValue, nozzleBar, s.nozzleTemp, s.nozzleTarget, 300);
  setTemp(bedValue, bedBar, s.bedTemp, s.bedTarget, 110);
  // right-align the values inside their columns
  lv_obj_update_layout(nozzleValue);
  lv_obj_align_to(nozzleValue, nozzleBar, LV_ALIGN_OUT_TOP_RIGHT, 0, -4);
  lv_obj_update_layout(bedValue);
  lv_obj_align_to(bedValue, bedBar, LV_ALIGN_OUT_TOP_RIGHT, 0, -4);

  // Pause button doubles as resume
  lv_label_set_text(pauseBtnLabel, isState("PAUSE") ? LV_SYMBOL_PLAY : LV_SYMBOL_PAUSE);
  if (active) {
    lv_obj_clear_state(pauseBtn, LV_STATE_DISABLED);
    lv_obj_clear_state(stopBtn, LV_STATE_DISABLED);
  } else {
    lv_obj_add_state(pauseBtn, LV_STATE_DISABLED);
    lv_obj_add_state(stopBtn, LV_STATE_DISABLED);
  }

  // Light button lights up in warn yellow when the chamber light is on
  lv_obj_set_style_border_color(lightBtn, s.chamberLight ? COL_WARN : COL_LINE, 0);
  lv_obj_set_style_text_color(lightBtn, s.chamberLight ? COL_WARN : COL_TEXT, 0);
  lv_obj_set_style_bg_color(lightBtn, s.chamberLight ? lv_color_mix(COL_WARN, COL_PANEL_HI, 40) : COL_PANEL_HI, 0);

  lv_label_set_text(speedBtnLabel, speedShort(s.speedLevel));
}
