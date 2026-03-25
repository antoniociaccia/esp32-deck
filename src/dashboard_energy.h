#ifndef DASHBOARD_ENERGY_H
#define DASHBOARD_ENERGY_H

#include "display.h"

void notifyUserActivity();
void applyEnergyPolicy(Display &screen);
void applyWifiUserEnabled(bool enabled);

#endif
