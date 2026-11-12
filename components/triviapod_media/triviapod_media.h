#pragma once
#include "esphome/core/component.h"
#include "esphome/core/automation.h"
#include "esphome/core/log.h"
#include "esphome/components/media_player/media_player.h"
#include "esphome/components/speaker/media_player/speaker_media_player.h"
#include <functional>

namespace esphome::triviapod_media {
// The native backend is internal. This is its only externally exposed control path.
class GuardedMediaPlayer : public Component, public media_player::MediaPlayer {
 public:
  void set_backend(speaker::SpeakerMediaPlayer *backend) { backend_ = backend; }
  void set_speaker(speaker::Speaker *speaker) { speaker_ = speaker; }
  void set_local_busy(std::function<bool()> fn) { local_busy_ = std::move(fn); }
  void set_amplifier(std::function<bool(bool)> fn) { amplifier_ = std::move(fn); }
  Trigger<bool> *get_busy_trigger() { return &busy_trigger_; }
  media_player::MediaPlayerTraits get_traits() override { return backend_->get_traits(); }
  bool is_muted() const override { return backend_->is_muted(); }
  bool busy() const { return reserved_; }
  void setup() override {
    backend_->add_on_state_callback([this](media_player::MediaPlayerState state) {
      if (active_(state)) seen_active_ = true;
      sync_state_();
    });
  }
  void loop() override {
    if (!reserved_) return;
    const uint32_t now = millis();
    if (active_(backend_->state)) seen_active_ = true;
    const bool idle = !active_(backend_->state) && speaker_->is_stopped();
    // Normally release only after observing backend activity followed by idle.
    if (idle && seen_active_) {
      release_();
    } else if (stop_sent_ && idle && now - stop_at_ >= 1000) {
      // A failed URL may never reach an active state. Drain it with STOP first.
      release_();
    } else if (!stop_sent_ && !seen_active_ && now - reserved_at_ >= 10000) {
      send_stop_();
      ESP_LOGW("media_check", "event=start_timeout_stop_requested");
    }
  }
 protected:
  static bool active_(media_player::MediaPlayerState state) {
    return state == media_player::MEDIA_PLAYER_STATE_PLAYING ||
           state == media_player::MEDIA_PLAYER_STATE_ANNOUNCING ||
           state == media_player::MEDIA_PLAYER_STATE_PAUSED;
  }
  void sync_state_() {
    state = backend_->state;
    volume = backend_->volume;
    publish_state();
  }
  void release_() {
    amplifier_(false);
    reserved_ = false;
    busy_trigger_.trigger(false);
    sync_state_();
    ESP_LOGI("media_check", "event=idle amp_off_requested");
  }
  void send_stop_() {
    backend_->make_call().set_command(media_player::MEDIA_PLAYER_COMMAND_STOP).perform();
    stop_sent_ = true;
    stop_at_ = millis();
  }
  void control(const media_player::MediaPlayerCall &call) override {
    if (!backend_->is_ready() || local_busy_()) {
      ESP_LOGI("media_check", "event=rejected reason=local_audio_or_not_ready");
      return;
    }
    if (call.get_media_url().has_value()) {
      // One URL at a time; no queued work may survive into a microphone session.
      if (reserved_ || !speaker_->is_stopped()) {
        ESP_LOGI("media_check", "event=rejected reason=media_busy");
        return;
      }
      if (!amplifier_(true)) {
        amplifier_(false);
        ESP_LOGE("media_check", "event=rejected reason=amp_enable_readback");
        return;
      }
      reserved_ = true;
      seen_active_ = false;
      stop_sent_ = false;
      reserved_at_ = millis();
      busy_trigger_.trigger(true);  // Synchronous: blocks local diagnostics before queuing.
      // Local diagnostics set their own DAC volume. Reapply the HA volume on every clip.
      backend_->make_call().set_volume(call.get_volume().value_or(backend_->volume)).perform();
      backend_->make_call().set_media_url(*call.get_media_url()).set_announcement(true).perform();
      ESP_LOGI("media_check", "event=accepted announcement=yes uptime_ms=%u", unsigned(millis()));
      return;
    }
    if (call.get_command().has_value() &&
        (*call.get_command() == media_player::MEDIA_PLAYER_COMMAND_STOP ||
         *call.get_command() == media_player::MEDIA_PLAYER_COMMAND_TURN_OFF)) {
      send_stop_();
      return;
    }
    // This backend is announcement-only. Do not expose playlist/pause/resume paths.
    if (call.get_volume().has_value())
      backend_->make_call().set_volume(*call.get_volume()).perform();
    else if (call.get_command().has_value()) {
      const auto cmd = *call.get_command();
      if (cmd == media_player::MEDIA_PLAYER_COMMAND_MUTE || cmd == media_player::MEDIA_PLAYER_COMMAND_UNMUTE ||
          cmd == media_player::MEDIA_PLAYER_COMMAND_VOLUME_UP || cmd == media_player::MEDIA_PLAYER_COMMAND_VOLUME_DOWN)
        backend_->make_call().set_command(cmd).perform();
    }
  }
  speaker::SpeakerMediaPlayer *backend_{nullptr};
  speaker::Speaker *speaker_{nullptr};
  std::function<bool()> local_busy_;
  std::function<bool(bool)> amplifier_;
  Trigger<bool> busy_trigger_;
  bool reserved_{false}, seen_active_{false}, stop_sent_{false};
  uint32_t reserved_at_{0}, stop_at_{0};
};
}
