#include "Plaguefront/Game.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <set>
#include <stdexcept>

namespace Plaguefront {
  void Game::VerifyPhase4Controls() {
    int checks = 0;
    auto require = [&](bool condition, const std::string &message) {
      ++checks;
      if (!condition)
        throw std::runtime_error("Phase4 controls: " + message);
    };
    auto valid = [&]() {std::string error;const bool ok=match_->Validate(error);require(ok,"invalid fixture: "+error); };
    auto key = [&](sf::Keyboard::Key code) {sf::Event::KeyPressed event{};event.code=code;OnEvent(sf::Event(event)); };
    auto movePointer = [&](sf::Vector2f point) {
      ctx.cursor.SetPosition(point);
      sf::Event::MouseMoved event{};
      event.position = ctx.renderer.LogicalToPixel(point);
      OnEvent(sf::Event(event));
      return ctx.renderer.PixelToLogical(event.position);
    };
    auto click = [&](const Button &button) {
      const auto point = button.bounds.position + button.bounds.size * .5f;
      ctx.cursor.SetPosition(point);
      sf::Event::MouseButtonPressed event{};
      event.button = sf::Mouse::Button::Left;
      event.position = ctx.renderer.LogicalToPixel(point);
      require(button.bounds.contains(ctx.renderer.PixelToLogical(event.position)), "output-event coordinates do not reach requested button " + button.label);
      OnEvent(sf::Event(event));
    };
    auto rows = [&]() {
      buttons_.clear();
      RenderRoster();
      std::vector<Button> result;
      for (const auto &button : buttons_)
        if (button.action == RosterSelect)
          result.push_back(button);
      return result;
    };
    auto scrollButton = [&](int direction) {
      buttons_.clear();
      RenderRoster();
      const auto found = std::ranges::find_if(buttons_, [&](const Button &button) { return button.action == RosterScroll && button.value == direction; });
      require(found != buttons_.end(), "missing roster page arrow");
      return *found;
    };
    auto wheel = [&](float delta) {
      ctx.cursor.SetPosition({110, 350});
      sf::Event::MouseWheelScrolled event{};
      event.delta = delta;
      event.position = ctx.renderer.LogicalToPixel({110, 350});
      require(sf::FloatRect({16, 88}, {212, 700}).contains(ctx.renderer.PixelToLogical(event.position)), "wheel event lies outside roster");
      OnEvent(sf::Event(event));
    };
    auto select = [&](int id) {
      Dispatch({{}, "", RosterSelect, id});
      require(selected_ == id && tactical_ == TacticalContext::Orders, "controller failed roster selection");
    };
    auto moveSelected = [&]() {
      Dispatch({{}, "", OpenMove});
      require(tactical_ == TacticalContext::Move, "move context unavailable for fresh fixture unit");
      UpdatePreview({});
      const auto destination = std::ranges::find_if(reachable_, [&](const auto &entry) { return entry.first != match_->Find(selected_)->pos; });
      require(destination != reachable_.end(), "fixture unit lacks a legal movement destination");
      const Hex target = destination->first;
      HandleCell(target);
      require(match_->Find(selected_)->pos == target && match_->Find(selected_)->moved, "controller did not execute legal preview destination");
      visuals_.Update(5.f);
    };

    // Las catorce tropas utilizan el terreno del escenario y sus estadísticas reales.
    scenario_ = 2;
    mode_ = Mode::Local;
    LoadArmies();
    BeginGame();
    ChangeScreen(Screen::Playing);
    rosterScroll_ = 0;
    for (int extra = 0; extra < 9; ++extra) {
      const auto spot = std::ranges::find_if(match_->state.cells, [&](const Cell &cell) { return cell.building == Building::None && cell.terrain != Terrain::Blocked && cell.terrain != Terrain::DeepWater && !match_->Occupant(cell.pos); });
      require(spot != match_->state.cells.end(), "large scenario cannot host expanded roster");
      const auto type = static_cast<UnitClass>(extra % 7);
      const auto &stats = match_->config.classes[static_cast<std::size_t>(type)];
      Unit unit;
      unit.id = match_->state.nextId++;
      unit.owner = 0;
      unit.type = type;
      unit.pos = spot->pos;
      unit.hp = stats.hp;
      unit.mp = type == UnitClass::Caster ? match_->config.maxMana : 0;
      match_->state.units.push_back(unit);
    }
    ResetPresentation();
    valid();
    require(match_->AvailableCount() == 14, "expanded logical army must contain fourteen available troops");
    auto firstPage = rows();
    require(firstPage.size() == 9, "roster page must display nine troop rows");
    std::set<int> ids;
    for (const auto &row : firstPage) {
      require(ids.insert(row.value).second, "first page repeats a unit");
      require(row.bounds.position.x >= 16 && row.bounds.position.x + row.bounds.size.x <= 228 && row.bounds.position.y >= 174 && row.bounds.position.y + row.bounds.size.y <= 700, "roster row overlaps header scroll controls or army end button");
      require(row.enabled && match_->Available(*match_->Find(row.value)), "fresh roster row does not match logical availability");
    }
    require(!scrollButton(-1).enabled && scrollButton(1).enabled, "initial arrow availability disagrees with page bounds");
    click(scrollButton(1));
    require(rosterScroll_ == 1, "actual down-arrow click did not advance one row");
    click(scrollButton(-1));
    require(rosterScroll_ == 0, "actual up-arrow click did not restore first row");
    wheel(-1);
    require(rosterScroll_ == 1, "actual wheel event did not scroll down one row");
    for (int attempt = 0; attempt < 12; ++attempt)
      wheel(-1);
    require(rosterScroll_ == 5, "wheel overscroll must clamp stored offset to final nine-row page");
    auto lastPage = rows();
    require(lastPage.size() == 9 && !scrollButton(1).enabled && scrollButton(-1).enabled, "final page must keep nine rows and disable only down arrow");
    require(lastPage.front().value != firstPage.front().value && lastPage.back().value == match_->state.units.back().id, "final page must expose later actual troops");
    wheel(1);
    require(rosterScroll_ == 4, "one wheel-up event must leave overscrolled final page immediately");
    for (int attempt = 0; attempt < 12; ++attempt)
      wheel(1);
    require(rosterScroll_ == 0, "wheel underscroll must clamp to zero");
    firstPage = rows();
    click(firstPage.front());
    const int acted = selected_;
    require(acted == firstPage.front().value && match_->state.activeUnit == -1 && !match_->Find(acted)->activationStarted, "actual row click must inspect without beginning activation");
    FinishTurn();
    require(match_->Find(acted)->activated && match_->state.currentPlayer == 0, "unit wait must mark acted without handing over army");
    Dispatch({{}, "", RosterScroll, 1000});
    lastPage = rows();
    const auto usedRow = std::ranges::find(lastPage, acted, &Button::value);
    require(usedRow != lastPage.end() && usedRow->label == "ACTUADA", "acted troop must move to used section while remaining inspectable");
    const int moneyBeforeInspection = match_->state.players[0].coins;
    click(*usedRow);
    require(selected_ == acted && !CheckAction(OpenMove) && !CheckAction(AbilitySelect, static_cast<int>(Ability::Attack)), "used roster selection must not enable ordinary orders");
    require(match_->state.players[0].coins == moneyBeforeInspection && match_->state.activeUnit == -1, "used inspection must not mutate currency or activation");

    // Alterna dos activaciones sin terminar mediante los mismos controles de la partida interactiva.
    const auto casterIt = std::ranges::find_if(match_->state.units, [&](const Unit &unit) { return unit.owner == 0 && unit.type == UnitClass::Caster && match_->Available(unit) && match_->Reachable(unit.id).size() > 1; });
    require(casterIt != match_->state.units.end(), "interleaving fixture lacks a mobile Caster");
    const int caster = casterIt->id;
    match_->Find(caster)->mp = 2;
    match_->Find(caster)->hp -= 6;
    match_->Find(caster)->inventory = {Item::Potion};
    ResetPresentation();
    select(caster);
    moveSelected();
    require(match_->Find(caster)->mp == 3 && match_->Find(caster)->activationStarted && !match_->Find(caster)->activated, "Caster move should begin once and recover one mana");
    const auto saberIt = std::ranges::find_if(match_->state.units, [&](const Unit &unit) { return unit.owner == 0 && unit.type == UnitClass::Saber && match_->Available(unit) && match_->Reachable(unit.id).size() > 1; });
    require(saberIt != match_->state.units.end(), "interleaving fixture lacks a mobile Saber after Caster movement");
    const int saber = saberIt->id;
    select(saber);
    moveSelected();
    require(match_->state.activeUnit == saber && match_->Find(caster)->moved && match_->Find(saber)->moved, "second UI activation must preserve first unit movement");
    for (int inspection = 0; inspection < 4; ++inspection) {
      select(caster);
      select(saber);
    }
    require(match_->Find(caster)->mp == 3 && !match_->Find(caster)->activated, "repeated roster inspections cannot farm mana or finish unfinished unit");
    select(caster);
    Dispatch({{}, "", OpenInventory});
    Dispatch({{}, "", UseItem, 0});
    HandleCell(match_->Find(caster)->pos);
    visuals_.Update(5.f);
    require(match_->state.activeUnit == caster && match_->Find(caster)->acted && match_->Find(caster)->inventory.empty() && match_->Find(caster)->mp == 3, "resuming through real item controller must preserve one-time mana and consume only item action");
    require(!CheckAction(OpenMove), "resuming via inventory cannot restore movement");
    FinishTurn();
    select(saber);
    Dispatch({{}, "", AbilitySelect, static_cast<int>(Ability::Guard)});
    visuals_.Update(5.f);
    FinishTurn();
    require(match_->Find(caster)->activated && match_->Find(saber)->activated && match_->state.currentPlayer == 0, "both interleaved units must finish inside same army turn");
    valid();

    const int remaining = match_->AvailableCount(), blueCoins = match_->state.players[0].coins, redCoins = match_->state.players[1].coins, redIncome = match_->Income(1);
    keyboardFocus_ = -1;
    key(sf::Keyboard::Key::F);
    require(screen_ == Screen::ConfirmTurn && match_->AvailableCount() == remaining && remaining == 11, "keyboard army end must confirm exact remaining logical count");
    buttons_.clear();
    RenderModal();
    require(std::ranges::any_of(buttons_, [](const Button &button) { return button.action == ConfirmArmyEnd && button.enabled; }), "confirmation modal lacks active explicit army-end control");
    key(sf::Keyboard::Key::Escape);
    require(screen_ == Screen::Playing && match_->state.currentPlayer == 0 && match_->AvailableCount() == remaining && match_->state.players[0].coins == blueCoins && match_->state.players[1].coins == redCoins, "cancelling confirmation must preserve all availability and money");
    keyboardFocus_ = -1;
    key(sf::Keyboard::Key::F);
    key(sf::Keyboard::Key::Enter);
    require(screen_ == Screen::Handoff && match_->state.currentPlayer == 1 && match_->state.round == 1, "confirming keyboard army end must hand over exactly once");
    require(match_->state.players[0].coins == blueCoins && match_->state.players[1].coins == redCoins + redIncome, "confirmation pays incoming army once according to real forecast");
    require(!ability_ && itemSlot_ < 0 && !previewPath_ && reachable_.empty() && selected_ < 0 && match_->state.activeUnit < 0, "army handover must clear all pending tactical controllers");
    key(sf::Keyboard::Key::Enter);
    key(sf::Keyboard::Key::Enter);
    require(screen_ == Screen::Playing && match_->state.currentPlayer == 1 && match_->state.round == 1 && match_->state.players[1].coins == redCoins + redIncome, "repeated Enter after confirmation cannot replay turnover or income");
    valid();

    // El tablero mínimo válido aísla los seis contextos sin inventar condiciones de legalidad.
    Match fixture(balance_);
    fixture.state.mode = Mode::Local;
    fixture.config.weatherPeriod = 1000;
    for (int q = -5; q <= 5; ++q)
      for (int r = -4; r <= 4; ++r)
        fixture.state.cells.push_back({{q, r}});
    fixture.At({-5, 0})->building = Building::Fortress;
    fixture.At({-5, 0})->owner = 0;
    fixture.At({5, 0})->building = Building::Fortress;
    fixture.At({5, 0})->owner = 1;
    fixture.At({0, 0})->building = Building::House;
    auto add = [&](int owner, UnitClass type, Hex pos) {
      const auto &stats = fixture.config.classes[static_cast<std::size_t>(type)];
      Unit unit;
      unit.id = fixture.state.nextId++;
      unit.owner = owner;
      unit.type = type;
      unit.pos = pos;
      unit.hp = stats.hp;
      unit.mp = type == UnitClass::Caster ? fixture.config.maxMana : 0;
      fixture.state.units.push_back(unit);
      return unit.id;
    };
    const int actor = add(0, UnitClass::Saber, {});
    add(1, UnitClass::Saber, {1, 0});
    add(0, UnitClass::Caster, {-2, 0});
    match_ = std::move(fixture);
    ChangeScreen(Screen::Playing);
    ResetPresentation();
    selected_ = -1;
    SetContext(TacticalContext::Selection);
    FocusCamera(true);
    valid();
    auto cursorAt = [&](Hex cell, CursorContext expected) {
      buttons_.clear();
      const auto point = movePointer(Center(cell));
      const auto picked = PickCell(point);
      require(picked && *picked == cell && hovered_ == picked, "mouse motion output-event hotspot does not pick requested board cell");
      require(ctx.cursor.GetContext() == expected, "cursor context disagrees with actual legal board selection");
    };
    cursorAt({0, 1}, CursorContext::Normal);
    select(actor);
    Dispatch({{}, "", OpenMove});
    cursorAt({0, 1}, CursorContext::Move);
    Dispatch({{}, "", ContextBack});
    cursorAt({1, 0}, CursorContext::Attack);
    cursorAt({}, CursorContext::Capture);
    Dispatch({{}, "", AbilitySelect, static_cast<int>(Ability::Attack)});
    cursorAt({0, 1}, CursorContext::Blocked);
    Dispatch({{}, "", ContextBack});
    selectedCell_ = Hex{-5, 0};
    Dispatch({{}, "", OpenBuilding});
    Dispatch({{}, "", Tab, 1});
    buttons_.clear();
    RenderTacticalPanel();
    const auto recruit = std::ranges::find_if(buttons_, [](const Button &button) { return button.action == Recruit && button.enabled; });
    require(recruit != buttons_.end(), "real recruitment panel lacks affordable economic action");
    const auto economic = *recruit;
    const auto economicPoint = movePointer(economic.bounds.position + economic.bounds.size * .5f);
    require(economic.bounds.contains(economicPoint), "economic mouse motion does not map to real UI button");
    require(ctx.cursor.GetContext() == CursorContext::Economy, "real enabled recruitment button must show economic cursor");
    match_->state.players[0].coins = 0;
    buttons_.clear();
    RenderTacticalPanel();
    const auto disabled = std::ranges::find_if(buttons_, [](const Button &button) { return button.action == Recruit && !button.enabled; });
    require(disabled != buttons_.end(), "unaffordable economic action must remain represented as disabled");
    movePointer(disabled->bounds.position + disabled->bounds.size * .5f);
    require(ctx.cursor.GetContext() == CursorContext::Blocked, "real disabled economic button must show blocked cursor");
    require(!match_->Find(actor)->activationStarted && match_->state.activeUnit == -1 && match_->At({0, 0})->captureProgress == 0, "cursor legality probes cannot consume actions or begin capture");
    valid();
    std::cout << "Plaguefront phase4 controls: " << checks << " checks passed (14-unit roster, paging, interleaved UI orders, turn confirmation and six legal cursor contexts).\n";
  }
} // Espacio de nombres Plaguefront
