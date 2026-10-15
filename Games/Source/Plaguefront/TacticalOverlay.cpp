#include "Plaguefront/Game.h"
#include "Core/EngineConfig.h"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace Plaguefront {
  namespace {
    const sf::Color Gold{232, 202, 137}, Muted{184, 194, 187};
    const std::array<sf::Color, 2> Teams{sf::Color{109, 186, 224}, sf::Color{225, 100, 120}};
  } // Espacio de nombres
  void Game::RenderRoster() const {
    const auto &game = *match_;
    const auto &s = game.state;
    Box({{16, 88}, {UI::RosterWidth, UiHeight() - 160}}, {27, 31, 41}, {113, 108, 91});
    Text("EJÉRCITO " + std::string(s.currentPlayer == 0 ? "AZUL" : "CARMESÍ"), 28, 104, 18, Teams[s.currentPlayer]);
    Text("Disponibles " + std::to_string(game.AvailableCount()), 28, 133, 16, Gold);
    const int acted = static_cast<int>(std::ranges::count_if(s.units, [&](const Unit &u) { return u.Alive() && u.owner == s.currentPlayer && u.activated; }));
    Text("Actuadas " + std::to_string(acted), 28, 153, 16, Muted);
    std::vector<const Unit *> rows;
    for (const auto &u : s.units)
      if (u.Alive() && u.owner == s.currentPlayer)
        rows.push_back(&u);
    std::ranges::stable_sort(rows, [&](const Unit *a, const Unit *b) { return game.Available(*a) > game.Available(*b); });
    const int visible = RosterVisibleRows();
    const int start = std::clamp(rosterScroll_, 0, std::max(0, static_cast<int>(rows.size()) - visible));
    for (int i = start; i < static_cast<int>(rows.size()) && i < start + visible; ++i) {
      const auto &u = *rows[i];
      const float y = UI::RosterTop + (i - start) * UI::RosterStep;
      const bool available = game.Available(u), chosen = selected_ == u.id;
      Box({{24, y}, {196, 54}}, chosen ? sf::Color{43, 56, 64} : sf::Color{21, 27, 34}, chosen ? Gold : sf::Color{58, 66, 70});
      if (unitIcons_ && !layoutOnly_) {
        sf::Sprite sprite(*unitIcons_, sf::IntRect({static_cast<int>(u.type) * 24, u.owner * 24}, {24, 24}));
        sprite.setPosition({28, y + 4});
        ctx.renderer.Draw(sprite);
      }
      Text(Name(u.type), 52, y + 3, 17, chosen ? Gold : Teams[u.owner]);
      Text(std::to_string(u.hp) + "/" + std::to_string(game.config.classes[static_cast<int>(u.type)].hp), 159, y + 4, 16);
      const auto *cell = game.At(u.pos);
      const bool capturing = cell && cell->capturingPlayer == u.owner && cell->captureProgress > 0;
      const std::string status = u.readyRound > s.round ? "REFUERZO" : available ? "DISPONIBLE"
                                                                                 : "ACTUADA";
      Text(status + (capturing ? " · CAPT." : ""), 33, y + 29, 16, available ? sf::Color{167, 212, 157} : Muted);
      buttons_.push_back({{{24, y}, {196, 54}}, status, RosterSelect, u.id, !visuals_.Busy() && !(s.mode == Mode::VersusAI && s.currentPlayer == 1)});
    }
    if (static_cast<int>(rows.size()) > visible) {
      ButtonAt("↑", 26, UiHeight() - 194, 66, 30, RosterScroll, -1, start > 0);
      Text(std::to_string(start + 1) + "–" + std::to_string(std::min(start + visible, static_cast<int>(rows.size()))) + "/" + std::to_string(rows.size()), 100, UiHeight() - 189, 16);
      ButtonAt("↓", 158, UiHeight() - 194, 60, 30, RosterScroll, 1, start + visible < static_cast<int>(rows.size()));
    }
    ButtonAt("FIN DE TURNO", 26, UiHeight() - 134, 192, 44, ArmyEnd, 0, !visuals_.Busy() && s.winner < 0 && !(s.mode == Mode::VersusAI && s.currentPlayer == 1));
  }
  void Game::UpdateCursor(std::optional<sf::Vector2f> eventPosition) {
    CursorContext context = CursorContext::Normal;
    const auto cursor = eventPosition.value_or(ctx.cursor.GetPosition());
    for (const auto &button : buttons_)
      if (button.bounds.contains(cursor)) {
        context = !button.enabled ? CursorContext::Blocked : button.action == Capture                                                                                                                          ? CursorContext::Capture
                                                         : button.action == Buy || button.action == Recruit || button.action == Battalion || button.action == ConfirmPurchase || button.action == ConfirmBuild ? CursorContext::Economy
                                                                                                                                                                                                               : CursorContext::Normal;
        ctx.cursor.SetContext(context);
        return;
      }
    if (match_ && screen_ == Screen::Playing && hovered_ && selected_ >= 0 && !visuals_.Busy()) {
      const auto *unit = match_->Find(selected_);
      const auto *target = match_->Occupant(*hovered_);
      if (unit && unit->owner == match_->state.currentPlayer) {
        if (ability_ || itemSlot_ >= 0) {
          Match test = PreviewRules();
          const auto legal = ability_ ? test.UseAbility(selected_, *ability_, *hovered_) : test.UseItem(selected_, itemSlot_, *hovered_);
          context = legal ? (ability_ && *ability_ == Ability::Heal ? CursorContext::Normal : CursorContext::Attack) : CursorContext::Blocked;
        } else if (target && target->owner != unit->owner)
          context = CheckAction(AbilitySelect, static_cast<int>(Ability::Attack)) && PreviewRules().CanTarget(selected_, target->id, Ability::Attack) ? CursorContext::Attack : CursorContext::Blocked;
        else if (reachable_.contains(*hovered_) && *hovered_ != unit->pos)
          context = CursorContext::Move;
        else if (const auto *cell = match_->At(*hovered_); cell && cell->building != Building::None && (cell->owner != unit->owner || cell->captureProgress > 0))
          context = *hovered_ == unit->pos && CheckAction(Capture) ? CursorContext::Capture : CursorContext::Blocked;
        else if (tactical_ == TacticalContext::Move && *hovered_ != unit->pos)
          context = CursorContext::Blocked;
      }
    }
    ctx.cursor.SetContext(context);
  }
  void Game::RenderTacticalInformation() const {
    const auto &game = *match_;
    const auto bounds = BoardBounds();
    const float scale = WorldScale(), zoom = static_cast<float>(camera_.Zoom());
    auto ui = [&](Point p) { return sf::Vector2f{BoardOrigin.x + p.x * scale, BoardOrigin.y + p.y * scale}; };
    std::vector<sf::FloatRect> occupied;
    std::vector<std::function<void()>> labels;
    auto place = [&](sf::Vector2f anchor, sf::Vector2f size) {
      sf::FloatRect box{{std::clamp(anchor.x - size.x / 2, bounds.position.x + 3, bounds.position.x + bounds.size.x - size.x - 3), std::clamp(anchor.y - size.y, bounds.position.y + 4, bounds.position.y + bounds.size.y - size.y - 4)}, size};
      const auto origin = box.position;
      bool found = false;
      for (int ring = 0; ring < 7 && !found; ++ring) {
        const int steps = ring == 0 ? 1 : 8;
        for (int n = 0; n < steps; ++n) {
          constexpr float dx[]{0, -1, 1, 0, -1, 1, -1, 1}, dy[]{-1, 0, 0, 1, -1, -1, 1, 1};
          const float distance = static_cast<float>(ring);
          box.position = {std::clamp(origin.x + (ring ? dx[n] * (size.x * .55f + 6) * distance : 0.f), bounds.position.x + 3, bounds.position.x + bounds.size.x - size.x - 3), std::clamp(origin.y + (ring ? dy[n] * (size.y + 5) * distance : 0.f), bounds.position.y + 4, bounds.position.y + bounds.size.y - size.y - 4)};
          if (std::ranges::none_of(occupied, [&](const sf::FloatRect &other) { return box.findIntersection(other).has_value(); })) {
            found = true;
            break;
          }
        }
      }
      occupied.push_back({box.position - sf::Vector2f{3, 3}, box.size + sf::Vector2f{6, 6}});
      return box;
    };
    auto connector = [&](sf::Vector2f from, sf::FloatRect box, sf::Color color) {
      sf::VertexArray line(sf::PrimitiveType::Lines, 2);
      line[0] = {{from.x, from.y}, color};
      line[1] = {{box.position.x + box.size.x / 2, box.position.y + box.size.y}, color};
      ctx.renderer.Draw(line);
    };
    // El progreso pertenece al edificio y se conserva durante las capturas pausadas.
    for (const auto &cell : game.state.cells)
      if (cell.captureProgress > 0 && cell.capturingPlayer >= 0) {
        const auto base = ui(camera_.Project(cell.pos, static_cast<float>(cell.height)));
        if (!bounds.contains(base))
          continue;
        const float height = cell.building == Building::Fortress ? 52.f : cell.building == Building::Outpost ? 40.f
                                                                                                             : 30.f;
        const auto box = place({base.x, base.y - height * zoom * scale}, {180, 46});
        connector({base.x, base.y - 14 * zoom * scale}, box, Teams[cell.capturingPlayer]);
        const auto *property = &cell;
        labels.push_back([&, box, property] {const auto &cell=*property;Box(box,{15,23,29,240},Teams[cell.capturingPlayer]);const int total=game.CaptureRequired(cell);
    const auto *occupant=game.Occupant(cell.pos);const bool ongoing=occupant&&occupant->owner==cell.capturingPlayer;
    Text(std::to_string(cell.captureProgress)+"/"+std::to_string(total)+(ongoing?" · CONTINUAR":" · PAUSA"),box.position.x+7,box.position.y+2,16);
    Box({box.position+sf::Vector2f{7,29},{166,8}},{44,47,50});
    Box({box.position+sf::Vector2f{7,29},{166.f*std::clamp(static_cast<float>(cell.captureProgress)/std::max(1,total),0.f,1.f),8}},Teams[cell.capturingPlayer]); });
      }
    std::vector<const Unit *> units;
    for (const auto &unit : game.state.units)
      if (unit.Alive())
        units.push_back(&unit);
    auto priority = [&](const Unit *unit) { return unit->id == selected_ ? 0 : hovered_ && unit->pos == *hovered_ ? 1
                                                                                                                  : 2; };
    std::ranges::stable_sort(units, [&](const Unit *a, const Unit *b) { return priority(a) < priority(b); });
    for (const auto *unit : units) {
      const auto visual = visuals_.VisualFor(*unit);
      const auto base = ui(camera_.ProjectContinuous(visual.q, visual.r, visual.elevation));
      if (!bounds.contains(base))
        continue;
      const auto stats = game.EffectiveStats(*unit);
      const bool detailed = priority(unit) < 2;
      const bool compact = !detailed && zoom * scale < 1.6f;
      std::vector<std::string> warnings;
      std::size_t rotWarningCount{};
      if (detailed) {
        const auto exposure = RotPresentationAt(game, unit->pos);
        if (exposure.Active())
          warnings.push_back(std::string(exposure.local ? "PUTREF. LOCAL -" : "PESTE GLOBAL -") + std::to_string(exposure.damage) + " HP");
        if (exposure.local && exposure.global)
          warnings.push_back("Global no se suma");
        rotWarningCount = warnings.size();
        const int fire = game.FireDamageAt(unit->pos);
        if (fire > 0)
          warnings.push_back("FUEGO -" + std::to_string(fire) + " HP");
        if (smoke_)
          for (const auto &warning : warnings) {
            const float uiScale = ctx.renderer.UiScale();
            const unsigned pixels = static_cast<unsigned>(std::max(12.f, std::round(16.f * uiScale)));
            sf::Text measured(font_, sf::String::fromUtf8(warning.begin(), warning.end()), pixels);
            if (measured.getLocalBounds().size.x / uiScale > 202.f)
              throw std::runtime_error("Smoke: environmental warning exceeds unit card: " + warning);
          }
      }
      const float cardHeight = warnings.empty() ? 100.f : 106.f + 20.f * static_cast<float>(warnings.size() - 1);
      const auto box = place({base.x, base.y - (unit->type == UnitClass::Rider ? 58.f : 50.f) * zoom * scale}, detailed ? sf::Vector2f{218, cardHeight} : compact ? sf::Vector2f{38, 9}
                                                                                                                                                                  : sf::Vector2f{66, 29});
      connector({base.x, base.y - 25 * zoom * scale}, box, Teams[unit->owner]);
      labels.push_back([&, unit, stats, detailed, compact, box, warnings, rotWarningCount] {Box(box,{15,21,29,238},Teams[unit->owner]);
    const float ratio=std::clamp(static_cast<float>(unit->hp)/std::max(1,stats.hp),0.f,1.f);
    const sf::Color health=ratio>.55f?sf::Color{130,203,115}:ratio>.25f?sf::Color{228,193,88}:sf::Color{235,110,84};
    const float barY=detailed?box.position.y+32:compact?box.position.y+3:box.position.y+21;
    Box({{box.position.x+4,barY},{box.size.x-8,4}},{49,52,57});Box({{box.position.x+4,barY},{(box.size.x-8)*ratio,4}},health);
    if(detailed) {
      Text(Name(unit->type)+"  "+std::to_string(unit->hp)+"/"+std::to_string(stats.hp),box.position.x+8,box.position.y+4,18,Teams[unit->owner]);
      std::string numbers="ATQ "+std::to_string(stats.attack)+"  DEF "+std::to_string(stats.defense);
      if(unit->type==UnitClass::Caster)numbers+="  MP "+std::to_string(unit->mp)+"/"+std::to_string(game.config.maxMana);
      Text(numbers,box.position.x+8,box.position.y+39,16);
      std::string status=unit->owner==game.state.currentPlayer?(unit->readyRound>game.state.round?"REFUERZO":game.Available(*unit)?"DISPONIBLE":"ACTUADA"):"RIVAL";
      if(unit->guard)status+=" · PARADA";else if(unit->smoke)status+=" · HUMO";else if(unit->pike)status+=" · PICAS";else if(unit->marked)status+=" · MARCA";else if(unit->slowThroughRound>=game.state.round)status+=" · HIELO";
      Text(status,box.position.x+8,box.position.y+65,16,Muted);
      for(std::size_t i=0;i<warnings.size();++i)Text(warnings[i],box.position.x+8,box.position.y+82+20.f*static_cast<float>(i),16,i<rotWarningCount?gConfig.rotPalette.uiText:sf::Color{229,204,107});
    } else if(!compact)Text(std::to_string(unit->hp)+"/"+std::to_string(stats.hp),box.position.x+7,box.position.y-1,16); });
    }
    for (const auto &draw : labels)
      draw();
    for (const auto &corpse : game.state.corpses) {
      const auto base = ui(camera_.Project(corpse.pos, static_cast<float>(game.At(corpse.pos)->height)));
      const auto *dead = game.Find(corpse.unitId);
      if (!bounds.contains(base) || (dead && visuals_.VisualFor(*dead).visible))
        continue;
      const int wait = std::max(0, game.config.rotWait - (game.state.round - corpse.deathRound));
      Text("† " + std::to_string(wait) + " R", base.x - 18, base.y + 9, 16, {204, 184, 161});
    }
  }
} // Espacio de nombres Plaguefront
