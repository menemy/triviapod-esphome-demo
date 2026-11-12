#pragma once
#include <atomic>
#include <cstdint>
#include <functional>
namespace trivia_metrics {
inline std::function<void()> before_local_audio;
inline std::atomic<int> microphone_peak{0};
inline std::atomic<uint32_t> microphone_samples{0};
}
