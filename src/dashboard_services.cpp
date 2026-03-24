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

static bool weatherUiStateChanged(
  ServiceFetchState previousState,
  int previousHttpCode,
  bool previousValid,
  int previousTemperature,
  const char *previousWeatherText,
  const char *previousWeatherIconCode) {
  return previousState != app.weatherState
    || previousHttpCode != app.weatherLastHttpCode
    || previousValid != app.weatherValid
    || previousTemperature != app.weatherTemperatureC
    || strcmp(previousWeatherText, app.weatherLabelText) != 0
    || strcmp(previousWeatherIconCode, app.weatherIconCode) != 0;
}

static void markWeatherUiDirtyIfChanged(
  ServiceFetchState previousState,
  int previousHttpCode,
  bool previousValid,
  int previousTemperature,
  const char *previousWeatherText,
  const char *previousWeatherIconCode) {
  if (weatherUiStateChanged(
      previousState,
      previousHttpCode,
      previousValid,
      previousTemperature,
      previousWeatherText,
      previousWeatherIconCode)) {
    markUiDirty(UI_DIRTY_HEADER | UI_DIRTY_MAIN_WEATHER);
  }
}

static void logWeatherStateIfChanged(
  ServiceFetchState previousState,
  int previousHttpCode,
  bool previousValid,
  int previousTemperature,
  const char *previousWeatherText,
  const char *previousWeatherIconCode) {
  if (!weatherUiStateChanged(
      previousState,
      previousHttpCode,
      previousValid,
      previousTemperature,
      previousWeatherText,
      previousWeatherIconCode)) {
    return;
  }

  DEBUG_NETWORK_PRINTF(
    "Weather state=%s http=%d valid=%d temp=%d label='%s'\n",
    serviceFetchStateLabel(app.weatherState),
    app.weatherLastHttpCode,
    app.weatherValid ? 1 : 0,
    app.weatherTemperatureC,
    app.weatherLabelText);
}

static void setWeatherFetchFailure(ServiceFetchState state, const char *labelText, int httpCode = 0) {
  app.weatherValid = false;
  app.weatherState = state;
  app.weatherLastHttpCode = httpCode;
  app.weatherTemperatureC = 0;
  strlcpy(app.weatherLabelText, labelText, sizeof(app.weatherLabelText));
  app.weatherIconCode[0] = '\0';
}

static bool newsUiStateChanged(
  ServiceFetchState previousState,
  int previousHttpCode,
  bool previousValid,
  int previousNewsItemCount,
  const char *previousNewsTicker,
  const char *previousFirstNewsItem) {
  return previousState != app.newsState
    || previousHttpCode != app.newsLastHttpCode
    || previousValid != app.newsValid
    || previousNewsItemCount != app.newsItemCount
    || strcmp(previousNewsTicker, app.newsTicker) != 0
    || strcmp(previousFirstNewsItem, app.newsItemCount > 0 ? app.newsItems[0] : "") != 0;
}

static void markNewsUiDirtyIfChanged(
  ServiceFetchState previousState,
  int previousHttpCode,
  bool previousValid,
  int previousNewsItemCount,
  const char *previousNewsTicker,
  const char *previousFirstNewsItem) {
  if (newsUiStateChanged(
      previousState,
      previousHttpCode,
      previousValid,
      previousNewsItemCount,
      previousNewsTicker,
      previousFirstNewsItem)) {
    markUiDirty(UI_DIRTY_FOOTER | UI_DIRTY_MAIN_NEWS);
  }
}

static void logNewsStateIfChanged(
  ServiceFetchState previousState,
  int previousHttpCode,
  bool previousValid,
  int previousNewsItemCount,
  const char *previousNewsTicker,
  const char *previousFirstNewsItem) {
  if (!newsUiStateChanged(
      previousState,
      previousHttpCode,
      previousValid,
      previousNewsItemCount,
      previousNewsTicker,
      previousFirstNewsItem)) {
    return;
  }

  DEBUG_NETWORK_PRINTF(
    "News state=%s http=%d valid=%d items=%d\n",
    serviceFetchStateLabel(app.newsState),
    app.newsLastHttpCode,
    app.newsValid ? 1 : 0,
    app.newsItemCount);
}

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

