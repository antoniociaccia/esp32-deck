# esp32-news

Dashboard ESP32 per il display touch Freenove ESP32-S3 2.8" (`240x320`, usato in landscape `320x240`).

Stato attuale del progetto:
- display `TFT_eSPI` con setup ufficiale Freenove
- UI `LVGL`
- touch capacitivo `FT6336U`
- orario reale via NTP
- batteria letta da ADC
- meteo via OpenWeather
- ticker news continuo nel footer

## Hardware

Board target:
- Freenove ESP32-S3 Display 2.8"

Stack usato:
- `TFT_eSPI`
- `LVGL`
- `FT6336U CTP Controller`
- `WiFi.h`
- `HTTPClient`

## File Principali

Struttura:
- root: sketch Arduino, script, config e documentazione
- `src/`: logica C++ del progetto
- `assets/`: asset SVG e conversioni `LVGL`

- [`news.ino`](/Users/antoniociaccia/Documents/Arduino/news/news.ino): entrypoint Arduino con `setup()` e `loop()`
- [`dashboard_app.h`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_app.h): tipi condivisi, costanti globali e stato app/UI
- [`dashboard_app.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_app.cpp): parser JSON leggero, ticker news e helper condivisi
- [`dashboard_ui.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_ui.cpp): costruzione dashboard `LVGL` e aggiornamenti UI
- [`dashboard_battery.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_battery.cpp): lettura ADC, filtro e logica batteria
- [`dashboard_services.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_services.cpp): Wi-Fi, NTP, meteo e fetch news
- [`dashboard_runtime.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_runtime.cpp): bootstrap dashboard e ciclo runtime
- [`display.h`](/Users/antoniociaccia/Documents/Arduino/news/src/display.h): interfaccia display/touch
- [`display.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/display.cpp): bridge `LVGL + TFT_eSPI + touch`
- [`config_debug.h`](/Users/antoniociaccia/Documents/Arduino/news/src/config_debug.h): flag e macro per debug seriale
- [`lv_conf.h`](/Users/antoniociaccia/Documents/Arduino/news/lv_conf.h): configurazione locale `LVGL`
- [`weather_icons.h`](/Users/antoniociaccia/Documents/Arduino/news/src/weather_icons.h): dichiarazioni asset meteo
- [`weather_icons.c`](/Users/antoniociaccia/Documents/Arduino/news/src/weather_icons.c): inclusione asset meteo generati da `LVGL Image Converter`
- [`partitions.csv`](/Users/antoniociaccia/Documents/Arduino/news/partitions.csv): layout flash single-app senza dual OTA
- [`flash-common.sh`](/Users/antoniociaccia/Documents/Arduino/news/flash-common.sh): helper condivisi per detection porta e controlli seriali
- [`flash.sh`](/Users/antoniociaccia/Documents/Arduino/news/flash.sh): compile + upload
- [`flash-monitor.sh`](/Users/antoniociaccia/Documents/Arduino/news/flash-monitor.sh): upload + monitor seriale
- [`secrets.example.h`](/Users/antoniociaccia/Documents/Arduino/news/secrets.example.h): template credenziali/config
- [`secrets.h`](/Users/antoniociaccia/Documents/Arduino/news/secrets.h): file locale ignorato da git

## Configurazione

1. Copia [`secrets.example.h`](/Users/antoniociaccia/Documents/Arduino/news/secrets.example.h) in `secrets.h`
2. Inserisci:
   - `WIFI_SSID`
   - `WIFI_PASSWORD`
   - `OPENWEATHER_API_KEY`
   - `WEATHER_CITY_LABEL`
   - `WEATHER_LATITUDE`
   - `WEATHER_LONGITUDE`
   - `NEWS_API_URL`
   - `NEWS_API_KEY`

`secrets.h` è escluso dal repo tramite [`.gitignore`](/Users/antoniociaccia/Documents/Arduino/news/.gitignore).

## Build E Upload

Board:
- `esp32:esp32:esp32s3`

Comandi principali:
- `./flash.sh`
- `./flash.sh --clean`
- `./flash.sh --build-only`
- `./flash.sh --upload-only`
- `./flash.sh --port /dev/cu.usbmodem14301`
- `./flash.sh --jobs 0`
- `./flash-monitor.sh`

