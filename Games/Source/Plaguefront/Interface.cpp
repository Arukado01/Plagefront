#include "Plaguefront/Game.h"
#include <SFML/Graphics.hpp>
#include <algorithm>
#include <cmath>
#include <sstream>
namespace Plaguefront {
  namespace {
    const sf::Color Ink{17, 20, 29}, Panel{29, 33, 42}, Edge{105, 98, 78}, Gold{216, 184, 116}, Muted{184, 194, 187};
    const std::array<sf::Color, 2> Teams{sf::Color{101, 177, 216}, sf::Color{225, 100, 120}};
    sf::String Utf8(const std::string &text) { return sf::String::fromUtf8(text.begin(), text.end()); }
    std::string DifficultyName(Difficulty d) { return std::array<std::string, 3>{"Fácil", "Normal", "Difícil"}[static_cast<int>(d)]; }
  } // Espacio de nombres
  void Game::Text(const std::string &text, float x, float y, unsigned size, sf::Color color) const {
    if (layoutOnly_)
      return;
    y += layoutYOffset_;
    const float scale = ctx.renderer.UiScale();
    const auto pixels = static_cast<unsigned>(std::max(12.f, std::round(std::max(16u, size) * scale)));
    sf::Text drawable(size >= 25 && headingFontLoaded_ ? headingFont_ : font_, Utf8(text), pixels);
    drawable.setScale({1.f / scale, 1.f / scale});
    drawable.setPosition({std::round(x * scale) / scale, std::round(y * scale) / scale});
    drawable.setFillColor(color);
    ctx.renderer.Draw(drawable);
  }
  void Game::Wrapped(const std::string &text, float x, float y, float width, unsigned size, sf::Color color) const {
    if (layoutOnly_)
      return;
    std::istringstream words(text);
    std::string word, line;
    while (words >> word) {
      auto candidate = line.empty() ? word : line + " " + word;
      const float scale = ctx.renderer.UiScale();
      sf::Text measure(font_, Utf8(candidate), static_cast<unsigned>(std::max(12.f, std::round(std::max(16u, size) * scale))));
      measure.setScale({1.f / scale, 1.f / scale});
      if (!line.empty() && measure.getGlobalBounds().size.x > width) {
        Text(line, x, y, size, color);
        y += size * 1.5f;
        line = word;
      } else
        line = candidate;
    }
    if (!line.empty())
      Text(line, x, y, size, color);
  }
  void Game::Box(sf::FloatRect bounds, sf::Color color, sf::Color outline, float thickness) const {
    if (layoutOnly_)
      return;
    bounds.position.y += layoutYOffset_;
    sf::RectangleShape box(bounds.size);
    box.setPosition(bounds.position);
    box.setFillColor(color);
    box.setOutlineColor(outline);
    box.setOutlineThickness(thickness);
    ctx.renderer.Draw(box);
    if (skin_ && bounds.size.x >= 60 && bounds.size.y >= 32 && bounds.size.y < 800 && color.a > 150)
      Skin(bounds, 0);
  }
  void Game::ButtonAt(const std::string &label, float x, float y, float width, float height, int action, int value, bool enabled) const {
    const float displayY = y + layoutYOffset_;
    bool hover = sf::FloatRect({x, displayY}, {width, height}).contains(ctx.cursor.GetPosition()) || keyboardFocus_ == static_cast<int>(buttons_.size());
    Box({{x, y}, {width, height}}, enabled ? (hover ? sf::Color{48, 73, 62} : sf::Color{27, 44, 41}) : sf::Color{26, 31, 31}, enabled && hover ? Gold : Edge);
    if (skin_)
      Skin({{x, displayY}, {width, height}}, hover && enabled ? 2 : 1, enabled ? sf::Color::White : sf::Color{110, 115, 111});
    unsigned size = height < 36 ? 16 : 17;
    int icon = -1;
    if (action == Buy)
      icon = 8 + value;
    if (action == Capture)
      icon = 14;
    if (action == Build)
      icon = 15;
    if (action == EndTurn || action == ArmyEnd)
      icon = 23;
    if (action == AbilitySelect) {
      static const std::array<int, 16> mapping{0, 7, 24, 25, 1, 26, 2, 27, 4, 28, 29, 30, 31, 6, 12, 21};
      if (value >= 0 && value < 16)
        icon = mapping[value];
    }
    float textOffset = 12.f;
    if (icons_ && icon >= 0 && !layoutOnly_) {
      sf::Sprite sprite(*icons_, sf::IntRect({(icon % 8) * 16, (icon / 8) * 16}, {16, 16}));
      const float scale = ctx.renderer.UiScale();
      const float pixel = std::max(1.f, std::floor((height * scale - 6) / 16));
      const float factor = pixel / scale;
      sprite.setPosition({x + 6, displayY + (height - 16 * factor) / 2});
      sprite.setScale({factor, factor});
      if (!enabled)
        sprite.setColor({145, 153, 148});
      ctx.renderer.Draw(sprite);
      textOffset = 16 * factor + 12;
    }
    if (smoke_) {
      const float scale = ctx.renderer.UiScale();
      sf::Text measured(font_, Utf8(label), static_cast<unsigned>(std::max(12.f, std::round(std::max(16u, size) * scale))));
      if (measured.getLocalBounds().size.x / scale > width - textOffset - 8)
        throw std::runtime_error("Smoke: etiqueta demasiado ancha: " + label);
      if (x < 0 || displayY < 0 || x + width > UiWidth() + 1 || displayY + height > UiHeight() + 1)
        throw std::runtime_error("Smoke: botón fuera de pantalla: " + label);
    }
    Text(label, x + textOffset, y + (height - size) / 2.f - 3, size, enabled ? sf::Color{233, 237, 222} : sf::Color{151, 160, 154});
    buttons_.push_back({{{x, displayY}, {width, height}}, label, action, value, enabled});
  }
  void Game::RenderMenu() const {
    if (menu_ && !layoutOnly_) {
      const float scale = std::ceil(std::max(ctx.renderer.OutputSize().x / 720.f, ctx.renderer.OutputSize().y / 450.f)) / ctx.renderer.UiScale();
      sf::Sprite background(*menu_);
      background.setScale({scale, scale});
      background.setPosition({(UiWidth() - 720 * scale) / 2, (UiHeight() - 450 * scale) / 2});
      ctx.renderer.Draw(background);
    }
    Box({{0, -layoutYOffset_}, {690, UiHeight()}}, sf::Color{8, 17, 20, 196});
    Text("PLAGUEFRONT", 76, 131, 58, Gold);
    Text("LA FRONTERA GRIS", 81, 211, 18, {146, 172, 156});
    Box({{81, 258}, {411, 2}}, Gold);
    Wrapped("La guerra deja muertos. Los muertos cambian la guerra.", 81, 280, 390, 20);
    float y = 391;
    ButtonAt("JUGAR", 81, y, 370, 47, Play);
    y += 60;
    ButtonAt("NUEVA PARTIDA", 81, y, 370, 47, New);
    y += 60;
    if (canContinue_) {
      ButtonAt("CONTINUAR PARTIDA GUARDADA", 81, y, 370, 47, Continue);
      y += 60;
    }
    ButtonAt("CONFIGURACIÓN", 81, y, 370, 47, Settings);
    y += 60;
    ButtonAt("CRÉDITOS", 81, y, 370, 47, Credits);
    y += 60;
    ButtonAt("SALIR", 81, y, 370, 47, Exit);
    Text("ESTRATEGIA HEXAGONAL  /  DOS EJÉRCITOS  /  UN TERRITORIO", 79, 829, 12, Muted);
    Text("AwesomeEngine · Edición local", 1060, 849, 14, Muted);
    if (!message_.empty())
      Wrapped(message_, 740, 736, 610, 15);
  }
  void Game::RenderSetup() const {
    const float o = (UiWidth() - 1440.f) / 2.f;
    Text("PREPARAR LA CAMPAÑA", 65 + o, 62, 35, Gold);
    Text("Elige el escenario y cinco unidades por ejército. Haz clic en una clase para cambiarla.", 67 + o, 115, 18, Muted);
    Box({{65 + o, 173}, {485, 575}}, Panel, Edge);
    Box({{577 + o, 173}, {797, 575}}, Panel, Edge);
    Text("MODO DE PARTIDA", 88 + o, 200, 19, Gold);
    ButtonAt(std::string(mode_ == Mode::VersusAI ? "● " : "○ ") + "Contra la máquina", 88 + o, 244, 213, 44, ModeSelect, 0);
    ButtonAt(std::string(mode_ == Mode::Local ? "● " : "○ ") + "Dos jugadores", 313 + o, 244, 213, 44, ModeSelect, 1);
    Text("DIFICULTAD DE LA IA", 88 + o, 329, 18, Gold);
    for (int i = 0; i < 3; ++i)
      ButtonAt(std::string(static_cast<int>(difficulty_) == i ? "● " : "") + DifficultyName(static_cast<Difficulty>(i)), 88 + o + i * 148.f, 372, 140, 43, DifficultySelect, i, mode_ == Mode::VersusAI);
    Text("ESCENARIO", 88 + o, 461, 18, Gold);
    for (int i = 0; i < 3; ++i)
      ButtonAt(std::string(scenario_ == i ? "● " : "○ ") + std::array<std::string, 3>{"La frontera gris", "Las ciénagas", "Los tres pasos · 188 celdas"}[i], 88 + o, 501 + i * 58.f, 438, 45, ScenarioSelect, i);
    Wrapped("Tres cruces del río, ruinas y seis casas. Los mapas anteriores conservan sus 99 celdas.", 88 + o, 686, 430, 16);
    for (int team = 0; team < 2; ++team) {
      const float y = 204 + team * 245.f;
      Text(team == 0 ? "CORONA DE CENIZA" : "PACTO DEL UMBRAL", 606 + o, y, 23, Teams[team]);
      Text(team == 1 && mode_ == Mode::VersusAI ? "Controlado por la IA · " + DifficultyName(difficulty_) : "Control local", 606 + o, y + 39, 17, Muted);
      for (int slot = 0; slot < static_cast<int>(armies_[team].size()); ++slot) {
        const float x = 608 + o + slot * 146.f;
        if (units_ && !layoutOnly_) {
          const int row = static_cast<int>(armies_[team][slot]) + 7 * team, frame = static_cast<int>(animationTime_ * 9) % 8;
          sf::Sprite sprite(*units_, sf::IntRect({frame * 48, row * 64}, {48, 64}));
          sprite.setOrigin({24, 56});
          sprite.setPosition({x + 68, y + 156 + layoutYOffset_});
          sprite.setScale({2, 2});
          ctx.renderer.Draw(sprite);
        }
        ButtonAt(Name(armies_[team][slot]), x, y + 169, 136, 36, ArmySelect, team * 10 + slot);
      }
    }
    ButtonAt("VOLVER", 65 + o, 790, 210, 48, Back);
    ButtonAt("COMENZAR PARTIDA  →", 1063 + o, 790, 311, 48, StartGame);
    if (!message_.empty())
      Wrapped(message_, 310 + o, 791, 710, 17);
  }
  void Game::RenderPanel() const { RenderTacticalPanel(); }

