#include "doctest.h"
#include "Arduino.h"
#include "dashboard_app.h"
#include "dashboard_support.h"
#include "config_network.h"
#include <climits>
#include <cstring>

// ─── intervalElapsed ─────────────────────────────────────────────────────────

TEST_CASE("intervalElapsed — interval not yet elapsed") {
  unsigned long last = mockMillis;
  CHECK(intervalElapsed(last, 1000) == false);
  CHECK(last == mockMillis);  // lastRunMs not updated
}

TEST_CASE("intervalElapsed — interval elapsed updates lastRunMs") {
  unsigned long last = mockMillis;
  mockMillis += 1001;
  CHECK(intervalElapsed(last, 1000) == true);
  CHECK(last == mockMillis);
}

TEST_CASE("intervalElapsed — millis wrap-around handled correctly") {
  mockMillis = ULONG_MAX - 500UL;
  unsigned long last = mockMillis;
  mockMillis = 600UL;  // simulazione wrap-around
  CHECK(intervalElapsed(last, 1000) == true);
  CHECK(last == mockMillis);
}

// ─── normalizeNewsText ────────────────────────────────────────────────────────

TEST_CASE("normalizeNewsText — only spaces becomes empty") {
  String s = "   ";
  normalizeNewsText(s);
  CHECK(strcmp(s.c_str(), "") == 0);
}

TEST_CASE("normalizeNewsText — only newlines becomes empty") {
  String s = "\n\n\n";
  normalizeNewsText(s);
  CHECK(strcmp(s.c_str(), "") == 0);
}

TEST_CASE("normalizeNewsText — multiple spaces collapse to one") {
  String s = "hello   world";
  normalizeNewsText(s);
  CHECK(strcmp(s.c_str(), "hello world") == 0);
}

TEST_CASE("normalizeNewsText — mixed whitespace collapses and trims") {
  String s = "  foo\n\r bar  ";
  normalizeNewsText(s);
  CHECK(strcmp(s.c_str(), "foo bar") == 0);
}

TEST_CASE("normalizeNewsText — normal text unchanged") {
  String s = "hello world";
  normalizeNewsText(s);
  CHECK(strcmp(s.c_str(), "hello world") == 0);
}

// ─── buildNewsFooterText ──────────────────────────────────────────────────────

TEST_CASE("buildNewsFooterText — ready state shows ticker") {
  app.news = NewsState{};
  app.news.state = SERVICE_FETCH_READY;
  app.news.itemCount = 2;
  strlcpy(app.news.ticker, "notizia uno • notizia due", NEWS_MAX_TICKER_LEN);

  char buf[256];
  buildNewsFooterText(buf, sizeof(buf));
  CHECK(strcmp(buf, "notizia uno • notizia due") == 0);
}

TEST_CASE("buildNewsFooterText — ready state with empty ticker") {
  app.news = NewsState{};
  app.news.state = SERVICE_FETCH_READY;
  app.news.itemCount = 0;
  app.news.ticker[0] = '\0';

  char buf[128];
  buildNewsFooterText(buf, sizeof(buf));
  CHECK(strcmp(buf, "NEWS | feed live ma vuoto") == 0);
}

TEST_CASE("buildNewsFooterText — truncates long ticker into small buffer") {
  app.news = NewsState{};
  app.news.state = SERVICE_FETCH_READY;
  app.news.itemCount = 1;
  memset(app.news.ticker, 'A', NEWS_MAX_TICKER_LEN - 1);
  app.news.ticker[NEWS_MAX_TICKER_LEN - 1] = '\0';

  char smallBuf[32];
  buildNewsFooterText(smallBuf, sizeof(smallBuf));
  CHECK(strlen(smallBuf) == sizeof(smallBuf) - 1);
  for (size_t i = 0; i < sizeof(smallBuf) - 1; i++) {
    CHECK(smallBuf[i] == 'A');
  }
}

TEST_CASE("buildNewsFooterText — error states show status text") {
  app.news = NewsState{};
  char buf[256];

  SUBCASE("Idle") {
    app.news.state = SERVICE_FETCH_IDLE;
    buildNewsFooterText(buf, sizeof(buf));
    CHECK(strstr(buf, "attesa") != nullptr);
  }

  SUBCASE("Fetching") {
    app.news.state = SERVICE_FETCH_FETCHING;
    buildNewsFooterText(buf, sizeof(buf));
    CHECK(strstr(buf, "recupero") != nullptr);
  }

  SUBCASE("Offline") {
    app.news.state = SERVICE_FETCH_OFFLINE;
    buildNewsFooterText(buf, sizeof(buf));
    CHECK(strstr(buf, "offline") != nullptr);
  }

  SUBCASE("Config missing") {
    app.news.state = SERVICE_FETCH_CONFIG_MISSING;
    buildNewsFooterText(buf, sizeof(buf));
    CHECK(strstr(buf, "config") != nullptr);
  }

  SUBCASE("HTTP error includes code") {
    app.news.state = SERVICE_FETCH_HTTP_ERROR;
    app.news.lastHttpCode = 503;
    buildNewsFooterText(buf, sizeof(buf));
    CHECK(strstr(buf, "503") != nullptr);
  }

  SUBCASE("Invalid payload") {
    app.news.state = SERVICE_FETCH_INVALID_PAYLOAD;
    buildNewsFooterText(buf, sizeof(buf));
    CHECK(strstr(buf, "payload") != nullptr);
  }
}

TEST_CASE("buildNewsFooterText — error state with cached ticker appends cache") {
  app.news = NewsState{};
  app.news.state = SERVICE_FETCH_OFFLINE;
  app.news.itemCount = 1;
  strlcpy(app.news.ticker, "vecchia notizia", NEWS_MAX_TICKER_LEN);

  char buf[256];
  buildNewsFooterText(buf, sizeof(buf));
  CHECK(strstr(buf, "cache") != nullptr);
  CHECK(strstr(buf, "vecchia notizia") != nullptr);
}
