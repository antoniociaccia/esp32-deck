#include "dashboard_services.h"
#include "dashboard_app.h"
#include "dashboard_ui.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "secrets.h"

void updateClockUi() {
  if (!intervalElapsed(app.lastClockUpdateMs, CLOCK_REFRESH_INTERVAL_MS)) {
    return;
  }

  if (ui.timeLabel == nullptr) {
    return;
  }

  char timeBuffer[24];
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    snprintf(timeBuffer, sizeof(timeBuffer), "%02d/%02d %02d:%02d",
      timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_hour, timeinfo.tm_min);
    app.timeSynced = true;
  } else {
    snprintf(timeBuffer, sizeof(timeBuffer), "sync orario...");
  }
  lv_label_set_text(ui.timeLabel, timeBuffer);
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
  unsigned long refreshInterval = app.weatherValid ? WEATHER_REFRESH_INTERVAL_MS : WEATHER_RETRY_INTERVAL_MS;
  if (!intervalElapsed(app.lastWeatherUpdateMs, refreshInterval)) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    app.weatherValid = false;
    app.weatherTemperatureC = 0;
    setWeatherStatus("meteo offline");
    return;
  }

  if (strlen(OPENWEATHER_API_KEY) == 0) {
    app.weatherValid = false;
    app.weatherTemperatureC = 0;
    setWeatherStatus("meteo n/d");
    return;
  }

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, buildWeatherUrl())) {
    app.weatherValid = false;
    app.weatherTemperatureC = 0;
    setWeatherStatus("meteo errore");
    return;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    app.weatherValid = false;
    app.weatherTemperatureC = 0;
    setWeatherStatus("meteo errore");
    http.end();
    return;
  }

  String payload = http.getString();
  http.end();

  int temperature = extractJsonIntAfterKey(payload, "\"temp\"", 0);
  String iconCode = extractJsonStringAfterKey(payload, "\"icon\"", "");
  char weatherBuffer[32];
  snprintf(weatherBuffer, sizeof(weatherBuffer), "%s %dC", WEATHER_CITY_LABEL, temperature);
  setWeatherStatus(weatherBuffer);
  updateWeatherIconUi(iconCode);
  app.weatherValid = true;
  app.weatherTemperatureC = temperature;
}

void updateNewsFeed() {
  unsigned long refreshInterval = app.newsValid ? NEWS_REFRESH_INTERVAL_MS : NEWS_RETRY_INTERVAL_MS;
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
    if (ui.timeLabel != nullptr) {
      lv_label_set_text(ui.timeLabel, "config WiFi");
    }
    updateWifiUi();
    return;
  }

  ensureWifiConnection();
  Serial.println("Connessione WiFi per NTP...");
  configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
  app.lastTimeSyncAttemptMs = millis();
  updateWifiUi();
}

void maintainTimeSync() {
  updateWifiUi();

  if (app.timeSynced) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - app.lastTimeSyncAttemptMs > WIFI_RECONNECT_INTERVAL_MS) {
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

  if (millis() - app.lastTimeSyncAttemptMs > NTP_RETRY_INTERVAL_MS) {
    Serial.println("Ritento sync NTP");
    configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
    app.lastTimeSyncAttemptMs = millis();
  }
}
