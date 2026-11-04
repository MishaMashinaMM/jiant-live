// SPDX-License-Identifier: GPL-3.0-only
// Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
// Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
// fm1backup.js against a simulated device: 7-bit packing, CRC, manifest checks, a full capture,
// validation before the first write, restore order (live music last) and abort on a failed chunk.
import { BACKUP_IDS, BACKUP_CMD, bkU32, bkR32, bkPack, bkUnpack, bkCrc, bkManifest, readBackup, captureBackup, restoreBackup } from "./fm1backup.js";

let fails = 0;
const ok = (c, what) => { console.log(`${what.padEnd(72)} ${c ? "ok" : "FAIL"}`); if (!c) fails++; };
const throws = (f) => { try { f(); return false; } catch { return true; } };
const athrows = async (f) => { try { await f(); return false; } catch { return true; } };

const rnd = (n, seed) => Uint8Array.from({ length: n }, (_, i) => (i * 131 + seed * 17 + (i >> 3)) & 255);
ok(bkR32(bkU32(0xdeadbeef)) === 0xdeadbeef && bkCrc(new TextEncoder().encode("123456789")) === 0xcbf43926, "backup: numbers and CRC-32");
const b = rnd(1000, 1);
ok(bkUnpack(bkPack(b), b.length).every((v, i) => v === b[i]), "backup: 7-bit pack / unpack round trip");
ok(throws(() => bkUnpack([...bkPack(b.subarray(0, 14)), 0], 14)), "backup: trailing bytes refused");

/* a device: objects by id, the order of writes */
function device(objs, opt = {}) {
  const d = { objs: new Map(objs), log: [], staged: null };
  d.request = async ([cmd, a]) => {
    if (cmd === BACKUP_CMD.LIST) {
      const out = [1, 0, BACKUP_IDS.length];
      for (const id of BACKUP_IDS) { const v = d.objs.get(id) || new Uint8Array(0); out.push(id, ...bkU32(v.length), ...bkU32(v.length ? bkCrc(v) : 0)); }
      return out;
    }
    if (cmd === BACKUP_CMD.GET) {
      const id = a[0], off = bkR32(a, 1), n = a[6] | a[7] << 7, v = d.objs.get(id);
      if (opt.changeOnGet === id && off === 0) v[0] ^= 1;
      return [id, 0, ...bkU32(off), n & 127, n >> 7, ...bkPack(v.subarray(off, off + n))];
    }
    if (cmd === BACKUP_CMD.PUT) {
      const [op, id] = a;
      if (op === 0 && id > (opt.maxId ?? 71)) { d.log.push(`refused ${id}`); return [op, id, 1]; }   // (an older firmware)
      if (op === 0) { d.staged = { id, size: bkR32(a, 2), crc: bkR32(a, 7), bytes: [] }; return [op, id, 0]; }
      if (op === 1) {
        if (opt.failChunk === id) return [op, id, 2];
        const off = bkR32(a, 2), p = a.slice(7), n = Math.min(256, d.staged.size - off);
        d.staged.bytes.push(...bkUnpack(p, n)); return [op, id, 0];
      }
      if (op === 2) { const s = Uint8Array.from(d.staged.bytes); const rc = bkCrc(s) === d.staged.crc ? 0 : 2; if (!rc) { d.objs.set(id, s); d.log.push(id); } return [op, id, rc]; }
      if (op === 3) { d.log.push(`abort ${id}`); return [op, id, 0]; }
    }
    throw new Error(`unexpected ${cmd}`);
  };
  return d;
}
const objs = [[0, rnd(3388, 2)], [1, rnd(1200, 3)], [2, rnd(3388, 4)], [6, rnd(3080, 5)]];
const dev = device(objs);
const file = await captureBackup(dev.request, "TEST");
ok(file.objects.length === BACKUP_IDS.length && file.objects[0].size === 3388 && file.objects[3].size === 0, "backup: capture lists every object, empty ones as 0");
ok(readBackup(JSON.stringify(file)).objects[2].bytes.every((v, i) => v === objs[2][1][i]), "backup: capture -> file -> bytes round trip");
ok(await athrows(() => captureBackup(device(objs.map(([i, v]) => [i, v.slice()]), { changeOnGet: 2 }).request, "TEST")), "backup: a device that changes during capture fails the capture");

