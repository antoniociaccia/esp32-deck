#include "doctest.h"
#include "dashboard_support.h"
#include "dashboard_services_common.h"
#include <cstring>

TEST_CASE("Set Default News Items") {
  setDefaultNewsItems();
  CHECK(app.news.itemCount == NEWS_DEFAULT_ITEM_COUNT);
  CHECK(std::strlen(app.news.ticker) > 0);
}

TEST_CASE("News Items Parsing") {
  SUBCASE("Success with multiple valid items") {
    bool ok = parseNewsItems(
      String("{\"items\":[{\"text\":\"TECH | Primo titolo\"},{\"text\":\"WORLD | Riga con\\nspazio\"},{\"text\":\"ITALIA | Voce con \\\"quote\\\"\"}]}"));

    CHECK(ok == true);
    CHECK(app.news.itemCount == 3);
    CHECK(std::strcmp(app.news.items[0], "TECH | Primo titolo") == 0);
    CHECK(std::strcmp(app.news.items[1], "WORLD | Riga con spazio") == 0);
    CHECK(std::strcmp(app.news.items[2], "ITALIA | Voce con \"quote\"") == 0);
    CHECK(std::strstr(app.news.ticker, "Primo titolo") != nullptr);
  }

  SUBCASE("Failure on non-array items") {
    setDefaultNewsItems();
    int previousCount = app.news.itemCount;
    char previousTicker[NEWS_MAX_TICKER_LEN];
    strlcpy(previousTicker, app.news.ticker, sizeof(previousTicker));

    bool ok = parseNewsItems(String("{\"items\":\"not-an-array\"}"));

    CHECK(ok == false);
    CHECK(app.news.itemCount == previousCount);
    CHECK(std::strcmp(app.news.ticker, previousTicker) == 0);
  }
}

TEST_CASE("News Footer Text Building") {
  SUBCASE("State READY") {
    bool ok = parseNewsItems(String("{\"items\":[{\"text\":\"TECH | Footer live\"}]}"));
    CHECK(ok == true);
    app.news.state = SERVICE_FETCH_READY;
    app.news.lastHttpCode = 200;

    char footer[NEWS_MAX_TICKER_LEN];
    buildNewsFooterText(footer, sizeof(footer));

    CHECK(std::strcmp(footer, app.news.ticker) == 0);
  }

  SUBCASE("State HTTP_ERROR with cache") {
    bool ok = parseNewsItems(String("{\"items\":[{\"text\":\"TECH | Cached titolo\"}]}"));
    CHECK(ok == true);
    app.news.valid = false;
    app.news.state = SERVICE_FETCH_HTTP_ERROR;
    app.news.lastHttpCode = 503;

    char footer[NEWS_MAX_TICKER_LEN];
    buildNewsFooterText(footer, sizeof(footer));

    CHECK(std::strstr(footer, "HTTP 503") != nullptr);
    CHECK(std::strstr(footer, "cache") != nullptr);
    CHECK(std::strstr(footer, "Cached titolo") != nullptr);
  }

  SUBCASE("State OFFLINE without cache") {
    app.news.itemCount = 0;
    app.news.ticker[0] = '\0';
    app.news.valid = false;
    app.news.state = SERVICE_FETCH_OFFLINE;
    app.news.lastHttpCode = 0;

    char footer[NEWS_MAX_TICKER_LEN];
    buildNewsFooterText(footer, sizeof(footer));

    CHECK(std::strstr(footer, "offline") != nullptr);
    CHECK(std::strstr(footer, "nessun feed disponibile") != nullptr);
  }

  SUBCASE("State FETCHING with cache") {
    bool ok = parseNewsItems(String("{\"items\":[{\"text\":\"TECH | Cached titolo\"}]}"));
    CHECK(ok == true);
    app.news.state = SERVICE_FETCH_FETCHING;
    app.news.lastHttpCode = 0;

    char footer[NEWS_MAX_TICKER_LEN];
    buildNewsFooterText(footer, sizeof(footer));

    CHECK(std::strstr(footer, "recupero notizie...") != nullptr);
    CHECK(std::strstr(footer, "cache") != nullptr);
    CHECK(std::strstr(footer, "Cached titolo") != nullptr);
  }
}
