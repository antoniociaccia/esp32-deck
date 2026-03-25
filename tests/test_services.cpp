#include "doctest.h"
#include "Arduino.h"
#include "WiFi.h"
#include "dashboard_app.h"
#include "dashboard_services.h"
#include "config_timing.h"

// ─── updateClockUi ────────────────────────────────────────────────────────────

TEST_CASE("updateClockUi — interval guard skips update") {
  app.clock = ClockState{};
  app.uiDirtyMask = UI_DIRTY_NONE;
  mockGetLocalTimeResult = true;
  app.clock.lastUpdateMs = mockMillis;

  updateClockUi();

  CHECK(app.clock.labelText[0] == '\0');
  CHECK(app.uiDirtyMask.load() == UI_DIRTY_NONE);
}

TEST_CASE("updateClockUi — updates label and dirty mask when interval elapsed") {
  app.clock = ClockState{};
  app.uiDirtyMask = UI_DIRTY_NONE;
  mockGetLocalTimeResult = true;
  mockTimeinfoValue = {};
  mockTimeinfoValue.tm_mday = 5;
  mockTimeinfoValue.tm_mon  = 2;   // marzo (0-based)
  mockTimeinfoValue.tm_hour = 14;
  mockTimeinfoValue.tm_min  = 30;

  app.clock.lastUpdateMs = mockMillis;
  mockMillis += TIMING_CLOCK_REFRESH_MS + 1;
  updateClockUi();

  CHECK(strcmp(app.clock.labelText, "05/03 14:30") == 0);
  CHECK(app.clock.synced == true);
  CHECK(app.clock.lastUpdateMs == mockMillis);
  CHECK((app.uiDirtyMask.load() & (UI_DIRTY_HEADER | UI_DIRTY_MAIN_CLOCK)) ==
        (UI_DIRTY_HEADER | UI_DIRTY_MAIN_CLOCK));
}

TEST_CASE("updateClockUi — fallback label when getLocalTime fails") {
  app.clock = ClockState{};
  app.uiDirtyMask = UI_DIRTY_NONE;
  mockGetLocalTimeResult = false;

  app.clock.lastUpdateMs = mockMillis;
  mockMillis += TIMING_CLOCK_REFRESH_MS + 1;
  updateClockUi();

  CHECK(strcmp(app.clock.labelText, "sync orario...") == 0);
  CHECK(app.clock.synced == false);
  CHECK((app.uiDirtyMask.load() & (UI_DIRTY_HEADER | UI_DIRTY_MAIN_CLOCK)) ==
        (UI_DIRTY_HEADER | UI_DIRTY_MAIN_CLOCK));

  mockGetLocalTimeResult = true;
}

TEST_CASE("updateClockUi — no dirty mark when label and synced state unchanged") {
  app.clock = ClockState{};
  app.uiDirtyMask = UI_DIRTY_NONE;
  mockGetLocalTimeResult = true;
  mockTimeinfoValue = {};
  mockTimeinfoValue.tm_mday = 1;
  mockTimeinfoValue.tm_mon  = 0;
  mockTimeinfoValue.tm_hour = 10;
  mockTimeinfoValue.tm_min  = 0;

  // Prima chiamata: imposta label e segna dirty
  app.clock.lastUpdateMs = mockMillis;
  mockMillis += TIMING_CLOCK_REFRESH_MS + 1;
  updateClockUi();
  app.uiDirtyMask = UI_DIRTY_NONE;

  // Seconda chiamata: stesso orario, già synced → nessun dirty
  mockMillis += TIMING_CLOCK_REFRESH_MS + 1;
  updateClockUi();

  CHECK(app.uiDirtyMask.load() == UI_DIRTY_NONE);
}

// ─── maintainTimeSync ─────────────────────────────────────────────────────────

TEST_CASE("maintainTimeSync — skips all work when already synced") {
  app.clock = ClockState{};
  app.clock.synced = true;
  app.clock.lastSyncAttemptMs = 0;
  app.uiDirtyMask = UI_DIRTY_NONE;

  maintainTimeSync();

  CHECK(app.clock.synced == true);
  CHECK(app.clock.lastSyncAttemptMs == 0);
  CHECK(app.uiDirtyMask.load() == UI_DIRTY_NONE);
}

TEST_CASE("maintainTimeSync — WiFi disconnected, timer not elapsed, no reconnect") {
  app.clock = ClockState{};
  app.clock.synced = false;
  app.clock.lastSyncAttemptMs = mockMillis;
  WiFi.mockStatus = WL_DISCONNECTED;

  maintainTimeSync();

  CHECK(app.clock.synced == false);
  CHECK(app.clock.lastSyncAttemptMs == mockMillis);

  WiFi.mockStatus = WL_CONNECTED;
}

TEST_CASE("maintainTimeSync — WiFi disconnected, timer elapsed, triggers beginTimeSync") {
  app.clock = ClockState{};
  app.clock.synced = false;
  app.clock.lastSyncAttemptMs = mockMillis - TIMING_WIFI_RECONNECT_MS - 1;
  WiFi.mockStatus = WL_DISCONNECTED;

  maintainTimeSync();

  CHECK(app.clock.lastSyncAttemptMs == mockMillis);

  WiFi.mockStatus = WL_CONNECTED;
}

TEST_CASE("maintainTimeSync — WiFi connected, time available, marks synced") {
  app.clock = ClockState{};
  app.clock.synced = false;
  app.clock.lastUpdateMs = mockMillis;  // evita che updateClockUi riscriva il label
  WiFi.mockStatus = WL_CONNECTED;
  mockGetLocalTimeResult = true;

  maintainTimeSync();

  CHECK(app.clock.synced == true);
}

TEST_CASE("maintainTimeSync — WiFi connected, time unavailable, retry timer not elapsed") {
  app.clock = ClockState{};
  app.clock.synced = false;
  app.clock.lastSyncAttemptMs = mockMillis;
  WiFi.mockStatus = WL_CONNECTED;
  mockGetLocalTimeResult = false;

  maintainTimeSync();

  CHECK(app.clock.synced == false);
  CHECK(app.clock.lastSyncAttemptMs == mockMillis);

  mockGetLocalTimeResult = true;
}

TEST_CASE("maintainTimeSync — WiFi connected, time unavailable, retry timer elapsed") {
  app.clock = ClockState{};
  app.clock.synced = false;
  app.clock.lastSyncAttemptMs = mockMillis - TIMING_NTP_RETRY_MS - 1;
  WiFi.mockStatus = WL_CONNECTED;
  mockGetLocalTimeResult = false;

  maintainTimeSync();

  CHECK(app.clock.synced == false);
  CHECK(app.clock.lastSyncAttemptMs == mockMillis);

  mockGetLocalTimeResult = true;
}
