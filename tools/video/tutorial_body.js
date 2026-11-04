/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT 0.8.0) the tutorial film, drawn frame by frame (tools/video/film.mjs "tutorial"): black, Chakra Petch, the
 * heat palette and the beings (tutorial.html carries their code, from the design studies), the FM-1's real screen at
 * three times its size on the right, each chapter's animated graphic on the left, the voice-over's words as subtitles,
 * a thin bar of the chapters at the foot. data.json: film_prep.py's (the music's levels, each track's peak);
 * timeline.json: tools/video/tutorial_voice.py's (the chapters, each line's words, cue and time). */
const W = 1920, H2 = 1080, FPS = 30, BEAT = 60 / 116;
const cv = document.getElementById('cv'), X = cv.getContext('2d');
const scr = document.createElement('canvas'); scr.width = scr.height = 240; const sx = scr.getContext('2d'); const simg = sx.createImageData(240, 240);
let D, TL, VER = 'v0.8.0';
const FONT = '"Chakra Petch", sans-serif';
const ease = (u) => { u = clamp(u, 0, 1); return u < .5 ? 4 * u * u * u : 1 - Math.pow(-2 * u + 2, 3) / 2; };
const fade = (t, a, b) => clamp((t - a) / (b - a), 0, 1);
const EN = {"27 TECLAS: TOCAN LA PISTA ELEGIDA": "27 KEYS: THEY PLAY THE SELECTED TRACK", "6 PISTAS · 8 MOTORES · SECUENCIADOR · EFECTOS": "6 TRACKS · 8 ENGINES · SEQUENCER · EFFECTS", "6 tipos · AMP": "6 types · AMP", "8 SONIDOS": "8 SOUNDS", "ACTIVO": "ACTIVE", "AFINACIÓN": "PITCH", "AMPLITUD": "AMPLITUDE", "ARPEGIO": "ARPEGGIO", "ARRANGE: SECCIONES EN FILAS": "ARRANGE: SECTIONS IN ROWS", "BAJO": "BASS", "BATERÍA": "DRUMS", "CADA PISTA, SU LARGO: GIRAN UNAS CONTRA OTRAS": "EACH TRACK ITS LENGTH: THEY TURN AGAINST EACH OTHER", "CLOCK: EN TEMPO CON OTROS EQUIPOS": "CLOCK: IN TIME WITH OTHER GEAR", "COMPONENTES POR PASO": "COMPONENTS PER STEP", "Chrome o Edge, por USB": "Chrome or Edge, over USB", "DESHACER": "UNDO", "DESPACIO: AJUSTE FINO": "SLOW: FINE TUNING", "EFECTOS": "EFFECTS", "EL CARRIL DE PUNCH-IN": "THE PUNCH-IN LANE", "EL CELULAR · 10 CANALES": "THE PHONE · 10 CHANNELS", "EMULADOR": "EMULATOR", "ENCABEZADO": "HEADER", "ENV Y LFO EN CADA PISTA": "ENV AND LFO ON EVERY TRACK", "ENVÍOS · COMPARTIDOS": "SENDS · SHARED", "ESCALA": "SCALE", "ESQUEMA DEL PANEL": "PANEL SCHEMATIC", "FILTRO": "FILTER", "FX MANTENIDO: LAS TECLAS SON EFECTOS": "FX HELD: THE KEYS ARE EFFECTS", "GLO mantenido + G4 · otra vez: rehacer": "GLO held + G4 · again: redo", "GRACIAS A QUIENES APOYAN EL PROYECTO": "THANK YOU TO EVERYONE WHO SUPPORTS THE PROJECT", "GRACIAS": "THANK YOU", "GRÁFICO": "GRAPHIC", "INSTALADOR": "INSTALLER", "JIANT en el navegador": "JIANT in your browser", "LFO MANTENIDO: LAS PERILLAS SON M1–M4": "LFO HELD: THE KNOBS ARE M1–M4", "LISTO": "DONE", "LOS DADOS, PARTE POR PARTE": "THE DICE, PART BY PART", "MANTENER": "HOLD", "MATRIZ MOD: 4 RUTAS MÁS POR PISTA": "MOD MATRIX: 4 MORE ROUTES PER TRACK", "MOTORES": "ENGINES", "MUCHO": "A LOT", "MUTATE: CADA TANTOS COMPASES": "MUTATE: EVERY FEW BARS", "MUTEADA": "MUTED", "NIVEL": "LEVEL", "PARTES": "PARTS", "PIANO ROLL · HASTA 4 NOTAS POR PASO": "PIANO ROLL · UP TO 4 NOTES PER STEP", "PIE": "FOOTER", "PLAYHEADS: CABEZALES EXTRA": "PLAYHEADS: EXTRA HEADS", "POCO": "LITTLE", "PROYECTO": "PROJECT", "REC + PLAY: GRABÁS EN VIVO": "REC + PLAY: RECORD LIVE", "RUIDO": "NOISE", "RÁPIDO: TODO EL RANGO": "FAST: THE WHOLE RANGE", "SAVE MANTENIDO: LAS SECCIONES EN VIVO": "SAVE HELD: SECTIONS LIVE", "SELECCIONADO": "SELECTED", "SONIDOS": "SOUNDS", "STEMS+IN · NUEVO EN 0.8.0": "STEMS+IN · NEW IN 0.8.0", "TARJETAS": "CARDS", "TOCAR": "TAP", "TU DAW · 8 CANALES": "YOUR DAW · 8 CHANNELS", "TUTORIAL · EL SISTEMA COMPLETO": "TUTORIAL · THE WHOLE SYSTEM", "USB › STEMS · las 6 pistas por separado + los efectos": "USB › STEMS · the 6 tracks apart + the effects", "VELOCIDAD": "VELOCITY", "analógico o digital · pitch · ping pong": "analog or digital · pitch · ping pong", "bombo": "kick", "cada tecla, un efecto momentáneo": "each key, a momentary effect", "chip · bytebeat · floatbeat": "chip · bytebeat · floatbeat", "con OK para quedártelo o CANCEL para volver": "OK to keep it, CANCEL to go back", "distorsión de fase": "phase distortion", "dos osciladores, filtro por voz": "two oscillators, filter per voice", "efectos": "effects", "el sonido, a los dados": "the sound, rolled", "entran en el compás siguiente": "they come in on the next bar", "hat abierto": "open hat", "hat cerrado": "closed hat", "la criatura, la envolvente, los pasos…": "the creature, the envelope, the steps…", "la música: tocada por JIANT en el FM-1 · la voz: sintetizada": "music: played by JIANT on the FM-1 · voice: synthesized", "la pista": "the track", "la página y dónde estás": "the page and where you are", "la voz": "the voice", "las páginas": "the pages", "limitador · CLIP · PUNCH · DUCK": "limiter · CLIP · PUNCH · DUCK", "lo que tocaste se repite con el tema": "what you played repeats with the song", "los sonidos": "the sounds", "primero: Return to official V15 first (también guarda un backup)": "first: Return to official V15 first (it also saves a backup)", "redoblante": "snare", "reproducción · tempo · carga · batería": "transport · tempo · load · battery", "ruido coloreado y metálico": "coloured and metallic noise", "seis operadores, 32 algoritmos": "six operators, 32 algorithms", "sin samples": "no samples", "soltás: volvés a donde estabas": "let go: back where you were", "sonidos, bancos y backups": "sounds, banks and backups", "su variación": "its variation", "teclas negras: mutes · blancas: solos": "black keys: mutes · white: solos", "todo el kit, de un lado al otro": "the whole kit, side to side", "tu voz por los efectos de la última pista, en su propio canal": "your voice through the last track’s effects, in its own channel", "una idea": "an idea", "una lista con todos los motores": "one list with every engine", "una por perilla: KNOB 1–4": "one per knob: KNOB 1–4", "voz por formantes": "formant voice", "órgano con rotativo": "organ with rotary", "¿VENÍS DE OTRO FIRMWARE CUSTOM?": "COMING FROM ANOTHER CUSTOM FIRMWARE?", "y PANEO": "and PAN", "MOTOR": "ENGINE", "INSTALADOR WEB": "WEB INSTALLER", "CABLE USB DE DATOS": "USB DATA CABLE", "directo a la computadora, sin hub": "straight to the computer, no hub", "no desenchufes mientras escribe": "don’t unplug while it writes", "juanjiant-bit.github.io/JIANT · Chrome o Edge": "juanjiant-bit.github.io/JIANT · Chrome or Edge", "todo el kit": "the whole kit"};
let LANG = "es";
const tr = (s) => LANG !== "en" ? s : (EN[s] !== undefined ? EN[s] : s.replace(/ compases$/, " bars").replace(/^vuelta /, "loop ").replace(" · LA PANTALLA REAL, EN TIEMPO REAL", " · THE REAL SCREEN, IN REAL TIME"));
function T(s, x, y, size, w = 500, col = C.text, al = 'left', ls = 0) {
  s = tr(s);
  X.font = `${w} ${size}px ${FONT}`; X.fillStyle = col; X.textAlign = al; X.textBaseline = 'alphabetic';
  if (ls && X.letterSpacing !== undefined) X.letterSpacing = ls + 'px';
  X.fillText(s, x, y);
  if (ls && X.letterSpacing !== undefined) X.letterSpacing = '0px';
  return X.measureText(s).width;
}
function rr(x, y, w, h, r, stroke, fill, lw = 1.5) {
  X.beginPath(); X.roundRect(x, y, w, h, r);
  if (fill) { X.fillStyle = fill; X.fill(); }
  if (stroke) { X.strokeStyle = stroke; X.lineWidth = lw; X.stroke(); }
}
function heatRule(x, y, w, h = 2, a = 0, b = 0.86) {
  const g = X.createLinearGradient(x, 0, x + w, 0);
  for (let i = 0; i <= 6; i++) g.addColorStop(i / 6, heat((a + (b - a) * i / 6) * 256));
  X.fillStyle = g; X.fillRect(x, y, w, h);
}
function arrow(x1, y1, x2, y2, col, lw = 1.5, head = 8) {
  X.strokeStyle = col; X.fillStyle = col; X.lineWidth = lw;
  X.beginPath(); X.moveTo(x1, y1); X.lineTo(x2, y2); X.stroke();
  const a = Math.atan2(y2 - y1, x2 - x1);
  X.beginPath(); X.moveTo(x2, y2); X.lineTo(x2 - head * Math.cos(a - .4), y2 - head * Math.sin(a - .4));
  X.lineTo(x2 - head * Math.cos(a + .4), y2 - head * Math.sin(a + .4)); X.closePath(); X.fill();
}
function node(x, y, w, h, label, on, sub) {
  rr(x, y, w, h, 6, on ? C.orange : C.faint, on ? 'rgba(255,122,0,.08)' : '#08070b', on ? 2 : 1.2);
  T(label, x + w / 2, y + h / 2 + (sub ? -2 : 8), 22, 600, on ? C.text : C.mid, 'center');
  if (sub) T(sub, x + w / 2, y + h / 2 + 22, 14, 500, on ? C.orange : C.dim, 'center');
}
const SP = { ANALOG: [G.rad, { spikes: 24, len: 98, wob: 66, lob: 3, nuc: 2, ripple: 160 }], FM6: [G.pol, { spikes: 14, len: 110, wob: 80, lob: 6, nuc: 3, sep: 90 }],
  PHASE: [G.amo, { spikes: 5, len: 140, wob: 150, lob: 2, nuc: 1 }], LOFI: [G.dia, { spikes: 8, len: 90, wob: 50, lob: 4, nuc: 1 }],
  VOICE: [G.hyd, { spikes: 6, len: 160, wob: 90, lob: 3, nuc: 2, sep: 120 }], WHEEL: [G.cil, { spikes: 30, len: 80, wob: 70, lob: 5, nuc: 1 }],
  NOISE: [G.spo, { spikes: 16, len: 70, wob: 110, lob: 5, nuc: 1, ripple: 120 }], DRUM: [G.drm, { spikes: 12, len: 120, wob: 60, lob: 3, nuc: 1 }],
  PAD: [G.rad, { spikes: 8, len: 60, wob: 140, lob: 2, nuc: 1 }] };
