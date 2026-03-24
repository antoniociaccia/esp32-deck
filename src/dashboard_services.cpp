#include "dashboard_services.h"
#include "dashboard_app.h"
#include "dashboard_ota.h"
#include "dashboard_support.h"
#include "dashboard_ui.h"
#include "config_debug.h"
#include "config_timing.h"
#include "version.h"
#include <HTTPClient.h>
#include <HTTPUpdate.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "secrets.h"

static void prepareSecureHttpClient(HTTPClient &http, WiFiClientSecure &client);

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

static void clearOtaManifestState() {
  app.otaManifestValid = false;
  app.otaEligibility = OTA_ELIGIBILITY_INVALID;
  app.otaRemoteSizeBytes = 0;
  app.otaMinBatteryPercent = 0;
  app.otaRemoteVersion[0] = '\0';
  app.otaRemoteBuild[0] = '\0';
  app.otaRemoteBinUrl[0] = '\0';
}

static void setOtaApplyState(OtaApplyState state, int progressPercent, int errorCode, const char *statusText) {
  app.otaApplyState = state;
  app.otaApplyProgressPercent = progressPercent;
  app.otaApplyLastErrorCode = errorCode;
  strlcpy(app.otaApplyStatusText, statusText != nullptr ? statusText : "", sizeof(app.otaApplyStatusText));
}

static void pumpOtaUi() {
  markUiDirty(UI_DIRTY_HEADER);
  refreshDashboardUi();
  lv_task_handler();
  delay(1);
}

static bool otaStateChanged(
  ServiceFetchState previousState,
  int previousHttpCode,
  bool previousManifestValid,
  OtaEligibility previousEligibility,
  uint32_t previousRemoteSizeBytes,
  int previousMinBatteryPercent,
  const char *previousRemoteVersion,
  const char *previousRemoteBuild) {
  return previousState != app.otaState
    || previousHttpCode != app.otaLastHttpCode
    || previousManifestValid != app.otaManifestValid
    || previousEligibility != app.otaEligibility
    || previousRemoteSizeBytes != app.otaRemoteSizeBytes
    || previousMinBatteryPercent != app.otaMinBatteryPercent
    || strcmp(previousRemoteVersion, app.otaRemoteVersion) != 0
    || strcmp(previousRemoteBuild, app.otaRemoteBuild) != 0;
}

static void logOtaStateIfChanged(
  ServiceFetchState previousState,
  int previousHttpCode,
  bool previousManifestValid,
  OtaEligibility previousEligibility,
  uint32_t previousRemoteSizeBytes,
  int previousMinBatteryPercent,
  const char *previousRemoteVersion,
  const char *previousRemoteBuild) {
  if (!otaStateChanged(previousState, previousHttpCode, previousManifestValid, previousEligibility,
      previousRemoteSizeBytes, previousMinBatteryPercent, previousRemoteVersion, previousRemoteBuild)) {
    return;
  }

  DEBUG_NETWORK_PRINTF(
    "OTA state=%s http=%d manifest=%d eligibility=%s version='%s' size=%lu min_battery=%d\n",
    serviceFetchStateLabel(app.otaState),
    app.otaLastHttpCode,
    app.otaManifestValid ? 1 : 0,
    otaEligibilityLabel(app.otaEligibility),
    app.otaRemoteVersion,
    static_cast<unsigned long>(app.otaRemoteSizeBytes),
    app.otaMinBatteryPercent);
}

static void markOtaUiDirtyIfChanged(
  ServiceFetchState previousState,
  int previousHttpCode,
  bool previousManifestValid,
  OtaEligibility previousEligibility,
  uint32_t previousRemoteSizeBytes,
  int previousMinBatteryPercent,
  const char *previousRemoteVersion,
  const char *previousRemoteBuild) {
  if (otaStateChanged(previousState, previousHttpCode, previousManifestValid, previousEligibility,
      previousRemoteSizeBytes, previousMinBatteryPercent, previousRemoteVersion, previousRemoteBuild)) {
    markUiDirty(UI_DIRTY_HEADER);
  }
}

