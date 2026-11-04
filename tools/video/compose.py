# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# (JIANT 0.6.11; 0.6.37 the landing's look) the presentation videos: tools/video/capture.c's (or capture_dice.c's)
# footage, the FM-1's screen at 30 fps and its audio, composed at 1920 x 1080 as the README's hero page is drawn
# (tools/gen_readme_hero.py): pure black, the signature in heat colours, JIANT FM in bone white, the heat rule (cyan,
# violet, red, orange, yellow), each feature a card (its category in heat colour, a heat bar, the title, the words
# dim) beside the very page it lives on, the screen in a thin frame, sharp pixels. A heat scan and a short glitch at
# each cut, the frame pulsing on the beat; a grid of pages replayed (a scene of kind 2), the outro as the page's foot.
# Pipes the frames to ffmpeg.
#   python3 tools/video/compose.py build/video          -> build/video/jiant-fm.mp4
#   python3 tools/video/compose.py build/video 300,900  -> preview-0300.png, preview-0900.png
import re, subprocess, sys, wave
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parents[2]
D = Path(sys.argv[1] if len(sys.argv) > 1 else ROOT / 'build/video')
W, H, FPS = 1920, 1080, 30
SX, SY = W - 960 - 64, 60                          # the screen (240 x 4, nearest: the pixels stay sharp)
LX, LW = 80, 760                                   # the left column
TXT, MID, DIM, FRAME = (244, 241, 234), (160, 154, 150), (80, 76, 84), (40, 34, 48)
HEAT = [(0, 210, 255), (90, 60, 255), (180, 50, 230), (240, 40, 50), (255, 122, 0), (255, 215, 0), (255, 255, 255)]
def heat(v):
    v = max(0.0, min(1.0, v)) * 6; k = min(5, int(v)); f = v - k
    return tuple(int(HEAT[k][j] + (HEAT[k + 1][j] - HEAT[k][j]) * f) for j in range(3))
def heat_strip(w, h, a=0.0, b=0.86):                 # the rule: heat left to right
    row = np.array([heat(a + (b - a) * i / max(w - 1, 1)) for i in range(w)], np.uint8)
    return np.repeat(row[None], h, 0)
FONT = ROOT / 'assets/fonts'
def font(w, s): return ImageFont.truetype(str(FONT / f'ChakraPetch-{w}.ttf'), s)
VER = re.search(r'FELUCCA_VERSION "(v[^"]+)"', (ROOT / 'firmware/src/felucca.c').read_text()).group(1)

# ---- the script: scenes.txt (start s, category, title lines, subtitle lines, kind; '|' splits lines; an empty
# title ends the captions: the outro)
_hdr, *_rows = (D / 'scenes.txt').read_text().splitlines()
SPLASH = float(_hdr.split('splash ')[1].split(' s')[0])
END = float(_hdr.split('end ')[1].split(' s')[0])
BEAT = float(_hdr.split('beat ')[1].split(' s')[0])
BEATS = np.array([float(x) for x in (D / 'beats.txt').read_text().split()]) if (D / 'beats.txt').exists() else None
def beat_at(t):                                    # (phase 0..1 in the beat, the beat's number)
    if BEATS is None or len(BEATS) < 2: return (t / BEAT) % 1.0, int(t / BEAT)
    i = int(np.searchsorted(BEATS, t, 'right')) - 1
    if i < 0: return 0.0, 0
    nx = BEATS[i + 1] if i + 1 < len(BEATS) else BEATS[i] + (BEATS[i] - BEATS[i - 1])
    return (t - BEATS[i]) / max(nx - BEATS[i], 1e-3), i
N = int(END * FPS)
_cap = []
for r in _rows:
    t0, tg, ti, su, kd = (r.split('\t') + ['', '', '', '0'])[:5]
    _cap.append((float(t0), tg, [x for x in ti.split('|') if x], [x for x in su.split('|') if x], int(kd or 0)))
