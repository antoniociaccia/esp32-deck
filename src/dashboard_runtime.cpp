#include "dashboard_runtime.h"
#include "dashboard_app.h"
#include "dashboard_support.h"
#include "dashboard_battery.h"
#include "dashboard_services.h"
#include "dashboard_ui.h"
#include "config_debug.h"
#include "config_timing.h"
#include <WiFi.h>

static bool wifiStateInitialized = false;
static bool lastWifiConnected = false;

static void updateUiDirtyStateFromConnectivity() {
  bool wifiConnected = WiFi.status() == WL_CONNECTED;
  if (wifiStateInitialized && lastWifiConnected == wifiConnected) {
    return;
  }

  markUiDirty(UI_DIRTY_HEADER | UI_DIRTY_MAIN_CLOCK | UI_DIRTY_MAIN_WEATHER | UI_DIRTY_MAIN_NEWS);
  lastWifiConnected = wifiConnected;
  wifiStateInitialized = true;
}

void enterSafeBootRecovery(Display &screen) {
  screen.init();
  lv_obj_t *root = lv_scr_act();
  lv_obj_set_style_bg_color(root, colorFromHex(0x101418), 0);
  lv_obj_set_style_bg_opa(root, LV_OPA_COVER, 0);

  lv_obj_t *label = lv_label_create(root);
  lv_label_set_text(label, "SAFE LVGL");
  lv_obj_set_style_text_color(label, colorFromHex(0xFFFFFF), LV_PART_MAIN);
  lv_obj_set_style_text_font(label, &lv_font_montserrat_20, LV_PART_MAIN);
  lv_obj_center(label);

  DEBUG_SAFE_PRINT("[boot] safe mode active");
  DEBUG_SAFE_PRINT("[boot] screen.init ok");
}

void initializeDashboard(Display &screen) {
  screen.init();
  DEBUG_BOOT_PRINT("[boot] display init ok");

  app.currentModuleIndex = UI_DEFAULT_MODULE_INDEX;
  strlcpy(app.clockLabelText, "sync orario...", sizeof(app.clockLabelText));
  strlcpy(app.weatherLabelText, "meteo n/d", sizeof(app.weatherLabelText));
  app.weatherIconCode[0] = '\0';
  app.weatherState = SERVICE_FETCH_IDLE;
  app.weatherLastHttpCode = 0;
  app.newsState = SERVICE_FETCH_IDLE;
  app.newsLastHttpCode = 0;
  app.otaState = SERVICE_FETCH_IDLE;
  app.otaLastHttpCode = 0;
  app.otaEligibility = OTA_ELIGIBILITY_INVALID;
  app.otaRemoteBinUrl[0] = '\0';
  app.otaApplyRequested = false;
  app.otaApplyState = OTA_APPLY_IDLE;
  app.otaApplyProgressPercent = -1;
  app.otaApplyBytesCurrent = 0;
  app.otaApplyBytesTotal = 0;
  app.otaApplyLastErrorCode = 0;
  app.otaApplyStatusText[0] = '\0';
  setDefaultNewsItems();
  createDashboardUi();
  DEBUG_BOOT_PRINT("[boot] ui ok");

  initBatteryMonitoring();
  beginTimeSync();

  DEBUG_BOOT_PRINT("Desk dashboard LVGL");
  DEBUG_BOOT_PRINT("Setup done");
}

void runSafeModeLoop() {
  if (!intervalElapsed(app.lastSafeHeartbeatMs, TIMING_SAFE_HEARTBEAT_MS)) {
    delay(5);
    return;
  }

  DEBUG_SAFE_PRINTF("[safe] alive %lus\n", millis() / 1000UL);
  delay(5);
}

void runDashboardLoop(Display &screen) {
  screen.routine();
  updateUiDirtyStateFromConnectivity();
  maintainTimeSync();
  updateClockUi();
  updateBatteryUi();
  updateWeatherUi();
  updateNewsFeed();
  if (app.otaApplyRequested) {
    startOtaFirmwareUpdate();
  }
  updateOtaManifestCheck();
  refreshDashboardUi();
  delay(5);
}
