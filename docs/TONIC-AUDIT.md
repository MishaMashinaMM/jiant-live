# FELUCCA TONIC — Auditoría (Fase 0, en curso)

Base: Felucca 1.1.5.1 (`README.md:8`). Este documento junta lo comprobado en el código para
FELUCCA-TONIC-SPEC.md. Cada dato cita archivo y línea. Los tamaños salen del build real con el toolchain
de JieLi (sección 2b).

## 1. Decisiones tomadas

| Decisión | Fuente |
|---|---|
| DRUM se reemplaza por DRUM-X, construido sobre `drum_voice.c` / `eng_drum.c` (mismo número de engine, 10) | Juan |
| Se sacan los slots de samples de usuario (libera flash de datos para canciones) | Juan |
| Song mode: se adopta el modelo de SLOOP (sección/variación = proyecto completo, arranger por compases, live sections, quick chain, SONG REC) | Juan |
| SLICER por track (GATE/STUT) candidato a salir, reemplazado por punch-in FX MIDI secuenciados en lanes | Juan (propuesto) |

## 2. Supuestos [VERIFICAR]

| Supuesto | Resultado | Dónde |
|---|---|---|
| Versión de Felucca | 1.1.5.1 | `README.md:8` |
| Tracks / voces / pasos | 4 tracks, 8 voces compartidas, 64 pasos | `firmware/src/core.h:11-14` |
| FPU | El DSP es todo punto fijo (Q15); DRUM-X va en punto fijo | `firmware/src/dsp.c:3` |
| Proyectos en Felucca | 4 slots, 3648 B (FUN9) en un sector de 4 KB, copia A/B | `project.c:128`, `storage.c:29,69` |
| Patterns A–D | No existen como variaciones: la song encadena los 4 proyectos (16 filas, repeat 1–16) | `song_chain.c:3`, `core.h:256-261` |
| SLOOP tiene 4 proyectos | **Refutado**: sus secciones A–D son los 4 slots de proyecto; hay una sola canción | `sloop-fm1: arranger_scene.c:6-8`, `SLOOP.md` (Song mode) |
| Automatización | Un solo store de 64 eventos para los 4 tracks del proyecto, resolución de 1 paso | `motion.c:11`, `core.h:246-255` |
| Mutes | Por track (teclas negras en la capa FX), no se guardan; no hay mute por lane | `perform.c:7,295` |
| Matriz de modulación | 4 slots por track, 8 fuentes, 20 destinos | `web/EDITOR_PROTOCOL.md` (MOD) |
| Límite del app | 0x8DFBC B (~568 KB) | `tools/fm1pkg_make.py:30`, `firmware/app.ld` |
| RAM | 96 KB `.data/.bss` + pool 336 KB + noinit | `firmware/app.ld` |
| Tamaño real del binario | Ver sección 2b (build real) | `./build.sh` |
| CPU en el chip | Pendiente de hardware; en el emulador la canción pesada usa 2,1 % de tiempo real en un PC (no representa el chip) | `web/emu/emu_test.mjs` |

## 2b. Medidas reales (build de Felucca 1.1.5.1 con el toolchain JieLi, 2026-10-08)

Salida de `./build.sh` y `nm -S build/felucca.elf`. Todos los tests de host pasan (`tests/run_tests.sh`) y el
emulador compila y pasa `web/emu/emu_test.mjs`.

| Recurso | Usado | Límite | Libre |
|---|---|---|---|
| App (flash de código) | 447 596 B | 581 564 B (0x8DFBC) | **133 968 B (23 %)** |
| RAM `.data` + `.bss` | 91 220 B | 98 304 B | **7 084 B** |
| Pool | 331 204 B | 344 064 B | **12 860 B** |
| Paquete `.fwsc` | 610 086 B | — | — |

**Conclusión: la RAM es el cuello de botella, no la flash de código.** Cualquier función nueva que use RAM
(DRUM-X, canciones en RAM, mejores efectos) tiene que liberar RAM antes.

Por engine/bloque (suma de sus símbolos):

