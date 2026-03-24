#ifndef DASHBOARD_SUPPORT_H
#define DASHBOARD_SUPPORT_H

#include <Arduino.h>
#include "lvgl.h"
#include "config_ui.h"
#include "config_network.h"
#include "dashboard_app.h"

extern const char *const DEFAULT_NEWS_ITEMS[NEWS_DEFAULT_ITEM_COUNT];

lv_color_t colorFromHex(uint32_t hex);
bool intervalElapsed(unsigned long &lastRunMs, unsigned long intervalMs);

String decodeJsonString(const String &value);
void normalizeNewsText(String &text);
void rebuildNewsTicker();
void setDefaultNewsItems();
const char *serviceFetchStateLabel(ServiceFetchState state);
void buildNewsFooterText(char *buffer, size_t bufferSize);
bool parseWeatherPayload(const String &payload, int &temperatureOut, char *iconCodeOut, size_t iconCodeOutSize);
bool parseNewsItems(const String &payload);

#endif
