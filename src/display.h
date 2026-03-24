#ifndef __DISPLAY_H
#define __DISPLAY_H

#include "lvgl.h"

#ifndef I2C_SCL
#define I2C_SCL 15
#endif
#ifndef I2C_SDA
#define I2C_SDA 16
#endif
#ifndef INT_N_PIN
#define INT_N_PIN 17
#endif
#ifndef RST_N_PIN
#define RST_N_PIN 18
#endif

#define TFT_DIRECTION 1

class Display {
public:
  void init();
  void routine();
  bool isTouched() const;
  int touchX() const;
  int touchY() const;
};

#endif
