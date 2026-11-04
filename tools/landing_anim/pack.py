# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""Packs tools/landing_anim/capture.c's recording into web/landing/anim/<clip>.js, one clip a file:
  JA_ANIM.<clip> = {w, h, bb (the ink box over every frame, px), n (frames a loop), xf (crossfade frames after it), pal (colours #rrggbb),
  st (styles: colour index * 16 + alpha level 0..15), s (texts), d (base64 of zigzag varints)}
d, per frame: the count of its drawing operations, then each in the order the firmware drew it:
  0 style points x0 y0 dx dy ..   a polyline (Q2 px: a quarter pixel), simplified within 0.4 px
  1 style x y size                a square dot (px; vx_dot's 2 x 2, a single pixel)
  2 colour x y r                  a filled disc (og_node: Q2 centre, px radius; T_SURF ones mask what is under them)
  3 colour x y w h                a rectangle (px)
  4 font colour x y text#         a text (font 0: the 5 x 7 face AF_X, 1: AF_S; x y its pen and line top, px)
The page (web/landing/index.html, the JA player) decodes it once and draws each frame at any size.
  pack.py CAPTURE.txt OUT_DIR [--preview DIR]"""
import base64
import json
import sys
from pathlib import Path

TOL = 1.6                                   # Q2 px: 0.4 px
RECT = ("voice", "eco")                     # shown in their panel as the firmware does (VOICE: the crown off it)


def rgb(c):
    r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
    return "#%02x%02x%02x" % (r * 255 // 31, g * 255 // 63, b * 255 // 31)


def zz(v):
    return v << 1 if v >= 0 else ((-v) << 1) - 1


def varints(vals):
    out = bytearray()
    for v in vals:
        u = zz(int(v))
        while u >= 128:
            out.append((u & 127) | 128)
            u >>= 7
        out.append(u)
    return bytes(out)


def rdp(pts, tol):
    """the polyline without the points closer than tol to the line between their kept neighbours"""
    if len(pts) < 3:
        return pts
    if pts[0] == pts[-1]:                   # a closed outline: split at its farthest point (else the ends coincide)
        m = max(range(len(pts)), key=lambda i: (pts[i][0] - pts[0][0]) ** 2 + (pts[i][1] - pts[0][1]) ** 2)
        if 0 < m < len(pts) - 1:
            return rdp(pts[:m + 1], tol) + rdp(pts[m:], tol)[1:]
    keep, stack = {0, len(pts) - 1}, [(0, len(pts) - 1)]
    while stack:
        a, b = stack.pop()
        (x0, y0), (x1, y1) = pts[a], pts[b]
        dx, dy = x1 - x0, y1 - y0
        L = (dx * dx + dy * dy) ** .5 or 1.0
        best, bi = -1.0, -1
        for i in range(a + 1, b):
            d = abs((pts[i][0] - x0) * dy - (pts[i][1] - y0) * dx) / L
            if d > best:
                best, bi = d, i
        if best > tol:
            keep.add(bi)
            stack += [(a, bi), (bi, b)]
    return [pts[i] for i in sorted(keep)]


def parse(path):
    clips, cur, fr = [], None, None
    for ln in open(path, encoding="latin-1"):
        k = ln[:1]
        if k == "S":
            _, name, w, h, fpb, xf = ln.split()
            cur = {"name": name, "w": int(w), "h": int(h), "n": int(fpb), "xf": int(xf), "frames": []}
            clips.append(cur)
        elif k == "F":
            fr = []
            cur["frames"].append(fr)
        elif k in "LPNR":
            fr.append((k,) + tuple(int(x) for x in ln.split()[1:]))
        elif k == "T":
            p = ln.rstrip("\n").split(" ", 5)
            fr.append(("T", int(p[1]), int(p[2]), int(p[3]), int(p[4]), p[5] if len(p) > 5 else ""))
    return clips


def ops_of(fr):
    """the frame's operations: lines joined into polylines, 2 x 2 plot groups into dots"""
    ops, i = [], 0
    while i < len(fr):
        o = fr[i]
        if o[0] == "L":
            if o[6] <= 0:
                i += 1
                continue
            pts, c, a = [(o[1], o[2]), (o[3], o[4])], o[5], o[6]
            i += 1
            while i < len(fr) and fr[i][0] == "L" and fr[i][5] == c and abs(fr[i][6] - a) < 16 and (fr[i][1], fr[i][2]) == pts[-1]:
                pts.append((fr[i][3], fr[i][4]))
                i += 1
            ops.append(("L", c, a, pts))
        elif o[0] == "P":
            q = fr[i:i + 4]
            if (len(q) == 4 and all(p[0] == "P" and p[3] == o[3] for p in q)
                    and [(p[1] - o[1], p[2] - o[2]) for p in q] == [(0, 0), (1, 0), (0, 1), (1, 1)]):
                ops.append(("P", o[3], o[4], o[1], o[2], 2))
                i += 4
            else:
                if o[4] > 0:
                    ops.append(("P", o[3], o[4], o[1], o[2], 1))
                i += 1
        else:
            ops.append(o)
            i += 1
    return ops


def pack(clip):
    pal, pal_i, st, st_i, strs, str_i = [], {}, [], {}, [], {}

    def col(c):
        if c not in pal_i:
            pal_i[c] = len(pal)
            pal.append(rgb(c))
        return pal_i[c]

    def sty(c, a):
        key = col(c) * 16 + max(0, min(15, (a * 16 - 1) // 256))
        if key not in st_i:
            st_i[key] = len(st)
            st.append(key)
        return st_i[key]

    vals, bb = [], [1e9, 1e9, -1e9, -1e9]

    def box(x, y, r=0):
        bb[0], bb[1], bb[2], bb[3] = min(bb[0], x - r), min(bb[1], y - r), max(bb[2], x + r), max(bb[3], y + r)

    for fr in clip["frames"]:
        ops = ops_of(fr)
        vals.append(len(ops))
        for o in ops:
            if o[0] == "L":
                pts = rdp([(round(x / 4), round(y / 4)) for x, y in o[3]], TOL)
                vals += [0, sty(o[1], o[2]), len(pts), pts[0][0], pts[0][1]]
                for x, y in pts:
                    box(x / 4, y / 4)
                for k in range(1, len(pts)):
                    vals += [pts[k][0] - pts[k - 1][0], pts[k][1] - pts[k - 1][1]]
            elif o[0] == "P":
                vals += [1, sty(o[1], o[2]), o[3], o[4], o[5]]
                box(o[3], o[4])
            elif o[0] == "N":
                vals += [2, col(o[4]), round(o[1] / 4), round(o[2] / 4), o[3]]
            elif o[0] == "R":
                vals += [3, col(o[5]), o[1], o[2], o[3], o[4]]
            else:
                s = o[5]
                if s not in str_i:
                    str_i[s] = len(strs)
                    strs.append(s)
                vals += [4, o[3], col(o[4]), o[1], o[2], str_i[s]]
                box(o[1], o[2])
                box(o[1] + 6 * len(s), o[2] + 9)
    if clip["name"] in RECT:
        bb = [0, 0, clip["w"], clip["h"]]
    bb = [int(bb[0]) - 2, int(bb[1]) - 2, int(bb[2]) + 3, int(bb[3]) + 3] if bb[2] > bb[0] else [0, 0, clip["w"], clip["h"]]
    return {"w": clip["w"], "h": clip["h"], "bb": bb, "n": clip["n"], "xf": clip["xf"], "pal": pal, "st": st, "s": strs,
            "d": base64.b64encode(varints(vals)).decode()}


def preview(clip, out):
    from PIL import Image, ImageDraw
    sc, fs = 2, clip["frames"][:40:5]
    W, H = clip["w"] * sc, clip["h"] * sc
    im = Image.new("RGB", (W * len(fs), H))
    dr = ImageDraw.Draw(im)
    for k, fr in enumerate(fs):
        ox = k * W
        for o in ops_of(fr):
            if o[0] == "L":
                dr.line([(ox + x / 16 * sc, y / 16 * sc) for x, y in o[3]], fill=rgb(o[1]), width=1)
            elif o[0] == "P":
                dr.rectangle([ox + o[3] * sc, o[4] * sc, ox + (o[3] + o[5]) * sc - 1, (o[4] + o[5]) * sc - 1], fill=rgb(o[1]))
            elif o[0] == "N":
                x, y, r = o[1] / 16 * sc, o[2] / 16 * sc, (o[3] + .5) * sc
                dr.ellipse([ox + x - r, y - r, ox + x + r, y + r], fill=rgb(o[4]))
            elif o[0] == "R":
                dr.rectangle([ox + o[1] * sc, o[2] * sc, ox + (o[1] + o[3]) * sc - 1, (o[2] + o[4]) * sc - 1], fill=rgb(o[5]))
            else:
                dr.text((ox + o[1] * sc, o[2] * sc), o[5], fill=rgb(o[4]))
    im.save(out)


def main():
    src, out = sys.argv[1], Path(sys.argv[2])
    pv = sys.argv[sys.argv.index("--preview") + 1] if "--preview" in sys.argv else None
    out.mkdir(parents=True, exist_ok=True)
    for clip in parse(src):
        p = pack(clip)
        js = ("/* SPDX-License-Identifier: GPL-3.0-only\n   %s: recorded from the firmware's own drawing code "
              "(tools/landing_anim/build.sh); generated, do not edit */\n" % clip["name"])
        js += "(window.JA_ANIM=window.JA_ANIM||{})[%s]=%s;\n" % (json.dumps(clip["name"]), json.dumps(p, separators=(",", ":")))
        (out / (clip["name"] + ".js")).write_text(js)
        print("%-8s %3d frames %7d B" % (clip["name"], len(clip["frames"]), len(js)))
        if pv:
            Path(pv).mkdir(parents=True, exist_ok=True)
            preview(clip, Path(pv) / (clip["name"] + ".png"))


main()
