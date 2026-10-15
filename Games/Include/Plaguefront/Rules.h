#pragma once

#include "Config/PlaguefrontRulesConfig.h"
#include <map>
#include <optional>

namespace Plaguefront {
  int Distance(Hex a, Hex b);
  std::array<Hex, 6> Neighbors(Hex h);
  std::vector<Hex> HexLine(Hex a, Hex b);
  std::array<float, 2> HexToPixel(Hex h, float radius);
  Hex PixelToHex(float x, float y, float radius);

  std::string Name(UnitClass value);
  std::string Name(Terrain value);
  std::string Name(Weather value);
  std::string Name(Item value);
  std::string Name(Ability value);

  class Match {
  public:
    Config config;
    GameState state;
    explicit Match(Config rules = {});
    static Match Create(const Config &, const std::string &scenarioFile, Mode mode, Difficulty difficulty,
                        const std::array<std::vector<UnitClass>, 2> &armies = {});
    Cell *At(Hex pos);
    const Cell *At(Hex pos) const;
    Unit *Find(int id);
    const Unit *Find(int id) const;
    Unit *Occupant(Hex pos);
    const Unit *Occupant(Hex pos) const;
    Stats EffectiveStats(const Unit &) const;
    int MoveCost(const Unit &, Hex from, Hex to) const;
    std::optional<Path> FindPath(int unitId, Hex target) const;
    std::map<Hex, int> Reachable(int unitId) const;
    bool LineOfSight(Hex from, Hex to) const;
    bool CanTarget(int attackerId, int targetId, Ability ability = Ability::Attack) const;
    int EstimateDamage(int attackerId, int targetId, Ability ability = Ability::Attack) const;
    std::vector<Ability> Abilities(int unitId) const;
    bool Available(const Unit &) const;
    int AvailableCount(int player = -1) const;
    int Income(int player) const;
    int CaptureRequired(Building) const;
    int CaptureRequired(const Cell &cell) const { return CaptureRequired(cell.building); }
    int CounterDamage(int defenderId, int attackerId) const;
    Result BeginActivation(int unitId);
    Result Move(int unitId, Hex destination);
    Result UseAbility(int unitId, Ability ability, Hex target);
    Result EndActivation();
    Result EndTurn();
    Result Pass();
    Result Capture(int unitId);
    Result BuildOutpost(int unitId);
    Result Recruit(int player, UnitClass type, Hex building);
    Result RecruitBattalion(int player, const std::vector<UnitClass> &, Hex building);
    Result BuyItem(int unitId, Item item);
    Result UseItem(int unitId, int slot, Hex target);
    int RotDamageAt(Hex pos) const;
    int FireDamageAt(Hex pos) const;
    int LargestSourceGroup() const;
    void EndRound();
    void Damage(int unitId, int amount, int killerOwner = -1);
    void Log(std::string text);
    bool Validate(std::string &error) const;
    bool Save(const std::string &filename, std::string &error) const;
    static std::optional<Match> Load(const std::string &filename, const Config &, std::string &error);
    bool StepAI();

  private:
    void AdvanceTurn();
    void StartTurn(int player);
    void ResolveHit(int attackerId, int targetId, Ability, bool counter = false, int chainIndex = 0, Hex origin = {});
    void Ignite(Hex center, int owner);
    int CalculateDamage(const Unit &, const Unit &, Ability) const;
    Result CheckActive(int unitId, bool needsAction) const;
  };
} // espacio de nombres Plaguefront
