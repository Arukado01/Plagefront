#include "Plaguefront/Game.h"
#include <algorithm>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace Plaguefront {
  void Game::SmokePhase4(int step) {
    static int checks = 0;
    if (step == 0)
      checks = 0;
    auto require = [&](bool value, const char *why) {++checks;if(!value)throw std::runtime_error(why); };
    auto fresh = [&](int scenario = 0) {scenario_=scenario;mode_=Mode::Local;LoadArmies();BeginGame();ChangeScreen(Screen::Playing);rosterScroll_=0; };
    auto select = [&](int id) {Dispatch({{},"",RosterSelect,id});require(selected_==id&&tactical_==TacticalContext::Orders,"Phase4: roster selection failed"); };
    auto combat = [&](UnitClass type) {
      fresh();
      auto actor = std::ranges::find_if(match_->state.units, [&](const Unit &u) { return u.owner == 0 && u.type == type; });
      auto victim = std::ranges::find_if(match_->state.units, [](const Unit &u) { return u.owner == 1 && u.type == UnitClass::Saber; });
      require(actor != match_->state.units.end() && victim != match_->state.units.end(), "Phase4: combat fixture lacks troops");
      actor->pos = {0, 4};
      victim->pos = type == UnitClass::Caster ? Hex{2, 4} : Hex{1, 4};
      const int id = actor->id, target = victim->id;
      ResetPresentation();
      select(id);
      return target;
    };
    auto corpse = [&](int ends) {fresh();const int id=match_->state.units.front().id;match_->Find(id)->pos={0,4};ResetPresentation();match_->Damage(id,1000);SyncPresentation();visuals_.Update(3.f);for(int i=0;i<ends;++i)match_->EndRound();SyncPresentation();visuals_.Update(3.f);selected_=-1;selectedCell_=Hex{0,4};camera_.CenterOn({0,4},static_cast<float>(match_->At({0,4})->height),{world_.getSize().x/2.f,world_.getSize().y/2.f}); };
    auto capture = [&]() {
      fresh();
      const auto fortress = std::ranges::find_if(match_->state.cells, [](const Cell &c) { return c.building == Building::Fortress && c.owner == 1; });
      require(fortress != match_->state.cells.end(), "Phase4: fortress missing");
      const Hex pos = fortress->pos;
      if (auto *enemy = match_->Occupant(pos)) {
        const auto free = std::ranges::find_if(match_->state.cells, [&](const Cell &c) { return c.terrain == Terrain::Plain && !match_->Occupant(c.pos) && c.building == Building::None; });
        require(free != match_->state.cells.end(), "Phase4: defender relocation missing");
        enemy->pos = free->pos;
      }
      const int id = match_->state.units.front().id;
      match_->Find(id)->pos = pos;
      ResetPresentation();
      select(id);
      Dispatch({{}, "", Capture});
      require(match_->At(pos)->captureProgress > 0 && match_->state.winner < 0 && match_->At(pos)->owner == 1, "Phase4: first capture won prematurely");
      visuals_.Update(3.f);
      return std::pair{id, pos};
    };
    if (step == 0) {
      fresh(2);
      FocusCamera(true);
      require(match_->AvailableCount() == 5, "Phase4: initial army unavailable");
      message_ = "Turno azul: todas las tropas están disponibles. E termina una activación; F termina el turno del ejército.";
    } else if (step == 1) {
      fresh();
      const int first = match_->state.units.front().id;
      select(first);
      FinishTurn();
      require(match_->state.currentPlayer == 0 && match_->AvailableCount() == 4 && match_->Find(first)->activated, "Phase4: unit wait handed off army");
      select(first);
      require(!CheckAction(AbilitySelect, static_cast<int>(Ability::Guard)), "Phase4: acted unit reactivated through roster");
      const auto other = std::ranges::find_if(match_->state.units, [&](const Unit &u) { return match_->Available(u); });
      select(other->id);
      FocusCamera(true);
      FinishArmyTurn();
      require(screen_ == Screen::ConfirmTurn, "Phase4: early end has no confirmation");
      Dispatch({{}, "", CancelArmyEnd});
      require(screen_ == Screen::Playing && match_->state.currentPlayer == 0, "Phase4: cancel end changed army");
      message_ = "Una tropa ACTUADA; cuatro DISPONIBLES. Cancelar FIN DE TURNO conserva el mando.";
    } else if (step == 2) {
      fresh();
      FinishArmyTurn();
      require(screen_ == Screen::ConfirmTurn, "Phase4: unused confirmation missing");
      Dispatch({{}, "", ConfirmArmyEnd});
      require(match_->state.currentPlayer == 1 && screen_ == Screen::Handoff, "Phase4: army handoff failed");
      Dispatch({{}, "", Ready});
      const auto red = std::ranges::find_if(match_->state.units, [](const Unit &u) { return u.owner == 1; });
      select(red->id);
      FocusCamera(true);
      message_ = "Turno carmesí: paleta, estandartes e insignias distinguen el segundo ejército.";
    } else if (step == 3) {
      fresh(2);
      const int id = match_->state.units[2].id;
      const int coins = match_->state.players[0].coins;
      const Match original = *match_;
      select(id);
      require(match_->state.activeUnit < 0 && match_->state.players[0].coins == coins && !match_->Find(id)->activationStarted, "Phase4: roster inspection consumed activation");
      UpdatePreview({});
      require(reachable_.size() > 1, "Phase4: roster did not reveal movement");
      // Comprueba el clic en la lista mediante la conversión física del cursor y OnEvent.
      buttons_.clear();
      RenderRoster();
      const auto row = std::ranges::find_if(buttons_, [&](const Button &b) { return b.action == RosterSelect && b.value == id; });
      require(row != buttons_.end(), "Phase4: selected roster row missing");
      ctx.cursor.SetPosition(row->bounds.position + sf::Vector2f{40, 20});
      sf::Event::MouseButtonPressed click{};
      click.button = sf::Mouse::Button::Left;
      click.position = ctx.renderer.LogicalToPixel(row->bounds.position + sf::Vector2f{40, 20});
      OnEvent(sf::Event(click));
      require(selected_ == id && match_->state.activeUnit == original.state.activeUnit, "Phase4: list click failed or consumed activation");
      message_ = "Selección desde la lista izquierda: cámara centrada y movimiento legal disponible.";
    } else if (step == 4) {
      combat(UnitClass::Caster);
      match_->Find(selected_)->hp -= 6;
      hovered_ = match_->Find(selected_)->pos;
      message_ = "Ficha sobre la cabeza: HP, ATQ, DEF, maná y disponibilidad proceden del núcleo.";
    } else if (step == 5) {
      fresh();
      const auto tree = std::ranges::find_if(match_->state.cells, [](const Cell &c) { return c.terrain == Terrain::Forest; });
      require(tree != match_->state.cells.end(), "Phase4: forest missing");
      auto &unit = match_->state.units.front();
      unit.pos = tree->pos;
      const int id = unit.id;
      ResetPresentation();
      select(id);
      hovered_ = unit.pos;
      message_ = "Unidad consultada en bosque: follaje controlado e indicadores por encima del mundo.";
    } else if (step == 6) {
      corpse(0);
      require(match_->state.corpses.size() == 1 && match_->state.sources.empty(), "Phase4: fresh corpse state wrong");
      message_ = "Cadáver reciente: cuerpo tendido y equipo conservado; todavía no contamina.";
    } else if (step == 7) {
      corpse(2);
      require(match_->state.corpses.size() == 1 && match_->state.sources.empty(), "Phase4: waiting corpse disappeared");
      message_ = "Cadáver en espera: el contador indica rondas hasta convertirse en fuente.";
    } else if (step == 8 || step == 9) {
      corpse(4);
      require(match_->state.corpses.empty() && match_->state.sources.size() == 1, "Phase4: corpse did not convert on schedule");
      hovered_ = Hex{0, 4};
      if (step == 9) {
        int highlighted = 0;
        for (const auto &cell : match_->state.cells) {
          const bool local = Distance(cell.pos, {0, 4}) <= match_->config.rotRadius;
          require((match_->RotDamageAt(cell.pos) > 0) == local, "Phase4: local overlay disagrees with real radius");
          highlighted += local;
        }
        require(highlighted > 7, "Phase4: source radius fixture too small");
        FocusCamera(true);
      }
      message_ = "Fuente activa: radio hexagonal real " + std::to_string(match_->config.rotRadius) + ", daño local " + std::to_string(match_->config.rotDamage) + " HP al final de ronda.";
    } else if (step == 10) {
      fresh();
      std::vector<int> ids;
      for (const auto &u : match_->state.units)
        if (u.owner == 0 && ids.size() < 3)
          ids.push_back(u.id);
      for (int id : ids)
        match_->Damage(id, 1000);
      for (int i = 0; i < 4; ++i)
        match_->EndRound();
      SyncPresentation();
      visuals_.Update(5.f);
      require(match_->LargestSourceGroup() >= match_->config.globalThreshold && match_->state.globalRemaining > 0, "Phase4: connected sources did not trigger plague");
      const auto survivor = std::ranges::find_if(match_->state.units, [](const Unit &u) { return u.Alive(); });
      const int hp = survivor->hp;
      const int expected = match_->RotDamageAt(survivor->pos);
      match_->EndRound();
      require(match_->Find(survivor->id)->hp == std::max(0, hp - expected), "Phase4: plague environmental damage wrong");
      SyncPresentation();
      visuals_.Update(.10f);
      FocusCamera(true);
      message_ = "PESTE GLOBAL ACTIVA: conectividad transitiva y daños ambientales reales, sin sumar daño local y global.";
    } else if (step == 11) {
      const int victim = combat(UnitClass::Saber);
      const int actor = selected_, hp = match_->Find(actor)->hp;
      Dispatch({{}, "", AbilitySelect, static_cast<int>(Ability::Attack)});
      HandleCell(match_->Find(victim)->pos);
      require(match_->Find(actor)->hp < hp && match_->state.combatEvents.size() == 2 && match_->state.combatEvents.back().counter, "Phase4: legal attack has no unique counter");
      visuals_.Update(.70f);
      message_ = "Ataque y respuesta: dos impactos reales. La fuerza de CONTRA usa los HP posteriores al golpe inicial.";
    } else if (step == 12) {
      const int victim = combat(UnitClass::Caster);
      std::vector<Unit *> red;
      for (auto &u : match_->state.units)
        if (u.owner == 1 && u.id != victim)
          red.push_back(&u);
      red[0]->pos = {3, 4};
      red[1]->pos = {3, 3};
      ResetPresentation();
      select(selected_);
      Dispatch({{}, "", AbilitySelect, static_cast<int>(Ability::Lightning)});
      HandleCell(match_->Find(victim)->pos);
      int hits = 0;
      for (const auto &e : match_->state.combatEvents)
        if (!e.counter)
          ++hits;
      require(hits == 3, "Phase4: Lightning did not hit three legal nearby enemies");
      visuals_.Update(.61f);
      message_ = "Lightning: tres objetivos distintos, enlaces cortos con línea de visión y daño mágico real.";
    } else if (step == 13) {
      const int victim = combat(UnitClass::Caster);
      Dispatch({{}, "", AbilitySelect, static_cast<int>(Ability::Fire)});
      HandleCell(match_->Find(victim)->pos);
      require(!match_->state.fires.empty() && match_->FireDamageAt(match_->Find(victim)->pos) > 0, "Phase4: Fire created no persistent terrain");
      visuals_.Update(.28f);
      message_ = "Fire deja terreno incendiado durante " + std::to_string(match_->config.fireDuration) + " finales de ronda; las llamas se guardan y causan daño real.";
    } else if (step >= 14 && step <= 16) {
      const auto [id, pos] = capture();
      if (step == 14)
        message_ = "Fortaleza: captura parcial " + std::to_string(match_->At(pos)->captureProgress) + "/" + std::to_string(match_->CaptureRequired(*match_->At(pos))) + ". El propietario y ganador no cambian todavía.";
      else {
        require(match_->EndActivation().ok && match_->EndTurn().ok && match_->EndTurn().ok, "Phase4: complete army response failed");
        std::string error;
        const std::string path = "build/test-output/phase4-capture-smoke.json";
        require(match_->Save(path, error), "Phase4: partial capture save failed");
        auto loaded = Match::Load(path, balance_, error);
        require(loaded.has_value(), "Phase4: partial capture load failed");
        match_ = std::move(loaded);
        ResetPresentation();
        select(id);
        require(CheckAction(Capture).ok, "Phase4: continue capture disappeared after load/turn");
        if (step == 15) {
          const auto reachable = match_->Reachable(id);
          const auto free = std::ranges::find_if(reachable, [&](const auto &item) { return item.first != pos && item.second > 0; });
          require(free != reachable.end(), "Phase4: captor cannot leave");
          require(EnsureActive() && match_->Move(id, free->first).ok, "Phase4: captor movement failed");
          match_->EndActivation();
          SyncPresentation();
          visuals_.Update(4.f);
          selectedCell_ = pos;
          FocusCamera(false);
          message_ = "Captura interrumpida entre turnos y restaurada: los puntos permanecen, en PAUSA al abandonar el edificio.";
        } else {
          for (int n = 0; n < 8 && match_->state.winner < 0; ++n) {
            Dispatch({{}, "", Capture});
            visuals_.Update(3.f);
            if (match_->state.winner < 0) {
              require(match_->EndActivation().ok && match_->EndTurn().ok && match_->EndTurn().ok, "Phase4: capture continuation turn failed");
              SyncPresentation();
              select(id);
            }
          }
          require(match_->state.winner == 0 && match_->At(pos)->owner == 0, "Phase4: fortress completion did not atomically win");
          ChangeScreen(Screen::Victory);
        }
      }
    } else if (step == 17) {
      VerifyPhase4Controls();
      fresh();
      const int coins = match_->state.players[0].coins, income = match_->Income(0), id = match_->state.units.front().id;
      select(id);
      FinishTurn();
      require(match_->state.players[0].coins == coins, "Phase4: unit activation paid income");
      FinishArmyTurn(true);
      Dispatch({{}, "", Ready});
      FinishArmyTurn(true);
      Dispatch({{}, "", Ready});
      require(match_->state.players[0].coins == coins + income, "Phase4: army start income differs from HUD");
      select(id);
      message_ = "Ingreso comprobado: +" + std::to_string(income) + " una sola vez al comenzar el turno; terminar una unidad no entrega monedas.";
      std::cout << "Plaguefront phase4 UI: " << checks << " checks passed; 18 new application captures with real rule commands.\n";
    } else if (step == 18) {
      fresh();
      select(match_->state.units.front().id);
      FinishArmyTurn();
      require(screen_ == Screen::ConfirmTurn, "Phase4: explicit army confirmation missing");
    } else if (step == 19) {
      fresh(2);
      match_->state.units.clear();
      for (int team = 0; team < 2; ++team)
        for (int type = 0; type < 7; ++type) {
          const Hex pos = team == 1 && type == 5 ? Hex{9, 5} : Hex{team == 0 ? 3 : 8, 2 + type};
          const auto *cell = match_->At(pos);
          require(cell && cell->terrain != Terrain::DeepWater && cell->terrain != Terrain::Blocked, "Phase4: faction gallery placement invalid");
          Unit unit;
          unit.id = match_->state.nextId++;
          unit.owner = team;
          unit.type = static_cast<UnitClass>(type);
          unit.pos = pos;
          unit.hp = match_->config.classes[type].hp;
          unit.mp = type == 4 ? match_->config.maxMana : 0;
          match_->state.units.push_back(unit);
        }
      ResetPresentation();
      FocusCamera(true);
      std::string error;
      require(match_->Validate(error), "Phase4: all-class gallery match invalid");
      message_ = "Las siete clases de ambos bandos: cruz azul y doble barra carmesí, con metales, piel y madera compartidos.";
    } else if (step == 20) {
      fresh(2);
      ctx.window.SetFullscreen(true);
      ctx.window.ApplyPending();
      ctx.renderer.ConfigureOutput(ctx.window.OutputSize());
      ctx.cursor.SetView(ctx.renderer.View());
      RefreshLayout(true);
      displayRevision_ = ctx.window.Revision();
      select(match_->state.units.front().id);
      require(ctx.window.IsFullscreen(), "Phase4: final full-screen card fixture failed");
      message_ = "Ficha y listado a resolución de salida, en pantalla completa real. F11 restaura la ventana.";
    }
    ctx.cursor.SetPosition({BoardOrigin.x + BoardBounds().size.x * .58f, BoardOrigin.y + BoardBounds().size.y * .65f});
    UpdateCursor();
  }
} // Espacio de nombres Plaguefront