OUT = next(c[0] for c in _cap if not c[2])         # the outro: from the first empty caption
SCENES = []                                        # (start, end, category, title, subtitle, kind)
for i, (t0, tg, ti, su, kd) in enumerate(_cap):
    if not ti: break
    t1 = _cap[i + 1][0] if i + 1 < len(_cap) else OUT
    SCENES.append((t0, t1, tg, ti, su, kd))
CARDS = [s for s in SCENES if s[3][0].strip() and s[5] == 0]
FIRST = CARDS[0][0] if CARDS else (SCENES[0][0] if SCENES else SPLASH + 1.0)
SEPS = [float(x) for x in (D / 'seps.txt').read_text().split()] if (D / 'seps.txt').exists() else []
CUTS = [s[0] for s in SCENES if s[3][0].strip()] + [x + 0.5 for x in SEPS]

def ease(x): x = min(max(x, 0.0), 1.0); return 1 - (1 - x) ** 3
def text_layer(lines, f, color, gap=1.12):
    asc, desc = f.getmetrics()
    lh = int((asc + desc) * gap)
    wmax = max(int(f.getlength(l)) for l in lines) + 8
    m = Image.new('L', (wmax, lh * (len(lines) - 1) + asc + desc + 8))
    d = ImageDraw.Draw(m)
    for i, l in enumerate(lines): d.text((0, i * lh), l, font=f, fill=255)
    out = Image.new('RGBA', m.size, color + (0,)); out.putalpha(m)
    return out
def fit(lines, weight, size, wmax):                 # the largest size up to `size` that fits wmax
    while size > 30 and max(font(weight, size).getlength(l) for l in lines) > wmax: size -= 4
    return font(weight, size)
def paste(img, layer, x, y, alpha=1.0, reveal=1.0):
    if alpha <= 0.01 or reveal <= 0: return
    L = layer
    if reveal < 1: L = L.crop((0, 0, max(1, int(L.width * reveal)), L.height))
    if alpha < 1:
        L = L.copy(); L.putalpha(L.getchannel('A').point(lambda v: int(v * alpha)))
    img.alpha_composite(L, (int(x), int(y)))

# ---- the signature (firmware/src/ui_signature.h), drawn in heat colours up to a fraction of its length
_src = (ROOT / 'firmware/src/ui_signature.h').read_text()
_nums = [int(x) for x in re.findall(r'\b\d+\b', _src.split('{', 1)[1])]
_pts = [(_nums[i], _nums[i + 1]) for i in range(0, len(_nums) - 1, 2)]
SEGS, SIGL = [], 0
for i in range(len(_pts) - 1):
    if _pts[i + 1][1] & 128: continue
    a_ = (_pts[i][0], _pts[i][1] & 127); b_ = (_pts[i + 1][0], _pts[i + 1][1] & 127)
    l_ = max(abs(b_[0] - a_[0]), abs(b_[1] - a_[1])); SEGS.append((a_, b_, SIGL, l_)); SIGL += l_
def signature(sc, wd, frac=1.0):
    w_ = int(max(max(a[0], b[0]) for a, b, _, _ in SEGS) * sc + 2 * wd); h_ = int(130 * sc + 2 * wd)
    im = Image.new('RGBA', (w_, h_)); d = ImageDraw.Draw(im)
    lim = SIGL * frac
    for a, b, acc, l in SEGS:
        if acc >= lim: break
        k = min(1.0, (lim - acc) / max(l, 1))
        e = (a[0] + (b[0] - a[0]) * k, a[1] + (b[1] - a[1]) * k)
        d.line([(wd + a[0] * sc, wd + a[1] * sc), (wd + e[0] * sc, wd + e[1] * sc)], fill=heat(0.08 + 0.86 * acc / SIGL) + (255,), width=wd)
    return im
