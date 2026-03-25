#include "dashboard_ui_settings.h"
#include "dashboard_app.h"
#include "dashboard_settings.h"
#include "dashboard_energy.h"
#include "dashboard_services.h"
#include "dashboard_support.h"
#include "dashboard_ui_shared.h"
#include "config_ui.h"
#include "version.h"
#include "esp_bt.h"
#include <WiFi.h>

static constexpr lv_coord_t SETTINGS_ROW_H = 36;
static constexpr lv_coord_t SETTINGS_STEP_BTN_W = 30;
static constexpr lv_coord_t SETTINGS_STEP_BTN_H = 30;
static constexpr lv_coord_t SETTINGS_STEP_VAL_W = 42;

static lv_obj_t *sEnergySwitch = nullptr;
static lv_obj_t *sStartTimeLabel = nullptr;
static lv_obj_t *sEndTimeLabel = nullptr;
static lv_obj_t *sPowerModeLabel = nullptr;
static lv_obj_t *sTapWakeSwitch = nullptr;
static lv_obj_t *sTimeoutLabel = nullptr;
static lv_obj_t *sWifiSwitch = nullptr;
static lv_obj_t *sBluetoothSwitch = nullptr;
static lv_obj_t *sWeatherSwitch = nullptr;
static lv_obj_t *sNewsSwitch = nullptr;
static lv_obj_t *sSysVersionLabel = nullptr;
static lv_obj_t *sSysWifiLabel = nullptr;
static lv_obj_t *sSysNtpLabel = nullptr;
static lv_obj_t *sSysOtaLabel = nullptr;
static lv_obj_t *sSysUptimeLabel = nullptr;

// ---- Text helpers ----

static const char *powerModeText(uint8_t mode) {
  switch (mode) {
    case 0: return "solo display";
    case 1: return "display+Wi-Fi";
    case 2: return "deep sleep";
    default: return "--";
  }
}

static const char *otaStatusText() {
  if (app.ota.applyState == OTA_APPLY_IN_PROGRESS) {
    return "installazione";
  }
  if (app.ota.applyState == OTA_APPLY_FAILED) {
    return "errore update";
  }
  if (app.ota.applyState == OTA_APPLY_SUCCESS) {
    return "riavvio";
  }

  switch (app.ota.state) {
    case SERVICE_FETCH_IDLE:
      return "in attesa";
    case SERVICE_FETCH_FETCHING:
      return "controllo...";
    case SERVICE_FETCH_OFFLINE:
      return "offline";
    case SERVICE_FETCH_CONFIG_MISSING:
      return "non configurato";
    case SERVICE_FETCH_TRANSPORT_ERROR:
      return "errore rete";
    case SERVICE_FETCH_HTTP_ERROR:
      return "errore http";
    case SERVICE_FETCH_INVALID_PAYLOAD:
      return "manifest errato";
    case SERVICE_FETCH_READY:
      break;
  }

  switch (app.ota.eligibility) {
    case OTA_ELIGIBILITY_UPDATE_AVAILABLE:
      return "update disponibile";
    case OTA_ELIGIBILITY_UP_TO_DATE:
      return "aggiornato";
    case OTA_ELIGIBILITY_BATTERY_TOO_LOW:
      return "batteria bassa";
    case OTA_ELIGIBILITY_BATTERY_UNKNOWN:
      return "batteria ignota";
    case OTA_ELIGIBILITY_SLOT_TOO_SMALL:
      return "slot insufficiente";
    case OTA_ELIGIBILITY_INCOMPATIBLE_BOARD:
      return "board errata";
    case OTA_ELIGIBILITY_INCOMPATIBLE_CHANNEL:
      return "canale errato";
    case OTA_ELIGIBILITY_INVALID:
    default:
      return "stato non valido";
  }
}

// ---- Control refresh ----

