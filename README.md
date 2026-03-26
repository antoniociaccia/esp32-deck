# esp32-deck

Dashboard touch per `ESP32-S3` pensata per la `Freenove ESP32-S3 Display 2.8` con interfaccia `LVGL`, meteo via OpenWeather, ticker news remoto e supporto OTA.

Il progetto usa il display in orientamento landscape (`320x240`) e integra:

- UI basata su `LVGL`
- touch capacitivo con `FT6336U`
- sincronizzazione oraria via `NTP`
- lettura batteria via `ADC`
- meteo via `OpenWeather`
- ticker news continuo nel footer
- controllo aggiornamenti firmware OTA

## A chi serve

Questa documentazione e pensata per sviluppatori e maker che vogliono:

- compilare e caricare il firmware sulla board supportata
- configurare i servizi esterni necessari
- capire la struttura del codice prima di modificarlo
- verificare il comportamento locale con test e profili di debug

## Hardware e dipendenze

Board target:

- `Freenove ESP32-S3 Display 2.8`

Librerie principali:

- `TFT_eSPI`
- `lvgl`
- `FT6336U CTP Controller`
- `WiFi.h`
- `HTTPClient`

Configurazioni repository-owned:

- [`lib/TFT_eSPI/User_Setup.h`](/Users/antoniociaccia/Documents/Arduino/news/lib/TFT_eSPI/User_Setup.h)
- [`lv_conf.h`](/Users/antoniociaccia/Documents/Arduino/news/lv_conf.h)
- [`lib/FT6336U_CTP_Controller`](/Users/antoniociaccia/Documents/Arduino/news/lib/FT6336U_CTP_Controller)

## Quick Start

### 1. Configura i segreti locali

Duplica [`secrets.example.h`](/Users/antoniociaccia/Documents/Arduino/news/secrets.example.h) in `secrets.h` e compila i campi richiesti:

- `WIFI_SSID`
- `WIFI_PASSWORD`
- `OPENWEATHER_API_KEY`
- `WEATHER_CITY_LABEL`
- `WEATHER_LATITUDE`
- `WEATHER_LONGITUDE`
- `NEWS_API_URL`
- `NEWS_API_KEY`
- `OTA_MANIFEST_URL`

`secrets.h` resta locale e non deve essere versionato.

Manifest OTA consigliato se usi GitHub Releases in questo repository:

- `https://github.com/antoniociaccia/esp32-deck/releases/latest/download/manifest-stable.json`

### 2. Prepara l'ambiente Arduino

Lo script di build applica automaticamente:

- configurazione `LVGL`
- setup `TFT_eSPI`
- librerie locali vendorizzate

Se vuoi installarle manualmente:

- `bash ./scripts/install-lvgl-config.sh`
- `bash ./scripts/install-repo-libraries.sh`
- `bash ./scripts/apply-tftespi-setup.sh`

### 3. Compila

FQBN usato dal progetto:

- `esp32:esp32:esp32s3`

Comandi principali:

- `./flash.sh`
- `./flash.sh --build-only`
- `./flash.sh --clean`
- `./flash.sh --upload-only`
- `./flash.sh --port /dev/cu.usbmodem14301`

### 4. Carica e monitora

- `./flash.sh`
- `./flash-monitor.sh`

Gli script gestiscono cache di build, rilevamento della porta seriale e controllo della porta occupata.

## Servizi esterni richiesti

### Meteo

Provider:

- `OpenWeather`

Dettagli:

- trasporto `HTTPS`
- query per coordinate geografiche
- lingua risposta impostata a `it`

### News

Provider previsto:

- endpoint HTTP custom, tipicamente un webhook `n8n`

Contratto atteso:

- header `X-API-Key`
- payload con `items[].text`

### OTA

Il device legge un manifest remoto da `OTA_MANIFEST_URL` e verifica:

- board compatibile
- canale di release
- disponibilita di una versione piu recente
- soglia minima batteria
- dimensione firmware compatibile con lo slot OTA

## Flusso di sviluppo

Per un ciclo di sviluppo tipico:

1. aggiorna `secrets.h`
2. esegui `./flash.sh --build-only`
3. se il comportamento runtime cambia, carica il firmware sulla board
4. verifica seriale e UI su hardware reale

Per dettagli di contribuzione vedi [`CONTRIBUTING.md`](/Users/antoniociaccia/Documents/Arduino/news/CONTRIBUTING.md).

## Debug e simulazione locale

La diagnostica seriale e controllata da [`src/config_debug.h`](/Users/antoniociaccia/Documents/Arduino/news/src/config_debug.h).

Profilo predefinito:

- produzione, con log applicativi ridotti