/* the film's six tracks (capture_tutorial.c) */
const TRK = [['DRUM-X', 'BATERÍA', 'DRUM'], ['ANALOG', 'BAJO', 'ANALOG'], ['ANALOG', 'PAD', 'PAD'], ['PHASE', 'LEAD', 'PHASE'], ['FM6', 'ARPEGIO', 'FM6'], ['NOISE', 'METAL', 'NOISE']];
const sm = new Float32Array(6);
function en(f, k) { const v = D.trk[Math.min(f, D.n - 1)] ? D.trk[Math.min(f, D.n - 1)][k] : 0; return clamp(v, 0, 1.3); }
function being(sp, x, y, r, e, t, o = {}) { const [g, f] = SP[sp]; organism(X, g, f, x, y, r, e, t, { ex: 3, nv: 80, ...o }); }

/* ---- where we are */
function at(t) {
  const chs = TL.chapters;
  let ci = 0;
  for (let i = 0; i < chs.length; i++) if (t >= chs[i].start - 0.6) ci = i;
  const ch = chs[ci];
  let li = 0;
  for (let i = 0; i < ch.lines.length; i++) if (t >= ch.lines[i].start - 0.15) li = i;
  const ln = ch.lines[li];
  return { ci, ch, li, ln, cue: t < ch.lines[0].start - 0.15 ? 0 : ln.cue, ct: t - ch.start, lt: t - ln.start, ld: ln.end - ln.start };
}
/* the subtitle: the line in chunks at its commas and stops, each its share of the line's time */
function subtitle(t, A) {
  const { ln } = A;
  if (t > ln.end + 0.5 || t < ln.start - 0.1) return;
  const parts = ln.text.match(/[^.,:;?¿!]+[.,:;?!]*/g).map((s) => s.trim()).filter(Boolean);
  const chunks = []; let cur = '';
  for (const p of parts) { if ((cur + ' ' + p).trim().length > 74 && cur) { chunks.push(cur); cur = p; } else cur = (cur + ' ' + p).trim(); }
  if (cur) chunks.push(cur);
  const tot = chunks.reduce((a, c) => a + c.length, 0), u = clamp((t - ln.start) / (ln.end - ln.start), 0, 0.999) * tot;
  let acc = 0, s = chunks[0];
  for (const c of chunks) { if (u < acc + c.length) { s = c; break; } acc += c.length; }
  X.globalAlpha = fade(t, ln.start - 0.1, ln.start + 0.15) * (1 - fade(t, ln.end + 0.25, ln.end + 0.5));
  T(s, W / 2, 1000, 34, 500, C.text, 'center');
  X.globalAlpha = 1;
}
/* the FM-1's screen, three times its size, sharp */
const SX = 1080, SY = 128, SS = 720;
function screen(f, a) {
  X.globalAlpha = a;
  rr(SX - 14, SY - 14, SS + 28, SS + 28, 10, '#2b2833', '#030304', 1.5);
  X.imageSmoothingEnabled = false; X.drawImage(scr, SX, SY, SS, SS); X.imageSmoothingEnabled = true;
  T('FM-1', SX - 14, SY + SS + 50, 18, 600, C.mid);
  T(`JIANT ${VER} · LA PANTALLA REAL, EN TIEMPO REAL`, SX + 50, SY + SS + 50, 16, 500, C.dim);
  X.globalAlpha = 1;
}
function mark(x, y, w, h, a = 1) {   /* a region of the real screen, in 240-pixel units */
  X.globalAlpha = a; rr(SX + x * 3 - 4, SY + y * 3 - 4, w * 3 + 8, h * 3 + 8, 6, C.orange, null, 2.5); X.globalAlpha = 1;
}

