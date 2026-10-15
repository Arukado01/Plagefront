#pragma once
#include <SFML/Graphics.hpp>

#include <vector>
#include <memory>
#include <cstdint>

#include "Graphics/Effect.h"

class RenderManager {
private:
  sf::RenderTexture target_;
  sf::Texture backgroundTexture_;
  sf::RectangleShape background_;

  std::vector<std::unique_ptr<Effect>> effects_;
  bool effectsEnabled_{true};
  unsigned pixelScale_{1};
  sf::RenderTexture effectsTarget_;
  sf::Vector2u outputSize_;
  sf::Vector2f logicalSize_;
  sf::View view_;
  sf::View defaultView_;
  float uiScale_{1.f};
  bool responsiveLayout_{false}, layoutDirty_{true};
  std::uint64_t revision_{};

public:
  RenderManager();
  void Draw(const sf::Drawable &drawable);

  void SetView(const sf::View &view);
  void ResetView();
  void SetEffectsEnabled(bool enabled) { effectsEnabled_ = enabled; }
  // Las escenas heredadas conservan sus coordenadas; el modo adaptable expande el lienzo lógico sin deformar los ejes.
  void SetResponsiveLayout(bool enabled);
  bool ResponsiveLayout() const { return responsiveLayout_; }
  std::uint64_t Revision() const { return revision_; }
  void SetPixelScale(unsigned scale);
  void ConfigureOutput(sf::Vector2u size);
  sf::Vector2u OutputSize() const { return outputSize_; }
  sf::Vector2f LogicalSize() const { return logicalSize_; }
  float UiScale() const { return uiScale_; }
  const sf::View &View() const { return view_; }
  const sf::Texture &FrameTexture() const { return target_.getTexture(); }
  sf::Vector2f PixelToLogical(sf::Vector2i point) const;
  sf::Vector2i LogicalToPixel(sf::Vector2f point) const;

private:
  friend class Engine;

  void BeginDrawing();
  const sf::Texture &FinishDrawing();
};