Note su `flash.sh`:
- usa cache incrementale
- usa compilazione parallela con `--jobs 0` di default
- pulisce la build solo con `--clean`
- permette di separare build e upload con `--build-only` e `--upload-only`
- mostra barra percentuale per flash e RAM
- forza `upload.maximum_size` coerente con la partizione app reale
- forza `build.cdc_on_boot=1` per tenere disponibile la seriale USB runtime su ESP32-S3
- rileva in anticipo se la porta e assente o occupata e mostra il processo che la sta usando

Per velocizzare il ciclo di sviluppo:
- usa `./flash.sh --build-only` quando vuoi solo verificare che compili
- usa `./flash.sh --upload-only` se non hai cambiato codice e vuoi riflashare gli artefatti gia compilati
- evita `--clean` se non stai cambiando toolchain o librerie

## Debug Seriale

Il progetto usa [config_debug.h](/Users/antoniociaccia/Documents/Arduino/news/src/config_debug.h) per controllare i log seriali.

Flag principali:
- `DEBUG_SERIAL_ENABLED`
- `DEBUG_SERIAL_LEVEL`
- `DEBUG_LOG_BOOT`
- `DEBUG_LOG_NETWORK`
- `DEBUG_LOG_POWER`
- `DEBUG_LOG_LVGL`
- `DEBUG_LOG_SAFE`

Livelli disponibili:
- `DEBUG_LEVEL_ERROR`
- `DEBUG_LEVEL_INFO`
- `DEBUG_LEVEL_VERBOSE`

Macro usate nel codice:
- `DEBUG_PRINT(level, enabled, message)`
- `DEBUG_PRINTF(level, enabled, fmt, ...)`

Uso pratico:
- disattiva tutti i log mettendo `DEBUG_SERIAL_ENABLED = false`
- lascia attivi solo i log di rete o boot modificando i flag di gruppo
- abilita `DEBUG_LOG_POWER` se vuoi vedere i cambi di stato batteria
- abilita `DEBUG_LOG_LVGL` solo per diagnosi mirate, perche puo essere molto verboso

## Partizioni

Il progetto usa una partizione custom [`partitions.csv`](/Users/antoniociaccia/Documents/Arduino/news/partitions.csv) pensata per una flash da `4MB` con una sola app grande.

In pratica:
- niente layout OTA classico a doppio slot
- più spazio disponibile per il firmware

## Funzioni Attive

### Header
- data e ora in formato `dd/mm hh:mm`
- meteo con icona vera e temperatura città
- icona Wi-Fi
- batteria con:
  - percentuale
  - voltaggio
  - icona stato batteria

### Main
- card centrale con `lv_tileview`
- swipe orizzontale tra moduli/widget
- pager con indicatori sotto il main

### Footer
- ticker news continuo stile canale all-news
- testo da webhook `n8n`
- fallback a placeholder se rete/API non disponibili

## API Esterne

### Meteo
- provider: OpenWeather
- trasporto: HTTPS

### News
- provider previsto: webhook `n8n`
- autenticazione: header `X-API-Key`
- il firmware si aspetta un JSON con campo `items[].text`

## Asset Meteo

Le icone meteo SVG sorgenti stanno in:
- [`assets/weather-icons/`](/Users/antoniociaccia/Documents/Arduino/news/assets/weather-icons)

Gli asset convertiti per `LVGL v8` stanno in:
- [`assets/weather-icons/lvgl/`](/Users/antoniociaccia/Documents/Arduino/news/assets/weather-icons/lvgl)

Conversione consigliata con `LVGL Image Converter`:
- `LVGL v8`
- `C array`
- `CF_TRUE_COLOR_ALPHA`

## Stato Del Progetto

La base hardware è stabile:
- TFT ok
- touch ok
- LVGL ok
- Wi-Fi ok

Nota:
- il safe recovery usato per la diagnosi USB/TFT è stato disattivato
- il firmware torna ora al percorso completo del dashboard

Le prossime iterazioni sono concentrate su:
- rifinitura UI
- moduli reali nel `main`
- miglioramento gestione news/meteo/power
