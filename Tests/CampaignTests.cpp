#include "Plaguefront/Rules.h"

#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string>

using namespace Plaguefront;

namespace {
int commands = 0;
void Valid(const Match &match, const std::string &context) {
  std::string error;
  if (!match.Validate(error)) throw std::runtime_error(context + ": " + error);
  ++commands;
}
void Require(const Result &result, const char *context) {
  if (!result) throw std::runtime_error(std::string(context) + ": " + result.message);
}
void Dump(const Match &match) {
  for (const auto &unit : match.state.units) if (unit.Alive())
    std::cerr << "  P" << unit.owner << ' ' << Name(unit.type) << " #" << unit.id << " at "
              << unit.pos.q << ',' << unit.pos.r << " HP=" << unit.hp << '\n';
  for (const auto &cell : match.state.cells) if (cell.building != Building::None)
    std::cerr << "  Building " << static_cast<int>(cell.building) << " at " << cell.pos.q << ','
              << cell.pos.r << " owner=" << cell.owner << " capture=" << cell.captureUnit << '\n';
}
const char *DifficultyLabel(Difficulty difficulty) {
  switch (difficulty) {
    case Difficulty::Easy: return "easy";
    case Difficulty::Normal: return "normal";
    case Difficulty::Hard: return "hard";
  }
  return "unknown";
}
void Campaign(const char *scenario, Difficulty difficulty, std::chrono::steady_clock::time_point deadline) {
  Match match = Match::Create(Config::Load("Content/Plaguefront/balance.json"),
    std::string("Content/Plaguefront/scenarios/") + scenario + ".json", Mode::VersusAI, difficulty);
  const std::string label = std::string(scenario) + "/" + DifficultyLabel(difficulty);
  Valid(match, "initial scenario");
  int aiActivations = 0, lastRound = 0;
  while (match.state.winner == -1 && match.state.round <= 80 && aiActivations < 250) {
    if (std::chrono::steady_clock::now() > deadline) {
      Dump(match);
      throw std::runtime_error(label + ": campaign exceeded local 55-second execution budget");
    }
    if (match.state.currentPlayer == 0) Require(match.Pass(), "human passes remaining activations");
    else {
      if (!match.StepAI()) throw std::runtime_error("AI refused a legal current-player turn");
      ++aiActivations;
    }
    Valid(match, label + " after command");
    if (match.state.round != lastRound) {
      lastRound = match.state.round;
      std::cout << "[PROGRESS] " << label << " round=" << lastRound << " AI activations="
                << aiActivations << " winner=" << match.state.winner << '\n' << std::flush;
    }
  }
  if (match.state.winner != 1) {
    Dump(match);
    throw std::runtime_error(label + ": AI failed to conquer fortress within bounded campaign");
  }
  std::cout << "[PASS] " << label << ": AI conquered fortress in round " << match.state.round
            << " after " << aiActivations << " activations\n";
}
void ScriptedLocalConquest() {
  Match match;
  match.state.mode = Mode::Local;
  for (int q = -3; q <= 3; ++q) for (int r = -3; r <= 3; ++r)
    if (Distance({}, {q, r}) <= 3) match.state.cells.push_back({{q, r}});
  match.At({-3, 0})->building = Building::Fortress;
  match.At({-3, 0})->owner = 0;
  match.At({3, 0})->building = Building::Fortress;
  match.At({3, 0})->owner = 1;
  Unit rider;
  rider.id = match.state.nextId++; rider.owner = 0; rider.type = UnitClass::Rider;
  rider.pos = {-1, 0}; rider.hp = match.config.classes[static_cast<int>(rider.type)].hp;
  const int captorId = rider.id;
  match.state.units.push_back(rider);
  Unit defender;
  defender.id = match.state.nextId++; defender.owner = 1; defender.pos = {0, 3}; defender.hp = 28;
  match.state.units.push_back(defender);
  Valid(match, "local initial state");
  Require(match.BeginActivation(captorId), "activate local rider"); Valid(match, "local activation");
  const auto route = match.FindPath(captorId, {3, 0});
  if (!route || route->cost != 4 || route->cells.size() != 5) throw std::runtime_error("Unexpected local capture route");
  Require(match.Move(captorId, route->cells.back()), "follow route to enemy fortress"); Valid(match, "local movement");
  Require(match.Capture(captorId), "initiate fortress conquest"); Valid(match, "local capture initiation");
  if (match.state.winner != -1) throw std::runtime_error("Capture won before defender opportunity");
  Require(match.EndActivation(), "end captor activation"); Valid(match, "local turnover");
  Require(match.EndTurn(), "explicitly finish captor army turn"); Valid(match, "local army turnover");
  Require(match.Pass(), "defender declines response"); Valid(match, "local defender pass");
  Require(match.BeginActivation(captorId), "resume capture on next owner turn"); Valid(match, "local resumed activation");
  if (match.state.winner != -1) throw std::runtime_error("Selecting captor must not complete capture");
  Require(match.Capture(captorId), "complete conquest with another capture action"); Valid(match, "local victory");
  if (match.state.winner != 0 || match.At({3, 0})->owner != 0)
    throw std::runtime_error("Local scripted conquest did not finish the match");
  std::cout << "[PASS] local move -> partial capture -> both army turns -> continued capture -> victory\n";
}
void EconomyAfterArmyLoss() {
  auto match = Match::Create(Config{}, "Content/Plaguefront/scenarios/frontier.json", Mode::Local, Difficulty::Normal);
  for (const auto &unit : match.state.units) if (unit.owner == 0) match.Damage(unit.id, 1000);
  Valid(match, "army wipe");
  Require(match.Pass(), "eliminated player passes current round"); Valid(match, "wiped player pass");
  Require(match.Pass(), "opponent passes remaining round"); Valid(match, "opponent pass advances round");
  if (match.state.currentPlayer != 0 || match.state.round != 2)
    throw std::runtime_error("An eliminated player lost the next economic turn");
  Hex fortress;
  for (const auto &cell : match.state.cells)
    if (cell.building == Building::Fortress && cell.owner == 0) fortress = cell.pos;
  const auto count = match.state.units.size();
  Require(match.Recruit(0, UnitClass::Saber, fortress), "rebuild eliminated army with fortress income");
  Valid(match, "recruit after army wipe");
  if (match.state.units.size() != count + 1 || match.state.units.back().readyRound != 3)
    throw std::runtime_error("Replacement army must exist but wait for a complete activation round");
  std::cout << "[PASS] eliminated army retains income/recruit/pass economic turn\n";
}
void PurifySourceAboveCorpse() {
  auto match = Match::Create(Config{}, "Content/Plaguefront/scenarios/frontier.json", Mode::Local, Difficulty::Normal);
  int casterId = -1;
  for (const auto &unit : match.state.units) if (unit.owner == 0 && unit.type == UnitClass::Caster) casterId = unit.id;
  if (casterId < 0) throw std::runtime_error("Scenario needs a Caster for purifier integration test");
  const Hex pos = match.Find(casterId)->pos;
  match.state.corpses.push_back({match.state.nextId++, pos, match.state.round});
  match.state.sources.push_back({match.state.nextId++, pos, 5});
  Valid(match, "same-cell corpse and source");
  Require(match.BeginActivation(casterId), "activate purifier"); Valid(match, "purifier activation");
  Require(match.UseAbility(casterId, Ability::Purify, pos), "purify active source");
  Valid(match, "source-only purification");
  if (!match.state.sources.empty() || match.state.corpses.size() != 1)
    throw std::runtime_error("Purification removed corpse instead of active source");
  if (match.Find(casterId)->mp != 5 || !match.Find(casterId)->acted)
    throw std::runtime_error("Purification must consume mana and action exactly once");
  std::cout << "[PASS] purification removes active source and preserves same-cell corpse\n";
}
}

int main() {
  try {
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(55);
    ScriptedLocalConquest();
    EconomyAfterArmyLoss();
    PurifySourceAboveCorpse();
    for (Difficulty difficulty : {Difficulty::Easy, Difficulty::Normal, Difficulty::Hard}) {
      Campaign("frontier", difficulty, deadline);
      Campaign("marsh", difficulty, deadline);
    }
    std::cout << "7 campaigns and 2 regressions completed; " << commands << " complete-state validations passed.\n";
    return 0;
  } catch (const std::exception &error) {
    std::cerr << "[FAIL] " << error.what() << '\n';
    return 1;
  }
}
