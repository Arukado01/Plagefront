#pragma once
#include "Utils/InputBindings.h"
#include "Config/InputConfig.h"
#include <unordered_map>

class SaveManager;

class InputManager {
private:
  std::unordered_multimap<int, Input::Binding> bindings_;
  struct PadState {
    Input::DeviceCapabilities capabilities;
    std::array<float, sf::Joystick::AxisCount> axes{};
    std::array<bool, sf::Joystick::ButtonCount> buttons{};
    std::array<bool, 12> held{}, pending{}, pulses{};
    bool blocked{};
  };
  std::array<PadState, sf::Joystick::Count> pads_{};
  Input::GamepadConfig controls_;
  Input::Device device_{Input::Device::KeyboardMouse};
  std::optional<unsigned> activePad_;
  sf::Vector2i navigation_{}, previousNavigation_{};
  float repeatElapsed_{};
  bool repeating_{}, discover_{};
  bool focused_{true};

public:
  explicit InputManager(bool discoverDevices = true);
  void Bind(int action, const Input::Binding &binding);
  bool Pressed(int action) const;
  void Clear();
  void ProcessEvent(const sf::Event &event);
  void Update(float deltaTime);
  Input::Device Device() const { return device_; }
  bool GamepadAvailable() const { return activePad_.has_value(); }
  std::optional<unsigned> ActiveGamepad() const { return activePad_; }
  const Input::DeviceCapabilities *DeviceInfo(unsigned id) const;
  sf::Vector2i NavigationPulse() const { return navigation_; }
  sf::Vector2f PointerDirection() const;
  bool Consume(Input::PadAction action);
  void SuppressUntilReleased();
  const Input::GamepadConfig &Controls() const { return controls_; }
  void Configure(Input::GamepadConfig config);
  void LoadPreferences(SaveManager &save);
  void SavePreferences(SaveManager &save) const;
  // Comparte el registro de capacidades entre conexión dinámica y pruebas deterministas; producción consulta las capacidades reales de SFML.
  void RegisterDevice(unsigned id, Input::DeviceCapabilities capabilities);
  std::optional<unsigned> HardwareButton(GamepadButton button, unsigned id) const;

private:
  void Discover(unsigned id);
  void ReadButtons(PadState &pad);
  bool LogicalHeld(const PadState &pad, GamepadButton button) const;
  sf::Vector2i NavigationDirection() const;
  void SelectDevice(unsigned id);
  bool Pressed(const Input::Keyboard &) const;
  bool Pressed(const Input::Mouse &) const;
  bool Pressed(const Input::Gamepad &) const;
  bool Pressed(const Input::Axis &) const;
};
