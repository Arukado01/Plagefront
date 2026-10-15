#pragma once
#include <SFML/Graphics/RenderWindow.hpp>
#include <SFML/Graphics/Sprite.hpp>
#include <SFML/Graphics/Texture.hpp>
#include <SFML/Graphics/View.hpp>
#include <optional>

enum class CursorContext { Normal,
                           Move,
                           Attack,
                           Capture,
                           Economy,
                           Blocked };

class CursorManager {
private:
  sf::RenderWindow &window_;
  sf::Texture texture_;
  float speed_;
  bool visible_;
  bool focused_;
  bool systemCursorVisible_{};
  CursorContext context_{CursorContext::Normal};
  sf::View view_;
  std::optional<sf::Vector2i> lastRequestedOutputPosition_;
  std::optional<sf::Vector2f> virtualPosition_;

public:
  CursorManager(sf::RenderWindow &window);
  ~CursorManager();

  void SetSpeed(float speed);
  float GetSpeed() const;

  void SetVisible(bool visible);
  bool IsVisible() const;
  void SetFocused(bool focused);
  bool IsFocused() const { return focused_; }
  bool SystemCursorVisible() const { return systemCursorVisible_; }
  void SetContext(CursorContext context) { context_ = context; }
  CursorContext GetContext() const { return context_; }

  // Dibuja el punto activo en píxeles físicos; ignoreFocus reserva la última posición inyectada para capturas de pruebas ocultas.
  void Draw(sf::RenderTarget &target, bool ignoreFocus = false) const;
  std::optional<sf::Vector2i> LastRequestedOutputPosition() const {
    return lastRequestedOutputPosition_;
  }

  void SetPosition(sf::Vector2f position);
  // La escena puede mostrar un cursor de celdas o menús sin mover el puntero del sistema.
  void SetVirtualPosition(std::optional<sf::Vector2f> position) { virtualPosition_ = position; }
  sf::Vector2f GetPosition() const;
  sf::Vector2f GetPosition(const sf::View &view) const;
  void SetView(const sf::View &view) { view_ = view; }

private:
  friend class Engine;
  void Update(float deltaTime, sf::Vector2f direction);
  void Render() const;
  void ApplyVisibility();
};
