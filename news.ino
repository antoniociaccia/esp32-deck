#include "display.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "secrets.h"
#include "weather_icons.h"

Display screen;

static lv_obj_t *timeLabel = nullptr;
static lv_obj_t *weatherIcon = nullptr;
static lv_obj_t *weatherLabel = nullptr;
static lv_obj_t *wifiLabel = nullptr;
static lv_obj_t *batteryPercentLabel = nullptr;
static lv_obj_t *batteryVoltageLabel = nullptr;
static lv_obj_t *batteryIconLabel = nullptr;
static lv_obj_t *moduleDotsWrap = nullptr;
static lv_obj_t *newsLabel = nullptr;
static lv_obj_t *tileview = nullptr;
static lv_obj_t *moduleTiles[4] = {nullptr};
static lv_obj_t *moduleDots[4] = {nullptr};

static unsigned long lastClockUpdateMs = 0;
static unsigned long lastTimeSyncAttemptMs = 0;
static unsigned long lastBatteryUpdateMs = 0;
static unsigned long lastWeatherUpdateMs = 0;
static unsigned long lastNewsFetchMs = 0;
static int currentModuleIndex = 0;
static bool timeSynced = false;
static float filteredBatteryVoltage = 0.0f;
static bool batteryInitialized = false;
static bool weatherValid = false;
static bool newsValid = false;

static const lv_coord_t HEADER_W = 314;
static const lv_coord_t HEADER_H = 30;
static const lv_coord_t MAIN_W = 314;
static const lv_coord_t MAIN_H = 136;
static const lv_coord_t FOOTER_W = 314;
static const lv_coord_t FOOTER_H = 40;
static const lv_coord_t PANEL_RADIUS = 4;

static const int BAT_PIN = 9;
static constexpr float BATTERY_DIVIDER_RATIO = 2.0f;
static constexpr float BATTERY_CALIBRATION_FACTOR = 1.00f;
static constexpr int BATTERY_SAMPLE_COUNT = 12;
static constexpr float BATTERY_FILTER_ALPHA = 0.15f;
static constexpr float BATTERY_MAX_STEP_VOLTS = 0.20f;
static constexpr float BATTERY_EMPTY_VOLTS = 3.30f;
static constexpr float BATTERY_FULL_VOLTS = 4.20f;
static constexpr unsigned long BATTERY_REFRESH_INTERVAL_MS = 2000;
static constexpr unsigned long WEATHER_REFRESH_INTERVAL_MS = 1800000;
static constexpr unsigned long WEATHER_RETRY_INTERVAL_MS = 30000;
static constexpr unsigned long NEWS_REFRESH_INTERVAL_MS = 900000;
static constexpr unsigned long NEWS_RETRY_INTERVAL_MS = 60000;

static const char *DEFAULT_NEWS_ITEMS[] = {
  "IT | Dashboard pronta per dati reali",
  "TECH | Swipe orizzontale tra i widget",
  "WORLD | Prossimo step: news, meteo e batteria"
};

static const int DEFAULT_NEWS_ITEM_COUNT = sizeof(DEFAULT_NEWS_ITEMS) / sizeof(DEFAULT_NEWS_ITEMS[0]);
static const int MAX_NEWS_ITEMS = 12;
static const int MAX_NEWS_TEXT_LEN = 256;
static const int MAX_TICKER_LEN = 3600;
static char newsItems[MAX_NEWS_ITEMS][MAX_NEWS_TEXT_LEN];
static char newsTicker[MAX_TICKER_LEN];
static int newsItemCount = 0;

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

int extractJsonIntAfterKey(const String &payload, const char *key, int fallbackValue) {
  int keyIndex = payload.indexOf(key);
  if (keyIndex < 0) {
    return fallbackValue;
  }

  int colonIndex = payload.indexOf(':', keyIndex);
  if (colonIndex < 0) {
    return fallbackValue;
  }

  int start = colonIndex + 1;
  while (start < payload.length() && (payload[start] == ' ' || payload[start] == '\"')) {
    start++;
  }

  int end = start;
  while (end < payload.length() && (isDigit(payload[end]) || payload[end] == '-')) {
    end++;
  }

  if (end <= start) {
    return fallbackValue;
  }

  return payload.substring(start, end).toInt();
}

