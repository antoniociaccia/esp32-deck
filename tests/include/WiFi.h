#ifndef TESTS_FAKE_WIFI_H
#define TESTS_FAKE_WIFI_H

#define WL_CONNECTED 3
#define WL_DISCONNECTED 6
#define WIFI_STA 1

class WiFiClass {
public:
  int mockStatus = WL_CONNECTED;
  int status() { return mockStatus; }
  void mode(int) {}
  void begin(const char*, const char*) {}
};

extern WiFiClass WiFi;

#endif
