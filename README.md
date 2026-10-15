# Plaguefront — La frontera gris

Plaguefront es un juego táctico por turnos de ejército para dos jugadores locales o un jugador contra la IA. Dos facciones disputan un tablero hexagonal con alturas, caminos, bosques, casas y fortalezas. El objetivo es completar la captura de la fortaleza rival; ocuparla por sí solo no concede la victoria.

Su identidad combina un campo de batalla medieval de pixel art, cámara axonométrica y un territorio que conserva las consecuencias del combate: los cuerpos esperan varias rondas antes de convertirse en fuentes de putrefacción. El proyecto utiliza **AwesomeEngine**, el motor C++ de este repositorio. El código, los recursos y la documentación permiten estudiarlo y reproducirlo localmente.

![Arte del menú](Content/Textures/Plaguefront/Pixel/menu.png)

## Estado y características

La versión actual incluye:

- Tres escenarios: **La frontera gris**, **Las ciénagas** y **Los tres pasos**, con 99, 99 y 188 celdas. La preparación permite elegir cinco tropas iniciales por ejército.
- Siete clases: Saber, Archer, Lancer, Rider, Caster, Assassin y Berserker. Tienen estadísticas, alcances, habilidades y costes diferentes.
- Movimiento axial con rutas ponderadas por terreno, altura, ocupación y clima; previsualización del camino y su coste.
- Turnos completos por ejército, con selección de varias tropas y activaciones que pueden intercalarse sin recuperar movimiento, acción o maná adicionales.
- Combate con defensa física y resistencia mágica, ventajas, habilidades, contraataques legales, Lightning encadenado y fuego persistente.
- Capturas por puntos aportados por los HP actuales del captor; el progreso permanece en el edificio aunque la unidad se retire o muera.
- Casas, fuertes, ingresos por propietario, tienda, inventario, compras confirmadas, reclutamiento individual y batallones.
- Cadáveres, fuentes con radio hexagonal, peste global por conectividad, purificación e incineración. El daño local y global de putrefacción no se suma; el fuego se resuelve aparte.
- Clima determinista, tres dificultades de IA y partidas locales con cambio de mando.
- Mundo nítido con escala física entera, fuentes a resolución de salida, interfaz adaptable, zoom, desplazamiento y vista de conjunto.
- Animación de las siete clases y ambos bandos, indicadores de HP, economía, captura y contaminación, música, ambiente y efectos de audio.
- Teclado, ratón y mando mediante los servicios existentes del motor; preferencias y guardados JSON locales.

La compilación y las nueve suites de regresión funcionan en el entorno Windows/MSVC descrito abajo. El mando se comprueba mediante eventos SFML simulados; su validación física y una partida manual completa siguen pendientes. No hay multijugador por Internet, servidores ni publicación automática.

## Tecnologías y arquitectura

| Tecnología | Versión declarada | Función |
|---|---|---|
| C++ | 20 | Motor, reglas y presentación |
| SFML | 3.1.0 | Ventana, eventos, gráficos y audio |
| CMake | 3.28 como mínimo | Configuración y construcción |
| TGUI | 1.13.0 | Interfaz existente del motor |
| nlohmann JSON | 3.12.0 | Datos, preferencias y partidas |
| spdlog | 1.16.0 | Registro |
| magic_enum | 0.9.8 | Utilidades del motor |

AwesomeEngine conserva la ventana, el ciclo de escenas y los managers compartidos. **Match** es la autoridad del estado y las reglas; la IA ejecuta sus mismas órdenes. **Game** adapta entradas y pantallas a esas órdenes. La cámara, la animación y el audio representan resultados ya resueltos y no aplican daño por su cuenta.

La convención de módulos sigue el ejemplo real de Clicker: declaración en el header principal, comportamiento en el archivo de implementación, datos compartidos en **Types** y parámetros en **Config**. No existe otro gestor de gamepad, ventana o audio exclusivo de Plaguefront que duplique el motor.

