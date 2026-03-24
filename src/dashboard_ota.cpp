#include "dashboard_ota.h"

static int clampSearchEnd(const String &payload, int searchEnd) {
  if (searchEnd < 0 || searchEnd > payload.length()) {
    return payload.length();
  }
  return searchEnd;
}

static int findKeyIndex(const String &payload, const char *key, int searchStart = 0, int searchEnd = -1) {
  int boundedEnd = clampSearchEnd(payload, searchEnd);
  int keyIndex = payload.indexOf(key, searchStart);
  if (keyIndex < 0 || keyIndex >= boundedEnd) {
    return -1;
  }
  return keyIndex;
}

static int findColonIndex(const String &payload, int keyIndex, int searchEnd = -1) {
  int boundedEnd = clampSearchEnd(payload, searchEnd);
  int colonIndex = payload.indexOf(':', keyIndex);
  if (colonIndex < 0 || colonIndex >= boundedEnd) {
    return -1;
  }
  return colonIndex;
}

static int skipWhitespace(const String &payload, int start, int searchEnd = -1) {
  int boundedEnd = clampSearchEnd(payload, searchEnd);
  while (start < boundedEnd) {
    char c = payload[start];
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
      break;
    }
    start++;
  }
  return start;
}

static bool findStringEnd(const String &payload, int startQuote, int &endQuote, int searchEnd = -1) {
  int boundedEnd = clampSearchEnd(payload, searchEnd);
  bool escaped = false;
  for (int i = startQuote + 1; i < boundedEnd; ++i) {
    char c = payload[i];
    if (escaped) {
      escaped = false;
      continue;
    }
    if (c == '\\') {
      escaped = true;
      continue;
    }
    if (c == '\"') {
      endQuote = i;
      return true;
    }
  }
  return false;
}

static String decodeManifestString(const String &value) {
  String decoded;
  decoded.reserve(value.length());

  for (int i = 0; i < value.length(); ++i) {
    char c = value[i];
    if (c == '\\' && i + 1 < value.length()) {
      char next = value[++i];
      switch (next) {
        case '\"': decoded += '\"'; break;
        case '\\': decoded += '\\'; break;
        case '/': decoded += '/'; break;
        case 'b': decoded += ' '; break;
        case 'f': decoded += ' '; break;
        case 'n': decoded += ' '; break;
        case 'r': decoded += ' '; break;
        case 't': decoded += ' '; break;
        default: decoded += next; break;
      }
    } else {
      decoded += c;
    }
  }

  decoded.trim();
  return decoded;
}

static bool extractStringField(const String &payload, const char *key, char *out, size_t outSize) {
  int keyIndex = findKeyIndex(payload, key);
  if (keyIndex < 0) {
    return false;
  }

  int colonIndex = findColonIndex(payload, keyIndex);
  if (colonIndex < 0) {
    return false;
  }

  int startQuote = payload.indexOf('\"', colonIndex + 1);
  if (startQuote < 0) {
    return false;
  }

  int endQuote = -1;
  if (!findStringEnd(payload, startQuote, endQuote) || endQuote <= startQuote) {
    return false;
  }

  String decoded = decodeManifestString(payload.substring(startQuote + 1, endQuote));
  if (decoded.length() == 0) {
    return false;
  }

  strlcpy(out, decoded.c_str(), outSize);
  return true;
}

static bool extractUIntField(const String &payload, const char *key, uint32_t &valueOut) {
  int keyIndex = findKeyIndex(payload, key);
  if (keyIndex < 0) {
    return false;
  }

  int colonIndex = findColonIndex(payload, keyIndex);
  if (colonIndex < 0) {
    return false;
  }

  int start = skipWhitespace(payload, colonIndex + 1);
  int end = start;
  while (end < payload.length() && isDigit(payload[end])) {
    end++;
  }

  if (end <= start) {
    return false;
  }

  valueOut = static_cast<uint32_t>(payload.substring(start, end).toInt());
  return true;
}

static bool extractIntField(const String &payload, const char *key, int &valueOut) {
  int keyIndex = findKeyIndex(payload, key);
  if (keyIndex < 0) {
    return false;
  }

  int colonIndex = findColonIndex(payload, keyIndex);
  if (colonIndex < 0) {
    return false;
  }

  int start = skipWhitespace(payload, colonIndex + 1);
  int end = start;
  if (payload[end] == '-') {
    end++;
  }
  while (end < payload.length() && isDigit(payload[end])) {
    end++;
  }

  if (end <= start) {
    return false;
  }

  valueOut = payload.substring(start, end).toInt();
  return true;
}

static const char *skipVersionPrefix(const char *version) {
  if (version == nullptr) {
    return "";
  }
  return (version[0] == 'v' || version[0] == 'V') ? version + 1 : version;
}

static int readVersionPart(const char *&cursor) {
  int value = 0;
  while (*cursor != '\0' && isDigit(*cursor)) {
    value = (value * 10) + (*cursor - '0');
    cursor++;
  }
  return value;
}

int compareVersionStrings(const char *lhs, const char *rhs) {
  const char *left = skipVersionPrefix(lhs);
  const char *right = skipVersionPrefix(rhs);

  while (*left != '\0' || *right != '\0') {
    int leftPart = readVersionPart(left);
    int rightPart = readVersionPart(right);

    if (leftPart < rightPart) {
      return -1;
    }
    if (leftPart > rightPart) {
      return 1;
    }

    if (*left == '.') {
      left++;
    }
    if (*right == '.') {
      right++;
    }

    while (*left != '\0' && !isDigit(*left) && *left != '.') {
      left++;
    }
    while (*right != '\0' && !isDigit(*right) && *right != '.') {
      right++;
    }
  }

  return 0;
}

