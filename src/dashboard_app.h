#ifndef DASHBOARD_APP_H
#define DASHBOARD_APP_H

#include <Arduino.h>
#include "lvgl.h"
#include "config_ui.h"
#include "config_network.h"
#include "dashboard_ota.h"

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

enum ServiceFetchState : uint8_t {
  SERVICE_FETCH_IDLE = 0,
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
  lv_obj_t *otaPopupOverlay = nullptr;
  lv_obj_t *otaPopupBodyLabel = nullptr;
  lv_obj_t *otaPopupActionButton = nullptr;
  lv_obj_t *otaPopupActionLabel = nullptr;
};

struct AppState {
  unsigned long lastClockUpdateMs = 0;
  unsigned long lastTimeSyncAttemptMs = 0;
  unsigned long lastBatteryUpdateMs = 0;
  unsigned long lastWeatherUpdateMs = 0;
  unsigned long lastNewsFetchMs = 0;
  unsigned long lastOtaCheckMs = 0;
  unsigned long lastSafeHeartbeatMs = 0;
  int currentModuleIndex = 0;
  bool timeSynced = false;
  float filteredBatteryVoltage = 0.0f;
  bool batteryInitialized = false;
  bool batteryPresent = false;
  int batteryPercent = -1;
  float batteryVoltage = 0.0f;
  bool weatherValid = false;
  ServiceFetchState weatherState = SERVICE_FETCH_IDLE;
  int weatherLastHttpCode = 0;
  int weatherTemperatureC = 0;
  char clockLabelText[24] = {};
  char weatherLabelText[32] = {};
  char weatherIconCode[8] = {};
  bool newsValid = false;
  ServiceFetchState newsState = SERVICE_FETCH_IDLE;
  int newsLastHttpCode = 0;
  int newsItemCount = 0;
  char newsItems[NEWS_MAX_ITEMS][NEWS_MAX_TEXT_LEN] = {};
  char newsTicker[NEWS_MAX_TICKER_LEN] = {};
  bool otaManifestValid = false;
  ServiceFetchState otaState = SERVICE_FETCH_IDLE;
  int otaLastHttpCode = 0;
  OtaEligibility otaEligibility = OTA_ELIGIBILITY_INVALID;
  uint32_t otaRemoteSizeBytes = 0;
  int otaMinBatteryPercent = 0;
  char otaRemoteVersion[OTA_VERSION_MAX_LEN] = {};
  char otaRemoteBuild[OTA_BUILD_MAX_LEN] = {};
  char otaRemoteBinUrl[OTA_BIN_URL_MAX_LEN] = {};
  bool otaApplyRequested = false;
  OtaApplyState otaApplyState = OTA_APPLY_IDLE;
  int otaApplyProgressPercent = -1;
  uint32_t otaApplyBytesCurrent = 0;
  uint32_t otaApplyBytesTotal = 0;
  int otaApplyLastErrorCode = 0;
  char otaApplyStatusText[96] = {};
  uint32_t uiDirtyMask = UI_DIRTY_ALL;
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
