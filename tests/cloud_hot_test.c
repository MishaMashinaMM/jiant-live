/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* CLOUD (src/fx.c rev_cloud) driven hot: the tester's "the reverb crackles when pushed into overdrive, CLOUD most".
 *   build/host/cloud_hot_test [WAVDIR]
 * For each setting the same send is rendered at a low level and at a hot one. CLOUD's grain scheduling does not
 * depend on the signal (its own LCG), so a clean (linear) path gives the hot render = the quiet one x gain: the
 * difference, as a level under the hot render, is the distortion the integer path adds (clamps in the line, the
 * combs, the allpasses). Also: blocks fx_insane would trip on (fx.c FX_SANE: the block muted, every line cleared,
 * core 1 given up), int16 clamps in CLOUD's line, grains whose read crosses the write head (old audio spliced in). */
#ifndef CLOUD_TAG
#define CLOUD_TAG "x"
#endif
#ifndef ROOM_CLEAN
#define ROOM_CLEAN 40000                                 /* the send ROOM's int16 lines take without a clamp */
#endif
#define main hostsim_main
#include "hostsim.c"
#undef main

static int bad;
static void check(const char *what, int ok)
{
    printf("cloud: %-96s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}
#define SECS 4u
#define NS (SECS * FS / CTL * CTL)
static int32_t wl[NS], wr[NS], ql[NS], qr[NS];

/* the send: a minor chord of saws, through an overdrive (tanh, x drive), at amp peak; the tester's DIST'd synth */
static double sat(double x) { return tanh(x); }
static int32_t send_at(uint32_t t, double amp, double drive)
{
    static const double F[4] = {110.0, 130.81, 164.81, 220.0};
    double s = 0;
    uint32_t k;
    for (k = 0; k < 4u; k++) {
        double ph = fmod(t * F[k] * (1.0 + 0.002 * k) / FS, 1.0);
        s += 2.0 * ph - 1.0;
    }
    s /= 4.0;
    if (drive > 1.0) s = sat(s * drive) / sat(drive);
    if (t > NS / 2u) s = 0;                              /* the second half: the tail alone */
    return (int32_t)(s * amp);
}

typedef struct { uint32_t trips, line_sat, cross, comb_sat, ap_sat; int32_t pk; } stats_t;
static void cl_reset(void)
{
    host_tracks_init();
    rev_clear();
    fx.rtype = 4;
    memset(fx.rm_m, 0, sizeof fx.rm_m);
    memset(fx.comb_i, 0, sizeof fx.comb_i); memset(fx.ap_i, 0, sizeof fx.ap_i);
    fx.cl_w = 0; fx.cl_rng = 0; fx.cl_next = 0; fx.rm_ph = 0; fx.cl_lp = 0;
    fxg_g = 32767; fxg_quiet = 0;
    song.g[G_RTYPE] = 4;
    song.g[G_DMIX] = 0;
    song.g[G_RFILT] = 0;
}
static void render(double amp, double drive, int32_t *ol, int32_t *or_, stats_t *st)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    uint32_t t, i, k, prevd[CL_N];
    memset(st, 0, sizeof *st);
    for (k = 0; k < CL_N; k++) prevd[k] = 0;
    for (t = 0; t < NS; t += CTL) {
        uint32_t w0 = fx.cl_w;
        for (i = 0; i < CTL; i++) {
            c[i] = d[i] = 0;
            r[i] = send_at(t + i, amp, drive);
            fx_sl[i] = fx_sr[i] = 0;
        }
        fx_buses(c, d, r, w, CTL);
        for (i = 0; i < CTL; i++) {
            ol[t + i] = w[i] + fx_sl[i];
            or_[t + i] = w[i] + fx_sr[i];
            if (abs(ol[t + i]) > st->pk) st->pk = abs(ol[t + i]);
        }
        {
            uint32_t m = 0;
            for (i = 0; i < CTL; i++)
                m |= ((uint32_t)w[i] + FX_SANE) | ((uint32_t)fx_sl[i] + FX_SANE);
            st->trips += m >= 2u * FX_SANE;
        }
        for (i = w0 >> 1; i < fx.cl_w >> 1; i++) {       /* the line samples written this block */
            int16_t v = cloud_buf[i & (CL_LEN - 1u)];
            st->line_sat += v == 32767 || v == -32768;
        }
        for (i = 0; i < sizeof rev_comb / 2u; i++)       /* the combs at their clamps (held values, a snapshot a block) */
            st->comb_sat += rev_comb[i] == 32767 || rev_comb[i] == -32768;
        for (i = 0; i < sizeof rev_u.ap / 2u; i++)       /* .. the allpasses */
            st->ap_sat += rev_u.ap[i] == 32767 || rev_u.ap[i] == -32768;
        for (k = 0; k < CL_N; k++) {                     /* a grain's read past the write head: d wraps */
            uint32_t dd = ((fx.cl_w >> 1) - (cl_g[k].pos >> 16)) & (CL_LEN - 1u);
            if (cl_g[k].len && cl_g[k].age > 2u * CTL && prevd[k] && prevd[k] < 4096u && dd > CL_LEN - 4096u)
                st->cross++;
            prevd[k] = cl_g[k].len ? dd : 0u;
        }
    }
}
/* the distortion: hot - quiet x g, against hot (dB), over the whole render */
static double nl_db(double g)
{
    double e = 0, s = 0;
    uint32_t i;
    for (i = 0; i < NS * 5u / 8u; i++) {             /* (the send and 0.5 s after: not the gate's fade) */
        double a = wl[i] - ql[i] * g, b = wr[i] - qr[i] * g;
        e += a * a + b * b;
        s += (double)wl[i] * wl[i] + (double)wr[i] * wr[i];
    }
    return 10.0 * log10((e + 1) / (s + 1));
}
/* .. the same through a 3rd difference (+18 dB / octave: what a clamp's corners add, the crackle), against the hot
 * render's own 3rd difference */
static double hf_db(double g)
{
    double e = 0, s = 0;
    uint32_t i;
    for (i = 3; i < NS * 5u / 8u; i++) {
        double a = (wl[i] - 3.0 * wl[i - 1] + 3.0 * wl[i - 2] - wl[i - 3]) - g * (ql[i] - 3.0 * ql[i - 1] + 3.0 * ql[i - 2] - ql[i - 3]);
        double h = wl[i] - 3.0 * wl[i - 1] + 3.0 * wl[i - 2] - wl[i - 3];
        e += a * a;
        s += h * h;
    }
    return 10.0 * log10((e + 1) / (s + 1));
}
static void wav(const char *path, const int32_t *l, const int32_t *r, uint32_t n, double scale)
{
    FILE *f = fopen(path, "wb");
    uint32_t i, v;
    if (!f) return;
    fwrite("RIFF", 1, 4, f); v = 36 + n * 4; fwrite(&v, 4, 1, f); fwrite("WAVEfmt ", 1, 8, f);
    v = 16; fwrite(&v, 4, 1, f);
    {
        uint16_t h[2] = {1, 2}; fwrite(h, 2, 2, f);
        v = FS; fwrite(&v, 4, 1, f); v = FS * 4; fwrite(&v, 4, 1, f);
        h[0] = 4; h[1] = 16; fwrite(h, 2, 2, f);
    }
    fwrite("data", 1, 4, f); v = n * 4; fwrite(&v, 4, 1, f);
    for (i = 0; i < n; i++) {
        double a = l[i] * scale, b = r[i] * scale;
        int16_t s[2] = {(int16_t)(a > 32767 ? 32767 : a < -32768 ? -32768 : a), (int16_t)(b > 32767 ? 32767 : b < -32768 ? -32768 : b)};
        fwrite(s, 2, 2, f);
    }
    fclose(f);
}

/* the loop's stability: a 0.25 s burst at a modest level, then silence; the tail's RMS in its last 0.5 s against the
 * first 0.5 s after the burst (dB; a dying tail is well below 0) and the line's clamps over the whole run */
static double stab_n(int sz, int rate, int mod, uint32_t *clamps, uint32_t secs)
{
    static int32_t c[CTL], d[CTL], r[CTL], w[CTL];
    double e0 = 0, e1 = 0, hp = 0;
    uint32_t t, i, n = secs * FS / CTL * CTL;
    cl_reset();
    song.g[G_RSIZE] = (int16_t)sz; song.g[G_RRATE] = (int16_t)rate; song.g[G_RMOD] = (int16_t)mod;
    song.g[G_RWIDE] = 0; song.g[G_RPRE] = 0;
    *clamps = 0;
    for (t = 0; t < n; t += CTL) {
        uint32_t w0 = fx.cl_w;
        for (i = 0; i < CTL; i++) {
            c[i] = d[i] = 0;
            r[i] = t + i < FS / 4u ? send_at(t + i, 12000, 1.0) : 0;
            fx_sl[i] = fx_sr[i] = 0;
        }
        c[0] = 1;                                        /* (the gate held open: the loop itself measured) */
        fx_buses(c, d, r, w, CTL);
        for (i = 0; i < CTL; i++) {                      /* (DC out: ROOM's floors hold an offset, inaudible; the gate's) */
            hp += (w[i] - hp) * (1.0 / 512.0);
            w[i] = (int32_t)(w[i] - hp);
        }
        for (i = 0; i < CTL; i++) {
            double v = w[i];
            if (t + i >= (secs > 5u ? 4u * FS + FS / 2u : FS / 4u) && t + i < (secs > 5u ? 5u * FS : FS * 3u / 4u)) e0 += v * v;
            if (t + i >= n - FS / 2u) e1 += v * v;
        }
        for (i = w0 >> 1; i < fx.cl_w >> 1; i++) {
            int16_t v = cloud_buf[i & (CL_LEN - 1u)];
            *clamps += v == 32767 || v == -32768;
        }
    }
    if (e1 < 400.0 * 400.0 * (FS / 2u))                 /* (under RMS 400, -38 dBFS: ROOM's own rounding residue, the */
        return -99.0;                                   /* limit cycle the gate hides: died) */
    return 10.0 * log10((e1 + 1) / (e0 + 1));
}

static double stab(int sz, int rate, int mod, uint32_t *clamps) { return stab_n(sz, rate, mod, clamps, 5u); }

int main(int argc, char **argv)
{
    {
        static const int SZ[7] = {64, 90, 100, 110, 120, 125, 126}, RT[5] = {0, 30, 60, 90, 127};
        uint32_t a, b, cl;
        int unstable = 0;
        printf("cloud: stability (tail 4.5-5 s vs 0.25-0.75 s, dB / line clamps), MOD 0; rows SIZE, columns RATE 0 30 60 90 127\n");
        for (a = 0; a < 7u; a++) {
            printf("cloud:   SIZE %3d:", SZ[a]);
            for (b = 0; b < 5u; b++) {
                double db = stab(SZ[a], RT[b], 0, &cl);
                printf("  %6.1f/%-6u", db, cl);
                unstable |= (SZ[a] <= 110 && db > -6.0) || cl > 0;
            }
            printf("\n");
        }
        check("CLOUD: up to SIZE 110 the tail dies (4.5 s on, 6 dB under its start or more), the line never clamps", !unstable);
        {
            double d1 = stab_n(126, 127, 0, &cl, 20u), d2;
            uint32_t cl2;
            d2 = stab_n(126, 127, 127, &cl2, 20u);
            printf("cloud:   SIZE 126 RATE 127: 19.5-20 s against 4.5-5 s: %.1f dB (MOD 127: %.1f dB), line clamps %u %u\n", d1, d2, cl, cl2);
            check("CLOUD SIZE 126 RATE 127 (MOD 0 and 127): the long wash still dies (20 s on under 4.5 s), never clamps",
                  d1 < -3.0 && d2 < -3.0 && !cl && !cl2);
        }
    }
    static const struct { const char *nm; int sz, rate, mod, wide, air; } S[] = {
        {"defaults SIZE 90", 90, 30, 0, 0, 0},
        {"SIZE 120 RATE 127", 120, 127, 0, 0, 0},
        {"SIZE 120 RATE 127 MOD 127 WIDE 127", 120, 127, 127, 127, 0},
        {"SIZE 126 RATE 127 MOD 80 WIDE 127", 126, 127, 80, 127, 0},
        {"SIZE 100 MOD 127 AIR +12", 100, 80, 127, 60, 12},
        {"SIZE 100 MOD 127 AIR +24", 100, 80, 127, 60, 24},
    };
    static const double AMP[4] = {16000, 40000, 90000, 200000};
    const char *dir = argc > 1 ? argv[1] : 0;
    uint32_t s, a;
    int worst_trip = 0;
    double worst_nl = -200;
    uint32_t cross_air = 0;
    int any_clamp = 0;
    for (s = 0; s < NELEM(S); s++) {
        for (a = 0; a < 4u; a++) {
            stats_t sq, sh;
            double g = 4.0, nl;
            cl_reset();
            song.g[G_RSIZE] = (int16_t)S[s].sz; song.g[G_RRATE] = (int16_t)S[s].rate; song.g[G_RMOD] = (int16_t)S[s].mod;
            song.g[G_RWIDE] = (int16_t)S[s].wide; song.g[G_RPRE] = (int16_t)S[s].air;
            render(AMP[a] / g, 4.0, ql, qr, &sq);
            cl_reset();
            song.g[G_RSIZE] = (int16_t)S[s].sz; song.g[G_RRATE] = (int16_t)S[s].rate; song.g[G_RMOD] = (int16_t)S[s].mod;
            song.g[G_RWIDE] = (int16_t)S[s].wide; song.g[G_RPRE] = (int16_t)S[s].air;
            render(AMP[a], 4.0, wl, wr, &sh);
            nl = nl_db(g);
            double hf = hf_db(g);
            printf("cloud: %-36s send %6.0f: peak %7d, distortion %6.1f dB, HF %6.1f dB, line clamps %6u, comb clamps %7u, ap clamps %7u, trips %3u, crossings %u\n",
                   S[s].nm, AMP[a], sh.pk, nl, hf, sh.line_sat, sh.comb_sat, sh.ap_sat, sh.trips, sh.cross);
            if (AMP[a] <= 90000 && hf > worst_nl) worst_nl = hf;
            if (sh.trips) worst_trip = 1;
            if (sh.line_sat || (AMP[a] <= ROOM_CLEAN && (sh.comb_sat || sh.ap_sat))) any_clamp = 1;
            if (S[s].air >= 24) cross_air += sh.cross;
            if (dir && s == 3 && a == 2) {
                char p[512];
                snprintf(p, sizeof p, "%s/cloud_hot_%s.wav", dir, CLOUD_TAG);
                wav(p, wl, wr, NS, 1.0 / 8.0);
                printf("cloud: wav %s\n", p);
            }
        }
    }
    printf("cloud: worst HF distortion up to a 90000 send: %.1f dB\n", worst_nl);
    check("CLOUD's line never clamps (any send), nor ROOM's combs / allpasses under it up to ROOM_CLEAN's send", !any_clamp);
    check("CLOUD never trips the buses' range check (FX_SANE) up to a 200000 send", !worst_trip);
    check("CLOUD AIR +24: no grain reads across the write head", cross_air == 0);
    {   /* (0.9.16) the tester: "CLOUD at SIZE 100 % seems off". SIZE 127 (FREEZE) from a cleared line held silence
         * (fx_rest clears it in any pause); and past 2 grains' overlap the grains fell 12 dB (high RATE). Now a
         * cleared line first takes a line's worth of sound, then holds; the level about even across RATE. */
        static const int RT[5] = {0, 30, 60, 90, 127};
        double e[3][5];
        uint32_t b, k, i;
        int ok = 1;
        stats_t st;
        for (k = 0; k < 3u; k++)
            for (b = 0; b < 5u; b++) {
                double s0 = 0, s1 = 0;
                cl_reset();
                song.g[G_RSIZE] = (int16_t)(k == 0 ? 127 : k == 1 ? 126 : 100); song.g[G_RRATE] = (int16_t)RT[b];
                song.g[G_RMOD] = 0; song.g[G_RWIDE] = 0; song.g[G_RPRE] = 0;
                render(16000, 1.0, wl, wr, &st);
                for (i = FS * 3u / 4u; i < 2u * FS; i++) s0 += (double)wl[i] * wl[i] + (double)wr[i] * wr[i];
                for (i = 3u * FS; i < 4u * FS; i++) s1 += (double)wl[i] * wl[i] + (double)wr[i] * wr[i];
                e[k][b] = 10.0 * log10(s0 / (2.5 * FS) + 1.0);
                if (k == 0) {
                    double held = 10.0 * log10(s1 / (2.0 * FS) + 1.0);
                    printf("cloud:   SIZE 127 RATE %3d: %5.1f dB with the send, %5.1f dB held 1-2 s after it\n", RT[b], e[0][b], held);
                    ok &= held > e[0][b] - 6.0;
                }
            }
        for (b = 0; b < 5u; b++) {
            printf("cloud:   RATE %3d: SIZE 127 %5.1f dB, 126 %5.1f dB, 100 %5.1f dB\n", RT[b], e[0][b], e[1][b], e[2][b]);
            ok &= e[0][b] > e[1][b] - 6.0 && e[2][b] > e[2][0] - 8.0;
        }
        check("CLOUD SIZE 127 from a cleared line, any RATE: sounds (within 6 dB of 126) and holds; RATE 127 within 8 dB of 0", ok);
    }
    printf(bad ? "cloud hot test FAILED\n" : "cloud hot test passed\n");
    return bad != 0;
}
