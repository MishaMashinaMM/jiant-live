/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT 0.6.34; 0.6.35 short, no names) the DICE ALL film's footage: the generator thrown live on the host build of the
 * tests (tests/ui_test.c), the real engines playing each piece at its own tempo, the FM-1's screen at 30 fps. About a
 * minute: a first piece under the splash, then a throw every four seconds or so, each one HOME + SELECT (the dialog),
 * OCT+ (YES) and the piece while the camera visits a page or two (the beings, the lead's piano roll, the kit, the
 * mixer); the last one under the outro. No captions: compose.py rolls an animated die over each throw (its times in
 * seps.txt). Every beat's time goes to beats.txt (the tempo changes), the intro's words to intro.txt.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/video_dice/capture tools/video/capture_dice.c -lm
 *   build/video_dice/capture build/video_dice      -> audio.wav, screen.rgb, scenes.txt, beats.txt, seps.txt, intro.txt
 *   python3 tools/video/compose.py build/video_dice -> build/video_dice/jiant-fm.mp4 */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_VERSION "v0.7.6"
#include "ui_test.c"

#define FPS 30u
#define SPLASH_S 3.0
#define FIRST_S 5.0                                     /* the intro title until here */
#define PIECE_S 3.3                                     /* about how long a piece plays */
static FILE *wav, *rgb, *cap, *bts, *sep;
static uint32_t nsamp;
enum { G_IDM, G_DIDM, G_DMIN, G_DUB, G_DELEC, G_ACID, G_COLD, G_BROKEN, G_HYPNO, G_BREAKS, G_SOUND, G_HARD, G_INDIE,
       G_PUNK, G_CLASS };
/* the throws: genre, energy (the night's mostly, as the generator throws them) */
static const struct { int8_t g, e; } SCRIPT[] = {
    {G_DIDM, 0}, {G_DMIN, 1}, {G_IDM, 2}, {G_DUB, 1}, {G_ACID, 2}, {G_SOUND, 0}, {G_HYPNO, 1}, {G_DELEC, 2},
    {G_CLASS, 0}, {G_BREAKS, 2}, {G_HARD, 2}, {G_DIDM, 0},
};
#define NSCRIPT (sizeof SCRIPT / sizeof SCRIPT[0])

static void hold_btn(uint32_t label, int on)
{
    if (on) { fm1_in.buttons |= 1u << panel.btn[label]; host_pressed |= 1u << panel.btn[label]; }
    else fm1_in.buttons &= ~(1u << panel.btn[label]);
}
static void enc_turn(uint32_t role, int32_t s) { host_enc[panel.enc[role]] += s * panel.dir[role]; }
static void put_screen(void)
{
    static uint8_t buf[240u * 240u * 3u];
    uint32_t i;
    for (i = 0; i < 240u * 240u; i++) {
        uint16_t c = swap16(host_screen[i]);
        buf[3u * i] = (uint8_t)((c >> 11) * 255u / 31u);
        buf[3u * i + 1u] = (uint8_t)(((c >> 5) & 63u) * 255u / 63u);
        buf[3u * i + 2u] = (uint8_t)((c & 31u) * 255u / 31u);
    }
    fwrite(buf, 1, sizeof buf, rgb);
}
static int plays(uint32_t k)
{
    uint32_t i;
    for (i = 0; i < (uint32_t)trk[k].p[P_SLEN]; i++) if (trk[k].step[i].time == ST_NOTE) return 1;
    return 0;
}
/* the melodic track to watch: of those that play above the bass (their notes' average from E3), the one with the
 * most notes (the lead or the arp, rarely the chords); else any that plays */
static int32_t show_track(void)
{
    uint32_t k, i, best = NTRK, bn = 0;
    for (k = 0; k < NTRK; k++) {
        uint32_t cnt = 0, sum = 0;
        if (!plays(k) || drum_track(&trk[k])) continue;
        for (i = 0; i < (uint32_t)trk[k].p[P_SLEN]; i++)
            if (trk[k].step[i].time == ST_NOTE && trk[k].step[i].n) { cnt++; sum += trk[k].step[i].note[0]; }
        if (cnt && sum / cnt >= 52u && cnt > bn) { bn = cnt; best = k; }
    }
    if (best < NTRK) return (int32_t)best;
    for (k = 0; k < NTRK; k++) if (plays(k) && !drum_track(&trk[k])) return (int32_t)k;
    return 0;
}
static void look(uint32_t view)                         /* the camera: the beings, the piano roll, the beings, the kit */
{
    int32_t k = show_track();
    uint32_t dr = NTRK, j;
    for (j = 0; j < NTRK; j++) if (drum_track(&trk[j]) && plays(j)) dr = j;
    switch (view % 4u) {                                /* (0.6.36: HOME's beings most, no mixer) */
    case 1: song.sel = (uint8_t)k; go_title("STEP"); break;
    case 3: if (dr < NTRK) { song.sel = (uint8_t)dr; go_title("EDIT 1"); } else { song.sel = (uint8_t)k; go_home(); } break;
    default: song.sel = (uint8_t)(view & 2u && dr < NTRK ? dr : (uint32_t)k); go_home(); break;
    }
}