void refreshSettingsModuleTile() {
  if (sEnergySwitch != nullptr) {
    if (app.settings.energyScheduleEnabled) {
      lv_obj_add_state(sEnergySwitch, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(sEnergySwitch, LV_STATE_CHECKED);
    }
  }
  if (sStartTimeLabel != nullptr) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d",
      app.settings.activeStartHour, app.settings.activeStartMinute);
    setDashboardLabelTextIfChanged(sStartTimeLabel, buf);
  }
  if (sEndTimeLabel != nullptr) {
    char buf[8];
    snprintf(buf, sizeof(buf), "%02d:%02d",
      app.settings.activeEndHour, app.settings.activeEndMinute);
    setDashboardLabelTextIfChanged(sEndTimeLabel, buf);
  }
  if (sPowerModeLabel != nullptr) {
    setDashboardLabelTextIfChanged(sPowerModeLabel, powerModeText(app.settings.powerMode));
  }
  if (sTapWakeSwitch != nullptr) {
    if (app.settings.tapWakeEnabled) {
      lv_obj_add_state(sTapWakeSwitch, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(sTapWakeSwitch, LV_STATE_CHECKED);
    }
  }
  if (sTimeoutLabel != nullptr) {
    char buf[10];
    snprintf(buf, sizeof(buf), "%d min", app.settings.inactivityTimeoutMinutes);
    setDashboardLabelTextIfChanged(sTimeoutLabel, buf);
  }
  if (sWifiSwitch != nullptr) {
    if (app.settings.wifiEnabled) {
      lv_obj_add_state(sWifiSwitch, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(sWifiSwitch, LV_STATE_CHECKED);
    }
  }
  if (sBluetoothSwitch != nullptr) {
    if (app.settings.bluetoothEnabled) {
      lv_obj_add_state(sBluetoothSwitch, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(sBluetoothSwitch, LV_STATE_CHECKED);
    }
  }
  if (sWeatherSwitch != nullptr) {
    if (app.settings.weatherEnabled) {
      lv_obj_add_state(sWeatherSwitch, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(sWeatherSwitch, LV_STATE_CHECKED);
    }
  }
  if (sNewsSwitch != nullptr) {
    if (app.settings.newsEnabled) {
      lv_obj_add_state(sNewsSwitch, LV_STATE_CHECKED);
    } else {
      lv_obj_clear_state(sNewsSwitch, LV_STATE_CHECKED);
    }
  }
  if (sSysVersionLabel != nullptr) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%s · %s", FW_VERSION, FW_BOARD_ID);
    setDashboardLabelTextIfChanged(sSysVersionLabel, buf);
  }
  if (sSysWifiLabel != nullptr) {
    char buf[24];
    if (WiFi.status() == WL_CONNECTED) {
      snprintf(buf, sizeof(buf), "ok · %d dBm", WiFi.RSSI());
    } else {
      strlcpy(buf, "disconnesso", sizeof(buf));
    }
    setDashboardLabelTextIfChanged(sSysWifiLabel, buf);
  }
  if (sSysNtpLabel != nullptr) {
    setDashboardLabelTextIfChanged(sSysNtpLabel,
      app.clock.synced ? "sincronizzato" : "in attesa");
  }
  if (sSysOtaLabel != nullptr) {
    setDashboardLabelTextIfChanged(sSysOtaLabel, otaStatusText());
  }
  if (sSysUptimeLabel != nullptr) {
    char buf[16];
    unsigned long secs = millis() / 1000;
    unsigned long h = secs / 3600;
    unsigned long m = (secs % 3600) / 60;
    if (h > 0) {
      snprintf(buf, sizeof(buf), "%luh %02lum", h, m);
    } else {
      snprintf(buf, sizeof(buf), "%lum %02lus", m, secs % 60);
    }
    setDashboardLabelTextIfChanged(sSysUptimeLabel, buf);
  }
}

// ---- Event callbacks ----

static void commitSwitchState(lv_obj_t *sw, bool enabled) {
  if (enabled) {
    lv_obj_add_state(sw, LV_STATE_CHECKED);
  } else {
    lv_obj_clear_state(sw, LV_STATE_CHECKED);
  }
  lv_event_send(sw, LV_EVENT_VALUE_CHANGED, nullptr);
}

static void toggleSwitchFromRowCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED && code != LV_EVENT_SHORT_CLICKED) return;

  lv_obj_t *sw = static_cast<lv_obj_t *>(lv_event_get_user_data(e));
  if (sw == nullptr) return;
  commitSwitchState(sw, !lv_obj_has_state(sw, LV_STATE_CHECKED));
}

