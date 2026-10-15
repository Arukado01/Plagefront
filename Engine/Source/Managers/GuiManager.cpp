#include "Managers/GuiManager.h"

#include "Utils/InputBindings.h"

GuiManager::GuiManager(sf::RenderWindow &window)
    : window_(window), gui_(window) {
  tgui::Theme::setDefault("Content/Themes/Dark.txt");

  gui_.setTabKeyUsageEnabled(true);
  gui_.setFont("Content/Fonts/Relidux.otf");
}

void GuiManager::ProcessEvent(const sf::Event &event) {
  event.visit([this](const auto &type) {
    this->HandleEvent(type);
    this->gui_.handleEvent(type);
  });
}

void GuiManager::Render() { gui_.draw(); }

void GuiManager::Add(const tgui::Widget::Ptr &widget) { gui_.add(widget); }

void GuiManager::Remove(const tgui::Widget::Ptr &widget) {
  gui_.remove(widget);
}

void GuiManager::HandleEvent(const sf::Event::Resized &resized) {
  if (!resized.size.x || !resized.size.y)
    return;
  // TGUI recalcula sus controles relativos en píxeles físicos al redimensionar; la vista predeterminada de SFML puede estar obsoleta.
  gui_.setRelativeView({0, 0, 1, 1});
}

void GuiManager::HandleEvent(const sf::Event::JoystickButtonPressed &joystick) {
  if (Input::HardwareToLogical(joystick.button, joystick.joystickId) ==
      GamepadButton::South) {
    gui_.handleEvent(
        sf::Event::MouseButtonPressed{{}, sf::Mouse::getPosition(window_)});
  }
}

void GuiManager::HandleEvent(
    const sf::Event::JoystickButtonReleased &joystick) {
  if (Input::HardwareToLogical(joystick.button, joystick.joystickId) ==
      GamepadButton::South) {
    gui_.handleEvent(
        sf::Event::MouseButtonReleased{{}, sf::Mouse::getPosition(window_)});
  }
}
