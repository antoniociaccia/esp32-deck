#include "doctest.h"
#include "Arduino.h"
#include "WiFi.h"
#include "HTTPClient.h"
#include "dashboard_services_common.h"
#include "dashboard_services.h"
#include "dashboard_app.h"

TEST_CASE("Weather Network Service") {
  // Setup global mock states
  WiFi.mockStatus = WL_CONNECTED;
  HTTPClient::mockReturnCode = 200;
  HTTPClient::mockBeginResult = true;
  HTTPClient::mockResponseBody = "{\"main\":{\"temp\":15},\"weather\":[{\"icon\":\"01d\"}]}";

  // Reset App State
  app.weather.valid = false;
  app.weather.lastUpdateMs = 0;

  SUBCASE("Success Fetch") {
    mockMillis += 60000;
    updateWeatherUi();
    CHECK(app.weather.valid == true);
    CHECK(app.weather.temperatureC == 15);
    CHECK(app.weather.state == SERVICE_FETCH_READY);
  }

  SUBCASE("WiFi Disconnected") {
    mockMillis += 60000;
    WiFi.mockStatus = WL_DISCONNECTED;
    updateWeatherUi();
    CHECK(app.weather.valid == false);
    CHECK(app.weather.state == SERVICE_FETCH_OFFLINE);
  }

  SUBCASE("HTTP 404 Error") {
    mockMillis += 60000;
    HTTPClient::mockReturnCode = 404;
    updateWeatherUi();
    CHECK(app.weather.valid == false);
    CHECK(app.weather.state == SERVICE_FETCH_HTTP_ERROR);
    CHECK(app.weather.lastHttpCode == 404);
  }

  SUBCASE("Invalid JSON Payload") {
    mockMillis += 60000;
    HTTPClient::mockResponseBody = "Not a JSON";
    updateWeatherUi();
    CHECK(app.weather.valid == false);
    CHECK(app.weather.state == SERVICE_FETCH_INVALID_PAYLOAD);
  }
}

TEST_CASE("News Network Service") {
  WiFi.mockStatus = WL_CONNECTED;
  HTTPClient::mockReturnCode = 200;
  HTTPClient::mockBeginResult = true;
  HTTPClient::mockResponseBody = "{\"items\":[{\"text\":\"TEST NEWS FETCH\"}]}";

  app.news.valid = false;
  app.news.lastFetchMs = 0;

  SUBCASE("Success Fetch") {
    mockMillis += 60000;
    updateNewsFeed();
    CHECK(app.news.valid == true);
    CHECK(app.news.state == SERVICE_FETCH_READY);
    CHECK(app.news.itemCount == 1);
  }
}
