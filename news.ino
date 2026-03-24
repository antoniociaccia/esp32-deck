#include "src/display.h"
#include "src/dashboard_app.h"
#include "src/dashboard_runtime.h"

Display screen;

void setup() {
  Serial.begin(115200);
  delay(250);
  Serial.println();
  Serial.println("[boot] serial ready");

  if (SAFE_BOOT_RECOVERY) {
    enterSafeBootRecovery(screen);
    return;
  }

  initializeDashboard(screen);
}

void loop() {
  if (SAFE_BOOT_RECOVERY) {
    runSafeModeLoop();
    return;
  }

  runDashboardLoop(screen);
}
