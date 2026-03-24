#include "dashboard_ui_ota_popup.h"
#include "dashboard_app.h"
#include "dashboard_services.h"
#include "dashboard_support.h"
#include "dashboard_ui_shared.h"
#include "config_ui.h"
#include "version.h"

static void buildOtaPopupText(char *buffer, size_t bufferSize) {
  char sizeBuffer[24];
  char progressBuffer[48];
  if (app.ota.remoteSizeBytes > 0) {
    snprintf(sizeBuffer, sizeof(sizeBuffer), "%lu KB", static_cast<unsigned long>(app.ota.remoteSizeBytes / 1024UL));
  } else {
    strlcpy(sizeBuffer, "n/d", sizeof(sizeBuffer));
  }

  if (app.ota.applyState == OTA_APPLY_IN_PROGRESS) {
    if (app.ota.applyBytesTotal > 0) {
      snprintf(
        progressBuffer,
        sizeof(progressBuffer),
        "%d%% (%lu/%lu KB)",
        app.ota.applyProgressPercent >= 0 ? app.ota.applyProgressPercent : 0,
        static_cast<unsigned long>(app.ota.applyBytesCurrent / 1024UL),
        static_cast<unsigned long>(app.ota.applyBytesTotal / 1024UL));
    } else if (app.ota.applyBytesCurrent > 0) {
      snprintf(
        progressBuffer,
        sizeof(progressBuffer),
        "%lu KB scaricati",
        static_cast<unsigned long>(app.ota.applyBytesCurrent / 1024UL));
    } else {
      strlcpy(progressBuffer, "avvio download...", sizeof(progressBuffer));
    }

    snprintf(buffer, bufferSize,
      "Aggiornamento in corso.\nVersione corrente: %s\nVersione remota: %s\nProgresso: %s\n%s",
      FW_VERSION,
      app.ota.remoteVersion[0] != '\0' ? app.ota.remoteVersion : "n/d",
      progressBuffer,
      app.ota.applyStatusText[0] != '\0' ? app.ota.applyStatusText : "Download firmware...");
    return;
  }

  if (app.ota.applyState == OTA_APPLY_SUCCESS) {
    snprintf(buffer, bufferSize,
      "Aggiornamento completato.\nVersione remota: %s\n%s",
      app.ota.remoteVersion[0] != '\0' ? app.ota.remoteVersion : "n/d",
      app.ota.applyStatusText[0] != '\0' ? app.ota.applyStatusText : "Riavvio imminente...");
    return;
  }

  if (app.ota.applyState == OTA_APPLY_FAILED) {
    snprintf(buffer, bufferSize,
      "Aggiornamento fallito.\nVersione remota: %s\n%s",
      app.ota.remoteVersion[0] != '\0' ? app.ota.remoteVersion : "n/d",
      app.ota.applyStatusText[0] != '\0' ? app.ota.applyStatusText : "Errore OTA.");
    return;
  }

  if (app.ota.state != SERVICE_FETCH_READY) {
    switch (app.ota.state) {
      case SERVICE_FETCH_OFFLINE:
        snprintf(buffer, bufferSize,
          "OTA manifest non raggiungibile.\nStato: offline\nWi-Fi non connesso.");
        return;
      case SERVICE_FETCH_CONFIG_MISSING:
        snprintf(buffer, bufferSize,
          "OTA non configurato.\nImposta OTA_MANIFEST_URL in secrets.h.");
        return;
      case SERVICE_FETCH_TRANSPORT_ERROR:
        snprintf(buffer, bufferSize,
          "Errore di trasporto HTTPS.\nControllo manifest fallito.");
        return;
      case SERVICE_FETCH_HTTP_ERROR:
        snprintf(buffer, bufferSize,
          "Manifest OTA non disponibile.\nHTTP %d.", app.ota.lastHttpCode);
        return;
      case SERVICE_FETCH_INVALID_PAYLOAD:
        snprintf(buffer, bufferSize,
          "Manifest OTA non valido.\nControlla il payload JSON.");
        return;
      case SERVICE_FETCH_IDLE:
      default:
        snprintf(buffer, bufferSize,
          "Controllo OTA in attesa.\nVersione corrente: %s", FW_VERSION);
        return;
    }
  }

  switch (app.ota.eligibility) {
    case OTA_ELIGIBILITY_UPDATE_AVAILABLE:
      snprintf(buffer, bufferSize,
        "Aggiornamento disponibile.\nVersione corrente: %s\nVersione remota: %s\nBuild: %s\nDimensione: %s\nBatteria minima: %d%%",
        FW_VERSION,
        app.ota.remoteVersion,
        app.ota.remoteBuild[0] != '\0' ? app.ota.remoteBuild : "n/d",
        sizeBuffer,
        app.ota.minBatteryPercent);
      return;
    case OTA_ELIGIBILITY_UP_TO_DATE:
      snprintf(buffer, bufferSize,
        "Firmware aggiornato.\nVersione corrente: %s\nVersione remota: %s",
        FW_VERSION,
        app.ota.remoteVersion);
      return;
    case OTA_ELIGIBILITY_BATTERY_TOO_LOW:
      snprintf(buffer, bufferSize,
        "Aggiornamento trovato ma batteria insufficiente.\nVersione remota: %s\nMinimo richiesto: %d%%\nBatteria attuale: %d%%",
        app.ota.remoteVersion,
        app.ota.minBatteryPercent,
        app.battery.percent);
      return;
    case OTA_ELIGIBILITY_BATTERY_UNKNOWN:
      snprintf(buffer, bufferSize,
        "Aggiornamento trovato ma livello batteria sconosciuto.\nVersione remota: %s\nMinimo richiesto: %d%%",
        app.ota.remoteVersion,
        app.ota.minBatteryPercent);
      return;
    case OTA_ELIGIBILITY_SLOT_TOO_SMALL:
      snprintf(buffer, bufferSize,
        "Manifest valido ma firmware troppo grande.\nVersione remota: %s\nDimensione: %s",
        app.ota.remoteVersion,
        sizeBuffer);
      return;
    case OTA_ELIGIBILITY_INCOMPATIBLE_BOARD:
      snprintf(buffer, bufferSize,
        "Manifest OTA per board diversa.\nAttesa: %s\nRemota: %s",
        FW_BOARD_ID,
        app.ota.remoteVersion);
      return;
    case OTA_ELIGIBILITY_INCOMPATIBLE_CHANNEL:
      snprintf(buffer, bufferSize,
        "Manifest OTA su canale diverso.\nCanale locale: %s\nVersione remota: %s",
        FW_RELEASE_CHANNEL,
        app.ota.remoteVersion);
      return;
    case OTA_ELIGIBILITY_INVALID:
    default:
      snprintf(buffer, bufferSize,
        "Stato OTA non valido.\nManifest ricevuto ma non applicabile.");
      return;
  }
}

