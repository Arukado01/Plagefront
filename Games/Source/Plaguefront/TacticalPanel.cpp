#include "Plaguefront/Game.h"
#include "Core/EngineConfig.h"

#include <algorithm>
#include <array>
#include <string>

namespace Plaguefront {
  namespace {
    const sf::Color Panel{27, 31, 41}, Edge{113, 108, 91}, Gold{232, 202, 137}, Muted{202, 210, 207}, White{236, 239, 229};
    const std::array<sf::Color, 2> Teams{sf::Color{128, 201, 238}, sf::Color{239, 118, 135}};
    const std::array<std::string, 16> SkillSummary{
        "Ataque normal dentro del alcance.", "Reduce el próximo golpe físico.",
        "Prepara daño extra para el ataque.", "Marca y potencia los ataques aliados.",
        "Disparo potente; impide moverse después.", "Postura de picas: aumenta la defensa.",
        "Estocada en línea, alcance 1–2.", "Tras atacar, permite un repliegue corto.",
        "Daño mágico y llamas persistentes.", "Hielo: ralentiza la próxima ronda.",
        "Cadena de enemigos próximos, máx. configurable.", "Recupera HP de un aliado.",
        "Reduce el próximo daño recibido.", "Golpe amplio con daño adicional.",
        "Elimina una fuente a distancia 2.", "Elimina un cadáver adyacente."};
    int ManaCost(Ability skill, const Config &rules) {
      if (skill == Ability::Fire || skill == Ability::Ice)
        return 2;
      if (skill == Ability::Lightning || skill == Ability::Heal)
        return 3;
      return skill == Ability::Purify ? rules.purifyMana : 0;
    }
    std::string Position(Hex pos) { return "(" + std::to_string(pos.q) + ", " + std::to_string(pos.r) + ")"; }
    std::string BuildingName(Building building) {
      if (building == Building::Fortress)
        return "FORTALEZA";
      if (building == Building::Outpost)
        return "FUERTE AVANZADO";
      return building == Building::House ? "CASA" : "TERRENO";
    }
  } // Espacio de nombres

