#include "Managers/ScreenshotManager.h"

#include <SFML/Graphics/Image.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/RenderTexture.hpp>
#include <SFML/Graphics/Sprite.hpp>

#include <chrono>
#include <ctime>
#include <filesystem>
#include <format>
#include <string>

#include "Core/EngineConfig.h"
#include "Utils/Log.h"

ScreenshotManager::ScreenshotManager(const sf::RenderWindow &window)
    : window_(window),
      soundBuffer_("Content/Sounds/screenshot.mp3"),
      sound_(MakeSound(soundBuffer_)) {}

sf::Sound ScreenshotManager::MakeSound(sf::SoundBuffer &buffer) {
  sf::Sound sound(buffer);
  sound.setVolume(gConfig.globalVolume);
  return sound;
}

void ScreenshotManager::Take() const {
  auto now = std::chrono::system_clock::now();
  std::time_t t = std::chrono::system_clock::to_time_t(now);
  std::tm localTime{};
#ifdef _WIN32
  localtime_s(&localTime, &t);
#else
  localtime_r(&t, &localTime);
#endif
  const auto *local = &localTime;
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                now.time_since_epoch()) %
            1000;
  std::string filename =
      std::format("Screenshot_{:04d}{:02d}{:02d}_{:02d}{:02d}{:02d}_{:03d}.png",
                  local->tm_year + 1900, local->tm_mon + 1, local->tm_mday,
                  local->tm_hour, local->tm_min, local->tm_sec, ms.count());

  if (SaveWindow("Content/Screenshots/" + filename, true)) {
    LOG_INFO("Screenshot saved as {}", filename);
  } else {
    LOG_WARNING("Failed to save screenshot to {}", filename);
  }
}

bool ScreenshotManager::Encode(const sf::Image &image, const std::string &filename) {
  std::error_code error;
  const auto parent = std::filesystem::path(filename).parent_path();
  if (!parent.empty())
    std::filesystem::create_directories(parent, error);
  return !error && image.saveToFile(filename);
}

bool ScreenshotManager::SaveWindow(const std::string &filename, bool playSound) const {
  if (!window_.isOpen() || window_.getSize().x == 0 || window_.getSize().y == 0)
    return false;
  sf::Texture screenshot(window_.getSize());
  screenshot.update(window_);
  const bool saved = Encode(screenshot.copyToImage(), filename);
  if (saved && playSound)
    sound_.play();
  return saved;
}

bool ScreenshotManager::SaveFrame(const sf::Texture &frame, sf::Vector2u outputSize,
                                  const std::string &filename,
                                  const std::function<void(sf::RenderTarget &)> &drawOverlay) const {
  const auto sourceSize = frame.getSize();
  if (!sourceSize.x || !sourceSize.y || !outputSize.x || !outputSize.y)
    return false;
  sf::RenderTexture output(outputSize);
  output.setSmooth(false);
  output.clear();
  sf::Sprite sprite(frame);
  sprite.setScale({static_cast<float>(outputSize.x) / sourceSize.x,
                   static_cast<float>(outputSize.y) / sourceSize.y});
  output.draw(sprite);
  if (drawOverlay)
    drawOverlay(output);
  output.display();
  return Encode(output.getTexture().copyToImage(), filename);
}
