#include "dashboard_services.h"
#include "dashboard_services_common.h"
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
#include "secrets.h"

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
