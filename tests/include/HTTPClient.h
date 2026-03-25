#ifndef TESTS_FAKE_HTTPCLIENT_H
#define TESTS_FAKE_HTTPCLIENT_H

#include "Arduino.h"
#include "WiFiClientSecure.h"

#define HTTP_CODE_OK 200
#define HTTPC_STRICT_FOLLOW_REDIRECTS 1

class HTTPClient {
public:
  static int mockReturnCode;
  static String mockResponseBody;
  static bool mockBeginResult;

  void setTimeout(int ms) { (void)ms; }
  void setReuse(bool reuse) { (void)reuse; }
  void setFollowRedirects(int follow) { (void)follow; }

  bool begin(WiFiClientSecure &client, const String &url) {
    (void)client;
    (void)url;
    return mockBeginResult;
  }
  
  bool begin(WiFiClientSecure &client, const char* url) {
    (void)client;
    (void)url;
    return mockBeginResult;
  }

  void addHeader(const char *name, const char *value) {
    (void)name;
    (void)value;
  }

  int GET() {
    return mockReturnCode;
  }

  int getSize() {
    return mockResponseBody.length();
  }

  String getString() {
    return mockResponseBody;
  }

  void end() {}
};

#endif
