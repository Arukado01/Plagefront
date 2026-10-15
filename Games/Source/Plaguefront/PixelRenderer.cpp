#include "Plaguefront/Game.h"
#include "Core/EngineConfig.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <set>

namespace Plaguefront {
  namespace {
    const sf::Color Gold{223, 190, 117}, Ink{17, 20, 29}, Panel{29, 33, 42}, Muted{164, 169, 158};
    const std::array<sf::Color, 2> Team{sf::Color{109, 186, 224}, sf::Color{225, 100, 120}};
    template <class X, class Y, class W, class H>
    void RectAt(sf::RenderTarget &target, X x, Y y, W w, H h, sf::Color color) {
      sf::RectangleShape rectangle({static_cast<float>(w), static_cast<float>(h)});
      rectangle.setPosition({std::round(static_cast<float>(x)), std::round(static_cast<float>(y))});
      rectangle.setFillColor(color);
      target.draw(rectangle);
    }
    void Poly(sf::RenderTarget &target, const std::vector<Point> &points, sf::Color color) {
      sf::ConvexShape shape(points.size());
      for (std::size_t i = 0; i < points.size(); ++i)
        shape.setPoint(i, {std::round(points[i].x), std::round(points[i].y)});
      shape.setFillColor(color);
      target.draw(shape);
    }
    void Line(sf::RenderTarget &target, Point from, Point to, sf::Color color, int thickness = 1, bool dotted = false) {
      int x = static_cast<int>(std::round(from.x)), y = static_cast<int>(std::round(from.y));
      int endX = static_cast<int>(std::round(to.x)), endY = static_cast<int>(std::round(to.y));
      int dx = std::abs(endX - x), sx = x < endX ? 1 : -1, dy = -std::abs(endY - y), sy = y < endY ? 1 : -1, error = dx + dy, step = 0;
      sf::VertexArray points(sf::PrimitiveType::Points);
      for (;;) {
        if (!dotted || step % 3 != 0)
          for (int ox = 0; ox < thickness; ++ox)
            for (int oy = 0; oy < thickness; ++oy)
              points.append({{x + ox + 0.5f, y + oy + 0.5f}, color});
        if (x == endX && y == endY)
          break;
        int twice = 2 * error;
        if (twice >= dy) {
          error += dy;
          x += sx;
        }
        if (twice <= dx) {
          error += dx;
          y += sy;
        }
        ++step;
      }
      target.draw(points);
    }
    void Outline(sf::RenderTarget &target, const Camera &camera, const Cell &cell, sf::Color color, int thick = 1, bool dotted = false) {
      auto top = camera.Surface(cell.pos, static_cast<float>(cell.height));
      for (std::size_t i = 0; i < top.size(); ++i)
        Line(target, top[i], top[(i + 1) % 6], color, thick, dotted);
    }
    void Atlas(sf::RenderTarget &target, const sf::Texture *texture, int column, int row, int w, int h, Point origin, Point position, int scale = 1, sf::Color color = sf::Color::White) {
      if (!texture)
        return;
      sf::Sprite sprite(*texture, sf::IntRect({column * w, row * h}, {w, h}));
      sprite.setOrigin({origin.x, origin.y});
      sprite.setPosition({std::round(position.x), std::round(position.y)});
      sprite.setScale({static_cast<float>(scale), static_cast<float>(scale)});
      sprite.setColor(color);
      target.draw(sprite);
    }
    sf::Color RotColor(sf::Color color, float opacity = 1.f) {
      color.a = static_cast<std::uint8_t>(std::lround(color.a * std::clamp(opacity, 0.f, 1.f)));
      return color;
    }
    void RotGround(sf::RenderTarget &target, const Camera &camera, const Cell &cell, const RotExposure &exposure, float elapsed) {
      if (!exposure.local && !exposure.global)
        return;
      const int zoom = camera.Zoom();
      const auto top = camera.Surface(cell.pos, static_cast<float>(cell.height));
      const auto p = camera.Project(cell.pos, static_cast<float>(cell.height));
      const float pulse = .5f + .5f * std::sin(elapsed * RotVisual.pulseRadiansPerSecond + cell.pos.q * .47f + cell.pos.r * .31f);
      // Dibuja una sola capa por celda con la misma precedencia local y global que utiliza el daño.
      const auto &palette = gConfig.rotPalette;
      Poly(target, std::vector<Point>(top.begin(), top.end()), RotColor(exposure.local ? palette.localFill : palette.globalFill, .75f + pulse * .25f));
      if (exposure.local) {
        Outline(target, camera, cell, RotColor(gConfig.rotPalette.sourceShadow, 215.f / 255.f), 2 * zoom);
        Outline(target, camera, cell, RotColor(gConfig.rotPalette.localEdge, .9f + pulse * .1f), zoom);
        // Las vetas breves permiten reconocer el terreno bajo la corrupción.
        const auto vein = RotColor(gConfig.rotPalette.localEdge, (125.f + pulse * 40.f) / 255.f);
        Line(target, {p.x - 12 * zoom, p.y + zoom}, {p.x - 7 * zoom, p.y - 4 * zoom}, vein, zoom);
        Line(target, {p.x - 9 * zoom, p.y - 2 * zoom}, {p.x - 5 * zoom, p.y - zoom}, vein, zoom);
        Line(target, {p.x + 3 * zoom, p.y + 5 * zoom}, {p.x + 9 * zoom, p.y - zoom}, vein, zoom);
      }
      const int count = exposure.local ? RotVisual.localSporeCount : RotVisual.globalSporeCount;
      for (int i = 0; i < count; ++i) {
        const float phase = std::fmod(elapsed * (.22f + i * .07f) + (cell.pos.q * 7 + cell.pos.r * 13 + i * 19 + 2048) * .071f, 1.f);
        const float x = p.x + ((i ? 7.f : -6.f) + 2.f * std::sin(phase * 6.2831853f)) * zoom;
        const float y = p.y + (3.f - phase * 7.f) * zoom;
        RectAt(target, x, y, zoom, zoom, RotColor(gConfig.rotPalette.spores, exposure.local ? .75f : .4f));
      }
    }
  } // Espacio de nombres

