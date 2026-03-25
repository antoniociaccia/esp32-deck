#include "dashboard_settings.h"
#include "dashboard_app.h"
#include <Preferences.h>

static constexpr const char *NVS_NS = "dashboard";

void loadSettings() {
  Preferences prefs;
  prefs.begin(NVS_NS, true);

  app.settings.energyScheduleEnabled    = prefs.getBool("ene_sched",   false);
  app.settings.activeStartHour          = prefs.getUChar("act_start_h", 7);
  app.settings.activeStartMinute        = prefs.getUChar("act_start_m", 0);
  app.settings.activeEndHour            = prefs.getUChar("act_end_h",   23);
  app.settings.activeEndMinute          = prefs.getUChar("act_end_m",   0);
  app.settings.tapWakeEnabled           = prefs.getBool("tap_wake",    true);
  app.settings.inactivityTimeoutMinutes = prefs.getUChar("inact_to",   5);
  app.settings.wifiEnabled              = prefs.getBool("wifi_en",     true);
  app.settings.bluetoothEnabled         = prefs.getBool("bt_en",       false);
  app.settings.weatherEnabled           = prefs.getBool("weather_en",  true);
  app.settings.newsEnabled              = prefs.getBool("news_en",     true);
  app.settings.brightnessLevel          = prefs.getUChar("brightness", 100);
  app.settings.powerMode                = prefs.getUChar("power_mode", 1);

  prefs.end();
}

void saveSettings() {
  Preferences prefs;
  prefs.begin(NVS_NS, false);

  prefs.putBool("ene_sched",   app.settings.energyScheduleEnabled);
  prefs.putUChar("act_start_h", app.settings.activeStartHour);
  prefs.putUChar("act_start_m", app.settings.activeStartMinute);
  prefs.putUChar("act_end_h",   app.settings.activeEndHour);
  prefs.putUChar("act_end_m",   app.settings.activeEndMinute);
  prefs.putBool("tap_wake",    app.settings.tapWakeEnabled);
  prefs.putUChar("inact_to",   app.settings.inactivityTimeoutMinutes);
  prefs.putBool("wifi_en",     app.settings.wifiEnabled);
  prefs.putBool("bt_en",       app.settings.bluetoothEnabled);
  prefs.putBool("weather_en",  app.settings.weatherEnabled);
  prefs.putBool("news_en",     app.settings.newsEnabled);
  prefs.putUChar("brightness", app.settings.brightnessLevel);
  prefs.putUChar("power_mode", app.settings.powerMode);

  prefs.end();
}
