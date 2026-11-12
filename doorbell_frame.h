#pragma once
#include "esphome/components/image/image.h"
#include "esphome/components/online_image/online_image.h"
#include "esphome/core/log.h"
#include "esp_heap_caps.h"
#include <cstring>
#include "camera_cover.h"

namespace trivia_camera {
// Independent last-good pixels survive cancellation or failure of the next decode.
class LastFrame : public esphome::image::Image {
 public:
  LastFrame() : Image(nullptr, 0, 0, esphome::image::IMAGE_TYPE_RGB565,
                     esphome::image::TRANSPARENCY_OPAQUE) {}
  bool accept(esphome::online_image::OnlineImage &source) {
    if (source.get_width() != 352 || source.get_height() != 264 || source.get_data_start() == nullptr)
      return false;
    if (pixels_ == nullptr)
      pixels_ = static_cast<uint8_t *>(heap_caps_malloc(BYTES, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    if (pixels_ == nullptr) return false;
    std::memcpy(pixels_, source.get_data_start(), BYTES);
    data_start_ = pixels_; width_ = 352; height_ = 264;
    received_at = esphome::millis(); ++received;
    return true;
  }
  void clear() {
    data_start_ = nullptr; width_ = height_ = 0;
    received_at = received = presented = presented_version_ = 0;
  }
  void draw_cover(esphome::display::Display &display) const {
    if (pixels_ == nullptr) return;
    static constexpr CircleCover geometry{};
    for (int y = 0; y < CircleCover::DIAMETER; ++y) {
      const int sy = geometry.source_y[y];
      for (int x = geometry.left[y]; x < geometry.right[y]; ++x)
        display.draw_pixel_at(x, y, get_rgb565_pixel_(geometry.source_x[x], sy));
    }
  }
  void mark_presented() {
    if (received != presented_version_) { ++presented; presented_version_ = received; }
  }
  static constexpr size_t BYTES = 352 * 264 * 2;
  uint32_t received_at{0}, received{0}, presented{0};
 protected:
  uint8_t *pixels_{nullptr};
  uint32_t presented_version_{0};
};
inline LastFrame last;
}
