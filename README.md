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

- [`news.ino`](/Users/antoniociaccia/Documents/Arduino/news/news.ino): logica principale dashboard, UI, Wi-Fi, NTP, batteria, meteo, news
- [`display.h`](/Users/antoniociaccia/Documents/Arduino/news/display.h): interfaccia display/touch
- [`display.cpp`](/Users/antoniociaccia/Documents/Arduino/news/display.cpp): bridge `LVGL + TFT_eSPI + touch`
- [`lv_conf.h`](/Users/antoniociaccia/Documents/Arduino/news/lv_conf.h): configurazione locale `LVGL`
- [`weather_icons.h`](/Users/antoniociaccia/Documents/Arduino/news/weather_icons.h): dichiarazioni asset meteo
- [`weather_icons.c`](/Users/antoniociaccia/Documents/Arduino/news/weather_icons.c): inclusione asset meteo generati da `LVGL Image Converter`
- [`partitions.csv`](/Users/antoniociaccia/Documents/Arduino/news/partitions.csv): layout flash single-app senza dual OTA
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
- `./flash-monitor.sh`

Note su `flash.sh`:
- usa cache incrementale
- pulisce la build solo con `--clean`
- mostra barra percentuale per flash e RAM
- forza `upload.maximum_size` coerente con la partizione app reale
- forza `build.cdc_on_boot=1` per tenere disponibile la seriale USB runtime su ESP32-S3

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
