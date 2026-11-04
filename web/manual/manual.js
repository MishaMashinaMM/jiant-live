// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// The interactive manual (web/manual/index.html): the FM-1 drawn with every control, the command reference with its
// search, the recipes, the navigation map (nav.js, generated from the firmware's chains by tools/gen_manual.py), what's
// new, and the browser emulator. data.js holds the words ([es, en]). The logic part (JMANUAL) runs without a DOM, for
// web/test_manual.mjs; the page is drawn only when there is a document with #fm1.
(function (G) {
"use strict";
const M = G.JMAN, NAV = G.JNAV;

// ------------------------------------------------------------------------------------------- the panel's layout
// the FM-1 on its own proportions, as the front page draws it (web/landing: an 840 x 504 panel)
const KNOBS = [["MASTER", 67, 80], ["SELECT", 157, 80], ["PRESETS", 67, 168], ["ALGORITHM", 157, 168],
               ["KNOB 1", 473, 80], ["KNOB 2", 568, 80], ["KNOB 3", 663, 80], ["KNOB 4", 758, 80]];
const ROW1 = ["FX", "SCL", "ENV", "LFO", "EDIT", "GLO"], ROW2 = ["HOME", "SAVE", "ARP", "SEQ", "PLAY", "REC"];
const BX = [485, 538, 590, 643, 696, 749], BY = [172, 224];
const NOTE = ["F", "F#", "G", "G#", "A", "A#", "B", "C", "C#", "D", "D#", "E"];
const keyName = (n) => NOTE[n % 12] + (3 + Math.floor((n + 5) / 12));
const isBlack = (n) => [1, 3, 6, 8, 10].includes((n + 5) % 12);
const UPLABEL = ["OP1", "OP2", "OP3", "OP4", "OP5", "OP6", "PIT", "GLO", "MONO", "POLY", ""];   /* the stock print */
const KEYS = [];
for (let n = 0, w = 0, b = 0; n < 27; n++) {
  if (isBlack(n)) KEYS.push({ n, name: keyName(n), black: ++b, x: 64 + (w - 0.5) * 47.6, up: UPLABEL[b - 1] });
  else KEYS.push({ n, name: keyName(n), black: 0, x: 64 + (w++) * 47.6 });
}
const CONTROL_IDS = [...KNOBS.map((k) => k[0]), "OCT-", "OCT+", ...ROW1, ...ROW2];

// ------------------------------------------------------------------------------------------- text
const L = (a, lang) => Array.isArray(a) ? (lang === "en" ? a[1] : a[0]) : (a == null ? "" : String(a));
const norm = (s) => String(s).toLowerCase().normalize("NFD").replace(/[̀-ͯ]/g, "")
  .replace(/[−–]/g, "-").replace(/[›>·,;:()"“”/.¿?¡!]/g, " ").replace(/\s+/g, " ").trim();
const tokens = (k) => (String(k).match(/OCT[+-]|[^+]+/g) || []).map((x) => x.trim()).filter(Boolean);   /* "A+B"; OCT+ is one */

// the browser emulator's keys (web/emu/index.html KEYMAP): number row FX..REC, [ ] OCT, arrows SELECT / ALGORITHM,
// Z..Slash F3..A4, S D F H J L ; ' the black keys up to A#4, Q..O B4..G5
const EMU_BTN = { FX: "1", SCL: "2", ENV: "3", LFO: "4", EDIT: "5", GLO: "6", HOME: "7", SAVE: "8", ARP: "9", SEQ: "0",
                  PLAY: "-", REC: "=", "OCT-": "[", "OCT+": "]", SELECT: "↑ ↓", ALGORITHM: "← →" };
const EMU_KEY = {};
["Z", "X", "C", "V", "B", "N", "M", ",", ".", "/"].forEach((c, i) => { EMU_KEY[keyName([0, 2, 4, 6, 7, 9, 11, 12, 14, 16][i])] = c; });
[["S", 1], ["D", 3], ["F", 5], ["H", 8], ["J", 10], ["L", 13], [";", 15], ["'", 17]].forEach(([c, n]) => { EMU_KEY[keyName(n)] = c; });
["Q", "W", "E", "R", "T", "Y", "U", "I", "O"].forEach((c, i) => { EMU_KEY[keyName(18 + i)] = c; });
function emuKey(tok, lang) {
  if (EMU_BTN[tok]) return EMU_BTN[tok];
  if (EMU_KEY[tok]) return EMU_KEY[tok];
  if (tok === "STEP") return lang === "en" ? "a grid key" : "una tecla de la grilla";
  return lang === "en" ? "mouse wheel on " + tok : "rueda del mouse sobre " + tok;
}
function emuHint(k, lang) {
  const t = tokens(k);
  if (!t.length) return "";
  const keys = t.map((x) => { const k = emuKey(x, lang); return k.length <= 3 ? "«" + k + "»" : k; });
  if (t.length === 1) return (lang === "en" ? "Emulator: " : "Emulador: ") + keys[0];
  return (lang === "en" ? "Emulator: hold " : "Emulador: mantené ") + keys[0] +
         (lang === "en" ? " + " : " + ") + keys.slice(1).join(" + ");
}

// ------------------------------------------------------------------------------------------- search
function entryText(e, lang) {
  return norm([L(e.t, lang), L(e.d, lang), e.k || "", tokens(e.k || "").join(" "), L(e.t, lang === "en" ? "es" : "en"),
               (e.steps || []).map((s) => L(s, lang)).join(" "), sectionName(e.s, lang)].join(" "));
}
function sectionName(id, lang) { const s = M.sections.find((x) => x.id === id); return s ? L(s.t, lang) : id; }
// every token must be in the entry (in either language); the title weighs more
function search(q, lang, sec, list) {
  const items = list || M.commands;
  const qt = norm(q).split(" ").filter(Boolean);
  const out = [];
  items.forEach((e, i) => {
    if (sec && sec !== "all" && e.s !== sec) return;
    if (!qt.length) { out.push({ e, i, score: 0 }); return; }
    const txt = entryText(e, lang), tt = norm(L(e.t, lang) + " " + tokens(e.k || "").join(" "));
    let score = 0;
    for (const w of qt) {
      if (!txt.includes(w)) return;
      score += tt.includes(w) ? 3 : 1;
    }
    out.push({ e, i, score });
  });
  return out.sort((a, b) => b.score - a.score || a.i - b.i).map((r) => r.e);
}

// what each key does in each layer (data.js layers)
function keyLayers(name, lang) {
  const out = [];
  for (const [id, ly] of Object.entries(M.layers)) {
    let v = ly.all ? (ly.all === "ROOT" ? "ROOT = " + name.replace(/\d/, "") : ly.all) : ly.keys[name];
    if (v != null) out.push({ layer: id, t: L(ly.t, lang), v: L(v, lang) });
  }
  return out;
}
function keyLabel(layer, name, lang) {
  const ly = M.layers[layer];
  if (!ly) return "";
  if (ly.all) return name.replace(/\d/, "");
  return L(ly.keys[name], lang) || "";
}

// the shots the page shows: nav.js's pages and data.js's entries
function allShots() {
  const s = new Set();
  NAV.chains.forEach((c) => c.pages.forEach((p) => { if (p.shot) s.add(p.shot); }));
  Object.values(M.controls).forEach((c) => { if (c.shot) s.add(c.shot); });
  Object.values(M.layers).forEach((c) => { if (c.shot) s.add(c.shot); });
  M.commands.forEach((c) => { if (c.shot) s.add(c.shot); });
  M.recipes.forEach((r) => (r.shots || []).forEach((x) => s.add(x)));
  return [...s];
}

const UI = {
  es: { lang: "es", nav_panel: "El panel", nav_ref: "¿Cómo hago…?", nav_rec: "Recetas", nav_map: "Mapa", nav_new: "Novedades", nav_emu: "Emulador",
    kicker: "manual interactivo · firmware", h1a: "Manual", h1b: "interactivo",
    lede: "Todo JIANT en una página: tocá cualquier control del FM-1 para ver qué hace solo, mantenido y en combinación, con la pantalla real a la que lleva. Buscá cualquier comando, seguí una receta o probalo en vivo en el emulador.",
    p_eb: "El panel", p_h2: "Tocá un control", p_in: "Pasá el mouse o tocá cualquier perilla, botón o tecla. Elegí una capa para ver qué hace cada tecla mientras mantenés ese botón (como el MAP del firmware).",
    layer: "Capa en el teclado:", none: "ninguna", pick: "Elegí un control…",
    alone: "Solo", held: "Mantenido", combos: "Combinaciones", layers_k: "En cada capa", screen: "La pantalla", showlayer: "Ver esta capa en el teclado",
    knob: "perilla / encoder", button: "botón", key: "tecla", black: "negra", white: "blanca", find: "Buscar en ¿Cómo hago…?",
    r_eb: "Referencia", r_h2: "¿Cómo hago…?", r_in: "Cada comando y cada página, verificado contra el firmware 0.9.12 y su manual. Filtrá por sección o escribí lo que querés hacer.",
    ph: "ej.: borrar un paso, grabar sin cuantizar, MIDI LEARN", all: "Todo", results: "resultados", noresults: "Nada con eso. Probá otra palabra (o en inglés).",
    rc_eb: "Paso a paso", rc_h2: "Recetas", rc_in: "Las tareas de siempre, en orden.",
    m_eb: "Orientarse", m_h2: "Mapa de navegación", m_in: "Cada sección, sus grupos (las pestañas) y sus páginas en el orden en que SELECT las recorre, leídos de las tablas del propio firmware. Tocá una página para ver su pantalla y sus cuatro perillas.",
    m_full: "Abrir el mapa completo (SVG)", m_on: "naranja: la página al encender", m_cond: "punteada: solo en algunos motores o LOAD", m_rules: "Las seis reglas", m_hold: "mantener", m_sel: "+ SELECT", m_none: "sin capa", knobs4: "Las cuatro perillas", noshot: "(sin captura de esta página)",
    n_eb: "Para los que vuelven", n_h2: "Novedades por versión", n_in: "Lo importante de cada versión, de la 0.9.12 a la 0.8. El detalle completo está en el changelog del README.", n_more: "Versiones anteriores",
    e_eb: "En vivo", e_h2: "Probalo en el emulador", e_in: "El firmware completo corriendo en el navegador. Cada comando de este manual muestra qué teclas de la compu usar.", e_go: "Encender el emulador acá", e_tab: "Abrir en otra pestaña",
    e_keys: "Teclado: Z–' y Q–O las teclas · 1 … = los botones FX … REC · [ ] OCT · ↑ ↓ SELECT · ← → ALGORITHM · Shift-clic mantiene un botón · perillas: rueda del mouse.",
    close: "Cerrar", ver: "firmware", recipe_steps: "pasos", try: "Probar", emu_note: "(necesita conexión)",
    foot: "JIANT FM · Juan Jiant · Buenos Aires", home: "Inicio", manualmd: "Manual completo (texto)" },
  en: { lang: "en", nav_panel: "The panel", nav_ref: "How do I…?", nav_rec: "Recipes", nav_map: "Map", nav_new: "What's new", nav_emu: "Emulator",
    kicker: "interactive manual · firmware", h1a: "Interactive", h1b: "manual",
    lede: "All of JIANT on one page: touch any control of the FM-1 to see what it does alone, held and in combination, with the real screen it leads to. Search any command, follow a recipe or try it live in the emulator.",
    p_eb: "The panel", p_h2: "Touch a control", p_in: "Hover or tap any knob, button or key. Pick a layer to see what each key does while you hold that button (like the firmware's MAP).",
    layer: "Layer on the keys:", none: "none", pick: "Pick a control…",
    alone: "Alone", held: "Held", combos: "Combinations", layers_k: "In each layer", screen: "The screen", showlayer: "Show this layer on the keys",
    knob: "knob / encoder", button: "button", key: "key", black: "black", white: "white", find: "Find in How do I…?",
    r_eb: "Reference", r_h2: "How do I…?", r_in: "Every command and page, checked against firmware 0.9.12 and its manual. Filter by section or type what you want to do.",
    ph: "e.g. clear a step, record unquantized, MIDI LEARN", all: "All", results: "results", noresults: "Nothing with that. Try another word (or in Spanish).",
    rc_eb: "Step by step", rc_h2: "Recipes", rc_in: "The usual tasks, in order.",
    m_eb: "Find your way", m_h2: "Navigation map", m_in: "Every section, its groups (the tabs) and its pages in the order SELECT walks them, read from the firmware's own tables. Touch a page to see its screen and its four knobs.",
    m_full: "Open the full map (SVG)", m_on: "orange: the page at power-on", m_cond: "dashed: only on some engines or LOADs", m_rules: "The six rules", m_hold: "hold", m_sel: "+ SELECT", m_none: "no layer", knobs4: "The four knobs", noshot: "(no capture of this page)",
    n_eb: "For returning users", n_h2: "What's new, by version", n_in: "What matters in each version, from 0.9.12 back to 0.8. The full details are in the README's changelog.", n_more: "Earlier versions",
    e_eb: "Live", e_h2: "Try it in the emulator", e_in: "The whole firmware running in the browser. Every command in this manual shows which computer keys to use.", e_go: "Switch the emulator on here", e_tab: "Open in a new tab",
    e_keys: "Keyboard: Z–' and Q–O the keys · 1 … = the buttons FX … REC · [ ] OCT · ↑ ↓ SELECT · ← → ALGORITHM · Shift-click holds a button · knobs: mouse wheel.",
    close: "Close", ver: "firmware", recipe_steps: "steps", try: "Try", emu_note: "(needs a connection)",
    foot: "JIANT FM · Juan Jiant · Buenos Aires", home: "Home", manualmd: "Full manual (text)" }
};

const API = { KNOBS, ROW1, ROW2, KEYS, CONTROL_IDS, keyName, norm, tokens, emuHint, search, keyLayers, keyLabel, allShots, UI, L, sectionName };
G.JMANUAL = API;
if (typeof document === "undefined" || !document.getElementById("fm1")) return;

// ======================================================================================= the page
const $ = (s, r = document) => r.querySelector(s);
const $$ = (s, r = document) => [...r.querySelectorAll(s)];
const NS = "http://www.w3.org/2000/svg";
const esc = (s) => String(s).replace(/[&<>"]/g, (c) => ({ "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;" }[c]));
let lang = "es", cur = null, layer = "", sec = "all";
const store = { get(k) { try { return localStorage.getItem(k); } catch (e) { return null; } },
                set(k, v) { try { localStorage.setItem(k, v); } catch (e) { /* private mode */ } } };
const u = (k) => UI[lang][k] || k;
const shotSrc = (n) => "shots/" + n + ".png";
function caps(k) {
  return tokens(k).map((t) => `<kbd class="cap${/^[A-G]#?\d$/.test(t) ? " note" : ""}">${esc(t === "STEP" ? (lang === "en" ? "STEP held" : "PASO") : t.replace("-", "−"))}</kbd>`)
    .join('<span class="plus">+</span>');
}

// ------------------------------------------------------------------------------------------- the drawing
function el(tag, attrs, parent) {
  const e = document.createElementNS(NS, tag);
  for (const [k, v] of Object.entries(attrs)) e.setAttribute(k, v);
  if (parent) parent.appendChild(e);
  return e;
}
function text(parent, x, y, s, cls) { const t = el("text", { x, y, class: cls || "lab", "text-anchor": "middle" }, parent); t.textContent = s; return t; }
function drawPanel() {
  const svg = $("#fm1");
  svg.innerHTML = "";
  el("rect", { x: 1, y: 1, width: 838, height: 498, rx: 40, class: "body" }, svg);
  el("rect", { x: 44, y: 214, width: 134, height: 43, rx: 8, class: "well" }, svg);
  el("rect", { x: 448, y: 140, width: 338, height: 117, rx: 12, class: "well" }, svg);
  el("rect", { x: 28, y: 287, width: 784, height: 183, rx: 26, class: "well" }, svg);
  // the screen: the real render of what the control leads to
  el("rect", { x: 216, y: 39, width: 199, height: 198, rx: 22, class: "bezel" }, svg);
  el("image", { id: "lcd", x: 241, y: 63, width: 150, height: 150, href: shotSrc("home"), preserveAspectRatio: "none" }, svg);
  el("rect", { x: 240.5, y: 62.5, width: 151, height: 151, rx: 2, class: "lcdrim" }, svg);
  const ctl = (id, kind) => { const g = el("g", { class: "ctl " + kind, "data-ctl": id, tabindex: 0, role: "button", "aria-label": id }, svg); return g; };
  for (const [n, x, y] of KNOBS) {
    const g = ctl(n, "knob");
    el("circle", { cx: x, cy: y, r: 24, class: "hit" }, g);
    el("circle", { cx: x, cy: y, r: 17, class: "cap" }, g);
    el("circle", { cx: x, cy: y, r: 12.5, class: "capin" }, g);
    el("line", { x1: x, y1: y - 5, x2: x, y2: y - 15, class: "ind" }, g);
    text(g, x, y - 30, n);
  }
  [["OCT-", 54], ["OCT+", 120]].forEach(([n, x]) => {
    const g = ctl(n, "button");
    el("rect", { x, y: 222, width: 54, height: 28, rx: 6, class: "btn" }, g);
    text(g, x + 27, 240, n.replace("-", "−"), "blab");
  });
  [ROW1, ROW2].forEach((row, j) => row.forEach((n, i) => {
    const g = ctl(n, "button"), X = BX[i], Y = BY[j];
    el("rect", { x: X - 21, y: Y - 21, width: 42, height: 42, rx: 8, class: "btn" }, g);
    if (n === "PLAY") { text(g, X, Y - 2, "PLAY", "blab"); el("rect", { x: X - 12, y: Y + 1, width: 24, height: 1, class: "bar" }, g); text(g, X, Y + 11, "STOP", "blab"); }
    else text(g, X, Y + 3.5, n, "blab" + (n === "HOME" ? " home" : n === "REC" ? " rec" : ""));
  }));
  for (const k of KEYS) {
    const g = ctl(k.name, "key" + (k.black ? " black" : " white"));
    const y = k.black ? 300 : 382, w = k.black ? 40 : 43, h = k.black ? 76 : 77;
    el("rect", { x: k.x - w / 2, y, width: w, height: h, rx: w / 2, class: "pill" }, g);
    el("rect", { x: k.x - 2.4, y: y + (k.black ? 15 : 16), width: 4.8, height: k.black ? 47 : 45, rx: 2.4, class: "slit" }, g);
    if (k.up) text(g, k.x, 296, k.up, "print");
    text(g, k.x, k.black ? 390 : 472, k.name, "kname");
    const lt = el("text", { class: "klay", x: k.x, y: y + h / 2, "text-anchor": "middle", transform: `rotate(-90 ${k.x} ${y + h / 2})`, "dominant-baseline": "central" }, g);
    lt.textContent = "";
  }
  $$(".ctl", svg).forEach((g) => {
    const id = g.dataset.ctl;
    g.addEventListener("mouseenter", () => show(id));
    g.addEventListener("focus", () => show(id));
    g.addEventListener("click", () => { show(id, true); });
    g.addEventListener("keydown", (e) => { if (e.key === "Enter" || e.key === " ") { e.preventDefault(); show(id, true); } });
  });
  paintLayer();
}
function paintLayer() {
  for (const k of KEYS) {
    const g = $(`.ctl[data-ctl="${k.name}"]`);
    if (!g) continue;
    const v = layer ? keyLabel(layer, k.name, lang) : "";
    $(".klay", g).textContent = v.length > 11 ? v.slice(0, 10) + "…" : v;
    g.classList.toggle("has", !!v);
  }
  const btn = layer === "MANO" ? "GLO" : layer === "NAME" ? "" : layer;
  $$(".ctl.button").forEach((g) => g.classList.toggle("held", g.dataset.ctl === btn));
  $$("#layers button").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.layer === layer)));
}

// ------------------------------------------------------------------------------------------- the info panel
function setLcd(shot) { const i = $("#lcd"); if (i && shot) i.setAttribute("href", shotSrc(shot)); }
function show(id, pin) {
  cur = id;
  $$(".ctl").forEach((g) => g.classList.toggle("sel", g.dataset.ctl === id));
  const sel = $("#ctlsel"); if (sel && sel.value !== id) sel.value = id;
  const box = $("#info");
  const c = M.controls[id];
  let h = "", shot = null;
  if (c) {
    shot = c.shot || null;
    h += `<p class="ikind">${esc(u(c.kind))} · ${esc(sectionName(c.sec, lang))}</p><h3 class="iname">${esc(id.replace("-", "−"))}</h3>`;
    h += `<dl><dt>${u("alone")}</dt><dd>${esc(L(c.alone, lang))}</dd>`;
    if (c.held) h += `<dt>${u("held")}</dt><dd>${esc(L(c.held, lang))}</dd>`;
    h += "</dl>";
    if (c.note) h += `<p class="inote">${esc(L(c.note, lang))}</p>`;
    if (c.combos && c.combos.length) {
      h += `<h4>${u("combos")}</h4><ul class="combos">` + c.combos.map((x) =>
        `<li><button type="button" class="combo" data-q="${esc(tokens(x.k).join(" "))}">${caps(x.k)}</button><span>${esc(L(x.t, lang))}</span></li>`).join("") + "</ul>";
    }
    if (c.layer) h += `<p><button type="button" class="ghost" data-layer="${c.layer}">${u("showlayer")}</button></p>`;
    h += `<p class="emu">${esc(emuHint(id, lang))}</p>`;
  } else {
    const k = KEYS.find((x) => x.name === id);
    if (!k) return;
    h += `<p class="ikind">${esc(u("key"))} · ${k.black ? esc(u("black")) + " " + k.black : esc(u("white"))}</p><h3 class="iname">${esc(id)}</h3>`;
    h += `<dl><dt>${u("alone")}</dt><dd>${esc(L(M.keyAlone, lang))}</dd></dl>`;
    const ls = keyLayers(id, lang);
    if (ls.length) h += `<h4>${u("layers_k")}</h4><table class="klayers">` + ls.map((x) =>
      `<tr${x.layer === layer ? ' class="on"' : ""}><th><button type="button" class="ghost sm" data-layer="${x.layer}">${esc(x.t)}</button></th><td>${esc(x.v)}</td></tr>`).join("") + "</table>";
    h += `<p class="emu">${esc(emuHint(id, lang))}</p>`;
    shot = layer && M.layers[layer] ? M.layers[layer].shot : null;
  }
  if (shot) {
    setLcd(shot);
    h += `<figure class="ishot"><img src="${shotSrc(shot)}" alt="${esc(u("screen"))}: ${esc(shot)}" width="240" height="240" loading="lazy" data-zoom><figcaption>${u("screen")}</figcaption></figure>`;
  }
  h += `<p><button type="button" class="ghost" data-find="${esc(id)}">${u("find")} →</button></p>`;
  box.innerHTML = h;
  if (pin && matchMedia("(max-width: 900px)").matches) box.scrollIntoView({ block: "nearest", behavior: "smooth" });
}

// ------------------------------------------------------------------------------------------- reference and recipes
function cmdCard(e) {
  return `<article class="cmd brk"><header><span class="tag">${esc(sectionName(e.s, lang))}</span><span class="keys">${caps(e.k)}</span></header>
    <h3>${esc(L(e.t, lang))}</h3><p>${esc(L(e.d, lang))}</p>
    <footer><span class="emu">${esc(emuHint(e.k, lang))}</span>${e.shot ? `<img src="${shotSrc(e.shot)}" alt="" width="96" height="96" loading="lazy" data-zoom>` : ""}</footer></article>`;
}
function recipeCard(r, open) {
  return `<details class="rec brk"${open ? " open" : ""}><summary><span class="tag">${esc(sectionName(r.s, lang))}</span>${esc(L(r.t, lang))}</summary>
    <ol>${r.steps.map((s) => `<li>${esc(L(s, lang))}</li>`).join("")}</ol>
    ${(r.shots || []).length ? `<div class="rshots">${r.shots.map((s) => `<img src="${shotSrc(s)}" alt="" width="120" height="120" loading="lazy" data-zoom>`).join("")}</div>` : ""}</details>`;
}
function renderRef() {
  const q = $("#q").value;
  const res = search(q, lang, sec);
  const rec = search(q, lang, sec, M.recipes);
  $("#count").textContent = `${res.length + rec.length} ${u("results")}`;
  $("#cmds").innerHTML = res.length ? res.map(cmdCard).join("") : `<p class="empty">${u("noresults")}</p>`;
  $("#recipes").innerHTML = rec.map((r) => recipeCard(r, !!q.trim())).join("");
  $$("#secs button").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.sec === sec)));
}
function renderChips() {
  const n = (id) => M.commands.filter((c) => id === "all" || c.s === id).length + M.recipes.filter((c) => id === "all" || c.s === id).length;
  $("#secs").innerHTML = [{ id: "all", t: [u("all"), u("all")] }, ...M.sections].map((s) =>
    `<button type="button" data-sec="${s.id}" aria-pressed="false">${esc(L(s.t, lang))} <i>${n(s.id)}</i></button>`).join("");
}

