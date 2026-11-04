# FELUCCA TONIC — spec v3 (para Claude Code)

Fork de Felucca (hugelton/Felucca, GPL-3.0) para el M-VAVE FM-1.

## FILOSOFÍA
Instrumento tocable, deformable y versátil. No es una groovebox boombap.
- Muchas canciones guardadas dentro del aparato, cada una con ideas complejas.
- Cambios secuenciados dentro de cada canción: no quedar preso de un loop eterno donde solo se mueve un cutoff.
- Control hands on de todo lo que suena.
- Menos engines, pero bien elegidos y con más profundidad de sonido y modulación.
- De SLOOP se toma SOLO el sistema de proyectos/variaciones/chain (muy bien implementado). No se porta su enfoque groovebox.

## REGLAS DE TRABAJO
1. Todo lo marcado [VERIFICAR] es una suposición. Confirmalo leyendo el código antes de usarlo; si no coincide, avisame.
2. No inventes rutas, nombres ni números. Citá archivo y línea.
3. Probar primero en el emulador web. Flashear solo cuando suene bien ahí. Backup del flash antes de la primera instalación.
4. Una fase por sesión, en plan mode, un commit por fase. Frená al terminar cada una.
5. Medir flash y RAM antes de cambiar el modelo de datos. Si no entra, proponé recortes antes de implementar.
6. El fork queda GPL-3.0. Revisar la licencia de SLOOP (isod89/sloop-fm1) antes de copiar código.

## PRIORIDADES (en orden)
1. Motor de batería tipo Microtonic con morph A↔B.
2. Mutes por grupo: KICK, SNARE, HAT, PERCS (mínimo 4).
3. Proyectos = canciones independientes: 8 (mínimo 4), cada una con variaciones A B C D y song chain tipo SLOOP.
4. Cambios secuenciables dentro de cada canción (ver sección Escenas).
5. Punch-in FX MIDI para batería y sintes.
6. Master: limiter por defecto + clipper con amount + control PUNCH.
7. Auditoría y optimización de engines; más profundidad de sonido y modulación en los que queden.
8. Mejoras de efectos y mapa de automatización.

## FASE 0 — Auditoría (sin codear)
Entregables:
- CLAUDE.md con arquitectura real, build, emulador web y mapa de módulos.
- Tabla de supuestos [VERIFICAR] confirmados/refutados:
  - versión de Felucca, engines existentes, cantidad de tracks y voces
  - modelo actual de proyecto/song en Felucca (se cree 4 proyectos, patterns A–D)
  - cómo SLOOP implementa proyectos, variaciones, quick chain y song record (SLOOP tiene 4 proyectos y backup en web editor): qué estructuras de datos usa y cuánto flash ocupa cada proyecto
  - si el chip tiene FPU (si no, usar punto fijo en DSP)
  - flash y RAM totales, usados y libres; límite de tamaño del .fwsc
- Informe de engines: por cada uno, flash, RAM, costo de CPU/voces, calidad de sonido, y recomendación (mantener, recortar, fusionar). Foco: liberar espacio para más canciones sin perder expresividad.
- Mapa de automatización: qué parámetros se graban (locks por paso, motion de knobs), en qué páginas, con qué resolución, y qué límites de memoria tiene.
- Mapa de modulación: matriz actual (slots, fuentes, destinos) y dónde está el techo.
- Informe de efectos: qué hay, qué costo tiene cada uno y qué se puede mejorar.

## FASE 1 — DRUM-X mínimo (corte vertical)
Arquitectura Microtonic: oscilador principal con caída de pitch, modulador FM, generador de ruido, envolventes de decay.
- Cada sonido tiene dos patches (A y B) y un MORPH 0–127 que interpola linealmente.
- Primero un solo lane con MORPH y PITCH sonando en el emulador, disparado desde el secuenciador.
- Reutilizar voces de batería existentes cuando sirva.
Criterio: suena en el emulador, morph audible, nada roto.

