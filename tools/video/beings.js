/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT 0.8.0) the beings of the design studies (firmware/src/ui_being.c, ported to the canvas, with organelles):
 * the palette, the heat ramp, the genomes, organism(); for the films (tools/video/tutorial.html) */
/* ---------------------------------------------------------------- palette: JIANT + heat_col + the plates' inks --- */
const C = { text: "#f4f1ea", dim: "#6e6a78", mid: "#a39fae", faint: "#2a2633", rule: "#1c1a22",
            violet: "#b870ff", orange: "#ff7a00", cyan: "#3fd6dc", yellow: "#ffd400", red: "#f72831", pink: "#ff5aa8",
            green: "#7ccf5a", navy: "#060a2e", blue: "#1b2fd8" };
const H = [[0, 208, 255], [90, 60, 255], [181, 48, 230], [247, 40, 49], [255, 121, 0], [255, 212, 0], [255, 255, 255]];
function heat(v, a = 1) {
  v = Math.max(0, Math.min(256, v));
  const k = Math.min(5, Math.floor(v * 6 / 257)), m = (v * 6 - k * 256) / 256, p = H[k], q = H[k + 1];
  const c = p.map((x, j) => Math.round(x + (q[j] - x) * m));
  return a < 1 ? `rgba(${c},${a})` : `rgb(${c})`;
}
const clamp = (v, a, b) => Math.max(a, Math.min(b, v));
const TAU = Math.PI * 2;
const hash = (n) => { n = Math.imul(n ^ 0x9e3779b9, 2654435761); n ^= n >>> 15; return (n >>> 0) / 4294967296; };

/* ---------------------------------------------------------------- the beings: ui_being.c, ported, + organelles --- */
const G = {
  rad: [6, 18, 16, 3, 22, 0, 6], amo: [3, 5, 14, 2, 26, 3, 2], dia: [4, 6, 10, 4, 18, 0, 1], cil: [10, 30, 8, 5, 20, 1, 10],
  spo: [5, 9, 12, 3, 19, 2, 4], wor: [2, 4, 16, 2, 30, 1, 3], pol: [8, 12, 12, 6, 17, 0, 8], hyd: [3, 7, 15, 3, 21, 4, 5],
  drm: [12, 0, 10, 3, 18, 0, 6],
};
const LANE = [["BD", [3, 0, 2, 2, 16, 0, 2]], ["SD", [12, 0, 14, 3, 16, 0, 6]], ["CP", [5, 0, 8, 5, 18, 2, 3]], ["CH", [22, 0, 6, 4, 16, 0, 10]],
              ["OH", [18, 0, 12, 4, 16, 1, 8]], ["TM", [4, 0, 4, 3, 17, 0, 2]], ["RS", [4, 0, 9, 4, 16, 0, 1]], ["CB", [5, 0, 16, 5, 16, 0, 5]]];
const gen = (a) => ({ k0: a[0], kk: a[1], ss: a[2], lob: a[3], ax: a[4], tw: a[5], sh: a[6] });
const laneF = (ga, l) => ({ spikes: ga[0], len: ga[2] * 6, wob: 40, lob: ga[3], nuc: l === 2 ? 3 : 1, sep: 160 });
/* one isotherm's points (bg_outline). f.m blends in a second species (f.lobB, f.spikesB, gB): the organ transforming.
   tum: tumours, bumps {a, h, w} on every isotherm */
