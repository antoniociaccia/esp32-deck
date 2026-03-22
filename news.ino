/*
  ==========================================
  ESP32 + ST7789 (170x320) + BATTERIA DISPLAY
  ==========================================

  Configurazione:
  - Display SPI ST7789 (1.9", 170x320)
  - ESP32 LOLIN D32 Pro
  - Indicatore batteria in alto a destra

  Collegamenti display:
  --------------------------------
  Display   -> ESP32
  BLK       -> GPIO32 (retroilluminazione)
  CS        -> GPIO14 (chip select)
  DC        -> GPIO27 (data/command)
  RES       -> GPIO33 (reset)
  SDA       -> GPIO23 (MOSI - dati)
  SCL       -> GPIO18 (SCK - clock)
  VCC       -> 3V3
  GND       -> GND

  Lettura batteria:
  --------------------------------
  BAT_PIN   -> GPIO35

  NOTE IMPORTANTI:
  - SDA/SCL qui NON sono I2C -> sono SPI
  - ESP32 usa SPI hardware su GPIO18 e GPIO23
  - La batteria LiPo 3.7V va collegata al connettore JST della board
  - GPIO35 viene usato per leggere la tensione batteria
*/

#include <Arduino_GFX_Library.h>

// ================================
// DEFINIZIONE PIN DISPLAY
// ================================
#define TFT_CS 14
#define TFT_DC 27
#define TFT_RST 33
#define TFT_SCK 18
#define TFT_MOSI 23
#define TFT_BL 32

// ================================
// DEFINIZIONE PIN BATTERIA
// ================================
#define BAT_PIN 35

// ================================
// CONFIGURAZIONE LETTURA BATTERIA
// ================================
/*
  ADC_12db permette all'ESP32 di leggere tensioni
  più alte sul pin ADC rispetto all'impostazione di default.

  BATTERY_DIVIDER_RATIO:
  - se la tensione batteria viene dimezzata dal partitore, vale 2.0
  - se in futuro misuri col tester una differenza, puoi correggere qui

  Esempio:
  se il display mostra 3.95V ma il tester legge 4.05V,
  puoi aumentare leggermente BATTERY_CALIBRATION_FACTOR.
*/
static const adc_attenuation_t BATTERY_ADC_ATTENUATION = ADC_12db;
static constexpr float BATTERY_DIVIDER_RATIO = 2.0f;
static constexpr float BATTERY_CALIBRATION_FACTOR = 1.00f;
static constexpr int BATTERY_SAMPLE_COUNT = 12;

// ================================
// COLORI IN FORMATO RGB565
// ================================
#define COLOR_BLACK 0x0000
#define COLOR_WHITE 0xFFFF
#define COLOR_RED 0xF800
#define COLOR_GREEN 0x07E0
#define COLOR_BLUE 0x001F
#define COLOR_YELLOW 0xFFE0
#define COLOR_GRAY 0x8410

/*
  ================================
  CONFIGURAZIONE BUS SPI
  ================================

  Arduino_ESP32SPI:
  - gestisce la comunicazione SPI tra ESP32 e display

  Parametri:
  DC   = pin comando/dati
  CS   = selezione display
  SCK  = clock SPI
  MOSI = dati in uscita
  MISO = non usato (display non legge dati)
  VSPI = bus SPI hardware
  20MHz = velocità (abbassata per stabilità)
*/

Arduino_DataBus *bus = new Arduino_ESP32SPI(
  TFT_DC,
  TFT_CS,
  TFT_SCK,
  TFT_MOSI,
  GFX_NOT_DEFINED,
  VSPI,
  20000000);

/*
  ================================
  CONFIGURAZIONE DISPLAY
  ================================

  Arduino_ST7789:
  - driver per il controller ST7789

  Parametri:
  bus       = SPI configurato sopra
  TFT_RST   = pin reset
  rotation  = orientamento (0,1,2,3)
  true      = IPS (migliora colori)
  170,320   = risoluzione fisica
  offset    = necessario per allineamento
*/

