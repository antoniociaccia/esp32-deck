# Architettura del progetto

Questa pagina spiega come e organizzato `esp32-deck` e come i moduli collaborano durante il ciclo di vita dell'applicazione.

## Obiettivo

Il firmware implementa una dashboard embedded per `ESP32-S3` con queste responsabilita principali:

- inizializzare display, touch e runtime grafico
- mantenere uno stato applicativo condiviso
- acquisire dati locali e remoti
- aggiornare la UI senza bloccare il loop principale piu del necessario
- preparare il terreno per aggiornamenti OTA sicuri

## Flusso di avvio

L'avvio parte da [`news.ino`](/Users/antoniociaccia/Documents/Arduino/news/news.ino):

1. inizializzazione seriale
2. controllo di eventuale safe boot recovery
3. chiamata a `initializeDashboard(screen)`
4. esecuzione continua di `runDashboardLoop(screen)`

Questo rende `news.ino` volutamente sottile: la logica reale e spostata sotto `src/`.

## Blocchi principali

### Runtime

File chiave:

- [`src/dashboard_runtime.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_runtime.cpp)
- [`src/dashboard_runtime.h`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_runtime.h)

Responsabilita:

- coordinare il loop applicativo
- scandire refresh servizi e aggiornamenti UI
- gestire modalita safe/recovery quando abilitate

### Stato applicativo

File chiave:

- [`src/dashboard_app.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_app.cpp)
- [`src/dashboard_app.h`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_app.h)

Responsabilita:

- contenere lo stato globale `app`
- esporre utility condivise
- implementare parsing e normalizzazione di payload testuali e JSON

Qui si concentrano anche funzioni trasversali come:

- controllo intervalli temporali
- parsing del payload meteo
- parsing del feed news
- normalizzazione del testo mostrato in UI

### UI

File chiave:

- [`src/dashboard_ui.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_ui.cpp)
- [`src/dashboard_ui_main.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_ui_main.cpp)
- [`src/dashboard_ui_header.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_ui_header.cpp)
- [`src/dashboard_ui_footer.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_ui_footer.cpp)
- [`src/dashboard_ui_settings.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_ui_settings.cpp)
- [`src/dashboard_ui_ota_popup.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_ui_ota_popup.cpp)

Responsabilita:

- creare la gerarchia di widget `LVGL`
- aggiornare selettivamente header, contenuto principale e footer
- esporre controlli utente per impostazioni e OTA

La UI usa una logica a sezioni sporche (`dirty flags`) per evitare refresh completi non necessari.

### Servizi remoti

File chiave:

- [`src/dashboard_services.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_services.cpp)
- [`src/dashboard_services_weather.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_services_weather.cpp)
- [`src/dashboard_services_news.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_services_news.cpp)
- [`src/dashboard_services_common.h`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_services_common.h)

Responsabilita:

- interrogare backend remoti via `HTTPS`
- gestire errori di rete, configurazione e payload
- salvare lo stato del fetch per l'header e la UI principale

Strategia comune:

- controlla se il servizio e abilitato
- rispetta un intervallo di refresh o retry
- usa test mode per simulazioni locali
- esegue il fetch solo con Wi-Fi disponibile
- aggiorna lo stato condiviso e marca le sezioni UI da ridisegnare

### Batteria ed energia

File chiave:

- [`src/dashboard_battery.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_battery.cpp)
- [`src/dashboard_energy.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_energy.cpp)

Responsabilita:

- leggere il livello batteria
- produrre dati utilizzabili da UI e policy OTA

### Display e touch

File chiave:

- [`src/display.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/display.cpp)
- [`src/display.h`](/Users/antoniociaccia/Documents/Arduino/news/src/display.h)
- [`lib/TFT_eSPI/User_Setup.h`](/Users/antoniociaccia/Documents/Arduino/news/lib/TFT_eSPI/User_Setup.h)
- [`lib/FT6336U_CTP_Controller/src/FT6336U.cpp`](/Users/antoniociaccia/Documents/Arduino/news/lib/FT6336U_CTP_Controller/src/FT6336U.cpp)

Responsabilita:

- inizializzare il pannello grafico
- collegare touch e display alla UI `LVGL`
- centralizzare il setup hardware necessario alla board supportata

### OTA

File chiave:

- [`src/dashboard_ota.cpp`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_ota.cpp)
- [`src/dashboard_ota.h`](/Users/antoniociaccia/Documents/Arduino/news/src/dashboard_ota.h)
- [`src/version.h`](/Users/antoniociaccia/Documents/Arduino/news/src/version.h)

Responsabilita:

- definire il contratto del manifest remoto
- confrontare versioni, board e canale
- impedire aggiornamenti non applicabili o rischiosi

L'OTA e progettato come flusso in due fasi:

1. verifica read-only del manifest
2. applicazione firmware da popup UI quando il device e in stato valido

## Flusso dati

In termini pratici, il ciclo tipico e questo:

1. il runtime controlla gli intervalli configurati
2. i servizi aggiornano `app.weather`, `app.news` e altri segmenti di stato
3. la UI legge lo stato condiviso
4. le sezioni marcate come dirty vengono ridisegnate
5. il loop ricomincia senza ricostruire tutta la schermata

Questo approccio riduce il lavoro grafico e rende piu semplice testare la logica senza dipendere completamente dall'hardware.

## Testabilita

I test locali si concentrano soprattutto sulla logica che vale la pena isolare:

- parsing dei payload
- helper OTA
- comportamento dei servizi
- utility indipendenti dall'hardware

Vedi [`tests/run-parser-tests.sh`](/Users/antoniociaccia/Documents/Arduino/news/tests/run-parser-tests.sh).

## Limiti e assunzioni

L'architettura corrente assume:

- una singola board target ben nota
- servizi remoti con contratti stabili
- configurazione locale tramite `secrets.h`
- test hardware manuali per validare UI e interazioni touch

Non e ancora una piattaforma multi-board o una libreria riusabile: e un firmware applicativo specializzato.
