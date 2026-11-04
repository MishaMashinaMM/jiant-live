// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// (JIANT 0.7.1) Stems from the browser: a backup file (the editor's or the installer's "Back up", felucca-backup) into
// a fresh copy of the firmware (felucca.wasm), then a song (its rows, as PLAY in SONG mode plays them) or the live
// section (a loop of n bars) rendered once per part with every other part left out of the mix (fx.c mix_skip: the
// part keeps running, only its sound and its sends are left out), and once whole. Each pass is a new device from
// the same flash and seed, so the stems line up to the sample and sum to the mix before the master (the master's
// leveler, limiter and clip are not linear: the MIX file is the master as the FM-1 plays it). Every stem carries its
// own share of the DELAY, REVERB and CHORUS buses: what the part sends, nothing of the others. 16-bit stereo WAVs at
// 44.1 kHz in one ZIP (stored, no compression). Runs as a module Worker (index.html) and imports into Node
// (stems_test.mjs).
const RATE = 44100, BLOCK = 128, TRACKS = 6;
const SONG_IDS = [10, ...Array.from({ length: 32 }, (_, i) => 40 + i), 6, 7, 9, 0];   // the index, the songs, presets, live last

// ---------------------------------------------------------------------------------------------- CRC-32, WAV, ZIP
const CRC_T = (() => {
  const t = new Uint32Array(256);
  for (let n = 0; n < 256; n++) { let c = n; for (let k = 0; k < 8; k++) c = c & 1 ? 0xEDB88320 ^ (c >>> 1) : c >>> 1; t[n] = c >>> 0; }
  return t;
})();
export function crc32(bytes, crc = 0) {
  crc = ~crc >>> 0;
  for (let i = 0; i < bytes.length; i++) crc = CRC_T[(crc ^ bytes[i]) & 255] ^ (crc >>> 8);
  return (~crc) >>> 0;
}
const u16 = (v, a, o) => { a[o] = v & 255; a[o + 1] = (v >>> 8) & 255; };
const u32 = (v, a, o) => { u16(v & 0xFFFF, a, o); u16(v >>> 16, a, o + 2); };
/* a 16-bit stereo WAV of two Float32Arrays (clipped at full scale) */
export function wavStereo(l, r, rate = RATE) {
  const n = Math.min(l.length, r.length), out = new Uint8Array(44 + n * 4), dv = new DataView(out.buffer);
  const tag = (s, o) => { for (let i = 0; i < 4; i++) out[o + i] = s.charCodeAt(i); };
  tag("RIFF", 0); u32(36 + n * 4, out, 4); tag("WAVE", 8); tag("fmt ", 12); u32(16, out, 16); u16(1, out, 20); u16(2, out, 22);
  u32(rate, out, 24); u32(rate * 4, out, 28); u16(4, out, 32); u16(16, out, 34); tag("data", 36); u32(n * 4, out, 40);
  for (let i = 0, o = 44; i < n; i++, o += 4) {
    dv.setInt16(o, Math.max(-32768, Math.min(32767, Math.round(l[i] * 32767))), true);
    dv.setInt16(o + 2, Math.max(-32768, Math.min(32767, Math.round(r[i] * 32767))), true);
  }
  return out;
}
/* a ZIP archive of {name, data} entries, stored (method 0): local headers, the central directory, the end record */
export function zipStore(files, when = new Date()) {
  const dosTime = (when.getHours() << 11) | (when.getMinutes() << 5) | (when.getSeconds() >> 1);
  const dosDate = ((when.getFullYear() - 1980) << 9) | ((when.getMonth() + 1) << 5) | when.getDate();
  const enc = new TextEncoder(), parts = [], central = [];
  let off = 0;
  for (const f of files) {
    const name = enc.encode(f.name), crc = crc32(f.data), lh = new Uint8Array(30 + name.length), ch = new Uint8Array(46 + name.length);
    u32(0x04034B50, lh, 0); u16(20, lh, 4); u16(0, lh, 6); u16(0, lh, 8); u16(dosTime, lh, 10); u16(dosDate, lh, 12);
    u32(crc, lh, 14); u32(f.data.length, lh, 18); u32(f.data.length, lh, 22); u16(name.length, lh, 26); u16(0, lh, 28); lh.set(name, 30);
    u32(0x02014B50, ch, 0); u16(20, ch, 4); u16(20, ch, 6); u16(0, ch, 8); u16(0, ch, 10); u16(dosTime, ch, 12); u16(dosDate, ch, 14);
    u32(crc, ch, 16); u32(f.data.length, ch, 20); u32(f.data.length, ch, 24); u16(name.length, ch, 28); u16(0, ch, 30); u16(0, ch, 32);
    u16(0, ch, 34); u16(0, ch, 36); u32(0, ch, 38); u32(off, ch, 42); ch.set(name, 46);
    parts.push(lh, f.data); central.push(ch);
    off += lh.length + f.data.length;
  }
  const cdSize = central.reduce((n, c) => n + c.length, 0), end = new Uint8Array(22);
  u32(0x06054B50, end, 0); u16(0, end, 4); u16(0, end, 6); u16(files.length, end, 8); u16(files.length, end, 10); u32(cdSize, end, 12); u32(off, end, 16); u16(0, end, 20);
  const total = off + cdSize + 22, out = new Uint8Array(total);
  let p = 0;
  for (const a of [...parts, ...central, end]) { out.set(a, p); p += a.length; }
  return out;
}

