#include "dashboard_services.h"
#include "dashboard_app.h"
#include "dashboard_support.h"
#include "config_debug.h"
#include "config_timing.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "secrets.h"

void updateClockUi() {
  if (!intervalElapsed(app.lastClockUpdateMs, TIMING_CLOCK_REFRESH_MS)) {
    return;
  }

  char previousClockText[sizeof(app.clockLabelText)];
  strlcpy(previousClockText, app.clockLabelText, sizeof(previousClockText));
  bool previousTimeSynced = app.timeSynced;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    snprintf(app.clockLabelText, sizeof(app.clockLabelText), "%02d/%02d %02d:%02d",
      timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_hour, timeinfo.tm_min);
    app.timeSynced = true;
  } else {
    snprintf(app.clockLabelText, sizeof(app.clockLabelText), "sync orario...");
  }

  if (strcmp(previousClockText, app.clockLabelText) != 0 || previousTimeSynced != app.timeSynced) {
    markUiDirty(UI_DIRTY_HEADER | UI_DIRTY_MAIN_CLOCK);
  }
}

static String buildWeatherUrl() {
  String url = "https://api.openweathermap.org/data/2.5/weather?lat=";
  url += String(WEATHER_LATITUDE, 4);
  url += "&lon=";
  url += String(WEATHER_LONGITUDE, 4);
  url += "&units=metric&lang=it&appid=";
  url += OPENWEATHER_API_KEY;
  return url;
}

void updateWeatherUi() {
  unsigned long refreshInterval = app.weatherValid ? TIMING_WEATHER_REFRESH_MS : TIMING_WEATHER_RETRY_MS;
  if (!intervalElapsed(app.lastWeatherUpdateMs, refreshInterval)) {
    return;
  }

  char previousWeatherText[sizeof(app.weatherLabelText)];
  char previousWeatherIconCode[sizeof(app.weatherIconCode)];
  strlcpy(previousWeatherText, app.weatherLabelText, sizeof(previousWeatherText));
  strlcpy(previousWeatherIconCode, app.weatherIconCode, sizeof(previousWeatherIconCode));
  bool previousWeatherValid = app.weatherValid;
  int previousWeatherTemperature = app.weatherTemperatureC;

  if (WiFi.status() != WL_CONNECTED) {
    app.weatherValid = false;
    app.weatherTemperatureC = 0;
    snprintf(app.weatherLabelText, sizeof(app.weatherLabelText), "meteo offline");
    strlcpy(app.weatherIconCode, "", sizeof(app.weatherIconCode));
    if (previousWeatherValid != app.weatherValid
      || previousWeatherTemperature != app.weatherTemperatureC
      || strcmp(previousWeatherText, app.weatherLabelText) != 0
      || strcmp(previousWeatherIconCode, app.weatherIconCode) != 0) {
      markUiDirty(UI_DIRTY_HEADER | UI_DIRTY_MAIN_WEATHER);
    }
    return;
  }

  if (strlen(OPENWEATHER_API_KEY) == 0) {
    app.weatherValid = false;
    app.weatherTemperatureC = 0;
    snprintf(app.weatherLabelText, sizeof(app.weatherLabelText), "meteo n/d");
    strlcpy(app.weatherIconCode, "", sizeof(app.weatherIconCode));
    if (previousWeatherValid != app.weatherValid
      || previousWeatherTemperature != app.weatherTemperatureC
      || strcmp(previousWeatherText, app.weatherLabelText) != 0
      || strcmp(previousWeatherIconCode, app.weatherIconCode) != 0) {
      markUiDirty(UI_DIRTY_HEADER | UI_DIRTY_MAIN_WEATHER);
    }
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, buildWeatherUrl())) {
    app.weatherValid = false;
    app.weatherTemperatureC = 0;
    snprintf(app.weatherLabelText, sizeof(app.weatherLabelText), "meteo errore");
    strlcpy(app.weatherIconCode, "", sizeof(app.weatherIconCode));
    if (previousWeatherValid != app.weatherValid
      || previousWeatherTemperature != app.weatherTemperatureC
      || strcmp(previousWeatherText, app.weatherLabelText) != 0
      || strcmp(previousWeatherIconCode, app.weatherIconCode) != 0) {
      markUiDirty(UI_DIRTY_HEADER | UI_DIRTY_MAIN_WEATHER);
    }
    return;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    app.weatherValid = false;
    app.weatherTemperatureC = 0;
    snprintf(app.weatherLabelText, sizeof(app.weatherLabelText), "meteo errore");
    strlcpy(app.weatherIconCode, "", sizeof(app.weatherIconCode));
    http.end();
    if (previousWeatherValid != app.weatherValid
      || previousWeatherTemperature != app.weatherTemperatureC
      || strcmp(previousWeatherText, app.weatherLabelText) != 0
      || strcmp(previousWeatherIconCode, app.weatherIconCode) != 0) {
      markUiDirty(UI_DIRTY_HEADER | UI_DIRTY_MAIN_WEATHER);
    }
    return;
  }

  String payload = http.getString();
  http.end();

  int temperature = extractJsonIntAfterKey(payload, "\"temp\"", 0);
  String iconCode = extractJsonStringAfterKey(payload, "\"icon\"", "");
  snprintf(app.weatherLabelText, sizeof(app.weatherLabelText), "%s %dC", WEATHER_CITY_LABEL, temperature);
  strlcpy(app.weatherIconCode, iconCode.c_str(), sizeof(app.weatherIconCode));
  app.weatherValid = true;
  app.weatherTemperatureC = temperature;

  if (previousWeatherValid != app.weatherValid
    || previousWeatherTemperature != app.weatherTemperatureC
    || strcmp(previousWeatherText, app.weatherLabelText) != 0
    || strcmp(previousWeatherIconCode, app.weatherIconCode) != 0) {
    markUiDirty(UI_DIRTY_HEADER | UI_DIRTY_MAIN_WEATHER);
  }
}