static void prepareSecureHttpClient(HTTPClient &http, WiFiClientSecure &client) {
  client.setInsecure();
  http.setTimeout(NETWORK_HTTP_TIMEOUT_MS);
  http.setReuse(false);
}

static bool handleWeatherTestMode(
  ServiceFetchState previousWeatherState,
  int previousWeatherHttpCode,
  bool previousWeatherValid,
  int previousWeatherTemperature,
  const char *previousWeatherText,
  const char *previousWeatherIconCode) {
  switch (DEBUG_WEATHER_TEST_MODE) {
    case NETWORK_TEST_MODE_DISABLED:
      return false;
    case NETWORK_TEST_MODE_OFFLINE:
      setWeatherFetchFailure(SERVICE_FETCH_OFFLINE, "meteo offline");
      break;
    case NETWORK_TEST_MODE_CONFIG_MISSING:
      setWeatherFetchFailure(SERVICE_FETCH_CONFIG_MISSING, "meteo n/d");
      break;
    case NETWORK_TEST_MODE_TRANSPORT_ERROR:
      setWeatherFetchFailure(SERVICE_FETCH_TRANSPORT_ERROR, "meteo rete");
      break;
    case NETWORK_TEST_MODE_HTTP_ERROR:
      setWeatherFetchFailure(SERVICE_FETCH_HTTP_ERROR, "meteo http", DEBUG_WEATHER_TEST_HTTP_CODE);
      break;
    case NETWORK_TEST_MODE_INVALID_PAYLOAD:
      setWeatherFetchFailure(SERVICE_FETCH_INVALID_PAYLOAD, "meteo json");
      break;
    case NETWORK_TEST_MODE_SUCCESS_MOCK: {
      static const String mockPayload = "{\"main\":{\"temp\":21},\"weather\":[{\"icon\":\"01d\"}]}";
      int temperature = 0;
      char iconCode[sizeof(app.weatherIconCode)] = {};
      if (!parseWeatherPayload(mockPayload, temperature, iconCode, sizeof(iconCode))) {
        setWeatherFetchFailure(SERVICE_FETCH_INVALID_PAYLOAD, "meteo json");
      } else {
        snprintf(app.weatherLabelText, sizeof(app.weatherLabelText), "%s %dC", WEATHER_CITY_LABEL, temperature);
        strlcpy(app.weatherIconCode, iconCode, sizeof(app.weatherIconCode));
        app.weatherValid = true;
        app.weatherState = SERVICE_FETCH_READY;
        app.weatherLastHttpCode = HTTP_CODE_OK;
        app.weatherTemperatureC = temperature;
      }
      break;
    }
    default:
      return false;
  }

  DEBUG_NETWORK_PRINTF("Weather test mode active: %u\n", DEBUG_WEATHER_TEST_MODE);
  markWeatherUiDirtyIfChanged(previousWeatherState, previousWeatherHttpCode, previousWeatherValid,
    previousWeatherTemperature, previousWeatherText, previousWeatherIconCode);
  return true;
}

