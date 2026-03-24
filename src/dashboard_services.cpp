#include "dashboard_services.h"
#include "dashboard_services_common.h"
#include "dashboard_app.h"
#include "dashboard_support.h"
#include "config_debug.h"
#include "config_timing.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <time.h>
#include "secrets.h"

void prepareSecureHttpClient(HTTPClient &http, WiFiClientSecure &client) {
  client.setInsecure();
  http.setTimeout(NETWORK_HTTP_TIMEOUT_MS);
  http.setReuse(false);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);
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
