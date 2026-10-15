#pragma once

#include <SFML/Window/Event.hpp>

#include "Core/EngineContext.h"
#include "Scene/SceneUtils.h"

class Scene {
protected:
  EngineContext &ctx;

public:
  Scene(EngineContext &context) : ctx(context) {}
  virtual ~Scene() = default;

  virtual void Start() = 0;
  virtual void Update() = 0;
  virtual void Render() const = 0;

  virtual void OnEvent(const sf::Event &) {};
  virtual void OnPause(bool /* pausa */) {};
  virtual void OnCleanup() {};
  // Las escenas con menú integrado procesan Escape/R/M en lugar del panel de las demostraciones.
  virtual bool HandlesNavigation() const { return false; }
  virtual void SmokeStep(int) {}
  virtual bool OnCloseRequest() { return false; }
};
