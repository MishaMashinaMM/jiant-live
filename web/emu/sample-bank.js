// SPDX-License-Identifier: GPL-3.0-only
// Same IMA ADPCM format as the editor; sample data stays outside the firmware.
function crc32(bytes) {
  let crc = 0xFFFFFFFF;
  for (const byte of bytes) {
    crc ^= byte;
    for (let k = 0; k < 8; k++) crc = (crc >>> 1) ^ (crc & 1 ? 0xEDB88320 : 0);
  }
  return (~crc) >>> 0;
}
const codec = {
  SIZE: 6144, HEAD: 256, SLOTS: 15,
  STEP: [7, 8, 9, 10, 11, 12, 13, 14, 16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41, 45, 50, 55, 60, 66, 73, 80, 88, 97,
    107, 118, 130, 143, 157, 173, 190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658, 724, 796,
    876, 963, 1060, 1166, 1282, 1411, 1552, 1707, 1878, 2066, 2272, 2499, 2749, 3024, 3327, 3660, 4026, 4428,
    4871, 5358, 5894, 6484, 7132, 7845, 8630, 9493, 10442, 11487, 12635, 13899, 15289, 16818, 18500, 20350,
    22385, 24623, 27086, 29794, 32767],
  IDX: [-1, -1, -1, -1, 2, 4, 6, 8],
  /* Int16 samples -> nibbles (the device's decoder mirrored) */
  adpcm(x) {
    const out = new Uint8Array(x.length);
    let pred = 0, idx = 0;
    for (let k = 0; k < x.length; k++) {
      const st = this.STEP[idx];
      let d = x[k] - pred, nb = 0;
      if (d < 0) { nb = 8; d = -d; }
      if (d >= st) { nb |= 4; d -= st; }
      if (d >= st >> 1) { nb |= 2; d -= st >> 1; }
      if (d >= st >> 2) nb |= 1;
      let dd = st >> 3;
      if (nb & 4) dd += st;
      if (nb & 2) dd += st >> 1;
      if (nb & 1) dd += st >> 2;
      pred = Math.max(-32768, Math.min(32767, pred + (nb & 8 ? -dd : dd)));
      idx = Math.max(0, Math.min(88, idx + this.IDX[nb & 7]));
      out[k] = nb;
    }
    return out;
  },
  /* samples: [{ name, rate, pcm: Int16Array }] -> the bank (Uint8Array SIZE), what did not fit cut */
  build(samples) {
    const b = new Uint8Array(this.SIZE), dv = new DataView(b.buffer);
    let at = 0, count = 0;
    for (const s of samples.slice(0, this.SLOTS)) {
      const room = (this.SIZE - this.HEAD - at) * 2;
      const n = Math.min(s.pcm.length, room, 65535);
      if (n < 16) break;
      const nib = this.adpcm(s.pcm.subarray(0, n)), e = 16 + count * 16;
      for (let k = 0; k < n; k++) b[this.HEAD + at + (k >> 1)] |= nib[k] << ((k & 1) * 4);
      dv.setUint16(e, at, true); dv.setUint16(e + 2, n, true); dv.setUint16(e + 4, s.rate, true);
      const nm = String(s.name || "").toUpperCase().replace(/[^\x20-\x7E]/g, "").slice(0, 9);
      for (let i = 0; i < nm.length; i++) b[e + 6 + i] = nm.charCodeAt(i);
      at += (n + 1) >> 1;
      count++;
    }
    dv.setUint32(0, 0x504D534A, true); dv.setUint16(4, 1, true); dv.setUint16(6, count, true); dv.setUint32(8, at, true);
    dv.setUint32(12, crc32(b.subarray(16, this.HEAD + at)), true);
    return { bytes: b, count, used: this.HEAD + at };
  },
};

export function encodeSampleBank(bank) {
  if (bank?.format !== "jiant-factory-pcm" || bank.version !== 1 ||
      !Array.isArray(bank.samples) || !bank.samples.length || bank.samples.length > codec.SLOTS)
    throw new Error("Invalid sample bank");
  let used = codec.HEAD;
  const samples = bank.samples.map(s => {
    if (!Number.isInteger(s.rate) || s.rate < 1000 || s.rate > 16000 ||
        !Array.isArray(s.pcm) || s.pcm.length < 16 || s.pcm.length > 11776 ||
        s.pcm.some(x => !Number.isInteger(x) || x < -32768 || x > 32767))
      throw new Error("Invalid sample");
    used += Math.ceil(s.pcm.length / 2);
    return {name:s.name, rate:s.rate, pcm:Int16Array.from(s.pcm)};
  });
  if (used > codec.SIZE) throw new Error("Sample bank exceeds 6144 bytes");
  const built = codec.build(samples);
  if (built.count !== samples.length || built.used !== used) throw new Error("Truncated sample bank");
  return built.bytes;
}
export function withSampleBank(sectors, bank) {
  if (!(bank instanceof Uint8Array) || bank.length !== 6144) throw new Error("Invalid bank size");
  const result = {...sectors};
  for (let i = 0; i < bank.length; i += 4096) {
    const offset = 0xDA000 + i;
    const old = sectors?.[offset];
    const sector = old && old.length === 4096 ? Uint8Array.from(old) : new Uint8Array(4096).fill(255);
    sector.set(bank.subarray(i, i + 4096));
    result[offset] = sector;
  }
  return result;
}

export function hasSampleBank(sectors) {
  const bank = new Uint8Array(6144).fill(255);
  for (let i = 0; i < bank.length; i += 4096) {
    const sector = sectors?.[0xDA000 + i];
    if (sector?.length === 4096) bank.set(sector.subarray(0, Math.min(4096, bank.length-i)), i);
  }
  const d = new DataView(bank.buffer), len = d.getUint32(8, true);
  return d.getUint32(0,true) === 0x504D534A && d.getUint16(4,true) === 1 &&
    d.getUint16(6,true) > 0 && d.getUint16(6,true) <= 15 && len <= 5888 &&
    crc32(bank.subarray(16,256+len)) === d.getUint32(12,true);
}
