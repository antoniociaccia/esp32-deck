#ifndef DASHBOARD_SERVICES_COMMON_H
#define DASHBOARD_SERVICES_COMMON_H

#include <HTTPClient.h>
#include <WiFiClientSecure.h>

void prepareSecureHttpClient(HTTPClient &http, WiFiClientSecure &client);

#endif
