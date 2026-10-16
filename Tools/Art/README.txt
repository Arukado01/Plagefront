PLAGUEFRONT — PIPELINE LOCAL DEL ARTE APROBADO

Raíz del proyecto: D:\Proyectos\SFML\Plagefront\
Los comandos siguientes se ejecutan desde esa raíz. Son instrucciones de
regeneración y comprobación; no certifican una compilación o una sesión de juego.
El informe de entrega y las verificaciones realizadas se registran por separado
en build/art-final/ENTREGA.txt.

REGENERAR EL PACK

En PowerShell:
  & ./Tools/Art/BuildApprovedArt.ps1

Entrada pública equivalente, que usa el arte aprobado de forma predeterminada:
  & ./Tools/GeneratePixelArt.ps1

BuildApprovedArt.ps1 ejecuta BuildUnits.py, BuildEnvironment.py y BuildCasterEffects.py y copia
Tools/Art/Sources/menu-approved.png sobre la portada activa. Sustituye únicamente
units.png, terrain.png, props.png, effects.png y menu.png en
Content/Textures/Plaguefront/Pixel/. Conserva los recursos de UI, iconos,
cursores y las columnas de efectos no mágicos. No descarga imágenes ni llama a servicios
de generación durante esta reconstrucción.

El parámetro -ProjectRoot permite indicar otra raíz de proyecto. El script usa
primero el Python incluido en la instalación local de Codex, si está disponible;
en otro equipo usa python desde PATH. Dependencias: Python 3, Pillow y NumPy.
Puede comprobarse su disponibilidad con:
  python -c "import PIL, numpy"

La implementación procedural anterior se conserva explícitamente mediante:
  & ./Tools/GeneratePixelArt.ps1 -Legacy -ProjectRoot 'RUTA_DE_COPIA_AISLADA'

-Legacy es una referencia histórica: escribe el pack anterior en la raíz
indicada y puede sobrescribir atlas. Para compararlo debe usarse una copia
aislada, conservando el pack aprobado en el proyecto de trabajo.

FUENTES Y PROCEDENCIA

Los diseños originales proceden de la herramienta integrada image_gen. Los PNG
fuente se conservan localmente: no se necesita regenerarlos con IA para obtener
de nuevo el atlas. La adaptación técnica corta las regiones medidas, ajusta la
resolución y los materiales, articula las fases y empaqueta los fotogramas.
Idle se recupera exactamente del atlas anterior aprobado, conservado en
Sources/Units/Approved/approved-atlas.png y protegido por SHA256. Las 112 poses
regeneradas posteriores quedaron como antecedente inactivo: no son la entrada
del generador vigente. Los otros estados mantienen sus recursos reparados.

  Tools/Art/Sources/Units/
    approved-saber-lancer.png: diseños aprobados de Saber y Lancer.
    classes-keyposes.png: poses de las clases restantes, frente y espalda.
    attack-contact-front.png y attack-contact-back.png: contactos de ataque.
    fallen-keyposes.png: cuerpos caídos por clase.
    Approved/approved-atlas.png: copia inmutable del pack anterior aprobado;
      se recuperan sus 448 celdas Idle sin redibujar ni buscar nuevas insignias.
    Idle/: siete hojas posteriores y prompts, conservados como antecedentes;
      no participan en la reconstrucción vigente.

  Tools/Art/Sources/Effects/
    caster-impacts.png: veinte fases originales, cinco elementos y cuatro etapas.
    base-effects.png: atlas previo para conservar slash, arrow y rot_burst.
    prompt.txt: instrucciones de generación de los impactos.

  Tools/Art/Sources/Environment/
    materials-approved.png y props-approved.png: estudios de la propuesta aprobada.
    materials-supplement.png: Hill, Swamp, DeepWater y Blocked.
    props-supplement.png: roca, puesto avanzado, cadáver y putrefacción.
    surface-mask-source.png y bridge-deck-source.png: referencias técnicas del
    contorno hexagonal y de la huella del puente, conservadas para compatibilidad.
    sources.json: prompts completos de los suplementos, procedencia y contratos.

  Tools/Art/Sources/menu-approved.png: portada aprobada ya adaptada a 720x450.