int main(int argc, char **argv)
{
    char p[512];
    const char *dir = argc > 1 ? argv[1] : "build/video_dice";
    uint32_t f, si = 0, phase = 0, view = 0, total = 0;
    double t_ev = 0, t_end_piece = 0, t_look = 0, beat_acc = 0, end_t = 1e9;
    int first_screen = 1;
    snprintf(p, sizeof p, "%s/audio.wav", dir); wav = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/screen.rgb", dir); rgb = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/scenes.txt", dir); cap = fopen(p, "w");
    snprintf(p, sizeof p, "%s/beats.txt", dir); bts = fopen(p, "w");
    snprintf(p, sizeof p, "%s/seps.txt", dir); sep = fopen(p, "w");
    if (!wav || !rgb || !cap || !bts || !sep) { fprintf(stderr, "cannot write in %s\n", dir); return 1; }
    wav_hdr(wav, 0);                                    /* (its length written at the end) */
    snprintf(p, sizeof p, "%s/intro.txt", dir);
    { FILE *it = fopen(p, "w"); if (it) { fputs("DICE ALL|one press, a whole new piece", it); fclose(it); } }
    ui_power_on();
    settings.palette = UI_JIANT_INDEX; palette_set(UI_JIANT_INDEX);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();          /* TRACKS 6 */
    song.master_q12 = 4096;
    host_ticks = 777u;
    dice_force_style = SCRIPT[0].g; dice_force_energy = SCRIPT[0].e;
    dice_all();                                         /* the first piece under the splash */
    transport_req = 1;
    fprintf(cap, "%.4f\t\t \t\t0\n", FIRST_S);          /* (one silent scene: the intro title ends there) */
    t_end_piece = FIRST_S + PIECE_S * 0.6; t_look = FIRST_S; si = 1; phase = 3;
    for (f = 0; ; f++) {
        double t = (double)f / FPS;
        uint32_t want = (uint32_t)((uint64_t)(f + 1u) * FS / FPS);
        if (t >= end_t) break;
        /* ---- the script */
        if (phase == 3 && t >= t_look && t < t_end_piece) {   /* a page every two seconds or a bar, whichever longer */
            double bar_s = 4.0 * 60.0 / song.g[G_BPM];
            look(view++);
            t_look = t + (bar_s > 2.0 ? bar_s : 2.0);
        }
        if (phase == 3 && t >= t_end_piece) {
            if (si >= NSCRIPT) {                        /* the last piece goes on under the outro */
                fprintf(cap, "%.4f\t\t\t\t0\n", t);
                end_t = t + 6.5;
                phase = 9;
            } else {                                    /* the next throw: HOME + SELECT, the die rolls */
                fprintf(sep, "%.4f\n", t);
                song.sel = 0; go_home(); enc_turn(EN_SELECT, 1); fm1_ms += 1200u;
                phase = 1; t_ev = t + 0.42;
            }
        }
        if (phase == 1 && t >= t_ev) {                  /* OCT+: YES */
            dice_force_style = SCRIPT[si].g; dice_force_energy = SCRIPT[si].e;
            hold_btn(B_OCTUP, 1);
            phase = 2; t_ev = t + 0.07;
        } else if (phase == 2 && t >= t_ev) {           /* the button up: the dice (a tap acts on its release) */
            double bar_s;
            hold_btn(B_OCTUP, 0);
            ui_input();                                 /* (taken this frame: the piece's tempo for its length) */
            ui.msg_t = 0; ui.msg[0] = ui.msg2[0] = 0;   /* (no genre's name on the screen: the film names nothing) */
            bar_s = 4.0 * 60.0 / song.g[G_BPM];
            {
                uint32_t nb = (uint32_t)(PIECE_S / bar_s + 0.5);
                if (nb < 1u) nb = 1u;
                if (si + 1u == NSCRIPT) nb += 1u;       /* (the last a little longer) */
                t_end_piece = t + nb * bar_s;
            }
            t_look = t + 0.25;
            si++; phase = 3;
        }
        /* ---- the audio up to this frame's end, the beats' times */
        while (nsamp < want) {
            int32_t o[2 * CTL];
            int16_t s[2 * CTL];
            uint32_t i;
            mix_block(o, CTL);
            for (i = 1; i < CTL; i += 2u)
                scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)clamp(o[2u * i], -32768, 32767);
            for (i = 0; i < 2u * CTL; i++) s[i] = (int16_t)clamp(o[i], -32768, 32767);
            fwrite(s, 2, 2u * CTL, wav);
            nsamp += CTL;
            beat_acc += (double)CTL * song.g[G_BPM] / (60.0 * FS);
            if (beat_acc >= 1.0) { beat_acc -= 1.0; fprintf(bts, "%.4f\n", (double)nsamp / FS); }
        }
        host_ticks += 1000000u / FPS; fm1_ms += 1000u / FPS;
        if (t < SPLASH_S) {
            if (f == 0) { spl_anim = 1; draw_splash(); }
            splash_write((uint32_t)(t * 1000.0));
        } else {
            if (first_screen) { ui.force = 1; first_screen = 0; go_home(); }
            ui_input();
            ui_draw();
        }
        put_screen();
        total++;
    }
    fclose(rgb); fclose(cap); fclose(bts); fclose(sep);
    fclose(wav);
    snprintf(p, sizeof p, "%s/audio.wav", dir);
    { FILE *w2 = fopen(p, "r+b"); if (w2) { wav_hdr(w2, nsamp); fclose(w2); } }
    {   /* scenes.txt's header line first (compose.py reads it): splash, end, beat */
        FILE *in, *out;
        char path2[600], line[512];
        snprintf(p, sizeof p, "%s/scenes.txt", dir);
        snprintf(path2, sizeof path2, "%s/scenes.tmp", dir);
        in = fopen(p, "r"); out = fopen(path2, "w");
        fprintf(out, "# %u fps, dice film, splash %.4f s, end %.4f s, beat %.6f s\n", FPS, SPLASH_S, (double)total / FPS, 0.5);
        while (in && fgets(line, sizeof line, in)) fputs(line, out);
        if (in) fclose(in);
        fclose(out);
        rename(path2, p);
    }
    printf("%u frames, %u samples\n", total, nsamp);
    return 0;
}
