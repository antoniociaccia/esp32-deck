#ifndef DASHBOARD_UI_MAIN_H
#define DASHBOARD_UI_MAIN_H

#include <Arduino.h>
#include "lvgl.h"

void createDashboardMain(lv_obj_t *parent);
void refreshDashboardMainUi(uint32_t dirtyMask);

#endif