// ------------------------------------------------------------------------------------------- the map
const ORDER = ["HOME", "EDIT", "SEQ", "FX", "GLO", "ENV", "LFO", "SCL", "ARP", "SAVE"];
function renderMap() {
  const T = NAV.lang[lang];
  let h = "";
  for (const s of ORDER) {
    const chains = NAV.chains.filter((c) => c.sec === s);
    const hint = T.hints[s] || {};
    h += `<section class="sec brk" id="map-${s}"><header><kbd class="cap big">${s}</kbd>
      <span class="hh">${u("m_hold")}: <b>${esc(hint.hold || u("m_none"))}</b></span><span class="hh">${u("m_sel")}: <b>${esc(hint.sel || "")}</b></span></header>`;
    for (const c of chains) {
      if (c.label) h += `<p class="clabel">${esc(L([c.label.es, c.label.en], lang))}</p>`;
      h += `<div class="chain">`;
      c.groups.forEach((g, gi) => {
        const pages = c.pages.map((p, i) => ({ p, i })).filter((x) => x.p.g === gi);
        h += `<div class="grp"><div class="tab">${esc(g)}</div>` + pages.map(({ p, i }) =>
          `<button type="button" class="pg${p.on ? " on" : ""}${p.cond ? " cond" : ""}" data-sec="${s}" data-ce="${c.ce}" data-i="${i}"><i>${i + 1}</i>${esc(p.name)}</button>`).join("") + "</div>";
      });
      h += "</div>";
    }
    if (T.notes[s]) h += `<p class="mnote">${esc(T.notes[s])}</p>`;
    h += "</section>";
  }
  $("#map").innerHTML = h;
  $("#rules").innerHTML = T.rules.map((r, i) => `<li><span class="circ">${i + 1}</span><b>${esc(r.t)}</b> ${esc(r.d)}</li>`).join("");
  const sf = lang === "es" ? ".es" : "";
  $("#mapfull").href = "../docs/navigation-map" + sf + ".svg";
}
function showPage(secName, ce, i) {
  const c = NAV.chains.find((x) => x.ce === ce), p = c.pages[i];
  const knobs = M.pages[secName + "/" + p.name] || [];
  const grp = c.groups[p.g];
  let h = `<button type="button" class="x" data-close aria-label="${u("close")}">×</button>
    <p class="crumb">${esc(secName)} · ${esc(grp)} · ${esc(p.name)} <span>${i + 1}/${c.pages.length}</span></p>`;
  h += p.shot ? `<img src="${shotSrc(p.shot)}" alt="${esc(p.name)}" width="240" height="240">` : `<p class="empty">${u("noshot")}</p>`;
  if (knobs.length) h += `<h4>${u("knobs4")}</h4><ol class="k4">${knobs.map((k) => `<li>${esc(k || "—")}</li>`).join("")}</ol>`;
  h += `<p><button type="button" class="ghost" data-find="${esc(p.name)}">${u("find")} →</button></p>`;
  const d = $("#pagebox");
  d.innerHTML = h;
  d.hidden = false;
  $$(".pg").forEach((b) => b.classList.toggle("sel", b.dataset.ce === ce && +b.dataset.i === i));
}

