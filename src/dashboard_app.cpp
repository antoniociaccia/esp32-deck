#include "dashboard_app.h"

UiRefs ui;
AppState app;

const char *const DEFAULT_NEWS_ITEMS[DEFAULT_NEWS_ITEM_COUNT] = {
  "IT | Dashboard pronta per dati reali",
  "TECH | Swipe orizzontale tra i widget",
  "WORLD | Prossimo step: news, meteo e batteria"
};

const ModuleContent MODULES[MODULE_COUNT] = {
  {"Pomodoro", "24:52", "focus session in corso"},
  {"Email", "3 nuove", "2 urgenti e 1 follow-up"},
  {"Meteo", "18C", "Roma, cielo sereno"},
  {"Touch", "OK", "swipe destra o sinistra"}
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

int extractJsonIntAfterKey(const String &payload, const char *key, int fallbackValue) {
  int keyIndex = payload.indexOf(key);
  if (keyIndex < 0) {
    return fallbackValue;
  }

  int colonIndex = payload.indexOf(':', keyIndex);
  if (colonIndex < 0) {
    return fallbackValue;
  }

  int start = colonIndex + 1;
  while (start < payload.length() && (payload[start] == ' ' || payload[start] == '\"')) {
    start++;
  }

  int end = start;
  while (end < payload.length() && (isDigit(payload[end]) || payload[end] == '-')) {
    end++;
  }

  if (end <= start) {
    return fallbackValue;
  }

  return payload.substring(start, end).toInt();
}

String extractJsonStringAfterKey(const String &payload, const char *key, const char *fallbackValue) {
  int keyIndex = payload.indexOf(key);
  if (keyIndex < 0) {
    return String(fallbackValue);
  }

  int colonIndex = payload.indexOf(':', keyIndex);
  if (colonIndex < 0) {
    return String(fallbackValue);
  }

  int startQuote = payload.indexOf('\"', colonIndex + 1);
  if (startQuote < 0) {
    return String(fallbackValue);
  }

  int endQuote = payload.indexOf('\"', startQuote + 1);
  if (endQuote < 0 || endQuote <= startQuote) {
    return String(fallbackValue);
  }

  return payload.substring(startQuote + 1, endQuote);
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
      strlcat(app.newsTicker, "     •     ", MAX_TICKER_LEN);
    }
    strlcat(app.newsTicker, app.newsItems[i], MAX_TICKER_LEN);
  }

  if (ui.newsLabel != nullptr) {
    lv_label_set_text(ui.newsLabel, app.newsTicker);
  }
}

void setDefaultNewsItems() {
  app.newsItemCount = DEFAULT_NEWS_ITEM_COUNT;
  for (int i = 0; i < DEFAULT_NEWS_ITEM_COUNT; ++i) {
    strlcpy(app.newsItems[i], DEFAULT_NEWS_ITEMS[i], MAX_NEWS_TEXT_LEN);
  }
  rebuildNewsTicker();
}

bool parseNewsItems(const String &payload) {
  int parsedCount = 0;
  int searchFrom = 0;

  while (parsedCount < MAX_NEWS_ITEMS) {
    int textKey = payload.indexOf("\"text\"", searchFrom);
    if (textKey < 0) {
      break;
    }

    int colonIndex = payload.indexOf(':', textKey);
    int startQuote = payload.indexOf('\"', colonIndex + 1);
    if (colonIndex < 0 || startQuote < 0) {
      break;
    }

    bool escaped = false;
    int endQuote = -1;
    for (int i = startQuote + 1; i < payload.length(); ++i) {
      char c = payload[i];
      if (c == '\\' && !escaped) {
        escaped = true;
        continue;
      }
      if (c == '\"' && !escaped) {
        endQuote = i;
        break;
      }
      escaped = false;
    }

    if (endQuote < 0) {
      break;
    }

    String decoded = decodeJsonString(payload.substring(startQuote + 1, endQuote));
    normalizeNewsText(decoded);
    if (decoded.length() > 0) {
      strlcpy(app.newsItems[parsedCount], decoded.c_str(), MAX_NEWS_TEXT_LEN);
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
