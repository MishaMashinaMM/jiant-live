// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
//
// (JIANT 0.9.11) The web installer's decisions, out of the page so web/test_web.mjs runs them against simulated FM-1s:
// what firmware the connected FM-1 runs (fwKind), the two flows (Install, Return to official V15), the browser and
// MIDI checks, the official file's checks, and the report the page copies (COPY REPORT). Every refusal is an error
// with a code (fm1ota.js fail) the page translates and explains (its troubleshooting box).
import { fail, sleep } from "./fm1ota.js";
import { BackupConnection, captureBackup } from "./fm1backup.js";
import { productOf } from "./fm1pkg.js";

/* ------------------------------------------------------------- what the FM-1 runs --- */
/* its identity (the update protocol's handshake, "FM-1_015"):
 *   FM-1_015            official V15                  -> Install writes over it
 *   FM-1_001..014       an older official firmware    -> Return to official V15 first (no backup: it cannot make one)
 *   FM-1_016..099       an official firmware newer than V15 (none known yet): Return to official V15 first
 *   FM-1_7XY            Skiff (a Felucca fork: tools/build.py PRODUCT "FM-1_7" X Y)
 *   FM-1_9XY            JIANT, Felucca or another Felucca fork: the editor's INFO tells which
 *   anything else       another custom firmware */
export function classifyIdentity(text) {
  const m = /^FM-1_(\d{3})$/.exec(String(text || ""));
  if (!m) return text ? "otherfw" : "none";
  const n = +m[1];
  if (n === 15) return "stock";
  if (n < 15) return "oldstock";
  if (n < 100) return "newstock";
  if (n >= 700 && n < 800) return "skiff";
  if (n >= 900) return "felucca9";
  return "otherfw";
}
export function infoVersion(a) {
  const z = a.indexOf(0);
  return z < 0 ? "" : String.fromCharCode(...a.slice(0, z));
}
/* (0.6.62) JIANT: its tag 0x4A in INFO, or an older JIANT's six tracks with a v0.x version */
export function isJiantInfo(a) {
  const z = a.indexOf(0); if (z < 0) return false;
  const version = String.fromCharCode(...a.slice(0, z));
  if (!/^(FELUCCA|JIANT) /i.test(version)) return false;
  let pos = z + 1; const engines = a[pos]; pos += 5;
  for (let i = 0; i < engines; i++) { const end = a.indexOf(0, pos); if (end < 0) return false; pos = end + 1; }
  const ntrk = a[pos], ext = a.slice(pos + 1);
  for (let i = 0; i + 2 < ext.length; i++) if (ext[i] === 0x4A && ext[i + 1] === 1 && ext[i + 2] === 1) return true;
  return ntrk === 6 && /^(FELUCCA|JIANT) v0\./i.test(version);
}
/* the complete backup (cmds 65..67, INFO 42 01 bit 0): JIANT, Felucca 1.0.3 and later, Skiff (all lay INFO out alike) */
export function archiveInfo(a) {
  let pos = a.indexOf(0); if (pos < 0) throw new Error("Invalid INFO");
  const version = String.fromCharCode(...a.slice(0, pos)); pos++;
  const engines = a[pos]; pos += 5;
  for (let i = 0; i < engines; i++) { const end = a.indexOf(0, pos); if (end < 0) throw new Error("Invalid INFO"); pos = end + 1; }
  const ext = a.slice(pos + 1);
  const supported = /^(FELUCCA|JIANT) /i.test(version) && ext[0] === 16 && ext[1] === 0x55 && ext[2] === 1 &&
    ext[4] === 0x4d && ext[5] === 1 && ext[8] === 0x42 && ext[9] === 1 && !!(ext[10] & 1);
  return { version, supported };
}

/* the running firmware: { kind, text (identity), version (INFO's), backup (a complete backup can be taken), port };
 * kind: none, stock, oldstock, newstock, jiant, felucca, skiff, otherfw. A Felucca-family firmware (7XY, 9XY) is asked
 * the editor's INFO (retried: a busy firmware answers late); no answer: otherfw (or skiff), no backup */
