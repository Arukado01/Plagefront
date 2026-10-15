#include "Managers/RenderManager.h"

#include <utility>
#include <algorithm>
#include <cmath>

#include "Graphics/Effects/EffectBloom.h"
#include "Graphics/Effects/EffectMonitor.h"
#include "Graphics/Effects/EffectGlitch.h"

#include "Core/EngineConfig.h"
#include "Utils/Verify.h"

RenderManager::RenderManager()
    : target_(sf::Vector2u(gConfig.windowSize)),
      backgroundTexture_("Content/Textures/Background.png"),
      background_(gConfig.windowSize) {
  target_.setSmooth(true);

  background_.setTexture(&backgroundTexture_);
  background_.setFillColor(gConfig.backgroundColor);

  if (sf::Shader::isAvailable()) {
    VERIFY(effectsTarget_.resize(sf::Vector2u(gConfig.windowSize)));

    // Post Process Effects
    effects_.emplace_back(std::make_unique<EffectBloom>());
    effects_.emplace_back(std::make_unique<EffectMonitor>());
    // effects_.emplace_back(std::make_unique<EffectGlitch>());
  }
  ConfigureOutput(sf::Vector2u(gConfig.windowSize));
}

void RenderManager::Draw(const sf::Drawable &drawable) {
  target_.draw(drawable);
}

void RenderManager::SetView(const sf::View &view) {
  view_ = view;
  target_.setView(view_);
}

void RenderManager::ResetView() { SetView(defaultView_); }

void RenderManager::SetResponsiveLayout(bool enabled) {
  if (responsiveLayout_ == enabled)
    return;
  responsiveLayout_ = enabled;
  layoutDirty_ = true;
  ConfigureOutput(outputSize_);
}

void RenderManager::SetPixelScale(unsigned scale) {
  scale = std::clamp(scale, 1u, 4u);
  if (pixelScale_ == scale)
    return;
  pixelScale_ = scale;
  layoutDirty_ = true;
  ConfigureOutput(outputSize_);
}

void RenderManager::ConfigureOutput(sf::Vector2u size) {
  if (size.x == 0 || size.y == 0)
    return;
  if (outputSize_ == size && !layoutDirty_)
    return;
  outputSize_ = size;
  uiScale_ = std::min(static_cast<float>(size.x) / gConfig.referenceSize.x,
                      static_cast<float>(size.y) / gConfig.referenceSize.y);
  logicalSize_ = responsiveLayout_ ? sf::Vector2f(size) / uiScale_ : gConfig.windowSize;
  defaultView_ = sf::View(sf::FloatRect({0.f, 0.f}, logicalSize_));
  defaultView_.setViewport(sf::FloatRect({0.f, 0.f}, {1.f, 1.f}));
  const sf::Vector2u nativeSize{std::max(1u, size.x / pixelScale_), std::max(1u, size.y / pixelScale_)};
  if (target_.getSize() != nativeSize)
    VERIFY(target_.resize(nativeSize));
  if (sf::Shader::isAvailable() && effectsTarget_.getSize() != nativeSize)
    VERIFY(effectsTarget_.resize(nativeSize));
  target_.setSmooth(false);
  background_.setSize(logicalSize_);
  layoutDirty_ = false;
  ++revision_;
  ResetView();
}

sf::Vector2f RenderManager::PixelToLogical(sf::Vector2i point) const {
  const auto viewport = view_.getViewport();
  const sf::FloatRect pixels{
      {static_cast<float>(std::lround(viewport.position.x * outputSize_.x)),
       static_cast<float>(std::lround(viewport.position.y * outputSize_.y))},
      {static_cast<float>(std::lround(viewport.size.x * outputSize_.x)),
       static_cast<float>(std::lround(viewport.size.y * outputSize_.y))}};
  const sf::Vector2f normalized{
      (static_cast<float>(point.x) - pixels.position.x) / pixels.size.x * 2.f - 1.f,
      1.f - (static_cast<float>(point.y) - pixels.position.y) / pixels.size.y * 2.f};
  return view_.getInverseTransform().transformPoint(normalized);
}

sf::Vector2i RenderManager::LogicalToPixel(sf::Vector2f point) const {
  const auto viewport = view_.getViewport();
  const sf::FloatRect pixels{
      {static_cast<float>(std::lround(viewport.position.x * outputSize_.x)),
       static_cast<float>(std::lround(viewport.position.y * outputSize_.y))},
      {static_cast<float>(std::lround(viewport.size.x * outputSize_.x)),
       static_cast<float>(std::lround(viewport.size.y * outputSize_.y))}};
  const auto normalized = view_.getTransform().transformPoint(point);
  return {static_cast<int>(std::lround((normalized.x + 1.f) * 0.5f * pixels.size.x + pixels.position.x)),
          static_cast<int>(std::lround((1.f - normalized.y) * 0.5f * pixels.size.y + pixels.position.y))};
}

void RenderManager::BeginDrawing() {
  target_.clear();
  Draw(background_);
}

const sf::Texture &RenderManager::FinishDrawing() {
  target_.display();

  sf::RenderTexture *input = &target_;
  sf::RenderTexture *output = &effectsTarget_;

  if (!effectsEnabled_)
    return target_.getTexture();
  for (auto &effect : effects_) {
    output->clear();
    effect->Apply(input->getTexture(), *output);
    output->display();

    std::swap(input, output);
  }

  return input->getTexture();
}
