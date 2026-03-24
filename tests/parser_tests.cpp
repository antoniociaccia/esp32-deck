#include <cassert>
#include <cstdio>
#include <cstring>

#include "dashboard_support.h"

static void testSetDefaultNewsItems() {
  setDefaultNewsItems();
  assert(app.newsItemCount == NEWS_DEFAULT_ITEM_COUNT);
  assert(std::strlen(app.newsTicker) > 0);
}

static void testParseWeatherPayloadSuccess() {
  int temperature = 0;
  char iconCode[8] = {};
  bool ok = parseWeatherPayload(
    String("{\"main\":{\"temp\":18},\"weather\":[{\"icon\":\"10d\"}]}"),
    temperature,
    iconCode,
    sizeof(iconCode));

  assert(ok);
  assert(temperature == 18);
  assert(std::strcmp(iconCode, "10d") == 0);
}

static void testParseWeatherPayloadFailure() {
  int temperature = 0;
  char iconCode[8] = {};
  bool ok = parseWeatherPayload(
    String("{\"main\":{\"humidity\":90},\"weather\":[{\"description\":\"cloudy\"}]}"),
    temperature,
    iconCode,
    sizeof(iconCode));

  assert(!ok);
}

static void testParseNewsItemsSuccess() {
  bool ok = parseNewsItems(
    String("{\"items\":[{\"text\":\"TECH | Primo titolo\"},{\"text\":\"WORLD | Riga con\\nspazio\"},{\"text\":\"ITALIA | Voce con \\\"quote\\\"\"}]}"));

  assert(ok);
  assert(app.newsItemCount == 3);
  assert(std::strcmp(app.newsItems[0], "TECH | Primo titolo") == 0);
  assert(std::strcmp(app.newsItems[1], "WORLD | Riga con spazio") == 0);
  assert(std::strcmp(app.newsItems[2], "ITALIA | Voce con \"quote\"") == 0);
  assert(std::strstr(app.newsTicker, "Primo titolo") != nullptr);
}

static void testParseNewsItemsFailure() {
  setDefaultNewsItems();
  int previousCount = app.newsItemCount;
  char previousTicker[NEWS_MAX_TICKER_LEN];
  strlcpy(previousTicker, app.newsTicker, sizeof(previousTicker));

  bool ok = parseNewsItems(String("{\"items\":\"not-an-array\"}"));

  assert(!ok);
  assert(app.newsItemCount == previousCount);
  assert(std::strcmp(app.newsTicker, previousTicker) == 0);
}

static void testBuildNewsFooterTextReady() {
  bool ok = parseNewsItems(String("{\"items\":[{\"text\":\"TECH | Footer live\"}]}"));
  assert(ok);
  app.newsState = SERVICE_FETCH_READY;
  app.newsLastHttpCode = 200;

  char footer[NEWS_MAX_TICKER_LEN];
  buildNewsFooterText(footer, sizeof(footer));

  assert(std::strcmp(footer, app.newsTicker) == 0);
}

static void testBuildNewsFooterTextCachedHttpError() {
  bool ok = parseNewsItems(String("{\"items\":[{\"text\":\"TECH | Cached titolo\"}]}"));
  assert(ok);
  app.newsValid = false;
  app.newsState = SERVICE_FETCH_HTTP_ERROR;
  app.newsLastHttpCode = 503;

  char footer[NEWS_MAX_TICKER_LEN];
  buildNewsFooterText(footer, sizeof(footer));

  assert(std::strstr(footer, "HTTP 503") != nullptr);
  assert(std::strstr(footer, "cache") != nullptr);
  assert(std::strstr(footer, "Cached titolo") != nullptr);
}

static void testBuildNewsFooterTextOfflineNoCache() {
  app.newsItemCount = 0;
  app.newsTicker[0] = '\0';
  app.newsValid = false;
  app.newsState = SERVICE_FETCH_OFFLINE;
  app.newsLastHttpCode = 0;

  char footer[NEWS_MAX_TICKER_LEN];
  buildNewsFooterText(footer, sizeof(footer));

  assert(std::strstr(footer, "offline") != nullptr);
  assert(std::strstr(footer, "nessun feed disponibile") != nullptr);
}

int main() {
  testSetDefaultNewsItems();
  testParseWeatherPayloadSuccess();
  testParseWeatherPayloadFailure();
  testParseNewsItemsSuccess();
  testParseNewsItemsFailure();
  testBuildNewsFooterTextReady();
  testBuildNewsFooterTextCachedHttpError();
  testBuildNewsFooterTextOfflineNoCache();

  std::puts("parser tests: ok");
  return 0;
}
