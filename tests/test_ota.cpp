#include "doctest.h"
#include "dashboard_ota.h"
#include "version.h"
#include <cstring>

TEST_CASE("OTA Manifest Parsing") {
  OtaManifest manifest;

  SUBCASE("Success parsing valid manifest") {
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

    CHECK(ok == true);
    CHECK(std::strcmp(manifest.channel, "stable") == 0);
    CHECK(std::strcmp(manifest.board, "esp32s3") == 0);
    CHECK(std::strcmp(manifest.version, "0.2.0") == 0);
    CHECK(manifest.sizeBytes == 1351793U);
    CHECK(manifest.minBatteryPercent == 40);
  }

  SUBCASE("Failure on incomplete manifest") {
    bool ok = parseOtaManifest(
      String("{\"channel\":\"stable\",\"version\":\"0.2.0\"}"),
      manifest);

    CHECK(ok == false);
  }

  const char *fullManifest =
    "{"
    "\"channel\":\"stable\","
    "\"board\":\"esp32s3\","
    "\"version\":\"0.2.0\","
    "\"build\":\"2026-03-24T18:30:00Z\","
    "\"bin_url\":\"https://example.com/fw.bin\","
    "\"sha256\":\"0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef\","
    "\"size\":1351793,"
    "\"min_battery_percent\":40"
    "}";

  SUBCASE("Failure when board is missing") {
    CHECK(parseOtaManifest(String(
      "{\"channel\":\"stable\",\"version\":\"0.2.0\",\"build\":\"b\","
      "\"bin_url\":\"u\",\"sha256\":\"s\",\"size\":1000,\"min_battery_percent\":0}"),
      manifest) == false);
  }

  SUBCASE("Failure when version is missing") {
    CHECK(parseOtaManifest(String(
      "{\"channel\":\"stable\",\"board\":\"esp32s3\",\"build\":\"b\","
      "\"bin_url\":\"u\",\"sha256\":\"s\",\"size\":1000,\"min_battery_percent\":0}"),
      manifest) == false);
  }

  SUBCASE("Failure when bin_url is missing") {
    CHECK(parseOtaManifest(String(
      "{\"channel\":\"stable\",\"board\":\"esp32s3\",\"version\":\"1.0.0\","
      "\"build\":\"b\",\"sha256\":\"s\",\"size\":1000,\"min_battery_percent\":0}"),
      manifest) == false);
  }

  SUBCASE("Failure when sha256 is missing") {
    CHECK(parseOtaManifest(String(
      "{\"channel\":\"stable\",\"board\":\"esp32s3\",\"version\":\"1.0.0\","
      "\"build\":\"b\",\"bin_url\":\"u\",\"size\":1000,\"min_battery_percent\":0}"),
      manifest) == false);
  }

  SUBCASE("Failure when size is zero or missing") {
    CHECK(parseOtaManifest(String(
      "{\"channel\":\"stable\",\"board\":\"esp32s3\",\"version\":\"1.0.0\","
      "\"build\":\"b\",\"bin_url\":\"u\",\"sha256\":\"s\",\"size\":0,\"min_battery_percent\":0}"),
      manifest) == false);
  }

  SUBCASE("notes field is optional") {
    CHECK(parseOtaManifest(String(fullManifest), manifest) == true);
    CHECK(manifest.notes[0] == '\0');
  }
}

TEST_CASE("Version Strings Comparison") {
  CHECK(compareVersionStrings("0.2.0", "0.1.9") > 0);
  CHECK(compareVersionStrings("v1.0.0", "1.0.0") == 0);
  CHECK(compareVersionStrings("1.0.0", "1.0.1") < 0);
  CHECK(compareVersionStrings("1.10.0", "1.2.0") > 0);

  // Formati inattesi
  CHECK(compareVersionStrings("", "") == 0);
  CHECK(compareVersionStrings("V1.0.0", "1.0.0") == 0);   // prefisso V maiuscolo
  CHECK(compareVersionStrings("1", "1.0.0") == 0);         // componenti mancanti = 0
  CHECK(compareVersionStrings("1.0", "1.0.0") == 0);
  CHECK(compareVersionStrings("2", "1") > 0);
  CHECK(compareVersionStrings("1", "2") < 0);
  CHECK(compareVersionStrings("1.0.0", "1.0") == 0);
}

TEST_CASE("OTA Eligibility") {
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
  
  REQUIRE(ok == true);

  SUBCASE("Update availability checks") {
    CHECK(isOtaManifestCompatible(manifest, FW_BOARD_ID, FW_RELEASE_CHANNEL));
    CHECK(isOtaUpdateAvailable(manifest, FW_VERSION, FW_BOARD_ID, FW_RELEASE_CHANNEL));
    CHECK(isOtaUpdateAvailable(manifest, "0.2.0", FW_BOARD_ID, FW_RELEASE_CHANNEL) == false);
    CHECK(isOtaUpdateAvailable(manifest, FW_VERSION, "esp32", FW_RELEASE_CHANNEL) == false);
  }

  SUBCASE("Eligibility evaluation") {
    CHECK(isOtaManifestValid(manifest));
    CHECK(evaluateOtaEligibility(manifest, FW_VERSION, FW_BOARD_ID, FW_RELEASE_CHANNEL, 1966080U, 75) == OTA_ELIGIBILITY_UPDATE_AVAILABLE);
    CHECK(evaluateOtaEligibility(manifest, FW_VERSION, FW_BOARD_ID, FW_RELEASE_CHANNEL, 1350000U, 75) == OTA_ELIGIBILITY_SLOT_TOO_SMALL);
    CHECK(evaluateOtaEligibility(manifest, FW_VERSION, FW_BOARD_ID, FW_RELEASE_CHANNEL, 1966080U, -1) == OTA_ELIGIBILITY_BATTERY_UNKNOWN);
    CHECK(evaluateOtaEligibility(manifest, FW_VERSION, FW_BOARD_ID, FW_RELEASE_CHANNEL, 1966080U, 20) == OTA_ELIGIBILITY_BATTERY_TOO_LOW);
    CHECK(evaluateOtaEligibility(manifest, FW_VERSION, "esp32", FW_RELEASE_CHANNEL, 1966080U, 75) == OTA_ELIGIBILITY_INCOMPATIBLE_BOARD);
    CHECK(evaluateOtaEligibility(manifest, FW_VERSION, FW_BOARD_ID, "beta", 1966080U, 75) == OTA_ELIGIBILITY_INCOMPATIBLE_CHANNEL);
    CHECK(evaluateOtaEligibility(manifest, "0.2.0", FW_BOARD_ID, FW_RELEASE_CHANNEL, 1966080U, 75) == OTA_ELIGIBILITY_UP_TO_DATE);
    
    CHECK(std::strcmp(otaEligibilityLabel(OTA_ELIGIBILITY_UPDATE_AVAILABLE), "available") == 0);
    CHECK(std::strcmp(otaEligibilityLabel(OTA_ELIGIBILITY_SLOT_TOO_SMALL), "slot") == 0);
  }
}
