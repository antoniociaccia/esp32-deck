#include "dashboard_ui_header.h"
#include "dashboard_app.h"
#include "dashboard_services.h"
#include "dashboard_support.h"
#include "dashboard_ui_shared.h"
#include "dashboard_ui_ota_popup.h"
#include "config_debug.h"
#include "version.h"
#include <WiFi.h>
#include "weather_icons.h"

static constexpr lv_coord_t UI_HEADER_TOUCH_OVERLAY_HEIGHT = 48;
static constexpr lv_coord_t UI_HEADER_OTA_TOUCH_ZONE_X1 = 214;
static constexpr lv_coord_t UI_HEADER_OTA_TOUCH_ZONE_X2 = 266;


static void consumeHeaderTouchEvent(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code == LV_EVENT_PRESSED
    || code == LV_EVENT_PRESSING
    || code == LV_EVENT_RELEASED
    || code == LV_EVENT_CLICKED
    || code == LV_EVENT_SHORT_CLICKED
    || code == LV_EVENT_GESTURE) {
    lv_event_stop_bubbling(e);
  }
}

static bool headerTouchHitsOtaZone(lv_event_t *e) {
  lv_indev_t *indev = lv_indev_get_act();
  if (indev == nullptr) {
    return false;
  }

  lv_point_t point;
  lv_indev_get_point(indev, &point);

  lv_area_t coords;
  lv_obj_get_coords(lv_event_get_target(e), &coords);
  lv_coord_t localX = point.x - coords.x1;
  return localX >= UI_HEADER_OTA_TOUCH_ZONE_X1 && localX <= UI_HEADER_OTA_TOUCH_ZONE_X2;
}

static void headerTouchOverlayEventCb(lv_event_t *e) {
  consumeHeaderTouchEvent(e);

  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED && code != LV_EVENT_SHORT_CLICKED) {
    return;
  }

  if (!headerTouchHitsOtaZone(e)) {
    return;
  }

  showOtaPopup();
}

static bool lastWifiOnline = false;
static bool wifiUiInitialized = false;
static char lastClockHeaderText[24] = {};
static char lastWeatherHeaderText[32] = {};
static char lastWeatherHeaderIconCode[8] = {};
static bool batteryHeaderInitialized = false;
static bool lastBatteryPresent = false;
static int lastBatteryPercent = -999;
static char lastBatteryVoltageText[12] = {};
static char lastWeatherHeaderCompactText[20] = {};
static bool otaHeaderInitialized = false;
static ServiceFetchState lastOtaState = SERVICE_FETCH_IDLE;
static OtaEligibility lastOtaEligibility = OTA_ELIGIBILITY_INVALID;
static char lastOtaVersion[OTA_VERSION_MAX_LEN] = {};
static int lastOtaHttpCode = 0;
static OtaApplyState lastOtaApplyState = OTA_APPLY_IDLE;
static int lastOtaApplyProgressPercent = -1;

static const char *otaHeaderSymbol() {
  if (app.ota.applyState == OTA_APPLY_IN_PROGRESS) {
    return LV_SYMBOL_REFRESH;
  }
  if (app.ota.applyState == OTA_APPLY_SUCCESS) {
    return LV_SYMBOL_OK;
  }
  if (app.ota.applyState == OTA_APPLY_FAILED) {
    return LV_SYMBOL_WARNING;
  }

  if (app.ota.state != SERVICE_FETCH_READY) {
    return LV_SYMBOL_DOWNLOAD;
  }

  switch (app.ota.eligibility) {
    case OTA_ELIGIBILITY_UPDATE_AVAILABLE:
      return LV_SYMBOL_DOWNLOAD;
    case OTA_ELIGIBILITY_UP_TO_DATE:
      return LV_SYMBOL_OK;
    case OTA_ELIGIBILITY_BATTERY_TOO_LOW:
    case OTA_ELIGIBILITY_BATTERY_UNKNOWN:
    case OTA_ELIGIBILITY_SLOT_TOO_SMALL:
    case OTA_ELIGIBILITY_INCOMPATIBLE_BOARD:
    case OTA_ELIGIBILITY_INCOMPATIBLE_CHANNEL:
      return LV_SYMBOL_WARNING;
    case OTA_ELIGIBILITY_INVALID:
    default:
      return LV_SYMBOL_REFRESH;
  }
}