static void energySwitchCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  app.settings.energyScheduleEnabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  saveSettings();
  refreshSettingsModuleTile();
}

static void tapWakeSwitchCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  app.settings.tapWakeEnabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  saveSettings();
}

static void wifiSwitchCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  app.settings.wifiEnabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  applyWifiUserEnabled(app.settings.wifiEnabled);
  markUiDirty(UI_DIRTY_HEADER);
  saveSettings();
}

static void bluetoothSwitchCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  app.settings.bluetoothEnabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  if (app.settings.bluetoothEnabled) {
    btStart();
  } else {
    btStop();
  }
  markUiDirty(UI_DIRTY_HEADER);
  saveSettings();
}

static void weatherSwitchCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  app.settings.weatherEnabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  saveSettings();
}

static void newsSwitchCb(lv_event_t *e) {
  if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) return;
  app.settings.newsEnabled = lv_obj_has_state(lv_event_get_target(e), LV_STATE_CHECKED);
  saveSettings();
}

static void otaCheckCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED && code != LV_EVENT_SHORT_CLICKED) return;
  requestOtaManifestRefresh();
  refreshSettingsModuleTile();
}

enum SettingsStepAction : uint8_t {
  STEP_START_HOUR_INC = 0,
  STEP_START_HOUR_DEC,
  STEP_END_HOUR_INC,
  STEP_END_HOUR_DEC,
  STEP_TIMEOUT_INC,
  STEP_TIMEOUT_DEC,
};

static void stepCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED && code != LV_EVENT_SHORT_CLICKED) return;
  auto action = static_cast<SettingsStepAction>((uintptr_t)lv_event_get_user_data(e));
  switch (action) {
    case STEP_START_HOUR_INC:
      app.settings.activeStartHour = (app.settings.activeStartHour + 1) % 24;
      break;
    case STEP_START_HOUR_DEC:
      app.settings.activeStartHour = (app.settings.activeStartHour + 23) % 24;
      break;
    case STEP_END_HOUR_INC:
      app.settings.activeEndHour = (app.settings.activeEndHour + 1) % 24;
      break;
    case STEP_END_HOUR_DEC:
      app.settings.activeEndHour = (app.settings.activeEndHour + 23) % 24;
      break;
    case STEP_TIMEOUT_INC:
      if (app.settings.inactivityTimeoutMinutes < 60) {
        app.settings.inactivityTimeoutMinutes += 1;
      }
      break;
    case STEP_TIMEOUT_DEC:
      if (app.settings.inactivityTimeoutMinutes > 1) {
        app.settings.inactivityTimeoutMinutes -= 1;
      }
      break;
  }
  saveSettings();
  refreshSettingsModuleTile();
}

static void powerModeCycleCb(lv_event_t *e) {
  lv_event_code_t code = lv_event_get_code(e);
  if (code != LV_EVENT_CLICKED && code != LV_EVENT_SHORT_CLICKED) return;
  app.settings.powerMode = (app.settings.powerMode + 1) % 3;
  saveSettings();
  refreshSettingsModuleTile();
}

// ---- Layout helpers ----

