#ifndef CONFIG_TIMING_H
#define CONFIG_TIMING_H

static constexpr unsigned long TIMING_CLOCK_REFRESH_MS = 1000;
static constexpr unsigned long TIMING_BATTERY_REFRESH_MS = 2000;
static constexpr unsigned long TIMING_WEATHER_REFRESH_MS = 1800000;
static constexpr unsigned long TIMING_WEATHER_RETRY_MS = 30000;
static constexpr unsigned long TIMING_NEWS_REFRESH_MS = 900000;
static constexpr unsigned long TIMING_NEWS_RETRY_MS = 60000;
static constexpr unsigned long TIMING_OTA_REFRESH_MS = 3600000;
static constexpr unsigned long TIMING_OTA_RETRY_MS = 300000;
static constexpr unsigned long TIMING_WIFI_RECONNECT_MS = 10000;
static constexpr unsigned long TIMING_NTP_RETRY_MS = 15000;
static constexpr unsigned long TIMING_SAFE_HEARTBEAT_MS = 2000;

static constexpr bool DASHBOARD_SAFE_BOOT_RECOVERY = false;

#endif
