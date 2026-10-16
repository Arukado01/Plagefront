#pragma once

#include "Types/PlaguefrontPresentationTypes.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace Plaguefront {
  struct RainDrop {
    Point head{}, tail{};
    std::uint8_t opacity{}, layer{};
  };

  inline std::vector<RainDrop> RainDrops(float width, float height, float elapsed, bool storm = false) {
    if (!std::isfinite(width) || !std::isfinite(height) || !std::isfinite(elapsed) || width <= 0.F || height <= 0.F)
      return {};
    const double area = static_cast<double>(width) * height;
    const int count = static_cast<int>(std::clamp(std::ceil(area / (storm ? 2600.0 : 3300.0)), 64.0, 360.0));
    const int columns = static_cast<int>(std::clamp(std::ceil(std::sqrt(count * static_cast<double>(width) / height)), 1.0, static_cast<double>(count)));
    const int rows = (count + columns - 1) / columns;
    const auto noise = [](std::uint32_t seed) {
      seed ^= seed >> 16;
      seed *= 0x7feb352du;
      seed ^= seed >> 15;
      seed *= 0x846ca68bu;
      seed ^= seed >> 16;
      return static_cast<double>(seed & 0xffffu) / 65535.0;
    };
    const auto wrap = [](double value, float extent) {
      double position = std::fmod(value, static_cast<double>(extent));
      if (position < 0.0)
        position += extent;
      return std::clamp(static_cast<float>(position), 0.F, std::nextafter(extent, 0.F));
    };
    std::vector<RainDrop> drops;
    drops.reserve(static_cast<std::size_t>(count));
    for (int i = 0; i < count; ++i) {
      const auto seed = static_cast<std::uint32_t>(i + 1);
      const int layer = i % 3;
      const double speed = (70.0 + layer * 41.0 + noise(seed * 53u) * 20.0) * (storm ? 1.10 : 1.0);
      const double wind = .28 + layer * .025 + noise(seed * 97u) * .035;
      const double x = ((i % columns) + .15 + noise(seed * 11u) * .70) / columns * width;
      const double y = ((i / columns) + .15 + noise(seed * 31u) * .70) / rows * height;
      const Point head{wrap(x - elapsed * speed * wind, width), wrap(y + elapsed * speed, height)};
      const float length = static_cast<float>(3.5 + layer * 2.25 + noise(seed * 71u));
      const float inverse = 1.F / std::sqrt(1.F + static_cast<float>(wind * wind));
      const Point tail{head.x + length * static_cast<float>(wind) * inverse, head.y - length * inverse};
      const auto opacity = static_cast<std::uint8_t>(43 + layer * 23 + static_cast<int>(noise(seed * 101u) * 12.0) + (storm ? 6 : 0));
      drops.push_back({head, tail, opacity, static_cast<std::uint8_t>(layer)});
    }
    return drops;
  }
} // espacio de nombres Plaguefront
