#pragma once

#include "Types/PlaguefrontPresentationTypes.h"
#include <algorithm>

namespace Plaguefront {
  enum class SpellStage { Preparation, Launch, Travel, Impact, Fade };
  struct SpellSample {
    SpellStage stage{SpellStage::Impact};
    float phase{}, travel{}, impact{};
    int impactFrame{};
    bool projectile{};
  };
  inline bool FireAwaitsContact(const FireZone &fire, const std::vector<VisualEffect> &effects) {
    return std::any_of(effects.begin(), effects.end(), [&](const VisualEffect &effect) {
      return effect.stagedSpell && effect.type == EffectType::Fire &&
             (effect.delay > 0.F || effect.elapsed < effect.contactSeconds) &&
             std::find(effect.createdFireIds.begin(), effect.createdFireIds.end(), fire.id) != effect.createdFireIds.end();
    });
  }
  inline SpellSample SampleSpell(const VisualEffect &effect) {
    SpellSample sample;
    const float contact = std::clamp(effect.contactSeconds, 0.f, effect.lifetime);
    const float elapsed = std::max(0.f, effect.elapsed);
    sample.projectile = (effect.type == EffectType::Fire || effect.type == EffectType::Ice) && effect.start != effect.end;
    if (contact > 0.f && elapsed < contact) {
      const float preparationEnd = contact * .45f, releaseEnd = contact * .60f;
      if (elapsed < preparationEnd) {
        sample.stage = SpellStage::Preparation;
        sample.phase = elapsed / preparationEnd;
      } else if (elapsed < releaseEnd) {
        sample.stage = SpellStage::Launch;
        sample.phase = (elapsed - preparationEnd) / (releaseEnd - preparationEnd);
      } else {
        sample.stage = SpellStage::Travel;
        sample.phase = (elapsed - releaseEnd) / std::max(.0001f, contact - releaseEnd);
        sample.travel = sample.phase;
      }
    } else {
      sample.impact = std::clamp((elapsed - contact) / std::max(.0001f, effect.lifetime - contact), 0.f, 1.f);
      sample.stage = sample.impact < .65f ? SpellStage::Impact : SpellStage::Fade;
      sample.phase = sample.stage == SpellStage::Impact ? sample.impact / .65f : (sample.impact - .65f) / .35f;
      sample.impactFrame = std::min(3, static_cast<int>(sample.impact * 4.f));
    }
    return sample;
  }
}
