#include "Plaguefront/Rules.h"

#include <nlohmann/json.hpp>
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <limits>
#include <set>
#include <stdexcept>

namespace Plaguefront {
  namespace {
    using Json = nlohmann::json;
    constexpr std::size_t MaxFileBytes = 4 * 1024 * 1024;
    constexpr std::size_t MaxCells = 4096, MaxUnits = 512, MaxEffects = 4096;
#define PF_CONFIG_FIELDS(X)                                                                                                       \
  X(forestDefense)                                                                                                                \
  X(forestArcherAttack)                                                                                                           \
  X(forestRiderSurcharge) X(uphillCost) X(rangedHeightAttack) X(swampDefensePenalty) X(ruinsDefense)                              \
      X(rainMoveSurcharge) X(stormRiderPenalty) X(weatherRangePenalty) X(maxMana) X(manaRecovery) X(healAmount)                   \
          X(potionHeal) X(largePotionHeal) X(manaPotionRestore) X(advantageDamage) X(damageReductionPercent)                      \
              X(preciseBonus) X(markBonus) X(focusBonus) X(pikeBonus) X(thrustBonus) X(chargeBonus) X(flankBonus) X(furiousBonus) \
                  X(frenzyAttack) X(frenzyDefensePenalty) X(chargeSteps) X(retreatSteps)
#define PF_PHASE4_CONFIG_FIELDS(X) \
  X(houseCapture)                  \
  X(outpostCapture)                \
  X(fortressCapture) X(lightningMaxTargets) X(lightningRadius) X(fireRadius) X(fireDuration) X(fireDamage)

    Json ReadJson(const std::string &filename) {
      std::ifstream file(filename, std::ios::binary | std::ios::ate);
      if (!file)
        throw std::runtime_error("No se puede abrir " + filename);
      const auto size = file.tellg();
      if (size <= 0 || size > static_cast<std::streamoff>(MaxFileBytes))
        throw std::runtime_error("Archivo vacio o demasiado grande (maximo 4 MiB)");
      std::string bytes(static_cast<std::size_t>(size), '\0');
      file.seekg(0);
      if (!file.read(bytes.data(), static_cast<std::streamsize>(bytes.size())))
        throw std::runtime_error("Lectura incompleta");
      auto result = Json::parse(bytes, [](int depth, Json::parse_event_t, Json &) {
        if (depth > 48)
          throw std::runtime_error("JSON demasiado anidado");
        return true;
      });
      if (!result.is_object())
        throw std::runtime_error("Se esperaba un objeto JSON");
      return result;
    }

    int Number(const Json &value, int low, int high) {
      if (!value.is_number_integer() || (value.is_number_unsigned() && value.get<std::uint64_t>() > static_cast<std::uint64_t>(high)))
        throw std::runtime_error("Se esperaba un entero dentro del intervalo permitido");
      const auto number = value.get<std::int64_t>();
      if (number < low || number > high)
        throw std::runtime_error("Entero fuera del intervalo permitido");
      return static_cast<int>(number);
    }
    int Number(const Json &object, const char *key, int low, int high) { return Number(object.at(key), low, high); }
    std::string Text(const Json &value, std::size_t limit = 160) {
      if (!value.is_string())
        throw std::runtime_error("Se esperaba texto");
      auto text = value.get<std::string>();
      if (text.size() > limit || text.find('\0') != std::string::npos)
        throw std::runtime_error("Texto invalido o demasiado largo");
      return text;
    }
    void Array(const Json &value, std::size_t maximum) {
      if (!value.is_array() || value.size() > maximum)
        throw std::runtime_error("Lista invalida o demasiado grande");
    }
    Hex ReadHex(const Json &value) {
      Array(value, 2);
      if (value.size() != 2)
        throw std::runtime_error("Coordenada incompleta");
      return {Number(value[0], -10000, 10000), Number(value[1], -10000, 10000)};
    }
    Json WriteHex(Hex h) { return Json::array({h.q, h.r}); }
    template <class Enum>
    Enum EnumNumber(const Json &value, int maximum) { return static_cast<Enum>(Number(value, 0, maximum)); }
    UnitClass ClassName(const Json &value) {
      const auto name = Text(value);
      constexpr const char *names[] = {"Saber", "Archer", "Lancer", "Rider", "Caster", "Assassin", "Berserker"};
      for (int i = 0; i < 7; ++i)
        if (name == names[i])
          return static_cast<UnitClass>(i);
      throw std::runtime_error("Clase desconocida: " + name);
    }
    Terrain TerrainName(const Json &value) {
      const auto name = Text(value);
      constexpr const char *names[] = {"Plain", "Forest", "Hill", "Swamp", "Ruins", "Road", "River", "Bridge", "DeepWater", "Blocked"};
      for (int i = 0; i < 10; ++i)
        if (name == names[i])
          return static_cast<Terrain>(i);
      throw std::runtime_error("Terreno desconocido: " + name);
    }
    Building BuildingName(const Json &value) {
      const auto name = Text(value);
      constexpr const char *names[] = {"None", "House", "Fortress", "Outpost"};
      for (int i = 0; i < 4; ++i)
        if (name == names[i])
          return static_cast<Building>(i);
      throw std::runtime_error("Edificio desconocido: " + name);
    }

