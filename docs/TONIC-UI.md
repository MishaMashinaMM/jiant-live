# FELUCCA TONIC — Interfaz JIANT FM

El concepto ([jiant-fm-ui-concept.jpg](jiant-fm-ui-concept.jpg)) tiene 20 paneles. Cada uno corresponde a una
página o capa del firmware. Las ilustraciones son el fondo; los valores (BPM, sección, MORPH, pasos, niveles)
los dibuja el firmware encima, y algunas partes reaccionan a los knobs: arcos, pétalos, celdas, fila activa.

## Mapa: panel → firmware

| # | Panel | En el firmware | Estado de la función |
| --- | --- | --- | --- |
| 01 | HOME | HOME: los 4 tracks con su engine, tempo, canción y sección | existe |
| 02 | TRACK | EDIT del track (los parámetros de su engine) | existe |
| 03 | FM ALGORITHM | FM6: algoritmo y operadores | existe |
| 04 | TRACK MAP | los tracks y las 8 voces compartidas (activas / libres / robadas) | existe (falta la vista) |
| 05 | SEQUENCER | SEQ > STEP, grilla de batería de 8 lanes, 64 pasos | existe |
| 06 | SEQ PERFORMANCE | capa SEQ: SEQ TOOLS, grabación y mutes en vivo | existe |
| 07 | DRUM-X | el kit DRUM-X: un pétalo por sonido | hecho |
| 08 | DRUM MORPH | edición de un sonido: lado A / B, MORPH, PITCH DECAY NOISE DRIVE PAN | Fase 2 |
| 09 | MACRO | macros M1–M4 (LFO sostenido; fuentes M1–M4 en MOD) | existe |
| 10 | MACRO MAP | qué mueve cada macro (el mapa de la capa MACRO) | existe |
| 11 | PUNCH-IN FX | capa FX: hoy REPEAT 1/8, 1/16, 1/32, LPF, HPF; después los punch-in MIDI | parcial (Fase 7) |
| 12 | FX RACK | efectos por track: distorsión, envíos a chorus, delay, reverb | existe |
| 13 | MIXER | MIXER (GLO): nivel, pan, envíos, mute | existe |
| 14 | SONG | página SONG y capa de canción (SAVE sostenido) | hecho |
| 15 | CHORD → SHIFT | CHORD retirado; en su lugar el desfase OFS / PIT (capa SCL, SEQ > SHIFT) | existe |
| 16 | ARP | arpegiador | existe |
| 17 | AUTOMATION | automatización por paso y de knobs (AUTO LIST) | existe |
| 18 | VISUALIZER | osciloscopio | existe (sencillo) |
| 19 | SAVE / PROJECT | 8 canciones × secciones A–D: guardar, cargar, renombrar | hecho (falta renombrar) |
| 20 | PERFORMANCE | tempo, sección y macros en vivo, el punch-in activo | pendiente (macros) |

Equivalencias de nombres: en el concepto **PRJ** es la canción (1–8) y **VAR** la sección (A–D). FM1, FM2,
FM3, DRUM y VOICE son ejemplos de engines: el firmware tiene 4 tracks y cualquier engine en cada uno.

## Decisión: todo dibujado por código

Medido: las 20 pantallas como imágenes (16 colores, deflate, ≈ 262 KB) desbordaban la región de la app por
113 768 bytes. Decisión del usuario: **todo por código**, con animaciones que reaccionen a los parámetros, sin
gastar memoria en lo visual. Las pantallas del concepto quedan como referencia en `assets/ui-art/art/`.

- **`firmware/src/ui_organic.c`**: líneas antialiasadas en subpíxel (Q4), curvas Bézier, pétalos con nervaduras,
  anillos y nodos. Todo con enteros y la tabla de senos de `dsp.c`. Los colores (crema, coral, teal, mostaza,
  menta) salen de la paleta: en GREY y MONO todo queda en grises.
- **Paleta JIANT** (por defecto desde JIANT 0.1): fondo casi negro, línea crema, teal para los valores, coral
  para lo activo. Las otras paletas siguen en MENU > COLOR.
- **Animación**: el gráfico se redibuja cuando cambia su firma; las páginas vivas suman `ui.frame` a la firma.
  Respiración y vaivén salen de senos de `ui.frame`; los golpes, de las envolventes de las voces.

## Estilo: lámina anatómica

Referencia del usuario: las láminas murales de anatomía (Deyrolle / Auzoux, años 60). Línea crema fina y cerrada
sobre negro; color solo en los detalles (puntas rojas, vasos amarillos, cyan y verdes en paralelo); letras de
referencia cyan en círculos; ejes punteados; punteado para sombrear. Nada de flores genéricas: cada pantalla es un
**espécimen** cuyos órganos son partes del instrumento, y cuya anatomía cambia con los parámetros.

## Hoy: paleta térmica, gráficos funcionales

Por decisión del usuario, los dibujos quedan **en pausa** hasta terminar sonido y performance: las páginas usan otra
vez los gráficos funcionales (onda, envolvente, LFO, FX, algoritmo) y solo se aplica la **paleta térmica**.

- **Paleta JIANT** (la de fábrica): fondo negro, superficie índigo casi negra, texto blanco cálido, cyan para los
  valores y el tema, naranja para lo activo.
- **Rampa de calor** `heat_col` (`ui_graph.c`): índigo → violeta → rojo → naranja → amarillo → blanco. En JIANT la
  barra de cada tarjeta toma el color de su valor (bajo violeta, alto amarillo); en las otras paletas, el color de
  tema de siempre, y la rampa va de su superficie a su acento y su texto.
