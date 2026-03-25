#include "doctest.h"
#include "dashboard_battery.h"

TEST_CASE("batteryPercentFromVoltage — segment breakpoints") {
  CHECK(batteryPercentFromVoltage(4.20f) == 100);
  CHECK(batteryPercentFromVoltage(4.15f) == 95);
  CHECK(batteryPercentFromVoltage(4.08f) == 85);
  CHECK(batteryPercentFromVoltage(4.00f) == 72);
  CHECK(batteryPercentFromVoltage(3.92f) == 58);
  CHECK(batteryPercentFromVoltage(3.85f) == 42);
  CHECK(batteryPercentFromVoltage(3.78f) == 27);
  CHECK(batteryPercentFromVoltage(3.68f) == 14);
  CHECK(batteryPercentFromVoltage(3.55f) == 6);
  CHECK(batteryPercentFromVoltage(3.30f) == 0);
}

TEST_CASE("batteryPercentFromVoltage — clamping above and below range") {
  CHECK(batteryPercentFromVoltage(4.50f) == 100);
  CHECK(batteryPercentFromVoltage(5.00f) == 100);
  CHECK(batteryPercentFromVoltage(3.00f) == 0);
  CHECK(batteryPercentFromVoltage(0.00f) == 0);
}

TEST_CASE("batteryPercentFromVoltage — midpoint interpolation") {
  // midpoint [4.15, 4.20]: 95 + 0.025/0.05 * 5 = 97.5 → 98
  int pct = batteryPercentFromVoltage(4.175f);
  CHECK(pct >= 97);
  CHECK(pct <= 98);

  // midpoint [3.68, 3.78]: 14 + 0.05/0.10 * 13 = 20.5 → 21
  pct = batteryPercentFromVoltage(3.73f);
  CHECK(pct >= 20);
  CHECK(pct <= 22);

  // midpoint [3.30, 3.55]: 0 + 0.125/0.25 * 6 = 3 → 3
  pct = batteryPercentFromVoltage(3.425f);
  CHECK(pct >= 2);
  CHECK(pct <= 4);
}
