#include "Plaguefront/Game.h"
#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <iostream>
#include <stdexcept>

namespace Plaguefront {
  // Las pruebas usan los controles reales con partidas y preferencias aisladas de los archivos del usuario.
  void Game::SmokeStep(int step) {
    if (step >= 49) {
      SmokePhase5(step - 49);
      return;
    }
    if (step >= 28) {
      SmokePhase4(step - 28);
      return;
    }
    static int checks = 0;
    if (step == 0)
      checks = 0;
    const auto require = [&](bool ok, const char *why) {++checks;if(!ok)throw std::runtime_error(why); };
    auto applyDisplay = [&](bool frame = true) {
      ctx.window.ApplyPending();
      ctx.renderer.ConfigureOutput(ctx.window.OutputSize());
      ctx.cursor.SetView(ctx.renderer.View());
      RefreshLayout(frame);
      displayRevision_ = ctx.window.Revision();
    };
    auto setOutput = [&](sf::Vector2u size) {
      ctx.window.SetFullscreen(false);
      ctx.window.SetResolution(size);
      applyDisplay();
      require(ctx.window.OutputSize() == size && ctx.renderer.OutputSize() == size, "Smoke: output resolution not applied exactly");
      const sf::Vector2f point{718.25f, 413.75f};
      const auto restored = ctx.renderer.PixelToLogical(ctx.renderer.LogicalToPixel(point));
      require(std::abs(restored.x - point.x) <= 1.f / ctx.renderer.UiScale() && std::abs(restored.y - point.y) <= 1.f / ctx.renderer.UiScale(), "Smoke: UI/pixel conversion drifted after resize");
      std::cout << "Plaguefront output runtime: " << size.x << 'x' << size.y << ", UI " << ctx.renderer.LogicalSize().x << "x900, scale=" << ctx.renderer.UiScale() << ".\n";
    };
    auto newLocal = [&](int scenario = 0) {
      mode_ = Mode::Local;
      scenario_ = scenario;
      LoadArmies();
      BeginGame();
      require(match_.has_value(), "Smoke: match missing");
      SetContext(TacticalContext::Selection);
      RefreshLayout(true);
    };
    auto selectUnit = [&](int id) {
      selected_ = -1;
      SetContext(TacticalContext::Selection);
      HandleCell(match_->Find(id)->pos);
      require(selected_ == id && tactical_ == TacticalContext::Orders, "Smoke: allied selection did not open unit orders");
    };
    auto prepareCombat = [&](UnitClass type, Hex attackerPos, Hex targetPos) {
      newLocal();
      auto actor = std::ranges::find_if(match_->state.units, [&](const Unit &u) { return u.owner == 0 && u.type == type; });
      auto victim = std::ranges::find_if(match_->state.units, [](const Unit &u) { return u.owner == 1 && u.type == UnitClass::Saber; });
      require(actor != match_->state.units.end() && victim != match_->state.units.end(), "Smoke: combat fixture missing units");
      actor->pos = attackerPos;
      victim->pos = targetPos;
      const int actorId = actor->id, victimId = victim->id;
      ResetPresentation();
      selectUnit(actorId);
      FocusCamera(false);
      return victimId;
    };
    auto ownFortress = [&] {
      auto cell = std::ranges::find_if(match_->state.cells, [](const Cell &c) { return c.building == Building::Fortress && c.owner == 0; });
      require(cell != match_->state.cells.end(), "Smoke: own fortress missing");
      return cell->pos;
    };
    auto openProperty = [&](int tab) {
      selectedCell_ = ownFortress();
      Dispatch({{}, "", OpenBuilding});
      require(tactical_ == TacticalContext::Building, "Smoke: property context missing");
      Dispatch({{}, "", Tab, tab});
      require(tactical_ == (tab == 1 ? TacticalContext::Recruitment : TacticalContext::Shop), "Smoke: economic submenu missing");
    };
    auto assertNoEconomy = [&] {
      buttons_.clear();
      RenderPanel();
      for (const auto &button : buttons_)
        require(button.action != Recruit && button.action != Buy && button.action != Battalion && button.action != CustomConfirm, "Smoke: economic controls appeared in a combat context");
    };
    auto key = [&](sf::Keyboard::Key code) {sf::Event::KeyPressed event{};event.code=code;OnEvent(sf::Event(event)); };
    auto validateCameraController = [&] {
      const Camera originalCamera = camera_;
      const int originalSelection = selected_;
      const auto originalCell = selectedCell_, originalHover = hovered_;
      const auto originalAbility = ability_;
      const auto originalContext = tactical_;
      const int originalSlot = itemSlot_;
      const Match logicalBefore = *match_;
      const auto restore = [&] {camera_=originalCamera;selected_=originalSelection;selectedCell_=originalCell;hovered_=originalHover;SetContext(originalContext);ability_=originalAbility;itemSlot_=originalSlot; };
      try {
        std::vector<int> units;
        for (const auto &unit : match_->state.units)
          if (unit.owner == 0 && unit.Alive())
            units.push_back(unit.id);
        require(units.size() == 5, "Smoke: camera selection fixture needs five initial units");
        int selections = 0;
        for (int zoom = 1; zoom <= 3; ++zoom) {
          require(camera_.SetZoom(zoom, {world_.getSize().x / 2.f, world_.getSize().y / 2.f}), "Smoke: integer camera zoom rejected");
          for (int pan = 0; pan < 2; ++pan)
            for (int id : units) {
              const auto *unit = match_->Find(id);
              const auto *cell = match_->At(unit->pos);
              camera_.CenterOn(unit->pos, static_cast<float>(cell->height), {world_.getSize().x / 2.f, world_.getSize().y / 2.f});
              if (pan == 1)
                camera_.PanBy({7, -5});
              require(BoardBounds().contains(Center(unit->pos)), "Smoke: camera fixture escaped visible board");
              const auto picked = PickCell(Center(unit->pos));
              require(picked && *picked == unit->pos, "Smoke: wrong surface selected after pan/zoom");
              selected_ = -1;
              SetContext(TacticalContext::Selection);
              HandleCell(*picked);
              require(selected_ == id && selectedCell_ == unit->pos, "Smoke: visual picking did not reach unit controller");
              ++selections;
            }
        }
        require(match_->state.round == logicalBefore.state.round && match_->state.activeUnit == logicalBefore.state.activeUnit && match_->state.currentPlayer == logicalBefore.state.currentPlayer && match_->state.units.size() == logicalBefore.state.units.size(), "Smoke: selection advanced tactical state");
        for (const auto &unit : match_->state.units) {
          const auto *old = logicalBefore.Find(unit.id);
          require(old && old->pos == unit.pos && old->hp == unit.hp && old->mp == unit.mp && old->activated == unit.activated, "Smoke: selection changed a unit");
        }
        std::cout << "Plaguefront camera runtime: " << selections << " visible surface-to-controller selections passed (five units, zooms 1..3, centered/panned).\n";
      } catch (...) {
        restore();
        throw;
      }
      restore();
    };
    auto validateSettingsRoundTrip = [&] {
      const std::string originalPath = settingsFile_;
      const std::filesystem::path fixture = "build/test-output/phase3-preferences-smoke.json";
      const int master = volume_, music = musicVolume_, sounds = soundVolume_;
      const bool mute = muted_, grid = grid_, effects = effects_;
      const float speed = aiDelay_;
      const bool wasFullscreen = ctx.window.IsFullscreen();
      const auto oldScreen = screen_, oldPrevious = previous_;
      const auto oldWindow = ctx.window.WindowedSize(), oldResolution = ctx.window.Resolution(), oldFullscreenResolution = ctx.window.FullscreenResolution();
      const float actualMaster = audio_.MasterVolume(), actualMusic = audio_.MusicVolume(), actualSounds = audio_.EffectsVolume();
      const auto restore = [&] {
        settingsFile_ = originalPath;
        volume_ = master;
        musicVolume_ = music;
        soundVolume_ = sounds;
        muted_ = mute;
        grid_ = grid;
        effects_ = effects;
        aiDelay_ = speed;
        audio_.SetVolumes(actualMaster, actualMusic, actualSounds, true);
        ctx.window.SetFullscreen(false);
        ctx.window.SetResolution(oldWindow);
        ctx.window.SetFullscreenResolution(oldFullscreenResolution);
        ctx.window.SetFullscreen(wasFullscreen);
        if (wasFullscreen)
          ctx.window.SetResolution(oldResolution);
        applyDisplay();
        if (resolutionOpen_)
          CloseResolutionSelect(false);
        ChangeScreen(oldScreen);
        previous_ = oldPrevious;
        std::error_code error;
        std::filesystem::remove(fixture, error);
      };
      try {
        settingsFile_ = fixture.string();
        volume_ = 20;
        musicVolume_ = 40;
        soundVolume_ = 60;
        muted_ = false;
        audio_.SetVolumes(20, 40, 60, true);
        Dispatch({{}, "", Mute});
        Dispatch({{}, "", Volume});
        Dispatch({{}, "", MusicVolume});
        Dispatch({{}, "", SoundVolume});
        ctx.window.SetFullscreen(false);
        ctx.window.SetResolution({1280, 720});
        Dispatch({{}, "", Fullscreen});
        SaveSettings();
        applyDisplay();
        const auto savedFullscreen = ctx.window.Resolution();
        require(ctx.window.IsFullscreen() && ctx.window.OutputSize() == savedFullscreen, "Smoke: fullscreen did not reach actual window");
        require(std::filesystem::exists(fixture) && std::filesystem::file_size(fixture) > 0, "Smoke: isolated settings file missing");
        volume_ = 1;
        musicVolume_ = 2;
        soundVolume_ = 3;
        muted_ = false;
        grid_ = !grid;
        effects_ = !effects;
        aiDelay_ = 0.44f;
        ctx.window.SetFullscreen(false);
        ctx.window.SetResolution({1440, 900});
        applyDisplay();
        LoadSettings();
        applyDisplay();
        require(volume_ == 40 && musicVolume_ == 60 && soundVolume_ == 80 && muted_, "Smoke: volume/mute did not round trip");
        require(grid_ == grid && effects_ == effects && std::abs(aiDelay_ - speed) < 0.0001f, "Smoke: unrelated settings changed during round trip");
        require(audio_.MasterVolume() == 40 && audio_.MusicVolume() == 60 && audio_.EffectsVolume() == 80 && audio_.Muted(), "Smoke: preferences did not reach active audio buses");
        require(ctx.window.IsFullscreen() && ctx.window.WindowedSize() == sf::Vector2u{1280, 720} && ctx.window.Resolution() == savedFullscreen && ctx.window.OutputSize() == savedFullscreen, "Smoke: fullscreen/resolutions did not round trip");
        const auto modes = ctx.window.Resolutions();
        const auto different = std::ranges::find_if(modes, [&](sf::Vector2u size) { return size != savedFullscreen; });
        const auto remembered = different == modes.end() ? savedFullscreen : *different;
        Dispatch({{}, "", Fullscreen});
        applyDisplay();
        require(ctx.window.OutputSize() == sf::Vector2u{1280, 720}, "Smoke: leaving fullscreen did not restore saved window size");
        // Comprueba que cargar preferencias de inicio en ventana restaura también la resolución independiente de pantalla completa.
        ctx.window.SetFullscreenResolution(remembered);
        SaveSettings();
        ctx.window.SetFullscreenResolution(savedFullscreen);
        ctx.window.SetResolution({1440, 900});
        applyDisplay();
        LoadSettings();
        applyDisplay();
        require(!ctx.window.IsFullscreen() && ctx.window.OutputSize() == sf::Vector2u{1280, 720} && ctx.window.FullscreenResolution() == remembered,
                "Smoke: windowed preferences discarded the separately selected fullscreen resolution");
        require(audio_.Muted() && volume_ == 40 && musicVolume_ == 60 && soundVolume_ == 80, "Smoke: windowed graphics reload altered audio preferences");
        // El select recorre candidatos sin modificar la salida y aplica sólo la opción confirmada.
        const int selectChecks = checks;
        ChangeScreen(Screen::Settings);
        for (const bool fullscreen : {false, true}) {
          ctx.window.SetFullscreen(fullscreen);
          applyDisplay();
          const auto before = ctx.window.Resolution();
          const auto revision = ctx.window.Revision();
          const auto click = [&](int action, int value = 0) {
            RebuildControls();
            const auto found = std::ranges::find_if(buttons_, [&](const Button &button) { return button.action == action && button.value == value && button.enabled; });
            require(found != buttons_.end(), "Smoke: select control missing");
            const auto pixel = ctx.renderer.LogicalToPixel(found->bounds.position + found->bounds.size / 2.f);
            OnEvent(sf::Event::MouseButtonPressed{sf::Mouse::Button::Left, pixel});
          };
          std::filesystem::remove(fixture);
          click(Resolution);
          require(resolutionOpen_ && !resolutionOptions_.empty() && ctx.window.Resolution() == before && ctx.window.Revision() == revision,
                  "Smoke: opening resolution select changed display");
          require(resolutionFullscreen_ == fullscreen && std::ranges::all_of(buttons_, [](const Button &button) {
                    return button.action == Resolution || button.action == ResolutionSelect || button.action == ResolutionScroll;
                  }), "Smoke: resolution select leaked underlying settings controls");
          if (fullscreen)
            for (const auto size : resolutionOptions_)
              require(std::ranges::find(ctx.window.Resolutions(), size) != ctx.window.Resolutions().end(), "Smoke: select offered unsupported fullscreen mode");
          key(sf::Keyboard::Key::End);
          require(resolutionOption_ == static_cast<int>(resolutionOptions_.size()) - 1 && resolutionScroll_ + ResolutionVisibleRows >= static_cast<int>(resolutionOptions_.size()),
                  "Smoke: select did not scroll focused last option into view");
          key(sf::Keyboard::Key::Home);
          OnEvent(sf::Event::MouseWheelScrolled{sf::Mouse::Wheel::Vertical, -1.f, {0, 0}});
          require(resolutionOption_ == std::min(1, static_cast<int>(resolutionOptions_.size()) - 1) && ctx.window.Resolution() == before,
                  "Smoke: select wheel changed display before confirmation");
          key(sf::Keyboard::Key::Escape);
          require(!resolutionOpen_ && screen_ == Screen::Settings && ctx.window.Resolution() == before && !std::filesystem::exists(fixture),
                  "Smoke: Escape applied or left resolution settings");
          require(keyboardFocus_ >= 0 && buttons_[keyboardFocus_].action == Resolution, "Smoke: cancelled select lost keyboard focus");
          click(Resolution);
          OnEvent(sf::Event::Resized{ctx.window.OutputSize()});
          require(!resolutionOpen_ && screen_ == Screen::Settings && ctx.window.Resolution() == before && !std::filesystem::exists(fixture),
                  "Smoke: resizing retained a stale resolution list or committed a candidate");
          const auto volumeBefore = volume_;
          RebuildControls();
          const auto volumeButton = std::ranges::find_if(buttons_, [](const Button &button) { return button.action == Volume; });
          require(volumeButton != buttons_.end(), "Smoke: volume control missing for outside click");
          const auto outside = ctx.renderer.LogicalToPixel(volumeButton->bounds.position + volumeButton->bounds.size / 2.f);
          click(Resolution);
          OnEvent(sf::Event::MouseButtonPressed{sf::Mouse::Button::Left, outside});
          require(!resolutionOpen_ && volume_ == volumeBefore && !std::filesystem::exists(fixture), "Smoke: outside select click activated underlying control");
          click(Resolution);
          const auto options = resolutionOptions_;
          int choice = 0;
          if (fullscreen) {
            // El runner gráfico oculto puede rechazar cambios físicos de modo: confirma el nativo real.
            const auto native = std::ranges::find(options, ctx.window.OutputSize());
            require(native != options.end(), "Smoke: native fullscreen output missing from select");
            choice = static_cast<int>(native - options.begin());
          } else {
            const auto current = std::ranges::find(options, before);
            const int currentIndex = current == options.end() ? -1 : static_cast<int>(current - options.begin());
            const int gap = options.size() > 2 ? 2 : 1;
            for (std::size_t candidate = 0; candidate < options.size(); ++candidate)
              if (options[candidate] != before && std::abs(static_cast<int>(candidate) - currentIndex) >= gap) {
                choice = static_cast<int>(candidate);
                break;
              }
            require(options.size() == 1 || options[choice] != before, "Smoke: windowed select fixture must change actual resolution");
          }
          MoveResolutionSelect(choice - resolutionOption_);
          if (fullscreen) {
            FocusGamepad({0, 1});
            FocusGamepad({0, -1});
            require(buttons_[keyboardFocus_].action == ResolutionSelect && buttons_[keyboardFocus_].value == resolutionOption_, "Smoke: controller select focus escaped options");
            MoveResolutionSelect(choice - resolutionOption_);
            key(sf::Keyboard::Key::Enter);
          } else
            click(ResolutionSelect, choice);
          require(!resolutionOpen_ && ctx.window.Resolution() == options[choice], "Smoke: selecting resolution did not apply exact chosen option");
          applyDisplay();
          require(ctx.window.OutputSize() == options[choice], "Smoke: chosen select resolution did not reach actual output");
          const auto document = ctx.save.ReadDocument(settingsFile_);
          const auto widthKey = fullscreen ? "fullscreen_width" : "window_width", heightKey = fullscreen ? "fullscreen_height" : "window_height";
          require(document && document->at(widthKey) == options[choice].x && document->at(heightKey) == options[choice].y,
                  "Smoke: select did not persist exact chosen resolution");
          click(Resolution);
          ChangeScreen(Screen::Controls);
          require(!resolutionOpen_, "Smoke: changing settings screen retained select popup");
          ChangeScreen(Screen::Settings);
        }
        std::cout << "Plaguefront resolution select runtime: " << checks - selectChecks << " checks passed; window/fullscreen lists, direct non-adjacent windowed selection, native fullscreen Enter confirmation, scrolling/controller focus, cancellation, outside click and exact saved mode (no physical fullscreen mode change required by hidden runner).\n";
        std::cout << "Plaguefront preferences runtime: isolated audio/fullscreen/windowed round trips passed; fullscreen choice " << remembered.x << 'x' << remembered.y << " survived a windowed start.\n";
      } catch (...) {
        restore();
        throw;
      }
      restore();
      require(!std::filesystem::exists(fixture), "Smoke: isolated preferences fixture not removed");
    };
    auto measureNativeSubmission = [&] {
      constexpr int samples = 8;
      RenderWorld();
      double total = 0, minimum = 1e30, maximum = 0;
      for (int sample = 0; sample < samples; ++sample) {
        const auto start = std::chrono::steady_clock::now();
        RenderWorld();
        const double elapsed = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - start).count();
        total += elapsed;
        minimum = std::min(minimum, elapsed);
        maximum = std::max(maximum, elapsed);
      }
      std::cout << "Plaguefront RenderWorld CPU submission: " << samples << " samples, average=" << total / samples << " ms, min=" << minimum << " ms, max=" << maximum << " ms (" << world_.getSize().x << 'x' << world_.getSize().y << " native; excludes window presentation/vsync).\n";
    };
    auto validateContextFlow = [&] {
      for (const auto mode : {Mode::VersusAI, Mode::Local}) {
        const int victim = prepareCombat(UnitClass::Saber, {0, 4}, {1, 4});
        match_->state.mode = mode;
        mode_ = mode;
        const int actor = selected_, hp = match_->Find(victim)->hp;
        require(match_->state.activeUnit == -1 && !match_->Find(actor)->activated, "Smoke: selection consumed activation");
        assertNoEconomy();
        const auto pan = camera_.Pan();
        key(sf::Keyboard::Key::A);
        require(tactical_ == TacticalContext::Orders && !ability_ && camera_.Pan().x == pan.x + 12.f && camera_.Pan().y == pan.y, "Smoke: A must pan the camera without opening an attack context");
        key(sf::Keyboard::Key::H);
        require(tactical_ == TacticalContext::Abilities, "Smoke: H did not open abilities");
        key(sf::Keyboard::Key::Escape);
        require(tactical_ == TacticalContext::Orders, "Smoke: abilities shortcut cancellation lost unit orders");
        key(sf::Keyboard::Key::T);
        require(tactical_ == TacticalContext::Target && ability_ == Ability::Attack, "Smoke: T did not open attack targeting");
        assertNoEconomy();
        const auto actorBefore = *match_->Find(actor);
        HandleCell(actorBefore.pos); // Una unidad aliada no es un objetivo válido de ataque.
        require(match_->state.activeUnit == -1 && !match_->Find(actor)->activated && !match_->Find(actor)->acted &&
                    match_->Find(actor)->hp == actorBefore.hp && match_->Find(actor)->mp == actorBefore.mp && match_->Find(victim)->hp == hp,
                "Smoke: invalid target committed BeginActivation or consumed action/resources");
        require(tactical_ == TacticalContext::Target && ability_ == Ability::Attack, "Smoke: invalid target lost the pending command context");
        key(sf::Keyboard::Key::Escape);
        require(tactical_ == TacticalContext::Orders && !ability_, "Smoke: Escape did not return target selection to orders");
        require(match_->state.activeUnit == -1 && !match_->Find(actor)->activated && match_->Find(victim)->hp == hp, "Smoke: opening/canceling targeting changed tactical state");
        const auto distant = std::ranges::find_if(match_->state.units, [&](const Unit &unit) { return unit.owner == 1 && Distance(unit.pos, actorBefore.pos) > 1; });
        require(distant != match_->state.units.end(), "Smoke: distant enemy fixture missing");
        const int distantId = distant->id, distantHp = distant->hp;
        HandleCell(distant->pos);
        require(match_->state.activeUnit == -1 && !match_->Find(actor)->activated && match_->Find(distantId)->hp == distantHp && tactical_ == TacticalContext::Orders,
                "Smoke: invalid direct attack consumed an activation before checking range");
        openProperty(1);
        key(sf::Keyboard::Key::Escape);
        require(tactical_ == TacticalContext::Building, "Smoke: recruitment did not cancel to property");
        key(sf::Keyboard::Key::Escape);
        require(tactical_ == TacticalContext::Selection, "Smoke: property did not cancel to board");
        selectUnit(actor);
        assertNoEconomy();
        Dispatch({{}, "", AbilitySelect, static_cast<int>(Ability::Attack)});
        HandleCell(match_->Find(victim)->pos);
        require(match_->Find(victim)->hp < hp, "Smoke: attack failed after economic context cancellation");
        visuals_.Update(3.f);
        selectUnit(actor);
        buttons_.clear();
        RenderPanel();
        const auto inventory = std::ranges::find_if(buttons_, [](const Button &b) { return b.action == OpenInventory && b.enabled; });
        require(inventory != buttons_.end(), "Smoke: inventory navigation button missing");
        const int focus = static_cast<int>(inventory - buttons_.begin());
        for (std::size_t i = 0; i <= buttons_.size() && keyboardFocus_ != focus; ++i)
          key(sf::Keyboard::Key::Down);
        require(keyboardFocus_ == focus, "Smoke: keyboard could not focus inventory");
        key(sf::Keyboard::Key::Enter);
        require(tactical_ == TacticalContext::Inventory, "Smoke: Enter did not open focused inventory");
        key(sf::Keyboard::Key::Escape);
        require(tactical_ == TacticalContext::Orders, "Smoke: keyboard cancellation lost unit context");
      }
      // Prevé maná y habilidades contra fuentes sin modificar la partida al abrir o cancelar la selección.
      const int target = prepareCombat(UnitClass::Caster, {0, 4}, {2, 4});
      const int caster = selected_;
      match_->Find(caster)->mp = 1;
      ResetPresentation();
      selectUnit(caster);
      Dispatch({{}, "", AbilitySelect, static_cast<int>(Ability::Fire)});
      const auto &spellPreview = PreviewRules();
      require(tactical_ == TacticalContext::Target && spellPreview.state.activeUnit == caster, "Smoke: uncommitted spell target preview has no isolated activation");
      require(spellPreview.Find(caster)->mp == std::min(match_->config.maxMana, 1 + match_->config.manaRecovery) && spellPreview.CanTarget(caster, target, Ability::Fire),
              "Smoke: Fire target preview ignored activation mana recovery");
      require(match_->state.activeUnit == -1 && match_->Find(caster)->mp == 1 && !match_->Find(caster)->activated, "Smoke: spell target prediction changed real activation or mana");
      CancelContext();
      require(match_->state.activeUnit == -1 && match_->Find(caster)->mp == 1, "Smoke: canceled recovered-mana spell preview changed the match");
      match_->Find(caster)->mp = 3;
      const Hex source{1, 4};
      match_->state.sources.push_back({match_->state.nextId++, source, 5});
      ResetPresentation();
      selectUnit(caster);
      Dispatch({{}, "", AbilitySelect, static_cast<int>(Ability::Purify)});
      const auto &sourcePreview = PreviewRules();
      Match candidate = sourcePreview;
      require(tactical_ == TacticalContext::Target && sourcePreview.state.activeUnit == caster && sourcePreview.Find(caster)->mp == std::min(match_->config.maxMana, 3 + match_->config.manaRecovery),
              "Smoke: source target preview did not predict legal caster activation");
      require(candidate.UseAbility(caster, Ability::Purify, source).ok && candidate.state.sources.empty(), "Smoke: Purify source marker preview was not a legal activated command");
      require(match_->state.activeUnit == -1 && match_->Find(caster)->mp == 3 && !match_->Find(caster)->activated && match_->state.sources.size() == 1 && match_->state.sources.front().pos == source && match_->state.sources.front().remaining == 5,
              "Smoke: source target prediction consumed real mana, activation or contamination");
      CancelContext();
      require(tactical_ == TacticalContext::Orders && match_->state.activeUnit == -1 && match_->Find(caster)->mp == 3 && match_->state.sources.size() == 1,
              "Smoke: canceled Purify prediction changed the source or selected-unit context");
      std::cout << "Plaguefront contexts runtime: H/T shortcuts, A camera pan, invalid-target protection, isolated activation/mana/source predictions, cancellation, subsequent combat, separated economy and keyboard focus passed.\n";
    };
    auto prepareRoute = [&] {
      newLocal();
      const int id = match_->state.units.front().id;
      selectUnit(id);
      Dispatch({{}, "", OpenMove});
      require(tactical_ == TacticalContext::Move, "Smoke: movement context missing");
      UpdatePreview({});
      std::optional<Hex> destination;
      std::size_t steps = 0;
      for (const auto &[hex, cost] : reachable_)
        if (auto path = match_->FindPath(id, hex); path && path->cells.size() > steps && cost > 1) {
          steps = path->cells.size();
          destination = hex;
        }
      require(destination && steps >= 3, "Smoke: no multi-step route for arrows fixture");
      hovered_ = destination;
      UpdatePreview(destination);
      require(previewPath_ && previewPath_->cells.size() >= 3, "Smoke: route preview missing");
      FocusCamera(false);
      return *destination;
    };
    auto validateRoute = [&] {
      const Hex destination = prepareRoute();
      const int id = selected_;
      const Match before = *match_;
      const auto expected = match_->FindPath(id, destination);
      require(expected && previewPath_ && previewPath_->cells == expected->cells && previewPath_->cost == expected->cost, "Smoke: displayed route disagreed with weighted rule path");
      int sum = 0;
      for (std::size_t i = 1; i < expected->cells.size(); ++i)
        sum += match_->MoveCost(*match_->Find(id), expected->cells[i - 1], expected->cells[i]);
      require(sum == previewPath_->cost, "Smoke: route cost omitted terrain/elevation costs");
      const auto *cached = previewPath_->cells.data();
      UpdatePreview(destination);
      require(previewPath_->cells.data() == cached && previewRevision_ == matchRevision_, "Smoke: unchanged route did not retain cache");
      UpdatePreview(Hex{999, 999});
      require(!previewPath_, "Smoke: unreachable target displayed a route");
      UpdatePreview(match_->state.units[1].pos);
      require(!previewPath_, "Smoke: occupied target displayed a route");
      UpdatePreview(destination);
      CancelContext();
      require(!previewPath_ && tactical_ == TacticalContext::Orders, "Smoke: canceled route retained preview");
      require(match_->Find(id)->pos == before.Find(id)->pos && match_->state.activeUnit == before.state.activeUnit && !match_->Find(id)->activated, "Smoke: preview/cancel consumed movement or activation");
      Dispatch({{}, "", OpenMove});
      UpdatePreview(destination);
      HandleCell(destination);
      require(match_->Find(id)->pos == destination && match_->Find(id)->moved && tactical_ == TacticalContext::Orders, "Smoke: route confirmation did not move and restore orders");
      require(visuals_.VisualFor(*match_->Find(id)).path == expected->cells, "Smoke: animated route disagreed with preview");
      visuals_.Update(5.f);
      require(!previewPath_, "Smoke: committed route retained preview");
      std::cout << "Plaguefront route runtime: weighted cost, cache, unreachable/occupied targets, cancellation, immutable preview and identical animated path passed.\n";
    };