  const sf::Texture *Game::RotTexture(const sf::Texture *source) const {
    if (!source)
      return nullptr;
    if (const auto found = rotTextures_.find(source); found != rotTextures_.end())
      return &found->second;
    auto pixels = source->copyToImage();
    const auto size = pixels.getSize();
    for (unsigned y = 0; y < size.y; ++y)
      for (unsigned x = 0; x < size.x; ++x)
        pixels.setPixel({x, y}, gConfig.rotPalette.SpriteColor(pixels.getPixel({x, y})));
    sf::Texture recolored(pixels);
    recolored.setSmooth(false);
    const auto [found, inserted] = rotTextures_.try_emplace(source, std::move(recolored));
    (void)inserted;
    return &found->second;
  }

  sf::Vector2f Game::Center(Hex hex) const {
    auto cell = match_ ? match_->At(hex) : nullptr;
    auto p = camera_.Project(hex, cell ? static_cast<float>(cell->height) : 0.f);
    return {BoardOrigin.x + p.x * WorldScale(), BoardOrigin.y + p.y * WorldScale()};
  }

  void Game::Skin(sf::FloatRect bounds, int patch, sf::Color tint) const {
    if (layoutOnly_ || !skin_)
      return;
    constexpr int sourceCuts[4]{0, 8, 24, 32};
    float border = std::min({16.f, bounds.size.x / 3.f, bounds.size.y / 3.f});
    border = std::floor(border / 2) * 2;
    const float xs[]{bounds.position.x, bounds.position.x + border, bounds.position.x + bounds.size.x - border, bounds.position.x + bounds.size.x};
    const float ys[]{bounds.position.y, bounds.position.y + border, bounds.position.y + bounds.size.y - border, bounds.position.y + bounds.size.y};
    for (int y = 0; y < 3; ++y)
      for (int x = 0; x < 3; ++x) {
        int w = sourceCuts[x + 1] - sourceCuts[x], h = sourceCuts[y + 1] - sourceCuts[y];
        sf::Sprite sprite(*skin_, sf::IntRect({patch * 32 + sourceCuts[x], sourceCuts[y]}, {w, h}));
        sprite.setPosition({std::round(xs[x] / 2) * 2, std::round(ys[y] / 2) * 2});
        sprite.setScale({(xs[x + 1] - xs[x]) / w, (ys[y + 1] - ys[y]) / h});
        sprite.setColor(tint);
        ctx.renderer.Draw(sprite);
      }
  }

