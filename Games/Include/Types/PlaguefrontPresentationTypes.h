#pragma once

#include "Types/PlaguefrontRulesTypes.h"

namespace Plaguefront {
  struct Point {
    float x{}, y{};
  };
  struct Rect {
    float left{}, top{}, width{}, height{};
  };

  enum class AnimState { Idle,
                         Walk,
                         Attack,
                         Hurt,
                         Death,
                         Cast };
  enum class EffectType { Slash,
                          Arrow,
                          Fire,
                          Ice,
                          Lightning,
                          Heal,
                          RotBurst,
                          Purify,
                          Capture,
                          Damage,
                          Spawn,
                          Smoke };
  struct UnitVisual {
    int id{}, owner{};
    UnitClass type{UnitClass::Saber};
    float q{}, r{}, elevation{}, elapsed{};
    AnimState state{AnimState::Idle};
    int facing{}, frame{};
    bool visible{true};
    std::vector<Hex> path;
    std::vector<float> heights;
  };
  struct VisualEffect {
    EffectType type{EffectType::Slash};
    Hex start{}, end{};
    float startHeight{}, endHeight{}, elapsed{}, lifetime{0.45F}, delay{};
    int amount{}, owner{-1};
    std::string text;
    bool Active() const { return delay <= 0.F && elapsed < lifetime; }
    float Progress() const;
  };

  // La clasificación es visual; Match determina el daño y la prioridad entre contaminación local y global.
  struct RotExposure {
    bool local{}, global{};
    int damage{}, coveringSources{};
    bool Active() const { return damage > 0; }
  };
} // espacio de nombres Plaguefront
