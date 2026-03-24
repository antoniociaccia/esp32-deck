#ifndef DASHBOARD_APP_H
#define DASHBOARD_APP_H

#include <Arduino.h>
#include "lvgl.h"
#include "config_ui.h"
#include "config_network.h"

struct ModuleContent {
  const char *title;
  const char *value;
  const char *meta;
};

struct BatteryReading {
  float voltage;
  int spreadMilliVolts;
  bool present;
};

struct UiRefs {
  lv_obj_t *timeLabel = nullptr;
  lv_obj_t *weatherIcon = nullptr;
  lv_obj_t *weatherLabel = nullptr;
  lv_obj_t *wifiLabel = nullptr;
  lv_obj_t *batteryPercentLabel = nullptr;
  lv_obj_t *batteryVoltageLabel = nullptr;
  lv_obj_t *batteryIconLabel = nullptr;
  lv_obj_t *moduleDotsWrap = nullptr;
  lv_obj_t *newsLabel = nullptr;
  lv_obj_t *tileview = nullptr;
  lv_obj_t *moduleTiles[UI_MODULE_COUNT] = {nullptr};
  lv_obj_t *moduleDots[UI_MODULE_COUNT] = {nullptr};
  lv_obj_t *moduleTitleLabels[UI_MODULE_COUNT] = {nullptr};
  lv_obj_t *moduleIconRefs[UI_MODULE_COUNT] = {nullptr};
  lv_obj_t *moduleBadgeLabels[UI_MODULE_COUNT] = {nullptr};
  lv_obj_t *moduleValueLabels[UI_MODULE_COUNT] = {nullptr};
  lv_obj_t *moduleMetaLabels[UI_MODULE_COUNT] = {nullptr};
};

struct AppState {
  unsigned long lastClockUpdateMs = 0;
  unsigned long lastTimeSyncAttemptMs = 0;
  unsigned long lastBatteryUpdateMs = 0;
  unsigned long lastWeatherUpdateMs = 0;
  unsigned long lastNewsFetchMs = 0;
  unsigned long lastSafeHeartbeatMs = 0;
  int currentModuleIndex = 0;
  bool timeSynced = false;
  float filteredBatteryVoltage = 0.0f;
  bool batteryInitialized = false;
  bool batteryPresent = false;
  int batteryPercent = -1;
  float batteryVoltage = 0.0f;
  bool weatherValid = false;
  int weatherTemperatureC = 0;
  char clockLabelText[24] = {};
  char weatherLabelText[32] = {};
  char weatherIconCode[8] = {};
  bool newsValid = false;
  int newsItemCount = 0;
  char newsItems[NEWS_MAX_ITEMS][NEWS_MAX_TEXT_LEN] = {};
  char newsTicker[NEWS_MAX_TICKER_LEN] = {};
};

extern UiRefs ui;
extern AppState app;

#endif
