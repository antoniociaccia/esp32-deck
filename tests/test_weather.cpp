#include "doctest.h"
#include "dashboard_support.h"
#include <cstring>

TEST_CASE("Weather Payload Parsing") {
  int temperature = 0;
  char iconCode[8] = {};

  SUBCASE("Success parsing integer temperature") {
    bool ok = parseWeatherPayload(
      String("{\"main\":{\"temp\":18},\"weather\":[{\"icon\":\"10d\"}]}"),
      temperature, iconCode, sizeof(iconCode));
    
    CHECK(ok == true);
    CHECK(temperature == 18);
    CHECK(std::strcmp(iconCode, "10d") == 0);
  }

  SUBCASE("Success parsing float temperature") {
    bool ok = parseWeatherPayload(
      String("{\"main\":{\"temp\":18.6},\"weather\":[{\"icon\":\"01n\"}]}"),
      temperature, iconCode, sizeof(iconCode));
    
    CHECK(ok == true);
    CHECK(temperature == 19);
    CHECK(std::strcmp(iconCode, "01n") == 0);
  }

  SUBCASE("Failure on missing temperature or weather") {
    bool ok = parseWeatherPayload(
      String("{\"main\":{\"humidity\":90},\"weather\":[{\"description\":\"cloudy\"}]}"),
      temperature, iconCode, sizeof(iconCode));
    
    CHECK(ok == false);
  }
}