// ---------------------------------------------------------------------------------------------- the device
/* a fresh device from the wasm bytes with the backup's objects in it (web_bk_put, felucca_web.c): the song index and
 * the songs' sections into its flash, the user presets, the live music last */
export async function deviceFromBackup(wasmBytes, backup, seed = 0x4A49414E) {
  const { instance } = await WebAssembly.instantiate(wasmBytes, {});
  const ex = instance.exports, mem = ex.memory;
  ex._initialize();
  ex.web_nor_erase();
  ex.web_master(1023);                                  // the MASTER knob full: the DAC path at the mix's level
  if (ex.web_seed) ex.web_seed(seed);
  ex.web_boot();
  const buf = ex.web_bk_buf(), max = ex.web_bk_max(), byId = new Map((backup?.objects || []).map((o) => [o.id, o]));
  for (const id of SONG_IDS) {
    const o = byId.get(id);
    if (!o || !o.bytes || !o.bytes.length) continue;
    if (o.bytes.length > max) throw new Error(`backup object ${id}: ${o.bytes.length} bytes, more than the device takes`);
    new Uint8Array(mem.buffer, buf, o.bytes.length).set(o.bytes);
    const rc = ex.web_bk_put(id, o.bytes.length);
    if (rc) throw new Error(`backup object ${id} refused (${rc === 2 ? "not a valid object" : rc === 4 ? "flash write failed" : "unknown id"})`);
  }
  return { ex, mem };
}
/* one pass: the song (opts.mode "song", opts.song 0..7) or the live section looped opts.bars bars, with the parts of
 * skipMask left out; opts.tail seconds after the end (the FX tails); frames capped at opts.maxFrames. Returns {l, r} */
