#include "dashboard_services.h"
#include "dashboard_app.h"
#include "dashboard_support.h"
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

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    snprintf(app.clockLabelText, sizeof(app.clockLabelText), "%02d/%02d %02d:%02d",
      timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_hour, timeinfo.tm_min);
    app.timeSynced = true;
  } else {
    snprintf(app.clockLabelText, sizeof(app.clockLabelText), "sync orario...");
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

  if (WiFi.status() != WL_CONNECTED) {
    app.weatherValid = false;
    app.weatherTemperatureC = 0;
    snprintf(app.weatherLabelText, sizeof(app.weatherLabelText), "meteo offline");
    strlcpy(app.weatherIconCode, "", sizeof(app.weatherIconCode));
    return;
  }

  if (strlen(OPENWEATHER_API_KEY) == 0) {
    app.weatherValid = false;
    app.weatherTemperatureC = 0;
    snprintf(app.weatherLabelText, sizeof(app.weatherLabelText), "meteo n/d");
    strlcpy(app.weatherIconCode, "", sizeof(app.weatherIconCode));
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
    return;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    app.weatherValid = false;
    app.weatherTemperatureC = 0;
    snprintf(app.weatherLabelText, sizeof(app.weatherLabelText), "meteo errore");
    strlcpy(app.weatherIconCode, "", sizeof(app.weatherIconCode));
    http.end();
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
}

void updateNewsFeed() {
  unsigned long refreshInterval = app.newsValid ? TIMING_NEWS_REFRESH_MS : TIMING_NEWS_RETRY_MS;
  if (!intervalElapsed(app.lastNewsFetchMs, refreshInterval)) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    app.newsValid = false;
    return;
  }

  if (strlen(NEWS_API_URL) == 0 || strlen(NEWS_API_KEY) == 0) {
    app.newsValid = false;
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, NEWS_API_URL)) {
    app.newsValid = false;
    return;
  }

  http.addHeader("X-API-Key", NEWS_API_KEY);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    app.newsValid = false;
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  app.newsValid = parseNewsItems(payload);
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
    Serial.println("WiFi non configurato: imposta WIFI_SSID e WIFI_PASSWORD");
    snprintf(app.clockLabelText, sizeof(app.clockLabelText), "config WiFi");
    return;
  }

  ensureWifiConnection();
  Serial.println("Connessione WiFi per NTP...");
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
    Serial.println("Orario NTP sincronizzato");
    updateClockUi();
    return;
  }

  if (millis() - app.lastTimeSyncAttemptMs > TIMING_NTP_RETRY_MS) {
    Serial.println("Ritento sync NTP");
    configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
    app.lastTimeSyncAttemptMs = millis();
  }
}