String extractJsonStringAfterKey(const String &payload, const char *key, const char *fallbackValue) {
  int keyIndex = payload.indexOf(key);
  if (keyIndex < 0) {
    return String(fallbackValue);
  }

  int colonIndex = payload.indexOf(':', keyIndex);
  if (colonIndex < 0) {
    return String(fallbackValue);
  }

  int startQuote = payload.indexOf('\"', colonIndex + 1);
  if (startQuote < 0) {
    return String(fallbackValue);
  }

  int endQuote = payload.indexOf('\"', startQuote + 1);
  if (endQuote < 0 || endQuote <= startQuote) {
    return String(fallbackValue);
  }

  return payload.substring(startQuote + 1, endQuote);
}

String decodeJsonString(const String &value) {
  String decoded;
  decoded.reserve(value.length());

  for (int i = 0; i < value.length(); ++i) {
    char c = value[i];
    if (c == '\\' && i + 1 < value.length()) {
      char next = value[++i];
      switch (next) {
        case '\"': decoded += '\"'; break;
        case '\\': decoded += '\\'; break;
        case '/': decoded += '/'; break;
        case 'b': decoded += ' '; break;
        case 'f': decoded += ' '; break;
        case 'n': decoded += ' '; break;
        case 'r': decoded += ' '; break;
        case 't': decoded += ' '; break;
        default: decoded += next; break;
      }
    } else {
      decoded += c;
    }
  }

  decoded.trim();
  return decoded;
}

void normalizeNewsText(String &text) {
  text.replace("\n", " ");
  text.replace("\r", " ");
  while (text.indexOf("  ") >= 0) {
    text.replace("  ", " ");
  }
  text.trim();
}

void rebuildNewsTicker() {
  newsTicker[0] = '\0';
  for (int i = 0; i < newsItemCount; ++i) {
    if (i > 0) {
      strlcat(newsTicker, "     •     ", MAX_TICKER_LEN);
    }
    strlcat(newsTicker, newsItems[i], MAX_TICKER_LEN);
  }

  if (newsLabel != nullptr) {
    lv_label_set_text(newsLabel, newsTicker);
  }
}

void setDefaultNewsItems() {
  newsItemCount = DEFAULT_NEWS_ITEM_COUNT;
  for (int i = 0; i < DEFAULT_NEWS_ITEM_COUNT; ++i) {
    strlcpy(newsItems[i], DEFAULT_NEWS_ITEMS[i], MAX_NEWS_TEXT_LEN);
  }
  rebuildNewsTicker();
}

bool parseNewsItems(const String &payload) {
  int parsedCount = 0;
  int searchFrom = 0;

  while (parsedCount < MAX_NEWS_ITEMS) {
    int textKey = payload.indexOf("\"text\"", searchFrom);
    if (textKey < 0) {
      break;
    }

    int colonIndex = payload.indexOf(':', textKey);
    int startQuote = payload.indexOf('\"', colonIndex + 1);
    if (colonIndex < 0 || startQuote < 0) {
      break;
    }

    bool escaped = false;
    int endQuote = -1;
    for (int i = startQuote + 1; i < payload.length(); ++i) {
      char c = payload[i];
      if (c == '\\' && !escaped) {
        escaped = true;
        continue;
      }
      if (c == '\"' && !escaped) {
        endQuote = i;
        break;
      }
      escaped = false;
    }

    if (endQuote < 0) {
      break;
    }

    String decoded = decodeJsonString(payload.substring(startQuote + 1, endQuote));
    normalizeNewsText(decoded);
    if (decoded.length() > 0) {
      strlcpy(newsItems[parsedCount], decoded.c_str(), MAX_NEWS_TEXT_LEN);
      parsedCount++;
    }

    searchFrom = endQuote + 1;
  }

  if (parsedCount <= 0) {
    return false;
  }

  newsItemCount = parsedCount;
  rebuildNewsTicker();
  return true;
}

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

