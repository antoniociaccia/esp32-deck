#include "dashboard_ui.h"
#include "dashboard_app.h"
#include <time.h>
#include <WiFi.h>
#include "secrets.h"
#include "weather_icons.h"

static void setLabelFont(lv_obj_t *label, const lv_font_t *font) {
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
}

static void setLabelColor(lv_obj_t *label, uint32_t colorHex) {
  if (label == nullptr) {
    return;
  }

  lv_obj_set_style_text_color(label, colorFromHex(colorHex), LV_PART_MAIN);
}

static void stylePanel(lv_obj_t *panel, uint32_t bgColorHex, uint32_t borderColorHex) {
  lv_obj_set_style_radius(panel, PANEL_RADIUS, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_border_color(panel, colorFromHex(borderColorHex), 0);
  lv_obj_set_style_bg_color(panel, colorFromHex(bgColorHex), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(panel, 0, 0);
}

static void createSpacer(lv_obj_t *parent, lv_coord_t height) {
  lv_obj_t *spacer = lv_obj_create(parent);
  lv_obj_set_size(spacer, 1, height);
  lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(spacer, 0, 0);
  lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);
}

void updateWifiUi() {
  if (ui.wifiLabel == nullptr) {
    return;
  }

  lv_label_set_text(ui.wifiLabel, LV_SYMBOL_WIFI);
  setLabelColor(ui.wifiLabel, WiFi.status() == WL_CONNECTED ? COLOR_WIFI_ONLINE : COLOR_TEXT_MUTED);
}

static void updateModuleDots() {
  for (int i = 0; i < MODULE_COUNT; ++i) {
    if (ui.moduleDots[i] == nullptr) {
      continue;
    }

    bool isActive = i == app.currentModuleIndex;
    lv_obj_set_size(ui.moduleDots[i], isActive ? 14 : 8, 8);
    lv_obj_set_style_bg_color(ui.moduleDots[i],
      colorFromHex(isActive ? COLOR_ACCENT : COLOR_DOT_IDLE), 0);
    lv_obj_set_style_bg_opa(ui.moduleDots[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui.moduleDots[i], 0, 0);
    lv_obj_set_style_radius(ui.moduleDots[i], 4, 0);
  }
}

static void updateClockModuleCard() {
  if (ui.moduleValueLabels[0] == nullptr || ui.moduleMetaLabels[0] == nullptr) {
    return;
  }

  char valueBuffer[24];
  char metaBuffer[48];
  struct tm timeinfo;
  bool hasTime = getLocalTime(&timeinfo, 10);

  if (hasTime) {
    snprintf(valueBuffer, sizeof(valueBuffer), "%02d:%02d", timeinfo.tm_hour, timeinfo.tm_min);
  } else {
    snprintf(valueBuffer, sizeof(valueBuffer), "sync...");
  }

  snprintf(metaBuffer, sizeof(metaBuffer), "%s | %s",
    WiFi.status() == WL_CONNECTED ? "wifi ok" : "wifi off",
    app.timeSynced ? "ntp ok" : "ntp retry");

  lv_label_set_text(ui.moduleValueLabels[0], valueBuffer);
  lv_label_set_text(ui.moduleMetaLabels[0], metaBuffer);
}

static void updatePowerModuleCard() {
  if (ui.moduleValueLabels[1] == nullptr || ui.moduleMetaLabels[1] == nullptr) {
    return;
  }

  char valueBuffer[24];
  char metaBuffer[48];

  if (!app.batteryPresent || app.batteryPercent < 0) {
    snprintf(valueBuffer, sizeof(valueBuffer), "--");
    snprintf(metaBuffer, sizeof(metaBuffer), "batteria assente o instabile");
  } else {
    snprintf(valueBuffer, sizeof(valueBuffer), "%d%%", app.batteryPercent);
    snprintf(metaBuffer, sizeof(metaBuffer), "%.2fV filtrata", app.batteryVoltage);
  }

  lv_label_set_text(ui.moduleValueLabels[1], valueBuffer);
  lv_label_set_text(ui.moduleMetaLabels[1], metaBuffer);
}

static void updateWeatherModuleCard() {
  if (ui.moduleValueLabels[2] == nullptr || ui.moduleMetaLabels[2] == nullptr) {
    return;
  }

  char valueBuffer[24];
  char metaBuffer[64];

  if (!app.weatherValid) {
    snprintf(valueBuffer, sizeof(valueBuffer), "--");
    snprintf(metaBuffer, sizeof(metaBuffer), "%s | offline o retry", WEATHER_CITY_LABEL);
  } else {
    snprintf(valueBuffer, sizeof(valueBuffer), "%dC", app.weatherTemperatureC);
    snprintf(metaBuffer, sizeof(metaBuffer), "%s | dati live", WEATHER_CITY_LABEL);
  }

  lv_label_set_text(ui.moduleValueLabels[2], valueBuffer);
  lv_label_set_text(ui.moduleMetaLabels[2], metaBuffer);
}

static void updateNewsModuleCard() {
  if (ui.moduleValueLabels[3] == nullptr || ui.moduleMetaLabels[3] == nullptr) {
    return;
  }

  char valueBuffer[24];
  if (app.newsItemCount > 0) {
    snprintf(valueBuffer, sizeof(valueBuffer), "%d news", app.newsItemCount);
  } else {
    snprintf(valueBuffer, sizeof(valueBuffer), "0 news");
  }

  const char *headline = app.newsItemCount > 0 ? app.newsItems[0] : "feed non disponibile";
  lv_label_set_text(ui.moduleValueLabels[3], valueBuffer);
  lv_label_set_text(ui.moduleMetaLabels[3], headline);
}

void updateModuleUi() {
  updateClockModuleCard();
  updatePowerModuleCard();
  updateWeatherModuleCard();
  updateNewsModuleCard();
  updateModuleDots();
}

void setBatteryUiUnavailable() {
  app.batteryInitialized = false;
  if (ui.batteryPercentLabel == nullptr || ui.batteryVoltageLabel == nullptr || ui.batteryIconLabel == nullptr) {
    return;
  }

  lv_label_set_text(ui.batteryPercentLabel, "--");
  lv_label_set_text(ui.batteryVoltageLabel, "--.--V");
  lv_label_set_text(ui.batteryIconLabel, LV_SYMBOL_BATTERY_EMPTY);
  setLabelColor(ui.batteryPercentLabel, COLOR_TEXT_MUTED);
  setLabelColor(ui.batteryVoltageLabel, COLOR_TEXT_MUTED);
  setLabelColor(ui.batteryIconLabel, COLOR_TEXT_MUTED);
}

void setBatteryUiValue(float voltage, int batteryPercent) {
  if (ui.batteryPercentLabel == nullptr || ui.batteryVoltageLabel == nullptr || ui.batteryIconLabel == nullptr) {
    return;
  }

  char percentBuffer[12];
  snprintf(percentBuffer, sizeof(percentBuffer), "%d%%", batteryPercent);
  lv_label_set_text(ui.batteryPercentLabel, percentBuffer);

  char voltageBuffer[12];
  snprintf(voltageBuffer, sizeof(voltageBuffer), "%.2fV", voltage);
  lv_label_set_text(ui.batteryVoltageLabel, voltageBuffer);

  if (batteryPercent >= 85) {
    lv_label_set_text(ui.batteryIconLabel, LV_SYMBOL_BATTERY_FULL);
  } else if (batteryPercent >= 60) {
    lv_label_set_text(ui.batteryIconLabel, LV_SYMBOL_BATTERY_3);
  } else if (batteryPercent >= 35) {
    lv_label_set_text(ui.batteryIconLabel, LV_SYMBOL_BATTERY_2);
  } else if (batteryPercent >= 15) {
    lv_label_set_text(ui.batteryIconLabel, LV_SYMBOL_BATTERY_1);
  } else {
    lv_label_set_text(ui.batteryIconLabel, LV_SYMBOL_BATTERY_EMPTY);
  }

  setLabelColor(ui.batteryPercentLabel, COLOR_BATTERY_OK);
  setLabelColor(ui.batteryVoltageLabel, COLOR_BATTERY_OK);
  setLabelColor(ui.batteryIconLabel, COLOR_BATTERY_OK);
}

void updateWeatherIconUi(const String &iconCode) {
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

void setWeatherStatus(const char *text) {
  if (ui.weatherLabel != nullptr) {
    lv_label_set_text(ui.weatherLabel, text);
  }
}

static void createHeader(lv_obj_t *parent) {
  lv_obj_t *header = lv_obj_create(parent);
  lv_obj_set_size(header, HEADER_W, HEADER_H);
  lv_obj_set_style_pad_hor(header, 8, 0);
  lv_obj_set_style_pad_ver(header, 3, 0);
  lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  stylePanel(header, COLOR_HEADER_BG, COLOR_HEADER_BORDER);

  ui.timeLabel = lv_label_create(header);
  lv_label_set_text(ui.timeLabel, "sync orario...");
  setLabelFont(ui.timeLabel, &lv_font_montserrat_14);
  setLabelColor(ui.timeLabel, COLOR_TEXT_LIGHT);
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
  lv_label_set_text(ui.weatherLabel, "Roma 18C");
  setLabelFont(ui.weatherLabel, &lv_font_montserrat_14);
  setLabelColor(ui.weatherLabel, COLOR_TEXT_INFO);

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
  setLabelFont(ui.wifiLabel, &lv_font_montserrat_12);
  setLabelColor(ui.wifiLabel, COLOR_TEXT_MUTED);

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
  setLabelFont(ui.batteryPercentLabel, &lv_font_montserrat_10);
  setLabelColor(ui.batteryPercentLabel, COLOR_BATTERY_OK);
  lv_obj_set_width(ui.batteryPercentLabel, 34);
  lv_obj_set_style_text_align(ui.batteryPercentLabel, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

  ui.batteryVoltageLabel = lv_label_create(batteryTextWrap);
  lv_label_set_text(ui.batteryVoltageLabel, "--.--V");
  setLabelFont(ui.batteryVoltageLabel, &lv_font_montserrat_10);
  setLabelColor(ui.batteryVoltageLabel, COLOR_BATTERY_OK);
  lv_obj_set_width(ui.batteryVoltageLabel, 34);
  lv_obj_set_style_text_align(ui.batteryVoltageLabel, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

  ui.batteryIconLabel = lv_label_create(rightWrap);
  lv_label_set_text(ui.batteryIconLabel, LV_SYMBOL_BATTERY_3);
  setLabelFont(ui.batteryIconLabel, &lv_font_montserrat_12);
  setLabelColor(ui.batteryIconLabel, COLOR_BATTERY_OK);
}

static void tileviewEventCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
    return;
  }

  lv_obj_t *activeTile = lv_tileview_get_tile_act(ui.tileview);
  for (int i = 0; i < MODULE_COUNT; ++i) {
    if (ui.moduleTiles[i] == activeTile) {
      app.currentModuleIndex = i;
      updateModuleUi();
      break;
    }
  }
}

static void createModuleTileContent(lv_obj_t *tile, const ModuleContent &module) {
  lv_obj_set_style_bg_opa(tile, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(tile, 0, 0);
  lv_obj_set_style_pad_all(tile, 14, 0);
  lv_obj_set_style_pad_row(tile, 0, 0);
  lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);

  lv_obj_t *title = lv_label_create(tile);
  lv_label_set_text(title, module.title);
  setLabelFont(title, &lv_font_montserrat_14);
  setLabelColor(title, COLOR_ACCENT);

  createSpacer(tile, 22);

  lv_obj_t *value = lv_label_create(tile);
  lv_label_set_text(value, module.value);
  setLabelFont(value, &lv_font_montserrat_20);
  setLabelColor(value, COLOR_TEXT_PRIMARY);
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(value, MAIN_W - 28);

  createSpacer(tile, 26);

  lv_obj_t *meta = lv_label_create(tile);
  lv_obj_set_width(meta, MAIN_W - 28);
  lv_obj_set_style_text_align(meta, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_label_set_text(meta, module.meta);
  setLabelFont(meta, &lv_font_montserrat_14);
  setLabelColor(meta, COLOR_TEXT_SECONDARY);

  for (int i = 0; i < MODULE_COUNT; ++i) {
    if (ui.moduleTiles[i] == tile) {
      ui.moduleValueLabels[i] = value;
      ui.moduleMetaLabels[i] = meta;
      break;
    }
  }
}

static void createMain(lv_obj_t *parent) {
  ui.tileview = lv_tileview_create(parent);
  lv_obj_set_size(ui.tileview, MAIN_W, MAIN_H);
  lv_obj_set_style_pad_all(ui.tileview, 0, 0);
  lv_obj_set_scrollbar_mode(ui.tileview, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(ui.tileview, LV_OBJ_FLAG_SCROLL_ELASTIC);
  stylePanel(ui.tileview, COLOR_MAIN_BG, COLOR_MAIN_BORDER);

  for (int i = 0; i < MODULE_COUNT; ++i) {
    lv_dir_t directions = i == 0 ? LV_DIR_RIGHT
      : (i == MODULE_COUNT - 1 ? LV_DIR_LEFT : (LV_DIR_LEFT | LV_DIR_RIGHT));
    ui.moduleTiles[i] = lv_tileview_add_tile(ui.tileview, i, 0, directions);
    createModuleTileContent(ui.moduleTiles[i], MODULES[i]);
  }

  lv_obj_add_event_cb(ui.tileview, tileviewEventCb, LV_EVENT_VALUE_CHANGED, NULL);

  createSpacer(parent, 5);

  ui.moduleDotsWrap = lv_obj_create(parent);
  lv_obj_set_size(ui.moduleDotsWrap, 72, 10);
  lv_obj_set_flex_flow(ui.moduleDotsWrap, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ui.moduleDotsWrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(ui.moduleDotsWrap, 0, 0);
  lv_obj_set_style_pad_gap(ui.moduleDotsWrap, 6, 0);
  lv_obj_set_style_bg_opa(ui.moduleDotsWrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ui.moduleDotsWrap, 0, 0);
  lv_obj_clear_flag(ui.moduleDotsWrap, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < MODULE_COUNT; ++i) {
    ui.moduleDots[i] = lv_obj_create(ui.moduleDotsWrap);
  }

  createSpacer(parent, 5);
  updateModuleUi();
}

static void createFooter(lv_obj_t *parent) {
  lv_obj_t *footer = lv_obj_create(parent);
  lv_obj_set_size(footer, FOOTER_W, FOOTER_H);
  lv_obj_set_style_pad_hor(footer, 10, 0);
  lv_obj_set_style_pad_ver(footer, 5, 0);
  lv_obj_set_scrollbar_mode(footer, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
  stylePanel(footer, COLOR_HEADER_BG, COLOR_HEADER_BORDER);

  ui.newsLabel = lv_label_create(footer);
  lv_obj_set_width(ui.newsLabel, FOOTER_W - 20);
  lv_obj_set_style_text_align(ui.newsLabel, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_label_set_long_mode(ui.newsLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_scrollbar_mode(ui.newsLabel, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(ui.newsLabel, LV_OBJ_FLAG_SCROLLABLE);
  lv_label_set_text(ui.newsLabel, app.newsTicker);
  setLabelFont(ui.newsLabel, &lv_font_montserrat_16);
  setLabelColor(ui.newsLabel, COLOR_TEXT_NEWS);
  lv_obj_align(ui.newsLabel, LV_ALIGN_LEFT_MID, 0, 0);
}

void createDashboardUi() {
  lv_obj_t *screenRoot = lv_scr_act();
  lv_obj_set_style_bg_color(screenRoot, colorFromHex(COLOR_SCREEN_BG), 0);
  lv_obj_set_style_bg_opa(screenRoot, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_left(screenRoot, 3, 0);
  lv_obj_set_style_pad_right(screenRoot, 3, 0);
  lv_obj_set_style_pad_top(screenRoot, 3, 0);
  lv_obj_set_style_pad_bottom(screenRoot, 3, 0);
  lv_obj_set_style_pad_gap(screenRoot, 0, 0);
  lv_obj_set_layout(screenRoot, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screenRoot, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(screenRoot, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  createHeader(screenRoot);
  createSpacer(screenRoot, 6);
  createMain(screenRoot);
  createFooter(screenRoot);
}