void destroyOtaPopup() {
  if (ui.otaPopupOverlay != nullptr) {
    lv_obj_del(ui.otaPopupOverlay);
    ui.otaPopupOverlay = nullptr;
    ui.otaPopupBodyLabel = nullptr;
    ui.otaPopupActionButton = nullptr;
    ui.otaPopupActionLabel = nullptr;
    if (ui.headerTouchOverlay != nullptr) {
      lv_obj_clear_flag(ui.headerTouchOverlay, LV_OBJ_FLAG_HIDDEN);
      lv_obj_move_foreground(ui.headerTouchOverlay);
    }
  }
}

static void otaPopupCloseEventCb(lv_event_t *e) {
  if (lv_event_get_target(e) == ui.otaPopupActionButton || lv_event_get_target(e) == ui.otaPopupActionLabel) {
    return;
  }
  if (lv_event_get_target(e) != ui.otaPopupOverlay
    && lv_event_get_current_target(e) == ui.otaPopupOverlay) {
    return;
  }

  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_PRESSED && code != LV_EVENT_CLICKED && code != LV_EVENT_RELEASED && code != LV_EVENT_SHORT_CLICKED) {
    return;
  }
  destroyOtaPopup();
}

static void otaPopupDeletedEventCb(lv_event_t *e) {
  LV_UNUSED(e);
  ui.otaPopupOverlay = nullptr;
  ui.otaPopupBodyLabel = nullptr;
  ui.otaPopupActionButton = nullptr;
  ui.otaPopupActionLabel = nullptr;
}

