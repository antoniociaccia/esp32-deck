#include "dashboard_ui_main.h"
#include "dashboard_app.h"
#include "dashboard_ui.h"
#include "dashboard_support.h"
#include "dashboard_ui_shared.h"
#include <WiFi.h>
#include "secrets.h"
#include "weather_icons.h"

static constexpr lv_coord_t UI_MODULE_TILE_INNER_HEIGHT = UI_MAIN_HEIGHT - 8;
static constexpr size_t UI_NEWS_PREVIEW_MAX_LEN = 92;
static constexpr size_t UI_MODULE_ICON_TEXT_LEN = 12;
static constexpr size_t UI_MODULE_BADGE_TEXT_LEN = 16;

struct ModuleBadgeState {
  bool initialized = false;
  char text[UI_MODULE_BADGE_TEXT_LEN] = {};
  uint32_t bgColorHex = 0;
  uint32_t textColorHex = 0;
};

struct ModuleIconState {
  bool initialized = false;
  char text[UI_MODULE_ICON_TEXT_LEN] = {};
  uint32_t colorHex = 0;
};

static ModuleBadgeState moduleBadgeStates[UI_MODULE_COUNT];
static ModuleIconState moduleIconStates[UI_MODULE_COUNT];
static const void *lastWeatherModuleIconSource = nullptr;
static int lastStyledModuleIndex = -1;
static int lastDotsModuleIndex = -1;

static void createSettingsModuleCard(lv_obj_t *tile, int moduleIndex);
static void updateSettingsModuleCard();
static void createClockModuleCard(lv_obj_t *tile, int moduleIndex);
static void updateClockModuleCard();
static void createPowerModuleCard(lv_obj_t *tile, int moduleIndex);
static void updatePowerModuleCard();
static void createWeatherModuleCard(lv_obj_t *tile, int moduleIndex);
static void updateWeatherModuleCard();
static void createNewsModuleCard(lv_obj_t *tile, int moduleIndex);
static void updateNewsModuleCard();

static const ModuleCardDef MODULE_REGISTRY[UI_MODULE_COUNT] = {
  {"Settings", "Configura", "energia, rete e sistema", UI_DIRTY_ALL, createSettingsModuleCard, updateSettingsModuleCard},
  {"Clock", "--:--", "attesa sync NTP", UI_DIRTY_MAIN_CLOCK, createClockModuleCard, updateClockModuleCard},
  {"Power", "--", "attesa batteria", UI_DIRTY_MAIN_POWER, createPowerModuleCard, updatePowerModuleCard},
  {"Weather", "--", "attesa meteo", UI_DIRTY_MAIN_WEATHER, createWeatherModuleCard, updateWeatherModuleCard},
  {"News", "--", "attesa feed", UI_DIRTY_MAIN_NEWS, createNewsModuleCard, updateNewsModuleCard}
};

static bool wifiOnline() {
  return WiFi.status() == WL_CONNECTED;
}