function iso(g, f, cx, cy, r, outer, t, tum, nv) {
  const pts = [], m = f.m || 0, gB = f.gB || g, ax = g.ax + (gB.ax - g.ax) * m, sh = g.sh + (gB.sh - g.sh) * m, tw = g.tw + (gB.tw - g.tw) * m;
  for (let i = 0; i <= nv; i++) {
    const th = (i % nv) / nv * TAU;
    const wA = 0.28 * Math.sin(f.lob * th + t) + 0.12 * Math.sin((f.lob + 2) * th - 2 * t);
    const wB = m ? 0.28 * Math.sin(f.lobB * th + t) + 0.12 * Math.sin((f.lobB + 2) * th - 2 * t) : 0;
    let rr = r + (r * f.wob / 256) * (wA * (1 - m) + wB * m);
    if (outer) {
      const sp = (k) => { const c2 = Math.sin(k * th / 2 + tw * 3000 / 65536 * TAU + Math.PI / 2); return c2 > 0 ? Math.pow(c2, sh) : 0; };
      const p = sp(f.spikes) * (1 - m) + (m ? sp(f.spikesB) * m : 0);
      rr += (r * f.len / 256) * p * 26 / 16;
      rr += (r * f.ripple / 256) * Math.sin(th * 18 + 3 * t) / 14;
      if (f.fray) rr += ((hash(i % nv + Math.floor(t * 7) * 977) * 256 - 128) * (r * f.fray / 256)) / 1800;
    }
    for (const u of tum) { const d = Math.atan2(Math.sin(th - u.a), Math.cos(th - u.a)); rr += r * u.h * Math.exp(-((d / u.w) ** 2)); }
    pts.push([cx + rr * Math.cos(th) * ax / 16, cy + rr * Math.sin(th), rr, th]);
  }
  return pts;
}
function path(ctx, pts) { ctx.beginPath(); pts.forEach((p, i) => (i ? ctx.lineTo(p[0], p[1]) : ctx.moveTo(p[0], p[1]))); ctx.closePath(); }
function ring(ctx, x, y, r, col) { ctx.beginPath(); ctx.arc(x, y, Math.max(0.5, r), 0, TAU); ctx.strokeStyle = col; ctx.stroke(); }
function hatch(ctx, ang, gap, col, x, y, s) {   /* parallel lines through the current clip, around x y */
  ctx.strokeStyle = col; ctx.lineWidth = 1; ctx.beginPath();
  const c = Math.cos(ang), sn = Math.sin(ang);
  for (let k = -s; k <= s; k += gap) { ctx.moveTo(x + k * -sn - s * c, y + k * c - s * sn); ctx.lineTo(x + k * -sn + s * c, y + k * c + s * sn); }
  ctx.stroke();
}
function star(ctx, x, y, r, n, col, a0 = 0) {
  ctx.strokeStyle = col; ctx.lineWidth = 1; ctx.beginPath();
  for (let k = 0; k < n; k++) { const a = a0 + k / n * TAU; ctx.moveTo(x, y); ctx.lineTo(x + Math.cos(a) * r, y + Math.sin(a) * r); }
  ctx.stroke(); ctx.fillStyle = col; ctx.beginPath(); ctx.arc(x, y, Math.max(1, r * 0.22), 0, TAU); ctx.fill();
}
/* the organism. o.style: "thermal" (the firmware's lines), "plate" (Auzoux: pellicle, cilia, vacuoles), "pet"
   (filled isotherms), "ghost" (an ancestor, cream and dim), "dim" (muted). o.ex excitement, o.tum tumours,
   o.voices food vacuoles, o.beat 0..1 the contractile vacuole */