    Json WriteConfig(const Config &c) {
      Json classes = Json::array();
      for (const auto &s : c.classes)
        classes.push_back({s.hp, s.attack, s.defense, s.magic, s.resistance, s.movement, s.minRange, s.maxRange, s.cost});
      Json result = {{"classes", classes}, {"itemCosts", c.itemCosts}, {"terrainCosts", c.terrainCosts}, {"economy", {c.initialCoins, c.fortressIncome, c.houseIncome, c.neutralReward, c.killReward, c.outpostCost}}, {"rot", {c.rotWait, c.rotRadius, c.rotDamage, c.rotDuration, c.globalThreshold, c.connectionDistance, c.globalDamage, c.globalDuration}}, {"magic", {c.firePower, c.icePower, c.lightningPower, c.purifyMana, c.purifierRange}}, {"weatherPeriod", c.weatherPeriod}, {"abilityCooldowns", c.abilityCooldowns}};
#define PF_WRITE_CONFIG(name) result[#name] = c.name;
      PF_CONFIG_FIELDS(PF_WRITE_CONFIG)
      PF_PHASE4_CONFIG_FIELDS(PF_WRITE_CONFIG)
#undef PF_WRITE_CONFIG
      return result;
    }
    Config ReadConfigSnapshot(const Json &j, bool requirePhase4 = false) {
      Config c;
      Array(j.at("classes"), 7);
      if (j.at("classes").size() != 7)
        throw std::runtime_error("Faltan clases");
      for (int i = 0; i < 7; ++i) {
        const auto &a = j.at("classes")[i];
        Array(a, 9);
        if (a.size() != 9)
          throw std::runtime_error("Estadisticas incompletas");
        c.classes[i] = {Number(a[0], 1, 10000), Number(a[1], 0, 1000), Number(a[2], 0, 1000),
                        Number(a[3], 0, 1000), Number(a[4], 0, 1000), Number(a[5], 1, 100),
                        Number(a[6], 1, 100), Number(a[7], 1, 100), Number(a[8], 1, 100000)};
        if (c.classes[i].minRange > c.classes[i].maxRange)
          throw std::runtime_error("Alcance invertido");
      }
      const auto readList = [&j](const char *key, auto &out, int minimum, int maximum) {
        const auto &a = j.at(key);
        Array(a, out.size());
        if (a.size() != out.size())
          throw std::runtime_error("Lista de configuracion incompleta");
        for (std::size_t i = 0; i < out.size(); ++i)
          out[i] = Number(a[i], minimum, maximum);
      };
      readList("itemCosts", c.itemCosts, 0, 100000);
      readList("terrainCosts", c.terrainCosts, 1, 100000);
      std::array<int, 6> economy{};
      readList("economy", economy, 0, 1000000);
      c.initialCoins = economy[0];
      c.fortressIncome = economy[1];
      c.houseIncome = economy[2];
      c.neutralReward = economy[3];
      c.killReward = economy[4];
      c.outpostCost = economy[5];
      std::array<int, 8> rot{};
      readList("rot", rot, 1, 1000);
      c.rotWait = rot[0];
      c.rotRadius = rot[1];
      c.rotDamage = rot[2];
      c.rotDuration = rot[3];
      c.globalThreshold = rot[4];
      c.connectionDistance = rot[5];
      c.globalDamage = rot[6];
      c.globalDuration = rot[7];
      std::array<int, 5> magic{};
      readList("magic", magic, 0, 1000);
      c.firePower = magic[0];
      c.icePower = magic[1];
      c.lightningPower = magic[2];
      c.purifyMana = magic[3];
      c.purifierRange = magic[4];
      c.weatherPeriod = Number(j, "weatherPeriod", 1, 1000);
      readList("abilityCooldowns", c.abilityCooldowns, 0, 1000);
#define PF_READ_CONFIG(name) c.name = Number(j, #name, 0, 1000);
      PF_CONFIG_FIELDS(PF_READ_CONFIG)
#undef PF_READ_CONFIG
#define PF_READ_PHASE4_CONFIG(name)       \
  if (requirePhase4 || j.contains(#name)) \
    c.name = Number(j, #name, 0, 1000);
      PF_PHASE4_CONFIG_FIELDS(PF_READ_PHASE4_CONFIG)
#undef PF_READ_PHASE4_CONFIG
      if (c.damageReductionPercent > 100 || c.maxMana < 1 || c.retreatSteps > 100 || c.houseCapture < 1 || c.outpostCapture < 1 || c.fortressCapture < 1 ||
          c.lightningMaxTargets < 1 || c.lightningMaxTargets > 512 || c.lightningRadius < 1 || c.fireDuration < 1 || c.fireDamage < 1)
        throw std::runtime_error("Parametros especiales invalidos");
      return c;
    }
    bool ValidConfig(const Config &c) {
      try {
        (void)ReadConfigSnapshot(WriteConfig(c));
        return true;
      } catch (...) {
        return false;
      }
    }

    Json WriteState(const GameState &s) {
      Json j = {{"scenario", s.scenario}, {"round", s.round}, {"currentPlayer", s.currentPlayer}, {"activeUnit", s.activeUnit}, {"nextId", s.nextId}, {"winner", s.winner}, {"globalRemaining", s.globalRemaining}, {"activationSerial", s.activationSerial}, {"weather", static_cast<int>(s.weather)}, {"mode", static_cast<int>(s.mode)}, {"difficulty", static_cast<int>(s.difficulty)}, {"weatherSeed", s.weatherSeed}, {"log", s.log}, {"nextEventId", s.nextEventId}};
      j["players"] = Json::array();
      for (const auto &p : s.players)
        j["players"].push_back({{"name", p.name}, {"coins", p.coins}});
      j["cells"] = Json::array();
      for (const auto &c : s.cells)
        j["cells"].push_back({{"pos", WriteHex(c.pos)}, {"terrain", static_cast<int>(c.terrain)}, {"height", c.height}, {"building", static_cast<int>(c.building)}, {"owner", c.owner}, {"reward", c.neutralRewardClaimed}, {"capture", c.captureUnit}, {"captureRound", c.captureStartedRound}, {"captureSerial", c.captureDefenderSerial}, {"capturingPlayer", c.capturingPlayer}, {"captureProgress", c.captureProgress}});
      j["units"] = Json::array();
      for (const auto &u : s.units)
        j["units"].push_back({{"id", u.id}, {"owner", u.owner}, {"class", static_cast<int>(u.type)}, {"pos", WriteHex(u.pos)}, {"hp", u.hp}, {"mp", u.mp}, {"readyRound", u.readyRound}, {"flags", {u.activated, u.moved, u.acted, u.attacked, u.moveLocked, u.guard, u.smoke, u.pike, u.precise, u.marked}}, {"stepsMoved", u.stepsMoved}, {"slowThroughRound", u.slowThroughRound}, {"mudUntilActivation", u.mudUntilActivation}, {"retreatRemaining", u.retreatRemaining}, {"activationStarted", u.activationStarted}, {"cooldowns", u.cooldowns}, {"inventory", Json::array()}});
      for (std::size_t i = 0; i < s.units.size(); ++i)
        for (auto item : s.units[i].inventory)
          j["units"][i]["inventory"].push_back(static_cast<int>(item));
      j["corpses"] = Json::array();
      for (const auto &c : s.corpses)
        j["corpses"].push_back({{"id", c.id}, {"pos", WriteHex(c.pos)}, {"deathRound", c.deathRound}, {"owner", c.owner}, {"class", static_cast<int>(c.type)}, {"unitId", c.unitId}});
      j["sources"] = Json::array();
      for (const auto &x : s.sources)
        j["sources"].push_back({{"id", x.id}, {"pos", WriteHex(x.pos)}, {"remaining", x.remaining}});
      j["fires"] = Json::array();
      for (const auto &fire : s.fires)
        j["fires"].push_back({{"id", fire.id}, {"pos", WriteHex(fire.pos)}, {"remaining", fire.remaining}, {"owner", fire.owner}});
      return j;
    }
    GameState ReadState(const Json &j, int version, const Config &config) {
      GameState s;
      s.scenario = Text(j.at("scenario"));
      s.round = Number(j, "round", 1, 1000000);
      s.currentPlayer = Number(j, "currentPlayer", 0, 1);
      s.activeUnit = Number(j, "activeUnit", -1, 100000000);
      s.nextId = Number(j, "nextId", 1, 100000000);
      s.winner = Number(j, "winner", -1, 1);
      s.globalRemaining = Number(j, "globalRemaining", 0, 1000);
      s.nextEventId = version >= 2 ? Number(j, "nextEventId", 1, 100000000) : 1;
      const auto &serial = j.at("activationSerial");
      Array(serial, 2);
      if (serial.size() != 2)
        throw std::runtime_error("Serial de turnos incompleto");
      for (int i = 0; i < 2; ++i)
        s.activationSerial[i] = Number(serial[i], 0, 100000000);
      s.weather = EnumNumber<Weather>(j.at("weather"), 3);
      s.mode = EnumNumber<Mode>(j.at("mode"), 1);
      s.difficulty = EnumNumber<Difficulty>(j.at("difficulty"), 2);
      s.weatherSeed = Number(j, "weatherSeed", 0, std::numeric_limits<int>::max());
      const auto &players = j.at("players");
      Array(players, 2);
      if (players.size() != 2)
        throw std::runtime_error("Se requieren dos jugadores");
      for (int i = 0; i < 2; ++i)
        s.players[i] = {Text(players[i].at("name"), 80), Number(players[i], "coins", 0, 100000000)};
      Array(j.at("cells"), MaxCells);
      for (const auto &a : j.at("cells")) {
        Cell c;
        c.pos = ReadHex(a.at("pos"));
        c.terrain = EnumNumber<Terrain>(a.at("terrain"), 9);
        c.height = Number(a, "height", -20, 20);
        c.building = EnumNumber<Building>(a.at("building"), 3);
        c.owner = Number(a, "owner", -1, 1);
        c.neutralRewardClaimed = a.at("reward").get<bool>();
        c.captureUnit = Number(a, "capture", -1, 100000000);
        c.captureStartedRound = Number(a, "captureRound", 0, 1000000);
        c.captureDefenderSerial = Number(a, "captureSerial", 0, 100000000);
        if (version >= 2) {
          c.capturingPlayer = Number(a, "capturingPlayer", -1, 1);
          c.captureProgress = Number(a, "captureProgress", 0, 1000);
        }
        s.cells.push_back(c);
      }
      Array(j.at("units"), MaxUnits);
      for (const auto &a : j.at("units")) {
        Unit u;
        u.id = Number(a, "id", 1, 100000000);
        u.owner = Number(a, "owner", 0, 1);
        u.type = EnumNumber<UnitClass>(a.at("class"), 6);
        u.pos = ReadHex(a.at("pos"));
        u.hp = Number(a, "hp", 0, 10000);
        u.mp = Number(a, "mp", 0, 1000);
        u.readyRound = Number(a, "readyRound", 1, 1000001);
        const auto &f = a.at("flags");
        Array(f, 10);
        if (f.size() != 10)
          throw std::runtime_error("Estados incompletos");
        u.activated = f[0].get<bool>();
        u.moved = f[1].get<bool>();
        u.acted = f[2].get<bool>();
        u.attacked = f[3].get<bool>();
        u.moveLocked = f[4].get<bool>();
        u.guard = f[5].get<bool>();
        u.smoke = f[6].get<bool>();
        u.pike = f[7].get<bool>();
        u.precise = f[8].get<bool>();
        u.marked = f[9].get<bool>();
        u.activationStarted = version >= 2 ? a.at("activationStarted").get<bool>() : u.activated;
        // En v1 activated significaba iniciada; la unidad activa debe poder reanudar su activación en v2.
        if (version == 1 && u.id == s.activeUnit)
          u.activated = false;
        u.stepsMoved = Number(a, "stepsMoved", 0, 1000);
        u.slowThroughRound = Number(a, "slowThroughRound", 0, 1000001);
        u.mudUntilActivation = Number(a, "mudUntilActivation", 0, 100000001);
        u.retreatRemaining = Number(a, "retreatRemaining", 0, 100);
        const auto &cooldowns = a.at("cooldowns");
        Array(cooldowns, u.cooldowns.size());
        if (cooldowns.size() != u.cooldowns.size())
          throw std::runtime_error("Recargas incompletas");
        for (std::size_t i = 0; i < u.cooldowns.size(); ++i)
          u.cooldowns[i] = Number(cooldowns[i], 0, 1000);
        Array(a.at("inventory"), 2);
        for (const auto &item : a.at("inventory"))
          u.inventory.push_back(EnumNumber<Item>(item, 4));
        s.units.push_back(u);
      }
      Array(j.at("corpses"), MaxEffects);
      std::set<int> representedDead;
      for (const auto &a : j.at("corpses")) {
        Corpse corpse{Number(a, "id", 1, 100000000), ReadHex(a.at("pos")), Number(a, "deathRound", 1, 1000000)};
        if (version >= 2) {
          corpse.owner = Number(a, "owner", 0, 1);
          corpse.type = EnumNumber<UnitClass>(a.at("class"), 6);
          corpse.unitId = Number(a, "unitId", -1, 100000000);
        } else {
          const auto fallen = std::ranges::find_if(s.units, [&](const Unit &unit) { return !unit.Alive() && unit.pos == corpse.pos && !representedDead.contains(unit.id); });
          if (fallen != s.units.end()) {
            corpse.owner = fallen->owner;
            corpse.type = fallen->type;
            corpse.unitId = fallen->id;
            representedDead.insert(fallen->id);
          }
        }
        s.corpses.push_back(corpse);
      }
      Array(j.at("sources"), MaxEffects);
      for (const auto &a : j.at("sources"))
        s.sources.push_back({Number(a, "id", 1, 100000000), ReadHex(a.at("pos")), Number(a, "remaining", 1, 1000)});
      if (version >= 2) {
        Array(j.at("fires"), MaxEffects);
        for (const auto &a : j.at("fires"))
          s.fires.push_back({Number(a, "id", 1, 100000000), ReadHex(a.at("pos")), Number(a, "remaining", 1, 1000), Number(a, "owner", 0, 1)});
      } else {
        // Migra una captura pendiente válida de v1 a una acción basada en HP sin completarla al cargar.
        for (Cell &cell : s.cells)
          if (cell.captureUnit >= 0) {
            const auto captor = std::ranges::find(s.units, cell.captureUnit, &Unit::id);
            if (cell.building != Building::Fortress || captor == s.units.end() || !captor->Alive() || captor->pos != cell.pos || captor->owner == cell.owner || cell.owner < 0 ||
                cell.captureStartedRound < 1 || cell.captureStartedRound > s.round || cell.captureDefenderSerial < 0 || cell.captureDefenderSerial > s.activationSerial[static_cast<std::size_t>(cell.owner)])
              throw std::runtime_error("Captura antigua inconsistente");
            cell.capturingPlayer = captor->owner;
            cell.captureProgress = std::min(captor->hp, config.fortressCapture - 1);
            if (cell.captureProgress <= 0) {
              cell.captureUnit = -1;
              cell.capturingPlayer = -1;
              cell.captureStartedRound = cell.captureDefenderSerial = 0;
            }
          }
      }
      Array(j.at("log"), 200);
      for (const auto &entry : j.at("log"))
        s.log.push_back(Text(entry, 512));
      return s;
    }
  } // espacio de nombres anónimo

  Config Config::Load(const std::string &filename) {
    const auto j = ReadJson(filename);
    Config c;
    if (j.contains("snapshot"))
      return ReadConfigSnapshot(j.at("snapshot"));
    if (j.contains("classes")) {
      if (!j.at("classes").is_object())
        throw std::runtime_error("classes debe ser un objeto");
      for (auto it = j.at("classes").begin(); it != j.at("classes").end(); ++it) {
        const auto type = ClassName(it.key());
        const auto &a = it.value();
        auto &s = c.classes[static_cast<int>(type)];
        s = {Number(a, "hp", 1, 10000), Number(a, "attack", 0, 1000), Number(a, "defense", 0, 1000), Number(a, "magic", 0, 1000),
             Number(a, "resistance", 0, 1000), Number(a, "movement", 1, 100), Number(a, "minRange", 1, 100), Number(a, "maxRange", 1, 100), Number(a, "cost", 1, 100000)};
      }
    }
    auto integer = [&j](const char *key, int &out, int minimum = 0, int maximum = 1000000) { if(j.contains(key)) out=Number(j,key,minimum,maximum); };
    integer("initialCoins", c.initialCoins);
    integer("fortressIncome", c.fortressIncome);
    integer("houseIncome", c.houseIncome);
    integer("neutralReward", c.neutralReward);
    integer("killReward", c.killReward);
    integer("outpostCost", c.outpostCost);
    integer("rotWait", c.rotWait, 1, 1000);
    integer("rotRadius", c.rotRadius, 1, 1000);
    integer("rotDamage", c.rotDamage, 1, 1000);
    integer("rotDuration", c.rotDuration, 1, 1000);
    integer("globalThreshold", c.globalThreshold, 1, 1000);
    integer("connectionDistance", c.connectionDistance, 1, 1000);
    integer("globalDamage", c.globalDamage, 1, 1000);
    integer("globalDuration", c.globalDuration, 1, 1000);
    integer("weatherPeriod", c.weatherPeriod, 1, 1000);
    integer("firePower", c.firePower, 0, 1000);
    integer("icePower", c.icePower, 0, 1000);
    integer("lightningPower", c.lightningPower, 0, 1000);
    integer("purifyMana", c.purifyMana, 0, 1000);
    integer("purifierRange", c.purifierRange, 0, 100);
#define PF_LOAD_CONFIG(name) integer(#name, c.name, 0, 1000);
    PF_CONFIG_FIELDS(PF_LOAD_CONFIG)
    PF_PHASE4_CONFIG_FIELDS(PF_LOAD_CONFIG)
#undef PF_LOAD_CONFIG
    if (j.contains("abilityCooldowns")) {
      Array(j.at("abilityCooldowns"), c.abilityCooldowns.size());
      if (j.at("abilityCooldowns").size() != c.abilityCooldowns.size())
        throw std::runtime_error("Faltan recargas");
      for (std::size_t i = 0; i < c.abilityCooldowns.size(); ++i)
        c.abilityCooldowns[i] = Number(j.at("abilityCooldowns")[i], 0, 1000);
    }
    if (j.contains("itemCosts")) {
      Array(j.at("itemCosts"), 5);
      if (j.at("itemCosts").size() != 5)
        throw std::runtime_error("Faltan costes de objetos");
      for (int i = 0; i < 5; ++i)
        c.itemCosts[i] = Number(j.at("itemCosts")[i], 0, 100000);
    }
    if (j.contains("terrainCosts")) {
      Array(j.at("terrainCosts"), 10);
      if (j.at("terrainCosts").size() != 10)
        throw std::runtime_error("Faltan costes de terreno");
      for (int i = 0; i < 10; ++i)
        c.terrainCosts[i] = Number(j.at("terrainCosts")[i], 1, 100000);
    }
    if (!ValidConfig(c))
      throw std::runtime_error("Configuracion de balance invalida");
    return c;
  }

  Match Match::Create(const Config &config, const std::string &scenarioFile, Mode mode, Difficulty difficulty,
                      const std::array<std::vector<UnitClass>, 2> &armies) {
    const auto j = ReadJson(scenarioFile);
    Match m(config);
    m.state.scenario = Text(j.at("name"));
    m.state.mode = mode;
    m.state.difficulty = difficulty;
    m.state.weatherSeed = j.contains("weather_seed") ? Number(j, "weather_seed", 0, std::numeric_limits<int>::max()) : 1;
    m.state.players = {Player{"Corona de ceniza", config.initialCoins}, Player{"Pacto del umbral", config.initialCoins}};
    Array(j.at("cells"), MaxCells);
    for (const auto &a : j.at("cells")) {
      Cell c;
      c.pos = ReadHex(a.at("pos"));
      c.terrain = TerrainName(a.at("terrain"));
      c.height = Number(a, "height", -20, 20);
      c.building = a.contains("building") ? BuildingName(a.at("building")) : Building::None;
      c.owner = a.contains("owner") ? Number(a, "owner", -1, 1) : -1;
      m.state.cells.push_back(c);
    }
    Array(j.at("armies"), 2);
    Array(j.at("spawns"), 2);
    if (j.at("armies").size() != 2 || j.at("spawns").size() != 2)
      throw std::runtime_error("Faltan ejercitos iniciales");
    for (int owner = 0; owner < 2; ++owner) {
      Array(j.at("armies")[owner], 5);
      Array(j.at("spawns")[owner], 5);
      if (j.at("spawns")[owner].size() != 5 || j.at("armies")[owner].size() != 5 || (!armies[owner].empty() && armies[owner].size() != 5))
        throw std::runtime_error("Cada ejercito inicial requiere cinco unidades y posiciones");
      for (int i = 0; i < 5; ++i) {
        const auto type = armies[owner].empty() ? ClassName(j.at("armies")[owner][i]) : armies[owner][i];
        if (static_cast<int>(type) < 0 || static_cast<int>(type) > 6)
          throw std::runtime_error("Clase de ejercito invalida");
        Unit u;
        u.id = m.state.nextId++;
        u.owner = owner;
        u.type = type;
        u.pos = ReadHex(j.at("spawns")[owner][i]);
        u.hp = config.classes[static_cast<int>(type)].hp;
        u.mp = type == UnitClass::Caster ? config.maxMana : 0;
        m.state.units.push_back(u);
      }
    }
    std::string error;
    if (!m.Validate(error))
      throw std::runtime_error("Escenario invalido: " + error);
    m.Log("Comienza " + m.state.scenario + ". Captura la fortaleza rival.");
    m.StartTurn(0);
    return m;
  }

  bool Match::Validate(std::string &error) const {
    auto reject = [&error](const char *message) {error=message;return false; };
    if (!ValidConfig(config))
      return reject("Balance invalido");
    if (state.cells.empty() || state.cells.size() > MaxCells || state.units.size() > MaxUnits || state.corpses.size() > MaxEffects || state.sources.size() > MaxEffects || state.fires.size() > MaxEffects || state.combatEvents.size() > MaxUnits + 1)
      return reject("Numero de entidades invalido");
    if (state.round < 1 || state.round > 1000000 || state.currentPlayer < 0 || state.currentPlayer > 1 || state.winner < -1 || state.winner > 1 || state.nextId < 1 || state.nextId > 100000000 || state.nextEventId < 1 || state.nextEventId > 100000000)
      return reject("Control de partida invalido");
    if (static_cast<int>(state.weather) < 0 || static_cast<int>(state.weather) > 3 || static_cast<int>(state.mode) < 0 || static_cast<int>(state.mode) > 1 || static_cast<int>(state.difficulty) < 0 || static_cast<int>(state.difficulty) > 2 || state.weatherSeed < 0)
      return reject("Modo o clima invalido");
    if (state.globalRemaining < 0 || state.globalRemaining > config.globalDuration)
      return reject("Temporizador global invalido");
    if (state.scenario.size() > 160 || state.log.size() > 200)
      return reject("Metadatos demasiado grandes");
    for (const auto &entry : state.log)
      if (entry.size() > 512)
        return reject("Registro demasiado largo");
    for (int i = 0; i < 2; ++i)
      if (state.players[i].coins < 0 || state.players[i].coins > 100000000 || state.players[i].name.size() > 80 || state.activationSerial[i] < 0 || state.activationSerial[i] > 100000000)
        return reject("Jugador invalido");
    std::set<Hex> cells, occupied;
    std::set<int> ids;
    std::array<int, 2> fortresses{};
    for (const auto &c : state.cells) {
      if (c.pos.q < -10000 || c.pos.q > 10000 || c.pos.r < -10000 || c.pos.r > 10000 || !cells.insert(c.pos).second)
        return reject("Celdas duplicadas o coordenada invalida");
      if (static_cast<int>(c.terrain) < 0 || static_cast<int>(c.terrain) > 9 || c.height < -20 || c.height > 20 || static_cast<int>(c.building) < 0 || static_cast<int>(c.building) > 3 || c.owner < -1 || c.owner > 1)
        return reject("Terreno o edificio invalido");
      if (c.building == Building::None && c.owner != -1)
        return reject("Propietario sin edificio");
      if (c.building == Building::Fortress) {
        if (c.owner < 0)
          return reject("Fortaleza sin propietario");
        ++fortresses[c.owner];
      }
      if (c.building != Building::None && (c.terrain == Terrain::Blocked || c.terrain == Terrain::DeepWater))
        return reject("Edificio inaccesible");
      if (c.capturingPlayer < -1 || c.capturingPlayer > 1 || c.captureProgress < 0 || c.captureProgress >= std::max(1, CaptureRequired(c)) ||
          c.captureStartedRound < 0 || c.captureStartedRound > state.round || c.captureDefenderSerial < 0 || c.captureDefenderSerial > 100000000 ||
          (c.captureProgress == 0 && (c.capturingPlayer != -1 || c.captureUnit != -1)) ||
          (c.captureProgress > 0 && (c.building == Building::None || c.capturingPlayer < 0 || c.capturingPlayer == c.owner || c.captureStartedRound < 1 || c.captureStartedRound > state.round)))
        return reject("Progreso de captura inconsistente");
      if (c.captureUnit != -1) {
        const auto *u = Find(c.captureUnit);
        if (!u || !u->Alive() || u->pos != c.pos || u->owner != c.capturingPlayer)
          return reject("Captor inconsistente");
      }
    }
    if (state.winner == -1 && (fortresses[0] != 1 || fortresses[1] != 1))
      return reject("Se requiere una fortaleza por jugador");
    if (state.winner != -1 && (fortresses[state.winner] != 2 || fortresses[1 - state.winner] != 0))
      return reject("Resultado de conquista inconsistente");
    for (const auto &u : state.units) {
      if (u.id < 1 || u.id >= state.nextId || !ids.insert(u.id).second || u.owner < 0 || u.owner > 1 || static_cast<int>(u.type) < 0 || static_cast<int>(u.type) > 6)
        return reject("Identidad de unidad invalida");
      const auto *cell = At(u.pos);
      if (!cell || u.hp < 0 || u.hp > config.classes[static_cast<int>(u.type)].hp || u.mp < 0 || u.mp > config.maxMana || (u.type != UnitClass::Caster && u.mp != 0))
        return reject("Recursos de unidad invalidos");
      if (u.Alive() && (!occupied.insert(u.pos).second || cell->terrain == Terrain::Blocked || cell->terrain == Terrain::DeepWater))
        return reject("Ocupacion de unidad invalida");
      if (u.inventory.size() > 2 || u.readyRound < 1 || u.readyRound > state.round + 1 || u.stepsMoved < 0 || u.stepsMoved > 1000 || u.slowThroughRound < 0 || u.slowThroughRound > state.round + 1 || u.mudUntilActivation < 0 || u.mudUntilActivation > 100000001 || u.retreatRemaining < 0 || u.retreatRemaining > config.retreatSteps)
        return reject("Estados de unidad invalidos");
      for (auto item : u.inventory)
        if (static_cast<int>(item) < 0 || static_cast<int>(item) > 4)
          return reject("Objeto invalido");
      for (auto cd : u.cooldowns)
        if (cd < 0 || cd > 1000)
          return reject("Recarga invalida");
    }
    for (const auto &c : state.corpses)
      if (c.id < 1 || c.id >= state.nextId || !ids.insert(c.id).second || !At(c.pos) || c.deathRound < 1 || c.deathRound > state.round || c.owner < 0 || c.owner > 1 || static_cast<int>(c.type) < 0 || static_cast<int>(c.type) > 6)
        return reject("Cadaver invalido");
    for (const auto &corpse : state.corpses)
      if (corpse.unitId != -1) {
        const Unit *unit = Find(corpse.unitId);
        if (!unit || unit->Alive() || unit->pos != corpse.pos || unit->owner != corpse.owner || unit->type != corpse.type)
          return reject("Identidad de cadaver inconsistente");
      }
    for (const auto &s : state.sources)
      if (s.id < 1 || s.id >= state.nextId || !ids.insert(s.id).second || !At(s.pos) || s.remaining < 1 || s.remaining > config.rotDuration)
        return reject("Fuente invalida");
    for (const auto &fire : state.fires) {
      const Cell *cell = At(fire.pos);
      if (fire.id < 1 || fire.id >= state.nextId || !ids.insert(fire.id).second || !cell || fire.remaining < 1 || fire.remaining > config.fireDuration || fire.owner < 0 || fire.owner > 1 ||
          cell->terrain == Terrain::River || cell->terrain == Terrain::DeepWater || cell->terrain == Terrain::Swamp || cell->terrain == Terrain::Blocked)
        return reject("Incendio invalido");
    }
    int lastEvent = 0;
    for (const auto &event : state.combatEvents) {
      if (event.id <= lastEvent || event.id >= state.nextEventId || !Find(event.attackerId) || !Find(event.targetId) || !At(event.from) || !At(event.to) ||
          static_cast<int>(event.ability) < 0 || static_cast<int>(event.ability) > 15 || event.amount < 1 || event.amount > 10000 || event.chainIndex < 0 || event.chainIndex >= config.lightningMaxTargets)
        return reject("Evento de combate invalido");
      lastEvent = event.id;
    }
    if (state.activeUnit != -1) {
      const auto *u = Find(state.activeUnit);
      if (!u || !u->Alive() || u->owner != state.currentPlayer || u->activated || !u->activationStarted || u->readyRound > state.round || state.winner >= 0)
        return reject("Activacion invalida");
    }
    error.clear();
    return true;
  }

  bool Match::Save(const std::string &filename, std::string &error) const {
    try {
      if (!Validate(error))
        return false;
      const auto data = Json{{"format", "Plaguefront"}, {"version", 2}, {"config", WriteConfig(config)}, {"state", WriteState(state)}}.dump(2);
      if (data.size() > MaxFileBytes)
        throw std::runtime_error("Partida demasiado grande");
      const std::filesystem::path target(filename), temp(filename + ".tmp");
      if (!target.parent_path().empty())
        std::filesystem::create_directories(target.parent_path());
      {
        std::ofstream out(temp, std::ios::binary | std::ios::trunc);
        if (!out || !out.write(data.data(), static_cast<std::streamsize>(data.size())) || !out.flush())
          throw std::runtime_error("No se puede escribir la partida temporal");
      }
      // Conserva el último guardado válido si falla el reemplazo; rename no sobrescribe archivos en Windows.
      const std::filesystem::path backup(filename + ".bak");
      std::error_code ec;
      std::filesystem::remove(backup, ec);
      const bool exists = std::filesystem::exists(target);
      if (exists)
        std::filesystem::rename(target, backup);
      try {
        std::filesystem::rename(temp, target);
      } catch (...) {
        if (exists)
          std::filesystem::rename(backup, target, ec);
        throw;
      }
      std::filesystem::remove(backup, ec);
      error.clear();
      return true;
    } catch (const std::exception &e) {
      error = e.what();
      return false;
    }
  }

  std::optional<Match> Match::Load(const std::string &filename, const Config &, std::string &error) {
    try {
      const auto j = ReadJson(filename);
      if (Text(j.at("format")) != "Plaguefront")
        throw std::runtime_error("Formato de guardado incompatible");
      const int version = Number(j, "version", 1, 2);
      Match m(ReadConfigSnapshot(j.at("config"), version >= 2));
      m.state = ReadState(j.at("state"), version, m.config);
      if (version == 1)
        m.Log("Guardado v1 migrado a turnos por ejercito; no se conceden ingresos durante la carga.");
      if (!m.Validate(error))
        return std::nullopt;
      error.clear();
      return m;
    } catch (const std::exception &e) {
      error = e.what();
      return std::nullopt;
    }
  }
} // espacio de nombres Plaguefront
