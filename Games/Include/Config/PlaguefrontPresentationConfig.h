#pragma once

#include "Types/PlaguefrontPresentationTypes.h"

namespace Plaguefront {
  // Define píxeles nativos del tablero; la vista de SFML y la interfaz aplican después la escala externa.
  inline constexpr float ColumnStep = 30.F, ColumnRise = 14.F, RowStep = 28.F, ElevationStep = 6.F;
  inline constexpr std::array<Point, 6> TopOffsets{{{10.F, -14.F}, {20.F, 0.F}, {10.F, 14.F}, {-10.F, 14.F}, {-20.F, 0.F}, {-10.F, -14.F}}};
  struct AnimationConfig {
    int frames{8};
    float idleFrameSeconds{.18F}, walkFrameSeconds{.035F};
    // Cada casilla reproduce el ciclo completo; movimiento visual y audio comparten esta duración.
    float walkCellSeconds{static_cast<float>(frames) * walkFrameSeconds};
    float attackSeconds{.45F}, hurtSeconds{.30F}, deathSeconds{.65F}, castSeconds{.60F};
    float weaponContactSeconds{.18F}, spellContactSeconds{.225F}, chainIntervalSeconds{.18F};
  };
  inline constexpr AnimationConfig AnimationTiming{};
  // Estos parámetros son visuales; Config de reglas conserva la duración, el radio y el daño de putrefacción.
  struct RotVisualConfig {
    float pulseRadiansPerSecond{2.8F};
    int sourceSporeCount{7}, localSporeCount{2}, globalSporeCount{1};
  };
  inline constexpr RotVisualConfig RotVisual{};
} // espacio de nombres Plaguefront