function organism(ctx, ga, f0, cx, cy, r, en, t, o = {}) {
  const g = gen(ga), E = en * 256, f = { sep: 0, fray: 0, ripple: 0, ...f0 }, st = o.style || "thermal";
  if (f.gB && !f.gB.ax) f.gB = gen(f.gB);
  if (o.ex) {
    const e = E * 0.4;
    f.wob = clamp(f.wob + e * o.ex / 4, 0, 256); f.len = clamp(f.len + e * o.ex / 3, 0, 420);
    f.ripple = clamp(f.ripple + e * o.ex / 3, 0, 256); f.fray = clamp(f.fray + e * o.ex / 6, 0, 256);
  }
  r += r * E * 30 / 65536;
  const SC = [256, 180, 112, 52], HT = [28, 90, 150, 206], tum = o.tum || [], nv = o.nv || 96;
  const isos = SC.map((s, i) => iso(g, f, cx, cy, r * s / 256, !i, t, tum, i ? nv / 2 : nv));
  const nuc = [];
  for (let n = 0; n < f.nuc; n++) {
    const a = t + n * TAU / f.nuc, d = f.nuc > 1 ? r * (24 + f.sep * 40 / 256) / 100 : 0;
    nuc.push([cx + d * Math.cos(a) * g.ax / 16, cy + d * Math.sin(a) * 0.7]);
  }
  ctx.lineWidth = 1;
  if (st === "dim") { path(ctx, isos[0]); ctx.setLineDash([2, 2]); ctx.strokeStyle = o.col || C.dim; ctx.stroke(); ctx.setLineDash([]); return { r, isos, nuc, out: isos[0], f }; }
  if (st === "pet") {
    const gr = ctx.createRadialGradient(cx, cy, r * 0.6, cx, cy, r * 2.1);
    gr.addColorStop(0, "rgba(27,47,216,.75)"); gr.addColorStop(0.6, "rgba(20,30,150,.35)"); gr.addColorStop(1, "rgba(6,10,46,0)");
    ctx.fillStyle = gr; ctx.beginPath(); ctx.ellipse(cx, cy, r * 2.1 * g.ax / 16, r * 2.1, 0, 0, TAU); ctx.fill();
    ctx.save(); ctx.filter = `blur(${o.blur ?? 1}px)`;
    const PH = [10, 70, 140, 200];
    isos.forEach((p, i) => { path(ctx, p); ctx.fillStyle = heat(PH[i] + E * 50 / 256); ctx.fill(); });
    nuc.forEach(([x, y]) => { ctx.fillStyle = heat(236 + E * 20 / 256); ctx.beginPath(); ctx.arc(x, y, r * 0.13, 0, TAU); ctx.fill(); });
    tum.forEach((u) => { const p = atAngle(isos[1], u.a); ctx.fillStyle = heat(220); ctx.beginPath(); ctx.arc(p[0], p[1], r * u.h * 0.6, 0, TAU); ctx.fill(); });
    ctx.restore();
    return { r, isos, nuc, out: isos[0], f };
  }
  const ghost = st === "ghost";
  isos.forEach((p, i) => {
    path(ctx, p);
    ctx.strokeStyle = ghost ? (i ? "rgba(244,241,234,.22)" : "rgba(244,241,234,.5)") : st === "plate" && !i ? C.text : heat(HT[i] + E * 40 / 256);
    ctx.stroke();
  });
  if (st === "plate") {
    const out = isos[0];
    path(ctx, iso(g, f, cx, cy, r * 0.9, true, t, tum, nv)); ctx.strokeStyle = "rgba(244,241,234,.45)"; ctx.stroke();   /* the pellicle, doubled */
    ctx.strokeStyle = "rgba(244,241,234,.55)"; ctx.beginPath();                                                    /* the cilia, beating */
    for (let i = 0; i < out.length - 1; i += 2) {
      const [x, y] = out[i], [x2, y2] = out[i + 1], nx = y2 - y, ny = -(x2 - x), l = Math.hypot(nx, ny) || 1;
      const len = (1.6 + 1.2 * Math.sin(i * 0.45 - t * 7)) * (1 + f.fray / 200) * (r > 14 ? 1.3 : 0.8);
      ctx.moveTo(x, y); ctx.lineTo(x + nx / l * len, y + ny / l * len);
    }
    ctx.stroke();
    for (let k = 0; k < 14; k++) {                                                                                 /* granules */
      const a = hash(k * 31 + 7) * TAU, d = r * 0.45 * Math.sqrt(hash(k * 17 + 3));
      ctx.fillStyle = k % 3 ? "rgba(255,122,0,.8)" : "rgba(255,212,0,.8)"; ctx.fillRect(cx + Math.cos(a + t * 0.1) * d * g.ax / 16, cy + Math.sin(a + t * 0.1) * d, 1, 1);
    }
    const nvc = o.voices ?? 1;                                                                                      /* food vacuoles: a voice each */
    for (let k = 0; k < nvc; k++) {
      const a = -t * 0.3 + k * TAU / Math.max(nvc, 1) + 1, d = r * 0.3, x = cx + Math.cos(a) * d * g.ax / 16, y = cy + Math.sin(a) * d * 0.8, rv = r * 0.12 + 0.6;
      ring(ctx, x, y, rv, C.pink); ctx.fillStyle = C.red; ctx.fillRect(x - 0.5, y - 0.5, 1.5, 1.5);
    }
    const b = o.beat ?? 0, sx = cx + r * 0.42 * g.ax / 16, sy = cy - r * 0.18;                                       /* the contractile vacuole */
    star(ctx, sx, sy, r * (0.1 + 0.12 * (1 - b)), 6, C.orange, t * 0.4);
    o.cv = [sx, sy];
  }
  tum.forEach((u) => {                                                                                             /* tumours: hot hatched nodules */
    const p = atAngle(isos[0], u.a), q = atAngle(isos[1], u.a), x = (p[0] + q[0]) / 2, y = (p[1] + q[1]) / 2, rt = Math.max(1.5, r * u.h * 0.55);
    ctx.save(); ctx.beginPath(); ctx.arc(x, y, rt, 0, TAU); ctx.clip(); hatch(ctx, 0.8, 2, C.red, x, y, rt + 2); ctx.restore();
    ring(ctx, x, y, rt, C.orange); ctx.fillStyle = heat(240); ctx.fillRect(x - 0.5, y - 0.5, 1.5, 1.5);
  });
  nuc.forEach(([x, y]) => { ring(ctx, x, y, r * 0.15, ghost ? "rgba(244,241,234,.5)" : st === "plate" ? C.cyan : heat(214 + E * 42 / 256)); ring(ctx, x, y, r * 0.06, ghost ? C.dim : heat(240)); });
  return { r, isos, nuc, out: isos[0], f, cv: o.cv };
}
const tipOf = (pts) => pts.reduce((m, p) => (p[2] > m[2] ? p : m), pts[0]);
function atAngle(pts, a) { let best = pts[0], bd = 9; for (const p of pts) { const d = Math.abs(Math.atan2(Math.sin(p[3] - a), Math.cos(p[3] - a))); if (d < bd) { bd = d; best = p; } } return best; }

