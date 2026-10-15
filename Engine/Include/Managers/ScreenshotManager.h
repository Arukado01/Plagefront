#pragma once

#include <SFML/Audio/Sound.hpp>
#include <SFML/Audio/SoundBuffer.hpp>

#include <SFML/Graphics/RenderWindow.hpp>
#include <functional>
#include <string>

class ScreenshotManager {
private:
  const sf::RenderWindow &window_;
  sf::SoundBuffer soundBuffer_;
  mutable sf::Sound sound_;

public:
  ScreenshotManager(const sf::RenderWindow &window);
  void Take() const;
  // Compone capturas antes de codificarlas para evitar lecturas indefinidas tras display(); concentra aquí la escritura PNG.
  bool SaveFrame(const sf::Texture &frame, sf::Vector2u outputSize,
                 const std::string &filename,
                 const std::function<void(sf::RenderTarget &)> &drawOverlay = {}) const;
  bool SaveWindow(const std::string &filename, bool playSound = false) const;

private:
  [[nodiscard]] static sf::Sound MakeSound(sf::SoundBuffer &buffer);
  static bool Encode(const sf::Image &image, const std::string &filename);
};
