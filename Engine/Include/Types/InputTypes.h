#pragma once
#include <SFML/System/Vector2.hpp>
#include <SFML/Window/Joystick.hpp>
#include <array>
#include <optional>
#include <string>
#include <string_view>

// Conserva el espacio de nombres global para los consumidores existentes de Clicker e InputBindings.
enum class GamepadButton { West,
                           South,
                           East,
                           North,
                           L1,
                           R1,
                           L2,
                           R2,
                           Select,
                           Start,
                           L3,
                           R3 };

namespace Input {
  enum class Device { KeyboardMouse,
                      Gamepad };
  enum class GamepadProfile { Auto,
                              Xbox,
                              PlayStation,
                              Generic };
  enum class PadAction { Confirm,
                         Cancel,
                         Orders,
                         Information,
                         PreviousUnit,
                         NextUnit,
                         Center,
                         ZoomOut,
                         ZoomIn,
                         FinishUnit,
                         EndTurn,
                         Pause,
                         Count };
  inline constexpr std::size_t PadActionCount = static_cast<std::size_t>(PadAction::Count);
  struct DeviceCapabilities {
    bool connected{};
    unsigned buttonCount{}, vendorId{}, productId{};
    std::string name;
    std::array<bool, sf::Joystick::AxisCount> axes{};
  };
  std::string_view ActionName(PadAction action);
  std::string_view ButtonName(GamepadButton button);
  std::string_view ProfileName(GamepadProfile profile);
} // espacio de nombres Input