float clampBatteryVoltage(float value) {
  if (value < BATTERY_EMPTY_VOLTS) {
    return BATTERY_EMPTY_VOLTS;
  }
  if (value > BATTERY_FULL_VOLTS) {
    return BATTERY_FULL_VOLTS;
  }
  return value;
}

float interpolateSegment(float voltage, float v1, float p1, float v2, float p2) {
  if (v2 <= v1) {
    return p1;
  }

  float ratio = (voltage - v1) / (v2 - v1);
  return p1 + (ratio * (p2 - p1));
}

int batteryPercentFromVoltage(float voltage) {
  float clamped = clampBatteryVoltage(voltage);

  // Piecewise LiPo approximation for a 1-cell battery under light load.
  float percentFloat = 0.0f;
  if (clamped >= 4.15f) {
    percentFloat = interpolateSegment(clamped, 4.15f, 95.0f, 4.20f, 100.0f);
  } else if (clamped >= 4.08f) {
    percentFloat = interpolateSegment(clamped, 4.08f, 85.0f, 4.15f, 95.0f);
  } else if (clamped >= 4.00f) {
    percentFloat = interpolateSegment(clamped, 4.00f, 72.0f, 4.08f, 85.0f);
  } else if (clamped >= 3.92f) {
    percentFloat = interpolateSegment(clamped, 3.92f, 58.0f, 4.00f, 72.0f);
  } else if (clamped >= 3.85f) {
    percentFloat = interpolateSegment(clamped, 3.85f, 42.0f, 3.92f, 58.0f);
  } else if (clamped >= 3.78f) {
    percentFloat = interpolateSegment(clamped, 3.78f, 27.0f, 3.85f, 42.0f);
  } else if (clamped >= 3.68f) {
    percentFloat = interpolateSegment(clamped, 3.68f, 14.0f, 3.78f, 27.0f);
  } else if (clamped >= 3.55f) {
    percentFloat = interpolateSegment(clamped, 3.55f, 6.0f, 3.68f, 14.0f);
  } else {
    percentFloat = interpolateSegment(clamped, 3.30f, 0.0f, 3.55f, 6.0f);
  }

  int percent = (int)lroundf(percentFloat);
  if (percent < 0) {
    return 0;
  }
  if (percent > 100) {
    return 100;
  }
  return percent;
}

float readBatteryVoltage() {
  uint32_t totalMilliVolts = 0;
  for (int i = 0; i < BATTERY_SAMPLE_COUNT; ++i) {
    totalMilliVolts += analogReadMilliVolts(BAT_PIN);
    delay(2);
  }

  float averageMilliVolts = totalMilliVolts / (float)BATTERY_SAMPLE_COUNT;
  return (averageMilliVolts / 1000.0f) * BATTERY_DIVIDER_RATIO * BATTERY_CALIBRATION_FACTOR;
}

void initBatteryMonitoring() {
  analogSetPinAttenuation(BAT_PIN, ADC_11db);
}

