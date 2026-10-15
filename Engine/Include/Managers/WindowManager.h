#pragma once

#include <SFML/Graphics/RenderWindow.hpp>
#include <cstdint>
#include <vector>
#include <nlohmann/json_fwd.hpp>

// RenderWindow conserva su dirección; la escena solicita transiciones que el motor aplica entre fotogramas sin invalidar referencias.
class WindowManager {
public:
  explicit WindowManager(sf::RenderWindow &window, bool visible = true);

  bool IsFullscreen() const { return fullscreen_; }
  bool SetFullscreen(bool fullscreen);
  bool ToggleFullscreen() { return SetFullscreen(!fullscreen_); }
  bool SetResolution(sf::Vector2u size);
  bool SetFullscreenResolution(sf::Vector2u size);
  sf::Vector2u Resolution() const { return fullscreen_ ? fullscreenSize_ : windowedSize_; }
  sf::Vector2u WindowedSize() const { return windowedSize_; }
  sf::Vector2u FullscreenResolution() const { return fullscreenSize_; }
  sf::Vector2u OutputSize() const { return window_.getSize(); }
  sf::Vector2u DesktopResolution() const { return desktopSize_; }
  const std::vector<sf::Vector2u> &Resolutions() const;
  std::uint64_t Revision() const { return revision_; }
  void LoadPreferences(const nlohmann::json &values);
  void StorePreferences(nlohmann::json &values) const;
  bool Maximize();
  bool Restore();
  bool IsMaximized() const;

  // Expone la transición para pruebas locales deterministas sobre la misma ventana.
  bool ApplyPending();
  void OnResized(sf::Vector2u size);

private:
  sf::RenderWindow &window_;
  bool visible_{true};
  bool fullscreen_{false};
  bool appliedFullscreen_{false};
  bool pending_{false};
  sf::Vector2u windowedSize_;
  sf::Vector2u fullscreenSize_;
  sf::Vector2u desktopSize_;
  sf::Vector2i windowedPosition_;
  std::uint64_t revision_{0};
  std::vector<sf::Vector2u> windowedResolutions_;
  std::vector<sf::Vector2u> fullscreenResolutions_;

  void ConfigureWindow();
};