static uint32_t otaHeaderColor() {
  if (app.ota.applyState == OTA_APPLY_IN_PROGRESS) {
    return UI_COLOR_ACCENT;
  }
  if (app.ota.applyState == OTA_APPLY_SUCCESS) {
    return UI_COLOR_WIFI_ONLINE;
  }
  if (app.ota.applyState == OTA_APPLY_FAILED) {
    return UI_COLOR_ACCENT;
  }

  if (app.ota.state != SERVICE_FETCH_READY) {
    switch (app.ota.state) {
      case SERVICE_FETCH_CONFIG_MISSING:
      case SERVICE_FETCH_HTTP_ERROR:
      case SERVICE_FETCH_INVALID_PAYLOAD:
        return UI_COLOR_ACCENT;
      case SERVICE_FETCH_OFFLINE:
      case SERVICE_FETCH_TRANSPORT_ERROR:
      case SERVICE_FETCH_IDLE:
      default:
        return UI_COLOR_TEXT_MUTED;
    }
  }

  switch (app.ota.eligibility) {
    case OTA_ELIGIBILITY_UPDATE_AVAILABLE:
      return UI_COLOR_ACCENT;
    case OTA_ELIGIBILITY_UP_TO_DATE:
      return UI_COLOR_WIFI_ONLINE;
    case OTA_ELIGIBILITY_BATTERY_TOO_LOW:
    case OTA_ELIGIBILITY_BATTERY_UNKNOWN:
    case OTA_ELIGIBILITY_SLOT_TOO_SMALL:
    case OTA_ELIGIBILITY_INCOMPATIBLE_BOARD:
    case OTA_ELIGIBILITY_INCOMPATIBLE_CHANNEL:
      return UI_COLOR_ACCENT;
    case OTA_ELIGIBILITY_INVALID:
    default:
      return UI_COLOR_TEXT_MUTED;
  }
}

static void otaHeaderEventCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_PRESSED && code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED && code != LV_EVENT_SHORT_CLICKED) {
    return;
  }

  showOtaPopup();
}

static void updateWifiUi() {
  if (ui.wifiLabel == nullptr) {
    return;
  }

  bool online = WiFi.status() == WL_CONNECTED;
  if (wifiUiInitialized && lastWifiOnline == online) {
    return;
  }

  setDashboardLabelTextIfChanged(ui.wifiLabel, LV_SYMBOL_WIFI);
  setDashboardLabelColor(ui.wifiLabel, online ? UI_COLOR_WIFI_ONLINE : UI_COLOR_TEXT_MUTED);
  lastWifiOnline = online;
  wifiUiInitialized = true;
}

static const char *compactWeatherHeaderText() {
  if (app.weather.state == SERVICE_FETCH_READY && app.weather.valid) {
    return app.weather.labelText;
  }

  switch (app.weather.state) {
    case SERVICE_FETCH_OFFLINE:
      return "meteo off";
    case SERVICE_FETCH_CONFIG_MISSING:
      return "meteo cfg";
    case SERVICE_FETCH_TRANSPORT_ERROR:
      return "meteo rete";
    case SERVICE_FETCH_HTTP_ERROR:
      return "meteo http";
    case SERVICE_FETCH_INVALID_PAYLOAD:
      return "meteo err";
    case SERVICE_FETCH_IDLE:
    default:
      return "meteo n/d";
  }
}

static void refreshOtaHeaderUi() {
  if (ui.otaButton == nullptr || ui.otaLabel == nullptr) {
    return;
  }

  bool otaVisible = DEBUG_OTA_BUTTON_ALWAYS_VISIBLE
    || (app.ota.state == SERVICE_FETCH_READY && app.ota.eligibility == OTA_ELIGIBILITY_UPDATE_AVAILABLE)
    || (app.ota.applyState != OTA_APPLY_IDLE);
  if (otaVisible) {
    lv_obj_clear_flag(ui.otaButton, LV_OBJ_FLAG_HIDDEN);
  } else {
    lv_obj_add_flag(ui.otaButton, LV_OBJ_FLAG_HIDDEN);
    destroyOtaPopup();
  }

  if (otaHeaderInitialized
    && lastOtaState == app.ota.state
    && lastOtaEligibility == app.ota.eligibility
    && lastOtaHttpCode == app.ota.lastHttpCode
    && lastOtaApplyState == app.ota.applyState
    && lastOtaApplyProgressPercent == app.ota.applyProgressPercent
    && strcmp(lastOtaVersion, app.ota.remoteVersion) == 0) {
    refreshOtaPopupUi();
    return;
  }

  setDashboardLabelTextIfChanged(ui.otaLabel, otaHeaderSymbol());
  setDashboardLabelColor(ui.otaLabel, otaHeaderColor());
  lastOtaState = app.ota.state;
  lastOtaEligibility = app.ota.eligibility;
  lastOtaHttpCode = app.ota.lastHttpCode;
  lastOtaApplyState = app.ota.applyState;
  lastOtaApplyProgressPercent = app.ota.applyProgressPercent;
  strlcpy(lastOtaVersion, app.ota.remoteVersion, sizeof(lastOtaVersion));
  otaHeaderInitialized = true;
  refreshOtaPopupUi();
}

