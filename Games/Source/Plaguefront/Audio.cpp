#include "Plaguefront/Audio.h"
#include "Config/PlaguefrontPresentationConfig.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace Plaguefront {
  namespace {
    bool Offensive(Ability ability) {
      return ability == Ability::Attack || ability == Ability::Focus || ability == Ability::Thrust ||
             ability == Ability::Fire || ability == Ability::Ice || ability == Ability::Lightning || ability == Ability::Furious;
    }
    std::optional<Ability> NewAttackLog(const Match &before, const Match &after) {
      // Detecta sólo entradas nuevas tras el sufijo conservado para no repetir ataques de un registro acotado.
      const auto &old = before.state.log;
      const auto &next = after.state.log;
      std::size_t preserved = std::min(old.size(), next.size());
      for (; preserved > 0; --preserved)
        if (std::equal(old.end() - static_cast<std::ptrdiff_t>(preserved), old.end(), next.begin()))
          break;
      for (std::size_t i = next.size(); i > preserved; --i)
        for (int a = 0; a <= static_cast<int>(Ability::Incinerate); ++a) {
          const auto ability = static_cast<Ability>(a);
          if (Offensive(ability) && next[i - 1].starts_with(Name(ability) + ":") && next[i - 1].ends_with(" de dano."))
            return ability;
        }
      return {};
    }
    const Unit *InferAttacker(const Match &before, const Match &after, Ability ability) {
      const Unit *best = nullptr;
      int bestScore = -1;
      std::vector<const Unit *> eligible;
      for (const auto &old : before.state.units)
        if (old.Alive() && old.owner == before.state.currentPlayer && old.readyRound <= before.state.round &&
            (!old.activated || before.state.activeUnit == old.id) && after.Find(old.id))
          eligible.push_back(&old);
      for (const auto *old : eligible) {
        const auto *next = after.Find(old->id);
        const auto allowed = before.Abilities(old->id);
        if (std::find(allowed.begin(), allowed.end(), ability) == allowed.end())
          continue;
        int score = eligible.size() == 1 ? 100 : 0;
        if (before.state.activeUnit == old->id)
          score += 100;
        if (next->pos != old->pos)
          score += 25;
        if (next->mp < old->mp)
          score += 35;
        if (next->cooldowns[static_cast<std::size_t>(ability)] > old->cooldowns[static_cast<std::size_t>(ability)])
          score += 25;
        // Infiere al atacante con el clima y los blancos previos a EndRound, incluso si un repliegue posterior cambia el alcance.
        Match probe = before;
        auto *actor = probe.Find(old->id);
        actor->pos = next->pos;
        if (!old->activated && actor->type == UnitClass::Caster)
          actor->mp = std::min(probe.config.maxMana, actor->mp + probe.config.manaRecovery);
        for (const auto &target : after.state.units) {
          const auto *oldTarget = before.Find(target.id);
          if (oldTarget && oldTarget->Alive() && oldTarget->owner != old->owner && target.hp < oldTarget->hp && probe.CanTarget(old->id, target.id, ability)) {
            score += 50;
            break;
          }
        }
        if (score > bestScore && score > 0) {
          bestScore = score;
          best = next;
        }
      }
      return best;
    }
  } // Espacio de nombres
  bool AudioDirector::Start(const std::string &manifest) {
    if (ready_)
      return true;
    try {
      std::ifstream input(manifest);
      if (!input)
        throw std::runtime_error("No se encuentra el manifiesto de audio");
      const auto j = nlohmann::json::parse(input);
      bool complete = true;
      for (const auto &cue : j.at("cues")) {
        const auto busName = cue.at("bus").get<std::string>();
        const auto bus = busName == "music" ? AudioBus::Music : (busName == "ambient" ? AudioBus::Ambient : AudioBus::Effects);
        complete = manager_.LoadCue(cue.at("id").get<std::string>(), cue.at("file").get<std::string>(), bus,
                                    cue.value("gain", 70.f), cue.value("loop", false), cue.value("cooldown", 0.08f)) &&
                   complete;
      }
      ready_ = complete;
      return ready_;
    } catch (const std::exception &e) {
      std::cerr << "AudioDirector: " << e.what() << '\n';
      return false;
    }
  }
  void AudioDirector::SetScene(const std::string &scene) {
    if (scene_ == scene)
      return;
    scene_ = scene;
    pulseTimer_ = 0;
    if (scene != "battle")
      pending_.clear();
    if (scene == "battle") {
      Play("music_battle");
      Play("ambient_battle");
    } else {
      manager_.StopAmbient();
      contaminated_ = false;
      if (scene == "victory")
        Play("music_victory");
      else if (scene == "defeat")
        Play("music_defeat");
      else
        Play("music_menu");
    }
  }
  bool AudioDirector::Play(const std::string &cue) { return manager_.PlayCue(cue); }
  void AudioDirector::SetVolumes(float master, float music, float effects, bool muted) { manager_.SetChannelVolumes(master, music, effects, muted); }
  void AudioDirector::Update(float seconds) {
    manager_.Update(seconds);
    for (auto &cue : pending_) {
      cue.delay -= std::max(0.f, seconds);
      if (cue.delay <= 0.f)
        Play(cue.cue);
    }
    std::erase_if(pending_, [](const DelayedCue &cue) { return cue.delay <= 0.f; });
    if (scene_ == "battle" && contaminated_) {
      pulseTimer_ += std::max(0.f, seconds);
      if (pulseTimer_ >= 7.5f) {
        pulseTimer_ = 0;
        Play("rot_pulse");
      }
    }
  }
  void AudioDirector::Observe(const Match &before, const Match &after, std::optional<Ability> hint) {
    const auto &a = before.state;
    const auto &b = after.state;
    contaminated_ = !b.sources.empty() || b.globalRemaining > 0;
    bool damage = false, death = false, healing = false, movement = false, itemUsed = false;
    bool enemyHurt = false;
    const Unit *attacker = nullptr, *oldAttacker = nullptr;
    for (const auto &next : b.units) {
      const auto *old = before.Find(next.id);
      if (!old) {
        Play("recruit");
        continue;
      }
      if (old->Alive() && next.Alive() && old->pos != next.pos)
        movement = true;
      if (old->hp > next.hp)
        damage = true;
      if (old->Alive() && old->owner != a.currentPlayer && old->hp > next.hp)
        enemyHurt = true;
      if (old->Alive() && !next.Alive())
        death = true;
      if (old->hp < next.hp)
        healing = true;
      if (next.inventory.size() < old->inventory.size())
        itemUsed = true;
      if (next.attacked && !old->attacked) {
        attacker = &next;
        oldAttacker = old;
      }
    }
    std::vector<const CombatEvent *> events;
    for (const auto &event : b.combatEvents)
      if (event.id >= a.nextEventId)
        events.push_back(&event);
    if (!events.empty()) {
      attacker = nullptr;
      oldAttacker = nullptr;
      auto schedule = [&](const std::string &cue, float delay) {if(delay<=0.f)Play(cue);else pending_.push_back({delay,cue}); };
      float base = 0.f, lastImpact = 0.f, sequenceEnd = 0.f;
      for (const auto *event : events)
        if (!event->counter && event->chainIndex == 0)
          if (const auto *old = before.Find(event->attackerId); old && old->pos != event->from)
            if (const auto path = before.FindPath(old->id, event->from))
              base = static_cast<float>(path->cells.size() - 1) * AnimationTiming.walkCellSeconds;
      for (const auto *event : events) {
        const auto *actor = after.Find(event->attackerId), *target = after.Find(event->targetId);
        if (!actor || !target)
          continue;
        const float start = event->counter ? lastImpact + AnimationTiming.hurtSeconds : base + (event->chainIndex > 0 ? AnimationTiming.spellContactSeconds : 0.f) + event->chainIndex * AnimationTiming.chainIntervalSeconds;
        const float impact = start + (event->chainIndex > 0 ? 0.f : actor->type == UnitClass::Caster ? AnimationTiming.spellContactSeconds
                                                                                                     : AnimationTiming.weaponContactSeconds);
        const std::string cue = event->ability == Ability::Fire ? "spell_fire" : event->ability == Ability::Ice     ? "spell_ice"
                                                                             : event->ability == Ability::Lightning ? "spell_lightning"
                                                                             : actor->type == UnitClass::Caster     ? "spell_support"
                                                                             : actor->type == UnitClass::Archer     ? "arrow"
                                                                             : actor->type == UnitClass::Lancer     ? "spear"
                                                                             : actor->type == UnitClass::Rider      ? "charge"
                                                                             : actor->type == UnitClass::Berserker  ? "axe"
                                                                             : actor->type == UnitClass::Assassin   ? "dagger"
                                                                                                                    : "sword";
        schedule(cue, start);
        schedule("impact", impact);
        if (event->lethal)
          schedule("death", impact + 0.01f);
        lastImpact = std::max(lastImpact, impact);
        sequenceEnd = std::max({sequenceEnd, start + (actor->type == UnitClass::Caster ? AnimationTiming.castSeconds : AnimationTiming.attackSeconds), impact + (event->lethal ? AnimationTiming.deathSeconds : AnimationTiming.hurtSeconds)});
      }
      bool ambientImpact = false, ambientDeath = false;
      for (const auto &unit : b.units)
        if (const auto *old = before.Find(unit.id)) {
          int combat = 0;
          bool combatDeath = false;
          for (const auto *event : events)
            if (event->targetId == unit.id) {
              combat += event->amount;
              combatDeath |= event->lethal;
            }
          if (old->hp - unit.hp > combat) {
            ambientImpact = true;
            if (old->Alive() && !unit.Alive() && !combatDeath)
              ambientDeath = true;
          }
        }
      const float ambientTime = std::max(lastImpact + 0.35f, sequenceEnd);
      if (ambientImpact)
        schedule("impact", ambientTime);
      if (ambientDeath)
        schedule("death", ambientTime + 0.01f);
    }
    const auto loggedAttack = NewAttackLog(before, after);
    if (!hint && loggedAttack)
      hint = loggedAttack;
    // Exige un ataque nuevo y pérdida real de HP rival cuando StepAI termina la ronda y borra la marca attacked.
    if (events.empty() && !attacker && loggedAttack && enemyHurt) {
      attacker = InferAttacker(before, after, *loggedAttack);
      if (attacker)
        oldAttacker = before.Find(attacker->id);
    }
    if (movement)
      Play("move");
    if (attacker) {
      switch (attacker->type) {
      case UnitClass::Archer:
        Play("arrow");
        break;
      case UnitClass::Lancer:
        Play("spear");
        break;
      case UnitClass::Rider:
        Play("charge");
        break;
      case UnitClass::Berserker:
        Play("axe");
        break;
      case UnitClass::Assassin:
        Play("dagger");
        break;
      case UnitClass::Caster: {
        bool ice = false;
        for (const auto &u : b.units) {
          const auto *old = before.Find(u.id);
          if (old && u.slowThroughRound > old->slowThroughRound)
            ice = true;
        }
        const bool began = !oldAttacker->activated && (attacker->activated || b.round > a.round || b.activationSerial[attacker->owner] > a.activationSerial[attacker->owner]);
        const int startingMana = began ? std::min(after.config.maxMana, oldAttacker->mp + after.config.manaRecovery) : oldAttacker->mp;
        const int spent = startingMana - attacker->mp;
        if (hint == Ability::Fire)
          Play("spell_fire");
        else if (hint == Ability::Ice)
          Play("spell_ice");
        else if (hint == Ability::Lightning)
          Play("spell_lightning");
        else
          Play(ice ? "spell_ice" : (spent >= 3 ? "spell_lightning" : (spent > 0 ? "spell_fire" : "spell_support")));
        break;
      }
      default:
        Play("sword");
        break;
      }
    }
    if (damage && events.empty())
      Play("impact");
    if (death && events.empty())
      Play("death");
    if (healing && !itemUsed)
      Play("spell_heal");
    for (const auto &u : b.units)
      if (const auto *old = before.Find(u.id)) {
        if (u.inventory.size() > old->inventory.size())
          Play("buy");
        else if (u.inventory.size() < old->inventory.size() && b.sources.size() >= a.sources.size())
          Play("item");
        if ((u.guard && !old->guard) || (u.smoke && !old->smoke) || (u.pike && !old->pike) || (u.precise && !old->precise) || (u.marked && !old->marked))
          Play("ability");
      }
    for (const auto &cell : b.cells)
      if (const auto *old = before.At(cell.pos)) {
        if (cell.owner != old->owner)
          Play(cell.building == Building::Fortress ? "capture_fortress" : "capture_house");
        else if (cell.captureProgress > old->captureProgress)
          Play("capture_fortress");
        if (cell.building == Building::Outpost && old->building != Building::Outpost)
          Play("build");
      }
    if (b.sources.size() > a.sources.size())
      Play("rot_birth");
    if (b.sources.size() < a.sources.size() && b.round == a.round)
      Play("purify");
    if (b.corpses.size() < a.corpses.size() && b.sources.size() <= a.sources.size() && b.round == a.round)
      Play("incinerate");
    if (b.globalRemaining > 0 && a.globalRemaining == 0)
      Play("notice");
    if (b.round > a.round)
      Play("round");
    if (b.currentPlayer != a.currentPlayer || b.activationSerial != a.activationSerial) {
      Play("turn");
      if (b.players[b.currentPlayer].coins > a.players[b.currentPlayer].coins)
        Play("income");
    }
    if (b.winner >= 0 && a.winner < 0)
      SetScene(b.mode == Mode::VersusAI && b.winner == 1 ? "defeat" : "victory");
  }
} // Espacio de nombres Plaguefront
