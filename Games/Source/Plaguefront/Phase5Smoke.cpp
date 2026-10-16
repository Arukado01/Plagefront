#include "Plaguefront/Game.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace Plaguefront {
  void Game::SmokePhase5(int step) {
    static int checks = 0;
    if (step == 0)
      checks = 0;
    auto require = [&](bool ok, const std::string &why) {++checks;if(!ok)throw std::runtime_error("Phase5: "+why); };
    auto applyDisplay = [&] {ctx.window.ApplyPending();ctx.renderer.ConfigureOutput(ctx.window.OutputSize());ctx.cursor.SetView(ctx.renderer.View());RefreshLayout();displayRevision_=ctx.window.Revision(); };
    auto resize = [&](sf::Vector2u size) {ctx.window.SetFullscreen(false);ctx.window.SetResolution(size);applyDisplay();require(ctx.renderer.OutputSize()==size,"actual resize mismatch"); };
    auto useMouse = [&] {ctx.input.ProcessEvent(sf::Event::MouseMoved{{8,8}});ctx.cursor.SetVirtualPosition({}); };
    auto fresh = [&] {useMouse();mode_=Mode::Local;scenario_=2;LoadArmies();BeginGame();selected_=match_->state.units.front().id;selectedCell_=match_->Find(selected_)->pos;SetContext(TacticalContext::Orders);FocusCamera(false); };
    auto snapshot = [&](const std::string &suffix) {std::string error;const auto path="build/test-output/phase5-layout-"+suffix+".json";require(match_->Save(path,error),"snapshot save failed: "+error);std::ifstream input(path);std::ostringstream bytes;bytes<<input.rdbuf();return bytes.str(); };
    auto verifyButtons = [&] {
      RebuildControls();
      require(!buttons_.empty(), "layout contains no interactive controls");
      for (const auto &button : buttons_) {
        require(button.bounds.position.x >= 0 && button.bounds.position.y >= 0 && button.bounds.position.x + button.bounds.size.x <= UiWidth() + 1 && button.bounds.position.y + button.bounds.size.y <= UiHeight() + 1, "control outside responsive viewport: " + button.label);
        const auto center = button.bounds.position + button.bounds.size / 2.f;
        require(button.bounds.contains(ctx.renderer.PixelToLogical(ctx.renderer.LogicalToPixel(center))), "visual center and hitbox disagree: " + button.label);
      }
    };
    auto verifyAllLayouts = [&] {
      const auto originalScreen = screen_;
      const auto originalContext = tactical_;
      const auto originalCell = selectedCell_;
      const auto originalFocus = keyboardFocus_;
      for (const auto &cell : match_->state.cells)
        if (cell.owner == match_->state.currentPlayer && cell.building == Building::Fortress) {
          selectedCell_ = cell.pos;
          break;
        }
      for (int i = 0; i <= static_cast<int>(TacticalContext::Purchase); ++i) {
        screen_ = Screen::Playing;
        SetContext(static_cast<TacticalContext>(i));
        verifyButtons();
      }
      for (const auto screen : {Screen::Menu, Screen::Setup, Screen::Settings, Screen::Credits, Screen::Paused, Screen::ConfirmExit, Screen::Handoff, Screen::ConfirmTurn, Screen::Controls}) {
        screen_ = screen;
        verifyButtons();
      }
      screen_ = originalScreen;
      selectedCell_ = originalCell;
      SetContext(originalContext);
      keyboardFocus_ = originalFocus;
      RebuildControls();
    };
    auto verifyGeometry = [&] {
      const int selection = selected_;
      const auto tactical = tactical_;
      const auto zoom = camera_.Zoom();
      RebuildControls();
      require(!buttons_.empty(), "layout contains no interactive controls");
      for (const auto &button : buttons_) {
        require(button.bounds.position.x >= 0 && button.bounds.position.y >= 0 && button.bounds.position.x + button.bounds.size.x <= UiWidth() + 1 && button.bounds.position.y + button.bounds.size.y <= UiHeight() + 1, "control outside responsive viewport: " + button.label);
        const auto center = button.bounds.position + button.bounds.size / 2.f;
        require(button.bounds.contains(ctx.renderer.PixelToLogical(ctx.renderer.LogicalToPixel(center))), "visual center and hitbox disagree: " + button.label);
      }
      require(std::abs(WorldScale() * ctx.renderer.UiScale() - worldPixelScale_) < .001f, "world pixels are not integer physical pixels");
      const auto *unit = match_->Find(selected_);
      require(unit != nullptr, "selection lost during resize");
      const auto point = Center(unit->pos);
      require(BoardBounds().contains(point), "selected unit left viewport after resize");
      const auto converted = ctx.renderer.PixelToLogical(ctx.renderer.LogicalToPixel(point));
      const auto picked = PickCell(converted);
      require(picked && *picked == unit->pos, "elevated selection disagrees after resize");
      require(selected_ == selection && tactical_ == tactical && camera_.Zoom() == zoom, "resize reset tactical selection context or zoom");
    };
    auto clickAction = [&](int action) {
      RebuildControls();
      const auto found = std::ranges::find_if(buttons_, [&](const Button &b) { return b.action == action && b.enabled; });
      require(found != buttons_.end(), "requested actual UI action is unavailable");
      sf::Event::MouseButtonPressed event{};
      event.button = sf::Mouse::Button::Left;
      event.position = ctx.renderer.LogicalToPixel(found->bounds.position + found->bounds.size / 2.f);
      ctx.input.ProcessEvent(sf::Event(event));
      OnEvent(sf::Event(event));
    };
    auto verifyTreeHover = [&] {
      const auto pixels = props_->copyToImage();
      const float originalTime = animationTime_;
      for (int scenario = 0; scenario < 3; ++scenario) {
        useMouse();
        mode_ = Mode::Local;
        scenario_ = scenario;
        LoadArmies();
        BeginGame();
        auto tree = std::ranges::find_if(match_->state.cells, [](const Cell &cell) { return cell.terrain == Terrain::Forest && cell.building == Building::None; });
        require(tree != match_->state.cells.end(), "tree hover fixture has no forest in scenario " + std::to_string(scenario));
        const Cell forest = *tree;
        Unit occupant = match_->state.units.front();
        occupant.pos = forest.pos;
        // Aísla un árbol real del escenario para distinguir el hover de las oclusiones de otras celdas.
        match_->state.cells = {forest};
        match_->state.units.clear();
        match_->state.sources.clear();
        match_->state.corpses.clear();
        match_->state.fires.clear();
        match_->state.globalRemaining = 0;
        match_->state.weather = Weather::Clear;
        selected_ = -1;
        ResetPresentation();
        for (int zoom = 1; zoom <= 3; ++zoom) {
          const int frame = (scenario + zoom) % 4;
          animationTime_ = (frame + .1f) / 5.f;
          require(camera_.SetZoom(zoom, {}), "tree hover integer zoom rejected");
          camera_.CenterOn(forest.pos, static_cast<float>(forest.height), {world_.getSize().x / 2.f, (world_.getSize().y + 60.f * zoom) / 2.f});
          const auto anchor = camera_.Project(forest.pos, static_cast<float>(forest.height));
          sf::Vector2i leaf{-1, -1};
          for (int y = 0; y < 12 && leaf.x < 0; ++y)
            for (int x = 0; x < 64; ++x)
              if (pixels.getPixel({static_cast<unsigned>(x), static_cast<unsigned>(frame * 80 + y)}).a == 255) {
                leaf = {x, y};
                break;
              }
          require(leaf.x >= 0, "tree atlas has no opaque upper canopy pixel");
          const Point leafPoint{std::round(anchor.x) + (leaf.x - 32 + .5f) * zoom, std::round(anchor.y) + (leaf.y - 68 + .5f) * zoom};
          const sf::Vector2u sample{static_cast<unsigned>(std::floor(leafPoint.x)), static_cast<unsigned>(std::floor(leafPoint.y))};
          auto hover = [&](Point nativePoint) {
            const sf::Vector2f logical{BoardOrigin.x + nativePoint.x * WorldScale(), BoardOrigin.y + nativePoint.y * WorldScale()};
            require(BoardBounds().contains(logical), "tree hover test point is outside the board");
            ctx.cursor.SetVirtualPosition(logical);
            hovered_ = PickCell(logical);
          };
          auto renderedLeaf = [&] {
            RenderWorld();
            return world_.getTexture().copyToImage().getPixel(sample);
          };
          ctx.cursor.SetVirtualPosition(sf::Vector2f{4, 4});
          hovered_.reset();
          const auto opaque = renderedLeaf();
          hover(leafPoint);
          require(!hovered_, "upper canopy must exercise hover beyond the ground hex");
          const auto faded = renderedLeaf();
          require(faded != opaque, "empty forest did not fade when cursor crossed its canopy");
          hover(anchor);
          require(hovered_ == forest.pos, "tree ground center was not picked");
          require(renderedLeaf() == faded, "empty forest did not fade when cursor crossed its cell");
          hover({std::round(anchor.x) - 31.5f * zoom, std::round(anchor.y) - 67.5f * zoom});
          require(!hovered_, "transparent sprite corner must be beyond the ground hex");
          require(renderedLeaf() == opaque, "transparent atlas padding incorrectly faded the forest");
          ctx.cursor.SetVirtualPosition(sf::Vector2f{4, 4});
          hovered_.reset();
          require(renderedLeaf() == opaque, "tree opacity did not recover after the cursor left");
          match_->state.units = {occupant};
          visuals_.Reset(*match_);
          require(renderedLeaf() == faded, "occupied forest must fade even without a selected unit");
          match_->state.units.clear();
          visuals_.Reset(*match_);
        }
      }
      animationTime_ = originalTime;
      ctx.cursor.SetVirtualPosition({});
      std::cout << "Plaguefront tree hover: empty canopy beyond hex, ground cell, transparent padding, leave and unselected occupant passed in three scenarios at zooms 1..3.\n";
    };
    if (step == 0) {
      resize({1440, 900});
      fresh();
      const auto before = snapshot("before");
      const int selection = selected_;
      for (const auto size : {sf::Vector2u{1280, 720}, sf::Vector2u{1366, 768}, sf::Vector2u{1440, 900}, sf::Vector2u{1920, 1080}, sf::Vector2u{1100, 1100}}) {
        resize(size);
        verifyGeometry();
        verifyAllLayouts();
      }
      for (int i = 0; i < 24; ++i) {
        resize({static_cast<unsigned>(1100 + i * 12), static_cast<unsigned>(700 + i * 7)});
        verifyGeometry();
      }
      require(before == snapshot("after"), "continuous resize changed the serialized match");
      require(selected_ == selection, "continuous resize discarded selected unit");
      resize({1280, 720});
      verifyGeometry();
      clickAction(Pause);
      require(screen_ == Screen::Paused, "click at resized pause control failed");
      clickAction(Resume);
      require(screen_ == Screen::Playing, "click at resized resume control failed");
      message_ = "1280×720: paneles anclados, clics sobre sus hitboxes y selección elevada verificados sin reiniciar la partida.";
    } else if (step == 1) {
      resize({1366, 768});
      verifyGeometry();
      message_ = "1366×768: misma selección, cámara y estado lógico; el layout usa el tamaño actual de salida.";
    } else if (step == 2) {
      resize({1440, 900});
      verifyGeometry();
      message_ = "1440×900: interfaz y tablero conservan espacios separados y píxeles de mundo enteros.";
    } else if (step == 3) {
      resize({1920, 1080});
      verifyGeometry();
      message_ = "1920×1080: textos rasterizados a resolución de salida y botones coherentes con sus áreas interactivas.";
    } else if (step == 4) {
      ctx.window.SetFullscreen(true);
      applyDisplay();
      verifyGeometry();
      require(ctx.window.IsFullscreen(), "fullscreen not applied");
      message_ = "Pantalla completa real: misma partida y selección. F11 restaura la ventana.";
    } else if (step == 5) {
      resize({1100, 1100});
      verifyGeometry();
      message_ = "Ventana alta: el viewport se expande verticalmente; lista, tablero y barras se anclan a sus bordes.";
    } else if (step == 6) {
      resize({1440, 900});
      const auto before = snapshot("maximize-before");
      const auto size = ctx.window.OutputSize();
      require(ctx.window.Maximize(), "native maximize unavailable");
      applyDisplay();
      require(ctx.window.IsMaximized(), "native window was not maximized");
      verifyGeometry();
      require(ctx.window.Restore(), "native restore unavailable");
      applyDisplay();
      require(!ctx.window.IsMaximized(), "native restore left maximized state");
      resize(size);
      verifyGeometry();
      require(before == snapshot("maximize-after"), "maximize/restore mutated match");
      message_ = "Maximizar y restaurar ejecutados por WindowManager; el estado completo de la partida sigue idéntico.";
    } else if (step >= 7 && step <= 10) {
      resize({1440, 900});
      if (step == 7)
        SmokeGamepad();
      PrepareGamepadCapture(step - 7);
      RebuildControls();
      SyncGamepadCursor();
    } else if (step >= 11 && step <= 17) {
      resize({1440, 900});
      useMouse();
      SmokeRotPhase5(step - 11);
    } else if (step == 18) {
      resize({1100, 1100});
      previous_ = Screen::Playing;
      ChangeScreen(Screen::Settings);
      RebuildControls();
      for (const auto &button : buttons_)
        require(button.bounds.position.y + button.bounds.size.y <= UiHeight(), "centered tall settings clipped");
      message_ = "Configuración centrada en ventana alta.";
    } else if (step == 19) {
      resize({1366, 768});
      fresh();
      SetContext(TacticalContext::Inventory);
      RebuildControls();
      require(tactical_ == TacticalContext::Inventory, "resize lost inventory context");
      message_ = "Inventario y menú contextual después del redimensionamiento.";
    } else if (step == 20) {
      resize({1440, 900});
      verifyTreeHover();
      fresh();
      const auto *unit = match_->Find(selected_);
      const auto pos = unit->pos;
      SetContext(TacticalContext::Orders);
      FocusCamera(false);
      const auto *cell = match_->At(pos);
      const auto anchor = camera_.Project(pos, static_cast<float>(cell->height));
      require(camera_.SetZoom(2, anchor), "zoom fixture failed");
      verifyGeometry();
      message_ = "Zoom ×2 tras cambiar resolución: las fichas, el picking y el cursor comparten la transformación real.";
    } else if (step == 21) {
      resize({1100, 1100});
      ChangeScreen(Screen::Setup);
      RebuildControls();
      verifyButtons();
      require(std::abs(layoutYOffset_) < .001f, "render leaked centered layout offset");
      message_ = "Preparación centrada: los retratos, los textos y los botones comparten el desplazamiento vertical.";
      std::cout << "Plaguefront phase5 layout: " << checks << " checks passed; eleven tactical layouts and nine screens at five resolutions, 24 continuous native resizes, full-screen, maximize/restore, hitboxes and unchanged serialized match.\n";
      SmokeVisualCorrections();
      SmokeUnitIntegration();
    }
    if (ctx.input.Device() != Input::Device::Gamepad)
      ctx.cursor.SetPosition({BoardOrigin.x + BoardBounds().size.x * .6f, BoardOrigin.y + BoardBounds().size.y * .62f});
  }
} // Espacio de nombres Plaguefront
