// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// The FM-1 USB-MIDI update protocol over WebMIDI (after tools/fm1_install.py).
// Step 1: the running firmware (official
// or Felucca) reads parts of the package and stages the update loader. Step 2:
// the loader reads the whole image and writes it. Both steps are "the device
// asks, we answer": cmd 0x30 read requests on the logical image.

const HS_QUERY = [0xF0, 0x00, 0x32, 0x45, 0x00, 0x00, 0x00, 0x40, 0x7F, 0xF7];
const UPGRADE = [0xF0, 0x22, 0x24, 0x35, 0x7F, 0xF7];
const MAXDATA = 512;
export const sleep = (ms) => new Promise((r) => setTimeout(r, ms));
const IS_OTA = (id) => /^ota-/i.test(id.model);
/* (0.9.11) every wait of the update, in ms; the pages keep these, a test passes short ones. Longer than before: a slow
 * USB hub, and Windows installing a driver the first time the update loader (another USB identity) appears, took
 * longer than the 30 s the loader had */
export const UPDATE_TIMES = {
  hs: 1000, hsTries: 2,        // the identity query: an answer within hs, tries times per port
  findTries: 3, backoff: 400,  // a device not found: scan again after backoff, 2 x backoff...
  settle1: 2000, idle1: 12000, upgradeTries: 3,   // step 1: the running firmware's first request; UPGRADE again if none
  loaderSettle: 3000, loader: 90000,              // the update loader's port appearing
  settle2: 2000, idle2: 180000,                   // step 2: the loader's requests
  back: 90000, poll: 1000, refreshEvery: 5,       // the new firmware's port; a fresh MIDIAccess every 5 polls
  info: 2500, infoTries: 3,                       // the editor's INFO (fm1diag.js fwKind): JIANT, Felucca, Skiff...
};
/* the ports worth asking for an identity: the FM-1 (official: its own name or the USB composite's), JIANT, Felucca and
 * its forks, the update loaders. Other gear is not probed, except a port that appears while we wait for the loader */
export const PORT_RE = /fm-?1|felucca|jiant|skiff|ota|composite|sinco|usb-midi|m-?vave|cuvave/i;
/* Windows names one device's ports "FM-1", "2- FM-1", "MIDIIN2 (FM-1)" / "MIDIOUT2 (FM-1)", "FM-1 1": the device's name */
export const portBase = (name) => String(name || "").trim().replace(/^\d+\s*-\s*/, "")
  .replace(/^MIDI(?:IN|OUT)\d*\s*\((.*)\)$/i, "$1").replace(/\s+\d+$/, "").trim().toLowerCase();
/* the output of an input: the same name, else the same device name, else the only live output of an FM-1-like name */
export function pairOutput(input, outs) {
  const live = outs.filter((o) => o.state !== "disconnected");
  const same = live.find((o) => o.name === input.name);
  if (same) return same;
  const base = portBase(input.name), byBase = live.filter((o) => portBase(o.name) === base);
  if (byBase.length === 1) return byBase[0];
  const named = live.filter((o) => PORT_RE.test(o.name || ""));
  return PORT_RE.test(input.name || "") && named.length === 1 ? named[0] : null;
}

// errors carry a code the pages translate: notfound, busy (a port another program holds), noanswer (a port that
// does not answer the identity query), model, noupdate (the firmware never asked for the update), stopped, noloader,
// lost, noreturn, mismatch (detail: the identity the device reports), badreq
export function fail(code, msg, detail = "") { const e = new Error(msg); e.code = code; e.detail = detail; return e; }

export function pack7(data) {
  const out = []; let acc = 0, nb = 0;
  for (const b of data) {
    acc |= b << nb; nb += 8;
    while (nb >= 7) { out.push(acc & 0x7F); acc >>>= 7; nb -= 7; }
  }
  if (nb) out.push(acc & 0x7F);
  return out;
}

export function unpack7(s) {
  const out = []; let acc = 0, nb = 0;
  for (const b of s) {
    acc |= (b & 0x7F) << nb; nb += 7;
    while (nb >= 8) { out.push(acc & 0xFF); acc >>>= 8; nb -= 8; }
  }
  return out;
}