/* ---------------------------------------------------------------- the graphics: one a chapter --- */
const GX = 120, GY = 300, GW = 860, GH = 560;
const Gr = {};
Gr.install = (A, t, f) => {
  const cards = [['1', 'INSTALADOR WEB', 'juanjiant-bit.github.io/JIANT · Chrome o Edge'], ['2', 'CABLE USB DE DATOS', 'directo a la computadora, sin hub'],
                 ['3', 'INSTALL', 'no desenchufes mientras escribe']];
  cards.forEach((c, i) => {
    const y = GY + i * 140, on = A.cue === 0 ? i < 2 : A.cue === 1 ? i === 2 : false, a = fade(A.ct, 0.3 + i * 0.35, 0.9 + i * 0.35);
    X.globalAlpha = a;
    rr(GX, y, GW, 116, 8, on ? C.orange : C.faint, '#07060a', on ? 2 : 1.2);
    T(c[0], GX + 44, y + 76, 54, 600, on ? heat(170) : C.faint, 'center');
    T(c[1], GX + 100, y + 52, 30, 600, on ? C.text : C.mid);
    T(c[2], GX + 100, y + 86, 19, 500, C.dim);
    if (i === 2 && A.cue >= 1) {
      const p = A.cue === 1 ? ease(A.lt / A.ld) : 1;
      rr(GX + 560, y + 50, 260, 10, 5, C.faint, null, 1); heatRule(GX + 560, y + 50, 260 * p, 10, 0, p * 0.9);
      T(p >= 1 ? 'LISTO' : Math.round(p * 100) + ' %', GX + 820, y + 90, 18, 600, p >= 1 ? C.orange : C.mid, 'right');
    }
    X.globalAlpha = 1;
  });
  if (A.cue === 2) {
    const a = fade(A.lt, 0, 0.4); X.globalAlpha = a;
    rr(GX, GY + 430, GW, 100, 8, C.violet, 'rgba(184,112,255,.07)', 2);
    T('¿VENÍS DE OTRO FIRMWARE CUSTOM?', GX + 30, GY + 470, 24, 600, C.text);
    T('primero: Return to official V15 first (también guarda un backup)', GX + 30, GY + 505, 19, 500, C.violet);
    X.globalAlpha = 1;
  }
};
Gr.panel = (A, t, f) => {
  const bx = GX, by = GY + 10, bw = GW, bh = 520;
  rr(bx, by, bw, bh, 22, '#2b2833', '#060508', 1.5);
  const encs = [['ALGORITHM', 'la pista'], ['PRESETS', 'los sonidos'], ['SELECT', 'las páginas']];
  encs.forEach((e, i) => {
    const x = bx + 90 + i * 150, y = by + 95, on = A.cue === 0 || (A.cue === 1 && Math.floor(A.lt / (A.ld / 3)) === i);
    X.lineWidth = on ? 3 : 1.5; X.strokeStyle = on ? C.orange : C.faint; X.beginPath(); X.arc(x, y, 38, 0, TAU); X.stroke();
    const ang = t * (on ? 2.2 : 0.3) + i;
    X.beginPath(); X.moveTo(x + 22 * Math.cos(ang), y + 22 * Math.sin(ang)); X.lineTo(x + 34 * Math.cos(ang), y + 34 * Math.sin(ang)); X.stroke();
    T(e[0], x, y + 66, 16, 600, on ? C.text : C.mid, 'center');
    if (A.cue === 1 && on) T(e[1], x, y + 90, 18, 500, C.orange, 'center');
  });
  const kv = A.cue === 2 ? (A.lt < A.ld / 2 ? (A.lt * 3) % 1 : 0.5 + 0.08 * Math.sin(A.lt * 2)) : 0.3;
  for (let i = 0; i < 4; i++) {
    const x = bx + 560 + i * 74, y = by + 95, on = A.cue === 2;
    X.lineWidth = on ? 3 : 1.5; X.strokeStyle = on ? C.orange : C.faint; X.beginPath(); X.arc(x, y, 26, 0, TAU); X.stroke();
    const ang = Math.PI * 0.75 + (on ? kv : 0.3 + i * 0.1) * Math.PI * 1.5;
    X.beginPath(); X.moveTo(x, y); X.lineTo(x + 22 * Math.cos(ang), y + 22 * Math.sin(ang)); X.stroke();
    T('K' + (i + 1), x, y + 52, 15, 600, on ? C.text : C.mid, 'center');
  }
  if (A.cue === 2) T(A.lt < A.ld / 2 ? 'RÁPIDO: TODO EL RANGO' : 'DESPACIO: AJUSTE FINO', bx + 670, by + 190, 18, 600, C.orange, 'center');
  const btn = ['HOME', 'ENV', 'LFO', 'FX', 'SCL', 'EDIT', 'GLO', 'SAVE', 'ARP', 'SEQ', 'PLAY', 'REC'];
  btn.forEach((b, i) => {
    const x = bx + 34 + i * 67, y = by + 240, k = A.cue === 3 ? Math.floor(A.lt / (A.ld * 0.55 / 9)) : -1;
    const on = A.cue === 3 && i >= 1 && i <= 9 && k >= i - 1;
    rr(x, y, 58, 34, 5, on ? C.orange : C.faint, on ? 'rgba(255,122,0,.1)' : null, on ? 2 : 1.2);
    T(b, x + 29, y + 23, 14, 600, on ? C.text : b === 'PLAY' ? C.violet : b === 'REC' ? C.red : C.mid, 'center');
  });
  const kx = bx + 40, ky = by + 320, kw = (bw - 80) / 16;
  const lit = A.cue === 3 && A.lt > A.ld * 0.55;
  for (let i = 0; i < 16; i++) {
    const on = lit && Math.floor(t * 4.6 + i * 3) % 7 === 0;
    rr(kx + i * kw + 2, ky, kw - 4, 170, 4, on ? C.orange : '#2b2833', on ? 'rgba(255,122,0,.18)' : '#0b0a0e', 1.2);
  }
  [0, 1, 3, 4, 5, 7, 8, 10, 11, 12, 14].forEach((i, j) => {
    const on = lit && Math.floor(t * 4.6 + j * 5) % 9 === 0;
    rr(kx + (i + 1) * kw - kw * 0.3, ky, kw * 0.6, 100, 3, on ? C.orange : '#3a3644', on ? '#3a1d05' : '#141218', 1);
  });
  T(lit ? '27 TECLAS: TOCAN LA PISTA ELEGIDA' : 'ESQUEMA DEL PANEL', bx + bw / 2, by + bh - 14, 16, 600, lit ? C.orange : C.dim, 'center');
};
Gr.screen = (A, t, f) => {
  const R = [['ENCABEZADO', 'reproducción · tempo · carga · batería', 0, 0, 240, 22], ['TARJETAS', 'una por perilla: KNOB 1–4', 0, 22, 240, 40],
             ['GRÁFICO', 'la criatura, la envolvente, los pasos…', 0, 62, 240, 152], ['PIE', 'la página y dónde estás', 0, 214, 240, 26]];
  const sel = A.cue === 0 ? [0] : A.cue === 1 ? [1] : A.cue === 2 ? [2, 3] : [];
  const mx = GX + 20, my = GY + 10, ms = 1.55;
  R.forEach((r, i) => {
    const on = sel.includes(i);
    rr(mx + r[2] * ms, my + r[3] * ms, r[4] * ms, r[5] * ms, 3, on ? C.orange : C.faint, on ? 'rgba(255,122,0,.07)' : null, on ? 2 : 1.2);
    T(r[0], mx + 400, my + (r[3] + r[5] / 2) * ms + 2, 20, 600, on ? C.text : C.dim);
    T(r[1], mx + 400, my + (r[3] + r[5] / 2) * ms + 26, 15, 500, on ? C.orange : C.faint);
    if (on) { mark(r[2], r[3], r[4], r[5], 0.5 + 0.5 * Math.sin(t * 4) ** 2); X.globalAlpha = .35; arrow(mx + 372, my + (r[3] + r[5] / 2) * ms - 6, SX - 24, SY + (r[3] + r[5] / 2) * 3, C.orange, 1, 7); X.globalAlpha = 1; }
  });
  if (A.cue === 1) { for (let k = 0; k < 4; k++) rr(mx + k * 60 * ms + 4, my + 22 * ms + 4, 60 * ms - 8, 40 * ms - 8, 3, k === 1 ? C.orange : C.faint, null, k === 1 ? 2 : 1); }
  if (A.cue === 3) {
    const y = GY + 430, a = fade(A.lt, 0, .5); X.globalAlpha = a;
    heatRule(GX, y, GW, 18, 0, 1);
    T('POCO', GX, y + 48, 18, 600, C.cyan); T('MUCHO', GX + GW, y + 48, 18, 600, C.text, 'right');
    rr(GX, y + 76, 26, 26, 4, null, C.violet); T('SELECCIONADO', GX + 38, y + 96, 18, 600, C.text);
    rr(GX + 300, y + 76, 26, 26, 4, null, C.orange); T('ACTIVO', GX + 338, y + 96, 18, 600, C.text);
    X.globalAlpha = 1;
  }
};
Gr.home = (A, t, f) => {
  for (let k = 0; k < 6; k++) {
    const col = k % 3, row = Math.floor(k / 3), x = GX + 140 + col * 290, y = GY + 130 + row * 270;
    const e = en(f, k), sel = A.cue === 2 && k === 1, muted = e < 0.01;
    sm[k] += (e - sm[k]) * 0.35;
    being(TRK[k][2], x, y, (sel ? 64 : 46) * (1 + sm[k] * 0.35), sm[k], t * 0.8 + k * 1.7, muted ? { style: 'dim' } : {});
    T('T' + (k + 1), x - 110, y - 96, 18, 600, sel ? C.orange : C.mid);
    T(TRK[k][0], x, y + 118, 19, 600, muted ? C.dim : C.text, 'center');
    T(muted ? 'MUTEADA' : TRK[k][1], x, y + 142, 15, 500, muted ? C.faint : C.dim, 'center');
  }
};
Gr.layers = (A, t, f) => {
  const half = GW / 2 - 20;
  const L = (x, title, on) => { X.globalAlpha = on ? 1 : .35; T(title, x, GY + 30, 30, 600, on ? C.text : C.mid); X.globalAlpha = 1; };
  L(GX, 'TOCAR', A.cue === 0); L(GX + half + 40, 'MANTENER', A.cue >= 1);
  { /* tap: a ripple, the pages flip */
    const on = A.cue === 0, ph = (t % 1.4) / 1.4; X.globalAlpha = on ? 1 : .35;
    rr(GX, GY + 70, 120, 56, 8, C.orange, 'rgba(255,122,0,.1)', 2); T('EDIT', GX + 60, GY + 106, 20, 600, C.text, 'center');
    if (on) { X.globalAlpha = (1 - ph) * .8; X.strokeStyle = C.orange; X.lineWidth = 2; X.beginPath(); X.roundRect(GX - ph * 30, GY + 70 - ph * 30, 120 + ph * 60, 56 + ph * 60, 12); X.stroke(); X.globalAlpha = 1; }
    const pages = ['EDIT 1', 'EDIT 2', 'FILTER', 'VOICE'], k = on ? Math.floor(t / 1.4) % 4 : 0;
    pages.forEach((p, i) => { const d = (i - k + 4) % 4; X.globalAlpha = (on ? 1 : .35) * (1 - d * .28); rr(GX + d * 18, GY + 170 + d * 18, 300, 190, 8, d ? C.faint : C.orange, '#07060a', d ? 1 : 2); if (!d) T(p, GX + 24, GY + 214, 24, 600, C.text); });
    X.globalAlpha = 1;
  }
  { /* hold: the bar fills, the map of the keys */
    const x0 = GX + half + 40, on = A.cue >= 1; X.globalAlpha = on ? 1 : .35;
    const fx = A.cue === 2 && A.lt > A.ld * 0.45, name = A.cue === 2 ? (fx ? 'FX' : 'GLO') : 'GLO';
    rr(x0, GY + 70, 120, 56, 8, C.violet, 'rgba(184,112,255,.1)', 2); T(name, x0 + 60, GY + 106, 20, 600, C.text, 'center');
    const hp = on ? clamp((A.lt % 6) / 0.6, 0, 1) : 0; rr(x0 + 140, GY + 92, 220, 10, 5, C.faint, null, 1); X.fillStyle = C.violet; X.fillRect(x0 + 140, GY + 92, 220 * hp, 10);
    T('0,4 s', x0 + 372, GY + 102, 14, 500, C.dim);
    const keys = fx ? ['RPT 1/8', 'RPT 1/16', 'RPT 1/32', 'LPF', 'HPF', 'OCT−', 'OCT+', '1/2 T'] : ['SOLO 1', 'SOLO 2', 'SOLO 3', 'SOLO 4', 'SOLO 5', 'SOLO 6', 'UNMUTE', 'TAP'];
    const blk = fx ? ['MUTE', 'MUTE', 'MUTE', 'MUTE', 'MUTE', 'MUTE'] : ['MUTE 1', 'MUTE 2', 'MUTE 3', 'MUTE 4', 'MUTE 5', 'MUTE 6'];
    const kw = (half - 10) / 8;
    keys.forEach((s, i) => {
      const lit = on && hp >= 1 && (fx ? Math.floor(t * 3) % 8 === i : false);
      rr(x0 + i * kw, GY + 250, kw - 6, 150, 4, lit ? C.orange : '#2b2833', lit ? 'rgba(255,122,0,.15)' : '#0b0a0e', 1.2);
      X.save(); X.translate(x0 + i * kw + kw / 2 - 3, GY + 390); X.rotate(-Math.PI / 2); T(s, 0, 5, 13, 600, hp >= 1 ? C.text : C.dim); X.restore();
    });
    [0, 1, 2, 3, 4, 5].forEach((i) => {
      const lit = on && hp >= 1 && !fx && Math.floor(t * 1.2) % 6 === i;
      rr(x0 + (i + 1) * kw - kw * 0.32 - 3, GY + 250, kw * 0.6, 84, 3, lit ? C.violet : '#3a3644', lit ? '#2a1640' : '#141218', 1);
      if (hp >= 1) T(blk[i].replace('MUTE ', ''), x0 + (i + 1) * kw - 3, GY + 238, 12, 600, lit ? C.violet : C.dim, 'center');
    });
    if (hp >= 1) T(fx ? 'cada tecla, un efecto momentáneo' : 'teclas negras: mutes · blancas: solos', x0, GY + 450, 18, 500, fx ? C.orange : C.violet);
    T('soltás: volvés a donde estabas', x0, GY + 480, 16, 500, C.dim);
    X.globalAlpha = 1;
  }
};
Gr.engines = (A, t, f) => {
  const E = [['ANALOG', 'dos osciladores, filtro por voz'], ['FM6', 'seis operadores, 32 algoritmos'], ['PHASE', 'distorsión de fase'],
             ['LOFI', 'chip · bytebeat · floatbeat'], ['VOICE', 'voz por formantes'], ['WHEEL', 'órgano con rotativo'],
             ['NOISE', 'ruido coloreado y metálico'], ['DRUM', 'DRUM-X: batería sintetizada']];
  const hi = [[0, 1, 2, 3, 4, 5, 6, 7], [0, 1, 2], [3, 4, 5], [6, 7], []][A.cue] || [];
  E.forEach((e, i) => {
    const col = i % 4, row = Math.floor(i / 4), x = GX + 105 + col * 216, y = GY + 110 + row * 250;
    const on = hi.includes(i), a = A.cue === 4 ? .55 : on ? 1 : .28;
    X.globalAlpha = a;
    const pulse = 0.35 + 0.35 * Math.max(0, Math.sin(t * TAU / (BEAT * 2) + i));
    being(e[0], x, y, on ? 46 : 38, on ? pulse : 0.15, t * 0.7 + i * 2.1);
    T(e[0] === 'DRUM' ? 'DRUM-X' : e[0], x, y + 104, 20, 600, on ? C.text : C.mid, 'center');
    if (on && A.cue) T(e[1], x, y + 128, 14, 500, C.orange, 'center');
    X.globalAlpha = 1;
  });
  if (A.cue === 4) {
    const a = fade(A.lt, 0, .5); X.globalAlpha = a;
    rr(GX, GY + 470, GW / 2 - 20, 80, 8, C.violet, 'rgba(184,112,255,.07)', 2);
    T('PRESETS', GX + 24, GY + 505, 22, 600, C.text); T('una lista con todos los motores', GX + 24, GY + 533, 16, 500, C.violet);
    rr(GX + GW / 2 + 20, GY + 470, GW / 2 - 20, 80, 8, C.orange, 'rgba(255,122,0,.07)', 2);
    T('EDIT + SELECT', GX + GW / 2 + 44, GY + 505, 22, 600, C.text); T('el sonido, a los dados', GX + GW / 2 + 44, GY + 533, 16, 500, C.orange);
    X.globalAlpha = 1;
  }
};
Gr.signal = (A, t, f) => {
  const y = GY + 230, nw = 128, nh = 66;
  const N = [['MOTOR', 'la voz', GX], ['FILTRO', 'LP HP BP COMB', GX + 172], ['DIST', '6 tipos · AMP', GX + 344], ['NIVEL', 'y PANEO', GX + 516]];
  const on = (i) => (A.cue === 1 && i <= 1) || (A.cue === 2 && i === 2) || (A.cue === 3 && i === 3) || A.cue === 0;
  N.forEach((n, i) => { node(n[2], y, nw, nh, n[0], on(i), n[1]); if (i) arrow(n[2] - 44, y + nh / 2, n[2] - 4, y + nh / 2, C.mid, 1.5, 7); });
  const ml = A.cue === 1 || A.cue === 0;
  node(GX + 172, y - 150, nw, 56, 'ENV · LFO', ml);
  X.setLineDash([5, 5]); arrow(GX + 236, y - 92, GX + 236, y - 6, ml ? C.violet : C.faint, 1.5, 7); X.setLineDash([]);
  if (ml) { X.strokeStyle = C.violet; X.lineWidth = 2; X.beginPath(); for (let i = 0; i <= 60; i++) { const xx = GX + 186 + i * 1.65, yy = y - 128 + 8 * Math.sin(i / 6 + t * 3); i ? X.lineTo(xx, yy) : X.moveTo(xx, yy); } X.stroke(); }
  const busOn = A.cue === 3 || A.cue === 0;
  ['CHORUS', 'DELAY', 'REVERB'].forEach((b, i) => {
    const by = y - 110 + i * 110; node(GX + 700, by + 8, 150, 50, b, busOn);
    X.globalAlpha = busOn ? 1 : .4; arrow(GX + 644, y + nh / 2, GX + 696, by + 33, busOn ? C.mid : C.faint, 1.2, 6); X.globalAlpha = 1;
  });
  T('ENVÍOS · COMPARTIDOS', GX + 775, y - 128, 14, 600, busOn ? C.orange : C.dim, 'center');
  const mOn = A.cue === 4 || A.cue === 0;
  node(GX + 300, y + 200, 300, 70, 'MASTER', mOn, 'limitador · CLIP · PUNCH · DUCK');
  X.globalAlpha = mOn ? 1 : .4; arrow(GX + 580, y + nh, GX + 500, y + 196, C.mid, 1.4, 7); arrow(GX + 775, y + 168, GX + 604, y + 228, C.mid, 1.4, 7); X.globalAlpha = 1;
  /* the sound travelling: a dot a beat along the chain */
  const ph = (t / BEAT) % 1, px = GX + nw / 2 + ph * 516, k = clamp(1 - ph, 0, 1);
  X.fillStyle = heat(80 + 170 * k); X.beginPath(); X.arc(px, y + nh / 2, 6 + 4 * k, 0, TAU); X.fill();
  if (A.cue === 2) { /* the curve of the distortion */
    X.strokeStyle = C.orange; X.lineWidth = 2; X.beginPath();
    for (let i = 0; i <= 80; i++) { const u = i / 40 - 1, v = Math.tanh(u * (2.5 + 1.5 * Math.sin(t))); const xx = GX + 344 + 14 + i * 1.25, yy = y + nh + 70 - v * 40; i ? X.lineTo(xx, yy) : X.moveTo(xx, yy); }
    X.stroke(); T('SOFT · HARD · FOLD · CRUSH · RECT · AMP', GX + 408, y + nh + 140, 14, 600, C.orange, 'center');
  }
};
const DRIVE = ['X...x...X...x.x.', '', '....X.......X...', 'x.xxx.xxx.xxx.xx', '..x.......x.....', '.............x.x', '...x..x....x..x.', '......x.......x.'];
Gr.drumx = (A, t, f) => {
  const cx = GX + 300, cy = GY + 250, R = 180, step = Math.floor(t / (BEAT / 4)) % 16, frac = (t / (BEAT / 4)) % 1;
  const names = ['BD', 'SD', 'CP', 'CH', 'OH', 'TM', 'RS', 'CB'], full = ['bombo', 'redoblante', 'clap', 'hat cerrado', 'hat abierto', 'tom', 'rim', 'cowbell'];
  const morph = A.cue === 1 ? 0.5 + 0.5 * Math.sin(A.lt * 0.9 - Math.PI / 2) : 0.2;
  names.forEach((n, i) => {
    const a = i / 8 * TAU - Math.PI / 2, x = cx + R * Math.cos(a), y = cy + R * Math.sin(a);
    const hit = DRIVE[i][step] && DRIVE[i][step] !== '.' && en(f, 0) > 0.02, e = hit ? 1 - frac : 0;
    const g = LANE[i][1], ff = { ...laneF(g, i), m: morph, lobB: g[3] + 2, spikesB: g[0] + 6, gB: gen(G.cil) };
    organism(X, g, ff, x, y, 30 * (1 + e * 0.4), e * 0.9, t + i, { ex: 4, nv: 64 });
    T(n, x, y + 62, 17, 600, hit && e > .5 ? C.orange : C.text, 'center');
    T(full[i], x, y + 80, 12, 500, C.dim, 'center');
  });
  T('8 SONIDOS', cx, cy + 6, 22, 600, C.mid, 'center'); T('sin samples', cx, cy + 30, 14, 500, C.dim, 'center');
  const sx0 = GX + 600, sy0 = GY + 120;
  X.globalAlpha = A.cue >= 1 ? 1 : .35;
  T('MORPH', sx0 + 120, sy0 - 10, 22, 600, C.text, 'center');
  rr(sx0, sy0 + 20, 240, 8, 4, C.faint, null, 1); heatRule(sx0, sy0 + 20, 240 * morph, 8, 0, 0.8);
  X.fillStyle = C.orange; X.beginPath(); X.arc(sx0 + 240 * morph, sy0 + 24, 11, 0, TAU); X.fill();
  T('A', sx0 - 20, sy0 + 31, 22, 600, C.text, 'center'); T('B', sx0 + 260, sy0 + 31, 22, 600, C.text, 'center');
  T('todo el kit, de un lado al otro', sx0 + 120, sy0 + 70, 15, 500, C.dim, 'center');
  X.globalAlpha = A.cue === 2 ? 1 : .3;
  const kits = ['DRUM-X', '909 CLUB', '808 BOOM', 'LOFI DUST', 'INDUSTRIAL', 'ACOUSTIC', 'DUB ROOTS', 'TECHNO RAW', 'BREAKS'];
  kits.forEach((k, i) => { const on = A.cue === 2 && Math.floor(A.lt * 1.6) % 9 === i; rr(sx0, sy0 + 120 + i * 36, 240, 30, 4, on ? C.orange : C.faint, on ? 'rgba(255,122,0,.1)' : null, on ? 2 : 1); T(k, sx0 + 14, sy0 + 141 + i * 36, 15, 600, on ? C.text : C.mid); });
  X.globalAlpha = 1;
};
Gr.seq = (A, t, f) => {
  const s16 = t / (BEAT / 4);
  if (A.cue === 0 || A.cue === 2) {
    const lens = [16, 12, 10], cols = [heat(30), heat(110), heat(190)];
    lens.forEach((L, r) => {
      const y = GY + 60 + r * 130, cw = GW / 16, pos = Math.floor(s16) % L;
      T('LEN ' + L, GX, y - 14, 16, 600, C.mid);
      for (let i = 0; i < 16; i++) {
        const inL = i < L, hit = inL && (i * (r + 3)) % 5 < 2, cur = i === pos;
        rr(GX + i * cw + 3, y, cw - 6, 70, 4, cur ? C.orange : inL ? '#2b2833' : '#121016', hit ? (cur ? C.orange : cols[r]) : null, cur ? 2 : 1);
      }
    });
    T(A.cue === 2 ? 'REC + PLAY: GRABÁS EN VIVO' : 'CADA PISTA, SU LARGO: GIRAN UNAS CONTRA OTRAS', GX, GY + 480, 20, 600, A.cue === 2 ? C.red : C.orange);
    if (A.cue === 2 && (t * 2) % 1 < .5) { X.fillStyle = C.red; X.beginPath(); X.arc(GX + GW - 20, GY + 473, 10, 0, TAU); X.fill(); }
  } else if (A.cue === 1) {
    const rows = 14, cw = GW / 16, rh = 30, y0 = GY + 40, pos = Math.floor(s16) % 16;
    for (let r = 0; r < rows; r++) { X.fillStyle = r % 2 ? '#0a090d' : '#0e0d12'; X.fillRect(GX, y0 + r * rh, GW, rh); }
    const notes = [[0, 3, 9], [4, 2, 6], [6, 2, 5], [8, 3, 6], [12, 4, 9], [12, 4, 5], [12, 4, 2]];
    notes.forEach(([s, l, r]) => { const on = pos >= s && pos < s + l; rr(GX + s * cw + 2, y0 + r * rh + 3, l * cw - 4, rh - 6, 4, null, on ? C.orange : heat(60 + r * 10)); });
    X.fillStyle = C.text; X.fillRect(GX + pos * cw, y0, 2, rows * rh);
    T('ACC', GX + 2, y0 + rows * rh + 28, 14, 600, C.violet); T('SLD', GX + 6 * cw, y0 + rows * rh + 28, 14, 600, C.violet);
    T('PIANO ROLL · HASTA 4 NOTAS POR PASO', GX, GY + 520, 20, 600, C.orange);
  } else if (A.cue === 3) {
    const cw = GW / 16, y = GY + 150, pos = Math.floor(s16) % 16, loop = Math.floor(s16 / 16);
    const comp = { 2: 'PROB 50%', 5: 'RATCH ×3', 8: 'ECHO', 11: 'ARP', 14: 'WALK ↑' };
    for (let i = 0; i < 16; i++) {
      const cur = i === pos; rr(GX + i * cw + 3, y, cw - 6, 70, 4, cur ? C.orange : '#2b2833', i % 3 === 2 ? heat(120) : null, cur ? 2 : 1);
      if (comp[i]) { X.save(); X.translate(GX + i * cw + cw / 2, y - 16); T(comp[i], 0, 0, 14, 600, C.violet, 'center'); X.restore(); arrow(GX + i * cw + cw / 2, y - 10, GX + i * cw + cw / 2, y - 2, C.violet, 1, 5); }
    }
    const wn = loop % 5; T('vuelta ' + (loop % 5 + 1), GX + 14 * cw - 20, y + 120, 14, 500, C.dim);
    for (let k = 0; k < 5; k++) rr(GX + 14 * cw + 6, y + 250 - k * 26, cw - 12, 20, 3, null, k === wn ? C.orange : '#1d1a24');
    T('COMPONENTES POR PASO', GX, GY + 520, 20, 600, C.orange);
  } else {
    const cw = GW / 16, y = GY + 120;
    for (let i = 0; i < 16; i++) rr(GX + i * cw + 3, y, cw - 6, 70, 4, '#2b2833', i % 4 === 0 ? heat(90) : i % 3 === 1 ? heat(160) : null, 1);
    const heads = [[1, C.orange, 'x1'], [0.5, C.violet, 'x½'], [-1, C.cyan, '◀'], [2, C.yellow, 'x2']];
    heads.forEach(([r, col, lb], k) => { const p = ((s16 * r) % 16 + 16) % 16; X.fillStyle = col; X.fillRect(GX + p * cw, y - 10 - k * 8, cw, 4); T(lb, GX + GW + 8, y - 4 - k * 8, 12, 600, col); });
    T('PLAYHEADS: CABEZALES EXTRA', GX, y + 140, 20, 600, C.orange);
    const m = (t / (BEAT * 16)) % 1; being('LOFI', GX + 120, GY + 420, 50, .3 + .4 * m, t, { tum: [] });
    T('MUTATE: CADA TANTOS COMPASES', GX + 220, GY + 430, 20, 600, C.violet);
  }
};
Gr.mod = (A, t, f) => {
  const S = ['ENV', 'LFO', 'VELOCIDAD', 'MSEQ', 'RUIDO'], Dd = ['AFINACIÓN', 'FILTRO', 'FORMA', 'AMPLITUD', 'DELAY', 'REVERB'];
  if (A.cue < 2) {
    const routes = A.cue === 0 ? [[0, 1], [1, 0], [1, 1], [0, 3], [1, 2]] : [[0, 1], [1, 0], [2, 3], [3, 4], [4, 5], [1, 2]];
    S.forEach((s, i) => { const on = routes.some((r) => r[0] === i); node(GX, GY + 30 + i * 100, 190, 60, s, on); });
    Dd.forEach((d, i) => { const on = routes.some((r) => r[1] === i); node(GX + GW - 210, GY + 10 + i * 88, 210, 56, d, on, i >= 4 ? 'efectos' : ''); });
    routes.forEach(([a, b], k) => {
      const x1 = GX + 190, y1 = GY + 60 + a * 100, x2 = GX + GW - 214, y2 = GY + 38 + b * 88;
      X.strokeStyle = b >= 4 ? C.violet : heat(60 + k * 30); X.lineWidth = 1.8; X.beginPath(); X.moveTo(x1, y1); X.bezierCurveTo(x1 + 200, y1, x2 - 200, y2, x2, y2); X.stroke();
      const u = (t * 0.6 + k * 0.23) % 1, bx = (1 - u) ** 3 * x1 + 3 * (1 - u) ** 2 * u * (x1 + 200) + 3 * (1 - u) * u * u * (x2 - 200) + u ** 3 * x2;
      const by = (1 - u) ** 3 * y1 + 3 * (1 - u) ** 2 * u * y1 + 3 * (1 - u) * u * u * y2 + u ** 3 * y2;
      X.fillStyle = C.text; X.beginPath(); X.arc(bx, by, 4, 0, TAU); X.fill();
    });
    T(A.cue === 0 ? 'ENV Y LFO EN CADA PISTA' : 'MATRIZ MOD: 4 RUTAS MÁS POR PISTA', GX, GY + 550, 20, 600, C.orange);
  } else {
    const P = ['CUT', 'RES', 'DECAY', 'DLY', 'REV', 'DRIVE', 'PAN', 'SPEED', 'FOLD', 'TONE', 'MIX', 'WIDE'];
    for (let m = 0; m < 4; m++) {
      const x = GX + 100 + m * 220, y = GY + 140, v = 0.5 + 0.45 * Math.sin(t * (0.7 + m * 0.3) + m);
      X.lineWidth = 3; X.strokeStyle = C.orange; X.beginPath(); X.arc(x, y, 44, 0, TAU); X.stroke();
      const ang = Math.PI * 0.75 + v * Math.PI * 1.5; X.beginPath(); X.moveTo(x, y); X.lineTo(x + 38 * Math.cos(ang), y + 38 * Math.sin(ang)); X.stroke();
      T('M' + (m + 1), x, y + 80, 22, 600, C.text, 'center');
      for (let j = 0; j < 3; j++) {
        const px = x - 70 + j * 70, py = y + 270; X.globalAlpha = .4 + .6 * v; X.strokeStyle = heat(60 + m * 50); X.lineWidth = 1.5;
        X.beginPath(); X.moveTo(x, y + 96); X.lineTo(px, py - 26); X.stroke(); X.globalAlpha = 1;
        rr(px - 32, py - 24, 64, 30, 4, C.faint, null, 1); T(P[m * 3 + j], px, py - 3, 13, 600, C.mid, 'center');
      }
    }
    T('LFO MANTENIDO: LAS PERILLAS SON M1–M4', GX, GY + 550, 20, 600, C.orange);
  }
};
Gr.fx = (A, t, f) => {
  const box = (i, title, on) => { const x = GX + (i % 2) * (GW / 2 + 10), y = GY + Math.floor(i / 2) * 270; X.globalAlpha = on ? 1 : .3; rr(x, y, GW / 2 - 10, 250, 8, on ? C.orange : C.faint, '#07060a', on ? 2 : 1.2); T(title, x + 20, y + 36, 22, 600, C.text); return [x, y]; };
  if (A.cue <= 1) {
    let [x, y] = box(0, 'DELAY', A.cue === 0);
    for (let k = 0; k < 7; k++) { const ph = ((t / (BEAT * 0.75)) - k) ; const a = Math.max(0, 1 - k * 0.15); X.fillStyle = heat(220 - k * 28, a); X.beginPath(); X.arc(x + 50 + k * 56, y + 140, 14 * a + 3 * Math.max(0, 1 - (ph % 1)), 0, TAU); X.fill(); }
    T('analógico o digital · pitch · ping pong', x + 20, y + 225, 14, 500, C.dim);
    [x, y] = box(1, 'REVERB', A.cue === 0);
    for (let k = 0; k < 60; k++) { const hx = hash(k * 7), hy = hash(k * 13 + 5), d = (t * 0.1 + hx) % 1; X.fillStyle = heat(30 + hy * 180, 0.6 * (1 - d)); X.beginPath(); X.arc(x + 30 + hx * 360, y + 70 + hy * 110 - d * 20, 2 + 3 * d, 0, TAU); X.fill(); }
    ['ROOM', 'SPRING', 'SHIMMER', 'RESO', 'CLOUD'].forEach((m, i) => { const on = A.cue === 0 && Math.floor(A.lt * 1.2) % 5 === i; rr(x + 18 + i * 82, y + 196, 76, 30, 4, on ? C.orange : C.faint, null, on ? 2 : 1); T(m, x + 56 + i * 82, y + 217, 12, 600, on ? C.text : C.mid, 'center'); });
    [x, y] = box(2, 'CHORUS', A.cue === 1);
    for (let l = 0; l < 2; l++) { X.strokeStyle = l ? C.violet : C.cyan; X.lineWidth = 2; X.beginPath(); for (let i = 0; i <= 100; i++) { const xx = x + 20 + i * 3.8, yy = y + 140 + 40 * Math.sin(i / 9 + t * 2 + l * (1 + Math.sin(t))); i ? X.lineTo(xx, yy) : X.moveTo(xx, yy); } X.stroke(); }
    [x, y] = box(3, 'SLICER', A.cue === 1);
    const pat = [1, 0, 1, 1, 0, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1], st = Math.floor(t / (BEAT / 4)) % 16;
    pat.forEach((p, i) => rr(x + 20 + i * 24, y + 90, 18, 100, 3, i === st ? C.orange : C.faint, p ? (i === st ? C.orange : heat(130)) : null, 1));
    X.globalAlpha = 1;
  } else {
    const keys = ['RPT 1/8', 'RPT 1/16', 'RPT 1/32', 'LPF', 'HPF', 'OCT−', 'OCT+', '1/2 T', 'DEC−', 'DEC+', 'STUT 8', 'STUT 16'];
    const kw = GW / keys.length, playing = A.cue === 2 && A.lt > 3 && A.lt < 7;
    keys.forEach((k, i) => {
      const lit = (playing && i === 1 && (A.lt % 1) < .5) || (A.cue === 3 && Math.floor(t * 2) % 12 === i && i % 3 === 0);
      rr(GX + i * kw + 2, GY + 40, kw - 4, 200, 4, lit ? C.orange : '#2b2833', lit ? 'rgba(255,122,0,.2)' : '#0b0a0e', 1.2);
      X.save(); X.translate(GX + i * kw + kw / 2 + 5, GY + 230); X.rotate(-Math.PI / 2); T(k, 0, 0, 14, 600, lit ? C.text : C.mid); X.restore();
    });
    T('FX MANTENIDO: LAS TECLAS SON EFECTOS', GX, GY + 290, 20, 600, C.orange);
    if (A.cue === 3) {
      const cx = GX + 650, cy = GY + 420, r = 100, p = (t / (BEAT * 16)) % 1;
      for (let i = 0; i < 16; i++) { const a = i / 16 * TAU - Math.PI / 2; X.fillStyle = [6, 7, 12].includes(i) ? C.orange : '#1d1a24'; X.beginPath(); X.arc(cx + r * Math.cos(a), cy + r * Math.sin(a), 9, 0, TAU); X.fill(); }
      const a = p * TAU - Math.PI / 2; X.strokeStyle = C.text; X.lineWidth = 2; X.beginPath(); X.moveTo(cx, cy); X.lineTo(cx + (r - 18) * Math.cos(a), cy + (r - 18) * Math.sin(a)); X.stroke();
      T('EL CARRIL DE PUNCH-IN', GX, GY + 400, 22, 600, C.text); T('lo que tocaste se repite con el tema', GX, GY + 432, 16, 500, C.dim);
    }
  }
};
Gr.gen = (A, t, f) => {
  if (A.cue === 0) {
    const gs = ['ELECTRO', 'EBM', 'ITALO', 'SYNTHPOP', 'DARKWAVE', 'POST PUNK', 'MOTORIK', 'SHOEGAZE', 'DISCO', 'TRAP'];
    const k = Math.floor(A.lt * 3) % gs.length, stop = A.lt > A.ld * 0.35;
    gs.forEach((g, i) => { const on = (stop ? 5 : k) === i; rr(GX + (i % 5) * 172, GY + 20 + Math.floor(i / 5) * 50, 162, 40, 4, on ? C.orange : C.faint, on ? 'rgba(255,122,0,.12)' : null, on ? 2 : 1); T(g, GX + (i % 5) * 172 + 81, GY + 46 + Math.floor(i / 5) * 50, 14, 600, on ? C.text : C.mid, 'center'); });
    if (stop) {
      arrow(GX + GW / 2, GY + 140, GX + GW / 2, GY + 190, C.orange, 2, 9); T('OCT+', GX + GW / 2 + 20, GY + 172, 16, 600, C.orange);
      ['MOTORES', 'SONIDOS', 'PARTES', 'ESCALA', 'TEMPO', 'EFECTOS'].forEach((c, i) => { const a = fade(A.lt, A.ld * 0.4 + i * 0.35, A.ld * 0.4 + i * 0.35 + 0.3); X.globalAlpha = a; node(GX + (i % 3) * 290, GY + 220 + Math.floor(i / 3) * 100, 270, 70, c, true); X.globalAlpha = 1; });
    }
  } else if (A.cue === 1) {
    ['SEQ', 'LFO', 'FX'].forEach((p, i) => {
      const x = GX + 80 + i * 280, y = GY + 160, a = t * 3 + i * 1.3, s = 60;
      X.save(); X.translate(x, y); X.rotate(Math.sin(a) * 0.4); rr(-s / 2, -s / 2, s, s, 10, C.orange, 'rgba(255,122,0,.08)', 2);
      const v = (Math.floor(t * 4 + i * 2) % 6) + 1, P = { 1: [[0, 0]], 2: [[-1, -1], [1, 1]], 3: [[-1, -1], [0, 0], [1, 1]], 4: [[-1, -1], [1, -1], [-1, 1], [1, 1]], 5: [[-1, -1], [1, -1], [0, 0], [-1, 1], [1, 1]], 6: [[-1, -1], [1, -1], [-1, 0], [1, 0], [-1, 1], [1, 1]] };
      P[v].forEach(([dx, dy]) => { X.fillStyle = C.text; X.beginPath(); X.arc(dx * 15, dy * 15, 5, 0, TAU); X.fill(); }); X.restore();
      T(p + ' + SELECT', x, y + 80, 20, 600, C.text, 'center');
    });
    T('LOS DADOS, PARTE POR PARTE', GX, GY + 400, 22, 600, C.orange); T('con OK para quedártelo o CANCEL para volver', GX, GY + 432, 16, 500, C.dim);
  } else {
    const cx = GX + 200, cy = GY + 220, a0 = -Math.PI * 0.2, a1 = a0 + Math.PI * 1.6 * ease((A.lt % 2.4) / 1.6);
    X.strokeStyle = C.violet; X.lineWidth = 6; X.beginPath(); X.arc(cx, cy, 90, a0, a1); X.stroke();
    arrow(cx + 90 * Math.cos(a1), cy + 90 * Math.sin(a1), cx + 90 * Math.cos(a1 + .2), cy + 90 * Math.sin(a1 + .2), C.violet, 6, 18);
    T('DESHACER', GX + 380, GY + 210, 40, 600, C.text); T('GLO mantenido + G4 · otra vez: rehacer', GX + 380, GY + 250, 18, 500, C.violet);
  }
};
Gr.arrange = (A, t, f) => {
  if (A.cue === 0) {
    T('PROYECTO', GX, GY + 30, 18, 600, C.mid);
    for (let p = 0; p < 8; p++) rr(GX + 120 + p * 58, GY + 8, 46, 32, 4, p === 0 ? C.orange : C.faint, p === 0 ? 'rgba(255,122,0,.12)' : null, p === 0 ? 2 : 1), T(String(p + 1), GX + 143 + p * 58, GY + 30, 16, 600, p === 0 ? C.text : C.dim, 'center');
    ['A', 'B', 'C', 'D'].forEach((s, i) => {
      const x = GX + i * 218, y = GY + 90, full = i < 2 && A.lt > i * 2.5;
      rr(x, y, 200, 260, 8, full ? C.orange : C.faint, '#07060a', full ? 2 : 1.2); T(s, x + 100, y + 70, 60, 600, full ? C.text : C.faint, 'center');
      if (full) for (let r = 0; r < 6; r++) for (let c = 0; c < 8; c++) if (hash(i * 97 + r * 13 + c) > .55) rr(x + 24 + c * 20, y + 110 + r * 22, 14, 14, 2, null, heat(40 + r * 35));
      T(i === 0 ? 'una idea' : i === 1 ? 'su variación' : '', x + 100, y + 240, 15, 500, C.dim, 'center');
    });
  } else if (A.cue === 1) {
    const rows = [['A', 8], ['B', 8], ['A', 4], ['B', 8]], tot = 28, wpx = GW / tot, p = ((t - TL.chapters[A.ci].lines[1].start) / (BEAT * 4)) % tot;
    let x = GX;
    rows.forEach(([s, b], i) => { const on = p >= (x - GX) / wpx && p < (x - GX) / wpx + b; rr(x + 2, GY + 120, b * wpx - 4, 140, 6, on ? C.orange : C.faint, on ? 'rgba(255,122,0,.1)' : '#07060a', on ? 2 : 1.2); T(s, x + b * wpx / 2, GY + 205, 44, 600, on ? C.text : C.mid, 'center'); T(b + ' compases', x + b * wpx / 2, GY + 290, 14, 500, C.dim, 'center'); x += b * wpx; });
    X.fillStyle = C.text; X.fillRect(GX + p * wpx, GY + 100, 2, 180);
    T('ARRANGE: SECCIONES EN FILAS', GX, GY + 380, 22, 600, C.orange);
  } else {
    const kw = GW / 8;
    ['A', 'B', 'C', 'D', 'STORE A', 'STORE B', 'STORE C', 'STORE D'].forEach((k, i) => {
      const lit = i < 4 && Math.floor(A.lt / 1.5) % 4 === i;
      rr(GX + i * kw + 2, GY + 80, kw - 4, 220, 4, lit ? C.orange : '#2b2833', lit ? 'rgba(255,122,0,.2)' : '#0b0a0e', 1.2);
      T(k, GX + i * kw + kw / 2, GY + 280, i < 4 ? 26 : 13, 600, lit ? C.text : C.mid, 'center');
    });
    T('SAVE MANTENIDO: LAS SECCIONES EN VIVO', GX, GY + 360, 22, 600, C.orange); T('entran en el compás siguiente', GX, GY + 392, 16, 500, C.dim);
  }
};
Gr.usb = (A, t, f) => {
  const fmx = GX, fmy = GY + 180;
  rr(fmx, fmy, 200, 120, 10, C.text, '#07060a', 2); T('FM-1', fmx + 100, fmy + 70, 30, 600, C.text, 'center');
  if (A.cue <= 1) {
    const n = A.cue === 1 ? 10 : 8, lab = ['T1', 'T2', 'T3', 'T4', 'T5', 'T6', 'FX L', 'FX R', 'MIC', 'MIC'];
    const lx = GX + 360, lw = GW - 360;
    rr(lx, GY + 40, lw, 400, 10, C.faint, '#07060a', 1.2); T(A.cue === 1 ? 'EL CELULAR · 10 CANALES' : 'TU DAW · 8 CANALES', lx + 20, GY + 74, 18, 600, C.mid);
    arrow(fmx + 210, fmy + 50, lx - 10, fmy + 50, C.orange, 2, 9);
    for (let i = 0; i < n; i++) {
      const v = i < 6 ? en(f, i) : i < 8 ? clamp(D.rms[f] * 0.8, 0, 1) : A.cue === 1 ? clamp(0.3 + 0.5 * Math.abs(Math.sin(t * 5.3) * Math.sin(t * 1.7)), 0, 1) : 0;
      const bw2 = (lw - 40) / n, x = lx + 20 + i * bw2, segs = 16, lit = Math.round(clamp(v, 0, 1) * segs);
      for (let s = 0; s < segs; s++) rr(x + 4, GY + 400 - s * 18, bw2 - 8, 13, 2, null, s < lit ? heat(30 + s * 13) : '#151319');
      T(lab[i], x + bw2 / 2, GY + 428, 13, 600, i >= 8 ? C.violet : C.mid, 'center');
    }
    if (A.cue === 1) {
      const px = fmx + 40, py = fmy + 220; rr(px, py, 70, 130, 12, C.violet, null, 2); T('MIC', px + 35, py + 72, 14, 600, C.violet, 'center');
      for (let k = 0; k < 3; k++) { X.globalAlpha = 1 - ((t * 1.5 + k / 3) % 1); X.strokeStyle = C.violet; X.beginPath(); X.arc(px + 35, py - 10, 14 + ((t * 1.5 + k / 3) % 1) * 30, Math.PI * 1.15, Math.PI * 1.85); X.stroke(); X.globalAlpha = 1; }
      arrow(px + 80, py + 40, fmx + 150, fmy + 126, C.violet, 2, 9);
      T('STEMS+IN · NUEVO EN 0.8.0', lx, GY + 500, 20, 600, C.violet); T('tu voz por los efectos de la última pista, en su propio canal', lx, GY + 530, 15, 500, C.dim);
    } else T('USB › STEMS · las 6 pistas por separado + los efectos', GX + 360, GY + 500, 18, 600, C.orange);
  } else {
    ['USB', 'BLUETOOTH', 'TRS'].forEach((m, i) => { node(GX + 300 + i * 190, GY + 200, 170, 70, m, true, 'MIDI'); arrow(fmx + 210, fmy + 60, GX + 296 + i * 190, GY + 235, C.mid, 1.2, 6); });
    const ph = (t / BEAT) % 1; X.fillStyle = heat(250 * (1 - ph)); X.beginPath(); X.arc(GX + 300, GY + 380, 12 + 8 * (1 - ph), 0, TAU); X.fill();
    T('CLOCK: EN TEMPO CON OTROS EQUIPOS', GX + 330, GY + 388, 20, 600, C.orange);
  }
};
Gr.web = (A, t, f) => {
  const C3 = [['EMULADOR', 'JIANT en el navegador'], ['INSTALADOR', 'Chrome o Edge, por USB'], ['EDITOR', 'sonidos, bancos y backups']];
  C3.forEach((c, i) => { const on = A.cue === 0, a = fade(A.ct, 0.3 + i * 0.5, 0.8 + i * 0.5); X.globalAlpha = a * (on ? 1 : .35); rr(GX, GY + i * 120, GW, 100, 8, on ? C.orange : C.faint, '#07060a', on ? 2 : 1.2); T(c[0], GX + 30, GY + i * 120 + 48, 28, 600, C.text); T(c[1], GX + 30, GY + i * 120 + 80, 17, 500, C.dim); X.globalAlpha = 1; });
  T('juanjiant-bit.github.io/JIANT', GX, GY + 400, 24, 600, C.violet);
  if (A.cue === 1) {
    const zx = GX + 560, zy = GY + 380; rr(zx, zy, 200, 150, 10, C.orange, 'rgba(255,122,0,.07)', 2); T('STEMS.ZIP', zx + 100, zy + 136, 16, 600, C.orange, 'center');
    ['T1', 'T2', 'T3', 'T4', 'T5', 'T6', 'MIX'].forEach((s, i) => { const u = clamp((A.lt - i * 0.4) / 0.6, 0, 1); if (u <= 0) return; const y = zy - 80 + ease(u) * 120; X.globalAlpha = u < 1 ? 1 : .7; rr(zx + 14 + (i % 4) * 46, y + Math.floor(i / 4) * 10, 40, 26, 3, null, heat(40 + i * 30)); T(s, zx + 34 + (i % 4) * 46, y + 18 + Math.floor(i / 4) * 10, 11, 600, '#000', 'center'); X.globalAlpha = 1; });
  }
};