## Requisitos e instalación local

El entorno comprobado es **Windows x64, MSVC, CMake y Ninja**, con SFML/OpenGL disponible para abrir ventanas. Las pruebas de reglas y proyección no requieren ventana; las de audio y presentación necesitan sus respectivos servicios.

Las dependencias deben estar presentes en **build/_deps**. CMake fuerza FETCHCONTENT_FULLY_DISCONNECTED y FETCHCONTENT_UPDATES_DISCONNECTED: sus declaraciones de origen no autorizan ni realizan descargas. Para trasladar el proyecto, suministra las fuentes de las dependencias por medios locales o indica sus directorios con FETCHCONTENT_SOURCE_DIR_SFML, FETCHCONTENT_SOURCE_DIR_JSON, FETCHCONTENT_SOURCE_DIR_LOGS, FETCHCONTENT_SOURCE_DIR_TGUI y FETCHCONTENT_SOURCE_DIR_MAGIC_ENUM.

Los PNG, WAV, escenarios y fuentes necesarios ya están incluidos en Content. No es necesario ejecutar generadores para jugar. Los generadores opcionales utilizan PowerShell y C# local; las imágenes emplean System.Drawing de Windows.

## Compilación

Ejecuta desde la raíz en una terminal de desarrollo **x64 de MSVC**:

```powershell
Set-Location 'D:\Proyectos\SFML\Plagefront'
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_TRY_COMPILE_CONFIGURATION=Release -DBUILD_TESTING=ON
cmake --build build --parallel 4
```

El objetivo gráfico se llama **AwesomeEngine** y produce **build/Plaguefront.exe**. PlaguefrontLogic y PlaguefrontPresentationLogic son las bibliotecas de reglas y presentación comprobables sin ventana.

En este equipo también funciona:

```powershell
./Tools/BuildLocal.ps1 -Test
```

BuildLocal.ps1 prepara la instalación local de MSVC y construye en Release. Su ruta a VsDevCmd.bat es específica de este equipo; en otro equipo utiliza una terminal de desarrollo o adapta esa ruta tras inspeccionarla.

No mezcles la caché MSVC existente con MinGW. El entorno presentó C1902 en la comprobación Debug de MSVC; Release es la configuración verificada. Las tareas Debug y opciones de clangd heredadas de .vscode no constituyen un procedimiento Debug validado.

## Ejecución y primera partida

Abre [Jugar.cmd](Jugar.cmd) o ejecuta:

```powershell
Set-Location 'D:\Proyectos\SFML\Plagefront'
& .\build\Plaguefront.exe
```

En Windows, EngineConfig busca Content junto al ejecutable o en sus carpetas superiores. Para distribuirlo localmente, conserva esa relación de carpetas y las dependencias de ejecución.

1. Entra en la preparación desde **Jugar** o **Nueva partida**.
2. Elige modo, dificultad si juegas contra la IA y escenario. Pulsa las clases para modificar cada ejército.
3. Comienza la partida y selecciona una tropa propia; abrir sus órdenes no consume la activación.
4. Abre Mover, apunta a un destino iluminado, consulta la ruta y confirma. Usa Atacar, Habilidades o Inventario para las demás órdenes.
5. Termina la activación de cada tropa y después el turno del ejército. Si quedan tropas disponibles, el juego pide confirmar.
6. Administra un edificio propio para reclutar o comprar; las operaciones económicas se revisan antes de cobrar.
7. Para ganar, ocupa la fortaleza rival y ejecuta Capturar hasta completar sus puntos. El rival conserva oportunidades de respuesta entre turnos.

La paleta morada de putrefacción y peste se configura en `Content/Config.json → rotPalette` con nueve RGBA. Los valores ausentes o inválidos conservan sus respaldos. Cambia el JSON y reinicia el ejecutable para ver otros colores sin recompilar; el balance táctico permanece en su archivo separado.

