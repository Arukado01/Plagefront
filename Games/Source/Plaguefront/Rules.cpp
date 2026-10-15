#include "Plaguefront/Rules.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <queue>
#include <ranges>
#include <set>
#include <tuple>
#include <utility>

namespace Plaguefront {
  namespace {
    constexpr int impassable = 9999;
    constexpr float rootThree = 1.7320508075688772F;
    template <class T>
    constexpr auto index(T value) { return static_cast<std::size_t>(value); }
    Result Yes(std::string message) { return {true, std::move(message)}; }
    Result No(std::string message) { return {false, std::move(message)}; }
    bool ValidClass(UnitClass type) { return index(type) < 7; }
    bool RangedAbility(const Unit &unit, Ability ability) {
      return ability == Ability::Mark || ability == Ability::Fire || ability == Ability::Ice ||
             ability == Ability::Lightning || ability == Ability::Heal ||
             ((ability == Ability::Attack || ability == Ability::Focus) &&
              (unit.type == UnitClass::Archer || unit.type == UnitClass::Caster));
    }
    bool Spell(const Unit &unit, Ability ability) {
      return ability == Ability::Fire || ability == Ability::Ice || ability == Ability::Lightning ||
             (ability == Ability::Attack && unit.type == UnitClass::Caster);
    }
    int ManaCost(Ability ability, const Config &config) {
      switch (ability) {
      case Ability::Fire:
      case Ability::Ice:
        return 2;
      case Ability::Lightning:
      case Ability::Heal:
        return 3;
      case Ability::Purify:
        return config.purifyMana;
      default:
        return 0;
      }
    }
    bool HasAdvantage(UnitClass attacker, UnitClass defender) {
      constexpr std::array<UnitClass, 7> prey{UnitClass::Assassin, UnitClass::Lancer, UnitClass::Rider,
                                              UnitClass::Archer, UnitClass::Berserker, UnitClass::Caster, UnitClass::Saber};
      return ValidClass(attacker) && prey[index(attacker)] == defender;
    }
    Hex RoundedCube(float q, float r) {
      const float s = -q - r;
      int qi = static_cast<int>(std::round(q));
      int ri = static_cast<int>(std::round(r));
      const int si = static_cast<int>(std::round(s));
      const float dq = std::abs(static_cast<float>(qi) - q);
      const float dr = std::abs(static_cast<float>(ri) - r);
      const float ds = std::abs(static_cast<float>(si) - s);
      if (dq > dr && dq > ds)
        qi = -ri - si;
      else if (dr > ds)
        ri = -qi - si;
      return {qi, ri};
    }
    bool Eligible(const Unit &unit, int player, int round) {
      return unit.Alive() && unit.owner == player && unit.readyRound <= round && !unit.activated;
    }
  } // espacio de nombres anónimo

  int Distance(Hex a, Hex b) {
    return (std::abs(a.q - b.q) + std::abs(a.r - b.r) + std::abs(a.q + a.r - b.q - b.r)) / 2;
  }
  std::array<Hex, 6> Neighbors(Hex h) {
    return {{{h.q + 1, h.r}, {h.q + 1, h.r - 1}, {h.q, h.r - 1}, {h.q - 1, h.r}, {h.q - 1, h.r + 1}, {h.q, h.r + 1}}};
  }
  std::vector<Hex> HexLine(Hex a, Hex b) {
    const int length = Distance(a, b);
    std::vector<Hex> line;
    for (int i = 0; i <= length; ++i) {
      const float t = length == 0 ? 0.F : static_cast<float>(i) / static_cast<float>(length);
      // Desplazamientos iguales resuelven de forma determinista las líneas entre dos celdas.
      line.push_back(RoundedCube(static_cast<float>(a.q) + static_cast<float>(b.q - a.q) * t + 0.00001F,
                                 static_cast<float>(a.r) + static_cast<float>(b.r - a.r) * t + 0.00001F));
    }
    return line;
  }
  std::array<float, 2> HexToPixel(Hex h, float radius) {
    return {radius * rootThree * (static_cast<float>(h.q) + static_cast<float>(h.r) * 0.5F),
            radius * 1.5F * static_cast<float>(h.r)};
  }
  Hex PixelToHex(float x, float y, float radius) {
    if (radius <= 0.F)
      return {};
    return RoundedCube((rootThree / 3.F * x - y / 3.F) / radius, (2.F / 3.F * y) / radius);
  }

  std::string Name(UnitClass value) {
    constexpr std::array names{"Saber", "Archer", "Lancer", "Rider", "Caster", "Assassin", "Berserker"};
    return index(value) < names.size() ? names[index(value)] : "Clase desconocida";
  }
  std::string Name(Terrain value) {
    constexpr std::array names{"Llanura", "Bosque", "Colina", "Pantano", "Ruinas", "Camino", "Rio", "Puente", "Agua profunda", "Inaccesible"};
    return index(value) < names.size() ? names[index(value)] : "Terreno desconocido";
  }
  std::string Name(Weather value) {
    constexpr std::array names{"Despejado", "Lluvia", "Niebla", "Tormenta"};
    return index(value) < names.size() ? names[index(value)] : "Clima desconocido";
  }
  std::string Name(Item value) {
    constexpr std::array names{"Pocion", "Pocion grande", "Pocion de mana", "Antidoto", "Kit purificador"};
    return index(value) < names.size() ? names[index(value)] : "Objeto desconocido";
  }
  std::string Name(Ability value) {
    constexpr std::array names{"Ataque", "Parada", "Corte preciso", "Marcar objetivo", "Disparo concentrado",
                               "Formacion de picas", "Estocada de carga", "Repliegue montado", "Fire", "Ice", "Lightning", "Heal",
                               "Humo", "Golpe furioso", "Purificacion", "Incinerar cadaver"};
    return index(value) < names.size() ? names[index(value)] : "Habilidad desconocida";
  }