static void setOtaCheckFailure(ServiceFetchState state, int httpCode = 0) {
  clearOtaManifestState();
  app.otaState = state;
  app.otaLastHttpCode = httpCode;
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

void updateOtaManifestCheck() {
  if (app.otaApplyState == OTA_APPLY_IN_PROGRESS) {
    return;
  }

  unsigned long refreshInterval = app.otaManifestValid ? TIMING_OTA_REFRESH_MS : TIMING_OTA_RETRY_MS;
  bool firstCheck = app.otaState == SERVICE_FETCH_IDLE && app.lastOtaCheckMs == 0;
  bool retryAfterReconnect = app.otaState == SERVICE_FETCH_OFFLINE && WiFi.status() == WL_CONNECTED;
  if (!firstCheck && !retryAfterReconnect && !intervalElapsed(app.lastOtaCheckMs, refreshInterval)) {
    return;
  }
  if (firstCheck || retryAfterReconnect) {
    app.lastOtaCheckMs = millis();
  }

  bool previousManifestValid = app.otaManifestValid;
  ServiceFetchState previousState = app.otaState;
  int previousHttpCode = app.otaLastHttpCode;
  OtaEligibility previousEligibility = app.otaEligibility;
  uint32_t previousRemoteSizeBytes = app.otaRemoteSizeBytes;
  int previousMinBatteryPercent = app.otaMinBatteryPercent;
  char previousRemoteVersion[sizeof(app.otaRemoteVersion)];
  char previousRemoteBuild[sizeof(app.otaRemoteBuild)];
  strlcpy(previousRemoteVersion, app.otaRemoteVersion, sizeof(previousRemoteVersion));
  strlcpy(previousRemoteBuild, app.otaRemoteBuild, sizeof(previousRemoteBuild));

  if (WiFi.status() != WL_CONNECTED) {
    setOtaCheckFailure(SERVICE_FETCH_OFFLINE);
    logOtaStateIfChanged(previousState, previousHttpCode, previousManifestValid, previousEligibility,
      previousRemoteSizeBytes, previousMinBatteryPercent, previousRemoteVersion, previousRemoteBuild);
    markOtaUiDirtyIfChanged(previousState, previousHttpCode, previousManifestValid, previousEligibility,
      previousRemoteSizeBytes, previousMinBatteryPercent, previousRemoteVersion, previousRemoteBuild);
    return;
  }

  if (strlen(OTA_MANIFEST_URL) == 0) {
    setOtaCheckFailure(SERVICE_FETCH_CONFIG_MISSING);
    logOtaStateIfChanged(previousState, previousHttpCode, previousManifestValid, previousEligibility,
      previousRemoteSizeBytes, previousMinBatteryPercent, previousRemoteVersion, previousRemoteBuild);
    markOtaUiDirtyIfChanged(previousState, previousHttpCode, previousManifestValid, previousEligibility,
      previousRemoteSizeBytes, previousMinBatteryPercent, previousRemoteVersion, previousRemoteBuild);
    return;
  }

  WiFiClientSecure client;
  HTTPClient http;
  prepareSecureHttpClient(http, client);
  if (!http.begin(client, OTA_MANIFEST_URL)) {
    setOtaCheckFailure(SERVICE_FETCH_TRANSPORT_ERROR);
    logOtaStateIfChanged(previousState, previousHttpCode, previousManifestValid, previousEligibility,
      previousRemoteSizeBytes, previousMinBatteryPercent, previousRemoteVersion, previousRemoteBuild);
    markOtaUiDirtyIfChanged(previousState, previousHttpCode, previousManifestValid, previousEligibility,
      previousRemoteSizeBytes, previousMinBatteryPercent, previousRemoteVersion, previousRemoteBuild);
    return;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    setOtaCheckFailure(SERVICE_FETCH_HTTP_ERROR, httpCode);
    http.end();
    logOtaStateIfChanged(previousState, previousHttpCode, previousManifestValid, previousEligibility,
      previousRemoteSizeBytes, previousMinBatteryPercent, previousRemoteVersion, previousRemoteBuild);
    markOtaUiDirtyIfChanged(previousState, previousHttpCode, previousManifestValid, previousEligibility,
      previousRemoteSizeBytes, previousMinBatteryPercent, previousRemoteVersion, previousRemoteBuild);
    return;
  }

  String payload = http.getString();
  http.end();

  OtaManifest manifest;
  if (!parseOtaManifest(payload, manifest)) {
    setOtaCheckFailure(SERVICE_FETCH_INVALID_PAYLOAD, httpCode);
    logOtaStateIfChanged(previousState, previousHttpCode, previousManifestValid, previousEligibility,
      previousRemoteSizeBytes, previousMinBatteryPercent, previousRemoteVersion, previousRemoteBuild);
    markOtaUiDirtyIfChanged(previousState, previousHttpCode, previousManifestValid, previousEligibility,
      previousRemoteSizeBytes, previousMinBatteryPercent, previousRemoteVersion, previousRemoteBuild);
    return;
  }

  app.otaManifestValid = true;
  app.otaState = SERVICE_FETCH_READY;
  app.otaLastHttpCode = httpCode;
  app.otaEligibility = evaluateOtaEligibility(
    manifest,
    FW_VERSION,
    FW_BOARD_ID,
    FW_RELEASE_CHANNEL,
    NETWORK_OTA_SLOT_MAX_BYTES,
    app.batteryPercent);
  app.otaRemoteSizeBytes = manifest.sizeBytes;
  app.otaMinBatteryPercent = manifest.minBatteryPercent;
  strlcpy(app.otaRemoteVersion, manifest.version, sizeof(app.otaRemoteVersion));
  strlcpy(app.otaRemoteBuild, manifest.build, sizeof(app.otaRemoteBuild));
  strlcpy(app.otaRemoteBinUrl, manifest.binUrl, sizeof(app.otaRemoteBinUrl));

  logOtaStateIfChanged(previousState, previousHttpCode, previousManifestValid, previousEligibility,
    previousRemoteSizeBytes, previousMinBatteryPercent, previousRemoteVersion, previousRemoteBuild);
  markOtaUiDirtyIfChanged(previousState, previousHttpCode, previousManifestValid, previousEligibility,
    previousRemoteSizeBytes, previousMinBatteryPercent, previousRemoteVersion, previousRemoteBuild);
}

void requestOtaManifestRefresh() {
  if (app.otaApplyState == OTA_APPLY_IN_PROGRESS) {
    return;
  }

  app.otaApplyRequested = false;
  app.otaApplyState = OTA_APPLY_IDLE;
  app.otaApplyProgressPercent = -1;
  app.otaApplyBytesCurrent = 0;
  app.otaApplyBytesTotal = 0;
  app.otaApplyLastErrorCode = 0;
  app.otaApplyStatusText[0] = '\0';
  app.otaState = SERVICE_FETCH_IDLE;
  app.otaLastHttpCode = 0;
  app.lastOtaCheckMs = 0;
  markUiDirty(UI_DIRTY_HEADER);
}

void startOtaFirmwareUpdate() {
  app.otaApplyRequested = false;

  if (WiFi.status() != WL_CONNECTED) {
    setOtaApplyState(OTA_APPLY_FAILED, -1, 0, "Wi-Fi non connesso.");
    pumpOtaUi();
    return;
  }

  if (app.otaState != SERVICE_FETCH_READY || app.otaEligibility != OTA_ELIGIBILITY_UPDATE_AVAILABLE) {
    setOtaApplyState(OTA_APPLY_FAILED, -1, 0, "Nessun update installabile.");
    pumpOtaUi();
    return;
  }

  if (app.otaRemoteBinUrl[0] == '\0') {
    setOtaApplyState(OTA_APPLY_FAILED, -1, 0, "bin_url OTA mancante.");
    pumpOtaUi();
    return;
  }

  setOtaApplyState(OTA_APPLY_IN_PROGRESS, 0, 0, "Preparazione OTA...");
  app.otaApplyBytesCurrent = 0;
  app.otaApplyBytesTotal = 0;
  pumpOtaUi();

  WiFiClientSecure client;
  client.setInsecure();

  HTTPUpdate updater(NETWORK_HTTP_TIMEOUT_MS);
  updater.rebootOnUpdate(false);
  updater.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  updater.onStart([]() {
    setOtaApplyState(OTA_APPLY_IN_PROGRESS, 0, 0, "Download firmware...");
    app.otaApplyBytesCurrent = 0;
    app.otaApplyBytesTotal = 0;
    pumpOtaUi();
  });

  updater.onProgress([](int current, int total) {
    int percent = -1;
    if (total > 0) {
      percent = (current * 100) / total;
    }

    app.otaApplyBytesCurrent = current > 0 ? static_cast<uint32_t>(current) : 0;
    app.otaApplyBytesTotal = total > 0 ? static_cast<uint32_t>(total) : 0;

    char statusText[96];
    if (total > 0) {
      snprintf(
        statusText,
        sizeof(statusText),
        "Download firmware... %d%% (%lu/%lu KB)",
        percent,
        static_cast<unsigned long>(app.otaApplyBytesCurrent / 1024UL),
        static_cast<unsigned long>(app.otaApplyBytesTotal / 1024UL));
    } else {
      snprintf(
        statusText,
        sizeof(statusText),
        "Download firmware... %lu KB",
        static_cast<unsigned long>(app.otaApplyBytesCurrent / 1024UL));
    }
    setOtaApplyState(OTA_APPLY_IN_PROGRESS, percent, 0, statusText);
    pumpOtaUi();
  });

  updater.onEnd([]() {
    setOtaApplyState(OTA_APPLY_SUCCESS, 100, 0, "Update installato. Riavvio...");
    app.otaApplyBytesCurrent = app.otaApplyBytesTotal;
    pumpOtaUi();
  });

  updater.onError([](int errorCode) {
    char statusText[64];
    snprintf(statusText, sizeof(statusText), "Errore OTA (%d).", errorCode);
    setOtaApplyState(OTA_APPLY_FAILED, -1, errorCode, statusText);
    pumpOtaUi();
  });

  HTTPUpdateResult result = updater.update(client, String(app.otaRemoteBinUrl), String(FW_VERSION));
  if (result == HTTP_UPDATE_OK) {
    setOtaApplyState(OTA_APPLY_SUCCESS, 100, 0, "Update installato. Riavvio...");
    app.otaApplyBytesCurrent = app.otaApplyBytesTotal;
    pumpOtaUi();
    delay(500);
    ESP.restart();
    return;
  }

  if (result == HTTP_UPDATE_NO_UPDATES) {
    app.otaEligibility = OTA_ELIGIBILITY_UP_TO_DATE;
    setOtaApplyState(OTA_APPLY_IDLE, -1, 0, "");
    markUiDirty(UI_DIRTY_HEADER);
    return;
  }

  String errorText = updater.getLastErrorString();
  char statusText[96];
  if (errorText.length() > 0) {
    snprintf(statusText, sizeof(statusText), "Errore OTA (%d): %s", updater.getLastError(), errorText.c_str());
  } else {
    snprintf(statusText, sizeof(statusText), "Errore OTA (%d).", updater.getLastError());
  }
  setOtaApplyState(OTA_APPLY_FAILED, -1, updater.getLastError(), statusText);
  pumpOtaUi();
}

static void prepareSecureHttpClient(HTTPClient &http, WiFiClientSecure &client) {
  client.setInsecure();
  http.setTimeout(NETWORK_HTTP_TIMEOUT_MS);
  http.setReuse(false);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
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