static bool handleNewsTestMode(
  ServiceFetchState previousNewsState,
  int previousNewsHttpCode,
  bool previousNewsValid,
  int previousNewsItemCount,
  const char *previousNewsTicker,
  const char *previousFirstNewsItem) {
  switch (DEBUG_NEWS_TEST_MODE) {
    case NETWORK_TEST_MODE_DISABLED:
      return false;
    case NETWORK_TEST_MODE_OFFLINE:
      app.newsValid = false;
      app.newsState = SERVICE_FETCH_OFFLINE;
      app.newsLastHttpCode = 0;
      break;
    case NETWORK_TEST_MODE_CONFIG_MISSING:
      app.newsValid = false;
      app.newsState = SERVICE_FETCH_CONFIG_MISSING;
      app.newsLastHttpCode = 0;
      break;
    case NETWORK_TEST_MODE_TRANSPORT_ERROR:
      app.newsValid = false;
      app.newsState = SERVICE_FETCH_TRANSPORT_ERROR;
      app.newsLastHttpCode = 0;
      break;
    case NETWORK_TEST_MODE_HTTP_ERROR:
      app.newsValid = false;
      app.newsState = SERVICE_FETCH_HTTP_ERROR;
      app.newsLastHttpCode = DEBUG_NEWS_TEST_HTTP_CODE;
      break;
    case NETWORK_TEST_MODE_INVALID_PAYLOAD:
      app.newsValid = false;
      app.newsState = SERVICE_FETCH_INVALID_PAYLOAD;
      app.newsLastHttpCode = HTTP_CODE_OK;
      break;
    case NETWORK_TEST_MODE_SUCCESS_MOCK: {
      static const String mockPayload =
        "{\"items\":["
        "{\"text\":\"TEST | Feed locale attivo\"},"
        "{\"text\":\"TECH | Simulazione news in corso\"},"
        "{\"text\":\"WORLD | Backend reale non necessario per questa prova\"}"
        "]}";
      app.newsValid = parseNewsItems(mockPayload);
      app.newsState = app.newsValid ? SERVICE_FETCH_READY : SERVICE_FETCH_INVALID_PAYLOAD;
      app.newsLastHttpCode = HTTP_CODE_OK;
      break;
    }
    default:
      return false;
  }

  DEBUG_NETWORK_PRINTF("News test mode active: %u\n", DEBUG_NEWS_TEST_MODE);
  markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
    previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
  return true;
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
  ServiceFetchState previousWeatherState = app.weatherState;
  int previousWeatherHttpCode = app.weatherLastHttpCode;
  int previousWeatherTemperature = app.weatherTemperatureC;

  if (handleWeatherTestMode(previousWeatherState, previousWeatherHttpCode, previousWeatherValid,
      previousWeatherTemperature, previousWeatherText, previousWeatherIconCode)) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    setWeatherFetchFailure(SERVICE_FETCH_OFFLINE, "meteo offline");
    logWeatherStateIfChanged(previousWeatherState, previousWeatherHttpCode, previousWeatherValid,
      previousWeatherTemperature, previousWeatherText, previousWeatherIconCode);
    markWeatherUiDirtyIfChanged(previousWeatherState, previousWeatherHttpCode, previousWeatherValid,
      previousWeatherTemperature, previousWeatherText, previousWeatherIconCode);
    return;
  }

  if (strlen(OPENWEATHER_API_KEY) == 0) {
    setWeatherFetchFailure(SERVICE_FETCH_CONFIG_MISSING, "meteo n/d");
    logWeatherStateIfChanged(previousWeatherState, previousWeatherHttpCode, previousWeatherValid,
      previousWeatherTemperature, previousWeatherText, previousWeatherIconCode);
    markWeatherUiDirtyIfChanged(previousWeatherState, previousWeatherHttpCode, previousWeatherValid,
      previousWeatherTemperature, previousWeatherText, previousWeatherIconCode);
    return;
  }

  WiFiClientSecure client;
  HTTPClient http;
  prepareSecureHttpClient(http, client);
  if (!http.begin(client, buildWeatherUrl())) {
    setWeatherFetchFailure(SERVICE_FETCH_TRANSPORT_ERROR, "meteo rete");
    logWeatherStateIfChanged(previousWeatherState, previousWeatherHttpCode, previousWeatherValid,
      previousWeatherTemperature, previousWeatherText, previousWeatherIconCode);
    markWeatherUiDirtyIfChanged(previousWeatherState, previousWeatherHttpCode, previousWeatherValid,
      previousWeatherTemperature, previousWeatherText, previousWeatherIconCode);
    return;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    setWeatherFetchFailure(SERVICE_FETCH_HTTP_ERROR, "meteo http", httpCode);
    http.end();
    logWeatherStateIfChanged(previousWeatherState, previousWeatherHttpCode, previousWeatherValid,
      previousWeatherTemperature, previousWeatherText, previousWeatherIconCode);
    markWeatherUiDirtyIfChanged(previousWeatherState, previousWeatherHttpCode, previousWeatherValid,
      previousWeatherTemperature, previousWeatherText, previousWeatherIconCode);
    return;
  }

  String payload = http.getString();
  http.end();

  int temperature = 0;
  char iconCode[sizeof(app.weatherIconCode)] = {};
  if (!parseWeatherPayload(payload, temperature, iconCode, sizeof(iconCode))) {
    setWeatherFetchFailure(SERVICE_FETCH_INVALID_PAYLOAD, "meteo json");
    logWeatherStateIfChanged(previousWeatherState, previousWeatherHttpCode, previousWeatherValid,
      previousWeatherTemperature, previousWeatherText, previousWeatherIconCode);
    markWeatherUiDirtyIfChanged(previousWeatherState, previousWeatherHttpCode, previousWeatherValid,
      previousWeatherTemperature, previousWeatherText, previousWeatherIconCode);
    return;
  }

  snprintf(app.weatherLabelText, sizeof(app.weatherLabelText), "%s %dC", WEATHER_CITY_LABEL, temperature);
  strlcpy(app.weatherIconCode, iconCode, sizeof(app.weatherIconCode));
  app.weatherValid = true;
  app.weatherState = SERVICE_FETCH_READY;
  app.weatherLastHttpCode = httpCode;
  app.weatherTemperatureC = temperature;

  logWeatherStateIfChanged(previousWeatherState, previousWeatherHttpCode, previousWeatherValid,
    previousWeatherTemperature, previousWeatherText, previousWeatherIconCode);
  markWeatherUiDirtyIfChanged(previousWeatherState, previousWeatherHttpCode, previousWeatherValid,
    previousWeatherTemperature, previousWeatherText, previousWeatherIconCode);
}

