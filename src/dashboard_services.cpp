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
  if (!intervalElapsed(app.clock.lastUpdateMs, TIMING_CLOCK_REFRESH_MS)) {
    return;
  }

  char previousClockText[sizeof(app.clock.labelText)];
  strlcpy(previousClockText, app.clock.labelText, sizeof(previousClockText));
  bool previousTimeSynced = app.clock.synced;
  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    snprintf(app.clock.labelText, sizeof(app.clock.labelText), "%02d/%02d %02d:%02d",
      timeinfo.tm_mday, timeinfo.tm_mon + 1, timeinfo.tm_hour, timeinfo.tm_min);
    app.clock.synced = true;
  } else {
    snprintf(app.clock.labelText, sizeof(app.clock.labelText), "sync orario...");
  }

  if (strcmp(previousClockText, app.clock.labelText) != 0 || previousTimeSynced != app.clock.synced) {
    markUiDirty(UI_DIRTY_HEADER | UI_DIRTY_MAIN_SETTINGS | UI_DIRTY_MAIN_CLOCK);
  }
}

static void ensureWifiConnection() {
  if (strlen(WIFI_SSID) == 0) {
    return;
  }
  if (!app.settings.wifiEnabled || app.energy.wifiDisabledByPolicy) {
    return;
  }
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
}

void beginTimeSync() {
  if (strlen(WIFI_SSID) == 0) {
    DEBUG_NETWORK_PRINT("WiFi non configurato: imposta WIFI_SSID e WIFI_PASSWORD");
    snprintf(app.clock.labelText, sizeof(app.clock.labelText), "config WiFi");
    return;
  }

  ensureWifiConnection();
  DEBUG_NETWORK_PRINT("Connessione WiFi per NTP...");
  configTzTime(TZ_INFO, NTP_SERVER_1, NTP_SERVER_2);
  app.clock.lastSyncAttemptMs = millis();
}

void maintainTimeSync() {
  if (app.clock.synced) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    if (millis() - app.clock.lastSyncAttemptMs > TIMING_WIFI_RECONNECT_MS) {
      beginTimeSync();
    }
    return;
  }

  struct tm timeinfo;
  if (getLocalTime(&timeinfo, 10)) {
    app.clock.synced = true;
    DEBUG_NETWORK_PRINT("Orario NTP sincronizzato");
    updateClockUi();
    return;
  }

  if (millis() - app.clock.lastSyncAttemptMs > TIMING_NTP_RETRY_MS) {
    DEBUG_NETWORK_PRINT("Ritento sync NTP");
    beginTimeSync();
  }
}