void updateBatteryUi() {
  if (millis() - lastBatteryUpdateMs < BATTERY_REFRESH_INTERVAL_MS) {
    return;
  }

  lastBatteryUpdateMs = millis();

  float measuredVoltage = readBatteryVoltage();
  if (!batteryInitialized) {
    filteredBatteryVoltage = measuredVoltage;
    batteryInitialized = true;
  } else {
    float delta = measuredVoltage - filteredBatteryVoltage;
    if (delta > BATTERY_MAX_STEP_VOLTS) {
      measuredVoltage = filteredBatteryVoltage + BATTERY_MAX_STEP_VOLTS;
    } else if (delta < -BATTERY_MAX_STEP_VOLTS) {
      measuredVoltage = filteredBatteryVoltage - BATTERY_MAX_STEP_VOLTS;
    }

    filteredBatteryVoltage = (BATTERY_FILTER_ALPHA * measuredVoltage) +
      ((1.0f - BATTERY_FILTER_ALPHA) * filteredBatteryVoltage);
  }

  int batteryPercent = batteryPercentFromVoltage(filteredBatteryVoltage);
  char percentBuffer[12];
  snprintf(percentBuffer, sizeof(percentBuffer), "%d%%", batteryPercent);
  lv_label_set_text(batteryPercentLabel, percentBuffer);

  char voltageBuffer[12];
  snprintf(voltageBuffer, sizeof(voltageBuffer), "%.2fV", filteredBatteryVoltage);
  lv_label_set_text(batteryVoltageLabel, voltageBuffer);

  if (batteryPercent >= 85) {
    lv_label_set_text(batteryIconLabel, LV_SYMBOL_BATTERY_FULL);
  } else if (batteryPercent >= 60) {
    lv_label_set_text(batteryIconLabel, LV_SYMBOL_BATTERY_3);
  } else if (batteryPercent >= 35) {
    lv_label_set_text(batteryIconLabel, LV_SYMBOL_BATTERY_2);
  } else if (batteryPercent >= 15) {
    lv_label_set_text(batteryIconLabel, LV_SYMBOL_BATTERY_1);
  } else {
    lv_label_set_text(batteryIconLabel, LV_SYMBOL_BATTERY_EMPTY);
  }
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

void updateNewsFeed() {
  unsigned long refreshInterval = newsValid ? NEWS_REFRESH_INTERVAL_MS : NEWS_RETRY_INTERVAL_MS;
  if (millis() - lastNewsFetchMs < refreshInterval) {
    return;
  }

  lastNewsFetchMs = millis();

  if (WiFi.status() != WL_CONNECTED) {
    newsValid = false;
    return;
  }

  if (strlen(NEWS_API_URL) == 0 || strlen(NEWS_API_KEY) == 0) {
    newsValid = false;
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, NEWS_API_URL)) {
    newsValid = false;
    return;
  }

  http.addHeader("X-API-Key", NEWS_API_KEY);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    newsValid = false;
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  newsValid = parseNewsItems(payload);
}

void updateWeatherIconUi(const String &iconCode) {
  if (weatherIcon == nullptr) {
    return;
  }

  if (iconCode.startsWith("01")) {
    lv_img_set_src(weatherIcon, &IMG_WEATHER_SUN);
  } else if (iconCode.startsWith("02") || iconCode.startsWith("03") || iconCode.startsWith("04")) {
    lv_img_set_src(weatherIcon, &IMG_WEATHER_CLOUD);
  } else if (iconCode.startsWith("09") || iconCode.startsWith("10")) {
    lv_img_set_src(weatherIcon, &IMG_WEATHER_RAIN);
  } else if (iconCode.startsWith("11")) {
    lv_img_set_src(weatherIcon, &IMG_WEATHER_STORM);
  } else if (iconCode.startsWith("13")) {
    lv_img_set_src(weatherIcon, &IMG_WEATHER_SNOW);
  } else if (iconCode.startsWith("50")) {
    lv_img_set_src(weatherIcon, &IMG_WEATHER_FOG);
  } else {
    lv_img_set_src(weatherIcon, &IMG_WEATHER_CLOUD);
  }
}

