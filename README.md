# esp32-news

Arduino/ESP32 project currently aligned to the Freenove ESP32-S3 Display 2.8" with:
- `TFT_eSPI` official Freenove setup
- `LVGL` baseline
- capacitive touch `FT6336U`

## Notes

- Target board: `esp32:esp32:esp32s3`
- Upload helper: `./flash.sh`
- Upload + serial monitor: `./flash-monitor.sh`
- Partition layout: [`partitions.csv`](/Users/antoniociaccia/Documents/Arduino/news/partitions.csv)

Current baseline goal:
- verify TFT
- verify LVGL
- verify touch

The old battery/weather/news modules were removed from the active build and can be reintroduced later on top of this stable hardware baseline.
