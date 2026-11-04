#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""JIANT 0.5: the power-on signature (assets/ui-art/signature/jiant-fm1.jpg, white ink on black) as strokes.

  gen_signature.py IN.jpg OUT.h [--png PREVIEW.png]

The ink is thresholded at the drawing's scale (W x H px, the splash's canvas), thinned to one pixel (Zhang-Suen),
traced into polylines (from the ends first, then the loops), simplified (Ramer-Douglas-Peucker, 0.6 px) and ordered as
the hand wrote them: the top line (JIANT) then the bottom one (FM1), each by its strokes' leftmost points. Out: SIG_PTS, x y bytes
per point, a stroke starting where y has bit 7 set; SIG_N points, SIG_W x SIG_H. ui_draw.c draw_splash writes it.
Needs numpy and PIL (run by hand: the header is committed, firmware/src/ui_signature.h).
"""
import sys
import numpy as np
from PIL import Image

W, H = 220, 112


def thin(a):
    a = a.copy().astype(np.uint8)
    changed = True
    while changed:
        changed = False
        for step in (0, 1):
            p = np.pad(a, 1)
            P2, P3, P4, P5 = p[:-2, 1:-1], p[:-2, 2:], p[1:-1, 2:], p[2:, 2:]
            P6, P7, P8, P9 = p[2:, 1:-1], p[2:, :-2], p[1:-1, :-2], p[:-2, :-2]
            nb = [P2, P3, P4, P5, P6, P7, P8, P9]
            B = sum(x.astype(int) for x in nb)
            seq = nb + [P2]
            A = sum(((seq[i] == 0) & (seq[i + 1] == 1)).astype(int) for i in range(8))
            if step == 0:
                c = (P2 * P4 * P6 == 0) & (P4 * P6 * P8 == 0)
            else:
                c = (P2 * P4 * P8 == 0) & (P2 * P6 * P8 == 0)
            m = (a == 1) & (B >= 2) & (B <= 6) & (A == 1) & c
            if m.any():
                a[m] = 0
                changed = True
    return a


NB = [(-1, -1), (0, -1), (1, -1), (-1, 0), (1, 0), (-1, 1), (0, 1), (1, 1)]


def trace(a):
    pts = {(x, y) for y, x in zip(*np.nonzero(a))}
    deg = {p: sum((p[0] + dx, p[1] + dy) in pts for dx, dy in NB) for p in pts}
    seen_e = set()
    strokes = []

    def walk(s):
        path, cur, prev = [s], s, None
        while True:
            nxt = None
            for dx, dy in sorted(NB, key=lambda d: abs(d[0]) + abs(d[1])):
                q = (cur[0] + dx, cur[1] + dy)
                if q in pts and q != prev and frozenset((cur, q)) not in seen_e:
                    nxt = q
                    break
            if nxt is None:
                return path
            seen_e.add(frozenset((cur, nxt)))
            prev, cur = cur, nxt
            path.append(cur)

    for s in sorted([p for p in pts if deg[p] == 1], key=lambda p: (p[0], p[1])):
        if any(frozenset((s, (s[0] + dx, s[1] + dy))) in seen_e for dx, dy in NB):
            continue
        strokes.append(walk(s))
    for s in sorted(pts):
        for dx, dy in NB:
            q = (s[0] + dx, s[1] + dy)
            if q in pts and frozenset((s, q)) not in seen_e:
                strokes.append(walk(s))
    return [s for s in strokes if len(s) >= 3]


def join(strokes, gap):
    """chained: a stroke whose end lies within gap of another's end continues into it (reversed as needed)"""
    strokes = [list(s) for s in strokes]
    d2 = lambda a, b: (a[0] - b[0]) ** 2 + (a[1] - b[1]) ** 2
    merged = True
    while merged:
        merged = False
        for i in range(len(strokes)):
            best = None
            for j in range(len(strokes)):
                if i == j:
                    continue
                a, b = strokes[i], strokes[j]
                for ea, eb, k in ((a[-1], b[0], 0), (a[-1], b[-1], 1), (a[0], b[-1], 2), (a[0], b[0], 3)):
                    d = d2(ea, eb)
                    if d <= gap * gap and (best is None or d < best[0]):
                        best = (d, j, k)
            if best:
                _, j, k = best
                a, b = strokes[i], strokes[j]
                strokes[i] = a + b if k == 0 else a + b[::-1] if k == 1 else b + a if k == 2 else b[::-1] + a
                del strokes[j]
                merged = True
                break
    return strokes


