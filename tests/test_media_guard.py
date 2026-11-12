"""Host check of the real media guard header against a fake ESPHome backend."""
from pathlib import Path
import subprocess
import os
import shutil
import tempfile

project = Path(__file__).resolve().parents[1]
common = r'''
#pragma once
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <cstdint>
namespace esphome {
inline uint32_t now_ms = 0;
inline uint32_t millis() { return now_ms; }
class Component { public: virtual ~Component() = default; virtual void setup() {} virtual void loop() {} bool is_ready() const { return ready; } bool ready{true}; };
template<class T> class Trigger { public: std::function<void(T)> callback; void trigger(T value) { if (callback) callback(value); } };
namespace media_player {
enum MediaPlayerState { MEDIA_PLAYER_STATE_NONE, MEDIA_PLAYER_STATE_IDLE, MEDIA_PLAYER_STATE_PLAYING, MEDIA_PLAYER_STATE_PAUSED, MEDIA_PLAYER_STATE_ANNOUNCING, MEDIA_PLAYER_STATE_OFF };
enum MediaPlayerCommand { MEDIA_PLAYER_COMMAND_PLAY, MEDIA_PLAYER_COMMAND_STOP, MEDIA_PLAYER_COMMAND_TURN_OFF, MEDIA_PLAYER_COMMAND_MUTE, MEDIA_PLAYER_COMMAND_UNMUTE, MEDIA_PLAYER_COMMAND_VOLUME_UP, MEDIA_PLAYER_COMMAND_VOLUME_DOWN };
class MediaPlayerTraits {};
class MediaPlayer;
class MediaPlayerCall {
 public:
  explicit MediaPlayerCall(MediaPlayer *parent): parent(parent) {}
  MediaPlayerCall &set_command(MediaPlayerCommand x) { command=x; return *this; }
  MediaPlayerCall &set_media_url(const std::string &x) { url=x; return *this; }
  MediaPlayerCall &set_volume(float x) { volume=x; return *this; }
  MediaPlayerCall &set_announcement(bool x) { announcement=x; return *this; }
  const auto &get_command() const { return command; }
  const auto &get_media_url() const { return url; }
  const auto &get_volume() const { return volume; }
  void perform();
  MediaPlayer *parent;
  std::optional<MediaPlayerCommand> command;
  std::optional<std::string> url;
  std::optional<float> volume;
  bool announcement{false};
};
class MediaPlayer {
 public:
  virtual ~MediaPlayer()=default;
  virtual MediaPlayerTraits get_traits()=0;
  virtual bool is_muted() const { return false; }
  MediaPlayerCall make_call() { return MediaPlayerCall(this); }
  void add_on_state_callback(std::function<void(MediaPlayerState)> fn) { callbacks.push_back(fn); }
  void publish_state() { for (auto &fn:callbacks) fn(state); }
  MediaPlayerState state{MEDIA_PLAYER_STATE_IDLE}; float volume{0.5f};
 protected:
  friend class MediaPlayerCall;
  virtual void control(const MediaPlayerCall &call)=0;
  std::vector<std::function<void(MediaPlayerState)>> callbacks;
};
inline void MediaPlayerCall::perform() { parent->control(*this); }
}
namespace speaker {
class Speaker { public: bool is_stopped() const { return stopped; } bool stopped{true}; };
class SpeakerMediaPlayer: public Component, public media_player::MediaPlayer {
 public:
  media_player::MediaPlayerTraits get_traits() override { return {}; }
  std::vector<media_player::MediaPlayerCall> calls;
 protected:
  void control(const media_player::MediaPlayerCall &call) override { calls.push_back(call); }
};
}
}
#define ESP_LOGI(...) ((void)0)
#define ESP_LOGW(...) ((void)0)
#define ESP_LOGE(...) ((void)0)
'''
test = r'''
#include "guard.h"
#include <cassert>
#include <iostream>
using namespace esphome;
using namespace esphome::media_player;
int main() {
  speaker::SpeakerMediaPlayer backend;
  speaker::Speaker speaker;
  triviapod_media::GuardedMediaPlayer guard;
  bool local_busy=false, amp=false, reserved=false, amp_ok=true;
  guard.set_backend(&backend); guard.set_speaker(&speaker);
  guard.set_local_busy([&] { return local_busy; });
  guard.set_amplifier([&](bool x) { if (x && !amp_ok) return false; amp=x; return true; });
  guard.get_busy_trigger()->callback=[&](bool x) { reserved=x; };
  guard.setup();
  local_busy=true;
  guard.make_call().set_media_url("http://test/clip.wav").perform();
  assert(backend.calls.empty() && !reserved && !amp);
  guard.make_call().set_command(MEDIA_PLAYER_COMMAND_STOP).perform();
  assert(backend.calls.empty());
  local_busy=false; backend.ready=false;
  guard.make_call().set_media_url("http://test/clip.wav").perform();
  assert(backend.calls.empty());
  backend.ready=true;
  guard.make_call().set_media_url("http://test/clip.wav").perform();
  assert(reserved && guard.busy() && amp && backend.calls.size()==2);
  assert(backend.calls[0].get_volume().value()==backend.volume);
  assert(backend.calls[1].get_media_url().has_value() && backend.calls[1].announcement);
  guard.make_call().set_media_url("http://test/second.wav").perform();
  assert(backend.calls.size()==2); // No queued clip can start during a later local recording.
  backend.state=MEDIA_PLAYER_STATE_ANNOUNCING; backend.publish_state();
  speaker.stopped=false; guard.loop(); assert(reserved);
  backend.state=MEDIA_PLAYER_STATE_IDLE; backend.publish_state();
  guard.loop(); assert(reserved && amp); // Wait for actual speaker release too.
  speaker.stopped=true; guard.loop(); assert(!reserved && !amp);
  guard.make_call().set_media_url("http://test/fail.wav").perform();
  assert(reserved && amp);
  now_ms=10000; guard.loop();
  assert(reserved && backend.calls.back().get_command()==MEDIA_PLAYER_COMMAND_STOP);
  now_ms=10999; guard.loop(); assert(reserved);
  now_ms=11000; guard.loop(); assert(!reserved && !amp);
  amp_ok=false;
  const auto count=backend.calls.size();
  guard.make_call().set_media_url("http://test/clip.wav").perform();
  assert(backend.calls.size()==count && !reserved && !amp);
  std::cout << "PASS: local-audio rejection, pending-media lock, duplicate rejection, speaker drain, timeout STOP, volume restore, amp failure\n";
}
'''
with tempfile.TemporaryDirectory(prefix='triviapod-media-test-') as folder:
    temp=Path(folder)
    (temp/'fake.h').write_text(common)
    for header in ['core/component.h','core/automation.h','core/log.h',
                   'components/media_player/media_player.h',
                   'components/speaker/media_player/speaker_media_player.h']:
        target=temp/'esphome'/header
        target.parent.mkdir(parents=True,exist_ok=True)
        target.write_text('#include "fake.h"\n')
    (temp/'guard.h').write_text((project/'components/triviapod_media/triviapod_media.h').read_text())
    (temp/'test.cpp').write_text(test)
    subprocess.run([os.environ.get('CXX') or shutil.which('clang++') or shutil.which('g++'),'-std=c++17','-I',str(temp),str(temp/'test.cpp'),'-o',str(temp/'test')],check=True)
    subprocess.run([str(temp/'test')],check=True)
