/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * The front page's animations (web/landing/anim/): the firmware's own creatures, recorded from its own drawing code.
 * The browser emulator's sources (web/emu/felucca_web.c: the whole firmware on a simulated FM-1) built for the host,
 * with ui_organic.c's og_line_a / og_plot and gfx.c's cv_rect / cv_text noting what they draw (tools/landing_anim/
 * build.sh patches a copy: the firmware is not touched). Each engine on track 1 plays one bar at 118 BPM (the page's
 * demo clock) on the touch keys; every frame of the bar (32 a bar, 24 for ECO, then a few more for the loop's crossfade) its
 * creature (ui_engvis.c vx_draw, as on HOME's BEING) is drawn and its lines written out; HOME's ECO the same with the
 * generator's song playing on all six tracks.
 *   capture OUT.txt   ->  F (a frame), L x0 y0 x1 y1 c a (Q4 px, RGB565, 0..256), P x y c a (px), R x y w h c,
 *                         N x y r c (a filled dot: Q4 centre, px radius), T x y font c text; "S name w h frames-a-bar crossfade" starts a clip (tools/landing_anim/pack.py packs it) */
#include <stdio.h>
#include <stdint.h>
static FILE *cap_f;
static int cap_on, cap_inl;
static void cap_line(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint32_t c, int32_t a) { fprintf(cap_f, "L %d %d %d %d %u %d\n", x0, y0, x1, y1, c, a); }
static void cap_plot(int32_t x, int32_t y, uint32_t c, int32_t a) { fprintf(cap_f, "P %d %d %u %d\n", x, y, c, a); }
static void cap_node(int32_t x, int32_t y, int32_t r, uint32_t c) { fprintf(cap_f, "N %d %d %d %u\n", x, y, r, c); }
static void cap_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t c) { fprintf(cap_f, "R %d %d %d %d %u\n", x, y, w, h, c); }
static void cap_text(int32_t x, int32_t y, int font, uint32_t c, const char *s) { fprintf(cap_f, "T %d %d %d %u %s\n", x, y, font, c, s); }
#define EXPORT
#include "web/emu/felucca_web.c"

static double la_bar_ms;
/* keys held at step s (0..15) of the bar: a mask of the 27 touch keys (bit 0 = F3) */
static uint32_t la_pat[16];
static void la_ms(uint32_t n) { while (n--) web_ms(); }
static uint32_t la_t0;
static int la_hold;                     /* ECO: the beings kept in their places (a loop that closes) */
static void la_run_to(double ms_since_t0)
{
    while ((double)(fm1_ms - la_t0) < ms_since_t0) {
        double t = (double)(fm1_ms - la_t0), step = la_bar_ms / 16.0;
        uint32_t s = (uint32_t)(t / step), in = (uint32_t)((t - s * step) * 100.0 / step);
        web_keys(in < 55u ? la_pat[s % 16u] : 0u);
        if (la_hold) {
            uint32_t c;
            for (c = 0; c < ECO_N; c++) {
                eco.sx[c] = ECO_PX[c] * 256 + (c == 1u ? 10 * 256 : 0);
                eco.sy[c] = (ECO_PY[c] + 8) * 256;
                eco.vx[c] = eco.vy[c] = 0;
                trk[c].p[P_MUTE] = 0;
            }
            trk_live = NTRK;
        }
        web_ms();
    }
}
static void la_canvas(uint32_t w, uint32_t h)
{
    cv_w = w;
    cv_h = h;
    cv_oy = 0;
    memset(cv_px, 0, w * h * 2u);
}
/* bars of the loop: warm (not written) then one written */
static void la_clip(const char *name, uint32_t w, uint32_t h, void (*draw)(void), uint32_t warm, uint32_t fpb, uint32_t xf)
{
    uint32_t f;
    double base;
    la_t0 = fm1_ms;
    la_run_to(la_bar_ms * warm);
    base = la_bar_ms * warm;
    fprintf(cap_f, "S %s %u %u %u %u\n", name, w, h, fpb, xf);
    for (f = 0; f < fpb + xf; f++) {
        la_run_to(base + la_bar_ms * f / fpb);
        la_canvas(w, h);
        if (la_hold) { uint32_t c; for (c = 0; c < NTRK; c++) trk[c].p[P_MUTE] = 0; trk_live = NTRK; }
        fprintf(cap_f, "F\n");
        cap_on = 1;
        draw();
        cap_on = 0;
    }
    web_keys(0);
    la_ms(600);
}
static void la_vx(void) { vx_draw(&trk[0], 0, 0, (int32_t)cv_w, (int32_t)cv_h, -1); }
static void la_eco(void) { eco_draw(0, 0, 1); }
static void la_track(uint32_t eng, uint32_t preset)
{
    uint32_t i;
    song.playing = 0;
    for (i = 0; i < NTRK; i++) trk[i].p[P_MUTE] = i != 0u;
    song.sel = 0;
    set_engine_of(&trk[0], eng);
    apply_preset_to(&trk[0], preset);
    la_ms(300);
}
int main(int argc, char **argv)
{
    uint32_t i;
    /* the keys: semitones from F3 */
    enum { C4 = 7, D4 = 9, E4 = 11, F4 = 12, G4 = 14, A4 = 16, B4 = 18, C5 = 19, D5 = 21, E5 = 23, G5 = 26 };
    static const uint8_t ARP[16] = {C4, G4, C5, E5, D5, C5, G4, E4, A4, C5, E5, G5, E5, C5, A4, E4};
    cap_f = fopen(argc > 1 ? argv[1] : "capture.txt", "w");
    la_bar_ms = 60000.0 / 118.0 * 4.0;
    web_nor_erase();
    web_boot();
    la_ms(9000);                                     /* the splash, the first frames */
    song.g[G_BPM] = 118;
    static const struct { const char *name; uint32_t eng, preset, kind; } E[] = {
        {"analog", ENGI_ANALOG, 0, 0}, {"fm6", ENGI_FM6, 0, 0}, {"phase", ENGI_PHASE, 0, 0}, {"lofi", ENGI_LOFI, 0, 0},
        {"voice", ENGI_VOICE, 0, 1}, {"wheel", ENGI_WHEEL, 0, 2}, {"noise", ENGI_NOISE, 0, 0}, {"drumx", ENGI_DRUM, 0, 3},
        {"animal", ENGI_ANIMAL, 0, 1},
    };
    for (i = 0; i < sizeof E / sizeof E[0]; i++) {
        uint32_t s;
        for (s = 0; s < 16u; s++) {
            if (E[i].kind == 3u)                       /* DRUM-X: the kit's keys, a beat */
                la_pat[s] = (s % 4u == 0u ? 1u << 0 : 0u) | (s % 8u == 4u ? 1u << 2 : 0u) | (s % 2u == 1u ? 1u << 4 : 0u) |
                            (s == 14u ? 1u << 7 : 0u) | (s == 11u ? 1u << 5 : 0u) | (s == 7u ? 1u << 9 : 0u);
            else if (E[i].kind == 1u)                  /* a voice: phrases, held */
                la_pat[s] = 1u << (s < 8u ? (s < 4u ? C4 : E4) : (s < 12u ? G4 : D4));
            else if (E[i].kind == 2u)                  /* the organ: chords */
                la_pat[s] = s < 8u ? (1u << C4 | 1u << E4 | 1u << G4) : (1u << A4 | 1u << C5 | 1u << E4);
            else
                la_pat[s] = 1u << ARP[s];
        }
        la_track(E[i].eng, E[i].preset);
        la_clip(E[i].name, 200, 150, la_vx, 2, 32, 6);
    }
    /* HOME's ECO: the generator's song on all six tracks */
    for (i = 0; i < 16u; i++) la_pat[i] = 0;
    trk_live = NTRK;                                 /* (TRACKS 6: every being awake) */
    web_gen(0, 7, 2, 0);
    trk_live = NTRK;
    for (i = 0; i < NTRK; i++) trk[i].p[P_MUTE] = 0;
    song.g[G_BPM] = 118;
    if (!song.playing) transport_req = 1;
    la_hold = 1;
    la_clip("eco", 240, 140, la_eco, 2, 24, 4);
    fclose(cap_f);
    return 0;
}
