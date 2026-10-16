#include "Plaguefront/Game.h"
#include "Plaguefront/SpellPresentation.h"
#include "Core/EngineConfig.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>

namespace Plaguefront {
  namespace {
    constexpr float Tau = 6.283185307f;
    struct SpellPixels {
      sf::VertexArray pixels{sf::PrimitiveType::Points};
      int zoom;
      void Dot(Point p, sf::Color color, int size = 1) {
        for (int y = 0; y < size * zoom; ++y)
          for (int x = 0; x < size * zoom; ++x)
            pixels.append({{std::round(p.x) + x + .5f, std::round(p.y) + y + .5f}, color});
      }
      void Stroke(Point a, Point b, sf::Color color) {
        const int steps = std::max(1, static_cast<int>(std::ceil(std::max(std::abs(b.x - a.x), std::abs(b.y - a.y)) / zoom)));
        for (int i = 0; i <= steps; ++i) {
          const float t = static_cast<float>(i) / steps;
          Dot({a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t}, color);
        }
      }
    };
    sf::Color Opacity(sf::Color color, float amount) {
      color.a = static_cast<std::uint8_t>(std::round(color.a * std::clamp(amount, 0.f, 1.f)));
      return color;
    }
  }

  void Game::RenderSpellEffect(const VisualEffect &effect) const {
    const auto sample = SampleSpell(effect);
    const int zoom = camera_.Zoom();
    const auto groundFrom = camera_.Project(effect.start, effect.startHeight);
    const auto groundTo = camera_.Project(effect.end, effect.endHeight);
    const Point from{groundFrom.x, groundFrom.y - 21 * zoom}, to{groundTo.x, groundTo.y - 16 * zoom};
    const sf::Color ivory{241, 223, 164}, fire{231, 164, 81}, ice{131, 199, 210}, violet{195, 179, 214}, leaf{159, 179, 79};
    const sf::Color element = effect.type == EffectType::Fire ? fire : effect.type == EffectType::Ice ? ice : effect.type == EffectType::Lightning ? violet : effect.type == EffectType::Heal ? leaf : ivory;
    SpellPixels glyph{sf::VertexArray(sf::PrimitiveType::Points), zoom};
    if (sample.stage == SpellStage::Preparation || sample.stage == SpellStage::Launch) {
      const float close = sample.stage == SpellStage::Preparation ? 1.f - sample.phase : 0.f;
      for (int i = 0; i < 5; ++i) {
        const float angle = i * Tau / 5.f + effect.elapsed * 4.f;
        const Point mote{from.x + std::cos(angle) * (2.f + close * 7.f) * zoom,
                         from.y + std::sin(angle) * (2.f + close * 4.f) * zoom - i % 2 * zoom};
        glyph.Dot(mote, element);
        if (effect.type == EffectType::Ice || effect.type == EffectType::Purify)
          glyph.Dot({mote.x, mote.y - zoom}, ivory);
        else if (effect.type == EffectType::Lightning)
          glyph.Stroke(mote, {from.x + (i % 2 ? 2 : -2) * zoom, from.y}, ivory);
      }
      if (sample.stage == SpellStage::Launch) {
        glyph.Stroke({from.x - 3 * zoom, from.y}, {from.x + 3 * zoom, from.y}, ivory);
        glyph.Stroke({from.x, from.y - 4 * zoom}, {from.x, from.y + 2 * zoom}, element);
      }
    } else if (sample.stage == SpellStage::Travel) {
      if (sample.projectile) {
        const float t = sample.travel;
        const Point p{from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t - std::sin(t * Tau / 2.f) * 5 * zoom};
        const float dx = to.x - from.x, dy = to.y - from.y, length = std::max(1.f, std::sqrt(dx * dx + dy * dy));
        const Point tail{p.x - dx / length * 9 * zoom, p.y - dy / length * 9 * zoom};
        glyph.Stroke(tail, p, element);
        glyph.Dot({p.x - zoom, p.y - zoom}, ivory, 2);
        if (effect.type == EffectType::Fire) {
          glyph.Dot({tail.x - 2 * zoom, tail.y + zoom}, {197, 100, 63});
          glyph.Dot({tail.x + zoom, tail.y - 2 * zoom}, fire);
        } else {
          glyph.Stroke({p.x - 3 * zoom, p.y - 3 * zoom}, {p.x + 2 * zoom, p.y + zoom}, ivory);
          glyph.Stroke({p.x - 3 * zoom, p.y + 3 * zoom}, {p.x + 2 * zoom, p.y - zoom}, ice);
        }
      } else if (effect.type == EffectType::Lightning) {
        glyph.Stroke(from, {from.x + (to.x - from.x) * sample.travel, from.y + (to.y - from.y) * sample.travel}, violet);
      } else {
        for (int i = 0; i < 4; ++i) {
          const float t = std::clamp(sample.travel - i * .11f, 0.f, 1.f);
          glyph.Dot({from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t - std::sin(t * Tau / 2.f) * 8 * zoom}, element, i == 0 ? 2 : 1);
        }
      }
    } else {
      const float fade = sample.stage == SpellStage::Fade ? 1.f - sample.phase : 1.f;
      if (effect.type == EffectType::Lightning && sample.impact < .45f) {
        Point previous = from;
        for (int i = 1; i <= 7; ++i) {
          const float t = i / 7.f;
          const float jitter = i == 7 ? 0.f : std::sin(i * 7.f + std::floor(effect.elapsed * 30.f) * 2.f) * 3 * zoom;
          const Point joint{from.x + (to.x - from.x) * t + jitter, from.y + (to.y - from.y) * t - jitter};
          glyph.Stroke(previous, joint, i % 2 ? ivory : violet);
          if (i % 2 == 0)
            glyph.Stroke(joint, {joint.x + (i % 3 ? 4 : -4) * zoom, joint.y + 5 * zoom}, violet);
          previous = joint;
        }
      }
      const int count = effect.type == EffectType::Heal ? 7 : 6;
      for (int i = 0; i < count; ++i) {
        const float phase = sample.impact, angle = i * Tau / count + .4f;
        Point p{to.x + std::cos(angle) * (4.f + phase * 13.f) * zoom,
                to.y + std::sin(angle) * (3.f + phase * 7.f) * zoom};
        if (effect.type == EffectType::Heal)
          p.y -= phase * (8.f + i % 3 * 2.f) * zoom;
        else if (effect.type == EffectType::Fire)
          p.y -= phase * 8 * zoom;
        glyph.Dot(p, Opacity(i % 3 ? element : ivory, fade));
        if (effect.type == EffectType::Ice)
          glyph.Stroke(p, {p.x + (i % 2 ? 2 : -2) * zoom, p.y - 3 * zoom}, Opacity(ice, fade));
        else if (effect.type == EffectType::Purify)
          glyph.Dot({p.x + zoom, p.y - zoom}, Opacity(gConfig.rotPalette.sourceLight, fade * .65f));
      }
      if (spellEffects_) {
        const int column = effect.type == EffectType::Fire ? 2 : effect.type == EffectType::Ice ? 3 : effect.type == EffectType::Lightning ? 4 : effect.type == EffectType::Heal ? 5 : 7;
        sf::Sprite impact(*spellEffects_, sf::IntRect({column * 48, sample.impactFrame * 48}, {48, 48}));
        impact.setOrigin({24, 24});
        impact.setPosition({std::round(to.x), std::round(to.y)});
        impact.setScale({static_cast<float>(zoom), static_cast<float>(zoom)});
        impact.setColor(Opacity(sf::Color::White, fade));
        world_.draw(impact);
      }
    }
    world_.draw(glyph.pixels);
  }
}
