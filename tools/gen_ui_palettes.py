#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
# Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""Stable palette identities and the single JIANT rendering definition.

Historical indices, tagged settings IDs and the 20-entry migration map are compatibility
metadata. Only JIANT has rendering colors. Derived tokens use gfx.c's unchanged RGB565
integer blends; --report validates the JIANT contrast pairings.
"""
import argparse
import sys
from pathlib import Path

# Compatibility catalogue: append-only historical indices, not rendering definitions.
PALETTE_NAMES = ("GREY", "GREEN", "AMBER", "ICE", "VIOLET", "ROSE", "PAPER",
                 "HI-CON", "NIGHT", "MONO", "JIANT")
JIANT_INDEX = 10
# bg, surf, text, theme, accent; preserve the original RGB565 conversion exactly.
JIANT_COLORS = ((0, 0, 0), (10, 9, 14), (244, 241, 234), (184, 112, 255), (255, 122, 0))
JIANT_KNOBS = ((138, 108, 255), (255, 210, 60), (88, 116, 255), (255, 75, 75))
JIANT_RAISE_PCT = 4
# the 20 palettes of the earlier firmware (index order) -> the new palette
OLD = ["GREEN", "AMBER", "CYAN", "RED", "MONO", "VIOLET", "PINK", "ICE", "WARM", "OCEAN", "DUSK", "HI-CON",
       "LIGHT", "PAPER", "SKY", "MINT", "LILAC", "ROSE", "SAND", "L-HICON"]
OLD_TO_NEW = {"GREEN": "GREEN", "AMBER": "AMBER", "CYAN": "ICE", "RED": "ROSE", "MONO": "GREY", "VIOLET": "VIOLET",
              "PINK": "ROSE", "ICE": "ICE", "WARM": "AMBER", "OCEAN": "ICE", "DUSK": "VIOLET", "HI-CON": "HI-CON",
              "LIGHT": "PAPER", "PAPER": "PAPER", "SKY": "PAPER", "MINT": "PAPER", "LILAC": "PAPER", "ROSE": "PAPER",
              "SAND": "PAPER", "L-HICON": "HI-CON"}
PAL_TAG = 64                       # stored id = PAL_TAG + index; below 20: an old id
REC_DARK = (255, 72, 72)
QR_LIGHT, QR_DARK, CRASH_BG, CRASH_INK = (255, 255, 255), (0, 0, 0), (160, 0, 0), (255, 255, 255)
PCT = {"MID": 70, "DIM": 42, "LINE": 18, "SEL": 80, "TINT": 12, "KEY": 78}


def to565(c):
    r, g, b = c
    if r == g == b:                                   # preserve the historical conversion (including fixed QR white)
        return ((r >> 3) << 11) | ((r >> 3) << 6) | (r >> 3)
    return ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)


def from565(v):
    return ((v >> 11) * 255 // 31, ((v >> 5) & 63) * 255 // 63, (v & 31) * 255 // 31)


def cdiv(a, b):
    return a // b if a >= 0 else -((-a) // b)


def mix(a, b, pct):
    """src/gfx.c ux_mix: a + (b - a) * pct / 100 per channel, rounded"""
    out = 0
    for sh, mask in ((11, 31), (5, 63), (0, 31)):
        x, y = (a >> sh) & mask, (b >> sh) & mask
        d = (y - x) * pct
        out |= (x + cdiv(d + (50 if d >= 0 else -50), 100)) << sh
    return out


def lum(c565):
    def lin(v):
        v /= 255.0
        return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4
    r, g, b = from565(c565)
    return 0.2126 * lin(r) + 0.7152 * lin(g) + 0.0722 * lin(b)


def contrast(a, b):
    la, lb = lum(a), lum(b)
    return (max(la, lb) + 0.05) / (min(la, lb) + 0.05)


def derive():
    bg, surf, text, theme, accent = map(to565, JIANT_COLORS)
    d = dict(bg=bg, surf=surf, text=text, theme=theme, accent=accent,
             mid=mix(bg, text, PCT["MID"]), dim=mix(bg, text, PCT["DIM"]),
             line=mix(bg, text, PCT["LINE"]), sel=mix(bg, theme, PCT["SEL"]),
             tint=mix(bg, theme, PCT["TINT"]), raise_=mix(surf, text, JIANT_RAISE_PCT),
             key=mix(bg, text, PCT["KEY"]), ink=bg, rec=to565(REC_DARK))
    d.update({f"k{i}": to565(c) for i, c in enumerate(JIANT_KNOBS)})
    return "JIANT", d


# (foreground, background, minimum contrast, why)
CHECKS = [("text", "bg", 12.0, "body text"), ("text", "surf", 9.0, "body text on a surface"),
          ("mid", "bg", 5.0, "labels"), ("mid", "surf", 4.0, "labels on a surface"),
          ("dim", "bg", 2.2, "inactive (not read)"), ("line", "bg", 1.25, "1 px dividers"),
          ("theme", "bg", 4.5, "values, curves"), ("theme", "surf", 4.5, "values on a surface"),
          ("accent", "bg", 4.5, "hot value, cursor, playhead"), ("accent", "surf", 4.5, "hot on a surface"),
          ("ink", "sel", 4.5, "text on a selection"), ("ink", "theme", 4.5, "text on a THEME fill"),
          ("sel", "bg", 1.8, "gauge fill vs track"), ("rec", "bg", 3.0, "REC (graphic)"),
          ("text", "raise_", 7.0, "text on a RAISE chip or button"), ("ink", "rec", 4.5, "text on the REC chip"),
          ("ink", "key", 4.5, "a keycap's label"), ("key", "bg", 3.0, "a keycap on the background"),
          ("key", "surf", 2.5, "a keycap on a surface"), ("ink", "dim", 1.8, "an unavailable keycap's label"),
          ("ink", "accent", 4.5, "text on the ARM badge")] + \
         [(f"k{i}", b, 4.5 if b != "raise_" else 4.0, f"KNOB {i + 1}'s value") for i in range(4) for b in ("bg", "surf", "raise_")]


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("out", nargs="?")
    ap.add_argument("--report", action="store_true")
    a = ap.parse_args()
    fails = []
    assert len(PALETTE_NAMES) == 11 and PALETTE_NAMES[JIANT_INDEX] == "JIANT" and len(OLD) == 20
    rows = [derive()]
    for name, d in rows:
        for f, b, lim, why in CHECKS:
            c = contrast(d[f], d[b])
            if c < lim:
                fails.append(f"{name}: {f}/{b} {c:.2f} < {lim} ({why})")
    if a.report:
        print(f"{'palette':8s} " + " ".join(f"{f}/{b}".ljust(12) for f, b, _, _ in CHECKS))
        for name, d in rows:
            print(f"{name:8s} " + " ".join(f"{contrast(d[f], d[b]):5.1f}".ljust(12) for f, b, _, _ in CHECKS))
        print("minimum  " + " ".join(f"{lim:<12.2f}" for _, _, lim, _ in CHECKS))
    if a.out:
        names = PALETTE_NAMES
        out = ["/* generated by tools/gen_ui_palettes.py: historical identities; JIANT-only rendering */",
               "#pragma once", "#include <stdint.h>", "",
               "/* Compatibility metadata only: historical indices must not change. */",
               "static const ui_pal_t UI_PALETTES[] = {"]
        out += [f'    {{"{name}"}},' for name in names]
        out += ["};", "static const ui_colors_t UI_JIANT_COLORS = {" +
                ", ".join(f"0x{to565(c):04x}" for c in JIANT_COLORS) + "};",
                "static const uint16_t UI_JIANT_KNOBS[4] = {" +
                ", ".join(f"0x{to565(c):04x}" for c in JIANT_KNOBS) + "};",
                f"#define UI_JIANT_RAISE_PCT {JIANT_RAISE_PCT}",
                f"#define UI_NPALETTES {len(names)}u", "#define UI_GREY_INDEX 0u",
                f"#define UI_BW_INDEX {names.index('MONO')}u",
                f"#define UI_JIANT_INDEX {JIANT_INDEX}u",
                f"#define UI_PAL_TAG {PAL_TAG}u"]
        out += [f"#define UI_{k}_PCT {v}" for k, v in PCT.items()]
        out += [f"#define UI_REC_DARK 0x{to565(REC_DARK):04x}u",
                f"#define UI_QR_LIGHT 0x{to565(QR_LIGHT):04x}u", f"#define UI_QR_DARK 0x{to565(QR_DARK):04x}u",
                f"#define UI_CRASH_BG 0x{to565(CRASH_BG):04x}u", f"#define UI_CRASH_INK 0x{to565(CRASH_INK):04x}u", "",
                "/* the 20 palettes of earlier firmware: " + " ".join(OLD) + " */",
                "static const uint8_t UI_PALETTE_MIGRATE[20] = {" +
                ", ".join(str(names.index(OLD_TO_NEW[n])) for n in OLD) + "};", ""]
        Path(a.out).write_text("\n".join(out), encoding="utf-8", newline="\n")
    for f in fails:
        print("FAIL", f)
    if fails:
        sys.exit(1)


if __name__ == "__main__":
    main()
