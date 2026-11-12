#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace trivia_tone {
inline constexpr size_t RATE = 16000;
inline constexpr size_t SAMPLES = RATE;
inline constexpr size_t BYTES = SAMPLES * 2;
inline constexpr float FREQUENCY = 440.0f;
inline constexpr float AMPLITUDE = 3200.0f;
enum class Profile { REFERENCE_A, HIGH_B };
inline Profile requested_profile{Profile::REFERENCE_A};
inline Profile playing_profile{Profile::REFERENCE_A};
inline const char *profile_name() { return playing_profile == Profile::HIGH_B ? "B" : "A"; }
inline const char *profile_label() { return playing_profile == Profile::HIGH_B ? "Tone B: 1k / 2k" : "Tone A: 440Hz"; }
inline unsigned first_frequency() { return playing_profile == Profile::HIGH_B ? 1000 : 440; }
inline unsigned second_frequency() { return playing_profile == Profile::HIGH_B ? 2000 : 0; }
inline unsigned peak_amplitude() { return playing_profile == Profile::HIGH_B ? 12000 : 3200; }

// These are producer-call timing metrics, not hardware-underrun detection.
struct WriteTiming {
  uint32_t attempts{0}, successful{0}, zero_accepts{0}, partial_accepts{0};
  uint32_t last_call{0}, last_success{0};
  uint32_t call_gap_min{std::numeric_limits<uint32_t>::max()}, call_gap_max{0};
  uint32_t success_gap_min{std::numeric_limits<uint32_t>::max()}, success_gap_max{0};
  uint32_t call_duration_max{0};
  void observe(uint32_t call_us, uint32_t done_us, size_t offered, size_t accepted) {
    if (attempts++) {
      const uint32_t gap = call_us - last_call;  // Unsigned wrap is intentional.
      call_gap_min = std::min(call_gap_min, gap); call_gap_max = std::max(call_gap_max, gap);
    }
    last_call = call_us;
    call_duration_max = std::max(call_duration_max, uint32_t(done_us - call_us));
    if (accepted == 0) { ++zero_accepts; return; }
    if (accepted < offered) ++partial_accepts;
    if (successful++) {
      const uint32_t gap = done_us - last_success;
      success_gap_min = std::min(success_gap_min, gap); success_gap_max = std::max(success_gap_max, gap);
    }
    last_success = done_us;
  }
  uint32_t min_call_gap() const { return attempts > 1 ? call_gap_min : 0; }
  uint32_t min_success_gap() const { return successful > 1 ? success_gap_min : 0; }
};
inline WriteTiming timing{};
inline std::array<uint8_t, 1024> chunk{};
inline size_t offset = 0;
inline uint32_t started_ms = 0;
inline bool failed = false;
inline bool all_queued = false;
inline bool drained = false;
inline std::atomic<bool> active{false};
inline std::atomic<uint32_t> dac_frames{0};
inline bool complete(bool speaker_stopped) {
  return !failed && all_queued && drained && speaker_stopped && dac_frames.load() == SAMPLES;
}

inline int16_t sample(size_t n) {
  if (n >= SAMPLES) return 0;
  // Preserve A exactly; B contains two separately faded 500ms segments.
  const bool high = playing_profile == Profile::HIGH_B;
  const size_t length = high ? SAMPLES / 2 : SAMPLES;
  const size_t index = high ? n % length : n;
  const float frequency = high ? (n < length ? 1000.0f : 2000.0f) : FREQUENCY;
  const float amplitude = high ? 12000.0f : AMPLITUDE;
  const float envelope = std::min(1.0f, std::min(float(index) / 160.0f, float(length - 1 - index) / 160.0f));
  return int16_t(amplitude * envelope * sinf(2.0f * 3.14159265f * frequency * float(index) / float(RATE)));
}
inline void reset() {
  active.store(false); dac_frames.store(0); offset = 0; started_ms = 0;
  failed = false; all_queued = false; drained = false; timing = {};
  playing_profile = requested_profile;  // Latch for the whole clip, even if UI selection changes.
}
inline size_t make_chunk() {
  if (offset >= BYTES) return 0;
  const size_t bytes = std::min(chunk.size(), BYTES - offset) & ~size_t(1);
  for (size_t i = 0; i < bytes; i += 2) {
    const int16_t value = sample((offset + i) / 2);
    chunk[i] = uint8_t(value);
    chunk[i + 1] = uint8_t(uint16_t(value) >> 8);
  }
  return bytes;
}
}