## FASE 2 — DRUM-X completo + mutes por grupo
- Parámetros: DECAY, NOISE, DRIVE, PAN, FM amount/ratio, pitch env. Morph y pitch lockeables por paso.
- Cada lane se asigna a un grupo: KICK, SNARE, HAT, PERCS. Un mute por grupo en vivo (teclas dedicadas), LED apagado = muteado, sin clicks, afecta la secuencia en tiempo real. Mute de lane individual opcional.
- Estado de mute guardado por variación [VERIFICAR viabilidad de memoria].
- Kits sintéticos: empezar con pocos; medir flash antes de escalar.

## FASE 3 — Master: clipper y punch
- Limiter por defecto al final de la cadena.
- CLIPPER de master con AMOUNT (soft clip / saturación previa al limiter).
- PUNCH (inspirado en Drum Buss de Ableton): énfasis de transitorios y compresión, con disparo por eventos del grupo KICK (sidechain por MIDI/eventos, sin detector de audio, para ahorrar CPU).
- Opción de ducking: el bombo baja el nivel de los sintes con amount y release ajustables.
- [VERIFICAR] costo de CPU de cada bloque antes de dejarlo siempre activo.
Criterio: pegada audible, sin pumping indeseado con amount en 0.

## FASE 4 — Proyectos como canciones + chain
- 8 proyectos (mínimo 4) independientes, cada uno con variaciones A–D.
- Chain tipo SLOOP: quick chain (mantener SAVE y tocar A B B C), repeticiones por paso, tempo opcional por parte, grabación de la canción en vivo.
- Ajustar tamaño de proyecto según Fase 0. Si no entran 8, reducir y avisar con números.
- Nombres de proyecto, autosave por proyecto, backup/restore en web editor.

## FASE 5 — Escenas: cambios secuenciados dentro de cada canción
Cada paso de la chain puede llevar, además de la variación:
- estado de mutes por grupo
- posiciones de macros
- punch-in FX activos
Así se puede secuenciar la evolución de la canción, no solo el loop.
[VERIFICAR] cuánta memoria cuesta por paso de chain.

## FASE 6 — Macros en los 4 knobs
- M1–M4, cada una con hasta 4 destinos, amount −100…+100.
- Destinos: parámetros de engines, de DRUM-X, del master (clipper, punch), BRIGHT, FEEDBACK.
- Guardado por proyecto. Knobs sin perder clicks al girar rápido.

## FASE 7 — Punch-in FX MIDI (batería y sintes)
Overrides temporales no destructivos mientras se mantiene FX + tecla:
OCT DOWN, OCT UP, 1/2 TEMPO, DECAY CORTO, RELEASE/DECAY LARGO, STUTTER MIDI (1/16, 1/32, tresillo), ARP MOMENTÁNEO, DEFORMACIÓN RANDOM. FX LATCH opcional.
- Deben afectar tanto a sintes como a batería (con opción de elegir objetivo).
- Evaluar cómo guardarlos en las escenas (Fase 5). Medir RAM antes de decidir.
Criterio: al soltar, todo vuelve exacto.

## FASE 8 — Profundidad de sonido y modulación
Con los datos de Fase 0:
- Proponer mejoras en los engines que se mantengan: más fuentes y destinos en la matriz de modulación, más slots, LFOs/envolventes asignables, modulación por paso.
- Priorizar lo que dé más expresividad por byte de RAM.
- Mejoras de efectos y de DIST por canal (GAIN y TYPE).
- Proponer antes de implementar; yo decido cuáles.

## FASE 9 — Recorte de engines
- Borrar o fusionar solo lo que Fase 0 recomiende y yo apruebe.
- Verificar dependencias antes de borrar (ej. GRAIN y samples).

## FUERA DE ALCANCE
- Audio USB entrante de la PC pasando por efectos (sin evidencia de que sea posible).
- Bluetooth MIDI.
- Enfoque groovebox boombap de SLOOP.

## CRITERIOS GENERALES
- Compila con el build documentado y el .fwsc entra en el límite de tamaño.
- Arranca; rescate USB y retorno al firmware oficial siguen funcionando.
- Todo funciona en el emulador web antes de flashear.
