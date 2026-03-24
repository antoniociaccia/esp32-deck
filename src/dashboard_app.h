#ifndef DASHBOARD_APP_H
#define DASHBOARD_APP_H

#include <Arduino.h>
#include "display.h"

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

static constexpr lv_coord_t HEADER_W = 314;
static constexpr lv_coord_t HEADER_H = 30;
static constexpr lv_coord_t MAIN_W = 314;
static constexpr lv_coord_t MAIN_H = 136;
static constexpr lv_coord_t FOOTER_W = 314;
static constexpr lv_coord_t FOOTER_H = 40;
static constexpr lv_coord_t PANEL_RADIUS = 4;

static constexpr int BAT_PIN = 9;
static constexpr float BATTERY_DIVIDER_RATIO = 2.0f;
static constexpr float BATTERY_CALIBRATION_FACTOR = 1.00f;
static constexpr int BATTERY_SAMPLE_COUNT = 12;
static constexpr float BATTERY_FILTER_ALPHA = 0.15f;
static constexpr float BATTERY_MAX_STEP_VOLTS = 0.20f;
static constexpr float BATTERY_EMPTY_VOLTS = 3.30f;
static constexpr float BATTERY_FULL_VOLTS = 4.20f;
static constexpr float BATTERY_PRESENT_MIN_VOLTS = 3.20f;
static constexpr float BATTERY_PRESENT_MAX_VOLTS = 4.35f;
static constexpr int BATTERY_MAX_STABLE_SPREAD_MV = 180;

static constexpr unsigned long CLOCK_REFRESH_INTERVAL_MS = 1000;
static constexpr unsigned long BATTERY_REFRESH_INTERVAL_MS = 2000;
static constexpr unsigned long WEATHER_REFRESH_INTERVAL_MS = 1800000;
static constexpr unsigned long WEATHER_RETRY_INTERVAL_MS = 30000;
static constexpr unsigned long NEWS_REFRESH_INTERVAL_MS = 900000;
static constexpr unsigned long NEWS_RETRY_INTERVAL_MS = 60000;
static constexpr unsigned long WIFI_RECONNECT_INTERVAL_MS = 10000;
static constexpr unsigned long NTP_RETRY_INTERVAL_MS = 15000;
static constexpr unsigned long SAFE_HEARTBEAT_INTERVAL_MS = 2000;

static constexpr bool SAFE_BOOT_RECOVERY = false;

static constexpr uint32_t COLOR_HEADER_BG = 0x18344A;
static constexpr uint32_t COLOR_HEADER_BORDER = 0x28485E;
static constexpr uint32_t COLOR_MAIN_BG = 0xFFF8EF;
static constexpr uint32_t COLOR_MAIN_BORDER = 0xE7D8C7;
static constexpr uint32_t COLOR_SCREEN_BG = 0xD7E6F1;
static constexpr uint32_t COLOR_ACCENT = 0x8A3B12;
static constexpr uint32_t COLOR_TEXT_PRIMARY = 0x111827;
static constexpr uint32_t COLOR_TEXT_SECONDARY = 0x475569;
static constexpr uint32_t COLOR_TEXT_LIGHT = 0xF8FAFC;
static constexpr uint32_t COLOR_TEXT_INFO = 0xCFE8FF;
static constexpr uint32_t COLOR_TEXT_NEWS = 0xE2E8F0;
static constexpr uint32_t COLOR_TEXT_MUTED = 0x94A3B8;
static constexpr uint32_t COLOR_BATTERY_OK = 0xC7F9CC;
static constexpr uint32_t COLOR_WIFI_ONLINE = 0x86EFAC;
static constexpr uint32_t COLOR_DOT_IDLE = 0xC6B8AA;

static constexpr int DEFAULT_NEWS_ITEM_COUNT = 3;
static constexpr int MAX_NEWS_ITEMS = 12;
static constexpr int MAX_NEWS_TEXT_LEN = 256;
static constexpr int MAX_TICKER_LEN = 3600;

static constexpr int MODULE_COUNT = 4;

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
  lv_obj_t *moduleTiles[MODULE_COUNT] = {nullptr};
  lv_obj_t *moduleDots[MODULE_COUNT] = {nullptr};
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
  bool weatherValid = false;
  bool newsValid = false;
  int newsItemCount = 0;
  char newsItems[MAX_NEWS_ITEMS][MAX_NEWS_TEXT_LEN] = {};
  char newsTicker[MAX_TICKER_LEN] = {};
};

extern UiRefs ui;
extern AppState app;
extern const char *const DEFAULT_NEWS_ITEMS[DEFAULT_NEWS_ITEM_COUNT];
extern const ModuleContent MODULES[MODULE_COUNT];

lv_color_t colorFromHex(uint32_t hex);
bool intervalElapsed(unsigned long &lastRunMs, unsigned long intervalMs);

int extractJsonIntAfterKey(const String &payload, const char *key, int fallbackValue);
String extractJsonStringAfterKey(const String &payload, const char *key, const char *fallbackValue);
String decodeJsonString(const String &value);
void normalizeNewsText(String &text);
void rebuildNewsTicker();
void setDefaultNewsItems();
bool parseNewsItems(const String &payload);

#endif
