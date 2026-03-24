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
  return previousState != app.newsState
    || previousHttpCode != app.newsLastHttpCode
    || previousValid != app.newsValid
    || previousNewsItemCount != app.newsItemCount
    || strcmp(previousNewsTicker, app.newsTicker) != 0
    || strcmp(previousFirstNewsItem, app.newsItemCount > 0 ? app.newsItems[0] : "") != 0;
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
    serviceFetchStateLabel(app.newsState),
    app.newsLastHttpCode,
    app.newsValid ? 1 : 0,
    app.newsItemCount);
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
      app.newsValid = false;
      app.newsState = SERVICE_FETCH_OFFLINE;
      app.newsLastHttpCode = 0;
      break;
    case NETWORK_TEST_MODE_CONFIG_MISSING:
      app.newsValid = false;
      app.newsState = SERVICE_FETCH_CONFIG_MISSING;
      app.newsLastHttpCode = 0;
      break;
    case NETWORK_TEST_MODE_TRANSPORT_ERROR:
      app.newsValid = false;
      app.newsState = SERVICE_FETCH_TRANSPORT_ERROR;
      app.newsLastHttpCode = 0;
      break;
    case NETWORK_TEST_MODE_HTTP_ERROR:
      app.newsValid = false;
      app.newsState = SERVICE_FETCH_HTTP_ERROR;
      app.newsLastHttpCode = DEBUG_NEWS_TEST_HTTP_CODE;
      break;
    case NETWORK_TEST_MODE_INVALID_PAYLOAD:
      app.newsValid = false;
      app.newsState = SERVICE_FETCH_INVALID_PAYLOAD;
      app.newsLastHttpCode = HTTP_CODE_OK;
      break;
    case NETWORK_TEST_MODE_SUCCESS_MOCK: {
      static const String mockPayload =
        "{\"items\":["
        "{\"text\":\"TEST | Feed locale attivo\"},"
        "{\"text\":\"TECH | Simulazione news in corso\"},"
        "{\"text\":\"WORLD | Backend reale non necessario per questa prova\"}"
        "]}";
      app.newsValid = parseNewsItems(mockPayload);
      app.newsState = app.newsValid ? SERVICE_FETCH_READY : SERVICE_FETCH_INVALID_PAYLOAD;
      app.newsLastHttpCode = HTTP_CODE_OK;
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
  unsigned long refreshInterval = app.newsValid ? TIMING_NEWS_REFRESH_MS : TIMING_NEWS_RETRY_MS;
  if (!intervalElapsed(app.lastNewsFetchMs, refreshInterval)) {
    return;
  }

  bool previousNewsValid = app.newsValid;
  ServiceFetchState previousNewsState = app.newsState;
  int previousNewsHttpCode = app.newsLastHttpCode;
  int previousNewsItemCount = app.newsItemCount;
  char previousNewsTicker[sizeof(app.newsTicker)];
  char previousFirstNewsItem[NEWS_MAX_TEXT_LEN];
  strlcpy(previousNewsTicker, app.newsTicker, sizeof(previousNewsTicker));
  strlcpy(previousFirstNewsItem, app.newsItemCount > 0 ? app.newsItems[0] : "", sizeof(previousFirstNewsItem));

  if (handleNewsTestMode(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem)) {
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    app.newsValid = false;
    app.newsState = SERVICE_FETCH_OFFLINE;
    app.newsLastHttpCode = 0;
    logNewsStateIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    return;
  }

  if (strlen(NEWS_API_URL) == 0 || strlen(NEWS_API_KEY) == 0) {
    app.newsValid = false;
    app.newsState = SERVICE_FETCH_CONFIG_MISSING;
    app.newsLastHttpCode = 0;
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
    app.newsValid = false;
    app.newsState = SERVICE_FETCH_TRANSPORT_ERROR;
    app.newsLastHttpCode = 0;
    logNewsStateIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    return;
  }

  http.addHeader("X-API-Key", NEWS_API_KEY);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    app.newsValid = false;
    app.newsState = SERVICE_FETCH_HTTP_ERROR;
    app.newsLastHttpCode = httpCode;
    http.end();
    logNewsStateIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
      previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
    return;
  }

  String payload = http.getString();
  http.end();

  app.newsValid = parseNewsItems(payload);
  app.newsState = app.newsValid ? SERVICE_FETCH_READY : SERVICE_FETCH_INVALID_PAYLOAD;
  app.newsLastHttpCode = httpCode;
  logNewsStateIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
    previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
  markNewsUiDirtyIfChanged(previousNewsState, previousNewsHttpCode, previousNewsValid,
    previousNewsItemCount, previousNewsTicker, previousFirstNewsItem);
}