/* ---------------------------------------------------------------- the song --- */
const TR = [
  { n: "ACID", eng: "ANALOG", sp: "RADIOLARIA", ab: "RAD", id: "D732", g: G.rad, f: { spikes: 24, len: 98, wob: 66, lob: 3, nuc: 2, ripple: 202 }, db: "-6.0", lv: 0.78, pan: -0.31, pat: 0xB2B2, amp: 0.9, tau: 0.16, v: 1 },
  { n: "PAD", eng: "PHASE", sp: "AMOEBA", ab: "AMO", id: "S209", g: G.amo, f: { spikes: 5, len: 140, wob: 150, lob: 2, nuc: 1 }, db: "-14", lv: 0.55, pan: 0.2, pat: 0x8080, amp: 0.55, tau: 0.9, v: 4 },
  { n: "PULS", eng: "FM6", sp: "POLLEN", ab: "POL", id: "R078", g: G.pol, f: { spikes: 14, len: 110, wob: 80, lob: 6, nuc: 3, sep: 90 }, db: "-24", lv: 0.3, pan: 0.6, pat: 0x5555, amp: 0.5, tau: 0.1, mute: true, v: 2 },
  { n: "DRUM", eng: "DRUM-X", sp: "COLONY", ab: "COL", id: "F802", g: G.drm, f: { spikes: 12, len: 120, wob: 60, lob: 3, nuc: 1 }, db: "-1.0", lv: 0.92, pan: 0, pat: 0x8888, amp: 1, tau: 0.14, v: 1 },
  { n: "BRAS", eng: "VOICE", sp: "HYDRA", ab: "HYD", id: "V763", g: G.hyd, f: { spikes: 6, len: 160, wob: 90, lob: 3, nuc: 2, sep: 120 }, db: "-4.0", lv: 0.7, pan: -0.7, pat: 0x2424, amp: 0.75, tau: 0.35, v: 3 },
  { n: "FULL", eng: "WHEEL", sp: "CILIATE", ab: "CIL", id: "E940", g: G.cil, f: { spikes: 30, len: 80, wob: 70, lob: 5, nuc: 1 }, db: "-4.0", lv: 0.66, pan: 0.45, pat: 0x0A0A, amp: 0.7, tau: 0.22, v: 2 },
];
const STEP = 60 / 124 / 4;
