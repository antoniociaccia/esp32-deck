#include "dashboard_app.h"
#include "dashboard_support.h"

UiRefs ui;
AppState app;

const char *const DEFAULT_NEWS_ITEMS[NEWS_DEFAULT_ITEM_COUNT] = {
  "IT | Dashboard pronta per dati reali",
  "TECH | Swipe orizzontale tra i widget",
  "WORLD | Prossimo step: news, meteo e batteria"
};

const ModuleContent MODULES[UI_MODULE_COUNT] = {
  {"Clock", "--:--", "attesa sync NTP"},
  {"Power", "--", "attesa batteria"},
  {"Weather", "--", "attesa meteo"},
  {"News", "--", "attesa feed"}
};

lv_color_t colorFromHex(uint32_t hex) {
  return lv_color_hex(hex);
}

bool intervalElapsed(unsigned long &lastRunMs, unsigned long intervalMs) {
  unsigned long now = millis();
  if (now - lastRunMs < intervalMs) {
    return false;
  }

  lastRunMs = now;
  return true;
}

static int clampJsonSearchEnd(const String &payload, int searchEnd) {
  if (searchEnd < 0 || searchEnd > payload.length()) {
    return payload.length();
  }
  return searchEnd;
}

static int findJsonKeyIndex(const String &payload, const char *key, int searchStart, int searchEnd) {
  int keyIndex = payload.indexOf(key, searchStart);
  if (keyIndex < 0 || keyIndex >= searchEnd) {
    return -1;
  }
  return keyIndex;
}

static int findJsonColonAfterKey(const String &payload, int keyIndex, int searchEnd) {
  int colonIndex = payload.indexOf(':', keyIndex);
  if (colonIndex < 0 || colonIndex >= searchEnd) {
    return -1;
  }
  return colonIndex;
}

static int skipJsonWhitespace(const String &payload, int start, int searchEnd) {
  while (start < searchEnd) {
    char c = payload[start];
    if (c != ' ' && c != '\t' && c != '\n' && c != '\r') {
      break;
    }
    start++;
  }
  return start;
}