static void updateWeatherHeaderIcon(const String &iconCode) {
  if (ui.weatherIcon == nullptr) {
    return;
  }

  if (iconCode.startsWith("01")) {
    setDashboardImageSourceIfChanged(ui.weatherIcon, &IMG_WEATHER_SUN);
  } else if (iconCode.startsWith("02") || iconCode.startsWith("03") || iconCode.startsWith("04")) {
    setDashboardImageSourceIfChanged(ui.weatherIcon, &IMG_WEATHER_CLOUD);
  } else if (iconCode.startsWith("09") || iconCode.startsWith("10")) {
    setDashboardImageSourceIfChanged(ui.weatherIcon, &IMG_WEATHER_RAIN);
  } else if (iconCode.startsWith("11")) {
    setDashboardImageSourceIfChanged(ui.weatherIcon, &IMG_WEATHER_STORM);
  } else if (iconCode.startsWith("13")) {
    setDashboardImageSourceIfChanged(ui.weatherIcon, &IMG_WEATHER_SNOW);
  } else if (iconCode.startsWith("50")) {
    setDashboardImageSourceIfChanged(ui.weatherIcon, &IMG_WEATHER_FOG);
  } else {
    setDashboardImageSourceIfChanged(ui.weatherIcon, &IMG_WEATHER_CLOUD);
  }
}

static void refreshClockHeaderUi() {
  if (ui.timeLabel == nullptr) {
    return;
  }

  const char *clockText = strlen(app.clock.labelText) > 0 ? app.clock.labelText : "sync orario...";
  if (strcmp(lastClockHeaderText, clockText) == 0) {
    return;
  }

  setDashboardLabelTextIfChanged(ui.timeLabel, clockText);
  strlcpy(lastClockHeaderText, clockText, sizeof(lastClockHeaderText));
}

static void refreshWeatherHeaderUi() {
  if (ui.weatherLabel == nullptr) {
    return;
  }

  const char *weatherText = compactWeatherHeaderText();
  if (strcmp(lastWeatherHeaderCompactText, weatherText) != 0) {
    setDashboardLabelTextIfChanged(ui.weatherLabel, weatherText);
    strlcpy(lastWeatherHeaderCompactText, weatherText, sizeof(lastWeatherHeaderCompactText));
  }

  if (strcmp(lastWeatherHeaderIconCode, app.weather.iconCode) == 0) {
    return;
  }

  updateWeatherHeaderIcon(String(app.weather.iconCode));
  strlcpy(lastWeatherHeaderIconCode, app.weather.iconCode, sizeof(lastWeatherHeaderIconCode));
}

static void refreshBatteryHeaderUi() {
  if (ui.batteryPercentLabel == nullptr || ui.batteryVoltageLabel == nullptr || ui.batteryIconLabel == nullptr) {
    return;
  }

  char percentBuffer[12];
  char voltageBuffer[12];
  const char *iconText = LV_SYMBOL_BATTERY_EMPTY;
  uint32_t textColor = UI_COLOR_TEXT_MUTED;

  if (!app.battery.present || app.battery.percent < 0) {
    strlcpy(percentBuffer, "--", sizeof(percentBuffer));
    strlcpy(voltageBuffer, "--.--V", sizeof(voltageBuffer));
  } else {
    snprintf(percentBuffer, sizeof(percentBuffer), "%d%%", app.battery.percent);
    snprintf(voltageBuffer, sizeof(voltageBuffer), "%.2fV", app.battery.voltage);

    if (app.battery.percent >= 85) {
      iconText = LV_SYMBOL_BATTERY_FULL;
    } else if (app.battery.percent >= 60) {
      iconText = LV_SYMBOL_BATTERY_3;
    } else if (app.battery.percent >= 35) {
      iconText = LV_SYMBOL_BATTERY_2;
    } else if (app.battery.percent >= 15) {
      iconText = LV_SYMBOL_BATTERY_1;
    }

    textColor = UI_COLOR_BATTERY_OK;
  }

  if (batteryHeaderInitialized
    && lastBatteryPresent == app.battery.present
    && lastBatteryPercent == app.battery.percent
    && strcmp(lastBatteryVoltageText, voltageBuffer) == 0) {
    return;
  }

  setDashboardLabelTextIfChanged(ui.batteryPercentLabel, percentBuffer);
  setDashboardLabelTextIfChanged(ui.batteryVoltageLabel, voltageBuffer);
  setDashboardLabelTextIfChanged(ui.batteryIconLabel, iconText);
  setDashboardLabelColor(ui.batteryPercentLabel, textColor);
  setDashboardLabelColor(ui.batteryVoltageLabel, textColor);
  setDashboardLabelColor(ui.batteryIconLabel, textColor);

  lastBatteryPresent = app.battery.present;
  lastBatteryPercent = app.battery.percent;
  strlcpy(lastBatteryVoltageText, voltageBuffer, sizeof(lastBatteryVoltageText));
  batteryHeaderInitialized = true;
}

