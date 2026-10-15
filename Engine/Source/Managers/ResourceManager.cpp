#include "Managers/ResourceManager.h"
#include "Utils/Log.h"

sf::Texture *ResourceManager::FetchTexture(const std::string &filename) {
  if (!textures_.contains(filename)) {
    sf::Texture texture;
    if (!texture.loadFromFile("Content/Textures/" + filename)) {
      LOG_ERROR("Failed to load texture: {}", filename);
      return nullptr;
    }
    textures_.emplace(filename, std::move(texture));
  }

  return &textures_.at(filename);
}

sf::SoundBuffer *ResourceManager::FetchSound(const std::string &filename) {
  if (!sounds_.contains(filename)) {
    sf::SoundBuffer buffer;
    if (!buffer.loadFromFile("Content/Sounds/" + filename)) {
      LOG_ERROR("Failed to load sound: {}", filename);
      return nullptr;
    }
    sounds_.emplace(filename, std::move(buffer));
  }

  return &sounds_.at(filename);
}

sf::Font *ResourceManager::FetchFont(const std::string &filename) {
  if (!fonts_.contains(filename)) {
    sf::Font font;
    if (!font.openFromFile("Content/Fonts/" + filename)) {
      LOG_ERROR("Failed to load font: {}", filename);
      return nullptr;
    }
    fonts_.emplace(filename, std::move(font));
  }

  return &fonts_.at(filename);
}

std::optional<sf::Music>
ResourceManager::FetchMusic(const std::string &filename) const {
  sf::Music music;

  if (!music.openFromFile("Content/Musics/" + filename)) {
    LOG_ERROR("Failed to load music: {}", filename);
    return std::nullopt;
  }

  return music;
}