Per sviluppo locale:

- imposta `DEBUG_PROFILE` su `DEBUG_PROFILE_DEVELOPMENT`

Per simulare stati rete senza backend reali:

- `DEBUG_WEATHER_TEST_MODE`
- `DEBUG_NEWS_TEST_MODE`

Modalita disponibili:

- `NETWORK_TEST_MODE_DISABLED`
- `NETWORK_TEST_MODE_OFFLINE`
- `NETWORK_TEST_MODE_CONFIG_MISSING`
- `NETWORK_TEST_MODE_TRANSPORT_ERROR`
- `NETWORK_TEST_MODE_HTTP_ERROR`
- `NETWORK_TEST_MODE_INVALID_PAYLOAD`
- `NETWORK_TEST_MODE_SUCCESS_MOCK`

## Test

I test parser e logica sono sotto [`tests/`](/Users/antoniociaccia/Documents/Arduino/news/tests).

Runner disponibile:

- `bash ./tests/run-parser-tests.sh`

Il runner compila test C++ locali per:

- parsing meteo
- parsing news
- OTA
- UI state
- servizi
- utility
- batteria

Nota: `doctest.h` deve essere presente in `tests/include/`.

## Layout flash

Il repository include due layout:

- [`partitions.csv`](/Users/antoniociaccia/Documents/Arduino/news/partitions.csv): layout principale
- [`partitions_ota.csv`](/Users/antoniociaccia/Documents/Arduino/news/partitions_ota.csv): layout compatibile con dual-slot OTA

Per verificare il budget firmware sul layout OTA:

- `./flash.sh --partitions-file ./partitions_ota.csv --build-only`

## Release OTA

Elementi coinvolti:

- [`src/version.h`](/Users/antoniociaccia/Documents/Arduino/news/src/version.h): versione firmware, canale, board id
- [`src/dashboard_ota.h`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_ota.h): contratto manifest e helper OTA
- [`scripts/build-ota-release.sh`](/Users/antoniociaccia/Documents/Arduino/news/scripts/build-ota-release.sh)
- [`scripts/release-tag.sh`](/Users/antoniociaccia/Documents/Arduino/news/scripts/release-tag.sh)
- [`.github/workflows/release-ota.yml`](/Users/antoniociaccia/Documents/Arduino/news/.github/workflows/release-ota.yml)

Flusso tipico:

1. aggiorna [`src/version.h`](/Users/antoniociaccia/Documents/Arduino/news/src/version.h)
2. esegui commit e push
3. lancia `Release OTA` da GitHub Actions oppure `bash ./scripts/release-tag.sh`
4. attendi la pubblicazione del `.bin` e del manifest stabile

## Struttura del progetto

Punti di ingresso e moduli principali:

- [`news.ino`](/Users/antoniociaccia/Documents/Arduino/news/news.ino): entrypoint Arduino
- [`src/dashboard_app.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_app.cpp): stato applicativo, parser e utility condivise
- [`src/dashboard_runtime.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_runtime.cpp): loop runtime e orchestrazione
- [`src/dashboard_ui_main.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_ui_main.cpp): vista principale
- [`src/dashboard_ui_header.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_ui_header.cpp): header e stato servizi
- [`src/dashboard_ui_footer.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_ui_footer.cpp): ticker footer
- [`src/dashboard_services_weather.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_services_weather.cpp): fetch e parsing meteo
- [`src/dashboard_services_news.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_services_news.cpp): fetch e parsing news
- [`src/dashboard_battery.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_battery.cpp): lettura batteria
- [`src/display.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/display.cpp): bootstrap display e bridge grafico

Una spiegazione piu dettagliata e disponibile in [`docs/architecture.md`](/Users/antoniociaccia/Documents/Arduino/news/docs/architecture.md).

## Documenti correlati

- [`CONTRIBUTING.md`](/Users/antoniociaccia/Documents/Arduino/news/CONTRIBUTING.md)
- [`SECURITY.md`](/Users/antoniociaccia/Documents/Arduino/news/SECURITY.md)
- [`BUGS.md`](/Users/antoniociaccia/Documents/Arduino/news/BUGS.md)
- [`examples/OTA_DESIGN.md`](/Users/antoniociaccia/Documents/Arduino/news/examples/OTA_DESIGN.md)
- [`examples/OPTIMIZATIONS.md`](/Users/antoniociaccia/Documents/Arduino/news/examples/OPTIMIZATIONS.md)

## Stato corrente

Baseline stabile:

- display operativo
- touch operativo
- `LVGL` operativo
- Wi-Fi operativo
- meteo integrato
- ticker news integrato
- controllo OTA disponibile