Una fuente activa se distingue del cuerpo inerte por su centro, pulso, esporas y radio con borde y trama. El HUD anuncia la peste global mientras su contador siga activo. La ficha de una tropa afectada muestra el daño previsto y si el global queda excluido.

## Controles

| Dispositivo | Acciones principales |
|---|---|
| Ratón | Clic izquierdo: seleccionar o confirmar; derecho: cancelar; rueda: zoom sobre tablero o desplazar lista; arrastre central: cámara. |
| Teclado táctico | M mover, T atacar, H habilidades, I inventario; E/Espacio termina una tropa y F el ejército. |
| Cámara y menús | WASD desplaza; Inicio centra; V encuadra mapa; Tab/Arriba/Abajo recorre botones; Enter confirma y Escape vuelve o pausa. |
| Ventana y archivos | F11/Alt+Enter cambia pantalla completa; F5/F9 guarda/carga en partida; F12 captura. |
| Mando | Stick/cruceta navega; Inferior confirma; Derecho cancela; Izquierdo alterna tablero/órdenes; Superior informa; L1/R1 cambia tropas; R3 centra; L2/R2 cambia zoom; L3 termina tropa; Select termina ejército; Start pausa. |

Los botones del mando son **lógicos**: el perfil y las capacidades reales determinan sus índices físicos. Configuración → Controles permite consultar el mapeo y ajustar parámetros. Las asignaciones avanzadas se editan en la configuración común; no hay editor visual individual de botones. El mando controla al jugador activo, sin asignar automáticamente un mando a cada bando. Cambiar a teclado/ratón o desconectar el mando conserva la selección y la partida.

