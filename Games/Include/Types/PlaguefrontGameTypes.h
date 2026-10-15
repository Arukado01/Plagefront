#pragma once
#include "Config/PlaguefrontGameConfig.h"
#include "Plaguefront/Rules.h"
#include <SFML/Graphics/Rect.hpp>
#include <optional>
#include <string>
namespace Plaguefront {
  enum class GameScreen { Menu,
                          Setup,
                          Playing,
                          Paused,
                          Settings,
                          Credits,
                          Victory,
                          ConfirmExit,
                          Handoff,
                          ConfirmTurn,
                          Controls };
  enum class TacticalContext { Selection,
                               Orders,
                               Move,
                               Abilities,
                               Target,
                               Inventory,
                               Building,
                               Recruitment,
                               Shop,
                               Construction,
                               Purchase };
  struct Button {
    sf::FloatRect bounds;
    std::string label;
    int action{}, value{};
    bool enabled{true};
  };
  enum GameAction { Play,
                    New,
                    Continue,
                    Settings,
                    Credits,
                    Exit,
                    Back,
                    StartGame,
                    ModeSelect,
                    DifficultySelect,
                    ScenarioSelect,
                    ArmySelect,
                    Resume,
                    Pause,
                    Restart,
                    MainMenu,
                    Save,
                    Load,
                    EndTurn,
                    AbilitySelect,
                    Capture,
                    Build,
                    Recruit,
                    Battalion,
                    Buy,
                    UseItem,
                    Tab,
                    Ready,
                    Grid,
                    Effects,
                    Speed,
                    Volume,
                    MusicVolume,
                    SoundVolume,
                    Mute,
                    ConfirmExit,
                    CancelExit,
                    CustomAdd,
                    CustomClear,
                    CustomConfirm,
                    CustomBack,
                    OpenMove,
                    OpenAbilities,
                    OpenInventory,
                    OpenBuilding,
                    ContextBack,
                    OpenConstruction,
                    ConfirmBuild,
                    ConfirmPurchase,
                    Fullscreen,
                    Resolution,
                    CenterCamera,
                    FrameMap,
                    ShopUnit,
                    ArmyEnd,
                    ConfirmArmyEnd,
                    CancelArmyEnd,
                    RosterSelect,
                    RosterScroll,
                    Controls,
                    ControlProfile,
                    ControlDeadzone,
                    ControlSensitivity,
                    ControlInvertX,
                    ControlInvertY,
                    ControlRepeat,
                    ControlMapping,
                    ControlReset,
                    MaximizeWindow,
                    ResolutionSelect,
                    ResolutionScroll };
  struct CampaignSetup {
    Mode mode;
    Difficulty difficulty;
    int scenario;
    std::array<std::vector<UnitClass>, 2> armies;
  };
  struct LegalityKey {
    std::uint64_t revision{};
    int selected{}, active{}, player{}, round{}, coins0{}, coins1{};
    std::optional<Hex> building;
    bool busy{};
    std::vector<UnitClass> battalion;
    bool operator==(const LegalityKey &) const = default;
  };
} // Espacio de nombres Plaguefront
