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
  DEBUG_BOOT_PRINTF("[boot] Free Heap: %u, Min Free: %u\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());

  app.currentModuleIndex = UI_DEFAULT_MODULE_INDEX;
  strlcpy(app.clock.labelText, "sync orario...", sizeof(app.clock.labelText));
  strlcpy(app.weather.labelText, "meteo n/d", sizeof(app.weather.labelText));
  app.weather.iconCode[0] = '\0';
  app.weather.state = SERVICE_FETCH_IDLE;
  app.weather.lastHttpCode = 0;
  app.news.state = SERVICE_FETCH_IDLE;
  app.news.lastHttpCode = 0;
  app.ota.state = SERVICE_FETCH_IDLE;
  app.ota.lastHttpCode = 0;
  app.ota.eligibility = OTA_ELIGIBILITY_INVALID;
  app.ota.remoteBinUrl[0] = '\0';
  app.ota.applyRequested = false;
  app.ota.applyState = OTA_APPLY_IDLE;
  app.ota.applyProgressPercent = -1;
  app.ota.applyBytesCurrent = 0;
  app.ota.applyBytesTotal = 0;
  app.ota.applyLastErrorCode = 0;
  app.ota.applyStatusText[0] = '\0';
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

static unsigned long lastHeapLogMs = 0;

void runDashboardLoop(Display &screen) {
  screen.routine();
  updateUiDirtyStateFromConnectivity();
  maintainTimeSync();
  updateClockUi();
  updateBatteryUi();
  updateWeatherUi();
  updateNewsFeed();
  if (app.ota.applyRequested) {
    startOtaFirmwareUpdate();
  }
  updateOtaManifestCheck();
  refreshDashboardUi();
  
  if (intervalElapsed(lastHeapLogMs, TIMING_HEAP_LOG_MS)) {
    DEBUG_NETWORK_PRINTF("[memory] Free Heap: %u bytes, Min Free: %u bytes\n", ESP.getFreeHeap(), ESP.getMinFreeHeap());
  }

  delay(5);
}
