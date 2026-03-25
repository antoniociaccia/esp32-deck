#include "doctest.h"
#include <cstdint>
#include "dashboard_support.h"
#include "dashboard_services_common.h"

int lvTaskHandlerCalls = 0;
uint32_t lv_task_handler(void) {
  lvTaskHandlerCalls++;
  return 0;
}

int refreshUiCalls = 0;
void refreshDashboardUi() {
  refreshUiCalls++;
}

TEST_CASE("UI Commit and Pump") {
  app.uiDirtyMask = 0;
  lvTaskHandlerCalls = 0;
  refreshUiCalls = 0;

  WeatherState state;
  state.temperatureC = 20;

  ServiceSnapshot<WeatherState> snap(state, UI_DIRTY_MAIN_WEATHER);
  state.temperatureC = 25; // Simulate change

  snap.commitAndPumpUi("WeatherTest");

  CHECK((app.uiDirtyMask.load() & UI_DIRTY_MAIN_WEATHER) != 0);
  CHECK(refreshUiCalls == 1);
  CHECK(lvTaskHandlerCalls == 1);
}
