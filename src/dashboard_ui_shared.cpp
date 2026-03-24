#include "dashboard_ui_shared.h"
#include "dashboard_support.h"
#include "config_ui.h"

void setDashboardLabelFont(lv_obj_t *label, const lv_font_t *font) {
  lv_obj_set_style_text_font(label, font, LV_PART_MAIN);
}

void setDashboardLabelColor(lv_obj_t *label, uint32_t colorHex) {
  if (label == nullptr) {
    return;
  }

  lv_obj_set_style_text_color(label, colorFromHex(colorHex), LV_PART_MAIN);
}

bool setDashboardLabelTextIfChanged(lv_obj_t *label, const char *text) {
  if (label == nullptr || text == nullptr) {
    return false;
  }

  const char *currentText = lv_label_get_text(label);
  if (currentText != nullptr && strcmp(currentText, text) == 0) {
    return false;
  }

  lv_label_set_text(label, text);
  return true;
}

bool setDashboardImageSourceIfChanged(lv_obj_t *image, const void *src) {
  if (image == nullptr || src == nullptr) {
    return false;
  }

  if (lv_img_get_src(image) == src) {
    return false;
  }

  lv_img_set_src(image, src);
  return true;
}

void styleDashboardPanel(lv_obj_t *panel, uint32_t bgColorHex, uint32_t borderColorHex) {
  lv_obj_set_style_radius(panel, UI_PANEL_RADIUS, 0);
  lv_obj_set_style_border_width(panel, 1, 0);
  lv_obj_set_style_border_color(panel, colorFromHex(borderColorHex), 0);
  lv_obj_set_style_bg_color(panel, colorFromHex(bgColorHex), 0);
  lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
  lv_obj_set_style_shadow_width(panel, 0, 0);
}

void createDashboardSpacer(lv_obj_t *parent, lv_coord_t height) {
  lv_obj_t *spacer = lv_obj_create(parent);
  lv_obj_set_size(spacer, 1, height);
  lv_obj_set_style_bg_opa(spacer, LV_OPA_TRANSP, 0);
  lv_obj_set_style_border_width(spacer, 0, 0);
  lv_obj_clear_flag(spacer, LV_OBJ_FLAG_SCROLLABLE);
}