  void Game::RenderWorld() const {
    const Match &game = *match_;
    const auto &state = game.state;
    int zoom = camera_.Zoom();
    world_.clear({17, 23, 29});
    // El horizonte discreto y la sombra de base enmarcan el tablero elevado.
    sf::VertexArray backdrop(sf::PrimitiveType::Points);
    for (unsigned y = 0; y < world_.getSize().y; y += 8)
      for (unsigned x = (y % 16 ? 4u : 0u); x < world_.getSize().x; x += 12)
        backdrop.append({{static_cast<float>(x) + .5f, static_cast<float>(y) + .5f}, {30, 38, 43}});
    world_.draw(backdrop);
    const auto *selected = game.Find(selected_);
    bool own = selected && selected->Alive() && selected->owner == state.currentPlayer && (!selected->activated || state.activeUnit == selected_) && selected->readyRound <= state.round;
    const Match &targeting = own ? PreviewRules() : game;
    const bool canAct = own && targeting.state.activeUnit == selected_ && !targeting.Find(selected_)->acted;
    const bool movementContext = tactical_ == TacticalContext::Move || tactical_ == TacticalContext::Orders;
    const auto &reachable = reachable_;
    std::set<Hex> specialTargets;
    if (canAct && ability_ && (*ability_ == Ability::Purify || *ability_ == Ability::Incinerate)) {
      for (const auto &source : state.sources) {
        Match test = targeting;
        if (test.UseAbility(selected_, *ability_, source.pos))
          specialTargets.insert(source.pos);
      }
      for (const auto &corpse : state.corpses) {
        Match test = targeting;
        if (test.UseAbility(selected_, *ability_, corpse.pos))
          specialTargets.insert(corpse.pos);
      }
    }
    struct Entry {
      float depth{};
      int kind{};
      const Cell *cell{};
      const Unit *unit{};
      UnitVisual visual;
    };
    std::vector<Entry> queue;
    queue.reserve(state.cells.size() * 2 + state.units.size());
    for (const auto &cell : state.cells) {
      queue.push_back({GroundDepth(cell.pos), 0, &cell});
      queue.push_back({GroundDepth(cell.pos) + 0.5f, 1, &cell});
    }
    for (const auto &unit : state.units) {
      auto visual = visuals_.VisualFor(unit);
      if (visual.visible)
        queue.push_back({ColumnRise * visual.q + RowStep * visual.r + 1.f, 2, nullptr, &unit, std::move(visual)});
    }
    std::ranges::sort(queue, [](const Entry &a, const Entry &b) {if(a.depth!=b.depth)return a.depth<b.depth;if(a.kind!=b.kind)return a.kind<b.kind;if(a.cell&&b.cell)return a.cell->pos.q<b.cell->pos.q;return a.unit&&b.unit?a.unit->id<b.unit->id:false; });
    const int environmentFrame = static_cast<int>(animationTime_ * 5) % 4;
    for (const auto &entry : queue) {
      if (entry.kind == 0) {
        const Cell &cell = *entry.cell;
        auto p = camera_.Project(cell.pos, static_cast<float>(cell.height));
        auto top = camera_.Surface(cell.pos, static_cast<float>(cell.height));
        float extrusion = std::max(4, 6 * (cell.height + 1)) * static_cast<float>(zoom);
        const bool water = cell.terrain == Terrain::River || cell.terrain == Terrain::DeepWater;
        const std::array<sf::Color, 3> shades = water ? std::array<sf::Color, 3>{{{34, 49, 64}, {30, 44, 56}, {24, 36, 49}}} : std::array<sf::Color, 3>{{{86, 72, 56}, {69, 57, 48}, {48, 47, 44}}};
        for (int side = 0; side < 3; ++side) {
          auto a = top[side + 1], b = top[side + 2];
          Poly(world_, {a, b, {b.x, b.y + extrusion}, {a.x, a.y + extrusion}}, shades[side]);
          Line(world_, {a.x, a.y + extrusion - 1}, {b.x, b.y + extrusion - 1}, {24, 30, 32}, zoom);
          for (int band = 6 * zoom; band < extrusion - 2; band += 7 * zoom)
            Line(world_, {a.x + zoom, a.y + band}, {b.x - zoom, b.y + band}, {103, 87, 62, 80}, 1, true);
        }
        Atlas(world_, tiles_, static_cast<int>(cell.terrain), environmentFrame, 48, 32, {24, 16}, p, zoom);
        if (cell.terrain == Terrain::Road || cell.terrain == Terrain::Bridge) {
          for (auto neighbor : Neighbors(cell.pos))
            if (const auto *next = game.At(neighbor); next && (next->terrain == Terrain::Road || next->terrain == Terrain::Bridge)) {
              auto end = camera_.Project(neighbor, static_cast<float>(next->height));
              Point half{(p.x + end.x) / 2, (p.y + end.y) / 2};
              Line(world_, p, half, {69, 57, 43}, 5 * zoom);
              Line(world_, p, half, {149, 130, 85}, 3 * zoom);
            }
        }
        if (grid_)
          Outline(world_, camera_, cell, {182, 183, 137, 75});
        if (own && movementContext && !visuals_.Busy() && reachable.contains(cell.pos) && cell.pos != selected->pos) {
          Poly(world_, std::vector<Point>(top.begin(), top.end()), {58, 139, 180, 38});
          Outline(world_, camera_, cell, {100, 176, 212, 160}, 1, true);
        }
        RotGround(world_, camera_, cell, RotPresentationAt(game, cell.pos), animationTime_);
        if (own && movementContext && previewPath_) {
          const auto &path = previewPath_->cells;
          auto found = std::ranges::find(path, cell.pos);
          if (found != path.end()) {
            const auto index = static_cast<std::size_t>(found - path.begin());
            for (int direction : {-1, 1})
              if ((direction < 0 && index > 0) || (direction > 0 && index + 1 < path.size())) {
                const auto next = path[static_cast<std::size_t>(static_cast<int>(index) + direction)];
                const auto flat = camera_.Project(next, static_cast<float>(cell.height));
                Point half{(p.x + flat.x) / 2, (p.y + flat.y) / 2};
                Line(world_, p, half, {16, 23, 28}, 5 * zoom);
                Line(world_, p, half, Gold, 3 * zoom);
                if (direction > 0) {
                  const float dx = half.x - p.x, dy = half.y - p.y;
                  const float length = std::max(1.f, std::sqrt(dx * dx + dy * dy));
                  const Point tip{p.x + dx * .72f, p.y + dy * .72f};
                  for (float side : {-1.f, 1.f})
                    Line(world_, tip, {tip.x - dx / length * 5 * zoom + side * dy / length * 3 * zoom, tip.y - dy / length * 5 * zoom - side * dx / length * 3 * zoom}, {247, 238, 180}, zoom);
                }
              }
            if (index == 1)
              Outline(world_, camera_, cell, {250, 226, 134}, 2, true);
            if (index + 1 == path.size()) {
              Outline(world_, camera_, cell, {250, 226, 134}, 2);
              RectAt(world_, p.x - 2 * zoom, p.y - 2 * zoom, 4 * zoom, 4 * zoom, Gold);
            }
          }
        }
        if (const auto *target = game.Occupant(cell.pos); canAct && target && (tactical_ == TacticalContext::Orders || tactical_ == TacticalContext::Target) && targeting.CanTarget(selected_, target->id, ability_.value_or(Ability::Attack)))
          Outline(world_, camera_, cell, target->owner == selected->owner ? sf::Color{138, 213, 174} : sf::Color{227, 114, 91}, 2);
        if (specialTargets.contains(cell.pos))
          Outline(world_, camera_, cell, {181, 167, 238}, 2, true);
        if (selected && selected->Alive() && cell.pos == selected->pos)
          Outline(world_, camera_, cell, Gold, 2);
        if (hovered_ && cell.pos == *hovered_)
          Outline(world_, camera_, cell, {240, 234, 195}, 1);
      } else if (entry.kind == 1) {
        const Cell &cell = *entry.cell;
        auto p = camera_.Project(cell.pos, static_cast<float>(cell.height));
        int prop = cell.terrain == Terrain::Forest ? 0 : cell.terrain == Terrain::Ruins ? 1
                                                     : cell.terrain == Terrain::Blocked ? 2
                                                                                        : -1;
        if (cell.building != Building::None)
          prop = cell.building == Building::House ? 3 : cell.building == Building::Fortress ? 4
                                                                                            : 5;
        if (prop >= 0) {
          sf::Color tint = sf::Color::White;
          for (const auto &unit : state.units)
            if (unit.Alive() && (unit.id == selected_ || (hovered_ && unit.pos == *hovered_))) {
              auto visual = visuals_.VisualFor(unit);
              auto unitPoint = camera_.ProjectContinuous(visual.q, visual.r, visual.elevation);
              if (GroundDepth(cell.pos) > ColumnRise * visual.q + RowStep * visual.r && std::abs(unitPoint.x - p.x) < 26 * zoom && unitPoint.y > p.y - 50 * zoom && unitPoint.y < p.y + 3 * zoom) {
                tint.a = 100;
                break;
              }
            }
          Atlas(world_, props_, prop, environmentFrame, 64, 80, {32, 68}, p, zoom, tint);
        }
        if (cell.building != Building::None) {
          float y = p.y - (cell.building == Building::Fortress ? 45 : cell.building == Building::Outpost ? 33
                                                                                                         : 23) *
                              zoom;
          float x = p.x + 11 * zoom;
          Line(world_, {x, y + 8 * zoom}, {x, y - 2 * zoom}, {190, 176, 127}, zoom);
          auto team = cell.owner < 0 ? sf::Color{170, 172, 151} : Team[cell.owner];
          RectAt(world_, x + zoom, y + (environmentFrame % 2) * zoom, (7 - environmentFrame % 2) * zoom, 3 * zoom, team);
          if (cell.owner == 0)
            RectAt(world_, x + 4 * zoom, y + zoom, zoom, zoom, {240, 238, 197});
          else if (cell.owner == 1) {
            RectAt(world_, x + 3 * zoom, y, zoom, 3 * zoom, {59, 46, 34});
            RectAt(world_, x + 6 * zoom, y, zoom, 3 * zoom, {59, 46, 34});
          }
          if (cell.captureProgress > 0)
            Outline(world_, camera_, cell, Team[cell.capturingPlayer], 2, true);
        }
        for (const auto &corpse : state.corpses)
          if (corpse.pos == cell.pos) {
            const auto *dead = game.Find(corpse.unitId);
            if (!dead || !visuals_.VisualFor(*dead).visible) {
              RectAt(world_, p.x - 14 * zoom, p.y - 2 * zoom, 28 * zoom, 5 * zoom, {12, 16, 20, 190});
              Atlas(world_, units_, static_cast<int>(AnimState::Death) * 8 + 7, static_cast<int>(corpse.type) + 7 * corpse.owner, 48, 64, {24, 56}, p, zoom);
            }
          }
        for (const auto &fire : state.fires)
          if (fire.pos == cell.pos)
            Atlas(world_, spellEffects_, 2, environmentFrame, 48, 48, {24, 24}, {p.x, p.y - 5 * zoom}, zoom);
        for (const auto &source : state.sources)
          if (source.remaining > 0 && source.pos == cell.pos) {
            const float pulse = .5f + .5f * std::sin(animationTime_ * RotVisual.pulseRadiansPerSecond);
            const int radius = static_cast<int>(11 + 3 * pulse) * zoom;
            Poly(world_, {{p.x - radius, p.y}, {p.x, p.y - 7 * zoom}, {p.x + radius, p.y}, {p.x, p.y + 7 * zoom}}, RotColor(gConfig.rotPalette.sourceShadow, 235.f / 255.f));
            Line(world_, {p.x - radius, p.y}, {p.x, p.y - 7 * zoom}, gConfig.rotPalette.sourceGlow, zoom);
            Line(world_, {p.x, p.y + 7 * zoom}, {p.x + radius, p.y}, gConfig.rotPalette.localEdge, zoom);
            Atlas(world_, RotTexture(props_), 7, environmentFrame, 64, 80, {32, 68}, p, zoom);
          }
      } else {
        const Unit &unit = *entry.unit;
        const auto &visual = entry.visual;
        auto p = camera_.ProjectContinuous(visual.q, visual.r, visual.elevation);
        RectAt(world_, p.x - 8 * zoom, p.y + zoom, 16 * zoom, 2 * zoom, {10, 15, 24, 140});
        RectAt(world_, p.x - 5 * zoom, p.y + 3 * zoom, 10 * zoom, zoom, {10, 15, 24, 100});
        sf::Color tint = unit.activated && state.activeUnit != unit.id ? sf::Color{164, 173, 169} : sf::Color::White;
        Atlas(world_, units_, static_cast<int>(visual.state) * 8 + std::clamp(visual.frame, 0, 7), static_cast<int>(unit.type) + 7 * unit.owner + 14 * visual.facing, 48, 64, {24, 56}, p, zoom, tint);
        if (unit.Alive()) {
          // Distingue el bando por forma además de color: un rombo frente a dos barras.
          if (unit.owner == 0) {
            RectAt(world_, p.x - 14 * zoom, p.y + 4 * zoom, 3 * zoom, zoom, Team[0]);
            RectAt(world_, p.x - 13 * zoom, p.y + 3 * zoom, zoom, 3 * zoom, Team[0]);
          } else {
            RectAt(world_, p.x - 14 * zoom, p.y + 3 * zoom, zoom, 3 * zoom, Team[1]);
            RectAt(world_, p.x - 12 * zoom, p.y + 3 * zoom, zoom, 3 * zoom, Team[1]);
          }
          if (unit.guard || unit.smoke || unit.pike || unit.marked || unit.slowThroughRound == state.round)
            RectAt(world_, p.x + 10 * zoom, p.y - 18 * zoom, 3 * zoom, 3 * zoom, unit.marked ? sf::Color{226, 126, 101} : sf::Color{177, 186, 222});
        }
      }
    }
    if (state.globalRemaining > 0) {
      const float width = static_cast<float>(world_.getSize().x), height = static_cast<float>(world_.getSize().y);
      for (int i = 0; i < 32; ++i) {
        const float x = std::fmod(i * 47.f + animationTime_ * 5, width), y = std::fmod(i * 37.f + animationTime_ * 2, height);
        RectAt(world_, x, y, 17 + (i % 4) * 5.f, 1, RotColor(gConfig.rotPalette.spores, .18f));
      }
    }
    // Conserva las señales esenciales sin efectos cosméticos y dibuja las fichas de HP después, en la interfaz.
    for (const auto &source : state.sources)
      if (source.remaining > 0) {
        const auto *cell = game.At(source.pos);
        if (!cell)
          continue;
        const auto p = camera_.Project(source.pos, static_cast<float>(cell->height));
        const float pulse = .5f + .5f * std::sin(animationTime_ * RotVisual.pulseRadiansPerSecond);
        RectAt(world_, p.x - 10 * zoom, p.y - 26 * zoom, 22 * zoom, 18 * zoom, RotColor(gConfig.rotPalette.sourceShadow, 245.f / 255.f));
        Line(world_, {p.x - 10 * zoom, p.y - 26 * zoom}, {p.x + 11 * zoom, p.y - 26 * zoom}, RotColor(gConfig.rotPalette.localEdge, (190.f + pulse * 65.f) / 255.f), zoom);
        Atlas(world_, RotTexture(icons_), 4, 2, 16, 16, {8, 8}, {p.x - zoom, p.y - 17 * zoom}, zoom);
        for (int i = 0; i < RotVisual.sourceSporeCount; ++i) {
          const float phase = std::fmod(animationTime_ * (.45f + i * .035f) + i * .17f + source.id * .03f, 1.f);
          const float x = p.x + (std::sin(i * 2.4f + phase * 2.8f) * (8.f + phase * 9.f)) * zoom;
          const float y = p.y - (3.f + phase * 32.f) * zoom;
          RectAt(world_, x, y, zoom, zoom, RotColor(gConfig.rotPalette.spores, .625f + (1.f - phase) * .375f));
        }
      }
    // Dibuja la selección por encima para que un árbol o techo posterior no la oculte.
    if (selected && selected->Alive()) {
      const auto visual = visuals_.VisualFor(*selected);
      const auto p = camera_.ProjectContinuous(visual.q, visual.r, visual.elevation);
      const float y = p.y - 36 * zoom;
      RectAt(world_, p.x - 3 * zoom, y, 6 * zoom, zoom, Gold);
      RectAt(world_, p.x - 2 * zoom, y + zoom, 4 * zoom, zoom, Gold);
      RectAt(world_, p.x - zoom, y + 2 * zoom, 2 * zoom, zoom, Gold);
      for (const int side : {-1, 1}) {
        RectAt(world_, p.x + side * 13 * zoom, p.y, zoom, 8 * zoom, Gold);
        RectAt(world_, p.x + (side < 0 ? -13 : 10) * zoom, p.y + 8 * zoom, 4 * zoom, zoom, Gold);
      }
    }
    for (const auto &effect : visuals_.Effects())
      if (effect.Active()) {
        float t = effect.Progress();
        auto from = camera_.Project(effect.start, effect.startHeight), to = camera_.Project(effect.end, effect.endHeight);
        Point p = to;
        int column = 0;
        switch (effect.type) {
        case EffectType::Arrow:
          p = {from.x + (to.x - from.x) * t, from.y + (to.y - from.y) * t - 14 * std::sin(t * 3.14159265f) * zoom};
          column = 1;
          break;
        case EffectType::Fire:
          column = 2;
          break;
        case EffectType::Ice:
          column = 3;
          break;
        case EffectType::Lightning:
          column = 4;
          break;
        case EffectType::Heal:
          column = 5;
          break;
        case EffectType::RotBurst:
          column = 6;
          break;
        case EffectType::Purify:
          column = 7;
          break;
        case EffectType::Capture:
          column = 5;
          break;
        case EffectType::Spawn:
          column = 7;
          break;
        case EffectType::Smoke:
          column = 6;
          break;
        default:
          column = 0;
          break;
        }
        const auto *effectTexture = effect.type == EffectType::RotBurst ? RotTexture(spellEffects_) : spellEffects_;
        if (effect.type == EffectType::Damage) {
          continue; // Dibuja los avisos de texto en la interfaz a resolución de salida.
        } else if (effect.type == EffectType::Lightning) {
          Line(world_, {from.x, from.y - 18 * zoom}, {(from.x + to.x) / 2 + 4 * zoom, (from.y + to.y) / 2 - 24 * zoom}, {225, 232, 255}, 2 * zoom);
          Line(world_, {(from.x + to.x) / 2 + 4 * zoom, (from.y + to.y) / 2 - 24 * zoom}, {to.x, to.y - 18 * zoom}, {143, 166, 255}, 2 * zoom);
          Atlas(world_, effectTexture, column, std::min(3, static_cast<int>(t * 4)), 48, 48, {24, 24}, {p.x, p.y - 12 * zoom}, zoom);
        } else
          Atlas(world_, effectTexture, column, std::min(3, static_cast<int>(t * 4)), 48, 48, {24, 24}, {p.x, p.y - 12 * zoom}, zoom);
      }
    if (effects_) {
      const float width = static_cast<float>(world_.getSize().x), height = static_cast<float>(world_.getSize().y);
      if (state.weather == Weather::Rain || state.weather == Weather::Storm)
        for (int i = 0; i < 64; ++i) {
          float x = std::fmod(i * 83.f, width), y = std::fmod(i * 53.f + animationTime_ * 85.f, height);
          Line(world_, {x, y}, {x - 2, y + 5}, {140, 162, 184, 90});
        }
      if (state.weather == Weather::Fog)
        for (int i = 0; i < 24; ++i) {
          float x = std::fmod(i * 47.f + animationTime_ * 3, width), y = std::fmod(45.f + i * 37.f, height);
          RectAt(world_, x, y, 25 + (i % 4) * 4.f, 2, {174, 182, 165, 30});
        }
    }
    world_.display();
  }