export async function fwKind(up) {
  const dev = await up.find((id) => !/^ota-/i.test(id.model), { tries: up.t.findTries });
  if (!dev) return { kind: "none" };
  const text = dev.id.text, cls = classifyIdentity(text), out = { kind: cls, text, version: "", backup: false, port: dev.name };
  const input = dev.link.input, output = dev.link.output;
  dev.link.close();
  Object.defineProperty(out, "ports", { value: { input, output }, enumerable: false });
  if (cls !== "felucca9" && cls !== "skiff") return out;
  const link = new BackupConnection(input, output);
  try {
    const a = await link.request([1, []], { timeout: up.t.info, retries: up.t.infoTries - 1 });
    out.version = infoVersion(a);
    out.backup = archiveInfo(a).supported;
    if (cls === "skiff" || /skiff/i.test(out.version)) out.kind = "skiff";
    else if (isJiantInfo(a)) out.kind = "jiant";
    else if (/^FELUCCA v?1\./i.test(out.version)) out.kind = "felucca";
    else out.kind = "otherfw";
  } catch (e) {
    out.kind = cls === "skiff" ? "skiff" : "otherfw"; out.version = "(no INFO answer)";
  } finally { link.close(); }
  return out;
}
const fwName = (fw) => [fw.text, fw.version].filter(Boolean).join(" ");

/* ------------------------------------------------------------------ the two flows --- */
/* ctx: { up (fm1ota.js Updater), image, product (Install) or stock {image, product} (Return), recovery (the resume box),
 *        state (the report: flow, fw, step), ui: { say(key, arg), step(key, n), confirm(key, arg) -> bool,
 *        save(archive, fw), openStock() } }
 * -> "done" or "cancel"; anything else is thrown with a code */
const otaOf = (up) => up.find((id) => /^ota-/i.test(id.model));

export async function runInstall(ctx) {
  const { up, ui, state } = ctx;
  state.flow = "install";
  const ota = await otaOf(up);
  if (ota) {                                   // in update mode (an interrupted install): the write goes on
    ota.link.close();
    state.fw = { kind: "loader", text: ota.id.text };
    ui.say("resuming", ota.id.text);
    if (!(await up.resume(ctx.image, ui.step, ctx.product))) throw fail("stopped", "the write in update mode stopped");
    return "done";
  }
  ui.say("checking");
  const fw = state.fw = await fwKind(up);
  if (fw.kind === "none") throw up.notFound();
  if (fw.kind === "stock" || fw.kind === "jiant") {
    await up.install(ctx.image, ctx.product, ui.step);
    return "done";
  }
  ui.openStock();
  throw fail(fw.kind, `nothing was written: this FM-1 runs ${fwName(fw)} (${fw.kind})`, fwName(fw));
}

