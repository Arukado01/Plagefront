#pragma once
#include "Scene/Scene.h"
#include "Plaguefront/Rules.h"
#include "Plaguefront/Presentation.h"
#include "Plaguefront/Audio.h"
#include "Types/PlaguefrontGameTypes.h"
#include <functional>

namespace Plaguefront {
  class Game final : public Scene {
    using Screen = GameScreen;
    using Action = GameAction;
    using enum GameAction;
    Screen screen_{Screen::Menu}, previous_{Screen::Menu}, exitPrevious_{Screen::Menu};
    TacticalContext tactical_{TacticalContext::Selection}, purchaseReturn_{TacticalContext::Building};
    int pendingAction_{}, pendingValue_{}, keyboardFocus_{-1};
    std::string pendingLabel_;
    std::optional<CampaignSetup> restartSetup_;
    Config balance_;
    std::optional<Match> match_;
    std::optional<Match> observed_;
    sf::Font &font_, &headingFont_;
    bool headingFontLoaded_{};
    AudioDirector audio_;
    Camera camera_;
    AnimationDirector visuals_;
    mutable sf::RenderTexture world_;
    mutable std::map<const sf::Texture *, sf::Texture> rotTextures_;
    unsigned worldPixelScale_{2};
    std::uint64_t displayRevision_{};
    bool smoke_{};
    std::optional<Path> previewPath_;
    std::map<Hex, int> reachable_;
    std::optional<Hex> previewDestination_;
    int previewUnit_{-1};
    std::uint64_t matchRevision_{}, previewRevision_{~std::uint64_t{0}};
    mutable std::optional<Match> targetingRules_;
    mutable std::uint64_t targetingRevision_{~std::uint64_t{0}};
    mutable int targetingUnit_{-1};
    mutable std::optional<LegalityKey> legalityKey_;
    mutable std::map<std::pair<int, int>, Result> legalityCache_;
    sf::Texture *tiles_{}, *units_{}, *icons_{}, *menu_{};
    sf::Texture *props_{}, *spellEffects_{}, *skin_{}, *unitIcons_{};
    mutable std::vector<Button> buttons_;
    std::array<std::vector<UnitClass>, 2> armies_;
    std::vector<UnitClass> customBattalion_;
    bool customBattalionEditing_{};
    Mode mode_{Mode::VersusAI};
    Difficulty difficulty_{Difficulty::Normal};
    int scenario_{}, selected_{-1}, tab_{}, itemSlot_{-1}, rosterScroll_{};
    std::optional<Ability> ability_;
    std::optional<Hex> hovered_, selectedCell_;
    bool canContinue_{}, grid_{true}, effects_{true}, muted_{}, dragging_{};
    sf::Vector2f dragPosition_{}, dragRemainder_{};
    float aiDelay_{0.55f}, aiElapsed_{}, animationTime_{};
    int volume_{60}, musicVolume_{60}, soundVolume_{80}, hoverButton_{-1};
    std::string message_;
    std::string settingsFile_{"Saves/settings.json"};
    static constexpr sf::Vector2f BoardOrigin = UI::BoardOrigin;
    mutable bool layoutOnly_{};
    mutable float layoutYOffset_{};
    std::uint64_t layoutRevision_{~std::uint64_t{0}};
    std::optional<Hex> padCell_;
    bool padBoard_{true};
    bool resolutionOpen_{}, resolutionFullscreen_{};
    std::vector<sf::Vector2u> resolutionOptions_;
    int resolutionOption_{}, resolutionScroll_{};
    static constexpr int ResolutionVisibleRows = 8;

  public:
    explicit Game(EngineContext &context);
    void Start() override;
    void Update() override;
    void Render() const override;
    void OnEvent(const sf::Event &) override;
    bool HandlesNavigation() const override { return true; }
    void SmokeStep(int step) override;
    bool OnCloseRequest() override {
      if (screen_ != Screen::ConfirmExit)
        exitPrevious_ = screen_;
      ChangeScreen(Screen::ConfirmExit);
      return true;
    }

  private:
    void ChangeScreen(Screen screen);
    void Dispatch(Button button);
    void BeginGame(bool restart = false);
    void LoadArmies();
    void RefreshContinue();
    void LoadSettings();
    void SaveSettings();
    void Apply(Result result, std::optional<Ability> hint = {});
    void SyncPresentation(std::optional<Ability> hint = {});
    void ResetPresentation();
    void SetContext(TacticalContext);
    void CancelContext();
    void UpdatePreview(std::optional<Hex>);
    void RefreshLayout(bool frame = false);
    void FocusCamera(bool all);
    sf::FloatRect BoardBounds() const;
    float WorldScale() const;
    float UiWidth() const;
    float UiHeight() const;
    int RosterVisibleRows() const;
    void RebuildControls();
    void UpdateGamepad(float);
    void MoveGamepadCursor(sf::Vector2i);
    void FocusGamepad(sf::Vector2i);
    void SyncGamepadCursor();
    void RenderGamepadCursor() const;
    void RenderControls() const;
    void AdjustControls(int action, int value);
    void ToggleResolutionSelect();
    void CloseResolutionSelect(bool restoreFocus = true);
    void MoveResolutionSelect(int delta);
    bool HandleResolutionEvent(const sf::Event &);
    void RenderResolutionSelect() const;
    void SmokeGamepad();
    void PrepareGamepadCapture(int);
    void SmokePhase5(int);
    void SmokeRotPhase5(int);
    void SmokeVisualCorrections();
    void SmokeUnitIntegration();
    Result CheckAction(int action, int value = 0) const;
    const Match &PreviewRules() const;
    void RenderWorldLabels() const;
    std::optional<Hex> PickCell(sf::Vector2f cursor) const;
    bool EnsureActive();
    void HandleCell(Hex cell);
    void FinishTurn();
    void FinishArmyTurn(bool confirmed = false);
    void SelectRosterUnit(int id);
    void UpdateCursor(std::optional<sf::Vector2f> eventPosition = {});
    void RenderRoster() const;
    void RenderTacticalInformation() const;
    void SmokePhase4(int step);
    void VerifyPhase4Controls();
    void SelectAbility(Ability ability);
    std::string ScenarioFile() const;
    sf::Vector2f Center(Hex cell) const;
    void Text(const std::string &, float x, float y, unsigned size = 17, sf::Color color = sf::Color(220, 225, 214)) const;
    void Wrapped(const std::string &, float x, float y, float width, unsigned size = 17, sf::Color color = sf::Color(180, 193, 179)) const;
    void Box(sf::FloatRect, sf::Color, sf::Color outline = sf::Color::Transparent, float thickness = 1) const;
    void Skin(sf::FloatRect bounds, int patch, sf::Color tint = sf::Color::White) const;
    void ButtonAt(const std::string &, float x, float y, float width, float height, int action, int value = 0, bool enabled = true) const;
    void RenderMenu() const;
    void RenderSetup() const;
    void RenderBoard() const;
    void RenderWorld() const;
    void RenderSpellEffect(const VisualEffect &) const;
    const sf::Texture *RotTexture(const sf::Texture *) const;
    void RenderPanel() const;
    void RenderTacticalPanel() const;
    void RenderModal() const;
  };
} // Espacio de nombres Plaguefront
