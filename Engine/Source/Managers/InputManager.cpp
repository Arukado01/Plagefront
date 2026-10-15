#include "Managers/InputManager.h"
#include "Managers/SaveManager.h"
#include <algorithm>
#include <cmath>

namespace {
  std::size_t AxisIndex(sf::Joystick::Axis axis) { return static_cast<std::size_t>(axis); }
  int Sign(float value, float threshold) { return value > threshold ? 1 : value < -threshold ? -1
                                                                                             : 0; }
} // espacio de nombres

InputManager::InputManager(bool discoverDevices) : discover_(discoverDevices) {
  if (discover_) {
    sf::Joystick::update();
    for (unsigned id = 0; id < sf::Joystick::Count; ++id)
      Discover(id);
  }
}
void InputManager::Bind(int action, const Input::Binding &binding) { bindings_.emplace(action, binding); }
bool InputManager::Pressed(int action) const {
  const auto [first, last] = bindings_.equal_range(action);
  return std::any_of(first, last, [this](const auto &entry) { return std::visit([this](const auto &binding) { return Pressed(binding); }, entry.second); });
}
void InputManager::Clear() {
  bindings_.clear();
  SuppressUntilReleased();
}
bool InputManager::Pressed(const Input::Keyboard &binding) const { return sf::Keyboard::isKeyPressed(binding.key); }
bool InputManager::Pressed(const Input::Mouse &binding) const { return sf::Mouse::isButtonPressed(binding.button); }
bool InputManager::Pressed(const Input::Gamepad &binding) const {
  if (binding.id < 0 || binding.id >= static_cast<int>(pads_.size()))
    return false;
  const auto &pad = pads_[binding.id];
  return pad.capabilities.connected && !pad.blocked && LogicalHeld(pad, binding.button);
}
bool InputManager::Pressed(const Input::Axis &binding) const {
  if (binding.id < 0 || binding.id >= static_cast<int>(pads_.size()) || AxisIndex(binding.axis) >= sf::Joystick::AxisCount)
    return false;
  const auto &pad = pads_[binding.id];
  if (!pad.capabilities.connected || !pad.capabilities.axes[AxisIndex(binding.axis)] || pad.blocked)
    return false;
  const float value = pad.axes[AxisIndex(binding.axis)];
  return binding.threshold >= 0 ? value > binding.threshold : value < binding.threshold;
}
void InputManager::Discover(unsigned id) {
  if (id >= pads_.size())
    return;
  Input::DeviceCapabilities info;
  info.connected = sf::Joystick::isConnected(id);
  if (info.connected) {
    const auto identification = sf::Joystick::getIdentification(id);
    info.name = identification.name.toAnsiString();
    info.vendorId = identification.vendorId;
    info.productId = identification.productId;
    info.buttonCount = sf::Joystick::getButtonCount(id);
    for (unsigned a = 0; a < sf::Joystick::AxisCount; ++a)
      info.axes[a] = sf::Joystick::hasAxis(id, static_cast<sf::Joystick::Axis>(a));
  }
  RegisterDevice(id, info);
  if (info.connected) {
    auto &pad = pads_[id];
    for (unsigned a = 0; a < sf::Joystick::AxisCount; ++a)
      if (info.axes[a])
        pad.axes[a] = sf::Joystick::getAxisPosition(id, static_cast<sf::Joystick::Axis>(a)) / 100.f;
    for (unsigned b = 0; b < info.buttonCount; ++b)
      pad.buttons[b] = sf::Joystick::isButtonPressed(id, b);
    ReadButtons(pad);
    pad.pending.fill(false);
  }
}
void InputManager::RegisterDevice(unsigned id, Input::DeviceCapabilities info) {
  if (id >= pads_.size())
    return;
  info.buttonCount = std::min(info.buttonCount, sf::Joystick::ButtonCount);
  pads_[id] = {};
  pads_[id].capabilities = std::move(info);
  if (pads_[id].capabilities.connected) {
    if (!activePad_)
      activePad_ = id;
  } else if (activePad_ == id) {
    activePad_.reset();
    for (unsigned candidate = 0; candidate < pads_.size(); ++candidate)
      if (pads_[candidate].capabilities.connected) {
        activePad_ = candidate;
        break;
      }
    device_ = Input::Device::KeyboardMouse;
    navigation_ = previousNavigation_ = {};
    repeatElapsed_ = 0;
    repeating_ = false;
  }
}
const Input::DeviceCapabilities *InputManager::DeviceInfo(unsigned id) const { return id < pads_.size() ? &pads_[id].capabilities : nullptr; }
std::optional<unsigned> InputManager::HardwareButton(GamepadButton button, unsigned id) const {
  const auto index = static_cast<std::size_t>(button);
  if (id >= pads_.size() || index >= 12 || !pads_[id].capabilities.connected)
    return {};
  const auto &info = pads_[id].capabilities;
  const int configured = controls_.hardwareButtons[index];
  const int hardware = configured == -2 ? Input::DefaultHardwareMapping(controls_.profile, info.vendorId)[index] : configured;
  if (hardware < 0 || static_cast<unsigned>(hardware) >= info.buttonCount)
    return {};
  return static_cast<unsigned>(hardware);
}
bool InputManager::LogicalHeld(const PadState &pad, GamepadButton button) const {
  const unsigned id = static_cast<unsigned>(&pad - pads_.data());
  if (auto hardware = HardwareButton(button, id))
    return pad.buttons[*hardware];
  const auto index = static_cast<std::size_t>(button);
  if (index >= 12 || controls_.hardwareButtons[index] != -2)
    return false;
  if (button != GamepadButton::L2 && button != GamepadButton::R2)
    return false;
  const auto axis = button == GamepadButton::R2 && controls_.separateTriggers ? controls_.rightTriggerAxis : controls_.triggerAxis;
  const auto a = AxisIndex(axis);
  if (a >= sf::Joystick::AxisCount || !pad.capabilities.axes[a])
    return false;
  const float value = pad.axes[a] * (controls_.triggerInverted ? -1.f : 1.f);
  if (controls_.separateTriggers)
    return value > 0.5f;
  return button == GamepadButton::L2 ? value > 0.5f : value < -0.5f;
}
void InputManager::ReadButtons(PadState &pad) {
  for (std::size_t b = 0; b < pad.held.size(); ++b) {
    const bool down = LogicalHeld(pad, static_cast<GamepadButton>(b));
    if (down && !pad.held[b] && !pad.blocked)
      pad.pending[b] = true;
    pad.held[b] = down;
  }
}
void InputManager::SelectDevice(unsigned id) {
  if (focused_ && id < pads_.size() && pads_[id].capabilities.connected) {
    activePad_ = id;
    device_ = Input::Device::Gamepad;
  }
}
void InputManager::ProcessEvent(const sf::Event &event) {
  if (const auto *connected = event.getIf<sf::Event::JoystickConnected>()) {
    if (discover_)
      Discover(connected->joystickId);
    return;
  }
  if (const auto *disconnected = event.getIf<sf::Event::JoystickDisconnected>()) {
    RegisterDevice(disconnected->joystickId, {});
    return;
  }
  if (event.is<sf::Event::FocusLost>()) {
    focused_ = false;
    SuppressUntilReleased();
    return;
  }
  if (event.is<sf::Event::FocusGained>()) {
    focused_ = true;
    SuppressUntilReleased();
    return;
  }
  if (event.is<sf::Event::KeyPressed>() || event.is<sf::Event::MouseButtonPressed>() || event.is<sf::Event::MouseMoved>() || event.is<sf::Event::MouseWheelScrolled>()) {
    device_ = Input::Device::KeyboardMouse;
    navigation_ = previousNavigation_ = {};
    repeatElapsed_ = 0;
    repeating_ = false;
    return;
  }
  if (const auto *moved = event.getIf<sf::Event::JoystickMoved>()) {
    if (moved->joystickId >= pads_.size() || AxisIndex(moved->axis) >= sf::Joystick::AxisCount)
      return;
    auto &pad = pads_[moved->joystickId];
    if (!pad.capabilities.connected || !pad.capabilities.axes[AxisIndex(moved->axis)])
      return;
    pad.axes[AxisIndex(moved->axis)] = std::clamp(moved->position / 100.f, -1.f, 1.f);
    if (std::abs(moved->position) > controls_.deadzone * 100.f)
      SelectDevice(moved->joystickId);
    ReadButtons(pad);
    return;
  }
  if (const auto *pressed = event.getIf<sf::Event::JoystickButtonPressed>()) {
    if (pressed->joystickId >= pads_.size())
      return;
    auto &pad = pads_[pressed->joystickId];
    if (!pad.capabilities.connected || pressed->button >= pad.capabilities.buttonCount)
      return;
    pad.buttons[pressed->button] = true;
    SelectDevice(pressed->joystickId);
    ReadButtons(pad);
    return;
  }
  if (const auto *released = event.getIf<sf::Event::JoystickButtonReleased>()) {
    if (released->joystickId >= pads_.size())
      return;
    auto &pad = pads_[released->joystickId];
    if (released->button >= pad.buttons.size())
      return;
    pad.buttons[released->button] = false;
    ReadButtons(pad);
  }
}
sf::Vector2f InputManager::PointerDirection() const {
  if (!activePad_ || !focused_)
    return {};
  const auto &pad = pads_[*activePad_];
  if (pad.blocked)
    return {};
  auto axis = [&](sf::Joystick::Axis axis) {const auto a=AxisIndex(axis);return a<sf::Joystick::AxisCount&&pad.capabilities.axes[a]?pad.axes[a]:0.f; };
  sf::Vector2f direction{axis(controls_.stickX) * (controls_.invertX ? -1.f : 1.f), axis(controls_.stickY) * (controls_.invertY ? -1.f : 1.f)};
  const float magnitude = direction.length();
  if (magnitude <= controls_.deadzone)
    return {};
  return direction / magnitude * (std::min(1.f, (magnitude - controls_.deadzone) / (1.f - controls_.deadzone)) * controls_.sensitivity);
}
sf::Vector2i InputManager::NavigationDirection() const {
  if (!activePad_ || !focused_)
    return {};
  const auto &pad = pads_[*activePad_];
  if (pad.blocked)
    return {};
  auto axis = [&](sf::Joystick::Axis axis) {const auto a=AxisIndex(axis);return a<sf::Joystick::AxisCount&&pad.capabilities.axes[a]?pad.axes[a]:0.f; };
  sf::Vector2i result{Sign(axis(controls_.stickX) * (controls_.invertX ? -1.f : 1.f), controls_.deadzone), Sign(axis(controls_.stickY) * (controls_.invertY ? -1.f : 1.f), controls_.deadzone)};
  const int povX = Sign(axis(sf::Joystick::Axis::PovX), 0.5f), povY = Sign(axis(sf::Joystick::Axis::PovY), 0.5f) * (controls_.povYUpPositive ? -1 : 1);
  if (povX)
    result.x = povX;
  if (povY)
    result.y = povY;
  return result;
}
void InputManager::Update(float deltaTime) {
  navigation_ = {};
  for (auto &pad : pads_) {
    pad.pulses = pad.pending;
    pad.pending.fill(false);
    if (pad.blocked) {
      const bool buttons = std::ranges::any_of(pad.buttons, [](bool held) { return held; });
      bool axes = false;
      for (const auto axis : {controls_.stickX, controls_.stickY, sf::Joystick::Axis::PovX, sf::Joystick::Axis::PovY})
        if (pad.capabilities.axes[AxisIndex(axis)] && std::abs(pad.axes[AxisIndex(axis)]) > controls_.deadzone)
          axes = true;
      if (!buttons && !axes) {
        pad.blocked = false;
        pad.held.fill(false);
      }
      pad.pulses.fill(false);
    }
  }
  if (device_ != Input::Device::Gamepad || !focused_)
    return;
  const auto direction = NavigationDirection();
  if (direction == sf::Vector2i{}) {
    previousNavigation_ = {};
    repeatElapsed_ = 0;
    repeating_ = false;
    return;
  }
  if (direction != previousNavigation_) {
    navigation_ = direction;
    previousNavigation_ = direction;
    repeatElapsed_ = 0;
    repeating_ = false;
    return;
  }
  repeatElapsed_ += std::clamp(deltaTime, 0.f, 1.f);
  const float interval = controls_.repeatInterval / controls_.sensitivity;
  const float threshold = repeating_ ? interval : controls_.initialRepeat;
  if (repeatElapsed_ >= threshold) {
    navigation_ = direction;
    repeatElapsed_ = std::fmod(repeatElapsed_ - threshold, interval);
    repeating_ = true;
  }
}
bool InputManager::Consume(Input::PadAction action) {
  const auto index = static_cast<std::size_t>(action);
  if (!activePad_ || device_ != Input::Device::Gamepad || index >= Input::PadActionCount || !focused_)
    return false;
  auto &pad = pads_[*activePad_];
  const auto button = static_cast<std::size_t>(controls_.actionButtons[index]);
  const bool pulse = pad.pulses[button];
  pad.pulses[button] = false;
  return pulse;
}
void InputManager::SuppressUntilReleased() {
  for (auto &pad : pads_) {
    pad.blocked = true;
    pad.pending.fill(false);
    pad.pulses.fill(false);
  }
  navigation_ = previousNavigation_ = {};
  repeatElapsed_ = 0;
  repeating_ = false;
}
void InputManager::Configure(Input::GamepadConfig config) {
  const auto sane = [](float value, float fallback, float min, float max) { return std::isfinite(value) ? std::clamp(value, min, max) : fallback; };
  config.deadzone = sane(config.deadzone, .22f, .05f, .8f);
  config.sensitivity = sane(config.sensitivity, 1.f, .25f, 3.f);
  config.initialRepeat = sane(config.initialRepeat, .32f, .15f, 1.f);
  config.repeatInterval = sane(config.repeatInterval, .12f, .06f, .5f);
  if (static_cast<unsigned>(config.profile) > 3)
    config.profile = Input::GamepadProfile::Auto;
  for (auto &button : config.hardwareButtons)
    button = std::clamp(button, -2, static_cast<int>(sf::Joystick::ButtonCount) - 1);
  for (std::size_t i = 0; i < config.actionButtons.size(); ++i)
    if (static_cast<unsigned>(config.actionButtons[i]) >= 12)
      config.actionButtons[i] = Input::GamepadConfig{}.actionButtons[i];
  for (auto *axis : {&config.stickX, &config.stickY, &config.triggerAxis, &config.rightTriggerAxis})
    if (AxisIndex(*axis) >= sf::Joystick::AxisCount)
      *axis = sf::Joystick::Axis::X;
  controls_ = config;
  SuppressUntilReleased();
}
void InputManager::LoadPreferences(SaveManager &save) {
  try {
    const auto json = save.Get(Input::PreferencesKey, nlohmann::json::object());
    if (!json.is_object())
      return;
    auto config = Input::GamepadConfig{};
    config.deadzone = json.value("deadzone", config.deadzone);
    config.sensitivity = json.value("sensitivity", config.sensitivity);
    config.initialRepeat = json.value("initial_repeat", config.initialRepeat);
    config.repeatInterval = json.value("repeat_interval", config.repeatInterval);
    config.invertX = json.value("invert_x", false);
    config.invertY = json.value("invert_y", false);
    config.povYUpPositive = json.value("pov_y_up_positive", true);
    config.profile = static_cast<Input::GamepadProfile>(json.value("profile", 0));
    config.stickX = static_cast<sf::Joystick::Axis>(json.value("stick_x", 0));
    config.stickY = static_cast<sf::Joystick::Axis>(json.value("stick_y", 1));
    config.triggerAxis = static_cast<sf::Joystick::Axis>(json.value("trigger_axis", 2));
    config.rightTriggerAxis = static_cast<sf::Joystick::Axis>(json.value("right_trigger_axis", 3));
    config.separateTriggers = json.value("separate_triggers", false);
    config.triggerInverted = json.value("trigger_inverted", false);
    if (auto found = json.find("hardware_buttons"); found != json.end() && found->is_array() && found->size() == 12)
      config.hardwareButtons = found->get<std::array<int, 12>>();
    if (auto found = json.find("action_buttons"); found != json.end() && found->is_array() && found->size() == Input::PadActionCount)
      for (std::size_t i = 0; i < Input::PadActionCount; ++i)
        config.actionButtons[i] = static_cast<GamepadButton>((*found)[i].get<int>());
    Configure(config);
  } catch (const nlohmann::json::exception &) {
    Configure({});
  }
}
void InputManager::SavePreferences(SaveManager &save) const {
  auto json = nlohmann::json{{"version", 1}, {"deadzone", controls_.deadzone}, {"sensitivity", controls_.sensitivity}, {"initial_repeat", controls_.initialRepeat}, {"repeat_interval", controls_.repeatInterval}, {"invert_x", controls_.invertX}, {"invert_y", controls_.invertY}, {"pov_y_up_positive", controls_.povYUpPositive}, {"profile", static_cast<int>(controls_.profile)}, {"stick_x", static_cast<int>(controls_.stickX)}, {"stick_y", static_cast<int>(controls_.stickY)}, {"trigger_axis", static_cast<int>(controls_.triggerAxis)}, {"right_trigger_axis", static_cast<int>(controls_.rightTriggerAxis)}, {"separate_triggers", controls_.separateTriggers}, {"trigger_inverted", controls_.triggerInverted}, {"hardware_buttons", controls_.hardwareButtons}};
  json["action_buttons"] = nlohmann::json::array();
  for (auto button : controls_.actionButtons)
    json["action_buttons"].push_back(static_cast<int>(button));
  save.Set(Input::PreferencesKey, json);
}
