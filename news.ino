#include "src/display.h"
#include "src/dashboard_app.h"
#include "src/dashboard_runtime.h"
#include "src/config_debug.h"
#include "src/config_timing.h"

Display screen;

void setup() {
  Serial.begin(115200);
  delay(250);
  DEBUG_PRINT(DEBUG_LEVEL_INFO, DEBUG_LOG_BOOT, "");
  DEBUG_BOOT_PRINT("[boot] serial ready");

  if (DASHBOARD_SAFE_BOOT_RECOVERY) {
    enterSafeBootRecovery(screen);
    return;
  }

  initializeDashboard(screen);
}

void loop() {
  if (DASHBOARD_SAFE_BOOT_RECOVERY) {
    runSafeModeLoop();
    return;
  }

  runDashboardLoop(screen);
}
