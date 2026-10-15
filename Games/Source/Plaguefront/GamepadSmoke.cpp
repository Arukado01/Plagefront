#include "Plaguefront/Game.h"
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace Plaguefront {
  namespace {
    Input::DeviceCapabilities SimulatedPad() {
      Input::DeviceCapabilities info;
      info.connected = true;
      info.vendorId = 0x045e;
      info.buttonCount = 10;
      info.name = "Mando simulado · eventos SFML";
      for (const auto axis : {sf::Joystick::Axis::X, sf::Joystick::Axis::Y, sf::Joystick::Axis::Z, sf::Joystick::Axis::PovX, sf::Joystick::Axis::PovY})
        info.axes[static_cast<unsigned>(axis)] = true;
      return info;
    }
  } // Espacio de nombres
  void Game::SmokeGamepad() {
    int checks = 0;
    const auto require = [&](bool valid, const char *message) {++checks;if(!valid)throw std::runtime_error(message); };
    const auto originalControls = ctx.input.Controls();
    // Inyecta foco en el manager para la ventana oculta de pruebas; el juego sigue ignorando órdenes sin foco.
    ctx.input.ProcessEvent(sf::Event::FocusGained{});
    ctx.input.Configure({});
    ctx.input.RegisterDevice(7, SimulatedPad());
    ctx.input.Update(.016f);
    const auto frame = [&] {ctx.input.Update(.016f);UpdateGamepad(.016f);Render(); };
    const auto feed = [&](const sf::Event &event) {ctx.input.ProcessEvent(event);OnEvent(event);frame(); };
    const auto tap = [&](GamepadButton logical) {
      const auto hardware = ctx.input.HardwareButton(logical, 7);
      require(hardware.has_value(), "Gamepad UI: test action has no physical button");
      feed(sf::Event::JoystickButtonPressed{7, *hardware});
      feed(sf::Event::JoystickButtonReleased{7, *hardware});
    };
    const auto choose = [&](int action, int value = 0) {
      Render();
      const auto button = std::ranges::find_if(buttons_, [&](const Button &b) { return b.action == action && b.value == value && b.enabled; });
      if (button == buttons_.end()) {
        std::string listed;
        for (const auto &candidate : buttons_)
          listed += " [" + std::to_string(candidate.action) + ":" + std::to_string(candidate.value) + (candidate.enabled ? " enabled " : " disabled ") + candidate.label + "]";
        throw std::runtime_error("Gamepad UI: requested legal button missing action=" + std::to_string(action) + " value=" + std::to_string(value) + " screen=" + std::to_string(static_cast<int>(screen_)) + " context=" + std::to_string(static_cast<int>(tactical_)) + " message=" + message_ + listed);
      }
      require(button != buttons_.end(), "Gamepad UI: requested legal button missing");
      keyboardFocus_ = static_cast<int>(button - buttons_.begin());
      padBoard_ = false;
      tap(GamepadButton::South);
    };
    const auto local = [&] {
      mode_ = Mode::Local;
      scenario_ = 0;
      LoadArmies();
      BeginGame();
      visuals_.Update(10.f);
      padCell_.reset();
      padBoard_ = true;
      ctx.input.Update(.016f);
      Render();
    };
    const auto select = [&](int id) {
      SetContext(TacticalContext::Selection);
      selected_ = -1;
      padCell_ = match_->Find(id)->pos;
      padBoard_ = true;
      keyboardFocus_ = -1;
      tap(GamepadButton::South);
      require(selected_ == id && tactical_ == TacticalContext::Orders, "Gamepad UI: board selection failed");
    };
    try {
      match_.reset();
      ChangeScreen(Screen::Menu);
      Render();
      ctx.input.Update(.016f);
      feed(sf::Event::JoystickMoved{7, sf::Joystick::Axis::Y, 80.f});
      require(ctx.input.Device() == Input::Device::Gamepad && keyboardFocus_ >= 0, "Gamepad UI: menu did not gain controller focus");
      const int focus = keyboardFocus_;
      ctx.input.Update(.03f);
      UpdateGamepad(.03f);
      require(keyboardFocus_ == focus, "Gamepad UI: menu repeated before controller delay");
      ctx.input.Update(.4f);
      UpdateGamepad(.4f);
      require(keyboardFocus_ != focus, "Gamepad UI: timed menu repeat did not move focus");
      feed(sf::Event::JoystickMoved{7, sf::Joystick::Axis::Y, 0.f});
      choose(Play);
      require(screen_ == Screen::Setup, "Gamepad UI: main menu confirm failed");
      choose(ModeSelect, static_cast<int>(Mode::VersusAI));
      choose(DifficultySelect, static_cast<int>(Difficulty::Normal));
      choose(ModeSelect, static_cast<int>(Mode::Local));
      choose(ScenarioSelect, 0);
      const auto armyClass = armies_[0][0];
      choose(ArmySelect, 0);
      require(armies_[0][0] != armyClass, "Gamepad UI: army setup did not change class");
      choose(StartGame);
      require(screen_ == Screen::Playing && match_->state.mode == Mode::Local, "Gamepad UI: setup did not start local match");
      const int id = match_->state.units.front().id;
      select(id);
      require(match_->state.activeUnit < 0, "Gamepad UI: inspecting unit consumed activation");
      choose(OpenMove);
      require(tactical_ == TacticalContext::Move && padBoard_, "Gamepad UI: movement did not transfer focus to board");
      const Hex before = match_->Find(id)->pos;
      feed(sf::Event::JoystickMoved{7, sf::Joystick::Axis::X, 85.f});
      require(padCell_ && *padCell_ != before && match_->At(*padCell_), "Gamepad UI: tactical cursor did not move to actual adjacent cell");
      feed(sf::Event::JoystickMoved{7, sf::Joystick::Axis::X, 0.f});
      require(!reachable_.empty(), "Gamepad UI: selected movement range missing");
      const auto destination = std::ranges::find_if(reachable_, [&](const auto &entry) { return entry.first != before && !match_->Occupant(entry.first); });
      require(destination != reachable_.end(), "Gamepad UI: no legal movement fixture");
      padCell_ = destination->first;
      SyncGamepadCursor();
      require(previewPath_ && previewPath_->cells.back() == *padCell_, "Gamepad UI: cursor did not preview shared legal path");
      const Hex target = *padCell_;
      tap(GamepadButton::South);
      require(match_->Find(id)->pos == target && match_->Find(id)->moved, "Gamepad UI: confirm did not execute shared movement");
      require(match_->state.currentPlayer == 0, "Gamepad UI: movement handed army over");
      visuals_.Update(10.f);
      Render();
      choose(OpenAbilities);
      require(tactical_ == TacticalContext::Abilities, "Gamepad UI: ability menu did not open");
      tap(GamepadButton::East);
      require(tactical_ == TacticalContext::Orders, "Gamepad UI: cancel did not return to orders");
      tap(GamepadButton::R1);
      require(selected_ != id && match_->Available(*match_->Find(selected_)), "Gamepad UI: available-unit cycle failed");
      tap(GamepadButton::L1);
      require(selected_ == id, "Gamepad UI: previous-unit cycle failed");
      tap(GamepadButton::L3);
      require(match_->Find(id)->activated && match_->state.currentPlayer == 0, "Gamepad UI: end activation did not use same army rules");
      tap(GamepadButton::Select);
      require(screen_ == Screen::ConfirmTurn, "Gamepad UI: army end bypassed unused-unit confirmation");
      tap(GamepadButton::East);
      require(screen_ == Screen::Playing && match_->state.currentPlayer == 0, "Gamepad UI: cancelling army end lost context");
      tap(GamepadButton::Select);
      choose(ConfirmArmyEnd);
      require(screen_ == Screen::Handoff && match_->state.currentPlayer == 1, "Gamepad UI: local army handoff missing");
      // Mantener Confirmar durante el cambio de mando no debe cerrar el aviso.
      feed(sf::Event::JoystickButtonPressed{7, 0});
      require(screen_ == Screen::Playing, "Gamepad UI: next player could not explicitly accept handoff");
      ctx.input.Update(.4f);
      UpdateGamepad(.4f);
      require(screen_ == Screen::Playing && selected_ < 0, "Gamepad UI: held handoff button selected next unit");
      feed(sf::Event::JoystickButtonReleased{7, 0});
      const auto red = std::ranges::find_if(match_->state.units, [&](const Unit &u) { return u.owner == 1 && match_->Available(u); });
      require(red != match_->state.units.end(), "Gamepad UI: second army unavailable");
      select(red->id);
      tap(GamepadButton::Start);
      require(screen_ == Screen::Paused, "Gamepad UI: Start did not pause integrated scene");
      choose(Settings);
      choose(Controls);
      require(screen_ == Screen::Controls, "Gamepad UI: controls screen inaccessible");
      const float oldDeadzone = ctx.input.Controls().deadzone;
      choose(ControlDeadzone);
      require(ctx.input.Controls().deadzone != oldDeadzone, "Gamepad UI: controller preference did not change");
      choose(Back);
      require(screen_ == Screen::Settings, "Gamepad UI: controls back did not preserve settings");
      choose(Back);
      require(screen_ == Screen::Paused, "Gamepad UI: settings back lost pause parent");
      choose(Resume);

      // Prepara posiciones y objetos y ejecuta combate e inventario por la ruta común InputManager → Dispatch.
      local();
      auto actor = std::ranges::find_if(match_->state.units, [](const Unit &u) { return u.owner == 0 && u.type == UnitClass::Saber; });
      auto defender = std::ranges::find_if(match_->state.units, [](const Unit &u) { return u.owner == 1 && u.type == UnitClass::Saber; });
      require(actor != match_->state.units.end() && defender != match_->state.units.end(), "Gamepad UI: combat fixture missing Saber");
      actor->pos = {0, 4};
      defender->pos = {1, 4};
      const int actorId = actor->id, defenderId = defender->id;
      const int hp = defender->hp;
      ResetPresentation();
      select(actorId);
      choose(AbilitySelect, static_cast<int>(Ability::Attack));
      require(tactical_ == TacticalContext::Target && padBoard_, "Gamepad UI: attack target did not activate board cursor");
      padCell_ = match_->Find(defenderId)->pos;
      SyncGamepadCursor();
      tap(GamepadButton::South);
      require(match_->Find(defenderId)->hp < hp && match_->Find(actorId)->attacked, "Gamepad UI: shared legal attack did not execute");
      visuals_.Update(10.f);
      local();
      const int buyer = match_->state.units.front().id;
      match_->Find(buyer)->inventory = {Item::Potion};
      match_->Find(buyer)->hp -= 5;
      ResetPresentation();
      select(buyer);
      choose(OpenInventory);
      require(tactical_ == TacticalContext::Inventory, "Gamepad UI: inventory navigation failed");
      choose(UseItem, 0);
      require(itemSlot_ == 0 && padBoard_, "Gamepad UI: item target selection missing");
      padCell_ = match_->Find(buyer)->pos;
      SyncGamepadCursor();
      const int injured = match_->Find(buyer)->hp;
      tap(GamepadButton::South);
      require(match_->Find(buyer)->hp > injured && match_->Find(buyer)->inventory.empty(), "Gamepad UI: item did not use shared legality/action");
      visuals_.Update(10.f);
      local();
      const auto fortress = std::ranges::find_if(match_->state.cells, [](const Cell &cell) { return cell.building == Building::Fortress && cell.owner == 0; });
      require(fortress != match_->state.cells.end(), "Gamepad UI: own property fixture missing");
      selectedCell_ = fortress->pos;
      SetContext(TacticalContext::Building);
      padBoard_ = false;
      Render();
      choose(Tab, 1);
      require(tactical_ == TacticalContext::Recruitment, "Gamepad UI: recruitment menu inaccessible");
      choose(Recruit, static_cast<int>(UnitClass::Saber));
      require(tactical_ == TacticalContext::Purchase, "Gamepad UI: recruitment did not request purchase confirmation");
      const auto count = match_->state.units.size();
      choose(ConfirmPurchase);
      require(match_->state.units.size() == count + 1, "Gamepad UI: recruitment purchase confirm failed");
      visuals_.Update(10.f);
      tap(GamepadButton::East);
      require(tactical_ == TacticalContext::Building, "Gamepad UI: recruitment cancel did not return to property");
      choose(Tab, 2);
      require(tactical_ == TacticalContext::Shop, "Gamepad UI: shop submenu inaccessible");
      tap(GamepadButton::East);

      // Cambiar de dispositivo conserva la partida, el contexto táctico y la selección.
      const int selected = selected_, round = match_->state.round;
      const auto context = tactical_;
      feed(sf::Event::JoystickDisconnected{7});
      require(ctx.input.Device() == Input::Device::KeyboardMouse && !ctx.input.DeviceInfo(7)->connected, "Gamepad UI: disconnect did not restore fallback");
      require(selected_ == selected && tactical_ == context && match_->state.round == round, "Gamepad UI: disconnect changed tactical state");
      ctx.input.RegisterDevice(7, SimulatedPad());
      feed(sf::Event::JoystickMoved{7, sf::Joystick::Axis::X, 80});
      require(ctx.input.Device() == Input::Device::Gamepad, "Gamepad UI: hot reconnect did not regain control");
      feed(sf::Event::JoystickMoved{7, sf::Joystick::Axis::X, 0});
      feed(sf::Event::MouseMoved{{40, 40}});
      require(ctx.input.Device() == Input::Device::KeyboardMouse && selected_ == selected, "Gamepad UI: mouse handover lost selection");
      feed(sf::Event::KeyPressed{});
      require(ctx.input.Device() == Input::Device::KeyboardMouse, "Gamepad UI: keyboard fallback unavailable");
      std::cout << "Plaguefront gamepad UI: " << checks << " checks using shared InputManager and simulated SFML events; no physical controller test\n";
    } catch (...) {
      ctx.input.RegisterDevice(7, {});
      ctx.input.Configure(originalControls);
      ctx.cursor.SetVirtualPosition({});
      throw;
    }
    ctx.input.RegisterDevice(7, {});
    ctx.input.Configure(originalControls);
    ctx.cursor.SetVirtualPosition({});
    padCell_.reset();
    padBoard_ = true;
  }

  void Game::PrepareGamepadCapture(int kind) {
    ctx.input.ProcessEvent(sf::Event::FocusGained{});
    ctx.input.Configure({});
    ctx.input.RegisterDevice(7, SimulatedPad());
    ctx.input.Update(.016f);
    if (kind == 0) {
      message_.clear();
      ChangeScreen(Screen::Menu);
    } else {
      mode_ = Mode::Local;
      scenario_ = 0;
      LoadArmies();
      BeginGame();
      visuals_.Update(10.f);
      padCell_ = match_->state.units.front().pos;
      padBoard_ = true;
      Render();
      ctx.input.Update(.016f);
      if (kind == 1) {
        ctx.input.ProcessEvent(sf::Event::JoystickButtonPressed{7, 0});
        ctx.input.Update(.016f);
        UpdateGamepad(.016f);
        ctx.input.ProcessEvent(sf::Event::JoystickButtonReleased{7, 0});
        ctx.input.Update(.016f);
        UpdateGamepad(.016f);
      } else if (kind == 3)
        ChangeScreen(Screen::Controls);
    }
    Render();
    ctx.input.Update(.016f);
    ctx.input.ProcessEvent(sf::Event::JoystickMoved{7, sf::Joystick::Axis::Y, 80});
    ctx.input.Update(.016f);
    UpdateGamepad(.016f);
    ctx.input.ProcessEvent(sf::Event::JoystickMoved{7, sf::Joystick::Axis::Y, 0});
    ctx.input.Update(.016f);
    UpdateGamepad(.016f);
    Render();
    SyncGamepadCursor();
    if (kind == 2)
      message_ = "Cursor táctico: stick / cruceta recorre celdas; A selecciona; X alterna tablero y órdenes.";
  }
} // Espacio de nombres Plaguefront