  Config::Config() : classes{{{28, 9, 5, 2, 3, 4, 1, 1, 45}, {22, 7, 2, 1, 3, 3, 2, 4, 35}, {27, 9, 4, 1, 2, 3, 1, 1, 45}, {30, 9, 3, 1, 2, 5, 1, 1, 60}, {20, 4, 2, 8, 4, 3, 2, 3, 55}, {21, 10, 2, 2, 3, 5, 1, 1, 50}, {34, 11, 4, 0, 1, 3, 1, 1, 55}}} {}

  Match::Match(Config rules) : config(std::move(rules)) {
    state.players = {{{"Jugador 1", config.initialCoins}, {"Jugador 2", config.initialCoins}}};
  }
  Cell *Match::At(Hex pos) {
    const auto found = std::ranges::find(state.cells, pos, &Cell::pos);
    return found == state.cells.end() ? nullptr : &*found;
  }
  const Cell *Match::At(Hex pos) const {
    const auto found = std::ranges::find(state.cells, pos, &Cell::pos);
    return found == state.cells.end() ? nullptr : &*found;
  }
  Unit *Match::Find(int id) {
    const auto found = std::ranges::find(state.units, id, &Unit::id);
    return found == state.units.end() ? nullptr : &*found;
  }
  const Unit *Match::Find(int id) const {
    const auto found = std::ranges::find(state.units, id, &Unit::id);
    return found == state.units.end() ? nullptr : &*found;
  }
  Unit *Match::Occupant(Hex pos) {
    const auto found = std::ranges::find_if(state.units, [pos](const Unit &unit) { return unit.Alive() && unit.pos == pos; });
    return found == state.units.end() ? nullptr : &*found;
  }
  const Unit *Match::Occupant(Hex pos) const {
    const auto found = std::ranges::find_if(state.units, [pos](const Unit &unit) { return unit.Alive() && unit.pos == pos; });
    return found == state.units.end() ? nullptr : &*found;
  }
  void Match::Log(std::string text) {
    state.log.push_back(std::move(text));
    if (state.log.size() > 160)
      state.log.erase(state.log.begin(), state.log.begin() + 40);
  }

  Stats Match::EffectiveStats(const Unit &unit) const {
    if (!ValidClass(unit.type))
      return {};
    auto stats = config.classes[index(unit.type)];
    if (const Cell *cell = At(unit.pos)) {
      if (cell->terrain == Terrain::Forest) {
        stats.defense += config.forestDefense;
        if (unit.type == UnitClass::Archer)
          stats.attack += config.forestArcherAttack;
      }
      if (cell->terrain == Terrain::Ruins)
        stats.defense += config.ruinsDefense;
    }
    if (unit.mudUntilActivation > 0)
      stats.defense -= config.swampDefensePenalty;
    if (unit.pike)
      stats.defense += config.pikeBonus;
    if (unit.type == UnitClass::Berserker && unit.hp * 2 <= stats.hp) {
      stats.attack += config.frenzyAttack;
      stats.defense -= config.frenzyDefensePenalty;
    }
    // Ice afecta la siguiente ronda completa, aunque el objetivo ya hubiera terminado su activación al recibirlo.
    if (unit.slowThroughRound == state.round)
      --stats.movement;
    if (state.weather == Weather::Storm && unit.type == UnitClass::Rider)
      stats.movement -= config.stormRiderPenalty;
    if (stats.maxRange > 1 && (state.weather == Weather::Fog ||
                               (state.weather == Weather::Storm && unit.type == UnitClass::Archer)))
      stats.maxRange = std::max(stats.minRange, stats.maxRange - config.weatherRangePenalty);
    stats.defense = std::max(0, stats.defense);
    stats.movement = std::max(1, stats.movement);
    return stats;
  }
  int Match::MoveCost(const Unit &unit, Hex from, Hex to) const {
    const Cell *origin = At(from), *destination = At(to);
    if (!origin || !destination || Distance(from, to) != 1 || index(destination->terrain) >= config.terrainCosts.size())
      return impassable;
    if (destination->terrain == Terrain::DeepWater || destination->terrain == Terrain::Blocked)
      return impassable;
    if (const Unit *blocking = Occupant(to); blocking && blocking->id != unit.id)
      return impassable;
    int cost = std::max(1, config.terrainCosts[index(destination->terrain)]);
    cost += std::max(0, destination->height - origin->height) * config.uphillCost;
    if (unit.type == UnitClass::Rider && destination->terrain == Terrain::Forest)
      cost += config.forestRiderSurcharge;
    if (state.weather == Weather::Rain && (destination->terrain == Terrain::Swamp || destination->height < 0))
      cost += config.rainMoveSurcharge;
    return std::clamp(cost, 1, impassable);
  }