export function renderPass(dev, skipMask, opts, onFrames) {
  const { ex, mem } = dev, tail = Math.round((opts.tail ?? 3) * RATE), maxFrames = opts.maxFrames ?? RATE * 60 * 20;
  const l = [], r = [];
  let frames = 0, started = false, ending = -1, loopEnd = -1;
  ex.web_mix_skip(skipMask & 63);
  if (opts.mode === "song") {
    if (ex.web_song_select((opts.song | 0) & 7) && ex.web_song_cur() !== ((opts.song | 0) & 7)) throw new Error("the song could not be selected (stop the device first)");
    if (!ex.web_song_rows()) throw new Error(`song ${((opts.song | 0) & 7) + 1} has no rows: nothing to render`);
    const rc = ex.web_song_play();
    if (rc) throw new Error(rc === 1 ? "the song has no rows" : rc === 2 ? "the device is busy" : `section ${"ABCD"[rc - 3] || "?"} is empty`);
    const end = ex.web_song_end(), times = end === 255 ? (opts.loops || 1) : end >= 2 ? end : 1;   // a LOOP song: once round (opts.loops)
    loopEnd = times * ex.web_song_bars() * 4 * ex.web_beat_samples();   // (the rows' bars; the chain stops on its own before it unless it loops)
  } else {
    loopEnd = Math.max(1, opts.bars | 0) * 4 * ex.web_beat_samples();
    ex.web_play(1);
  }
  while (frames < maxFrames) {
    ex.web_render(BLOCK);
    l.push(new Float32Array(mem.buffer, ex.web_out_l(), BLOCK).slice());
    r.push(new Float32Array(mem.buffer, ex.web_out_r(), BLOCK).slice());
    frames += BLOCK;
    if (onFrames && !(frames % (BLOCK * 64))) onFrames(frames);
    if (ending >= 0) { if (frames >= ending) break; continue; }
    const playing = ex.web_playing();
    if (playing) started = true;
    if (opts.mode === "song") {
      if (started && !playing) ending = frames + tail;            // the song's end (the chain stopped the transport)
      else if (!started && frames > RATE) throw new Error("the song did not start");
      else if (frames >= loopEnd) { ex.web_play(0); ending = frames + tail; }   // a LOOP song: its rows once round
    } else if (frames >= loopEnd) { ex.web_play(0); ending = frames + tail; }
  }
  const n = ending >= 0 ? Math.min(ending, frames) : frames, L = new Float32Array(n), R = new Float32Array(n);
  for (let i = 0, o = 0; i < l.length && o < n; i++, o += BLOCK) { L.set(l[i].subarray(0, Math.min(BLOCK, n - o)), o); R.set(r[i].subarray(0, Math.min(BLOCK, n - o)), o); }
  return { l: L, r: R };
}
/* every stem: T1..T6 (the parts the device plays: TRACKS 4 leaves 5 and 6 out) each alone, then MIX (all), as WAV
 * files, trimmed to one length. Silent parts are left out of the files. onProgress(step, steps, text) */
export async function renderStems(wasmBytes, backup, opts = {}, onProgress = () => {}) {
  const files = [], passes = [];
  const first = await deviceFromBackup(wasmBytes, backup);
  const live = first.ex.web_trk_live ? first.ex.web_trk_live() : TRACKS;
  const stems = (opts.stems || Array.from({ length: live }, (_, i) => i)).filter((k) => k < live);
  const steps = stems.length + 1;
  onProgress(0, steps, "MIX");
  const mix = renderPass(first, 0, opts, (f) => onProgress(0, steps, `MIX ${(f / RATE).toFixed(0)} s`));
  const n = mix.l.length;
  for (let s = 0; s < stems.length; s++) {
    const k = stems[s];
    onProgress(s + 1, steps, `T${k + 1}`);
    const dev = await deviceFromBackup(wasmBytes, backup);
    const skip = (63 & ~(1 << k)) & 63;
    const p = renderPass(dev, skip, { ...opts, maxFrames: n }, (f) => onProgress(s + 1, steps, `T${k + 1} ${(f / RATE).toFixed(0)} s`));
    let peak = 0;
    for (let i = 0; i < p.l.length; i++) peak = Math.max(peak, Math.abs(p.l[i]), Math.abs(p.r[i]));
    passes.push({ k, peak, n: p.l.length });
    if (peak > 1 / 32768) files.push({ name: `T${k + 1}.wav`, data: wavStereo(p.l.subarray(0, n), p.r.subarray(0, n)) });
  }
  files.push({ name: "MIX.wav", data: wavStereo(mix.l, mix.r) });
  return { files, seconds: n / RATE, passes, zip: zipStore(files) };
}

// ---------------------------------------------------------------------------------------------- the Worker
if (typeof self !== "undefined" && typeof WorkerGlobalScope !== "undefined" && self instanceof WorkerGlobalScope) {
  self.onmessage = async (e) => {
    const { wasm, backup, opts } = e.data;
    try {
      const out = await renderStems(wasm, backup, opts, (step, steps, text) => self.postMessage({ type: "progress", step, steps, text }));
      self.postMessage({ type: "done", zip: out.zip, seconds: out.seconds, files: out.files.map((f) => f.name), passes: out.passes }, [out.zip.buffer]);
    } catch (err) {
      self.postMessage({ type: "error", message: String((err && err.message) || err) });
    }
  };
}