def rdp(pts, eps):
    if len(pts) < 3:
        return pts
    a, b = np.array(pts[0], float), np.array(pts[-1], float)
    d = b - a
    n = np.hypot(*d) or 1.0
    dist = [abs(d[0] * (a[1] - p[1]) - d[1] * (a[0] - p[0])) / n for p in pts]
    i = int(np.argmax(dist))
    if dist[i] > eps:
        return rdp(pts[:i + 1], eps)[:-1] + rdp(pts[i:], eps)
    return [pts[0], pts[-1]]


def main():
    src, out = sys.argv[1], sys.argv[2]
    im = Image.open(src).convert("L")
    bb = im.point(lambda v: 255 if v > 100 else 0).getbbox()
    im = im.crop(bb)
    s = min(W / im.width, H / im.height)
    w, h = int(im.width * s), int(im.height * s)
    im = im.resize((w * 2, h * 2), Image.LANCZOS)          # traced at 2x, stored at 1x
    a = (np.asarray(im) > 60).astype(np.uint8)
    for _ in range(2):                                  # closed: the JPEG's broken edges joined before thinning
        p = np.pad(a, 1)
        a = a | p[:-2, 1:-1] | p[2:, 1:-1] | p[1:-1, :-2] | p[1:-1, 2:]
    sk = thin(a)
    strokes = [st for st in trace(sk) if len(st) >= 6]   # (the thinning's spurs left out)
    strokes = join(strokes, 6)
    strokes = [rdp(st, 1.4) for st in strokes]
    ox, oy = (W - w) // 2, (H - h) // 2
    strokes = [[(ox + x // 2, oy + y // 2) for x, y in st] for st in strokes]
    strokes = [st for st in strokes if len(st) >= 2 and (abs(st[0][0] - st[-1][0]) + abs(st[0][1] - st[-1][1]) > 1 or len(st) > 2)]
    strokes.sort(key=lambda st: (min(p[1] for p in st) > H // 2, min(p[0] for p in st)))   # JIANT, then FM1
    data, n = [], 0
    for st in strokes:
        for i, (x, y) in enumerate(st):
            data += [x & 255, (y & 127) | (128 if i == 0 else 0)]
            n += 1
    lines = ["/* generated by tools/gen_signature.py from assets/ui-art/signature/jiant-fm1.jpg: the power-on signature */",
             "#pragma once", f"#define SIG_W {W}", f"#define SIG_H {H}", f"#define SIG_N {n}",
             "static const uint8_t SIG_PTS[SIG_N * 2] = {"]
    for i in range(0, len(data), 24):
        lines.append("    " + ", ".join(str(v) for v in data[i:i + 24]) + ",")
    lines.append("};")
    open(out, "w").write("\n".join(lines) + "\n")
    print(f"{len(strokes)} strokes, {n} points, {len(data)} bytes")
    if "--png" in sys.argv:
        from PIL import ImageDraw
        pv = Image.new("RGB", (W * 3, H * 3))
        d = ImageDraw.Draw(pv)
        for k, st in enumerate(strokes):
            c = (60 + (k * 53) % 196, 255 - (k * 31) % 160, 120 + (k * 71) % 136)
            d.line([(x * 3, y * 3) for x, y in st], fill=c, width=3)
            d.text((st[0][0] * 3, st[0][1] * 3), str(k), fill=(255, 255, 255))
        pv.save(sys.argv[sys.argv.index("--png") + 1])


if __name__ == "__main__":
    main()
