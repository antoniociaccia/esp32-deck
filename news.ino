#include "display.h"
#include <WiFi.h>
#include <time.h>
#include "secrets.h"

Display screen;

static lv_obj_t *timeLabel = nullptr;
static lv_obj_t *weatherLabel = nullptr;
static lv_obj_t *batteryLabel = nullptr;
static lv_obj_t *wifiLabel = nullptr;
static lv_obj_t *moduleDotsWrap = nullptr;
static lv_obj_t *newsLabel = nullptr;
static lv_obj_t *tileview = nullptr;
static lv_obj_t *moduleTiles[4] = {nullptr};
static lv_obj_t *moduleDots[4] = {nullptr};

static unsigned long lastClockUpdateMs = 0;
static unsigned long lastNewsRotateMs = 0;
static unsigned long lastTimeSyncAttemptMs = 0;
static int currentNewsIndex = 0;
static int currentModuleIndex = 0;
static bool timeSynced = false;

static const lv_coord_t HEADER_W = 314;
static const lv_coord_t HEADER_H = 30;
static const lv_coord_t MAIN_W = 314;
static const lv_coord_t MAIN_H = 136;
static const lv_coord_t FOOTER_W = 314;
static const lv_coord_t FOOTER_H = 40;
static const lv_coord_t PANEL_RADIUS = 4;

static const char *NEWS_ITEMS[] = {
  "IT | Dashboard pronta per dati reali",
  "TECH | Swipe orizzontale tra i widget",
  "WORLD | Prossimo step: news, meteo e batteria"
};

static const int NEWS_ITEM_COUNT = sizeof(NEWS_ITEMS) / sizeof(NEWS_ITEMS[0]);

struct ModuleContent {
  const char *title;
  const char *value;
  const char *meta;
};

static const ModuleContent MODULES[] = {
  {"Pomodoro", "24:52", "focus session in corso"},
  {"Email", "3 nuove", "2 urgenti e 1 follow-up"},
  {"Meteo", "18C", "Roma, cielo sereno"},
  {"Touch", "OK", "swipe destra o sinistra"}
};

static const int MODULE_COUNT = sizeof(MODULES) / sizeof(MODULES[0]);

void setLabelFont(lv_obj_t *label, const lv_font_t *font) {
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
}

void setLabelColor(lv_obj_t *label, lv_color_t color) {
  lv_obj_set_style_text_color(label, color, LV_PART_MAIN);
}

void updateWifiUi() {
  if (wifiLabel == nullptr) {
    return;
  }

  lv_label_set_text(wifiLabel, LV_SYMBOL_WIFI);
  if (WiFi.status() == WL_CONNECTED) {
    setLabelColor(wifiLabel, lv_color_hex(0x86EFAC));
  } else {
    setLabelColor(wifiLabel, lv_color_hex(0x94A3B8));
  }
}

void stylePanel(lv_obj_t *panel, lv_color_t bgColor, lv_color_t borderColor) {
  lv_obj_set_style_radius(panel, PANEL_RADIUS, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_border_color(panel, borderColor, 0);
  lv_obj_set_style_bg_color(panel, bgColor, 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(panel, 0, 0);
}

void createSpacer(lv_obj_t *parent, lv_coord_t height) {
  lv_obj_t *spacer = lv_obj_create(parent);
  lv_obj_set_size(spacer, 1, height);
  lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(spacer, 0, 0);
  lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);
}

void updateModuleDots() {
  for (int i = 0; i < MODULE_COUNT; ++i) {
    if (moduleDots[i] == nullptr) {
      continue;
    }

    lv_obj_set_size(moduleDots[i], i == currentModuleIndex ? 14 : 8, 8);
    lv_obj_set_style_bg_color(moduleDots[i],
      i == currentModuleIndex ? lv_color_hex(0x8A3B12) : lv_color_hex(0xC6B8AA), 0);
    lv_obj_set_style_bg_opa(moduleDots[i], LV_OPA_COVER, 0);
    lv_obj_set_style_border_width(moduleDots[i], 0, 0);
    lv_obj_set_style_radius(moduleDots[i], 4, 0);
  }
}

void updateModuleUi() {
  updateModuleDots();
}

void updateClockUi() {
  if (millis() - lastClockUpdateMs < 1000) {
    return;
  }

  lastClockUpdateMs = millis();

  char timeBuffer[24];
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d/%02d %02d:%02d",
      timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_hour, timeinfo.tm_min);
    timeSynced = true;
  } else {
    snprintf(timeBuffer, sizeof(timeBuffer), "sync orario...");
  }
  lv_label_set_text(timeLabel, timeBuffer);
}

