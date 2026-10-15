#pragma once

#include <array>
#include <compare>
#include <string>
#include <vector>

namespace Plaguefront {
  struct Hex {
    int q{}, r{};
    auto operator<=>(const Hex &) const = default;
  };
  enum class UnitClass { Saber,
                         Archer,
                         Lancer,
                         Rider,
                         Caster,
                         Assassin,
                         Berserker };
  enum class Terrain { Plain,
                       Forest,
                       Hill,
                       Swamp,
                       Ruins,
                       Road,
                       River,
                       Bridge,
                       DeepWater,
                       Blocked };
  enum class Building { None,
                        House,
                        Fortress,
                        Outpost };
  enum class Weather { Clear,
                       Rain,
                       Fog,
                       Storm };
  enum class Mode { VersusAI,
                    Local };
  enum class Difficulty { Easy,
                          Normal,
                          Hard };
  enum class Item { Potion,
                    LargePotion,
                    ManaPotion,
                    Antidote,
                    Purifier };
  enum class Ability { Attack,
                       Guard,
                       Precise,
                       Mark,
                       Focus,
                       Pike,
                       Thrust,
                       Retreat,
                       Fire,
                       Ice,
                       Lightning,
                       Heal,
                       Smoke,
                       Furious,
                       Purify,
                       Incinerate };
  struct Stats {
    int hp{}, attack{}, defense{}, magic{}, resistance{}, movement{}, minRange{}, maxRange{}, cost{};
  };
  struct Cell {
    Hex pos;
    Terrain terrain{Terrain::Plain};
    int height{};
    Building building{Building::None};
    int owner{-1};
    bool neutralRewardClaimed{};
    int captureUnit{-1}, captureStartedRound{}, captureDefenderSerial{};
    int capturingPlayer{-1}, captureProgress{};
  };
  struct Player {
    std::string name;
    int coins{120};
  };
  struct Unit {
    int id{}, owner{};
    UnitClass type{UnitClass::Saber};
    Hex pos;
    int hp{}, mp{}, readyRound{1};
    bool activated{}, moved{}, acted{}, attacked{}, moveLocked{}, guard{}, smoke{}, pike{}, precise{}, marked{};
    bool activationStarted{};
    int stepsMoved{}, slowThroughRound{}, mudUntilActivation{}, retreatRemaining{};
    std::array<int, 17> cooldowns{};
    std::vector<Item> inventory;
    bool Alive() const { return hp > 0; }
  };
  struct Corpse {
    int id{};
    Hex pos;
    int deathRound{};
    int owner{};
    UnitClass type{UnitClass::Saber};
    int unitId{-1};
  };
  struct Source {
    int id{};
    Hex pos;
    int remaining{};
  };
  struct FireZone {
    int id{};
    Hex pos;
    int remaining{}, owner{};
  };
  struct CombatEvent {
    int id{}, attackerId{}, targetId{};
    Ability ability{Ability::Attack};
    Hex from, to;
    int amount{};
    bool counter{};
    int chainIndex{};
    bool lethal{};
  };
  struct GameState {
    std::string scenario{"La frontera gris"};
    std::array<Player, 2> players;
    std::vector<Cell> cells;
    std::vector<Unit> units;
    std::vector<Corpse> corpses;
    std::vector<Source> sources;
    std::vector<FireZone> fires;
    // La presentación consume impactos secuenciados sin dirigir ni repetir la lógica de combate.
    std::vector<CombatEvent> combatEvents;
    int nextEventId{1};
    int round{1}, currentPlayer{}, activeUnit{-1}, nextId{1}, winner{-1}, globalRemaining{};
    std::array<int, 2> activationSerial{};
    Weather weather{Weather::Clear};
    Mode mode{Mode::VersusAI};
    Difficulty difficulty{Difficulty::Normal};
    int weatherSeed{1};
    std::vector<std::string> log;
  };
  struct Path {
    std::vector<Hex> cells;
    int cost{};
  };
  struct Result {
    bool ok{};
    std::string message;
    explicit operator bool() const { return ok; }
  };

} // espacio de nombres Plaguefront