  void Game::RenderWorldLabels() const {
    RenderTacticalInformation();
    const auto &game = *match_;
    const auto bounds = BoardBounds();
    const float scale = WorldScale();
    const int zoom = camera_.Zoom();
    const auto ui = [&](Point p) { return sf::Vector2f{BoardOrigin.x + p.x * scale, BoardOrigin.y + p.y * scale}; };
    for (const auto &source : game.state.sources)
      if (source.remaining > 0 && game.At(source.pos)) {
        const auto p = ui(camera_.Project(source.pos, static_cast<float>(game.At(source.pos)->height)));
        if (bounds.contains(p))
          Text(std::to_string(source.remaining), p.x + 8 * zoom * scale, p.y - 21 * zoom * scale, 17, gConfig.rotPalette.uiText);
      }
    for (const auto &effect : visuals_.Effects())
      if (effect.Active() && effect.type == EffectType::Damage) {
        const auto p = ui(camera_.Project(effect.end, effect.endHeight));
        const float y = p.y - (27 * zoom + 10 * effect.Progress()) * scale;
        if (bounds.contains({p.x, y})) {
          const auto label = effect.text.empty() ? std::to_string(effect.amount) : effect.text;
          Text(label, p.x - 6 * scale + 1, y + 1, 23, {15, 19, 24});
          Text(label, p.x - 6 * scale, y, 23, effect.amount < 0 ? sf::Color{148, 223, 172} : sf::Color{255, 183, 135});
        }
      }
    if (previewPath_ && !previewPath_->cells.empty() && tactical_ == TacticalContext::Orders) {
      const auto *unit = game.Find(selected_);
      const auto *cell = game.At(previewPath_->cells.back());
      if (unit && cell) {
        const float x = BoardOrigin.x + 12, y = bounds.position.y + bounds.size.y - 74;
        Box({{x, y}, {std::min(675.f, bounds.size.x - 24), 62}}, sf::Color{23, 31, 35}, Gold);
        const int budget = unit->retreatRemaining > 0 ? unit->retreatRemaining : game.EffectiveStats(*unit).movement;
        bool extra = false;
        for (std::size_t i = 1; i < previewPath_->cells.size(); ++i)
          extra |= game.MoveCost(*unit, previewPath_->cells[i - 1], previewPath_->cells[i]) > 1;
        Text("DESTINO (" + std::to_string(cell->pos.q) + ", " + std::to_string(cell->pos.r) + ")  ·  " + Name(cell->terrain), x + 14, y + 8, 18, Gold);
        Text("Coste " + std::to_string(previewPath_->cost) + " MOV  ·  Restan " + std::to_string(budget - previewPath_->cost) + (extra ? "  ·  Incluye sobrecostes" : ""), x + 14, y + 34, 18);
      }
    }
  }

