#include "dashboard_energy.h"
#include "dashboard_app.h"
#include "dashboard_support.h"
#include "config_timing.h"
#include <time.h>
#include <WiFi.h>
#include "secrets.h"
#include "esp_sleep.h"
#include "esp_bt.h"

static unsigned long sLastScheduleCheckMs = 0;

static bool isWithinActiveWindow(int hour, int minute) {
  const int current = hour * 60 + minute;
  const int start   = app.settings.activeStartHour * 60 + app.settings.activeStartMinute;
  const int end     = app.settings.activeEndHour   * 60 + app.settings.activeEndMinute;

  if (start == end) return true;
  if (start < end)  return current >= start && current < end;
  return current >= start || current < end;
}

static void applyDisplayOn(Display &screen) {
  if (!app.energy.displayOn) {
    app.energy.displayOn = true;
    screen.setBacklight(true);
  }
}

static void applyDisplayOff(Display &screen) {
  if (app.energy.displayOn) {
    app.energy.displayOn = false;
    screen.setBacklight(false);
  }
}

static void applyWifiOn() {
  if (!app.energy.wifiDisabledByPolicy) return;
  app.energy.wifiDisabledByPolicy = false;
  if (app.settings.wifiEnabled && WiFi.status() != WL_CONNECTED && strlen(WIFI_SSID) > 0) {
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  }
}

static void applyWifiOff() {
  if (app.energy.wifiDisabledByPolicy) return;
  app.energy.wifiDisabledByPolicy = true;
  WiFi.disconnect(true);
  WiFi.mode(WIFI_OFF);
}

static void evaluateSchedulePolicy(Display &screen) {
  if (!app.settings.energyScheduleEnabled) {
    // Schedule off: restore WiFi if policy had it off, leave display as-is
    applyWifiOn();
    return;
  }

  struct tm timeinfo;
  if (!getLocalTime(&timeinfo, 10)) return;

  bool inWindow = isWithinActiveWindow(timeinfo.tm_hour, timeinfo.tm_min);

  // WiFi policy
  if (!inWindow && app.settings.powerMode >= 1) {
    applyWifiOff();
  } else {
    applyWifiOn();
  }

  // Display policy: only act if not already managed by inactivity
  if (!app.energy.displayOffByInactivity) {
    if (!inWindow && !app.energy.manualWakeActive) {
      applyDisplayOff(screen);
    } else if (inWindow) {
      applyDisplayOn(screen);
      app.energy.manualWakeActive = false;
    }
  }

  // Deep sleep: only when powerMode==2, outside window, time synced, no manual wake
  if (!inWindow && app.settings.powerMode == 2
      && app.clock.synced && !app.energy.manualWakeActive) {
    int nowMinutes  = timeinfo.tm_hour * 60 + timeinfo.tm_min;
    int wakeMinutes = app.settings.activeStartHour * 60 + app.settings.activeStartMinute;
    int diffMinutes = (wakeMinutes > nowMinutes)
      ? (wakeMinutes - nowMinutes)
      : (1440 - nowMinutes + wakeMinutes);
    if (diffMinutes >= 5) {
      // Wake up 1 minute early to allow Wi-Fi + NTP reconnect
      uint64_t sleepUs = (uint64_t)(diffMinutes - 1) * 60ULL * 1000000ULL;
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
      btStop();
      esp_sleep_enable_timer_wakeup(sleepUs);
      esp_deep_sleep_start();
    }
  }
}

// ---- Public API ----

void applyWifiUserEnabled(bool enabled) {
  if (enabled) {
    // Re-enable: reconnect if policy allows it
    if (!app.energy.wifiDisabledByPolicy && strlen(WIFI_SSID) > 0) {
      WiFi.mode(WIFI_STA);
      WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    }
  } else {
    // Disable: disconnect only if policy hasn't already done so
    if (!app.energy.wifiDisabledByPolicy) {
      WiFi.disconnect(true);
      WiFi.mode(WIFI_OFF);
    }
  }
}

void notifyUserActivity() {
  app.energy.lastActivityMs = millis();

  if (app.settings.tapWakeEnabled && !app.energy.displayOn) {
    app.energy.manualWakeActive = true;
    app.energy.displayOffByInactivity = false;
  }
}

void applyEnergyPolicy(Display &screen) {
  // --- Fast path: manual wake → turn display on immediately ---
  if (app.energy.manualWakeActive && !app.energy.displayOn) {
    applyDisplayOn(screen);
    app.energy.displayOffByInactivity = false;
  }

  // --- Fast path: standalone inactivity timeout ---
  if (app.energy.displayOn && app.energy.lastActivityMs > 0) {
    unsigned long timeoutMs = (unsigned long)app.settings.inactivityTimeoutMinutes * 60000UL;
    if (millis() - app.energy.lastActivityMs > timeoutMs) {
      applyDisplayOff(screen);
      app.energy.displayOffByInactivity = true;
      app.energy.manualWakeActive = false;
      return;
    }
  }

  // --- Slow path: energy schedule ---
  if (!intervalElapsed(sLastScheduleCheckMs, TIMING_ENERGY_SCHEDULE_CHECK_MS)) {
    return;
  }
  evaluateSchedulePolicy(screen);
}