SIG_SMALL = signature(1.0, 3)
SIG_BIG = signature(2.4, 5)
SIG_OUT = signature(1.9, 5)

# ---- the static layers
_intro = (D / 'intro.txt').read_text().strip().split('|') if (D / 'intro.txt').exists() else \
    ['JIANT FM', 'bio-synthetic operating system for the M-VAVE FM-1']
L_INTRO_T = text_layer([_intro[0]], fit([_intro[0]], 'Bold', 132, LW), TXT)
L_INTRO_S = text_layer([_intro[1] if len(_intro) > 1 else ' '], fit([_intro[1] if len(_intro) > 1 else ' '], 'Medium', 32, LW), MID)
L_INTRO_V = text_layer([f'firmware {VER}', 'synthesis · sequencing · mutation · performance'], font('Medium', 24), DIM, 1.3)
L_BRAND = text_layer(['JIANT FM'], font('Bold', 54), TXT)
L_BVER = text_layer([f'M-VAVE FM-1 · {VER}'], font('Medium', 22), DIM)
RULE = Image.fromarray(heat_strip(LW, 4)).convert('RGBA')
CARD_L = []
for i, (s0, s1, tg, ti, su, kd) in enumerate(CARDS):
    c = heat(0.04 + 0.82 * i / max(len(CARDS) - 1, 1))
    ft = fit(ti, 'Bold', 92, LW - 30)
    CARD_L.append((c, text_layer([tg or ' '], font('SemiBold', 28), c), text_layer(ti, ft, TXT, 1.0),
                   text_layer(su or [' '], font('Medium', 34), MID, 1.3)))
GRIDS = []
for s0, s1, tg, ti, su, kd in SCENES:
    if kd != 2: continue
    cells = []
    for j, name in enumerate(su[:8]):
        src = next((c for c in CARDS if c[3][0] == name), None)
        if src: cells.append((src, text_layer([name], font('SemiBold', 24), heat(0.04 + 0.82 * j / 7))))
    GRIDS.append((s0, s1, text_layer(ti, font('Bold', 60), TXT), cells))
OUT_L = [text_layer(['JIANT FM'], font('Bold', 124), TXT),
         text_layer(['try it in your browser'], font('SemiBold', 40), TXT),
         text_layer(['juanjiant-bit.github.io/JIANT'], font('Medium', 34), heat(0.62)),
         text_layer(['emulator · web installer · editor · cheat sheets'], font('Medium', 24), MID),
         text_layer(['support: ko-fi.com/juanjiant · cafecito.app/juanjiant'], font('Medium', 24), heat(0.3))]

