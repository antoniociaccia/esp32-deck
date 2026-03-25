#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include "doctest.h"
#include "Arduino.h"
#include "WiFi.h"
#include "HTTPClient.h"

WiFiClass WiFi;
int HTTPClient::mockReturnCode = 200;
String HTTPClient::mockResponseBody = "";
bool HTTPClient::mockBeginResult = true;
unsigned long mockMillis = 0;
bool mockGetLocalTimeResult = true;
struct tm mockTimeinfoValue = {};
