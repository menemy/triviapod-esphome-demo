#pragma once
#include <cstdint>
#include <cstdio>
#include <string>

namespace esphome::triviapod::rtc_probe {
struct DateTime {
  unsigned second, minute, hour, weekday, day, month, year;
  unsigned seconds_of_day() const { return hour * 3600 + minute * 60 + second; }
  std::string text() const {
    char result[24];
    std::snprintf(result, sizeof(result), "%04u-%02u-%02u %02u:%02u:%02u", year, month, day, hour, minute, second);
    return result;
  }
};
inline bool bcd(uint8_t value, unsigned &out) {
  if ((value & 15) > 9 || (value >> 4) > 9) return false;
  out = (value >> 4) * 10 + (value & 15);
  return true;
}
inline bool decode(const uint8_t *raw, DateTime &out) {
  unsigned year;
  if (!bcd(raw[0] & 0x7F, out.second) || !bcd(raw[1] & 0x7F, out.minute) ||
      !bcd(raw[2] & 0x3F, out.hour) || !bcd(raw[4] & 0x3F, out.day) ||
      !bcd(raw[5] & 0x1F, out.month) || !bcd(raw[6], year)) return false;
  out.year = 2000 + year;
  out.weekday = raw[3] & 0x7F;
  if (out.second > 59 || out.minute > 59 || out.hour > 23 || out.month < 1 || out.month > 12 ||
      out.weekday == 0 || (out.weekday & (out.weekday - 1)) != 0) return false;
  static constexpr unsigned days[] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
  const bool leap = out.year % 4 == 0 && (out.year % 100 != 0 || out.year % 400 == 0);
  const unsigned limit = days[out.month - 1] + (out.month == 2 && leap ? 1 : 0);
  return out.day >= 1 && out.day <= limit;
}
}
