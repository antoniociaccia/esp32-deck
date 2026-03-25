#ifndef DASHBOARD_BATTERY_H
#define DASHBOARD_BATTERY_H

void initBatteryMonitoring();
void updateBatteryUi();
int batteryPercentFromVoltage(float voltage);

#endif
