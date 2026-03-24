#include "dashboard_ui_header.h"
#include "dashboard_app.h"
#include "dashboard_support.h"
#include "dashboard_ui_shared.h"
#include <WiFi.h>
#include "weather_icons.h"

static void updateWifiUi() {
  if (ui.wifiLabel == nullptr) {
    return;
  }

  lv_label_set_text(ui.wifiLabel, LV_SYMBOL_WIFI);
  setDashboardLabelColor(ui.wifiLabel,
    WiFi.status() == WL_CONNECTED ? UI_COLOR_WIFI_ONLINE : UI_COLOR_TEXT_MUTED);
}

static void updateWeatherHeaderIcon(const String &iconCode) {
  if (ui.weatherIcon == nullptr) {
    return;
  }

  if (iconCode.startsWith("01")) {
    lv_img_set_src(ui.weatherIcon, &IMG_WEATHER_SUN);
  } else if (iconCode.startsWith("02") || iconCode.startsWith("03") || iconCode.startsWith("04")) {
    lv_img_set_src(ui.weatherIcon, &IMG_WEATHER_CLOUD);
  } else if (iconCode.startsWith("09") || iconCode.startsWith("10")) {
    lv_img_set_src(ui.weatherIcon, &IMG_WEATHER_RAIN);
  } else if (iconCode.startsWith("11")) {
    lv_img_set_src(ui.weatherIcon, &IMG_WEATHER_STORM);
  } else if (iconCode.startsWith("13")) {
    lv_img_set_src(ui.weatherIcon, &IMG_WEATHER_SNOW);
  } else if (iconCode.startsWith("50")) {
    lv_img_set_src(ui.weatherIcon, &IMG_WEATHER_FOG);
  } else {
    lv_img_set_src(ui.weatherIcon, &IMG_WEATHER_CLOUD);
  }
}

static void refreshClockHeaderUi() {
  if (ui.timeLabel == nullptr) {
    return;
  }

  const char *clockText = strlen(app.clockLabelText) > 0 ? app.clockLabelText : "sync orario...";
  lv_label_set_text(ui.timeLabel, clockText);
}

static void refreshWeatherHeaderUi() {
  if (ui.weatherLabel == nullptr) {
    return;
  }

  const char *weatherText = strlen(app.weatherLabelText) > 0 ? app.weatherLabelText : "meteo n/d";
  lv_label_set_text(ui.weatherLabel, weatherText);
  updateWeatherHeaderIcon(String(app.weatherIconCode));
}

static void refreshBatteryHeaderUi() {
  if (ui.batteryPercentLabel == nullptr || ui.batteryVoltageLabel == nullptr || ui.batteryIconLabel == nullptr) {
    return;
  }

  if (!app.batteryPresent || app.batteryPercent < 0) {
    lv_label_set_text(ui.batteryPercentLabel, "--");
    lv_label_set_text(ui.batteryVoltageLabel, "--.--V");
    lv_label_set_text(ui.batteryIconLabel, LV_SYMBOL_BATTERY_EMPTY);
    setDashboardLabelColor(ui.batteryPercentLabel, UI_COLOR_TEXT_MUTED);
    setDashboardLabelColor(ui.batteryVoltageLabel, UI_COLOR_TEXT_MUTED);
    setDashboardLabelColor(ui.batteryIconLabel, UI_COLOR_TEXT_MUTED);
    return;
  }

  char percentBuffer[12];
  snprintf(percentBuffer, sizeof(percentBuffer), "%d%%", app.batteryPercent);
  lv_label_set_text(ui.batteryPercentLabel, percentBuffer);

  char voltageBuffer[12];
  snprintf(voltageBuffer, sizeof(voltageBuffer), "%.2fV", app.batteryVoltage);
  lv_label_set_text(ui.batteryVoltageLabel, voltageBuffer);

  if (app.batteryPercent >= 85) {
    lv_label_set_text(ui.batteryIconLabel, LV_SYMBOL_BATTERY_FULL);
  } else if (app.batteryPercent >= 60) {
    lv_label_set_text(ui.batteryIconLabel, LV_SYMBOL_BATTERY_3);
  } else if (app.batteryPercent >= 35) {
    lv_label_set_text(ui.batteryIconLabel, LV_SYMBOL_BATTERY_2);
  } else if (app.batteryPercent >= 15) {
    lv_label_set_text(ui.batteryIconLabel, LV_SYMBOL_BATTERY_1);
  } else {
    lv_label_set_text(ui.batteryIconLabel, LV_SYMBOL_BATTERY_EMPTY);
  }

  setDashboardLabelColor(ui.batteryPercentLabel, UI_COLOR_BATTERY_OK);
  setDashboardLabelColor(ui.batteryVoltageLabel, UI_COLOR_BATTERY_OK);
  setDashboardLabelColor(ui.batteryIconLabel, UI_COLOR_BATTERY_OK);
}

