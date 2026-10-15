#include "Core/Engine.h"
#include "Core/EngineConfig.h"
#include "Core/Overlay.h"
#include "Utils/Log.h"
#include <stdexcept>
#include <iostream>
#include <cmath>
#include <utility>

/** Constructor */
Engine::Engine(bool visible) : window_(sf::VideoMode(sf::Vector2u(gConfig.windowSize)),
                                       gConfig.windowTitle),
                               context_(window_, visible),
                               scenes_(SceneFactory::CreateScenes(context_)),
                               currentScene_(nullptr),
                               overlay_(context_.gui),
                               cursorWasVisible_(true) {

  // WindowManager reaplica la configuración compartida cuando se recrea el manejador nativo.
  EventWindowResized(window_.getSize());

  if (gConfig.disableSfmlLogs) { // Si disableSfmlLogs es true, silencia los logs propios de SFML (redirige el stream de errores a null para usar el sistema propio de logs)
    sf::err().rdbuf(nullptr);
  }

  LOG_INFO("Window created");

  context_.audio.SetGlobalVolume(gConfig.globalVolume); // Aplica el volumen global de audio definido en la configuración
  context_.input.LoadPreferences(context_.save);
  context_.scenes.ChangeScene("Menu");
}

/** Indica si el bucle principal debe seguir ejecutándose */
bool Engine::IsRunning() const { return window_.isOpen(); }

/** Devuelve verdadero si la ventana tiene el focus actualmente. */
bool Engine::HasFocus() const {
  return window_.hasFocus();
}

/** Procesa todos los eventos de la ventana pendientes en este frame */
void Engine::ProcessEvent() {
  ApplyWindowChanges();
  if (const auto nextScene = context_.scenes.FetchNextScene()) { // Si hay un cambio de escena pendiente, lo aplica antes de procesar eventos
    EventSceneChange(*nextScene);
  }

  while (const auto event = window_.pollEvent()) { // Extrae y reparte cada evento de la cola de la ventana

    ProcessWindowEvent(*event);
  }

  if (const auto selection = overlay_.FetchSelection()) { // Si el usuario seleccionó una opción del overlay de pausa, la consume y la procesa (FetchSelection devuelve vacío tras leerla)
    EventOverlaySelect(*selection);
  }
  if (context_.scenes.FetchQuitRequest())
    EventWindowClose();
  ApplyWindowChanges();
  SynchronizeWindowFocus();
}

void Engine::ProcessWindowEvent(const sf::Event &event) {
  context_.input.ProcessEvent(event);
  if (event.visit(EngineVisitor{*this}))
    return;
  context_.gui.ProcessEvent(event);
  if (!overlay_.IsVisible() && currentScene_)
    currentScene_->OnEvent(event);
}

bool Engine::ApplyWindowChanges() {
  if (!context_.window.ApplyPending())
    return false;
  EventWindowResized(window_.getSize());
  // create() sustituye el manejador nativo; la política del cursor se reaplica aunque el foco lógico coincida.
  SynchronizeWindowFocus(true);
  const sf::Event resized(sf::Event::Resized{window_.getSize()});
  context_.gui.ProcessEvent(resized);
  if (currentScene_)
    currentScene_->OnEvent(resized);
  return true;
}

void Engine::SynchronizeWindowFocus(bool reapplyCursor) {
  const bool focused = window_.hasFocus();
  if (focused != context_.cursor.IsFocused()) {
    if (focused)
      EventWindowFocusGained();
    else
      EventWindowFocusLost();
  } else if (reapplyCursor)
    context_.cursor.SetFocused(focused);
}

bool Engine::EventFullscreenShortcut(const sf::Event::KeyPressed &key) {
  const bool f11 = key.scancode == sf::Keyboard::Scan::F11 || key.code == sf::Keyboard::Key::F11;
  const bool enter = key.scancode == sf::Keyboard::Scan::Enter || key.code == sf::Keyboard::Key::Enter;
  if (!f11 && !enter)
    return false;
  const bool held = std::exchange(fullscreenKeysHeld_[f11 ? 0 : 1], true);
  if (!f11 && !key.alt)
    return false;
  if (!held)
    context_.window.ToggleFullscreen();
  return true;
}