/* ---- the lab film (news_script_en.py): stems into a DAW, the web editor, the voice through the FM-1, the app */
function win(x, y, w, h, title) { rr(x, y, w, h, 10, '#2b2833', '#07060a', 1.5); X.fillStyle = '#121017'; X.fillRect(x + 1, y + 1, w - 2, 30); [0, 1, 2].forEach((i) => { X.fillStyle = ['#ff5f57', '#febc2e', '#28c840'][i]; X.beginPath(); X.arc(x + 18 + i * 18, y + 16, 5, 0, TAU); X.fill(); }); T(title, x + 80, y + 21, 14, 500, C.dim); }
function lane(x, y, w, h, k, f, lab, col) {
  rr(x, y, w, h, 4, null, '#0d0c11'); T(lab, x + 10, y + h / 2 + 5, 14, 600, C.mid);
  for (let i = 0; i < 120; i++) { const ff = Math.max(0, f - (119 - i) * 2), v = k < 6 ? en(ff, k) : clamp(D.rms[Math.min(ff, D.n - 1)] * .7, 0, 1); const a = v * (h / 2 - 4); X.fillStyle = col || heat(40 + v * 200); X.fillRect(x + 70 + i * (w - 80) / 120, y + h / 2 - a, Math.max(1, (w - 80) / 120 - 1), a * 2); }
}
Gr.stems = (A, t, f) => {
  if (A.cue < 2) {
    rr(GX, GY + 200, 150, 90, 10, C.text, '#07060a', 2); T('FM-1', GX + 75, GY + 255, 26, 600, C.text, 'center');
    T('USB › STEMS', GX + 75, GY + 320, 15, 600, C.orange, 'center');
    arrow(GX + 156, GY + 245, GX + 200, GY + 245, C.orange, 2, 9);
    win(GX + 210, GY, GW - 210, 520, A.cue === 1 ? 'Ableton · Logic · Reaper · any DAW' : 'your DAW · 8 inputs');
    const names = ['T1 DRUMS', 'T2 BASS', 'T3 PAD', 'T4 LEAD', 'T5 ARP', 'T6 METAL', 'FX L', 'FX R'];
    names.forEach((n, i) => lane(GX + 220, GY + 42 + i * 59, GW - 230, 52, i, f, n.split(' ')[0], i >= 6 ? C.violet : null));
  } else {
    win(GX, GY, GW, 300, 'juanjiant-bit.github.io/JIANT · emulator');
    T('BACKUP → STEMS', GX + 30, GY + 90, 28, 600, C.text); T('the same channels, rendered faster than real time', GX + 30, GY + 124, 17, 500, C.dim);
    const p = clamp(A.lt / (A.ld * .8), 0, 1); rr(GX + 30, GY + 160, GW - 60, 12, 6, C.faint, null, 1); heatRule(GX + 30, GY + 160, (GW - 60) * p, 12, 0, p * .9);
    T(p >= 1 ? 'RENDERED · STEMS.ZIP' : 'RENDERING ' + Math.round(p * 100) + ' %', GX + 30, GY + 210, 16, 600, p >= 1 ? C.orange : C.mid);
    ['T1', 'T2', 'T3', 'T4', 'T5', 'T6', 'MIX'].forEach((s2, i) => { const u = clamp((A.lt - i * .35) / .5, 0, 1); if (!u) return; X.globalAlpha = u; rr(GX + 30 + i * 118, GY + 360, 104, 130, 8, C.faint, '#07060a', 1.2); T(s2 + '.WAV', GX + 82 + i * 118, GY + 440, 15, 600, C.text, 'center'); X.globalAlpha = 1; });
  }
};
Gr.editor = (A, t, f) => {
  win(GX, GY, GW, 500, 'juanjiant-bit.github.io/JIANT/webapp/editor');
  const tabs = ['SOUND', 'FM6 BANK', 'PRESETS', 'BACKUP', 'MENU'], act = A.cue === 1 ? 1 + (Math.floor(A.lt / 3) % 3) : 0;
  tabs.forEach((n, i) => { rr(GX + 16, GY + 50 + i * 46, 150, 38, 5, i === act ? C.orange : C.faint, i === act ? 'rgba(255,122,0,.1)' : null, i === act ? 2 : 1); T(n, GX + 30, GY + 75 + i * 46, 14, 600, i === act ? C.text : C.mid); });
  const mx = GX + 190, my = GY + 50;
  if (act === 0) {
    for (let i = 0; i < 8; i++) {
      const x = mx + (i % 4) * 160 + 60, y = my + 70 + Math.floor(i / 4) * 190, v = .5 + .4 * Math.sin(t * (.4 + i * .13) + i);
      X.lineWidth = 2.5; X.strokeStyle = i === Math.floor(t) % 8 ? C.orange : C.mid; X.beginPath(); X.arc(x, y, 40, Math.PI * .75, Math.PI * (.75 + 1.5 * v)); X.stroke();
      X.strokeStyle = C.faint; X.lineWidth = 1; X.beginPath(); X.arc(x, y, 40, Math.PI * (.75 + 1.5 * v), Math.PI * 2.25); X.stroke();
      T(['CUT', 'RES', 'ATK', 'REL', 'WAVE', 'DETUNE', 'MIX', 'NOISE'][i], x, y + 72, 14, 600, C.text, 'center'); T(String(Math.round(v * 127)), x, y + 8, 18, 600, C.text, 'center');
    }
  } else if (act === 1) {
    for (let i = 0; i < 9; i++) { const on = Math.floor(A.lt * 2) % 9 === i; rr(mx, my + i * 46, GW - 220, 38, 4, on ? C.orange : C.faint, on ? 'rgba(255,122,0,.08)' : null, on ? 2 : 1); T('B' + (i + 1) + '  ' + ['E.PIANO 1', 'BRASS 1', 'STRINGS', 'MARIMBA', 'TUB BELLS', 'HARPSICH', 'BASS 1', 'FLUTE', 'CLAV'][i], mx + 14, my + 25 + i * 46, 15, 600, on ? C.text : C.mid); }
    T('DX7 BANK → FM6', mx + GW - 400, my + 450, 15, 600, C.violet);
  } else {
    T(act === 2 ? 'USER PRESETS U01–U32' : 'FULL BACKUP', mx, my + 30, 22, 600, C.text);
    const p = (A.lt % 4) / 4; rr(mx, my + 60, GW - 220, 12, 6, C.faint, null, 1); heatRule(mx, my + 60, (GW - 220) * p, 12, 0, p);
    for (let i = 0; i < 8; i++) { const done = i < p * 8; rr(mx + (i % 4) * 150, my + 110 + Math.floor(i / 4) * 120, 136, 100, 6, done ? C.orange : C.faint, '#07060a', done ? 2 : 1); T((act === 2 ? 'U0' : 'PROJECT ') + (i + 1), mx + 68 + (i % 4) * 150, my + 165 + Math.floor(i / 4) * 120, 15, 600, done ? C.text : C.dim, 'center'); }
  }
  if (A.cue === 2) { const ph = (t * 1.5) % 1; T('USB MIDI · LIVE', GX + GW - 20, GY + 540, 18, 600, C.orange, 'right'); X.fillStyle = C.orange; X.beginPath(); X.arc(GX + GW - 230 - ph * 300, GY + 535, 5, 0, TAU); X.fill(); X.fillStyle = C.violet; X.beginPath(); X.arc(GX + GW - 530 + ph * 300, GY + 535, 5, 0, TAU); X.fill(); }
};
Gr.micin = (A, t, f) => {
  if (A.cue < 3) {
    const py = GY + 140; rr(GX, py, 90, 170, 14, C.violet, null, 2); T('MIC', GX + 45, py + 92, 16, 600, C.violet, 'center');
    for (let k = 0; k < 3; k++) { const u = (t * 1.4 + k / 3) % 1; X.globalAlpha = 1 - u; X.strokeStyle = C.violet; X.lineWidth = 2; X.beginPath(); X.arc(GX + 45, py - 12, 14 + u * 34, Math.PI * 1.15, Math.PI * 1.85); X.stroke(); X.globalAlpha = 1; }
    arrow(GX + 98, py + 85, GX + 150, py + 85, C.violet, 2, 9);
    node(GX + 160, py + 50, 150, 70, 'T6', A.cue === 1, 'LEVEL · PAN');
    ['CHORUS', 'DELAY', 'REVERB'].forEach((b, i) => { node(GX + 380, py - 60 + i * 95, 150, 56, b, A.cue === 1); X.globalAlpha = A.cue === 1 ? 1 : .4; arrow(GX + 314, py + 85, GX + 376, py - 32 + i * 95, C.mid, 1.2, 6); X.globalAlpha = 1; });
    T('THE LAST TRACK\'S SENDS', GX + 455, py - 82, 13, 600, A.cue === 1 ? C.orange : C.dim, 'center');
    if (A.cue >= 1) {
      const lab = ['T1', 'T2', 'T3', 'T4', 'T5', 'T6', 'FX L', 'FX R', 'VOICE', 'RAW'], bx0 = GX + 580, bw2 = (GW - 590) / 10;
      for (let i = 0; i < 10; i++) {
        const v = i < 6 ? en(f, i) : i < 8 ? clamp(D.rms[f] * .8, 0, 1) : clamp(.3 + .5 * Math.abs(Math.sin(t * 5.3) * Math.sin(t * 1.7)), 0, 1), lit = Math.round(v * 14);
        for (let s2 = 0; s2 < 14; s2++) rr(bx0 + i * bw2 + 2, GY + 420 - s2 * 22, bw2 - 4, 16, 2, null, s2 < lit ? (i >= 8 ? C.violet : heat(30 + s2 * 15)) : '#151319');
        X.save(); X.translate(bx0 + i * bw2 + bw2 / 2 + 4, GY + 448); X.rotate(-Math.PI / 4); T(lab[i], 0, 0, 12, 600, i >= 8 ? C.violet : C.mid, 'right'); X.restore();
      }
      T(A.cue === 2 ? '10 CHANNELS BACK' : 'YOUR VOICE, WITH THE FM-1\'S SPACE', GX, GY + 520, 22, 600, C.orange);
    }
  } else {
    /* the latency: the voice lands late, the app moves it back */
    const y1 = GY + 140, y2 = GY + 330, wv = GW - 120, u = ease(clamp((A.lt - 1.5) / 2, 0, 1)), lat = 70 * (1 - u);
    T('BEAT', GX, y1 - 30, 16, 600, C.mid); T('VOICE', GX, y2 - 30, 16, 600, C.violet);
    for (let i = 0; i < 4; i++) { X.fillStyle = heat(200); X.fillRect(GX + 100 + i * wv / 4, y1 - 50, 3, 100); }
    for (let i = 0; i < 4; i++) { const x0 = GX + 100 + i * wv / 4 + lat; for (let j = 0; j < 60; j++) { const a = Math.exp(-j / 18) * 50 * Math.abs(Math.sin(j * .9 + i)); X.fillStyle = C.violet; X.fillRect(x0 + j * 2.2, y2 - a, 1.6, a * 2); } }
    if (u < 1) arrow(GX + 100 + lat + 20, y2 + 80, GX + 100 + 10, y2 + 80, C.orange, 2, 9);
    T(u < 1 ? '− ' + Math.round(24 * (1 - u)) + ' ms' : 'IN TIME', GX + 160, y2 + 120, 26, 600, u < 1 ? C.orange : C.text);
    T('measured by the app, moved back when you stop recording', GX, GY + 520, 17, 500, C.dim);
  }
};
Gr.app = (A, t, f) => {
  const px = GX + 40, py = GY - 10, pw = 300, ph = 580;
  rr(px, py, pw, ph, 36, '#2b2833', '#000', 2); rr(px + 14, py + 18, pw - 28, ph - 36, 24, null, '#040306');
  X.save(); X.beginPath(); X.roundRect(px + 14, py + 18, pw - 28, ph - 36, 24); X.clip();
  const sx0 = px + 30, sw = pw - 60;
  if (A.cue === 0) {
    T('REC', sx0, py + 60, 18, 600, C.red); if ((t * 2) % 1 < .6) { X.fillStyle = C.red; X.beginPath(); X.arc(sx0 + 52, py + 54, 7, 0, TAU); X.fill(); }
    T('01:' + String(Math.floor(A.lt) % 60).padStart(2, '0'), sx0 + sw, py + 62, 22, 600, C.text, 'right');
    for (let k = 0; k < 6; k++) { const e = en(f, k); sm[k] += (e - sm[k]) * .3; being(TRK[k][2], sx0 + 40 + (k % 3) * 80, py + 140 + Math.floor(k / 3) * 100, 22 * (1 + sm[k] * .4), sm[k], t + k); }
    for (let i = 0; i < 8; i++) { const v = i < 6 ? en(f, i) : clamp(D.rms[f] * .8, 0, 1), lit = Math.round(v * 12); for (let s2 = 0; s2 < 12; s2++) rr(sx0 + i * sw / 8 + 2, py + 470 - s2 * 13, sw / 8 - 4, 9, 2, null, s2 < lit ? heat(30 + s2 * 18) : '#151319'); }
    X.strokeStyle = C.red; X.lineWidth = 2; X.beginPath(); X.arc(px + pw / 2, py + 520, 22, 0, TAU); X.stroke(); X.fillStyle = C.red; X.beginPath(); X.arc(px + pw / 2, py + 520, 15, 0, TAU); X.fill();
  } else if (A.cue === 1) {
    T('EDIT', sx0, py + 60, 18, 600, C.text); T('BARS 9–40', sx0 + sw, py + 60, 13, 500, C.dim, 'right');
    for (let k = 0; k < 7; k++) lane(sx0 - 60, py + 90 + k * 52, sw + 60, 44, k < 6 ? k : 6, f, '', k === 0 ? C.orange : null);
    X.fillStyle = 'rgba(255,122,0,.15)'; X.fillRect(sx0 + 10, py + 90, 30, 52 * 7); X.fillStyle = C.orange; X.fillRect(sx0 + 40, py + 90, 2, 52 * 7);
    rr(sx0, py + 480, sw, 40, 6, C.text, C.text); T('EXPORT · STEMS ZIP / MIX', px + pw / 2, py + 506, 13, 700, '#000', 'center');
  } else if (A.cue === 2) {
    T('USB', sx0, py + 60, 18, 600, C.text);
    node(sx0, py + 100, sw, 80, 'ANDROID AUDIO', false, 'some channels only');
    X.strokeStyle = C.red; X.lineWidth = 3; X.beginPath(); X.moveTo(sx0 + 20, py + 110); X.lineTo(sx0 + sw - 20, py + 170); X.stroke();
    node(sx0, py + 220, sw, 80, 'OWN USB DRIVER', true, 'the FM-1\'s whole stream');
    for (let i = 0; i < 10; i++) { const v = .3 + .6 * Math.abs(Math.sin(t * 3 + i)); rr(sx0 + i * sw / 10 + 2, py + 420 - v * 90, sw / 10 - 4, v * 90, 2, null, heat(40 + v * 200)); }
  } else {
    const g = X.createRadialGradient(px + pw / 2, py + ph / 2, 0, px + pw / 2, py + ph / 2, 260); g.addColorStop(0, `rgba(90,60,255,${.2 + D.rms[f] * .2})`); g.addColorStop(1, 'rgba(0,0,0,0)'); X.fillStyle = g; X.fillRect(px, py, pw, ph);
    for (let i = 0; i < 260; i++) { const a = hash(i) * TAU + t * (.1 + hash(i + 9) * .3), r = 40 + hash(i + 3) * 120 * (1 + en(f, 0) * .5); X.fillStyle = heat(60 + hash(i + 5) * 190, .6); X.fillRect(px + pw / 2 + Math.cos(a) * r, py + ph / 2 + Math.sin(a) * r * 1.4, 2, 2); }
    for (let k = 0; k < 3; k++) being(TRK[k * 2][2], px + pw / 2 + Math.cos(t * .3 + k * 2.1) * 70, py + ph / 2 + Math.sin(t * .3 + k * 2.1) * 120, 30 * (1 + en(f, k * 2) * .5), en(f, k * 2), t + k);
  }
  X.restore();
  /* beside the phone: what this screen is */
  const cap = [['RECORD', 'every track, live, from the FM-1'], ['EDIT · EXPORT', 'trim, cut, mix · ZIP of stems or one file'], ['ITS OWN DRIVER', 'the FM-1\'s stream, read directly'], ['LIVE VISUALS', 'clouds, smoke, mitosis, the microscope']];
  cap.forEach((c, i) => { const on = i === A.cue; X.globalAlpha = on ? 1 : .3; T(c[0], px + pw + 60, GY + 90 + i * 120, 26, 600, on ? C.text : C.mid); T(c[1], px + pw + 60, GY + 122 + i * 120, 16, 500, on ? C.orange : C.dim); X.globalAlpha = 1; });
};
/* the lab film's COMING NEXT: the roadmap, one card a line, Ko-fi's early access last */
Gr.next = (A, t, f) => {
  const it = [['FX BUSES A–D', 'four assignable effect chains, routed per track'], ['COMING SOON', 'a separate product'],
    ['JIANT ON THE SMK', 'a port of the firmware: the same instrument, new hardware'], ['EARLY ACCESS', 'Ko-fi supporters play every new version first']];
  it.forEach((c, i) => {
    const on = A.cue === i + 1, seen = A.cue === 0 || A.cue > i, y = GY + 30 + i * 132, a = fade(A.ct, .3 + i * .25, .8 + i * .25);
    X.globalAlpha = a * (on ? 1 : seen ? .55 : .25);
    rr(GX, y, GW - 40, 108, 10, on ? (i === 3 ? C.orange : C.violet) : '#2b2833', on ? 'rgba(90,60,255,.07)' : null, on ? 2 : 1.5);
    T(String(i + 1).padStart(2, '0'), GX + 30, y + 66, 30, 600, on ? C.orange : C.mid);
    T(c[0], GX + 110, y + 50, 30, 600, on ? C.text : C.mid); T(c[1], GX + 110, y + 84, 17, 500, on ? C.orange : C.dim);
    if (on) heatRule(GX + 110, y + 98, (GW - 260) * ease(A.lt / .8), 2);
    if (i === 2) { const cx = GX + GW - 150, cy = y + 54, e = en(f, 0); sm[0] += (e - sm[0]) * .3; being(TRK[0][2], cx, cy, 26 * (1 + sm[0] * .4), sm[0], t); }
  });
  X.globalAlpha = A.cue === 4 ? fade(A.lt, .4, 1) : 0;
  T('ko-fi.com/juanjiant', GX, GY + 600, 40, 600, C.orange); T('new versions here first', GX + 470, GY + 600, 20, 500, C.mid);
  X.globalAlpha = 1;
};
/* the lab film's LIVE VISUALS: the WebGL visuals' frames (visuals_film.mjs) full screen, the style's name */
let visImg = null, visF = -1;
async function visuals(A, t, f) {
  if (visF !== f) { try { const r = await fetch('vis/' + f); if (r.ok) { visImg = await createImageBitmap(await r.blob()); visF = f; } } catch (e) {} }
  const a = fade(A.ct, -0.6, 0.4) * (1 - fade(t, A.ch.end - 0.5, A.ch.end + 0.3));
  X.globalAlpha = a; if (visImg) X.drawImage(visImg, 0, 0, W, H2); X.globalAlpha = 1;
  const g = X.createLinearGradient(0, H2 - 300, 0, H2); g.addColorStop(0, 'rgba(0,0,0,0)'); g.addColorStop(1, 'rgba(0,0,0,.75)'); X.fillStyle = g; X.fillRect(0, H2 - 300, W, 300);
  const names = ['NEBULA', 'SMOKE', 'MEMBRANE', 'SCAN', 'ORBITS', 'MICROSCOPE'];
  X.globalAlpha = a; T(A.ch.k + ' / ' + String(TL.chapters.filter((c) => c.k).length).padStart(2, '0') + '  ·  ' + A.ch.title, GX, 120, 20, 600, C.mid);
  X.globalAlpha = a * fade(A.lt, 0, .5); T(names[A.cue] || '', GX, 920, 64, 600, C.text, 'left', 6); heatRule(GX, 940, 300 * ease(A.lt / .8), 3);
  X.globalAlpha = 1;
}
/* the intro and the outro: the ecosystem, the name */
function eco(f, t, cx, cy, R, grow) {
  for (let k = 0; k < 6; k++) {
    const a = k / 6 * TAU - Math.PI / 2 + t * 0.03, x = cx + R * Math.cos(a) + 10 * Math.sin(t * 0.3 + k), y = cy + R * 0.62 * Math.sin(a) + 8 * Math.sin(t * 0.27 + k * 2);
    const e = en(f, k); sm[k] += (e - sm[k]) * 0.3;
    being(TRK[k][2], x, y, 44 * grow * (1 + sm[k] * 0.4), sm[k], t * 0.8 + k * 1.9, e < 0.01 ? { style: 'ghost' } : {});
  }
}
function intro(A, t, f) {
  const grow = ease(t / 3.2);
  eco(f, t, W / 2, 360, 320, grow);
  const a0 = fade(t, 4.2, 5.4);
  X.globalAlpha = a0; T('JIANT', W / 2, 760, 130, 600, C.text, 'center', 24); heatRule(W / 2 - 300, 792, 600, 3);
  X.globalAlpha = fade(t, TL.chapters[0].lines[1].start, TL.chapters[0].lines[1].start + .6) * (A.ci === 0 ? 1 : 0);
  T(TL.meta && TL.meta.tagline ? TL.meta.tagline : '6 PISTAS · 8 MOTORES · SECUENCIADOR · EFECTOS', W / 2, 846, 24, 500, C.mid, 'center', 3);
  if (A.ci === 0 && A.cue >= 2) {
    X.globalAlpha = fade(A.lt, 0, .6) * (A.cue === 2 ? 1 : 1 - fade(A.lt, 0, .5));
    X.fillStyle = '#000'; X.fillRect(W / 2 - 420, 822, 840, 40);
    rr(W / 2 - 380, 826, 760, 124, 12, C.orange, 'rgba(255,122,0,.06)', 2);
    T('GRACIAS A QUIENES APOYAN EL PROYECTO', W / 2, 872, 24, 600, C.text, 'center');
    T('ko-fi.com/juanjiant', W / 2, 922, 32, 600, C.orange, 'center');
  }
  if (A.ci === 0 && A.cue === 3) { X.globalAlpha = fade(A.lt, 0.2, .8); X.fillStyle = '#000'; X.fillRect(W / 2 - 420, 822, 840, 40); T(TL.meta && TL.meta.intro3 ? TL.meta.intro3 : 'TUTORIAL · EL SISTEMA COMPLETO', W / 2, 900, 28, 600, C.violet, 'center', 4); }
  X.globalAlpha = 1;
}
function outro(A, t, f) {
  const end = TL.end, out = 1 - fade(t, end - 3.5, end - 0.5);
  X.globalAlpha = out;
  eco(f, t, W / 2, 330, 320, 1);
  T('GRACIAS', W / 2, 730, 110, 600, C.text, 'center', 18); heatRule(W / 2 - 260, 762, 520, 3);
  X.globalAlpha = out * fade(A.ct, 2, 3);
  T('ko-fi.com/juanjiant', W / 2 - 230, 840, 30, 600, C.orange, 'center'); T('cafecito.app/juanjiant', W / 2 + 230, 840, 30, 600, C.violet, 'center');
  T('juanjiant-bit.github.io/JIANT', W / 2, 894, 24, 500, C.mid, 'center');
  X.globalAlpha = out * fade(A.ct, 8, 9);
  T('la música: tocada por JIANT en el FM-1 · la voz: sintetizada', W / 2, 944, 18, 500, C.dim, 'center');
  X.globalAlpha = 1;
}