static lv_obj_t *createRow(lv_obj_t *parent) {
  lv_obj_t *row = lv_obj_create(parent);
  lv_obj_set_size(row, LV_PCT(100), LV_SIZE_CONTENT);
  lv_obj_set_style_min_height(row, SETTINGS_ROW_H, 0);
  lv_obj_set_style_bg_opa(row, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_hor(row, 8, 0);
  lv_obj_set_style_pad_ver(row, 4, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_clear_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  return row;
}

static void createRowLabel(lv_obj_t *row, const char *text) {
  lv_obj_t *label = lv_label_create(row);
  lv_label_set_text(label, text);
  setDashboardLabelFont(label, &lv_font_montserrat_12);
  setDashboardLabelColor(label, UI_COLOR_TEXT_PRIMARY);
  lv_obj_set_flex_grow(label, 1);
}

static void createSeparator(lv_obj_t *parent) {
  lv_obj_t *sep = lv_obj_create(parent);
  lv_obj_set_size(sep, LV_PCT(100), 1);
  lv_obj_set_style_bg_color(sep, colorFromHex(UI_COLOR_MAIN_BORDER), 0);
  lv_obj_set_style_border_width(sep, 0, 0);
  lv_obj_set_style_pad_all(sep, 0, 0);
  lv_obj_clear_flag(sep, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
}

static lv_obj_t *createSwitch(lv_obj_t *row) {
  lv_obj_t *sw = lv_switch_create(row);
  lv_obj_set_size(sw, 46, 24);
  lv_obj_set_style_bg_color(sw, colorFromHex(0xCBD5E1), 0);
  lv_obj_set_style_bg_color(sw, colorFromHex(UI_COLOR_ACCENT), LV_STATE_CHECKED);
  lv_obj_set_ext_click_area(sw, 8);
  return sw;
}

static lv_obj_t *createActionButton(lv_obj_t *row, const char *text, lv_event_cb_t cb) {
  lv_obj_t *btn = lv_btn_create(row);
  lv_obj_set_size(btn, 84, 28);
  lv_obj_set_style_bg_color(btn, colorFromHex(UI_COLOR_HEADER_BG), 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_radius(btn, 6, 0);
  lv_obj_set_style_pad_all(btn, 4, 0);
  lv_obj_set_ext_click_area(btn, 6);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_CLICKED, nullptr);
  lv_obj_add_event_cb(btn, cb, LV_EVENT_SHORT_CLICKED, nullptr);

  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, text);
  setDashboardLabelFont(lbl, &lv_font_montserrat_10);
  setDashboardLabelColor(lbl, UI_COLOR_TEXT_INFO);
  lv_obj_center(lbl);
  return btn;
}

static lv_obj_t *createStepBtn(lv_obj_t *row, const char *sym, SettingsStepAction action) {
  lv_obj_t *btn = lv_btn_create(row);
  lv_obj_set_size(btn, SETTINGS_STEP_BTN_W, SETTINGS_STEP_BTN_H);
  lv_obj_set_style_bg_color(btn, colorFromHex(UI_COLOR_HEADER_BG), 0);
  lv_obj_set_style_border_width(btn, 0, 0);
  lv_obj_set_style_shadow_width(btn, 0, 0);
  lv_obj_set_style_radius(btn, 6, 0);
  lv_obj_set_style_pad_all(btn, 0, 0);
  lv_obj_set_ext_click_area(btn, 6);
  lv_obj_add_event_cb(btn, stepCb, LV_EVENT_CLICKED, (void *)(uintptr_t)action);
  lv_obj_add_event_cb(btn, stepCb, LV_EVENT_SHORT_CLICKED, (void *)(uintptr_t)action);
  lv_obj_t *lbl = lv_label_create(btn);
  lv_label_set_text(lbl, sym);
  setDashboardLabelFont(lbl, &lv_font_montserrat_14);
  setDashboardLabelColor(lbl, UI_COLOR_TEXT_LIGHT);
  lv_obj_center(lbl);
  return btn;
}

static lv_obj_t *createStepValueLabel(lv_obj_t *row) {
  lv_obj_t *lbl = lv_label_create(row);
  lv_label_set_text(lbl, "--");
  setDashboardLabelFont(lbl, &lv_font_montserrat_12);
  setDashboardLabelColor(lbl, UI_COLOR_TEXT_SECONDARY);
  lv_obj_set_width(lbl, SETTINGS_STEP_VAL_W);
  lv_obj_set_style_text_align(lbl, LV_TEXT_ALIGN_CENTER, LV_PART_MAIN);
  return lbl;
}

// ---- Tile ----

void createSettingsModuleTile(lv_obj_t *tile) {
  lv_obj_set_style_pad_all(tile, 8, 0);
  lv_obj_set_style_pad_row(tile, 2, 0);
  lv_obj_add_flag(tile, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_scrollbar_mode(tile, LV_SCROLLBAR_MODE_ACTIVE);

  // Row: Pianificazione energetica
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Pianificazione energetica");
    sEnergySwitch = createSwitch(row);
    lv_obj_add_event_cb(sEnergySwitch, energySwitchCb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(row, toggleSwitchFromRowCb, LV_EVENT_CLICKED, sEnergySwitch);
    lv_obj_add_event_cb(row, toggleSwitchFromRowCb, LV_EVENT_SHORT_CLICKED, sEnergySwitch);
  }

  createSeparator(tile);

  // Row: Attivo da
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Attivo da");
    createStepBtn(row, "-", STEP_START_HOUR_DEC);
    sStartTimeLabel = createStepValueLabel(row);
    createStepBtn(row, "+", STEP_START_HOUR_INC);
  }

  // Row: Attivo fino a
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Attivo fino a");
    createStepBtn(row, "-", STEP_END_HOUR_DEC);
    sEndTimeLabel = createStepValueLabel(row);
    createStepBtn(row, "+", STEP_END_HOUR_INC);
  }

  createSeparator(tile);

  // Row: Modalita energia
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Modalita energia");
    lv_obj_t *modeBtn = lv_btn_create(row);
    lv_obj_set_size(modeBtn, 114, 28);
    lv_obj_set_style_bg_color(modeBtn, colorFromHex(UI_COLOR_HEADER_BG), 0);
    lv_obj_set_style_border_width(modeBtn, 0, 0);
    lv_obj_set_style_shadow_width(modeBtn, 0, 0);
    lv_obj_set_style_radius(modeBtn, 6, 0);
    lv_obj_set_style_pad_all(modeBtn, 4, 0);
    lv_obj_set_ext_click_area(modeBtn, 4);
    lv_obj_add_event_cb(modeBtn, powerModeCycleCb, LV_EVENT_CLICKED, nullptr);
    lv_obj_add_event_cb(modeBtn, powerModeCycleCb, LV_EVENT_SHORT_CLICKED, nullptr);
    sPowerModeLabel = lv_label_create(modeBtn);
    lv_label_set_text(sPowerModeLabel, "--");
    setDashboardLabelFont(sPowerModeLabel, &lv_font_montserrat_10);
    setDashboardLabelColor(sPowerModeLabel, UI_COLOR_TEXT_INFO);
    lv_obj_center(sPowerModeLabel);
  }

  createSeparator(tile);

  // Row: Wake con tap
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Wake con tap");
    sTapWakeSwitch = createSwitch(row);
    lv_obj_add_event_cb(sTapWakeSwitch, tapWakeSwitchCb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(row, toggleSwitchFromRowCb, LV_EVENT_CLICKED, sTapWakeSwitch);
    lv_obj_add_event_cb(row, toggleSwitchFromRowCb, LV_EVENT_SHORT_CLICKED, sTapWakeSwitch);
  }

  // Row: Timeout inattivita
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Timeout inattivita");
    createStepBtn(row, "-", STEP_TIMEOUT_DEC);
    sTimeoutLabel = createStepValueLabel(row);
    createStepBtn(row, "+", STEP_TIMEOUT_INC);
  }

  createSeparator(tile);

  // Row: Wi-Fi
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Wi-Fi");
    sWifiSwitch = createSwitch(row);
    lv_obj_add_event_cb(sWifiSwitch, wifiSwitchCb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(row, toggleSwitchFromRowCb, LV_EVENT_CLICKED, sWifiSwitch);
    lv_obj_add_event_cb(row, toggleSwitchFromRowCb, LV_EVENT_SHORT_CLICKED, sWifiSwitch);
  }

  // Row: Bluetooth
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Bluetooth");
    sBluetoothSwitch = createSwitch(row);
    lv_obj_add_event_cb(sBluetoothSwitch, bluetoothSwitchCb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(row, toggleSwitchFromRowCb, LV_EVENT_CLICKED, sBluetoothSwitch);
    lv_obj_add_event_cb(row, toggleSwitchFromRowCb, LV_EVENT_SHORT_CLICKED, sBluetoothSwitch);
  }

  createSeparator(tile);

  // Row: Fetch meteo
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Fetch meteo");
    sWeatherSwitch = createSwitch(row);
    lv_obj_add_event_cb(sWeatherSwitch, weatherSwitchCb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(row, toggleSwitchFromRowCb, LV_EVENT_CLICKED, sWeatherSwitch);
    lv_obj_add_event_cb(row, toggleSwitchFromRowCb, LV_EVENT_SHORT_CLICKED, sWeatherSwitch);
  }

  // Row: Fetch news
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Fetch news");
    sNewsSwitch = createSwitch(row);
    lv_obj_add_event_cb(sNewsSwitch, newsSwitchCb, LV_EVENT_VALUE_CHANGED, nullptr);
    lv_obj_add_event_cb(row, toggleSwitchFromRowCb, LV_EVENT_CLICKED, sNewsSwitch);
    lv_obj_add_event_cb(row, toggleSwitchFromRowCb, LV_EVENT_SHORT_CLICKED, sNewsSwitch);
  }

  createSeparator(tile);

  // Section header: Sistema
  {
    lv_obj_t *row = createRow(tile);
    lv_obj_t *label = lv_label_create(row);
    lv_label_set_text(label, "SISTEMA");
    setDashboardLabelFont(label, &lv_font_montserrat_10);
    setDashboardLabelColor(label, UI_COLOR_TEXT_MUTED);
  }

  // Row: Firmware
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Firmware");
    sSysVersionLabel = lv_label_create(row);
    lv_label_set_text(sSysVersionLabel, "--");
    setDashboardLabelFont(sSysVersionLabel, &lv_font_montserrat_12);
    setDashboardLabelColor(sSysVersionLabel, UI_COLOR_TEXT_SECONDARY);
  }

  // Row: Controllo OTA
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Controllo OTA");
    createActionButton(row, "Verifica", otaCheckCb);
  }

  // Row: Wi-Fi status
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Wi-Fi");
    sSysWifiLabel = lv_label_create(row);
    lv_label_set_text(sSysWifiLabel, "--");
    setDashboardLabelFont(sSysWifiLabel, &lv_font_montserrat_12);
    setDashboardLabelColor(sSysWifiLabel, UI_COLOR_TEXT_SECONDARY);
  }

  // Row: NTP
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "NTP");
    sSysNtpLabel = lv_label_create(row);
    lv_label_set_text(sSysNtpLabel, "--");
    setDashboardLabelFont(sSysNtpLabel, &lv_font_montserrat_12);
    setDashboardLabelColor(sSysNtpLabel, UI_COLOR_TEXT_SECONDARY);
  }

  // Row: Stato OTA
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Stato OTA");
    sSysOtaLabel = lv_label_create(row);
    lv_label_set_text(sSysOtaLabel, "--");
    setDashboardLabelFont(sSysOtaLabel, &lv_font_montserrat_12);
    setDashboardLabelColor(sSysOtaLabel, UI_COLOR_TEXT_SECONDARY);
  }

  // Row: Uptime
  {
    lv_obj_t *row = createRow(tile);
    createRowLabel(row, "Uptime");
    sSysUptimeLabel = lv_label_create(row);
    lv_label_set_text(sSysUptimeLabel, "--");
    setDashboardLabelFont(sSysUptimeLabel, &lv_font_montserrat_12);
    setDashboardLabelColor(sSysUptimeLabel, UI_COLOR_TEXT_SECONDARY);
  }

  refreshSettingsModuleTile();
}