  std::optional<Path> Match::FindPath(int unitId, Hex target) const {
    const Unit *unit = Find(unitId);
    if (!unit || !unit->Alive() || !At(target))
      return std::nullopt;
    if (const Unit *blocker = Occupant(target); blocker && blocker->id != unitId)
      return std::nullopt;
    using Entry = std::pair<int, Hex>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> frontier;
    std::map<Hex, int> cost{{unit->pos, 0}};
    std::map<Hex, Hex> cameFrom;
    frontier.push({0, unit->pos});
    while (!frontier.empty()) {
      const auto [currentCost, current] = frontier.top();
      frontier.pop();
      if (current == target)
        break;
      if (currentCost != cost.at(current))
        continue;
      for (Hex neighbor : Neighbors(current)) {
        const int step = MoveCost(*unit, current, neighbor);
        if (step >= impassable)
          continue;
        const int candidate = currentCost + step;
        if (!cost.contains(neighbor) || candidate < cost.at(neighbor)) {
          cost[neighbor] = candidate;
          cameFrom[neighbor] = current;
          frontier.push({candidate, neighbor});
        }
      }
    }
    if (!cost.contains(target))
      return std::nullopt;
    Path path{{target}, cost.at(target)};
    while (path.cells.back() != unit->pos)
      path.cells.push_back(cameFrom.at(path.cells.back()));
    std::ranges::reverse(path.cells);
    return path;
  }
  std::map<Hex, int> Match::Reachable(int unitId) const {
    const Unit *unit = Find(unitId);
    if (!unit || !unit->Alive() || unit->moveLocked || (unit->moved && unit->retreatRemaining <= 0))
      return {};
    const int budget = unit->retreatRemaining > 0 ? unit->retreatRemaining : EffectiveStats(*unit).movement;
    using Entry = std::pair<int, Hex>;
    std::priority_queue<Entry, std::vector<Entry>, std::greater<>> frontier;
    std::map<Hex, int> cost{{unit->pos, 0}};
    frontier.push({0, unit->pos});
    while (!frontier.empty()) {
      const auto [spent, current] = frontier.top();
      frontier.pop();
      if (spent != cost.at(current))
        continue;
      for (Hex neighbor : Neighbors(current)) {
        const int step = MoveCost(*unit, current, neighbor);
        const int candidate = spent + step;
        if (step >= impassable || candidate > budget)
          continue;
        if (!cost.contains(neighbor) || candidate < cost.at(neighbor)) {
          cost[neighbor] = candidate;
          frontier.push({candidate, neighbor});
        }
      }
    }
    return cost;
  }
  bool Match::LineOfSight(Hex from, Hex to) const {
    const Cell *origin = At(from), *destination = At(to);
    if (!origin || !destination)
      return false;
    const auto line = HexLine(from, to);
    for (std::size_t i = 1; i + 1 < line.size(); ++i) {
      const Cell *cell = At(line[i]);
      if (!cell || cell->terrain == Terrain::Blocked || cell->terrain == Terrain::Ruins)
        return false;
      const float t = static_cast<float>(i) / static_cast<float>(line.size() - 1);
      const float eyeHeight = 1.F + static_cast<float>(origin->height) + static_cast<float>(destination->height - origin->height) * t;
      if (static_cast<float>(cell->height) > eyeHeight)
        return false;
    }
    return true;
  }

  std::vector<Ability> Match::Abilities(int unitId) const {
    const Unit *unit = Find(unitId);
    if (!unit || !unit->Alive())
      return {};
    std::vector<Ability> abilities{Ability::Attack};
    switch (unit->type) {
    case UnitClass::Saber:
      abilities.insert(abilities.end(), {Ability::Guard, Ability::Precise});
      break;
    case UnitClass::Archer:
      abilities.insert(abilities.end(), {Ability::Mark, Ability::Focus});
      break;
    case UnitClass::Lancer:
      abilities.insert(abilities.end(), {Ability::Pike, Ability::Thrust});
      break;
    case UnitClass::Rider:
      abilities.push_back(Ability::Retreat);
      break;
    case UnitClass::Caster:
      abilities.insert(abilities.end(), {Ability::Fire, Ability::Ice, Ability::Lightning, Ability::Heal, Ability::Purify});
      break;
    case UnitClass::Assassin:
      abilities.push_back(Ability::Smoke);
      break;
    case UnitClass::Berserker:
      abilities.push_back(Ability::Furious);
      break;
    }
    abilities.push_back(Ability::Incinerate);
    return abilities;
  }
  bool Match::CanTarget(int attackerId, int targetId, Ability ability) const {
    const Unit *attacker = Find(attackerId), *target = Find(targetId);
    if (!attacker || !target || !attacker->Alive() || !target->Alive())
      return false;
    const auto allowed = Abilities(attackerId);
    if (std::ranges::find(allowed, ability) == allowed.end())
      return false;
    if (index(ability) >= attacker->cooldowns.size() || attacker->cooldowns[index(ability)] > 0 || attacker->mp < ManaCost(ability, config))
      return false;
    if (ability == Ability::Heal) {
      if (attacker->owner != target->owner || target->hp >= config.classes[index(target->type)].hp)
        return false;
    } else if (attacker->owner == target->owner)
      return false;
    if (ability == Ability::Guard || ability == Ability::Precise || ability == Ability::Pike || ability == Ability::Smoke ||
        ability == Ability::Retreat || ability == Ability::Purify || ability == Ability::Incinerate)
      return false;
    const Stats stats = EffectiveStats(*attacker);
    const int distance = Distance(attacker->pos, target->pos);
    int minimum = stats.minRange, maximum = stats.maxRange;
    if (ability == Ability::Heal)
      minimum = 0;
    if (ability == Ability::Thrust) {
      minimum = 1;
      maximum = 2;
      const Hex delta{target->pos.q - attacker->pos.q, target->pos.r - attacker->pos.r};
      if (delta.q != 0 && delta.r != 0 && delta.q + delta.r != 0)
        return false;
    }
    if (distance < minimum || distance > maximum)
      return false;
    return distance <= 1 || LineOfSight(attacker->pos, target->pos);
  }
  int Match::EstimateDamage(int attackerId, int targetId, Ability ability) const {
    const Unit *attacker = Find(attackerId), *target = Find(targetId);
    if (!attacker || !target || !CanTarget(attackerId, targetId, ability) || ability == Ability::Heal || ability == Ability::Mark)
      return 0;
    return CalculateDamage(*attacker, *target, ability);
  }
  int Match::CalculateDamage(const Unit &attackerUnit, const Unit &targetUnit, Ability ability) const {
    const Unit *attacker = &attackerUnit, *target = &targetUnit;
    const Stats offense = EffectiveStats(*attacker), defense = EffectiveStats(*target);
    const bool magic = Spell(*attacker, ability);
    int attack = magic ? offense.magic : offense.attack;
    if (ability == Ability::Fire)
      attack += config.firePower - (state.weather == Weather::Rain ? 1 : 0);
    if (ability == Ability::Ice)
      attack += config.icePower;
    if (ability == Ability::Lightning)
      attack += config.lightningPower +
                ((state.weather == Weather::Rain || state.weather == Weather::Storm) ? 1 : 0);
    if (ability == Ability::Focus)
      attack += config.focusBonus;
    if (RangedAbility(*attacker, ability)) {
      const Cell *a = At(attacker->pos), *b = At(target->pos);
      if (a && b && a->height > b->height)
        attack += config.rangedHeightAttack;
    }
    int damage = std::max(1, attack - (magic ? defense.resistance : defense.defense));
    if (HasAdvantage(attacker->type, target->type))
      damage += config.advantageDamage;
    if (!magic && attacker->precise)
      damage += config.preciseBonus;
    if (target->marked)
      damage += config.markBonus;
    if (ability == Ability::Thrust)
      damage += config.thrustBonus;
    if (ability == Ability::Furious)
      damage += config.furiousBonus;
    if (attacker->type == UnitClass::Rider && attacker->stepsMoved >= config.chargeSteps)
      damage += config.chargeBonus;
    if (attacker->type == UnitClass::Assassin && Distance(attacker->pos, target->pos) == 1) {
      const Hex opposite{target->pos.q * 2 - attacker->pos.q, target->pos.r * 2 - attacker->pos.r};
      const Unit *ally = Occupant(opposite);
      if (ally && ally->owner == attacker->owner)
        damage += config.flankBonus;
    }
    if (target->smoke || (!magic && target->guard))
      damage = std::max(1, (damage * (100 - std::clamp(config.damageReductionPercent, 0, 100)) + 99) / 100);
    return damage;
  }

