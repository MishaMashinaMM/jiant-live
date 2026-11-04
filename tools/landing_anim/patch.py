# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
"""The copy's drawing primitives note what they draw (tools/landing_anim/capture.c's cap_*): og_line_a, og_plot
(outside a line), cv_rect, cv_text_flags. patch.py SRC_DIR"""
import sys
from pathlib import Path
src = Path(sys.argv[1])
o = src / "ui_organic.c"
s = o.read_text()
a = "static void og_line_a(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, int32_t a)\n{"
assert s.count(a) == 1
s = s.replace(a, "static void og_line_a_raw(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, int32_t a)\n{")
b = "static void og_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c)"
assert s.count(b) == 1
s = s.replace(b, "static void og_line_a(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint16_t c, int32_t a)\n"
              "{ if (cap_on) cap_line(x0, y0 + cv_oy * 16, x1, y1 + cv_oy * 16, c, a); cap_inl++; og_line_a_raw(x0, y0, x1, y1, c, a); cap_inl--; }\n" + b)
p = "static inline void og_plot(int32_t x, int32_t y, uint16_t c, int32_t a)\n{"
assert s.count(p) == 1
s = s.replace(p, p + "\n    if (cap_on && !cap_inl) cap_plot(x, y + cv_oy, c, a);")
n = "static void og_node(int32_t cx, int32_t cy, int32_t r, uint16_t c)\n{"
assert s.count(n) == 1
s = s.replace(n, "static void og_node_raw(int32_t cx, int32_t cy, int32_t r, uint16_t c)\n{")
lb = "static void og_label("
assert s.count(lb) == 1
s = s.replace(lb, "static void og_node(int32_t cx, int32_t cy, int32_t r, uint16_t c)\n"
              "{ if (cap_on) cap_node(cx, cy + cv_oy * 16, r, c); cap_inl++; og_node_raw(cx, cy, r, c); cap_inl--; }\n" + lb)
o.write_text(s)
g = src / "gfx.c"
s = g.read_text()
r = "static void cv_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint16_t c)\n{"
assert s.count(r) == 1
s = s.replace(r, r + "\n    if (cap_on) cap_rect(x, y + cv_oy, w, h, c);")
t = "                             uint32_t flags)\n{"
assert s.count(t) == 1
s = s.replace(t, t + "\n    if (cap_on) cap_text(x, y + cv_oy, f == &AF_X ? 0 : f == &AF_S ? 1 : 2, fg, s);")
g.write_text(s)