// ------------------------------------------------------------------------------------------- what's new
function renderNews() {
  const card = (n) => `<article class="ver brk"><h3><span>v</span>${esc(n.v)}</h3><ul>${n.items.map((x) => `<li>${esc(L(x, lang))}</li>`).join("")}</ul></article>`;
  $("#news").innerHTML = M.news.slice(0, 4).map(card).join("") +
    `<details class="older"><summary>${u("n_more")} (${esc(M.news[4].v)} – ${esc(M.news[M.news.length - 1].v)})</summary><div class="news">${M.news.slice(4).map(card).join("")}</div></details>`;
}

// ------------------------------------------------------------------------------------------- language and wiring
function setLang(l) {
  lang = l === "en" ? "en" : "es";
  document.documentElement.lang = lang;
  $$("[data-i]").forEach((e) => { e.textContent = u(e.dataset.i); });
  $$("[data-iph]").forEach((e) => { e.placeholder = u(e.dataset.iph); });
  $$(".lang button").forEach((b) => b.setAttribute("aria-pressed", String(b.dataset.lang === lang)));
  $("#ctlsel").innerHTML = `<option value="">${esc(u("pick"))}</option>` +
    [...CONTROL_IDS, ...KEYS.map((k) => k.name)].map((id) => `<option value="${esc(id)}">${esc(id.replace("-", "−"))}</option>`).join("");
  $("#layers").innerHTML = `<span data-i="layer">${u("layer")}</span>` + ["", ...Object.keys(M.layers)].map((id) =>
    `<button type="button" data-layer="${id}" aria-pressed="false">${id ? esc(id) : esc(u("none"))}</button>`).join("");
  $("#ver").textContent = NAV.version;
  store.set("jiant-lang", lang);
  renderChips(); renderRef(); renderMap(); renderNews(); paintLayer();
  show(cur || "SELECT");
}
function zoom(src) {
  const d = $("#zoom");
  $("img", d).src = src;
  d.hidden = false;
}
function init() {
  drawPanel();
  let l = new URLSearchParams(location.search).get("lang") || store.get("jiant-lang");
  if (!l) l = (navigator.language || "es").toLowerCase().startsWith("en") ? "en" : "es";
  setLang(l);
  $$(".lang button").forEach((b) => b.addEventListener("click", () => setLang(b.dataset.lang)));
  $("#q").addEventListener("input", renderRef);
  $("#ctlsel").addEventListener("change", (e) => { if (e.target.value) show(e.target.value, true); });
  document.addEventListener("click", (e) => {
    const t = e.target.closest("button, img[data-zoom], [data-close]");
    if (!t) return;
    if (t.dataset.close !== undefined) { t.closest("#pagebox, #zoom").hidden = true; return; }
    if (t.matches("img[data-zoom]")) { zoom(t.src); return; }
    if (t.dataset.layer !== undefined) { layer = t.dataset.layer; paintLayer(); if (cur && !M.controls[cur]) show(cur); return; }
    if (t.dataset.sec && t.closest("#secs")) { sec = t.dataset.sec; renderRef(); return; }
    if (t.classList.contains("pg")) { showPage(t.dataset.sec, t.dataset.ce, +t.dataset.i); return; }
    const q = t.dataset.q || t.dataset.find;
    if (q) { $("#q").value = q; sec = "all"; renderRef(); $("#ref").scrollIntoView({ behavior: "smooth" }); return; }
    if (t.id === "emugo") {
      const f = document.createElement("iframe");
      f.src = "../emu/"; f.title = "JIANT FM emulator"; f.allow = "autoplay"; f.loading = "lazy";
      $("#emubox").replaceChildren(f);
    }
  });
  document.addEventListener("keydown", (e) => { if (e.key === "Escape") { $("#zoom").hidden = true; $("#pagebox").hidden = true; } });
  if ("serviceWorker" in navigator && /^https?:$/.test(location.protocol)) navigator.serviceWorker.register("sw.js").catch(() => {});
}
init();
})(typeof window !== "undefined" ? window : globalThis);
