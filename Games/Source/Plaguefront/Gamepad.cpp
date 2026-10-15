#include "Plaguefront/Game.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Plaguefront {
  namespace {
    const sf::Color Gold{235, 205, 128}, Muted{194, 204, 195};
  }

  void Game::FocusGamepad(sf::Vector2i direction) {
    if (buttons_.empty())
      return;
    const auto valid = [&](int index) { return index >= 0 && index < static_cast<int>(buttons_.size()) && buttons_[index].enabled; };
    if (!valid(keyboardFocus_)) {
      keyboardFocus_ = -1;
      if (screen_ == Screen::Playing)
        for (std::size_t i = 0; i < buttons_.size(); ++i)
          if (buttons_[i].enabled && buttons_[i].bounds.position.x >= UiWidth() - 360.f) {
            keyboardFocus_ = static_cast<int>(i);
            break;
          }
      if (keyboardFocus_ < 0)
        for (std::size_t i = 0; i < buttons_.size(); ++i)
          if (buttons_[i].enabled) {
            keyboardFocus_ = static_cast<int>(i);
            break;
          }
      return;
    }
    if (direction == sf::Vector2i{})
      return;
    const auto &current = buttons_[keyboardFocus_].bounds;
    const auto center = current.position + current.size / 2.f;
    const sf::Vector2f desired = sf::Vector2f(direction).normalized();
    int chosen = -1;
    float score = std::numeric_limits<float>::max();
    for (std::size_t i = 0; i < buttons_.size(); ++i) {
      if (!buttons_[i].enabled || static_cast<int>(i) == keyboardFocus_)
        continue;
      const auto delta = buttons_[i].bounds.position + buttons_[i].bounds.size / 2.f - center;
      const float forward = delta.dot(desired);
      if (forward <= 1.f)
        continue;
      const float side = std::abs(delta.cross(desired));
      const float candidate = forward + side * 3.f + side * side / std::max(1.f, forward);
      if (candidate < score) {
        score = candidate;
        chosen = static_cast<int>(i);
      }
    }
    if (chosen < 0) {
      const int increment = direction.y < 0 || direction.x < 0 ? -1 : 1;
      for (int n = 1; n <= static_cast<int>(buttons_.size()); ++n) {
        const int index = (keyboardFocus_ + increment * n + static_cast<int>(buttons_.size())) % static_cast<int>(buttons_.size());
        if (valid(index)) {
          chosen = index;
          break;
        }
      }
    }
    if (chosen >= 0 && chosen != keyboardFocus_) {
      keyboardFocus_ = chosen;
      audio_.Play("hover");
    }
  }

  void Game::MoveGamepadCursor(sf::Vector2i direction) {
    if (!match_ || !padCell_ || direction == sf::Vector2i{})
      return;
    const auto *cell = match_->At(*padCell_);
    if (!cell)
      return;
    const auto current = camera_.Project(cell->pos, static_cast<float>(cell->height));
    const sf::Vector2f desired = sf::Vector2f(direction).normalized();
    std::optional<Hex> chosen;
    float score = std::numeric_limits<float>::max();
    for (const auto neighbor : Neighbors(cell->pos)) {
      const auto *next = match_->At(neighbor);
      if (!next)
        continue;
      const auto point = camera_.Project(next->pos, static_cast<float>(next->height));
      const sf::Vector2f delta{point.x - current.x, point.y - current.y};
      const float forward = delta.dot(desired);
      if (forward <= 0.f)
        continue;
      const float side = std::abs(delta.cross(desired));
      const float candidate = side * 4.f + delta.length();
      if (candidate < score) {
        score = candidate;
        chosen = neighbor;
      }
    }
    if (!chosen)
      return;
    padCell_ = *chosen;
    hovered_ = padCell_;
    UpdatePreview(padCell_);
    const auto bounds = BoardBounds();
    auto center = Center(*padCell_);
    const sf::Vector2f inset{std::min(bounds.size.x * .25f, 20.f * camera_.Zoom() * WorldScale() + 12.f),
                             std::min(bounds.size.y * .25f, 14.f * camera_.Zoom() * WorldScale() + 12.f)};
    const sf::FloatRect safe{bounds.position + inset, bounds.size - inset * 2.f};
    if (!safe.contains(center)) {
      const auto *target = match_->At(*padCell_);
      camera_.CenterOn(*padCell_, static_cast<float>(target->height), {world_.getSize().x / 2.f, world_.getSize().y / 2.f});
    }
    SyncGamepadCursor();
  }

  void Game::SyncGamepadCursor() {
    if (ctx.input.Device() != Input::Device::Gamepad || !ctx.input.GamepadAvailable()) {
      ctx.cursor.SetVirtualPosition({});
      return;
    }
    if (screen_ == Screen::Playing && match_ && padBoard_) {
      if (!padCell_ || !match_->At(*padCell_)) {
        if (const auto *unit = match_->Find(selected_); unit && unit->Alive())
          padCell_ = unit->pos;
        else if (selectedCell_ && match_->At(*selectedCell_))
          padCell_ = selectedCell_;
        else {
          for (const auto &candidate : match_->state.units)
            if (candidate.Alive() && candidate.owner == match_->state.currentPlayer) {
              padCell_ = candidate.pos;
              break;
            }
          if (!padCell_ && !match_->state.cells.empty())
            padCell_ = match_->state.cells.front().pos;
        }
      }
      if (padCell_) {
        const auto bounds = BoardBounds();
        const sf::Vector2f inset{std::min(bounds.size.x * .25f, 20.f * camera_.Zoom() * WorldScale() + 12.f),
                                 std::min(bounds.size.y * .25f, 14.f * camera_.Zoom() * WorldScale() + 12.f)};
        const auto safe = sf::FloatRect(bounds.position + inset, bounds.size - inset * 2.f);
        if (!safe.contains(Center(*padCell_))) {
          const auto *cell = match_->At(*padCell_);
          camera_.CenterOn(*padCell_, static_cast<float>(cell->height), {world_.getSize().x / 2.f, world_.getSize().y / 2.f});
        }
        ctx.cursor.SetVirtualPosition(Center(*padCell_));
        hovered_ = padCell_;
        UpdatePreview(padCell_);
      }
    } else {
      FocusGamepad({});
      if (keyboardFocus_ >= 0 && keyboardFocus_ < static_cast<int>(buttons_.size())) {
        const auto &button = buttons_[keyboardFocus_];
        ctx.cursor.SetVirtualPosition(button.bounds.position + button.bounds.size / 2.f);
      } else
        ctx.cursor.SetVirtualPosition({});
    }
  }

  void Game::UpdateGamepad(float /* tiempo no utilizado */) {
    if (ctx.input.Device() != Input::Device::Gamepad || !ctx.input.GamepadAvailable()) {
      ctx.cursor.SetVirtualPosition({});
      return;
    }
    SyncGamepadCursor();
    const auto consume = [&](Input::PadAction action) { return ctx.input.Consume(action); };
    if (consume(Input::PadAction::Pause)) {
      if (screen_ == Screen::Playing)
        ChangeScreen(Screen::Paused);
      else if (screen_ == Screen::Paused)
        ChangeScreen(Screen::Playing);
      SyncGamepadCursor();
      return;
    }
    if (consume(Input::PadAction::Cancel)) {
      if (screen_ == Screen::Playing) {
        if (tactical_ != TacticalContext::Selection) {
          CancelContext();
          padBoard_ = tactical_ == TacticalContext::Selection;
          keyboardFocus_ = -1;
        } else if (!padBoard_) {
          padBoard_ = true;
          keyboardFocus_ = -1;
        } else
          ChangeScreen(Screen::Paused);
      } else if (screen_ == Screen::Paused)
        ChangeScreen(Screen::Playing);
      else if (screen_ == Screen::ConfirmTurn)
        ChangeScreen(Screen::Playing);
      else if (screen_ == Screen::ConfirmExit)
        ChangeScreen(exitPrevious_);
      else if (screen_ != Screen::Handoff)
        Dispatch({{}, "", Back});
      SyncGamepadCursor();
      return;
    }
    const bool playing = screen_ == Screen::Playing && match_.has_value();
    if (playing) {
      if (consume(Input::PadAction::EndTurn)) {
        FinishArmyTurn();
        SyncGamepadCursor();
        return;
      }
      if (consume(Input::PadAction::FinishUnit)) {
        FinishTurn();
        padBoard_ = false;
        keyboardFocus_ = -1;
      }
      if (consume(Input::PadAction::Center)) {
        FocusCamera(false);
        if (const auto *unit = match_->Find(selected_))
          padCell_ = unit->pos;
      }
      for (const auto action : {Input::PadAction::PreviousUnit, Input::PadAction::NextUnit})
        if (consume(action)) {
          std::vector<int> eligible;
          for (const auto &unit : match_->state.units)
            if (match_->Available(unit))
              eligible.push_back(unit.id);
          if (!eligible.empty()) {
            const auto current = std::find(eligible.begin(), eligible.end(), selected_);
            const int index = current == eligible.end() ? (action == Input::PadAction::PreviousUnit ? 0 : -1) : static_cast<int>(current - eligible.begin());
            const int step = action == Input::PadAction::PreviousUnit ? -1 : 1;
            const int next = (index + step + static_cast<int>(eligible.size())) % static_cast<int>(eligible.size());
            SelectRosterUnit(eligible[next]);
            if (const auto *unit = match_->Find(selected_))
              padCell_ = unit->pos;
            padBoard_ = false;
            keyboardFocus_ = -1;
          }
        }
      if (consume(Input::PadAction::Information) && padCell_) {
        const auto *unit = match_->Occupant(*padCell_);
        selected_ = unit ? unit->id : -1;
        selectedCell_ = padCell_;
        SetContext(unit && unit->owner == match_->state.currentPlayer ? TacticalContext::Orders : TacticalContext::Selection);
        padBoard_ = true;
        keyboardFocus_ = -1;
      }
      if (consume(Input::PadAction::Orders)) {
        if (padBoard_) {
          if (padCell_)
            if (const auto *unit = match_->Occupant(*padCell_); unit && unit->owner == match_->state.currentPlayer)
              SelectRosterUnit(unit->id);
          padBoard_ = false;
          keyboardFocus_ = -1;
          FocusGamepad({});
        } else {
          padBoard_ = true;
          keyboardFocus_ = -1;
        }
      }
      for (const auto action : {Input::PadAction::ZoomOut, Input::PadAction::ZoomIn})
        if (consume(action)) {
          const auto center = padCell_ ? Center(*padCell_) : BoardBounds().position + BoardBounds().size / 2.f;
          camera_.SetZoom(camera_.Zoom() + (action == Input::PadAction::ZoomIn ? 1 : -1), {(center.x - BoardOrigin.x) / WorldScale(), (center.y - BoardOrigin.y) / WorldScale()});
        }
    }
    const auto navigation = ctx.input.NavigationPulse();
    if (navigation != sf::Vector2i{}) {
      if (playing && padBoard_)
        MoveGamepadCursor(navigation);
      else
        FocusGamepad(navigation);
    }
    if (consume(Input::PadAction::Confirm)) {
      if (playing && padBoard_ && padCell_) {
        const auto old = tactical_;
        HandleCell(*padCell_);
        if (tactical_ == TacticalContext::Orders && (old == TacticalContext::Selection || old == TacticalContext::Move || old == TacticalContext::Target)) {
          padBoard_ = false;
          keyboardFocus_ = -1;
        }
      } else {
        FocusGamepad({});
        if (keyboardFocus_ >= 0 && keyboardFocus_ < static_cast<int>(buttons_.size())) {
          const auto button = buttons_[keyboardFocus_];
          Dispatch(button);
          if (screen_ == Screen::Playing && (tactical_ == TacticalContext::Move || tactical_ == TacticalContext::Target)) {
            padBoard_ = true;
            keyboardFocus_ = -1;
          }
        }
      }
    }
    SyncGamepadCursor();
  }

  void Game::RenderGamepadCursor() const {
    if (!match_ || screen_ != Screen::Playing || !padBoard_ || !padCell_ || ctx.input.Device() != Input::Device::Gamepad)
      return;
    const auto *cell = match_->At(*padCell_);
    if (!cell)
      return;
    const auto points = camera_.Surface(cell->pos, static_cast<float>(cell->height));
    sf::ConvexShape outline(6);
    for (std::size_t i = 0; i < points.size(); ++i)
      outline.setPoint(i, {BoardOrigin.x + points[i].x * WorldScale(), BoardOrigin.y + points[i].y * WorldScale()});
    outline.setFillColor({248, 221, 135, 22});
    outline.setOutlineColor({255, 228, 147});
    outline.setOutlineThickness(2.f / ctx.renderer.UiScale());
    ctx.renderer.Draw(outline);
  }

  void Game::AdjustControls(int action, int /* valor no utilizado */) {
    auto controls = ctx.input.Controls();
    if (action == ControlProfile)
      controls.profile = static_cast<Input::GamepadProfile>((static_cast<int>(controls.profile) + 1) % 4);
    if (action == ControlDeadzone)
      controls.deadzone = controls.deadzone >= .39f ? .12f : controls.deadzone + .05f;
    if (action == ControlSensitivity)
      controls.sensitivity = controls.sensitivity >= 1.99f ? .5f : controls.sensitivity + .25f;
    if (action == ControlInvertX)
      controls.invertX = !controls.invertX;
    if (action == ControlInvertY)
      controls.invertY = !controls.invertY;
    if (action == ControlRepeat)
      controls.repeatInterval = controls.repeatInterval >= .24f ? .08f : controls.repeatInterval + .04f;
    if (action == ControlMapping)
      controls.separateTriggers = !controls.separateTriggers;
    if (action == ControlReset)
      controls = {};
    ctx.input.Configure(controls);
    if (!smoke_)
      ctx.input.SavePreferences(ctx.save);
    message_ = "Preferencias del mando actualizadas en InputManager.";
  }

  void Game::RenderControls() const {
    const float left = UiWidth() / 2.f - 540.f, x = left + 28.f, right = left + 562.f;
    Box({{left, 60}, {1080, 790}}, sf::Color{27, 32, 42}, sf::Color{107, 102, 85});
    Text("CONTROLES DEL MANDO", x, 83, 30, Gold);
    const auto &controls = ctx.input.Controls();
    std::string device = "Sin mando: teclado y ratón disponibles";
    if (auto id = ctx.input.ActiveGamepad())
      if (const auto *info = ctx.input.DeviceInfo(*id))
        device = info->name + " · " + std::to_string(info->buttonCount) + " botones";
    Text(device, x, 128, 17, Muted);
    Text("ASIGNACIÓN LÓGICA", x, 170, 20, Gold);
    for (std::size_t i = 0; i < Input::PadActionCount; ++i) {
      const float y = 207.f + 34.f * static_cast<float>(i);
      Text(std::string(Input::ActionName(static_cast<Input::PadAction>(i))), x, y, 17);
      Text(std::string(Input::ButtonName(controls.actionButtons[i])), x + 264.f, y, 16, Gold);
    }
    Text("Stick izquierdo / cruceta: navegar", x, 633, 18, Gold);
    Wrapped("Órdenes alterna panel y tablero. Confirmar ejecuta la misma orden que el ratón; Cancelar regresa. El mando controla al ejército activo.", x, 670, 486, 17, Muted);
    ButtonAt("Perfil: " + std::string(Input::ProfileName(controls.profile)), right, 178, 486, 42, ControlProfile);
    ButtonAt("Zona muerta: " + std::to_string(static_cast<int>(std::lround(controls.deadzone * 100.f))) + "%", right, 230, 486, 42, ControlDeadzone);
    ButtonAt("Sensibilidad: " + std::to_string(static_cast<int>(std::lround(controls.sensitivity * 100.f))) + "%", right, 282, 486, 42, ControlSensitivity);
    ButtonAt(std::string("Invertir horizontal: ") + (controls.invertX ? "sí" : "no"), right, 334, 486, 42, ControlInvertX);
    ButtonAt(std::string("Invertir vertical: ") + (controls.invertY ? "sí" : "no"), right, 386, 486, 42, ControlInvertY);
    ButtonAt("Repetición: " + std::to_string(static_cast<int>(std::lround(controls.repeatInterval * 1000.f))) + " ms", right, 438, 486, 42, ControlRepeat);
    ButtonAt(controls.separateTriggers ? "Gatillos: ejes independientes" : "Gatillos: eje combinado", right, 490, 486, 42, ControlMapping);
    ButtonAt("RESTAURAR CONTROLES", right, 542, 486, 42, ControlReset);
    Wrapped("El perfil adapta botones a las capacidades del dispositivo. Los cambios se conservan en este equipo. Consulta la documentación para configurar ejes y asignaciones avanzadas.", right, 614, 486, 17, Muted);
    ButtonAt("VOLVER", x, 780, 1024, 44, Back);
  }
} // Espacio de nombres Plaguefront
