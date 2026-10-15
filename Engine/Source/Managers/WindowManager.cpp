#include "Managers/WindowManager.h"

#include "Core/EngineConfig.h"
#include "Utils/Log.h"
#include <SFML/Graphics/Image.hpp>
#include <algorithm>
#include <array>
#include <optional>
#include <string>
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

#ifdef _WIN32
  std::wstring MonitorDevice(HWND handle) {
    MONITORINFOEXW info{};
    info.cbSize = sizeof(info);
    if (!GetMonitorInfoW(MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST), &info))
      return {};
    return info.szDevice;
  }

  bool ReadMonitorMode(const std::wstring &device, DEVMODEW &mode) {
    mode = {};
    mode.dmSize = sizeof(mode);
    return EnumDisplaySettingsW(device.c_str(), ENUM_CURRENT_SETTINGS, &mode) != FALSE;
  }

  std::optional<DEVMODEW> FindMonitorMode(const std::wstring &device, sf::Vector2u size, const DEVMODEW &current) {
    std::optional<DEVMODEW> selected;
    DEVMODEW mode{};
    mode.dmSize = sizeof(mode);
    for (DWORD index = 0; EnumDisplaySettingsW(device.c_str(), index, &mode); ++index) {
      if (mode.dmPelsWidth == size.x && mode.dmPelsHeight == size.y && mode.dmBitsPerPel == current.dmBitsPerPel) {
        if (!selected || mode.dmDisplayFrequency > selected->dmDisplayFrequency)
          selected = mode;
        if (mode.dmDisplayFrequency == current.dmDisplayFrequency)
          return mode;
      }
    }
    return selected;
  }

  DEVMODEW ApplyMonitorMode(const std::wstring &device, sf::Vector2u requested) {
    DEVMODEW current{};
    if (!ReadMonitorMode(device, current))
      return current;
    if (current.dmPelsWidth == requested.x && current.dmPelsHeight == requested.y)
      return current;
    if (auto mode = FindMonitorMode(device, requested, current)) {
      if (ChangeDisplaySettingsExW(device.c_str(), &*mode, nullptr, CDS_FULLSCREEN, nullptr) == DISP_CHANGE_SUCCESSFUL) {
        ReadMonitorMode(device, current);
        return current;
      }
    }
    LOG_WARNING("No se pudo aplicar {}x{} al monitor seleccionado; se conserva su modo actual.", requested.x, requested.y);
    return current;
  }
#endif
} // espacio de nombres

struct WindowManager::NativeState {
#ifdef _WIN32
  std::wstring device;
  DEVMODEW originalMode{};
  WINDOWPLACEMENT placement{sizeof(WINDOWPLACEMENT)};
  sf::Vector2u windowedSize;
  bool maximized{false};
  bool modeChanged{false};
#endif
};

WindowManager::WindowManager(sf::RenderWindow &window, bool visible)
    : window_(window), visible_(visible), native_(std::make_unique<NativeState>()), windowedSize_(window.getSize()),
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
  RefreshMonitor();
  ConfigureWindow();
}

WindowManager::~WindowManager() { ExitMonitorFullscreen(); }

void WindowManager::RefreshMonitor() {
#ifdef _WIN32
  if (!window_.isOpen() || appliedFullscreen_)
    return;
  const auto device = MonitorDevice(window_.getNativeHandle());
  DEVMODEW current{};
  if (device.empty() || !ReadMonitorMode(device, current))
    return;
  const auto previousDesktop = desktopSize_;
  desktopSize_ = {current.dmPelsWidth, current.dmPelsHeight};
  fullscreenResolutions_.clear();
  DEVMODEW mode{};
  mode.dmSize = sizeof(mode);
  for (DWORD index = 0; EnumDisplaySettingsW(device.c_str(), index, &mode); ++index) {
    if (mode.dmBitsPerPel == current.dmBitsPerPel && mode.dmPelsWidth >= gConfig.minimumWindowSize.x &&
        mode.dmPelsHeight >= gConfig.minimumWindowSize.y)
      AddUnique(fullscreenResolutions_, {mode.dmPelsWidth, mode.dmPelsHeight});
  }
  AddUnique(fullscreenResolutions_, desktopSize_);
  std::sort(fullscreenResolutions_.begin(), fullscreenResolutions_.end(), SizeOrder);
  if (fullscreenSize_ == previousDesktop ||
      std::find(fullscreenResolutions_.begin(), fullscreenResolutions_.end(), fullscreenSize_) == fullscreenResolutions_.end())
    fullscreenSize_ = desktopSize_;
#endif
}

const std::vector<sf::Vector2u> &WindowManager::Resolutions() const {
  return fullscreen_ ? fullscreenResolutions_ : windowedResolutions_;
}