void Engine::EventShortcutReleased(const sf::Event::KeyReleased &key) {
  if (key.scancode == sf::Keyboard::Scan::F11 || key.code == sf::Keyboard::Key::F11)
    fullscreenKeysHeld_[0] = false;
  if (key.scancode == sf::Keyboard::Scan::Enter || key.code == sf::Keyboard::Key::Enter)
    fullscreenKeysHeld_[1] = false;
}

/** Actualiza la lógica del juego una vez por frame */
void Engine::Update() {

  context_.time.Update(); // Actualiza el tiempo con el delta time del frame
  context_.input.Update(context_.time.GetDeltaTime());
  context_.cursor.Update(context_.time.GetDeltaTime(), currentScene_ && currentScene_->HandlesNavigation()
                                                           ? sf::Vector2f{}
                                                           : context_.input.PointerDirection());

  if (!overlay_.IsVisible()) { // Actualizamos la escena solo si el overlay está oculto (con la pausa abierta el juego se congela)
    currentScene_->Update(); // Actualiza la lógica de la escena actual
  }
}

/** Dibuja el frame completo en la ventana */
void Engine::Render() {
  ApplyWindowChanges();
  SynchronizeWindowFocus();
  context_.renderer.ConfigureOutput(window_.getSize());
  context_.cursor.SetView(context_.renderer.View());
  window_.clear(); // Limpia el frame anterior

  context_.renderer.BeginDrawing(); // Renderiza la escena a un buffer intermedio ->
  currentScene_->Render();
  const auto &texture = context_.renderer.FinishDrawing();
  completedFrame_ = &texture;
  sf::Sprite frame(texture);
  window_.setView(sf::View(sf::FloatRect({0.f, 0.f}, sf::Vector2f(window_.getSize()))));
  frame.setScale({static_cast<float>(window_.getSize().x) / texture.getSize().x,
                  static_cast<float>(window_.getSize().y) / texture.getSize().y});
  window_.draw(frame); // Amplía el lienzo lógico por vecino más cercano, incluido el modo pixel art.

  context_.gui.Render(); // Encima de la escena se dibuja la GUI
  context_.cursor.Render(); // y también el cursor personalizado

  window_.display(); // Muestra el frame terminado
}

/** Evento: el usuario cerró la ventana */
void Engine::EventWindowClose() {
  context_.cursor.SetFocused(false);
  window_.close();
  LOG_INFO("Window closed");
}

/** Evento: la ventana cambió de tamaño */
void Engine::EventWindowResized(sf::Vector2u size) {
  if (size.x == 0 || size.y == 0)
    return;
  context_.window.OnResized(size);
  context_.renderer.ConfigureOutput(size);
  context_.cursor.SetView(context_.renderer.View());
  window_.setView(sf::View(sf::FloatRect({0.f, 0.f}, sf::Vector2f(size))));
  LOG_INFO("Window resized to: {}x{}", size.x, size.y);
}

bool Engine::SaveScreenshot(const std::string &filename) const {
  const auto &frame = completedFrame_ ? *completedFrame_ : context_.renderer.FrameTexture();
  return context_.screenshot.SaveFrame(frame, context_.renderer.OutputSize(), filename,
                                       [this](sf::RenderTarget &target) { context_.cursor.Draw(target, true); });
}

void Engine::SmokeStep(int step) {
  if (step == 49)
    SmokeConsumers();
  currentScene_->SmokeStep(step);
  if (step == 21)
    SmokeWindowTransitions();
}