void createDashboardHeader(lv_obj_t *parent) {
  lv_obj_t *header = lv_obj_create(parent);
  lv_obj_set_size(header, UI_HEADER_WIDTH, UI_HEADER_HEIGHT);
  lv_obj_set_style_pad_left(header, 8, 0);
  lv_obj_set_style_pad_right(header, 6, 0);
  lv_obj_set_style_pad_top(header, 3, 0);
  lv_obj_set_style_pad_bottom(header, 3, 0);
  lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(header, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(header, consumeHeaderTouchEvent, LV_EVENT_ALL, nullptr);
  styleDashboardPanel(header, UI_COLOR_HEADER_BG, UI_COLOR_HEADER_BORDER);

  lv_obj_t *leftWrap = lv_obj_create(header);
  lv_obj_set_size(leftWrap, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
  lv_obj_align(leftWrap, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_set_style_pad_all(leftWrap, 0, 0);
  lv_obj_set_style_bg_opa(leftWrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(leftWrap, 0, 0);
  lv_obj_clear_flag(leftWrap, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(leftWrap, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(leftWrap, consumeHeaderTouchEvent, LV_EVENT_ALL, nullptr);

  ui.timeLabel = lv_label_create(leftWrap);
  lv_label_set_text(ui.timeLabel, "sync orario...");
  setDashboardLabelFont(ui.timeLabel, &lv_font_montserrat_14);
  setDashboardLabelColor(ui.timeLabel, UI_COLOR_TEXT_LIGHT);

  lv_obj_t *weatherWrap = lv_obj_create(header);
  lv_obj_set_size(weatherWrap, 98, LV_SIZE_CONTENT);
  lv_obj_align(weatherWrap, LV_ALIGN_CENTER, 8, 0);
  lv_obj_set_flex_flow(weatherWrap, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(weatherWrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(weatherWrap, 0, 0);
  lv_obj_set_style_pad_gap(weatherWrap, 3, 0);
  lv_obj_set_style_bg_opa(weatherWrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(weatherWrap, 0, 0);
  lv_obj_clear_flag(weatherWrap, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(weatherWrap, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(weatherWrap, consumeHeaderTouchEvent, LV_EVENT_ALL, nullptr);

  ui.weatherIcon = lv_img_create(weatherWrap);
  lv_img_set_src(ui.weatherIcon, &IMG_WEATHER_SUN);
  lv_obj_clear_flag(ui.weatherIcon, LV_OBJ_FLAG_SCROLLABLE);

  ui.weatherLabel = lv_label_create(weatherWrap);
  lv_label_set_text(ui.weatherLabel, "meteo n/d");
  setDashboardLabelFont(ui.weatherLabel, &lv_font_montserrat_12);
  setDashboardLabelColor(ui.weatherLabel, UI_COLOR_TEXT_INFO);
  lv_label_set_long_mode(ui.weatherLabel, LV_LABEL_LONG_DOT);
  lv_obj_set_width(ui.weatherLabel, 78);

  lv_obj_t *rightWrap = lv_obj_create(header);
  lv_obj_set_size(rightWrap, 96, 22);
  lv_obj_align(rightWrap, LV_ALIGN_RIGHT_MID, 0, 0);
  lv_obj_set_flex_flow(rightWrap, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(rightWrap, LV_FLEX_ALIGN_END, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(rightWrap, 0, 0);
  lv_obj_set_style_pad_gap(rightWrap, 2, 0);
  lv_obj_set_style_bg_opa(rightWrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(rightWrap, 0, 0);
  lv_obj_clear_flag(rightWrap, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(rightWrap, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(rightWrap, consumeHeaderTouchEvent, LV_EVENT_ALL, nullptr);

  ui.otaButton = lv_btn_create(rightWrap);
  lv_obj_set_size(ui.otaButton, 26, 22);
  lv_obj_set_style_bg_color(ui.otaButton, colorFromHex(UI_COLOR_HEADER_BORDER), 0);
  lv_obj_set_style_bg_opa(ui.otaButton, LV_OPA_30, 0);
  lv_obj_set_style_border_width(ui.otaButton, 0, 0);
  lv_obj_set_style_shadow_width(ui.otaButton, 0, 0);
  lv_obj_set_style_radius(ui.otaButton, 10, 0);
  lv_obj_set_style_pad_all(ui.otaButton, 0, 0);
  lv_obj_clear_flag(ui.otaButton, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ui.otaButton, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(ui.otaButton, 10);
  lv_obj_add_event_cb(ui.otaButton, otaHeaderEventCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(ui.otaButton, otaHeaderEventCb, LV_EVENT_SHORT_CLICKED, nullptr);
  lv_obj_add_event_cb(ui.otaButton, otaHeaderEventCb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(ui.otaButton, otaHeaderEventCb, LV_EVENT_PRESSED, nullptr);

  ui.otaLabel = lv_label_create(ui.otaButton);
  lv_label_set_text(ui.otaLabel, LV_SYMBOL_DOWNLOAD);
  setDashboardLabelFont(ui.otaLabel, &lv_font_montserrat_12);
  setDashboardLabelColor(ui.otaLabel, UI_COLOR_TEXT_MUTED);
  lv_obj_center(ui.otaLabel);
  lv_obj_add_flag(ui.otaLabel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ui.otaLabel, otaHeaderEventCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(ui.otaLabel, otaHeaderEventCb, LV_EVENT_SHORT_CLICKED, nullptr);
  lv_obj_add_event_cb(ui.otaLabel, otaHeaderEventCb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(ui.otaLabel, otaHeaderEventCb, LV_EVENT_PRESSED, nullptr);
  lv_obj_add_flag(ui.otaButton, LV_OBJ_FLAG_HIDDEN);

  ui.wifiLabel = lv_label_create(rightWrap);
  lv_label_set_text(ui.wifiLabel, LV_SYMBOL_WIFI);
  setDashboardLabelFont(ui.wifiLabel, &lv_font_montserrat_12);
  setDashboardLabelColor(ui.wifiLabel, UI_COLOR_TEXT_MUTED);

  lv_obj_t *batteryTextWrap = lv_obj_create(rightWrap);
  lv_obj_set_size(batteryTextWrap, 32, 20);
  lv_obj_set_flex_flow(batteryTextWrap, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(batteryTextWrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(batteryTextWrap, 0, 0);
  lv_obj_set_style_pad_row(batteryTextWrap, 0, 0);
  lv_obj_set_style_bg_opa(batteryTextWrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(batteryTextWrap, 0, 0);
  lv_obj_clear_flag(batteryTextWrap, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(batteryTextWrap, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(batteryTextWrap, consumeHeaderTouchEvent, LV_EVENT_ALL, nullptr);

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

  ui.headerTouchOverlay = lv_obj_create(lv_layer_top());
  lv_obj_set_pos(ui.headerTouchOverlay, 3, 3);
  lv_obj_set_size(ui.headerTouchOverlay, UI_HEADER_WIDTH, UI_HEADER_TOUCH_OVERLAY_HEIGHT);
  lv_obj_set_scrollbar_mode(ui.headerTouchOverlay, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_opa(ui.headerTouchOverlay, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ui.headerTouchOverlay, 0, 0);
  lv_obj_set_style_radius(ui.headerTouchOverlay, 0, 0);
  lv_obj_set_style_pad_all(ui.headerTouchOverlay, 0, 0);
  lv_obj_clear_flag(ui.headerTouchOverlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ui.headerTouchOverlay, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(ui.headerTouchOverlay, headerTouchOverlayEventCb, LV_EVENT_ALL, nullptr);
  lv_obj_move_foreground(ui.headerTouchOverlay);
}

void refreshDashboardHeaderUi() {
  updateWifiUi();
  refreshOtaHeaderUi();
  refreshClockHeaderUi();
  refreshWeatherHeaderUi();
  refreshBatteryHeaderUi();
}
