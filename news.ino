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

void setup() {
  initSerialDebug();
  initBacklight();
  initBatteryAdc();
  initWifiTime();
  initDisplay();
  drawInitialScreen();
}

void loop() {
  /*
    Aggiorniamo orologio e data in alto a sinistra.
    Il tempo continua ad avanzare localmente dopo la sincronizzazione NTP.
  */
  drawDateTime();

  /*
    Aggiorniamo solo l'indicatore batteria.
    Non ridisegniamo tutto il display ogni volta,
    così evitiamo sfarfallii inutili.
  */
  drawBatteryStatus();
  printDebugStatus();
  delay(1000);
}
