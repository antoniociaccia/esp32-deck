#include "display.h"

Display screen;
static lv_obj_t *titleLabel = nullptr;
static lv_obj_t *statusLabel = nullptr;
static lv_obj_t *touchLabel = nullptr;
static unsigned long lastUiUpdateMs = 0;

void updateTouchUi() {
  if (millis() - lastUiUpdateMs < 50) {
    return;
  }

  lastUiUpdateMs = millis();

  if (screen.isTouched()) {
    lv_label_set_text(statusLabel, "Touch: OK");

    char buffer[48];
    snprintf(buffer, sizeof(buffer), "x=%d y=%d", screen.touchX(), screen.touchY());
    lv_label_set_text(touchLabel, buffer);
  } else {
    lv_label_set_text(statusLabel, "Touch: attesa");
    lv_label_set_text(touchLabel, "tocca lo schermo");
  }
}

void setup() {
  Serial.begin(115200);

  screen.init();

  String text = "Hello ESP32-S3! ";
  text += String('V') + lv_version_major() + "." + lv_version_minor() + "." + lv_version_patch();

  Serial.println(text);
  Serial.println("Freenove LVGL baseline");

  titleLabel = lv_label_create(lv_scr_act());
  lv_label_set_text(titleLabel, text.c_str());
  lv_obj_align(titleLabel, LV_ALIGN_TOP_MID, 0, 80);

  statusLabel = lv_label_create(lv_scr_act());
  lv_label_set_text(statusLabel, "Touch: attesa");
  lv_obj_align(statusLabel, LV_ALIGN_CENTER, 0, 0);

  touchLabel = lv_label_create(lv_scr_act());
  lv_label_set_text(touchLabel, "tocca lo schermo");
  lv_obj_align(touchLabel, LV_ALIGN_BOTTOM_MID, 0, -80);

  Serial.println("Setup done");
}

void loop() {
  screen.routine();
  updateTouchUi();
  delay(5);
}
