#include "dashboard_ui_main.h"
#include "dashboard_app.h"
#include "dashboard_support.h"
#include "dashboard_ui_shared.h"
#include <WiFi.h>
#include "secrets.h"

static void updateModuleDots() {
  for (int i = 0; i < UI_MODULE_COUNT; ++i) {
    if (ui.moduleDots[i] == nullptr) {
      continue;
    }

    bool isActive = i == app.currentModuleIndex;
    lv_obj_set_size(ui.moduleDots[i], isActive ? 14 : 8, 8);
    lv_obj_set_style_bg_color(ui.moduleDots[i],
      colorFromHex(isActive ? UI_COLOR_ACCENT : UI_COLOR_DOT_IDLE), 0);
    lv_obj_set_style_bg_opa(ui.moduleDots[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui.moduleDots[i], 0, 0);
    lv_obj_set_style_radius(ui.moduleDots[i], 4, 0);
  }
}

static void updateClockModuleCard() {
  if (ui.moduleValueLabels[0] == nullptr || ui.moduleMetaLabels[0] == nullptr) {
    return;
  }

  char metaBuffer[48];
  snprintf(metaBuffer, sizeof(metaBuffer), "%s | %s",
    WiFi.status() == WL_CONNECTED ? "wifi ok" : "wifi off",
    app.timeSynced ? "ntp ok" : "ntp retry");

  const char *clockText = strlen(app.clockLabelText) > 0 ? app.clockLabelText : "sync...";
  lv_label_set_text(ui.moduleValueLabels[0], clockText);
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

static void tileviewEventCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
    return;
  }

  lv_obj_t *activeTile = lv_tileview_get_tile_act(ui.tileview);
  for (int i = 0; i < UI_MODULE_COUNT; ++i) {
    if (ui.moduleTiles[i] == activeTile) {
      app.currentModuleIndex = i;
      refreshDashboardMainUi();
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
  setDashboardLabelFont(title, &lv_font_montserrat_14);
  setDashboardLabelColor(title, UI_COLOR_ACCENT);

  createDashboardSpacer(tile, 22);

  lv_obj_t *value = lv_label_create(tile);
  lv_label_set_text(value, module.value);
  setDashboardLabelFont(value, &lv_font_montserrat_20);
  setDashboardLabelColor(value, UI_COLOR_TEXT_PRIMARY);
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(value, UI_MAIN_WIDTH - 28);

  createDashboardSpacer(tile, 26);

  lv_obj_t *meta = lv_label_create(tile);
  lv_obj_set_width(meta, UI_MAIN_WIDTH - 28);
  lv_obj_set_style_text_align(meta, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_label_set_text(meta, module.meta);
  setDashboardLabelFont(meta, &lv_font_montserrat_14);
  setDashboardLabelColor(meta, UI_COLOR_TEXT_SECONDARY);

  for (int i = 0; i < UI_MODULE_COUNT; ++i) {
    if (ui.moduleTiles[i] == tile) {
      ui.moduleValueLabels[i] = value;
      ui.moduleMetaLabels[i] = meta;
      break;
    }
  }
}

void createDashboardMain(lv_obj_t *parent) {
  ui.tileview = lv_tileview_create(parent);
  lv_obj_set_size(ui.tileview, UI_MAIN_WIDTH, UI_MAIN_HEIGHT);
  lv_obj_set_style_pad_all(ui.tileview, 0, 0);
  lv_obj_set_scrollbar_mode(ui.tileview, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(ui.tileview, LV_OBJ_FLAG_SCROLL_ELASTIC);
  styleDashboardPanel(ui.tileview, UI_COLOR_MAIN_BG, UI_COLOR_MAIN_BORDER);

  for (int i = 0; i < UI_MODULE_COUNT; ++i) {
    lv_dir_t directions = i == 0 ? LV_DIR_RIGHT
      : (i == UI_MODULE_COUNT - 1 ? LV_DIR_LEFT : (LV_DIR_LEFT | LV_DIR_RIGHT));
    ui.moduleTiles[i] = lv_tileview_add_tile(ui.tileview, i, 0, directions);
    createModuleTileContent(ui.moduleTiles[i], MODULES[i]);
  }

  lv_obj_add_event_cb(ui.tileview, tileviewEventCb, LV_EVENT_VALUE_CHANGED, NULL);

  createDashboardSpacer(parent, 5);

  ui.moduleDotsWrap = lv_obj_create(parent);
  lv_obj_set_size(ui.moduleDotsWrap, 72, 10);
  lv_obj_set_flex_flow(ui.moduleDotsWrap, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ui.moduleDotsWrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(ui.moduleDotsWrap, 0, 0);
  lv_obj_set_style_pad_gap(ui.moduleDotsWrap, 6, 0);
  lv_obj_set_style_bg_opa(ui.moduleDotsWrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ui.moduleDotsWrap, 0, 0);
  lv_obj_clear_flag(ui.moduleDotsWrap, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < UI_MODULE_COUNT; ++i) {
    ui.moduleDots[i] = lv_obj_create(ui.moduleDotsWrap);
  }

  createDashboardSpacer(parent, 5);
}

void refreshDashboardMainUi() {
  updateClockModuleCard();
  updatePowerModuleCard();
  updateWeatherModuleCard();
  updateNewsModuleCard();
  updateModuleDots();
}
