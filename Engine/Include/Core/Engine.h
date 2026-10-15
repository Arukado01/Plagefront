#pragma once

#include "Core/EngineContext.h"
#include "Core/EngineVisitor.h"
#include "Core/Overlay.h"
#include "Scene/SceneFactory.h"
#include <SFML/Graphics.hpp>
#include <array>

class Engine {
private:
  sf::RenderWindow window_;
  EngineContext context_;

  SceneFactory::Scenes scenes_;
  Scene *currentScene_;

  Overlay overlay_;
  bool cursorWasVisible_;
  const sf::Texture *completedFrame_{};
  std::array<bool, 2> fullscreenKeysHeld_{};

public:
  explicit Engine(bool visible = true);

  bool IsRunning() const;
  bool HasFocus() const;

  void ProcessEvent();
  void Update();
  void Render();
  void SmokeStep(int step);
  bool SaveScreenshot(const std::string &filename) const;

private:
  friend EngineVisitor;
  void SmokeWindowTransitions();
  void SmokeConsumers();
  void ProcessWindowEvent(const sf::Event &event);
  bool ApplyWindowChanges();
  void SynchronizeWindowFocus(bool reapplyCursor = false);
  bool EventFullscreenShortcut(const sf::Event::KeyPressed &key);
  void EventShortcutReleased(const sf::Event::KeyReleased &key);
  void EventWindowClose();
  void EventWindowResized(sf::Vector2u size);
  void EventWindowFocusLost();
  void EventWindowFocusGained();
  void EventGamepadConnected(int id);
  void EventGamepadDisconnected(int id);
  void EventWindowScreenshot() const;
  void EventSceneChange(const std::string &name);
  void EventSceneRestart();
  void EventSceneMenuReturn();
  void EventOverlayPauseToggle();
  void EventOverlaySelect(OverlaySelection selection);
};
