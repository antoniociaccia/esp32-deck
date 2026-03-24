#ifndef CONFIG_DEBUG_H
#define CONFIG_DEBUG_H

#include <Arduino.h>

static constexpr uint8_t DEBUG_PROFILE_PRODUCTION = 0;
static constexpr uint8_t DEBUG_PROFILE_DEVELOPMENT = 1;

static constexpr uint8_t DEBUG_LEVEL_ERROR = 0;
static constexpr uint8_t DEBUG_LEVEL_INFO = 1;
static constexpr uint8_t DEBUG_LEVEL_VERBOSE = 2;

static constexpr uint8_t NETWORK_TEST_MODE_DISABLED = 0;
static constexpr uint8_t NETWORK_TEST_MODE_OFFLINE = 1;
static constexpr uint8_t NETWORK_TEST_MODE_CONFIG_MISSING = 2;
static constexpr uint8_t NETWORK_TEST_MODE_TRANSPORT_ERROR = 3;
static constexpr uint8_t NETWORK_TEST_MODE_HTTP_ERROR = 4;
static constexpr uint8_t NETWORK_TEST_MODE_INVALID_PAYLOAD = 5;
static constexpr uint8_t NETWORK_TEST_MODE_SUCCESS_MOCK = 6;

static constexpr uint8_t DEBUG_PROFILE = DEBUG_PROFILE_PRODUCTION;

static constexpr bool DEBUG_SERIAL_ENABLED = DEBUG_PROFILE != DEBUG_PROFILE_PRODUCTION;
static constexpr uint8_t DEBUG_SERIAL_LEVEL =
  DEBUG_PROFILE == DEBUG_PROFILE_DEVELOPMENT ? DEBUG_LEVEL_INFO : DEBUG_LEVEL_ERROR;

static constexpr bool DEBUG_LOG_BOOT = DEBUG_PROFILE == DEBUG_PROFILE_DEVELOPMENT;
static constexpr bool DEBUG_LOG_NETWORK = DEBUG_PROFILE == DEBUG_PROFILE_DEVELOPMENT;
static constexpr bool DEBUG_LOG_POWER = false;
static constexpr bool DEBUG_LOG_LVGL = false;
static constexpr bool DEBUG_LOG_SAFE = DEBUG_PROFILE == DEBUG_PROFILE_DEVELOPMENT;

static constexpr uint8_t DEBUG_WEATHER_TEST_MODE = NETWORK_TEST_MODE_DISABLED;
static constexpr uint8_t DEBUG_NEWS_TEST_MODE = NETWORK_TEST_MODE_DISABLED;
static constexpr int DEBUG_WEATHER_TEST_HTTP_CODE = 503;
static constexpr int DEBUG_NEWS_TEST_HTTP_CODE = 503;

#define DEBUG_SHOULD_LOG(level, enabled) (DEBUG_SERIAL_ENABLED && (enabled) && ((level) <= DEBUG_SERIAL_LEVEL))

#define DEBUG_PRINT(level, enabled, message) \
  do { \
    if (DEBUG_SHOULD_LOG((level), (enabled))) { \
      Serial.println((message)); \
    } \
  } while (0)

#define DEBUG_PRINTF(level, enabled, fmt, ...) \
  do { \
    if (DEBUG_SHOULD_LOG((level), (enabled))) { \
      Serial.printf((fmt), ##__VA_ARGS__); \
    } \
  } while (0)

#define DEBUG_BOOT_PRINT(message) DEBUG_PRINT(DEBUG_LEVEL_INFO, DEBUG_LOG_BOOT, (message))
#define DEBUG_BOOT_PRINTF(fmt, ...) DEBUG_PRINTF(DEBUG_LEVEL_INFO, DEBUG_LOG_BOOT, (fmt), ##__VA_ARGS__)

#define DEBUG_NETWORK_PRINT(message) DEBUG_PRINT(DEBUG_LEVEL_INFO, DEBUG_LOG_NETWORK, (message))
#define DEBUG_NETWORK_PRINTF(fmt, ...) DEBUG_PRINTF(DEBUG_LEVEL_INFO, DEBUG_LOG_NETWORK, (fmt), ##__VA_ARGS__)

#define DEBUG_POWER_PRINT(message) DEBUG_PRINT(DEBUG_LEVEL_INFO, DEBUG_LOG_POWER, (message))
#define DEBUG_POWER_PRINTF(fmt, ...) DEBUG_PRINTF(DEBUG_LEVEL_INFO, DEBUG_LOG_POWER, (fmt), ##__VA_ARGS__)

#define DEBUG_SAFE_PRINT(message) DEBUG_PRINT(DEBUG_LEVEL_INFO, DEBUG_LOG_SAFE, (message))
#define DEBUG_SAFE_PRINTF(fmt, ...) DEBUG_PRINTF(DEBUG_LEVEL_INFO, DEBUG_LOG_SAFE, (fmt), ##__VA_ARGS__)

#define DEBUG_LVGL_PRINTF(fmt, ...) DEBUG_PRINTF(DEBUG_LEVEL_VERBOSE, DEBUG_LOG_LVGL, (fmt), ##__VA_ARGS__)

#endif
