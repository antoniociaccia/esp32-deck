#ifndef DASHBOARD_UI_H
#define DASHBOARD_UI_H

#include <Arduino.h>

void createDashboardUi();
void updateWifiUi();
void updateModuleUi();
void updateWeatherIconUi(const String &iconCode);
void setWeatherStatus(const char *text);
void setBatteryUiUnavailable();
void setBatteryUiValue(float voltage, int batteryPercent);

#endif
