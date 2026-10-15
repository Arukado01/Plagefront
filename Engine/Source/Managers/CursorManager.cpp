#include "Managers/CursorManager.h"

#include <SFML/Window/Mouse.hpp>

#include <algorithm>

#include "Core/EngineConfig.h"

CursorManager::CursorManager(sf::RenderWindow &window)
    : window_(window), texture_("Content/Textures/Plaguefront/Pixel/cursors.png"),
      speed_(gConfig.cursorSpeed), visible_(true), focused_(window.hasFocus()),
      view_(window.getDefaultView()) {
  texture_.setSmooth(false);
  ApplyVisibility();
}

CursorManager::~CursorManager() {
  if (window_.isOpen())
    window_.setMouseCursorVisible(true);
}

void CursorManager::Update(float deltaTime, sf::Vector2f direction) {
  if (direction.lengthSquared() > 0.f) {
    auto position = GetPosition() + direction * speed_ * deltaTime;
    const auto topLeft = view_.getCenter() - view_.getSize() / 2.f;
    position.x = std::clamp(position.x, topLeft.x,
                            topLeft.x + view_.getSize().x - 1.f);
    position.y = std::clamp(position.y, topLeft.y,
                            topLeft.y + view_.getSize().y - 1.f);
    SetPosition(position);
  }
}

void CursorManager::Draw(sf::RenderTarget &target, bool ignoreFocus) const {
  if (!visible_ || (!ignoreFocus && !focused_))
    return;
  const auto position = virtualPosition_ ? window_.mapCoordsToPixel(*virtualPosition_, view_) : ignoreFocus && lastRequestedOutputPosition_ ? *lastRequestedOutputPosition_
                                                                                                                                            : sf::Mouse::getPosition(window_);
  const auto windowSize = window_.getSize();
  if (!ignoreFocus && (position.x < 0 || position.y < 0 ||
                       position.x >= static_cast<int>(windowSize.x) ||
                       position.y >= static_cast<int>(windowSize.y)))
    return;
  const float scale = target.getSize().y >= 1200 ? 2.f : 1.f;
  sf::Sprite sprite(texture_);
  sprite.setTextureRect(sf::IntRect({static_cast<int>(context_) * 24, 0}, {24, 24}));
  sprite.setOrigin({1.f, 1.f});
  sprite.setScale({scale, scale});
  sprite.setPosition(sf::Vector2f(position));
  const auto previousView = target.getView();
  // La vista inicial de SFML conserva el tamaño de creación al maximizar/restaurar.
  target.setView(sf::View(sf::FloatRect({0.f, 0.f}, sf::Vector2f(target.getSize()))));
  target.draw(sprite);
  target.setView(previousView);
}

void CursorManager::Render() const { Draw(window_); }

void CursorManager::SetSpeed(float speed) { speed_ = speed; }

float CursorManager::GetSpeed() const { return speed_; }

void CursorManager::SetVisible(bool visible) {
  visible_ = visible;
  ApplyVisibility();
}

bool CursorManager::IsVisible() const { return visible_; }

void CursorManager::SetFocused(bool focused) {
  focused_ = focused;
  ApplyVisibility();
}

void CursorManager::ApplyVisibility() {
  systemCursorVisible_ = !visible_ || !focused_;
  if (window_.isOpen())
    window_.setMouseCursorVisible(systemCursorVisible_);
}

void CursorManager::SetPosition(sf::Vector2f position) {
  const auto outputPosition = window_.mapCoordsToPixel(position, view_);
  lastRequestedOutputPosition_ = outputPosition;
  sf::Mouse::setPosition(outputPosition, window_);
}

sf::Vector2f CursorManager::GetPosition() const {
  return virtualPosition_.value_or(GetPosition(view_));
}

sf::Vector2f CursorManager::GetPosition(const sf::View &view) const {
  return window_.mapPixelToCoords(sf::Mouse::getPosition(window_), view);
}