void refreshOtaPopupUi() {
  if (ui.otaPopupBodyLabel == nullptr) {
    return;
  }

  char bodyText[320];
  buildOtaPopupText(bodyText, sizeof(bodyText));
  setDashboardLabelTextIfChanged(ui.otaPopupBodyLabel, bodyText);

  if (ui.otaPopupActionButton == nullptr || ui.otaPopupActionLabel == nullptr) {
    return;
  }

  if (app.ota.applyState == OTA_APPLY_IN_PROGRESS || app.ota.applyState == OTA_APPLY_SUCCESS) {
    lv_obj_add_flag(ui.otaPopupActionButton, LV_OBJ_FLAG_HIDDEN);
    return;
  }

  lv_obj_clear_flag(ui.otaPopupActionButton, LV_OBJ_FLAG_HIDDEN);

  if (app.ota.state == SERVICE_FETCH_READY && app.ota.eligibility == OTA_ELIGIBILITY_UPDATE_AVAILABLE) {
    setDashboardLabelTextIfChanged(
      ui.otaPopupActionLabel,
      app.ota.applyState == OTA_APPLY_FAILED ? "Riprova" : "Aggiorna");
    return;
  }

  setDashboardLabelTextIfChanged(ui.otaPopupActionLabel, "Controlla");
}

static void otaPopupActionEventCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED && code != LV_EVENT_SHORT_CLICKED) {
    return;
  }

  if (app.ota.applyRequested || app.ota.applyState == OTA_APPLY_IN_PROGRESS) {
    return;
  }

  if (app.ota.state != SERVICE_FETCH_READY || app.ota.eligibility != OTA_ELIGIBILITY_UPDATE_AVAILABLE) {
    requestOtaManifestRefresh();
    refreshOtaPopupUi();
    return;
  }

  app.ota.applyRequested = true;
  app.ota.applyState = OTA_APPLY_IN_PROGRESS;
  app.ota.applyProgressPercent = 0;
  app.ota.applyLastErrorCode = 0;
  strlcpy(app.ota.applyStatusText, "Preparazione OTA...", sizeof(app.ota.applyStatusText));
  markUiDirty(UI_DIRTY_HEADER);
  refreshOtaPopupUi();
}

