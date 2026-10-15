#pragma once
#include <SFML/Window.hpp>
#include <optional>
#include <variant>
#include "Types/InputTypes.h"

namespace Input {
  struct Keyboard {
    sf::Keyboard::Scan key;
  };
  struct Mouse {
    sf::Mouse::Button button;
  };
  struct Gamepad {
    GamepadButton button;
    int id = 0;
  };
  struct Axis {
    sf::Joystick::Axis axis;
    float threshold = 0.5f;
    int id = 0;
  };

  using Binding = std::variant<Keyboard, Mouse, Gamepad, Axis>;

  std::optional<int> LogicalToHardware(GamepadButton button, int id);
  std::optional<GamepadButton> HardwareToLogical(int button, int id);
  std::array<int, 12> DefaultHardwareMapping(GamepadProfile profile, unsigned vendorId);
} // espacio de nombres Input