export async function runStock(ctx) {
  const { up, ui, state, stock } = ctx;
  state.flow = "stock";
  const ota = await otaOf(up);
  if (ota) {
    ota.link.close();
    state.fw = { kind: "loader", text: ota.id.text };
    if (!ctx.recovery) throw fail("stockNeedBackup", "update mode: no backup can be taken there");
    if (!(await ui.confirm("stockConfirm"))) return "cancel";
    if (!(await up.resume(stock.image, ui.step, stock.product))) throw fail("stopped", "the write in update mode stopped");
    return "done";
  }
  ui.say("checking");
  const fw = state.fw = await fwKind(up);
  if (fw.kind === "none") throw up.notFound();
  /* an official firmware: nothing to back up (it does not answer the editor's backup), straight to V15 */
  if (fw.kind === "stock" || fw.kind === "oldstock" || fw.kind === "newstock") {
    if (!(await ui.confirm(fw.kind === "stock" ? "stockAgainConfirm" : "officialConfirm", fw.text))) return "cancel";
    await up.install(stock.image, stock.product, ui.step);
    return "done";
  }
  /* JIANT without the complete backup (before 0.3): update JIANT first, its songs then come along */
  if (fw.kind === "jiant" && !fw.backup) throw fail("stockUnsupported", `${fwName(fw)}: no complete backup`, fwName(fw));
  let archive = null;
  if (fw.backup) {
    /* Felucca, Skiff and other forks: every object they list (their samples, Skiff's pattern bank), restorable with
     * their own editor; JIANT: its archive, restorable with the JIANT editor */
    const link = new BackupConnection(fw.ports.input, fw.ports.output);
    try {
      ui.say("backup", "0%");
      archive = await captureBackup((r, o) => link.request(r, o), fw.version || fw.text,
        (done, total) => ui.say("backup", `${Math.round(done / Math.max(1, total) * 100)}%`), { complete: fw.kind !== "jiant" });
    } catch (e) {
      state.backupError = e.message;
      if (!(await ui.confirm("backupFailConfirm", e.message))) throw fail("backupfail", e.message, e.message);
    } finally { link.close(); }
  }
  if (archive) {
    archive.identity = fw.text;
    await ui.save(archive, fw);
    if (!(await ui.confirm("stockConfirm"))) return "cancel";
  } else if (!state.backupError && !(await ui.confirm("nobackupConfirm", fwName(fw)))) return "cancel";
  await up.install(stock.image, stock.product, ui.step);
  return "done";
}

/* --------------------------------------------------------------- browser and MIDI --- */
export function envOf(nav = {}) {
  const ua = String(nav.userAgent || ""), uad = nav.userAgentData;
  const touchMac = /Macintosh/.test(ua) && (nav.maxTouchPoints || 0) > 1;
  const os = /iPhone|iPad|iPod/.test(ua) || touchMac ? "ios" : /Android/.test(ua) ? "android" : /CrOS/.test(ua) ? "chromeos"
    : /Windows/.test(ua) ? "windows" : /Mac OS X|Macintosh/.test(ua) ? "mac" : /Linux/.test(ua) ? "linux" : "other";
  const brands = uad && Array.isArray(uad.brands) ? uad.brands.map((b) => b.brand).join(", ") : "";
  const browser = os === "ios" ? "ios" : nav.brave || /Brave/.test(brands) ? "brave" : /Edg\//.test(ua) ? "edge"
    : /OPR\/|Opera/.test(ua) ? "opera" : /SamsungBrowser/.test(ua) ? "samsung" : /Firefox\//.test(ua) ? "firefox"
    : /Chrome\/|Chromium\//.test(ua) ? "chrome" : /Safari\//.test(ua) ? "safari" : "other";
  return { ua, os, browser, brands, webmidi: typeof nav.requestMIDIAccess === "function", webusb: !!nav.usb,
    lang: nav.language || "" };
}
/* no WebMIDI at all: which message (iPhone/iPad: no browser has it; Safari: none; Firefox: an old one) */
export function nomidiCode(env) {
  return env.os === "ios" ? "nomidiIos" : env.browser === "safari" ? "nomidiSafari" : env.browser === "firefox" ? "nomidiFirefox" : "nomidi";
}
/* MIDI with SysEx; refused: blocked (the browser no longer asks: where to allow it) or denied (the prompt dismissed); a
 * MIDI service that failed to start (Windows, after sleep) is asked again once */
