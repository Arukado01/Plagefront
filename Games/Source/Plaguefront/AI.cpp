#include "Plaguefront/Rules.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <set>

namespace Plaguefront {
  namespace {
    struct Decision {
      Hex destination{}, target{};
      Ability ability{Ability::Attack};
      enum class Action { None,
                          Ability,
                          Capture,
                          Item,
                          Build } action{Action::None};
      int slot{-1};
      double score{-1e20};
    };

    // Esta estimación de peligro no sustituye la validación de las órdenes reales en Match.
    double Threat(const Match &m, const Unit &u) {
      double danger = 0;
      for (const auto &enemy : m.state.units)
        if (enemy.Alive() && enemy.owner != u.owner) {
          const auto stats = m.EffectiveStats(enemy);
          const auto distance = Distance(enemy.pos, u.pos);
          if (distance <= stats.maxRange + stats.movement && distance >= stats.minRange)
            danger += std::max(1, stats.attack - m.EffectiveStats(u).defense) * (distance <= stats.maxRange ? 1.0 : 0.35);
        }
      return danger;
    }
    double PositionScore(const Match &m, const Unit &u, Difficulty difficulty) {
      double score = 0;
      int goalDistance = 100000, fortressDistance = 100000;
      bool homeThreat = false;
      const Cell *home = nullptr;
      for (const auto &cell : m.state.cells)
        if (cell.building == Building::Fortress && cell.owner == u.owner)
          home = &cell;
      if (home)
        for (const auto &enemy : m.state.units)
          if (enemy.Alive() && enemy.owner != u.owner && Distance(enemy.pos, home->pos) <= 4)
            homeThreat = true;
      for (const auto &cell : m.state.cells) {
        if (cell.capturingPlayer == u.owner && cell.captureProgress > 0 && cell.pos == u.pos)
          score += 260;
        if (cell.building != Building::None && cell.owner != u.owner) {
          // Las casas conservan su valor económico aunque haya un enemigo más próximo.
          const int distance = Distance(u.pos, cell.pos);
          if (cell.building == Building::Fortress)
            fortressDistance = std::min(fortressDistance, distance);
          else
            goalDistance = std::min(goalDistance, distance);
        }
      }
      if (goalDistance < 100000)
        score += std::max(0, 14 - goalDistance) * 1.7;
      if (fortressDistance < 100000)
        score += std::max(0, 20 - fortressDistance) * 1.1;
      if (homeThreat && home && difficulty != Difficulty::Easy)
        score += std::max(0, 7 - Distance(u.pos, home->pos)) * (difficulty == Difficulty::Hard ? 4.5 : 2.0);
      if (difficulty != Difficulty::Easy) {
        const double danger = Threat(m, u);
        score -= danger * (difficulty == Difficulty::Hard ? 0.65 : 0.25);
        if (danger >= u.hp)
          score -= difficulty == Difficulty::Hard ? 30 : 8;
        score -= (m.RotDamageAt(u.pos) + m.FireDamageAt(u.pos)) * (difficulty == Difficulty::Hard ? 8 : 3);
        const auto *cell = m.At(u.pos);
        if (cell && (cell->terrain == Terrain::Forest || cell->terrain == Terrain::Ruins))
          score += 2;
        if (cell)
          score += cell->height * 0.6;
        if (difficulty == Difficulty::Hard)
          for (const auto &corpse : m.state.corpses)
            if (Distance(corpse.pos, u.pos) <= m.config.rotRadius && m.state.round - corpse.deathRound >= m.config.rotWait - 1)
              score -= 3;
      }
      return score;
    }
    double ActionScore(const Match &before, const Match &after, int owner) {
      double score = 0;
      for (const auto &u : before.state.units) {
        const auto *next = after.Find(u.id);
        if (!next)
          continue;
        const int delta = u.hp - next->hp;
        if (u.owner != owner) {
          score += delta * 2.5;
          if (u.Alive() && !next->Alive())
            score += 25;
        } else
          score -= delta * 2.0;
      }
      score += (static_cast<int>(before.state.sources.size()) - static_cast<int>(after.state.sources.size())) * 12;
      score += (static_cast<int>(before.state.corpses.size()) - static_cast<int>(after.state.corpses.size())) * 4;
      return score;
    }
  } // espacio de nombres anónimo