void Engine::SmokeConsumers() {
  int checks = 0;
  const auto check = [&](bool value, const char *message) {
    ++checks;
    if (!value)
      throw std::runtime_error(message);
  };
  const auto clickerBefore = context_.save.Get("Clicker:Best Time", nlohmann::json{});
  const auto bounceBefore = context_.save.Get("Bounce:High Score", nlohmann::json{});
  for (const auto &name : {std::string{"Clicker"}, std::string{"Bounce"}}) {
    EventSceneChange(name);
    check(!context_.renderer.ResponsiveLayout(), "Legacy consumer inherited the responsive tactical canvas");
    check(context_.renderer.LogicalSize() == gConfig.windowSize, "Legacy consumer lost its configured coordinate space");
    // Aleja el puntero del blanco de Clicker durante la prueba automática para conservar puntuaciones y récords.
    context_.cursor.SetVirtualPosition(sf::Vector2f{4.f, 4.f});
    Update();
    Render();
    check(window_.isOpen(), "Legacy consumer closed the shared engine window");
    check(context_.renderer.OutputSize() == window_.getSize(), "Legacy consumer lost the physical render output");
    const auto point = context_.renderer.LogicalToPixel(gConfig.windowSize / 2.f);
    const auto logical = context_.renderer.PixelToLogical(point);
    check(std::abs(logical.x - gConfig.windowSize.x / 2.f) < 1.f &&
              std::abs(logical.y - gConfig.windowSize.y / 2.f) < 1.f,
          "Legacy consumer cursor mapping disagreed with drawing");
    context_.cursor.SetFocused(true);
    check(context_.cursor.SystemCursorVisible() == !context_.cursor.IsVisible(), "Focused legacy cursor fallback changed");
    check(context_.cursor.IsVisible() == (name == "Clicker"), "Legacy scene lost its intentional custom cursor visibility");
    SynchronizeWindowFocus(true);
    const bool mode = context_.window.IsFullscreen();
    for (int shortcut = 0; shortcut < 2; ++shortcut) {
      sf::Event::KeyPressed pressed{};
      pressed.scancode = shortcut == 0 ? sf::Keyboard::Scan::F11 : sf::Keyboard::Scan::Enter;
      pressed.code = shortcut == 0 ? sf::Keyboard::Key::F11 : sf::Keyboard::Key::Enter;
      pressed.alt = shortcut != 0;
      ProcessWindowEvent(sf::Event(pressed));
      check(ApplyWindowChanges(), "Legacy fullscreen shortcut did not apply through Engine");
      check(context_.cursor.IsFocused() == window_.hasFocus(), "Legacy cursor kept focus from the old native window");
      check(context_.renderer.LogicalSize() == gConfig.windowSize, "Fullscreen changed legacy scene coordinates");
      sf::Event::KeyReleased released{};
      released.scancode = pressed.scancode;
      released.code = pressed.code;
      ProcessWindowEvent(sf::Event(released));
    }
    check(context_.window.IsFullscreen() == mode, "Legacy fullscreen shortcuts did not restore the initial mode");
    Render();
    check(SaveScreenshot("build/screenshots/phase5-" + std::string(name == "Clicker" ? "clicker" : "bounce") + ".png"),
          "ScreenshotManager could not capture the legacy consumer");
  }
  check(context_.save.Get("Clicker:Best Time", nlohmann::json{}) == clickerBefore,
        "Consumer compatibility fixture changed the user's Clicker record");
  check(context_.save.Get("Bounce:High Score", nlohmann::json{}) == bounceBefore,
        "Consumer compatibility fixture changed the user's Bounce record");
  context_.cursor.SetVirtualPosition({});
  EventSceneChange("Menu");
  check(context_.renderer.ResponsiveLayout(), "Returning to Plaguefront did not restore its responsive policy");
  std::cout << "Engine consumers: " << checks << " checks passed; actual Clicker and Bounce Start/Update/Render, shared-window captures and unchanged records\n";
}

