#include "Plaguefront/Game.h"
#include <SFML/Graphics.hpp>
#include <nlohmann/json.hpp>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Plaguefront {
  namespace {
    const sf::Color Ink{17, 20, 29}, Panel{29, 33, 42, 255}, Edge{105, 98, 78}, Gold{216, 184, 116}, Muted{164, 169, 158};
    const std::array<sf::Color, 2> Teams{sf::Color{101, 177, 216}, sf::Color{224, 100, 119}};
    sf::Font &CachedFont(EngineContext &context, const std::string &name) {
      auto *font = context.resources.FetchFont(name);
      if (!font)
        throw std::runtime_error("Falta fuente local: " + name);
      return *font;
    }
    const std::string SaveFile = "Saves/plaguefront.json";
    sf::String Utf8(const std::string &text) { return sf::String::fromUtf8(text.begin(), text.end()); }
    std::string DifficultyName(Difficulty d) { return std::array<std::string, 3>{"Fácil", "Normal", "Difícil"}[static_cast<int>(d)]; }
  } // Espacio de nombres

  Game::Game(EngineContext &context) : Scene(context), font_(CachedFont(context, "DejaVuSans.ttf")), headingFont_(CachedFont(context, "Caudex-Regular.ttf")), audio_(context.audio), world_(sf::Vector2u{486, 320}) {
    font_.setSmooth(true);
    headingFontLoaded_ = true;
    headingFont_.setSmooth(true);
    world_.setSmooth(false);
  }

  void Game::Start() {
    ctx.cursor.SetVisible(true);
    ctx.renderer.SetResponsiveLayout(true);
    ctx.renderer.SetPixelScale(1);
    balance_ = Config::Load("Content/Plaguefront/balance.json");
    tiles_ = ctx.resources.FetchTexture("Plaguefront/Pixel/terrain.png");
    units_ = ctx.resources.FetchTexture("Plaguefront/Pixel/units.png");
    icons_ = ctx.resources.FetchTexture("Plaguefront/Pixel/icons.png");
    menu_ = ctx.resources.FetchTexture("Plaguefront/Pixel/menu.png");
    props_ = ctx.resources.FetchTexture("Plaguefront/Pixel/props.png");
    spellEffects_ = ctx.resources.FetchTexture("Plaguefront/Pixel/effects.png");
    skin_ = ctx.resources.FetchTexture("Plaguefront/Pixel/ui.png");
    unitIcons_ = ctx.resources.FetchTexture("Plaguefront/Pixel/unit-icons.png");
    if (!tiles_ || !units_ || !icons_ || !menu_ || !props_ || !spellEffects_ || !skin_)
      throw std::runtime_error("Faltan recursos pixel art de Plaguefront.");
    for (auto texture : {tiles_, units_, icons_, menu_, props_, spellEffects_, skin_})
      texture->setSmooth(false);
    if (!audio_.Start())
      message_ = "No se pudo inicializar el audio. Revisa Content/Plaguefront/audio.json.";
    LoadSettings();
    RefreshContinue();
    LoadArmies();
    ChangeScreen(Screen::Menu);
  }

  std::string Game::ScenarioFile() const { return "Content/Plaguefront/scenarios/" + std::array<std::string, 3>{"frontier.json", "marsh.json", "highlands.json"}[std::clamp(scenario_, 0, 2)]; }
  void Game::LoadArmies() {
    try {
      auto preview = Match::Create(balance_, ScenarioFile(), mode_, difficulty_);
      for (auto &army : armies_)
        army.clear();
      for (const auto &u : preview.state.units)
        armies_[u.owner].push_back(u.type);
    } catch (const std::exception &e) {
      message_ = e.what();
    }
  }
  void Game::RefreshContinue() {
    std::string error;
    canContinue_ = Match::Load(SaveFile, balance_, error).has_value();
  }
  void Game::LoadSettings() {
    try {
      const auto loaded = ctx.save.ReadDocument(settingsFile_);
      if (!loaded && std::filesystem::exists(settingsFile_))
        throw std::runtime_error("Configuración inválida");
      if (loaded) {
        const auto &j = *loaded;
        grid_ = j.value("grid", true);
        effects_ = j.value("effects", false);
        aiDelay_ = std::clamp(j.value("ai_delay", 0.55f), 0.15f, 1.2f);
        volume_ = std::clamp(j.value("volume", 60), 0, 100);
        musicVolume_ = std::clamp(j.value("music_volume", 60), 0, 100);
        soundVolume_ = std::clamp(j.value("sound_volume", 80), 0, 100);
        muted_ = j.value("muted", false);
        ctx.window.LoadPreferences(j);
      }
    } catch (...) {
      message_ = "Configuración inválida: se usan los valores iniciales.";
    }
    ctx.renderer.SetEffectsEnabled(false); // Conserva los bordes del pixel art; la animación ambiental se configura por separado.
    audio_.SetVolumes(static_cast<float>(volume_), static_cast<float>(musicVolume_), static_cast<float>(soundVolume_), muted_);
    displayRevision_ = ctx.window.Revision();
  }
  void Game::SaveSettings() {
    try {
      auto values = nlohmann::json{{"grid", grid_}, {"effects", effects_}, {"ai_delay", aiDelay_}, {"volume", volume_}, {"music_volume", musicVolume_}, {"sound_volume", soundVolume_}, {"muted", muted_}};
      ctx.window.StorePreferences(values);
      if (!ctx.save.WriteDocument(settingsFile_, values))
        throw std::runtime_error("No se pudo guardar la configuración local.");
    } catch (const std::exception &e) {
      message_ = e.what();
    }
  }
  void Game::ChangeScreen(Screen screen) {
    if (resolutionOpen_)
      CloseResolutionSelect(false);
    if (screen_ != screen)
      audio_.Play(screen == Screen::Handoff ? "handoff" : "menu");
    if (screen_ != screen)
      ctx.input.SuppressUntilReleased();
    screen_ = screen;
    buttons_.clear();
    hoverButton_ = -1;
    keyboardFocus_ = -1;
    if (screen == Screen::Menu || screen == Screen::Setup) {
      SetContext(TacticalContext::Selection);
      customBattalionEditing_ = false;
    }
    if (screen == Screen::Setup)
      message_.clear();
    previewPath_.reset();
    previewDestination_.reset();
    previewRevision_ = ~std::uint64_t{0};
    if (screen == Screen::Victory && match_)
      audio_.SetScene(match_->state.mode == Mode::VersusAI && match_->state.winner == 1 ? "defeat" : "victory");
    else if (screen == Screen::Playing || screen == Screen::Paused || screen == Screen::Handoff)
      audio_.SetScene("battle");
    else if (screen == Screen::Menu || screen == Screen::Setup || screen == Screen::Credits)
      audio_.SetScene("menu");
  }

  void Game::ResetPresentation() {
    if (!match_)
      return;
    ++matchRevision_;
    // El tercer escenario usa columnas alternas que, sin este giro de cámara,
    // forman un rectángulo frontal. Conserva las coordenadas de reglas/guardado.
    camera_.SetRotation(match_->state.scenario == "Los tres pasos" ? 1 : 0);
    visuals_.SetRotation(camera_.Rotation());
    RefreshLayout(true);
    visuals_.Reset(*match_);
    observed_ = *match_;
    audio_.Observe(*match_, *match_);
    SetContext(TacticalContext::Selection);
    const auto bounds = camera_.Bounds(match_->state.cells);
    if (bounds.width > world_.getSize().x || bounds.height > world_.getSize().y)
      FocusCamera(false);
  }
  void Game::SyncPresentation(std::optional<Ability> hint) {
    if (!match_)
      return;
    if (observed_) {
      visuals_.Observe(*observed_, *match_, hint);
      audio_.Observe(*observed_, *match_, hint);
    } else
      visuals_.Reset(*match_);
    observed_ = *match_;
    ++matchRevision_;
    previewRevision_ = ~std::uint64_t{0};
  }
  std::optional<Hex> Game::PickCell(sf::Vector2f cursor) const {
    if (!match_ || !BoardBounds().contains(cursor))
      return {};
    const float scale = WorldScale();
    return camera_.Pick(match_->state.cells, {(cursor.x - BoardOrigin.x) / scale, (cursor.y - BoardOrigin.y) / scale});
  }
  void Game::BeginGame(bool restart) {
    try {
      CampaignSetup options{mode_, difficulty_, scenario_, armies_};
      Config rules = balance_;
      if (restart && restartSetup_) {
        options = *restartSetup_;
        if (match_)
          rules = match_->config;
      }
      match_ = Match::Create(rules, "Content/Plaguefront/scenarios/" + std::array<std::string, 3>{"frontier.json", "marsh.json", "highlands.json"}[std::clamp(options.scenario, 0, 2)], options.mode, options.difficulty, options.armies);
      restartSetup_ = options;
      selected_ = -1;
      selectedCell_.reset();
      padCell_.reset();
      padBoard_ = true;
      aiElapsed_ = 0;
      worldPixelScale_ = 2;
      ResetPresentation();
      message_ = "Selecciona una unidad. Azul: movimiento; rojo: objetivo. La fortaleza decide la victoria.";
      ChangeScreen(Screen::Playing);
    } catch (const std::exception &e) {
      message_ = e.what();
    }
  }
  void Game::Apply(Result result, std::optional<Ability> hint) {
    message_ = result.message;
    if (result.ok)
      SyncPresentation(hint);
    else
      audio_.Play("cancel");
    if (match_ && match_->state.winner >= 0 && !visuals_.Busy())
      ChangeScreen(Screen::Victory);
  }
  bool Game::EnsureActive() {
    if (!match_ || selected_ < 0)
      return false;
    if (match_->state.activeUnit == selected_)
      return true;
    auto result = match_->BeginActivation(selected_);
    Apply(result);
    return result.ok;
  }
  void Game::FinishTurn() {
    if (!match_ || screen_ != Screen::Playing || visuals_.Busy() || (match_->state.mode == Mode::VersusAI && match_->state.currentPlayer == 1))
      return;
    const auto *unit = match_->Find(selected_);
    if (!unit || !match_->Available(*unit)) {
      message_ = "Selecciona una tropa disponible. F termina el turno del ejército.";
      return;
    }
    if (!EnsureActive())
      return;
    Apply(match_->EndActivation());
    SetContext(TacticalContext::Orders);
    message_ = "Activación terminada. Elige otra tropa o FIN DE TURNO.";
  }
  void Game::FinishArmyTurn(bool confirmed) {
    if (!match_ || visuals_.Busy() || match_->state.winner >= 0 || (match_->state.mode == Mode::VersusAI && match_->state.currentPlayer == 1))
      return;
    if (!confirmed && match_->AvailableCount() > 0) {
      ChangeScreen(Screen::ConfirmTurn);
      return;
    }
    const int old = match_->state.currentPlayer;
    Apply(match_->EndTurn());
    selected_ = -1;
    selectedCell_.reset();
    hovered_.reset();
    padCell_.reset();
    rosterScroll_ = 0;
    aiElapsed_ = 0;
    SetContext(TacticalContext::Selection);
    reachable_.clear();
    ChangeScreen(match_->state.mode == Mode::Local && old != match_->state.currentPlayer ? Screen::Handoff : Screen::Playing);
    message_ = "Comienza el turno de " + match_->state.players[match_->state.currentPlayer].name + ". Ingreso +" + std::to_string(match_->Income(match_->state.currentPlayer)) + " monedas.";
  }
  void Game::SelectRosterUnit(int id) {
    if (!match_ || visuals_.Busy())
      return;
    const auto *unit = match_->Find(id);
    if (!unit || !unit->Alive() || unit->owner != match_->state.currentPlayer)
      return;
    selected_ = id;
    selectedCell_ = unit->pos;
    SetContext(TacticalContext::Orders);
    FocusCamera(false);
    UpdatePreview({});
    message_ = unit->readyRound > match_->state.round ? "REFUERZO: disponible en la próxima ronda." : match_->Available(*unit) ? "Tropa disponible: mueve, actúa y termina su activación."
                                                                                                                               : "ACTUADA: puedes consultar sus estadísticas.";
  }
  void Game::SelectAbility(Ability ability) {
    if (visuals_.Busy())
      return;
    if (ability == Ability::Guard || ability == Ability::Pike || ability == Ability::Smoke || ability == Ability::Precise || ability == Ability::Retreat) {
      if (!EnsureActive())
        return;
      Apply(match_->UseAbility(selected_, ability, match_->Find(selected_)->pos), ability);
      SetContext(TacticalContext::Orders);
    } else {
      SetContext(TacticalContext::Target);
      ability_ = ability;
      itemSlot_ = -1;
      message_ = Name(ability) + ": selecciona un objetivo. Esc: volver a las órdenes.";
    }
  }
  void Game::Dispatch(Button b) {
    if (!b.enabled)
      return;
    if (tactical_ != TacticalContext::Purchase && (b.action == Recruit || b.action == Buy || b.action == CustomConfirm || (b.action == Battalion && b.value != 3))) {
      auto legal = CheckAction(b.action, b.value);
      if (!legal) {
        message_ = legal.message;
        audio_.Play("cancel");
        return;
      }
      pendingAction_ = b.action;
      pendingValue_ = b.value;
      purchaseReturn_ = tactical_;
      pendingLabel_ = b.label.empty() ? "Confirmar operación económica" : b.label;
      if (b.action == Battalion || b.action == CustomConfirm) {
        const std::array<std::vector<UnitClass>, 3> templates{{{UnitClass::Saber, UnitClass::Lancer, UnitClass::Berserker}, {UnitClass::Rider, UnitClass::Archer, UnitClass::Assassin}, {UnitClass::Caster, UnitClass::Saber}}};
        const auto &composition = b.action == CustomConfirm ? customBattalion_ : templates[static_cast<std::size_t>(b.value)];
        int cost = 0;
        pendingLabel_ = "";
        for (const auto type : composition) {
          if (!pendingLabel_.empty())
            pendingLabel_ += ", ";
          pendingLabel_ += Name(type);
          cost += match_->config.classes[static_cast<int>(type)].cost;
        }
        pendingLabel_ += " · " + std::to_string(cost) + " monedas";
      }
      SetContext(TacticalContext::Purchase);
      audio_.Play("open");
      return;
    }
    if (b.action == CancelExit || b.action == Back)
      audio_.Play("cancel");
    else if (b.action == StartGame || b.action == Continue || b.action == Save || b.action == CustomConfirm)
      audio_.Play("confirm");
    else if (b.action == Settings || b.action == Pause || b.action == Tab)
      audio_.Play("open");
    else if (b.action == Resume || b.action == Ready)
      audio_.Play("close");
    else
      audio_.Play("select");
    switch (b.action) {
    case Play:
      ChangeScreen(match_ && match_->state.winner < 0 ? Screen::Playing : Screen::Setup);
      break;
    case New:
      ChangeScreen(Screen::Setup);
      break;
    case Continue:
    case Load: {
      std::string error;
      auto saved = Match::Load(SaveFile, balance_, error);
      if (saved) {
        match_ = std::move(saved);
        mode_ = match_->state.mode;
        difficulty_ = match_->state.difficulty;
        scenario_ = match_->state.scenario == "Cienaga del ultimo juramento" ? 1 : 0;
        for (int i = 0; i < 3; ++i) {
          // Reanuda el mapa incluido en el guardado aunque su plantilla de escenario haya cambiado o desaparecido.
          try {
            auto preview = Match::Create(balance_, "Content/Plaguefront/scenarios/" + std::array<std::string, 3>{"frontier.json", "marsh.json", "highlands.json"}[i], mode_, difficulty_);
            if (preview.state.scenario == match_->state.scenario)
              scenario_ = i;
          } catch (const std::exception &) {
          }
        }
        for (auto &army : armies_)
          army.clear();
        for (const auto &unit : match_->state.units)
          if (unit.readyRound == 1 && armies_[unit.owner].size() < 5)
            armies_[unit.owner].push_back(unit.type);
        restartSetup_ = CampaignSetup{mode_, difficulty_, scenario_, armies_};
        ResetPresentation();
        selected_ = -1;
        selectedCell_.reset();
        ChangeScreen(Screen::Playing);
        message_ = "Partida restaurada.";
      } else {
        message_ = error;
        RefreshContinue();
      }
      break;
    }
    case Settings:
      previous_ = screen_;
      ChangeScreen(Screen::Settings);
      break;
    case Credits:
      ChangeScreen(Screen::Credits);
      break;
    case Exit:
      OnCloseRequest();
      break;
    case ConfirmExit:
      ctx.scenes.RequestQuit();
      break;
    case CancelExit:
      ChangeScreen(exitPrevious_);
      break;
    case Back:
      ChangeScreen(screen_ == Screen::Controls ? Screen::Settings : screen_ == Screen::Settings ? previous_
                                                                                                : Screen::Menu);
      break;
    case Controls:
      ChangeScreen(Screen::Controls);
      break;
    case ControlProfile:
    case ControlDeadzone:
    case ControlSensitivity:
    case ControlInvertX:
    case ControlInvertY:
    case ControlRepeat:
    case ControlMapping:
    case ControlReset:
      AdjustControls(b.action, b.value);
      break;
    case MaximizeWindow:
      if (ctx.window.IsMaximized())
        ctx.window.Restore();
      else
        ctx.window.Maximize();
      break;
    case StartGame:
      BeginGame();
      break;
    case ModeSelect:
      mode_ = static_cast<Mode>(b.value);
      break;
    case DifficultySelect:
      difficulty_ = static_cast<Difficulty>(b.value);
      break;
    case ScenarioSelect:
      scenario_ = b.value;
      LoadArmies();
      break;
    case ArmySelect: {
      int team = b.value / 10, slot = b.value % 10;
      if (slot < static_cast<int>(armies_[team].size()))
        armies_[team][slot] = static_cast<UnitClass>((static_cast<int>(armies_[team][slot]) + 1) % 7);
      break;
    }
    case Resume:
    case Ready:
      ChangeScreen(Screen::Playing);
      break;
    case Pause:
      ChangeScreen(Screen::Paused);
      break;
    case Restart:
      BeginGame(true);
      break;
    case MainMenu:
      RefreshContinue();
      ChangeScreen(Screen::Menu);
      break;
    case Save:
      if (match_) {
        std::string error;
        canContinue_ = match_->Save(SaveFile, error);
        message_ = canContinue_ ? "Partida guardada localmente." : error;
      }
      break;
    case EndTurn:
      FinishTurn();
      break;
    case ArmyEnd:
      FinishArmyTurn();
      break;
    case ConfirmArmyEnd:
      FinishArmyTurn(true);
      break;
    case CancelArmyEnd:
      ChangeScreen(Screen::Playing);
      break;
    case RosterSelect:
      SelectRosterUnit(b.value);
      break;
    case RosterScroll:
      if (match_) {
        int count = static_cast<int>(std::ranges::count_if(match_->state.units, [&](const Unit &u) { return u.Alive() && u.owner == match_->state.currentPlayer; }));
        rosterScroll_ = std::clamp(rosterScroll_ + b.value, 0, std::max(0, count - RosterVisibleRows()));
      }
      break;
    case AbilitySelect:
      SelectAbility(static_cast<Ability>(b.value));
      break;
    case Capture:
      if (EnsureActive())
        Apply(match_->Capture(selected_));
      break;
    case Build:
      if (EnsureActive())
        Apply(match_->BuildOutpost(selected_));
      break;
    case Recruit:
      if (selectedCell_)
        Apply(match_->Recruit(match_->state.currentPlayer, static_cast<UnitClass>(b.value), *selectedCell_));
      break;
    case Battalion:
      if (selectedCell_) {
        if (b.value == 3) {
          customBattalionEditing_ = true;
          customBattalion_.clear();
          break;
        }
        std::vector<UnitClass> classes;
        if (b.value == 0)
          classes = {UnitClass::Saber, UnitClass::Lancer, UnitClass::Berserker};
        else if (b.value == 1)
          classes = {UnitClass::Rider, UnitClass::Archer, UnitClass::Assassin};
        else if (b.value == 2)
          classes = {UnitClass::Caster, UnitClass::Saber};
        else
          classes = armies_[match_->state.currentPlayer];
        Apply(match_->RecruitBattalion(match_->state.currentPlayer, classes, *selectedCell_));
      }
      break;
    case Buy:
      if (selected_ >= 0)
        Apply(match_->BuyItem(selected_, static_cast<Item>(b.value)));
      break;
    case UseItem:
      if (match_ && match_->Find(selected_)) {
        auto u = match_->Find(selected_);
        if (b.value >= 0 && b.value < static_cast<int>(u->inventory.size())) {
          const auto item = u->inventory[b.value];
          SetContext(TacticalContext::Target);
          itemSlot_ = b.value;
          ability_.reset();
          message_ = item == Item::Purifier ? "Kit purificador: selecciona una fuente cercana." : Name(item) + ": selecciona la propia unidad o un aliado adyacente. Esc: cancelar.";
        }
        break;
      }
    case Tab:
      tab_ = b.value;
      SetContext(b.value == 1 ? TacticalContext::Recruitment : b.value == 2 ? TacticalContext::Shop
                                                                            : TacticalContext::Orders);
      break;
    case OpenMove: {
      auto legal = CheckAction(OpenMove);
      if (legal)
        SetContext(TacticalContext::Move);
      else
        message_ = legal.message;
      break;
    }
    case OpenAbilities:
    case OpenInventory:
      if (match_ && match_->Find(selected_) && match_->Find(selected_)->Alive() && match_->Find(selected_)->owner == match_->state.currentPlayer)
        SetContext(b.action == OpenAbilities ? TacticalContext::Abilities : TacticalContext::Inventory);
      break;
    case OpenBuilding: {
      const Cell *building = selectedCell_ ? match_->At(*selectedCell_) : nullptr;
      auto ownBase = [&](const Cell *cell) { return cell && cell->owner == match_->state.currentPlayer && (cell->building == Building::Fortress || cell->building == Building::Outpost); };
      if (!ownBase(building)) {
        building = nullptr;
        if (const auto *unit = match_->Find(selected_))
          for (const auto &cell : match_->state.cells)
            if (ownBase(&cell) && Distance(cell.pos, unit->pos) <= 1) {
              building = &cell;
              break;
            }
      }
      if (building) {
        selectedCell_ = building->pos;
        SetContext(TacticalContext::Building);
      } else
        message_ = "Selecciona una fortaleza o fuerte propio.";
      break;
    }
    case ContextBack:
      CancelContext();
      break;
    case OpenConstruction:
      SetContext(TacticalContext::Construction);
      break;
    case ConfirmBuild:
      if (EnsureActive()) {
        Apply(match_->BuildOutpost(selected_));
        SetContext(TacticalContext::Orders);
      }
      break;
    case ConfirmPurchase: {
      const auto parent = purchaseReturn_;
      Dispatch({{}, pendingLabel_, pendingAction_, pendingValue_, true});
      SetContext(parent);
      break;
    }
    case ShopUnit:
      selected_ = b.value;
      break;
    case CenterCamera:
      FocusCamera(false);
      break;
    case FrameMap:
      FocusCamera(true);
      break;
    case Fullscreen:
      ctx.window.ToggleFullscreen();
      SaveSettings();
      displayRevision_ = ctx.window.Revision();
      break;
    case Resolution: {
      ToggleResolutionSelect();
      break;
    }
    case ResolutionSelect: {
      if (resolutionOpen_ && resolutionFullscreen_ == ctx.window.IsFullscreen() && b.value >= 0 && b.value < static_cast<int>(resolutionOptions_.size())) {
        const auto size = resolutionOptions_[b.value];
        const auto &available = ctx.window.Resolutions();
        const bool valid = !ctx.window.IsFullscreen() || std::ranges::find(available, size) != available.end();
        CloseResolutionSelect();
        if (!valid)
          break;
        ctx.window.SetResolution(size);
        SaveSettings();
        displayRevision_ = ctx.window.Revision();
      }
      break;
    }
    case ResolutionScroll:
      if (resolutionOpen_)
        MoveResolutionSelect(b.value);
      break;
    case CustomAdd:
      if (customBattalion_.size() < 7 && b.value >= 0 && b.value < 7)
        customBattalion_.push_back(static_cast<UnitClass>(b.value));
      break;
    case CustomClear:
      customBattalion_.clear();
      break;
    case CustomBack:
      customBattalionEditing_ = false;
      break;
    case CustomConfirm:
      if (selectedCell_) {
        auto result = match_->RecruitBattalion(match_->state.currentPlayer, customBattalion_, *selectedCell_);
        Apply(result);
        if (result)
          customBattalionEditing_ = false;
      }
      break;
    case Grid:
      grid_ = !grid_;
      SaveSettings();
      break;
    case Effects:
      effects_ = !effects_;
      SaveSettings();
      break;
    case Speed:
      aiDelay_ = aiDelay_ > 0.4f ? 0.2f : 0.7f;
      SaveSettings();
      break;
    case Volume:
      volume_ = volume_ >= 100 ? 0 : std::min(100, volume_ + 20);
      audio_.SetVolumes(static_cast<float>(volume_), static_cast<float>(musicVolume_), static_cast<float>(soundVolume_), muted_);
      SaveSettings();
      break;
    case MusicVolume:
      musicVolume_ = musicVolume_ >= 100 ? 0 : std::min(100, musicVolume_ + 20);
      audio_.SetVolumes(static_cast<float>(volume_), static_cast<float>(musicVolume_), static_cast<float>(soundVolume_), muted_);
      SaveSettings();
      break;
    case SoundVolume:
      soundVolume_ = soundVolume_ >= 100 ? 0 : std::min(100, soundVolume_ + 20);
      audio_.SetVolumes(static_cast<float>(volume_), static_cast<float>(musicVolume_), static_cast<float>(soundVolume_), muted_);
      SaveSettings();
      break;
    case Mute:
      muted_ = !muted_;
      audio_.SetVolumes(static_cast<float>(volume_), static_cast<float>(musicVolume_), static_cast<float>(soundVolume_), muted_);
      SaveSettings();
      break;
    }
  }

  void Game::HandleCell(Hex cell) {
    if (!match_ || !match_->At(cell))
      return;
    if (match_->state.mode == Mode::VersusAI && match_->state.currentPlayer == 1)
      return;
    if (visuals_.Busy())
      return;
    const auto legalTarget = [&](std::optional<Ability> skill, int item) {
      Match test = *match_;
      if (test.state.activeUnit != selected_) {
        auto start = test.BeginActivation(selected_);
        if (!start)
          return start;
      }
      return skill ? test.UseAbility(selected_, *skill, cell) : test.UseItem(selected_, item, cell);
    };
    if (itemSlot_ >= 0) {
      const auto legal = legalTarget({}, itemSlot_);
      if (!legal) {
        message_ = legal.message;
        audio_.Play("cancel");
        return;
      }
      if (EnsureActive()) {
        auto result = match_->UseItem(selected_, itemSlot_, cell);
        Apply(result);
        if (result)
          SetContext(TacticalContext::Orders);
      }
      return;
    }
    if (ability_) {
      const auto legal = legalTarget(ability_, -1);
      if (!legal) {
        message_ = legal.message;
        audio_.Play("cancel");
        return;
      }
      if (EnsureActive()) {
        auto result = match_->UseAbility(selected_, *ability_, cell);
        Apply(result, *ability_);
        if (result)
          SetContext(TacticalContext::Orders);
      }
      return;
    }
    auto occupant = match_->Occupant(cell);
    auto selected = match_->Find(selected_);
    const bool unitContext = tactical_ == TacticalContext::Orders || tactical_ == TacticalContext::Move;
    if (unitContext && occupant && selected && selected->owner == match_->state.currentPlayer && occupant->owner != selected->owner) {
      const auto legal = legalTarget(Ability::Attack, -1);
      if (!legal) {
        message_ = legal.message;
        audio_.Play("cancel");
        return;
      }
      if (EnsureActive()) {
        Apply(match_->UseAbility(selected_, Ability::Attack, cell), Ability::Attack);
        SetContext(TacticalContext::Orders);
      }
    } else if (occupant && occupant->owner == match_->state.currentPlayer) {
      selected_ = occupant->id;
      selectedCell_ = cell;
      SetContext(TacticalContext::Orders);
      UpdatePreview({});
    } else if (unitContext && selected && selected->owner == match_->state.currentPlayer && match_->Reachable(selected_).contains(cell)) {
      if (EnsureActive()) {
        auto result = match_->Move(selected_, cell);
        Apply(result);
        if (result) {
          selectedCell_ = cell;
          SetContext(TacticalContext::Orders);
        }
      }
    } else {
      selectedCell_ = cell;
      const auto *terrain = match_->At(cell);
      if (terrain->owner == match_->state.currentPlayer && (terrain->building == Building::Fortress || terrain->building == Building::Outpost) && !occupant)
        SetContext(TacticalContext::Building);
      else if (!unitContext) {
        selected_ = occupant ? occupant->id : -1;
        SetContext(TacticalContext::Selection);
      }
    }
  }
  void Game::OnEvent(const sf::Event &event) {
    if (ctx.input.Device() == Input::Device::KeyboardMouse)
      ctx.cursor.SetVirtualPosition({});
    if (layoutRevision_ != ctx.renderer.Revision())
      RefreshLayout();
    if (HandleResolutionEvent(event))
      return;
    if (const auto *moved = event.getIf<sf::Event::MouseMoved>(); moved && screen_ == Screen::Playing) {
      const auto point = ctx.renderer.PixelToLogical(moved->position);
      hovered_ = PickCell(point);
      UpdatePreview(hovered_);
      UpdateCursor(point);
    }
    if (const auto *wheel = event.getIf<sf::Event::MouseWheelScrolled>(); wheel && screen_ == Screen::Playing) {
      auto cursor = ctx.renderer.PixelToLogical(wheel->position);
      if (sf::FloatRect({16, 88}, {UI::RosterWidth, UiHeight() - 200.f}).contains(cursor) && wheel->delta != 0) {
        Dispatch({{}, "", RosterScroll, wheel->delta > 0 ? -1 : 1});
        return;
      }
      if (BoardBounds().contains(cursor) && wheel->delta != 0)
        camera_.SetZoom(camera_.Zoom() + (wheel->delta > 0 ? 1 : -1), {(cursor.x - BoardOrigin.x) / WorldScale(), (cursor.y - BoardOrigin.y) / WorldScale()});
    }
    if (const auto *released = event.getIf<sf::Event::MouseButtonReleased>(); released && released->button == sf::Mouse::Button::Middle) {
      dragging_ = false;
      dragRemainder_ = {};
    }
    if (event.is<sf::Event::MouseMoved>() && dragging_ && screen_ == Screen::Playing) {
      const auto cursor = ctx.renderer.PixelToLogical(event.getIf<sf::Event::MouseMoved>()->position);
      const sf::Vector2f delta{(cursor.x - dragPosition_.x) / WorldScale() + dragRemainder_.x, (cursor.y - dragPosition_.y) / WorldScale() + dragRemainder_.y};
      const sf::Vector2f integral{std::trunc(delta.x), std::trunc(delta.y)};
      camera_.PanBy({integral.x, integral.y});
      dragRemainder_ = delta - integral;
      dragPosition_ = cursor;
    }
    if (const auto *key = event.getIf<sf::Event::KeyPressed>()) {
      if (key->code == sf::Keyboard::Key::Escape) {
        if (screen_ == Screen::Playing) {
          if (tactical_ != TacticalContext::Selection)
            CancelContext();
          else
            ChangeScreen(Screen::Paused);
        } else if (screen_ == Screen::Paused)
          ChangeScreen(Screen::Playing);
        else if (screen_ == Screen::ConfirmExit)
          ChangeScreen(exitPrevious_);
        else if (screen_ == Screen::ConfirmTurn)
          ChangeScreen(Screen::Playing);
        else if (screen_ == Screen::Controls)
          ChangeScreen(Screen::Settings);
        else
          ChangeScreen(Screen::Menu);
      } else if (key->code == sf::Keyboard::Key::Tab || key->code == sf::Keyboard::Key::Up || key->code == sf::Keyboard::Key::Down) {
        if (!buttons_.empty())
          for (std::size_t n = 0; n < buttons_.size(); ++n) {
            const int direction = key->code == sf::Keyboard::Key::Up ? -1 : 1;
            keyboardFocus_ = (keyboardFocus_ + direction + static_cast<int>(buttons_.size())) % static_cast<int>(buttons_.size());
            if (buttons_[keyboardFocus_].enabled)
              break;
          }
      } else if (key->code == sf::Keyboard::Key::Enter && !key->alt && keyboardFocus_ >= 0 && keyboardFocus_ < static_cast<int>(buttons_.size())) {
        auto button = buttons_[keyboardFocus_];
        Dispatch(button);
      } else if (screen_ == Screen::ConfirmTurn && key->code == sf::Keyboard::Key::Enter && !key->alt)
        FinishArmyTurn(true);
      else if (screen_ == Screen::Handoff && key->code == sf::Keyboard::Key::Enter && !key->alt)
        ChangeScreen(Screen::Playing);
      else if (screen_ == Screen::Playing && tactical_ == TacticalContext::Purchase && key->code == sf::Keyboard::Key::Enter && !key->alt)
        Dispatch({{}, "", ConfirmPurchase});
      else if (screen_ == Screen::Playing) {
        if (key->code == sf::Keyboard::Key::F)
          FinishArmyTurn();
        if (key->code == sf::Keyboard::Key::Home)
          FocusCamera(false);
        if (key->code == sf::Keyboard::Key::V)
          FocusCamera(true);
        if (key->code == sf::Keyboard::Key::Left || key->code == sf::Keyboard::Key::A)
          camera_.PanBy({12, 0});
        if (key->code == sf::Keyboard::Key::Right || key->code == sf::Keyboard::Key::D)
          camera_.PanBy({-12, 0});
        if (key->code == sf::Keyboard::Key::W)
          camera_.PanBy({0, 12});
        if (key->code == sf::Keyboard::Key::S)
          camera_.PanBy({0, -12});
        if (key->code == sf::Keyboard::Key::Enter && !key->alt && previewPath_ && !previewPath_->cells.empty())
          HandleCell(previewPath_->cells.back());
        if (key->code == sf::Keyboard::Key::M)
          Dispatch({{}, "", OpenMove});
        if (key->code == sf::Keyboard::Key::I)
          Dispatch({{}, "", OpenInventory});
        if (key->code == sf::Keyboard::Key::T && CheckAction(AbilitySelect, static_cast<int>(Ability::Attack)))
          Dispatch({{}, "", AbilitySelect, static_cast<int>(Ability::Attack)});
        if (key->code == sf::Keyboard::Key::H)
          Dispatch({{}, "", OpenAbilities});
        if (key->code == sf::Keyboard::Key::E || key->code == sf::Keyboard::Key::Space)
          FinishTurn();
        if (key->code == sf::Keyboard::Key::F5)
          Dispatch({{}, "", Save});
        if (key->code == sf::Keyboard::Key::F9)
          Dispatch({{}, "", Load});
      }
    }
    if (const auto *mouse = event.getIf<sf::Event::MouseButtonPressed>()) {
      const auto eventPoint = ctx.renderer.PixelToLogical(mouse->position);
      if (mouse->button == sf::Mouse::Button::Middle && screen_ == Screen::Playing && BoardBounds().contains(eventPoint)) {
        dragging_ = true;
        dragPosition_ = eventPoint;
        dragRemainder_ = {};
        return;
      }
      if (mouse->button == sf::Mouse::Button::Right) {
        if (screen_ == Screen::Playing)
          CancelContext();
        return;
      }
      if (mouse->button != sf::Mouse::Button::Left)
        return;
      const auto pos = eventPoint;
      keyboardFocus_ = -1;
      for (const auto &button : buttons_)
        if (button.bounds.contains(pos)) {
          auto copy = button;
          Dispatch(copy);
          return;
        }
      if (screen_ == Screen::Playing)
        if (auto cell = PickCell(pos))
          HandleCell(*cell);
    }
  }
  void Game::Update() {
    if (resolutionOpen_ && resolutionFullscreen_ != ctx.window.IsFullscreen())
      CloseResolutionSelect();
    RefreshLayout();
    if (displayRevision_ != ctx.window.Revision()) {
      displayRevision_ = ctx.window.Revision();
      if (!smoke_)
        SaveSettings();
    }
    float dt = ctx.time.GetDeltaTime();
    animationTime_ += dt;
    audio_.Update(dt);
    if (screen_ == Screen::Playing || screen_ == Screen::Victory)
      visuals_.Update(dt);
    auto cursor = ctx.cursor.GetPosition();
    int hoveredButton = -1;
    for (std::size_t i = 0; i < buttons_.size(); ++i)
      if (buttons_[i].enabled && buttons_[i].bounds.contains(cursor)) {
        hoveredButton = static_cast<int>(i);
        break;
      }
    if (hoveredButton >= 0 && hoveredButton != hoverButton_)
      audio_.Play("hover");
    hoverButton_ = hoveredButton;
    hovered_.reset();
    if (match_ && screen_ == Screen::Playing && BoardBounds().contains(cursor)) {
      hovered_ = PickCell(cursor);
    }
    if (match_)
      UpdatePreview(screen_ == Screen::Playing ? hovered_ : std::optional<Hex>{});
    UpdateGamepad(dt);
    UpdateCursor();
    if (match_ && screen_ == Screen::Playing) {
      if (match_->state.winner >= 0) {
        if (!visuals_.Busy())
          ChangeScreen(Screen::Victory);
        return;
      }
      if (match_->state.mode == Mode::VersusAI && match_->state.currentPlayer == 1) {
        aiElapsed_ += dt;
        if (aiElapsed_ >= aiDelay_ && !visuals_.Busy()) {
          aiElapsed_ = 0;
          match_->StepAI();
          SyncPresentation();
          if (match_->state.winner >= 0 && !visuals_.Busy())
            ChangeScreen(Screen::Victory);
        }
      }
    }
  }

} // Espacio de nombres Plaguefront
