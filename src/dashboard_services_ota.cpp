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
#include <Update.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "secrets.h"
#include "mbedtls/sha256.h"

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

  setOtaApplyState(OTA_APPLY_IN_PROGRESS, 0, 0, "Connessione al server OTA...");
  app.ota.applyBytesCurrent = 0;
  app.ota.applyBytesTotal = 0;
  pumpOtaUi();

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.setTimeout(NETWORK_OTA_DOWNLOAD_TIMEOUT_MS);
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS);

  if (!http.begin(client, String(app.ota.remoteBinUrl))) {
    setOtaApplyState(OTA_APPLY_FAILED, -1, 0, "Connessione al server OTA fallita.");
    pumpOtaUi();
    return;
  }

  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    char statusText[64];
    snprintf(statusText, sizeof(statusText), "Errore HTTP OTA: %d.", httpCode);
    setOtaApplyState(OTA_APPLY_FAILED, -1, httpCode, statusText);
    http.end();
    pumpOtaUi();
    return;
  }

  int contentLength = http.getSize();
  Stream *stream = http.getStreamPtr();

  if (!Update.begin(contentLength > 0 ? (size_t)contentLength : UPDATE_SIZE_UNKNOWN)) {
    char statusText[64];
    snprintf(statusText, sizeof(statusText), "Errore avvio flash (%d).", (int)Update.getError());
    setOtaApplyState(OTA_APPLY_FAILED, -1, (int)Update.getError(), statusText);
    http.end();
    pumpOtaUi();
    return;
  }

  setOtaApplyState(OTA_APPLY_IN_PROGRESS, 0, 0, "Download firmware...");
  app.ota.applyBytesTotal = contentLength > 0 ? (uint32_t)contentLength : 0;
  pumpOtaUi();

  mbedtls_sha256_context sha;
  mbedtls_sha256_init(&sha);
  mbedtls_sha256_starts(&sha, 0);

  static uint8_t buf[1024];
  int totalWritten = 0;
  unsigned long lastPumpMs = millis();
  unsigned long lastDataMs = millis();
  bool streamError = false;

  while (http.connected() && (contentLength < 0 || totalWritten < contentLength)) {
    size_t available = stream->available();
    if (available == 0) {
      if (millis() - lastDataMs > 15000UL) {
        streamError = true;
        break;
      }
      delay(1);
      continue;
    }
    lastDataMs = millis();

    size_t toRead = available < sizeof(buf) ? available : sizeof(buf);
    size_t bytesRead = stream->readBytes(buf, toRead);
    if (bytesRead == 0) {
      streamError = true;
      break;
    }

    mbedtls_sha256_update(&sha, buf, bytesRead);
    if (Update.write(buf, bytesRead) != bytesRead) {
      streamError = true;
      break;
    }

    totalWritten += (int)bytesRead;
    app.ota.applyBytesCurrent = (uint32_t)totalWritten;

    if (millis() - lastPumpMs > 200UL) {
      int percent = contentLength > 0 ? (totalWritten * 100) / contentLength : -1;
      char statusText[96];
      if (contentLength > 0) {
        snprintf(statusText, sizeof(statusText),
          "Download firmware... %d%% (%lu/%lu KB)",
          percent,
          (unsigned long)(totalWritten / 1024),
          (unsigned long)(contentLength / 1024));
      } else {
        snprintf(statusText, sizeof(statusText),
          "Download firmware... %lu KB",
          (unsigned long)(totalWritten / 1024));
      }
      setOtaApplyState(OTA_APPLY_IN_PROGRESS, percent, 0, statusText);
      pumpOtaUi();
      lastPumpMs = millis();
    }
  }

  http.end();

  uint8_t computedHash[32];
  mbedtls_sha256_finish(&sha, computedHash);
  mbedtls_sha256_free(&sha);

  if (streamError || (contentLength > 0 && totalWritten != contentLength)) {
    Update.abort();
    setOtaApplyState(OTA_APPLY_FAILED, -1, 0, "Download interrotto o incompleto.");
    pumpOtaUi();
    return;
  }

  if (app.ota.remoteSha256[0] != '\0' && strlen(app.ota.remoteSha256) == 64) {
    bool hashOk = true;
    for (int i = 0; i < 32 && hashOk; i++) {
      char hex[3] = { app.ota.remoteSha256[i * 2], app.ota.remoteSha256[i * 2 + 1], '\0' };
      if ((uint8_t)strtol(hex, nullptr, 16) != computedHash[i]) {
        hashOk = false;
      }
    }
    if (!hashOk) {
      Update.abort();
      setOtaApplyState(OTA_APPLY_FAILED, -1, -1, "Checksum SHA256 non valido. Update annullato.");
      pumpOtaUi();
      return;
    }
    DEBUG_NETWORK_PRINT("[OTA] SHA256 verificato.");
  } else {
    DEBUG_NETWORK_PRINT("[OTA] SHA256 assente nel manifest, skip verifica.");
  }

  if (!Update.end(true)) {
    char statusText[64];
    snprintf(statusText, sizeof(statusText), "Errore finalizzazione flash (%d).", (int)Update.getError());
    setOtaApplyState(OTA_APPLY_FAILED, -1, (int)Update.getError(), statusText);
    pumpOtaUi();
    return;
  }

  setOtaApplyState(OTA_APPLY_SUCCESS, 100, 0, "Update installato. Riavvio...");
  app.ota.applyBytesCurrent = app.ota.applyBytesTotal;
  pumpOtaUi();
  delay(500);
  ESP.restart();
}
