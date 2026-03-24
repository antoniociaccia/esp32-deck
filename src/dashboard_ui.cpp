#include "dashboard_ui.h"
#include "dashboard_support.h"
#include "config_ui.h"
#include "dashboard_ui_shared.h"
#include "dashboard_ui_header.h"
#include "dashboard_ui_main.h"
#include "dashboard_ui_footer.h"

void createDashboardUi() {
  lv_obj_t *screenRoot = lv_scr_act();
  lv_obj_set_style_bg_color(screenRoot, colorFromHex(UI_COLOR_SCREEN_BG), 0);
  lv_obj_set_style_bg_opa(screenRoot, LV_OPA_COVER, 0);
  lv_obj_set_style_pad_left(screenRoot, 3, 0);
  lv_obj_set_style_pad_right(screenRoot, 3, 0);
  lv_obj_set_style_pad_top(screenRoot, 3, 0);
  lv_obj_set_style_pad_bottom(screenRoot, 3, 0);
  lv_obj_set_style_pad_gap(screenRoot, 0, 0);
  lv_obj_set_layout(screenRoot, LV_LAYOUT_FLEX);
  lv_obj_set_flex_flow(screenRoot, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(screenRoot, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

  createDashboardHeader(screenRoot);
  createDashboardSpacer(screenRoot, 6);
  createDashboardMain(screenRoot);
  createDashboardFooter(screenRoot);
  refreshDashboardUi();
}

void refreshDashboardUi() {
  refreshDashboardHeaderUi();
  refreshDashboardMainUi();
  refreshDashboardFooterUi();
}
