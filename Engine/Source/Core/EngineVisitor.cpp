#include "Core/EngineVisitor.h"
#include "Core/Engine.h"
#include "Utils/InputBindings.h"

bool EngineVisitor::operator()(const sf::Event::Closed &) {
  if (!engine.currentScene_ || !engine.currentScene_->OnCloseRequest())
    engine.EventWindowClose();
  return true;
}

bool EngineVisitor::operator()(const sf::Event::Resized &resized) {
  engine.EventWindowResized(resized.size);
  return false;
}

bool EngineVisitor::operator()(const sf::Event::FocusLost &) {
  engine.EventWindowFocusLost();
  return false;
}

bool EngineVisitor::operator()(const sf::Event::FocusGained &) {
  engine.EventWindowFocusGained();
  return false;
}

bool EngineVisitor::operator()(const sf::Event::JoystickConnected &joystick) {
  engine.EventGamepadConnected(joystick.joystickId);
  return false;
}

bool EngineVisitor::operator()(
    const sf::Event::JoystickDisconnected &joystick) {
  engine.EventGamepadDisconnected(joystick.joystickId);
  return false;
}

bool EngineVisitor::operator()(const sf::Event::KeyPressed &key) {
  if (engine.EventFullscreenShortcut(key))
    return true;
  if (key.scancode == sf::Keyboard::Scan::F12) {
    engine.EventWindowScreenshot();
    return true;
  }
  if (engine.currentScene_ && engine.currentScene_->HandlesNavigation())
    return false;
  if (key.scancode == sf::Keyboard::Scan::R) {
    engine.EventSceneRestart();
    return true;
  }
  if (key.scancode == sf::Keyboard::Scan::M) {
    engine.EventSceneMenuReturn();
    return true;
  }
  if (key.scancode == sf::Keyboard::Scan::Escape) {
    engine.EventOverlayPauseToggle();
    return true;
  }
  return false;
}

bool EngineVisitor::operator()(const sf::Event::KeyReleased &key) {
  engine.EventShortcutReleased(key);
  return false;
}

bool EngineVisitor::operator()(
    const sf::Event::JoystickButtonPressed &joystick) {
  // Los juegos integrados usan InputManager; las demostraciones conservan sus atajos del panel de pausa.
  if (engine.currentScene_ && engine.currentScene_->HandlesNavigation())
    return false;
  if (const auto button =
          Input::HardwareToLogical(joystick.button, joystick.joystickId)) {

    if (*button == GamepadButton::Select) {
      engine.EventSceneMenuReturn();
      return true;
    } else if (*button == GamepadButton::Start) {
      engine.EventOverlayPauseToggle();
      return true;
    }
  }
  return false;
}
