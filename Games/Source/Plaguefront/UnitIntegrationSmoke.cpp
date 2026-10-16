#include "Plaguefront/Game.h"
#include "Plaguefront/UnitPresentation.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <nlohmann/json.hpp>
#include <set>
#include <sstream>
#include <stdexcept>

namespace Plaguefront {
  void Game::SmokeUnitIntegration() {
    using Json = nlohmann::json;
    int checks = 0;
    const auto require = [&](bool value, const std::string &reason) { ++checks; if (!value) throw std::runtime_error("Integración de unidades: " + reason); };
    const auto folder = std::filesystem::path("build/unit-integration/runtime");
    std::filesystem::create_directories(folder);
    std::filesystem::remove(folder / "verification.json");
    const auto savedMatch = match_, savedObserved = observed_;
    const auto savedVisuals = visuals_; const auto savedCamera = camera_;
    const auto savedScreen = screen_; const auto savedContext = tactical_;
    const auto savedCell = selectedCell_, savedHover = hovered_;
    const auto savedMode = mode_; const auto savedScenario = scenario_, savedSelected = selected_;
    const auto savedScale = worldPixelScale_; const auto savedEffects = effects_, savedGrid = grid_;
    const auto savedTime = animationTime_; const auto savedMessage = message_;
    const auto savedOutput = ctx.window.OutputSize();
    auto *savedUnits = units_; sf::Texture candidateTexture;
    const auto resize = [&](sf::Vector2u size) {
      ctx.window.SetFullscreen(false); ctx.window.SetResolution(size); ctx.window.ApplyPending();
      ctx.renderer.ConfigureOutput(ctx.window.OutputSize()); ctx.cursor.SetView(ctx.renderer.View());
      RefreshLayout(true); displayRevision_ = ctx.window.Revision();
      require(ctx.renderer.OutputSize() == size, "la resolución real no coincide con la solicitada");
    };
    const auto restore = [&] {
      units_ = savedUnits; match_ = savedMatch; observed_ = savedObserved; screen_ = savedScreen; mode_ = savedMode;
      scenario_ = savedScenario; selected_ = savedSelected; selectedCell_ = savedCell; hovered_ = savedHover;
      effects_ = savedEffects; grid_ = savedGrid; animationTime_ = savedTime; message_ = savedMessage;
      worldPixelScale_ = savedScale; resize(savedOutput); camera_ = savedCamera; visuals_ = savedVisuals;
      SetContext(savedContext); ctx.cursor.SetVirtualPosition({});
    };
    const auto saveImage = [&](const sf::Image &image, const std::string &name) {
      const sf::Texture texture(image);
      require(ctx.screenshot.SaveFrame(texture, image.getSize(), (folder / name).string()), "ScreenshotManager no pudo guardar " + name);
    };
    const auto render = [&] { RenderWorld(); return world_.getTexture().copyToImage(); };
    const auto snapshot = [&] {
      const auto file = folder / "match-inmutabilidad.json"; std::string error;
      require(match_->Save(file.string(), error), "la partida de prueba no es válida: " + error);
      std::ifstream input(file); std::ostringstream bytes; bytes << input.rdbuf(); return bytes.str();
    };
    const auto add = [&](int owner, UnitClass type, Hex pos) {
      Unit unit; unit.id = match_->state.nextId++; unit.owner = owner; unit.type = type; unit.pos = pos;
      unit.hp = match_->config.classes[static_cast<std::size_t>(type)].hp;
      unit.mp = type == UnitClass::Caster ? match_->config.maxMana : 0;
      match_->state.units.push_back(unit); return unit.id;
    };
    const auto composite = [](sf::Color below, sf::Color above) {
      const auto channel = [&](unsigned a, unsigned b) { return static_cast<std::uint8_t>((a * above.a + b * (255 - above.a) + 127) / 255); };
      return sf::Color{channel(above.r, below.r), channel(above.g, below.g), channel(above.b, below.b), 255};
    };
    const auto nearColor = [](sf::Color actual, sf::Color expected) {
      return std::abs(static_cast<int>(actual.r) - expected.r) <= 2 && std::abs(static_cast<int>(actual.g) - expected.g) <= 2 && std::abs(static_cast<int>(actual.b) - expected.b) <= 2 && actual.a == expected.a;
    };
    const std::array<sf::Color, 2> teams{{{109, 186, 224}, {225, 100, 120}}};
    Json report{{"pass", false}, {"units", Json::array()}, {"battle", Json::object()}};
    try {
      resize({1440, 900}); worldPixelScale_ = 2; RefreshLayout(true);
      const auto candidateFolder = std::filesystem::path("build/unit-integration/candidate");
      const auto candidatePath = candidateFolder / "units.png";
      const bool previewCandidate = std::filesystem::exists(candidateFolder / "preview-only") && std::filesystem::exists(candidatePath);
      if (previewCandidate) {
        require(candidateTexture.loadFromFile(candidatePath), "SFML no puede cargar el candidato aislado");
        candidateTexture.setSmooth(false); units_ = &candidateTexture;
      }
      report["resource"] = previewCandidate ? candidatePath.generic_string() : "Content/Textures/Plaguefront/Pixel/units.png";
      report["temporary_texture_override"] = units_ != savedUnits;
      report["human_playthrough"] = false;
      require(units_ != nullptr, "el renderer no tiene el atlas de unidades cargado");
      const auto atlas = units_->copyToImage(); require(atlas.getSize() == sf::Vector2u{2304, 3584}, "el atlas cargado no conserva sus celdas de 48 por 64");
      sf::Image approved;
      require(approved.loadFromFile("Tools/Art/Sources/Units/Approved/approved-atlas.png"), "falta la copia original aprobada para contrastar la identidad");
      require(approved.getSize() == atlas.getSize(), "la copia aprobada tiene otro contrato de atlas");
      for (unsigned y = 0; y < atlas.getSize().y; ++y)
        require(std::memcmp(atlas.getPixelsPtr() + y * 2304 * 4, approved.getPixelsPtr() + y * 2304 * 4, 384 * 4) == 0, "el atlas cargado altera los píxeles del reposo original");
      report["original_idle_cells_exactly_recovered"] = 448;
      const auto anchorsPath = previewCandidate ? candidateFolder / "locomotion-anchors.json" : std::filesystem::path("Tools/Art/Sources/Units/Approved/locomotion-anchors.json");
      std::ifstream anchorInput(anchorsPath);
      require(anchorInput.good(), "falta el manifiesto obligatorio de insignias originales");
      std::map<std::array<int, 5>, Json> anchors;
      if (anchorInput) {
        const auto anchorData = Json::parse(anchorInput);
        for (const auto &entry : anchorData.at("frames")) {
          const std::array<int, 5> key{{entry.at("class"), entry.at("owner"), entry.at("direction"), entry.at("state"), entry.at("frame")}};
          require(anchors.emplace(key, entry).second, "el manifiesto repite una marca anatómica");
        }
        require(anchors.size() == 7 * 2 * 4 * 2 * 8, "el manifiesto no cubre Idle y Walk de todas las tropas y orientaciones");
      }
      report["anatomical_badge_status"] = anchors.empty() ? "pendiente: sin máscaras de las insignias originales" : "patrón original contrastado sobre el atlas y el render; no se dibujan nuevas insignias";
      const std::array<Hex, 4> destinations{{{1, 0}, {-1, 1}, {1, -1}, {-1, 0}}};
      constexpr unsigned sceneWidth = 160, sceneHeight = 112;
      for (int owner = 0; owner < 2; ++owner) for (int cls = 0; cls < 7; ++cls) for (int direction = 0; direction < 4; ++direction) {
        match_ = Match(balance_); match_->state.mode = Mode::Local; match_->state.currentPlayer = owner; match_->config.weatherPeriod = 1000;
        const int height = cls % 3;
        for (int q = -4; q <= 4; ++q) for (int r = -3; r <= 3; ++r) match_->state.cells.push_back({{q, r}, Terrain::Plain, height});
        match_->At({-4, 0})->building = Building::Fortress; match_->At({-4, 0})->owner = 0;
        match_->At({4, 0})->building = Building::Fortress; match_->At({4, 0})->owner = 1;
        const int id = add(owner, static_cast<UnitClass>(cls), {});
        add(0, UnitClass::Saber, {-3, 1}); add(1, UnitClass::Lancer, {3, -1});
        mode_ = Mode::Local; screen_ = Screen::Playing; selected_ = -1; selectedCell_.reset(); hovered_.reset();
        effects_ = false; grid_ = false; animationTime_ = .37F; ctx.cursor.SetVirtualPosition(sf::Vector2f{4, 4});
        require(match_->BeginActivation(id).ok, "la tropa no puede iniciar su movimiento real");
        ResetPresentation(); camera_.SetZoom(1, {}); camera_.CenterOn({}, static_cast<float>(height), {world_.getSize().x / 2.F, world_.getSize().y / 2.F});
        const auto initialPoint = SnapUnitPoint(camera_.Project({}, static_cast<float>(height)));
        const sf::IntRect sceneRect({static_cast<int>(initialPoint.x) - 80, static_cast<int>(initialPoint.y) - 72}, {sceneWidth, sceneHeight});
        require(sceneRect.position.x >= 0 && sceneRect.position.y >= 0 && sceneRect.position.x + static_cast<int>(sceneWidth) <= static_cast<int>(world_.getSize().x) && sceneRect.position.y + static_cast<int>(sceneHeight) <= static_cast<int>(world_.getSize().y), "la cámara no contiene el recorte fijo de la secuencia");
        const Match before = *match_; const Hex destination = destinations[static_cast<std::size_t>(direction)];
        const auto path = before.FindPath(id, destination); require(path && path->cells.size() == 2, "la marcha de prueba no recorre exactamente una casilla legal");
        const auto initialPixels = render(); sf::Image initialScene({sceneWidth, sceneHeight}, sf::Color::Transparent);
        require(initialScene.copy(initialPixels, {}, sceneRect), "no se pudo capturar el reposo inicial real");
        const auto initialName = "initial-idle-" + std::to_string(cls) + "-" + std::to_string(owner) + "-" + std::to_string(direction) + ".png";
        saveImage(initialScene, initialName);
        require(match_->Move(id, destination).ok, "Match::Move rechazó el destino legal");
        visuals_.Observe(before, *match_); const auto committed = snapshot();
        const auto allUnits = match_->state.units;
        std::erase_if(match_->state.units, [id](const Unit &unit) { return unit.id == id; }); const auto background = render(); match_->state.units = allUnits;
        sf::Image strip({sceneWidth * 25, sceneHeight}, sf::Color::Transparent);
        Json samples = Json::array(); std::set<int> walkFrames; float elapsed = 0.F; int totalShadowPixels = 0, totalGlyphPixels = 0;
        const auto advanceTo = [&](float target) {
          require(target >= elapsed, "la secuencia intenta retroceder el reloj");
          while (target - elapsed > .000001F) { const float step = std::min(.025F, target - elapsed); visuals_.Update(step); elapsed += step; }
        };
        for (int sample = 0; sample < 25; ++sample) {
          const bool walking = sample < 8, arriving = sample == 8;
          const int frame = walking ? sample : arriving ? 0 : (sample - 9) % 8;
          const float target = walking ? (static_cast<float>(frame) + .5F) * AnimationTiming.walkFrameSeconds
                                       : arriving ? AnimationTiming.walkCellSeconds + .0001F
                                                  : AnimationTiming.walkCellSeconds + (static_cast<float>(sample - 9) + .5F) * AnimationTiming.idleFrameSeconds;
          advanceTo(target); const auto *unit = match_->Find(id); const auto visual = visuals_.VisualFor(*unit);
          require(visual.state == (walking ? AnimState::Walk : AnimState::Idle) && visual.frame == frame && visual.facing == direction, "Update omitió un marco o cambió la orientación real");
          const float fraction = walking ? target / AnimationTiming.walkCellSeconds : 1.F;
          require(std::abs(visual.q - destination.q * fraction) < .0001F && std::abs(visual.r - destination.r * fraction) < .0001F && std::abs(visual.elevation - height) < .0001F, "el marco se dibuja fuera de la interpolación y altura comprometidas");
          require(visuals_.Busy() == walking, "la marcha o idle bloquean entradas durante un intervalo incorrecto");
          if (walking) walkFrames.insert(visual.frame);
          const auto projected = camera_.ProjectContinuous(visual.q, visual.r, visual.elevation), point = SnapUnitPoint(projected);
          const int x0 = static_cast<int>(point.x) - 24, y0 = static_cast<int>(point.y) - 56;
          require(x0 >= 0 && y0 >= 0 && x0 + 48 <= static_cast<int>(world_.getSize().x) && y0 + 64 <= static_cast<int>(world_.getSize().y), "el marco de unidad quedó recortado por la vista");
          const int column = static_cast<int>(visual.state) * 8 + visual.frame, row = cls + 7 * owner + 14 * direction;
          std::array<std::uint8_t, 48 * 64> alpha{};
          for (int y = 0; y < 64; ++y) for (int x = 0; x < 48; ++x) alpha[static_cast<std::size_t>(y * 48 + x)] = atlas.getPixel({static_cast<unsigned>(column * 48 + x), static_cast<unsigned>(row * 64 + y)}).a;
          const auto contact = UnitContactGeometry(unit->type, visual.state, direction, frame, alpha);
          require(contact.valid && contact.shadowTop == contact.bottom - 1, "el apoyo real del atlas no tiene contacto con su sombra");
          const auto pixels = render(); int opaquePixels = 0, shadowPixels = 0, groundBadgePixels = 0;
          for (int y = 0; y < 64; ++y) for (int x = 0; x < 48; ++x) {
            const sf::Vector2u screen{static_cast<unsigned>(x0 + x), static_cast<unsigned>(y0 + y)};
            auto expected = background.getPixel(screen); const auto source = atlas.getPixel({static_cast<unsigned>(column * 48 + x), static_cast<unsigned>(row * 64 + y)});
            unsigned shadowAlpha = 0; const int shadowRow = y - contact.shadowTop, coreRows = contact.shadowHeight - 1;
            if (shadowRow >= 0 && shadowRow < coreRows) {
              const int inset = coreRows > 1 && shadowRow == 0 ? 2 : 0;
              if (x >= contact.shadowLeft + inset && x < contact.shadowLeft + contact.shadowWidth - inset) shadowAlpha = 90;
            } else if (shadowRow == coreRows && x >= contact.shadowLeft + 2 && x < contact.shadowLeft + contact.shadowWidth - 2) shadowAlpha = 45;
            if (shadowAlpha) expected = composite(expected, {10, 15, 24, static_cast<std::uint8_t>(shadowAlpha)});
            if (source.a) expected = composite(expected, source);
            const bool badge = owner == 0 ? ((x >= 10 && x < 13 && y == 60) || (x == 11 && y >= 59 && y < 62)) : ((x == 10 || x == 12) && y >= 59 && y < 62);
            if (badge) expected = teams[static_cast<std::size_t>(owner)];
            const auto actual = pixels.getPixel(screen);
            if (!nearColor(actual, expected)) require(false, "el recorte real difiere del atlas, suelo, sombra o insignia en clase " + std::to_string(cls) + " estado " + std::to_string(static_cast<int>(visual.state)) + " marco " + std::to_string(frame) + " píxel " + std::to_string(x) + "," + std::to_string(y));
            else ++checks;
            opaquePixels += source.a == 255; shadowPixels += shadowAlpha > 0 && source.a == 0 && !badge; groundBadgePixels += badge;
          }
          require(opaquePixels > 0 && shadowPixels > 0 && groundBadgePixels > 0, "el marco no contiene cuerpo, sombra visible e identificación de bando");
          const auto anchor = anchors.find({cls, owner, direction, static_cast<int>(visual.state), frame});
          int glyphPixels = 0; Json glyphAnchor = nullptr;
          if (!anchors.empty()) {
            require(anchor != anchors.end(), "falta la marca anatómica del marco dibujado"); glyphAnchor = anchor->second.at("anchor");
            for (const auto &pixel : anchor->second.at("glyph_pixels")) {
              const int x = pixel.at(0), y = pixel.at(1); require(x >= 0 && x < 48 && y >= 0 && y < 64, "una marca anatómica sale de su celda");
              const auto expected = sf::Color{215, 206, 170, 255};
              require(atlas.getPixel({static_cast<unsigned>(column * 48 + x), static_cast<unsigned>(row * 64 + y)}) == expected && pixels.getPixel({static_cast<unsigned>(x0 + x), static_cast<unsigned>(y0 + y)}) == expected, "la insignia bordada no sigue su ancla anatómica declarada");
              ++glyphPixels;
            }
            require(glyphPixels >= 4, "la insignia anatómica perdió su patrón identificable");
          }
          require(strip.copy(pixels, {static_cast<unsigned>(sample) * sceneWidth, 0}, sceneRect), "no se pudo copiar el recorte de cámara fija");
          if (owner == 0 && (direction == 0 || direction == 2) && sample == 3) saveImage(pixels, "scene-" + std::to_string(cls) + "-" + std::to_string(direction) + ".png");
          totalShadowPixels += shadowPixels; totalGlyphPixels += glyphPixels;
          samples.push_back({{"sample", sample}, {"time", target}, {"arrival", arriving}, {"state", static_cast<int>(visual.state)}, {"frame", frame}, {"q", visual.q}, {"r", visual.r}, {"height", visual.elevation}, {"projected", {projected.x, projected.y}}, {"snapped", {point.x, point.y}}, {"atlas_cell", {column, row}}, {"crop", {x0, y0, 48, 64}}, {"support", {contact.left, contact.right, contact.bottom}}, {"shadow", {contact.shadowLeft, contact.shadowTop, contact.shadowWidth, contact.shadowHeight}}, {"visible_shadow_pixels", shadowPixels}, {"glyph_anchor", glyphAnchor}, {"glyph_pixels", glyphPixels}, {"opaque_pixels", opaquePixels}, {"busy", visuals_.Busy()}});
        }
        require(walkFrames.size() == 8 && !visuals_.Busy() && snapshot() == committed, "la secuencia no completa los ocho marcos o modifica la partida resuelta");
        const auto name = "motion-" + std::to_string(cls) + "-" + std::to_string(owner) + "-" + std::to_string(direction) + ".png";
        saveImage(strip, name);
        report["units"].push_back({{"class", cls}, {"owner", owner}, {"direction", direction}, {"path", {{0, 0}, {destination.q, destination.r}}}, {"path_cost", path->cost}, {"walk_seconds", AnimationTiming.walkCellSeconds}, {"walk_frames", walkFrames.size()}, {"idle_cycles", 2}, {"native_frame", {sceneWidth, sceneHeight}}, {"camera_crop", {sceneRect.position.x, sceneRect.position.y, sceneWidth, sceneHeight}}, {"initial_idle", initialName}, {"strip", name}, {"visible_shadow_pixels", totalShadowPixels}, {"verified_glyph_pixels", totalGlyphPixels}, {"samples", samples}, {"logical_unchanged", true}});
      }
      scenario_ = 0; match_ = Match::Create(balance_, ScenarioFile(), Mode::Local, Difficulty::Normal);
      mode_ = Mode::Local; screen_ = Screen::Playing; selected_ = -1; selectedCell_.reset(); hovered_.reset(); effects_ = false; grid_ = true;
      ResetPresentation(); FocusCamera(false); camera_.SetZoom(1, {});
      int actor = -1; Hex destination{};
      for (const auto &candidate : match_->state.units) if (candidate.owner == match_->state.currentPlayer)
        for (const auto neighbor : Neighbors(candidate.pos)) if (const auto path = match_->FindPath(candidate.id, neighbor); path && path->cells.size() == 2 && path->cost <= match_->EffectiveStats(candidate).movement) { actor = candidate.id; destination = neighbor; break; }
      require(actor >= 0 && match_->BeginActivation(actor).ok, "el escenario cargado no tiene una marcha legal de batalla");
      visuals_.Reset(*match_); const Match beforeBattle = *match_; require(match_->Move(actor, destination).ok, "la batalla cargada rechazó su marcha legal");
      visuals_.Observe(beforeBattle, *match_); const auto committedBattle = snapshot();
      const auto *battleUnit = match_->Find(actor); const auto *startCell = match_->At(beforeBattle.Find(actor)->pos);
      camera_.CenterOn(startCell->pos, static_cast<float>(startCell->height), {world_.getSize().x / 2.F, world_.getSize().y / 2.F});
      Json battleFrames = Json::array();
      for (int frame = 0; frame < 8; ++frame) {
        const int steps = frame == 0 ? 1 : 2;
        for (int step = 0; step < steps; ++step) visuals_.Update(AnimationTiming.walkFrameSeconds * .5F);
        const auto visual = visuals_.VisualFor(*battleUnit); require(visual.state == AnimState::Walk && visual.frame == frame, "el escenario cargado omite marcos de marcha");
        const auto name = "battle-walk-" + std::to_string(frame) + ".png"; saveImage(render(), name);
        battleFrames.push_back({{"frame", frame}, {"q", visual.q}, {"r", visual.r}, {"height", visual.elevation}, {"capture", name}});
      }
      visuals_.Update(AnimationTiming.walkFrameSeconds * .5F); visuals_.Update(AnimationTiming.walkFrameSeconds * .5F);
      require(visuals_.VisualFor(*battleUnit).state == AnimState::Idle && snapshot() == committedBattle, "la batalla real no llega a idle o altera sus reglas al renderizar");
      saveImage(render(), "battle-arrival.png");
      report["battle"] = {{"scenario", match_->state.scenario}, {"units", match_->state.units.size()}, {"cells", match_->state.cells.size()}, {"actor", actor}, {"loaded_scenario", true}, {"frames", battleFrames}, {"logical_unchanged", true}};
      report["walk_interval_seconds"] = AnimationTiming.walkFrameSeconds; report["idle_interval_seconds"] = AnimationTiming.idleFrameSeconds;
      report["controlled_delta_max_seconds"] = .025F; report["atlas_size"] = {atlas.getSize().x, atlas.getSize().y}; report["anatomical_anchors"] = anchors.empty() ? Json(nullptr) : Json(anchorsPath.generic_string());
      report["runtime"] = "Match::Move, Observe, Update con delta controlado, RenderWorld y ScreenshotManager reales; cámara fija e interpolación visible; no es una sesión humana ni una medición de FPS.";
      restore(); report["checks"] = checks; report["pass"] = true; std::ofstream(folder / "verification.json") << report.dump(2) << '\n';
      std::cout << "Plaguefront unit integration: " << checks << " runtime checks; 56 complete walks, 112 idle cycles, alpha support, contact shadows, ground badges, loaded battle scenario; visual identity approval remains separate.\n";
    } catch (const std::exception &error) { report["checks"] = checks; report["error"] = error.what(); std::ofstream(folder / "verification.json") << report.dump(2) << '\n'; restore(); throw; }
  }
} // Espacio de nombres Plaguefront
