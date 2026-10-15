#include "Plaguefront/Game.h"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>

namespace Plaguefront {
  // Obtiene fuentes y peste global mediante muertes y finales de ronda reales, sin inventar estados visuales.
  void Game::SmokeRotPhase5(int step) {
    static int checks = 0;
    if (step == 0)
      checks = 0;
    auto require = [&](bool value, const char *why) {++checks;if(!value)throw std::runtime_error(why); };
    require(step >= 0 && step < 7, "Phase5 rot: fixture index out of bounds");
    scenario_ = 0;
    mode_ = Mode::Local;
    LoadArmies();
    BeginGame();
    ChangeScreen(Screen::Playing);
    require(match_.has_value(), "Phase5 rot: real scenario did not load");
    const std::array<Hex, 3> centers{{{0, 4}, {2, 4}, {3, 3}}};
    const int count = step < 3 ? 1 : step == 3 ? 2
                                               : 3;
    std::vector<int> victims;
    for (const auto &unit : match_->state.units)
      if (unit.owner == 1 && static_cast<int>(victims.size()) < count)
        victims.push_back(unit.id);
    require(static_cast<int>(victims.size()) == count, "Phase5 rot: fixture lacks victims");
    auto ally = std::ranges::find_if(match_->state.units, [](const Unit &unit) { return unit.owner == 0 && unit.Alive(); });
    require(ally != match_->state.units.end(), "Phase5 rot: fixture lacks live ally");
    const int allyId = ally->id;
    if (step == 6)
      ally->pos = {1, 4};
    for (int i = 0; i < count; ++i) {
      const auto *cell = match_->At(centers[i]);
      require(cell && cell->terrain != Terrain::Blocked && cell->terrain != Terrain::DeepWater, "Phase5 rot: corpse position is not traversable");
      require(!match_->Occupant(centers[i]), "Phase5 rot: corpse placement would overlap a living unit");
      match_->Find(victims[i])->pos = centers[i];
    }
    ResetPresentation();
    for (int id : victims)
      match_->Damage(id, 1000);
    SyncPresentation();
    visuals_.Update(3.f);
    require(static_cast<int>(match_->state.corpses.size()) == count && match_->state.sources.empty(), "Phase5 rot: real deaths must initially remain inert");
    if (step > 0) {
      for (int round = 0; round <= match_->config.rotWait; ++round)
        match_->EndRound();
      SyncPresentation();
      visuals_.Update(3.f);
      require(match_->state.corpses.empty() && static_cast<int>(match_->state.sources.size()) == count, "Phase5 rot: death wait did not create exactly the expected real sources");
    }
    selected_ = -1;
    selectedCell_ = centers[0];
    hovered_.reset();
    SetContext(TacticalContext::Selection);
    if (step == 0 || step == 1) {
      FocusCamera(false);
      camera_.CenterOn(centers[0], static_cast<float>(match_->At(centers[0])->height), {world_.getSize().x / 2.f, world_.getSize().y / 2.f});
    } else
      FocusCamera(true);
    animationTime_ = .45f + step * .17f;
    int localCells = 0, globalOnlyCells = 0, overlapCells = 0;
    for (const auto &cell : match_->state.cells) {
      const auto exposure = RotPresentationAt(*match_, cell.pos);
      int expected = 0;
      for (const auto &source : match_->state.sources)
        if (source.remaining > 0 && Distance(cell.pos, source.pos) <= match_->config.rotRadius)
          ++expected;
      require(exposure.local == (expected > 0) && exposure.coveringSources == expected, "Phase5 rot: rendered cell mask differs from real active source radius");
      require(exposure.global == (match_->state.globalRemaining > 0) && exposure.damage == match_->RotDamageAt(cell.pos), "Phase5 rot: HUD warning or damage disagrees with authoritative rules");
      localCells += exposure.local;
      globalOnlyCells += exposure.global && !exposure.local;
      overlapCells += exposure.coveringSources > 1;
    }
    if (step == 0) {
      require(localCells == 0 && match_->state.globalRemaining == 0, "Phase5 rot: inert corpse must not tint the map");
      message_ = "Cadáver inerte: cuerpo tendido y contador; sin borde de contaminación, pulso ni esporas activas.";
    } else if (step == 1 || step == 2) {
      require(localCells > 7 && match_->state.globalRemaining == 0, "Phase5 rot: local source must cover the real radius without a global plague");
      message_ = "Fuente activa: centro contrastado, pulso, esporas y " + std::to_string(localCells) + " celdas del radio real " + std::to_string(match_->config.rotRadius) + ".";
    } else if (step == 3) {
      require(overlapCells > 0 && match_->state.globalRemaining == 0, "Phase5 rot: two-source overlap fixture must remain local");
      message_ = "Dos fuentes: " + std::to_string(overlapCells) + " celdas solapadas conservan una sola capa visual y el daño local normal.";
    } else {
      require(match_->LargestSourceGroup() >= match_->config.globalThreshold && match_->state.globalRemaining > 0 && globalOnlyCells > 0, "Phase5 rot: real connected group did not activate global effect beyond local radii");
      if (step == 4)
        message_ = "Tres fuentes conectadas por las reglas: el grupo activa la peste y conserva centros y radios distinguibles.";
      if (step == 5)
        message_ = "PESTE GLOBAL ACTIVA: atmósfera en todo el tablero, " + std::to_string(globalOnlyCells) + " celdas fuera del radio local y duración real en el HUD.";
      if (step == 6) {
        Dispatch({{}, "", RosterSelect, allyId});
        hovered_ = match_->Find(allyId)->pos;
        const auto exposure = RotPresentationAt(*match_, match_->Find(allyId)->pos);
        require(selected_ == allyId && exposure.local && exposure.global && exposure.damage == match_->config.rotDamage, "Phase5 rot: selected warning must report local damage without adding global damage");
        message_ = "Unidad afectada: la ficha distingue putrefacción local y peste global; el daño no se acumula.";
      }
    }
    std::string error;
    require(match_->Validate(error), "Phase5 rot: constructed scenario failed real model validation");
    ctx.cursor.SetPosition({BoardBounds().position.x + BoardBounds().size.x * .7f, BoardBounds().position.y + BoardBounds().size.y * .7f});
    UpdateCursor();
    if (step == 6)
      std::cout << "Plaguefront phase5 rot runtime: " << checks << " checks across seven real corpse/source/overlap/global/selected-unit captures.\n";
  }
} // Espacio de nombres Plaguefront