void updateNewsFeed() {
  unsigned long refreshInterval = app.newsValid ? TIMING_NEWS_REFRESH_MS : TIMING_NEWS_RETRY_MS;
  if (!intervalElapsed(app.lastNewsFetchMs, refreshInterval)) {
    return;
  }

  bool previousNewsValid = app.newsValid;
  ServiceFetchState previousNewsState = app.newsState;
  int previousNewsHttpCode = app.newsLastHttpCode;
  int previousNewsItemCount = app.newsItemCount;
  char previousNewsTicker[sizeof(app.newsTicker)];
  char previousFirstNewsItem[NEWS_MAX_TEXT_LEN];
  strlcpy(previousNewsTicker, app.newsTicker, sizeof(previousNewsTicker));
  strlcpy(previousFirstNewsItem, app.newsItemCount > 0 ? app.newsItems[0] : "", sizeof(previousFirstNewsItem));

  if (handleNewsTestMode(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem)) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    app.newsValid = false;
    app.newsState = SERVICE_FETCH_OFFLINE;
    app.newsLastHttpCode = 0;
    logNewsStateIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    return;
  }

  if (strlen(NEWS_API_URL) == 0 || strlen(NEWS_API_KEY) == 0) {
    app.newsValid = false;
    app.newsState = SERVICE_FETCH_CONFIG_MISSING;
    app.newsLastHttpCode = 0;
    logNewsStateIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    return;
  }

  WiFiClientSecure client;
  HTTPClient http;
  prepareSecureHttpClient(http, client);
  if (!http.begin(client, NEWS_API_URL)) {
    app.newsValid = false;
    app.newsState = SERVICE_FETCH_TRANSPORT_ERROR;
    app.newsLastHttpCode = 0;
    logNewsStateIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    return;
  }

  http.addHeader("X-API-Key", NEWS_API_KEY);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    app.newsValid = false;
    app.newsState = SERVICE_FETCH_HTTP_ERROR;
    app.newsLastHttpCode = httpCode;
    http.end();
    logNewsStateIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    return;
  }

  String payload = http.getString();
  http.end();

  app.newsValid = parseNewsItems(payload);
  app.newsState = app.newsValid ? SERVICE_FETCH_READY : SERVICE_FETCH_INVALID_PAYLOAD;
  app.newsLastHttpCode = httpCode;
  logNewsStateIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
    previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
  markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
    previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
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