function response(addr, data, fl = 0) {
  const len = data.length;
  const body = [0x00, 0x59, 0x30, (len + 8) & 0xFF, ((len + 8) >> 8) & 0xFF, 0, fl,
    addr & 0xFF, (addr >>> 8) & 0xFF, (addr >>> 16) & 0xFF, (addr >>> 24) & 0xFF,
    len & 0xFF, (len >> 8) & 0xFF, 0, ...data];
  let s = 0;
  for (let i = 6; i < body.length; i++) s += body[i];
  body.push(~s & 0xFF);
  return [0xF0, ...pack7(body), 0xF7];
}

function parseRequest(pkt) {
  if (pkt[0] !== 0xF0 || pkt[pkt.length - 1] !== 0xF7) return null;
  const u = unpack7(pkt.slice(1, -1));
  if (u.length !== 15 || u[0] !== 0 || u[1] !== 0x59 || u[2] !== 0x30) return null;
  let s = 0;
  for (let i = 6; i < 14; i++) s += u[i];
  if ((~s & 0xFF) !== u[14]) return null;
  return { fl: u[6], addr: (u[7] | (u[8] << 8) | (u[9] << 16) | (u[10] << 24)) >>> 0, len: u[11] | (u[12] << 8) | (u[13] << 16) };
}

function parseIdentity(pkt) {
  const d = unpack7(pkt.slice(1, -1));
  if (d.length !== 34 || d[0] !== 0 || d[1] !== 0x59 || d[2] !== 0x11) return null;
  const txt = String.fromCharCode(...d.slice(6, 33)).replace(/\0+$/, "");
  const m = /^([^_]+)_(\d+)$/.exec(txt);
  if (m) return { text: txt, model: m[1], version: parseInt(m[2], 10) };
  /* (0.9.11) an identity of another shape (an old or unknown firmware): kept, so the page can name it */
  return /^[\x20-\x7E]+$/.test(txt) ? { text: txt, model: txt.split("_")[0], version: NaN } : null;
}

// one MIDI in/out pair with a SysEx queue
class Link {
  constructor(input, output) {
    this.input = input; this.output = output; this.q = []; this.waiter = null;
    input.onmidimessage = (ev) => {
      if (ev.data[0] !== 0xF0) return;
      this.q.push(Array.from(ev.data));
      if (this.waiter) { const w = this.waiter; this.waiter = null; w(); }
    };
  }
  get lost() { return this.input.state === "disconnected" || this.output.state === "disconnected"; }
  // false when the port is gone (unplugged): WebMIDI throws on a disconnected output
  send(bytes) {
    try { this.output.send(bytes); return true; } catch (_) { return false; }
  }
  drain() { this.q.length = 0; }
  async read(ms) {
    if (!this.q.length) {
      let tm;
      await Promise.race([new Promise((r) => { this.waiter = r; }), new Promise((r) => { tm = setTimeout(r, ms); })]);
      clearTimeout(tm);
      this.waiter = null;
    }
    return this.q.shift() || null;
  }
  close() { this.input.onmidimessage = null; }
}

async function handshake(link, tries = 3, ms = 1000) {
  link.drain();
  for (let t = 0; t < tries; t++) {
    if (!link.send(HS_QUERY)) return null;
    const end = Date.now() + ms * (1 + t);         // (0.9.11) a little longer each try
    while (Date.now() < end) {
      const p = await link.read(end - Date.now());
      if (!p) break;
      const id = parseIdentity(p);
      if (id) return id;
    }
  }
  return null;
}


export class Updater {
  /* opts.times: UPDATE_TIMES overrides; opts.refresh: async () => a fresh MIDIAccess (some systems list a port that
   * appears later, the update loader's, only in a new one) */
  constructor(access, log, opts = {}) {
    this.access = access;
    this.log = log || (() => {});
    this.t = { ...UPDATE_TIMES, ...(opts.times || {}) };
    this.refresh = opts.refresh || null;
    this.lastScan = { ports: [], outputs: [] };
    this.baseline = null;                          // the port names of the first scan (unnamed new ones are probed later)
    this.sawNamed = false;
  }

