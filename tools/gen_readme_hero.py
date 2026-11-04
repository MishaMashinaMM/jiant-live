#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
# (JIANT 0.5; 0.6.30: from the presentation video's footage) the README's images, docs/jiant-hero.png and
# docs/jiant-screens.png. (0.9.2) From the firmware's own renders in the default skin on the JIANT palette (0.9.7: AIR,
# tests/ui_render.c -> build/ui_new/AIR_JIANT/<screen>.png, with MENU > DISPLAY > SCOPE OFF as a new FM-1 has it: the
# *_full scenes; JIANT/ COLORS and HUD_JIANT/ the other skins), so the README shows the screens as the
# firmware draws them now (the tabs, the knobs in their colours, the breadcrumb):
#   python3 -c "import sys; sys.path.insert(0, 'tools'); import build; build.generate()"
#   mkdir -p build/ui_new/ppm && cc -O1 -w -DFELUCCA_VERSION=\"v0.9.2\" -Ibuild/gen -Ifirmware/src -Itests \
#       -o build/ui_render tests/ui_render.c -lm && build/ui_render build/ui_new && python3 tests/ui_render.py build/ui_new
#   python3 tools/gen_readme_hero.py build/ui_new docs        (needs PIL; tests/run_tests.sh makes build/ui_new too)
import re, sys
from pathlib import Path
from PIL import Image, ImageDraw, ImageFont
R = Path(__file__).resolve().parents[1]
U = Path(sys.argv[1] if len(sys.argv) > 1 else R / 'build/ui_new')
OUT = Path(sys.argv[2] if len(sys.argv) > 2 else R / 'docs')
VERSION = re.search(r'FELUCCA_VERSION "(v[^"]+)"', (R / 'firmware/src/felucca.c').read_text()).group(1)
F = R / 'assets/fonts'
def font(w, s): return ImageFont.truetype(str(F / f'ChakraPetch-{w}.ttf'), s)
H = [(0, 210, 255), (90, 60, 255), (180, 50, 230), (240, 40, 50), (255, 122, 0), (255, 215, 0), (255, 255, 255)]
def heat(v):
    v = max(0, min(1, v)) * 6; k = min(5, int(v)); f = v - k
    return tuple(int(H[k][j] + (H[k + 1][j] - H[k][j]) * f) for j in range(3))
BG, TXT, MID, DIM = (0, 0, 0), (244, 241, 234), (160, 154, 150), (80, 76, 84)


def shot(name, skin='AIR_JIANT'):                         # a screen as the firmware draws it (240 x 240)
    return Image.open(U / skin / f'{name}.png').convert('RGB')


def signature(d, ox, oy, sc, width):
    src = (R / 'firmware/src/ui_signature.h').read_text()
    nums = [int(x) for x in re.findall(r'\b\d+\b', src.split('{', 1)[1])]
    pts = [(nums[i], nums[i + 1]) for i in range(0, len(nums) - 1, 2)]
    segs, L = [], 0
    for i in range(len(pts) - 1):
        if pts[i + 1][1] & 128: continue
        a = (pts[i][0], pts[i][1] & 127); b = (pts[i + 1][0], pts[i + 1][1] & 127)
        l = max(abs(b[0] - a[0]), abs(b[1] - a[1])); segs.append((a, b, L)); L += l
    for a, b, acc in segs:
        d.line([(ox + a[0] * sc, oy + a[1] * sc), (ox + b[0] * sc, oy + b[1] * sc)], fill=heat(0.08 + 0.86 * acc / L), width=width)


def framed(im, img, x, y, s, resample=Image.NEAREST):
    d = ImageDraw.Draw(im)
    d.rounded_rectangle([x - 4, y - 4, x + 240 * s + 3, y + 240 * s + 3], 10, outline=(40, 34, 48), width=2)
    im.paste(img.resize((int(240 * s), int(240 * s)), resample), (x, y))


# ---- the hero
W, Hh = 1600, 1900
im = Image.new('RGB', (W, Hh), BG); d = ImageDraw.Draw(im)
signature(d, 70, 40, 2.6, 5)
d.text((690, 70), "JIANT FM", font=font('Bold', 128), fill=TXT)
d.text((696, 215), "bio-synthetic operating system for the M-VAVE FM-1", font=font('Medium', 30), fill=MID)
d.text((696, 258), f"firmware {VERSION}  ·  synthesis · sequencing · mutation · performance", font=font('Medium', 24), fill=DIM)
for i in range(W - 140):
    d.line([(70 + i, 330), (70 + i, 335)], fill=heat(i / (W - 140)))