  bool Match::Available(const Unit &unit) const { return state.winner < 0 && Eligible(unit, state.currentPlayer, state.round); }
  int Match::AvailableCount(int player) const {
    if (state.winner >= 0)
      return 0;
    if (player < 0)
      player = state.currentPlayer;
    return static_cast<int>(std::ranges::count_if(state.units, [&](const Unit &unit) { return Eligible(unit, player, state.round); }));
  }
  int Match::Income(int player) const {
    if (player < 0 || player > 1)
      return 0;
    int income = 0;
    for (const Cell &cell : state.cells)
      if (cell.owner == player) {
        if (cell.building == Building::Fortress)
          income += config.fortressIncome;
        else if (cell.building == Building::House)
          income += config.houseIncome;
      }
    return income;
  }
  int Match::CaptureRequired(Building building) const {
    switch (building) {
    case Building::House:
      return config.houseCapture;
    case Building::Outpost:
      return config.outpostCapture;
    case Building::Fortress:
      return config.fortressCapture;
    default:
      return 0;
    }
  }
  int Match::CounterDamage(int defenderId, int attackerId) const {
    const Unit *defender = Find(defenderId);
    const int base = EstimateDamage(defenderId, attackerId, Ability::Attack);
    if (!defender || base <= 0)
      return 0;
    const int maximum = config.classes[index(defender->type)].hp;
    return std::max(1, (base * defender->hp + maximum - 1) / maximum);
  }

  Result Match::CheckActive(int unitId, bool needsAction) const {
    const Unit *unit = Find(unitId);
    if (state.winner >= 0)
      return No("La partida ha terminado.");
    if (!unit || !unit->Alive() || unit->owner != state.currentPlayer || state.activeUnit != unitId || unit->activated || !unit->activationStarted)
      return No("Activa una unidad propia antes de dar la orden.");
    if (needsAction && unit->acted)
      return No("La unidad ya utilizo su accion.");
    return Yes("");
  }
  Result Match::BeginActivation(int unitId) {
    if (state.winner >= 0)
      return No("La partida ha terminado.");
    Unit *unit = Find(unitId);
    if (!unit || !Available(*unit))
      return No("Esa unidad no puede activarse ahora.");
    state.activeUnit = unitId;
    if (unit->activationStarted)
      return Yes("Se reanuda la activacion de " + Name(unit->type) + ".");
    unit->activationStarted = true;
    unit->moved = unit->acted = unit->attacked = unit->moveLocked = unit->guard = unit->smoke = unit->pike = false;
    unit->stepsMoved = unit->mudUntilActivation = unit->retreatRemaining = 0;
    if (unit->type == UnitClass::Caster)
      unit->mp = std::min(config.maxMana, unit->mp + config.manaRecovery);
    if (RotDamageAt(unit->pos) > 0)
      Log("Alerta: " + Name(unit->type) + " esta en una zona contaminada.");
    else if (std::ranges::any_of(state.corpses, [&](const Corpse &corpse) {
               return state.round - corpse.deathRound >= config.rotWait && Distance(corpse.pos, unit->pos) <= config.rotRadius;
             }))
      Log("Alerta: un cadaver cercano va a activar la putrefaccion.");
    return Yes("Activacion de " + Name(unit->type) + ".");
  }
  Result Match::Move(int unitId, Hex destination) {
    if (const auto result = CheckActive(unitId, false); !result)
      return result;
    Unit *unit = Find(unitId);
    if (unit->moveLocked)
      return No("El disparo concentrado impide moverse.");
    const bool retreat = unit->retreatRemaining > 0;
    if (!retreat && unit->moved)
      return No("El movimiento ya no esta disponible.");
    if (destination == unit->pos)
      return No("Selecciona otra celda.");
    const auto path = FindPath(unitId, destination);
    const int budget = retreat ? unit->retreatRemaining : EffectiveStats(*unit).movement;
    if (!path || path->cost > budget || (retreat && static_cast<int>(path->cells.size()) - 1 > config.retreatSteps))
      return No("Destino fuera del alcance o bloqueado.");
    for (Cell &cell : state.cells)
      if (cell.captureUnit == unitId) {
        cell.captureUnit = -1;
        Log("Captura en pausa: la unidad abandono el edificio; el progreso se conserva.");
      }
    unit->stepsMoved += static_cast<int>(path->cells.size()) - 1;
    unit->pos = destination;
    unit->moved = true;
    unit->retreatRemaining = 0;
    if (At(destination)->terrain == Terrain::Swamp)
      unit->mudUntilActivation = 1;
    return Yes("Movimiento: " + std::to_string(path->cost) + " puntos.");
  }