void updateNewsFeed() {
  unsigned long refreshInterval = app.newsValid ? TIMING_NEWS_REFRESH_MS : TIMING_NEWS_RETRY_MS;
  if (!intervalElapsed(app.lastNewsFetchMs, refreshInterval)) {
    return;
  }

  bool previousNewsValid = app.newsValid;
  int previousNewsItemCount = app.newsItemCount;
  char previousNewsTicker[sizeof(app.newsTicker)];
  char previousFirstNewsItem[NEWS_MAX_TEXT_LEN];
  strlcpy(previousNewsTicker, app.newsTicker, sizeof(previousNewsTicker));
  strlcpy(previousFirstNewsItem, app.newsItemCount > 0 ? app.newsItems[0] : "", sizeof(previousFirstNewsItem));

  if (WiFi.status() != WL_CONNECTED) {
    app.newsValid = false;
    if (previousNewsValid != app.newsValid) {
      markUiDirty(UI_DIRTY_FOOTER | UI_DIRTY_MAIN_NEWS);
    }
    return;
  }

  if (strlen(NEWS_API_URL) == 0 || strlen(NEWS_API_KEY) == 0) {
    app.newsValid = false;
    if (previousNewsValid != app.newsValid) {
      markUiDirty(UI_DIRTY_FOOTER | UI_DIRTY_MAIN_NEWS);
    }
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, NEWS_API_URL)) {
    app.newsValid = false;
    if (previousNewsValid != app.newsValid) {
      markUiDirty(UI_DIRTY_FOOTER | UI_DIRTY_MAIN_NEWS);
    }
    return;
  }

  http.addHeader("X-API-Key", NEWS_API_KEY);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    app.newsValid = false;
    http.end();
    if (previousNewsValid != app.newsValid) {
      markUiDirty(UI_DIRTY_FOOTER | UI_DIRTY_MAIN_NEWS);
    }
    return;
  }

  String payload = http.getString();
  http.end();

  app.newsValid = parseNewsItems(payload);
  if (previousNewsValid != app.newsValid
    || previousNewsItemCount != app.newsItemCount
    || strcmp(previousNewsTicker, app.newsTicker) != 0
    || strcmp(previousFirstNewsItem, app.newsItemCount > 0 ? app.newsItems[0] : "") != 0) {
    markUiDirty(UI_DIRTY_FOOTER | UI_DIRTY_MAIN_NEWS);
  }
}

static void ensureWifiConnection() {
  if (WiFi.status() == WL_CONNECTED || strlen(WIFI_SSID) == 0) {
    return;
  }

  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void beginTimeSync() {
  if (strlen(WIFI_SSID) == 0) {
    DEBUG_NETWORK_PRINT("WiFi non configurato: imposta WIFI_SSID e WIFI_PASSWORD");
    snprintf(app.clockLabelText, sizeof(app.clockLabelText), "config WiFi");
    return;
  }

  ensureWifiConnection();
  DEBUG_NETWORK_PRINT("Connessione WiFi per NTP...");
  configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
  app.lastTimeSyncAttemptMs = millis();
}

void maintainTimeSync() {
  if (app.timeSynced) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - app.lastTimeSyncAttemptMs > TIMING_WIFI_RECONNECT_MS) {
      beginTimeSync();
    }
    return;
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    app.timeSynced = true;
    DEBUG_NETWORK_PRINT("Orario NTP sincronizzato");
    updateClockUi();
    return;
  }

  if (millis() - app.lastTimeSyncAttemptMs > TIMING_NTP_RETRY_MS) {
    DEBUG_NETWORK_PRINT("Ritento sync NTP");
    configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
    app.lastTimeSyncAttemptMs = millis();
  }
}
