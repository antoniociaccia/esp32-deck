#pragma once

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern const lv_img_dsc_t sun;
extern const lv_img_dsc_t cloud;
extern const lv_img_dsc_t rain;
extern const lv_img_dsc_t storm;
extern const lv_img_dsc_t snow;
extern const lv_img_dsc_t fog;

#ifdef __cplusplus
}
#endif

#define IMG_WEATHER_SUN sun
#define IMG_WEATHER_CLOUD cloud
#define IMG_WEATHER_RAIN rain
#define IMG_WEATHER_STORM storm
#define IMG_WEATHER_SNOW snow
#define IMG_WEATHER_FOG fog
