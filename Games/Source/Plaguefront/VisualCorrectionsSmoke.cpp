#include "Plaguefront/Game.h"
#include "Plaguefront/SpellPresentation.h"
#include "Plaguefront/Weather.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <stdexcept>

namespace Plaguefront {
  void Game::SmokeVisualCorrections() {
    using Json = nlohmann::json;
    int checks = 0;
    const auto require = [&](bool value, const std::string &reason) { ++checks; if (!value) throw std::runtime_error("Correcciones visuales: " + reason); };
    const auto folder = std::filesystem::path("build/visual-corrections/runtime");
    std::filesystem::create_directories(folder);
    const auto savedMatch = match_, savedObserved = observed_;
    const auto savedVisuals = visuals_;
    const auto savedCamera = camera_;
    const auto savedScreen = screen_;
    const auto savedContext = tactical_;
    const auto savedCell = selectedCell_, savedHover = hovered_;
    const auto savedMode = mode_;
    const auto savedScenario = scenario_, savedSelected = selected_;
    const auto savedScale = worldPixelScale_;
    const auto savedEffects = effects_, savedGrid = grid_;
    const auto savedTime = animationTime_;
    const auto savedMessage = message_;
    const auto savedOutput = ctx.window.OutputSize();
    auto *savedProps = props_, *savedUnits = units_;
    const auto resize = [&](sf::Vector2u size) {
      ctx.window.SetFullscreen(false); ctx.window.SetResolution(size); ctx.window.ApplyPending();
      ctx.renderer.ConfigureOutput(ctx.window.OutputSize()); ctx.cursor.SetView(ctx.renderer.View());
      RefreshLayout(true); displayRevision_ = ctx.window.Revision();
      require(ctx.renderer.OutputSize() == size, "la resolución real no coincide con la solicitada");
    };
    const auto restore = [&] {
      props_ = savedProps; units_ = savedUnits; match_ = savedMatch; observed_ = savedObserved;
      screen_ = savedScreen; mode_ = savedMode; scenario_ = savedScenario; selected_ = savedSelected;
      selectedCell_ = savedCell; hovered_ = savedHover; effects_ = savedEffects; grid_ = savedGrid;
      animationTime_ = savedTime; message_ = savedMessage; worldPixelScale_ = savedScale;
      resize(savedOutput); camera_ = savedCamera; visuals_ = savedVisuals; SetContext(savedContext);
      ctx.cursor.SetVirtualPosition({});
    };
    const auto imageHash = [](const sf::Image &image) {
      std::uint64_t hash = 1469598103934665603ull;
      const auto bytes = static_cast<std::size_t>(image.getSize().x) * image.getSize().y * 4;
      for (std::size_t i = 0; i < bytes; ++i) { hash ^= image.getPixelsPtr()[i]; hash *= 1099511628211ull; }
      return hash;
    };
    const auto saveImage = [&](const sf::Image &image, const std::string &name) {
      const sf::Texture texture(image);
      require(ctx.screenshot.SaveFrame(texture, image.getSize(), (folder / name).string()), "ScreenshotManager no pudo guardar " + name);
    };
    const auto render = [&] { RenderWorld(); return world_.getTexture().copyToImage(); };
    const auto snapshot = [&] {
      const auto file = folder / "match-inmutabilidad.json";
      std::string error; require(match_->Save(file.string(), error), "fixture lógico inválido: " + error);
      std::ifstream input(file); std::ostringstream bytes; bytes << input.rdbuf(); return bytes.str();
    };
    const auto add = [&](int owner, UnitClass type, Hex pos) {
      Unit unit; unit.id = match_->state.nextId++; unit.owner = owner; unit.type = type; unit.pos = pos;
      unit.hp = match_->config.classes[static_cast<std::size_t>(type)].hp;
      unit.mp = type == UnitClass::Caster ? match_->config.maxMana : 0;
      match_->state.units.push_back(unit); return unit.id;
    };
    const auto fixture = [&] {
      match_ = Match(balance_); match_->state.mode = Mode::Local; match_->config.weatherPeriod = 1000;
      for (int q = -4; q <= 4; ++q) for (int r = -3; r <= 3; ++r) match_->state.cells.push_back({{q, r}});
      match_->At({-4, 0})->building = Building::Fortress; match_->At({-4, 0})->owner = 0;
      match_->At({4, 0})->building = Building::Fortress; match_->At({4, 0})->owner = 1;
      mode_ = Mode::Local; screen_ = Screen::Playing; selected_ = -1; selectedCell_.reset(); hovered_.reset();
      effects_ = false; grid_ = false; animationTime_ = .37f; ResetPresentation(); FocusCamera(true);
      ctx.cursor.SetVirtualPosition(sf::Vector2f{4, 4});
    };
    Json report; report["idle"] = Json::array(); report["rain"] = Json::array(); report["plague"] = Json::array(); report["spells"] = Json::array();
    try {
      resize({1440, 900});
      const auto unitPixels = units_->copyToImage();
      const std::array<Hex, 4> destinations{{{1, 0}, {-1, 1}, {1, -1}, {-1, 0}}};
      for (int owner = 0; owner < 2; ++owner) for (int cls = 0; cls < 7; ++cls) for (int direction = 0; direction < 4; ++direction) {
        fixture(); match_->state.currentPlayer = owner; const int id = add(owner, static_cast<UnitClass>(cls), {});
        require(match_->BeginActivation(id).ok, "la tropa idle no pudo activarse"); visuals_.Reset(*match_); const Match before = *match_;
        require(match_->Move(id, destinations[static_cast<std::size_t>(direction)]).ok, "la orientación idle no sigue un movimiento legal");
        visuals_.Observe(before, *match_); visuals_.Update(AnimationTiming.walkCellSeconds + .0001f);
        const auto *unit = match_->Find(id); const auto origin = visuals_.VisualFor(*unit);
        camera_.SetZoom(1, {}); camera_.CenterOn(unit->pos, 0, {world_.getSize().x / 2.f, world_.getSize().y / 2.f});
        const auto anchor = camera_.Project(unit->pos, 0); const auto committed = snapshot();
        const int x0 = static_cast<int>(std::round(anchor.x)) - 24, y0 = static_cast<int>(std::round(anchor.y)) - 56;
        require(x0 >= 0 && y0 >= 0 && x0 + 48 < static_cast<int>(world_.getSize().x) && y0 + 64 < static_cast<int>(world_.getSize().y), "el sprite temporal quedó fuera del lienzo real");
        sf::Image strip({48 * 8, 64}, sf::Color::Transparent); std::vector<std::uint64_t> hashes; std::set<std::uint64_t> distinct;
        visuals_.Update(AnimationTiming.idleFrameSeconds * .5f);
        for (int sample = 0; sample < 16; ++sample) {
          if (sample) visuals_.Update(AnimationTiming.idleFrameSeconds);
          const auto visual = visuals_.VisualFor(*unit);
          require(visual.state == AnimState::Idle && visual.frame == sample % 8 && visual.facing == direction, "el motor no reproduce Idle en el orden temporal configurado");
          require(visual.q == origin.q && visual.r == origin.r && visual.elevation == origin.elevation, "Idle cambió el anclaje lógico");
          const auto pixels = render(); sf::Image crop({48, 64}, sf::Color::Transparent);
          const int row = cls + 7 * owner + 14 * direction;
          for (unsigned y = 0; y < 64; ++y) for (unsigned x = 0; x < 48; ++x) {
            const auto actual = pixels.getPixel({static_cast<unsigned>(x0) + x, static_cast<unsigned>(y0) + y}); crop.setPixel({x, y}, actual);
            const auto expected = unitPixels.getPixel({static_cast<unsigned>(visual.frame * 48) + x, static_cast<unsigned>(row * 64) + y});
            if (expected.a == 255) require(actual == expected, "el fotograma dibujado no coincide con el rectángulo real del atlas");
            if (sample < 8) strip.setPixel({static_cast<unsigned>(sample * 48) + x, y}, actual);
          }
          const auto hash = imageHash(crop); hashes.push_back(hash); distinct.insert(hash);
          if (sample >= 8) require(hash == hashes[static_cast<std::size_t>(sample - 8)], "la vuelta de Idle cambia de escala, recorte o posición");
        }
        require(distinct.size() > 1, "Idle mantiene una imagen completamente estática durante dos ciclos");
        require(!visuals_.Busy() && snapshot() == committed, "reproducir Idle bloquea el turno o modifica la partida");
        const auto name = "idle-" + std::to_string(cls) + "-" + std::to_string(owner) + "-" + std::to_string(direction) + ".png";
        saveImage(strip, name);
        report["idle"].push_back({{"class", cls}, {"owner", owner}, {"direction", direction}, {"frames", 16}, {"seconds_per_frame", AnimationTiming.idleFrameSeconds}, {"distinct_rendered_frames", distinct.size()}, {"loop_hashes", hashes}, {"strip", name}});
      }

      for (const auto size : {sf::Vector2u{1280, 720}, sf::Vector2u{1440, 900}, sf::Vector2u{1920, 1080}, sf::Vector2u{1100, 1100}}) {
        resize(size); fixture(); add(0, UnitClass::Saber, {}); visuals_.Reset(*match_); effects_ = true;
        for (int zoom : {1, 3}) {
          camera_.SetZoom(zoom, {world_.getSize().x / 2.f, world_.getSize().y / 2.f}); camera_.PanBy({37.f, -29.f});
          for (Weather weather : {Weather::Rain, Weather::Storm}) {
            match_->state.weather = weather; const auto committed = snapshot(); std::vector<std::uint64_t> phases;
            for (int phase = 0; phase < 3; ++phase) {
              animationTime_ = .31f + .08f * phase; match_->state.weather = Weather::Clear; const auto clear = render();
              match_->state.weather = weather; const auto rainy = render(); phases.push_back(imageHash(rainy));
              std::array<int, 16> regions{}; int changed = 0;
              for (unsigned y = 0; y < rainy.getSize().y; ++y) for (unsigned x = 0; x < rainy.getSize().x; ++x) if (rainy.getPixel({x, y}) != clear.getPixel({x, y})) {
                ++changed; ++regions[static_cast<std::size_t>((y * 4 / rainy.getSize().y) * 4 + x * 4 / rainy.getSize().x)];
              }
              require(std::ranges::all_of(regions, [](int count) { return count > 0; }), "la lluvia dibujada dejó una región del viewport sin cubrir");
              const auto count = RainDrops(static_cast<float>(rainy.getSize().x), static_cast<float>(rainy.getSize().y), animationTime_, weather == Weather::Storm).size();
              require(count >= 64 && count <= 360 && changed > 0, "la lluvia perdió su presupuesto o no dibujó trazos");
              const auto name = "rain-" + std::to_string(size.x) + "x" + std::to_string(size.y) + "-z" + std::to_string(zoom) + "-w" + std::to_string(static_cast<int>(weather)) + "-t" + std::to_string(phase) + ".png";
              if (zoom == 1 && weather == Weather::Rain) saveImage(rainy, name);
              report["rain"].push_back({{"output", {size.x, size.y}}, {"native", {rainy.getSize().x, rainy.getSize().y}}, {"zoom", zoom}, {"weather", static_cast<int>(weather)}, {"time", animationTime_}, {"drops", count}, {"changed_pixels", changed}, {"regions", regions}});
            }
            require(phases[0] != phases[1] && phases[1] != phases[2], "la lluvia permanece estática entre instantes");
            require(snapshot() == committed, "la lluvia cambió el clima lógico, reglas o estado de la partida");
          }
        }
      }

      resize({1920, 1080});
      for (int scenario = 0; scenario < 4; ++scenario) {
        if (scenario < 3) {
          scenario_ = scenario; match_ = Match::Create(balance_, ScenarioFile(), Mode::Local, Difficulty::Normal);
          screen_ = Screen::Playing; selected_ = -1; selectedCell_.reset(); hovered_.reset(); effects_ = false; grid_ = false;
          ResetPresentation(); FocusCamera(true);
        } else {
          fixture(); std::erase_if(match_->state.cells, [](const Cell &cell) { return (cell.pos.q < -2 && cell.pos.r > 0) || (cell.pos.q > 1 && cell.pos.r < -1) || cell.pos == Hex{0, 2}; });
          for (std::size_t i = 0; i < match_->state.cells.size(); ++i) if (match_->state.cells[i].building == Building::None) match_->state.cells[i].terrain = static_cast<Terrain>(i % 10);
          add(0, UnitClass::Saber, {-4, 0}); add(1, UnitClass::Saber, {4, 0}); ResetPresentation(); FocusCamera(true);
        }
        match_->state.weather = Weather::Clear; match_->config.weatherPeriod = 1000; match_->config.rotRadius = 1;
        std::vector<Hex> centers;
        for (const auto &cell : match_->state.cells) if (cell.terrain != Terrain::DeepWater && cell.terrain != Terrain::Blocked && (centers.empty() || Distance(centers.front(), cell.pos) <= match_->config.connectionDistance)) {
          centers.push_back(cell.pos); if (centers.size() == 3) break;
        }
        require(centers.size() == 3, "el escenario no tiene posiciones conectadas para el fixture local");
        match_->state.sources.clear(); for (Hex pos : centers) match_->state.sources.push_back({match_->state.nextId++, pos, 2});
        match_->state.globalRemaining = 0; animationTime_ = .37f; visuals_.Reset(*match_);
        require(match_->LargestSourceGroup() >= match_->config.globalThreshold, "el fixture no forma el componente lógico real");
        int beforeLocal = 0; for (const auto &cell : match_->state.cells) { const auto exposure = RotPresentationAt(*match_, cell.pos); beforeLocal += exposure.local; require(!exposure.global, "se dibuja peste global antes de activar su contador"); }
        require(beforeLocal > 0 && beforeLocal < static_cast<int>(match_->state.cells.size()), "el fixture local no distingue celdas lejanas");
        saveImage(render(), "plague-" + std::to_string(scenario) + "-local.png");
        match_->EndRound(); visuals_.Reset(*match_); require(match_->state.globalRemaining > 0, "el cierre real no activó la peste");
        const auto active = *match_; const auto committed = snapshot(); int globalCells = 0, occupied = 0;
        for (const auto &cell : match_->state.cells) {
          const auto exposure = RotPresentationAt(*match_, cell.pos); require(exposure.global, "la cobertura global omite una celda real del escenario");
          require(exposure.damage == match_->RotDamageAt(cell.pos), "el overlay global inventa una cifra de daño");
          ++globalCells; occupied += match_->Occupant(cell.pos) != nullptr;
        }
        saveImage(render(), "plague-" + std::to_string(scenario) + "-global.png");
        props_ = nullptr; units_ = nullptr; match_->state.sources.clear(); match_->state.globalRemaining = 0;
        const auto clean = render(); match_ = active; const auto infected = render(); int pixelCells = 0;
        for (const auto &cell : match_->state.cells) {
          const auto p = camera_.Project(cell.pos, static_cast<float>(cell.height)); int changed = 0, tested = 0;
          for (int dy = -3; dy <= 3; ++dy) for (int dx = -5; dx <= 5; ++dx) {
            const int x = static_cast<int>(std::round(p.x)) + dx, y = static_cast<int>(std::round(p.y)) + dy;
            if (x >= 0 && y >= 0 && x < static_cast<int>(infected.getSize().x) && y < static_cast<int>(infected.getSize().y)) { ++tested; changed += clean.getPixel({static_cast<unsigned>(x), static_cast<unsigned>(y)}) != infected.getPixel({static_cast<unsigned>(x), static_cast<unsigned>(y)}); }
          }
          if (tested) { require(changed > tested / 4, "una superficie visible no recibe cobertura global en los píxeles reales"); ++pixelCells; }
        }
        require(pixelCells == globalCells, "la vista de conjunto dejó celdas fuera de la prueba global"); props_ = savedProps; units_ = savedUnits;
        require(snapshot() == committed, "dibujar la peste volvió a aplicar daño o cambió una entidad");
        camera_.SetZoom(2, {}); camera_.PanBy({73, -41});
        const auto *far = &match_->state.cells.back(); camera_.CenterOn(far->pos, static_cast<float>(far->height), {world_.getSize().x / 2.f, world_.getSize().y / 2.f});
        saveImage(render(), "plague-" + std::to_string(scenario) + "-pan.png");
        match_->EndRound(); visuals_.Reset(*match_);
        require(match_->state.sources.empty() && match_->state.globalRemaining > 0, "las fuentes no dejaron la reserva global existente");
        for (const auto &cell : match_->state.cells) { const auto exposure = RotPresentationAt(*match_, cell.pos); require(exposure.global && !exposure.local && exposure.damage == match_->config.globalDamage, "la peste residual no cubre el escenario sin fuentes"); }
        saveImage(render(), "plague-" + std::to_string(scenario) + "-residual.png");
        while (match_->state.globalRemaining > 0) match_->EndRound();
        visuals_.Reset(*match_); for (const auto &cell : match_->state.cells) require(!RotPresentationAt(*match_, cell.pos).Active(), "la contaminación no desaparece al agotar el estado real");
        saveImage(render(), "plague-" + std::to_string(scenario) + "-final.png");
        report["plague"].push_back({{"scenario", scenario}, {"cells", globalCells}, {"local_before", beforeLocal}, {"occupied_cells", occupied}, {"pixel_covered_cells", pixelCells}, {"irregular", scenario == 3}, {"real_round_activation_and_expiry", true}});
      }
      resize({1440, 900});
      const std::array<const char *, 5> stageNames{{"preparation", "launch", "travel", "impact", "fade"}};
      for (Ability ability : {Ability::Attack, Ability::Fire, Ability::Ice, Ability::Lightning, Ability::Heal, Ability::Purify, Ability::Incinerate}) {
        fixture(); const int caster = add(0, UnitClass::Caster, {}); Hex target{2, 0};
        if (ability == Ability::Heal) { const int ally = add(0, UnitClass::Saber, target); match_->Find(ally)->hp = 6; }
        else add(1, UnitClass::Saber, target);
        if (ability == Ability::Lightning) { add(1, UnitClass::Saber, {3, 0}); add(1, UnitClass::Saber, {3, -1}); }
        if (ability == Ability::Purify) match_->state.sources.push_back({match_->state.nextId++, target, 4});
        if (ability == Ability::Incinerate) { target = {1, 0}; match_->state.corpses.push_back({match_->state.nextId++, target, match_->state.round, 1, UnitClass::Saber, -1}); }
        require(match_->BeginActivation(caster).ok, "el Caster no pudo iniciar su acción real");
        visuals_.Reset(*match_); const Match before = *match_;
        const auto result = match_->UseAbility(caster, ability, target); require(result.ok, "el hechizo real fue rechazado: " + result.message);
        visuals_.Observe(before, *match_, ability); const auto committed = snapshot();
        const auto found = std::ranges::find_if(visuals_.Effects(), [](const VisualEffect &effect) { return effect.stagedSpell; });
        require(found != visuals_.Effects().end(), "el Caster no creó una presentación mágica por etapas"); const auto primary = *found;
        const auto effectCount = visuals_.Effects().size(); visuals_.Observe(*match_, *match_);
        require(visuals_.Effects().size() == effectCount, "observar el mismo hechizo duplicó efectos");
        const float contact = primary.contactSeconds, tail = primary.lifetime - contact;
        require(contact > 0.f && tail > 0.f, "el hechizo perdió su intervalo anterior de contacto o recuperación");
        const std::array<float, 5> times{{contact * .2f, contact * .525f, contact * .8f, contact + tail * .2f, contact + tail * .85f}};
        Json stages = Json::array(); float elapsed = 0.f; std::set<std::uint64_t> stageHashes;
        for (std::size_t index = 0; index < times.size(); ++index) {
          const float absolute = primary.delay + times[index]; visuals_.Update(absolute - elapsed); elapsed = absolute;
          const auto current = std::ranges::find_if(visuals_.Effects(), [&](const VisualEffect &effect) { return effect.stagedSpell && effect.type == primary.type && effect.start == primary.start && effect.end == primary.end; });
          require(current != visuals_.Effects().end() && current->Active(), "una etapa visible desapareció antes de su tiempo");
          const auto sample = SampleSpell(*current); require(static_cast<std::size_t>(sample.stage) == index, "el render recibió etapas mágicas fuera de orden");
          const auto image = render(); const auto hash = imageHash(image); stageHashes.insert(hash);
          const auto name = "spell-" + std::to_string(static_cast<int>(ability)) + "-" + stageNames[index] + ".png"; saveImage(image, name);
          stages.push_back({{"stage", stageNames[index]}, {"elapsed", current->elapsed}, {"absolute_time", elapsed}, {"render_hash", hash}, {"capture", name}, {"projectile", sample.projectile}});
          require(snapshot() == committed, "una etapa del Caster cambió otra vez el daño, maná u objetivo");
        }
        require(stageHashes.size() > 1, "el hechizo mantiene una imagen idéntica durante todas sus etapas");
        visuals_.Update(5.f); require(visuals_.Effects().empty() && !visuals_.Busy(), "el Caster dejó partículas huérfanas o bloqueó el siguiente turno");
        require(snapshot() == committed, "desvanecer la magia ejecutó de nuevo una acción lógica");
        report["spells"].push_back({{"ability", static_cast<int>(ability)}, {"contact_seconds", contact}, {"end_seconds", primary.delay + primary.lifetime}, {"stages", stages}, {"distinct_stage_captures", stageHashes.size()}, {"gameplay_unchanged", true}});
      }
      restore();
      report["checks"] = checks; report["pass"] = true;
      report["runtime"] = "AnimationDirector::Update, RenderWorld, copyToImage y ScreenshotManager reales; delta visual controlado, sin afirmar una sesión humana.";
      std::ofstream(folder / "verification.json") << report.dump(2) << '\n';
      std::cout << "Plaguefront visual corrections: " << checks << " runtime checks; temporal atlas playback, rain viewports and full-map plague lifecycle.\n";
    } catch (...) { restore(); throw; }
  }
} // Espacio de nombres Plaguefront
