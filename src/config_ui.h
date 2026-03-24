#ifndef CONFIG_UI_H
#define CONFIG_UI_H

#include "lvgl.h"

static constexpr lv_coord_t UI_HEADER_WIDTH = 314;
static constexpr lv_coord_t UI_HEADER_HEIGHT = 30;
static constexpr lv_coord_t UI_MAIN_WIDTH = 314;
static constexpr lv_coord_t UI_MAIN_HEIGHT = 136;
static constexpr lv_coord_t UI_FOOTER_WIDTH = 314;
static constexpr lv_coord_t UI_FOOTER_HEIGHT = 40;
static constexpr lv_coord_t UI_PANEL_RADIUS = 4;

static constexpr uint32_t UI_COLOR_HEADER_BG = 0x18344A;
static constexpr uint32_t UI_COLOR_HEADER_BORDER = 0x28485E;
static constexpr uint32_t UI_COLOR_MAIN_BG = 0xFFF8EF;
static constexpr uint32_t UI_COLOR_MAIN_BORDER = 0xE7D8C7;
static constexpr uint32_t UI_COLOR_CARD_ACTIVE_BG = 0xFFF2E2;
static constexpr uint32_t UI_COLOR_CARD_ACTIVE_BORDER = 0xD6A770;
static constexpr uint32_t UI_COLOR_SCREEN_BG = 0xD7E6F1;
static constexpr uint32_t UI_COLOR_ACCENT = 0x8A3B12;
static constexpr uint32_t UI_COLOR_TEXT_PRIMARY = 0x111827;
static constexpr uint32_t UI_COLOR_TEXT_SECONDARY = 0x475569;
static constexpr uint32_t UI_COLOR_TEXT_LIGHT = 0xF8FAFC;
static constexpr uint32_t UI_COLOR_TEXT_INFO = 0xCFE8FF;
static constexpr uint32_t UI_COLOR_TEXT_NEWS = 0xE2E8F0;
static constexpr uint32_t UI_COLOR_TEXT_MUTED = 0x94A3B8;
static constexpr uint32_t UI_COLOR_BATTERY_OK = 0xC7F9CC;
static constexpr uint32_t UI_COLOR_WIFI_ONLINE = 0x86EFAC;
static constexpr uint32_t UI_COLOR_DOT_IDLE = 0xC6B8AA;
static constexpr uint32_t UI_COLOR_BADGE_OK_BG = 0xDFF6E3;
static constexpr uint32_t UI_COLOR_BADGE_RETRY_BG = 0xFDE6C8;
static constexpr uint32_t UI_COLOR_BADGE_OFFLINE_BG = 0xE2E8F0;
static constexpr uint32_t UI_COLOR_BADGE_ALERT_BG = 0xFAD2CF;
static constexpr uint32_t UI_COLOR_CARD_ICON_SOFT = 0xB86A2A;

static constexpr int UI_MODULE_COUNT = 4;

#endif
