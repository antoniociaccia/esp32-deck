#include "dashboard_services.h"
#include "dashboard_services_common.h"
#include "dashboard_app.h"
#include "dashboard_support.h"
#include "config_debug.h"
#include "config_timing.h"
#include <HTTPClient.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "secrets.h"

static bool newsUiStateChanged(
  ServiceFetchState previousState,
  int previousHttpCode,
  bool previousValid,
  int previousNewsItemCount,
  const char *previousNewsTicker,
  const char *previousFirstNewsItem) {
  return previousState != app.news.state
    || previousHttpCode != app.news.lastHttpCode
    || previousValid != app.news.valid
    || previousNewsItemCount != app.news.itemCount
    || strcmp(previousNewsTicker, app.news.ticker) != 0
    || strcmp(previousFirstNewsItem, app.news.itemCount > 0 ? app.news.items[0] : "") != 0;
}

static void markNewsUiDirtyIfChanged(
  ServiceFetchState previousState,
  int previousHttpCode,
  bool previousValid,
  int previousNewsItemCount,
  const char *previousNewsTicker,
  const char *previousFirstNewsItem) {
  if (newsUiStateChanged(
      previousState,
      previousHttpCode,
      previousValid,
      previousNewsItemCount,
      previousNewsTicker,
      previousFirstNewsItem)) {
    markUiDirty(UI_DIRTY_FOOTER | UI_DIRTY_MAIN_NEWS);
  }
}

static void logNewsStateIfChanged(
  ServiceFetchState previousState,
  int previousHttpCode,
  bool previousValid,
  int previousNewsItemCount,
  const char *previousNewsTicker,
  const char *previousFirstNewsItem) {
  if (!newsUiStateChanged(
      previousState,
      previousHttpCode,
      previousValid,
      previousNewsItemCount,
      previousNewsTicker,
      previousFirstNewsItem)) {
    return;
  }

  DEBUG_NETWORK_PRINTF(
    "News state=%s http=%d valid=%d items=%d\n",
    serviceFetchStateLabel(app.news.state),
    app.news.lastHttpCode,
    app.news.valid ? 1 : 0,
    app.news.itemCount);
}

static bool handleNewsTestMode(
  ServiceFetchState previousNewsState,
  int previousNewsHttpCode,
  bool previousNewsValid,
  int previousNewsItemCount,
  const char *previousNewsTicker,
  const char *previousFirstNewsItem) {
  switch (DEBUG_NEWS_TEST_MODE) {
    case NETWORK_TEST_MODE_DISABLED:
      return false;
    case NETWORK_TEST_MODE_OFFLINE:
      app.news.valid = false;
      app.news.state = SERVICE_FETCH_OFFLINE;
      app.news.lastHttpCode = 0;
      break;
    case NETWORK_TEST_MODE_CONFIG_MISSING:
      app.news.valid = false;
      app.news.state = SERVICE_FETCH_CONFIG_MISSING;
      app.news.lastHttpCode = 0;
      break;
    case NETWORK_TEST_MODE_TRANSPORT_ERROR:
      app.news.valid = false;
      app.news.state = SERVICE_FETCH_TRANSPORT_ERROR;
      app.news.lastHttpCode = 0;
      break;
    case NETWORK_TEST_MODE_HTTP_ERROR:
      app.news.valid = false;
      app.news.state = SERVICE_FETCH_HTTP_ERROR;
      app.news.lastHttpCode = DEBUG_NEWS_TEST_HTTP_CODE;
      break;
    case NETWORK_TEST_MODE_INVALID_PAYLOAD:
      app.news.valid = false;
      app.news.state = SERVICE_FETCH_INVALID_PAYLOAD;
      app.news.lastHttpCode = HTTP_CODE_OK;
      break;
    case NETWORK_TEST_MODE_SUCCESS_MOCK: {
      static const String mockPayload =
        "{\"items\":["
        "{\"text\":\"TEST | Feed locale attivo\"},"
        "{\"text\":\"TECH | Simulazione news in corso\"},"
        "{\"text\":\"WORLD | Backend reale non necessario per questa prova\"}"
        "]}";
      app.news.valid = parseNewsItems(mockPayload);
      app.news.state = app.news.valid ? SERVICE_FETCH_READY : SERVICE_FETCH_INVALID_PAYLOAD;
      app.news.lastHttpCode = HTTP_CODE_OK;
      break;
    }
    default:
      return false;
  }

  DEBUG_NETWORK_PRINTF("News test mode active: %u\n", DEBUG_NEWS_TEST_MODE);
  markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
    previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
  return true;
}

void updateNewsFeed() {
  unsigned long refreshInterval = app.news.valid ? TIMING_NEWS_REFRESH_MS : TIMING_NEWS_RETRY_MS;
  if (!intervalElapsed(app.news.lastFetchMs, refreshInterval)) {
    return;
  }

  bool previousNewsValid = app.news.valid;
  ServiceFetchState previousNewsState = app.news.state;
  int previousNewsHttpCode = app.news.lastHttpCode;
  int previousNewsItemCount = app.news.itemCount;
  char previousNewsTicker[sizeof(app.news.ticker)];
  char previousFirstNewsItem[NEWS_MAX_TEXT_LEN];
  strlcpy(previousNewsTicker, app.news.ticker, sizeof(previousNewsTicker));
  strlcpy(previousFirstNewsItem, app.news.itemCount > 0 ? app.news.items[0] : "", sizeof(previousFirstNewsItem));

  if (handleNewsTestMode(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem)) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    app.news.valid = false;
    app.news.state = SERVICE_FETCH_OFFLINE;
    app.news.lastHttpCode = 0;
    logNewsStateIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    return;
  }

  if (strlen(NEWS_API_URL) == 0 || strlen(NEWS_API_KEY) == 0) {
    app.news.valid = false;
    app.news.state = SERVICE_FETCH_CONFIG_MISSING;
    app.news.lastHttpCode = 0;
    logNewsStateIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    return;
  }

  WiFiClientSecure client;
  HTTPClient http;
  prepareSecureHttpClient(http, client);
  if (!http.begin(client, NEWS_API_URL)) {
    app.news.valid = false;
    app.news.state = SERVICE_FETCH_TRANSPORT_ERROR;
    app.news.lastHttpCode = 0;
    logNewsStateIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    return;
  }

  http.addHeader("X-API-Key", NEWS_API_KEY);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    app.news.valid = false;
    app.news.state = SERVICE_FETCH_HTTP_ERROR;
    app.news.lastHttpCode = httpCode;
    http.end();
    logNewsStateIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    return;
  }

  String payload = http.getString();
  http.end();

  app.news.valid = parseNewsItems(payload);
  app.news.state = app.news.valid ? SERVICE_FETCH_READY : SERVICE_FETCH_INVALID_PAYLOAD;
  app.news.lastHttpCode = httpCode;
  logNewsStateIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
    previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
  markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
    previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
}
