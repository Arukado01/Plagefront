#include "Plaguefront/Game.h"
#include <algorithm>
#include <cmath>

namespace Plaguefront {
  float Game::UiWidth() const { return ctx.renderer.LogicalSize().x; }
  float Game::UiHeight() const { return ctx.renderer.LogicalSize().y; }
  int Game::RosterVisibleRows() const { return std::max(1, static_cast<int>((UiHeight() - 194.f - UI::RosterTop) / UI::RosterStep)); }
  sf::FloatRect Game::BoardBounds() const { return {BoardOrigin, {UiWidth() - (UI::BoardOrigin.x + UI::OrdersWidth + 30.f), UiHeight() - 200.f}}; }
  void Game::RebuildControls() {
    std::optional<std::pair<int, int>> focus;
    if (keyboardFocus_ >= 0 && keyboardFocus_ < static_cast<int>(buttons_.size()))
      focus = std::pair{buttons_[keyboardFocus_].action, buttons_[keyboardFocus_].value};
    layoutOnly_ = true;
    try {
      Render();
    } catch (...) {
      layoutOnly_ = false;
      throw;
    }
    layoutOnly_ = false;
    if (focus) {
      const auto found = std::ranges::find_if(buttons_, [&](const Button &b) { return b.action == focus->first && b.value == focus->second; });
      keyboardFocus_ = found == buttons_.end() ? -1 : static_cast<int>(found - buttons_.begin());
    }
  }
  void Game::ToggleResolutionSelect() {
    if (resolutionOpen_) {
      CloseResolutionSelect();
      return;
    }
    if (screen_ != Screen::Settings)
      return;
    resolutionOptions_ = ctx.window.Resolutions();
    const auto current = ctx.window.Resolution();
    // Un tamaño arrastrado por el usuario también es una opción válida en ventana.
    if (!ctx.window.IsFullscreen() && std::ranges::find(resolutionOptions_, current) == resolutionOptions_.end()) {
      resolutionOptions_.push_back(current);
    }
    std::ranges::sort(resolutionOptions_, [](sf::Vector2u a, sf::Vector2u b) { return a.x == b.x ? a.y < b.y : a.x < b.x; });
    if (resolutionOptions_.empty())
      return;
    resolutionFullscreen_ = ctx.window.IsFullscreen();
    resolutionOpen_ = true;
    const auto selected = std::ranges::find(resolutionOptions_, current);
    resolutionOption_ = selected == resolutionOptions_.end() ? 0 : static_cast<int>(selected - resolutionOptions_.begin());
    resolutionScroll_ = 0;
    MoveResolutionSelect(0);
  }
  void Game::CloseResolutionSelect(bool restoreFocus) {
    resolutionOpen_ = false;
    resolutionOptions_.clear();
    keyboardFocus_ = -1;
    if (restoreFocus) {
      RebuildControls();
      const auto button = std::ranges::find_if(buttons_, [](const Button &candidate) { return candidate.action == Resolution; });
      if (button != buttons_.end())
        keyboardFocus_ = static_cast<int>(button - buttons_.begin());
    }
  }
  void Game::MoveResolutionSelect(int delta) {
    if (!resolutionOpen_ || resolutionOptions_.empty())
      return;
    resolutionOption_ = std::clamp(resolutionOption_ + delta, 0, static_cast<int>(resolutionOptions_.size()) - 1);
    const int rows = std::min(ResolutionVisibleRows, static_cast<int>(resolutionOptions_.size()));
    resolutionScroll_ = std::clamp(resolutionScroll_, std::max(0, resolutionOption_ - rows + 1), resolutionOption_);
    resolutionScroll_ = std::min(resolutionScroll_, static_cast<int>(resolutionOptions_.size()) - rows);
    keyboardFocus_ = -1;
    RebuildControls();
    const auto focused = std::ranges::find_if(buttons_, [&](const Button &candidate) { return candidate.action == ResolutionSelect && candidate.value == resolutionOption_; });
    if (focused != buttons_.end())
      keyboardFocus_ = static_cast<int>(focused - buttons_.begin());
  }
  bool Game::HandleResolutionEvent(const sf::Event &event) {
    if (!resolutionOpen_)
      return false;
    if (screen_ != Screen::Settings || resolutionFullscreen_ != ctx.window.IsFullscreen()) {
      CloseResolutionSelect();
      return false;
    }
    if (event.is<sf::Event::FocusLost>() || event.is<sf::Event::Resized>()) {
      CloseResolutionSelect();
      return false;
    }
    if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
      if (key->code == sf::Keyboard::Key::Escape) {
        CloseResolutionSelect();
        return true;
      }
      if (key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::Down || key->code == sf::Keyboard::Key::Tab) {
        MoveResolutionSelect(key->code == sf::Keyboard::Key::Up || (key->code == sf::Keyboard::Key::Tab && key->shift) ? -1 : 1);
        return true;
      }
      if (key->code == sf::Keyboard::Key::Home || key->code == sf::Keyboard::Key::End) {
        MoveResolutionSelect(key->code == sf::Keyboard::Key::Home ? -resolutionOption_ : static_cast<int>(resolutionOptions_.size()) - 1 - resolutionOption_);
        return true;
      }
      if (key->code == sf::Keyboard::Key::PageUp || key->code == sf::Keyboard::Key::PageDown) {
        MoveResolutionSelect(key->code == sf::Keyboard::Key::PageUp ? -ResolutionVisibleRows : ResolutionVisibleRows);
        return true;
      }
      if ((key->code == sf::Keyboard::Key::Enter || key->code == sf::Keyboard::Key::Space) && !key->alt) {
        Dispatch({{}, "", ResolutionSelect, resolutionOption_});
        return true;
      }
    }
    if (const auto *wheel = event.getIf<sf::Event::MouseWheelScrolled>(); wheel && wheel->delta != 0) {
      MoveResolutionSelect(wheel->delta > 0 ? -1 : 1);
      return true;
    }
    if (const auto *moved = event.getIf<sf::Event::MouseMoved>()) {
      const auto position = ctx.renderer.PixelToLogical(moved->position);
      for (std::size_t i = 0; i < buttons_.size(); ++i)
        if (buttons_[i].action == ResolutionSelect && buttons_[i].bounds.contains(position)) {
          resolutionOption_ = buttons_[i].value;
          keyboardFocus_ = static_cast<int>(i);
          break;
        }
    }
    if (const auto *mouse = event.getIf<sf::Event::MouseButtonPressed>()) {
      if (mouse->button == sf::Mouse::Button::Right) {
        CloseResolutionSelect();
        return true;
      }
      if (mouse->button == sf::Mouse::Button::Left) {
        const auto position = ctx.renderer.PixelToLogical(mouse->position);
        for (const auto &button : buttons_)
          if (button.bounds.contains(position)) {
            Dispatch(button);
            return true;
          }
        // El clic que cierra la lista no activa el control que había debajo.
        CloseResolutionSelect();
        return true;
      }
    }
    return false;
  }
  float Game::WorldScale() const { return static_cast<float>(worldPixelScale_) / ctx.renderer.UiScale(); }
  void Game::RefreshLayout(bool frame) {
    const auto bounds = BoardBounds();
    const float scale = WorldScale();
    const sf::Vector2u size{static_cast<unsigned>(std::floor(bounds.size.x / scale)), static_cast<unsigned>(std::floor(bounds.size.y / scale))};
    if (size != world_.getSize()) {
      const auto old = world_.getSize();
      if (!world_.resize(size))
        throw std::runtime_error("No se pudo crear el lienzo del tablero.");
      world_.setSmooth(false);
      if (match_ && !frame)
        camera_.PanBy({(static_cast<float>(size.x) - old.x) / 2.f, (static_cast<float>(size.y) - old.y) / 2.f});
    }
    if (frame && match_)
      camera_.Fit(match_->state.cells, static_cast<float>(size.x), static_cast<float>(size.y));
    if (layoutRevision_ != ctx.renderer.Revision()) {
      layoutRevision_ = ctx.renderer.Revision();
      RebuildControls();
      SyncGamepadCursor();
    }
  }
  void Game::FocusCamera(bool all) {
    if (!match_)
      return;
    worldPixelScale_ = all ? 1u : 2u;
    RefreshLayout();
    if (all)
      camera_.FrameAll(match_->state.cells, static_cast<float>(world_.getSize().x), static_cast<float>(world_.getSize().y));
    else {
      const Unit *unit = match_->Find(selected_);
      Hex focus = unit && unit->Alive() ? unit->pos : Hex{};
      if (!unit)
        for (const auto &cell : match_->state.cells)
          if (cell.building == Building::Fortress && cell.owner == match_->state.currentPlayer) {
            focus = cell.pos;
            break;
          }
      const auto *cell = match_->At(focus);
      camera_.CenterOn(focus, cell ? static_cast<float>(cell->height) : 0.f, {world_.getSize().x / 2.f, world_.getSize().y / 2.f});
    }
  }
  void Game::SetContext(TacticalContext context) {
    tactical_ = context;
    buttons_.clear();
    keyboardFocus_ = -1;
    previewPath_.reset();
    previewDestination_.reset();
    previewRevision_ = ~std::uint64_t{0};
    if (context != TacticalContext::Target) {
      ability_.reset();
      itemSlot_ = -1;
    }
  }
  void Game::CancelContext() {
    audio_.Play("cancel");
    if (tactical_ == TacticalContext::Purchase)
      SetContext(purchaseReturn_);
    else if (tactical_ == TacticalContext::Recruitment || tactical_ == TacticalContext::Shop) {
      if (customBattalionEditing_)
        customBattalionEditing_ = false;
      else
        SetContext(TacticalContext::Building);
    } else if (tactical_ == TacticalContext::Building) {
      SetContext(TacticalContext::Selection);
      selected_ = -1;
    } else if (tactical_ != TacticalContext::Selection && tactical_ != TacticalContext::Orders)
      SetContext(TacticalContext::Orders);
    else {
      SetContext(TacticalContext::Selection);
      selected_ = -1;
    }
  }
  void Game::UpdatePreview(std::optional<Hex> destination) {
    if (!match_)
      return;
    const auto *unit = match_->Find(selected_);
    const bool allowed = (tactical_ == TacticalContext::Orders || tactical_ == TacticalContext::Move) && unit && unit->Alive() && unit->owner == match_->state.currentPlayer && !visuals_.Busy() &&
                         (!unit->activated || match_->state.activeUnit == selected_) && unit->readyRound <= match_->state.round;
    if (!allowed) {
      previewPath_.reset();
      reachable_.clear();
      previewDestination_.reset();
      previewRevision_ = ~std::uint64_t{0};
      return;
    }
    if (previewUnit_ != selected_ || previewRevision_ != matchRevision_) {
      reachable_ = match_->Reachable(selected_);
      previewUnit_ = selected_;
      previewRevision_ = matchRevision_;
      previewDestination_.reset();
      previewPath_.reset();
    }
    if (destination == previewDestination_)
      return;
    previewDestination_ = destination;
    previewPath_.reset();
    if (destination && *destination != unit->pos && reachable_.contains(*destination))
      previewPath_ = match_->FindPath(selected_, *destination);
  }
  const Match &Game::PreviewRules() const {
    if (!targetingRules_ || targetingRevision_ != matchRevision_ || targetingUnit_ != selected_) {
      targetingRules_ = *match_;
      targetingRevision_ = matchRevision_;
      targetingUnit_ = selected_;
      if (targetingRules_->state.activeUnit != selected_)
        targetingRules_->BeginActivation(selected_);
    }
    return *targetingRules_;
  }
  Result Game::CheckAction(int action, int value) const {
    if (!match_)
      return {false, "No hay una partida."};
    const auto &state = match_->state;
    const LegalityKey key{matchRevision_, selected_, state.activeUnit, state.currentPlayer, state.round,
                          state.players[0].coins, state.players[1].coins, selectedCell_, visuals_.Busy(), customBattalion_};
    if (!legalityKey_ || *legalityKey_ != key) {
      legalityKey_ = key;
      legalityCache_.clear();
    }
    const auto command = std::pair{action, value};
    if (const auto cached = legalityCache_.find(command); cached != legalityCache_.end())
      return cached->second;
    // Consulta órdenes reales sobre copias y conserva el resultado mientras no cambien selección, estado, animación, economía o composición.
    const auto compute = [&]() -> Result {
      if (key.busy)
        return {false, "Espera a que termine la animación."};
      if (state.mode == Mode::VersusAI && state.currentPlayer == 1)
        return {false, "Turno de la IA."};
      const auto *property = selectedCell_ ? match_->At(*selectedCell_) : nullptr;
      const bool controlled = property && property->owner == state.currentPlayer &&
                              (property->building == Building::Fortress || property->building == Building::Outpost);
      if (action == Tab)
        return {controlled, controlled ? "" : "Selecciona una fortaleza o fuerte propio."};
      if (action == Recruit || action == Battalion || action == CustomConfirm || action == Buy)
        if (!controlled)
          return {false, "Selecciona una fortaleza o fuerte propio."};
      Match test = *match_;
      auto *unit = test.Find(selected_);
      if (action == Battalion || action == CustomConfirm) {
        if (action == Battalion && value == 3)
          return {true, ""};
        if (action == Battalion && (value < 0 || value > 2))
          return {false, "Batallón desconocido."};
        const std::array<std::vector<UnitClass>, 3> templates{{{UnitClass::Saber, UnitClass::Lancer, UnitClass::Berserker}, {UnitClass::Rider, UnitClass::Archer, UnitClass::Assassin}, {UnitClass::Caster, UnitClass::Saber}}};
        return test.RecruitBattalion(state.currentPlayer, action == CustomConfirm ? customBattalion_ : templates[static_cast<std::size_t>(value)], *selectedCell_);
      }
      if (action == Recruit) {
        if (value < 0 || value > 6)
          return {false, "Clase desconocida."};
        return test.Recruit(state.currentPlayer, static_cast<UnitClass>(value), *selectedCell_);
      }
      if (action == Buy) {
        if (value < 0 || value > 4)
          return {false, "Objeto desconocido."};
        if (!unit || !unit->Alive() || unit->owner != state.currentPlayer || Distance(unit->pos, property->pos) > 1)
          return {false, "Elige una unidad propia junto a este edificio."};
        return test.BuyItem(selected_, static_cast<Item>(value));
      }
      if (!unit || !unit->Alive() || unit->owner != state.currentPlayer)
        return {false, "Selecciona una unidad propia."};
      if (test.state.activeUnit != selected_) {
        auto start = test.BeginActivation(selected_);
        if (!start)
          return start;
      }
      // La recuperación inicial de maná también se simula sin alterar la partida real.
      unit = test.Find(selected_);
      if (action == Capture)
        return test.Capture(selected_);
      if (action == Build || action == ConfirmBuild)
        return test.BuildOutpost(selected_);
      if (action == OpenMove) {
        const auto cells = test.Reachable(selected_);
        return {cells.size() > 1, cells.size() > 1 ? "" : "Movimiento consumido o sin destinos legales."};
      }
      if (action == UseItem) {
        if (value < 0 || value >= static_cast<int>(unit->inventory.size()))
          return {false, "No existe ese espacio del inventario."};
        auto self = test;
        Result reason = self.UseItem(selected_, value, unit->pos);
        if (reason)
          return reason;
        if (unit->inventory[static_cast<std::size_t>(value)] == Item::Purifier) {
          for (const auto &source : test.state.sources) {
            auto candidate = test;
            const auto legal = candidate.UseItem(selected_, value, source.pos);
            if (legal)
              return legal;
            reason = legal;
          }
        } else
          for (const auto &target : test.state.units)
            if (target.Alive() && target.owner == unit->owner && target.id != selected_) {
              auto candidate = test;
              const auto legal = candidate.UseItem(selected_, value, target.pos);
              if (legal)
                return legal;
            }
        return reason;
      }
      if (action == AbilitySelect) {
        if (value < 0 || value > static_cast<int>(Ability::Incinerate))
          return {false, "Habilidad desconocida."};
        const auto ability = static_cast<Ability>(value);
        auto self = test;
        Result reason = self.UseAbility(selected_, ability, unit->pos);
        if (reason)
          return reason;
        if (ability == Ability::Purify)
          for (const auto &source : test.state.sources) {
            auto candidate = test;
            const auto legal = candidate.UseAbility(selected_, ability, source.pos);
            if (legal)
              return legal;
            reason = legal;
          }
        else if (ability == Ability::Incinerate)
          for (const auto &corpse : test.state.corpses) {
            auto candidate = test;
            const auto legal = candidate.UseAbility(selected_, ability, corpse.pos);
            if (legal)
              return legal;
            reason = legal;
          }
        else
          for (const auto &target : test.state.units)
            if (test.CanTarget(selected_, target.id, ability)) {
              auto candidate = test;
              const auto legal = candidate.UseAbility(selected_, ability, target.pos);
              if (legal)
                return legal;
              reason = legal;
            }
        return reason;
      }
      return {true, ""};
    };
    const auto result = compute();
    legalityCache_.emplace(command, result);
    return result;
  }
} // Espacio de nombres Plaguefront
