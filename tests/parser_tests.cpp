#include <cassert>
#include <cstdio>
#include <cstring>

#include "dashboard_ota.h"
#include "dashboard_support.h"
#include "version.h"

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

static void testParseWeatherPayloadFloatSuccess() {
  int temperature = 0;
  char iconCode[8] = {};
  bool ok = parseWeatherPayload(
    String("{\"main\":{\"temp\":18.6},\"weather\":[{\"icon\":\"01n\"}]}"),
    temperature,
    iconCode,
    sizeof(iconCode));

  assert(ok);
  assert(temperature == 19);
  assert(std::strcmp(iconCode, "01n") == 0);
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

static void testParseOtaManifestSuccess() {
  OtaManifest manifest;
  bool ok = parseOtaManifest(
    String("{"
      "\"channel\":\"stable\","
      "\"board\":\"esp32s3\","
      "\"version\":\"0.2.0\","
      "\"build\":\"2026-03-24T18:30:00Z\","
      "\"bin_url\":\"https://example.com/fw.bin\","
      "\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
      "\"size\":1351793,"
      "\"min_battery_percent\":40,"
      "\"notes\":\"OTA release\""
    "}"),
    manifest);

  assert(ok);
  assert(std::strcmp(manifest.channel, "stable") == 0);
  assert(std::strcmp(manifest.board, "esp32s3") == 0);
  assert(std::strcmp(manifest.version, "0.2.0") == 0);
  assert(manifest.sizeBytes == 1351793U);
  assert(manifest.minBatteryPercent == 40);
}

static void testParseOtaManifestFailure() {
  OtaManifest manifest;
  bool ok = parseOtaManifest(
    String("{\"channel\":\"stable\",\"version\":\"0.2.0\"}"),
    manifest);

  assert(!ok);
}

static void testCompareVersionStrings() {
  assert(compareVersionStrings("0.2.0", "0.1.9") > 0);
  assert(compareVersionStrings("v1.0.0", "1.0.0") == 0);
  assert(compareVersionStrings("1.0.0", "1.0.1") < 0);
  assert(compareVersionStrings("1.10.0", "1.2.0") > 0);
}

static void testIsOtaUpdateAvailable() {
  OtaManifest manifest;
  bool ok = parseOtaManifest(
    String("{"
      "\"channel\":\"stable\","
      "\"board\":\"esp32s3\","
      "\"version\":\"0.2.0\","
      "\"build\":\"2026-03-24T18:30:00Z\","
      "\"bin_url\":\"https://example.com/fw.bin\","
      "\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
      "\"size\":1351793,"
      "\"min_battery_percent\":40"
    "}"),
    manifest);

  assert(ok);
  assert(isOtaManifestCompatible(manifest, FW_BOARD_ID, FW_RELEASE_CHANNEL));
  assert(isOtaUpdateAvailable(manifest, FW_VERSION, FW_BOARD_ID, FW_RELEASE_CHANNEL));
  assert(!isOtaUpdateAvailable(manifest, "0.2.0", FW_BOARD_ID, FW_RELEASE_CHANNEL));
  assert(!isOtaUpdateAvailable(manifest, FW_VERSION, "esp32", FW_RELEASE_CHANNEL));
}

static void testEvaluateOtaEligibility() {
  OtaManifest manifest;
  bool ok = parseOtaManifest(
    String("{"
      "\"channel\":\"stable\","
      "\"board\":\"esp32s3\","
      "\"version\":\"0.2.0\","
      "\"build\":\"2026-03-24T18:30:00Z\","
      "\"bin_url\":\"https://example.com/fw.bin\","
      "\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
      "\"size\":1351793,"
      "\"min_battery_percent\":40"
    "}"),
    manifest);

  assert(ok);
  assert(isOtaManifestValid(manifest));
  assert(evaluateOtaEligibility(manifest, FW_VERSION, FW_BOARD_ID, FW_RELEASE_CHANNEL, 1966080U, 75) == OTA_ELIGIBILITY_UPDATE_AVAILABLE);
  assert(evaluateOtaEligibility(manifest, FW_VERSION, FW_BOARD_ID, FW_RELEASE_CHANNEL, 1350000U, 75) == OTA_ELIGIBILITY_SLOT_TOO_SMALL);
  assert(evaluateOtaEligibility(manifest, FW_VERSION, FW_BOARD_ID, FW_RELEASE_CHANNEL, 1966080U, -1) == OTA_ELIGIBILITY_BATTERY_UNKNOWN);
  assert(evaluateOtaEligibility(manifest, FW_VERSION, FW_BOARD_ID, FW_RELEASE_CHANNEL, 1966080U, 20) == OTA_ELIGIBILITY_BATTERY_TOO_LOW);
  assert(evaluateOtaEligibility(manifest, FW_VERSION, "esp32", FW_RELEASE_CHANNEL, 1966080U, 75) == OTA_ELIGIBILITY_INCOMPATIBLE_BOARD);
  assert(evaluateOtaEligibility(manifest, FW_VERSION, FW_BOARD_ID, "beta", 1966080U, 75) == OTA_ELIGIBILITY_INCOMPATIBLE_CHANNEL);
  assert(evaluateOtaEligibility(manifest, "0.2.0", FW_BOARD_ID, FW_RELEASE_CHANNEL, 1966080U, 75) == OTA_ELIGIBILITY_UP_TO_DATE);
  assert(std::strcmp(otaEligibilityLabel(OTA_ELIGIBILITY_UPDATE_AVAILABLE), "available") == 0);
  assert(std::strcmp(otaEligibilityLabel(OTA_ELIGIBILITY_SLOT_TOO_SMALL), "slot") == 0);
}

int main() {
  testSetDefaultNewsItems();
  testParseWeatherPayloadSuccess();
  testParseWeatherPayloadFloatSuccess();
  testParseWeatherPayloadFailure();
  testParseNewsItemsSuccess();
  testParseNewsItemsFailure();
  testBuildNewsFooterTextReady();
  testBuildNewsFooterTextCachedHttpError();
  testBuildNewsFooterTextOfflineNoCache();
  testParseOtaManifestSuccess();
  testParseOtaManifestFailure();
  testCompareVersionStrings();
  testIsOtaUpdateAvailable();
  testEvaluateOtaEligibility();

  std::puts("parser tests: ok");
  return 0;
}