  bool Match::StepAI() {
    if (state.mode != Mode::VersusAI || state.currentPlayer != 1 || state.winner != -1)
      return false;
    const auto difficulty = state.difficulty;
    if (state.activeUnit == -1) {
      int living = 0;
      for (const auto &u : state.units)
        if (u.owner == 1 && u.Alive())
          ++living;
      const int limit = difficulty == Difficulty::Easy ? 6 : (difficulty == Difficulty::Normal ? 8 : 10);
      if (living < limit && state.units.size() < 500) {
        std::vector<Hex> recruiting;
        for (const auto &c : state.cells)
          if (c.owner == 1 && (c.building == Building::Fortress || c.building == Building::Outpost))
            recruiting.push_back(c.pos);
        const auto type = living % 3 == 0 ? UnitClass::Caster : (living % 3 == 1 ? UnitClass::Lancer : UnitClass::Archer);
        if (state.players[1].coins >= config.classes[static_cast<int>(type)].cost + 20)
          for (const auto &pos : recruiting)
            if (Recruit(1, type, pos))
              break;
      }
      int chosen = -1;
      double priority = -1e20;
      for (const auto &u : state.units)
        if (Available(u)) {
          double value = 0;
          for (const auto &c : state.cells)
            if (c.pos == u.pos && c.capturingPlayer == u.owner && c.captureProgress > 0)
              value += 1000;
          for (const auto &enemy : state.units)
            if (enemy.Alive() && enemy.owner == 0)
              value += std::max(0, 8 - Distance(u.pos, enemy.pos));
          value += u.type == UnitClass::Caster ? 2 : 0;
          if (value > priority) {
            priority = value;
            chosen = u.id;
          }
        }
      if (chosen == -1) {
        EndTurn();
        return true;
      }
      if (!BeginActivation(chosen)) {
        EndTurn();
        return true;
      }
      if (state.winner != -1)
        return true;
    }
    const int id = state.activeUnit;
    const auto *active = Find(id);
    if (!active || active->owner != 1) {
      EndTurn();
      return true;
    }
    if (difficulty != Difficulty::Easy && active->inventory.size() < 2) {
      const bool wounded = active->hp * 2 < config.classes[static_cast<int>(active->type)].hp;
      const bool contaminated = std::any_of(state.sources.begin(), state.sources.end(), [&](const Source &s) { return Distance(active->pos, s.pos) <= config.purifierRange; });
      const Item item = wounded ? Item::Potion : Item::Purifier;
      if ((wounded || contaminated) && state.players[1].coins >= config.itemCosts[static_cast<int>(item)] + 20)
        BuyItem(id, item);
    }
    const Hex start = active->pos;
    std::set<Hex> destinations{start};
    for (const auto &[pos, cost] : Reachable(id)) {
      (void)cost;
      destinations.insert(pos);
    }
    Decision best;
    for (const auto destination : destinations) {
      Match moved = *this;
      if (destination != start && !moved.Move(id, destination))
        continue;
      const auto *unit = moved.Find(id);
      if (!unit || !unit->Alive())
        continue;
      const double base = PositionScore(moved, *unit, difficulty) - (destination == start ? 0 : 0.15);
      if (base > best.score)
        best = {destination, {}, Ability::Attack, Decision::Action::None, -1, base};
      {
        Match captured = moved;
        if (captured.Capture(id)) {
          const auto *cell = moved.At(destination);
          const double score = base + (cell && cell->building == Building::Fortress ? 300 : 65);
          if (score > best.score)
            best = {destination, destination, Ability::Attack, Decision::Action::Capture, -1, score};
        }
      }
      if (difficulty == Difficulty::Hard && state.players[1].coins >= config.outpostCost + 55) {
        const auto *cell = moved.At(destination);
        int nearestBase = 100000;
        for (const auto &baseCell : moved.state.cells)
          if (baseCell.owner == 1 && (baseCell.building == Building::Fortress || baseCell.building == Building::Outpost))
            nearestBase = std::min(nearestBase, Distance(destination, baseCell.pos));
        if (cell && cell->building == Building::House && cell->owner == 1 && nearestBase >= 4) {
          Match built = moved;
          if (built.BuildOutpost(id) && base + 18 > best.score)
            best = {destination, destination, Ability::Attack, Decision::Action::Build, -1, base + 18};
        }
      }
      std::set<Hex> targets{destination};
      for (const auto &u : moved.state.units)
        if (u.Alive())
          targets.insert(u.pos);
      if (difficulty != Difficulty::Easy) {
        for (const auto &s : moved.state.sources)
          targets.insert(s.pos);
        for (const auto &c : moved.state.corpses)
          targets.insert(c.pos);
      }
      for (const auto ability : moved.Abilities(id))
        for (const auto target : targets) {
          Match simulated = moved;
          if (!simulated.UseAbility(id, ability, target))
            continue;
          double benefit = ActionScore(moved, simulated, 1);
          // Preparar una postura defensiva conserva valor cuando no hay un ataque disponible.
          if (benefit == 0) {
            if (ability == Ability::Guard || ability == Ability::Smoke || ability == Ability::Pike)
              benefit = 1.5;
            else if (ability == Ability::Precise || ability == Ability::Mark)
              benefit = 2;
          }
          const double score = base + benefit;
          if (score > best.score)
            best = {destination, target, ability, Decision::Action::Ability, -1, score};
        }
      for (std::size_t slot = 0; slot < unit->inventory.size(); ++slot)
        for (const auto target : targets) {
          Match simulated = moved;
          if (!simulated.UseItem(id, static_cast<int>(slot), target))
            continue;
          const double score = base + ActionScore(moved, simulated, 1) - 1;
          if (score > best.score)
            best = {destination, target, Ability::Attack, Decision::Action::Item, static_cast<int>(slot), score};
        }
    }
    if (best.destination != start)
      Move(id, best.destination);
    if (best.action == Decision::Action::Capture)
      Capture(id);
    else if (best.action == Decision::Action::Ability)
      UseAbility(id, best.ability, best.target);
    else if (best.action == Decision::Action::Item)
      UseItem(id, best.slot, best.target);
    else if (best.action == Decision::Action::Build)
      BuildOutpost(id);
    // Rider puede replegarse tras atacar; busca seguridad respetando las mismas reglas de movimiento y captura.
    active = Find(id);
    if (active && active->type == UnitClass::Rider && active->attacked && difficulty != Difficulty::Easy && active->retreatRemaining == 0)
      UseAbility(id, Ability::Retreat, active->pos);
    active = Find(id);
    if (active && active->retreatRemaining > 0 && difficulty != Difficulty::Easy) {
      Hex retreat = active->pos;
      double score = PositionScore(*this, *active, difficulty);
      for (const auto &[pos, cost] : Reachable(id)) {
        (void)cost;
        Match copy = *this;
        if (copy.Move(id, pos)) {
          const double candidate = PositionScore(copy, *copy.Find(id), difficulty);
          if (candidate > score) {
            score = candidate;
            retreat = pos;
          }
        }
      }
      if (retreat != active->pos)
        Move(id, retreat);
    }
    if (state.activeUnit != -1)
      EndActivation();
    // Cada llamada completa una unidad del mismo ejército y la última activación disponible termina su turno.
    if (state.winner == -1 && AvailableCount() == 0)
      EndTurn();
    return true;
  }
} // espacio de nombres Plaguefront