  /* one pass over the inputs: named ports ("named"), or other ports ("other": the ones absent from the first scan
   * unless all is true). Each port's outcome goes to lastScan (the installer's report) */
  async scan(filter, mode = "named", all = false) {
    const outs = [...this.access.outputs.values()];
    const scan = this.lastScan;
    scan.outputs = outs.map((o) => `${o.name} [${o.state}${o.connection ? "/" + o.connection : ""}]`);
    for (const input of this.access.inputs.values()) {
      const named = PORT_RE.test(input.name || "");
      if ((mode === "named") !== named) continue;
      if (!named && !all && this.baseline && this.baseline.has(input.name)) continue;
      const rec = { name: input.name || "?", manufacturer: input.manufacturer || "", state: input.state, result: "" };
      scan.ports.push(rec);
      if (input.state === "disconnected") { rec.result = "disconnected"; continue; }
      const output = pairOutput(input, outs);
      if (!output) { rec.result = "no output port"; continue; }
      /* Windows: a port another program holds (the M-VAVE app, a DAW, MIDI-OX) cannot be opened: open() rejects */
      try { await input.open(); await output.open(); }
      catch (e) { rec.result = `busy (${(e && e.name) || "open failed"})`; rec.busy = true; this.log(`port ${input.name}: ${rec.result}`); continue; }
      const link = new Link(input, output);
      const id = await handshake(link, this.t.hsTries, this.t.hs);
      rec.result = id ? id.text : "no answer"; rec.answered = !!id;
      if (id && (!filter || filter(id))) return { link, id, name: input.name };
      link.close();
    }
    return null;
  }

  // the first in/out pair that answers the handshake and passes filter; tries scans with a growing pause between them
  async find(filter, { tries = 1, others = true } = {}) {
    for (let k = 0; k < tries; k++) {
      if (k) await sleep(this.t.backoff * 2 ** (k - 1));
      this.lastScan = { ports: [], outputs: [] };
      const dev = await this.scan(filter, "named");
      const namedSeen = this.lastScan.ports.length > 0;
      if (namedSeen) this.sawNamed = true;
      /* no FM-1-like port there: the other ports. All of them while no FM-1-like port was ever seen (an FM-1 under a
       * name we do not know); else only the ones absent from the first scan (the update loader under a new name) */
      const d2 = dev || (others && !namedSeen ? await this.scan(filter, "other", !this.sawNamed) : null);
      if (!this.baseline) this.baseline = new Set([...this.access.inputs.values()].map((p) => p.name));
      if (d2) return d2;
    }
    return null;
  }

  // why find() found nothing, as an error the page translates (from the last scan)
  notFound(what = "FM-1") {
    const ports = this.lastScan.ports;
    const busy = ports.filter((p) => p.busy), quiet = ports.filter((p) => p.result === "no answer");
    if (busy.length) return fail("busy", `${what}: the port ${busy.map((p) => p.name).join(", ")} is in use by another program`, busy.map((p) => p.name).join(", "));
    if (quiet.length) return fail("noanswer", `${what}: ${quiet.map((p) => p.name).join(", ")} does not answer`, quiet.map((p) => p.name).join(", "));
    return fail("notfound", `${what} not found (USB cable, and no other app using it?)`);
  }

  async waitFor(filter, ms) {
    const end = Date.now() + ms;
    for (let n = 1; Date.now() < end; n++) {
      const dev = await this.find(filter);
      if (dev) return dev;
      if (this.refresh && n % this.t.refreshEvery === 0) {
        try { const a = await this.refresh(); if (a) { this.access = a; this.log("MIDI access refreshed"); } } catch (_) {}
      }
      await sleep(this.t.poll);
    }
    return null;
  }

  // serve read requests until the device asks for one of the finish addresses; stops early
  // when the port disappears (lost: true) instead of waiting out idleMs
  async serve(link, image, finishAddr, idleMs, onProgress) {
    let served = 0, last = Date.now();
    for (;;) {
      const pkt = await link.read(Math.min(1000, idleMs));
      if (!pkt) {
        if (link.lost) return { served, finished: false, lost: true };
        if (Date.now() - last > idleMs) return { served, finished: false };
        continue;
      }
      const r = parseRequest(pkt);
      if (!r) continue;
      last = Date.now();
      if (r.addr === 0xE0000000 || r.addr === 0xF0000000) {
        link.send(response(r.addr, [...new TextEncoder().encode("success"), 0]));
        if (r.addr === finishAddr) return { served, finished: true };
        continue;
      }
      if (r.len > MAXDATA || r.addr + r.len > image.length) throw fail("badreq", `bad request ${r.addr.toString(16)}+${r.len}`);
      await sleep(10);
      if (!link.send(response(r.addr, Array.from(image.subarray(r.addr, r.addr + r.len)), r.fl))) {
        return { served, finished: false, lost: true };
      }
      served++;
      if (onProgress) onProgress(served, r.addr);
    }
  }