| Bloque | Flash | RAM (bss + pool) |
|---|---|---|
| Samples de fábrica (`SMP_DATA`) | 126 717 | — |
| Fuentes de UI | 42 900 | — |
| DRUM (`eng_drum.c` + `drum_voice.c`) | 13 159 | 7 296 |
| FM6 | 13 125 | 9 433 |
| PHYS | 9 053 | **51 552** |
| SLICE | 6 514 | 8 616 |
| PERFORM (capa FX) | 5 284 | 83 (usa `sl_buf`) |
| GRAIN | 4 666 | **28 080** |
| TRIO | 3 170 | 0 |
| SAMPLE | 2 710 | 417 |
| DIGITAL (conversión a FM6) | 2 514 | 0 |
| NOISE | 2 144 | 0 |
| VOICE (formant) | 2 058 | 4 |
| ANALOG | 1 732 | 0 |
| WHEEL | 1 682 | 0 |
| FX buses (delay, chorus, reverb, dist) | 1 432 | **147 040** (delay 131 072) |
| LOFI | 1 388 | 0 |
| PHASE | 1 126 | 0 |
| SLICER | 952 | **32 772** |
| Canvas de pantalla (`cv_px`) | — | 59 520 |
| Song chain actual (`chain`) | — | 12 512 |

## 3. Mapa de flash (datos)

| Rango | Uso hoy | Fuente |
|---|---|---|
| `0x97000–0x9EFFF` | 4 proyectos × A/B (8 sectores) | `storage.c:20-21,69` |
| `0x9F000` | FM6 de user presets, copia A | `storage.c:22,64` |
| `0xA0000–0xDBFFF` | 3 slots de samples de usuario (240 KB) | `eng_sample.c:46`, `storage.c:21` |
| `0xDC000–0xDFFFF` | Bancos de user presets | `storage.c:68` |
| `0xE0000–0xE4FFF` | Staging del loader OTA | `fm1_flash.h:42-43` |
| `0xE5000–0xE6FFF` | Autosave A/B | `fm1_flash.h:44-45` |
| `0xE7000–0xFBFFF` | Libre (~84 KB). **[VERIFICAR]** el firmware oficial hace su staging de update en esta zona; SLOOP usa parte para su banco FM6 | `storage.c:23-24` |
| `0xFC000–0xFEFFF` | Settings, FM6 de user presets copia B | `storage.c:62-64` |

## 4. Song mode de SLOOP (sloop-fm1 2.4.1, GPL-3.0-only)

| Pieza | Qué hace | Dónde (sloop-fm1) |
|---|---|---|
| Sección | Un proyecto completo (~3,8 KB, FUN5), 4 en RAM `.noinit` | `firmware/src/project.c:108` |
| Arranger | Módulo autocontenido: reloj de compases (muestras × BPM, conserva el resto), 16 entradas `{sección, compases 1–64}` + loop | `firmware/src/arranger.h` |
| Cambio de sección | En el borde de bloque: aplica el proyecto y reinicia los tracks al paso 0, en el compás | `arranger_scene.c:34`, `seq.c:1641` |
| Live sections | SAVE + teclas 1–4 saltan en el próximo compás; 5–8 guardan el loop en A–D | `seq.c:1586`, `ui_layers.c:360` |
| Quick chain | SAVE + varias secciones (hasta 8), en loop; cada una dura su patrón más largo | `seq.c:1592`, `arranger_scene.c:11` |
| SONG REC | Graba sección + compases tocados; más de 64 compases sigue en otra entrada | `seq.c:1599-1678` |
| Song backup | Tocar la canción aparta el loop y lo devuelve al parar | `arranger_scene.c:52` |
| Guardado | Secciones a RAM al instante; a flash con el transporte parado (borrar corta el audio ~50 ms); la canción va dentro de settings | `project.c:521,739-765` |

Qué se reutiliza: `arranger.h` tal cual (con crédito); live sections, quick chain y SONG REC se portan adaptados al
reloj de `seq.c` de Felucca; el formato de proyecto y la UI son los de Felucca. `song_chain.c` se reemplaza.

## 5. Diseño propuesto: canciones

- **Variación = proyecto completo (FUN9).** Los pasos ocupan ~3 KB de los 3,6 KB; separar sonidos por canción
  ahorraría poco y quitaría la posibilidad de cambiar sonidos por variación.
- **Flash:** 8 canciones × 4 variaciones × A/B = 64 sectores + índice de canciones A/B (nombre, chain de 16
  entradas, escenas) = 66 sectores. Disponibles sin samples de usuario: 68 (`0x97000–0x9EFFF`, `0xA0000–0xDBFFF`).
  **Margen: 2 sectores.**
- **Escena por entrada de chain:** sección + compases + mutes de grupo (1 B) + macros (4 B) + punch-in FX (1 B):
  ~8 B × 16 = ~130 B por canción.
- **RAM:** solo la canción activa (4 variaciones, ~14,6 KB), reemplaza `chain` (12 512 B medidos): suma
  ~2 KB a una RAM que tiene 7 KB libres en `.bss` y 12,8 KB en el pool.
- **Cambio de canción:** solo con el transporte parado.

