/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT 0.6.89) the README's newer screens: the presentation song (capture.c's) plays a few bars, then each page
 * below is shown with something on it (the lead off the grid on TIMING, HUMAN turned, the delay feeding back, ...)
 * and its screen, after a second of it alive, goes to shots.rgb (240 x 240 RGB24 each, in this order; names in
 * shots.txt), for tools/gen_readme_hero.py.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/video/capture_shots tools/video/capture_shots.c -lm
 *   build/video/capture_shots build/video */
#define CAPTURE_NO_MAIN 1
#include "capture.c"

static void run_s(double s)                         /* the song on for s seconds, the screen drawn at 30 fps */
{
    uint32_t f, n = (uint32_t)(s * FPS);
    for (f = 0; f < n; f++) {
        uint32_t want = nsamp + FS / FPS;
        while (nsamp < want) { int32_t o[2 * CTL]; uint32_t i; mix_block(o, CTL);
            for (i = 1; i < CTL; i += 2u) scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)clamp(o[2u * i], -32768, 32767);
            nsamp += CTL; }
        host_ticks += 1000000u / FPS; fm1_ms += 1000u / FPS;
        ui_input(); ui_draw();
    }
}
static FILE *names;
static void shot(const char *name) { run_s(1.2); put_screen(); fprintf(names, "%s\n", name); }
int main(int argc, char **argv)
{
    char p[512];
    const char *dir = argc > 1 ? argv[1] : "build/video";
    uint32_t i, k;
    snprintf(p, sizeof p, "%s/shots.rgb", dir); rgb = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/shots.txt", dir); names = fopen(p, "w");
    if (!rgb || !names) { fprintf(stderr, "cannot write in %s\n", dir); return 1; }
    song_build();
    ui.force = 1;
    for (k = 0; k < NTRK; k++) mute(k, 0);
    drums(MIN2);
    run_s(4);
    /* TIMING: the lead's notes off the grid, the cursor on one late */
    for (i = 0; i < 64u; i++)
        if (trk[T_LEAD].step[i].time == ST_NOTE) ST_MICRO_SET(&trk[T_LEAD].step[i], (int32_t)((i * 7u) % 13u) - 6);
    trk[T_LEAD].p[P_HRQ] = 40;
    page(T_LEAD, "TIMING");
    for (i = 0; i < 16u && !(trk[T_LEAD].step[i].time == ST_NOTE && ST_MICRO(&trk[T_LEAD].step[i]) > 2); i++)
        ;
    ui.cursor = (uint8_t)(i % 16u); ui.bank = 0;
    shot("TIMING");
    trk[T_LEAD].p[P_HVEL] = 60; trk[T_LEAD].p[P_HTIM] = 80; trk[T_LEAD].p[P_HGAT] = 90;   /* (stored 127 - the amount) */
    page(T_LEAD, "HUMAN"); shot("HUMAN");
    page(T_BASS, "STRETCH"); enc_turn(EN_K1, 1); shot("STRETCH");
    trk[T_PAD].p[P_CHRD] = 2; trk[T_PAD].p[P_CVOI] = 1; trk[T_PAD].p[P_CSTR] = 30;
    page(T_PAD, "CHORD"); shot("CHORD");
    song.g[G_DFDBK] = 108; song.g[G_DCOLOR] = 40; song.g[G_DMIX] = 90; trk[T_ARP].p[P_DLY] = 110;
    page(T_ARP, "DLY"); shot("DLY");
    song.g[G_DMOD] = 40; song.g[G_WIDTH] = 60;
    page(T_ARP, "DLY 2"); shot("DLY 2");
    page(T_LEAD, "NOTE"); ui.cursor = (uint8_t)(i % 16u); shot("NOTE");
    fclose(rgb); fclose(names);
    printf("shots in %s\n", dir);
    return 0;
}
