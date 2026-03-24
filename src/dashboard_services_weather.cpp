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

static void setWeatherFetchFailure(ServiceFetchState state, const char *labelText, int httpCode = 0) {
  app.weather.valid = false;
  app.weather.state = state;
  app.weather.lastHttpCode = httpCode;
  app.weather.temperatureC = 0;
  strlcpy(app.weather.labelText, labelText, sizeof(app.weather.labelText));
  app.weather.iconCode[0] = '\0';
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

static bool handleWeatherTestMode() {
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
      char iconCode[sizeof(app.weather.iconCode)] = {};
      if (!parseWeatherPayload(mockPayload, temperature, iconCode, sizeof(iconCode))) {
        setWeatherFetchFailure(SERVICE_FETCH_INVALID_PAYLOAD, "meteo json");
      } else {
        snprintf(app.weather.labelText, sizeof(app.weather.labelText), "%s %dC", WEATHER_CITY_LABEL, temperature);
        strlcpy(app.weather.iconCode, iconCode, sizeof(app.weather.iconCode));
        app.weather.valid = true;
        app.weather.state = SERVICE_FETCH_READY;
        app.weather.lastHttpCode = HTTP_CODE_OK;
        app.weather.temperatureC = temperature;
      }
      break;
    }
    default:
      return false;
  }

  DEBUG_NETWORK_PRINTF("Weather test mode active: %u\n", DEBUG_WEATHER_TEST_MODE);
  return true;
}

void updateWeatherUi() {
  unsigned long refreshInterval = app.weather.valid ? TIMING_WEATHER_REFRESH_MS : TIMING_WEATHER_RETRY_MS;
  if (!intervalElapsed(app.weather.lastUpdateMs, refreshInterval)) {
    return;
  }

  ServiceSnapshot<WeatherState> snap(app.weather, UI_DIRTY_HEADER | UI_DIRTY_MAIN_WEATHER);

  if (handleWeatherTestMode()) {
    snap.commitIfChanged("Weather");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    setWeatherFetchFailure(SERVICE_FETCH_OFFLINE, "meteo offline");
    snap.commitIfChanged("Weather");
    return;
  }

  if (strlen(OPENWEATHER_API_KEY) == 0) {
    setWeatherFetchFailure(SERVICE_FETCH_CONFIG_MISSING, "meteo n/d");
    snap.commitIfChanged("Weather");
    return;
  }

  app.weather.state = SERVICE_FETCH_FETCHING;
  snap.commitAndPumpUi("Weather");

  WiFiClientSecure client;
  HTTPClient http;
  prepareSecureHttpClient(http, client);
  if (!http.begin(client, buildWeatherUrl())) {
    setWeatherFetchFailure(SERVICE_FETCH_TRANSPORT_ERROR, "meteo rete");
    snap.commitIfChanged("Weather");
    return;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    setWeatherFetchFailure(SERVICE_FETCH_HTTP_ERROR, "meteo http", httpCode);
    http.end();
    snap.commitIfChanged("Weather");
    return;
  }

  String payload = http.getString();
  http.end();

  int temperature = 0;
  char iconCode[sizeof(app.weather.iconCode)] = {};
  if (!parseWeatherPayload(payload, temperature, iconCode, sizeof(iconCode))) {
    setWeatherFetchFailure(SERVICE_FETCH_INVALID_PAYLOAD, "meteo json");
    snap.commitIfChanged("Weather");
    return;
  }

  snprintf(app.weather.labelText, sizeof(app.weather.labelText), "%s %dC", WEATHER_CITY_LABEL, temperature);
  strlcpy(app.weather.iconCode, iconCode, sizeof(app.weather.iconCode));
  app.weather.valid = true;
  app.weather.state = SERVICE_FETCH_READY;
  app.weather.lastHttpCode = httpCode;
  app.weather.temperatureC = temperature;

  snap.commitIfChanged("Weather");
}
