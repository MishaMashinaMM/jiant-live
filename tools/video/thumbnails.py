#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# (JIANT 0.6.35; 0.6.37 the landing's black, bone white and heat rule) the two films' thumbnails, 1280 x 720, from their footage (screen.rgb) in the project's look (black,
# violet to orange, Chakra Petch, the signature):
#   python3 tools/video/thumbnails.py build/video_dice build/video3 OUTDIR
#   -> OUTDIR/thumb-dice-all.png (the DICE ALL film), OUTDIR/thumb-jiant-fm.png (every feature)
import re, sys
from pathlib import Path
import numpy as np
from PIL import Image, ImageDraw, ImageFilter, ImageFont

ROOT = Path(__file__).resolve().parents[2]
DICE, FEAT, OUT = (Path(a) for a in sys.argv[1:4])
W, H = 1280, 720
VIOLET, ORANGE, WHITE, DIM = (184, 112, 255), (255, 122, 0), (244, 240, 250), (150, 140, 170)
VER = re.search(r'FELUCCA_VERSION "(v[^"]+)"', (ROOT / 'firmware/src/felucca.c').read_text()).group(1)
def font(w, s): return ImageFont.truetype(str(ROOT / f'assets/fonts/ChakraPetch-{w}.ttf'), s)
def grad(w, h, a=VIOLET, b=ORANGE):
    t = np.linspace(0, 1, w)[None, :, None]
    return np.repeat(np.array(a)[None, None] * (1 - t) + np.array(b)[None, None] * t, h, 0).astype(np.uint8)
def gtext(img, xy, s, f, a=VIOLET, b=ORANGE):     # gradient text
    m = Image.new('L', img.size); ImageDraw.Draw(m).text(xy, s, font=f, fill=255)
    box = m.getbbox()
    g = Image.new('RGB', img.size); g.paste(Image.fromarray(grad(box[2] - box[0], box[3] - box[1], a, b)), box[:2])
    img.paste(g, (0, 0), m)
    return box
HEAT = [(0, 210, 255), (90, 60, 255), (180, 50, 230), (240, 40, 50), (255, 122, 0), (255, 215, 0), (255, 255, 255)]
def heat_c(v):
    v = max(0, min(1, v)) * 6; k = min(5, int(v)); f = v - k
    return tuple(int(HEAT[k][j] + (HEAT[k + 1][j] - HEAT[k][j]) * f) for j in range(3))
def heat_rule(w, h):                               # (0.6.37) the landing page's rule: cyan to yellow
    row = np.array([heat_c(0.86 * i / max(w - 1, 1)) for i in range(w)], np.uint8)
    return Image.fromarray(np.repeat(row[None], h, 0))
BONE = (244, 241, 234)
def black():                                       # (0.6.37) the landing's page: pure black
    return Image.new('RGBA', (W, H), (0, 0, 0, 255))
def bg(seed):                                      # black, a faint grid, a violet and an orange glow
    rng = np.random.default_rng(seed)
    a = np.zeros((H, W, 3), np.float32); a[:] = (6, 4, 10)
    a[::40, :] += 6; a[:, ::40] += 6
    yy, xx = np.mgrid[0:H, 0:W]
    for (cx, cy, r, c, k) in ((W * 0.78, H * 0.45, 420, VIOLET, 0.38), (W * 0.2, H * 0.85, 360, ORANGE, 0.22)):
        g = np.exp(-((xx - cx) ** 2 + (yy - cy) ** 2) / (2 * r * r))[..., None]
        a += g * np.array(c) * k
    a += rng.normal(0, 2.0, a.shape)
    return Image.fromarray(np.clip(a, 0, 255).astype(np.uint8)).convert('RGBA')
def frames(d):
    return np.memmap(d / 'screen.rgb', dtype=np.uint8, mode='r').reshape(-1, 240, 240, 3)
def screen(img, fr, x, y, s, rot=0.0):             # an FM-1 screen, sharp pixels, a rim, a shadow, tilted
    sc = Image.fromarray(np.array(fr)).resize((int(240 * s), int(240 * s)), Image.NEAREST).convert('RGBA')
    pad = 10
    card = Image.new('RGBA', (sc.width + 2 * pad, sc.height + 2 * pad))
    ImageDraw.Draw(card).rounded_rectangle((0, 0, card.width - 1, card.height - 1), 14, fill=(18, 12, 28, 255),
                                           outline=(70, 52, 96, 255), width=3)
    card.alpha_composite(sc, (pad, pad))
    if rot: card = card.rotate(rot, resample=Image.BICUBIC, expand=True)
    sh = Image.new('RGBA', card.size, (0, 0, 0, 0)); sh.putalpha(card.getchannel('A').point(lambda v: v * 0.7))
    sh = sh.filter(ImageFilter.GaussianBlur(18))
    img.alpha_composite(sh, (int(x + 14), int(y + 22)))
    img.alpha_composite(card, (int(x), int(y)))
def busiest(fr, lo, hi, step=7):                  # the frame with the most colour in a span (a lively screen)
    best, bi = -1, lo
    for i in range(lo, min(hi, len(fr)), step):
        v = float(np.asarray(fr[i], np.float32).std())
        if v > best: best, bi = v, i
    return fr[bi]