Los prompts de la exploración aprobada se conservan en
build/art-direction-v1/generated/*-prompt.txt. Los recortes y hashes usados por
los generadores se registran en sus informes de build/art-final/. Las fuentes
son originales locales; este pipeline no realiza commits, cambios de remotos
ni publicaciones.

CONTRATOS QUE DEBEN CONSERVARSE

Unidades: Content/Textures/Plaguefront/Pixel/units.png, 2304x3584.
  Celda 48x64; ancla de pies (24,56); 48 columnas y 56 filas.
  7 clases x 2 propietarios x 4 orientaciones x 6 estados x 8 fases = 2688 frames.
  Clases 0..6: Saber, Archer, Lancer, Rider, Caster, Assassin, Berserker.
  Estados 0..5: Idle, Walk, Attack, Hurt, Death, Cast.
  Orientaciones 0..3: frente derecha, frente izquierda, espalda derecha,
  espalda izquierda. Las vistas izquierdas están reflejadas en el atlas.
  Columna = estado*8 + fase; fila = clase + 7*propietario + 14*orientación.
  Death termina en cuerpo horizontal; el renderer reutiliza la fase 7 para
  cadáveres persistentes, con clase y facción reconocibles.

Retratos: unit-icons.png, 168x48; celda 24x24; siete columnas por clase y dos
  filas por propietario. Se colocan por esquina superior izquierda.

Terreno: terrain.png, 1920x128; celda 48x32; ancla (24,16).
  40 columnas x 4 filas: cuatro bloques de diez tipos de suelo.
  Tipos 0..9: Plain, Forest, Hill, Swamp, Ruins, Road, River, Bridge,
  DeepWater, Blocked. Columna = tipo + 10*variante; fila = fase 0..3.
  160 frames, cuatro variantes de composición y cuatro fases a 5 fps.
  Los primeros diez tipos conservan su orden; no reutilizar las filas de
  animación como variantes estáticas. Las superficies mantienen la misma
  máscara hexagonal. Laterales, extrusión y uniones de caminos siguen siendo
  responsabilidad del renderer existente.

Props: props.png, 512x320; celda 64x80; ancla (32,68).
  Ocho columnas: bosque, ruinas, roca bloqueada, casa, fortaleza, puesto avanzado,
  cadáver genérico, putrefacción. Cuatro filas a 5 fps: 32 frames en total.
  El cadáver genérico es contenido del atlas; el juego utiliza principalmente
  units.png/Death para los cuerpos persistentes. Las banderas de propietario
  se dibujan aparte. Mantener el bosque en la primera columna, ya que su alfa
  sirve también para hover y desvanecimiento de la silueta.

Portada: menu.png, 720x450 opaca; sin textos, botones o UI incrustados.
  Mantener tranquila la izquierda, donde se superponen los menús actuales.

Los atlas utilizan alfa real 0/255 y filtro nearest. Las ampliaciones de
comparación usan nearest. El juego conserva su escala física habitual 2x,
vista general 1x y zoom entero existente. Los generadores verifican márgenes
de los frames; no se debe corregir un recorte desplazando anclas o cámara.

PALETAS Y CAMBIO MÍNIMO DE PRESENTACIÓN

Propietario 0, Corona de ceniza: azul y cruz clara.
  #1e3e76 #2866b7 #67a9e3 #aad5f0
Propietario 1, Pacto del umbral: carmesí y doble barra clara.
  #571c32 #a92e4c #de5b6c #f4b09c
Conservar por separado acero, piel, madera, cuero y otros materiales compartidos;
la variación de facción se aplica a tejidos e insignias, no como tinte global.

La putrefacción fuente mantiene los cuatro verdes exactos:
  #334535 #647d3c #9fb34f #d5d47a
Game::RotTexture los remapea a rotPalette de Content/Config.json. Así siguen
funcionando la paleta púrpura configurada y futuras opciones de presentación.
No sustituir esos colores fuente por el color final mostrado en una captura.

La variación de terreno se conserva mediante TerrainVariant en
Games/Source/Plaguefront/PixelRenderer.cpp: un hash unsigned de (q,r) elige
variante 0..3 y suma 10*variante a la columna. No usa tiempo, altura, cámara,
selección o estado de partida. Las cuatro filas siguen animándose por separado.
La variación es visual y estable; no modifica terrenos lógicos, reglas, mapas,
picking, movimiento, controles, partidas ni geometría. La proyección permanece
x=30q, y=14q+28r-6*altura, con la superficie hexagonal original.
Las correcciones posteriores de lluvia, Caster y peste se explican en las
secciones 7.8 de docs/REFERENCIA_TECNICA_PLAGUEFRONT.md y lección 33 del curso.
Su evidencia está en build/visual-corrections; la entrega anterior permanece
identificada en build/art-final y no certifica por sí sola esta revisión.

REGENERAR O VALIDAR COMPONENTES

Usando el Python disponible (el wrapper completo lo localiza automáticamente):
  python Tools/Art/BuildEnvironment.py
  python Tools/Art/BuildEnvironment.py --verify-only
  python Tools/Art/BuildUnits.py
  python -B Tools/Art/BuildUnits.py --idle-only
  python -B Tools/Art/BuildUnits.py --restore-approved-idle
  python -B Tools/Art/BuildCasterEffects.py

BuildEnvironment.py --verify-only regenera en memoria y compara los píxeles de
los atlas activos; no escribe archivos. Comprueba dimensiones, alfa, máscara,
cuatro fases distintas, cuatro variantes, límites de props y uso de variantes
en los escenarios. --output-dir y --preview-dir permiten destinos alternativos.

BuildUnits.py dispone de --project-root; actualmente no ofrece --verify-only.
Su ejecución reconstruye solamente units.png, conserva unit-icons.png y genera informes de
2688 frames, 336 secuencias, fases distintas, alfa, bordes y hashes fuente.
Consultar build/art-final/units/validation.json y sequences.json después de
regenerarlo; no confundir una ejecución terminada con la aprobación de todas
las comprobaciones. Los informes identifican cualquier secuencia repetida.
--restore-approved-idle y su alias --idle-only recuperan únicamente las 448
celdas originales sobre el atlas activo; conservan literalmente los otros
2240, retratos y otros recursos. --output-atlas admite un PNG alternativo para
comparar antes de integrar. El generador completo conserva esta misma base
Idle y reproduce el pack recuperado sin fuentes nuevas. BuildCasterEffects.py
solo escribe las columnas 2,3,4,5,7 del atlas de efectos; conserva 0,1,6.
El generador completo también corrige sólo Walk del Berserker: asigna los
píxeles originales del faldellín a la cintura para dibujarlos delante de ambas
piernas. Son 64 celdas, sin repintar ni modificar el diseño. Recuperar sólo
Idle no cambia esos estados; sobre el atlas final es una operación idempotente.
Sources/Units/Approved/locomotion-anchors.json conserva las 896 máscaras de
insignias originales Idle/Walk; el smoke exige este manifiesto y contrasta sus
píxeles con el atlas cargado y la captura real, sin omitirlo si falta.
Para la recuperación actual use BuildUnits.py, sin el wrapper completo ni
regenerar efectos. La evidencia vigente está en build/unit-integration/units
y build/unit-integration/runtime; build/visual-corrections conserva la fase
anterior de lluvia, magia y peste, cuyos cambios se mantienen.

Auditoría independiente de los PNG integrados y del alcance de cambios:
  python build/art-final/verification/verify_art.py
  python build/art-final/verification/audit_environment.py

Los informes y hojas de contacto están en build/art-final/units/ y
build/art-final/environment/. Los GIF muestran las fases; las vistas de tablero
del generador son composiciones independientes, no capturas del juego.
Hashes distintos no prueban movimiento natural ni calidad artística: revisar
también siluetas, contactos, cuerpos caídos, colores y alineación en las hojas.

La compilación, las suites y la revisión visual dentro del juego son pasos
adicionales. Consultar build/art-final/verification/LEEME.txt para repetirlos y
build/art-final/ENTREGA.txt para saber cuáles se ejecutaron realmente. Este
README no declara que esas comprobaciones de runtime ya hayan ocurrido.
