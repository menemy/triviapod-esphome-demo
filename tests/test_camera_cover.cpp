#include "camera_cover.h"
#include <cassert>
#include <iostream>

int main() {
  constexpr trivia_camera::CircleCover g{};
  static_assert(g.source_x[0] == 44 && g.source_x[465] == 307);
  static_assert(g.source_y[0] == 0 && g.source_y[465] == 263);
  unsigned covered = 0;
  for (int y = 0; y < g.DIAMETER; ++y) {
    assert(g.left[y] < g.right[y]);
    assert(g.left[y] == g.left[g.DIAMETER - 1 - y]);
    for (int x = 0; x < g.DIAMETER; ++x) {
      const bool drawn = x >= g.left[y] && x < g.right[y];
      // Independent geometric criterion at the physical pixel center.
      const double dx = x + 0.5 - 233.0, dy = y + 0.5 - 233.0;
      assert(drawn == (dx * dx + dy * dy <= 233.0 * 233.0));
      if (!drawn) continue;
      ++covered;
      assert(g.source_x[x] >= 44 && g.source_x[x] < 308);
      assert(g.source_y[y] < 264);
      // Both axes use the same scale and symmetric center crop.
      assert(g.source_x[x] - 44 == g.source_y[x]);
      assert(g.source_x[x] + g.source_x[465 - x] == 351);
      assert(g.source_y[y] + g.source_y[465 - y] == 263);
      if (x) assert(g.source_x[x] - g.source_x[x - 1] <= 1);
      if (y) assert(g.source_y[y] - g.source_y[y - 1] <= 1);
    }
  }
  assert(covered > 170000 && covered < 171000);
  assert(g.left[232] == 0 && g.right[232] == 466);
  std::cout << "Camera cover passed: " << covered
            << " round-panel pixels, no gaps, symmetric crop, equal-axis scale.\n";
}