Arduino_GFX *gfx = new Arduino_ST7789(
  bus,
  TFT_RST,
  1,  // rotazione (1 = orizzontale)
  true,
  170, 320,
  35, 0,  // offset (dipende dal display)
  35, 0);

/*
  ================================
  FUNZIONE: LEGGI TENSIONE BATTERIA
  ================================

  Qui usiamo analogReadMilliVolts(), che su ESP32
  è in genere più precisa rispetto alla formula manuale.

  Passi:
  1. leggiamo più campioni
  2. facciamo la media
  3. convertiamo da millivolt a volt
  4. ricostruiamo la tensione reale batteria
     usando il rapporto del partitore
  5. applichiamo un eventuale fattore di calibrazione
*/
float readBatteryVoltage() {
  /*
    Invece di leggere una sola volta, facciamo una media
    di più campioni. Questo riduce un po' il rumore
    dell'ADC dell'ESP32.
  */
  uint32_t totalMilliVolts = 0;

  for (int i = 0; i < BATTERY_SAMPLE_COUNT; i++) {
    totalMilliVolts += analogReadMilliVolts(BAT_PIN);
    delay(2);
  }

  float averageMilliVolts = totalMilliVolts / (float)BATTERY_SAMPLE_COUNT;

  /*
    Per trasformare il valore letto in tensione reale batteria:
    1. convertiamo da millivolt a volt
    2. moltiplichiamo per il rapporto del partitore
    3. applichiamo un eventuale fattore di calibrazione finale
  */
  float pinVoltage = averageMilliVolts / 1000.0f;
  float batteryVoltage = pinVoltage * BATTERY_DIVIDER_RATIO * BATTERY_CALIBRATION_FACTOR;

  /*
    Se la media è praticamente zero, il pin è molto probabilmente
    flottante o non sta leggendo nulla di utile.
  */
  if (averageMilliVolts < 10.0f) {
    return 0.0f;
  }

  return batteryVoltage;
}

/*
  ======================================
  FUNZIONE: CONVERTI TENSIONE IN PERCENTO
  ======================================

  Valori tipici batteria LiPo 1 cella:
  - 4.2V = carica piena
  - 3.7V = circa metà
  - 3.0V = quasi scarica

  ATTENZIONE:
  questa conversione è semplice e lineare.
  Nella realtà la curva di scarica delle LiPo
  non è perfettamente lineare.
*/
int batteryPercent(float voltage) {
  int percent = (int)((voltage - 3.0) / (4.2 - 3.0) * 100.0);
  percent = constrain(percent, 0, 100);
  return percent;
}

/*
  ================================
  FUNZIONE: DISEGNA ICONA BATTERIA
  ================================

  x, y     = posizione dell'angolo alto sinistro
  percent  = percentuale batteria (0-100)

  Disegna:
  - contorno batteria
  - polo positivo a destra
  - riempimento interno colorato
*/
void drawBatteryIcon(int x, int y, int percent) {
  int bodyWidth = 26;
  int bodyHeight = 12;
  int terminalWidth = 3;
  int terminalHeight = 6;

  // Colore riempimento in base alla percentuale
  uint16_t fillColor = COLOR_GREEN;

  if (percent < 20) {
    fillColor = COLOR_RED;
  } else if (percent < 50) {
    fillColor = COLOR_YELLOW;
  }

  // Contorno batteria
  gfx->drawRect(x, y, bodyWidth, bodyHeight, COLOR_WHITE);

  // Polo positivo
  gfx->fillRect(x + bodyWidth, y + 3, terminalWidth, terminalHeight, COLOR_WHITE);

  // Area interna
  int innerWidth = bodyWidth - 4;
  int fillWidth = (innerWidth * percent) / 100;

  // Sfondo interno
  gfx->fillRect(x + 2, y + 2, innerWidth, bodyHeight - 4, COLOR_BLACK);

  // Riempimento livello batteria
  if (fillWidth > 0) {
    gfx->fillRect(x + 2, y + 2, fillWidth, bodyHeight - 4, fillColor);
  }
}

