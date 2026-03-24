#ifndef TESTS_FAKE_LVGL_H
#define TESTS_FAKE_LVGL_H

#include <cstdint>

using lv_coord_t = int16_t;
using lv_obj_t = struct _lv_obj_t;

struct lv_color_t {
  uint32_t full;
};

inline lv_color_t lv_color_hex(uint32_t hex) {
  return lv_color_t{hex};
}

uint32_t lv_task_handler(void);

#endif
