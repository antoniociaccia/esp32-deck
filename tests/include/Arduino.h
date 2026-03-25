#ifndef TESTS_FAKE_ARDUINO_H
#define TESTS_FAKE_ARDUINO_H

#include <algorithm>
#include <cctype>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>
#include <ctime>

using uint8_t = std::uint8_t;
using uint16_t = std::uint16_t;
using uint32_t = std::uint32_t;

extern unsigned long mockMillis;
inline unsigned long millis() {
  return mockMillis;
}

inline bool isDigit(char c) {
  return std::isdigit(static_cast<unsigned char>(c)) != 0;
}

inline std::size_t strlcpy(char *dst, const char *src, std::size_t size) {
  std::size_t srcLen = std::strlen(src);
  if (size > 0) {
    std::size_t copyLen = std::min(srcLen, size - 1);
    std::memcpy(dst, src, copyLen);
    dst[copyLen] = '\0';
  }
  return srcLen;
}

inline std::size_t strlcat(char *dst, const char *src, std::size_t size) {
  std::size_t dstLen = std::strlen(dst);
  std::size_t srcLen = std::strlen(src);
  if (dstLen >= size) {
    return size + srcLen;
  }
  std::size_t copyLen = std::min(srcLen, size - dstLen - 1);
  std::memcpy(dst + dstLen, src, copyLen);
  dst[dstLen + copyLen] = '\0';
  return dstLen + srcLen;
}

class String {
public:
  String() = default;
  String(const char *s) : value_(s == nullptr ? "" : s) {}
  String(const std::string &s) : value_(s) {}
  String(int val) : value_(std::to_string(val)) {}
  String(uint32_t val) : value_(std::to_string(val)) {}
  String(double val, int decimalPlaces = 2) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", decimalPlaces, val);
    value_ = buf;
  }

  int length() const { return static_cast<int>(value_.size()); }
  void reserve(std::size_t size) { value_.reserve(size); }
  const char *c_str() const { return value_.c_str(); }

  int indexOf(const char *needle, int fromIndex = 0) const {
    std::size_t pos = value_.find(needle == nullptr ? "" : needle, static_cast<std::size_t>(std::max(fromIndex, 0)));
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }

  int indexOf(char needle, int fromIndex = 0) const {
    std::size_t pos = value_.find(needle, static_cast<std::size_t>(std::max(fromIndex, 0)));
    return pos == std::string::npos ? -1 : static_cast<int>(pos);
  }

  String substring(int start, int end) const {
    int clampedStart = std::max(start, 0);
    int clampedEnd = std::max(end, clampedStart);
    if (clampedStart >= length()) {
      return String("");
    }
    clampedEnd = std::min(clampedEnd, length());
    return String(value_.substr(static_cast<std::size_t>(clampedStart), static_cast<std::size_t>(clampedEnd - clampedStart)));
  }

  int toInt() const {
    try {
      return std::stoi(value_);
    } catch (...) {
      return 0;
    }
  }

  float toFloat() const {
    try {
      return std::stof(value_);
    } catch (...) {
      return 0.0f;
    }
  }

  void replace(const char *from, const char *to) {
    std::string needle = from == nullptr ? "" : from;
    std::string replacement = to == nullptr ? "" : to;
    if (needle.empty()) {
      return;
    }

    std::size_t pos = 0;
    while ((pos = value_.find(needle, pos)) != std::string::npos) {
      value_.replace(pos, needle.size(), replacement);
      pos += replacement.size();
    }
  }

  void trim() {
    auto isSpace = [](unsigned char c) { return std::isspace(c) != 0; };
    auto begin = std::find_if_not(value_.begin(), value_.end(), isSpace);
    auto end = std::find_if_not(value_.rbegin(), value_.rend(), isSpace).base();
    if (begin >= end) {
      value_.clear();
      return;
    }
    value_ = std::string(begin, end);
  }

  char operator[](int index) const {
    return value_[static_cast<std::size_t>(index)];
  }

  String &operator+=(char c) {
    value_ += c;
    return *this;
  }

  String &operator+=(const char *s) {
    value_ += (s == nullptr ? "" : s);
    return *this;
  }

  String &operator+=(const String &rhs) {
    value_ += rhs.value_;
    return *this;
  }

private:
  std::string value_;
};
struct SerialStub {
  void println(const char * /*msg*/) {}
  void printf(const char * /*fmt*/, ...) {}
};
extern SerialStub Serial;

extern bool mockGetLocalTimeResult;
extern struct tm mockTimeinfoValue;

inline bool getLocalTime(struct tm *info, uint32_t /*ms*/ = 5000) {
  if (info) *info = mockTimeinfoValue;
  return mockGetLocalTimeResult;
}

inline void configTzTime(const char* /*tz*/, const char* /*server1*/, const char* /*server2*/) {}

inline int analogReadMilliVolts(int /*pin*/) { return 0; }
inline void delay(unsigned long /*ms*/) {}
inline void analogSetPinAttenuation(int /*pin*/, int /*atten*/) {}
static constexpr int ADC_11db = 3;

#endif