static void setBadgeStyle(lv_obj_t *label, uint32_t bgColorHex, uint32_t textColorHex) {
  if (label == nullptr) {
    return;
  }

  setDashboardLabelColor(label, textColorHex);
  lv_obj_set_style_bg_color(label, colorFromHex(bgColorHex), LV_PART_MAIN);
  lv_obj_set_style_bg_opa(label, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_radius(label, 10, LV_PART_MAIN);
  lv_obj_set_style_pad_left(label, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_right(label, 8, LV_PART_MAIN);
  lv_obj_set_style_pad_top(label, 3, LV_PART_MAIN);
  lv_obj_set_style_pad_bottom(label, 3, LV_PART_MAIN);
}

static void setModuleBadge(int index, const char *text, uint32_t bgColorHex, uint32_t textColorHex) {
  if (index < 0 || index >= UI_MODULE_COUNT || ui.moduleBadgeLabels[index] == nullptr) {
    return;
  }

  ModuleBadgeState &state = moduleBadgeStates[index];
  if (state.initialized
    && strcmp(state.text, text) == 0
    && state.bgColorHex == bgColorHex
    && state.textColorHex == textColorHex) {
    return;
  }

  setDashboardLabelTextIfChanged(ui.moduleBadgeLabels[index], text);
  setBadgeStyle(ui.moduleBadgeLabels[index], bgColorHex, textColorHex);
  strlcpy(state.text, text, sizeof(state.text));
  state.bgColorHex = bgColorHex;
  state.textColorHex = textColorHex;
  state.initialized = true;
}

static void setServiceBadgeFromState(int moduleIndex, ServiceFetchState state) {
  switch (state) {
    case SERVICE_FETCH_READY:
      setModuleBadge(moduleIndex, "live", UI_COLOR_BADGE_OK_BG, UI_COLOR_ACCENT);
      break;
    case SERVICE_FETCH_OFFLINE:
      setModuleBadge(moduleIndex, "offline", UI_COLOR_BADGE_OFFLINE_BG, UI_COLOR_TEXT_SECONDARY);
      break;
    case SERVICE_FETCH_CONFIG_MISSING:
      setModuleBadge(moduleIndex, "config", UI_COLOR_BADGE_ALERT_BG, UI_COLOR_ACCENT);
      break;
    case SERVICE_FETCH_TRANSPORT_ERROR:
      setModuleBadge(moduleIndex, "rete", UI_COLOR_BADGE_RETRY_BG, UI_COLOR_ACCENT);
      break;
    case SERVICE_FETCH_HTTP_ERROR:
      setModuleBadge(moduleIndex, "http", UI_COLOR_BADGE_ALERT_BG, UI_COLOR_ACCENT);
      break;
    case SERVICE_FETCH_INVALID_PAYLOAD:
      setModuleBadge(moduleIndex, "json", UI_COLOR_BADGE_ALERT_BG, UI_COLOR_ACCENT);
      break;
    case SERVICE_FETCH_IDLE:
    default:
      setModuleBadge(moduleIndex, "init", UI_COLOR_BADGE_OFFLINE_BG, UI_COLOR_TEXT_SECONDARY);
      break;
  }
}

static void setModuleIconLabel(int index, const char *text, uint32_t colorHex) {
  if (index < 0 || index >= UI_MODULE_COUNT || ui.moduleIconRefs[index] == nullptr) {
    return;
  }

  ModuleIconState &state = moduleIconStates[index];
  if (state.initialized && strcmp(state.text, text) == 0 && state.colorHex == colorHex) {
    return;
  }

  setDashboardLabelTextIfChanged(ui.moduleIconRefs[index], text);
  setDashboardLabelColor(ui.moduleIconRefs[index], colorHex);
  strlcpy(state.text, text, sizeof(state.text));
  state.colorHex = colorHex;
  state.initialized = true;
}

static void styleModuleTile(lv_obj_t *tile, bool isActive) {
  if (tile == nullptr) {
    return;
  }

  styleDashboardPanel(tile,
    isActive ? UI_COLOR_CARD_ACTIVE_BG : UI_COLOR_MAIN_BG,
    isActive ? UI_COLOR_CARD_ACTIVE_BORDER : UI_COLOR_MAIN_BORDER);
  lv_obj_set_style_pad_all(tile, 14, 0);
}

static void updateModuleDots() {
  if (lastDotsModuleIndex == app.currentModuleIndex) {
    return;
  }

  for (int i = 0; i < UI_MODULE_COUNT; ++i) {
    if (ui.moduleDots[i] == nullptr) {
      continue;
    }

    bool isActive = i == app.currentModuleIndex;
    if (i == 0) {
      setDashboardLabelTextIfChanged(ui.moduleDots[i], LV_SYMBOL_SETTINGS);
      setDashboardLabelFont(ui.moduleDots[i], &lv_font_montserrat_12);
      setDashboardLabelColor(ui.moduleDots[i], isActive ? UI_COLOR_ACCENT : UI_COLOR_DOT_IDLE);
      continue;
    }

    lv_obj_set_size(ui.moduleDots[i], isActive ? 14 : 8, 8);
    lv_obj_set_style_bg_color(ui.moduleDots[i],
      colorFromHex(isActive ? UI_COLOR_ACCENT : UI_COLOR_DOT_IDLE), 0);
    lv_obj_set_style_bg_opa(ui.moduleDots[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(ui.moduleDots[i], 0, 0);
    lv_obj_set_style_radius(ui.moduleDots[i], 4, 0);
  }

  lastDotsModuleIndex = app.currentModuleIndex;
}

static void updateModuleTileStates() {
  if (lastStyledModuleIndex == app.currentModuleIndex) {
    return;
  }

  for (int i = 0; i < UI_MODULE_COUNT; ++i) {
    styleModuleTile(ui.moduleTiles[i], i == app.currentModuleIndex);
  }

  lastStyledModuleIndex = app.currentModuleIndex;
}

static const void *weatherCardImageSource() {
  if (strncmp(app.weather.iconCode, "01", 2) == 0) {
    return &IMG_WEATHER_SUN;
  }
  if (strncmp(app.weather.iconCode, "02", 2) == 0
    || strncmp(app.weather.iconCode, "03", 2) == 0
    || strncmp(app.weather.iconCode, "04", 2) == 0) {
    return &IMG_WEATHER_CLOUD;
  }
  if (strncmp(app.weather.iconCode, "09", 2) == 0 || strncmp(app.weather.iconCode, "10", 2) == 0) {
    return &IMG_WEATHER_RAIN;
  }
  if (strncmp(app.weather.iconCode, "11", 2) == 0) {
    return &IMG_WEATHER_STORM;
  }
  if (strncmp(app.weather.iconCode, "13", 2) == 0) {
    return &IMG_WEATHER_SNOW;
  }
  if (strncmp(app.weather.iconCode, "50", 2) == 0) {
    return &IMG_WEATHER_FOG;
  }
  return &IMG_WEATHER_CLOUD;
}

static void buildNewsPreview(char *buffer, size_t bufferSize) {
  const char *headline = app.news.itemCount > 0 ? app.news.items[0] : "Feed news non disponibile";
  size_t headlineLen = strlen(headline);
  if (headlineLen < bufferSize) {
    strlcpy(buffer, headline, bufferSize);
    return;
  }

  size_t copyLen = bufferSize > 4 ? bufferSize - 4 : 0;
  while (copyLen > 28 && headline[copyLen] != '\0' && headline[copyLen] != ' ') {
    copyLen--;
  }
  if (copyLen <= 28) {
    copyLen = bufferSize - 4;
  }

  memcpy(buffer, headline, copyLen);
  buffer[copyLen] = '\0';
  strlcat(buffer, "...", bufferSize);
}

static lv_obj_t *createModuleTopRow(lv_obj_t *parent, int moduleIndex) {
  lv_obj_t *topRow = lv_obj_create(parent);
  lv_obj_set_width(topRow, lv_pct(100));
  lv_obj_set_height(topRow, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(topRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(topRow, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(topRow, 0, 0);
  lv_obj_set_style_pad_gap(topRow, 6, 0);
  lv_obj_set_style_bg_opa(topRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(topRow, 0, 0);
  lv_obj_clear_flag(topRow, LV_OBJ_FLAG_SCROLLABLE);

  ui.moduleTitleLabels[moduleIndex] = lv_label_create(topRow);
  lv_label_set_text(ui.moduleTitleLabels[moduleIndex], MODULE_REGISTRY[moduleIndex].title);
  setDashboardLabelFont(ui.moduleTitleLabels[moduleIndex], &lv_font_montserrat_12);
  setDashboardLabelColor(ui.moduleTitleLabels[moduleIndex], UI_COLOR_TEXT_SECONDARY);

  ui.moduleBadgeLabels[moduleIndex] = lv_label_create(topRow);
  lv_label_set_text(ui.moduleBadgeLabels[moduleIndex], "init");
  setDashboardLabelFont(ui.moduleBadgeLabels[moduleIndex], &lv_font_montserrat_12);
  setBadgeStyle(ui.moduleBadgeLabels[moduleIndex], UI_COLOR_BADGE_OFFLINE_BG, UI_COLOR_TEXT_SECONDARY);

  return topRow;
}

static void createStandardModuleCard(lv_obj_t *tile, int moduleIndex,
                                     const char* defaultIcon,
                                     const lv_font_t* iconFont,
                                     const lv_font_t* valueFont,
                                     const lv_font_t* metaFont,
                                     uint32_t valueColor,
                                     uint32_t metaColor,
                                     lv_coord_t metaHeight = 0) {
  createModuleTopRow(tile, moduleIndex);
  createDashboardSpacer(tile, 8);

  lv_obj_t *heroRow = lv_obj_create(tile);
  lv_obj_set_width(heroRow, lv_pct(100));
  lv_obj_set_height(heroRow, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(heroRow, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(heroRow, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(heroRow, 0, 0);
  lv_obj_set_style_pad_gap(heroRow, 8, 0);
  lv_obj_set_style_bg_opa(heroRow, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(heroRow, 0, 0);
  lv_obj_clear_flag(heroRow, LV_OBJ_FLAG_SCROLLABLE);

  if (defaultIcon != nullptr) {
    ui.moduleIconRefs[moduleIndex] = lv_label_create(heroRow);
    lv_label_set_text(ui.moduleIconRefs[moduleIndex], defaultIcon);
    setDashboardLabelFont(ui.moduleIconRefs[moduleIndex], iconFont);
    setDashboardLabelColor(ui.moduleIconRefs[moduleIndex], UI_COLOR_CARD_ICON_SOFT);
  } else {
    ui.moduleIconRefs[moduleIndex] = lv_img_create(heroRow);
    lv_img_set_src(ui.moduleIconRefs[moduleIndex], &IMG_WEATHER_CLOUD);
    lv_obj_clear_flag(ui.moduleIconRefs[moduleIndex], LV_OBJ_FLAG_SCROLLABLE);
  }

  ui.moduleValueLabels[moduleIndex] = lv_label_create(heroRow);
  lv_label_set_text(ui.moduleValueLabels[moduleIndex], MODULE_REGISTRY[moduleIndex].value);
  setDashboardLabelFont(ui.moduleValueLabels[moduleIndex], valueFont);
  setDashboardLabelColor(ui.moduleValueLabels[moduleIndex], valueColor);

  createDashboardSpacer(tile, 8);

  ui.moduleMetaLabels[moduleIndex] = lv_label_create(tile);
  lv_obj_set_width(ui.moduleMetaLabels[moduleIndex], lv_pct(100));
  if (metaHeight > 0) {
    lv_obj_set_height(ui.moduleMetaLabels[moduleIndex], metaHeight);
  }
  lv_label_set_text(ui.moduleMetaLabels[moduleIndex], MODULE_REGISTRY[moduleIndex].meta);
  lv_label_set_long_mode(ui.moduleMetaLabels[moduleIndex], LV_LABEL_LONG_WRAP);
  setDashboardLabelFont(ui.moduleMetaLabels[moduleIndex], metaFont);
  setDashboardLabelColor(ui.moduleMetaLabels[moduleIndex], metaColor);
}

static void createClockModuleCard(lv_obj_t *tile, int moduleIndex) {
  createStandardModuleCard(tile, moduleIndex, LV_SYMBOL_REFRESH, &lv_font_montserrat_20, &lv_font_montserrat_20, &lv_font_montserrat_12, UI_COLOR_TEXT_PRIMARY, UI_COLOR_TEXT_SECONDARY);
}

static void createPowerModuleCard(lv_obj_t *tile, int moduleIndex) {
  createStandardModuleCard(tile, moduleIndex, LV_SYMBOL_BATTERY_3, &lv_font_montserrat_20, &lv_font_montserrat_20, &lv_font_montserrat_12, UI_COLOR_TEXT_PRIMARY, UI_COLOR_TEXT_SECONDARY);
}

static void createWeatherModuleCard(lv_obj_t *tile, int moduleIndex) {
  createStandardModuleCard(tile, moduleIndex, nullptr, nullptr, &lv_font_montserrat_20, &lv_font_montserrat_12, UI_COLOR_TEXT_PRIMARY, UI_COLOR_TEXT_SECONDARY);
}

static void createNewsModuleCard(lv_obj_t *tile, int moduleIndex) {
  createStandardModuleCard(tile, moduleIndex, LV_SYMBOL_BELL, &lv_font_montserrat_16, &lv_font_montserrat_14, &lv_font_montserrat_14, UI_COLOR_ACCENT, UI_COLOR_TEXT_PRIMARY, 48);
}

static void createSettingsModuleCard(lv_obj_t *tile, int moduleIndex) {
  createStandardModuleCard(tile, moduleIndex, LV_SYMBOL_SETTINGS, &lv_font_montserrat_20, &lv_font_montserrat_16, &lv_font_montserrat_12, UI_COLOR_TEXT_PRIMARY, UI_COLOR_TEXT_SECONDARY, 52);
}

static void createModuleTileContent(lv_obj_t *tile, int moduleIndex) {
  styleModuleTile(tile, moduleIndex == app.currentModuleIndex);
  lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_flow(tile, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(tile, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(tile, 0, 0);
  lv_obj_set_height(tile, UI_MODULE_TILE_INNER_HEIGHT);

  if (moduleIndex >= 0 && moduleIndex < UI_MODULE_COUNT && MODULE_REGISTRY[moduleIndex].createCb) {
    MODULE_REGISTRY[moduleIndex].createCb(tile, moduleIndex);
  }
}

static void updateSettingsModuleCard() {
  if (ui.moduleValueLabels[0] == nullptr || ui.moduleMetaLabels[0] == nullptr) {
    return;
  }

  setDashboardLabelTextIfChanged(ui.moduleValueLabels[0], "Apri");
  setDashboardLabelTextIfChanged(ui.moduleMetaLabels[0], "Pianificazione energetica, luminosita, rete e hardware.");
  setModuleIconLabel(0, LV_SYMBOL_SETTINGS, UI_COLOR_CARD_ICON_SOFT);
  setModuleBadge(0, "soon", UI_COLOR_BADGE_OFFLINE_BG, UI_COLOR_TEXT_SECONDARY);
}

static void updateClockModuleCard() {
  if (ui.moduleValueLabels[1] == nullptr || ui.moduleMetaLabels[1] == nullptr) {
    return;
  }

  char metaBuffer[48];
  const bool online = wifiOnline();
  snprintf(metaBuffer, sizeof(metaBuffer), "%s | %s",
    online ? "wifi ok" : "wifi off",
    app.clock.synced ? "ntp ok" : "ntp retry");

  const char *clockText = strlen(app.clock.labelText) > 0 ? app.clock.labelText : "sync...";
  setDashboardLabelTextIfChanged(ui.moduleValueLabels[1], clockText);
  setDashboardLabelTextIfChanged(ui.moduleMetaLabels[1], metaBuffer);

  if (app.clock.synced) {
    setModuleIconLabel(1, LV_SYMBOL_OK, UI_COLOR_ACCENT);
    setModuleBadge(1, "ok", UI_COLOR_BADGE_OK_BG, UI_COLOR_ACCENT);
  } else if (online) {
    setModuleIconLabel(1, LV_SYMBOL_REFRESH, UI_COLOR_CARD_ICON_SOFT);
    setModuleBadge(1, "retry", UI_COLOR_BADGE_RETRY_BG, UI_COLOR_ACCENT);
  } else {
    setModuleIconLabel(1, LV_SYMBOL_WIFI, UI_COLOR_TEXT_MUTED);
    setModuleBadge(1, "sync", UI_COLOR_BADGE_OFFLINE_BG, UI_COLOR_TEXT_SECONDARY);
  }
}

static void updatePowerModuleCard() {
  if (ui.moduleValueLabels[2] == nullptr || ui.moduleMetaLabels[2] == nullptr) {
    return;
  }

  char valueBuffer[24];
  char metaBuffer[48];

    if (!app.battery.present || app.battery.percent < 0) {
    strlcpy(valueBuffer, "--", sizeof(valueBuffer));
    strlcpy(metaBuffer, "batteria assente o instabile", sizeof(metaBuffer));
    setModuleIconLabel(2, LV_SYMBOL_BATTERY_EMPTY, UI_COLOR_TEXT_MUTED);
    setModuleBadge(2, "probe", UI_COLOR_BADGE_OFFLINE_BG, UI_COLOR_TEXT_SECONDARY);
  } else {
    snprintf(valueBuffer, sizeof(valueBuffer), "%d%%", app.battery.percent);
    snprintf(metaBuffer, sizeof(metaBuffer), "%.2fV filtrata", app.battery.voltage);

      if (app.battery.percent >= 85) {
      setModuleIconLabel(2, LV_SYMBOL_BATTERY_FULL, UI_COLOR_ACCENT);
      setModuleBadge(2, "ok", UI_COLOR_BADGE_OK_BG, UI_COLOR_ACCENT);
    } else if (app.battery.percent >= 25) {
      setModuleIconLabel(2, LV_SYMBOL_BATTERY_2, UI_COLOR_CARD_ICON_SOFT);
      setModuleBadge(2, "watch", UI_COLOR_BADGE_RETRY_BG, UI_COLOR_ACCENT);
    } else {
      setModuleIconLabel(2, LV_SYMBOL_BATTERY_1, UI_COLOR_ACCENT);
      setModuleBadge(2, "low", UI_COLOR_BADGE_ALERT_BG, UI_COLOR_ACCENT);
    }
  }

  setDashboardLabelTextIfChanged(ui.moduleValueLabels[2], valueBuffer);
  setDashboardLabelTextIfChanged(ui.moduleMetaLabels[2], metaBuffer);
}

static void updateWeatherModuleCard() {
  if (ui.moduleValueLabels[3] == nullptr || ui.moduleMetaLabels[3] == nullptr || ui.moduleIconRefs[3] == nullptr) {
    return;
  }

  char valueBuffer[24];
  char metaBuffer[64];

  if (app.weather.state == SERVICE_FETCH_READY && app.weather.valid) {
    snprintf(valueBuffer, sizeof(valueBuffer), "%dC", app.weather.temperatureC);
    snprintf(metaBuffer, sizeof(metaBuffer), "%s | feed live", WEATHER_CITY_LABEL);
  } else {
    strlcpy(valueBuffer, "--", sizeof(valueBuffer));
    switch (app.weather.state) {
      case SERVICE_FETCH_OFFLINE:
        snprintf(metaBuffer, sizeof(metaBuffer), "%s | Wi-Fi assente", WEATHER_CITY_LABEL);
        break;
      case SERVICE_FETCH_CONFIG_MISSING:
        snprintf(metaBuffer, sizeof(metaBuffer), "%s | API key assente", WEATHER_CITY_LABEL);
        break;
      case SERVICE_FETCH_TRANSPORT_ERROR:
        snprintf(metaBuffer, sizeof(metaBuffer), "%s | richiesta fallita", WEATHER_CITY_LABEL);
        break;
      case SERVICE_FETCH_HTTP_ERROR:
        snprintf(metaBuffer, sizeof(metaBuffer), "%s | HTTP %d", WEATHER_CITY_LABEL, app.weather.lastHttpCode);
        break;
      case SERVICE_FETCH_INVALID_PAYLOAD:
        snprintf(metaBuffer, sizeof(metaBuffer), "%s | payload non valido", WEATHER_CITY_LABEL);
        break;
      case SERVICE_FETCH_IDLE:
      default:
        snprintf(metaBuffer, sizeof(metaBuffer), "%s | attesa primo fetch", WEATHER_CITY_LABEL);
        break;
    }
  }

  const void *weatherIconSource = weatherCardImageSource();
  if (lastWeatherModuleIconSource != weatherIconSource) {
    setDashboardImageSourceIfChanged(ui.moduleIconRefs[3], weatherIconSource);
    lastWeatherModuleIconSource = weatherIconSource;
  }
  setDashboardLabelTextIfChanged(ui.moduleValueLabels[3], valueBuffer);
  setDashboardLabelTextIfChanged(ui.moduleMetaLabels[3], metaBuffer);
  setServiceBadgeFromState(3, app.weather.state);
}

static void updateNewsModuleCard() {
  if (ui.moduleValueLabels[4] == nullptr || ui.moduleMetaLabels[4] == nullptr) {
    return;
  }

  char valueBuffer[24];
  char headlineBuffer[UI_NEWS_PREVIEW_MAX_LEN];

  if (app.news.state == SERVICE_FETCH_READY) {
    snprintf(valueBuffer, sizeof(valueBuffer), "%d news", app.news.itemCount);
  } else if (app.news.itemCount > 0) {
    snprintf(valueBuffer, sizeof(valueBuffer), "%d cached", app.news.itemCount);
  } else {
    strlcpy(valueBuffer, "0 news", sizeof(valueBuffer));
  }

  switch (app.news.state) {
    case SERVICE_FETCH_READY:
      buildNewsPreview(headlineBuffer, sizeof(headlineBuffer));
      break;
    case SERVICE_FETCH_OFFLINE:
      strlcpy(headlineBuffer, "Feed fermo: Wi-Fi non connesso", sizeof(headlineBuffer));
      break;
    case SERVICE_FETCH_CONFIG_MISSING:
      strlcpy(headlineBuffer, "Feed fermo: URL o API key mancanti", sizeof(headlineBuffer));
      break;
    case SERVICE_FETCH_TRANSPORT_ERROR:
      strlcpy(headlineBuffer, "Feed fermo: webhook non raggiungibile", sizeof(headlineBuffer));
      break;
    case SERVICE_FETCH_HTTP_ERROR:
      snprintf(headlineBuffer, sizeof(headlineBuffer), "Feed fermo: HTTP %d", app.news.lastHttpCode);
      break;
    case SERVICE_FETCH_INVALID_PAYLOAD:
      strlcpy(headlineBuffer, "Feed fermo: payload news non valido", sizeof(headlineBuffer));
      break;
    case SERVICE_FETCH_IDLE:
    default:
      strlcpy(headlineBuffer, "Feed in attesa del primo aggiornamento", sizeof(headlineBuffer));
      break;
  }

  setDashboardLabelTextIfChanged(ui.moduleValueLabels[4], valueBuffer);
  setDashboardLabelTextIfChanged(ui.moduleMetaLabels[4], headlineBuffer);

  switch (app.news.state) {
    case SERVICE_FETCH_READY:
      setModuleIconLabel(4, LV_SYMBOL_BELL, UI_COLOR_CARD_ICON_SOFT);
      break;
    case SERVICE_FETCH_OFFLINE:
      setModuleIconLabel(4, LV_SYMBOL_WIFI, UI_COLOR_TEXT_MUTED);
      break;
    case SERVICE_FETCH_CONFIG_MISSING:
    case SERVICE_FETCH_HTTP_ERROR:
    case SERVICE_FETCH_INVALID_PAYLOAD:
      setModuleIconLabel(4, LV_SYMBOL_WARNING, UI_COLOR_ACCENT);
      break;
    case SERVICE_FETCH_TRANSPORT_ERROR:
    case SERVICE_FETCH_IDLE:
    default:
      setModuleIconLabel(4, LV_SYMBOL_REFRESH, UI_COLOR_CARD_ICON_SOFT);
      break;
  }
  setServiceBadgeFromState(4, app.news.state);
}

static void tileviewEventCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
    return;
  }

  lv_obj_t *activeTile = lv_tileview_get_tile_act(ui.tileview);
  for (int i = 0; i < UI_MODULE_COUNT; ++i) {
    if (ui.moduleTiles[i] == activeTile) {
      app.currentModuleIndex = i;
      markUiDirty(UI_DIRTY_MAIN_TILE_STATE);
      refreshDashboardUi();
      break;
    }
  }
}

void createDashboardMain(lv_obj_t *parent) {
  ui.tileview = lv_tileview_create(parent);
  lv_obj_set_size(ui.tileview, UI_MAIN_WIDTH, UI_MAIN_HEIGHT);
  lv_obj_set_style_pad_all(ui.tileview, 4, 0);
  lv_obj_set_scrollbar_mode(ui.tileview, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_scroll_dir(ui.tileview, LV_DIR_HOR);
  lv_obj_clear_flag(ui.tileview, LV_OBJ_FLAG_SCROLL_ELASTIC);
  lv_obj_add_flag(ui.tileview, LV_OBJ_FLAG_SCROLL_ONE);
  lv_obj_set_scroll_snap_x(ui.tileview, LV_SCROLL_SNAP_CENTER);
  lv_obj_set_scroll_snap_y(ui.tileview, LV_SCROLL_SNAP_CENTER);
  styleDashboardPanel(ui.tileview, UI_COLOR_MAIN_BG, UI_COLOR_MAIN_BORDER);

  for (int i = 0; i < UI_MODULE_COUNT; ++i) {
    lv_dir_t directions = i == 0 ? LV_DIR_RIGHT
      : (i == UI_MODULE_COUNT - 1 ? LV_DIR_LEFT : (LV_DIR_LEFT | LV_DIR_RIGHT));
    ui.moduleTiles[i] = lv_tileview_add_tile(ui.tileview, i, 0, directions);
    createModuleTileContent(ui.moduleTiles[i], i);
  }

  lv_obj_set_tile_id(ui.tileview, UI_DEFAULT_MODULE_INDEX, 0, LV_ANIM_OFF);

  lv_obj_add_event_cb(ui.tileview, tileviewEventCb, LV_EVENT_VALUE_CHANGED, NULL);

  createDashboardSpacer(parent, 5);

  ui.moduleDotsWrap = lv_obj_create(parent);
  lv_obj_set_size(ui.moduleDotsWrap, 88, 12);
  lv_obj_set_flex_flow(ui.moduleDotsWrap, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(ui.moduleDotsWrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(ui.moduleDotsWrap, 0, 0);
  lv_obj_set_style_pad_gap(ui.moduleDotsWrap, 6, 0);
  lv_obj_set_style_bg_opa(ui.moduleDotsWrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(ui.moduleDotsWrap, 0, 0);
  lv_obj_clear_flag(ui.moduleDotsWrap, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < UI_MODULE_COUNT; ++i) {
    if (i == 0) {
      ui.moduleDots[i] = lv_label_create(ui.moduleDotsWrap);
      lv_label_set_text(ui.moduleDots[i], LV_SYMBOL_SETTINGS);
      setDashboardLabelFont(ui.moduleDots[i], &lv_font_montserrat_12);
      setDashboardLabelColor(ui.moduleDots[i], UI_COLOR_DOT_IDLE);
      continue;
    }

    ui.moduleDots[i] = lv_obj_create(ui.moduleDotsWrap);
  }

  createDashboardSpacer(parent, 5);
}

void refreshDashboardMainUi(uint32_t dirtyMask) {
  for (int i = 0; i < UI_MODULE_COUNT; ++i) {
    if ((dirtyMask & MODULE_REGISTRY[i].dirtyMask) != 0 && MODULE_REGISTRY[i].updateCb) {
      MODULE_REGISTRY[i].updateCb();
    }
  }

  if ((dirtyMask & UI_DIRTY_MAIN_TILE_STATE) != 0) {
    updateModuleTileStates();
    updateModuleDots();
  }
}
