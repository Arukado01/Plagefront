#pragma once

#include "Types/PlaguefrontPresentationTypes.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <span>

namespace Plaguefront {
  struct UnitSupportWindow {
    int left{}, top{}, right{}, bottom{};
  };
  struct UnitContact {
    bool valid{};
    int left{}, right{}, bottom{};
    int shadowLeft{}, shadowTop{}, shadowWidth{}, shadowHeight{};
  };
  using UnitLowerBand = std::array<std::uint64_t, 20>;

  inline Point SnapUnitPoint(Point point) { return {std::round(point.x), std::round(point.y)}; }

  inline bool UnitWalkSurfaceEligible(const UnitVisual &visual, Hex position, float height, float cellSeconds) {
    if (visual.state != AnimState::Walk || visual.path.size() < 2 || cellSeconds <= 0.F || !std::isfinite(cellSeconds) ||
        !std::isfinite(visual.elapsed) || !std::isfinite(visual.elevation) || !std::isfinite(height))
      return false;
    if (height <= visual.elevation + .0001F)
      return true;
    const auto segment = static_cast<std::size_t>(std::clamp(std::floor(std::max(0.F, visual.elapsed) / cellSeconds), 0.F, static_cast<float>(visual.path.size() - 2)));
    return position == visual.path[segment] || position == visual.path[segment + 1];
  }

  inline UnitSupportWindow UnitContactWindow(UnitClass type, AnimState state, int facing, int frame) {
    const int cls = static_cast<int>(type);
    if (cls < 0 || cls >= 7)
      return {};
    constexpr std::array<int, 7> left{{9, 10, 9, 7, 10, 10, 8}}, right{{38, 36, 38, 41, 32, 38, 39}};
    UnitSupportWindow window{left[cls], 44, right[cls], 58};
    // El bastón del Caster y las armas periféricas quedan fuera de la ventana de apoyo.
    if (state == AnimState::Death && frame >= 4)
      window = type == UnitClass::Rider ? UnitSupportWindow{7, 44, 41, 59} : UnitSupportWindow{10, 44, 38, 59};
    if (facing % 2 != 0) {
      const int mirroredLeft = 48 - window.right;
      window.right = 48 - window.left;
      window.left = mirroredLeft;
    }
    return window;
  }

  inline UnitContact UnitContactGeometry(UnitClass type, AnimState state, int facing, int frame, std::span<const std::uint8_t, 48 * 64> alpha) {
    const auto window = UnitContactWindow(type, state, facing, frame);
    UnitContact contact;
    for (int y = window.bottom - 1; y >= window.top && !contact.valid; --y)
      for (int x = window.left; x < window.right; ++x)
        if (alpha[static_cast<std::size_t>(y * 48 + x)] > 0) {
          contact.bottom = y + 1;
          contact.valid = true;
          break;
        }
    if (!contact.valid)
      return contact;
    contact.left = window.right;
    for (int y = std::max(window.top, contact.bottom - 3); y < contact.bottom; ++y)
      for (int x = window.left; x < window.right; ++x)
        if (alpha[static_cast<std::size_t>(y * 48 + x)] > 0) {
          contact.left = std::min(contact.left, x);
          contact.right = std::max(contact.right, x + 1);
        }
    const int cls = static_cast<int>(type);
    const bool rider = type == UnitClass::Rider, fallen = state == AnimState::Death && frame >= 4;
    constexpr std::array<int, 7> minimum{{10, 8, 10, 18, 8, 8, 12}}, maximum{{16, 13, 16, 28, 12, 12, 18}};
    const int measuredWidth = contact.right - contact.left;
    const int doubledCenter = std::clamp(contact.left + contact.right, 44, 52);
    contact.shadowWidth = fallen ? std::clamp(static_cast<int>(std::lround(measuredWidth * .70f)), rider ? 22 : 18, rider ? 30 : 24)
                                : std::clamp(static_cast<int>(std::lround(measuredWidth * (rider ? .75f : .55f))), minimum[cls], maximum[cls]);
    contact.shadowLeft = std::clamp((doubledCenter - contact.shadowWidth + (facing % 2 != 0 ? 1 : 0)) / 2, 1, 47 - contact.shadowWidth);
    contact.shadowTop = contact.bottom - 1;
    contact.shadowHeight = rider && !fallen ? 3 : 2;
    return contact;
  }