  Result Match::UseAbility(int unitId, Ability ability, Hex targetPos) {
    const bool retreat = ability == Ability::Retreat;
    if (const auto result = CheckActive(unitId, !retreat); !result)
      return result;
    Unit *unit = Find(unitId);
    const auto abilities = Abilities(unitId);
    if (std::ranges::find(abilities, ability) == abilities.end())
      return No("La clase no dispone de esta habilidad.");
    if (index(ability) >= unit->cooldowns.size() || unit->cooldowns[index(ability)] > 0)
      return No("La habilidad esta en recarga.");
    const int mana = ManaCost(ability, config);
    if (unit->mp < mana)
      return No("Mana insuficiente.");
    const int eventBudget = ability == Ability::Lightning ? config.lightningMaxTargets + 1 : 2;
    const bool damaging = ability == Ability::Attack || ability == Ability::Focus || ability == Ability::Thrust || ability == Ability::Furious ||
                          ability == Ability::Fire || ability == Ability::Ice || ability == Ability::Lightning;
    if (damaging && state.nextEventId > 100000000 - eventBudget)
      return No("Se agoto el espacio de identificadores de combate; no se ejecuto la orden.");
    if (retreat) {
      if (!unit->attacked)
        return No("Ataca antes de activar el repliegue.");
      unit->retreatRemaining = config.retreatSteps;
    } else if (ability == Ability::Guard)
      unit->guard = true;
    else if (ability == Ability::Precise)
      unit->precise = true;
    else if (ability == Ability::Pike)
      unit->pike = true;
    else if (ability == Ability::Smoke)
      unit->smoke = true;
    else if (ability == Ability::Purify || ability == Ability::Incinerate) {
      const int range = ability == Ability::Purify ? config.purifierRange : 1;
      if (Distance(unit->pos, targetPos) > range || !At(targetPos))
        return No("Objetivo fuera del alcance de purificacion.");
      const auto corpse = std::ranges::find(state.corpses, targetPos, &Corpse::pos);
      const auto source = std::ranges::find(state.sources, targetPos, &Source::pos);
      if (ability == Ability::Incinerate) {
        if (corpse == state.corpses.end())
          return No("No hay un cadaver que incinerar.");
        state.corpses.erase(corpse);
      } else {
        if (source == state.sources.end())
          return No("No hay una fuente activa que purificar.");
        state.sources.erase(source);
      }
      Log(ability == Ability::Incinerate ? "Cadaver incinerado." : "Contaminacion purificada.");
    } else {
      Unit *target = Occupant(targetPos);
      if (!target || !CanTarget(unitId, target->id, ability))
        return No("Objetivo invalido: comprueba bando, alcance y linea de vision.");
      const int targetId = target->id;
      if (ability == Ability::Heal) {
        target->hp = std::min(config.classes[index(target->type)].hp, target->hp + config.healAmount);
        Log("Heal recupera vida de " + Name(target->type) + ".");
      } else if (ability == Ability::Mark)
        target->marked = true;
      else {
        // Conserva la orden completa, incluidas cadenas largas; los observadores ya reconocen las anteriores por su ID.
        state.combatEvents.clear();
        if (ability == Ability::Focus)
          unit->moveLocked = true;
        unit->attacked = true;
        ResolveHit(unitId, targetId, ability);
        if (ability == Ability::Fire)
          Ignite(targetPos, unit->owner);
        if (ability == Ability::Lightning) {
          // Cada enlace parte del último impacto y resuelve distancia, visión y empates por ID de forma determinista.
          std::set<int> hit{targetId};
          Hex previous = targetPos;
          for (int chain = 1; chain < config.lightningMaxTargets; ++chain) {
            const Unit *next = nullptr;
            for (const Unit &candidate : state.units) {
              const int distance = Distance(previous, candidate.pos);
              if (!candidate.Alive() || candidate.owner == unit->owner || hit.contains(candidate.id) ||
                  distance > config.lightningRadius || !LineOfSight(previous, candidate.pos))
                continue;
              if (!next || std::tuple{distance, candidate.id} < std::tuple{Distance(previous, next->pos), next->id})
                next = &candidate;
            }
            if (!next)
              break;
            const int id = next->id;
            const Hex position = next->pos;
            ResolveHit(unitId, id, ability, false, chain, previous);
            hit.insert(id);
            previous = position;
          }
        }
        // Solo responde el objetivo primario; los enlaces secundarios y el daño ambiental no generan contraataques.
        if (CounterDamage(targetId, unitId) > 0)
          ResolveHit(targetId, unitId, Ability::Attack, true);
      }
    }
    unit = Find(unitId);
    unit->mp -= mana;
    if (!retreat)
      unit->acted = true;
    unit->cooldowns[index(ability)] = config.abilityCooldowns[index(ability)];
    return Yes(Name(ability) + " ejecutada.");
  }

  void Match::ResolveHit(int attackerId, int targetId, Ability ability, bool counter, int chainIndex, Hex origin) {
    Unit *attacker = Find(attackerId), *target = Find(targetId);
    if (!attacker || !target || !attacker->Alive() || !target->Alive())
      return;
    const int damage = counter ? CounterDamage(attackerId, targetId) : CalculateDamage(*attacker, *target, ability);
    if (damage <= 0)
      return;
    const int applied = std::min(damage, target->hp);
    const Hex from = chainIndex > 0 ? origin : attacker->pos;
    state.combatEvents.push_back({state.nextEventId++, attackerId, targetId, ability, from, target->pos, applied, counter, chainIndex, damage >= target->hp});
    target->smoke = false;
    if (!Spell(*attacker, ability)) {
      target->guard = false;
      attacker->precise = false;
    }
    target->marked = false;
    if (ability == Ability::Ice)
      target->slowThroughRound = state.round + 1;
    const int owner = attacker->owner;
    Damage(targetId, damage, owner);
    Log(std::string(counter ? "Contraataque" : Name(ability)) + (chainIndex > 0 ? " enlace " + std::to_string(chainIndex + 1) : "") +
        ": " + std::to_string(applied) + " de dano de " + Name(attacker->type) + " a " + Name(target->type) + ".");
  }