void showOtaPopup() {
  if (ui.otaPopupOverlay != nullptr) {
    refreshOtaPopupUi();
    lv_obj_move_foreground(ui.otaPopupOverlay);
    return;
  }

  if (ui.headerTouchOverlay != nullptr) {
    lv_obj_add_flag(ui.headerTouchOverlay, LV_OBJ_FLAG_HIDDEN);
  }

  lv_obj_t *overlay = lv_obj_create(lv_layer_top());
  ui.otaPopupOverlay = overlay;
  lv_obj_set_size(overlay, LV_PCT(100), LV_PCT(100));
  lv_obj_set_scrollbar_mode(overlay, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_bg_color(overlay, colorFromHex(0x0F172A), 0);
  lv_obj_set_style_bg_opa(overlay, LV_OPA_60, 0);
  lv_obj_set_style_border_width(overlay, 0, 0);
  lv_obj_set_style_radius(overlay, 0, 0);
  lv_obj_set_style_pad_all(overlay, 0, 0);
  lv_obj_clear_flag(overlay, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_event_cb(overlay, otaPopupDeletedEventCb, LV_EVENT_DELETE, nullptr);
  lv_obj_add_event_cb(overlay, otaPopupCloseEventCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(overlay, otaPopupCloseEventCb, LV_EVENT_SHORT_CLICKED, nullptr);

  lv_obj_t *panel = lv_obj_create(overlay);
  lv_obj_set_size(panel, 244, 188);
  lv_obj_center(panel);
  lv_obj_set_scrollbar_mode(panel, LV_SCROLLBAR_MODE_OFF);
  lv_obj_set_style_pad_all(panel, 12, 0);
  lv_obj_set_style_pad_row(panel, 8, 0);
  lv_obj_set_style_bg_color(panel, colorFromHex(UI_COLOR_MAIN_BG), 0);
  lv_obj_set_style_border_color(panel, colorFromHex(UI_COLOR_MAIN_BORDER), 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_radius(panel, 8, 0);
  lv_obj_clear_flag(panel, LV_OBJ_FLAG_SCROLLABLE);

  lv_obj_t *title = lv_label_create(panel);
  lv_label_set_text(title, "OTA Status");
  setDashboardLabelFont(title, &lv_font_montserrat_16);
  setDashboardLabelColor(title, UI_COLOR_TEXT_PRIMARY);
  lv_obj_align(title, LV_ALIGN_TOP_LEFT, 0, 0);

  lv_obj_t *closeButton = lv_btn_create(panel);
  lv_obj_set_size(closeButton, 34, 28);
  lv_obj_align(closeButton, LV_ALIGN_TOP_RIGHT, 0, -4);
  lv_obj_set_style_bg_color(closeButton, colorFromHex(UI_COLOR_HEADER_BG), 0);
  lv_obj_set_style_border_width(closeButton, 0, 0);
  lv_obj_set_style_radius(closeButton, 14, 0);
  lv_obj_clear_flag(closeButton, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(closeButton, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(closeButton, 10);
  lv_obj_add_event_cb(closeButton, otaPopupCloseEventCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(closeButton, otaPopupCloseEventCb, LV_EVENT_SHORT_CLICKED, nullptr);
  lv_obj_add_event_cb(closeButton, otaPopupCloseEventCb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(closeButton, otaPopupCloseEventCb, LV_EVENT_PRESSED, nullptr);

  lv_obj_t *closeLabel = lv_label_create(closeButton);
  lv_label_set_text(closeLabel, LV_SYMBOL_CLOSE);
  setDashboardLabelFont(closeLabel, &lv_font_montserrat_12);
  setDashboardLabelColor(closeLabel, UI_COLOR_TEXT_LIGHT);
  lv_obj_center(closeLabel);
  lv_obj_add_flag(closeLabel, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(closeLabel, otaPopupCloseEventCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(closeLabel, otaPopupCloseEventCb, LV_EVENT_SHORT_CLICKED, nullptr);
  lv_obj_add_event_cb(closeLabel, otaPopupCloseEventCb, LV_EVENT_RELEASED, nullptr);
  lv_obj_add_event_cb(closeLabel, otaPopupCloseEventCb, LV_EVENT_PRESSED, nullptr);

  ui.otaPopupBodyLabel = lv_label_create(panel);
  lv_obj_set_width(ui.otaPopupBodyLabel, 220);
  lv_label_set_long_mode(ui.otaPopupBodyLabel, LV_LABEL_LONG_WRAP);
  setDashboardLabelFont(ui.otaPopupBodyLabel, &lv_font_montserrat_12);
  setDashboardLabelColor(ui.otaPopupBodyLabel, UI_COLOR_TEXT_SECONDARY);
  lv_obj_align(ui.otaPopupBodyLabel, LV_ALIGN_TOP_LEFT, 0, 30);

  ui.otaPopupActionButton = lv_btn_create(panel);
  lv_obj_set_size(ui.otaPopupActionButton, 96, 30);
  lv_obj_align(ui.otaPopupActionButton, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(ui.otaPopupActionButton, colorFromHex(UI_COLOR_ACCENT), 0);
  lv_obj_set_style_bg_opa(ui.otaPopupActionButton, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(ui.otaPopupActionButton, 0, 0);
  lv_obj_set_style_shadow_width(ui.otaPopupActionButton, 0, 0);
  lv_obj_set_style_radius(ui.otaPopupActionButton, 14, 0);
  lv_obj_clear_flag(ui.otaPopupActionButton, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_add_flag(ui.otaPopupActionButton, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_ext_click_area(ui.otaPopupActionButton, 10);
  lv_obj_add_event_cb(ui.otaPopupActionButton, otaPopupActionEventCb, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(ui.otaPopupActionButton, otaPopupActionEventCb, LV_EVENT_SHORT_CLICKED, nullptr);

  ui.otaPopupActionLabel = lv_label_create(ui.otaPopupActionButton);
  lv_label_set_text(ui.otaPopupActionLabel, "Aggiorna");
  setDashboardLabelFont(ui.otaPopupActionLabel, &lv_font_montserrat_12);
  setDashboardLabelColor(ui.otaPopupActionLabel, UI_COLOR_TEXT_LIGHT);
  lv_obj_center(ui.otaPopupActionLabel);

  refreshOtaPopupUi();
}
