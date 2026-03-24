#ifndef CONFIG_POWER_H
#define CONFIG_POWER_H

static constexpr int POWER_BATTERY_ADC_PIN = 9;
static constexpr float POWER_BATTERY_DIVIDER_RATIO = 2.0f;
static constexpr float POWER_BATTERY_CALIBRATION_FACTOR = 1.00f;
static constexpr int POWER_BATTERY_SAMPLE_COUNT = 12;
static constexpr float POWER_BATTERY_FILTER_ALPHA = 0.15f;
static constexpr float POWER_BATTERY_MAX_STEP_VOLTS = 0.20f;
static constexpr float POWER_BATTERY_EMPTY_VOLTS = 3.30f;
static constexpr float POWER_BATTERY_FULL_VOLTS = 4.20f;
static constexpr float POWER_BATTERY_PRESENT_MIN_VOLTS = 3.20f;
static constexpr float POWER_BATTERY_PRESENT_MAX_VOLTS = 4.35f;
static constexpr int POWER_BATTERY_MAX_STABLE_SPREAD_MV = 180;

#endif