export async function midiAccess(nav, log = () => {}) {
  const env = envOf(nav);
  if (!env.webmidi) throw fail(nomidiCode(env), `no WebMIDI in this browser (${env.browser}, ${env.os})`);
  let err = null;
  for (let k = 0; k < 2; k++) {
    try { return await nav.requestMIDIAccess({ sysex: true }); }
    catch (e) {
      err = e; log(`requestMIDIAccess: ${e && e.name} ${e && e.message}`);
      if (e && (e.name === "SecurityError" || e.name === "NotAllowedError")) break;
      await sleep(700);
    }
  }
  let state = "";
  try { state = (await nav.permissions.query({ name: "midi", sysex: true })).state; } catch (_) {}
  const refused = err && (err.name === "SecurityError" || err.name === "NotAllowedError");
  throw fail(state === "denied" ? "blocked" : refused ? "denied" : "midifail", `MIDI: ${err && err.name} ${err && err.message} (permission ${state || "?"})`, err && err.name);
}

/* ------------------------------------------------------------- the official file --- */
/* why a file picked for Return to official V15 is not FM-1.fwsc of V15 (size 699956): the ZIP as downloaded, another
 * official version, a JIANT / Felucca package, a modified file. null: worth the full check (validateStockPackage) */
export function stockFileProblem(name, bytes, v15size) {
  const n = String(name || "");
  if (bytes.length >= 4 && bytes[0] === 0x50 && bytes[1] === 0x4B && bytes[2] === 3 && bytes[3] === 4) return { code: "stockZip", detail: n };
  if (/\.(rar|7z|exe|dmg|pkg|apk)$/i.test(n)) return { code: "stockZip", detail: n };
  let product = "";
  try { product = bytes.length > 960 ? productOf(bytes) : ""; } catch (_) {}
  if (/^FM-1_9\d\d$|^FM-1_7\d\d$/.test(product) || /^(felucca|jiant|skiff)/i.test(n)) return { code: "stockCustom", detail: `${n} ${product}`.trim() };
  if (/^FM-1_0\d\d$/.test(product) && product !== "FM-1_015") return { code: "stockOtherVer", detail: `${n}: ${product}` };
  if (bytes.length !== v15size) return { code: "stockNotV15", detail: `${n}: ${bytes.length} B` };
  return null;
}

/* ------------------------------------------------------------------- the report --- */
export function makeReport({ env, page = "", installer = "", state = {}, scan = null, permission = "", error = null, log = "" }) {
  const fw = state.fw || {};
  const lines = [
    "JIANT installer report",
    `date: ${new Date().toISOString()}`,
    `page: ${page}`,
    `installer: ${installer}`,
    `browser: ${env.browser} | os: ${env.os} | lang: ${env.lang}${env.brands ? " | brands: " + env.brands : ""}`,
    `user agent: ${env.ua}`,
    `webmidi: ${env.webmidi ? "yes" : "NO"} | webusb: ${env.webusb ? "yes" : "no"} | midi permission: ${permission || "?"}`,
    `flow: ${state.flow || "-"} | last step: ${state.step || "-"}`,
    `firmware: ${fw.kind || "-"} | identity: ${fw.text || "-"} | INFO: ${fw.version || "-"} | backup: ${fw.backup ? "yes" : "no"}${fw.port ? " | port: " + fw.port : ""}`,
  ];
  if (state.backupError) lines.push(`backup error: ${state.backupError}`);
  if (scan) {
    lines.push(`midi inputs probed (${scan.ports.length}):`);
    for (const p of scan.ports) lines.push(`  - ${p.name}${p.manufacturer ? " [" + p.manufacturer + "]" : ""} ${p.state}: ${p.result}`);
    if (state.inputs) lines.push(`midi inputs: ${state.inputs.join(" | ") || "(none)"}`);
    lines.push(`midi outputs: ${scan.outputs.join(" | ") || "(none)"}`);
  } else if (state.inputs) lines.push(`midi inputs: ${state.inputs.join(" | ") || "(none)"}`);
  if (state.editorTabs) lines.push(`editor tabs open: ${state.editorTabs}`);
  lines.push(`error: ${error ? `${error.code || "-"}: ${error.message || error}` : "-"}`);
  const tail = String(log).trim().split("\n").slice(-40);
  lines.push("log:", ...tail.map((l) => "  " + l));
  return lines.join("\n");
}
