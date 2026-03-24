#ifndef DASHBOARD_UI_SHARED_H
#define DASHBOARD_UI_SHARED_H

#include "lvgl.h"

void setDashboardLabelFont(lv_obj_t *label, const lv_font_t *font);
void setDashboardLabelColor(lv_obj_t *label, uint32_t colorHex);
bool setDashboardLabelTextIfChanged(lv_obj_t *label, const char *text);
bool setDashboardImageSourceIfChanged(lv_obj_t *image, const void *src);
void styleDashboardPanel(lv_obj_t *panel, uint32_t bgColorHex, uint32_t borderColorHex);
void createDashboardSpacer(lv_obj_t *parent, lv_coord_t height);

#endif