La [referencia técnica, entrada y preferencias](docs/REFERENCIA_TECNICA_PLAGUEFRONT.md#5-entrada-y-preferencias) contiene el mapeo completo, perfiles, ejes, gatillos, zona muerta y persistencia.

## Guardados y configuración

| Ruta | Responsabilidad |
|---|---|
| Content/Config.json | Configuración inicial de AwesomeEngine |
| Content/Plaguefront/balance.json | Parámetros de nuevas partidas |
| Content/Plaguefront/scenarios | Mapas y ejércitos iniciales |
| Saves/plaguefront.json | Partida guardada |
| Saves/settings.json | Opciones de presentación, sonido y ventana |
| Content/Save.json | Almacén compartido del motor; Engine:Input guarda preferencias de mando y conserva los récords de las demostraciones |

El guardado incluye mapa y configuración de balance propios. Editar balance.json afecta a partidas nuevas, sin transformar silenciosamente las ya guardadas. La carga valida el formato y migra la versión anterior admitida; no sobrescribe el original al cargar.

## Estructura del repositorio

```text
Engine/                       AwesomeEngine y sus servicios compartidos
Games/Include/Plaguefront/     API de Game, Match, cámara, animación y audio
Games/Include/Types/           Tipos compartidos; incluye los de Clicker
Games/Include/Config/          Parámetros de los módulos
Games/Source/Plaguefront/      Reglas, persistencia, IA, interfaz y presentación
Content/Plaguefront/           Balance, escenarios y manifiestos
Content/Textures/Plaguefront/  Arte preservado y atlas actuales en Pixel/
Content/Audio/Plaguefront/     Música, ambiente y efectos
Content/Fonts/                Fuentes y sus licencias
Tests/                        Suites lógicas, gráficas, sonoras y del motor
Tools/                        Construcción y generadores locales
docs/                         Curso y referencia técnica
build/                        Ejecutables, dependencias y evidencias locales
Saves/                        Datos creados durante el uso
```

## Pruebas

```powershell
ctest --test-dir build --output-on-failure
ctest --test-dir build -LE "graphics|audio" --output-on-failure
ctest --test-dir build -R '^PlaguefrontRules$' --output-on-failure
& .\build\Plaguefront.exe --smoke
```

La verificación final del 15 de octubre de 2026 aprobó **9/9 suites en 17,93 segundos** después de consolidar la documentación. Las nueve suites comprueban reglas, campañas, proyección/animación, los recorridos históricos conservados, audio, presentación real, managers e input. Los nombres PlaguefrontPhase3 y PlaguefrontPhase4 siguen identificando suites de regresión, aunque la documentación ya esté consolidada.

La comprobación nueva del curso reconstruyó el proyecto desde AwesomeEngine original y aprobó **9/9 suites en 18,73 segundos** en una copia independiente. Los checkpoints previos del núcleo aprobaron 4/4; los resultados se separan de la normalización anterior en la referencia.

El recorrido gráfico produce 71 capturas de Plaguefront y dos de Clicker/Bounce mediante ScreenshotManager. Usa ventanas y renderizado SFML reales con interacciones programadas; la ventana permanece oculta. Las pruebas escriben sus fixtures bajo build/test-output.

La verificación definitiva de los ajustes C1 de paleta/cursor aprobó **9/9 suites en 17,46 s**. La nueva regresión comprueba 74 condiciones de fullscreen/cursor y 33 de consumidores; el experimento de JSON mantuvo idéntico el binario.

Los resultados medidos, recuentos, capturas y límites se conservan en la [referencia técnica](docs/REFERENCIA_TECNICA_PLAGUEFRONT.md#7-construccion-y-pruebas). No se presentan pruebas automáticas como sustituto de una partida manual o un mando físico.

## Documentación y ruta de aprendizaje

- [Curso interactivo de reconstrucción](docs/CURSO_DESARROLLO_PLAGUEFRONT.md): trece etapas desde AwesomeEngine original, 31 lecciones, fichas de cambios A/B/C, código progresivo, puntos de control y doce recorridos visuales Mermaid. Empieza por la [etapa 0](docs/CURSO_DESARROLLO_PLAGUEFRONT.md#etapa-0), trabaja en una copia independiente y comprueba cada resultado.
- [Referencia técnica de Plaguefront](docs/REFERENCIA_TECNICA_PLAGUEFRONT.md): inventario de archivos, contratos, tablas numéricas, formatos, recursos, controles, comandos y resultados para consultar mientras sigues el curso.
- [Licencia del proyecto](LICENSE.md): texto legal original conservado. Las fuentes mantienen sus licencias dentro de Content/Fonts.

Las etapas 0–10 reconstruyen el snapshot C0 suministrado; 11–12 incorporan la paleta configurable y la corrección de cursor del juego actual C1. Los laboratorios independientes enseñan escenario, clase, habilidad y terreno nuevos. El curso permite reconstruir, comprobar y comprender cada sistema; la referencia reúne los datos exactos. La [ruta práctica](docs/CURSO_DESARROLLO_PLAGUEFRONT.md#ruta-práctica) enlaza cada etapa con sus lecciones y el [atlas navegable](docs/CURSO_DESARROLLO_PLAGUEFRONT.md#atlas-diagramas). Mermaid necesita un visor compatible; no ejecuta el juego. Los 15 documentos anteriores se consolidaron en estos dos destinos; la copia local previa se conserva en build/normalizacion-baseline.

## Limitaciones verificadas

No se ha validado un mando físico, su ergonomía, ejes o conexión física. El redimensionamiento continuo se probó programáticamente mediante la ventana nativa, sin arrastre humano. Pantalla completa alcanzó el tamaño del escritorio y restauró la ventana; un aviso del controlador en la transición oculta impide certificar el cambio exclusivo físico del monitor.

Clicker y Bounce mantienen su lienzo heredado estirado en ultrapanorámico; Plaguefront utiliza escala uniforme. Maximizar/restaurar es una capacidad nativa de Windows. La cámara tiene zoom entero y desplazamiento, sin rotación; el arte utiliza perfiles dibujados y reflejados, sin seis direcciones corporales independientes. No se realizó una partida manual completa, evaluación competitiva de la IA, escucha humana de la mezcla ni una configuración Debug alternativa.
