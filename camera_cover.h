#pragma once
#include <array>
#include <cstdint>

namespace trivia_camera {
// Center-crop a 352x264 frame to 264x264, then scale uniformly to the
// 466px round panel. Pixel-center coordinates cover every visible pixel.
struct CircleCover {
  static constexpr int DIAMETER = 466;
  static constexpr int SOURCE_WIDTH = 352;
  static constexpr int SOURCE_HEIGHT = 264;
  static constexpr int CROP_LEFT = (SOURCE_WIDTH - SOURCE_HEIGHT) / 2;
  std::array<uint16_t, DIAMETER> source_x{}, source_y{}, left{}, right{};

  static constexpr bool visible(int x, int y) {
    const int dx = 2 * x + 1 - DIAMETER;
    const int dy = 2 * y + 1 - DIAMETER;
    return dx * dx + dy * dy <= DIAMETER * DIAMETER;
  }
  constexpr CircleCover() {
    for (int v = 0; v < DIAMETER; ++v) {
      // Resolve exact half-pixel ties outward to keep mirror symmetry.
      const int source = ((2 * v + 1) * SOURCE_HEIGHT - (v < DIAMETER / 2 ? 1 : 0)) / (2 * DIAMETER);
      source_x[v] = CROP_LEFT + source;
      source_y[v] = source;
      int edge = 0;
      while (!visible(edge, v)) ++edge;
      left[v] = edge;
      right[v] = DIAMETER - edge;  // Exclusive.
    }
  }
};
}  // namespace trivia_camera
