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
  app.ota.valid = false;
  app.ota.eligibility = OTA_ELIGIBILITY_INVALID;
  app.ota.remoteSizeBytes = 0;
  app.ota.minBatteryPercent = 0;
  app.ota.remoteVersion[0] = '\0';
  app.ota.remoteBuild[0] = '\0';
  app.ota.remoteBinUrl[0] = '\0';
  app.ota.remoteSha256[0] = '\0';
}

static void setOtaApplyState(OtaApplyState state, int progressPercent, int errorCode, const char *statusText) {
  app.ota.applyState = state;
  app.ota.applyProgressPercent = progressPercent;
  app.ota.applyLastErrorCode = errorCode;
  strlcpy(app.ota.applyStatusText, statusText != nullptr ? statusText : "", sizeof(app.ota.applyStatusText));
}

static void pumpOtaUi() {
  markUiDirty(UI_DIRTY_HEADER);
  refreshDashboardUi();
  lv_task_handler();
  delay(1);
}

static void setOtaCheckFailure(ServiceFetchState state, int httpCode = 0) {
  clearOtaManifestState();
  app.ota.state = state;
  app.ota.lastHttpCode = httpCode;
}

void updateOtaManifestCheck() {
  if (app.ota.applyState == OTA_APPLY_IN_PROGRESS) {
    return;
  }

  unsigned long refreshInterval = app.ota.valid ? TIMING_OTA_REFRESH_MS : TIMING_OTA_RETRY_MS;
  bool firstCheck = app.ota.state == SERVICE_FETCH_IDLE && app.ota.lastCheckMs == 0;
  bool retryAfterReconnect = app.ota.state == SERVICE_FETCH_OFFLINE && WiFi.status() == WL_CONNECTED;
  if (!firstCheck && !retryAfterReconnect && !intervalElapsed(app.ota.lastCheckMs, refreshInterval)) {
    return;
  }
  if (firstCheck || retryAfterReconnect) {
    app.ota.lastCheckMs = millis();
  }

  ServiceSnapshot<OtaState> snap(app.ota, UI_DIRTY_HEADER);

  if (WiFi.status() != WL_CONNECTED) {
    setOtaCheckFailure(SERVICE_FETCH_OFFLINE);
    snap.commitIfChanged("OTA");
    return;
  }

  if (strlen(OTA_MANIFEST_URL) == 0) {
    setOtaCheckFailure(SERVICE_FETCH_CONFIG_MISSING);
    snap.commitIfChanged("OTA");
    return;
  }

  app.ota.state = SERVICE_FETCH_FETCHING;
  snap.commitAndPumpUi("OTA");

  WiFiClientSecure client;
  HTTPClient http;
  prepareSecureHttpClient(http, client);
  if (!http.begin(client, OTA_MANIFEST_URL)) {
    setOtaCheckFailure(SERVICE_FETCH_TRANSPORT_ERROR);
    snap.commitIfChanged("OTA");
    return;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    setOtaCheckFailure(SERVICE_FETCH_HTTP_ERROR, httpCode);
    http.end();
    snap.commitIfChanged("OTA");
    return;
  }

  String payload = http.getString();
  http.end();

  OtaManifest manifest;
  if (!parseOtaManifest(payload, manifest)) {
    setOtaCheckFailure(SERVICE_FETCH_INVALID_PAYLOAD, httpCode);
    snap.commitIfChanged("OTA");
    return;
  }

  app.ota.valid = true;
  app.ota.state = SERVICE_FETCH_READY;
  app.ota.lastHttpCode = httpCode;
  app.ota.eligibility = evaluateOtaEligibility(
    manifest,
    FW_VERSION,
    FW_BOARD_ID,
    FW_RELEASE_CHANNEL,
    NETWORK_OTA_SLOT_MAX_BYTES,
    app.battery.percent);
  app.ota.remoteSizeBytes = manifest.sizeBytes;
  app.ota.minBatteryPercent = manifest.minBatteryPercent;
  strlcpy(app.ota.remoteVersion, manifest.version, sizeof(app.ota.remoteVersion));
  strlcpy(app.ota.remoteBuild, manifest.build, sizeof(app.ota.remoteBuild));
  strlcpy(app.ota.remoteBinUrl, manifest.binUrl, sizeof(app.ota.remoteBinUrl));
  strlcpy(app.ota.remoteSha256, manifest.sha256, sizeof(app.ota.remoteSha256));

  snap.commitIfChanged("OTA");
}