PIPS = {1: [(0, 0)], 2: [(-1, -1), (1, 1)], 3: [(-1, -1), (0, 0), (1, 1)], 4: [(-1, -1), (1, -1), (-1, 1), (1, 1)],
        5: [(-1, -1), (1, -1), (0, 0), (-1, 1), (1, 1)], 6: [(-1, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (1, 1)]}
def die_face(n, size=300):                         # a die: black face, a heat rim, bone-white pips
    im = Image.new('RGBA', (size, size))
    m = Image.new('L', (size, size)); ImageDraw.Draw(m).rounded_rectangle((0, 0, size - 1, size - 1), size // 6, fill=255)
    rim = Image.fromarray(heat_strip(size, size, 0.05, 0.8)).convert('RGBA'); rim.putalpha(m)
    im.alpha_composite(rim)
    d = ImageDraw.Draw(im)
    d.rounded_rectangle((12, 12, size - 13, size - 13), size // 7, fill=(0, 0, 0, 255))
    r = size // 11
    for px, py in PIPS[n]:
        cx, cy = size // 2 + px * size // 4, size // 2 + py * size // 4
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=TXT + (255,))
    return im
DIE = {n: die_face(n) for n in range(1, 7)}
def draw_dice(img, t):                             # over a throw: the die tumbles in, its faces flicker, it lands
    for i, s0 in enumerate(SEPS):
        u = t - s0
        if -0.05 <= u < 0.75:
            k = ease(u / 0.6)
            face = (1 + int(u * 30) * 7 + i) % 6 + 1 if u < 0.55 else (i * 5 + 3) % 6 + 1
            sz = int(300 * (0.6 + 0.55 * k) * (1 + 0.15 * np.exp(-max(u - 0.55, 0) * 12)))
            L = DIE[face].resize((sz, sz), Image.BILINEAR).rotate(-540 * (1 - k), resample=Image.BICUBIC, expand=True)
            x = int(-200 + 620 * k) - L.width // 2; y = int(600 - 120 * np.sin(np.pi * min(u / 0.6, 1)) * (1 - k * 0.3)) - L.height // 2
            for g_ in (3, 2, 1):
                paste(img, L, x - int(60 * g_ * (1 - k)), y, 0.18 * (1 - k) / g_)
            paste(img, L, x, y, min(1.0, (u + 0.05) / 0.12) * (1 - ease((u - 0.62) / 0.13)))
            return
    if SEPS and SEPS[0] < t < OUT:                 # between throws: the last face, small and calm
        i = max(j for j, s0 in enumerate(SEPS) if s0 <= t)
        L = DIE[(i * 5 + 3) % 6 + 1].resize((150, 150), Image.BILINEAR)
        paste(img, L, 420 - 75, 600 - 75, 0.55 * ease((t - SEPS[i] - 0.7) / 0.3))

def glitch(arr, x0, y0, w, h, amt, rng):
    reg = arr[y0:y0 + h, x0:x0 + w]
    for _ in range(int(6 * amt) + 1):
        y = rng.integers(0, h - 8); bh = rng.integers(4, 40)
        reg[y:y + bh] = np.roll(reg[y:y + bh], int(rng.integers(-40, 40) * amt), axis=1)
    sh = int(8 * amt)
    reg[..., 0] = np.roll(reg[..., 0], sh, axis=1); reg[..., 2] = np.roll(reg[..., 2], -sh, axis=1)

# ---- the footage, the audio's loudness a frame (the beat's pulse)
scr = np.memmap(D / 'screen.rgb', dtype=np.uint8, mode='r').reshape(-1, 240, 240, 3)
with wave.open(str(D / 'audio.wav')) as w:
    a = np.frombuffer(w.readframes(w.getnframes()), dtype=np.int16).reshape(-1, 2).astype(np.float32)
spf = 44100 // FPS
rms = np.array([np.sqrt((a[i * spf:(i + 1) * spf] ** 2).mean() + 1) for i in range(N)])
pulse = np.clip((rms - np.percentile(rms, 20)) / (np.percentile(rms, 99) - np.percentile(rms, 20) + 1), 0, 1)
env = np.zeros(N)
for i in range(N): env[i] = max(pulse[i], env[i - 1] * 0.86 if i else 0)
def frame_at(f): return scr[min(max(f, 0), len(scr) - 1)]

def card_at(t):                                    # the card on now: (index, start, end)
    for i, c in enumerate(CARDS):
        if c[0] <= t < c[1]: return i, c[0], c[1]
    return None
def grid_at(t):
    for g in GRIDS:
        if g[0] <= t < g[1]: return g
    return None
def heat_now(t):
    ci = card_at(t)
    if ci: return CARD_L[ci[0]][0]
    return heat(0.05 + 0.8 * t / END)

PREVIEW = [int(x) for x in sys.argv[2].split(',')] if len(sys.argv) > 2 else None
ff = None if PREVIEW else subprocess.Popen(['ffmpeg', '-y', '-loglevel', 'error', '-f', 'rawvideo', '-pix_fmt', 'rgb24', '-s', f'{W}x{H}',
                       '-r', str(FPS), '-i', '-', '-i', str(D / 'audio.wav'),
                       '-af', f'afade=t=in:d=0.3,afade=t=out:st={END - 2.0:.2f}:d=2.0',
                       '-c:v', 'libx264', '-preset', 'slow', '-crf', '18', '-pix_fmt', 'yuv420p',
                       '-c:a', 'aac', '-b:a', '256k', '-movflags', '+faststart', '-shortest',
                       str(D / 'jiant-fm.mp4')], stdin=subprocess.PIPE)
rng = np.random.default_rng(7)
for f in (PREVIEW or range(N)):
    t = f / FPS
    arr = np.zeros((H, W, 3), np.uint8)
    ph, bi = beat_at(t)
    kick = float(np.exp(-ph * 6) * env[f]) if t > SPLASH else 0.0
    g = grid_at(t)
    ga = 0.0
    if g: ga = min(ease((t - g[0]) / 0.35), 1 - ease((t - g[1] + 0.35) / 0.3))
    # ---- the screen: in its thin frame, the frame warming with the beat in the scene's colour
    sa = 1 - ga
    if sa > 0.01:
        sc_ = np.repeat(np.repeat(frame_at(f), 4, 0), 4, 1)
        arr[SY:SY + 960, SX:SX + 960] = (sc_ * sa).astype(np.uint8)
        hc = np.array(heat_now(t), np.float32)
        fc = (np.array(FRAME, np.float32) * (1 - 0.8 * kick) + hc * 0.8 * kick) * sa
        arr[SY - 6:SY - 4, SX - 6:SX + 966] = fc; arr[SY + 964:SY + 966, SX - 6:SX + 966] = fc
        arr[SY - 6:SY + 966, SX - 6:SX - 4] = fc; arr[SY - 6:SY + 966, SX + 964:SX + 966] = fc
        dc = min([abs(t - c) for c in CUTS] or [9])
        if dc < 0.12: glitch(arr, SX, SY, 960, 960, 1 - dc / 0.12, rng)
        for c in CUTS:                                 # the heat scan down the screen at each cut
            if 0 <= t - c < 0.24:
                yb = SY + int(960 * ease((t - c) / 0.24))
                y0 = max(yb - 18, SY)
                arr[y0:yb, SX:SX + 960] = (arr[y0:yb, SX:SX + 960] * 0.25 + heat_strip(960, yb - y0) * 0.75).astype(np.uint8)
    img = Image.fromarray(arr).convert('RGBA')
    # ---- the intro: the hero page's head, the signature written
    if t < FIRST:
        a_ = 1 - ease((t - FIRST + 0.45) / 0.4)
        sg = signature(2.4, 5, ease((t - 0.2) / 2.6)) if t < 2.9 else SIG_BIG
        paste(img, sg, LX - 6, 120, a_)
        paste(img, L_INTRO_T, LX, 420, a_ * ease((t - 1.4) / 0.5), ease((t - 1.4) / 0.6))
        rw = int(LW * ease((t - 1.7) / 0.9))
        if rw > 0: paste(img, RULE.crop((0, 0, rw, 4)), LX, 600, a_)
        paste(img, L_INTRO_S, LX + 4, 630, a_ * ease((t - 2.0) / 0.6))
        paste(img, L_INTRO_V, LX + 4, 700, a_ * ease((t - 2.4) / 0.6))
    # ---- the head on the left while the cards run: small signature, the name, the rule
    elif t < OUT and not g:
        a_ = min(ease((t - FIRST) / 0.5), 1 - ease((t - OUT + 0.4) / 0.4))
        paste(img, SIG_SMALL, LX - 4, 52, a_)
        paste(img, L_BRAND, LX + 250, 58, a_)
        paste(img, L_BVER, LX + 254, 124, a_)
        paste(img, RULE, LX, 200, a_)
    # ---- the card: its category, a heat bar growing, the title written, the words
    ci = card_at(t)
    if ci and not g:
        i, s0, s1 = ci
        c, L_tag, L_ti, L_su = CARD_L[i]
        d_ = min(0.45, (s1 - s0) * 0.3)
        out_ = 1 - ease((t - s1 + 0.22) / 0.2)
        e = ease((t - s0) / d_)
        y = 340
        paste(img, L_tag, LX + 26 - 30 * (1 - e), y, e * out_)
        yt = y + 52
        bh = int((L_ti.height + 26 + L_su.height) * ease((t - s0) / 0.3))
        if bh > 0 and out_ > 0.01:
            bar = Image.new('RGBA', (6, bh), c + (int(255 * out_),)); img.alpha_composite(bar, (LX, yt))
        paste(img, L_ti, LX + 26, yt - 6, out_, ease((t - s0 - 0.06) / 0.35))
        paste(img, L_su, LX + 28, yt + L_ti.height + 20, out_ * ease((t - s0 - 0.25) / 0.45))
    # ---- the grid: pages replayed, each its own colour, as on the hero page
    if g:
        g0, g1, L_g, cells = g
        paste(img, L_g, 132, 64, ga, ease((t - g0) / 0.5))
        rw = int(1656 * ease((t - g0 - 0.1) / 0.6))
        if rw > 0: paste(img, Image.fromarray(heat_strip(1656, 4)).convert('RGBA').crop((0, 0, rw, 4)), 132, 146, ga)
        for j, (src, L_n) in enumerate(cells):
            x = 132 + (j % 4) * 424; y = 186 + (j // 4) * 450
            u = (t - g0 - 0.15 - j * 0.14)
            ca = ga * ease(u / 0.25)
            if ca <= 0.01: continue
            dur = max(src[1] - src[0] - 0.9, 0.5)
            sf = int((src[0] + 0.6 + (max(u, 0) % dur)) * FPS)
            cell = Image.fromarray(np.ascontiguousarray(frame_at(sf))).resize((384, 384), Image.LANCZOS).convert('RGBA')
            fr_ = Image.new('RGBA', (392, 392), FRAME + (255,)); fr_.alpha_composite(cell, (4, 4))
            kz = 1 + 0.06 * (1 - ease(u / 0.25))
            if kz > 1.001: fr_ = fr_.resize((int(392 * kz), int(392 * kz)), Image.BILINEAR)
            paste(img, fr_, x - (fr_.width - 392) // 2, y - (fr_.height - 392) // 2, ca)
            paste(img, L_n, x, y + 400, ca)
    # ---- the die over each throw (the DICE ALL film)
    if SEPS: draw_dice(img, t)
    # ---- the outro: the hero page's foot
    if t >= OUT:
        a0 = ease((t - OUT - 0.2) / 0.5)
        paste(img, SIG_OUT, LX - 6, 110, a0)
        y = 400
        for j, L in enumerate(OUT_L):
            k = ease((t - OUT - 0.5 - j * 0.18) / 0.45)
            paste(img, L, LX, y, k, k)
            if j == 0:
                y += L.height + 14
                rw = int(LW * ease((t - OUT - 0.6) / 0.8))
                if rw > 0: paste(img, RULE.crop((0, 0, rw, 4)), LX, y)
                y += 30
            else:
                y += L.height + (16 if j == 1 else 10)
    # ---- the progress: a heat line along the foot
    pw = int(W * (f + 1) / N)
    img.paste(Image.fromarray(heat_strip(W, 3)[:, :pw]), (0, H - 3))
    out = np.asarray(img.convert('RGB'))
    if t > END - 1.2: out = (out * (1 - ease((t - END + 1.2) / 1.15))).astype(np.uint8)
    if PREVIEW: Image.fromarray(out).save(D / f'preview-{f:04d}.png'); continue
    ff.stdin.write(out.tobytes())
    if f % 300 == 0: print(f'{f}/{N}', flush=True)
if ff: ff.stdin.close(); ff.wait(); print(D / 'jiant-fm.mp4')
