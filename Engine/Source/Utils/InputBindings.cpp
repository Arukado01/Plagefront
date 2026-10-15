#include "Utils/InputBindings.h"

std::array<int, 12> Input::DefaultHardwareMapping(GamepadProfile profile, unsigned vendorId) {
  if (profile == GamepadProfile::Auto)
    profile = vendorId == 0x045e ? GamepadProfile::Xbox : vendorId == 0x054c ? GamepadProfile::PlayStation
                                                                             : GamepadProfile::Generic;
  if (profile == GamepadProfile::Xbox)
    return {2, 0, 1, 3, 4, 5, -1, -1, 6, 7, 8, 9};
  return {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};
}
std::optional<int> Input::LogicalToHardware(GamepadButton button, int id) {
  if (id < 0 || id >= static_cast<int>(sf::Joystick::Count) || !sf::Joystick::isConnected(id))
    return {};
  const auto index = static_cast<std::size_t>(button);
  if (index >= 12)
    return {};
  const int hardware = DefaultHardwareMapping(GamepadProfile::Auto, sf::Joystick::getIdentification(id).vendorId)[index];
  if (hardware < 0 || static_cast<unsigned>(hardware) >= sf::Joystick::getButtonCount(id))
    return {};
  return hardware;
}
std::optional<GamepadButton> Input::HardwareToLogical(int button, int id) {
  if (button < 0 || id < 0 || id >= static_cast<int>(sf::Joystick::Count) || !sf::Joystick::isConnected(id) ||
      static_cast<unsigned>(button) >= sf::Joystick::getButtonCount(id))
    return {};
  const auto mapping = DefaultHardwareMapping(GamepadProfile::Auto, sf::Joystick::getIdentification(id).vendorId);
  for (std::size_t i = 0; i < mapping.size(); ++i)
    if (mapping[i] == button)
      return static_cast<GamepadButton>(i);
  return {};
}
std::string_view Input::ActionName(PadAction action) {
  static constexpr std::array<std::string_view, PadActionCount> names{
      "Confirmar", "Cancelar / volver", "Órdenes / tablero", "Consultar información", "Unidad anterior", "Unidad siguiente",
      "Centrar cámara", "Alejar", "Acercar", "Terminar activación", "Fin de turno", "Pausa"};
  const auto i = static_cast<std::size_t>(action);
  return i < names.size() ? names[i] : "Sin acción";
}
std::string_view Input::ButtonName(GamepadButton button) {
  static constexpr std::array<std::string_view, 12> names{"Izquierdo / X", "Inferior / A", "Derecho / B", "Superior / Y",
                                                          "LB / L1", "RB / R1", "LT / L2", "RT / R2", "Vista / Select", "Menu / Start", "Stick izquierdo", "Stick derecho"};
  const auto i = static_cast<std::size_t>(button);
  return i < names.size() ? names[i] : "Sin botón";
}
std::string_view Input::ProfileName(GamepadProfile profile) {
  static constexpr std::array<std::string_view, 4> names{"Automático", "Xbox", "PlayStation", "Genérico"};
  const auto i = static_cast<std::size_t>(profile);
  return i < names.size() ? names[i] : names[0];
}
