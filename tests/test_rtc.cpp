#include "components/triviapod/rtc_decode.h"
#include <cassert>
#include <iostream>
int main() {
  using namespace esphome::triviapod::rtc_probe;
  uint8_t raw[] = {0x59,0x58,0x23,0x04,0x29,0x02,0x24}; DateTime out{};
  assert(decode(raw,out) && out.year==2024 && out.month==2 && out.day==29 && out.seconds_of_day()==86339);
  raw[6]=0x23;assert(!decode(raw,out)); // 2023 is not a leap year.
  raw[4]=0x28;assert(decode(raw,out));
  raw[0]=0x6A;assert(!decode(raw,out)); // Invalid BCD.
  raw[0]=0x60;assert(!decode(raw,out)); // Valid BCD, invalid seconds.
  raw[0]=0;raw[3]=0x03;assert(!decode(raw,out)); // Weekday must be one hot.
  raw[3]=0x40;raw[5]=0;assert(!decode(raw,out));
  raw[5]=0x12;raw[4]=0x31;assert(decode(raw,out));
  assert(out.text()=="2023-12-31 23:58:00");
  std::cout << "PASS: RTC BCD, ranges, leap date, one-hot weekday and readout formatting\n";
}