void createDashboardHeader(lv_obj_t *parent) {
  lv_obj_t *header = lv_obj_create(parent);
  lv_obj_set_size(header, UI_HEADER_WIDTH, UI_HEADER_HEIGHT);
  lv_obj_set_style_pad_hor(header, 8, 0);
  lv_obj_set_style_pad_ver(header, 3, 0);
  lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  styleDashboardPanel(header, UI_COLOR_HEADER_BG, UI_COLOR_HEADER_BORDER);

  ui.timeLabel = lv_label_create(header);
  lv_label_set_text(ui.timeLabel, "sync orario...");
  setDashboardLabelFont(ui.timeLabel, &lv_font_montserrat_14);
  setDashboardLabelColor(ui.timeLabel, UI_COLOR_TEXT_LIGHT);
  lv_obj_align(ui.timeLabel, LV_ALIGN_LEFT_MID, 0, 0);

  lv_obj_t *weatherWrap = lv_obj_create(header);
  lv_obj_set_size(weatherWrap, 94, 18);
  lv_obj_align(weatherWrap, LV_ALIGN_CENTER, 0, 0);
  lv_obj_set_flex_flow(weatherWrap, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(weatherWrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(weatherWrap, 0, 0);
  lv_obj_set_style_pad_gap(weatherWrap, 4, 0);
  lv_obj_set_style_bg_opa(weatherWrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(weatherWrap, 0, 0);
  lv_obj_clear_flag(weatherWrap, LV_OBJ_FLAG_SCROLLABLE);

  ui.weatherIcon = lv_img_create(weatherWrap);
  lv_img_set_src(ui.weatherIcon, &IMG_WEATHER_SUN);
  lv_obj_clear_flag(ui.weatherIcon, LV_OBJ_FLAG_SCROLLABLE);

  ui.weatherLabel = lv_label_create(weatherWrap);
  lv_label_set_text(ui.weatherLabel, "meteo n/d");
  setDashboardLabelFont(ui.weatherLabel, &lv_font_montserrat_14);
  setDashboardLabelColor(ui.weatherLabel, UI_COLOR_TEXT_INFO);

  lv_obj_t *rightWrap = lv_obj_create(header);
  lv_obj_set_size(rightWrap, 96, 24);
  lv_obj_align(rightWrap, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_flex_flow(rightWrap, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(rightWrap, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(rightWrap, 0, 0);
  lv_obj_set_style_pad_gap(rightWrap, 4, 0);
  lv_obj_set_style_bg_opa(rightWrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(rightWrap, 0, 0);
  lv_obj_clear_flag(rightWrap, LV_OBJ_FLAG_SCROLLABLE);

  ui.wifiLabel = lv_label_create(rightWrap);
  lv_label_set_text(ui.wifiLabel, LV_SYMBOL_WIFI);
  setDashboardLabelFont(ui.wifiLabel, &lv_font_montserrat_12);
  setDashboardLabelColor(ui.wifiLabel, UI_COLOR_TEXT_MUTED);

  lv_obj_t *batteryTextWrap = lv_obj_create(rightWrap);
  lv_obj_set_size(batteryTextWrap, 34, 20);
  lv_obj_set_flex_flow(batteryTextWrap, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(batteryTextWrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(batteryTextWrap, 0, 0);
  lv_obj_set_style_pad_row(batteryTextWrap, 0, 0);
  lv_obj_set_style_bg_opa(batteryTextWrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(batteryTextWrap, 0, 0);
  lv_obj_clear_flag(batteryTextWrap, LV_OBJ_FLAG_SCROLLABLE);

  ui.batteryPercentLabel = lv_label_create(batteryTextWrap);
  lv_label_set_text(ui.batteryPercentLabel, "--%");
  setDashboardLabelFont(ui.batteryPercentLabel, &lv_font_montserrat_10);
  setDashboardLabelColor(ui.batteryPercentLabel, UI_COLOR_BATTERY_OK);
  lv_obj_set_width(ui.batteryPercentLabel, 34);
  lv_obj_set_style_text_align(ui.batteryPercentLabel, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

  ui.batteryVoltageLabel = lv_label_create(batteryTextWrap);
  lv_label_set_text(ui.batteryVoltageLabel, "--.--V");
  setDashboardLabelFont(ui.batteryVoltageLabel, &lv_font_montserrat_10);
  setDashboardLabelColor(ui.batteryVoltageLabel, UI_COLOR_BATTERY_OK);
  lv_obj_set_width(ui.batteryVoltageLabel, 34);
  lv_obj_set_style_text_align(ui.batteryVoltageLabel, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

  ui.batteryIconLabel = lv_label_create(rightWrap);
  lv_label_set_text(ui.batteryIconLabel, LV_SYMBOL_BATTERY_3);
  setDashboardLabelFont(ui.batteryIconLabel, &lv_font_montserrat_12);
  setDashboardLabelColor(ui.batteryIconLabel, UI_COLOR_BATTERY_OK);
}

void refreshDashboardHeaderUi() {
  updateWifiUi();
  refreshClockHeaderUi();
  refreshWeatherHeaderUi();
  refreshBatteryHeaderUi();
}
