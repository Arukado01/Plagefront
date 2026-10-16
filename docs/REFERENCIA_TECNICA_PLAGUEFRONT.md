# Referencia técnica de Plaguefront

Consulta del código local de AwesomeEngine y Plaguefront, revisada el **16 de octubre de 2026**. El [curso](CURSO_DESARROLLO_PLAGUEFRONT.md) desarrolla la construcción y las prácticas; esta referencia concentra contratos, valores, rutas, formatos y evidencias. Las rutas de tablas son relativas a la raíz. Los recursos iniciales preservados no describen el renderizador actual. La sección 7.8 registra la verificación final de las correcciones visuales y separa sus resultados de los históricos.

- [1. Inventario](#1-inventario)
- [Auditoría A/B/C y procedencia del motor](#auditoria-abc)
- [2. Contratos y gestores](#2-contratos-y-gestores)
- [3. Configuración y reglas numéricas](#3-configuración-y-reglas-numéricas)
- [4. Formatos y validaciones](#4-formatos-y-validaciones)
- [5. Entrada y preferencias](#5-entrada-y-preferencias)
- [6. Recursos y licencias](#6-recursos-y-licencias)
- [7. Compilación y pruebas](#7-compilación-y-pruebas)
- [8. Diagnóstico y límites](#8-diagnóstico-y-límites)
- [9. Índice de capturas](#9-índice-de-capturas)
- [10. Evolución y consolidación](#10-evolución-y-consolidación)

## 1. Inventario

### 1.1. Archivos de Plaguefront

| Archivo | Responsabilidad y símbolos de entrada |
| --- | --- |
| `Games/Include/Plaguefront/Rules.h` | API pública de `Match`, geometría, nombres y consultas; incluye tipos/configuración sin SFML. |
| `Games/Include/Types/PlaguefrontRulesTypes.h` | `Hex`, enumeraciones tácticas, `Stats`, `Cell`, `Player`, `Unit`, `Corpse`, `Source`, `FireZone`, `CombatEvent`, `GameState`, `Path`, `Result`. |
| `Games/Include/Config/PlaguefrontRulesConfig.h` | `Config`: clases, precios, terreno y parámetros de reglas. |
| `Games/Source/Plaguefront/Rules.cpp` | Constructor de balance, geometría, estadísticas efectivas, rutas, visión, combate, activación, captura, economía, ambiente y clima. |
| `Games/Source/Plaguefront/Persistence.cpp` | `Config::Load`, `Match::Create/Validate/Save/Load`, parsers y escritura de instantáneas. |
| `Games/Source/Plaguefront/AI.cpp` | `Match::StepAI`, `PositionScore`, `ActionScore` y `Threat`; simula órdenes públicas sobre copias. |
| `Games/Include/Plaguefront/Presentation.h` | `Camera` y su orientación/profundidad, `AnimationDirector`, `GroundDepth`, `DrawOrder`, `InsideTop`, `RotPresentationAt`. |
| `Games/Include/Types/PlaguefrontPresentationTypes.h` | `Point`, `Rect`, `AnimState`, `EffectType`, `UnitVisual`, `VisualEffect`, `RotExposure`. |
| `Games/Include/Config/PlaguefrontPresentationConfig.h` | Base de proyección, superficie, `AnimationTiming` y `RotVisual`. |
| `Games/Source/Plaguefront/Presentation.cpp` | Cámara afín, selección elevada, observación de instantáneas, colas de clips y efectos; sin SFML. |
| `Games/Include/Plaguefront/Weather.h` | `RainDrop/RainDrops`: trazos deterministas de lluvia por tamaño nativo y tiempo visual; sin reglas meteorológicas. |
| `Games/Include/Plaguefront/SpellPresentation.h` | `SpellStage/SpellSample/SampleSpell`: fases normalizadas; `FireAwaitsContact` coordina la visibilidad de incendios recién creados. |
| `Games/Source/Plaguefront/SpellRenderer.cpp` | `Game::RenderSpellEffect`: preparación, proyectiles, enlaces, impactos y desvanecimiento mediante píxeles y atlas. |
| `Games/Include/Plaguefront/Game.h` | `Game : Scene`: ciclo y estado privado de interacción/presentación. |
| `Games/Include/Types/PlaguefrontGameTypes.h` | `GameScreen`, once `TacticalContext`, `GameAction`, `Button`, `CampaignSetup`, `LegalityKey`. |
| `Games/Include/Config/PlaguefrontGameConfig.h` | Dimensiones de interfaz y escala externa del mundo. |
| `Games/Source/Plaguefront/Game.cpp` | `Start/Update/OnEvent`, `Dispatch`, `HandleCell`, `Apply`, `SyncPresentation`, preparación y preferencias. |
| `Games/Source/Plaguefront/Contexts.cpp` | `SetContext/CancelContext`, `CheckAction`, `PreviewRules`, `UpdatePreview`, layout/cámara y cachés. |
| `Games/Source/Plaguefront/Interface.cpp` | Texto a resolución de salida, botones y geometría compartida, menú, preparación, configuración con resolución desplegable y modales. |
| `Games/Source/Plaguefront/TacticalPanel.cpp` | Órdenes, habilidades, objetivos, inventario, administración, reclutamiento, tienda, construcción y compra. |
| `Games/Source/Plaguefront/TacticalOverlay.cpp` | Lista desplazable, estadísticas/HP/MP, barras de captura, etiquetas y alertas ambientales reales. |
| `Games/Source/Plaguefront/PixelRenderer.cpp` | `RenderBoard/RenderWorld/RenderWorldLabels`, superficies/laterales, profundidad según cámara, atlas, follaje al señalar, flechas de ruta, contaminación, efectos y `Skin` de nueve regiones. |
| `Games/Source/Plaguefront/Gamepad.cpp` | `UpdateGamepad`, foco geométrico, cursor hexagonal y `RenderControls/AdjustControls`. |
| `Games/Include/Plaguefront/Audio.h`, `Games/Source/Plaguefront/Audio.cpp` | `AudioDirector`: manifiesto, música por pantalla, observación y cues temporizados. |
| `Games/Source/Plaguefront/Smoke.cpp` | `Game::SmokeStep`: 28 estados gráficos originales y regresiones UI/cámara/preferencias. |
| `Games/Source/Plaguefront/Phase4Smoke.cpp` | `SmokePhase4`: 21 estados de turnos, combate, captura, identidad y HUD. |
| `Games/Source/Plaguefront/Phase4Controls.cpp` | Catorce tropas, lista, activaciones intercaladas, confirmación y seis cursores. |
| `Games/Source/Plaguefront/Phase5Smoke.cpp` | `SmokePhase5`: matriz de pantallas/contextos/salidas, resize, estado serializado y 22 estados finales. |
| `Games/Source/Plaguefront/GamepadSmoke.cpp` | `SmokeGamepad`, `PrepareGamepadCapture`: SFML simulado sobre la escena real. |
| `Games/Source/Plaguefront/RotSmoke.cpp` | `SmokeRotPhase5`: siete estados mediante muertes y cierres de ronda reales. |
| `Games/Source/Plaguefront/VisualCorrectionsSmoke.cpp` | `SmokeVisualCorrections`: reproducción temporal y píxeles de Idle, lluvia por viewport, etapas del Caster y ciclo global en mapas reales e irregular; se llama al terminar `SmokePhase5(21)`. |

Los números de fase en nombres de pruebas/fixtures identifican archivos existentes; no significan implementaciones alternativas del juego. La aplicación utiliza un solo núcleo vigente.

### 1.2. Integración y archivos del motor

Cada manager tiene declaración `Engine/Include/Managers/NOMBRE.h` e implementación `Engine/Source/Managers/NOMBRE.cpp`. Los trece nombres reales son **AudioManager, ClipboardManager, CursorManager, GuiManager, InputManager, RandomManager, RenderManager, ResourceManager, SaveManager, SceneManager, ScreenshotManager, TimeManager y WindowManager**. No existen GamepadManager, FontManager, ConfigManager ni Controllers adicionales.

| Archivos | Relación con Plaguefront |
| --- | --- |
| `Engine/Source/Main.cpp` | Arranque normal y `--smoke`; construye Engine y procesa/actualiza/renderiza. |
| `Engine/Include/Core/Engine.h`, `Engine/Source/Core/Engine.cpp` | Ventana principal, orden de servicios, eventos, cambios de escena y captura delegada. |
| `Engine/Include/Core/EngineContext.h` | Posee managers y entrega referencias a Scene. |
| `Engine/Include/Core/EngineConfig.h`, `Engine/Source/Core/EngineConfig.cpp` | gConfig, JSON inicial y localización de Content desde el ejecutable en Windows. |
| `Engine/Include/Core/EngineVisitor.h`, `Engine/Source/Core/EngineVisitor.cpp` | Atajos/eventos globales; respeta HandlesNavigation. |
| `Engine/Include/Core/Overlay.h`, `Engine/Source/Core/Overlay.cpp` | Panel TGUI de demostraciones; Plaguefront usa pantallas propias. |
| `Engine/Include/Scene/Scene.h` | Start/Update/Render/OnEvent/OnPause/OnCleanup, navegación y cierre. |
| `Engine/Include/Scene/SceneFactory.h`, `Engine/Source/Scene/SceneFactory.cpp` | Registra Menu→Plaguefront::Game, Clicker y Bounce. |
| `Engine/Include/Scene/SceneUtils.h` | Utilidades SFML de escenas heredadas. |
| `Engine/Include/Types/InputTypes.h`, `Engine/Include/Config/InputConfig.h` | Capacidades/acciones/perfiles y GamepadConfig. |
| `Engine/Include/utils/InputBindings.h`, `Engine/Source/Utils/InputBindings.cpp` | Enlaces anteriores y mapeo lógico/físico validado; Windows tolera utils/Utils. |
| `Engine/Include/utils/Cooldown.h`, `Engine/Source/Utils/Cooldown.cpp` | Temporizadores de demos, no rondas tácticas. |
| `Engine/Include/utils/Log.h`, `Engine/Include/utils/Verify.h` | Registro y comprobación del motor. |
| `Engine/Include/Graphics/Effect.h`, `Engine/Include/Graphics/Effects/*.h`, `Engine/Source/Graphics/Effects/*.cpp` | Bloom/Monitor/Glitch/Inverted compartidos; Plaguefront desactiva posprocesado. La implementación real se llama EfflectBloom.cpp. |

La convención procede de `Games/Include/Clicker.h`, `Games/Source/Clicker.cpp`, `Games/Include/Types/ClickerTypes.h`, `Games/Include/Config/ClickerConfig.h`: API, comportamiento, datos y parámetros. Bounce conserva `Bounce.h/.cpp`, `Types/BounceTypes.h`, `Config/BounceConfig.h`. Estos proyectos se preservan.

### 1.3. Datos, herramientas y pruebas

| Ruta | Contrato |
| --- | --- |
| `CMakeLists.txt` | Dependencias desconectadas, bibliotecas, ejecutable y nueve registros CTest. |
| `Jugar.cmd` | Cambia a la raíz y abre build/Plaguefront.exe; exige compilación previa. |
| `Tools/BuildLocal.ps1` | Entorno MSVC instalado, Release, compilación; opciones -Test/-Smoke. |
| `Tools/GenerateScenario.ps1` | Genera Los tres pasos sin cambiar los otros mapas. |
| `Tools/GeneratePixelArt.ps1` | Entrada pública al paquete raster aprobado; `-Legacy` activa expresamente el generador procedural histórico. |
| `Tools/Art/BuildApprovedArt.ps1` | Reconstrucción local de unidades, entorno, impactos mágicos y portada desde fuentes conservadas; conserva los retratos existentes; Python con Pillow/NumPy. |
| `Tools/Art/BuildUnits.py`, `BuildIdle.py`, `BuildEnvironment.py`, `BuildCasterEffects.py` | Empaquetado y acabado nativo de cada componente; la recuperación de Idle copia exactamente sus 448 celdas aprobadas y conserva las otras 2240; los impactos conservan las columnas ajenas a magia. |
| `Tools/Art/Sources/` | PNG originales y procedencia de unidades, entorno, impactos mágicos y portada; `Units/Approved/approved-atlas.png` conserva la identidad del Idle recuperado. Las fuentes de intentos posteriores no implican integración. |
| `Tools/GenerateArt.ps1` | Cinco PNG iniciales preservados, semilla 731051. |
| `Tools/GenerateAudio.ps1` | Síntesis local de 42 WAV y manifiesto. |
| `Tests/PlaguefrontTests.cpp` | Núcleo: 18 grupos. |
| `Tests/CampaignTests.cpp` | Campañas, conquista local y regresiones de economía/ambiente. |
| `Tests/PresentationTests.cpp` | Cámara, animación/eventos, exposición ambiental, dos ciclos Idle, geometría de lluvia y etapas mágicas; sin ventana SFML. |
| `Tests/Phase3Tests.cpp` | Viewports, rutas, poses, 188 celdas y campañas del mapa grande. |
| `Tests/Phase4Tests.cpp` | Turnos, ingresos, respuesta, Lightning, Fire, captura, migración y capacidad de eventos. |
| `Tests/AudioTests.cpp` | WAV reales, manager y director; reproducción silenciada. |
| `Tests/EngineManagerTests.cpp`, `Tests/InputTests.cpp` | Servicios gráficos/documentos/recursos/capturas y entrada determinista. |
| `Content/Config.json`, `Content/Save.json` | Configuración inicial y almacén de claves del motor. |
| `Content/Plaguefront/balance.json` | Balance editable de partidas nuevas. |
| `Content/Plaguefront/scenarios/{frontier,marsh,highlands}.json` | Escenarios explícitos. |
| `Content/Plaguefront/art.json`, `audio.json` | Contratos de recursos y manifiesto sonoro. |
| `Content/Textures/Plaguefront/Pixel/`, `Content/Textures/Plaguefront/` | Recursos actuales e iniciales, relacionados en sección 6. |
| `Content/Fonts/{Caudex-Regular.ttf,CaudexLicense.txt,DejaVuSans.ttf,DejaVuSansLicense.txt}` | Tipografías y avisos legales; Relidux permanece como recurso del motor. |
| `Content/Audio/Plaguefront/{Music,Sfx,Ambient}/` | 42 WAV enumerados en sección 6. |
| `Saves/plaguefront.json`, `Saves/settings.json` | Se crean al guardar; no se requieren para comenzar. |
| `build/test-output/`, `build/screenshots/`, `build/Testing/` | Resultados de pruebas, PNG y registros. |
| `build/visual-corrections/` | Baseline, verificaciones de recursos y resultados temporales nuevos; el estado de ejecución se registra en 7.8. |
| `.vscode/launch.json`, `.vscode/tasks.json`, `.vscode/settings.json` | Configuración heredada/local; no certifica una ruta Debug/F5. |
| `README.md`, `docs/CURSO_DESARROLLO_PLAGUEFRONT.md`, este documento | Entrada, curso y consulta; LICENSE.md conserva la licencia del repositorio. |

<a id="auditoria-abc"></a>

### 1.4. Auditoría verificable A/B/C0

Esta matriz conserva el resultado previo a los ajustes posteriores de paleta y pantalla completa: su C es el snapshot C0. C1 añade las correcciones descritas en las lecciones 30–31. C2 es el juego vigente e incorpora además el arreglo posterior de restauración/fullscreen por monitor y los cuatro ajustes de presentación de la [lección 32](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-32); sus contratos y verificaciones se registran por separado.

**A** significa el árbol del commit local `19e1162fe338eb46e9dd0b2c563d3eebfb1572d9`; **B**, la diferencia neta entre ese árbol y el working tree; **C**, la implementación vigente inspeccionada. A se recuperó con `git show`/`git ls-tree`, sin descargar otro motor. El historial local prueba esos dos extremos; las integraciones intermedias de Plaguefront no tienen commits propios. Los snapshots de `build` sirven para contrastar fases documentadas, pero no permiten atribuir cada hunk a una fecha o a un commit inexistente. La revisión didáctica actual no vuelve a modificar ese código.

En A, `Engine::Engine` agenda **Clicker**, `SceneFactory::CreateScenes` sólo registra **Bounce/Clicker**, `Content/Config.json` configura **800×800** y hay **doce managers**. Ya existen el ciclo eventos→update→render, escenas, `EngineContext`, enlaces de teclado/ratón/mando, recursos/fuentes/música, JSON de récords, screenshots con sonido, overlay TGUI y efectos. Se añade `WindowManager` como decimotercer manager. No se atribuyen al juego esos servicios preexistentes. La política heredada conserva el espacio definido por `gConfig.windowSize`: en el resultado actual ese valor es **1440×900**, después del cambio explícito de Config, no un tamaño histórico del motor original.

Para comprobar el origen desde la raíz, sin modificar archivos:

```powershell
git rev-parse 19e1162
git show 19e1162:Engine/Source/Core/Engine.cpp
git show 19e1162:Engine/Source/Scene/SceneFactory.cpp
git show 19e1162:Content/Config.json
git diff -- Engine/Source/Managers/ResourceManager.cpp
```

El [catálogo de todas las rutas](../build/reconstruccion-catalogo-git.json) contiene **223 archivos: 114 creados, 31 modificados, 78 reutilizados sin diferencias Git y ninguno eliminado**, respecto de A. «Creado» aquí es procedencia frente al commit, no creación durante la tarea documental; los documentos y el juego ya estaban en el working tree al comenzarla. La clasificación de «reutilizado» utiliza `git diff`, que puede normalizar finales de línea. La comparación SHA256 de la intervención es independiente. El [inventario original](../build/reconstruccion-original-inventario.txt) y el [parche exacto de Engine, Config y CMake](../build/reconstruccion-engine.patch) permiten cotejar las rutas y cambios; el curso proporciona explicación y reconstrucción por etapas, sin exigir adivinar el parche.

El [lote compartido de la etapa 1](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-1) incorpora las capacidades del motor antes de usarlas desde el juego; la última columna localiza su uso progresivo y comprobación. Engine final agenda Menu; el checkpoint 1 usa el alias didáctico `Menu`→`Clicker::Game`, sustituido por `Menu`→`Plaguefront::Game` en la etapa 2. El Main inicial de la práctica verifica un frame; el runner final de 71 pantallas y los registros CTest se activan cuando existen sus archivos. Los helpers Smoke de Engine pueden compilar antes sin ejecutarse. Las fichas del curso distinguen este orden didáctico del parche neto completo A→C.

| Familia / archivos afectados | A comprobado | B y contrato vigente C | Uso progresivo y regresión |
| --- | --- | --- | --- |
| `WindowManager.h/.cpp` | No existen; Engine configura directamente la ventana y resize sólo registra | Servicio nuevo sobre la misma `RenderWindow`; solicitudes diferidas, resoluciones, fullscreen, preferencias y maximizar/restaurar | [Etapa 8](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-8), [lección 8](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-8); `EngineManagers`, smoke |
| `RenderManager.h/.cpp` | Targets fijos, efectos, `Draw/SetView/ResetView`; SetView sólo modifica target | Métricas, política Legacy/Responsive, revisión, escala, vista activa y conversiones; efectos conmutables | [Etapa 8](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-8), [lecciones 7–8](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-7); comparación SFML y layout |
| `InputManager.h/.cpp`, `InputBindings.h/.cpp` | `Bind/Pressed/Clear`, polling y mapping Microsoft/resto | Ampliación del mismo manager con estado de eventos, capacidades, perfiles, flancos, repetición, foco, hotplug y preferencias | [Etapa 8](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-8), [lección 24](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-24); `InputManager`, smoke UI |
| `Types/InputTypes.h`, `Config/InputConfig.h` | No existen; enum global `GamepadButton` dentro de InputBindings | Archivos nuevos; traslado del enum conservando nombre/orden y datos/configuración añadidos | [Etapa 8](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-8), [lección 24](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-24); compatibilidad `Bind/Pressed` |
| `CursorManager.h/.cpp` | Círculo, joystick 0 directo y `defaultView` | Atlas contextual, foco/vista, dirección recibida de InputManager y posición virtual; conserva métodos públicos anteriores | [Etapa 8](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-8), [lección 24](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-24); seis contextos y consumidores |
| `AudioManager.h/.cpp` | Envoltorio de Listener, volumen/posición/dirección | Cues, buffers/streams, buses, cooldown, silencio y pool de doce voces; API espacial conservada | [Etapa 7](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-7), [lección 23](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-23); `PlaguefrontAudio` |
| `SaveManager.h/.cpp` | Claves JSON en `Content/Save.json`, escritura al destruir si no vacío | Ruta inyectable, documentos genéricos, fallback, dirty/Flush y reemplazo temporal; récords preservados | [Etapa 9](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-9), [lección 25](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-25); documentos, Clear y JSON inválido |
| `ResourceManager.cpp` | FetchTexture/FetchSound/FetchFont/FetchMusic ya existen; `operator[]` inserta antes de validar | Tres cargas ahora insertan sólo tras éxito. Header y FetchMusic sin cambios | [Etapa 7](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-7), [lección 21](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-21); petición faltante repetida |
| `GuiManager.cpp` | Resize escala inversa respecto de defaultView | Sólo cambia handler resize: ignora cero y conserva viewport físico completo | [Etapa 8](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-8), [lección 8](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-8); compatibilidad overlay |
| `Scene.h`, `SceneManager.h` | Ciclo/cleanup opcionales y cambio diferido; `.cpp` del manager intacto | Tres virtuales con defaults y quit consumible; no se reescribe el sistema de escenas | [Etapa 2](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-2), [lecciones 2–3](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-2); demos/confirmaciones |
| `EngineContext.h` | Doce servicios, screenshot/cursor/gui reciben ventana | Añade WindowManager primero y visibilidad opcional; misma referencia compartida | [Etapa 2](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-2), [lección 2](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-2); compilación consumidores |
| `EngineConfig.h/.cpp`, `Content/Config.json` | JSON relativo a cwd, assert de apertura, 800×800 | Referencia/mínimo/icono configurables, excepción de apertura y búsqueda desde ejecutable Windows; arranque 1440×900 | [Etapa 0](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-0), [lecciones 1/25](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-1); runner desde build |
| `ScreenshotManager.h/.cpp` | Take, fecha, PNG, directorio y sonido | SaveFrame/SaveWindow/Encode y fecha segura; único codificador, API Take preservada | [Etapa 9](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-9), [lección 26](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-26); frame+overlay y PNG reales |
| `Engine.h/.cpp` | Inicio Clicker y ciclo existente; resize sólo log | Integra ventana/input/vista/foco y captura; inicio Menu; hooks de pruebas y política por escena | [Etapas 2](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-2), [8](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-8) y [9](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-9), [lección 3](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-3); aplicación real |
| `EngineVisitor.cpp` | Cierre directo y atajos globales de demos | Negocia cierre, añade fullscreen y respeta HandlesNavigation para teclado/mando; F12 global | [Etapa 8](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-8), [lección 24](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-24); F11/Alt+Enter y retornos |
| `SceneFactory.cpp` | Dos includes/registros Bounce y Clicker | Sólo añade include y registro Menu→Plaguefront; demos intactos | [Etapa 2](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-2), [lección 2](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-2); SmokeConsumers |
| `Main.cpp` | Bucle normal y GPU preference | Firma argc/argv y rama --smoke antes del mismo bucle normal | [Etapa 9](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-9), [lección 26](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-26); CTest smoke |
| `CMakeLists.txt` | Versiones ya actuales, glob y un ejecutable; sin CTest | Offline, bibliotecas puras, filtro para evitar definiciones duplicadas, UTF-8/showIncludes y nueve suites | [Etapas 0](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-0) y [9](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-9), [lección 1](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-1); configurar/compilar/CTest |

El conjunto completo de Plaguefront (`Games/Include/Plaguefront`, `Games/Source/Plaguefront`, sus Types/Config), `Tests`, `Tools`, `Jugar.cmd`, datos/arte/audio/fuentes añadidos y documentación **no figura en A**. Son módulos específicos, pruebas, herramientas o contenido nuevos frente al motor. `Rules.cpp`, `Persistence.cpp` y `AI.cpp` no son archivos originales modificados: se crean para el juego y permanecen sin cambios durante esta revisión didáctica. [Etapas 3–6](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-3) los reconstruyen; [7–9](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-7) los conectan con servicios.

Los ocho archivos de Clicker/Bounce, `SceneFactory.h`, `SceneManager.cpp`, `ResourceManager.h`, Clipboard/Random/Time, Overlay, efectos/shaders y recursos heredados se reutilizan sin diferencias Git. También permanecen `.vscode/tasks.json`, `settings.json` y otros auxiliares; no certifica que sus rutas Debug/F5 funcionen en otro equipo. Los cambios de soporte restantes frente a A están en `.gitignore`, `.vscode/launch.json` y `README.md`, además de CMake/Config ya detallados. La relación nominal y SHA256 actual de cada ruta está en el catálogo.

Los [213 hashes de código/configuración/recursos](../build/reconstruccion-integridad-auditoria.json) comparados con el manifiesto previo a esta tarea coinciden en la auditoría: **213/213**. Esta prueba protege el alcance documental actual; la autoridad de reglas en C continúa siendo `Match`. Los registros de ejecución histórica se conservan con sus fechas en secciones 7 y 10; documentar una prueba no equivale a ejecutarla durante esta intervención.

## 2. Contratos y gestores

### 2.1. Autoridad y consumidores

| Componente | Autoridad / uso demostrado |
| --- | --- |
| Match | Única autoridad de HP, posiciones, activaciones, dinero, efectos y ganador; UI e IA ejecutan sus órdenes. |
| Game | Selección, once contextos, intención pendiente, cámara y ritmo; Apply observa resultados aceptados. |
| Camera | Proyección/superficies/picking; no concede movimiento. |
| AnimationDirector | Clips/efectos por ID observados de before/after; no aplica daño. |
| AudioDirector | Pantallas/cambios/eventos→cues; no condiciona turnos. |
| WindowManager | Misma RenderWindow; cambios solicitados/aplicados entre fotogramas. |
| RenderManager | Textura principal, vista activa, métricas y conversiones. |
| InputManager | Descubrimiento, capacidades, eventos, ejes, flancos, dispositivo y preferencias; Bind/Pressed/Clear siguen para demos. |
| CursorManager | Cursor contextual único; posición virtual para mando sin mover puntero del sistema. |
| ResourceManager | Caché compartida; FetchTexture/FetchFont alimentan Game y sólo insertan cargas correctas. |
| AudioManager | Cues, buffers, streams, buses y doce voces; lo usa AudioDirector. |
| ScreenshotManager | Composición y único codificador PNG; Engine delega, F12 usa Take. |
| SaveManager | Récords y documentos de preferencias; el esquema táctico pertenece a Match. |
| SceneManager | Cambio diferido y RequestQuit; Dispatch no destruye la ventana. |
| TimeManager | Delta que recibe Game para animaciones/IA/audio, sin reloj táctico paralelo. |
| GuiManager | TGUI/overlay/eventos compartidos; UI táctica dibujada directamente por Game. |
| RandomManager | Clicker/Bounce; no decide el clima persistido de Match. |
| ClipboardManager | Disponible en EngineContext, sin consumidor activo en Plaguefront. |

La auditoría de la integración previa, conservada en `build/phase5-baseline`, detectó PNG duplicado, joystick consultado independientemente por cursor, SetView desincronizado con entrada y caché con cargas fallidas. Ese snapshot intermedio no es el estado A: el original ya tenía ScreenshotManager como propietario de Take, sin los helpers de captura añadidos después al integrar el juego. El estado vigente concentra PNG en ScreenshotManager, ejes en InputManager, vista/conversión en RenderManager y fuentes en ResourceManager. No se añadió otro servicio paralelo.

### 2.2. Contratos públicos de integración

- WindowManager::SetFullscreen/ToggleFullscreen/SetResolution/SetFullscreenResolution solicitan cambios; ApplyPending los realiza. LoadPreferences/StorePreferences manejan campos gráficos. Maximize/Restore/IsMaximized utilizan Windows y devuelven falso fuera de esa plataforma.
- RenderManager::SetResponsiveLayout tiene valor inicial falso, compatible con Clicker/Bounce. Game pide verdadero y SetPixelScale (1). ConfigureOutput actualiza métricas/texturas/revisión. SetView sincroniza dibujo/entrada; ResetView recupera la vista calculada para la salida.
- InputManager::ProcessEvent recibe eventos antes del visitor y Game. Update (dt) antecede a cursor y escena. Gamepad.cpp consume NavigationPulse/Consume(PadAction). RegisterDevice comparte la entrada de capacidades reales y simuladas.
- SaveManager::ReadDocument (path) devuelve JSON opcional; WriteDocument (path, json) informa éxito. Get/Set/Has/Erase/Clear/Flush conservan el almacén anterior y escriben sólo al cambiar. Windows usa MoveFileExW(REPLACE_EXISTING|WRITE_THROUGH) y otras plataformas rename; no usa el .bak de Match.
- ScreenshotManager::SaveFrame (frame, outputSize, filename, drawOverlay) compone frame terminado/cursor antes de codificar; SaveWindow captura la ventana final. Engine::SaveScreenshot conserva su API delegada. RenderManager no escribe PNG.

El cambio de escena restaura la política heredada antes de Start y sincroniza cursor después. Engine::SmokeConsumers ejecuta Start/Update/Render reales de Clicker/Bounce, verifica récords y vuelve a Menu. Estas demos mantienen estiramiento ultrapanorámico heredado; Plaguefront adopta escala uniforme.

### 2.3. Espacios de coordenadas

| Espacio | Contrato |
| --- | --- |
| Táctico | Hex{q, r}, s=−q−r; sólo coordenadas existentes en state.cells. |
| Distancia | (abs(dq)+abs(dr)+abs(dq+dr))/2; vecinos (+1,0),(+1,−1),(0,−1),(−1,0),(−1,+1),(0,+1). |
| Regular auxiliar | HexToPixel/PixelToHex: x=R√3(q+r/2), y=1.5Rr; no selecciona el mundo elevado actual. |
| Cámara | x=O.x+P.x+Z·30q′; y=O.y+P.y+Z·(14q′+28r′−6h); zoom entero 1–3. Frontera/Ciénaga: (q′,r′)=(q,r). Los tres pasos: (q′,r′)=(q+r,−q), giro visual de 60°. |
| Superficie | (10,−14),(20,0),(10,14),(−10,14),(−20,0),(−10,−14), multiplicados por zoom. |
| Profundidad | Camera::Depth/DepthContinuous: 14q′+28r′, empate q/r; Pick recorre el orden visual al revés con altura real. GroundDepth/DrawOrder conservan el contrato auxiliar sin giro. Laterales/props no son superficies seleccionables. |
| Interfaz | UiScale=min(Output.x/reference.x, Output.y/reference.y); LogicalSize=Output/UiScale. |
| Mundo nativo | WorldScale=worldPixelScale/UiScale; píxel físico ×2 táctico/×1 conjunto, vecino más cercano. |
| Entrada física | PixelToLogical/LogicalToPixel incluyen centro, rotación y viewport SFML con sus bordes redondeados. CursorManager::Draw usa una vista física creada desde target.getSize(), también tras maximizar/restaurar. |
| Entrada al tablero | Restar BoardOrigin y dividir WorldScale antes de Camera::Pick; mando usa posición virtual del mismo CursorManager. |

Camera::Fit reserva 24 píxeles laterales, 60 superiores y 25 inferiores. FrameAll escoge máximo zoom entero que cabe, borra pan; si ×1 no cabe lo mantiene y permite recorrer el mapa. SetZoom conserva el ancla con error máximo de medio píxel nativo/eje y PanBy alinea a enteros. SetRotation recibe pasos hexagonales de 60°; Game::ResetPresentation asigna 1 si match_->state.scenario es `Los tres pasos`, y 0 en otro caso, antes de ajustar el layout. La elección usa la partida cargada, no el borrador de preparación. Project, Inverse, Pick y profundidad usan esa misma orientación; AnimationDirector recibe la rotación para los cuerpos al caminar/atacar. Es una orientación fija por escenario; no hay un control de giro libre.

RenderManager::Revision detecta cambios de métricas. RefreshLayout conserva centro/selección; RebuildControls calcula los mismos botones sin dibujar y conserva foco por acción/valor. OnEvent actualiza geometría antes del clic. Resize no recrea Match. Text rasteriza a tamaño físico y aplica escala UI inversa; no amplía letras desde el lienzo pixel art.

## 3. Configuración y reglas numéricas

Fuente: `PlaguefrontRulesConfig.h`, `Config::Config` en Rules.cpp, balance.json y las órdenes reales. Los valores siguientes son los iniciales actuales; una partida cargada usa su instantánea config.

### 3.1. Enumeraciones y estadísticas

Los índices de UnitClass son Saber=0, Archer=1, Lancer=2, Rider=3, Caster=4, Assassin=5, Berserker=6. Building: None=0, House=1, Fortress=2, Outpost=3. Weather: Clear=0, Rain=1, Fog=2, Storm=3. Mode: VersusAI=0, Local=1. Difficulty: Easy=0, Normal=1, Hard=2.

| Clase | HP | ATQ | DEF | MAG | RES | MOV | Alcance mín.–máx. | Precio |
| --- | ---: | ---: | ---: | ---: | ---: | ---: | --- | ---: |
| Saber | 28 | 9 | 5 | 2 | 3 | 4 | 1–1 | 45 |
| Archer | 22 | 7 | 2 | 1 | 3 | 3 | 2–4 | 35 |
| Lancer | 27 | 9 | 4 | 1 | 2 | 3 | 1–1 | 45 |
| Rider | 30 | 9 | 3 | 1 | 2 | 5 | 1–1 | 60 |
| Caster | 20 | 4 | 2 | 8 | 4 | 3 | 2–3 | 55 |
| Assassin | 21 | 10 | 2 | 2 | 3 | 5 | 1–1 | 50 |
| Berserker | 34 | 11 | 4 | 0 | 1 | 3 | 1–1 | 55 |

EffectiveStats aplica bosque, ruinas, barro, picas, Frenesí, Ice y clima sin modificar el catálogo. DEF nunca baja de 0; MOV nunca de 1; la reducción del alcance máximo no baja del mínimo. No hay azar de impacto ni críticos.

| Terrain / índice | terrainCosts | Modificación inicial |
| --- | ---: | --- |
| Plain / 0 | 1 | Ninguna |
| Forest / 1 | 1 | DEF +1, Archer ATQ +2, Rider coste MOV +1 |
| Hill / 2 | 1 | La altura real determina la subida |
| Swamp / 3 | 2 | Terminar movimiento aplica DEF −1 hasta siguiente activación |
| Ruins / 4 | 1 | DEF +1 y obstáculo intermedio de visión |
| Road / 5 | 1 | Ninguna |
| River / 6 | 2 | No conserva Fire |
| Bridge / 7 | 1 | Puede arder |
| DeepWater / 8 | 9999 | Inaccesible por regla, aunque se edite su coste |
| Blocked / 9 | 9999 | Inaccesible y bloquea visión |

MoveCost añade coste del terreno de destino, max (0, alturaDestino−alturaOrigen)×uphillCost y recargos de Rider/lluvia. Unidades vivas bloquean paso/llegada; cadáveres/fuentes no. FindPath usa Dijkstra y puede superar MOV; Reachable limita al presupuesto e incluye origen. path.cells.size()−1 mide pasos geométricos y path.cost mide puntos gastados. LineOfSight comprueba celdas intermedias de HexLine, existencia, ruinas/Blocked y alturas sobre la línea interpolada de ojos; las unidades no obstruyen. Ataques adyacentes no necesitan rayo.

### 3.2. Catálogo de habilidades

Las 16 habilidades ocupan índices 0–15; arrays de recargas tienen 17 posiciones y el índice 16 queda reservado. Las pasivas no son entradas adicionales de Ability. Las recargas bajan una vez por ronda completa; 2 usada en R vuelve en R+2.

| Índice / Ability | Clase | MP | Recarga | Efecto inicial / condición |
| --- | --- | ---: | ---: | --- |
| 0 Attack | Todas | 0 | 0 | Ataque ordinario; Caster mágico, resto físico |
| 1 Guard | Saber | 0 | 0 | Parada: reduce siguiente ataque físico 50% hasta siguiente activación |
| 2 Precise | Saber | 0 | 2 | Prepara próximo ataque físico +3 |
| 3 Mark | Archer | 0 | 2 | Marca enemigo en alcance/LOS; siguiente daño de enemigo del marcado +2 |
| 4 Focus | Archer | 0 | 0 | Ataque con ATQ +2; bloquea movimiento posterior |
| 5 Pike | Lancer | 0 | 0 | DEF +2 hasta siguiente activación |
| 6 Thrust | Lancer | 0 | 2 | Distancia 1–2 en eje q, r o q+r constante; daño +3 |
| 7 Retreat | Rider | 0 | 3 | Exige attacked real; habilita movimiento adicional de hasta 2 pasos y coste 2; no gasta otra acción |
| 8 Fire | Caster | 2 | 0 | Potencia +2, lluvia −1; ignición lógica independiente |
| 9 Ice | Caster | 2 | 0 | Potencia +1, programa MOV −1 para siguiente ronda completa |
| 10 Lightning | Caster | 3 | 0 | Potencia +5 (+1 lluvia/tormenta), hasta 3 enemigos con radio 1 por salto y LOS propia |
| 11 Heal | Caster | 3 | 0 | +6 HP aliado sin superar máximo, alcance mínimo 0 y máximo efectivo |
| 12 Smoke | Assassin | 0 | 3 | Humo: reduce siguiente daño de ataque 50% hasta siguiente activación |
| 13 Furious | Berserker | 0 | 2 | Ataque físico +3 daño |
| 14 Purify | Caster | 3 | 0 | Elimina una fuente activa a distancia ≤2 |
| 15 Incinerate | Todas | 0 | 0 | Elimina un cadáver a distancia ≤1, no una fuente activa |
| Pasiva Carga | Rider | — | — | Tras ≥3 celdas recorridas, daño +3 |
| Pasiva Flanqueo | Assassin | — | — | Adyacente, aliado en 2×objetivo−atacante: daño +3 |
| Pasiva Frenesí | Berserker | — | — | HP≤50%: ATQ +2 y DEF −1 |

MP de Fire/Ice=2 y Lightning/Heal=3 está fijado en ManaCost; sólo Purify usa purifyMana configurable. Las preparaciones Guard/Precise/Pike/Smoke consumen acción; no se preparan y atacan en una misma activación. UseAbility es la validación final: CanTarget por sí sola no comprueba acción consumida. Todas las órdenes rechazadas conservan HP, recursos y acciones. Heal y Mark no son ataques de daño ni habilitan Repliegue.

### 3.3. Fórmulas y combate secuenciado

CalculateDamage calcula base=max (1, potencia efectiva−DEF o RES). Focus/potencias mágicas/ventaja de altura modifican potencia antes de ese mínimo. Luego añade ventaja de clase, Precise físico, marca, Thrust/Furious, Carga y Flanqueo. Ventaja: Berserker→Saber→Assassin→Caster→Berserker y Lancer→Rider→Archer→Lancer; cada ataque recibe como máximo una ventaja +2.

Si hay Smoke, o Guard frente a ataque físico, aplica una única reducción: max (1, ceil (daño×(100−damageReductionPercent)/100)). El daño ambiental fijo no usa esta fórmula ni consume esas protecciones. La marca se consume en el siguiente ataque de daño de un enemigo del objetivo.

CounterDamage=max (1, ceil(EstimateDamage (defensor, atacante, Attack)×HP del defensor después del golpe/HP máximo)). Sólo responde el objetivo primario si sigue vivo y su Attack normal es legal en alcance/LOS/clima. No consume su acción futura ni genera otro contraataque. Víctimas secundarias y daño ambiental no responden. Lightning selecciona vivo enemigo no alcanzado más cercano desde el último impacto, empate por ID; cada enlace calcula RES propia. Un hechizo gasta MP/acción una vez.

CombatEvent registra id, atacante, víctima, habilidad, from/to, daño aplicado, counter, chainIndex y lethal en el instante del golpe. Una orden de daño válida reemplaza la secuencia anterior; un rechazo la conserva. UseAbility reserva IDs antes de mutar: presupuesto 2 o lightningMaxTargets+1; rechaza desbordamiento del límite 100000000. Las acciones defensivas siguen disponibles. La cola admite hasta 513 eventos y no se serializa; nextEventId sí.

### 3.4. Turnos, captura y economía

BeginActivation inicia una vez o reanuda; activationStarted evita repetir MP/limpieza. EndActivation termina sólo una tropa y mantiene bando, incluso al agotar AvailableCount. EndTurn (Pass es alias) entrega el ejército; después del rojo resuelve un EndRound y comienza azul. StartTurn reinicia sólo unidades del entrante y paga Income una vez. Se puede terminar anticipadamente. Recluta nacido en R usa readyRound=R+1.

Capturar gasta acción y añade HP actuales a progreso del bando en el edificio; propiedad/ganador cambian sólo al alcanzar CaptureRequired. Mover/morir pausa y conserva puntos; un aliado puede continuar. El rival primero resta progreso enemigo y usa excedente para iniciar el propio si puede conquistar; el dueño sólo asegura su propiedad. Capturar fortaleza rival completa es la condición de victoria; perder todas las unidades no es derrota automática.

| Parámetro económico | Inicial | Contrato |
| --- | ---: | --- |
| initialCoins | 120 | Tesorería por jugador; ejército inicial de cinco no se compra |
| fortressIncome / houseIncome | 10 / 5 | Por propiedad al inicio de su turno, incluido primer azul |
| neutralReward / killReward | 20 / 10 | Primera casa neutral una vez / muerte directa atribuida a enemigo una vez |
| outpostCost | 70 | Acción de construcción, distancia≤2 de fortaleza propia o convertir casa propia ocupada |
| houseCapture / outpostCapture / fortressCapture | 40 / 50 / 60 | HP acumulados necesarios; no duración visual |

Fuertes permiten tienda/reclutamiento pero no ingresos ni sustituyen fortaleza como objetivo. Convertir casa pierde ingreso. Recruit requiere propiedad propia, jugador activo, dinero y espacio: edificio libre primero, después primer vecino válido. Un enemigo en edificio bloquea. RecruitBattalion prueba composición completa sobre copia y confirma transaccionalmente sin descuentos; editor admite 1–7 integrantes, con repeticiones, sujeto a espacio real.

| Item / índice | Precio | Uso (acción + objeto sólo con efecto válido) |
| --- | ---: | --- |
| Potion / 0 | 15 | +8 HP propio o aliado adyacente |
| LargePotion / 1 | 25 | +14 HP |
| ManaPotion / 2 | 15 | +4 MP de Caster |
| Antidote / 3 | 10 | Elimina Ice programado/activo, barro y marca |
| Purifier / 4 | 20 | Elimina una fuente activa a distancia≤2 |

Inventario máximo dos objetos. Comprar no gasta acción, exige turno propio y fortaleza/fuerte propio a distancia≤1; el panel comprueba además el edificio concreto seleccionado. Se rechaza restauración inútil y no se consume el objeto.

Las plantillas de batallón de TacticalPanel/CheckAction se compran mediante RecruitBattalion, sin descuentos. Los costes se calculan sumando Config::classes; con el balance inicial actual:

| Plantilla | Integrantes | Coste inicial |
| --- | --- | ---: |
| Vanguardia | Saber, Lancer, Berserker | 145 |
| Reconocimiento | Rider, Archer, Assassin | 145 |
| Arcano | Caster, Saber | 100 |
| Personalizado | Hasta siete integrantes, con repeticiones | Suma de los elegidos |

El límite de siete corresponde al edificio y sus seis vecinos; no garantiza espacio libre. El núcleo rechaza la operación completa sin cobrar si un integrante no puede aparecer.

### 3.5. Ambiente y clima

| Grupo / campos | Iniciales | Contrato |
| --- | --- | --- |
| rotWait, rotRadius, rotDamage, rotDuration | 3,3,2,5 | Esperas completas, radio, HP y ticks locales |
| globalThreshold, connectionDistance, globalDamage, globalDuration | 3,3,1,5 | Componente mínimo, arista hexagonal, HP y reserva global |
| fireRadius, fireDuration, fireDamage | 1,2,2 | Área existente, cierres y HP; lluvia reduce daño a1 |
| lightningMaxTargets, lightningRadius | 3,1 | Incluye primer enemigo; radio por enlace |
| weatherPeriod | 3 | Cambia al comenzar R4, R7,…, una vez tras ambos ejércitos |

Muerte en R deja Corpse no bloqueante, identificable por clase/bando/unitId. La ronda parcial R no cuenta: conversión tras daño al final R+3, cinco daños locales al final R+4…R+8. Damage ignora una unidad ya muerta y crea un cadáver/recompensa como máximo una vez. La fotografía (unitId, daño) del ambiente precede a todas las muertes del tick; una muerte ambiental no acelera su propio envejecimiento.

EndRound: calcula grupo activo y renueva global si corresponde; fotografía/aplica RotDamageAt+FireDamageAt; descuenta fuentes/fuego; descuenta reserva global sólo sin grupo; convierte cadáveres maduros; actualiza conectividad de fuentes nuevas; reduce recargas y avanza ronda/clima. Los ingresos pertenecen a StartTurn.

RotDamageAt devuelve 2 si alguna fuente remaining>0 cubre Distance≤rotRadius; si no, 1 si globalRemaining>0; si no, 0. Ni fuentes solapadas ni local/global se suman. FireDamageAt aporta una cantidad adicional única por celda; repetición renueva duración máxima sin duplicar tick. River/DeepWater/Swamp/Blocked no conservan fuego; Bridge sí. Ambiente afecta ambos bandos, no paga bajas ni genera respuesta.

LargestSourceGroup recorre componentes transitivos de fuentes activas conectadas a distancia≤3; A (0, 0)–B (3, 0)–C (6, 0) forma grupo de 3. Un cadáver inerte o fuente agotada no cuenta. Con grupo suficiente, reserva global se renueva a 5 sin gastarse; al desaparecer quedan cinco ticks residuales, uno por cierre sin grupo. Crear tercera fuente activa estado tras daño y primer daño global llega al cierre siguiente. Purify/kit eliminan una fuente y conservan cadáver superpuesto; Incinerate elimina un cadáver y conserva fuente. Quitar una fuente no borra reserva global.

| Weather | Modificación real |
| --- | --- |
| Clear | Ninguna |
| Rain | Coste +1 al entrar Swamp o altura<0; Fire potencia−1 y daño persistente−1 (mínimo1); Lightning +1 |
| Fog | Alcance máximo a distancia−1, nunca debajo del mínimo |
| Storm | Rider MOV−1, Archer máximo−1, Lightning +1 |

Clima determinista: weatherSeed=(unsigned (seed)×1664525+1013904223)&0x7fffffff; offset=1+seed%3 y nuevo clima=(actual+offset)%4. Nunca repite inmediatamente el mismo. Guardar conserva semilla/clima. Dificultad no modifica estadísticas.

### 3.6. Catálogo completo de modificadores restantes

Estos campos de Config se cargan por sus nombres exactos en balance.json; la instantánea los serializa por nombre salvo el grupo mágico y el array abilityCooldowns, descritos en 4.1. Se aceptan enteros, no fracciones. Incluyen todos los modificadores no enumerados en las tablas económicas/ambientales anteriores.

| Campos | Valores iniciales | Aplicación |
| --- | --- | --- |
| forestDefense, forestArcherAttack, forestRiderSurcharge | 1,2,1 | DEF, ATQ Archer, coste Rider |
| uphillCost, rangedHeightAttack, swampDefensePenalty, ruinsDefense | 1,1,1,1 | Subida, potencia desde altura, barro, DEF ruinas |
| rainMoveSurcharge, stormRiderPenalty, weatherRangePenalty | 1,1,1 | Penalizaciones climáticas |
| maxMana, manaRecovery, healAmount | 8,1,6 | Caster: máximo/primer inicio; Heal |
| firePower, icePower, lightningPower, purifyMana, purifierRange | 2,1,5,3,2 | Potencia hechizos/coste Purify/radio contramedidas |
| potionHeal, largePotionHeal, manaPotionRestore | 8,14,4 | Consumibles |
| advantageDamage, damageReductionPercent | 2,50 | Ventaja y reducción porcentual |
| preciseBonus, markBonus, focusBonus, pikeBonus | 3,2,2,2 | Precise, marca, Focus, formación |
| thrustBonus, chargeBonus, flankBonus, furiousBonus | 3,3,3,3 | Daño de habilidad/pasivas |
| frenzyAttack, frenzyDefensePenalty, chargeSteps, retreatSteps | 2,1,3,2 | Frenesí, pasos de Carga y presupuesto de retirada |
| abilityCooldowns | [0,0,2,2,0,0,2,3,0,0,0,0,3,2,0,0,0] | Orden Ability; última entrada reservada |

### 3.7. Parámetros visuales y de interfaz

PlaguefrontGameConfig.h define DesignHeight=900, HeaderHeight=72, Margin=16, RosterWidth=212, OrdersWidth=334, BoardOrigin=(244, 88), BottomLogHeight=48, RosterTop=174, RosterStep=58, RosterRowHeight=54, TacticalPixelScale=2, OverviewPixelScale=1. BoardBounds usa ancho UiWidth−608 y alto UiHeight−200. Filas visibles=max (1, int((UiHeight−194−174)/58)).

En RenderWorld, un bosque sin edificio utiliza alfa **100/255** cuando su celda contiene una unidad viva, cuando se señala su celda o cuando el cursor toca un píxel visible de su sprite. TreeSpriteContains transforma el cursor al atlas 64×80 con ancla (32,68), el mismo redondeo y zoom del dibujo, y consulta una máscara alfa de sus cuatro marcos, almacenada en CPU al cambiar de textura. La copa puede recibir hover fuera del hexágono. Se conserva la atenuación de objetos que ocultan una unidad seleccionada o consultada. La comprobación de cursor sólo se activa jugando y dentro del tablero; usa CursorManager tanto para ratón como para posición virtual.

La previsualización de ruta mantiene trazo dorado de **3 píxeles nativos**, con borde oscuro de 5, y **una única cabeza al final del recorrido**, situada en el centro de la celda de destino y orientada según el último tramo. Su triángulo relleno mide **12 de ancho ×7 de largo** y utiliza el mismo `Gold` del cuerpo; el contorno oscuro mide 14×9 y comparte el color del borde del cuerpo. El borde de la cabeza se dibuja antes del relleno del cuerpo, y el triángulo dorado al final, para mantener una unión continua sin una línea oscura atravesada. Todo se multiplica por el zoom entero. Los tramos intermedios no llevan puntas y se elimina el marcador cuadrado final. Esta geometría sólo comunica el destino de previewPath y no altera el coste ni la orden de movimiento.

AnimationTiming vigente: frames8; idleFrameSeconds=.18, walkFrameSeconds=.035, walkCellSeconds=frames×walkFrameSeconds=.28, attackSeconds=.45, hurtSeconds=.30, deathSeconds=.65, castSeconds=.60, weaponContactSeconds=.18, spellContactSeconds=.225, chainIntervalSeconds=.18. La reducción autorizada de .05/.40 a .035/.28 disminuye un 30% tanto el intervalo Walk como la duración por casilla y conserva sus ocho marcos; véase 7.10. Los anteriores .12 s por casilla y .06 s por marco dejaban avanzar sólo dos de los ocho marcos Walk antes de llegar a Idle; no era suficiente que existiera la tira completa. AudioDirector comparte la duración de desplazamiento, los contactos y la separación de cadena; el tiempo visual nunca avanza una ronda. Los valores .11/.06/.12 pertenecen a 7.8 y .05/.40 a 7.9; sus cifras se conservan como resultados de aquellas etapas.

`Game::Update` obtiene `dt` de TimeManager y llama a `AnimationDirector::Update`; `Sample` calcula Idle como `floor(elapsed/idleFrameSeconds)%8`, actualmente con .18 s por marco. `Observe` de una instantánea sin cambios conserva esa fase. El renderer selecciona el marco completo de 48×64, sin recortar miembros durante la reproducción. Una tira de ocho poses y ocho hashes diferentes no demuestran por sí solos que esa ruta temporal funcione: los fixtures comparan el píxel dibujado con el rectángulo del atlas y comprueban vueltas completas, posición, elevación y orientación. Las verificaciones previas están en 7.8–7.9; el estado parcial actual de velocidad e investigación de Idle está en 7.10. La reproducción técnica del recurso vigente no certifica que sus poses resuelvan la nueva solicitud de Idle.

`UnitPresentation.h` define `UnitContactWindow`, `UnitContactGeometry` y `SnapUnitPoint`. El apoyo se obtiene del alfa dentro de una ventana de pies/pezuñas por clase, pose y orientación, no del rectángulo completo que puede incluir bastón, arma o capa. Se inspeccionan las tres últimas filas del apoyo y se calcula una huella proporcional, con límite específico por clase; Rider usa aproximadamente el 75% del ancho medido. La sombra comienza en `bottom−1`, solapando un píxel de apoyo: dos filas para humanos y cuerpos caídos, tres para Rider de pie, con alfa 90 en el núcleo y 45 en la fila final. El cadáver identificable de Death 7 usa su propia huella y sustituye el antiguo rectángulo uniforme 28×5 con alfa 190. Unidad, sombra e insignia de suelo comparten el mismo punto proyectado redondeado. `FrameGeometryFor` centraliza alfa, contacto y máscara inferior en caché por textura/handle/tamaño; sólo lee el atlas al cambiar esa identidad y calcula cada marco al solicitarlo, sin lectura GPU por unidad y fotograma.

La corrección de profundidad sólo se aplica a Walk: comprueba la banda opaca real y44…63, la sombra y el marcador contra superficies de celdas existentes. Una intersección envolvente de 48×20 precede a la máscara exacta. `UnitWalkSurfaceEligible` admite celdas a cota igual o inferior a la elevación interpolada y, como excepción, los dos extremos del tramo activo aunque uno esté más alto; una pared ajena más elevada conserva su oclusión. Si hay intersección, la profundidad es `max(profundidadInterpolada+1, profundidadSuelo+.25)`, sin alterar las entradas de terreno ni de props, que mantienen su desplazamiento +.5. Incluye vecinos fuera de la ruta porque una espada, sombra o marca de suelo puede alcanzarlos antes que el centro del cuerpo. No cambia cámara, dimensiones, trayectoria ni posición lógica. Su comprobación gráfica actual se describe en 7.9.

La lluvia usa `RainDrops(width,height,elapsed,storm)` de Weather.h sobre todo el lienzo nativo `world_`. Su cantidad es `clamp(ceil(width*height/(storm?2600:3300)),64,360)`, con tres capas de longitud aproximada 3.5–9 píxeles, velocidad, viento y opacidad diferentes. Una cuadrícula con variación determinista distribuye las gotas; módulo positivo mantiene las cabezas dentro de la vista incluso con tiempo negativo. Los trazos viajan hacia abajo e izquierda y se dibujan en un solo lote de puntos enteros, después del mundo. Al desplazar la cámara no se reutiliza una región fija del mapa. La opción cosmética existente sigue controlando lluvia/niebla; no cambia Weather, costes, fuego o bonificaciones eléctricas.

`VisualEffect` añade `contactSeconds` y `stagedSpell`; no guarda una segunda acción táctica. `SampleSpell` divide el intervalo previo al contacto en Preparation (0–45%), Launch (45–60%) y Travel (60–100%); después dedica el primer 65% a Impact y el resto a Fade. Fire/Ice viajan como proyectiles, Lightning dibuja enlaces quebrados y Heal/Purify emplean motas diferenciadas. Los contactos ofensivos permanecen en .225 s y los saltos adicionales mantienen .18 s; su preparación adelanta el comienzo visual y conserva el final anterior, en lugar de posponer el daño o añadir espera. Los enlaces secundarios con contacto cero empiezan directamente en impacto. `AnimationDirector::Update` elimina los efectos al terminar su vida; `RenderSpellEffect` no aplica daño ni mana y usa las coordenadas/alturas de eventos ya resueltos.

`createdFireIds` registra en el efecto primario de Fire solamente los IDs nuevos del área aparecidos entre las dos instantáneas. `FireAwaitsContact` retrasa su dibujo persistente mientras exista espera de cola o no se alcance `contactSeconds`; no retrasa su creación lógica ni vuelve a ejecutar ignición/daño. Los incendios previos, incluidos aquellos cuya duración se renueva conservando su ID, siguen visibles durante la preparación. El kit purificador mantiene la presentación anterior, también si lo usa un Caster: quitar una fuente mediante inventario no equivale a lanzar Purify. Purify/Incinerate solo adoptan la presentación mágica por etapas cuando los ejecuta el Caster como habilidad; sus usos físicos conservan el efecto anterior.

RotVisual conserva pulseRadiansPerSecond=2.8 y los conteos sourceSporeCount=7, localSporeCount=2, globalSporeCount=1. En C1 los colores y el alfa proceden de `Content/Config.json → rotPalette`, interpretados por `EngineConfig::ReadRotPalette`. Cada propiedad acepta exactamente cuatro enteros de 0 a 255; si falta o es inválida conserva su respaldo, sin invalidar las otras propiedades.

| Propiedad rotPalette | Respaldo RGBA | Uso |
|---|---|---|
| sourceShadow | [58,27,76,255] | Material oscuro, centro y sombras de contaminación |
| sourceBase | [117,57,161,255] | Material base de fuente/icono/RotBurst |
| sourceLight | [187,112,230,255] | Material iluminado |
| sourceGlow | [239,191,255,255] | Brillo y contorno del centro |
| localFill | [139,55,185,66] | Suelo local y su pulso multiplicador |
| localEdge | [211,139,247,255] | Borde, venas y halo local |
| globalFill | [104,61,153,28] | Base global de cada celda existente; el renderer modula su alfa sin sustituir esta configuración |
| spores | [231,173,255,240] | Partículas y estelas de contaminación |
| uiText | [232,190,255,255] | Duración, HUD y advertencias de putrefacción/peste |

`RotPaletteConfig::SpriteColor` remapea cuatro materiales RGB concretos del atlas; `Game::RotTexture` conserva texturas derivadas en caché y deja el recurso original intacto. Sólo las regiones de fuente, icono 20 y RotBurst usan esa copia; Smoke/Heal/Purify y el resto del atlas usan el original. El alfa configurado se multiplica por el alfa original del píxel y por los pulsos visuales cuando corresponde.

RotPresentationAt sigue consultando local/global/damage/coveringSources del modelo, sin activar peste por un grupo aún no resuelto. `RenderWorld` recorre `state.cells`; `RotGround` dibuja una sola base por cada celda local/global. Con peste activa utiliza `globalFill` también dentro de radios locales: multiplica su alfa por `1.9+.3*pulse`, con máximo 105, y añade vetas moradas derivadas de sourceLight/localEdge y del alfa configurado. Conserva los bordes de origen local, los centros y sus indicadores sin sumar otro relleno. Esto refuerza una cobertura lógica que ya era global, antes demasiado tenue fuera de las fuentes; no modifica `RotDamageAt`, conectividad, duración o daño. Los avisos esenciales permanecen con efectos cosméticos desactivados. La paleta no pertenece a balance.json ni al guardado de Match. [Implementación paso a paso y JSON completo](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-30).

### 3.8. IA: política y límites concretos

StepAI sólo actúa en VersusAI, turno rojo y sin ganador. Escoge una unidad disponible, BeginActivation, simula Reachable/Move/Capture/UseAbility/UseItem y ejecuta mejor decisión. EndActivation conserva el ejército; sólo al agotarlo EndTurn. Reclutamiento intenta límites 6/8/10 por dificultad, con reserva 20. Normal/difícil pueden comprar poción/kit legalmente; difícil considera fuerte reservando 55 además de su coste.

Heurística: captura casa65/fortaleza300; HP enemigo perdido×2.5, baja+25, daño aliado−2, fuente quitada+12, cadáver quitado+4. Normal añade amenazas/terreno/defensa/contramedidas; difícil penaliza más peligro letal, ambiente y cadáveres próximos. Los desempates conservan orden estable. Explora una activación sobre copias, no minimax ni planificación económica multirronda. No usa azar táctico, información oculta ni ventajas de estadísticas.

## 4. Formatos y validaciones

Todos los formatos tácticos se implementan en Persistence.cpp con nlohmann JSON. Los ejemplos parciales se identifican como tales; para cargar un escenario completo se usan los JSON enlazados en 4.2. Guardados y preferencias tienen contratos diferentes.

### 4.1. Balance editable frente a instantánea

Config::Load parte de Config{} y sustituye campos presentes. `classes` del balance es objeto por nombre exacto; cada clase declarada exige los nueve campos hp, attack, defense, magic, resistance, movement, minRange, maxRange, cost. Clases desconocidas se rechazan; clases omitidas conservan valores iniciales. Los escalares y arrays de sección 3 se nombran exactamente como en balance.json. Un objeto `snapshot` solicita el esquema de instantánea directamente.

| Campo de config guardada | Longitud / orden exacto |
| --- | --- |
| classes | 7 filas UnitClass; cada fila [hp, attack, defense, magic, resistance, movement, minRange, maxRange, cost] |
| itemCosts | 5 valores en orden Item |
| terrainCosts | 10 valores en orden Terrain |
| economy | [initialCoins, fortressIncome, houseIncome, neutralReward, killReward, outpostCost] |
| rot | [rotWait, rotRadius, rotDamage, rotDuration, globalThreshold, connectionDistance, globalDamage, globalDuration] |
| magic | [firePower, icePower, lightningPower, purifyMana, purifierRange] |
| weatherPeriod | Entero escalar |
| abilityCooldowns | 17 enteros, último reservado |
| Campos por nombre | Todos los modificadores de 3.6 salvo firePower, icePower, lightningPower, purifyMana, purifierRange y abilityCooldowns, más houseCapture, outpostCapture, fortressCapture, lightningMaxTargets, lightningRadius, fireRadius, fireDuration, fireDamage |

No se debe copiar literalmente un bloque config guardado como balance editable sin envolverlo en snapshot: usan distinta forma de classes/economy/rot/magic. Match::Load utiliza la config guardada; su argumento Config se conserva por compatibilidad pero no sustituye esa instantánea.

| Restricción de balance/instantánea | Intervalo |
| --- | --- |
| HP de clase / ATQ, DEF, MAG, RES / MOV, alcances / precio de clase | 1–10000 / 0–1000 / 1–100 / 1–100000; mín.≤máx. |
| itemCosts / terrainCosts / economy | 0–100000 / 1–100000 / 0–1000000 |
| rot y weatherPeriod | 1–1000 |
| magic y modificadores por nombre | 0–1000; purifierRange editable limita además a100 |
| damageReductionPercent / maxMana / retreatSteps | ≤100 / ≥1 / ≤100 |
| houseCapture, outpostCapture, fortressCapture | 1–1000 |
| lightningMaxTargets / lightningRadius | 1–512 / 1–1000 |
| fireRadius / fireDuration, fireDamage | 0–1000 / 1–1000 |
| abilityCooldowns | 17 entradas exactas 0–1000 |

Las validaciones finales recorren la instantánea completa. Declarar un parámetro nuevo en Config no lo convierte automáticamente en editable ni persistente: debe añadirse a lectura/escritura/validación.

### 4.2. Escenarios reales

| Archivo completo | Nombre exacto / seed | Tamaño / organización |
| --- | --- | --- |
| [frontier.json](../Content/Plaguefront/scenarios/frontier.json) | La frontera gris /17 | 99 celdas,11×9 visuales |
| [marsh.json](../Content/Plaguefront/scenarios/marsh.json) | Cienaga del ultimo juramento /29 | 99 celdas,11×9 visuales |
| [highlands.json](../Content/Plaguefront/scenarios/highlands.json) | Los tres pasos /71 | 188 celdas,15 columnas con 13/12 filas alternas; simetría (q, r)→(14−q,5−r) |

| Campo de escenario | Contrato de Match::Create |
| --- | --- |
| name | Texto≤160 bytes sin NUL |
| weather_seed | Opcional entero0–INT_MAX, inicial1 |
| cells | Lista no vacía, máximo4096; coordenadas explícitas |
| cells[].pos / terrain / height | [q, r] enteros−10000…10000; nombre Terrain exacto; altura−20…20 |
| cells[].building / owner | Opcionales: None /−1; nombre Building y dueño−1/0/1 |
| armies | Dos arrays de cinco nombres de clase; pueden sustituirse por cinco clases/bando desde preparación |
| spawns | Dos arrays de cinco Hex; deben existir, ser transitables y no solapar vivos |
| version, description, columns, rows, coordinate_system | Metadatos presentes en archivos; Create no usa esos campos como autoridad ni valida version |

En Frontera/Ciénaga: q=columna−floor(fila/2), r=fila. Ambas tienen fortalezas azules (−2, 4), rojas (8, 4); casas neutrales (2, 2), (2, 4), (4, 4), (4, 6). Apariciones azules [(-2, 4), (0, 3), (-1, 4), (-2, 5), (-1, 5)]; rojas [(8, 4), (8, 3), (7, 4), (8, 5), (7, 5)]. Clases azules [Saber, Archer, Lancer, Rider, Caster]; rojas [Saber, Archer, Berserker, Assassin, Caster].

Los tres pasos: fortalezas (1, 5)/(13, 0); casas neutrales (4, 3), (4, 7), (5, 0), (9, 5), (10, −2), (10, 2). Apariciones azules[(1, 5), (1, 4), (2, 4), (0, 6), (1, 6)]; rojas[(13, 0), (13, 1), (12, 1), (14, −1), (13, −1)]. Ambos ejércitos usan [Saber, Archer, Lancer, Rider, Caster]. Hay tres cruces, cuatro celdas Bridge, bosque/ruinas/colinas y pantano bajo; las pruebas aíslan cada cruce para verificar alternativas. C2 gira su presentación 60° para dar la misma sensación de perspectiva de los mapas pequeños; estas coordenadas, alturas, conexiones, contenido JSON y formato de guardado se conservan.

| Terreno | Frontera | Ciénaga | Los tres pasos |
| --- | ---: | ---: | ---: |
| Plain | 69 | 50 | 88 |
| Forest | 8 | 6 | 20 |
| Hill | 2 | 0 | 18 |
| Swamp | 0 | 24 | 12 |
| Ruins | 3 | 2 | 6 |
| Road | 6 | 6 | 26 |
| River | 5 | 5 | 6 |
| Bridge | 2 | 2 | 4 |
| DeepWater | 2 | 2 | 2 |
| Blocked | 2 | 2 | 6 |

Cambiar el JSON de escenario afecta partidas nuevas. Guardados contienen sus celdas completas; Load no necesita recrear desde una plantilla ni modifica mapas anteriores. Create exige una fortaleza por bando, crea diez unidades sin pagar reclutamiento, valida y llama StartTurn (0), con su ingreso inicial.

### 4.3. Guardado versión2: esquema completo

Raíz: `format:"Plaguefront"`, `version:2`, `config:{...}`, `state:{...}`. config sigue 4.1; no se guarda cámara, selección, foco UI, audio o segundos de animación.

| Campos de state | Contenido |
| --- | --- |
| scenario, round, currentPlayer, activeUnit, nextId, winner | Nombre, ronda, bando, ID activo o−1, siguiente ID y ganador−1/0/1 |
| globalRemaining, activationSerial[2] | Reserva global y serial de finales por ejército |
| weather, mode, difficulty, weatherSeed | Enumeraciones numéricas y semilla |
| players[2] | Objetos {name, coins} |
| cells[] | {pos, terrain, height, building, owner, reward, capture, captureRound, captureSerial, capturingPlayer, captureProgress} |
| units[] | {id, owner, class, pos, hp, mp, readyRound, flags, stepsMoved, slowThroughRound, mudUntilActivation, retreatRemaining, activationStarted, cooldowns, inventory} |
| corpses[] | {id, pos, deathRound, owner, class, unitId}; unitId=−1 sin asociación histórica |
| sources[] | {id, pos, remaining} |
| fires[] | {id, pos, remaining, owner} |
| log[] | Registro reciente de texto |
| nextEventId | Siguiente ID de combate; combatEvents no se escribe ni restaura |

Correspondencia de Cell: reward=neutralRewardClaimed, capture=captureUnit, captureRound=captureStartedRound, captureSerial=captureDefenderSerial. capture es último participante informativo aún presente; no otra condición de conquista. capturingPlayer/captureProgress mantienen el progreso entre unidades/turnos. Campos terrain/building/class/item usan índices de enumeración en guardados, nombres en escenarios.

flags tiene **10 booleanos** en orden [activated, moved, acted, attacked, moveLocked, guard, smoke, pike, precise, marked]. activationStarted va por separado. cooldowns tiene 17 enteros; inventory hasta 2 índices Item. Un muerto conserva su Unit con HP 0 y crea un Corpse separado; IDs de unidades/cadáveres/fuentes/fuego comparten nextId. IDs de CombatEvent usan nextEventId independiente.

### 4.4. Límites y relaciones validadas

| Dato | Límite y comprobación |
| --- | --- |
| Archivo táctico | >0 y≤4MiB; raíz objeto; profundidad JSON≤48, lectura completa |
| Colecciones | cells1–4096, units≤512, corpses/sources/fires≤4096 cada una; combatEvents en memoria≤513 |
| Control | round1–1000000, currentPlayer0/1, winner−1/0/1, nextId/nextEventId1–100000000 |
| Identidades | ID positivo<nextId y único entre unidades/efectos; event.id creciente<nextEventId |
| Textos | scenario≤160, name jugador≤80, log≤200 entradas≤512 bytes, sin NUL al leer |
| Jugadores | Dos exactos; coins0–100000000, activationSerial0–100000000 |
| Celdas | Coordenadas únicas/en intervalo, terreno/edificio válido, altura−20…20, dueño−1/0/1; None exige−1 |
| Edificios | No Blocked/DeepWater; fortaleza con dueño; sin ganador exactamente una por jugador, con ganador las dos del ganador |
| Recursos de unidad | Celda existe; HP0…máximo de clase, MP0…maxMana y sólo Caster puede MP>0 |
| Ocupación | Vivos únicos por celda transitable; muertos no bloquean |
| Tiempos de unidad | readyRound1…round+1, stepsMoved0…1000, slowThroughRound0…round+1, mudUntilActivation0…100000001, retreatRemaining0…retreatSteps, cooldowns0…1000 |
| Captura | 0≤progress<max(1, total); Building::None sólo admite0; cero exige capturingPlayer=capture=−1; positiva exige edificio real, bando distinto del dueño, round de inicio válido |
| Participante de captura | Si capture≠−1: unidad viva en celda y bando capturingPlayer; progreso positivo admite capture=−1 |
| Cadáver | Celda existente, deathRound1…round, clase/bando válido; si unitId≠−1 debe señalar muerto coincidente en posición/clase/bando |
| Fuente / fuego | Posición existe, remaining1…duración configurada; fuego con dueño válido y terreno que pueda arder |
| Peste | globalRemaining0…globalDuration |
| Unidad activa | Viva, del currentPlayer, no activated, sí activationStarted, readyRound≤round, sin ganador |
| Eventos en memoria | Atacante/víctima existentes, extremos existentes, Ability0…15, amount1…10000, chainIndex0…lightningMaxTargets−1 |

Un JSON correcto sintácticamente puede fallar estas relaciones. Load devuelve nullopt y error sin reemplazar la partida. Save valida antes de escribir y rechaza una serialización>4 MiB. Genera .tmp, cierra/flush, mueve el anterior a .bak y renombra el temporal; ante fallo de reemplazo intenta restaurar .bak. No garantiza una transacción frente a cualquier pérdida eléctrica entre renombrados en Windows. La carga nunca sobrescribe el archivo fuente.

### 4.5. Migración explícita versión1

Load acepta versiones1/2 y rechaza otras. En v1 toma activated como activationStarted; sólo la unidad referida por activeUnit queda activated=false para reanudar, las demás activadas siguen terminadas. Captura pendiente válida de fortaleza se convierte en min(HP del captor, fortressCapture−1); nunca concede victoria al cargar. Valida captor vivo/presente/enemigo y metadatos de ronda/serial. Si no hay progreso posible limpia participación.

Cadáveres v1 infieren propietario/clase/unitId desde muertos de esa celda todavía no asociados, si existen; de otro modo quedan valores iniciales y unitId−1. Parámetros nuevos reciben valores Config iniciales, no se crean incendios y nextEventId comienza 1. Se añade una entrada de migración sin pagar ingresos ni adelantar ronda. La próxima escritura explícita usa v2; el original permanece intacto.

## 5. Entrada y preferencias

### 5.1. Teclado y ratón

F11 y Alt+Enter pasan por EngineVisitor→EventFullscreenShortcut y la misma operación WindowManager::ToggleFullscreen. Un latch por tecla impide repetir la alternancia mientras se mantenga pulsada; KeyReleased libera el flanco. El evento consumido no confirma menús. ApplyWindowChanges sincroniza tamaño, foco real, vista del cursor y layout antes del frame siguiente. En Windows, WindowManager elige el monitor de la ventana mediante MonitorFromWindow, aplica el modo a su dispositivo con ChangeDisplaySettingsExW y coloca allí una ventana sin bordes. Al salir restaura el modo del dispositivo y WINDOWPLACEMENT, incluidos posición y estado maximizado. [Sincronización inicial](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-31) y [contrato vigente](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-32).

| Entrada | Acción vigente |
| --- | --- |
| Clic izquierdo | Botón, selección, destino u objetivo según contexto |
| Clic derecho / Escape | Regresa un contexto; Escape abre pausa desde selección general |
| Rueda sobre tablero | Zoom1/2/3 anclado al cursor |
| Arrastre central / WASD / flechas izquierda-derecha | Cámara; A ya está reservada para desplazar |
| Home / V | Centrar seleccionado/fortaleza / encuadrar mapa completo |
| M / T / H / I | Mover / atacar / habilidades / inventario |
| E / Espacio | Terminar sólo activación elegida |
| F / botón FIN DE TURNO | Entregar ejército, confirma si quedan disponibles |
| Tab / Arriba / Abajo | Recorrer botones habilitados; Enter ejecuta foco/confirma ruta o traspaso |
| Rueda sobre lista izquierda | Desplazar tropas disponibles y actuadas |
| F5 / F9 | Guardar / cargar durante partida |
| F11 / Alt+Enter | Alternar pantalla completa mediante WindowManager |
| F12 | Captura final mediante ScreenshotManager::Take en Content/Screenshots |

Los once contextos son Selection, Orders, Move, Abilities, Target, Inventory, Building, Recruitment, Shop, Construction, Purchase. Seleccionar/abrir/cancelar no consume activación. CheckAction ejecuta la orden real en una copia; PreviewRules inicia una activación sólo en copia para resaltar objetivos con recuperación de MP correcta. Esas predicciones no alimentan audio ni animación. Las cachés se invalidan por revisión, unidad, propiedad, turno, ronda, dinero, animación y composición personalizada.

### 5.2. Mando lógico y capacidades

| PadAction (orden serializado) | GamepadButton inicial / índice | Referencia habitual |
| --- | --- | --- |
| Confirm | South /1 | Inferior, A |
| Cancel | East /2 | Derecho, B |
| Orders | West /0 | Izquierdo, X |
| Information | North /3 | Superior, Y |
| PreviousUnit | L1 /4 | LB/L1 |
| NextUnit | R1 /5 | RB/R1 |
| Center | R3 /11 | Pulsar stick derecho |
| ZoomOut | L2 /6 | LT/L2 |
| ZoomIn | R2 /7 | RT/R2 |
| FinishUnit | L3 /10 | Pulsar stick izquierdo |
| EndTurn | Select /8 | Vista/Select |
| Pause | Start /9 | Menú/Start |

El stick izquierdo/cruceta mueve foco geométrico o cursor de hexágonos existentes. Orders alterna panel/tablero; Info consulta sin iniciar activación. Confirm usa Dispatch/HandleCell, las mismas órdenes de ratón. LB/RB recorren disponibles; el cursor conserva celda válida y cámara lo mantiene visible. Cambiar pantalla/turno suprime mantenidos hasta soltar. Desconectar conserva partida y permite teclado/ratón; fuera de foco no ejecuta mando. Un dispositivo conectado puede tomar control al emitir entrada; no se asigna un mando por ejército.

Button lógico: West 0, South 1, East 2, North 3, L1=4, R1=5, L2=6, R2=7, Select 8, Start 9, L3=10, R3=11. Auto reconoce vendor Microsoft 0x045e→Xbox y Sony 0x054c→PlayStation; otros→Generic. Xbox físico=[2, 0, 1, 3, 4, 5, −1, −1, 6, 7, 8, 9]; PlayStation/Generic=[0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11]. Cada acceso comprueba capacidades reales; un perfil no inventa un botón/eje ausente.

### 5.3. Preferencias comunes de InputManager

Ruta Content/Save.json, clave Engine:Input. Se carga al arrancar Engine y guarda por SaveManager, independiente de settings/partida. Las claves de récords Clicker/Bounce se conservan. No existe editor gráfico de asignación individual; Controles muestra asignación efectiva y ajusta perfil, zona muerta, sensibilidad, inversiones, repetición, gatillos/restablecimiento.

| Campo JSON | Inicial | Normalización / sentido |
| --- | --- | --- |
| version | 1 | Se escribe; LoadPreferences actual no bifurca por versión |
| profile | 0 | Auto0, Xbox1, PlayStation2, Generic3; otro→Auto |
| deadzone | .22 | Finito [.05,.80], inválido→.22 |
| sensitivity | 1 | [.25,3], inválido→1; dirección analógica y repetición |
| initial_repeat | .32s | [.15,1] |
| repeat_interval | .12s | [.06,.5], intervalo efectivo/base dividido por sensibilidad |
| invert_x, invert_y | false, false | Stick, no remapea botones |
| pov_y_up_positive | true | Convierte signo vertical de cruceta SFML |
| stick_x, stick_y | 0,1 | X, Y; ejes fuera de rango→X |
| trigger_axis, right_trigger_axis | 2,3 | Z, R |
| separate_triggers, trigger_inverted | false, false | Combinados/independientes y orientación |
| hardware_buttons | 12×−2 | −2 perfil,−1 desactiva,0–31 físico; limita intervalo |
| action_buttons | [1,2,0,3,4,5,11,6,7,10,8,9] | Doce índices Button por orden PadAction; inválido→valor inicial de esa acción |

Ejes SFML: X 0, Y 1, Z 2, R3, U 4, V 5, PovX 6, PovY 7. Normaliza [−100, 100]→[−1, 1]. Cruceta umbral.5; stick usa zona muerta. Gatillo combinado: >.5 izquierdo, <−.5 derecho; invertir intercambia. Independientes: cada eje>.5. Botón digital asignado tiene prioridad; ejes son alternativa sólo con asignación automática−2. Un hardware_buttons=−1 también desactiva esa alternativa.

Un pulso inicial de dirección precede a la repetición controlada; centrar stick reinicia demora. Botones/gatillos emiten flanco único, no repetición automática. Configurar suprime entradas hasta soltarlas. Compartir botón entre acciones permite que lo consuma la primera evaluada: conviene usar asignaciones distintas. Si Engine:Input no es objeto, LoadPreferences retorna y conserva configuración vigente; excepciones de tipos llaman Configure({}). Campos omitidos de un objeto válido parten de valores iniciales.

### 5.4. Preferencias de la escena y del arranque

| Archivo / campos | Inicial y límites |
| --- | --- |
| Saves/settings.json: grid | true |
| effects | Campo de Game inicial true; si existe JSON sin effects, LoadSettings usa false |
| ai_delay | .55s, limita .15–1.2; ritmo visual, no velocidad de reglas |
| volume, music_volume, sound_volume | 60,60,80; enteros0–100 |
| muted | false |
| fullscreen | false si omitido |
| window_width, window_height | 1440,900 si omitidos; mínimo configurado960×600, máximo8192 por eje |
| fullscreen_width, fullscreen_height | Escritorio si omitidos; mismo intervalo y modo SFML válido |

JSON de preferencias completo válido, adaptable a modos del monitor: `{"grid":true,"effects":true,"ai_delay":0.55,"volume":60,"music_volume":60,"sound_volume":80,"muted":false,"fullscreen":false,"window_width":1440,"window_height":900}`. Las dimensiones fullscreen opcionales preservan la selección independiente al guardar. Si no existe settings se conservan miembros iniciales; un documento inválido informa el problema. No se afirma transacción de carga total de preferencias: la función asigna campos en orden antes de capturar errores.

Content/Config.json arranca el motor con windowTitle=Plaguefront, windowSize=[1440, 900], referenceSize=[1440, 900], minimumWindowSize=[960, 600], windowIcon=Content/Textures/Plaguefront/emblem.png, disableSfmlLogs=true, maximumDeltaTime=.03, globalVolume=100, backgroundColor=[100, 100, 100], cursorRadius=5, cursorSpeed=800, cursorColor=[255, 100, 0], joystickDeadzone=.1. joystickDeadzone continúa leyéndose por compatibilidad, pero el control actual usa GamepadConfig.deadzone y no ese campo heredado; tampoco son parámetros de reglas tácticas.

WindowManager agrega tamaños comunes 1280×720, 1366×768, 1440×900, 1600×900, 1920×1080, 2560×1440, 3840×2160 cuando caben en escritorio, además de actual/escritorio. En Windows, RefreshMonitor consulta los modos del dispositivo de la ventana con EnumDisplaySettingsW antes de entrar en fullscreen; fuera de Windows se conserva getFullscreenModes de SFML. Guarda posición/tamaño y colocación de ventana en memoria para restaurar; la posición no se serializa en settings. Limita render a 60 FPS y desactiva repetición de teclas.

### 5.5. Selector desplegable de resolución

En Configuración, `Resolution` abre/cierra la lista; `ResolutionSelect(value=índice)` confirma una opción concreta y `ResolutionScroll(value=±1)` desplaza la vista. Game conserva opciones, índice, desplazamiento y modo de apertura en `resolutionOptions_`, `resolutionOption_`, `resolutionScroll_`, `resolutionFullscreen_` y `resolutionOpen_`. `RenderResolutionSelect` dibuja hasta ocho filas; `ToggleResolutionSelect`, `CloseResolutionSelect`, `MoveResolutionSelect` y `HandleResolutionEvent` comparten ese estado entre ratón, teclado y mando.

Las opciones corresponden al modo activo de `WindowManager::Resolutions()` y se ordenan por ancho y alto. En ventana puede añadirse el tamaño actual personalizado en el lugar correspondiente. Abrir o recorrer no solicita un cambio: la selección confirmada llama a `SetResolution`, cierra y guarda mediante `SaveSettings`; la aplicación continúa diferida en `ApplyPending`. Configuración conserva los campos de 5.4, sin un formato nuevo de preferencias.

| Entrada del selector | Contrato |
|---|---|
| Clic en opción, Enter/Espacio o Confirmar del mando | Confirmar directamente la resolución enfocada |
| Arriba/Abajo, Tab/Mayús+Tab, cruceta/stick | Recorrer opciones |
| Inicio/Fin, RePág/AvPág | Extremos o páginas de opciones |
| Rueda o ▲/▼ | Desplazar la lista |
| Esc, clic exterior/derecho o Cancelar del mando | Cerrar sin cambiar resolución; consumir clic exterior |
| Cambio de pantalla o modo fullscreen/F11 | Cerrar la lista anterior |
| Pérdida de foco o Resized | Cerrar sin aplicar ni guardar la opción pendiente |

[Implementación y práctica](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-32). El selector reemplaza la rotación de un tamaño por pulsación de C0/C1.

## 6. Recursos y licencias

### 6.1. Atlas actuales

Runtime carga `Content/Textures/Plaguefront/Pixel/`; `art.json.pixel_art.format_version=4` describe el contrato y conserva el bloque `textures` histórico. No es un cargador automático de cualquier atlas: los rectángulos de PixelRenderer deben cambiar de forma coherente al modificar el manifiesto. PNG de píxeles nativos con alfa0/255, vecino más cercano; texto de salida usa suavizado independiente.

| PNG en Pixel/ | Dimensión | Marco / disposición | Ancla |
| --- | --- | --- | --- |
| terrain.png | 1920×128 | 48×32;40×4 | (24,16) |
| props.png | 512×320 | 64×80;8×4 | (32,68) |
| units.png | 2304×3584 | 48×64;48×56 | (24,56) |
| effects.png | 384×192 | 48×48;8×4 | (24,24) |
| icons.png | 128×64 | 16×16;8×4 | (8,8) |
| ui.png | 96×32 | 32×32;3×1;bordes8 | Nueve regiones |
| menu.png | 720×450 | Imagen completa, opaca, sin texto impreso | (0,0) |
| cursors.png | 144×24 | 24×24;6×1 | Punto activo(1,1) |
| unit-icons.png | 168×48 | 24×24;7 clases×2bandos | (0,0) |

Unidades: columna=estado×8+frame, fila=clase+7×bando+14×dirección; rectángulo=(48×columna, 64×fila, 48, 64). Estados Idle 0, Walk 1, Attack 2, Hurt 3, Death 4, Cast 5; frame 0…7. Direcciones SE/frente 0, SO/frente 1, NE/espalda 2, NO/espalda 3. Dos dibujos corporales y sus espejos, caballo de perfil; no seis orientaciones volumétricas. Hay 7×2×4×6×8=2688 marcos, sin clip exclusivo por habilidad. Death 7 queda como cadáver identificable tras terminar la animación.

Terreno: columna=`Terrain+10*TerrainVariant(cell.pos)`, cuatro bloques de diez tipos y cuatro filas ambientales a 5 fps, 160 marcos. `TerrainVariant` mezcla las coordenadas axiales como unsigned de 32 bits y devuelve 0…3; es estable para coordenadas negativas y no depende de cámara, tiempo, altura o selección. Cada variante cambia la composición del material; las filas siguen siendo fases temporales. Superficie 40×28 dentro de 48×32 con la máscara original; caras laterales y uniones de caminos/puentes construidas por renderer. Props columnas bosque 0, ruinas 1, roca 2, casa 3, fortaleza 4, fuerte 5, restos 6, fuente 7; cuatro pasos a 5 fps. Banderas/insignias de dueño se dibujan desde estado, sin azul impreso en edificios.

Efectos columnas corte 0, flecha 1, fuego 2, hielo 3, rayo 4, curación 5, putrefacción 6, purificación 7; cuatro pasos. UI parches panel 0, botón 1, resaltado 2; centro 16×16 uniforme y ornamentación en borde 8. Cursor Normal 0, Move 1, Attack 2, Capture 3, Economy 4, Blocked 5; 24 px físicos con salida altura<1200 y 48 px a partir de 1200. Interactivo consulta puntero/foco real al dibujar; captura oculta usa última SetPosition o posición virtual, explícitamente para la prueba.

| Índices de icons.png | Contenido en orden |
| --- | --- |
| 0–7 | Saber, Archer, Lancer, Rider, Fire/Caster, Assassin, Berserker, Guard |
| 8–15 | Potion, LargePotion, ManaPotion, Antidote, Purifier, oro, captura, construcción |
| 16–23 | Clear, Rain, Fog, Storm, putrefacción, cadáver, movimiento, fin activación |
| 24–31 | Precise, Mark, Pike, Carga, Ice, Lightning, Heal, Smoke |

Azul/cruz usa telas #1e3e76, #2866b7, #67a9e3, #aad5f0; carmesí/doble barra #571c32, #a92e4c, #de5b6c, #f4b09c. Hierro, piel, cuero, madera y magia se conservan entre bandos; no se tiñe la imagen completa. Caster conserva túnica violeta y cambia estola/capa; caballo conserva pelo marrón y cambia manta. art.json conserva las rampas de materiales, pero la portada usa su paleta adaptativa y algunos recursos históricos contienen tonos adicionales. El presupuesto de colores o un mínimo de píxeles compartidos son ayudas de autoría, no requisitos del renderer.

### 6.2. Producción y verificaciones artísticas

El paquete parte de PNG originales creados con image_gen y conservados en `Tools/Art/Sources/`; `BuildApprovedArt.ps1` reconstruye localmente los atlas con Pillow/NumPy, sin pedir nuevas imágenes ni descargar dependencias. `BuildUnits.py` empaqueta los 2688 marcos de units.png desde diseños, contactos y cuerpos caídos, y recupera Idle mediante copia exacta del atlas histórico aprobado `Sources/Units/Approved/approved-atlas.png`, SHA256 `fa1b4944a505b7c22da6fb3574d4148f086556b73b4602006b03202b2f2ed603`. La imagen proporcionada explícitamente por el usuario confirma la identidad de `Sources/Units/approved-saber-lancer.png`, de 1086×1448: misma geometría y alfa, con diferencia RGB máxima 7. No se sustituye ese diseño por una nueva generación. `BuildIdle.py` delega en la misma recuperación exacta de 448 celdas; `BuildUnits.py --restore-approved-idle` y su alias `--idle-only` conservan literalmente las otras 2240 celdas del destino. La ejecución completa conserva las reparaciones compartidas de flutter/encaje de Rider y añade la reparación localizada del rig Walk de Berserker descrita a continuación. Ninguna de estas entradas escribe unit-icons.png: los retratos aprobados permanecen.

El atlas final integrado tiene SHA256 `a2aba86fe852c0b99f9d82ba63dad68f2f397220e267602b5411feb6843b86e0`: **448 Idle son copias exactas del atlas aprobado, 64 Walk de Berserker reparan la composición del faldellín y las otras 2176 celdas son idénticas al baseline de esta revisión (`0d6333b6639e…`)**. El rig tenía el faldellín en una máscara de pierna; la otra pierna llegaba a ocultar cuatro de los seis píxeles de las barras carmesí. Se reasignan los píxeles originales de tela y contorno a la cintura, delante de ambas piernas, sin repintar insignia, cambiar diseño ni añadir una capa nueva en runtime. La corrida CTest final aprobada se registra en 7.9. Las 112 poses de `Sources/Units/Idle/` y su validación describen la etapa de 7.8, no la fuente activa de la recuperación actual. Las nuevas propuestas Walk quedaron descartadas bajo `build/unit-integration/discarded-generated-walk/`, con sus prompts, sin integrarse.

`BuildEnvironment.py` recorta variantes de material, conserva la máscara hexagonal y anima reflejos/follaje; `BuildCasterEffects.py` sustituye únicamente las columnas 2,3,4,5,7 de effects.png desde `Sources/Effects/caster-impacts.png` y conserva 0,1,6 desde `base-effects.png`. La portada procede de `Sources/menu-approved.png`. Esta revisión de unidades no modifica el entorno, retratos, portada, efectos mágicos ni sus paletas. La corrección temporal y de contacto se registra por separado de la identidad artística.

Los muestrarios del rediseño están en `build/art-final/units/` y `build/art-final/environment/`, con hojas de clases/frente/espalda, contactos, cuerpos, estados y variantes. Las composiciones de tablero y GIF del generador son muestras de autoría, no capturas de la aplicación. `build/visual-corrections/effects/validation.json` y `caster-impacts-3x.png` describen los veinte marcos mágicos localizados. La etapa anterior guarda resultados reales por tiempo bajo `build/visual-corrections/runtime/`, contrastables con 7.8. Las comparaciones actuales de identidad y la ejecución de integración se separan bajo `build/unit-integration/`; véanse 7.9 y 9.7 antes de atribuirles una aceptación final.

El generador procedural anterior permanece detrás de `Tools/GeneratePixelArt.ps1 -Legacy`. Sus funciones CharacterHead, Torso, Mask, Limb, Hand, Boot, Horse y Fallen, sus nueve PNG y `build/phase4-art` son antecedentes. Su medición histórica de 2848 marcos no vacíos, 336 secuencias de ocho hashes distintos y 1344 parejas de facción no certifica el paquete raster nuevo. Los validadores actuales separan dimensiones, alfa, máscara, recortes y reproducción temporal; los hashes distintos tampoco certifican naturalidad del movimiento. La opción predeterminada de GeneratePixelArt.ps1 delega en el paquete aprobado y no activa Legacy accidentalmente.

### 6.3. Paquete inicial preservado

Fuera de Pixel/, GenerateArt.ps1 produce con semilla 731051 cinco PNG originales: tiles.png 512×128(8×2 de 64×64, ancla 32, 32, radio visual 31), units.png 448×128(7×2 de 64×64), icons.png 256×128(8×4 de 32×32), menu.png 1440×900 y emblem.png 128×128. El emblema continúa como icono de ventana; los otros cuatro son históricos, no atlas runtime.

En tiles inicial: 0…9 coinciden terrenos; 10 casa, 11 fortaleza, 12 fuerte, 13 putrefacción, 14 cadáver, 15 vacío. Unidades iniciales: clase por columna, bando azul/ocre por fila, un solo marco sin animación prerenderizada. Iconos conservan los 32 significados actuales. Sus colores históricos y atlas se conservan en art.json.textures/palette; el bando ocre no describe el actual carmesí. Estos recursos y generador no se eliminan durante consolidación documental.

### 6.4. Tipografías y procedencia legal

| Recurso | Uso y procedencia | Aviso |
| --- | --- | --- |
| Content/Fonts/Caudex-Regular.ttf | Títulos/nombres grandes; copia sin modificar desde build/_deps/harfbuzz-src/test/subset/data/fonts/Caudex-Regular.ttf | [CaudexLicense.txt](../Content/Fonts/CaudexLicense.txt): Copyright2011 Hjort Nidudsson, SIL OFL1.1 |
| Content/Fonts/DejaVuSans.ttf | Cifras, estadísticas, ayudas; copia local desde build/_deps/tgui-src/tests/resources/DejaVuSans.ttf | [DejaVuSansLicense.txt](../Content/Fonts/DejaVuSansLicense.txt): Bitstream/DejaVu/Arev |
| Content/Fonts/Relidux.otf | Recurso previo del motor; no se usa para títulos medievales de Game | Conservar avisos originales asociados |

SHA256 histórico Caudex origen/copia: DBB493E1ADC50AAEC52071535E6FCCF4176793C79545F54D95A812CBFB85169B. Fuentes locales no requieren instalación del sistema. PNG y WAV son producción original local del proyecto; no se atribuye a sus generadores la autoría de las fuentes. [LICENSE.md](../LICENSE.md) conserva el aviso MIT del repositorio, Copyright 2026 Carlos Cortina; los avisos específicos de fuentes mantienen sus términos originales.

### 6.5. Audio y manifiesto completo

audio.json.version=1 declara 42 cues bajo Content/Audio/Plaguefront, PCM 16 bits con signo, mono, 22050 Hz. Las entradas actuales contienen id, file, bus, gain, loop, cooldown; el lector exige id/file/bus y usa valores omitidos gain 70, loop=false, cooldown=.08. AudioDirector::Start carga una vez; bus music/ambient abre stream, effects precarga buffer. Las rutas de tabla se anteponen con Content/Audio/Plaguefront/. Duraciones consultadas en los WAV locales.

| id | Archivo | Bus | Gain | Bucle | Cooldown(s) | Duración(s) |
| --- | --- | --- | ---: | --- | ---: | ---: |
| music_menu | `Music/menu.wav` | music | 58 | sí | 0 | 24 |
| music_battle | `Music/battle.wav` | music | 51 | sí | 0 | 19.2 |
| music_victory | `Music/victory.wav` | music | 58 | no | 0 | 6.4 |
| music_defeat | `Music/defeat.wav` | music | 58 | no | 0 | 6.4 |
| ambient_battle | `Ambient/battle.wav` | ambient | 30 | sí | 0 | 18 |
| hover | `Sfx/hover.wav` | effects | 30 | no | 0.12 | 0.9 |
| select | `Sfx/select.wav` | effects | 65 | no | 0.08 | 0.9 |
| confirm | `Sfx/confirm.wav` | effects | 65 | no | 0.08 | 0.9 |
| cancel | `Sfx/cancel.wav` | effects | 65 | no | 0.08 | 0.9 |
| menu | `Sfx/menu.wav` | effects | 65 | no | 0.08 | 0.9 |
| open | `Sfx/open.wav` | effects | 65 | no | 0.08 | 0.9 |
| close | `Sfx/close.wav` | effects | 65 | no | 0.08 | 0.9 |
| notice | `Sfx/notice.wav` | effects | 78 | no | 0.08 | 1.5 |
| handoff | `Sfx/handoff.wav` | effects | 65 | no | 0.08 | 0.9 |
| move | `Sfx/move.wav` | effects | 48 | no | 0.15 | 0.9 |
| sword | `Sfx/sword.wav` | effects | 65 | no | 0.08 | 0.9 |
| dagger | `Sfx/dagger.wav` | effects | 65 | no | 0.08 | 0.9 |
| axe | `Sfx/axe.wav` | effects | 65 | no | 0.08 | 0.9 |
| arrow | `Sfx/arrow.wav` | effects | 65 | no | 0.08 | 0.9 |
| spear | `Sfx/spear.wav` | effects | 65 | no | 0.08 | 0.9 |
| charge | `Sfx/charge.wav` | effects | 65 | no | 0.08 | 0.9 |
| impact | `Sfx/impact.wav` | effects | 65 | no | 0.15 | 0.9 |
| death | `Sfx/death.wav` | effects | 65 | no | 0.08 | 0.9 |
| ability | `Sfx/ability.wav` | effects | 65 | no | 0.08 | 0.9 |
| spell_fire | `Sfx/spell_fire.wav` | effects | 65 | no | 0.08 | 0.9 |
| spell_ice | `Sfx/spell_ice.wav` | effects | 65 | no | 0.08 | 0.9 |
| spell_lightning | `Sfx/spell_lightning.wav` | effects | 78 | no | 0.08 | 1.5 |
| spell_heal | `Sfx/spell_heal.wav` | effects | 65 | no | 0.08 | 0.9 |
| spell_support | `Sfx/spell_support.wav` | effects | 65 | no | 0.08 | 0.9 |
| incinerate | `Sfx/incinerate.wav` | effects | 65 | no | 0.08 | 0.9 |
| purify | `Sfx/purify.wav` | effects | 65 | no | 0.08 | 1.5 |
| rot_birth | `Sfx/rot_birth.wav` | effects | 78 | no | 0.08 | 1.5 |
| rot_pulse | `Sfx/rot_pulse.wav` | effects | 48 | no | 5 | 1.9 |
| capture_house | `Sfx/capture_house.wav` | effects | 65 | no | 0.08 | 0.9 |
| capture_fortress | `Sfx/capture_fortress.wav` | effects | 78 | no | 0.08 | 0.9 |
| recruit | `Sfx/recruit.wav` | effects | 65 | no | 0.08 | 0.9 |
| buy | `Sfx/buy.wav` | effects | 65 | no | 0.08 | 0.9 |
| income | `Sfx/income.wav` | effects | 48 | no | 0.15 | 0.9 |
| build | `Sfx/build.wav` | effects | 65 | no | 0.08 | 0.9 |
| item | `Sfx/item.wav` | effects | 65 | no | 0.08 | 0.9 |
| turn | `Sfx/turn.wav` | effects | 48 | no | 0.08 | 0.9 |
| round | `Sfx/round.wav` | effects | 65 | no | 0.08 | 0.9 |

AudioManager limita gain/volúmenes0–100, cooldown 0–30; máximo 12 voces de efectos reutilizadas, reemplaza la más antigua si están ocupadas. Una música y un ambiente independientes; repetir pista actual no reinicia; cambiar detiene anterior del bus. Volumen stream/voz=gain×canal/100; maestro/silencio usa Listener. Ambiente usa canal de efectos. Silencio conserva niveles y streams continúan avanzando.

AudioDirector::SetScene acepta menu, battle, victory, defeat; salir de batalla detiene ambiente. Observe compara estado y CombatEvent nuevos; respeta contacto de arma/conjuro y respuestas de la misma orden. Existe inferencia de compatibilidad para instantáneas sin eventos y registros acotados por sufijo preservado. Update mantiene cues retrasados/pulso de putrefacción cada 7.5 s con cooldown adicional5 s; no ejecuta daño ni avanza rondas.

Tools/GenerateAudio.ps1 usa Add-Type/C# local: osciladores armónicos, ruido filtrado, transitorios, vibrato y envolventes. Menú 24 s (8 compases en re menor), batalla 19.2 s(a100pulsos/minuto), fanfarria victoria/derrota6.4s, ambiente 18 s; 37efectos0.9–1.9 s. Pico limitado a.84; colas que exceden bucle se mezclan al principio y capas sostenidas usan períodos enteros. Es instrumentación sintetizada original, no orquesta grabada ni adaptación de temas de otros juegos.

### 6.6. Comandos de regeneración opcional

```powershell
python -B Tools/Art/BuildUnits.py
python -B Tools/Art/BuildCasterEffects.py
python -B Tools/Art/BuildEnvironment.py --verify-only
```

Estos son los comandos localizados: `BuildUnits.py` sin flag reconstruye units.png con la recuperación exacta del Idle aprobado, las reparaciones compartidas y el rig de cintura Walk de Berserker, y conserva unit-icons.png; `BuildCasterEffects.py` escribe solo las cinco columnas mágicas de effects.png. `BuildEnvironment.py --verify-only` compara los píxeles del entorno en memoria sin escribir. Requieren Python 3, Pillow y NumPy. `BuildUnits.py --restore-approved-idle` (alias `--idle-only`), equivalente a `BuildIdle.py`, copia sólo 448 celdas aprobadas y conserva literalmente los 2240 marcos restantes del atlas de destino; no vuelve a generar las 112 poses de la etapa anterior ni repara otros estados, incluidos los 64 Walk de Berserker. `--output-atlas RUTA` permite producir una salida fuera del recurso activo, tanto en reconstrucción completa como en recuperación localizada. `BuildIdle.py --preview-only` produce la evidencia sin sustituir units.png. El resultado debe cotejarse con el hash final y la verificación de 7.9; el parche Idle aplicado a un atlas anterior no equivale por sí solo a la reconstrucción completa final.

Para reconstruir deliberadamente todo el paquete existen las entradas equivalentes `./Tools/Art/BuildApprovedArt.ps1` y `./Tools/GeneratePixelArt.ps1`; no son necesarias para repetir estas mejoras localizadas. El wrapper busca el Python local incluido y luego PATH. Los scripts originales GenerateAudio/GenerateScenario/GenerateArt utilizan PowerShell de Windows y, cuando corresponde, System.Drawing/Add-Type; GenerateArt acepta -ProjectRoot. Los recursos incluidos bastan para jugar. Regenerar exige revisar otra vez hashes, pruebas y render; para comparar el generador histórico, use `GeneratePixelArt.ps1 -Legacy -ProjectRoot 'RUTA_DE_COPIA_AISLADA'`.

<a id="7-construccion-y-pruebas"></a>

## 7. Compilación y pruebas

### 7.1. Entorno y dependencias

Ruta verificada: Windows x64, MSVC, CMake≥3.28, Ninja, Release, C++20. CMakeLists declara SFML 3.1.0, TGUI 1.13.0, nlohmann JSON 3.12.0, spdlog 1.16.0, magic_enum 0.9.8. TGUI usa SFML_GRAPHICS; red SFML desactivada, SPDLOG_USE_STD_FORMAT activo. Históricamente se registraron CMake 4.2.3 y MSVC 19.51; son el entorno de aquella ejecución, no requisitos mínimos inventados.

FETCHCONTENT_FULLY_DISCONNECTED y FETCHCONTENT_UPDATES_DISCONNECTED están forzados ON. Las fuentes deben existir localmente en build/_deps. Los identificadores de origen declarados en CMake fijan versión, pero esta configuración no descarga. En otro equipo se pueden suministrar fuentes por copia local e indicar FETCHCONTENT_SOURCE_DIR_SFML, JSON, LOGS, TGUI, MAGIC_ENUM (prefijo FETCHCONTENT_SOURCE_DIR_ para cada nombre). No mezclar caché MSVC con MinGW; otra toolchain requiere otra carpeta.

Desde raíz, en terminal de desarrollo MSVC x64: 

```powershell
Set-Location 'D:\Proyectos\SFML\Plagefront'
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
ctest --test-dir build --output-on-failure
& .\build\Plaguefront.exe
```

Objetivo AwesomeEngine conserva nombre del motor y produce build/Plaguefront.exe. PlaguefrontLogic compila Rules/Persistence/AI con JSON, sin SFML. PlaguefrontPresentationLogic añade Presentation sobre esa biblioteca, sin ventana. AwesomeEngine enlaza presentación, SFML Graphics/Audio, JSON, spdlog, TGUI, magic_enum. MSVC usa /W4 y /utf-8 en aplicación/núcleo/presentación; Release es la configuración verificada.

Alternativa específica de este equipo: `./Tools/BuildLocal.ps1 -Test` localiza `C:\Program Files\Microsoft Visual Studio\18\Community\Common7\Tools\VsDevCmd.bat`, configura Release y compila. Si no existe esa ruta, usar terminal MSVC y comandos anteriores; no es detección universal de cualquier instalación. -Smoke ejecuta además el runner gráfico. Jugar.cmd abre el ejecutable compilado desde raíz. EngineConfig busca Content junto al ejecutable y hasta carpetas superiores en Windows.

### 7.2. Matriz de suites y comandos aislados

Los recuentos siguientes identifican la ejecución histórica de normalización C0; las ampliaciones y la ejecución de C2 se registran en 7.7. Los nombres CTest y targets continúan siendo los comandos válidos para el árbol vigente.

| Nombre CTest exacto | Target / ejecutable | Cobertura base y resultado histórico |
| --- | --- | --- |
| PlaguefrontRules | PlaguefrontTests | 18 grupos/550 comprobaciones: geometría, rutas, reglas, habilidades, ambiente, economía, escenarios, guardado e IA |
| PlaguefrontCampaigns | PlaguefrontCampaignTests | 7campañas y2regresiones,402validaciones integrales tras comandos |
| PlaguefrontPresentation | AwesomeEngine→Plaguefront.exe --smoke | Aplicación real/71 capturas+2 consumidores; añade muestras temporales en visual-corrections/runtime; gráfica, serial, timeout60s; resultados nuevos en 7.8 |
| PlaguefrontProjection | PlaguefrontProjectionTests | Base histórica 15 grupos/6553 checks; C2 amplió a17 y las correcciones visuales añaden5 grupos de Idle, lluvia, etapas mágicas, incendios y soporte no mágico; ejecución nueva en 7.8 |
| PlaguefrontAudio | PlaguefrontAudioTests | 42 WAV/436 checks: decodificación, forma de onda, buses, voces, eventos/secuencia; audio, serial, timeout60s |
| PlaguefrontPhase3 | PlaguefrontPhase3Tests | 5 grupos/7790 checks; viewports, rutas, poses, mapa188y3campañas |
| PlaguefrontPhase4 | PlaguefrontPhase4Tests | 17 grupos/733 checks; turnos, respuesta, cadenas, fuego, captura, migración/capacidad |
| EngineManagers | EngineManagerTests | 339 checks; transformaciones SFML, ventana, documentos, recursos, captura; graphics;engine, serial |
| InputManager | InputManagerTests | 5 grupos/178 checks; eventos SFML, capacidad/perfiles, foco, hotplug, repetición, persistencia; input;engine |

Las suites lógicas/proyección no necesitan ventana. InputManager usa tipos SFML y capacidades simuladas, no exige mando físico. Todas usan comprobaciones activas en Release, no dependen de assert/NDEBUG. Tests puros se ejecutan desde raíz para resolver datos; CTest fija WORKING_DIRECTORY. El runner gráfico comienza desde build y EngineConfig localiza Content. Archivos de prueba van bajo build/test-output y no sustituyen guardados del usuario.

```powershell
ctest --test-dir build -LE "graphics|audio" --output-on-failure
ctest --test-dir build -R "^PlaguefrontRules$" --output-on-failure
ctest --test-dir build -R "^PlaguefrontProjection$" --output-on-failure
ctest --test-dir build -R "^PlaguefrontPhase4$" --output-on-failure
ctest --test-dir build -R "^EngineManagers$" --output-on-failure
ctest --test-dir build -R "^InputManager$" --output-on-failure
cmake --build build --target PlaguefrontAudioTests
ctest --test-dir build -R "^PlaguefrontAudio$" --output-on-failure
& .\build\Plaguefront.exe --smoke
```

El filtro -LE excluye por etiquetas; no evita toda dependencia de SFML: InputManager sigue enlazando SFML Window. --smoke ejecuta la aplicación, no un render de muestra externo. Conserva 28 fixtures originales y 21 de reglas/interfaz, añade 22 adaptables y dos escenas de compatibilidad. Al final del último fixture llama también a `SmokeVisualCorrections`, que avanza el director con deltas controlados y lee píxeles del renderer real; no depende de que el bucle histórico invoque `Engine::Update`. Excepciones identifican el paso exacto y devuelven código 3; fallo de PNG devuelve 2.

### 7.3. Resultado final de normalización

**15 de octubre de 2026.** La referencia inicial aprobó 9/9 suites en 17,31 s ([registro](../build/normalizacion-baseline/tests.log)). Tras traducir comentarios se recompiló Release sin advertencias nuevas y aprobaron 9/9 en 18,22 s ([compilación y CTest](../build/normalizacion-final-verification.log), [detalle](../build/normalizacion-final-test-details.log)). Después de consolidar y retirar los documentos anteriores, la **verificación final aprobó 9/9 suites en 17,93 s**: [construcción y CTest finales](../build/normalizacion-consolidacion-final.log), [detalle final de cada suite](../build/normalizacion-consolidacion-test-details.log). Ninja confirmó que no quedaban objetos por reconstruir. No se cambiaron reglas ni se añadieron pruebas para las traducciones; se ejecutaron las existentes sobre el proyecto final.

| Recorrido dentro de aplicación | Comprobaciones aprobadas |
| --- | ---: |
| Regresión UI original | 347 |
| Controles ampliados de catorce tropas | 143 |
| Casos de turnos/combate/captura | 167 |
| Layout adaptable | 3884 |
| Putrefacción runtime | 1470 |
| Mando UI simulado | 114 |
| Clicker/Bounce reales | 15 |

Layout recorre once contextos y nueve pantallas en 1280×720, 1366×768, 1440×900, 1920×1080, 1100×1100; comprueba todos los botones dentro de vista y roundtrip de centros físicos/lógicos. 24 tamaños nativos continuos, maximizar/restaurar y fullscreen conservan JSON de Match/selección. Clics SFML reales en pausa/continuar validan hitboxes; picking elevado y tamaño entero de píxel se mantienen. Las ventanas/píxeles SFML son reales, pero las interacciones son automatizadas.

Mando runtime verifica menú, preparación, selección, movimiento, ruta, habilidades, ataque, objetivo, inventario/objeto, ciclo de tropas, fin de unidad/ejército, traspaso local, pausa, ajustes, reclutamiento, compra/tienda y desconexión/reconexión. Para órdenes enfoca los botones reales y confirma con InputManager; navegación geométrica/repetición se prueba por separado con stick simulado. No representa una persona recorriendo todo con un mando físico.

Putrefacción usa siete estados de muerte/EndRound reales y compara máscara/daño de todas las celdas con RotDamageAt. Proyección incluye fuentes caducadas, superposición y peste residual. Managers comparan conversiones con SFML incluso vistas rotadas/viewport, cinco salidas, 24 resizes, preferencias anteriores, errores JSON, recursos ausentes dos veces y píxeles de captura compuesta. Los fallos de recurso esperados no son fallos de la suite.

Phase4Tests conserva los 17 grupos: activaciones intercaladas; ingresos/ambiente por ejército/ronda; reclutas; contraataque único y HP posterior; alcance/LOS/clase; Lightning; fuego; muertes/cuerpos; captura parcial guardada; captura pausada/disputada; recompensa neutral; grupo global; IA por ejército; migración de activación; migración captura/cadáver/rechazos; reserva transaccional de IDs; cadena configurable de 140 enemigos sin perder impactos. La regresión conjunta ataque/respuesta/muerte ambiental conserva letalidad del instante y tres componentes de daño en animación/audio.

Audio verifica todos los WAV, energía, clipping, huellas distintas y continuidad de extremos de bucles; manager comprueba caché, volumen/silencio, 12 voces, música única/ambiente y secuencia arma-impacto-respuesta-impacto. Las solicitudes se reproducen silenciadas; enumerar salidas o stream activo no certifica escucha humana. Campañas validan conquista legal frente a rival que pasa: límites 80 rondas/250 activaciones y 55 s globales en CampaignTests; highlands 90 rondas/450 activaciones, acotado además por timeout CTest.

La ejecución generó 73 PNG mediante ScreenshotManager; el índice de sección 9 conserva significado/dimensiones. La medición CPU del runner actual: 8 muestras, canvas 416×350, media 1.46599 ms, mínimo 1.3929, máximo 1.6015; excluye presentación/vsync y órdenes GPU pueden ser asíncronas. No es una medición de FPS sostenidos ni todos los mapas.

### 7.4. Integridad de esta intervención

El [informe final verificable](../build/normalizacion-verificacion-final.json) reúne pruebas, igualdad de tokens, cobertura de lecciones, recursos y comprobación de enlaces.

El snapshot previo conserva 128 archivos bajo build/normalizacion-baseline. [Inventario de archivos](../build/normalizacion-archivos.json), [comparación léxica C++](../build/normalizacion-verificacion-codigo.json) y [verificación de scripts](../build/normalizacion-scripts-verificacion.json) distinguen comentarios de tokens ejecutables/cadenas. Los 49 archivos C++ modificados mantienen tokens idénticos; 53 archivos de código/configuración/scripts en total. Todos los comentarios nuevos/modificados ocupan una línea física.

[87 archivos de Content conservados byte a byte](../build/normalizacion-recursos-verificacion.json): no se regeneró arte/audio, escenarios o fuentes. El resultado histórico de 94 hashes de fase 5 incluía además fuentes C++ que ahora cambian sólo comentarios; no se presenta como 94 archivos actuales byte a byte idénticos.

### 7.5. Verificación de la reconstrucción C0

Esta revisión documental del 15 de octubre de 2026 mantiene separados los resultados anteriores de normalización. Recuperó el commit A `19e1162fe338eb46e9dd0b2c563d3eebfb1572d9` mediante una exportación local y ensayó la reconstrucción dentro de build; no modificó la implementación jugable.

| Prueba ejecutada | Resultado nuevo | Evidencia |
|---|---|---|
| Motor original A con adaptación local Unity/showIncludes | Configuración y enlace aprobados | [Registro](../build/reconstruccion-etapa0-aprobada.log) |
| Motor ampliado con alias Menu→Clicker | Enlace y frame oculto, salida 0 | [Registro](../build/reconstruccion-etapa1-aprobada.log) |
| Escena mínima Plaguefront | Enlace y frame oculto, salida 0 | [Registro](../build/reconstruccion-etapa2.log), [imagen](../build/reconstruccion-etapa2.png) |
| Núcleo progresivo 3/4/5/6 | 4/4 CTest en 0,83 s; 333/383/487/550 comprobaciones | [Registro](../build/reconstruccion-nucleo-check.log), [funciones/hashes](../build/reconstruccion-nucleo/manifest.json) |
| Integración completa del taller | **9/9 CTest, 18,73 s** | [Construcción y CTest](../build/reconstruccion-integracion.log), [salidas archivadas](../build/reconstruccion-integracion-detalle.log) |
| Escenario/balance/snapshot/cue y selector/borde opcionales | Programa aprobado + smoke 1/1, 15,15 s | [Registro](../build/reconstruccion-ampliacion.log) |
| Archivos conservados antes de los ajustes C1 | **213/213 SHA256 de C0 conservados** | [Manifiesto histórico de integridad](../build/reconstruccion-integridad-C0.json) |

La política desconectada se mantuvo. Usar source overrides de terceros fuera de FETCHCONTENT_BASE_DIR requirió desactivar Unity por target en la copia del taller; se detectaron las colisiones reales de FreeType/Vorbis/FLAC. El CMake de producción permanece intacto. Renovar fechas de archivos transferidos evitó reutilizar objetos previos incompatibles. Ambos casos tienen instrucciones corregidas en la [etapa 0](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-0) y [etapa 1](CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-1).

El atlas contiene doce temas divididos en **22 bloques Mermaid**, analizados y renderizados con Mermaid 11.4.1 local sin red. [Informe de diagramas](../build/reconstruccion-mermaid-final/validacion.json). Sus 50 enlaces de nodos apuntan a IDs reales y anclas explícitas. La navegación Markdown y los SVG son complementarios; no se certifica que cualquier vista previa de editor incorpore Mermaid.

Los binarios del ensayo se ubican en build/reconstruccion-bin con su propio Content del taller; los fixtures lógicos tienen CWD build/reconstruccion-practica. Así ninguna prueba gráfica se apoya accidentalmente en Content del proyecto de referencia. Las 73 capturas nuevas están bajo build/reconstruccion-bin/build/screenshots. Los archivos de build son evidencia local regenerable; las instrucciones del curso usan la organización más simple $Taller/build.

La validación automática incluye el núcleo, consumidores heredados, eventos SFML y renderizado real. Continúan pendientes mando físico, partida humana completa, escucha de mezcla, arrastre humano de ventana, cambio exclusivo del monitor y otra toolchain. Los [laboratorios de clase](CURSO_DESARROLLO_PLAGUEFRONT.md#laboratorio-clase), [habilidad](CURSO_DESARROLLO_PLAGUEFRONT.md#laboratorio-habilidad) y [terreno](CURSO_DESARROLLO_PLAGUEFRONT.md#laboratorio-terreno) parten de C0 por separado: sus pruebas dedicadas aprobaron 68, 103 y 78 comprobaciones; también se compilaron sus integraciones gráficas y smoke. Los formatos 3 de clase y terreno son experimentos independientes. Los estados intermedios son didácticos: el historial demuestra A y C, pero no commits de cada fase de desarrollo.


### 7.6. Ajustes C1: paleta configurable y cursor en pantalla completa

C1 incorpora las correcciones solicitadas después de reconstruir C0. El snapshot [C0.zip](../build/reconstruccion-C0.zip) conserva los 213 archivos anteriores; no se utiliza el árbol mutable como material de las etapas 0–10. Las [lecciones 30](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-30) y [31](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-31) aplican después los cambios exactos al taller.

| Familia | Archivos de producción modificados | Propósito |
|---|---|---|
| Configuración visual | Engine/Include/Core/EngineConfig.h; Engine/Source/Core/EngineConfig.cpp; Content/Config.json | RotPaletteConfig, nueve RGBA, validación y valores de respaldo |
| Materiales y presentación | Games/Include/Plaguefront/Game.h; Games/Include/Config/PlaguefrontPresentationConfig.h; Games/Source/Plaguefront/PixelRenderer.cpp, TacticalOverlay.cpp, TacticalPanel.cpp | Caché de atlas derivados y colores sólo de putrefacción/peste; pulsos conservados |
| Ventana y cursor | Engine/Include/Core/Engine.h, EngineVisitor.h; Engine/Include/Managers/CursorManager.h; Engine/Source/Core/Engine.cpp, EngineVisitor.cpp; Engine/Source/Managers/CursorManager.cpp, WindowManager.cpp | Atajos compartidos, flancos, foco, viewport y política de visibilidad |
| Regresiones | Tests/EngineManagerTests.cpp; Games/Source/Plaguefront/RotSmoke.cpp | Parser/materiales y texto neutro respecto de la paleta; pruebas de fullscreen y consumidores dentro de Engine.cpp |

El fallo del cursor era una pérdida de estado al recrear el manejador nativo: WindowManager ocultaba el cursor del sistema mientras el cursor personalizado podía conservar focused_=false. ApplyWindowChanges centraliza recreación, tamaño/vista, reconciliación del foco nativo y notificación de resize a la escena. CursorManager conserva la política de visibilidad y la reaplica sobre la ventana recreada. F11 y Alt+Enter ya existían en C0; C1 conserva ambos, consume el atajo antes de GUI/Scene y bloquea repeticiones hasta KeyReleased. No se atribuye a C1 la creación histórica de esos atajos.

| Verificación ejecutada | Resultado | Evidencia |
|---|---|---|
| Parser RGBA, materiales y gestores | 9376 comprobaciones; 9037 nuevas +339 existentes | [Ensayo inicial](../build/ajustes-paleta-prueba-inicial.log) |
| Integración C1 inicial | 9/9 suites en 17,27 s | [Construcción/CTest](../build/ajustes-paleta-fullscreen-final.log), [detalle archivado](../build/ajustes-paleta-fullscreen-detalle.log) |
| Integración C1 definitiva, con texto neutro del fixture | **9/9 suites, 17,04 s** | [Registro definitivo](../build/ajustes-validacion-definitiva.log) |
| Aceptación final C1 y contrato de cursor heredado | **9/9 suites, 17,46 s; 74 comprobaciones de fullscreen y 33 de Clicker/Bounce** | [Registro final](../build/ajustes-aceptacion-final.log), [detalle](../build/ajustes-fullscreen-aceptacion-detalle.log) |
| JSON→render sin recompilar | Mismo SHA256 del ejecutable; salida 0 y píxeles cian nuevos sólo en la copia de configuración | [Experimento con píxeles](../build/palette-config-variant/experiment.json), [repetición con binario definitivo](../build/palette-config-variant/experiment-final.json), [log](../build/palette-config-variant/variant-smoke.log) |

La inspección visual de phase5-13 y phase5-18 confirma fuente, iconos, esporas, radio, peste global y advertencias morados; árboles, HP, selección y unidades conservan sus colores. La variante aislada cambia sourceBase y uiText a cian y confirma valores distintos con el mismo binario. Reabrir el proceso vuelve a cargar Config.json; no hay recarga continua. Los PNG originales permanecen intactos.

La comparación [C0→C1](../build/ajustes-integridad-C0-C1.json) registra 17 modificaciones autorizadas y ninguna inesperada; reglas, IA, persistencia táctica, balance, escenarios, PNG/WAV y LICENSE conservan sus bytes.

Las pruebas de modo de C1 utilizan ventanas SFML reales y eventos programados; sus capturas históricas ignoraban foco y la regresión nueva comprobó Draw normal. La corrección posterior de cursor al restaurar y elección del monitor de fullscreen se explica en 7.7: el usuario confirmó que ambos problemas ya funcionan. Esa confirmación no equivale a una partida humana completa ni a certificar cada combinación de hardware.

### 7.7. Presentación C2 y ventana por monitor

Esta intervención incorpora los cuatro pedidos posteriores a la confirmación del arreglo de ventana. C2 es una etiqueta documental del árbol vigente, sin cambiar versiones de escenarios o guardados. Los snapshots y resultados de C0/C1 se conservan como históricos.

| Cambio | Contrato vigente / archivos de entrada |
|---|---|
| Perspectiva del tercer mapa | Camera::SetRotation(1) gira 60° sólo Los tres pasos, elegido desde el escenario de Match en Game::ResetPresentation. Presentation.h/.cpp, Game.cpp y PixelRenderer.cpp comparten proyección, inversa, picking, profundidad y dirección visual al caminar/atacar; JSON y estado táctico conservan sus coordenadas. |
| Árbol bajo el cursor | PixelRenderer.cpp añade TreeSpriteContains y alfa 100/255 por ocupación viva, hover de celda o silueta del atlas. Mantiene el caso anterior de oclusión de unidades seleccionadas/consultadas. |
| Flecha terminal única | PixelRenderer.cpp dibuja una sola cabeza 12×7 en el centro del destino, orientada por el último tramo, con el mismo dorado y borde oscuro 14×9 que el cuerpo. Sin puntas intermedias ni cuadrado final; medidas nativas multiplicadas por zoom. |
| Resolución desplegable | PlaguefrontGameTypes.h, Game.h, Contexts.cpp, Game.cpp, Interface.cpp y Gamepad.cpp añaden apertura, selección directa, desplazamiento y cancelación; SaveSettings conserva el formato de preferencias. |
| Base de ventana ya confirmada | CursorManager.cpp dibuja en vista física del tamaño actual; WindowManager.h/.cpp elige monitor y restaura modo/colocación nativa en Windows. El usuario confirmó ambos arreglos antes de estos cuatro pedidos. |

`verifyTreeHover` en Phase5Smoke.cpp verifica píxeles de la escena real para los tres escenarios y zoom 1–3, incluida copa vacía fuera del hex, padding alfa 0, salida del puntero y ocupación sin seleccionar. Smoke.cpp (`validateSettingsRoundTrip` y paso 2) comprueba el selector con selección directa, cancelación, desplazamiento, cierre por Resized sin aplicar, lista válida en ventana/fullscreen y escritura aislada de preferencias. Los grupos `rotated large-map perspective and picking` y `rotated movement and attack facing` de PresentationTests.cpp comprueban orientación, inversa, picking, profundidad y cuerpos al caminar/atacar. La legibilidad de la cabeza de flecha requiere inspección visual; no se deduce de un resultado numérico de reglas.

El driver del runner oculto rechazó una solicitud de cambio físico de resolución fullscreen y se conservó el modo del monitor elegido, según el fallback de WindowManager. La regresión fullscreen confirma directamente su modo nativo actual; en ventana sí comprueba un cambio real de tamaño desde la lista. Esto valida el selector y su aplicación diferida, sin certificar una conmutación exclusiva física de vídeo.

Los resultados de esta tabla corresponden a C2 **antes de la corrección de punta terminal única**; se conservan como evidencia de aquella ejecución. La verificación del último ajuste se registra por separado a continuación.

| Verificación C2 previa ejecutada | Resultado | Evidencia |
|---|---|---|
| Compilación integrada final | Completada con MSVC Release | [Registro de construcción verificado](../build/presentation-update-build-verified.log) |
| Matriz CTest completa | **9/9 suites en 41,13 s** | [Resultado y detalle](../build/presentation-update-tests-final.log) |
| Cámara y animación | 17 grupos / 11654 comprobaciones en PlaguefrontProjection | Mismo registro CTest |
| Selector e interfaz | 49 comprobaciones del selector y 396 de UI; 71 capturas de Plaguefront | Mismo registro CTest |
| Layout, follaje y ventana | 4004 comprobaciones de layout, hover en tres mapas a zoom 1–3; EngineManagers: 9596 comprobaciones y dos monitores | Mismo registro CTest |
| Verificación posterior del orden de opciones y capturas finales | **PlaguefrontPresentation 1/1 en 36,38 s**; conserva las 49 comprobaciones del selector, hover en los tres mapas a zoom 1–3 y 71 capturas | [Interfaz final verificada](../build/presentation-update-ui-verified.log) |

| Verificación final de la punta terminal única | Resultado | Evidencia |
|---|---|---|
| Compilación de la corrección | Completada con el ejecutable actualizado | [Construcción final de flecha](../build/route-terminal-arrow-build-final.log) |
| PlaguefrontPresentation | **1/1 aprobada; 31,88 s de prueba / 31,92 s totales** | [Prueba final de flecha](../build/route-terminal-arrow-tests-final.log) |
| Cobertura de la regresión | 394 comprobaciones de UI, 49 del selector, 85 de ventana, 114 de mando, 4004 de layout, hover en tres mapas a zoom 1–3 y 71 capturas | Mismo registro de prueba final |

La inspección visual previa de [screen-10](../build/screenshots/screen-10.png) y [screen-16](../build/screenshots/screen-16.png) confirmó lista legible con elección directa y conjunto del mapa grande en perspectiva: el tamaño personalizado 1532×831 aparece entre las opciones de 1440 y 1600 de ancho, y el mapa grande se ve sin una ficha heredada tapando su centro. La nueva inspección de [screen-12](../build/screenshots/screen-12.png), regenerada después de la corrección terminal, confirma la ruta completa de coste 3 MOV con una sola cabeza en el destino, el mismo dorado y borde oscuro del cuerpo y una unión continua sin franja negra. El límite del modo físico del runner se mantiene.

La [lección 32](CURSO_DESARROLLO_PLAGUEFRONT.md#leccion-32) explica cada recorrido y sus comprobaciones prácticas. Las [copias de trabajo](../build/presentation-update-backup/) contienen fuentes de esta intervención y los dos documentos previos en `docs`; no constituyen un snapshot completo del motor. El snapshot C0 permanece intacto.

<a id="78-correcciones-localizadas-sobre-el-arte-aprobado"></a>

### 7.8. Correcciones visuales del paquete aprobado

**Etapa anterior, verificada el 16 de octubre de 2026: 9/9 suites aprobadas en 31.34 s, incluido el paquete Idle que se aceptó entonces como definitivo.** El recorrido gráfico terminó en 27.29 s y realizó 769681 comprobaciones nuevas de runtime. [Resultado de CTest de esta etapa](../build/visual-corrections/tests-final.log) y [detalle de cada suite](../build/visual-corrections/tests-final-details.log). Los éxitos de 7.3–7.7 corresponden a compilaciones anteriores. Se conserva aquí la historia y sus cifras; la recuperación posterior del diseño aprobado y los tiempos actuales se describen en 7.9 y necesitan su propia comprobación. No se presenta la presencia de PNG o de una spritesheet como prueba de reproducción temporal.

| Problema comprobado | Cambio localizado / autoridad conservada |
|---|---|
| Idle con movimiento poco coherente | El reloj y la selección de marcos ya avanzaban en Presentation.cpp/Game.cpp; las muestras anteriores no probaban los píxeles a lo largo del tiempo. BuildUnits.py integra los ciclos originales de BuildIdle.py por clase desde Tools/Art/Sources/Units/Idle, con escala fija y registro de apoyos. Los diseños de Walk/Attack/Hurt/Death/Cast se mantienen; la ejecución completa corrige flutter/encaje compartidos de Rider sin rediseñarlos. `--idle-only` cambia solo 448 celdas y no reproduce esas reparaciones compartidas. Conserva celda 48×64, ancla (24,56), ocho marcos y cuatro orientaciones. Las muestras temporales finales verifican la reproducción de ambos ciclos. |
| Lluvia uniforme | Weather.h reemplaza los 64 trazos iguales por una distribución variable de tres capas, con cantidad 64…360 según el viewport, viento y velocidad continua; PixelRenderer.cpp los dibuja en un lote. No se cambian las reglas de Rain/Storm, su activación ni sus modificadores. |
| Caster sin preparación/trayectoria | VisualEffect añade datos visuales; Presentation.cpp conserva el contacto/final ofensivo y la secuencia de eventos. SpellPresentation.h/SpellRenderer.cpp separan fases, proyectiles, enlaces e impactos; BuildCasterEffects.py actualiza solo veinte marcos de cinco columnas mágicas. FireAwaitsContact oculta hasta el contacto únicamente los incendios nuevos del área; conserva incendios previos/refrescados. Ataque ordinario del Caster, Fire, Ice, Lightning, Heal, Purify e Incinerate se comprueban por comandos reales; el kit, incluso usado por Caster, y el soporte no mágico mantienen sus efectos anteriores. |
| Peste global poco perceptible | RotPresentationAt ya clasificaba como global toda celda existente y RotDamageAt ya aplicaba daño global fuera del radio local. No se encontró divergencia de daño que justificase cambiar reglas. RotGround refuerza una única base global por celda y añade vetas usando la paleta configurada; mantiene centros y acentos locales. |

`Tests/PresentationTests.cpp` añade los grupos `ciclo idle temporal de todas las clases y orientaciones`, `lluvia continua y acotada en distintos viewports`, `etapas mágicas con contacto y duración conservados`, `incendio nuevo espera al contacto y conserva incendios previos` y `soporte no mágico conserva los efectos originales`. Comprueban dos ciclos Idle y límites temporales, estabilidad de coordenadas/facing/estado lógico, lluvia determinista y finita en dieciséis regiones, fases mágicas ordenadas, contacto de daño conservado, ausencia de efectos duplicados y liberación de la cola. La regresión de Fire distingue nuevos IDs de toda el área, refrescos y una acción encolada tras movimiento; el soporte incluye kit de Caster. La proyección numérica no sustituye la inspección de la animación dibujada.

`VisualCorrectionsSmoke.cpp` enlaza esos contratos con el render real:

- Reproduce dieciséis muestras de cada clase, bando y orientación mediante `AnimationDirector::Update`, tras orientar la tropa con un movimiento legal. Compara cada píxel opaco con el rectángulo del atlas seleccionado y el hash de ambas vueltas. La fase, posición y escala no se deducen de una hoja fuente.
- Compara lluvia con un fotograma despejado al mismo tiempo visual en 1280×720, 1440×900, 1920×1080 y 1100×1100, zoom 1/3 y cámara desplazada; mide movimiento y cobertura por dieciséis regiones del lienzo nativo, con presupuesto limitado.
- Recorre los tres escenarios y un fixture irregular de superficies diversas, con unidades y celdas vacías. Contrasta clasificación/daño por cada celda de `state.cells` y píxeles de su superficie; las imágenes comparativas de cobertura omiten temporalmente sprites de props/unidades para medir el suelo sin oclusión. Guarda además vistas normales con esas entidades. Activa peste mediante `EndRound`, agota las fuentes, conserva la reserva y verifica su desaparición con cierres reales; pan/zoom no condicionan la clasificación.
- Ejecuta siete acciones del Caster y muestrea Preparation/Launch/Travel/Impact/Fade. Conserva la instantánea serializada después de resolver la orden y exige que reproducción, render y desvanecimiento no la alteren; vuelve a observar la misma instantánea para detectar duplicados y comprueba la cola vacía al finalizar.

La evidencia final está en [verification.json](../build/visual-corrections/runtime/verification.json) y los PNG de cada serie. [resource-manifest.json](../build/visual-corrections/runtime/resource-manifest.json) vincula las capturas con los SHA256 de units.png (`0d6333b6639e…`), effects.png (`73c0fbaeac1d…`) y el ejecutable final. El baseline anterior a esta etapa contiene 238 hashes de proyecto, 48 PNG y nueve copias de atlas runtime, más existencia/hashes/copias de configuración y saves: [manifiesto](../build/visual-corrections/baseline/project-hashes.json), [PNG](../build/visual-corrections/baseline/png-hashes.json), [estado de usuario](../build/visual-corrections/baseline/user-state.json). Excluye build, .git, .vscode y cachés de Python.

```powershell
& .\Tools\BuildLocal.ps1 -Test
# Para repetir solo la comprobación gráfica después de una corrección nueva.
ctest --test-dir build -R '^PlaguefrontPresentation$' --output-on-failure
# Para repetir los contratos puros sin abrir la ventana.
ctest --test-dir build -R '^PlaguefrontProjection$' --output-on-failure
```

| Verificación final | Resultado y evidencia |
|---|---|
| Compilación MSVC Release | Completada; [construcción final](../build/visual-corrections/build-final.log) |
| Matriz CTest completa | **9/9 aprobadas, 31.34 s**; [registro final](../build/visual-corrections/tests-final.log) |
| Proyección, animación, lluvia, magia y regresiones de Fire/soporte | **22 grupos/27274 comprobaciones**, 0.03 s; [detalle final](../build/visual-corrections/tests-final-details.log) |
| Aplicación y muestras temporales | **769681 comprobaciones nuevas**, 27.29 s; conserva 71 pantallas anteriores y dos consumidores, además de las series nuevas |
| Dos vueltas Idle y píxeles de 56 combinaciones | **896 muestras**,16 por combinación, orden/anclaje y vuelta comprobados; tiras y [GIF de runtime](../build/visual-corrections/runtime/idle-runtime-2x.gif) |
| Lluvia, cuatro resoluciones y dos zooms | **48 muestras** de Rain/Storm, cobertura por dieciséis regiones, movimiento y presupuesto acotado; JSON de runtime |
| Caster, siete acciones y cinco fases | **35 capturas temporales**, contacto/final y estado lógico preservados, cola vacía; [montaje final](../build/visual-corrections/runtime/caster-stages-runtime.png) |
| Cobertura global de tres mapas más irregular | **436 celdas** (99+99+188+50), con activación real, cámara desplazada, reserva y fin; JSON/capturas de runtime |
| Alcance, reglas, mapas, configuración y saves | **68 archivos protegidos sin diferencias**; de 48 PNG preexistentes solo cambian units/effects, columnas 0/1/6 de effects idénticas; [informe de integridad](../build/visual-corrections/scope-verification.json) |

La comparación de celdas de units.png registra **1234 modificadas: 448 Idle y 786 por correcciones compartidas** (Walk 232, Attack 218, Hurt 56, Death 32, Cast 248); 1454 celdas ajenas a Idle permanecen idénticas. No se afirma que todo el cambio esté limitado a la primera banda de ocho columnas. [Detalle de alcance](../build/visual-corrections/units/correction-scope.json) y [registro de apoyos/escala del Idle](../build/visual-corrections/units/idle-validation.json). Los retratos, terreno, props, portada y demás PNG preexistentes se conservan.

La ejecución parcial de 8/8 en 3.80 s y la corrida diagnóstica de 755717 comprobaciones en 28.08 s con Idle anterior se conservan como antecedentes; no se utilizan para aceptar el Idle definitivo. La tabla anterior corresponde al recurso final y prevalece sobre esas comprobaciones intermedias. Se revisaron los montajes y capturas nativas finales de Idle, Caster y cobertura; las muestras son una inspección programada del juego, no una partida humana ni una medición de FPS.

El delta del fixture es controlado y recorre el mismo director y renderer de la aplicación; no certifica una sesión humana, fluidez subjetiva, prueba auditiva ni rendimiento de todos los equipos. Las capturas nuevas tienen índice en 9.6. La paleta morada continúa viniendo de Content/Config.json, sin segundo archivo de configuración ni cambios de balance autorizados por el aspecto del dibujo.

<a id="79-recuperación-del-diseño-y-contacto-de-unidades"></a>

### 7.9. Recuperación del diseño y contacto de unidades

**Verificación final del 16 de octubre de 2026: compilación Release y 9/9 suites CTest aprobadas en 36.34 s; la suite gráfica Presentation terminó en 32.33 s.** La identidad se confirmó con la imagen explícita del usuario y la corrida usa el atlas de producción `a2aba86fe852…`, sin sustitución temporal de textura. [Compilación final](../build/unit-integration/build-final.log), [CTest final](../build/unit-integration/tests-final.log) y [detalle de las suites](../build/unit-integration/tests-final-details.log). Los resultados de 7.8 pertenecen a `0d6333b6639e…`; esta ejecución comprueba el recurso recuperado. Se conservan reglas, cámara, controles, paletas, retratos, entorno y las correcciones de lluvia, peste y Caster.

| Causa comprobada | Corrección localizada / evidencia |
|---|---|
| Insignia que cambia de lugar sobre la figura | `finish_emblem` elegía de nuevo una zona de tela por pose y pintaba la insignia dentro del PNG: se midieron saltos de hasta 9 píxeles verticales. El marcador independiente del suelo no causaba ese desplazamiento corporal. Se recuperan exactamente los 448 Idle originales aprobados, sin redibujar personaje ni insignias. |
| Barras de Berserker ocultas al caminar | El faldellín pertenecía a una máscara de pierna y la pierna opuesta ocultaba cuatro de sus seis píxeles de barra. El rig reasigna los píxeles originales de tela y contorno a la cintura, delante de ambas piernas, sólo en los 64 marcos Walk de esa clase. No repinta el patrón ni modifica otras clases o estados. |
| Walk apenas perceptible | .12 s por casilla / .06 s por marco sólo mostraban dos marcos. Ahora la duración visual es ocho × .05 s = .40 s por casilla y Idle usa .18 s por marco. No se altera el coste, recorrido, posición lógica ni orden de movimiento; el audio comparte la duración visual. |
| Franja transparente antes de la sombra | El soporte Idle terminaba en la fila 55 del marco (bottom exclusivo 56), mientras la sombra empezaba un píxel después del ancla. `UnitContactGeometry` mide el alfa dentro de ventanas de apoyo por clase y refleja la ventana con la orientación; `shadowTop=bottom−1` da un píxel de encaje sin desplazar el sprite. |
| Mancha desproporcionada o demasiado gruesa | Huella proporcional al apoyo, con límites por clase, dos filas humanas y tres de Rider, núcleo alfa 90 y remate 45. Death 7 usa el contacto del cuerpo caído, en lugar de 28×5/alfa190. El caché evita leer el atlas cada fotograma y `SnapUnitPoint` comparte redondeo entre unidad, sombra e insignia de suelo. |
| Parte inferior o marcador tapados durante Walk | La profundidad del centro interpolado dejaba pintar después una superficie que ya tocaba espada, sombra o marca de suelo; también podía ocurrir con un vecino fuera de la ruta. Se comprueba la banda opaca y44…63, sombra y marcador contra superficies reales, con descarte envolvente 48×20. Sólo Walk puede subir a `suelo+.25`, conservando la profundidad mayor que ya tuviese y las entradas de props +.5. Se admiten suelos a cota igual/inferior a la elevación visual y los extremos del tramo actual; una pared ajena más alta no recibe esa excepción. |

La imagen explícita del usuario corresponde a `Tools/Art/Sources/Units/approved-saber-lancer.png`, 1086×1448, con geometría y alfa coincidentes y diferencia RGB máxima 7. El Idle se copia de `Tools/Art/Sources/Units/Approved/approved-atlas.png`, SHA256 completo `fa1b4944a505b7c22da6fb3574d4148f086556b73b4602006b03202b2f2ed603`. La celda sigue siendo 48×64 y el ancla (24,56). El atlas final integrado es `a2aba86fe852c0b99f9d82ba63dad68f2f397220e267602b5411feb6843b86e0`: **448 Idle originales exactos, 64 Walk de Berserker reparados y 2176 celdas restantes exactas respecto al baseline `0d6333b6639e…`**. [approved-idle-restore.json](../build/unit-integration/units/approved-idle-restore.json) documenta la copia localizada de Idle; [berserker-waist-scope.json](../build/unit-integration/units/berserker-waist-scope.json) documenta la reparación posterior del rig y su alcance. Los informes y comparaciones intermedios describen su propia versión, no reemplazan el hash final. Las 112 poses Idle regeneradas de la etapa anterior quedan como antecedentes; las nuevas fuentes Walk descartadas tampoco se integran.

`UnitIntegrationSmoke.cpp` recorre `Match::Move`, `Observe`, `Update`, `RenderWorld` y ScreenshotManager reales con delta controlado máximo .025 s y cámara fija. La corrida final aprobó **4338084 comprobaciones, 56 marchas completas y 112 ciclos Idle, con 1400 muestras**: reposo inicial, ocho marcos Walk y dos vueltas Idle al llegar por cada combinación de clase/bando/orientación. Verifica apoyos, sombra, marcador, conservación de insignias originales y un escenario de batalla cargado. El fixture exige las **896 anclas/máscaras** estables de `Tools/Art/Sources/Units/Approved/locomotion-anchors.json` para Idle/Walk y contrasta el patrón original sobre atlas y render, sin dibujar nuevas insignias. Las regresiones de profundidad incluyen superficies vecinas, sentidos opuestos, rotación y alturas. [verification.json](../build/unit-integration/runtime/verification.json) registra `pass=true`, el atlas de producción y `temporary_texture_override=false`.

| Comprobación final | Resultado / evidencia |
|---|---|
| Matriz CTest | **9/9, 36.34 s**; suite Presentation **32.33 s**, logs finales indicados arriba. |
| Contratos puros de presentación | **29 grupos y 38318 comprobaciones**; anclaje/contacto, tiempos, profundidad, lluvia y magia. |
| Integración temporal de unidades | **4338084 comprobaciones**, 56 marchas, 112 ciclos Idle y 1400 muestras; JSON y series del renderer. |
| Regresión de correcciones visuales | **755717 comprobaciones** de VisualCorrectionsSmoke; mantiene las rutas de lluvia, peste y Caster. |
| Audio | **454 comprobaciones**; conserva la sincronización con la duración visual vigente. |
| Alcance y estado del usuario | **68 archivos protegidos sin diferencias**; sólo cambia units.png entre los PNG runtime, configuración y guardados intactos; [auditoría de alcance](../build/unit-integration/scope-verification.json). |

Los GIF enlazados en 9.7 se componen de capturas reales: mantienen el reposo inicial durante 700 ms para facilitar la lectura, muestran los ocho marcos de marcha y la llegada con dos ciclos Idle. Las secuencias de marcha no se repiten automáticamente, evitando que un retorno del montaje al inicio parezca una teletransportación del juego. [resource-manifest.json](../build/unit-integration/runtime/resource-manifest.json) reúne la identidad del recurso, ejecutable e informes de esta entrega. Las cifras de rendimiento anteriores se conservan en sus secciones históricas y no se extrapolan a esta revisión. El resultado prueba reproducción y conservación de estado; no afirma fluidez subjetiva, una partida humana ni una medición sostenida de FPS.

### 7.10. Marcha acelerada verificada; nuevo Idle pendiente

**Cierre parcial: la reducción de duración está integrada y verificada; la nueva animación Idle no está resuelta ni integrada.** La configuración pasa de .05 s por marco y .40 s por casilla a .035/.28, un 30% menos, sin acelerar Idle, ataques, Hurt, Death, Cast, contactos ni cadenas. El nuevo grupo `FasterWalkPreservesCadenceAndOtherStates` de `Tests/PresentationTests.cpp` comprueba los ocho marcos, interpolación, destino, estado lógico y liberación de Busy al llegar con deltas variables de 13, 30, 21, 29 y 17 ms. La compilación Release y **9/9 suites CTest aprobaron en 37.83 s**, con Presentation en **33.72 s**: [compilación](../build/idle-agility/build-final.log), [CTest](../build/idle-agility/tests-final.log) y [detalle](../build/idle-agility/tests-final-details.log). Estos resultados verifican la velocidad y la regresión del paquete vigente, no un Idle nuevo.

La investigación de recursos produjo dos hojas de 32 poses cada una, rechazadas por poses casi duplicadas, pérdida de insignias e incoherencias de uniones. Después se preparó un piloto Saber con la pose completa original como marco 0 y siete ediciones independientes del personaje entero, sin rig ni separación móvil de cabeza, torso, brazos, piernas o capa. Cumple ese método de autoría, pero **también se rechaza por inestabilidad**: el marco 2 termina en bottom exclusivo 55 frente a 56 de la referencia; en el marco 6 el borde izquierdo de la capa se retrae tres píxeles y persisten variaciones de píxeles de armadura. Ocho imágenes distintas no bastan para aceptar identidad, apoyo o continuidad.

El atlas de producción sigue siendo `a2aba86fe852c0b99f9d82ba63dad68f2f397220e267602b5411feb6843b86e0`: **sus 2688 marcos permanecen idénticos** respecto al inicio de esta investigación. Se conserva el último recurso válido; no se propagó el piloto a otras clases o facciones ni se alteraron los dibujos Walk, entorno, efectos, interfaz o paletas. Los PNG/GIF de 9.8 son muestras de autoría rechazadas, sin reproducción nueva verificada en el juego.

La última instrucción del usuario exige mostrar primero un piloto válido y su vista previa y establece: «Después de mi aprobación, integra el recurso en el juego». Esa integración sigue pendiente; el piloto fallido no se presenta para solicitar aprobación ni como solución terminada. Antes de extenderlo harán falta poses coherentes, revisión a escala nativa y del ciclo 7→0, comprobación de apoyo e insignia y la aprobación solicitada; después corresponderá verificar el nuevo Idle dentro del juego y registrar una secuencia temporal real. No se cambian de nuevo los temporizadores para intentar ocultar el defecto de autoría. No se afirma naturalidad certificada, partida humana ni medición sostenida de FPS.


## 8. Diagnóstico y límites

### 8.1. Síntoma, evidencia y punto de inspección

| Síntoma | Diagnóstico reproducible / responsables |
| --- | --- |
| CMake intenta resolver dependencia ausente | Comprobar build/_deps y FETCHCONTENT_SOURCE_DIR_ correspondiente; configuración desconectada requiere fuentes locales. |
| MSVC C1902 durante detección Debug | Usar Release y CMAKE_TRY_COMPILE_CONFIGURATION=Release como comandos verificados; no hay alternativa Debug certificada. |
| Cabecera cambia pero objeto no se reconstruye | Revisar /showIncludes y CMAKE_CL_SHOWINCLUDES_PREFIX español UTF8 de CMakeLists; el fallo histórico CP850 mezcló tamaños de clases. No quitar corrección. |
| F5/VSCode no reproduce terminal | tasks.json apunta Debug y settings.json conserva query-driver MinGW/Build; sólo BuildLocal/terminal MSVC Release se verificó. |
| Content/Config.json o PNG/fuente no encontrado | Arrancar Jugar.cmd/desde raíz; en Windows EngineConfig busca Content en ubicación del ejecutable/superiores. Revisar CachedFont/FetchTexture y ruta real. |
| Segunda petición de recurso faltante parece correcta | ResourceManager sólo inserta tras carga correcta; EngineManagers exige dos fallos consecutivos. No introducir entrada vacía por operator[]. |
| Clic elige celda distinta tras resize/zoom | Comparar OutputSize, LogicalSize, UiScale, Revision y viewport de RenderManager; luego BoardOrigin, WorldScale, Camera::Pick/Surface/altura. No usar inversa regular. |
| Clic o profundidad incorrectos sólo en Los tres pasos | Revisar Camera::Rotation()==1 desde ResetPresentation y que Project/Inverse/Pick/Depth/DepthContinuous compartan orientación; las coordenadas de Match permanecen sin giro. |
| Árbol no se atenúa al pasar por su copa | Comprobar cursor dentro de Playing/BoardBounds, conversión a world, ancla redondeada y TreeSpriteContains sobre alfa del marco; no basta hovered_ porque la copa sobresale del hex. |
| Selector aplica al recorrer o usa opciones de otro modo | Resolution abre/cierra; sólo ResolutionSelect llama SetResolution/SaveSettings. Revisar cierre por cambio de modo y resolutionFullscreen_; MoveResolutionSelect sólo enfoca/desplaza. |
| Botón visible no responde o se solapa | Ver pantalla/contexto, bounds, enabled, layoutRevision y RebuildControls antes de OnEvent; comprobar conversión del propio evento, no ratón de otro frame. |
| Orden deshabilitada incorrectamente | Consultar CheckAction/Result.message y probar comando en copia activada mediante PreviewRules; CanTarget no valida acción consumida. Invalidar caché cuando cambie una condición. |
| Fuente/cadáver muestra daño antes de tiempo | Inspeccionar round/deathRound/remaining, Distance, rotRadius, LargestSourceGroup, globalRemaining, RotDamageAt y FireDamageAt en ese orden. |
| Daño local/global parece doble | RotDamageAt elige local antes que global y RenderWorld dibuja una sola capa/celda. FireDamageAt sí se suma como peligro distinto. |
| Peste activa parece limitada a unas fuentes | Comprobar globalRemaining y cada RotPresentationAt sobre state.cells; después globalFill.a, vetas y orden de RotGround. La cobertura lógica ya es global: no ampliar rotRadius ni sumar rellenos/daño para hacerla visible. |
| Idle tiene ocho imágenes pero no se ve animado | Seguir Game::Update→AnimationDirector::Update/Sample→fila/columna de Atlas; comprobar dos vueltas temporales y comparar píxeles reales con el marco esperado. No confundir poses independientes o hashes con reproducción. |
| Lluvia desaparece al desplazar cámara o redimensionar | RainDrops recibe world_.getSize(), no un rectángulo fijo del mapa; revisar presupuesto, módulo positivo y cobertura del viewport. El ajuste de cámara no debe alterar Weather lógico. |
| Caster impacta tarde o conserva partículas | Contrastar delay+contactSeconds con el aviso de Damage y el final delay+lifetime anterior; revisar stagedSpell, SampleSpell y borrado por AnimationDirector::Update, sin ejecutar otra vez UseAbility. |
| Captura se completa al seleccionar o cargar | BeginActivation no cambia propiedad; revisar CaptureRequired, captureProgress, capturingPlayer y orden Capture. En v1 la migración limita a total−1. |
| Se recupera maná o ingreso al alternar tropas | activationStarted impide repetir comienzo; StartTurn entrega Income una vez; EndActivation conserva bando. No duplicar esos cambios en UI/IA. |
| IA no avanza | StepAI requiere VersusAI/rojo/sin ganador; inspeccionar AvailableCount, mejor Decision y aceptación del comando simulado. Game espera Busy y aiDelay antes de llamarla. |
| Sonido ausente o duplicado | Revisar Ready, LoadedCueCount, CurrentMusic, CueStarts, volúmenes/muted/cooldown y reloj de AudioDirector. Silenciado puede seguir reproduciendo. Comparar IDs de combate y sufijo de log conservado. |
| Captura negra o frame anterior | El runner usa frame terminado+overlay en ScreenshotManager::SaveFrame; no leer backbuffer después de display de ventana oculta. F12 captura ventana final mediante Take. |
| Mando no responde | Ver foco, Device/ActiveGamepad, DeviceInfo.capabilities, perfil, HardwareButton, ejes y supresión; soltar botones/stick antes de reintentar. |
| Gatillo/dirección invertidos | Contrastar eje real y signo con InputConfig; activar separate_triggers/inversión o ajustar referencias; no asumir que Generic equivale a un driver específico. |
| Crash tras reclutar o restaurar estado | Unit y Cell viven en vectores; inserción/asignación puede invalidar punteros. Conservar ID/Hex y consultar Find/At otra vez. |
| Guardado rechazado | Leer error de Load/Validate y tabla4.4; conservar partida en memoria y archivo anterior. Texto JSON válido no garantiza referencias coherentes. |

Guardar un caso mínimo lógico permite usar los ejecutables existentes; el render completo se reproduce con --smoke o jugando. No existe una herramienta de inspección aislada por cada sistema ni se promete un editor de escenarios gráfico. Los archivos de build contienen logs concretos; LastTest.log siempre corresponde a la última ejecución y puede cambiar.

### 8.2. Límites comprobados

- La ventana del runner gráfico permanece oculta; render, contexto, texturas, eventos nativos y tamaños son reales. Resize continuo fue programado mediante ventana nativa, sin arrastre humano ni prueba humana de Alt+Tab. Los callbacks de foco sí se ejercitan.
- En los runners históricos C0/C1 el driver emitió `Failed to change display mode for fullscreen` durante transición exclusiva oculta. C2 usa en Windows un modo y una ventana sin bordes sobre el dispositivo elegido; su runner oculto también encontró un modo rechazado y conservó el actual. La prueba del selector fullscreen confirma el modo nativo, sin certificar conmutación exclusiva física. El usuario confirmó la corrección de restauración y fullscreen en su equipo antes de C2.
- No se utilizó mando físico. Faltan identificación/driver, numeración y orientación de ejes/gatillos, deriva, ergonomía y conexión física. Eventos simulados validan lógica y rutas UI, no esas propiedades de hardware.
- No se realizó partida manual humana completa, estudio competitivo de balance ni evaluación auditiva de mezcla/repetición. Campañas usan rival que pasa; prueban legalidad/progresión, no fuerza contra personas.
- Cámara usa zoom entero 1–3 y una orientación fija por escenario: 60° en Los tres pasos y 0° en los otros dos. No hay giro libre en los controles. El mundo conserva píxel físico entero y permite pan si no cabe; no se promete una distribución móvil arbitraria debajo del mínimo 960×600.
- Los cuerpos tienen frente/espalda y espejos, el caballo mantiene perfil lateral; son máscaras2D, no personajes volumétricos con seis direcciones completas. Las habilidades comparten Attack/Cast.
- Las series temporales finales de 7.8 usan deltas visuales controlados y lectura de píxeles del renderer. Su reproducción está comprobada; siguen pendientes una partida humana y una medición sostenida de FPS, sin generalizar el rendimiento a otros equipos.
- Maximize/Restore son nativos sólo en Windows. Compilación/lanzamiento/recursos generados están verificados en Windows MSVC Release; no se certifican Linux, macOS, MinGW ni Debug/F5.
- Clicker/Bounce preservan lienzo1440×900 estirado en ultrapanorámico; se comprobó API/ciclo/contenido/récords, sin adaptar sus layouts.
- Vectores y copias completas simplifican escenarios99/188 celdas; límites de archivo no prometen rendimiento constante para4096 celdas/512unidades. Una escala mayor requiere medir.
- La escritura con temporales protege fallos normales; Match::Save no garantiza recuperación ante todo corte eléctrico. Preferencias genéricas de SaveManager no heredan los límites 4 MiB/profundidad48 del parser táctico.
- No existen multijugador en red, nieve, equipamiento permanente, editor gráfico de botones ni planificador IA de varias rondas en esta versión.

## 9. Índice de capturas

La ejecución de normalización del 15 de octubre de 2026 generó **73 PNG**: 71 de Plaguefront y 2 de consumidores. Su [manifest histórico con dimensiones, fecha UTC y SHA256](../build/normalizacion-capturas.json) identifica aquella ejecución C0. Los enlaces de las tablas apuntan a las salidas de `build/screenshots`, que las regresiones posteriores pueden regenerar; no se atribuyen sus bytes vigentes a los hashes antiguos. Todas proceden del render real de la aplicación y ScreenshotManager. Los nombres históricos permiten localizar el fixture en Smoke.cpp, Phase4Smoke.cpp o Phase5Smoke.cpp; la revisión de C2 se registra en 9.5.

Las revisiones visuales documentadas el 14 de octubre inspeccionaron las 21 capturas de reglas/interfaz y luego las 22 adaptables+2 consumidores finales. Se corrigieron fichas que tapaban cabezas, conectores/textos, barras acumuladas, avisos ambientales fuera de margen, icono global ausente, mensaje heredado y retratos sin desplazamiento vertical. La normalización del 15 regeneró aquellos fixtures con tokens ejecutables idénticos; sus hashes históricos no representan los ajustes posteriores C1/C2 ni una nueva sesión humana de juego.

### 9.1. Regresión original:28 capturas

| PNG real | Tamaño actual | Estado del fixture |
| --- | --- | --- |
| [screen-0.png](../build/screenshots/screen-0.png) | 1440×900 | Menú principal |
| [screen-1.png](../build/screenshots/screen-1.png) | 1440×900 | Preparación de campaña |
| [screen-2.png](../build/screenshots/screen-2.png) | 1440×900 | Tablero inicial y recorridos de cámara/preferencias |
| [screen-3.png](../build/screenshots/screen-3.png) | 1440×900 | Órdenes de una unidad seleccionada |
| [screen-4.png](../build/screenshots/screen-4.png) | 1440×900 | Movimiento lógico y animación de ruta |
| [screen-5.png](../build/screenshots/screen-5.png) | 1440×900 | Ataque de Saber e impacto |
| [screen-6.png](../build/screenshots/screen-6.png) | 1440×900 | Fire real y consumo de MP |
| [screen-7.png](../build/screenshots/screen-7.png) | 1440×900 | Tres fuentes conectadas y peste global |
| [screen-8.png](../build/screenshots/screen-8.png) | 1440×900 | Casa con captura parcial |
| [screen-9.png](../build/screenshots/screen-9.png) | 1440×900 | Conquista legal y victoria |
| [screen-10.png](../build/screenshots/screen-10.png) | 1440×900 | Configuración y audio; en C2 selector de resolución abierto |
| [screen-11.png](../build/screenshots/screen-11.png) | 1440×900 | Traspaso de mando local |
| [screen-12.png](../build/screenshots/screen-12.png) | 1440×900 | Previsualización de ruta legal; contrato vigente: una sola punta terminal dorada con borde oscuro |
| [screen-13.png](../build/screenshots/screen-13.png) | 1440×900 | Panel de órdenes sin economía |
| [screen-14.png](../build/screenshots/screen-14.png) | 1440×900 | Reclutamiento y cancelación sin gasto |
| [screen-15.png](../build/screenshots/screen-15.png) | 1440×900 | Objetivo de Fire sin activar la tropa |
| [screen-16.png](../build/screenshots/screen-16.png) | 1440×900 | Los tres pasos: 188 celdas en conjunto; en C2 giro visual 60° |
| [screen-17.png](../build/screenshots/screen-17.png) | 1440×900 | Tienda y compra confirmada |
| [screen-18.png](../build/screenshots/screen-18.png) | 1440×900 | Construcción legal antes de gastar acción |
| [screen-19.png](../build/screenshots/screen-19.png) | 1440×900 | Inventario de unidad herida |
| [screen-20.png](../build/screenshots/screen-20.png) | 3440×1440 | Mapa grande en pantalla completa |
| [screen-21.png](../build/screenshots/screen-21.png) | 1920×1080 | Configuración1920×1080 |
| [screen-22.png](../build/screenshots/screen-22.png) | 1280×720 | Preparación1280×720 |
| [screen-23.png](../build/screenshots/screen-23.png) | 1440×900 | Selección restaurada1440×900 |
| [screen-24.png](../build/screenshots/screen-24.png) | 1440×900 | Habilidades de Caster |
| [screen-25.png](../build/screenshots/screen-25.png) | 1440×900 | Confirmación económica con precio |
| [screen-26.png](../build/screenshots/screen-26.png) | 1440×900 | Pausa conservando compra pendiente |
| [screen-27.png](../build/screenshots/screen-27.png) | 1440×900 | Derrota humana por conquista real |

### 9.2. Turnos, combate y captura:21 capturas

| PNG real | Tamaño actual | Estado del fixture |
| --- | --- | --- |
| [phase4-1.png](../build/screenshots/phase4-1.png) | 1440×900 | Turno azul con tropas disponibles |
| [phase4-2.png](../build/screenshots/phase4-2.png) | 1440×900 | Activación terminada y turno conservado |
| [phase4-3.png](../build/screenshots/phase4-3.png) | 1440×900 | Turno carmesí/traspaso |
| [phase4-4.png](../build/screenshots/phase4-4.png) | 1440×900 | Selección desde lista izquierda |
| [phase4-5.png](../build/screenshots/phase4-5.png) | 1440×900 | Ficha HP/estadísticas/MP |
| [phase4-6.png](../build/screenshots/phase4-6.png) | 1440×900 | Consulta en bosque y follaje |
| [phase4-7.png](../build/screenshots/phase4-7.png) | 1440×900 | Cadáver reciente tendido |
| [phase4-8.png](../build/screenshots/phase4-8.png) | 1440×900 | Cadáver durante espera |
| [phase4-9.png](../build/screenshots/phase4-9.png) | 1440×900 | Fuente tras cronología completa |
| [phase4-10.png](../build/screenshots/phase4-10.png) | 1440×900 | Máscara local real |
| [phase4-11.png](../build/screenshots/phase4-11.png) | 1440×900 | Grupo conectado y daño global |
| [phase4-12.png](../build/screenshots/phase4-12.png) | 1440×900 | Ataque y contraataque |
| [phase4-13.png](../build/screenshots/phase4-13.png) | 1440×900 | Lightning:tres impactos enlazados |
| [phase4-14.png](../build/screenshots/phase4-14.png) | 1440×900 | Fire y llamas persistentes |
| [phase4-15.png](../build/screenshots/phase4-15.png) | 1440×900 | Fortaleza parcial |
| [phase4-16.png](../build/screenshots/phase4-16.png) | 1440×900 | Captura pausada restaurada desde guardado |
| [phase4-17.png](../build/screenshots/phase4-17.png) | 1440×900 | Captura completada/victoria |
| [phase4-18.png](../build/screenshots/phase4-18.png) | 1440×900 | Economía e ingreso del propietario |
| [phase4-19.png](../build/screenshots/phase4-19.png) | 1440×900 | Confirmación de fin con tropas pendientes |
| [phase4-20.png](../build/screenshots/phase4-20.png) | 1440×900 | Catorce variantes clase/bando |
| [phase4-21.png](../build/screenshots/phase4-21.png) | 3440×1440 | Listado/ficha en fullscreen |

Los cuerpos de 7/8conservan clase/equipo y pose tendida posterior al clip; 9/10usan fuente distinta.16 usa Save/Load; 17 exige Capture hasta total sin asignar winner. Las fichas se componen sobre mundo, conectores asocian entidad; vista general compacta barras si falta espacio. El cursor normal se contrastó históricamente con píxeles opacos de su atlas en 1/3/5; los seis contextos se ejercitan por decisiones legales.

### 9.3. Layout, mando y ambiente:22 capturas

| PNG real | Tamaño actual | Estado del fixture |
| --- | --- | --- |
| [phase5-1.png](../build/screenshots/phase5-1.png) | 1280×720 | Layout1280×720 |
| [phase5-2.png](../build/screenshots/phase5-2.png) | 1366×768 | Layout1366×768 |
| [phase5-3.png](../build/screenshots/phase5-3.png) | 1440×900 | Layout1440×900 |
| [phase5-4.png](../build/screenshots/phase5-4.png) | 1920×1080 | Layout1920×1080 |
| [phase5-5.png](../build/screenshots/phase5-5.png) | 3440×1440 | Fullscreen del escritorio |
| [phase5-6.png](../build/screenshots/phase5-6.png) | 1100×1100 | Ventana alta1100×1100 |
| [phase5-7.png](../build/screenshots/phase5-7.png) | 1440×900 | Restauración tras maximizar |
| [phase5-8.png](../build/screenshots/phase5-8.png) | 1440×900 | Menú con foco de mando |
| [phase5-9.png](../build/screenshots/phase5-9.png) | 1440×900 | Órdenes con foco de mando |
| [phase5-10.png](../build/screenshots/phase5-10.png) | 1440×900 | Cursor hexagonal de mando |
| [phase5-11.png](../build/screenshots/phase5-11.png) | 1440×900 | Controles/mapeo/ajustes |
| [phase5-12.png](../build/screenshots/phase5-12.png) | 1440×900 | Cadáver inerte, sin máscara local |
| [phase5-13.png](../build/screenshots/phase5-13.png) | 1440×900 | Fuente activa:centro/pulso/esporas |
| [phase5-14.png](../build/screenshots/phase5-14.png) | 1440×900 | Radio local:borde/trama |
| [phase5-15.png](../build/screenshots/phase5-15.png) | 1440×900 | Dos fuentes:solapamiento sin intensificar |
| [phase5-16.png](../build/screenshots/phase5-16.png) | 1440×900 | Grupo conectado:fuentes y HUD global |
| [phase5-17.png](../build/screenshots/phase5-17.png) | 1440×900 | Peste global fuera de radios locales |
| [phase5-18.png](../build/screenshots/phase5-18.png) | 1440×900 | Unidad afectada y exclusión global |
| [phase5-19.png](../build/screenshots/phase5-19.png) | 1100×1100 | Configuración en ventana alta |
| [phase5-20.png](../build/screenshots/phase5-20.png) | 1366×768 | Inventario tras resize1366×768 |
| [phase5-21.png](../build/screenshots/phase5-21.png) | 1440×900 | Zoom×2 y picking coherente |
| [phase5-22.png](../build/screenshots/phase5-22.png) | 1100×1100 | Preparación y retratos en ventana alta |

Los siete estados 12–18 distinguen cadáver inerte, centro activo, máscara local, solapamiento, componente conectado, peste global y unidad afectada. La ficha muestra PUTREF.LOCAL−2 HP y «Global no se suma» en líneas separadas; fuego tendría cifra/línea propia. Se conserva una sola intensidad en solapamientos. Una imagen fija no demuestra pulso temporal; la ejecución y configuración de animación aportan esa comprobación. Los fixtures comparan máscara/daño/validación con 1470 checks.

### 9.4. Consumidores preservados:2 capturas

| PNG real | Estado |
| --- | --- |
| [phase5-clicker.png](../build/screenshots/phase5-clicker.png) | Start/Update/Render reales, blanco/HUD y récords conservados; lienzo heredado estirado |
| [phase5-bounce.png](../build/screenshots/phase5-bounce.png) | HUD, paleta, pelota en espera inicial, vidas/récords conservados |

Ventana oculta, fullscreen no certificado como cambio exclusivo físico, resize programado, mando simulado y audio silenciado mantienen los límites de sección 8. Los fixtures pueden recolocar unidades para aislar un caso; las órdenes/daño/turnos/conquista se ejecutan por Match. Capturas no sustituyen campaña humana completa ni estudio competitivo.

### 9.5. Revisión de presentación C2

| PNG regenerado | Resultado de la inspección visual |
|---|---|
| [screen-10.png](../build/screenshots/screen-10.png) | Selector abierto sobre Configuración, resolución actual visible y opciones de elección directa legibles. |
| [screen-12.png](../build/screenshots/screen-12.png) | Ruta completa de coste 3 MOV con una sola cabeza terminal, el mismo dorado y borde oscuro del cuerpo y unión continua sin franja negra. |
| [screen-16.png](../build/screenshots/screen-16.png) | Los tres pasos en conjunto con la nueva orientación; conserva sus 188 celdas. |

El hover de árboles se comprueba comparando píxeles de estados consecutivos en `verifyTreeHover`; un PNG fijo no prueba por sí solo que el árbol cambie al entrar y salir el cursor. Los resultados CTest de esta entrega están en 7.7.

### 9.6. Series temporales de las correcciones visuales

**Series finales generadas y revisadas en la ejecución de 7.8.** Son muestras de `world_` nativo capturadas mediante ScreenshotManager, además de las 71 pantallas históricas de la aplicación; el nombre de resolución identifica la salida solicitada, no obliga a que el PNG nativo tenga esas dimensiones. Los montajes conservan sus píxeles y no interpolan fotogramas.

| Serie bajo build/visual-corrections/runtime/ | Contenido verificado y lectura |
|---|---|
| `idle-CLASE-BANDO-DIRECCION.png` | Tira de ocho marcos dibujados, para 7×2×4 combinaciones; verification.json registra dieciséis hashes temporales y la vuelta del ciclo. |
| `rain-1440x900-z1-w1-t0.png`…`t2.png` y otras salidas | Tres instantes separados .08s; diferencias con Clear, cobertura y cantidad se registran para Rain/Storm y zoom 1/3. |
| `plague-ESCENARIO-local/global/pan/residual/final.png` | Radios antes de activación, cobertura global, cámara lejos del foco, reserva sin fuentes y agotamiento; escenario3 corresponde al fixture irregular. |
| `spell-8-preparation/launch/travel/impact/fade.png` | Cinco momentos de Fire; las otras seis acciones usan su índice de Ability y la misma nomenclatura de fases. |
| `verification.json` | Resultado del proceso, comprobaciones, hashes, tiempos, cobertura de lluvia/celdas y preservación lógica; debe comprobarse su fecha y pass. |
| [idle-runtime-2x.gif](../build/visual-corrections/runtime/idle-runtime-2x.gif) | Montaje animado de las tiras dibujadas por el motor; 56 combinaciones de clase, bando y orientación. |
| [caster-stages-runtime.png](../build/visual-corrections/runtime/caster-stages-runtime.png) | Las 35 capturas reales de siete acciones y cinco etapas, reunidas sin generar imágenes intermedias. |
| [resource-manifest.json](../build/visual-corrections/runtime/resource-manifest.json) | Registra SHA256 de units.png, effects.png, ejecutable y verification.json, y declara la procedencia de los montajes. |

Las barras de nombre anteriores representan archivos individuales. El GIF y el montaje indicados sí reúnen esos fotogramas reales; un muestrario de fuentes no se presenta como una captura del juego. El registro final verifica 896 muestras Idle, 48 de lluvia, 35 mágicas y 436 celdas de cobertura global; las limitaciones de sesión humana y FPS permanecen en 8.2.

### 9.7. Evidencia de la revisión de integración de unidades

**Series finales de la corrida aprobada de 7.9.** Los archivos bajo `build/unit-integration/` distinguen fuentes históricas, comparaciones intermedias, atlas final y capturas del renderer. El recurso final es `a2aba86fe852…`, cargado desde producción sin sustitución temporal. Los montajes de runtime emplean fotogramas capturados, sin inventar poses intermedias.

| Evidencia | Uso y estado |
|---|---|
| [approved-idle-restore.json](../build/unit-integration/units/approved-idle-restore.json) | Copia localizada exacta de 448 Idle desde la fuente aprobada fa1b; documenta esa operación anterior a la reparación del faldellín. |
| [berserker-waist-scope.json](../build/unit-integration/units/berserker-waist-scope.json) | Atlas final a2aba86f, 64 Walk de Berserker reparados, 448 Idle conservados y otras 2176 celdas intactas; sin repintar píxeles fuente. |
| [identity-comparison.json](../build/unit-integration/candidate/identity-comparison.json), `candidate/idle-three-way-1x/2x/3x.png` y GIF | Comparaciones de identidad entre atlas anterior, etapa 7.8 y recuperación de Idle; son evidencia intermedia de atlas, no captura del juego ni manifiesto del resultado CTest final. |
| `build/unit-integration/units/berserker-waist-frame6-before-after-6x.png` y `berserker-waist-cycle-before-after-6x.png` | Comparación localizada del faldellín antes/después; permite revisar el patrón recuperado en el rig. |
| `build/unit-integration/discarded-generated-walk/` y `prompts.json` | Nuevas propuestas descartadas; no integradas ni usadas como evidencia de runtime. |
| [verification.json](../build/unit-integration/runtime/verification.json) | `pass=true`, 4338084 comprobaciones, 56 marchas, 112 ciclos Idle y 1400 muestras; identifica el recurso de producción y descarta sustitución temporal. |
| [locomotion-anchors.json](../Tools/Art/Sources/Units/Approved/locomotion-anchors.json) | Las 896 anclas/máscaras originales obligatorias para contrastar Idle y Walk de todas las clases, bandos y orientaciones sobre atlas y render. |
| `build/unit-integration/runtime/initial-idle-CLASE-BANDO-DIRECCION.png` y `motion-CLASE-BANDO-DIRECCION.png` | Reposo inicial y tira de ocho marcos Walk más dos ciclos Idle al llegar, por cada una de las 56 combinaciones. |
| [saber-idle-walk-arrival-3x.gif](../build/unit-integration/runtime/saber-idle-walk-arrival-3x.gif) | Secuencia real de Saber a 3×: reposo inicial mantenido 700 ms, ocho marcos Walk y llegada con dos vueltas Idle; sin repetición automática. |
| [walk-arrival-idle-runtime-2x.gif](../build/unit-integration/runtime/walk-arrival-idle-runtime-2x.gif) | Montaje real por clases/orientaciones a 2× con reposo inicial, marcha y llegada; no reinicia la posición al terminar. |
| [idle-runtime-2x.gif](../build/unit-integration/runtime/idle-runtime-2x.gif) | Dos ciclos de reposo capturados por el renderer con el diseño recuperado. |
| `battle-walk-0.png`…`battle-walk-7.png`, `battle-arrival.png` y [battle-walk-arrival-runtime-2x.gif](../build/unit-integration/runtime/battle-walk-arrival-runtime-2x.gif) | Marcha y llegada de una orden legal en el escenario cargado; GIF real sin repetición automática. |
| [shadows-before-after-1x.png](../build/unit-integration/runtime/shadows-before-after-1x.png) / [2x](../build/unit-integration/runtime/shadows-before-after-2x.png) | Comparaciones de sombras mediante recortes de capturas auténticas anteriores/actuales, conservando la escala entera. |
| [resource-manifest.json](../build/unit-integration/runtime/resource-manifest.json) | Vincula el atlas final, ejecutable, informes y series temporales de esta entrega. |
| [galería anterior](../build/art-final/runtime/phase4-20.png) / [galería actual](../build/screenshots/phase4-20.png) | Mismo fixture de catorce tropas y encuadre `FocusCamera(true)`, salida 1440×900; la anterior conserva las sombras antiguas. Permite recortes comparables a escala entera sin fabricar un render antiguo. |
| [cadáver anterior](../build/art-final/runtime/phase4-7.png) / [cadáver actual](../build/screenshots/phase4-7.png) | Pareja del fixture de cuerpo caído para revisar también la sustitución de la mancha uniforme del cadáver. |

El baseline de esta etapa conserva hashes y copias bajo `build/unit-integration/baseline/`. Las galerías anteriores permanecen bajo `build/art-final/runtime/`; no se reemplazan por dibujos de comparación. Las carpetas de runtime que se regeneran pueden contener bytes posteriores a sus manifiestos históricos: revisar fecha y hash antes de atribuirles los resultados de 7.8. Ninguna de estas series equivale a una partida humana ni a una medición sostenida de FPS.

### 9.8. Evidencia de velocidad y piloto Idle rechazado

La corrida de velocidad aprobada y la evaluación del piloto tienen alcances distintos. Las siguientes hojas y animaciones no son capturas de un Idle nuevo integrado; documentan el intento que se rechazó en 7.10.

| Evidencia bajo build/idle-agility/ | Lectura y límite |
|---|---|
| [build-final.log](../build/idle-agility/build-final.log), [tests-final.log](../build/idle-agility/tests-final.log) y [tests-final-details.log](../build/idle-agility/tests-final-details.log) | Compilación y 9/9 suites, 37.83 s; Presentation 33.72 s. Validan la cadencia .035/.28 y el recurso de producción conservado. |
| [saber-idle-first-attempt.png](../build/idle-agility/saber-idle-first-attempt.png) / [second-attempt](../build/idle-agility/saber-idle-second-attempt.png) | Dos hojas de 32 poses rechazadas; diferencias de archivo no bastan para demostrar poses corporales coherentes. |
| [saber-idle-pilot-4x.png](../build/idle-agility/pilot/saber-idle-pilot-4x.png) | Hoja del piloto de ocho imágenes completas: semilla original más siete ediciones independientes. Rechazada por apoyo, capa y variación de armadura. |
| [saber-idle-pilot-4x.gif](../build/idle-agility/pilot/saber-idle-pilot-4x.gif) / [1x](../build/idle-agility/pilot/saber-idle-pilot-1x.gif) | Previsualización del piloto a .18 s por marco para revisar el ciclo y la escala real; no demuestra reproducción dentro del juego ni aprobación. |
| [pilot-manifest.json](../build/idle-agility/pilot/pilot-manifest.json) | Procedencia y hashes de las ocho poses, dimensiones 48×64, ancla (24,56) y medidas; el piloto no modifica producción y no supera la evaluación visual. |
| `baseline/project-hashes.json` | Referencia de integridad anterior a la investigación; los 2688 marcos del atlas a2aba siguen intactos. |

Sigue pendiente una animación piloto válida, su aprobación antes de integrar y la comprobación temporal posterior dentro del juego. La vista previa rechazada y los éxitos de CTest del recurso conservado no sustituyen esos pasos.

## 10. Evolución y consolidación

### 10.1. Historia técnica útil

Los registros originales de desarrollo se redactaron el 14 de octubre de 2026. Sus resultados corresponden a esas compilaciones; no sustituyen la ejecución nueva del 15 en 7.3. Se preservan snapshots y logs bajo build, sin mantener documentos de fase activos duplicados.

| Estado histórico | Cambio o evidencia conservada | Resultado de su ejecución final |
| --- | --- | --- |
| Núcleo inicial | AwesomeEngine integrado, reglas/campañas/UI, arte inicial y guardado; entonces alternaba unidades y captura diferida | 3/3suites,2.70s; [registro](../build/final-verification.log),8 capturas iniciales |
| Presentación y audio iniciales | Cámara(30q,9q+18r−8h), lienzo720×450,672marcos(4poses/2direcciones),7 PNG y42 WAV originales; núcleo protegido | 5/5,3.40s;18 grupos/529 checks reglas,391validaciones,10 grupos/4712proyección,421audio,12 capturas; [registro](../build/phase2-final-verification.log) |
| Refinamiento y mapa grande | Base(30,14)/(0,28)/altura6, entonces sin giro;188 celdas,2688marcos(8poses/4direcciones), texto a salida, contextos, cámara/ventana | 6/6,6.33s;529reglas,391validaciones,351UI,4712proyección,421audio,5 grupos/7772nuevos,28 capturas; [registro](../build/phase3-final-verification.log) |
| Turnos de ejército y combate | Cambio autorizado de reglas:EndActivation/EndTurn, respuesta, cadenas, fuego, captura porHP, guardado2/migración1, rojo/cuerpos/cursor | 7/7,10.46s;550reglas,402validaciones,4755proyección,7790fase3,436audio,733reglas nuevas, UI347+143+167,49 capturas; [registro](../build/phase4-final-verification.log) |
| Integración adaptable y mando | Políticas compartidas, fuentes/captura/persistencia en managers, layout en dos ejes, contaminación visible, mando existente, sin cambiar reglas | 9/9,18.55s;6553proyección,3884layout,1470rot,114mandoUI,339managers,178input,15compatibilidad;73 PNG; [registro](../build/phase5-final-verification.log) |
| Normalización actual | Comentarios españoles de una línea, contratos cotejados, curso29lecciones y tres documentos principales | 9/9 en 17,93 s tras consolidar; tokens ejecutables idénticos y enlaces revisados |

Historias de integridad no equivalen a invariantes perpetuos: fase 2 comparó 9 hashes de núcleo y fase 3 comparó 10 más 47 de audio; fase 4 preservó 47 (WAV/escenarios/AudioManager) mientras modificaba reglas; fase 5 preservó 94 (recursos y fuentes protegidas) mientras reorganizaba headers. La normalización conserva 87 recursos Content y verifica tokens ejecutables de código comentado. Snapshots: build/phase2-baseline, phase3-baseline, phase4-baseline, phase5-baseline y normalizacion-baseline.

El arte de fase 2 tenía units 1152×1792/672 marcos, 812 marcos visibles y 168 secuencias de 4 poses; fase 3 amplió a 2688 poses, 2828 marcos visibles/336 secuencias; fase 4 añadió retratos/cursores y validó 2848 marcos visibles, 1344 parejas de facción y cero recortes. Esas dimensiones antiguas no se usan como contrato runtime vigente.

### 10.2. Campañas y medidas históricas

Las campañas frente a rival pasivo son reproducibles mediante suites existentes. Ronda/activaciones históricas iniciales de Frontera/Ciénaga: fácil 7/36 y 8/42, normal 8/53 y 9/61, difícil 7/50 y 10/80. Tras añadir Los tres pasos, sus tres campañas dieron 7/36, 8/53, 8/61. El cambio posterior a turnos por ejército ajustó los recorridos; estado actual: 

| Dificultad | Frontera ronda/activaciones | Ciénaga ronda/activaciones | Los tres pasos ronda/activaciones |
| --- | --- | --- | --- |
| Easy | 9/48 | 9/48 | 8/42 |
| Normal | 7/45 | 9/60 | 8/53 |
| Hard | 7/50 | 10/78 | 8/60 |

El cierre económico funciona incluso sin tropas y la purificación sobre cadáver superpuesto conserva éste; son las dos regresiones de CampaignTests. Las seis campañas de mapas iniciales más conquista local suman 7 campañas y 2 regresiones/402 validaciones; las tres highlands pertenecen a Phase3Tests. Ningún resultado estima fuerza contra un rival humano activo.

Medidas CPU históricas, 8 muestras, sin presentación/vsync: fase 2 canvas 486×320, media 2.452 ms (2.397–2.574); fase 3 canvas 530×350, media 1.594 ms (1.321–2.667); fase 4 canvas 416×350, media 1.422 ms (1.342–1.799). Contexto/muestras difieren; no se convierten en una comparación de FPS. El runner vigente tiene su medición en 7.3.

Problemas históricos corregidos que siguen justificando código: C1902 llevó a Release para try_compile; prefijo español de showIncludes impidió rebuilds correctos hasta corregirlo; localtime generó C4996 y ScreenshotManager usa localtime_s/localtime_r; capturas leían frame anterior hasta componer textura terminada; activación confundida con terminada, retornos de modales y reinicio con borrador ajeno recibieron regresiones; logs acotados conservan sufijo para no repetir ataques; arrastre acumula fracciones antes de enviar pan entero; hooks de foco restauran cursor del sistema.

### 10.3. Transferencia de documentación anterior

Antes de consolidar se leyeron README, LICENSE y los 15 documentos de docs. La tabla indica dónde queda su información útil; se sustituyen duplicaciones y afirmaciones de versiones antiguas por contratos actuales. Los archivos originales se preservan en build/normalizacion-baseline/docs como respaldo histórico. LICENSE no se elimina ni se traduce su aviso legal. README permanece como entrada del proyecto.

| Documento anterior | Contenido transferido | Destino vigente |
| --- | --- | --- |
| ARCHITECTURE.md | Propiedad, capas, flujo y separación de servicios/coordenadas | Curso2–4,21–25; referencia1–2 |
| DEVELOPMENT_GUIDE.md | Recorridos reales, decisiones, extensión y depuración | Curso1–29; referencia1,2,8 |
| ENGINE_MANAGERS.md | Auditoría13managers, consumidores, duplicaciones/API/compatibilidad | Referencia2; curso2,3,23–25,28 |
| GAMEPLAY.md | Movimiento, clases, combate, activaciones, captura, economía, items/clima | Curso5–20; referencia3 |
| HEX_GRID.md | Axiales, distancia, Dijkstra, proyección, picking/profundidad | Curso6–9,11,21; referencia2.3,3.1,3.7 |
| ROT_SYSTEM.md | Cronología, grafo, daño único, reserva, contramedidas y diagnóstico | Curso16–17; referencia3.5,8,9 |
| AI.md | Simulación, heurística, dificultades, campañas y límites | Curso20; referencia3.8,7,10.2 |
| DATA_FORMATS.md | Balance, escenarios, guardado2/migración1, validación y reemplazo | Curso5,25,27; referencia4 |
| CONTROLS.md | Teclado/ratón/mando, mapeo, capacidades/preferencias y límites físicos | Curso12,24; referencia5,8 |
| ART_ASSETS.md | Atlas/índices/anclas, paletas, cuerpos, cursores, generación/licencias/verificación | Curso21–22,27; referencia6,9,10.1 |
| AUDIO.md | Manifiesto42 WAV, buses, voces, volúmenes, observación, generación y escucha pendiente | Curso23; referencia6.5,7,8 |
| TESTING.md | Comandos, coberturas, regresiones, fixture y límites | Curso26; referencia7–9 |
| PROGRESS.md | Evolución, resultados, hashes, incidencias, rendimiento y pendientes | Referencia7.3–7.4,8,10 |
| PHASE4_VISUAL_REVIEW.md | Índice21 capturas, lectura/correcciones, asociación cursor/cuerpos/indicadores | Referencia9.2,8 y curso21–22 |
| PHASE5_VISUAL_REVIEW.md | Índice22+2 capturas,7estadosambientales, layout/mando y límites | Referencia9.3–9.4,7,8 |

El [mapa de transferencia](../build/normalizacion-consolidacion.json) relaciona cada original respaldado, sus secciones y los destinos revisados antes de retirarlo.

### 10.4. Comentarios normalizados y alcance conservado

[Lista completa de 53 archivos modificados](../build/normalizacion-archivos.json): 18 del motor (InputConfig/InputTypes, Cursor/Input/Render/Save/Screenshot/Window managers, Scene, InputBindings, Engine/EngineConfig/EngineVisitor, AudioManager, GuiManager y SceneFactory); familias Game/Rules/Presentation/Audio y sus fixtures; pruebas; CMake y scripts de generación pertinentes. La comparación separada enlazada en 7.4 explica la igualdad de tokens/cadenas y ausencia de comentarios modificados multilínea.

Los comentarios introducidos para Plaguefront se traducen sin alterar identificadores, cadenas de interfaz, claves JSON, comandos, reglas ni datos. Se preservan comentarios ajenos de demos/efectos y avisos legales de terceros. Este trabajo no crea commits, no ejecuta operaciones remotas, no publica y no elimina código funcional. El curso ofrece un itinerario de compilación→núcleo→presentación→interacción→persistencia/pruebas→ampliación y réplica con referencias precisas, sin proyectos ficticios adicionales.