void updateNewsUi() {
  if (millis() - lastNewsRotateMs < 5000) {
    return;
  }

  lastNewsRotateMs = millis();
  currentNewsIndex = (currentNewsIndex + 1) % NEWS_ITEM_COUNT;
  lv_label_set_text(newsLabel, NEWS_ITEMS[currentNewsIndex]);
}

void beginTimeSync() {
  if (strlen(WIFI_SSID) == 0) {
    Serial.println("WiFi non configurato: imposta WIFI_SSID e WIFI_PASSWORD");
    lv_label_set_text(timeLabel, "config WiFi");
    updateWifiUi();
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    Serial.println("Connessione WiFi per NTP...");
  }

  configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
  lastTimeSyncAttemptMs = millis();
  updateWifiUi();
}

void maintainTimeSync() {
  updateWifiUi();

  if (timeSynced) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - lastTimeSyncAttemptMs > 10000) {
      beginTimeSync();
    }
    return;
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    timeSynced = true;
    Serial.println("Orario NTP sincronizzato");
    updateClockUi();
    return;
  }

  if (millis() - lastTimeSyncAttemptMs > 15000) {
    Serial.println("Ritento sync NTP");
    configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
    lastTimeSyncAttemptMs = millis();
  }
}

void createHeader(lv_obj_t *parent) {
  lv_obj_t *header = lv_obj_create(parent);
  lv_obj_set_size(header, HEADER_W, HEADER_H);
  lv_obj_set_style_pad_hor(header, 8, 0);
  lv_obj_set_style_pad_ver(header, 3, 0);
  stylePanel(header, lv_color_hex(0x18344A), lv_color_hex(0x28485E));

  timeLabel = lv_label_create(header);
  lv_label_set_text(timeLabel, "sync orario...");
  setLabelFont(timeLabel, &lv_font_montserrat_14);
  setLabelColor(timeLabel, lv_color_hex(0xF8FAFC));
  lv_obj_align(timeLabel, LV_ALIGN_LEFT_MID, 0, 0);

  weatherLabel = lv_label_create(header);
  lv_label_set_text(weatherLabel, "Roma 18C");
  setLabelFont(weatherLabel, &lv_font_montserrat_14);
  setLabelColor(weatherLabel, lv_color_hex(0xCFE8FF));
  lv_obj_align(weatherLabel, LV_ALIGN_CENTER, 0, 0);

  wifiLabel = lv_label_create(header);
  lv_label_set_text(wifiLabel, LV_SYMBOL_WIFI);
  setLabelFont(wifiLabel, &lv_font_montserrat_14);
  setLabelColor(wifiLabel, lv_color_hex(0x94A3B8));
  lv_obj_align(wifiLabel, LV_ALIGN_RIGHT_MID, -52, 0);

  batteryLabel = lv_label_create(header);
  lv_label_set_text(batteryLabel, "BAT 100%");
  setLabelFont(batteryLabel, &lv_font_montserrat_14);
  setLabelColor(batteryLabel, lv_color_hex(0xC7F9CC));
  lv_obj_align(batteryLabel, LV_ALIGN_RIGHT_MID, 0, 0);
}

void tileviewEventCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_VALUE_CHANGED) {
    return;
  }

  lv_obj_t *activeTile = lv_tileview_get_tile_act(tileview);
  for (int i = 0; i < MODULE_COUNT; ++i) {
    if (moduleTiles[i] == activeTile) {
      currentModuleIndex = i;
      updateModuleUi();
      break;
    }
  }
}