void updateWeatherUi() {
  unsigned long refreshInterval = weatherValid ? WEATHER_REFRESH_INTERVAL_MS : WEATHER_RETRY_INTERVAL_MS;
  if (millis() - lastWeatherUpdateMs < refreshInterval) {
    return;
  }

  lastWeatherUpdateMs = millis();

  if (WiFi.status() != WL_CONNECTED) {
    weatherValid = false;
    lv_label_set_text(weatherLabel, "meteo offline");
    return;
  }

  if (strlen(OPENWEATHER_API_KEY) == 0) {
    weatherValid = false;
    lv_label_set_text(weatherLabel, "meteo n/d");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = "https://api.openweathermap.org/data/2.5/weather?lat=";
  url += String(WEATHER_LATITUDE, 4);
  url += "&lon=";
  url += String(WEATHER_LONGITUDE, 4);
  url += "&units=metric&lang=it&appid=";
  url += OPENWEATHER_API_KEY;

  if (!http.begin(client, url)) {
    weatherValid = false;
    lv_label_set_text(weatherLabel, "meteo errore");
    return;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    weatherValid = false;
    lv_label_set_text(weatherLabel, "meteo errore");
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  int temperature = extractJsonIntAfterKey(payload, "\"temp\"", 0);
  String iconCode = extractJsonStringAfterKey(payload, "\"icon\"", "");
  char weatherBuffer[32];
  snprintf(weatherBuffer, sizeof(weatherBuffer), "%s %dC", WEATHER_CITY_LABEL, temperature);
  lv_label_set_text(weatherLabel, weatherBuffer);
  updateWeatherIconUi(iconCode);
  weatherValid = true;
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
  lv_obj_set_scrollbar_mode(header, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(header, LV_OBJ_FLAG_SCROLLABLE);
  stylePanel(header, lv_color_hex(0x18344A), lv_color_hex(0x28485E));

  timeLabel = lv_label_create(header);
  lv_label_set_text(timeLabel, "sync orario...");
  setLabelFont(timeLabel, &lv_font_montserrat_14);
  setLabelColor(timeLabel, lv_color_hex(0xF8FAFC));
  lv_obj_align(timeLabel, LV_ALIGN_LEFT_MID, 0, 0);

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

  weatherIcon = lv_img_create(weatherWrap);
  lv_img_set_src(weatherIcon, &IMG_WEATHER_SUN);
  lv_obj_clear_flag(weatherIcon, LV_OBJ_FLAG_SCROLLABLE);

  weatherLabel = lv_label_create(weatherWrap);
  lv_label_set_text(weatherLabel, "Roma 18C");
  setLabelFont(weatherLabel, &lv_font_montserrat_14);
  setLabelColor(weatherLabel, lv_color_hex(0xCFE8FF));

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

  wifiLabel = lv_label_create(rightWrap);
  lv_label_set_text(wifiLabel, LV_SYMBOL_WIFI);
  setLabelFont(wifiLabel, &lv_font_montserrat_12);
  setLabelColor(wifiLabel, lv_color_hex(0x94A3B8));

  lv_obj_t *batteryTextWrap = lv_obj_create(rightWrap);
  lv_obj_set_size(batteryTextWrap, 34, 20);
  lv_obj_set_flex_flow(batteryTextWrap, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(batteryTextWrap, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_set_style_pad_all(batteryTextWrap, 0, 0);
  lv_obj_set_style_pad_row(batteryTextWrap, 0, 0);
  lv_obj_set_style_bg_opa(batteryTextWrap, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(batteryTextWrap, 0, 0);
  lv_obj_clear_flag(batteryTextWrap, LV_OBJ_FLAG_SCROLLABLE);

  batteryPercentLabel = lv_label_create(batteryTextWrap);
  lv_label_set_text(batteryPercentLabel, "--%");
  setLabelFont(batteryPercentLabel, &lv_font_montserrat_10);
  setLabelColor(batteryPercentLabel, lv_color_hex(0xC7F9CC));
  lv_obj_set_width(batteryPercentLabel, 34);
  lv_obj_set_style_text_align(batteryPercentLabel, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

  batteryVoltageLabel = lv_label_create(batteryTextWrap);
  lv_label_set_text(batteryVoltageLabel, "--.--V");
  setLabelFont(batteryVoltageLabel, &lv_font_montserrat_10);
  setLabelColor(batteryVoltageLabel, lv_color_hex(0xC7F9CC));
  lv_obj_set_width(batteryVoltageLabel, 34);
  lv_obj_set_style_text_align(batteryVoltageLabel, LV_TEXT_ALIGN_RIGHT, LV_PART_MAIN);

  batteryIconLabel = lv_label_create(rightWrap);
  lv_label_set_text(batteryIconLabel, LV_SYMBOL_BATTERY_3);
  setLabelFont(batteryIconLabel, &lv_font_montserrat_12);
  setLabelColor(batteryIconLabel, lv_color_hex(0xC7F9CC));
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
  lv_label_set_text(newsLabel, newsTicker);
  setLabelFont(newsLabel, &lv_font_montserrat_16);
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
  setDefaultNewsItems();
  createDashboardUi();
  initBatteryMonitoring();
  beginTimeSync();

  Serial.println("Desk dashboard LVGL");
  Serial.println("Setup done");
}

void loop() {
  screen.routine();
  maintainTimeSync();
  updateClockUi();
  updateBatteryUi();
  updateWeatherUi();
  updateNewsFeed();
  delay(5);
}