  void Game::RenderBoard() const {
    const auto &g = *match_;
    const auto &s = g.state;
    const float width = UiWidth();
    Box({{0, 0}, {width, UiHeight()}}, Ink);
    if (!layoutOnly_) {
      RenderWorld();
      sf::Sprite board(world_.getTexture());
      board.setPosition(BoardOrigin);
      const float scale = WorldScale();
      board.setScale({scale, scale});
      ctx.renderer.Draw(board);
      RenderGamepadCursor();
      RenderWorldLabels();
    }
    Box({{0, 0}, {width, 72}}, Panel);
    Text("PLAGUEFRONT", 22, 19, 26, Gold);
    Text("RONDA " + std::to_string(s.round), 270, 24, 20);
    Text(s.players[s.currentPlayer].name, 402, 24, 20, Team[s.currentPlayer]);
    Text(Name(s.weather), 706, 24, 18, Muted);
    if (icons_ && !layoutOnly_) {
      sf::Sprite coin(*icons_, sf::IntRect({5 * 16, 16}, {16, 16}));
      coin.setPosition({22, 50});
      ctx.renderer.Draw(coin);
    }
    Text(std::to_string(s.players[s.currentPlayer].coins) + " monedas  |  +" + std::to_string(g.Income(s.currentPlayer)) + " por turno", 46, 47, 18, Gold);
    ButtonAt("CENTRAR", width - 548, 16, 118, 40, CenterCamera);
    ButtonAt("MAPA · V", width - 420, 16, 128, 40, FrameMap);
    ButtonAt("PAUSA", width - 282, 16, 114, 40, Pause);
    ButtonAt("GUARDAR", width - 158, 16, 142, 40, Save);
    Text(s.scenario, 22, UiHeight() - 99, 17, Gold);
    if (s.globalRemaining > 0) {
      if (icons_ && !layoutOnly_) {
        const float uiScale = ctx.renderer.UiScale(), factor = std::max(1.f, std::round(uiScale)) / uiScale;
        sf::Sprite plague(*RotTexture(icons_), sf::IntRect({4 * 16, 2 * 16}, {16, 16}));
        plague.setScale({factor, factor});
        plague.setPosition({width - 858 - 16 * factor, UiHeight() - 99});
        ctx.renderer.Draw(plague);
      }
      Text("PESTE GLOBAL ACTIVA · Grupo " + std::to_string(g.LargestSourceGroup()) + " · " + std::to_string(s.globalRemaining) + " R", width - 850, UiHeight() - 99, 17, gConfig.rotPalette.uiText);
    } else
      Text("Cadáveres " + std::to_string(s.corpses.size()) + " · Fuentes " + std::to_string(s.sources.size()) + " · Grupo " + std::to_string(g.LargestSourceGroup()), width - 850, UiHeight() - 99, 17, Muted);
    Text("Muerte → espera " + std::to_string(g.config.rotWait) + " R → fuente · radio " + std::to_string(g.config.rotRadius) + " · " + std::to_string(g.config.globalThreshold) + " fuentes conectadas: peste", BoardOrigin.x, UiHeight() - 83, 16, Muted);
    Box({{16, UiHeight() - 62}, {width - 32, UI::BottomLogHeight}}, Panel);
    Wrapped(message_.empty() ? "Selecciona una unidad. Rueda: zoom · Central/WASD: cámara · Inicio: centrar · V: mapa completo" : message_, 30, UiHeight() - 53, width - 60, 17);
    RenderRoster();
    RenderPanel();
  }

} // Espacio de nombres Plaguefront