  void Game::RenderResolutionSelect() const {
    const float x = UiWidth() / 2.f - 400.f;
    const auto current = ctx.window.Resolution();
    const int rows = std::min(ResolutionVisibleRows, static_cast<int>(resolutionOptions_.size()));
    // La lista se dibuja al final y es la única superficie interactiva mientras está abierta.
    buttons_.clear();
    ButtonAt("Resolución: " + std::to_string(current.x) + " × " + std::to_string(current.y) + "  ▴", x, 276, 386, 44, Resolution);
    Box({{x, 322}, {386, rows * 36.f + 50}}, Panel, Gold);
    for (int row = 0; row < rows; ++row) {
      const int index = resolutionScroll_ + row;
      const auto size = resolutionOptions_[index];
      ButtonAt(std::string(size == current ? "● " : "○ ") + std::to_string(size.x) + " × " + std::to_string(size.y), x + 6, 328 + row * 36.f, 374, 34, ResolutionSelect, index);
    }
    const float footer = 330.f + rows * 36.f;
    ButtonAt("▲", x + 6, footer, 60, 32, ResolutionScroll, -1, resolutionOption_ > 0);
    Text(std::to_string(resolutionScroll_ + 1) + "–" + std::to_string(resolutionScroll_ + rows) + " / " + std::to_string(resolutionOptions_.size()), x + 84, footer + 5, 16, Muted);
    ButtonAt("▼", x + 320, footer, 60, 32, ResolutionScroll, 1, resolutionOption_ + 1 < static_cast<int>(resolutionOptions_.size()));
  }