void Engine::SmokeWindowTransitions() {
  const auto initialFullscreen = context_.window.IsFullscreen();
  const auto initialWindowedSize = context_.window.WindowedSize();
  const auto initialResolution = context_.window.Resolution();
  int checks = 0;
  const auto check = [&](bool valid, const char *message) {
    ++checks;
    if (!valid)
      throw std::runtime_error(message);
  };
  const auto checkCursorOutput = [&] {
    const auto savedVisibility = context_.cursor.IsVisible();
    context_.cursor.SetVisible(true);
    const auto size = window_.getSize();
    sf::RenderTexture reference(size);
    sf::Texture captured(size);
    for (const auto pixel : {sf::Vector2i(size / 4u), sf::Vector2i(size * 3u / 4u)}) {
      context_.cursor.SetVirtualPosition(context_.renderer.PixelToLogical(pixel));
      reference.clear(sf::Color::Black);
      context_.cursor.Draw(reference, true);
      reference.display();
      // Captura el back buffer de la misma ventana: su vista predeterminada conserva el tamaño de creación.
      window_.clear(sf::Color::Black);
      context_.cursor.Draw(window_, true);
      captured.update(window_);
      const auto expected = reference.getTexture().copyToImage();
      const auto actual = captured.copyToImage();
      bool matches = true;
      bool visible = false;
      const auto *expectedPixels = expected.getPixelsPtr();
      const auto *actualPixels = actual.getPixelsPtr();
      // El formato alfa del framebuffer de la ventana puede diferir del de RenderTexture.
      for (std::size_t offset = 0; offset < static_cast<std::size_t>(size.x) * size.y * 4; offset += 4) {
        visible |= expectedPixels[offset] || expectedPixels[offset + 1] || expectedPixels[offset + 2];
        matches &= actualPixels[offset] == expectedPixels[offset] &&
                   actualPixels[offset + 1] == expectedPixels[offset + 1] &&
                   actualPixels[offset + 2] == expectedPixels[offset + 2];
      }
      check(visible && matches, "Native window cursor pixels differed from current-size render texture");
    }
    context_.cursor.SetVirtualPosition({});
    context_.cursor.SetVisible(savedVisibility);
  };
  check(!initialFullscreen, "External resize fixture requires windowed mode");
  // El redimensionamiento nativo debe actualizar preferencias, destino y transformación mediante el evento del sistema.
  window_.setSize({1280, 720});
  const auto resized = window_.getSize();
  EngineVisitor{*this}(sf::Event::Resized{resized});
  check(resized == sf::Vector2u{1280, 720}, "Native window resize was not applied");
  check(context_.window.WindowedSize() == resized && context_.renderer.OutputSize() == resized,
        "Native resize did not update window preferences and render target");
  for (const sf::Vector2i pixel : {sf::Vector2i{0, 0}, sf::Vector2i{640, 360}, sf::Vector2i{1279, 719}}) {
    const auto logical = context_.renderer.PixelToLogical(pixel);
    check(context_.renderer.LogicalToPixel(logical) == pixel, "UI coordinates did not round trip after native resize");
    const auto sfml = window_.mapPixelToCoords(pixel, context_.renderer.View());
    check(std::abs(sfml.x - logical.x) < 0.001f && std::abs(sfml.y - logical.y) < 0.001f,
          "SFML mouse mapping and renderer mapping disagreed after native resize");
  }
  checkCursorOutput();
#ifdef _WIN32
  const auto pumpNativeEvents = [&] {
    while (const auto event = window_.pollEvent())
      ProcessWindowEvent(*event);
    check(context_.renderer.OutputSize() == window_.getSize(), "Native resize did not update the cursor's render output");
  };
  check(context_.window.Maximize(), "Native maximize was not applied");
  pumpNativeEvents();
  checkCursorOutput();
  check(context_.window.Restore(), "Native restore was not applied");
  pumpNativeEvents();
  check(window_.getSize() == resized, "Native restore did not recover the resized window dimensions");
  checkCursorOutput();
#endif
  window_.setSize(initialWindowedSize);
  EngineVisitor{*this}(sf::Event::Resized{window_.getSize()});
  check(window_.getSize() == initialWindowedSize && context_.window.WindowedSize() == initialWindowedSize,
        "Native resize did not restore the prior window before fullscreen checks");
  for (int shortcut = 0; shortcut < 2; ++shortcut) {
    for (int toggle = 0; toggle < 2; ++toggle) {
      sf::Event::KeyPressed event;
      event.scancode = shortcut == 0 ? sf::Keyboard::Scan::F11 : sf::Keyboard::Scan::Enter;
      event.code = shortcut == 0 ? sf::Keyboard::Key::F11 : sf::Keyboard::Key::Enter;
      event.alt = shortcut == 1;
      ProcessWindowEvent(sf::Event(event));
      check(context_.window.IsFullscreen() == (toggle == 0 ? !initialFullscreen : initialFullscreen),
            "Fullscreen shortcut did not request the expected mode");
      const auto revision = context_.window.Revision();
      for (int repeat = 0; repeat < 8; ++repeat)
        ProcessWindowEvent(sf::Event(event));
      check(context_.window.Revision() == revision, "Held fullscreen key toggled repeatedly before applying the mode");
      // Reproduce el foco cacheado de la ventana anterior sin depender del foco físico del runner oculto.
      context_.cursor.SetFocused(!window_.hasFocus());
      check(ApplyWindowChanges(), "Fullscreen shortcut did not apply a window transition");
      check(window_.isOpen(), "Window closed during fullscreen transition");
      check(context_.renderer.OutputSize() == window_.getSize(), "Render target did not follow display mode");
      check(context_.cursor.IsFocused() == window_.hasFocus(), "Fullscreen did not synchronize the cursor with native focus");
      check(context_.cursor.SystemCursorVisible() == (!context_.cursor.IsVisible() || !window_.hasFocus()),
            "Native cursor policy disagreed with focus after recreating the window");
      ProcessWindowEvent(sf::Event(event));
      check(context_.window.Revision() == revision, "Held fullscreen key toggled again after window recreation");
      for (const sf::Vector2i pixel : {sf::Vector2i{0, 0}, sf::Vector2i(window_.getSize() / 2u), sf::Vector2i(window_.getSize() - sf::Vector2u{1, 1})}) {
        const auto logical = context_.renderer.PixelToLogical(pixel);
        check(context_.renderer.LogicalToPixel(logical) == pixel, "Fullscreen cursor conversion lost a physical pixel");
        const auto sfml = window_.mapPixelToCoords(pixel, context_.renderer.View());
        check(std::abs(logical.x - sfml.x) < 0.001f && std::abs(logical.y - sfml.y) < 0.001f,
              "Fullscreen cursor and SFML views differed");
      }
      sf::Event::KeyReleased released{};
      released.scancode = event.scancode;
      released.code = event.code;
      ProcessWindowEvent(sf::Event(released));
    }
  }
  check(context_.window.WindowedSize() == initialWindowedSize, "Fullscreen changed the saved window dimensions");
  check(context_.window.Resolution() == initialResolution, "Fullscreen did not restore the prior resolution");
  const auto savedVisibility = context_.cursor.IsVisible();
  context_.cursor.SetVisible(true);
  const auto pixel = sf::Vector2i(window_.getSize() / 2u);
  context_.cursor.SetVirtualPosition(context_.renderer.PixelToLogical(pixel));
  const auto cursorPixels = [&] {
    sf::RenderTexture target(window_.getSize());
    target.clear(sf::Color::Transparent);
    context_.cursor.Draw(target);
    target.display();
    const auto image = target.getTexture().copyToImage();
    int visible = 0;
    for (int y = pixel.y - 2; y < pixel.y + 52; ++y)
      for (int x = pixel.x - 2; x < pixel.x + 52; ++x)
        visible += image.getPixel({static_cast<unsigned>(x), static_cast<unsigned>(y)}).a != 0;
    return visible;
  };
  ProcessWindowEvent(sf::Event(sf::Event::FocusLost{}));
  context_.cursor.SetVirtualPosition(context_.renderer.PixelToLogical(pixel));
  check(!context_.cursor.IsFocused() && context_.cursor.SystemCursorVisible(), "Focus loss hid both cursor policies");
  check(cursorPixels() == 0, "Custom cursor drew while unfocused");
  ProcessWindowEvent(sf::Event(sf::Event::FocusGained{}));
  context_.cursor.SetVirtualPosition(context_.renderer.PixelToLogical(pixel));
  check(context_.cursor.IsFocused() && !context_.cursor.SystemCursorVisible(), "Focus gain did not restore the custom cursor policy");
  check(cursorPixels() > 0, "Normal cursor drawing stayed invisible after focus gain");
  context_.cursor.SetVisible(false);
  check(context_.cursor.SystemCursorVisible() && cursorPixels() == 0, "Hiding the custom cursor lost the inherited system cursor fallback");
  context_.cursor.SetVisible(true);
  check(!context_.cursor.SystemCursorVisible() && cursorPixels() > 0, "Restoring the custom cursor did not draw its normal pixels");
  context_.cursor.SetVirtualPosition({});
  context_.cursor.SetVisible(savedVisibility);
  SynchronizeWindowFocus(true);
  std::cout << "Fullscreen regression: " << checks << " checks passed; resize/maximize/restore cursor pixels, both shortcuts, held keys, native focus synchronization and coordinate mappings\n";
}