- **DRUM** (EDIT del track de batería): 8 columnas, una por sonido (BD SD CP CH OH TM RS CB), cuya altura y calor
  siguen el golpe y su caída; un sonido muteado queda tachado; arriba, la barra de MORPH de A a B. En EDIT > SOUND,
  el sonido editado en cyan.

Las láminas (`assets/ui-shapes/plates.svg`), su conversor y `og_ill` siguen en el repo para retomarlas después; el
firmware ya no las incluye (−11 KB de flash).

## Cómo reemplazar los dibujos (vectores propios)

Los dibujos actuales son **bocetos**. Las formas no están en el código: son SVG en `assets/ui-shapes/`, uno por
pantalla, y el build los convierte (`tools/gen_ui_shapes.py` → `build/gen/ui_shapes.h`). Para cambiarlos alcanza
con reemplazar los SVG respetando esto:

- `viewBox="-64 -64 128 128"`: una unidad es un paso guardado (int8) y el origen es el punto de anclaje que usa el
  firmware (en DRUM-X, la columna, el centro del espécimen). Y crece hacia abajo, como en SVG.
- Un `<path>` por forma, con `id`. Si la forma cambia con el MORPH (u otro parámetro), van dos: `NOMBRE_a` y
  `NOMBRE_b`, **con la misma cantidad de segmentos** (el firmware mueve cada punto de A a B). Una forma fija va con
  un solo path, sin sufijo.
- `data-mirror="1"`: el firmware también la dibuja reflejada (la mitad de un órgano simétrico, o uno de un par).
- Un solo subpath por path: `M` y luego `C S Q T L H V` (absolutos o relativos) y `Z`. Las líneas y cuadráticas se
  convierten en cúbicas. Los atributos de estilo (color, grosor) se ignoran: los colores los pone el firmware.
- Cada punto ocupa 2 bytes. El espécimen de DRUM-X entero son 148 bytes.

Las formas de DRUM-X y sus ids: `dorsal` (hats), `petal` (snare y clap, reflejado), `sepal` (kick, reflejado),
`lip` (percusión, reflejado). Si cambian la cantidad o los nombres de las formas de una pantalla, hay que ajustar
su función de dibujo (en DRUM-X, `graph_drumx` en `ui_graph.c`).

## Ilustraciones (las láminas del usuario)

`assets/ui-shapes/plates.svg` (del usuario): 17 especímenes en una lámina ([docs/plates-sheet.png](plates-sheet.png), dibujados
con el código del firmware). Un SVG **sin ids** se importa como ilustración: cada trazo se aplana y se simplifica a una
polilínea con error menor a medio píxel (Ramer–Douglas–Peucker), los círculos chicos pasan a ser nodos rellenos, y el
color dice el rol (0 blanco, 1 rojo, 2 teal, 3 cyan, 4 verde, 5 amarillo, 6 naranja, 7 azul / violeta). Los dibujos se
separan en 2D (cajas a menos de 12 unidades = el mismo espécimen; un punto suelto se une al dibujo más cercano) y se
numeran por filas (`SH_PLATES_1..17`, y las tablas `SH_PLATES_ILLS` / `SH_PLATES_LENS`). Las 17 ocupan 11 KB
(los contornos tal cual eran 46 KB). `og_ill` las dibuja y puede encender un rol con un valor o un golpe.

### Qué lámina va en cada pantalla

| Lámina | Pantalla | Qué se mueve |
| --- | --- | --- |
| 1 flor bilateral, 4 nodos rojos | HOME | un nodo rojo por track, late con sus voces |
| 2 vaso, 8 nodos teal | TRACK MAP | las 8 voces compartidas: encendida = sonando |
| 3 planta ramificada | FM6 algoritmo | los frondes por operador, su nivel |
| 4 iris, 8 nodos cyan | DRUM-X | un nodo por lane, se enciende con el golpe; MORPH abre los pétalos |
| 5 capullo cerrado | reposo / sin sección | — |
| 6 planta chica | ARP / SEQ | los nodos por paso |
| 7 cruz de 5 nodos | MACRO M1–M4 | cada nodo, el valor de su macro; el centro, el último tocado |
| 8 ala | FX | el ala se abre con los envíos |
| 9 vaina | SONG | los segmentos, las filas; la que suena encendida |
| 10 y 11 cinco espermatozoides | LFO / fuentes de modulación | la onda de la cola, la velocidad |
| 12 tallo con 4 puntas | MACRO MAP | cada punta, un destino; su brillo, el amount |
| 13 loto | MIXER | los pétalos, los niveles |
| 14 constelación | MOD MATRIX | las líneas punteadas, las rutas activas |
| 15 curvas | ENV / curvas | la curva elegida encendida |
| 16 capullo con rutas | PUNCH-IN FX | la ruta del efecto activo |
| 17 semilla de 3 nodos | SHIFT | el desfase de la secuencia |

## Pasos

1. HOME: los 4 tracks como un organismo (un nodo por track, latido con sus voces), tempo, canción y sección.
2. SONG y capa de canción: tallo con las filas como hojas, la que suena iluminada.
3. DRUM MORPH (Fase 2 de DRUM-X): la edición de un sonido con sus lados A y B.
4. PERFORMANCE, MACRO y PUNCH-IN cuando existan esas funciones.
