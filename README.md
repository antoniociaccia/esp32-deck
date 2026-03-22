# esp32-news

Arduino/ESP32 project for a LOLIN D32 Pro with ST7789 display, battery indicator, Wi-Fi status, and NTP date/time.

## Notes

- Target board: `esp32:esp32:d32_pro`
- Upload helper: `./flash.sh`
- Upload + serial monitor: `./flash-monitor.sh`
- Custom flash layout: [`partitions.csv`](/Users/antoniociaccia/Documents/Arduino/news/partitions.csv)

The current configuration assumes:
- `16MB` flash detected on the board
- `8MB` reserved for the application partition
- remaining flash reserved for SPIFFS and system partitions