    if (step == 0) {
      smoke_ = true;
      audio_.SetVolumes(static_cast<float>(volume_), static_cast<float>(musicVolume_), static_cast<float>(soundVolume_), true);
      setOutput({1440, 900});
      ChangeScreen(Screen::Menu);
    } else if (step == 1)
      ChangeScreen(Screen::Setup);
    else if (step == 2) {
      newLocal();
      validateCameraController();
      validateSettingsRoundTrip();
      measureNativeSubmission();
    } else if (step == 3)
      selectUnit(match_->state.units.front().id);
    else if (step == 4) {
      Dispatch({{}, "", OpenMove});
      UpdatePreview({});
      const auto start = match_->Find(selected_)->pos;
      auto destination = std::ranges::find_if(reachable_, [&](const auto &entry) { return entry.first != start && entry.second > 1; });
      require(destination != reachable_.end(), "Smoke: no initial movement");
      const Hex hex = destination->first;
      FocusCamera(false);
      const auto picked = PickCell(Center(hex));
      require(picked && *picked == hex, "Smoke: wrong movement surface picked");
      UpdatePreview(hex);
      HandleCell(*picked);
      require(match_->Find(selected_)->pos == hex && match_->Find(selected_)->moved, "Smoke: UI movement failed");
      visuals_.Update(0.10f);
    } else if (step == 5) {
      const int victim = prepareCombat(UnitClass::Saber, {0, 4}, {1, 4});
      const int hp = match_->Find(victim)->hp;
      Dispatch({{}, "", AbilitySelect, static_cast<int>(Ability::Attack)});
      HandleCell(match_->Find(victim)->pos);
      require(match_->Find(victim)->hp < hp, "Smoke: melee command failed");
      visuals_.Update(0.20f);
      message_ = "Ataque real: anticipación, contacto de espada y reacción del defensor.";
    } else if (step == 6) {
      const int victim = prepareCombat(UnitClass::Caster, {0, 4}, {2, 4});
      const int hp = match_->Find(victim)->hp;
      Dispatch({{}, "", AbilitySelect, static_cast<int>(Ability::Fire)});
      HandleCell(match_->Find(victim)->pos);
      require(match_->Find(victim)->hp < hp, "Smoke: Fire command failed");
      visuals_.Update(0.25f);
      message_ = "Fire real: consumo de maná, lanzamiento e impacto de fuego.";
    } else if (step == 7) {
      newLocal();
      std::vector<int> victims;
      for (const auto &u : match_->state.units)
        if (u.owner == 0 && victims.size() < 3)
          victims.push_back(u.id);
      for (int id : victims)
        match_->Damage(id, 1000);
      for (int i = 0; i < 5; ++i)
        match_->EndRound();
      SyncPresentation();
      visuals_.Update(5.f);
      require(match_->state.sources.size() == 3 && match_->state.globalRemaining > 0, "Smoke: rot not active");
      hovered_ = match_->state.sources.front().pos;
      FocusCamera(true);
      message_ = "Tres fuentes conectadas: daño local 2 HP; daño global 1 HP fuera del radio local.";
    } else if (step == 8) {
      newLocal();
      auto house = std::ranges::find_if(match_->state.cells, [](const Cell &c) { return c.building == Building::House && c.owner == -1; });
      require(house != match_->state.cells.end(), "Smoke: neutral house missing");
      auto &actor = match_->state.units.front();
      actor.pos = house->pos;
      const int id = actor.id;
      ResetPresentation();
      selectUnit(id);
      FocusCamera(false);
      require(EnsureActive(), "Smoke: house activator failed");
      Dispatch({{}, "", Capture});
      require(match_->At(match_->Find(id)->pos)->captureProgress > 0 && match_->At(match_->Find(id)->pos)->owner == -1, "Smoke: house capture progress failed");
      visuals_.Update(0.15f);
      message_ = "Captura parcial: la casa conserva propietario hasta completar sus puntos.";
    } else if (step == 9) {
      SmokePhase4(16);
    } else if (step == 10) {
      previous_ = Screen::Menu;
      ChangeScreen(Screen::Settings);
      Dispatch({{}, "", Resolution});
    } else if (step == 11) {
      newLocal();
      selected_ = match_->state.units.front().id;
      require(EnsureActive(), "Smoke: local activation failed");
      SelectAbility(Ability::Guard);
      require(match_->Find(selected_)->guard, "Smoke: Guard failed");
      visuals_.Update(2.f);
      FinishTurn();
      require(screen_ == Screen::Playing && match_->state.currentPlayer == 0, "Smoke: activation changed army");
      FinishArmyTurn(true);
      require(screen_ == Screen::Handoff, "Smoke: handoff missing");
      Dispatch({{}, "", Ready});
      Dispatch({{}, "", Settings});
      OnCloseRequest();
      OnCloseRequest();
      Dispatch({{}, "", CancelExit});
      require(screen_ == Screen::Settings, "Smoke: nested exit return failed");
      Dispatch({{}, "", Back});
      require(screen_ == Screen::Playing, "Smoke: settings return failed");
      const std::string scenario = match_->state.scenario;
      const auto mode = match_->state.mode;
      scenario_ = 1;
      mode_ = Mode::VersusAI;
      Dispatch({{}, "", Restart});
      require(match_->state.scenario == scenario && match_->state.mode == mode, "Smoke: restart used setup draft");
      openProperty(1);
      const auto count = match_->state.units.size();
      Dispatch({{}, "", Battalion, 3});
      Dispatch({{}, "", CustomAdd, 1});
      Dispatch({{}, "", CustomConfirm});
      require(tactical_ == TacticalContext::Purchase && match_->state.units.size() == count, "Smoke: custom recruitment skipped confirmation");
      Dispatch({{}, "", ConfirmPurchase});
      require(match_->state.units.size() == count + 1 && match_->state.units.back().readyRound == 2, "Smoke: custom recruitment failed");
      visuals_.Update(3.f);
      selectUnit(match_->state.units.front().id);
      require(EnsureActive(), "Smoke: final activation failed");
      FinishTurn();
      FinishArmyTurn(true);
      require(screen_ == Screen::Handoff, "Smoke: final handoff missing");
    } else if (step == 12) {
      validateContextFlow();
      validateRoute();
      newLocal();
      auto &actor = match_->state.units.front();
      actor.pos = {0, 5};
      const int id = actor.id;
      ResetPresentation();
      selectUnit(id);
      Dispatch({{}, "", OpenMove});
      hovered_ = Hex{1, 7};
      UpdatePreview(hovered_);
      require(previewPath_ && previewPath_->cells.size() >= 4, "Smoke: terminal arrow fixture needs a multi-cell route");
      FocusCamera(false);
      const Point center{world_.getSize().x / 2.f, world_.getSize().y / 2.f};
      camera_.SetZoom(2, center);
      camera_.CenterOn({0, 6}, 0, center);
      message_ = "Ruta legal prevista: una sola punta en el destino, coste ponderado y MOV restante. Cancelar no consume la activación.";
    } else if (step == 13) {
      newLocal();
      selectUnit(match_->state.units.front().id);
      FocusCamera(false);
      assertNoEconomy();
    } else if (step == 14) {
      newLocal();
      openProperty(1);
      buttons_.clear();
      RenderPanel();
      require(std::ranges::any_of(buttons_, [](const Button &b) { return b.action == Recruit && b.enabled; }), "Smoke: legal recruitment unavailable");
      const auto count = match_->state.units.size();
      const int coins = match_->state.players[0].coins;
      Dispatch({{}, "", Recruit, static_cast<int>(UnitClass::Archer)});
      require(tactical_ == TacticalContext::Purchase && match_->state.units.size() == count && match_->state.players[0].coins == coins, "Smoke: recruit selection spent coins");
      CancelContext();
      require(tactical_ == TacticalContext::Recruitment && match_->state.units.size() == count, "Smoke: canceled recruitment changed army");
    } else if (step == 15) {
      const int victim = prepareCombat(UnitClass::Caster, {0, 4}, {2, 4});
      Dispatch({{}, "", OpenAbilities});
      Dispatch({{}, "", AbilitySelect, static_cast<int>(Ability::Fire)});
      hovered_ = match_->Find(victim)->pos;
      require(tactical_ == TacticalContext::Target && match_->state.activeUnit == -1, "Smoke: targeting consumed activation");
      assertNoEconomy();
    } else if (step == 16) {
      newLocal(2);
      require(match_->state.cells.size() == 188, "Smoke: large scenario did not load explicit 188 cells");
      FocusCamera(true);
      hovered_.reset();
      message_ = "Los tres pasos · 188 celdas: tres cruces del río, aldeas neutrales y flancos por bosques y ruinas.";
    } else if (step == 17) {
      newLocal();
      selectUnit(match_->state.units.front().id);
      match_->Find(selected_)->inventory.clear();
      openProperty(2);
      Dispatch({{}, "", ShopUnit, match_->state.units.front().id});
      const int id = selected_, coins = match_->state.players[0].coins;
      Dispatch({{}, "", Buy, static_cast<int>(Item::Potion)});
      require(tactical_ == TacticalContext::Purchase && match_->Find(id)->inventory.empty() && match_->state.players[0].coins == coins, "Smoke: shop spent coins before confirmation");
      CancelContext();
      require(tactical_ == TacticalContext::Shop && match_->Find(id)->inventory.empty(), "Smoke: canceled shop purchase changed inventory");
      Dispatch({{}, "", Buy, static_cast<int>(Item::Potion)});
      key(sf::Keyboard::Key::Enter);
      require(match_->Find(id)->inventory.size() == 1 && match_->state.players[0].coins == coins - match_->config.itemCosts[0], "Smoke: confirmed item purchase failed");
      visuals_.Update(2.f);
      SetContext(TacticalContext::Shop);
    } else if (step == 18) {
      newLocal();
      auto &actor = match_->state.units.front();
      actor.pos = {-1, 3};
      const int id = actor.id;
      ResetPresentation();
      selectUnit(id);
      FocusCamera(false);
      Dispatch({{}, "", OpenConstruction});
      require(tactical_ == TacticalContext::Construction && CheckAction(ConfirmBuild).ok, "Smoke: legal construction context missing");
      require(match_->state.activeUnit == -1 && !match_->Find(id)->activated, "Smoke: construction menu consumed activation");
    } else if (step == 19) {
      newLocal();
      auto &actor = match_->state.units.front();
      actor.inventory = {Item::Potion, Item::Purifier};
      actor.hp -= 8;
      const int id = actor.id;
      ResetPresentation();
      selectUnit(id);
      FocusCamera(false);
      Dispatch({{}, "", OpenInventory});
      require(tactical_ == TacticalContext::Inventory, "Smoke: inventory context missing");
      assertNoEconomy();
    } else if (step == 20) {
      newLocal(2);
      ctx.window.SetFullscreen(true);
      applyDisplay();
      require(ctx.window.IsFullscreen() && ctx.window.OutputSize() == ctx.window.FullscreenResolution(), "Smoke: fullscreen board output does not match video mode");
      FocusCamera(true);
      message_ = "Pantalla completa real · F11 o Alt+Enter restauran la ventana anterior.";
    } else if (step == 21) {
      setOutput({1920, 1080});
      previous_ = Screen::Playing;
      ChangeScreen(Screen::Settings);
    } else if (step == 22) {
      setOutput({1280, 720});
      scenario_ = 2;
      LoadArmies();
      ChangeScreen(Screen::Setup);
    } else if (step == 23) {
      setOutput({1440, 900});
      newLocal(2);
      selectUnit(match_->state.units.front().id);
      FocusCamera(false);
      require(ctx.renderer.OutputSize() == sf::Vector2u{1440, 900}, "Smoke: final window restore failed");
    } else if (step == 24) {
      prepareCombat(UnitClass::Caster, {0, 4}, {2, 4});
      key(sf::Keyboard::Key::H);
      require(tactical_ == TacticalContext::Abilities && CheckAction(AbilitySelect, static_cast<int>(Ability::Fire)).ok, "Smoke: caster abilities fixture has no legal Fire");
      require(match_->state.activeUnit == -1 && !match_->Find(selected_)->activated, "Smoke: abilities overview consumed activation");
      message_ = "Habilidades individuales: coste de maná, recarga y motivo de bloqueo; Escape vuelve a las órdenes.";
    } else if (step == 25) {
      newLocal();
      openProperty(1);
      const auto count = match_->state.units.size();
      const int coins = match_->state.players[0].coins;
      const std::string price = Name(UnitClass::Saber) + " · " + std::to_string(match_->config.classes[static_cast<int>(UnitClass::Saber)].cost) + " monedas";
      Dispatch({{}, price, Recruit, static_cast<int>(UnitClass::Saber)});
      require(tactical_ == TacticalContext::Purchase && pendingLabel_ == price && match_->state.units.size() == count && match_->state.players[0].coins == coins,
              "Smoke: priced purchase review spent money or lost operation details before confirmation");
      message_ = "Revisa unidad y precio antes de confirmar. Cancelar conserva las monedas y el ejército.";
    } else if (step == 26) {
      require(screen_ == Screen::Playing && tactical_ == TacticalContext::Purchase, "Smoke: pause fixture must start in live purchase review");
      const int coins = match_->state.players[0].coins;
      const auto count = match_->state.units.size();
      Dispatch({{}, "", Pause});
      require(screen_ == Screen::Paused && tactical_ == TacticalContext::Purchase && match_->state.players[0].coins == coins && match_->state.units.size() == count,
              "Smoke: pause lost pending context or executed an economic action");
    } else if (step == 27) {
      newLocal();
      mode_ = Mode::VersusAI;
      match_->state.mode = Mode::VersusAI;
      const Hex fortress = ownFortress();
      if (auto *defender = match_->Occupant(fortress)) {
        const auto free = std::ranges::find_if(match_->state.cells, [&](const Cell &cell) { return cell.terrain == Terrain::Plain && cell.building == Building::None && !match_->Occupant(cell.pos); });
        require(free != match_->state.cells.end(), "Smoke: defeat fixture has no legal defender placement");
        defender->pos = free->pos;
      }
      auto enemy = std::ranges::find_if(match_->state.units, [](const Unit &unit) { return unit.owner == 1 && unit.type == UnitClass::Saber; });
      require(enemy != match_->state.units.end(), "Smoke: defeat fixture has no enemy captor");
      enemy->pos = fortress;
      const int id = enemy->id;
      selected_ = id;
      selectedCell_ = fortress;
      ResetPresentation();
      require(match_->Pass().ok && match_->state.currentPlayer == 1, "Smoke: enemy did not receive legal capture turn");
      require(match_->BeginActivation(id).ok && match_->Capture(id).ok && match_->state.winner == -1, "Smoke: defeat fortress capture did not start legally");
      require(match_->EndActivation().ok, "Smoke: enemy capture activation did not end");
      for (int n = 0; n < 8 && match_->state.winner < 0; ++n) {
        require(match_->EndTurn().ok && match_->EndTurn().ok, "Smoke: defender army response failed");
        require(match_->BeginActivation(id).ok && match_->Capture(id).ok, "Smoke: continued fortress capture failed");
        if (match_->state.winner < 0)
          require(match_->EndActivation().ok, "Smoke: partial capture end failed");
      }
      require(match_->state.winner == 1 && match_->At(fortress)->owner == 1, "Smoke: enemy did not complete capture points");
      SyncPresentation();
      visuals_.Update(5.f);
      FocusCamera(true);
      ChangeScreen(Screen::Victory);
      require(match_->state.mode == Mode::VersusAI && screen_ == Screen::Victory, "Smoke: legal enemy victory did not open defeat presentation");
      require(!ctx.window.IsFullscreen() && ctx.renderer.OutputSize() == sf::Vector2u{1440, 900}, "Smoke: final presentation did not preserve restored window mode");
      std::cout << "Plaguefront UI runtime: " << checks << " checks passed across 28 real application captures; automated interactions only.\n";
    }
  }
} // Espacio de nombres Plaguefront
