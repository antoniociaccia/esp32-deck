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
- [src/version.h](src/version.h): current firmware version, channel, board id
- [src/dashboard_ota.h](src/dashboard_ota.h): OTA manifest contract and version comparison helpers
- [lib/TFT_eSPI/User_Setup.h](lib/TFT_eSPI/User_Setup.h): repository-owned display setup for CI and releases
- [lib/FT6336U_CTP_Controller](lib/FT6336U_CTP_Controller): vendored touch controller library
- [scripts/build-ota-release.sh](scripts/build-ota-release.sh): OTA release build helper
- [scripts/release-tag.sh](scripts/release-tag.sh): one-command tag and push helper for OTA releases
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
   - `OTA_MANIFEST_URL`

Suggested OTA manifest URL when using GitHub Releases in this repository:

- `https://github.com/antoniociaccia/esp32-deck/releases/latest/download/manifest-stable.json`

`secrets.h` is ignored by Git.

## Build and flash

Target FQBN:

- `esp32:esp32:esp32s3`

Useful commands:

- `./flash.sh`
- `./flash.sh --clean`
- `./flash.sh --build-only`
- `./flash.sh --upload-only`
- `./flash.sh --ota-layout --build-only`
- `./flash.sh --port /dev/cu.usbmodem14301`
- `./flash-monitor.sh`
- `bash ./scripts/release-tag.sh`

Notes:

- build output is cached for incremental builds
- upload can be separated from compile
- `--ota-layout` compiles against [partitions_ota.csv](partitions_ota.csv) without replacing the default layout
- the scripts detect a busy serial port and show the owning process
- the scripts force `build.cdc_on_boot=1` for ESP32-S3 USB serial at runtime

## Debug

Serial logging is controlled in [src/config_debug.h](src/config_debug.h).

Default behavior uses the production profile, with application logs disabled.
For local debugging, switch `DEBUG_PROFILE` to `DEBUG_PROFILE_DEVELOPMENT`.

For local network-state simulation, use the test constants in [src/config_debug.h](src/config_debug.h):

- `DEBUG_WEATHER_TEST_MODE`
- `DEBUG_NEWS_TEST_MODE`

Available modes:

- `NETWORK_TEST_MODE_DISABLED`
- `NETWORK_TEST_MODE_OFFLINE`
- `NETWORK_TEST_MODE_CONFIG_MISSING`
- `NETWORK_TEST_MODE_TRANSPORT_ERROR`
- `NETWORK_TEST_MODE_HTTP_ERROR`
- `NETWORK_TEST_MODE_INVALID_PAYLOAD`
- `NETWORK_TEST_MODE_SUCCESS_MOCK`

## Display setup note

The board-specific `TFT_eSPI` setup used by this project is now stored in the repository at [lib/TFT_eSPI/User_Setup.h](lib/TFT_eSPI/User_Setup.h).

For local environments you can apply it with:

- `bash ./scripts/apply-tftespi-setup.sh`
- `bash ./scripts/install-lvgl-config.sh`

The touch controller dependency is also vendored in the repository and can be copied into the Arduino sketchbook libraries with:

- `bash ./scripts/install-repo-libraries.sh`

## Flash layout

The current [partitions.csv](partitions.csv) uses a single large app slot on a `4MB` flash configuration.

That keeps more space for the firmware today, but it is not yet an OTA-ready dual-slot layout.

A candidate OTA layout is available in [partitions_ota.csv](partitions_ota.csv), with two `0x1E0000` OTA slots and `otadata`.

## OTA

The repository now includes the OTA groundwork and the first end-to-end GitHub release flow:

- [src/version.h](src/version.h) exposes the current firmware version metadata
- [src/dashboard_ota.h](src/dashboard_ota.h) and `dashboard_ota.cpp` define the manifest contract
- the OTA helpers now distinguish `invalid`, `board`, `channel`, `up-to-date`, `slot`, `battery-low` and `available`
- `updateOtaManifestCheck()` now performs a read-only HTTPS manifest check against `OTA_MANIFEST_URL`
- the popup in the header can now start the OTA apply flow on device
- `./flash.sh --ota-layout --build-only` verifies that the current firmware fits the OTA slot budget
- [.github/workflows/release-ota.yml](.github/workflows/release-ota.yml) builds OTA release assets on Git tags and publishes:
  - a versioned firmware binary such as `esp32-deck-esp32s3-v0.1.0.bin`
  - a stable manifest asset `manifest-stable.json`

Release flow:

1. update [src/version.h](src/version.h)
2. commit and push `main`
3. trigger `Release OTA` from GitHub Actions (`workflow_dispatch`) or run `bash ./scripts/release-tag.sh`
4. wait for `Release OTA` in GitHub Actions

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