/** Evento: la ventana perdió el foco (la escena se pausa) */
void Engine::EventWindowFocusLost() {
  context_.input.ProcessEvent(sf::Event(sf::Event::FocusLost{}));
  context_.cursor.SetFocused(false);
  if (currentScene_)
    currentScene_->OnPause(true);
  LOG_INFO("Window focus lost");
}

/** Evento: la ventana recuperó el foco (la escena se reanuda, salvo que el overlay esté abierto, en cuyo caso sigue pausada) */
void Engine::EventWindowFocusGained() {
  context_.input.ProcessEvent(sf::Event(sf::Event::FocusGained{}));
  context_.cursor.SetFocused(true);
  // Una liberación fuera de la ventana puede no producir KeyReleased en su cola.
  if (!sf::Keyboard::isKeyPressed(sf::Keyboard::Key::F11))
    fullscreenKeysHeld_[0] = false;
  if (!sf::Keyboard::isKeyPressed(sf::Keyboard::Key::Enter))
    fullscreenKeysHeld_[1] = false;
  if (currentScene_)
    currentScene_->OnPause(overlay_.IsVisible());
  LOG_INFO("Window focus gained");
}

/** Evento: se conectó un gamepad */
void Engine::EventGamepadConnected(int id) {
  LOG_INFO("Gamepad {} connected", id);
}

