#pragma once
#include <SFML/System/Vector2.hpp>
namespace Plaguefront::UI {
  inline constexpr float DesignHeight = 900.f, HeaderHeight = 72.f, Margin = 16.f, RosterWidth = 212.f, OrdersWidth = 334.f;
  inline constexpr sf::Vector2f BoardOrigin{244.f, 88.f};
  inline constexpr float BottomLogHeight = 48.f, RosterTop = 174.f, RosterStep = 58.f, RosterRowHeight = 54.f;
  inline constexpr unsigned TacticalPixelScale = 2, OverviewPixelScale = 1;
} // Espacio de nombres Plaguefront::UI
