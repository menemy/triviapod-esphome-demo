// SPDX-License-Identifier: Apache-2.0
#include "triviapod.h"
#include "rtc_decode.h"
#include "esphome/core/hal.h"
#include "esphome/core/log.h"
#include <cstring>
#include <cmath>

namespace esphome::triviapod {
static const char *const TAG = "triviapod";
// M5IOE1 IO1 maps to bit 0. IO1 (MUX select) must be preserved.
static constexpr uint16_t OUTPUTS = 0x039C;
static constexpr uint16_t RAILS_RESETS = 0x009C;

bool TriviaPod::read_reg_(i2c::I2CDevice &dev, uint8_t reg, uint8_t *data, size_t size) {
  for (int attempt = 0; attempt < 3; ++attempt) {
    if (dev.read_register(reg, data, size) == i2c::ERROR_OK) return true;
    if (attempt < 2) delay(10);
  }
  ESP_LOGE(TAG, "Read failed address=0x%02X reg=0x%02X", dev.get_i2c_address(), reg);
  return false;
}

bool TriviaPod::write_reg_(i2c::I2CDevice &dev, uint8_t reg, uint16_t value, size_t size) {
  const uint8_t data[] = {uint8_t(value), uint8_t(value >> 8)};
  bool written = false;
  for (int attempt = 0; attempt < 3; ++attempt) {
    if (dev.write_register(reg, data, size) == i2c::ERROR_OK) { written = true; break; }
    if (attempt < 2) delay(10);
  }
  uint8_t actual[2]{};
  if (!written || !read_reg_(dev, reg, actual, size) || std::memcmp(data, actual, size) != 0) {
    ESP_LOGE(TAG, "Write/readback failed address=0x%02X reg=0x%02X", dev.get_i2c_address(), reg);
    return false;
  }
  return true;
}

bool TriviaPod::update_reg_(i2c::I2CDevice &dev, uint8_t reg, uint16_t mask, uint16_t value, size_t size) {
  uint8_t data[2]{};
  if (!read_reg_(dev, reg, data, size)) return false;
  const uint16_t old = data[0] | (uint16_t(data[1]) << 8);
  return write_reg_(dev, reg, (old & ~mask) | (value & mask), size);
}

void TriviaPod::setup() {
  for (uint8_t address : {uint8_t(0x6E), uint8_t(0x4F)}) {
    if (bus_->write_readv(address, nullptr, 0, nullptr, 0) != i2c::ERROR_OK) {
      ESP_LOGE(TAG, "TriviaPOD PMIC/expander not found at0x%02X; no rail/output setup", address);
      mark_failed(); return;
    }
  }
  ioe_.set_i2c_bus(bus_); ioe_.set_i2c_address(0x4F);
  touch_.set_i2c_bus(bus_); touch_.set_i2c_address(0x15);
  bool ok = write_reg_(*this, 0x09, 0, 1);
  ok = write_reg_(*this, 0x0A, 0, 1) && ok;
  ok = update_reg_(*this, 0x16, 0xF0, 0, 1) && ok;
  ok = update_reg_(*this, 0x13, 0x08, 0, 1) && ok;
  ok = update_reg_(*this, 0x11, 0x08, 0, 1) && ok;
  ok = update_reg_(*this, 0x10, 0x0C, 0x08, 1) && ok;
  // CHG_EN survives ESP32 resets. Preserve every other PWR_CFG bit.
  const bool charge_enabled = update_reg_(*this, 0x06, 0x01, 0x01, 1);
  if (charge_enabled) ESP_LOGI(TAG, "M5PM1 CHG_EN enabled: bit0-only RMW/readback passed");
  ok = charge_enabled && ok;
  ok = update_reg_(*this, 0x49, 0x19, 0x19, 1) && ok;
  ok = write_reg_(ioe_, 0x23, 0, 1) && ok;
  ok = write_reg_(ioe_, 0x1B, 0, 2) && ok;
  ok = update_reg_(ioe_, 0x13, OUTPUTS, 0, 2) && ok;
  ok = update_reg_(ioe_, 0x05, OUTPUTS, RAILS_RESETS, 2) && ok;
  ok = update_reg_(ioe_, 0x03, OUTPUTS, OUTPUTS, 2) && ok;
  if (!ok) { mark_failed(); return; }
  delay(20);
  if (!update_reg_(ioe_, 0x05, 0x18, 0, 2)) { mark_failed(); return; }
  delay(10);
  if (!update_reg_(ioe_, 0x05, 0x18, 0x18, 2)) { mark_failed(); return; }
  delay(120);
  if (!write_reg_(touch_, 0xFE, 1, 1)) ESP_LOGW(TAG, "Touch idle-sleep disable not confirmed");
  for (uint8_t addr : {0x15, 0x18, 0x32, 0x4F, 0x68, 0x6E}) {
    const auto result = bus_->write_readv(addr, nullptr, 0, nullptr, 0);
    ESP_LOGI(TAG, "Post-rail I2C address=0x%02X %s", addr, result == i2c::ERROR_OK ? "ACK" : "NO ACK");
  }
  ready_ = true;
  ESP_LOGI(TAG, "Power/expander setup readbacks passed; amp and motor off");
}
optional<bool> TriviaPod::power_button() {
  uint8_t value{};
  if (!ready_ || !read_reg_(*this, 0x48, &value, 1)) return {};
  return (value & 1) != 0;
}
float TriviaPod::voltage(uint8_t reg) {
  uint8_t value[2]{};
  if (!ready_ || (reg != 0x22 && reg != 0x24) || !read_reg_(*this, reg, value, 2)) return NAN;
  return (value[0] | (uint16_t(value[1]) << 8)) / 1000.0f;
}
optional<bool> TriviaPod::charging() {
  if (millis() - charging_read_at_ < 1000) return charging_cache_;
  charging_read_at_ = millis();
  const float vin = voltage(0x24);
  uint8_t value{};
  if (std::isnan(vin) || !read_reg_(*this, 0x12, &value, 1)) charging_cache_.reset();
  else charging_cache_ = vin > 4.0f && (value & 4) == 0;
  return charging_cache_;
}
void TriviaPod::motor_pulse(float level) {
  if (before_motor_pulse_) before_motor_pulse_();
  ++motor_sequence_;
  ESP_LOGI("motor_check", "event=request seq=%u requested_level=%.3f uptime_ms=%u", unsigned(motor_sequence_), level, unsigned(millis()));
  if (!ready_ || !std::isfinite(level)) {
    ESP_LOGW("motor_check", "event=rejected seq=%u ready=%s finite=%s", unsigned(motor_sequence_), ready_ ? "yes" : "no", std::isfinite(level) ? "yes" : "no");
    return;
  }
  level = clamp(level, 0.0f, 0.6f);
  // A failed start still requires an off attempt: the write may have reached the chip.
  motor_stop_pending_ = true;
  motor_deadline_ = millis();
  if (!write_reg_(ioe_, 0x1B, 0, 2)) return;
  if (!update_reg_(ioe_, 0x05, 0x100, 0, 2)) return;
  if (level <= 0) {
    motor_stop_pending_ = false;
    ESP_LOGI("motor_check", "Motor stop readback passed event=stop seq=%u uptime_ms=%u", unsigned(motor_sequence_), unsigned(millis()));
    return;
  }
  if (!write_reg_(ioe_, 0x25, 2000, 2)) return;
  if (!write_reg_(ioe_, 0x1B, 0x8000 | uint16_t(level * 4095), 2)) return;
  motor_deadline_ = millis() + 200;
  ESP_LOGI("motor_check", "event=start_readback_passed seq=%u level=%.3f duration_ms=200 uptime_ms=%u", unsigned(motor_sequence_), level, unsigned(millis()));
}
void TriviaPod::loop() {
  if (amp_stop_pending_ && int32_t(millis() - amp_retry_at_) >= 0) {
    amplifier(false);
  }
  if (motor_stop_pending_ && int32_t(millis() - motor_deadline_) >= 0) {
    const bool pwm_off = write_reg_(ioe_, 0x1B, 0, 2);
    const bool gpio_off = update_reg_(ioe_, 0x05, 0x100, 0, 2);
    if (pwm_off && gpio_off) {
      motor_stop_pending_ = false;
      ESP_LOGI("motor_check", "Motor stop readback passed event=stop seq=%u uptime_ms=%u", unsigned(motor_sequence_), unsigned(millis()));
    } else {
      motor_deadline_ = millis() + 50;
    }
  }
}
bool TriviaPod::amplifier(bool on) {
  if (!ready_) return false;
  const bool ok = update_reg_(ioe_, 0x05, 0x200, on ? 0x200 : 0, 2);
  amp_stop_pending_ = !ok || !on;
  if (!on && ok) amp_stop_pending_ = false;
  amp_retry_at_ = millis() + 100;
  return ok;
}
bool TriviaPod::verify_audio_output() {
  if (!ready_) return false;
  i2c::I2CDevice codec;
  codec.set_i2c_bus(bus_); codec.set_i2c_address(0x18);
  uint8_t system{}, dac_power{}, output{}, mute{}, volume{}, amp[2]{};
  if (!read_reg_(codec, 0x0D, &system, 1) || !read_reg_(codec, 0x12, &dac_power, 1) ||
      !read_reg_(codec, 0x13, &output, 1) || !read_reg_(codec, 0x31, &mute, 1) ||
      !read_reg_(codec, 0x32, &volume, 1) || !read_reg_(ioe_, 0x05, amp, 2)) {
    ESP_LOGE("tone_check", "event=output_register_read_failed");
    return false;
  }
  const bool amp_enabled = (amp[1] & 0x02) != 0;
  const bool ok = (mute & 0x60) == 0 && volume == 0xBF && amp_enabled;
  ESP_LOGI("tone_check", "event=output_registers system0d=0x%02X dac_power12=0x%02X output13=0x%02X mute31=0x%02X volume32=0x%02X amp_gpio10=%s ready=%s",
           system, dac_power, output, mute, volume, amp_enabled ? "on" : "off", ok ? "yes" : "no");
  return ok;
}
bool TriviaPod::arm_microphone() {
  if (!ready_) return false;
  i2c::I2CDevice codec;
  codec.set_i2c_bus(bus_); codec.set_i2c_address(0x18);
  return write_reg_(codec, 0x0D, 1, 1);
}
bool TriviaPod::led(bool on) {
  const bool ok = ready_ && update_reg_(*this, 0x06, 0x10, on ? 0x10 : 0, 1);
  ESP_LOGI("led_check", "requested=%s bit4_readback=%s uptime_ms=%u", on ? "on" : "off", ok ? "passed" : "failed", unsigned(millis()));
  return ok;
}
bool TriviaPod::log_imu_identity() {
  if (!ready_) return false;
  i2c::I2CDevice imu; imu.set_i2c_bus(bus_); imu.set_i2c_address(0x68);
  uint8_t identity{};
  if (!read_reg_(imu, 0x00, &identity, 1)) return false;
  ESP_LOGI("imu_check", "chip_id=0x%02X expected=0x24 match=%s", identity, identity == 0x24 ? "yes" : "no");
  return identity == 0x24;
}
std::string TriviaPod::rtc_readonly_status() {
  if (!ready_) return "RTC not ready";
  i2c::I2CDevice rtc; rtc.set_i2c_bus(bus_); rtc.set_i2c_address(0x32);
  uint8_t raw[7]{}, control[3]{};
  // Read-only compatibility probe: never clear flags, STOP, time or backup-control bits.
  if (!read_reg_(rtc, 0x10, raw, sizeof(raw)) || !read_reg_(rtc, 0x1D, control, sizeof(control))) {
    rtc_last_valid_ = false;
    ESP_LOGW("rtc_check", "event=read_failed address=0x32 writes=none");
    return "RTC read failed";
  }
  rtc_probe::DateTime date{};
  const bool valid = rtc_probe::decode(raw, date);
  const bool stopped = (control[1] & 0x40) != 0;
  const uint32_t now = millis();
  const uint32_t rtc_delta = valid && rtc_last_valid_ ? (date.seconds_of_day() + 86400 - rtc_last_tod_) % 86400 : 0;
  const uint32_t poll_delta = rtc_last_poll_ms_ ? now - rtc_last_poll_ms_ : 0;
  const std::string text = valid ? date.text() : "RTC invalid BCD/date";
  ESP_LOGI("rtc_check", "event=readonly time=%s fields_valid=%s stop=%s flag1d=0x%02X ctrl1e=0x%02X backup1f=0x%02X rtc_delta_s=%u poll_delta_ms=%u writes=none",
           text.c_str(), valid ? "yes" : "no", stopped ? "yes" : "no", control[0], control[1], control[2], unsigned(rtc_delta), unsigned(poll_delta));
  rtc_last_valid_ = valid;
  if (valid) rtc_last_tod_ = date.seconds_of_day();
  rtc_last_poll_ms_ = now;
  return valid && stopped ? text + " STOP" : text;
}
void TriviaPod::dump_config() { ESP_LOGCONFIG(TAG, "TriviaPOD power setup: %s", ready_ ? "ready" : "FAILED"); }
}
