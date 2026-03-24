#ifndef DASHBOARD_SERVICES_COMMON_H
#define DASHBOARD_SERVICES_COMMON_H

#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include "dashboard_app.h"

void prepareSecureHttpClient(HTTPClient &http, WiFiClientSecure &client);

// ---------------------------------------------------------------------------
// ServiceSnapshot — cattura e confronta lo stato di un servizio per
// decidere se serve log + markUiDirty.  Elimina il pattern ripetuto
// "salva precedente → muta → confronta → log → markDirty" presente in
// weather, news e ota.
//
// Uso:
//   ServiceSnapshot<WeatherState> snap(app.weather, UI_DIRTY_HEADER | UI_DIRTY_MAIN_WEATHER);
//   ... muta app.weather ...
//   snap.commitIfChanged("Weather");   // log + markDirty se diverso
// ---------------------------------------------------------------------------

template <typename T>
struct ServiceSnapshot {
  T previous;
  T &current;
  uint32_t dirtyMask;

  ServiceSnapshot(T &stateRef, uint32_t mask)
    : previous(stateRef), current(stateRef), dirtyMask(mask) {}

  bool changed() const {
    return memcmp(&previous, &current, sizeof(T)) != 0;
  }

  void markDirtyIfChanged() {
    if (changed()) {
      markUiDirty(dirtyMask);
    }
  }

  void commitIfChanged(const char *label) {
    if (!changed()) return;
    DEBUG_NETWORK_PRINTF("%s state=%s http=%d valid=%d\n",
      label,
      serviceFetchStateLabel(current.state),
      current.lastHttpCode,
      current.valid ? 1 : 0);
    markUiDirty(dirtyMask);
  }
};

#endif
