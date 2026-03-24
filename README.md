# esp32-deck

ESP32-S3 dashboard for the Freenove 2.8 inch touch display (`240x320`, used in landscape `320x240`).

Current features:

- LVGL-based UI
- capacitive touch with `FT6336U`
- NTP clock
- battery reading via ADC
- weather via OpenWeather
- continuous news ticker in the footer

## Hardware

Target board:

- Freenove ESP32-S3 Display 2.8

Main libraries:

- `TFT_eSPI`
- `lvgl`
- `FT6336U CTP Controller`
- `WiFi.h`
- `HTTPClient`

## Project structure

- [news.ino](news.ino): Arduino entrypoint
- [src/](src): application code split by runtime, UI, services, battery, display
- [assets/weather-icons/](assets/weather-icons): source weather icons
- [flash.sh](flash.sh): compile and upload helper
- [flash-monitor.sh](flash-monitor.sh): upload and serial monitor helper
- [partitions.csv](partitions.csv): current flash layout
- [secrets.example.h](secrets.example.h): local secrets template

## Setup

1. Copy [secrets.example.h](secrets.example.h) to `secrets.h`.
2. Fill in:
   - `WIFI_SSID`
   - `WIFI_PASSWORD`
   - `OPENWEATHER_API_KEY`
   - `WEATHER_CITY_LABEL`
   - `WEATHER_LATITUDE`
   - `WEATHER_LONGITUDE`
   - `NEWS_API_URL`
   - `NEWS_API_KEY`

`secrets.h` is ignored by Git.

## Build and flash

Target FQBN:

- `esp32:esp32:esp32s3`

Useful commands:

- `./flash.sh`
- `./flash.sh --clean`
- `./flash.sh --build-only`
- `./flash.sh --upload-only`
- `./flash.sh --port /dev/cu.usbmodem14301`
- `./flash-monitor.sh`

Notes:

- build output is cached for incremental builds
- upload can be separated from compile
- the scripts detect a busy serial port and show the owning process
- the scripts force `build.cdc_on_boot=1` for ESP32-S3 USB serial at runtime

## Debug

Serial logging is controlled in [src/config_debug.h](src/config_debug.h).

Default behavior uses the production profile, with application logs disabled.
For local debugging, switch `DEBUG_PROFILE` to `DEBUG_PROFILE_DEVELOPMENT`.

## Display setup note

This project depends on a board-specific `TFT_eSPI` configuration.

At the moment that setup lives in the local Arduino environment, not inside this repository. That is why the GitHub Actions workflow only performs repository checks and does not yet compile the firmware in CI.

## Flash layout

The current [partitions.csv](partitions.csv) uses a single large app slot on a `4MB` flash configuration.

That keeps more space for the firmware today, but it is not yet an OTA-ready dual-slot layout.

## External APIs

Weather:

- provider: OpenWeather
- transport: HTTPS

News:

- provider: custom `n8n` webhook
- auth header: `X-API-Key`
- expected payload: `items[].text`

## Status

Stable baseline:

- display ok
- touch ok
- LVGL ok
- Wi-Fi ok

Current next steps are local and intentionally kept out of the public repo backlog for now.
