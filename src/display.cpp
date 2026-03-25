#include "display.h"
#include "config_debug.h"
#include "TFT_eSPI.h"
#include "FT6336U.h"

static const uint16_t rawTouchWidth = 240;
static const uint16_t rawTouchHeight = 320;
static const uint16_t screenWidth = 320;
static const uint16_t screenHeight = 240;
static const int touchJitterThresholdPx = 4;
static const int touchSmoothingWeight = 3;

static lv_disp_draw_buf_t draw_buf;
static lv_color_t buf[screenWidth * 10];
static volatile bool touchPressed = false;
static volatile int lastTouchX = -1;
static volatile int lastTouchY = -1;

TFT_eSPI tft = TFT_eSPI(screenWidth, screenHeight);
FT6336U ft6336u(I2C_SDA, I2C_SCL, RST_N_PIN, INT_N_PIN);
FT6336U_TouchPointType tp;

static int stabilizeTouchAxis(int currentValue, int previousValue) {
  if (previousValue < 0) {
    return currentValue;
  }

  int delta = currentValue - previousValue;
  if (abs(delta) <= touchJitterThresholdPx) {
    return previousValue;
  }

  return (previousValue + (currentValue * touchSmoothingWeight)) / (touchSmoothingWeight + 1);
}

#if LV_USE_LOG != 0
void my_print(const char *buf) {
  DEBUG_LVGL_PRINTF("%s", buf);
  if (DEBUG_SHOULD_LOG(DEBUG_LEVEL_VERBOSE, DEBUG_LOG_LVGL)) {
    Serial.flush();
  }
}
#endif

void my_disp_flush(lv_disp_drv_t *disp, const lv_area_t *area, lv_color_t *color_p) {
  uint32_t w = (area->x2 - area->x1 + 1);
  uint32_t h = (area->y2 - area->y1 + 1);

  tft.startWrite();
  tft.setAddrWindow(area->x1, area->y1, w, h);
  tft.pushColors((uint16_t *)&color_p->full, w * h, true);
  tft.endWrite();

  lv_disp_flush_ready(disp);
}

void my_touchpad_read(lv_indev_drv_t *indev_driver, lv_indev_data_t *data) {
  (void)indev_driver;

  tp = ft6336u.scan();
  int touched = tp.touch_count;

  if (!touched) {
    touchPressed = false;
    data->state = LV_INDEV_STATE_REL;
    return;
  }

  int rawX = tp.tp[0].x;
  int rawY = tp.tp[0].y;
  int x = rawX;
  int y = rawY;

  // Freenove references:
  // - Sketch_13.1_LVGL uses raw portrait coordinates with TFT_DIRECTION 0
  // - Sketch_12.1_TFT_Touch_Draw_2.8_Inch uses this transform for setRotation(1):
  //   x = tp.tp[0].y;
  //   y = 240 - tp.tp[0].x;
  if (TFT_DIRECTION == 1) {
    x = rawY;
    y = screenHeight - rawX;
  } else if (TFT_DIRECTION == 2) {
    x = rawTouchWidth - rawX;
    y = rawTouchHeight - rawY;
  } else if (TFT_DIRECTION == 3) {
    x = rawTouchHeight - rawY;
    y = rawX;
  }

  if (x >= screenWidth) {
    x = screenWidth - 1;
  }
  if (y >= screenHeight) {
    y = screenHeight - 1;
  }

  if (x >= 0 && x < screenWidth && y >= 0 && y < screenHeight) {
    if (touchPressed) {
      x = stabilizeTouchAxis(x, lastTouchX);
      y = stabilizeTouchAxis(y, lastTouchY);
    }

    touchPressed = true;
    lastTouchX = x;
    lastTouchY = y;
    data->state = LV_INDEV_STATE_PR;
    data->point.x = x;
    data->point.y = y;
    return;
  }

  touchPressed = false;
  data->state = LV_INDEV_STATE_REL;
}

void Display::init(void) {
  ft6336u.begin();

#if LV_USE_LOG != 0
  lv_log_register_print_cb(my_print);
#endif

  lv_init();
  tft.begin();
  tft.setRotation(TFT_DIRECTION);
  lv_disp_draw_buf_init(&draw_buf, buf, NULL, screenWidth * 10);

  static lv_disp_drv_t disp_drv;
  lv_disp_drv_init(&disp_drv);
  disp_drv.hor_res = screenWidth;
  disp_drv.ver_res = screenHeight;
  disp_drv.flush_cb = my_disp_flush;
  disp_drv.draw_buf = &draw_buf;
  lv_disp_drv_register(&disp_drv);

  static lv_indev_drv_t indev_drv;
  lv_indev_drv_init(&indev_drv);
  indev_drv.type = LV_INDEV_TYPE_POINTER;
  indev_drv.read_cb = my_touchpad_read;
  lv_indev_drv_register(&indev_drv);
}

void Display::routine(void) {
  lv_task_handler();
}

void Display::setBacklight(bool on) {
#ifdef TFT_BL
  digitalWrite(TFT_BL, on ? TFT_BACKLIGHT_ON : !TFT_BACKLIGHT_ON);
#endif
}

bool Display::isTouched() const {
  return touchPressed;
}

int Display::touchX() const {
  return lastTouchX;
}

int Display::touchY() const {
  return lastTouchY;
}
