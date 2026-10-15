#include "Managers/WindowManager.h"

#include "Core/EngineConfig.h"
#include <SFML/Graphics/Image.hpp>
#include <algorithm>
#include <array>
#include <nlohmann/json.hpp>
#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>
#endif

namespace {
  void AddUnique(std::vector<sf::Vector2u> &sizes, sf::Vector2u size) {
    if (std::find(sizes.begin(), sizes.end(), size) == sizes.end())
      sizes.push_back(size);
  }

  bool SizeOrder(sf::Vector2u a, sf::Vector2u b) {
    return a.x == b.x ? a.y < b.y : a.x < b.x;
  }
} // espacio de nombres

WindowManager::WindowManager(sf::RenderWindow &window, bool visible)
    : window_(window), visible_(visible), windowedSize_(window.getSize()),
      fullscreenSize_(sf::VideoMode::getDesktopMode().size), desktopSize_(fullscreenSize_),
      windowedPosition_(window.getPosition()) {
  for (const auto &mode : sf::VideoMode::getFullscreenModes()) {
    if (mode.size.x >= gConfig.minimumWindowSize.x && mode.size.y >= gConfig.minimumWindowSize.y)
      AddUnique(fullscreenResolutions_, mode.size);
  }
  AddUnique(fullscreenResolutions_, desktopSize_);
  constexpr std::array common{sf::Vector2u{1280, 720}, sf::Vector2u{1366, 768}, sf::Vector2u{1440, 900},
                              sf::Vector2u{1600, 900}, sf::Vector2u{1920, 1080}, sf::Vector2u{2560, 1440}, sf::Vector2u{3840, 2160}};
  for (const auto size : common)
    if (size.x <= desktopSize_.x && size.y <= desktopSize_.y)
      AddUnique(windowedResolutions_, size);
  AddUnique(windowedResolutions_, windowedSize_);
  AddUnique(windowedResolutions_, desktopSize_);
  std::sort(fullscreenResolutions_.begin(), fullscreenResolutions_.end(), SizeOrder);
  std::sort(windowedResolutions_.begin(), windowedResolutions_.end(), SizeOrder);
  ConfigureWindow();
}

const std::vector<sf::Vector2u> &WindowManager::Resolutions() const {
  return fullscreen_ ? fullscreenResolutions_ : windowedResolutions_;
}

bool WindowManager::SetFullscreen(bool fullscreen) {
  if (fullscreen_ == fullscreen)
    return false;
  fullscreen_ = fullscreen;
  pending_ = true;
  ++revision_;
  return true;
}

bool WindowManager::SetResolution(sf::Vector2u size) {
  if (size.x < gConfig.minimumWindowSize.x || size.y < gConfig.minimumWindowSize.y || size.x > 8192 || size.y > 8192)
    return false;
  if (fullscreen_)
    return SetFullscreenResolution(size);
  if (windowedSize_ == size)
    return false;
  windowedSize_ = size;
  AddUnique(windowedResolutions_, size);
  pending_ = true;
  ++revision_;
  return true;
}

bool WindowManager::SetFullscreenResolution(sf::Vector2u size) {
  if (size.x < gConfig.minimumWindowSize.x || size.y < gConfig.minimumWindowSize.y || size.x > 8192 || size.y > 8192 ||
      !sf::VideoMode(size, sf::VideoMode::getDesktopMode().bitsPerPixel).isValid() || fullscreenSize_ == size)
    return false;
  fullscreenSize_ = size;
  pending_ |= fullscreen_;
  ++revision_;
  return true;
}

void WindowManager::ConfigureWindow() {
  window_.setVisible(visible_);
  window_.setFramerateLimit(60);
  window_.setKeyRepeatEnabled(false);
  if (!gConfig.windowIcon.empty()) {
    sf::Image icon;
    if (icon.loadFromFile(gConfig.windowIcon))
      window_.setIcon(icon);
  }
  window_.setMinimumSize(appliedFullscreen_ ? std::nullopt : std::optional{gConfig.minimumWindowSize});
}

bool WindowManager::ApplyPending() {
  if (!pending_ || !window_.isOpen())
    return false;
  pending_ = false;
  if (fullscreen_ != appliedFullscreen_) {
    if (fullscreen_)
      windowedPosition_ = window_.getPosition();
    const auto mode = sf::VideoMode(Resolution(), sf::VideoMode::getDesktopMode().bitsPerPixel);
    window_.create(mode, gConfig.windowTitle, sf::Style::Default,
                   fullscreen_ ? sf::State::Fullscreen : sf::State::Windowed);
    appliedFullscreen_ = fullscreen_;
    ConfigureWindow();
    if (!fullscreen_)
      window_.setPosition(windowedPosition_);
    if (visible_)
      window_.requestFocus();
  } else if (fullscreen_) {
    // SFML recrea el manejador nativo para cambiar el modo de vídeo a pantalla completa.
    window_.create(sf::VideoMode(fullscreenSize_, sf::VideoMode::getDesktopMode().bitsPerPixel),
                   gConfig.windowTitle, sf::Style::Default, sf::State::Fullscreen);
    ConfigureWindow();
    if (visible_)
      window_.requestFocus();
  } else {
    window_.setSize(windowedSize_);
  }
  return true;
}

void WindowManager::OnResized(sf::Vector2u size) {
  if (size.x == 0 || size.y == 0 || fullscreen_ || pending_ || size == windowedSize_)
    return;
  windowedSize_ = size;
  ++revision_;
}

void WindowManager::LoadPreferences(const nlohmann::json &values) {
  SetFullscreen(false);
  SetResolution({values.value("window_width", static_cast<unsigned>(gConfig.windowSize.x)),
                 values.value("window_height", static_cast<unsigned>(gConfig.windowSize.y))});
  SetFullscreenResolution({values.value("fullscreen_width", desktopSize_.x),
                           values.value("fullscreen_height", desktopSize_.y)});
  SetFullscreen(values.value("fullscreen", false));
}

void WindowManager::StorePreferences(nlohmann::json &values) const {
  values["fullscreen"] = fullscreen_;
  values["window_width"] = windowedSize_.x;
  values["window_height"] = windowedSize_.y;
  values["fullscreen_width"] = fullscreenSize_.x;
  values["fullscreen_height"] = fullscreenSize_.y;
}

bool WindowManager::Maximize() {
#ifdef _WIN32
  if (fullscreen_ || !window_.isOpen())
    return false;
  ShowWindow(window_.getNativeHandle(), SW_MAXIMIZE);
  if (!visible_)
    window_.setVisible(false);
  return IsMaximized();
#else
  return false;
#endif
}

bool WindowManager::Restore() {
#ifdef _WIN32
  if (fullscreen_ || !window_.isOpen())
    return false;
  ShowWindow(window_.getNativeHandle(), SW_RESTORE);
  if (!visible_)
    window_.setVisible(false);
  return !IsMaximized();
#else
  return false;
#endif
}

bool WindowManager::IsMaximized() const {
#ifdef _WIN32
  return window_.isOpen() && IsZoomed(window_.getNativeHandle()) != FALSE;
#else
  return false;
#endif
}