  // full install: running firmware -> loader -> new firmware
  async install(image, product, onStep) {
    const step = onStep || (() => {});
    const dev = await this.find((id) => !IS_OTA(id), { tries: this.t.findTries });
    if (!dev) throw this.notFound();
    const [model] = product.split("_");
    if (dev.id.model !== model) { dev.link.close(); throw fail("model", `the device is ${dev.id.text}, the package is for ${product}`, dev.id.text); }
    step("start", dev.id.text);
    /* (0.9.11) a firmware that does not start reading after UPGRADE (busy, or the message lost on a hub) is asked
     * again; once it has read anything it is never asked twice */
    let s1 = { served: 0, finished: false };
    for (let k = 0; k < this.t.upgradeTries; k++) {
      if (k) this.log(`no update request: UPGRADE again (${k + 1})`);
      dev.link.drain();
      if (!dev.link.send(UPGRADE)) { s1 = { served: 0, finished: false, lost: true }; break; }
      await sleep(this.t.settle1);
      s1 = await this.serve(dev.link, image, 0xE0000000, this.t.idle1, (n) => step("verify", n));
      if (s1.finished || s1.lost || s1.served) break;
    }
    dev.link.close();
    if (!s1.finished && !s1.lost && !s1.served) throw fail("noupdate", `${dev.id.text} did not start the update: nothing was written`, dev.id.text);
    if (!s1.finished) throw fail(s1.lost ? "lost" : "stopped", `the device ${s1.lost ? "was disconnected" : "stopped"} after ${s1.served} requests: nothing was written`);
    step("loader");
    await sleep(this.t.loaderSettle);
    const ota = await this.waitFor((id) => id.model === "ota-" + model, this.t.loader);
    if (!ota) throw fail("noloader", "the update loader did not appear. Replug the USB cable and press Install again: the device stays in update mode until it is finished.");
    const s2 = await this.write(ota, image, step);
    if (!s2.finished) throw fail(s2.lost ? "lost" : "stopped", `the loader ${s2.lost ? "was disconnected" : "stopped"} after ${s2.served} requests. Replug and press Install again to resume.`);
    step("reboot");
    await sleep(this.t.loaderSettle);
    const back = await this.waitFor((id) => !IS_OTA(id), this.t.back);
    if (!back) throw fail("noreturn", "the device did not come back: power-cycle it");
    back.link.close();
    if (back.id.text !== product) throw fail("mismatch", `installed, but the device reports ${back.id.text}`, back.id.text);
    step("done", back.id.text);
    return back.id.text;
  }

  // step 2 with the loader found by find()/waitFor()
  async write(ota, image, step) {
    this.log(`loader: ${ota.id.text} on ${ota.name}`);
    ota.link.send(UPGRADE);
    await sleep(this.t.settle2);
    const s2 = await this.serve(ota.link, image, 0xF0000000, this.t.idle2, (n) => step("write", n));
    ota.link.close();
    return s2;
  }

  // resume: the device is already in update mode (loader) -> true when the write finished
  async resume(image, onStep, product = null) {
    const model = product && product.split("_")[0];
    const ota = await this.find((id) => model ? id.model === "ota-" + model : IS_OTA(id), { tries: 2 });
    if (!ota) return false;
    const step = onStep || (() => {});
    const result = await this.write(ota, image, step);
    if (!result.finished) return false;
    if (product) {
      step("reboot");
      await sleep(this.t.loaderSettle);
      const back = await this.waitFor((id) => id.model === model, this.t.back);
      if (!back) throw fail("noreturn", "The FM-1 did not return after resuming the write.");
      back.link.close();
      if (back.id.text !== product) throw fail("mismatch", "The FM-1 reports a different firmware after resuming.", back.id.text);
      step("done", back.id.text);
    }
    return true;
  }
}