  void Game::RenderTacticalPanel() const {
    if (!match_)
      return;
    const auto &game = *match_;
    const auto &state = game.state;
    const auto *unit = game.Find(selected_);
    const auto *property = selectedCell_ ? game.At(*selectedCell_) : nullptr;
    const bool own = unit && unit->Alive() && unit->owner == state.currentPlayer;
    const bool ai = state.mode == Mode::VersusAI && state.currentPlayer == 1;
    const bool ready = !ai && !visuals_.Busy();
    const bool controlled = property && property->owner == state.currentPlayer &&
                            (property->building == Building::Fortress || property->building == Building::Outpost);
    const float x = UiWidth() - 350.f, left = x + 16.f, width = 302.f;
    Box({{x, 88}, {UI::OrdersWidth, UiHeight() - 160}}, Panel, Edge);
    const std::array<std::string, 11> titles{
        "SELECCIÓN TÁCTICA", "ÓRDENES DE UNIDAD", "ELEGIR DESTINO", "HABILIDADES", "ELEGIR OBJETIVO",
        "INVENTARIO", "ADMINISTRAR EDIFICIO", "RECLUTAMIENTO", "TIENDA", "CONSTRUCCIÓN", "CONFIRMAR COMPRA"};
    Text(ai ? "TURNO DE LA IA" : titles[static_cast<int>(tactical_)], left, 108, 19, Gold);
    if (ai) {
      Wrapped("El enemigo está eligiendo su próxima orden. Puedes consultar el terreno y mover la cámara.", left, 161, width, 18, White);
      Text("F11 · pantalla completa", left, 760, 16, Muted);
      return;
    }
    auto back = [&](const std::string &label = "VOLVER  ·  ESC") { ButtonAt(label, left, UiHeight() - 130, width, 40, ContextBack); };
    auto action = [&](const std::string &label, float y, int command, int value = 0, float height = 40.f) {
      const auto legal = CheckAction(command, value);
      ButtonAt(label, left, y, width, height, command, value, legal.ok);
      return legal;
    };
    auto unitHeading = [&]() {
      if (!unit || !unit->Alive())
        return;
      const auto stats = game.EffectiveStats(*unit);
      Text(Name(unit->type), left, 146, 26, Teams[unit->owner]);
      std::string vitality = "HP " + std::to_string(unit->hp) + " / " + std::to_string(stats.hp);
      if (unit->type == UnitClass::Caster)
        vitality += "     MP " + std::to_string(unit->mp) + " / " + std::to_string(game.config.maxMana);
      Text(vitality, left, 187, 19, White);
      Text("ATQ " + std::to_string(stats.attack) + "    DEF " + std::to_string(stats.defense) + "    MOV " + std::to_string(stats.movement), left, 219, 17, White);
      Text("MAG " + std::to_string(stats.magic) + "    RES " + std::to_string(stats.resistance) + "    ALC " + std::to_string(stats.minRange) + "–" + std::to_string(stats.maxRange), left, 248, 17, Muted);
    };
    auto terrain = [&](float y) {
      const auto *cell = hovered_ ? game.At(*hovered_) : property;
      if (!cell)
        return;
      Box({{left - 2, y}, {width + 4, 88}}, sf::Color{19, 23, 31}, Edge);
      Text(Name(cell->terrain) + " · altura " + std::to_string(cell->height), left + 10, y + 8, 17, Gold);
      std::string detail = Position(cell->pos);
      const int damage = game.RotDamageAt(cell->pos);
      if (damage > 0)
        detail += " · Putrefacción " + std::to_string(damage) + " HP";
      Text(detail, left + 10, y + 35, 16, damage > 0 ? gConfig.rotPalette.uiText : Muted);
      std::string modifier = "Coste base " + std::to_string(game.config.terrainCosts[static_cast<int>(cell->terrain)]) + " MOV";
      if (cell->terrain == Terrain::Blocked || cell->terrain == Terrain::DeepWater)
        modifier = "Terreno inaccesible";
      else if (cell->terrain == Terrain::Forest)
        modifier += " · DEF +" + std::to_string(game.config.forestDefense);
      else if (cell->terrain == Terrain::Ruins)
        modifier += " · DEF +" + std::to_string(game.config.ruinsDefense);
      else if (cell->terrain == Terrain::Swamp)
        modifier += " · barro";
      Text(modifier, left + 10, y + 60, 16, Muted);
    };

    if (tactical_ == TacticalContext::Orders) {
      unitHeading();
      if (!own) {
        Wrapped("Selecciona una unidad de tu ejército para dar órdenes.", left, 300, width, 18, White);
        back();
        return;
      }
      const bool completed = unit->activated && state.activeUnit != unit->id;
      Text(completed ? "ACTUADA · activación terminada" : unit->readyRound > state.round ? "Disponible en la próxima ronda"
                                                                                         : "Movimiento: " + std::string(unit->moved ? "usado" : "listo") + " · Acción: " + std::string(unit->acted ? "usada" : "lista"),
           left, 285, 16, Muted);
      float y = 321;
      const auto move = action("MOVER  ·  M", y, OpenMove);
      y += 46;
      action("ATACAR  ·  T", y, AbilitySelect, static_cast<int>(Ability::Attack));
      y += 46;
      ButtonAt("HABILIDADES  ·  H", left, y, width, 40, OpenAbilities, 0, ready && !completed && unit->readyRound <= state.round);
      y += 46;
      ButtonAt("INVENTARIO  ·  I", left, y, width, 40, OpenInventory, 0, ready);
      y += 46;
      if (CheckAction(Capture).ok) {
        const auto *c = game.At(unit->pos);
        action(c && c->captureProgress > 0 ? "CONTINUAR CAPTURA" : "CAPTURAR", y, Capture);
        y += 46;
      }
      if (CheckAction(Build).ok) {
        ButtonAt("CONSTRUCCIÓN", left, y, width, 40, OpenConstruction, 0, ready);
        y += 46;
      }
      const bool nearby = std::ranges::any_of(state.cells, [&](const Cell &cell) { return cell.owner == state.currentPlayer &&
                                                                                          (cell.building == Building::Fortress || cell.building == Building::Outpost) && Distance(cell.pos, unit->pos) <= 1; });
      if (nearby) {
        ButtonAt("ADMINISTRAR EDIFICIO", left, y, width, 40, OpenBuilding, 0, ready);
        y += 46;
      }
      if (!move && y < 690)
        Wrapped(move.message, left, y, width, 16, Muted);
      ButtonAt(state.activeUnit == unit->id ? "TERMINAR  ·  E" : "ESPERAR  ·  E", left, UiHeight() - 182, width, 40, EndTurn, 0, ready && !completed && unit->readyRound <= state.round);
      back("DESELECCIONAR  ·  ESC");
      return;
    }
    if (tactical_ == TacticalContext::Move) {
      unitHeading();
      Wrapped("Apunta a una celda iluminada. La flecha muestra la ruta real; clic confirma el movimiento.", left, 300, width, 18, White);
      if (previewPath_ && previewDestination_ && unit) {
        const auto *destination = game.At(*previewDestination_);
        const auto stats = game.EffectiveStats(*unit);
        const int budget = unit->retreatRemaining > 0 ? unit->retreatRemaining : stats.movement;
        Text("DESTINO " + Position(*previewDestination_), left, 413, 19, Gold);
        Text("Coste: " + std::to_string(previewPath_->cost) + " MOV", left, 451, 19, White);
        Text("Restante: " + std::to_string(std::max(0, budget - previewPath_->cost)) + " MOV", left, 486, 19, White);
        if (destination)
          Text("Terreno: " + Name(destination->terrain), left, 521, 17, Muted);
        bool surcharge = false;
        for (std::size_t i = 1; i < previewPath_->cells.size(); ++i)
          surcharge |= game.MoveCost(*unit, previewPath_->cells[i - 1], previewPath_->cells[i]) > 1;
        if (surcharge)
          Wrapped("El recorrido incluye terreno, subida o clima con coste adicional.", left, 565, width, 17, Gold);
      } else
        Wrapped("Elige un destino alcanzable para ver coste y MOV restante.", left, 424, width, 18, Muted);
      terrain(UiHeight() - 240);
      back("CANCELAR  ·  ESC");
      return;
    }
    if (tactical_ == TacticalContext::Abilities) {
      if (unit)
        Text(Name(unit->type), left, 146, 21, White);
      Text("Elige una habilidad", left, 174, 16, Muted);
      float y = 193;
      std::string reason;
      if (unit)
        for (const auto skill : game.Abilities(unit->id))
          if (skill != Ability::Attack) {
            const int value = static_cast<int>(skill), mana = ManaCost(skill, game.config), cooldown = unit->cooldowns[value];
            std::string label = Name(skill);
            if (mana > 0)
              label += " · " + std::to_string(mana) + " MP";
            if (cooldown > 0)
              label += " · CD " + std::to_string(cooldown);
            const auto legal = action(label, y, AbilitySelect, value, 38.f);
            Wrapped(SkillSummary[value], left + 3, y + 41, width - 6, 16, Muted);
            y += 84;
            if (!legal && reason.empty())
              reason = legal.message;
          }
      if (!reason.empty() && y < 727)
        Wrapped(reason, left, y, width, 16, Gold);
      back("ÓRDENES  ·  ESC");
      return;
    }
    if (tactical_ == TacticalContext::Target) {
      if (unit)
        Text(Name(unit->type), left, 149, 26, Teams[unit->owner]);
      const std::string command = ability_ ? Name(*ability_) : itemSlot_ >= 0 && unit && itemSlot_ < static_cast<int>(unit->inventory.size()) ? Name(unit->inventory[itemSlot_])
                                                                                                                                              : "Selecciona un objetivo";
      Text(command, left, 197, 22, Gold);
      Wrapped("Haz clic en un objetivo válido del tablero. Los marcadores muestran el alcance legal.", left, 244, width, 18, White);
      if (hovered_)
        if (const auto *target = game.Occupant(*hovered_)) {
          Text(Name(target->type) + " · HP " + std::to_string(target->hp), left, 358, 19, Teams[target->owner]);
          if (unit && ability_ && PreviewRules().CanTarget(unit->id, target->id, *ability_))
            Text("Daño previsto: " + std::to_string(PreviewRules().EstimateDamage(unit->id, target->id, *ability_)), left, 396, 19, Gold);
        }
      Wrapped("Escape vuelve a las órdenes. Apuntar no consume movimiento, maná ni acción.", left, 481, width, 17, Muted);
      terrain(660);
      back("CANCELAR  ·  ESC");
      return;
    }
    if (tactical_ == TacticalContext::Inventory) {
      unitHeading();
      Text("OBJETOS · MÁXIMO 2", left, 303, 19, Gold);
      if (unit && unit->inventory.empty())
        Wrapped("Inventario vacío. Compra suministros desde una fortaleza o fuerte propio.", left, 348, width, 18, White);
      if (unit)
        for (int slot = 0; slot < static_cast<int>(unit->inventory.size()); ++slot) {
          const auto check = CheckAction(UseItem, slot);
          ButtonAt("USAR " + Name(unit->inventory[slot]), left, 350 + slot * 60.f, width, 40, UseItem, slot, check.ok);
          if (!check && slot == 0)
            Wrapped("Elige después un aliado o fuente que admita el efecto.", left, 486, width, 17, Muted);
        }
      Wrapped("Usar un objeto consume una acción cuando su objetivo es válido. Abrir el inventario no consume acciones.", left, 579, width, 17, Muted);
      back("ÓRDENES  ·  ESC");
      return;
    }
    if (tactical_ == TacticalContext::Building) {
      Text(property ? BuildingName(property->building) : "SIN EDIFICIO", left, 151, 25, Gold);
      if (property)
        Text(Position(property->pos), left, 191, 17, Muted);
      if (property && property->captureProgress > 0)
        Text("Captura " + std::to_string(property->captureProgress) + "/" + std::to_string(game.CaptureRequired(*property)) + " puntos", left, 275, 17, Gold);
      Wrapped(property ? (property->owner < 0 ? "Propietario: neutral" : "Propietario: " + state.players[property->owner].name) : "Selecciona una propiedad.", left, 231, width, 17, White);
      Text("MONEDAS: " + std::to_string(state.players[state.currentPlayer].coins), left, 305, 22, Gold);
      Wrapped("Administración de la propiedad. Las compras se confirman antes de descontar monedas.", left, 354, width, 18, White);
      ButtonAt("RECLUTAMIENTO", left, 463, width, 42, Tab, 1, controlled && ready);
      ButtonAt("TIENDA DE SUMINISTROS", left, 517, width, 42, Tab, 2, controlled && ready);
      Wrapped(controlled ? "Los reclutas comienzan a actuar en la próxima ronda." : "Las operaciones económicas requieren una fortaleza o fuerte propio.", left, 595, width, 17, Muted);
      back("AL TABLERO  ·  ESC");
      return;
    }
    if (tactical_ == TacticalContext::Recruitment) {
      Text(property ? BuildingName(property->building) : "SIN EDIFICIO", left, 146, 19, White);
      Text("Monedas: " + std::to_string(state.players[state.currentPlayer].coins), left, 176, 19, Gold);
      if (customBattalionEditing_) {
        Text("BATALLÓN PERSONALIZADO", left, 212, 17, Gold);
        for (int i = 0; i < 7; ++i)
          ButtonAt("+ " + Name(static_cast<UnitClass>(i)) + " · " + std::to_string(game.config.classes[i].cost), left, 247 + i * 40.f, width, 36, CustomAdd, i, customBattalion_.size() < 7 && controlled && ready);
        int total = 0;
        std::string composition;
        for (const auto type : customBattalion_) {
          total += game.config.classes[static_cast<int>(type)].cost;
          if (!composition.empty())
            composition += ", ";
          composition += Name(type);
        }
        Text(std::to_string(customBattalion_.size()) + " / 7 · " + std::to_string(total) + " monedas", left, 539, 19, Gold);
        if (composition.size() > 57)
          composition = composition.substr(0, 54) + "...";
        Wrapped(composition.empty() ? "Añade clases; se permiten repeticiones." : composition, left, 577, width, 16, White);
        const auto legal = CheckAction(CustomConfirm);
        ButtonAt("CONFIRMAR BATALLÓN", left, 662, width, 40, CustomConfirm, 0, legal.ok && !customBattalion_.empty());
        ButtonAt("VACIAR BORRADOR", left, 712, width, 36, CustomClear);
        if (!legal && !customBattalion_.empty())
          Text("Revisa monedas y espacio.", left, 631, 16, Muted);
        back("VOLVER  ·  ESC");
        return;
      }
      for (int i = 0; i < 7; ++i)
        action(Name(static_cast<UnitClass>(i)) + " · " + std::to_string(game.config.classes[i].cost) + " monedas", 216 + i * 41.f, Recruit, i, 36.f);
      Text("BATALLONES", left, 518, 18, Gold);
      const std::array<std::string, 4> names{"Vanguardia", "Reconocimiento", "Arcano", "Personalizado"};
      const std::array<std::vector<UnitClass>, 3> battalions{{{UnitClass::Saber, UnitClass::Lancer, UnitClass::Berserker}, {UnitClass::Rider, UnitClass::Archer, UnitClass::Assassin}, {UnitClass::Caster, UnitClass::Saber}}};
      for (int i = 0; i < 4; ++i) {
        int cost = 0;
        if (i < 3)
          for (const auto type : battalions[i])
            cost += game.config.classes[static_cast<int>(type)].cost;
        const auto legal = CheckAction(Battalion, i);
        ButtonAt(names[i] + (i < 3 ? " · " + std::to_string(cost) : ""), left, 551 + i * 43.f, width, 37, Battalion, i, legal.ok && controlled && ready);
      }
      Wrapped("Compra individual o grupo completo. Sin espacio o monedas no se cobra nada.", left, 728, width, 16, Muted);
      back();
      return;
    }
    if (tactical_ == TacticalContext::Shop) {
      Text("Monedas: " + std::to_string(state.players[state.currentPlayer].coins), left, 149, 21, Gold);
      Text("ELIGE QUIÉN COMPRA", left, 190, 17, White);
      float y = 223;
      int count = 0;
      if (property)
        for (const auto &buyer : state.units)
          if (buyer.Alive() && buyer.owner == state.currentPlayer && Distance(buyer.pos, property->pos) <= 1 && count < 5) {
            ButtonAt(std::string(selected_ == buyer.id ? "● " : "") + Name(buyer.type) + " #" + std::to_string(buyer.id) + " · " + std::to_string(buyer.inventory.size()) + "/2", left, y, width, 36, ShopUnit, buyer.id, controlled && ready);
            y += 40;
            ++count;
          }
      if (count == 0) {
        Wrapped("Acerca una unidad aliada al edificio para comprar suministros.", left, y, width, 17, Muted);
        y += 80;
      }
      y = std::max(437.f, y + 10.f);
      Text("SUMINISTROS", left, y, 18, Gold);
      y += 31;
      const bool buyerValid = controlled && own && property && Distance(unit->pos, property->pos) <= 1;
      for (int i = 0; i < 5; ++i) {
        const auto legal = CheckAction(Buy, i);
        ButtonAt(Name(static_cast<Item>(i)) + " · " + std::to_string(game.config.itemCosts[i]), left, y + i * 44.f, width, 38, Buy, i, buyerValid && legal.ok);
      }
      std::string reason = buyerValid ? CheckAction(Buy, 0).message : "Selecciona una unidad próxima al edificio.";
      if (!reason.empty())
        Wrapped(reason, left, 713, width, 16, Muted);
      back();
      return;
    }
    if (tactical_ == TacticalContext::Construction) {
      if (unit)
        Text(Name(unit->type), left, 151, 26, Teams[unit->owner]);
      Text("FUERTE AVANZADO", left, 207, 21, Gold);
      Text("Coste: " + std::to_string(game.config.outpostCost) + " monedas", left, 253, 21, White);
      Text("Monedas: " + std::to_string(state.players[state.currentPlayer].coins), left, 291, 19, Gold);
      Wrapped("Construye a distancia máxima 2 de tu fortaleza, o transforma una casa propia ocupada. Consume la acción y permite reclutar y comprar.", left, 345, width, 18, White);
      const auto legal = CheckAction(ConfirmBuild);
      if (!legal)
        Wrapped(legal.message, left, 510, width, 18, Gold);
      ButtonAt("CONSTRUIR FUERTE", left, 650, width, 42, ConfirmBuild, 0, legal.ok);
      back("CANCELAR  ·  ESC");
      return;
    }
    if (tactical_ == TacticalContext::Purchase) {
      Text("REVISAR OPERACIÓN", left, 153, 21, Gold);
      Wrapped(pendingLabel_, left, 215, width, 21, White);
      Text("Saldo: " + std::to_string(state.players[state.currentPlayer].coins) + " monedas", left, 376, 19, Gold);
      const auto legal = CheckAction(pendingAction_, pendingValue_);
      if (!legal)
        Wrapped(legal.message, left, 438, width, 18, Gold);
      Wrapped("Las monedas se descuentan solo al confirmar. Escape regresa sin comprar.", left, 522, width, 18, Muted);
      ButtonAt("CONFIRMAR  ·  ENTER", left, 662, width, 42, ConfirmPurchase, 0, legal.ok);
      back("CANCELAR  ·  ESC");
      return;
    }

    if (unit && unit->Alive()) {
      unitHeading();
      Wrapped(own ? "Selecciona esta unidad para abrir sus órdenes." : "Unidad rival. Selecciona una unidad propia antes de elegir un ataque.", left, 305, width, 18, White);
    } else
      Wrapped("Selecciona una unidad aliada para mover y actuar. Selecciona una fortaleza propia para administrar la propiedad.", left, 159, width, 19, White);
    if (property && property->building != Building::None) {
      Text(BuildingName(property->building), left, 438, 20, Gold);
      Wrapped(property->owner < 0 ? "Propietario: neutral" : "Propietario: " + state.players[property->owner].name, left, 477, width, 17, White);
      if (controlled)
        ButtonAt("ADMINISTRAR EDIFICIO", left, 552, width, 42, OpenBuilding, 0, ready);
      else
        Wrapped("Ocupa el edificio con una unidad y usa Capturar desde sus órdenes.", left, 548, width, 17, Muted);
    }
    terrain(660);
    Wrapped("E: termina una tropa. F: fin del ejército. Selecciona desde la lista izquierda.", left, UiHeight() - 146, width, 16, Muted);
  }
} // Espacio de nombres Plaguefront