  inline UnitLowerBand UnitLowerBandGeometry(std::span<const std::uint8_t, 48 * 64> alpha) {
    UnitLowerBand band{};
    // La banda conserva también espada, escudo y faldón; no sustituye la ventana anatómica de apoyo.
    for (int y = 44; y < 64; ++y)
      for (int x = 0; x < 48; ++x)
        if (alpha[static_cast<std::size_t>(y * 48 + x)] > 0)
          band[static_cast<std::size_t>(y - 44)] |= std::uint64_t{1} << x;
    return band;
  }

  inline bool UnitSurfaceTouchesRect(Rect rectangle, const std::array<Point, 6> &surface) {
    if (rectangle.width <= 0.F || rectangle.height <= 0.F || !std::isfinite(rectangle.left) || !std::isfinite(rectangle.top))
      return false;
    float left = surface[0].x, right = left, top = surface[0].y, bottom = top;
    for (const auto point : surface) {
      if (!std::isfinite(point.x) || !std::isfinite(point.y))
        return false;
      left = std::min(left, point.x); right = std::max(right, point.x);
      top = std::min(top, point.y); bottom = std::max(bottom, point.y);
    }
    const float x1 = rectangle.left + rectangle.width, y1 = rectangle.top + rectangle.height;
    if (x1 <= left || rectangle.left >= right || y1 <= top || rectangle.top >= bottom)
      return false;
    for (std::size_t edge = 0; edge < surface.size(); ++edge) {
      const auto a = surface[edge], b = surface[(edge + 1) % surface.size()];
      const Point normal{a.y - b.y, b.x - a.x};
      float polygonMin = surface[0].x * normal.x + surface[0].y * normal.y, polygonMax = polygonMin;
      for (const auto point : surface) {
        const float value = point.x * normal.x + point.y * normal.y;
        polygonMin = std::min(polygonMin, value); polygonMax = std::max(polygonMax, value);
      }
      const float rectangleMin = (normal.x >= 0.F ? rectangle.left : x1) * normal.x + (normal.y >= 0.F ? rectangle.top : y1) * normal.y;
      const float rectangleMax = (normal.x >= 0.F ? x1 : rectangle.left) * normal.x + (normal.y >= 0.F ? y1 : rectangle.top) * normal.y;
      if (rectangleMax <= polygonMin || rectangleMin >= polygonMax)
        return false;
    }
    return true;
  }

  inline bool UnitLowerBandTouchesSurface(const UnitLowerBand &band, const UnitContact &contact, Point anchor, int zoom, int owner, const std::array<Point, 6> &surface) {
    if (zoom <= 0 || !std::isfinite(anchor.x) || !std::isfinite(anchor.y))
      return false;
    const auto touches = [&](int x, int y, int width, int height) {
      return UnitSurfaceTouchesRect({anchor.x + (x - 24) * zoom, anchor.y + (y - 56) * zoom, static_cast<float>(width * zoom), static_cast<float>(height * zoom)}, surface);
    };
    if (contact.valid) {
      const int coreRows = contact.shadowHeight - 1;
      for (int row = 0; row < coreRows; ++row) {
        const int inset = coreRows > 1 && row == 0 ? 2 : 0;
        if (touches(contact.shadowLeft + inset, contact.shadowTop + row, contact.shadowWidth - inset * 2, 1))
          return true;
      }
      if (touches(contact.shadowLeft + 2, contact.shadowTop + coreRows, contact.shadowWidth - 4, 1))
        return true;
    }
    if ((owner == 0 && (touches(10, 60, 3, 1) || touches(11, 59, 1, 3))) || (owner == 1 && (touches(10, 59, 1, 3) || touches(12, 59, 1, 3))))
      return true;
    for (std::size_t row = 0; row < band.size(); ++row)
      for (auto pixels = band[row]; pixels; pixels &= pixels - 1)
        if (touches(static_cast<int>(std::countr_zero(pixels)), static_cast<int>(row) + 44, 1, 1))
          return true;
    return false;
  }
} // espacio de nombres Plaguefront
