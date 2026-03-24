#include "dashboard_ui_footer.h"
#include "dashboard_app.h"
#include "dashboard_support.h"
#include "dashboard_ui_shared.h"

static char lastNewsTickerText[NEWS_MAX_TICKER_LEN] = {};

void createDashboardFooter(lv_obj_t *parent) {
  lv_obj_t *footer = lv_obj_create(parent);
  lv_obj_set_size(footer, UI_FOOTER_WIDTH, UI_FOOTER_HEIGHT);
  lv_obj_set_style_pad_hor(footer, 10, 0);
  lv_obj_set_style_pad_ver(footer, 5, 0);
  lv_obj_set_scrollbar_mode(footer, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(footer, LV_OBJ_FLAG_SCROLLABLE);
  styleDashboardPanel(footer, UI_COLOR_HEADER_BG, UI_COLOR_HEADER_BORDER);

  ui.newsLabel = lv_label_create(footer);
  lv_obj_set_width(ui.newsLabel, UI_FOOTER_WIDTH - 20);
  lv_obj_set_style_text_align(ui.newsLabel, LV_TEXT_ALIGN_LEFT, LV_PART_MAIN);
  lv_label_set_long_mode(ui.newsLabel, LV_LABEL_LONG_SCROLL_CIRCULAR);
  lv_obj_set_scrollbar_mode(ui.newsLabel, LV_SCROLLBAR_MODE_OFF);
  lv_obj_clear_flag(ui.newsLabel, LV_OBJ_FLAG_SCROLLABLE);
  char footerText[NEWS_MAX_TICKER_LEN];
  buildNewsFooterText(footerText, sizeof(footerText));
  lv_label_set_text(ui.newsLabel, footerText);
  setDashboardLabelFont(ui.newsLabel, &lv_font_montserrat_16);
  setDashboardLabelColor(ui.newsLabel, UI_COLOR_TEXT_NEWS);
  lv_obj_align(ui.newsLabel, LV_ALIGN_LEFT_MID, 0, 0);
}

void refreshDashboardFooterUi() {
  if (ui.newsLabel == nullptr) {
    return;
  }

  char footerText[NEWS_MAX_TICKER_LEN];
  buildNewsFooterText(footerText, sizeof(footerText));
  if (strcmp(lastNewsTickerText, footerText) == 0) {
    return;
  }

  setDashboardLabelTextIfChanged(ui.newsLabel, footerText);
  strlcpy(lastNewsTickerText, footerText, sizeof(lastNewsTickerText));
}
