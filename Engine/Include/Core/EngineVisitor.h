#pragma once
#include <SFML/Window/Event.hpp>

struct EngineVisitor {
  class Engine &engine; // Forward Declaration

  // true consume el evento global; false permite que continúe hacia GUI y escena.
  bool operator()(const sf::Event::Closed &);
  bool operator()(const sf::Event::Resized &);
  bool operator()(const sf::Event::FocusLost &);
  bool operator()(const sf::Event::FocusGained &);
  bool operator()(const sf::Event::JoystickConnected &);
  bool operator()(const sf::Event::JoystickDisconnected &);
  bool operator()(const sf::Event::KeyPressed &);
  bool operator()(const sf::Event::KeyReleased &);
  bool operator()(const sf::Event::JoystickButtonPressed &);
  bool operator()(const auto &) { return false; }
};
