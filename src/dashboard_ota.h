#ifndef DASHBOARD_OTA_H
#define DASHBOARD_OTA_H

#include <Arduino.h>

static constexpr size_t OTA_CHANNEL_MAX_LEN = 16;
static constexpr size_t OTA_BOARD_MAX_LEN = 16;
static constexpr size_t OTA_VERSION_MAX_LEN = 24;
static constexpr size_t OTA_BUILD_MAX_LEN = 32;
static constexpr size_t OTA_BIN_URL_MAX_LEN = 384;
static constexpr size_t OTA_SHA256_MAX_LEN = 65;
static constexpr size_t OTA_NOTES_MAX_LEN = 128;

struct OtaManifest {
  char channel[OTA_CHANNEL_MAX_LEN] = {};
  char board[OTA_BOARD_MAX_LEN] = {};
  char version[OTA_VERSION_MAX_LEN] = {};
  char build[OTA_BUILD_MAX_LEN] = {};
  char binUrl[OTA_BIN_URL_MAX_LEN] = {};
  char sha256[OTA_SHA256_MAX_LEN] = {};
  char notes[OTA_NOTES_MAX_LEN] = {};
  uint32_t sizeBytes = 0;
  int minBatteryPercent = 0;
};

enum OtaEligibility : uint8_t {
  OTA_ELIGIBILITY_INVALID = 0,
  OTA_ELIGIBILITY_INCOMPATIBLE_BOARD,
  OTA_ELIGIBILITY_INCOMPATIBLE_CHANNEL,
  OTA_ELIGIBILITY_UP_TO_DATE,
  OTA_ELIGIBILITY_SLOT_TOO_SMALL,
  OTA_ELIGIBILITY_BATTERY_UNKNOWN,
  OTA_ELIGIBILITY_BATTERY_TOO_LOW,
  OTA_ELIGIBILITY_UPDATE_AVAILABLE
};

bool parseOtaManifest(const String &payload, OtaManifest &manifest);
int compareVersionStrings(const char *lhs, const char *rhs);
bool isOtaManifestValid(const OtaManifest &manifest);
bool isOtaManifestCompatible(const OtaManifest &manifest, const char *expectedBoard, const char *expectedChannel);
bool isOtaUpdateAvailable(const OtaManifest &manifest, const char *currentVersion, const char *expectedBoard, const char *expectedChannel);
OtaEligibility evaluateOtaEligibility(
  const OtaManifest &manifest,
  const char *currentVersion,
  const char *expectedBoard,
  const char *expectedChannel,
  uint32_t maxFirmwareBytes,
  int batteryPercent);
const char *otaEligibilityLabel(OtaEligibility eligibility);

#endif