void requestOtaManifestRefresh() {
  if (app.ota.applyState == OTA_APPLY_IN_PROGRESS) {
    return;
  }

  app.ota.applyRequested = false;
  app.ota.applyState = OTA_APPLY_IDLE;
  app.ota.applyProgressPercent = -1;
  app.ota.applyBytesCurrent = 0;
  app.ota.applyBytesTotal = 0;
  app.ota.applyLastErrorCode = 0;
  app.ota.applyStatusText[0] = '\0';
  app.ota.state = SERVICE_FETCH_IDLE;
  app.ota.lastHttpCode = 0;
  app.ota.lastCheckMs = 0;
  markUiDirty(UI_DIRTY_HEADER);
}

void startOtaFirmwareUpdate() {
  app.ota.applyRequested = false;

  if (WiFi.status() != WL_CONNECTED) {
    setOtaApplyState(OTA_APPLY_FAILED, -1, 0, "Wi-Fi non connesso.");
    pumpOtaUi();
    return;
  }

  if (app.ota.state != SERVICE_FETCH_READY || app.ota.eligibility != OTA_ELIGIBILITY_UPDATE_AVAILABLE) {
    setOtaApplyState(OTA_APPLY_FAILED, -1, 0, "Nessun update installabile.");
    pumpOtaUi();
    return;
  }

  if (app.ota.remoteBinUrl[0] == '\0') {
    setOtaApplyState(OTA_APPLY_FAILED, -1, 0, "bin_url OTA mancante.");
    pumpOtaUi();
    return;
  }

  setOtaApplyState(OTA_APPLY_IN_PROGRESS, 0, 0, "Preparazione OTA...");
  app.ota.applyBytesCurrent = 0;
  app.ota.applyBytesTotal = 0;
  pumpOtaUi();

  WiFiClientSecure client;
  client.setInsecure();

  HTTPUpdate updater(NETWORK_OTA_DOWNLOAD_TIMEOUT_MS);
  updater.rebootOnUpdate(false);
  updater.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  updater.onStart([]() {
    setOtaApplyState(OTA_APPLY_IN_PROGRESS, 0, 0, "Download firmware...");
    app.ota.applyBytesCurrent = 0;
    app.ota.applyBytesTotal = 0;
    pumpOtaUi();
  });

  updater.onProgress([](int current, int total) {
    int percent = -1;
    if (total > 0) {
      percent = (current * 100) / total;
    }

    app.ota.applyBytesCurrent = current > 0 ? static_cast<uint32_t>(current) : 0;
    app.ota.applyBytesTotal = total > 0 ? static_cast<uint32_t>(total) : 0;

    char statusText[96];
    if (total > 0) {
      snprintf(
        statusText,
        sizeof(statusText),
        "Download firmware... %d%% (%lu/%lu KB)",
        percent,
        static_cast<unsigned long>(app.ota.applyBytesCurrent / 1024UL),
        static_cast<unsigned long>(app.ota.applyBytesTotal / 1024UL));
    } else {
      snprintf(
        statusText,
        sizeof(statusText),
        "Download firmware... %lu KB",
        static_cast<unsigned long>(app.ota.applyBytesCurrent / 1024UL));
    }
    setOtaApplyState(OTA_APPLY_IN_PROGRESS, percent, 0, statusText);
    pumpOtaUi();
  });

  updater.onEnd([]() {
    setOtaApplyState(OTA_APPLY_SUCCESS, 100, 0, "Update installato. Riavvio...");
    app.ota.applyBytesCurrent = app.ota.applyBytesTotal;
    pumpOtaUi();
  });

  updater.onError([](int errorCode) {
    char statusText[64];
    snprintf(statusText, sizeof(statusText), "Errore OTA (%d).", errorCode);
    setOtaApplyState(OTA_APPLY_FAILED, -1, errorCode, statusText);
    pumpOtaUi();
  });

  HTTPUpdateResult result = updater.update(client, String(app.ota.remoteBinUrl), String(FW_VERSION));
  if (result == HTTP_UPDATE_OK) {
    setOtaApplyState(OTA_APPLY_SUCCESS, 100, 0, "Update installato. Riavvio...");
    app.ota.applyBytesCurrent = app.ota.applyBytesTotal;
    pumpOtaUi();
    delay(500);
    ESP.restart();
    return;
  }

  if (result == HTTP_UPDATE_NO_UPDATES) {
    DEBUG_NETWORK_PRINT("[OTA] HTTPUpdate: NO_UPDATES — version match o x-MD5 mismatch");
    app.ota.eligibility = OTA_ELIGIBILITY_UP_TO_DATE;
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
