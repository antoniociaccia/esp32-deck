#ifndef DASHBOARD_RUNTIME_H
#define DASHBOARD_RUNTIME_H

#include "display.h"

void enterSafeBootRecovery(Display &screen);
void initializeDashboard(Display &screen);
void runSafeModeLoop();
void runDashboardLoop(Display &screen);

#endif