bool batteryConnected(float voltage) {
  /*
    Sotto questa soglia consideriamo assente la batteria.
    Il valore è volutamente prudente per evitare falsi positivi.
  */
  return voltage > 2.8f;
}

/*
  ===================================
  FUNZIONE: DISEGNA INDICATORE BATTERIA
  ===================================

  Disegna in alto a destra:
  - icona batteria
  - percentuale
  - tensione in Volt

  Usiamo width() per adattarci automaticamente
  alla rotazione attuale del display.
*/
void drawBatteryStatus() {
  int w = gfx->width();

  float voltage = readBatteryVoltage();
  bool connected = batteryConnected(voltage);

  // Cancella l'area in alto a destra prima di ridisegnare
  gfx->fillRect(w - 95, 0, 95, 30, COLOR_BLACK);

  gfx->setTextColor(COLOR_WHITE);
  gfx->setTextSize(1);

  // 🔴 Se batteria NON collegata
  if (!connected) {
    gfx->setCursor(w - 90, 10);
    gfx->println("NO BAT");
    return;
  }

  // ✅ CALCOLO percentuale (MANCAVA QUESTO)
  int percent = batteryPercent(voltage);

  // Disegna icona batteria
  drawBatteryIcon(w - 60, 5, percent);

  // Disegna testo percentuale
  gfx->setCursor(w - 92, 9);
  gfx->printf("%3d%%", percent);

  // Disegna tensione
  gfx->setCursor(w - 92, 20);
  gfx->printf("%.2fV", voltage);
}

void setup() {

  // ================================
  // RETROILLUMINAZIONE
  // ================================
  pinMode(TFT_BL, OUTPUT);
  digitalWrite(TFT_BL, HIGH);  // accende il display

  // ================================
  // INIZIALIZZAZIONE ADC BATTERIA
  // ================================
  analogReadResolution(12);
  analogSetPinAttenuation(BAT_PIN, BATTERY_ADC_ATTENUATION);

  // ================================
  // INIZIALIZZAZIONE DISPLAY
  // ================================
  gfx->begin();
  gfx->fillScreen(COLOR_BLACK);

  // ================================
  // DIMENSIONI DINAMICHE
  // ================================
  // IMPORTANTE: con rotation cambiano!
  int w = gfx->width();   // larghezza attuale
  int h = gfx->height();  // altezza attuale

  /*
    Con rotation = 1:
    w = 320
    h = 170
  */

  // ================================
  // DISEGNO ELEMENTI DI TEST
  // ================================

  // Bordo display
  // gfx->drawRect(0, 0, w, h, COLOR_RED);

  // Diagonale ↘
  // gfx->drawLine(0, 0, w - 1, h - 1, COLOR_GREEN);

  // Diagonale ↙
  // gfx->drawLine(w - 1, 0, 0, h - 1, COLOR_BLUE);

  // ================================
  // TESTO
  // ================================
  // gfx->setTextColor(COLOR_WHITE);
  // gfx->setTextSize(2);

  // Coordinate (x, y)
  // // gfx->setCursor(10, 20);
  // gfx->println("TEST DISPLAY");

  /*
    Sistema coordinate:
    (0,0) = in alto a sinistra

    x -> aumenta verso destra
    y -> aumenta verso il basso
  */

  // ================================
  // DISEGNO INIZIALE BATTERIA
  // ================================
  drawBatteryStatus();
}

void loop() {
  /*
    Aggiorniamo solo l'indicatore batteria.
    Non ridisegniamo tutto il display ogni volta,
    così evitiamo sfarfallii inutili.
  */
  drawBatteryStatus();
  delay(1000);
}
