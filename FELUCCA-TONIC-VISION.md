# FELUCCA TONIC — Visión y wishlist completa

Documento de producto: qué queremos que sea el firmware y qué debe poder hacer.
Complementa a FELUCCA-TONIC-SPEC.md (que define el orden de trabajo).

Base: fork de Felucca (hugelton/Felucca, GPL-3.0) para el M-VAVE FM-1.

Leyenda:
- [CORE] pedido explícito y prioritario de Juan.
- [WISH] deseado, entra si la memoria y la CPU lo permiten.
- [EVALUAR] idea heredada de listas anteriores, sin confirmar. Se decide después de la auditoría.
- [VERIFICAR] depende de algo que todavía no se comprobó en el código o el hardware.

---

## 1. Qué es

Un instrumento de vivo, tocable y deformable, para componer y tocar canciones enteras sin computadora.
No es una groovebox de loops boombap. La idea es:
- Tener muchas canciones guardadas adentro del aparato, con ideas complejas.
- Poder cambiar y secuenciar esos cambios dentro de cada canción, para no quedar preso de un loop eterno donde solo se sube y baja un cutoff.
- Control hands on de todo lo que sucede, con los 4 knobs como superficie de deformación.
- Menos engines, pero buenos, con más profundidad de sonido y modulación.

---

## 2. Motor de batería DRUM-X (estilo Microtonic) [CORE]

Síntesis 100% en tiempo real, sin samples.

Arquitectura de cada sonido:
- Oscilador principal con frecuencia inicial y caída exponencial de pitch (cantidad y decay del pitch).
- Modulador FM con amount y ratio.
- Generador de ruido con su propio decay.
- Envolventes exponenciales de decay para oscilador, FM, pitch y ruido.

Morphing A↔B:
- Cada sonido guarda dos patches (A y B).
- Un parámetro MORPH (0–127) interpola entre ambos en tiempo real.
- MORPH y PITCH se pueden bloquear por paso (parameter locks) y mover con macros.

Parámetros por sonido: TONE A/B, MORPH, PITCH, DECAY, NOISE, DRIVE, PAN, FM amount/ratio, pitch env.

Kits:
- Kits sintéticos incluidos (empezar con pocos).
- Kits de usuario construidos desde el editor web.
- [VERIFICAR] cuántos kits entran en flash.

Notas técnicas:
- [VERIFICAR] si el chip tiene unidad de punto flotante. Si no, el DSP va en punto fijo.
- Reutilizar las voces de batería existentes de Felucca donde sirva.

### Mutes por grupo [CORE]
- Cuatro grupos como mínimo: KICK, SNARE, HAT, PERCS.
- Mute en vivo con teclas dedicadas, LED apagado = muteado, sin clicks.
- Afecta la secuencia en tiempo real, sin borrar nada.
- Mute individual por lane opcional [WISH].
- Estado de mute guardado por variación y secuenciable en las escenas [VERIFICAR memoria].

---

## 3. Proyectos como canciones [CORE]

- 8 proyectos (mínimo 4), cada uno una canción independiente.
- Cada proyecto tiene 4 variaciones: A, B, C, D.
- Song chain al estilo SLOOP: cadena de pasos con repeticiones, tempo opcional por parte.
- Quick chain en vivo: mantener SAVE y tocar A B B C para encolar la cadena.
- Grabación de la canción en vivo: tocás las variaciones y quedan registradas en la chain.
- Proyectos con nombre, autosave por proyecto, backup y restore completo desde el editor web.
- [VERIFICAR] cuánto ocupa un proyecto y cuántos entran. Si no entran 8, se reduce y se avisa con números.
- De SLOOP se toma solo este sistema, no su enfoque groovebox.

### Escenas: cambios secuenciados dentro de la canción [CORE]
Cada paso de la chain puede llevar, además de la variación:
- estado de mutes por grupo
- posiciones de macros
- punch-in FX activos
La canción evoluciona sola o con intervención en vivo, en vez de repetir un loop.
- [VERIFICAR] costo de memoria por paso de chain.

---

## 4. Macros en los 4 knobs [CORE]

- Cuatro macros globales M1–M4 asignadas a los 4 knobs.
- Cada macro con hasta 4 destinos, amount de −100 a +100.
- Destinos: parámetros de los engines, de DRUM-X por lane (pitch, decay, morph), del master (clipper, punch), envíos de efectos, brillo y feedback.
- Ejemplo: un knob sube el decay de la batería, baja el cutoff de un sinte y sube el envío de delay.
- Macros fijas útiles: BRIGHT (lowpass global) y FEEDBACK (feedback del engine activo) [WISH].
- Guardado por proyecto.
- Knobs más responsivos: sin perder clicks al girar rápido, aceleración configurable.

