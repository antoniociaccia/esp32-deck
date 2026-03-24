#include "dashboard_battery.h"
#include "dashboard_app.h"
#include "dashboard_ui.h"
#include <math.h>

static float clampBatteryVoltage(float value) {
  if (value < BATTERY_EMPTY_VOLTS) {
    return BATTERY_EMPTY_VOLTS;
  }
  if (value > BATTERY_FULL_VOLTS) {
    return BATTERY_FULL_VOLTS;
  }
  return value;
}

static float interpolateSegment(float voltage, float v1, float p1, float v2, float p2) {
  if (v2 <= v1) {
    return p1;
  }

  float ratio = (voltage - v1) / (v2 - v1);
  return p1 + (ratio * (p2 - p1));
}

static int batteryPercentFromVoltage(float voltage) {
  float clamped = clampBatteryVoltage(voltage);

  // Piecewise LiPo approximation for a 1-cell battery under light load.
  float percentFloat = 0.0f;
  if (clamped >= 4.15f) {
    percentFloat = interpolateSegment(clamped, 4.15f, 95.0f, 4.20f, 100.0f);
  } else if (clamped >= 4.08f) {
    percentFloat = interpolateSegment(clamped, 4.08f, 85.0f, 4.15f, 95.0f);
  } else if (clamped >= 4.00f) {
    percentFloat = interpolateSegment(clamped, 4.00f, 72.0f, 4.08f, 85.0f);
  } else if (clamped >= 3.92f) {
    percentFloat = interpolateSegment(clamped, 3.92f, 58.0f, 4.00f, 72.0f);
  } else if (clamped >= 3.85f) {
    percentFloat = interpolateSegment(clamped, 3.85f, 42.0f, 3.92f, 58.0f);
  } else if (clamped >= 3.78f) {
    percentFloat = interpolateSegment(clamped, 3.78f, 27.0f, 3.85f, 42.0f);
  } else if (clamped >= 3.68f) {
    percentFloat = interpolateSegment(clamped, 3.68f, 14.0f, 3.78f, 27.0f);
  } else if (clamped >= 3.55f) {
    percentFloat = interpolateSegment(clamped, 3.55f, 6.0f, 3.68f, 14.0f);
  } else {
    percentFloat = interpolateSegment(clamped, 3.30f, 0.0f, 3.55f, 6.0f);
  }

  int percent = (int)lroundf(percentFloat);
  if (percent < 0) {
    return 0;
  }
  if (percent > 100) {
    return 100;
  }
  return percent;
}

static BatteryReading readBatteryVoltage() {
  uint32_t totalMilliVolts = 0;
  int minMilliVolts = 1000000;
  int maxMilliVolts = 0;

  for (int i = 0; i < BATTERY_SAMPLE_COUNT; ++i) {
    int milliVolts = analogReadMilliVolts(BAT_PIN);
    totalMilliVolts += milliVolts;
    if (milliVolts < minMilliVolts) {
      minMilliVolts = milliVolts;
    }
    if (milliVolts > maxMilliVolts) {
      maxMilliVolts = milliVolts;
    }
    delay(2);
  }

  float averageMilliVolts = totalMilliVolts / (float)BATTERY_SAMPLE_COUNT;
  float voltage = (averageMilliVolts / 1000.0f) * BATTERY_DIVIDER_RATIO * BATTERY_CALIBRATION_FACTOR;
  int spreadMilliVolts = maxMilliVolts - minMilliVolts;
  bool present = voltage >= BATTERY_PRESENT_MIN_VOLTS &&
    voltage <= BATTERY_PRESENT_MAX_VOLTS &&
    spreadMilliVolts <= BATTERY_MAX_STABLE_SPREAD_MV;

  return {voltage, spreadMilliVolts, present};
}

void initBatteryMonitoring() {
  analogSetPinAttenuation(BAT_PIN, ADC_11db);
}

void updateBatteryUi() {
  if (!intervalElapsed(app.lastBatteryUpdateMs, BATTERY_REFRESH_INTERVAL_MS)) {
    return;
  }

  BatteryReading reading = readBatteryVoltage();
  float measuredVoltage = reading.voltage;

  if (!reading.present) {
    app.batteryPresent = false;
    app.batteryPercent = -1;
    app.batteryVoltage = 0.0f;
    setBatteryUiUnavailable();
    return;
  }

  if (!app.batteryInitialized) {
    app.filteredBatteryVoltage = measuredVoltage;
    app.batteryInitialized = true;
  } else {
    float delta = measuredVoltage - app.filteredBatteryVoltage;
    if (delta > BATTERY_MAX_STEP_VOLTS) {
      measuredVoltage = app.filteredBatteryVoltage + BATTERY_MAX_STEP_VOLTS;
    } else if (delta < -BATTERY_MAX_STEP_VOLTS) {
      measuredVoltage = app.filteredBatteryVoltage - BATTERY_MAX_STEP_VOLTS;
    }

    app.filteredBatteryVoltage = (BATTERY_FILTER_ALPHA * measuredVoltage) +
      ((1.0f - BATTERY_FILTER_ALPHA) * app.filteredBatteryVoltage);
  }

  int batteryPercent = batteryPercentFromVoltage(app.filteredBatteryVoltage);
  app.batteryPresent = true;
  app.batteryPercent = batteryPercent;
  app.batteryVoltage = app.filteredBatteryVoltage;
  setBatteryUiValue(app.filteredBatteryVoltage, batteryPercent);
}
