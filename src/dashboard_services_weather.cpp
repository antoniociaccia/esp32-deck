#include "dashboard_services.h"
#include "dashboard_services_common.h"
#include "dashboard_app.h"
#include "dashboard_support.h"
#include "config_debug.h"
#include "config_timing.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
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

static String buildWeatherUrl() {
  String url = "https://api.openweathermap.org/data/2.5/weather?lat=";
  url += String(WEATHER_LATITUDE, 4);
  url += "&lon=";
  url += String(WEATHER_LONGITUDE, 4);
  url += "&units=metric&lang=it&appid=";
  url += OPENWEATHER_API_KEY;
  return url;
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
