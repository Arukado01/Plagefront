#include "Scene/SceneFactory.h"

#include "Bounce.h"
#include "Clicker.h"
#include "Plaguefront/Game.h"

namespace SceneFactory {
  Scenes CreateScenes(EngineContext &context) {
    Scenes scenes;
    scenes.emplace("Menu", std::make_unique<Plaguefront::Game>(context));

    scenes.emplace("Bounce", std::make_unique<Bounce::Game>(context));
    scenes.emplace("Clicker", std::make_unique<Clicker::Game>(context));

    return scenes;
  }
} // espacio de nombres SceneFactory
