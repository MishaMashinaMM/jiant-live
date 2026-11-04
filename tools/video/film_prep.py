# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# (JIANT 0.6.66) the v5 films: what tools/video/film.html draws each frame with, from tools/video/capture_dark.c's
# footage: per frame the audio's level, its low / mid / high bands, a kick onset, a 32-band spectrum, and each track's
# peak (frames.txt) with its onsets; the captions (scenes.txt). Writes data.json next to them.
#   python3 tools/video/film_prep.py build/video5/feat
import json, sys, wave
from pathlib import Path
import numpy as np

D = Path(sys.argv[1])
FPS = 30
w = wave.open(str(D / 'audio.wav'))
sr = w.getframerate()
a = np.frombuffer(w.readframes(w.getnframes()), np.int16).reshape(-1, 2).astype(np.float32).mean(1) / 32768.0
hdr, *rows = (D / 'scenes.txt').read_text().splitlines()
splash = float(hdr.split('splash ')[1].split(' s')[0])
end = float(hdr.split('end ')[1].split(' s')[0])
beat = float(hdr.split('beat ')[1].split(' s')[0])
N = int(end * FPS)
hop = sr // FPS
win = 2048
han = np.hanning(win).astype(np.float32)
freqs = np.fft.rfftfreq(win, 1 / sr)
edges = np.geomspace(40, 16000, 33)
bands = [(freqs >= edges[i]) & (freqs < edges[i + 1]) for i in range(32)]
low, mid, high, rms, spec = [], [], [], [], []
for f in range(N):
    c = f * hop + hop // 2
    seg = a[max(0, c - win // 2): c + win // 2]
    if len(seg) < win: seg = np.pad(seg, (0, win - len(seg)))
    m = np.abs(np.fft.rfft(seg * han))
    low.append(float(m[(freqs > 30) & (freqs < 130)].mean()))
    mid.append(float(m[(freqs > 300) & (freqs < 2500)].mean()))
    high.append(float(m[(freqs > 5000) & (freqs < 14000)].mean()))
    rms.append(float(np.sqrt((a[f * hop:(f + 1) * hop] ** 2).mean() + 1e-12)))
    spec.append([float(m[b].mean()) if b.any() else 0.0 for b in bands])
def norm(x, q=0.98):
    x = np.array(x); return np.clip(x / (np.quantile(x, q) + 1e-9), 0, 1.4)
low, mid, high, rms = norm(low), norm(mid), norm(high), norm(rms)
spec = np.array(spec); spec = np.clip(np.log1p(spec / (np.quantile(spec, 0.97, axis=0) + 1e-9) * 6) / np.log1p(6), 0, 1.2)
kick = np.zeros(N)
for f in range(1, N):                                   # an onset: the low band jumps
    if low[f] - low[f - 1] > 0.18 and low[f] > 0.45: kick[f] = min(1.0, (low[f] - low[f - 1]) * 2.5)
F = np.loadtxt(D / 'frames.txt')[:N]
home = F[:, 0].astype(int)
pk = F[:, 1:7]
pkn = np.clip(pk / (np.quantile(pk, 0.97, axis=0) + 1), 0, 1.5)
on = np.zeros_like(pkn)
for k in range(6):
    s = 0.0
    for f in range(N):
        if pkn[f, k] > 0.55 and pkn[f, k] > s * 1.5 + 0.12: on[f, k] = min(1.0, pkn[f, k])
        s = max(pkn[f, k], s * 0.82)
scenes = []
for r in rows:
    t0, tg, ti, su, kd, da = (r.split('\t') + ['', '', '', '0', ''])[:6]
    scenes.append({'t': float(t0), 'tag': tg, 'title': [x for x in ti.split('|') if x],
                   'sub': [x for x in su.split('|') if x], 'kind': int(kd or 0), 'data': [x for x in da.split('|') if x]})
lst = []
if (D / 'list.txt').exists():                           # (capture_promo.c: what is new, the second of its picture)
    for r in (D / 'list.txt').read_text().splitlines():
        ti, wo, t0 = r.split('\t')
        lst.append({'title': ti, 'words': wo, 't': float(t0)})
keys, chords, hudline = None, None, None
if (D / 'keys.txt').exists():                           # (capture_chord.c: each frame the key LEDs and the chord's name)
    kr = [r.split('\t') for r in (D / 'keys.txt').read_text().splitlines()][:N]
    keys = [int(r[0]) for r in kr]
    chords = [r[1] if len(r) > 1 and r[1] != '-' else '' for r in kr]
if (D / 'hud.txt').exists():
    hudline = (D / 'hud.txt').read_text().strip()
r2 = lambda x: [round(float(v), 3) for v in x]
json.dump({'fps': FPS, 'n': N, 'list': lst, 'splash': splash, 'end': end, 'beat': beat, 'scenes': scenes,
           'rms': r2(rms), 'low': r2(low), 'mid': r2(mid), 'high': r2(high), 'kick': r2(kick), 'home': home.tolist(),
           'trk': [r2(pkn[f]) for f in range(N)], 'on': [r2(on[f]) for f in range(N)],
           'spec': [r2(spec[f]) for f in range(N)], 'keys': keys, 'chords': chords, 'hud': hudline},
          open(D / 'data.json', 'w'))
print(D / 'data.json', N, 'frames,', int(kick.sum() > 0) and int((kick > 0).sum()), 'kicks,', int((on > 0).sum()), 'track onsets')
