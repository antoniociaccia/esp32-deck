#include "dashboard_runtime.h"
#include "dashboard_app.h"
#include "dashboard_battery.h"
#include "dashboard_services.h"
#include "dashboard_ui.h"

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

  Serial.println("[boot] safe mode active");
  Serial.println("[boot] screen.init ok");
}

void initializeDashboard(Display &screen) {
  screen.init();
  Serial.println("[boot] display init ok");

  setDefaultNewsItems();
  createDashboardUi();
  Serial.println("[boot] ui ok");

  initBatteryMonitoring();
  beginTimeSync();

  Serial.println("Desk dashboard LVGL");
  Serial.println("Setup done");
}

void runSafeModeLoop() {
  if (!intervalElapsed(app.lastSafeHeartbeatMs, SAFE_HEARTBEAT_INTERVAL_MS)) {
    delay(5);
    return;
  }

  Serial.printf("[safe] alive %lus\n", millis() / 1000UL);
  delay(5);
}

void runDashboardLoop(Display &screen) {
  screen.routine();
  maintainTimeSync();
  updateClockUi();
  updateBatteryUi();
  updateWeatherUi();
  updateNewsFeed();
  delay(5);
}