/* ---------------------------------------------------------------- a frame --- */
async function render(f) {
  const t = f / FPS, A = at(t);
  await rgbFrame(f);
  X.fillStyle = '#000'; X.fillRect(0, 0, W, H2);
  const g = A.ch.g, full = g === 'intro' || g === 'outro' || g === 'visuals';
  if (g === 'visuals') await visuals(A, t, f);
  else if (g === 'intro') intro(A, t, f);
  else if (g === 'outro') outro(A, t, f);
  else {
    const a = fade(A.ct, -0.3, 0.5) * (1 - fade(t, A.ch.end - 0.4, A.ch.end + 0.2) * (A.ci + 1 < TL.chapters.length && TL.chapters[A.ci + 1].g === 'outro' ? 1 : 0));
    screen(f, fade(A.ct + (A.ci === 1 ? 0 : 5), 0, 1));
    X.globalAlpha = a;
    T(A.ch.k + ' / ' + String(TL.chapters.filter((c) => c.k).length).padStart(2, '0'), GX, 150, 20, 600, heat(30 + A.ci * 13 * 15 / Math.max(4, TL.chapters.length - 2)));
    const tw = T(A.ch.title, GX, 222, A.ch.title.length > 20 ? 46 : 62, 600, C.text, 'left', 2);
    heatRule(GX, 246, Math.min(tw, 860) * ease(A.ct / 1.0), 2);
    X.save(); X.translate(0, (1 - ease(A.ct / 0.6)) * 14);
    Gr[g](A, t, f);
    X.restore();
    X.globalAlpha = 1;
  }
  subtitle(t, A);
  /* the chapters at the foot */
  const chs = TL.chapters.filter((c) => c.k), x0 = 120, w0 = W - 240;
  if (!full) chs.forEach((c, i) => { const w = w0 / chs.length, on = c.id === A.ch.id; X.fillStyle = on ? heat(30 + i * 13) : '#1a1820'; X.fillRect(x0 + i * w + 2, 1058, w - 4, on ? 4 : 2); });
}
async function rgbFrame(f) {
  const b = new Uint8Array(await (await fetch('scr/' + f)).arrayBuffer()), d = simg.data;
  for (let i = 0, j = 0; i < 57600; i++, j += 3) { d[i * 4] = b[j]; d[i * 4 + 1] = b[j + 1]; d[i * 4 + 2] = b[j + 2]; d[i * 4 + 3] = 255; }
  sx.putImageData(simg, 0, 0);
}
async function boot(film) {
  D = await (await fetch('data.json')).json(); TL = await (await fetch('timeline.json')).json(); LANG = TL.lang || 'es';
  VER = (await (await fetch('version')).text()).trim();
  await document.fonts.load(`600 40px ${FONT}`); await document.fonts.load(`500 40px ${FONT}`);
  return Math.min(D.n, Math.round(TL.end * FPS));
}
async function frameJpeg(f, q) { await render(f); return cv.toDataURL('image/jpeg', q || .92); }