static bool findJsonStringEnd(const String &payload, int startQuote, int searchEnd, int &endQuote) {
  bool escaped = false;
  for (int i = startQuote + 1; i < searchEnd; ++i) {
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

static bool extractJsonIntAfterKey(const String &payload, const char *key, int &valueOut, int searchStart = 0, int searchEnd = -1) {
  int boundedEnd = clampJsonSearchEnd(payload, searchEnd);
  int keyIndex = findJsonKeyIndex(payload, key, searchStart, boundedEnd);
  if (keyIndex < 0) {
    return false;
  }

  int colonIndex = findJsonColonAfterKey(payload, keyIndex, boundedEnd);
  if (colonIndex < 0) {
    return false;
  }

  int start = skipJsonWhitespace(payload, colonIndex + 1, boundedEnd);
  int end = start;
  while (end < boundedEnd && (isDigit(payload[end]) || payload[end] == '-')) {
    end++;
  }

  if (end <= start) {
    return false;
  }

  valueOut = payload.substring(start, end).toInt();
  return true;
}

static bool extractJsonStringAfterKey(const String &payload, const char *key, String &valueOut, int searchStart = 0, int searchEnd = -1) {
  int boundedEnd = clampJsonSearchEnd(payload, searchEnd);
  int keyIndex = findJsonKeyIndex(payload, key, searchStart, boundedEnd);
  if (keyIndex < 0) {
    return false;
  }

  int colonIndex = findJsonColonAfterKey(payload, keyIndex, boundedEnd);
  if (colonIndex < 0) {
    return false;
  }

  int startQuote = payload.indexOf('\"', colonIndex + 1);
  if (startQuote < 0 || startQuote >= boundedEnd) {
    return false;
  }

  int endQuote = -1;
  if (!findJsonStringEnd(payload, startQuote, boundedEnd, endQuote) || endQuote <= startQuote) {
    return false;
  }

  valueOut = payload.substring(startQuote + 1, endQuote);
  return true;
}

static bool findJsonRangeAfterKey(
  const String &payload,
  const char *key,
  char openChar,
  char closeChar,
  int &rangeStart,
  int &rangeEnd,
  int searchStart = 0,
  int searchEnd = -1) {
  int boundedEnd = clampJsonSearchEnd(payload, searchEnd);
  int keyIndex = findJsonKeyIndex(payload, key, searchStart, boundedEnd);
  if (keyIndex < 0) {
    return false;
  }

  int colonIndex = findJsonColonAfterKey(payload, keyIndex, boundedEnd);
  if (colonIndex < 0) {
    return false;
  }

  int start = skipJsonWhitespace(payload, colonIndex + 1, boundedEnd);
  if (start >= boundedEnd || payload[start] != openChar) {
    return false;
  }

  int depth = 0;
  bool inString = false;
  bool escaped = false;
  for (int i = start; i < boundedEnd; ++i) {
    char c = payload[i];
    if (inString) {
      if (escaped) {
        escaped = false;
      } else if (c == '\\') {
        escaped = true;
      } else if (c == '\"') {
        inString = false;
      }
      continue;
    }

    if (c == '\"') {
      inString = true;
      continue;
    }

    if (c == openChar) {
      depth++;
      if (depth == 1) {
        rangeStart = i + 1;
      }
      continue;
    }

    if (c == closeChar) {
      depth--;
      if (depth == 0) {
        rangeEnd = i;
        return true;
      }
    }
  }

  return false;
}

String decodeJsonString(const String &value) {
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

void normalizeNewsText(String &text) {
  text.replace("\n", " ");
  text.replace("\r", " ");
  while (text.indexOf("  ") >= 0) {
    text.replace("  ", " ");
  }
  text.trim();
}

void rebuildNewsTicker() {
  app.newsTicker[0] = '\0';
  for (int i = 0; i < app.newsItemCount; ++i) {
    if (i > 0) {
      strlcat(app.newsTicker, "     •     ", NEWS_MAX_TICKER_LEN);
    }
    strlcat(app.newsTicker, app.newsItems[i], NEWS_MAX_TICKER_LEN);
  }

}

void setDefaultNewsItems() {
  app.newsItemCount = NEWS_DEFAULT_ITEM_COUNT;
  for (int i = 0; i < NEWS_DEFAULT_ITEM_COUNT; ++i) {
    strlcpy(app.newsItems[i], DEFAULT_NEWS_ITEMS[i], NEWS_MAX_TEXT_LEN);
  }
  rebuildNewsTicker();
}

bool parseWeatherPayload(const String &payload, int &temperatureOut, char *iconCodeOut, size_t iconCodeOutSize) {
  int mainStart = 0;
  int mainEnd = 0;
  int weatherStart = 0;
  int weatherEnd = 0;
  int parsedTemperature = 0;
  String parsedIconCode;

  if (!findJsonRangeAfterKey(payload, "\"main\"", '{', '}', mainStart, mainEnd)) {
    return false;
  }
  if (!extractJsonIntAfterKey(payload, "\"temp\"", parsedTemperature, mainStart, mainEnd)) {
    return false;
  }
  if (!findJsonRangeAfterKey(payload, "\"weather\"", '[', ']', weatherStart, weatherEnd)) {
    return false;
  }
  if (!extractJsonStringAfterKey(payload, "\"icon\"", parsedIconCode, weatherStart, weatherEnd)) {
    return false;
  }

  parsedIconCode.trim();
  if (parsedIconCode.length() == 0) {
    return false;
  }

  temperatureOut = parsedTemperature;
  strlcpy(iconCodeOut, parsedIconCode.c_str(), iconCodeOutSize);
  return true;
}

bool parseNewsItems(const String &payload) {
  int parsedCount = 0;
  int itemsStart = 0;
  int itemsEnd = 0;

  if (!findJsonRangeAfterKey(payload, "\"items\"", '[', ']', itemsStart, itemsEnd)) {
    return false;
  }

  int searchFrom = itemsStart;

  while (parsedCount < NEWS_MAX_ITEMS) {
    int textKey = findJsonKeyIndex(payload, "\"text\"", searchFrom, itemsEnd);
    if (textKey < 0) {
      break;
    }

    int colonIndex = findJsonColonAfterKey(payload, textKey, itemsEnd);
    int startQuote = payload.indexOf('\"', colonIndex + 1);
    if (colonIndex < 0 || startQuote < 0 || startQuote >= itemsEnd) {
      searchFrom = textKey + 6;
      continue;
    }

    int endQuote = -1;
    if (!findJsonStringEnd(payload, startQuote, itemsEnd, endQuote)) {
      searchFrom = startQuote + 1;
      continue;
    }

    String decoded = decodeJsonString(payload.substring(startQuote + 1, endQuote));
    normalizeNewsText(decoded);
    if (decoded.length() > 0) {
      strlcpy(app.newsItems[parsedCount], decoded.c_str(), NEWS_MAX_TEXT_LEN);
      parsedCount++;
    }

    searchFrom = endQuote + 1;
  }

  if (parsedCount <= 0) {
    return false;
  }

  app.newsItemCount = parsedCount;
  rebuildNewsTicker();
  return true;
}