  void Match::Ignite(Hex center, int owner) {
    int count = 0;
    for (const Cell &cell : state.cells) {
      if (Distance(center, cell.pos) > config.fireRadius || cell.terrain == Terrain::River || cell.terrain == Terrain::DeepWater ||
          cell.terrain == Terrain::Swamp || cell.terrain == Terrain::Blocked)
        continue;
      const auto existing = std::ranges::find(state.fires, cell.pos, &FireZone::pos);
      if (existing != state.fires.end()) {
        existing->remaining = std::max(existing->remaining, config.fireDuration);
        existing->owner = owner;
      } else
        state.fires.push_back({state.nextId++, cell.pos, config.fireDuration, owner});
      ++count;
    }
    if (count > 0)
      Log("Terreno incendiado: " + std::to_string(count) + " celdas durante " + std::to_string(config.fireDuration) + " finales de ronda.");
  }

  void Match::Damage(int unitId, int amount, int killerOwner) {
    if (state.winner >= 0)
      return;
    Unit *unit = Find(unitId);
    if (!unit || !unit->Alive() || amount <= 0)
      return;
    unit->hp = std::max(0, unit->hp - amount);
    if (unit->Alive())
      return;
    state.corpses.push_back({state.nextId++, unit->pos, state.round, unit->owner, unit->type, unit->id});
    if (state.activeUnit == unitId)
      state.activeUnit = -1;
    if (killerOwner >= 0 && killerOwner < 2 && killerOwner != unit->owner)
      state.players[static_cast<std::size_t>(killerOwner)].coins += config.killReward;
    for (Cell &cell : state.cells)
      if (cell.captureUnit == unitId) {
        cell.captureUnit = -1;
        Log("Captura en pausa: el captor ha caido; el progreso se conserva.");
      }
    Log(Name(unit->type) + " ha caido. Cadaver registrado en ronda " + std::to_string(state.round) + ".");
  }
  Result Match::EndActivation() {
    if (state.winner >= 0)
      return No("La partida ha terminado.");
    if (state.activeUnit < 0)
      return No("No hay activacion en curso.");
    Unit *unit = Find(state.activeUnit);
    if (unit) {
      unit->retreatRemaining = 0;
      unit->activated = true;
    }
    state.activeUnit = -1;
    return Yes("Activacion finalizada.");
  }
  void Match::AdvanceTurn() {
    if (state.winner >= 0)
      return;
    const int previous = state.currentPlayer;
    if (previous == 1)
      EndRound();
    StartTurn(1 - previous);
  }
  void Match::StartTurn(int player) {
    state.currentPlayer = player;
    state.activeUnit = -1;
    for (Unit &unit : state.units)
      if (unit.owner == player) {
        unit.activated = unit.activationStarted = false;
        unit.moved = unit.acted = unit.attacked = unit.moveLocked = false;
        unit.stepsMoved = unit.retreatRemaining = 0;
      }
    const int income = Income(player);
    state.players[static_cast<std::size_t>(player)].coins += income;
    Log("Turno de " + state.players[static_cast<std::size_t>(player)].name + ": +" + std::to_string(income) + " monedas de ingresos.");
  }
  Result Match::EndTurn() {
    if (state.winner >= 0)
      return No("La partida ha terminado.");
    state.activeUnit = -1;
    for (Unit &unit : state.units)
      if (unit.owner == state.currentPlayer) {
        if (Eligible(unit, state.currentPlayer, state.round))
          unit.activated = true;
        unit.retreatRemaining = 0;
      }
    ++state.activationSerial[static_cast<std::size_t>(state.currentPlayer)];
    AdvanceTurn();
    return Yes("Turno de ejercito finalizado.");
  }
  Result Match::Pass() { return EndTurn(); }