## 6. Recursos liberables

### RAM (medido, ver 2b)

| Bloque | RAM | Nota |
|---|---|---|
| Delay | 128 KB | mono, 16 bit, 1,49 s (`fx.c:5`) |
| PHYS | 50,3 KB | |
| Buffer SLICER (compartido con la capa FX: REPEAT, REVERSE, TAPE, FREEZE, OCT) | 32 KB | `slicer.c:20`, `perform.c:17` |
| GRAIN | 27,4 KB | engine más caro en CPU (`tests/target_budget.txt`: grain_render 1731) |
| Chain actual | 12,2 KB | se reemplaza |
| FM6 | 9,2 KB | |
| SLICE | 8,4 KB | |

### Flash de código

- Samples de fábrica embebidos: 126 244 B ADPCM (PIANO, FLUTE, SAX, BREAK de SLICE), de `tools/gen_samples.py`.
  Los usan SAMPLE, SLICE y GRAIN.
- Sacar engines libera código y RAM, **no** espacio para canciones (eso sale solo de la región de datos).

### Dependencias de los samples de usuario

SAMPLE USR1–3, SRC USR de GRAIN, USR de SLICE, `slice_store.c`, subida/grabación/trim del editor,
`tools/fm1_sample_upload.py`, comandos de samples del protocolo SysEx.

## 7. Punch-in FX y lanes

- SLICER por track: 4 parámetros, `slicer_track` 632 en `tests/target_budget.txt`. Candidato a salir.
- Capa FX: REPEAT queda duplicado por el STUTTER MIDI. REVERSE, TAPE STOP, FREEZE y OCT (shimmer) son de audio y
  no se imitan con MIDI.
- Costo estimado en lanes: punch-in FX 64 B por variación (8 bits por paso), macros 256 B por variación
  (4 × 64 pasos × 1 B). No consumen el store de 64 eventos de automatización.

## 8. Engines (propuesta preliminar, decide Juan)

| Engine | Propuesta |
|---|---|
| DRUM | Reemplazar por DRUM-X — **hecho** |
| ANALOG, FM6, VOICE | Mantener |
| TRIO | Fusionar con ANALOG |
| SAMPLE, SLICE | Recortar — **retirados (JIANT 0.3)** con sus samples |
| PHYS | Recortar (RAM) |
| GRAIN | Decidir (depende de los samples de fábrica) — **retirado (JIANT 0.3)** |
| PHASE, LOFI, NOISE, WHEEL | Opcional (poca RAM) |

## 9. Riesgos y decisiones abiertas

0. **RAM:** 7 KB libres en `.bss` y 12,8 KB en el pool. DRUM-X y las canciones necesitan liberar RAM primero
   (PHYS 50 KB, SLICER 32 KB, GRAIN 27 KB o el delay de 128 KB son las fuentes posibles).
1. **Tamaño de proyecto:** un sector admite 3840 B; FUN9 usa 3648 (quedan 192 B). DRUM-X A/B por lane
   necesita ~160 B. Si no entra: comprimir pasos vacíos. Medir en Fase 1.
2. **Botón de la capa de canción:** en SLOOP es SAVE sostenido; en Felucca SAVE sostenido es undo y SEQ
   sostenido abre la canción. Pendiente: mover el undo (SLOOP: EDIT + OCT−) o usar SEQ.
3. **Margen de flash:** 2 sectores. La zona `0xE7000–0xFBFFF` queda [VERIFICAR] por el staging del firmware oficial.
4. **Efectos de audio de la capa FX:** sacarlos todos (32 KB para mejor reverb) o conservar alguno.
5. **PHYS / GRAIN / SAMPLE / SLICE:** confirmar recortes.

## 10. Pendiente de la Fase 0

- `CLAUDE.md` con arquitectura, build y mapa de módulos.
- CPU en el chip (solo con hardware; el emulador corre en el PC).
- Informe de efectos y mapa completo de modulación y automatización.

## 11. Entorno de build (sesión cloud)

- Dominios habilitados: `pkgman.jieliapp.com`, `jl-update.oss-cn-shenzhen.aliyuncs.com` (toolchain),
  `gitee.com` (SDK AC79).
- Toolchain: `tools/get_toolchain.sh` → `~/.jieli/toolchain`. SDK: clon sparse de `cpu/wl82/tools` en
  `~/fw-AC79_AIoT_SDK` (gitee corta clones grandes; el sparse funciona).
- Emscripten: `emsdk` en `~/emsdk` (`source ~/emsdk/emsdk_env.sh`), luego `web/emu/build.sh`.