/** Evento: se desconectó un gamepad */
void Engine::EventGamepadDisconnected(int id) {
  LOG_INFO("Gamepad {} disconnected", id);
}

/** Evento: el usuario pidió una captura de pantalla */
void Engine::EventWindowScreenshot() const { context_.screenshot.Take(); }

/** Evento: cambio de escena por nombre */
void Engine::EventSceneChange(const std::string &name) {

  assert(scenes_.contains(name)); // En debug, aborta si el nombre no existe en el registro de escenas

  Scene *nextScene = scenes_.at(name).get(); // Obtiene el puntero crudo de la nueva escena (el mapa conserva el ownership)

  // Limpia la escena actual antes de salir (si existe)
  if (currentScene_) {
    currentScene_->OnCleanup();
  }

  context_.input.Clear(); // Reinicia el estado de entrada para que la nueva escena empiece limpia
  context_.renderer.SetResponsiveLayout(false); // Las demostraciones conservan sus coordenadas configuradas.

  currentScene_ = nextScene; // Cambia a la nueva escena e inicializa su estado
  currentScene_->Start();
  context_.cursor.SetView(context_.renderer.View());
}

/** Evento: reiniciar la escena actual */
void Engine::EventSceneRestart() {
  overlay_.SetVisible(false);
  context_.scenes.RestartCurrentScene();
}

/** Evento: volver al menú principal */
void Engine::EventSceneMenuReturn() {
  // Cierra el overlay de pausa antes de cambiar de escena
  overlay_.SetVisible(false);
  context_.scenes.ChangeScene("Menu");

  // Restaura el cursor del juego para navegar el menú
  context_.cursor.SetVisible(true);
  context_.cursor.SetSpeed(gConfig.cursorSpeed); // Restablece su velocidad por defecto
}

/** Evento: alterna el estado de pausa (muestra/oculta el overlay de pausa) y pausa/reanuda la escena en consecuencia */
void Engine::EventOverlayPauseToggle() {

  const bool overlayVisible = !overlay_.IsVisible(); // Calcula el nuevo estado: si estaba oculto, ahora se muestra (y viceversa)
  overlay_.SetVisible(overlayVisible);

  const bool cursorVisible = context_.cursor.IsVisible(); // Cursor: guarda su visibilidad actual antes de modificarla
  context_.cursor.SetVisible(overlayVisible || cursorWasVisible_); // Con el overlay abierto el cursor siempre se muestra (para navegar la GUI); al cerrarlo se recupera la visibilidad previa a la pausa
  cursorWasVisible_ = cursorVisible; // Recuerda el estado para restaurarlo luego

  currentScene_->OnPause(overlayVisible); // Pausa la escena al abrir el overlay; la reanuda al cerrarlo
  LOG_INFO(overlayVisible ? "Game paused" : "Game resumed");
}

/** Evento: el usuario eligió una opción del overlay de pausa. Redirige la elección al evento del motor correspondiente */
void Engine::EventOverlaySelect(OverlaySelection selection) {
  switch (selection) {
  case OverlaySelection::Resume: // Continuar el juego (cierra el overlay)
    EventOverlayPauseToggle();
    break;
  case OverlaySelection::Restart: // Reiniciar la escena actual
    EventSceneRestart();
    break;
  case OverlaySelection::Menu: // Volver al menú principal
    EventSceneMenuReturn();
    break;
  case OverlaySelection::Quit: // Cerrar la ventana y salir
    EventWindowClose();
    break;
  default:
    break;
  }
}