PIPS = {5: [(-1, -1), (1, -1), (0, 0), (-1, 1), (1, 1)], 6: [(-1, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (1, 1)],
        3: [(-1, -1), (0, 0), (1, 1)]}
def die(n, size):
    im = Image.new('RGBA', (size, size))
    m = Image.new('L', (size, size)); ImageDraw.Draw(m).rounded_rectangle((0, 0, size - 1, size - 1), size // 6, fill=255)
    rim = Image.fromarray(grad(size, size)).convert('RGBA'); rim.putalpha(m); im.alpha_composite(rim)
    d = ImageDraw.Draw(im); d.rounded_rectangle((12, 12, size - 13, size - 13), size // 7, fill=(14, 9, 22, 255))
    r = size // 11
    for px, py in PIPS[n]:
        cx, cy = size // 2 + px * size // 4, size // 2 + py * size // 4
        d.ellipse((cx - r, cy - r, cx + r, cy + r), fill=WHITE + (255,))
    return im
def signature(img, ox, oy, sc, wd):
    src = (ROOT / 'firmware/src/ui_signature.h').read_text()
    nums = [int(x) for x in re.findall(r'\b\d+\b', src.split('{', 1)[1])]
    pts = [(nums[i], nums[i + 1]) for i in range(0, len(nums) - 1, 2)]
    segs, L = [], 0
    for i in range(len(pts) - 1):
        if pts[i + 1][1] & 128: continue
        a = (pts[i][0], pts[i][1] & 127); b = (pts[i + 1][0], pts[i + 1][1] & 127)
        l = max(abs(b[0] - a[0]), abs(b[1] - a[1])); segs.append((a, b, L)); L += l
    H_ = [(0, 210, 255), (90, 60, 255), (180, 50, 230), (240, 40, 50), (255, 122, 0), (255, 215, 0), (255, 255, 255)]
    def heat(v):
        v = max(0, min(1, v)) * 6; k = min(5, int(v)); f = v - k
        return tuple(int(H_[k][j] + (H_[k + 1][j] - H_[k][j]) * f) for j in range(3))
    d = ImageDraw.Draw(img)
    for a, b, acc in segs:
        d.line([(ox + a[0] * sc, oy + a[1] * sc), (ox + b[0] * sc, oy + b[1] * sc)], fill=heat(0.08 + 0.86 * acc / L), width=wd)

# ---- the DICE ALL film's: the title, a die tumbling, three screens fanned (the dialog, the beings, a piano roll)
fr = frames(DICE)
seps = [float(x) for x in (DICE / 'seps.txt').read_text().split()]
dialog = fr[int((seps[1] + 0.3) * 30)]
home = busiest(fr, int(seps[2] * 30) + 20, int(seps[3] * 30))
roll = busiest(fr, int(seps[5] * 30) + 20, int(seps[6] * 30))
img = black()
screen(img, roll, 770, 210, 1.55, -8)
screen(img, home, 900, 60, 1.6, 6)
screen(img, dialog, 700, 300, 1.42, -2)
ImageDraw.Draw(img).text((56, 150), "DICE", font=font('Bold', 210), fill=BONE)
ImageDraw.Draw(img).text((56, 330), "ALL", font=font('Bold', 210), fill=BONE)
d6 = die(5, 190).rotate(-18, resample=Image.BICUBIC, expand=True)
img.alpha_composite(d6, (420, 360))
d = ImageDraw.Draw(img)
d.text((62, 560), "ONE PRESS · A WHOLE NEW PIECE", font=font("SemiBold", 34), fill=WHITE)
d.text((62, 612), "JIANT FM · M-VAVE FM-1", font=font('Medium', 28), fill=DIM)
img.paste(heat_rule(W, 8), (0, H - 8))
img.convert('RGB').save(OUT / 'thumb-dice-all.png')

# ---- every feature's: the signature and the name, a grid of six living screens
fr = frames(FEAT)
beat = 60 / 125
def at_beat(b): return fr[int(b * beat * 30)]
picks = [busiest(fr, int(226 * beat * 30), int(238 * beat * 30), 3),   # HOME, six beings (under the grid)
         at_beat(18.5),                                                    # DRUM-X
         at_beat(122.5),                                                   # the RESO reverb
         at_beat(146.5),                                                   # the SLICER bus
         at_beat(186.5),                                                   # ARRANGE
         at_beat(155.2)]                                                   # the punch-in FX
img = black()
for i, f_ in enumerate(picks):
    x = 610 + (i % 3) * 222; y = 70 + (i // 3) * 300
    screen(img, f_, x, y, 0.82, (-4, 3, -2, 2, -3, 4)[i])
signature(img, 40, 40, 1.35, 4)
ImageDraw.Draw(img).text((52, 250), "JIANT FM", font=font('Bold', 130), fill=BONE)
img.paste(heat_rule(520, 5), (58, 392))
d = ImageDraw.Draw(img)
d.text((58, 410), "ALTERNATIVE FIRMWARE", font=font('SemiBold', 40), fill=WHITE)
d.text((58, 452), "FOR THE M-VAVE FM-1", font=font('SemiBold', 40), fill=WHITE)
pill = font('Bold', 30)
txt = f"EVERY FEATURE · {VER}"
tw = int(pill.getlength(txt))
d.rounded_rectangle((58, 530, 58 + tw + 36, 584), 10, fill=heat_c(0.62))
d.text((76, 535), txt, font=pill, fill=(14, 8, 20))
d.text((58, 612), "synthesis · sequencing · mutation · performance", font=font('Medium', 26), fill=DIM)
img.paste(heat_rule(W, 8), (0, H - 8))
img.convert('RGB').save(OUT / 'thumb-jiant-fm.png')
print(OUT / 'thumb-dice-all.png', OUT / 'thumb-jiant-fm.png')