  Result Match::Capture(int unitId) {
    if (const auto result = CheckActive(unitId, true); !result)
      return result;
    Unit *unit = Find(unitId);
    Cell *cell = At(unit->pos);
    const bool restoring = cell && cell->owner == unit->owner && cell->capturingPlayer >= 0 && cell->capturingPlayer != unit->owner && cell->captureProgress > 0;
    if (!cell || cell->building == Building::None || (cell->owner == unit->owner && !restoring))
      return No("La celda no contiene un edificio capturable o disputado.");
    int strength = unit->hp;
    if (cell->captureProgress > 0 && cell->capturingPlayer != unit->owner) {
      const int removed = std::min(strength, cell->captureProgress);
      cell->captureProgress -= removed;
      strength -= removed;
      Log("Captura disputada: se eliminan " + std::to_string(removed) + " puntos rivales.");
      if (cell->captureProgress == 0) {
        cell->capturingPlayer = -1;
        cell->captureUnit = -1;
      }
    }
    if (!restoring && strength > 0) {
      if (cell->capturingPlayer < 0)
        cell->captureStartedRound = state.round;
      cell->capturingPlayer = unit->owner;
      cell->captureProgress += strength;
      cell->captureUnit = unitId;
    }
    const int required = CaptureRequired(*cell);
    if (!restoring && cell->capturingPlayer == unit->owner && cell->captureProgress >= required) {
      if (cell->building == Building::House && cell->owner < 0 && !cell->neutralRewardClaimed) {
        state.players[static_cast<std::size_t>(unit->owner)].coins += config.neutralReward;
        cell->neutralRewardClaimed = true;
      }
      const bool victory = cell->building == Building::Fortress && cell->owner != unit->owner;
      cell->owner = unit->owner;
      cell->captureUnit = -1;
      cell->captureProgress = 0;
      cell->capturingPlayer = -1;
      cell->captureStartedRound = cell->captureDefenderSerial = 0;
      Log("Edificio capturado por " + state.players[static_cast<std::size_t>(unit->owner)].name + ".");
      if (victory) {
        state.winner = unit->owner;
        state.activeUnit = -1;
        Log("Fortaleza conquistada. Victoria de " + state.players[static_cast<std::size_t>(unit->owner)].name + ".");
      }
    } else if (cell->captureProgress > 0) {
      Log("Captura parcial: " + std::to_string(cell->captureProgress) + "/" + std::to_string(required) + ". Puede continuar en el siguiente turno.");
    } else {
      cell->captureStartedRound = cell->captureDefenderSerial = 0;
      Log("Edificio asegurado: no quedan puntos de captura enemigos.");
    }
    unit->acted = true;
    return Yes(state.winner >= 0 ? "Victoria: fortaleza enemiga capturada." : "Captura ejecutada.");
  }
  Result Match::BuildOutpost(int unitId) {
    if (const auto result = CheckActive(unitId, true); !result)
      return result;
    Unit *unit = Find(unitId);
    Cell *cell = At(unit->pos);
    if (!cell || (cell->building != Building::None && !(cell->building == Building::House && cell->owner == unit->owner)))
      return No("Construye sobre una celda libre o una casa propia.");
    const bool onHouse = cell->building == Building::House && cell->owner == unit->owner;
    if (cell->captureProgress > 0)
      return No("Asegura la captura disputada antes de transformar el edificio.");
    const bool nearFortress = std::ranges::any_of(state.cells, [&](const Cell &other) {
      return other.building == Building::Fortress && other.owner == unit->owner && Distance(other.pos, unit->pos) <= 2;
    });
    if (!onHouse && !nearFortress)
      return No("El fuerte requiere una casa propia o estar a distancia 2 de la fortaleza propia.");
    Player &player = state.players[static_cast<std::size_t>(unit->owner)];
    if (player.coins < config.outpostCost)
      return No("Monedas insuficientes.");
    player.coins -= config.outpostCost;
    cell->building = Building::Outpost;
    cell->owner = unit->owner;
    unit->acted = true;
    Log("Fuerte avanzado construido.");
    return Yes("Fuerte construido; permite reclutar y comprar objetos.");
  }
  Result Match::Recruit(int player, UnitClass type, Hex building) {
    if (state.winner >= 0 || player < 0 || player > 1 || player != state.currentPlayer)
      return No("Solo puede reclutar el jugador activo.");
    if (!ValidClass(type))
      return No("Clase desconocida.");
    const Cell *cell = At(building);
    if (!cell || cell->owner != player || (cell->building != Building::Fortress && cell->building != Building::Outpost))
      return No("Recluta desde una fortaleza o fuerte propio.");
    if (const Unit *occupant = Occupant(building); occupant && occupant->owner != player)
      return No("El edificio esta ocupado por el enemigo.");
    Player &owner = state.players[static_cast<std::size_t>(player)];
    const Stats &stats = config.classes[index(type)];
    if (owner.coins < stats.cost)
      return No("Monedas insuficientes para " + Name(type) + ".");
    std::vector<Hex> candidates{building};
    for (Hex neighbor : Neighbors(building))
      candidates.push_back(neighbor);
    const auto spawn = std::ranges::find_if(candidates, [&](Hex pos) {
      const Cell *spot = At(pos);
      return spot && !Occupant(pos) && spot->terrain != Terrain::DeepWater && spot->terrain != Terrain::Blocked;
    });
    if (spawn == candidates.end())
      return No("No hay una celda libre junto al edificio.");
    Unit unit;
    unit.id = state.nextId++;
    unit.owner = player;
    unit.type = type;
    unit.pos = *spawn;
    unit.hp = stats.hp;
    unit.mp = type == UnitClass::Caster ? config.maxMana : 0;
    unit.readyRound = state.round + 1;
    state.units.push_back(unit);
    owner.coins -= stats.cost;
    Log(Name(type) + " reclutado: disponible en ronda " + std::to_string(unit.readyRound) + ".");
    return Yes("Reclutamiento completado.");
  }
  Result Match::RecruitBattalion(int player, const std::vector<UnitClass> &classes, Hex building) {
    if (classes.empty() || classes.size() > 7)
      return No("El batallon debe contener entre 1 y 7 unidades.");
    // El reclutamiento transaccional evita perder monedas o recibir un batallón parcial si falla una unidad.
    Match candidate = *this;
    for (UnitClass type : classes)
      if (const auto result = candidate.Recruit(player, type, building); !result)
        return result;
    state = std::move(candidate.state);
    return Yes("Batallon reclutado sin descuentos; cada unidad actua individualmente.");
  }
  Result Match::BuyItem(int unitId, Item item) {
    if (state.winner >= 0)
      return No("La partida ha terminado.");
    Unit *unit = Find(unitId);
    if (!unit || !unit->Alive() || unit->owner != state.currentPlayer || index(item) >= config.itemCosts.size())
      return No("Selecciona una unidad propia y un objeto valido.");
    if (unit->inventory.size() >= 2)
      return No("Inventario completo: dos espacios por unidad.");
    const bool shop = std::ranges::any_of(state.cells, [&](const Cell &cell) {
      return cell.owner == unit->owner && (cell.building == Building::Fortress || cell.building == Building::Outpost) && Distance(cell.pos, unit->pos) <= 1;
    });
    if (!shop)
      return No("La tienda requiere una fortaleza o fuerte propio adyacente.");
    Player &owner = state.players[static_cast<std::size_t>(unit->owner)];
    const int cost = config.itemCosts[index(item)];
    if (owner.coins < cost)
      return No("Monedas insuficientes.");
    owner.coins -= cost;
    unit->inventory.push_back(item);
    return Yes(Name(item) + " comprado.");
  }
  Result Match::UseItem(int unitId, int slot, Hex targetPos) {
    if (const auto result = CheckActive(unitId, true); !result)
      return result;
    Unit *unit = Find(unitId);
    if (slot < 0 || static_cast<std::size_t>(slot) >= unit->inventory.size())
      return No("Espacio de inventario vacio.");
    const Item item = unit->inventory[static_cast<std::size_t>(slot)];
    if (index(item) >= config.itemCosts.size())
      return No("Objeto desconocido.");
    if (item == Item::Purifier) {
      if (Distance(unit->pos, targetPos) > config.purifierRange)
        return No("Fuente fuera del alcance del kit.");
      const auto source = std::ranges::find(state.sources, targetPos, &Source::pos);
      if (source == state.sources.end())
        return No("El kit elimina una fuente activa; no hay ninguna en esta celda.");
      state.sources.erase(source);
    } else {
      Unit *target = Occupant(targetPos);
      if (!target || target->owner != unit->owner || Distance(unit->pos, targetPos) > 1)
        return No("Usa el objeto sobre un aliado adyacente o sobre la propia unidad.");
      if (item == Item::Potion || item == Item::LargePotion) {
        const int maximum = config.classes[index(target->type)].hp;
        if (target->hp >= maximum)
          return No("El objetivo ya tiene toda su vida.");
        target->hp = std::min(maximum, target->hp + (item == Item::Potion ? config.potionHeal : config.largePotionHeal));
      } else if (item == Item::ManaPotion) {
        if (target->type != UnitClass::Caster || target->mp >= config.maxMana)
          return No("El objetivo no necesita mana.");
        target->mp = std::min(config.maxMana, target->mp + config.manaPotionRestore);
      } else if (item == Item::Antidote) {
        if (target->slowThroughRound < state.round && target->mudUntilActivation <= 0 && !target->marked)
          return No("El objetivo no tiene estados compatibles.");
        target->slowThroughRound = 0;
        target->mudUntilActivation = 0;
        target->marked = false;
      }
    }
    unit->inventory.erase(unit->inventory.begin() + slot);
    unit->acted = true;
    return Yes(Name(item) + " utilizado.");
  }