bool isOtaManifestValid(const OtaManifest &manifest) {
  return manifest.channel[0] != '\0'
    && manifest.board[0] != '\0'
    && manifest.version[0] != '\0'
    && manifest.build[0] != '\0'
    && manifest.binUrl[0] != '\0'
    && manifest.sha256[0] != '\0'
    && manifest.sizeBytes > 0
    && manifest.minBatteryPercent >= 0
    && manifest.minBatteryPercent <= 100;
}

bool parseOtaManifest(const String &payload, OtaManifest &manifest) {
  OtaManifest parsed;
  if (!extractStringField(payload, "\"channel\"", parsed.channel, sizeof(parsed.channel))) {
    return false;
  }
  if (!extractStringField(payload, "\"board\"", parsed.board, sizeof(parsed.board))) {
    return false;
  }
  if (!extractStringField(payload, "\"version\"", parsed.version, sizeof(parsed.version))) {
    return false;
  }
  if (!extractStringField(payload, "\"build\"", parsed.build, sizeof(parsed.build))) {
    return false;
  }
  if (!extractStringField(payload, "\"bin_url\"", parsed.binUrl, sizeof(parsed.binUrl))) {
    return false;
  }
  if (!extractStringField(payload, "\"sha256\"", parsed.sha256, sizeof(parsed.sha256))) {
    return false;
  }
  if (!extractUIntField(payload, "\"size\"", parsed.sizeBytes) || parsed.sizeBytes == 0) {
    return false;
  }
  if (!extractIntField(payload, "\"min_battery_percent\"", parsed.minBatteryPercent)) {
    return false;
  }

  int notesKey = findKeyIndex(payload, "\"notes\"");
  if (notesKey >= 0) {
    extractStringField(payload, "\"notes\"", parsed.notes, sizeof(parsed.notes));
  }

  manifest = parsed;
  return true;
}

bool isOtaManifestCompatible(const OtaManifest &manifest, const char *expectedBoard, const char *expectedChannel) {
  return isOtaManifestValid(manifest)
    && expectedBoard != nullptr
    && expectedChannel != nullptr
    && strcmp(manifest.board, expectedBoard) == 0
    && strcmp(manifest.channel, expectedChannel) == 0;
}

bool isOtaUpdateAvailable(const OtaManifest &manifest, const char *currentVersion, const char *expectedBoard, const char *expectedChannel) {
  if (!isOtaManifestCompatible(manifest, expectedBoard, expectedChannel) || currentVersion == nullptr) {
    return false;
  }
  return compareVersionStrings(manifest.version, currentVersion) > 0;
}

OtaEligibility evaluateOtaEligibility(
  const OtaManifest &manifest,
  const char *currentVersion,
  const char *expectedBoard,
  const char *expectedChannel,
  uint32_t maxFirmwareBytes,
  int batteryPercent) {
  if (!isOtaManifestValid(manifest) || currentVersion == nullptr || expectedBoard == nullptr || expectedChannel == nullptr) {
    return OTA_ELIGIBILITY_INVALID;
  }

  if (strcmp(manifest.board, expectedBoard) != 0) {
    return OTA_ELIGIBILITY_INCOMPATIBLE_BOARD;
  }

  if (strcmp(manifest.channel, expectedChannel) != 0) {
    return OTA_ELIGIBILITY_INCOMPATIBLE_CHANNEL;
  }

  if (compareVersionStrings(manifest.version, currentVersion) <= 0) {
    return OTA_ELIGIBILITY_UP_TO_DATE;
  }

  if (maxFirmwareBytes > 0 && manifest.sizeBytes > maxFirmwareBytes) {
    return OTA_ELIGIBILITY_SLOT_TOO_SMALL;
  }

  if (manifest.minBatteryPercent > 0 && batteryPercent < 0) {
    return OTA_ELIGIBILITY_BATTERY_UNKNOWN;
  }

  if (manifest.minBatteryPercent > 0 && batteryPercent < manifest.minBatteryPercent) {
    return OTA_ELIGIBILITY_BATTERY_TOO_LOW;
  }

  return OTA_ELIGIBILITY_UPDATE_AVAILABLE;
}

const char *otaEligibilityLabel(OtaEligibility eligibility) {
  switch (eligibility) {
    case OTA_ELIGIBILITY_INVALID:
      return "invalid";
    case OTA_ELIGIBILITY_INCOMPATIBLE_BOARD:
      return "board";
    case OTA_ELIGIBILITY_INCOMPATIBLE_CHANNEL:
      return "channel";
    case OTA_ELIGIBILITY_UP_TO_DATE:
      return "up-to-date";
    case OTA_ELIGIBILITY_SLOT_TOO_SMALL:
      return "slot";
    case OTA_ELIGIBILITY_BATTERY_UNKNOWN:
      return "battery-unknown";
    case OTA_ELIGIBILITY_BATTERY_TOO_LOW:
      return "battery-low";
    case OTA_ELIGIBILITY_UPDATE_AVAILABLE:
      return "available";
    default:
      return "unknown";
  }
}
