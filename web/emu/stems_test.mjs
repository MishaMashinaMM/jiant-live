// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// (JIANT 0.7.1) Stems from the browser build (web/emu/stems.js) in Node:
//   node web/emu/stems_test.mjs build/emu/felucca.wasm
// A device makes a piece (web_gen, FORM CLUB: a song with rows), its objects come out as a backup file would hold
// them (web_bk_len into web_bk_buf), a fresh device takes them (web_bk_put) and renders: the live section as a loop of
// two bars, each part alone and the whole; then the song by its rows to its end. Checks: the files, their lengths
// equal, every stem inside the mix's length, a part alone is not the mix, the parts left out are silent, the ZIP's
// directory; CRC-32 of the known string.
import fs from "fs";
import { crc32, wavStereo, zipStore, deviceFromBackup, renderPass, renderStems } from "./stems.js";

const wasmPath = process.argv[2] || "build/emu/felucca.wasm";
const bytes = fs.readFileSync(wasmPath);
let fails = 0;
const check = (what, ok) => { console.log(`stems: ${what.padEnd(76)} ${ok ? "ok" : "FAIL"}`); if (!ok) fails++; };
const peak = (a) => { let p = 0; for (const v of a) p = Math.max(p, Math.abs(v)); return p; };

check("CRC-32 of \"123456789\" is 0xCBF43926 (zlib's)", crc32(new TextEncoder().encode("123456789")) === 0xCBF43926);
{
  const w = wavStereo(new Float32Array([0, 0.5, -1]), new Float32Array([1, 0, 0.25]));
  const dv = new DataView(w.buffer);
  check("WAV: 44-byte header, 16-bit stereo 44.1 kHz, the samples clipped and scaled", w.length === 44 + 12 && dv.getUint32(24, true) === 44100 &&
        dv.getInt16(44, true) === 0 && dv.getInt16(46, true) === 32767 && dv.getInt16(48, true) === 16384 && dv.getInt16(52, true) === -32767);
  const z = zipStore([{ name: "a.wav", data: w }, { name: "b.txt", data: new TextEncoder().encode("hi") }]);
  const zv = new DataView(z.buffer), end = z.length - 22;
  check("ZIP: the end record counts two entries, the central directory follows the data", zv.getUint32(end, true) === 0x06054B50 &&
        zv.getUint16(end + 8, true) === 2 && zv.getUint32(0, true) === 0x04034B50 && zv.getUint32(zv.getUint32(end + 16, true), true) === 0x02014B50);
}

/* the piece: a device of its own, the generator's CLUB form (sections and rows) */
async function pieceBackup() {
  const { instance } = await WebAssembly.instantiate(bytes, {});
  const ex = instance.exports, mem = ex.memory;
  ex._initialize(); ex.web_nor_erase(); ex.web_master(1023); ex.web_seed(77); ex.web_boot();
  ex.web_render(4096);
  ex.web_gen(6, 11, 60, 2);                            // TECHNO, seed 11, ENERGY 60, FORM CLUB
  for (let k = 0; k < 20; k++) ex.web_render(1024);    // (the main loop writes the sections)
  const objects = [];
  for (const id of [10, 40, 41, 42, 43, 0]) {
    const n = ex.web_bk_len(id);
    if (!n) continue;
    objects.push({ id, bytes: new Uint8Array(mem.buffer, ex.web_bk_buf(), n).slice() });
  }
  return { rows: ex.web_song_rows(), objects };
}

const bk = await pieceBackup();
check(`the generator's piece: the live music, the song index and sections (${bk.objects.length} objects, ${bk.rows} rows)`,
      bk.objects.some((o) => o.id === 0) && bk.objects.some((o) => o.id === 10) && bk.rows >= 2);

{ /* a fresh device from those objects: the live section as a two-bar loop, each part alone and whole */
  const dev = await deviceFromBackup(bytes, bk);
  check("a fresh device takes the objects; the song has the rows", dev.ex.web_song_rows() === bk.rows);
  const out = await renderStems(bytes, bk, { mode: "loop", bars: 2, tail: 1 });
  const names = out.files.map((f) => f.name);
  const mixLen = (out.files.find((f) => f.name === "MIX.wav").data.length - 44) / 4;
  check(`loop: MIX and the sounding parts' stems (${names.join(" ")}), ${out.seconds.toFixed(2)} s`,
        names.includes("MIX.wav") && names.length >= 2 && out.files.every((f) => (f.data.length - 44) / 4 === mixLen) && mixLen > 2 * 4 * 44100 / 2);
  check("loop: a ZIP of them", out.zip.length > out.files.reduce((n, f) => n + f.data.length, 0) && new DataView(out.zip.buffer).getUint32(0, true) === 0x04034B50);
  const d2 = await deviceFromBackup(bytes, bk), all = renderPass(d2, 63, { mode: "loop", bars: 1, tail: 0.2 });
  check("every part left out: silence", peak(all.l) === 0 && peak(all.r) === 0);
  const d3 = await deviceFromBackup(bytes, bk), one = renderPass(d3, 63 & ~(1 << 3), { mode: "loop", bars: 1, tail: 0.2 });
  const d4 = await deviceFromBackup(bytes, bk), mix = renderPass(d4, 0, { mode: "loop", bars: 1, tail: 0.2 });
  let diff = 0;
  for (let i = 0; i < Math.min(one.l.length, mix.l.length); i++) diff = Math.max(diff, Math.abs(one.l[i] - mix.l[i]));
  check("T4 alone sounds and is not the mix; the passes line up (equal lengths)", peak(one.l) > 0.01 && diff > 0.01 && one.l.length === mix.l.length);
  const d5 = await deviceFromBackup(bytes, bk), again = renderPass(d5, 63 & ~(1 << 3), { mode: "loop", bars: 1, tail: 0.2 });
  let same = again.l.length === one.l.length;
  for (let i = 0; same && i < one.l.length; i++) same = one.l[i] === again.l[i];
  check("the same pass twice: the same samples (deterministic from the flash and the seed)", same);
}
{ /* the song by its rows, to its end */
  const dev = await deviceFromBackup(bytes, bk);
  const t0 = Date.now(), song = renderPass(dev, 0, { mode: "song", song: 0, tail: 1 }), dt = (Date.now() - t0) / 1000;
  const bars = dev.ex.web_song_bars(), beat = dev.ex.web_beat_samples(), want = bars * 4 * beat + 44100;   /* (a LOOP song: once round, then the tail) */
  check(`song: ${bars} bars once round + the tail, ${(song.l.length / 44100).toFixed(1)} s of music in ${dt.toFixed(1)} s`,
        Math.abs(song.l.length - want) <= 128 && peak(song.l) > 0.01 && dev.ex.web_song_end() === 255);
}
console.log(fails ? `STEMS TESTS FAILED (${fails})` : "stems: all ok");
process.exit(fails ? 1 : 0);