  int Match::RotDamageAt(Hex pos) const {
    if (std::ranges::any_of(state.sources, [&](const Source &source) {
          return source.remaining > 0 && Distance(source.pos, pos) <= config.rotRadius;
        }))
      return config.rotDamage;
    return state.globalRemaining > 0 ? config.globalDamage : 0;
  }
  int Match::FireDamageAt(Hex pos) const {
    if (!std::ranges::any_of(state.fires, [pos](const FireZone &fire) { return fire.remaining > 0 && fire.pos == pos; }))
      return 0;
    return std::max(1, config.fireDamage - (state.weather == Weather::Rain ? 1 : 0));
  }
  int Match::LargestSourceGroup() const {
    std::set<int> visited;
    int largest = 0;
    for (const Source &source : state.sources) {
      if (source.remaining <= 0 || visited.contains(source.id))
        continue;
      std::vector<const Source *> pending{&source};
      visited.insert(source.id);
      int count = 0;
      while (!pending.empty()) {
        const Source *current = pending.back();
        pending.pop_back();
        ++count;
        for (const Source &candidate : state.sources) {
          if (candidate.remaining > 0 && !visited.contains(candidate.id) && Distance(current->pos, candidate.pos) <= config.connectionDistance) {
            visited.insert(candidate.id);
            pending.push_back(&candidate);
          }
        }
      }
      largest = std::max(largest, count);
    }
    return largest;
  }
  void Match::EndRound() {
    if (state.winner >= 0)
      return;
    state.activeUnit = -1;
    const bool connected = LargestSourceGroup() >= config.globalThreshold;
    if (connected) {
      if (state.globalRemaining == 0)
        Log("ALERTA: contaminacion global. Las fuentes forman un grupo conectado.");
      state.globalRemaining = config.globalDuration;
    }
    // Fotografía el daño antes de aplicarlo para que las muertes sean simultáneas y sus cadáveres no actúen en este ciclo.
    std::vector<std::pair<int, int>> damage;
    for (const Unit &unit : state.units)
      if (unit.Alive()) {
        const int amount = RotDamageAt(unit.pos) + FireDamageAt(unit.pos);
        if (amount > 0)
          damage.emplace_back(unit.id, amount);
      }
    for (const auto &[id, amount] : damage) {
      Damage(id, amount);
      Log("Dano ambiental: " + std::to_string(amount) + " HP a unidad " + std::to_string(id) + ".");
    }
    for (FireZone &fire : state.fires)
      --fire.remaining;
    std::erase_if(state.fires, [](const FireZone &fire) { return fire.remaining <= 0; });
    for (Source &source : state.sources)
      --source.remaining;
    std::erase_if(state.sources, [](const Source &source) { return source.remaining <= 0; });
    if (!connected && state.globalRemaining > 0)
      --state.globalRemaining;

    // Una muerte en R excluye ese cierre: espera R+1 a R+3 y, con la configuración inicial, causa daño de R+4 a R+8.
    for (const Corpse &corpse : state.corpses)
      if (state.round - corpse.deathRound >= config.rotWait) {
        state.sources.push_back({corpse.id, corpse.pos, config.rotDuration});
        Log("Un cadaver se convierte en fuente; su primer dano llegara al final de la proxima ronda.");
      }
    std::erase_if(state.corpses, [&](const Corpse &corpse) {
      return state.round - corpse.deathRound >= config.rotWait;
    });
    if (LargestSourceGroup() >= config.globalThreshold) {
      if (state.globalRemaining == 0)
        Log("ALERTA: contaminacion global. Las fuentes forman un grupo conectado.");
      state.globalRemaining = config.globalDuration;
    }
    for (Unit &unit : state.units) {
      for (int &cooldown : unit.cooldowns)
        cooldown = std::max(0, cooldown - 1);
    }
    ++state.round;
    if (config.weatherPeriod > 0 && (state.round - 1) % config.weatherPeriod == 0) {
      // La semilla local conserva un clima determinista y reproducible entre escenarios y guardados.
      state.weatherSeed = static_cast<int>((static_cast<unsigned>(state.weatherSeed) * 1664525U + 1013904223U) & 0x7fffffffU);
      const int offset = 1 + state.weatherSeed % 3;
      state.weather = static_cast<Weather>((static_cast<int>(state.weather) + offset) % 4);
      Log("Cambio de clima: " + Name(state.weather) + ".");
    }
    // Conserva el turno del ejército eliminado para que pueda gastar sus ingresos y pasarlo si aún no hay reclutas listos.
    Log("Comienza la ronda " + std::to_string(state.round) + ".");
  }
} // espacio de nombres Plaguefront