---

## 5. Punch-in FX MIDI [CORE]

Deformación MIDI temporal y no destructiva, para batería y sintes. Mientras se mantiene FX + tecla el efecto se aplica; al soltar, todo vuelve exacto.

- OCT DOWN y OCT UP de todo
- 1/2 TEMPO
- DECAY CORTO
- RELEASE y DECAY LARGOS
- STUTTER MIDI con divisiones 1/16, 1/32 y tresillo
- ARP MOMENTÁNEO
- DEFORMACIÓN RANDOM (pitch y velocity dentro de las notas tocadas)

Extras:
- FX LATCH para dejarlos fijos.
- Elegir el objetivo: sintes, batería o ambos.
- Poder secuenciarlos (guardados como lane de bools o dentro de las escenas) [VERIFICAR RAM].
- Se implementan como overrides temporales, no escriben en el patrón.

---

## 6. Master: clipper y punch [CORE]

- Limiter por defecto al final de la cadena.
- CLIPPER de master con AMOUNT (saturación o clip suave previo al limiter).
- Control PUNCH inspirado en Drum Buss de Ableton: énfasis de transitorios y compresión, disparado por los eventos del grupo KICK (sidechain por eventos, sin detector de audio, para ahorrar CPU).
- Ducking opcional: el bombo baja el nivel de los sintes, con amount y release [WISH].
- Con amount en 0 el sonido no cambia.
- [VERIFICAR] costo de CPU antes de dejarlo siempre activo.

---

## 7. Engines de síntesis y modulación [CORE]

Objetivo: menos engines, pero buenos, y liberar memoria para guardar más canciones.

- Auditoría de cada engine: flash, RAM, CPU, voces, calidad de sonido.
- Recomendación por engine: mantener, recortar o fusionar. Yo decido cuáles.
- Engines candidatos a mantener, según listas previas: FM de 6 operadores, analógico virtual, granular y vocal [EVALUAR].
- Candidatos a recorte: los que consuman mucha RAM sin aportar tanto [EVALUAR].
- [VERIFICAR] dependencias antes de borrar (ej. granular y samples).

Profundidad de sound design:
- Matriz de modulación más rica: más slots, más fuentes (LFO, envolventes, velocity, macros, modulación por paso) y más destinos.
- Envolventes y LFOs asignables.
- Priorizar lo que dé más expresividad por byte de RAM.
- Propuestas primero, implementación después de mi aprobación.

---

## 8. Efectos [WISH]

- Revisar los efectos actuales: costo, calidad, qué se puede mejorar.
- Distorsión por canal con GAIN y TYPE (soft, hard, waveshaper).
- Mantener reverb, delay y chorus, con mejoras si la auditoría las justifica.

---

## 9. Automatización [CORE: mapear] [WISH: ampliar]

- Mapear qué parámetros se pueden automatizar hoy, en qué páginas y con qué resolución.
- Grabación de movimiento de knobs por paso, con la mayor resolución posible (idea: 1/64) [VERIFICAR].
- Límites de memoria de la automatización.
- Ver las lanes de automatización en el editor web [EVALUAR].

---

## 10. Ideas heredadas de listas anteriores [EVALUAR]

Sin confirmar, se decide después de la auditoría y solo si no compiten con la memoria de canciones:

- Página CHORD con nombre del acorde y notas en pantalla, y LEDs con la raíz más brillante.
- Borrar notas manteniendo REC mientras el loop sigue sonando.
- Roll o stutter manteniendo ARP, grabado como ratchets.
- Visualizadores tipo OP-1 en la pantalla (osciloscopio, espectro, etc.).
- Parameter locks manteniendo un paso y girando un knob; micro timing por paso.
- Fills, undo y redo.
- Filtro por track con lowpass y highpass.
- Ghost y hard hits con las teclas de octava.

---

## 11. Fuera de alcance

- Audio USB entrante de la PC pasando por los efectos: no hay evidencia de que sea posible (hoy el audio USB de Felucca es una entrada hacia la computadora que graba el master).
- Bluetooth MIDI.
- Enfoque groovebox boombap.

---

## 12. Principios de calidad

- Todo se prueba primero en el emulador web.
- El firmware arranca siempre; el rescate USB y el retorno al firmware oficial (V15) deben seguir funcionando.
- Backup del flash antes de instalar.
- Las ideas se miden: sin números de flash y RAM no se cambia el modelo de datos.
- El proyecto queda GPL-3.0 y se respetan las licencias de lo que se porte.