cap = font('SemiBold', 22)
y0 = 370
big = [('home', 'HOME · six living tracks'), ('gen_explore', 'GEN · EXPLORE, a new song idea'),
       ('save_arrange', 'ARRANGE · the song in sections')]
for i, (b, c) in enumerate(big):
    x = 70 + i * 500; framed(im, shot(b), x, y0, 2); d.text((x, y0 + 492), c, font=cap, fill=heat(0.15 + 0.3 * i))
strips = [[('lente_step_note', 'NOTE · MICRO in 64ths'), ('lente_bar', 'SEQ › STEP · THE ROLL'), ('key_chord', 'CHORD'),
           ('fx_buses', 'FX › BUSES'), ('mixer', 'MIXER · DJ FILTER'), ('layer_glo_active', 'GLO held · MAP')],
          [('v6_analog_full', 'ACID ANALOG'), ('drumx', 'DRUM-X'), ('words', 'VOICE · WORDS'), ('ed_operator', 'FM6 OPERATOR'),
           ('reverb_reso', 'RESO REVERB'), (('home', 'JIANT'), 'COLORS skin')]]
y1 = y0 + 545
for r, row in enumerate(strips):
    for i, (b, c) in enumerate(row):
        x = 70 + i * 245; y = y1 + r * 270
        img = shot(*b) if isinstance(b, tuple) else shot(b)
        im.paste(img.resize((216, 216), Image.LANCZOS), (x, y))
        d.text((x, y + 222), c, font=font('SemiBold', 18), fill=MID)
y2 = y1 + 2 * 270 + 20
cards = [("SIX RULES", "SELECT walks a section's pages;\nits button jumps group to group."),
         ("THREE SKINS", "AIR, COLORS and HUD: each\nknob its colour, tabs, a breadcrumb."),
         ("EXPLORE", "HOME + SELECT writes a whole song;\nEXPLORE breaks every rule, 43 genres."),
         ("VOICE SINGS", "Words in ES, EN and JA: each note\nthe next syllable of your lyric."),
         ("ANIMAL", "Birds, crickets, frogs, wolves:\ncalls from physical models."),
         ("DRUM-X OCTAVES", "Above: ratchets x2..x8. Below:\nSHORT, GHOST, FLAM, per sound."),
         ("OFF THE GRID", "REC keeps where you played, to\na 64th of a step; MICRO edits it."),
         ("DJ FILTER", "One knob on the whole mix:\nlow-pass left, high-pass right."),
         ("USB STEMS", "Every track to the computer, or to\nthe phone with the JIANT Stems app.")]
ft, fd = font('SemiBold', 27), font('Medium', 20)
for i, (t, txt) in enumerate(cards):
    cx = 70 + (i % 3) * 500; cy = y2 + (i // 3) * 125
    d.rectangle([cx, cy, cx + 5, cy + 92], fill=heat(i / (len(cards) - 1)))
    d.text((cx + 22, cy - 2), t, font=ft, fill=TXT)
    d.multiline_text((cx + 22, cy + 38), txt, font=fd, fill=MID, spacing=6)
d.text((70, Hh - 50), "GPL-3.0  ·  fork of Felucca (Hügelton Instruments)  ·  song mode from SLOOP  ·  juanjiant-bit.github.io/JIANT",
       font=font('Medium', 20), fill=DIM)
im.save(OUT / 'jiant-hero.png', optimize=True); print(OUT / 'jiant-hero.png', im.size)

# ---- the screens: six of them, 2x, as a grid
sc = [('home', 'HOME · ECO'), ('save_arrange', 'SAVE › ARRANGE'), ('lente_step_note', 'SEQ › NOTE · OFF THE GRID'),
      ('fx_effect_delay', 'FX › DELAY'), ('drumx', 'EDIT › KIT · DRUM-X'), ('words', 'EDIT › WORDS · VOICE')]
g = Image.new('RGB', (3 * 520 + 40, 2 * 560 + 30), BG); d = ImageDraw.Draw(g)
for i, (b, c) in enumerate(sc):
    x = 40 + (i % 3) * 520; y = 30 + (i // 3) * 560
    framed(g, shot(b), x, y, 2)
    d.text((x, y + 492), c, font=font('SemiBold', 24), fill=heat(0.1 + 0.16 * i))
g.save(OUT / 'jiant-screens.png', optimize=True); print(OUT / 'jiant-screens.png', g.size)
