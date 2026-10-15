#pragma once
#include <SFML/Graphics/Color.hpp>
#include <SFML/System/Time.hpp>
#include <SFML/System/Vector2.hpp>
#include <string>
#include <cstdint>
#include <nlohmann/json_fwd.hpp>

struct RotPaletteConfig {
  sf::Color sourceShadow{58, 27, 76, 255};
  sf::Color sourceBase{117, 57, 161, 255};
  sf::Color sourceLight{187, 112, 230, 255};
  sf::Color sourceGlow{239, 191, 255, 255};
  sf::Color localFill{139, 55, 185, 66};
  sf::Color localEdge{211, 139, 247, 255};
  sf::Color globalFill{104, 61, 153, 28};
  sf::Color spores{231, 173, 255, 240};
  sf::Color uiText{232, 190, 255, 255};
  sf::Color SpriteColor(sf::Color source) const;
};

struct EngineConfig {
  std::string windowTitle;
  sf::Vector2f windowSize;
  sf::Vector2f referenceSize;
  sf::Vector2u minimumWindowSize;
  std::string windowIcon;
  bool disableSfmlLogs;
  sf::Time maximumDeltaTime;
  float globalVolume;
  sf::Color backgroundColor;
  float cursorRadius;
  float cursorSpeed;
  sf::Color cursorColor;
  float joystickDeadzone;
  RotPaletteConfig rotPalette;

  static RotPaletteConfig ReadRotPalette(const nlohmann::json &document);

  EngineConfig();
};

inline const EngineConfig gConfig;
