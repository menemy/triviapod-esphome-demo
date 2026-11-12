#pragma once
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <vector>
#include "esp_heap_caps.h"

namespace trivia_loopback {
inline constexpr size_t CAPACITY = 3 * 16000 * 2;
inline uint8_t *buffer = nullptr;
inline std::atomic<bool> recording{false};
inline std::atomic<size_t> captured_bytes{0};
inline size_t playback_offset = 0;
inline uint32_t playback_started_ms = 0;
inline bool queued_all = false;

// Call prepare/release only after both I2S directions report stopped.
inline void release() {
  recording.store(false, std::memory_order_release);
  if (buffer != nullptr) {
    std::memset(buffer, 0, CAPACITY);
    heap_caps_free(buffer);
    buffer = nullptr;
  }
  captured_bytes.store(0, std::memory_order_release);
  playback_offset = 0;
}
inline bool prepare() {
  release();
  buffer = static_cast<uint8_t *>(heap_caps_calloc(CAPACITY, 1, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
  queued_all = false;
  return buffer != nullptr;
}
inline size_t size() { return captured_bytes.load(std::memory_order_acquire); }
inline void append(const std::vector<uint8_t> &data) {
  if (!recording.load(std::memory_order_acquire) || buffer == nullptr) return;
  const size_t used = captured_bytes.load(std::memory_order_relaxed);
  if (used >= CAPACITY) return;
  const size_t count = std::min(data.size(), CAPACITY - used) & ~size_t(1);
  if (count != 0) std::memcpy(buffer + used, data.data(), count);
  captured_bytes.store(used + count, std::memory_order_release);
}
}
