// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// (0.6.91) The song generator heard in the browser build: every genre (or the ones named) made by web_gen on the wasm
// as the page runs it, played SECONDS each and written as WAV files to listen to, with a line of measurements a piece
// (peak, RMS, the tracks that sound, BPM). Also a check: the same genre and seed twice give the same samples.
//   node web/emu/gen_bench.mjs build/emu/felucca.wasm OUTDIR [SECONDS] [GENRE,GENRE..] [SEED]
import fs from "fs";

const wasmPath = process.argv[2] || "build/emu/felucca.wasm", out = process.argv[3] || "build/gen_wav";
const secs = +(process.argv[4] || 12), only = process.argv[5] ? process.argv[5].split(",").map(Number) : null;
const seed0 = +(process.argv[6] || 11);
const bytes = fs.readFileSync(wasmPath);
const B = { PLAY: 10 };
fs.mkdirSync(out, { recursive: true });

async function device() {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports, mem = ex.memory;
  ex._initialize();
  ex.web_nor_erase();
  ex.web_boot();
  const d = { ex, mem, peak: 0, sq: 0, n: 0, sig: 0, kept: [] };
  d.render = (ms, keep) => {
    const n = Math.round(ms * 44.1);
    for (let k = 0; k < n; k += 128) {
      const m = Math.min(128, n - k);
      ex.web_render(m);
      const l = new Float32Array(mem.buffer, ex.web_out_l(), m), r = new Float32Array(mem.buffer, ex.web_out_r(), m);
      for (let i = 0; i < m; i++) {
        if (keep) {
          d.peak = Math.max(d.peak, Math.abs(l[i]), Math.abs(r[i]));
          d.sq += l[i] * l[i] + r[i] * r[i]; d.n += 2;
          d.sig = (d.sig * 31 + Math.round(l[i] * 32768) + 7 * Math.round(r[i] * 32768)) % 1000000007;
          d.kept.push(l[i], r[i]);
        }
      }
    }
  };
  d.tap = (b) => { ex.web_buttons(1 << b); d.render(60); ex.web_buttons(0); d.render(60); };
  d.cstr = (p) => { const u = new Uint8Array(mem.buffer, p); let s = ""; for (let i = 0; u[i]; i++) s += String.fromCharCode(u[i]); return s; };
  return d;
}
function wav(path, samples) {
  const n = samples.length, buf = Buffer.alloc(44 + n * 2);
  buf.write("RIFF", 0); buf.writeUInt32LE(36 + n * 2, 4); buf.write("WAVE", 8); buf.write("fmt ", 12);
  buf.writeUInt32LE(16, 16); buf.writeUInt16LE(1, 20); buf.writeUInt16LE(2, 22); buf.writeUInt32LE(44100, 24);
  buf.writeUInt32LE(44100 * 4, 28); buf.writeUInt16LE(4, 32); buf.writeUInt16LE(16, 34); buf.write("data", 36);
  buf.writeUInt32LE(n * 2, 40);
  for (let i = 0; i < n; i++) buf.writeInt16LE(Math.max(-32768, Math.min(32767, Math.round(samples[i] * 32767))), 44 + i * 2);
  fs.writeFileSync(path, buf);
}

const probe = await device();
probe.render(2500);
const ngen = probe.ex.web_gen_count();
const names = []; for (let g = 0; g < ngen; g++) names.push(probe.cstr(probe.ex.web_gen_name(g)));
const list = only || names.map((_, i) => i);
let fails = 0;
async function piece(g, seed, keep) {
  const d = await device();
  d.render(2500);                                          // (the splash, HOME)
  d.ex.web_gen(g + 1, seed, 50, 0);
  d.render(100);
  d.tap(B.PLAY);
  d.render(secs * 1000, keep);
  return d;
}
for (const g of list) {
  const d = await piece(g, seed0, true);
  const name = names[g].replace(/[^A-Z0-9]+/g, "_");
  const path = `${out}/${String(g).padStart(2, "0")}_${name}_s${seed0}.wav`;
  wav(path, d.kept);
  const rms = Math.sqrt(d.sq / Math.max(1, d.n));
  const last = d.cstr(d.ex.web_gen_last());
  const ok = d.peak > 0.05 && d.peak <= 1.0 && rms > 0.01;
  if (!ok) fails++;
  console.log(`gen: ${last.padEnd(20)} peak ${d.peak.toFixed(3)} rms ${rms.toFixed(3)} playing ${d.ex.web_playing()}  ${path} ${ok ? "" : "FAIL"}`);
}
{ // deterministic on the wasm: the same genre and seed, the same samples
  const a = await piece(list[0], seed0, true), b = await piece(list[0], seed0, true);
  const ok = a.sig === b.sig && a.kept.length === b.kept.length;
  if (!ok) fails++;
  console.log(`gen: ${names[list[0]]} seed ${seed0} twice: ${ok ? "the same samples" : "DIFFERENT samples FAIL"}`);
}
console.log(fails ? `gen: ${fails} FAILED` : "gen: all ok");
process.exit(fails ? 1 : 0);
