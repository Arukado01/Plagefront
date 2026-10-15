#pragma once

#include <SFML/Graphics/RenderWindow.hpp>

#include "Managers/AudioManager.h"
#include "Managers/ClipboardManager.h"
#include "Managers/CursorManager.h"
#include "Managers/GuiManager.h"
#include "Managers/InputManager.h"
#include "Managers/RandomManager.h"
#include "Managers/RenderManager.h"
#include "Managers/ResourceManager.h"
#include "Managers/SaveManager.h"
#include "Managers/SceneManager.h"
#include "Managers/ScreenshotManager.h"
#include "Managers/TimeManager.h"
#include "Managers/WindowManager.h"

struct EngineContext {
  WindowManager window;
  RandomManager random;
  TimeManager time;
  SaveManager save;
  ClipboardManager clipboard;
  ResourceManager resources;
  AudioManager audio;
  InputManager input;
  RenderManager renderer;
  ScreenshotManager screenshot;
  CursorManager cursor;
  GuiManager gui;
  SceneManager scenes;

  EngineContext(sf::RenderWindow &renderWindow, bool visible = true)
      : window(renderWindow, visible), screenshot(renderWindow), cursor(renderWindow), gui(renderWindow) {}
};
