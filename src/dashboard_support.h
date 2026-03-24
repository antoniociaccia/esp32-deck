#ifndef DASHBOARD_SUPPORT_H
#define DASHBOARD_SUPPORT_H

#include <Arduino.h>
#include "lvgl.h"
#include "config_ui.h"
#include "config_network.h"
#include "dashboard_app.h"

extern const char *const DEFAULT_NEWS_ITEMS[NEWS_DEFAULT_ITEM_COUNT];
extern const ModuleContent MODULES[UI_MODULE_COUNT];

lv_color_t colorFromHex(uint32_t hex);
bool intervalElapsed(unsigned long &lastRunMs, unsigned long intervalMs);

int extractJsonIntAfterKey(const String &payload, const char *key, int fallbackValue);
String extractJsonStringAfterKey(const String &payload, const char *key, const char *fallbackValue);
String decodeJsonString(const String &value);
void normalizeNewsText(String &text);
void rebuildNewsTicker();
void setDefaultNewsItems();
bool parseNewsItems(const String &payload);

#endif
