// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// (JIANT 0.8.0) the lab film's LIVE VISUALS chapter: tools/video/visuals.html (the WebGL visuals of the stems app) drawn
// frame by frame in Chromium, moved by the film's own music (data.json: each track's level, the kicks) and turning
// style with each line of the chapter (timeline.json: the cue is the style); JPEGs in <dir>/vis that tutorial.html shows.
//   NODE_PATH=$(npm root -g) node tools/video/visuals_film.mjs build/news   (VSTART=frame: resume; VDATA: another data.json)
import { createRequire } from "module";
import fs from "fs";
import path from "path";
const require = createRequire(import.meta.url);
const { chromium } = require("playwright");
const dir = process.argv[2], W = +(process.env.VW || 960), H = +(process.env.VH || 540);
const TL = JSON.parse(fs.readFileSync(path.join(dir, "timeline.json"))), D = JSON.parse(fs.readFileSync(path.join(dir, process.env.VDATA || "data.json")));
const ch = TL.chapters.find((c) => c.id === "visuals");
const f0 = Math.max(0, Math.round((ch.start - 0.6) * 30)), f1 = Math.min(D.n - 1, Math.round(ch.end * 30) + 15), pre = 90;
fs.mkdirSync(path.join(dir, "vis"), { recursive: true });
const html = fs.readFileSync(path.join(path.dirname(new URL(import.meta.url).pathname), "visuals.html"), "utf8");
const b = await chromium.launch({ args: ["--use-gl=angle", "--use-angle=swiftshader", "--enable-unsafe-swiftshader", "--ignore-gpu-blocklist"] });
const p = await b.newPage({ viewport: { width: W, height: H } });
p.on("pageerror", (e) => console.error("page:", e.message));
await p.addInitScript(() => { window.FILM = true; });
await p.setContent('<!doctype html><html><head><meta charset=utf8></head><body style="margin:0">' + html + "</body></html>");
await p.evaluate(() => { RS = 1; resize(); });
const styleAt = (t) => { let s = ch.lines[0].cue, tc = -9, prev = s; for (const l of ch.lines) if (t >= l.start - 0.3) { prev = s; s = l.cue; tc = l.start - 0.3; } return [prev, s, tc]; };
let hit = 0; const t0 = Date.now();
const fs0 = +(process.env.VSTART || f0);   // (a render cut short resumed: from this frame, its pre-roll before it)
for (let f = fs0 - pre; f <= f1; f++) {
  const ff = Math.max(0, f), t = f / 30, [a, s, tc] = styleAt(Math.max(t, ch.lines[0].start));
  const E = (D.trk[ff] || [0, 0, 0, 0, 0, 0]).map((v) => Math.min(1.2, v * .85));
  hit = Math.max(D.kick[ff] || 0, (D.on[ff] || [0])[0] || 0, hit * .8);
  const m = t - tc < 1.2 ? (t - tc) / 1.2 : 1, A = m < 1 ? a : s;
  const url = await p.evaluate(([t, E, hit, A, B, m]) => { filmFrame(t, E, hit, A, B, m >= 1 ? 0 : m, 0); return document.getElementById("stage").toDataURL("image/jpeg", 0.9); }, [t, E, hit, A, s, m]);
  if (f >= fs0) fs.writeFileSync(path.join(dir, "vis", `${f}.jpg`), Buffer.from(url.split(",")[1], "base64"));
  if (f % 60 === 0) console.log(`${f - f0}/${f1 - f0} frames, ${((Date.now() - t0) / 1000).toFixed(0)} s`);
}
fs.writeFileSync(path.join(dir, "vis", "range.json"), JSON.stringify({ f0, f1 }));
await b.close();
