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

static void setNewsFetchFailure(ServiceFetchState state, int httpCode = 0) {
  app.news.valid = false;
  app.news.state = state;
  app.news.lastHttpCode = httpCode;
}

static bool handleNewsTestMode() {
  switch (DEBUG_NEWS_TEST_MODE) {
    case NETWORK_TEST_MODE_DISABLED:
      return false;
    case NETWORK_TEST_MODE_OFFLINE:
      setNewsFetchFailure(SERVICE_FETCH_OFFLINE);
      break;
    case NETWORK_TEST_MODE_CONFIG_MISSING:
      setNewsFetchFailure(SERVICE_FETCH_CONFIG_MISSING);
      break;
    case NETWORK_TEST_MODE_TRANSPORT_ERROR:
      setNewsFetchFailure(SERVICE_FETCH_TRANSPORT_ERROR);
      break;
    case NETWORK_TEST_MODE_HTTP_ERROR:
      setNewsFetchFailure(SERVICE_FETCH_HTTP_ERROR, DEBUG_NEWS_TEST_HTTP_CODE);
      break;
    case NETWORK_TEST_MODE_INVALID_PAYLOAD:
      setNewsFetchFailure(SERVICE_FETCH_INVALID_PAYLOAD, HTTP_CODE_OK);
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
  return true;
}

void updateNewsFeed() {
  if (!app.settings.newsEnabled) {
    return;
  }
  unsigned long refreshInterval = app.news.valid ? TIMING_NEWS_REFRESH_MS : TIMING_NEWS_RETRY_MS;
  if (!intervalElapsed(app.news.lastFetchMs, refreshInterval)) {
    return;
  }

  ServiceSnapshot<NewsState> snap(app.news, UI_DIRTY_FOOTER | UI_DIRTY_MAIN_NEWS);

  if (handleNewsTestMode()) {
    snap.commitIfChanged("News");
    return;
  }

  if (WiFi.status() != WL_CONNECTED) {
    setNewsFetchFailure(SERVICE_FETCH_OFFLINE);
    snap.commitIfChanged("News");
    return;
  }

  if (strlen(NEWS_API_URL) == 0 || strlen(NEWS_API_KEY) == 0) {
    setNewsFetchFailure(SERVICE_FETCH_CONFIG_MISSING);
    snap.commitIfChanged("News");
    return;
  }

  app.news.state = SERVICE_FETCH_FETCHING;
  snap.commitAndPumpUi("News");

  WiFiClientSecure client;
  HTTPClient http;
  prepareSecureHttpClient(http, client);
  if (!http.begin(client, NEWS_API_URL)) {
    setNewsFetchFailure(SERVICE_FETCH_TRANSPORT_ERROR);
    snap.commitIfChanged("News");
    return;
  }

  http.addHeader("X-API-Key", NEWS_API_KEY);
  int httpCode = http.GET();
  if (httpCode != HTTP_CODE_OK) {
    setNewsFetchFailure(SERVICE_FETCH_HTTP_ERROR, httpCode);
    http.end();
    snap.commitIfChanged("News");
    return;
  }

  String payload;
  int len = http.getSize();
  if (len > 0) {
    payload.reserve(len);
  }
  payload = http.getString();
  http.end();

  app.news.valid = parseNewsItems(payload);
  app.news.state = app.news.valid ? SERVICE_FETCH_READY : SERVICE_FETCH_INVALID_PAYLOAD;
  app.news.lastHttpCode = httpCode;
  snap.commitIfChanged("News");
}
