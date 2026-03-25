#include "dashboard_app.h"
#include "dashboard_support.h"

UiRefs ui;
AppState app;

const char *const DEFAULT_NEWS_ITEMS[NEWS_DEFAULT_ITEM_COUNT] = {
  "IT | Dashboard pronta per dati reali",
  "TECH | Swipe orizzontale tra i widget",
  "WORLD | Prossimo step: news, meteo e batteria"
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

static bool extractJsonFloatAfterKey(const String &payload, const char *key, float &valueOut, int searchStart = 0, int searchEnd = -1) {
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

  if (end < boundedEnd && payload[end] == '-') {
    end++;
  }

  bool foundDigit = false;
  bool foundDot = false;
  while (end < boundedEnd) {
    char c = payload[end];
    if (isDigit(c)) {
      foundDigit = true;
      end++;
      continue;
    }
    if (c == '.' && !foundDot) {
      foundDot = true;
      end++;
      continue;
    }
    break;
  }

  if (!foundDigit || end <= start) {
    return false;
  }

  valueOut = payload.substring(start, end).toFloat();
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
  String normalized;
  normalized.reserve(text.length());
  bool lastWasSpace = false;
  for (int i = 0; i < text.length(); ++i) {
    char c = text[i];
    if (c == '\n' || c == '\r' || c == ' ') {
      if (!lastWasSpace) {
        normalized += ' ';
        lastWasSpace = true;
      }
    } else {
      normalized += c;
      lastWasSpace = false;
    }
  }
  normalized.trim();
  text = normalized;
}

void rebuildNewsTicker() {
  app.news.ticker[0] = '\0';
  for (int i = 0; i < app.news.itemCount; ++i) {
    if (i > 0) {
      strlcat(app.news.ticker, "     •     ", NEWS_MAX_TICKER_LEN);
    }
    strlcat(app.news.ticker, app.news.items[i], NEWS_MAX_TICKER_LEN);
  }

}

void setDefaultNewsItems() {
  app.news.itemCount = NEWS_DEFAULT_ITEM_COUNT;
  for (int i = 0; i < NEWS_DEFAULT_ITEM_COUNT; ++i) {
    strlcpy(app.news.items[i], DEFAULT_NEWS_ITEMS[i], NEWS_MAX_TEXT_LEN);
  }
  rebuildNewsTicker();
}

const char *serviceFetchStateLabel(ServiceFetchState state) {
  switch (state) {
    case SERVICE_FETCH_READY:
      return "ready";
    case SERVICE_FETCH_FETCHING:
      return "fetching";
    case SERVICE_FETCH_OFFLINE:
      return "offline";
    case SERVICE_FETCH_CONFIG_MISSING:
      return "config";
    case SERVICE_FETCH_TRANSPORT_ERROR:
      return "transport";
    case SERVICE_FETCH_HTTP_ERROR:
      return "http";
    case SERVICE_FETCH_INVALID_PAYLOAD:
      return "invalid-payload";
    case SERVICE_FETCH_IDLE:
    default:
      return "idle";
  }
}

void buildNewsFooterText(char *buffer, size_t bufferSize) {
  if (buffer == nullptr || bufferSize == 0) {
    return;
  }

  buffer[0] = '\0';

  if (app.news.state == SERVICE_FETCH_READY) {
    if (app.news.ticker[0] != '\0') {
      strlcpy(buffer, app.news.ticker, bufferSize);
    } else {
      strlcpy(buffer, "NEWS | feed live ma vuoto", bufferSize);
    }
    return;
  }

  switch (app.news.state) {
    case SERVICE_FETCH_IDLE:
      strlcpy(buffer, "NEWS | attesa primo aggiornamento", bufferSize);
      break;
    case SERVICE_FETCH_FETCHING:
      strlcpy(buffer, "NEWS | recupero notizie...", bufferSize);
      break;
    case SERVICE_FETCH_OFFLINE:
      strlcpy(buffer, "NEWS | offline", bufferSize);
      break;
    case SERVICE_FETCH_CONFIG_MISSING:
      strlcpy(buffer, "NEWS | config mancante", bufferSize);
      break;
    case SERVICE_FETCH_TRANSPORT_ERROR:
      strlcpy(buffer, "NEWS | errore rete", bufferSize);
      break;
    case SERVICE_FETCH_HTTP_ERROR:
      snprintf(buffer, bufferSize, "NEWS | HTTP %d", app.news.lastHttpCode);
      break;
    case SERVICE_FETCH_INVALID_PAYLOAD:
      strlcpy(buffer, "NEWS | payload non valido", bufferSize);
      break;
    case SERVICE_FETCH_READY:
    default:
      strlcpy(buffer, "NEWS", bufferSize);
      break;
  }

  if (app.news.itemCount > 0 && app.news.ticker[0] != '\0') {
    strlcat(buffer, " | cache | ", bufferSize);
    strlcat(buffer, app.news.ticker, bufferSize);
  } else {
    strlcat(buffer, " | nessun feed disponibile", bufferSize);
  }
}

bool parseWeatherPayload(const String &payload, int &temperatureOut, char *iconCodeOut, size_t iconCodeOutSize) {
  int mainStart = 0;
  int mainEnd = 0;
  int weatherStart = 0;
  int weatherEnd = 0;
  float parsedTemperature = 0.0f;
  String parsedIconCode;

  if (findJsonRangeAfterKey(payload, "\"main\"", '{', '}', mainStart, mainEnd)) {
    if (!extractJsonFloatAfterKey(payload, "\"temp\"", parsedTemperature, mainStart, mainEnd)) {
      return false;
    }
  } else if (!extractJsonFloatAfterKey(payload, "\"temp\"", parsedTemperature)) {
    return false;
  }

  if (findJsonRangeAfterKey(payload, "\"weather\"", '[', ']', weatherStart, weatherEnd)) {
    if (!extractJsonStringAfterKey(payload, "\"icon\"", parsedIconCode, weatherStart, weatherEnd)) {
      return false;
    }
  } else if (!extractJsonStringAfterKey(payload, "\"icon\"", parsedIconCode)) {
    return false;
  }

  parsedIconCode.trim();
  if (parsedIconCode.length() == 0) {
    return false;
  }

  temperatureOut = static_cast<int>(parsedTemperature >= 0.0f ? parsedTemperature + 0.5f : parsedTemperature - 0.5f);
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
      strlcpy(app.news.items[parsedCount], decoded.c_str(), NEWS_MAX_TEXT_LEN);
      parsedCount++;
    }

    searchFrom = endQuote + 1;
  }

  if (parsedCount <= 0) {
    return false;
  }

  app.news.itemCount = parsedCount;
  rebuildNewsTicker();
  return true;
}