void createModuleTileContent(lv_obj_t *tile, const ModuleContent &module) {
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
  setLabelColor(title, lv_color_hex(0x8A3B12));

  createSpacer(tile, 22);

  lv_obj_t *value = lv_label_create(tile);
  lv_label_set_text(value, module.value);
  setLabelFont(value, &lv_font_montserrat_20);
  setLabelColor(value, lv_color_hex(0x111827));
  lv_obj_set_style_text_align(value, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  lv_obj_set_width(value, MAIN_W - 28);

  createSpacer(tile, 26);

  lv_obj_t *meta = lv_label_create(tile);
  lv_obj_set_width(meta, MAIN_W - 28);
  lv_obj_set_style_text_align(meta, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_label_set_text(meta, module.meta);
  setLabelFont(meta, &lv_font_montserrat_14);
  setLabelColor(meta, lv_color_hex(0x475569));
}

void createMain(lv_obj_t *parent) {
  tileview = lv_tileview_create(parent);
  lv_obj_set_size(tileview, MAIN_W, MAIN_H);
  lv_obj_set_style_pad_all(tileview, 0, 0);
  lv_obj_set_scrollbar_mode(tileview, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(tileview, LV_OBJ_FLAG_SCROLL_ELASTIC);
  stylePanel(tileview, lv_color_hex(0xFFF8EF), lv_color_hex(0xE7D8C7));

  for (int i = 0; i < MODULE_COUNT; ++i) {
    moduleTiles[i] = lv_tileview_add_tile(tileview, i, 0, i == 0 ? LV_DIR_RIGHT : (i == MODULE_COUNT - 1 ? LV_DIR_LEFT : (LV_DIR_LEFT | LV_DIR_RIGHT)));
    createModuleTileContent(moduleTiles[i], MODULES[i]);
  }

  lv_obj_add_event_cb(tileview, tileviewEventCb, LV_EVENT_VALUE_CHANGED, NULL);

  createSpacer(parent, 5);

  moduleDotsWrap = lv_obj_create(parent);
  lv_obj_set_size(moduleDotsWrap, 72, 10);
  lv_obj_set_flex_flow(moduleDotsWrap, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(moduleDotsWrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(moduleDotsWrap, 0, 0);
  lv_obj_set_style_pad_gap(moduleDotsWrap, 6, 0);
  lv_obj_set_style_bg_opa(moduleDotsWrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(moduleDotsWrap, 0, 0);
  lv_obj_clear_flag(moduleDotsWrap, LV_OBJ_FLAG_SCROLLABLE);

  for (int i = 0; i < MODULE_COUNT; ++i) {
    moduleDots[i] = lv_obj_create(moduleDotsWrap);
  }

  createSpacer(parent, 5);

  updateModuleUi();
}

void createFooter(lv_obj_t *parent) {
  lv_obj_t *footer = lv_obj_create(parent);
  lv_obj_set_size(footer, FOOTER_W, FOOTER_H);
  lv_obj_set_style_pad_hor(footer, 10, 0);
  lv_obj_set_style_pad_ver(footer, 5, 0);
  lv_obj_set_scrollbar_mode(footer, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
  stylePanel(footer, lv_color_hex(0x17344A), lv_color_hex(0x28485E));

  newsLabel = lv_label_create(footer);
  lv_obj_set_width(newsLabel, FOOTER_W - 20);
  lv_obj_set_style_text_align(newsLabel, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_label_set_long_mode(newsLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_scrollbar_mode(newsLabel, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(newsLabel, LV_OBJ_FLAG_SCROLLABLE);
  lv_label_set_text(newsLabel, NEWS_ITEMS[currentNewsIndex]);
  setLabelFont(newsLabel, &lv_font_montserrat_14);
  setLabelColor(newsLabel, lv_color_hex(0xE2E8F0));
  lv_obj_align(newsLabel, LV_ALIGN_LEFT_MID, 0, 0);
}

void createDashboardUi() {
  lv_obj_t *screenRoot = lv_scr_act();
  lv_obj_set_style_bg_color(screenRoot, lv_color_hex(0xD7E6F1), 0);
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

void setup() {
  Serial.begin(115200);

  screen.init();
  createDashboardUi();
  beginTimeSync();

  Serial.println("Desk dashboard LVGL");
  Serial.println("Setup done");
}

void loop() {
  screen.routine();
  maintainTimeSync();
  updateClockUi();
  updateNewsUi();
  delay(5);
}
