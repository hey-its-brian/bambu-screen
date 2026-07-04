#pragma once

// Builds the LVGL screen. Call once after displaySetup().
void uiSetup();

// Refreshes widgets from printerState when it is marked dirty.
void uiUpdate();