bool WindowManager::SetFullscreen(bool fullscreen) {
  if (fullscreen_ == fullscreen)
    return false;
  if (fullscreen)
    RefreshMonitor();
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
  RefreshMonitor();
  if (size.x < gConfig.minimumWindowSize.x || size.y < gConfig.minimumWindowSize.y || size.x > 8192 || size.y > 8192 ||
      std::find(fullscreenResolutions_.begin(), fullscreenResolutions_.end(), size) == fullscreenResolutions_.end() || fullscreenSize_ == size)
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
    if (fullscreen_) {
      // Aplica el tamaño pendiente antes de guardar la colocación de ventana.
      if (window_.getSize() != windowedSize_)
        window_.setSize(windowedSize_);
      windowedPosition_ = window_.getPosition();
#ifdef _WIN32
      GetWindowPlacement(window_.getNativeHandle(), &native_->placement);
      native_->windowedSize = windowedSize_;
      native_->maximized = IsMaximized();
      // SFML fija fullscreen en (0,0) y cambia el monitor principal.
      // La ventana sin bordes permite usar el dispositivo seleccionado.
      if (!EnterMonitorFullscreen()) {
        fullscreen_ = false;
        ++revision_;
      }
#else
      window_.create(sf::VideoMode(fullscreenSize_, sf::VideoMode::getDesktopMode().bitsPerPixel),
                     gConfig.windowTitle, sf::Style::Default, sf::State::Fullscreen);
#endif
    } else {
      ExitMonitorFullscreen();
      window_.create(sf::VideoMode(windowedSize_, sf::VideoMode::getDesktopMode().bitsPerPixel),
                     gConfig.windowTitle, sf::Style::Default, sf::State::Windowed);
      window_.setPosition(windowedPosition_);
#ifdef _WIN32
      // WINDOWPLACEMENT conserva el rectángulo normal incluso maximizada.
      // Sus coordenadas de workspace no son coordenadas de pantalla.
      auto placement = native_->placement;
      if (native_->maximized)
        placement.showCmd = SW_SHOWMAXIMIZED;
      if (windowedSize_ != native_->windowedSize) {
        RECT bounds{0, 0, static_cast<LONG>(windowedSize_.x), static_cast<LONG>(windowedSize_.y)};
        AdjustWindowRectEx(&bounds, static_cast<DWORD>(GetWindowLongPtrW(window_.getNativeHandle(), GWL_STYLE)),
                           FALSE, static_cast<DWORD>(GetWindowLongPtrW(window_.getNativeHandle(), GWL_EXSTYLE)));
        placement.rcNormalPosition.right = placement.rcNormalPosition.left + bounds.right - bounds.left;
        placement.rcNormalPosition.bottom = placement.rcNormalPosition.top + bounds.bottom - bounds.top;
        placement.showCmd = visible_ ? SW_SHOWNORMAL : SW_HIDE;
        placement.flags = 0;
      }
      SetWindowPlacement(window_.getNativeHandle(), &placement);
#endif
    }
    appliedFullscreen_ = fullscreen_;
    ConfigureWindow();
    if (visible_)
      window_.requestFocus();
  } else if (fullscreen_) {
#ifdef _WIN32
    RefreshMonitorFullscreen();
#else
    // SFML recrea el manejador nativo para cambiar el modo de vídeo a pantalla completa.
    window_.create(sf::VideoMode(fullscreenSize_, sf::VideoMode::getDesktopMode().bitsPerPixel),
                   gConfig.windowTitle, sf::Style::Default, sf::State::Fullscreen);
    ConfigureWindow();
    if (visible_)
      window_.requestFocus();
#endif
  } else {
    window_.setSize(windowedSize_);
  }
  return true;
}

bool WindowManager::EnterMonitorFullscreen() {
#ifdef _WIN32
  if (native_->modeChanged) {
    ExitMonitorFullscreen();
    if (native_->modeChanged)
      return false;
  }
  native_->device = MonitorDevice(window_.getNativeHandle());
  if (native_->device.empty() || !ReadMonitorMode(native_->device, native_->originalMode))
    return false;
  const auto mode = ApplyMonitorMode(native_->device, fullscreenSize_);
  if (!mode.dmPelsWidth || !mode.dmPelsHeight)
    return false;
  native_->modeChanged = mode.dmPelsWidth != native_->originalMode.dmPelsWidth ||
                         mode.dmPelsHeight != native_->originalMode.dmPelsHeight;
  fullscreenSize_ = {mode.dmPelsWidth, mode.dmPelsHeight};
  window_.create(sf::VideoMode(fullscreenSize_, mode.dmBitsPerPel), gConfig.windowTitle,
                 sf::Style::None, sf::State::Windowed);
  SetWindowPos(window_.getNativeHandle(), HWND_TOP, mode.dmPosition.x, mode.dmPosition.y,
               static_cast<int>(fullscreenSize_.x), static_cast<int>(fullscreenSize_.y),
               SWP_FRAMECHANGED | SWP_NOACTIVATE);
  return true;
#else
  return false;
#endif
}

bool WindowManager::RefreshMonitorFullscreen() {
#ifdef _WIN32
  const auto mode = ApplyMonitorMode(native_->device, fullscreenSize_);
  if (!mode.dmPelsWidth || !mode.dmPelsHeight)
    return false;
  native_->modeChanged |= mode.dmPelsWidth != native_->originalMode.dmPelsWidth ||
                          mode.dmPelsHeight != native_->originalMode.dmPelsHeight;
  fullscreenSize_ = {mode.dmPelsWidth, mode.dmPelsHeight};
  // Sincroniza también el tamaño cacheado por SFML antes de consumir Resized.
  window_.setSize(fullscreenSize_);
  SetWindowPos(window_.getNativeHandle(), HWND_TOP, mode.dmPosition.x, mode.dmPosition.y,
               static_cast<int>(fullscreenSize_.x), static_cast<int>(fullscreenSize_.y),
               SWP_FRAMECHANGED | SWP_NOACTIVATE);
  return true;
#else
  return false;
#endif
}

void WindowManager::ExitMonitorFullscreen() {
#ifdef _WIN32
  if (native_->modeChanged) {
    if (ChangeDisplaySettingsExW(native_->device.c_str(), &native_->originalMode, nullptr, 0, nullptr) != DISP_CHANGE_SUCCESSFUL)
      LOG_WARNING("No se pudo restaurar el modo original del monitor.");
    else
      native_->modeChanged = false;
  }
  if (!native_->modeChanged)
    native_->device.clear();
#endif
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
