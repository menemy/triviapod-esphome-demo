#pragma once
#include <functional>
#include <string>
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"

namespace esphome::triviapod {
class TriviaPod : public Component, public i2c::I2CDevice {
 public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::HARDWARE + 1; }
  optional<bool> power_button();
  float voltage(uint8_t reg);
  optional<bool> charging();
  void motor_pulse(float level);
  void set_before_motor_pulse(std::function<void()> fn) { before_motor_pulse_ = std::move(fn); }
  bool amplifier(bool on);
  bool arm_microphone();
  bool verify_audio_output();
  bool led(bool on);
  std::string rtc_readonly_status();
  bool log_imu_identity();
  void loop() override;
  bool ready() const { return ready_; }
  bool quiet_for_network() const { return ready_ && !motor_stop_pending_ && !amp_stop_pending_; }
 protected:
  bool read_reg_(i2c::I2CDevice &dev, uint8_t reg, uint8_t *data, size_t size);
  bool write_reg_(i2c::I2CDevice &dev, uint8_t reg, uint16_t value, size_t size);
  bool update_reg_(i2c::I2CDevice &dev, uint8_t reg, uint16_t mask, uint16_t value, size_t size);
  i2c::I2CDevice ioe_;
  i2c::I2CDevice touch_;
  bool ready_{false};
  bool motor_stop_pending_{false};
  bool amp_stop_pending_{false};
  uint32_t amp_retry_at_{0};
  uint32_t charging_read_at_{0};
  optional<bool> charging_cache_;
  uint32_t motor_deadline_{0};
  uint32_t motor_sequence_{0};
  bool rtc_last_valid_{false};
  uint32_t rtc_last_tod_{0};
  uint32_t rtc_last_poll_ms_{0};
  std::function<void()> before_motor_pulse_;
};
}