const bad = JSON.parse(JSON.stringify(file)); bad.objects[2].crc ^= 1;
ok(throws(() => readBackup(bad)), "backup: a damaged file is refused");
const noRun = JSON.parse(JSON.stringify(file)); noRun.objects[0] = { ...noRun.objects[0], size: 0, crc: 0, data: "" };
ok(throws(() => readBackup(noRun)), "backup: a file without the current music is refused");
ok(throws(() => bkManifest([1, 0, 3])), "backup: a short manifest is refused");
{ // a device still on Felucca lists its user sample slots (ids 32..34, up to 80 KiB each): left out, the rest kept
  const ids = [...BACKUP_IDS, 32, 33, 34], a = [1, 0, ids.length];
  for (const id of ids) a.push(id, ...bkU32(id >= 32 && id <= 34 ? 81920 : 100 + id), ...bkU32(id + 1));
  const m = bkManifest(a);
  ok(m.length === BACKUP_IDS.length && m.skipped === 3 && m.every((o, i) => o.id === BACKUP_IDS[i] && o.size === 100 + o.id),
     "backup: a Felucca manifest with sample slots: they are skipped, the rest kept");
}

const target = device([]);
const damaged = JSON.parse(JSON.stringify(file)); damaged.objects[6].crc ^= 1;
ok(await athrows(() => restoreBackup(target.request, damaged)) && target.log.length === 0, "backup: restore validates every byte before the first write");
await restoreBackup(target.request, file);
const order = target.log.filter((x) => typeof x === "number");
ok(order.at(-1) === 0 && order.at(-2) === 1 && order.indexOf(2) < order.indexOf(1), "backup: restore order: projects, banks, settings, live music last");
ok([0, 1, 2, 6].every((id) => target.objs.get(id).every((v, i) => v === objs.find((o) => o[0] === id)[1][i])), "backup: restored objects equal the source");
{   /* the FM6 patch bank (id 8) and the archives of firmware before it (8 objects, no id 8) */
  ok(BACKUP_IDS.length === 43 && file.objects.some((o) => o.id === 8) && file.objects.some((o) => o.id === 9) &&
     file.objects.some((o) => o.id === 10) && file.objects.filter((o) => o.id >= 40).length === 32 &&
     !file.objects.some((o) => o.id >= 32 && o.id <= 34),
     "backup: 43 objects: id 8 (the retired FM6 bank), 9 (user presets' FM6 patches), 10 the song index, 40..71 every song's sections");
  const v3 = JSON.parse(JSON.stringify(file)); v3.objects = v3.objects.filter((o) => o.id <= 9);
  ok(readBackup(v3).objects.length === 10, "backup: an archive of Felucca 1.0.3 .. JIANT 0.2 (10 objects, no songs) still reads");
  const v2 = JSON.parse(JSON.stringify(v3)); v2.objects = v2.objects.filter((o) => o.id !== 9);
  ok(readBackup(v2).objects.length === 9, "backup: an archive of 1.0..1.0.2 (9 objects, the bank as id 8) still reads");
  const old = JSON.parse(JSON.stringify(v3)); old.objects = old.objects.filter((o) => o.id !== 8 && o.id !== 9);
  ok(readBackup(old).objects.length === 8, "backup: an archive of the 8 objects before FM6 still reads");
  const odd = JSON.parse(JSON.stringify(file)); odd.objects = odd.objects.filter((o) => o.id !== 5);
  ok(throws(() => readBackup(odd)), "backup: an archive missing another object is refused");
}
{   /* archives with the retired user sample slots (ids 32..34, any content): those entries are skipped */
  const smp = (id, n) => { const v = rnd(n, id); let s = ""; for (const x of v) s += String.fromCharCode(x);
    return { id, size: n, crc: n ? bkCrc(v) : 0, data: btoa(s) }; };
  const withSmp = (f) => { const c = JSON.parse(JSON.stringify(f)); c.objects.push(smp(32, 5000), smp(33, 0), smp(34, 90000)); return c; };
  const f10 = JSON.parse(JSON.stringify(file)); f10.objects = f10.objects.filter((o) => o.id <= 9);
  const a13 = withSmp(f10), r13 = readBackup(a13);
  ok(r13.objects.length === 10 && r13.objects.every((o, i) => o.id === BACKUP_IDS[i]), "backup: an archive with sample slots (13 objects) reads, ids 32..34 skipped");
  const v2 = JSON.parse(JSON.stringify(f10)); v2.objects = v2.objects.filter((o) => o.id !== 9);
  const old = JSON.parse(JSON.stringify(f10)); old.objects = old.objects.filter((o) => o.id !== 8 && o.id !== 9);
  ok(readBackup(withSmp(v2)).objects.length === 9 && readBackup(withSmp(old)).objects.length === 8,
    "backup: 12- and 11-object archives with sample slots read too");
  const to = device([]);
  await restoreBackup(to.request, a13);
  ok(!to.log.some((x) => typeof x === "number" && x >= 32) && to.log.at(-1) === 0 && [0, 1, 2, 6].every((id) => to.objs.has(id)),
    "backup: restoring it writes no sample slot, the rest restored");
  const short = withSmp(file); short.objects = short.objects.filter((o) => o.id !== 5);
  ok(throws(() => readBackup(short)), "backup: an archive with sample slots but missing another object is refused");
}
{   /* 1.0.3: an old archive with a bank restores its id 8 (after 6, 7); a new archive onto 1.0.2 skips id 9 */
  const bankBytes = rnd(3472, 8), patches = rnd(3728, 9);
  const v2dev = device([...objs, [8, bankBytes]]);
  const v2file = await captureBackup(async ([cmd, a]) => {   // (a 1.0.2 device: 9 objects)
    const r = await v2dev.request([cmd, a]);
    if (cmd !== BACKUP_CMD.LIST) return r;
    const n = r[2], keep = [];
    for (let i = 0; i < n; i++) if (r[3 + i * 11] <= 8) keep.push(...r.slice(3 + i * 11, 14 + i * 11));
    return [1, 0, keep.length / 11, ...keep];
  }, "1.0.2");
  const to103 = device([]);
  await restoreBackup(to103.request, v2file);
  const lg = to103.log.filter((x) => typeof x === "number");
  ok(v2file.objects.length === 9 && lg.indexOf(8) > lg.indexOf(7) && lg.indexOf(7) > lg.indexOf(6) &&
     to103.objs.get(8).every((v, i) => v === bankBytes[i]), "backup: a 1.0.2 archive (id 8, the bank) restores, its bank after the user presets");
  const newFile = await captureBackup(device([...objs, [9, patches]]).request, "1.0.3");
  const to102 = device([], { maxId: 8 });
  await restoreBackup(to102.request, newFile);
  ok(to102.log.includes("refused 9") && !to102.objs.has(9) && to102.log.at(-1) === 0,
    "backup: a 1.0.3 archive onto older firmware: id 9 skipped, the rest restored");
  const to103b = device([]);
  await restoreBackup(to103b.request, newFile);
  ok(to103b.objs.get(9).every((v, i) => v === patches[i]), "backup: a 1.0.3 archive restores id 9");
  const songs = await captureBackup(device([...objs, [10, rnd(1328, 10)], [45, rnd(3840, 11)]]).request, "0.3");
  const to02 = device([], { maxId: 9 });
  await restoreBackup(to02.request, songs);
  ok(to02.log.includes("refused 10") && to02.log.includes("refused 45") && to02.objs.has(2) && to02.log.at(-1) === 0,
    "backup: a JIANT archive with songs onto older firmware: the songs skipped, the rest restored");
  const to03 = device([]);
  await restoreBackup(to03.request, songs);
  ok(to03.objs.get(10).length === 1328 && to03.objs.get(45).every((v, i) => v === rnd(3840, 11)[i]),
    "backup: the song index (10) and a song's section (45: song 2, section B) restore");
}
const failing = device([], { failChunk: 2 });
ok(await athrows(() => restoreBackup(failing.request, file)) && failing.log.includes("abort 2") && !failing.log.includes(0), "backup: a refused chunk aborts that object and stops before the live music");

console.log(fails ? `BACKUP WEB TESTS FAILED (${fails})` : "backup web tests passed");
process.exit(fails ? 1 : 0);