  void Game::RenderModal() const {
    buttons_.clear();
    Box({{0, -layoutYOffset_}, {UiWidth(), UiHeight()}}, sf::Color{4, 12, 14, 225});
    if (screen_ == Screen::Settings) {
      const float left = UiWidth() / 2 - 430, x = left + 30, right = left + 442;
      Box({{left, 82}, {860, 738}}, Panel, Edge);
      Text("CONFIGURACIÓN", x, 112, 30, Gold);
      Text("PANTALLA Y PRESENTACIÓN", x, 174, 19, Gold);
      Text("SONIDO", right, 174, 19, Gold);
      ButtonAt(ctx.window.IsFullscreen() ? "Pantalla completa  ·  F11" : "Modo ventana  ·  F11", x, 218, 386, 44, Fullscreen);
      const auto size = ctx.window.Resolution();
      ButtonAt("Resolución: " + std::to_string(size.x) + " × " + std::to_string(size.y) + "  ▾", x, 276, 386, 44, Resolution);
      ButtonAt(std::string("Cuadrícula: ") + (grid_ ? "visible" : "oculta"), x, 334, 386, 44, Grid);
      ButtonAt(std::string("Partículas: ") + (effects_ ? "activadas" : "desactivadas"), x, 392, 386, 44, Effects);
      ButtonAt(std::string("Ritmo de IA: ") + (aiDelay_ < 0.4f ? "rápido" : "pausado"), x, 450, 386, 44, Speed);
      ButtonAt("Maestro: " + std::to_string(volume_) + "%", right, 218, 386, 44, Volume);
      ButtonAt("Música: " + std::to_string(musicVolume_) + "%", right, 276, 386, 44, MusicVolume);
      ButtonAt("Efectos: " + std::to_string(soundVolume_) + "%", right, 334, 386, 44, SoundVolume);
      ButtonAt(muted_ ? "Restaurar sonido" : "Silenciar", right, 392, 386, 44, Mute);
      Wrapped("F11 o Alt + Enter alternan el modo de pantalla. La ventana recupera su tamaño anterior. Las resoluciones disponibles proceden del equipo.", x, 544, 382, 18);
      Wrapped("Los canales se ajustan al instante. El silencio conserva los volúmenes elegidos. Las preferencias se guardan localmente.", right, 482, 382, 18);
      ButtonAt("CONTROLES", x, 670, 386, 44, Controls);
      ButtonAt(ctx.window.IsMaximized() ? "Restaurar ventana" : "Maximizar ventana", right, 670, 386, 44, MaximizeWindow, 0, !ctx.window.IsFullscreen());
      ButtonAt("VOLVER", x, 732, 798, 48, Back);
      if (resolutionOpen_)
        RenderResolutionSelect();
      return;
    }
    const float left = UiWidth() / 2 - 340, x = left + 42;
    Box({{left, 105}, {680, 680}}, Panel, Edge);
    if (screen_ == Screen::Paused) {
      Text("PARTIDA EN PAUSA", x, 142, 30, Gold);
      ButtonAt("CONTINUAR", x, 224, 596, 46, Resume);
      ButtonAt("GUARDAR PARTIDA", x, 284, 596, 46, Save);
      ButtonAt("CARGAR PARTIDA", x, 344, 596, 46, Load, 0, canContinue_);
      ButtonAt("REINICIAR ESCENARIO", x, 404, 596, 46, Restart);
      ButtonAt("CONFIGURACIÓN", x, 464, 596, 46, Settings);
      ButtonAt("MENÚ PRINCIPAL", x, 524, 596, 46, MainMenu);
      ButtonAt("SALIR DEL JUEGO", x, 584, 596, 46, Exit);
      Wrapped(message_, x, 670, 590, 18);
    } else if (screen_ == Screen::Credits) {
      Text("CRÉDITOS", x, 150, 30, Gold);
      Wrapped("PLAGUEFRONT · Un territorio que recuerda sus batallas. Construido sobre AwesomeEngine, motor de Arukado.", x, 246, 590, 22);
      Wrapped("Código, escenarios, sprites y composiciones originales generados en este equipo. Sin recursos de terceros descargados.", x, 385, 590, 19);
      Wrapped("C++20, SFML 3.1, TGUI, nlohmann JSON, spdlog y magic_enum. Fuente local DejaVu Sans: licencia incluida en Content/Fonts.", x, 520, 590, 18);
      ButtonAt("VOLVER", x, 696, 596, 46, Back);
    } else if (screen_ == Screen::ConfirmExit) {
      Text("SALIR DE PLAGUEFRONT", x, 160, 29, Gold);
      Wrapped("Los cambios desde el último guardado se perderán. Cancela y guarda antes de salir si quieres conservarlos.", x, 290, 590, 23);
      ButtonAt("CANCELAR", x, 508, 596, 48, CancelExit);
      ButtonAt("SALIR", x, 574, 596, 48, ConfirmExit);
    } else if (screen_ == Screen::ConfirmTurn) {
      Text("FINALIZAR TURNO", x, 160, 32, Gold);
      Wrapped("Quedan " + std::to_string(match_->AvailableCount()) + " tropas disponibles. Sus órdenes pendientes se perderán al entregar el mando.", x, 285, 590, 24);
      ButtonAt("SEGUIR JUGANDO  ·  ESC", x, 508, 596, 48, CancelArmyEnd);
      ButtonAt("FIN DE TURNO  ·  ENTER", x, 574, 596, 48, ConfirmArmyEnd);
    } else if (screen_ == Screen::Handoff) {
      Text("CAMBIO DE MANDO", x, 160, 30, Gold);
      Wrapped("Entrega el control a " + match_->state.players[match_->state.currentPlayer].name + ". Sus decisiones e inventarios se mostrarán al continuar.", x, 284, 590, 24);
      ButtonAt("ESTOY LISTO  ·  ENTER", x, 575, 596, 50, Ready);
    } else if (screen_ == Screen::Victory) {
      const auto &s = match_->state;
      const bool defeat = s.mode == Mode::VersusAI && s.winner == 1;
      Text(defeat ? "DERROTA" : "VICTORIA", x, 160, 46, defeat ? sf::Color{213, 126, 99} : Gold);
      Wrapped(s.players[s.winner].name + " ha capturado la fortaleza rival. La frontera tiene un nuevo dueño.", x, 284, 590, 24);
      Text("Rondas disputadas: " + std::to_string(s.round), x, 430, 20, Muted);
      ButtonAt("VOLVER A JUGAR", x, 520, 596, 48, Restart);
      ButtonAt("MENÚ PRINCIPAL", x, 586, 596, 48, MainMenu);
    }
  }
  void Game::Render() const {
    buttons_.clear();
    layoutYOffset_ = 0;
    Box({{0, 0}, {UiWidth(), UiHeight()}}, Ink);
    if (screen_ == Screen::Menu) {
      layoutYOffset_ = (UiHeight() - UI::DesignHeight) / 2;
      RenderMenu();
    } else if (screen_ == Screen::Setup) {
      layoutYOffset_ = (UiHeight() - UI::DesignHeight) / 2;
      RenderSetup();
    } else if (screen_ == Screen::Playing)
      RenderBoard();
    else if (screen_ == Screen::Controls) {
      layoutYOffset_ = (UiHeight() - UI::DesignHeight) / 2;
      RenderControls();
    } else {
      if (match_ && screen_ != Screen::Credits && screen_ != Screen::Settings)
        RenderBoard();
      layoutYOffset_ = (UiHeight() - UI::DesignHeight) / 2;
      RenderModal();
    }
    if (!layoutOnly_ && screen_ == Screen::Playing && match_) {
      const auto cursor = ctx.cursor.GetPosition();
      for (const auto &button : buttons_)
        if (!button.enabled && button.bounds.contains(cursor)) {
          const auto reason = CheckAction(button.action, button.value);
          const float x = std::clamp(cursor.x - 390.f, 18.f, UiWidth() - 420.f), y = std::clamp(cursor.y + 22.f, 82.f, UiHeight() - 176.f);
          Box({{x, y}, {402, 100}}, Panel, Gold);
          Wrapped(reason.ok ? "Esta opción no está disponible en el contexto actual." : reason.message, x + 14, y + 12, 374, 18);
          break;
        }
    }
    layoutYOffset_ = 0;
  }
} // Espacio de nombres Plaguefront
