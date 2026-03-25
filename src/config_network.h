#ifndef CONFIG_NETWORK_H
#define CONFIG_NETWORK_H

static constexpr uint16_t NETWORK_HTTP_TIMEOUT_MS = 10000;
static constexpr uint32_t NETWORK_OTA_DOWNLOAD_TIMEOUT_MS = 120000;
static constexpr uint32_t NETWORK_OTA_SLOT_MAX_BYTES = 0x1E0000;

static constexpr int NEWS_DEFAULT_ITEM_COUNT = 3;
static constexpr int NEWS_MAX_ITEMS = 12;
static constexpr int NEWS_MAX_TEXT_LEN = 128;
static constexpr int NEWS_MAX_TICKER_LEN = 1024;

#endif
