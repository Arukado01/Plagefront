#pragma once
#include "Types/InputTypes.h"

namespace Input {
  struct GamepadConfig {
    float deadzone{0.22f}, sensitivity{1.f}, initialRepeat{0.32f}, repeatInterval{0.12f};
    bool invertX{}, invertY{}, povYUpPositive{true};
    sf::Joystick::Axis stickX{sf::Joystick::Axis::X}, stickY{sf::Joystick::Axis::Y};
    sf::Joystick::Axis triggerAxis{sf::Joystick::Axis::Z};
    // Los gatillos Xbox combinados usan +Z izquierdo y −Z derecho; otros dispositivos pueden asignar ejes independientes.
    sf::Joystick::Axis rightTriggerAxis{sf::Joystick::Axis::R};
    bool separateTriggers{}, triggerInverted{};
    GamepadProfile profile{GamepadProfile::Auto};
    // −2 utiliza la asignación del perfil; −1 desactiva el botón lógico.
    std::array<int, 12> hardwareButtons{-2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2, -2};
    std::array<GamepadButton, PadActionCount> actionButtons{
        GamepadButton::South, GamepadButton::East, GamepadButton::West, GamepadButton::North,
        GamepadButton::L1, GamepadButton::R1, GamepadButton::R3, GamepadButton::L2,
        GamepadButton::R2, GamepadButton::L3, GamepadButton::Select, GamepadButton::Start};
    bool operator==(const GamepadConfig &) const = default;
  };
  inline constexpr std::string_view PreferencesKey = "Engine:Input";
} // espacio de nombres Input
