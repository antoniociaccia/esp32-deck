#ifndef DASHBOARD_APP_H
#define DASHBOARD_APP_H

#include <Arduino.h>
#include "lvgl.h"
#include "config_ui.h"
#include "config_network.h"
#include "dashboard_ota.h"
#include <atomic>

struct ModuleCardDef {
  const char *title;
  const char *value;
  const char *meta;
  uint32_t dirtyMask;
  void (*createCb)(lv_obj_t *tile, int moduleIndex);
  void (*updateCb)();
};

struct BatteryReading {
  float voltage;
  int spreadMilliVolts;
  bool present;
};

enum ServiceFetchState : uint8_t {
  SERVICE_FETCH_IDLE = 0,
  SERVICE_FETCH_FETCHING,
  SERVICE_FETCH_READY,
  SERVICE_FETCH_OFFLINE,
  SERVICE_FETCH_CONFIG_MISSING,
  SERVICE_FETCH_TRANSPORT_ERROR,
  SERVICE_FETCH_HTTP_ERROR,
  SERVICE_FETCH_INVALID_PAYLOAD
};

enum OtaApplyState : uint8_t {
  OTA_APPLY_IDLE = 0,
  OTA_APPLY_IN_PROGRESS,
  OTA_APPLY_SUCCESS,
  OTA_APPLY_FAILED
};

static constexpr uint32_t UI_DIRTY_NONE = 0;
static constexpr uint32_t UI_DIRTY_HEADER = 1UL << 0;
static constexpr uint32_t UI_DIRTY_FOOTER = 1UL << 1;
static constexpr uint32_t UI_DIRTY_MAIN_CLOCK = 1UL << 2;
static constexpr uint32_t UI_DIRTY_MAIN_POWER = 1UL << 3;
static constexpr uint32_t UI_DIRTY_MAIN_WEATHER = 1UL << 4;
static constexpr uint32_t UI_DIRTY_MAIN_NEWS = 1UL << 5;
static constexpr uint32_t UI_DIRTY_MAIN_TILE_STATE = 1UL << 6;
static constexpr uint32_t UI_DIRTY_MAIN_ALL =
  UI_DIRTY_MAIN_CLOCK |
  UI_DIRTY_MAIN_POWER |
  UI_DIRTY_MAIN_WEATHER |
  UI_DIRTY_MAIN_NEWS |
  UI_DIRTY_MAIN_TILE_STATE;
static constexpr uint32_t UI_DIRTY_ALL = UI_DIRTY_HEADER | UI_DIRTY_FOOTER | UI_DIRTY_MAIN_ALL;

struct UiRefs {
  lv_obj_t *timeLabel = nullptr;
  lv_obj_t *weatherIcon = nullptr;
  lv_obj_t *weatherLabel = nullptr;
  lv_obj_t *wifiLabel = nullptr;
  lv_obj_t *headerTouchOverlay = nullptr;
  lv_obj_t *otaButton = nullptr;
  lv_obj_t *otaLabel = nullptr;
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
  lv_obj_t *btLabel = nullptr;
  lv_obj_t *otaPopupOverlay = nullptr;
  lv_obj_t *otaPopupBodyLabel = nullptr;
  lv_obj_t *otaPopupProgressBar = nullptr;
  lv_obj_t *otaPopupActionButton = nullptr;
  lv_obj_t *otaPopupActionLabel = nullptr;
};

struct ClockState {
  unsigned long lastUpdateMs = 0;
  unsigned long lastSyncAttemptMs = 0;
  bool synced = false;
  char labelText[24] = {};
};

struct BatteryState {
  unsigned long lastUpdateMs = 0;
  float filteredVoltage = 0.0f;
  bool initialized = false;
  bool present = false;
  bool usbPowered = false;
  int percent = -1;
  float voltage = 0.0f;
};

struct WeatherState {
  unsigned long lastUpdateMs = 0;
  bool valid = false;
  ServiceFetchState state = SERVICE_FETCH_IDLE;
  int lastHttpCode = 0;
  int temperatureC = 0;
  char labelText[32] = {};
  char iconCode[8] = {};
};

struct NewsState {
  unsigned long lastFetchMs = 0;
  bool valid = false;
  ServiceFetchState state = SERVICE_FETCH_IDLE;
  int lastHttpCode = 0;
  int itemCount = 0;
  char items[NEWS_MAX_ITEMS][NEWS_MAX_TEXT_LEN] = {};
  char ticker[NEWS_MAX_TICKER_LEN] = {};
};

struct OtaState {
  unsigned long lastCheckMs = 0;
  bool valid = false;
  ServiceFetchState state = SERVICE_FETCH_IDLE;
  int lastHttpCode = 0;
  OtaEligibility eligibility = OTA_ELIGIBILITY_INVALID;
  uint32_t remoteSizeBytes = 0;
  int minBatteryPercent = 0;
  char remoteVersion[OTA_VERSION_MAX_LEN] = {};
  char remoteBuild[OTA_BUILD_MAX_LEN] = {};
  char remoteBinUrl[OTA_BIN_URL_MAX_LEN] = {};
  char remoteSha256[OTA_SHA256_MAX_LEN] = {};
  bool applyRequested = false;
  OtaApplyState applyState = OTA_APPLY_IDLE;
  int applyProgressPercent = -1;
  uint32_t applyBytesCurrent = 0;
  uint32_t applyBytesTotal = 0;
  int applyLastErrorCode = 0;
  char applyStatusText[96] = {};
};

struct SettingsState {
  bool energyScheduleEnabled = false;
  uint8_t activeStartHour = 7;
  uint8_t activeStartMinute = 0;
  uint8_t activeEndHour = 23;
  uint8_t activeEndMinute = 0;
  bool tapWakeEnabled = true;
  uint8_t inactivityTimeoutMinutes = 5;
  bool wifiEnabled = true;
  bool bluetoothEnabled = false;
  bool weatherEnabled = true;
  bool newsEnabled = true;
  uint8_t brightnessLevel = 100;
  // 0=solo display off, 1=display+Wi-Fi off, 2=deep sleep (futuro)
  uint8_t powerMode = 1;
};

struct EnergyState {
  bool displayOn = true;
  bool displayOffByInactivity = false;
  bool wifiDisabledByPolicy = false;
  bool manualWakeActive = false;
  unsigned long lastActivityMs = 0;
};

struct AppState {
  ClockState clock;
  BatteryState battery;
  WeatherState weather;
  NewsState news;
  OtaState ota;
  SettingsState settings;
  EnergyState energy;
  int currentModuleIndex = 0;
  unsigned long lastSafeHeartbeatMs = 0;
  std::atomic<uint32_t> uiDirtyMask{UI_DIRTY_ALL};
};

extern UiRefs ui;
extern AppState app;

inline void markUiDirty(uint32_t mask) {
  app.uiDirtyMask |= mask;
}

inline uint32_t consumeUiDirtyMask() {
  uint32_t mask = app.uiDirtyMask;
  app.uiDirtyMask = UI_DIRTY_NONE;
  return mask;
}

#endif
