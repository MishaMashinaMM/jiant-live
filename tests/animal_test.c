/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* ANIMAL engine test (src/eng_animal.c, JIANT 0.8.3) on the host, through hostsim.c as regress.c.
 *   build/host/animal_test [DEMODIR]          (run_tests.sh: build/animal_demo)
 * 1. CMOS: the fast relaxation oscillator plays the key an octave up (gate only, no sweep: +-10 cents, A3 .. A6);
 *    CMOS BIRD at C5 matches the owner's video: the chirps' energy at 1.0 .. 1.3 kHz, repeating 2.4 .. 4.5 a second,
 *    each a rising hook (the pitch early in a chirp below the pitch late in it); CMOS TRILL 8 .. 10 chirps a second
 *    at a near constant ~1.1 kHz.
 * 2. PULSE: CRICKET's carrier 4.3 .. 5.3 kHz at C5, its chirps 1 .. 3 a second; CICADA's 3.8 .. 4.8 kHz; TEMP
 *    scales the rates (Dolbear): the cricket's chirp rate at 30 C over 14 C as an_temp's ratio (+-12 %).
 * 3. WHSTL: the Hopf oscillator's pitch follows the key (an octave apart: x2 +-1 %, at the key +-40 cents with the
 *    amplitude's pull); born by bifurcation: below the threshold (PRESS 15) silent, above it a tone whose level
 *    grows with PRESS (r ~ sqrt(mu)).
 * 4. every preset: sounds (RMS), no DC, ends after the release, a note from silence the same samples twice; every
 *    mode at the corners of its knobs, 8 notes at velocity 127, LEVEL 127: no voice sum past 2 x VOICE_FS a voice
 *    and no mix sample at full scale.
 * 5. cost: host ns per sample and voice, per mode (for information; the counts: tests/cpu_vg.c eng/ANIMAL.*).
 * 6. demos into DEMODIR: every preset playing a short phrase (each note a call), and the five modes. */
#define main hostsim_main
#include "hostsim.c"
#undef main

#define NFFT 8192
static int bad;
static void check(const char *what, int ok)
{
    printf("animal: %-96s %s\n", what, ok ? "ok" : "FAIL");
    bad += !ok;
}

static uint32_t eng_an(void)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++)
        if (ENGINES[e] == &ENG_ANIMAL)
            return e;
    return 0;
}
static int32_t preset_of(const char *name)
{
    uint32_t k;
    for (k = 0; k < ENG_ANIMAL.npresets; k++)
        if (str_eq(ENG_ANIMAL.presets[k].name, name))
            return (int32_t)k;
    printf("animal: no preset %s\n", name);
    bad++;
    return 0;
}

/* track 0 ANIMAL: preset pi (or -1: INIT), then e[] if given; ATK 0, SUS full, no sends */
static void setup(int32_t pi, const int16_t *e)
{
    uint32_t i;
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], eng_an(), pi < 0 ? 0 : (uint32_t)pi);
    if (e)
        for (i = 0; i < 8u; i++)
            trk[0].p[P_E0 + i] = e[i];
    trk[0].p[P_ATK] = 0;
    trk[0].p[P_SUS] = 127;
    trk[0].p[P_VOICE] = V_POLY;
    trk[0].p[P_CHOR] = trk[0].p[P_DLY] = trk[0].p[P_REV] = 0;
}

/* the voices' sum (before the mix) of a held note, n samples after `skip` s */
static void voice_render(uint32_t note, double skip, double *y, uint32_t n)
{
    static int32_t b[CTL];
    uint32_t i, k, sk = (uint32_t)(skip * FS) / CTL;
    trk_note_on(&trk[0], note, 100);
    for (i = 0; i < sk; i++)
        track_render(&trk[0], b, CTL);
    for (i = 0; i < n; i += CTL) {
        track_render(&trk[0], b, CTL);
        for (k = 0; k < CTL && i + k < n; k++)
            y[i + k] = b[k] / 32768.0;
    }
}

/* ------------------------------------------------------------ analysis --- */
static void fft(double *re, double *im, uint32_t n)
{
    uint32_t i, j, len;
    for (i = 1, j = 0; i < n; i++) {
        uint32_t bit = n >> 1;
        for (; j & bit; bit >>= 1)
            j ^= bit;
        j ^= bit;
        if (i < j) {
            double t = re[i]; re[i] = re[j]; re[j] = t;
            t = im[i]; im[i] = im[j]; im[j] = t;
        }
    }
    for (len = 2; len <= n; len <<= 1) {
        double a = -2 * M_PI / len;
        for (i = 0; i < n; i += len)
            for (j = 0; j < len / 2; j++) {
                double wr = cos(a * j), wi = sin(a * j);
                double xr = re[i + j + len / 2] * wr - im[i + j + len / 2] * wi;
                double xi = re[i + j + len / 2] * wi + im[i + j + len / 2] * wr;
                re[i + j + len / 2] = re[i + j] - xr, im[i + j + len / 2] = im[i + j] - xi;
                re[i + j] += xr, im[i + j] += xi;
            }
    }
}
/* the averaged power spectrum (Hann) of y[0..n) in frames of NFFT, into ps[NFFT / 2] */
static void spectrum(const double *y, uint32_t n, double *ps)
{
    static double re[NFFT], im[NFFT];
    uint32_t f, i;
    memset(ps, 0, sizeof(double) * NFFT / 2);
    for (f = 0; f + NFFT <= n; f += NFFT / 2) {
        for (i = 0; i < NFFT; i++) {
            re[i] = y[f + i] * (0.5 - 0.5 * cos(2 * M_PI * i / NFFT));
            im[i] = 0;
        }
        fft(re, im, NFFT);
        for (i = 0; i < NFFT / 2; i++)
            ps[i] += re[i] * re[i] + im[i] * im[i];
    }
}
/* the strongest peak between lo and hi Hz, interpolated */
static double peak_hz(const double *ps, double lo, double hi)
{
    uint32_t i, a = (uint32_t)(lo / FS * NFFT), b = (uint32_t)(hi / FS * NFFT), best = a;
    double l, c, r, d;
    for (i = a; i < b; i++)
        if (ps[i] > ps[best])
            best = i;
    l = log(ps[best - 1] + 1e-30), c = log(ps[best] + 1e-30), r = log(ps[best + 1] + 1e-30);
    d = 0.5 * (l - r) / (l - 2 * c + r);
    return (best + d) * FS / NFFT;
}
/* the RMS envelope (5 ms frames) and its strongest repetition between lo and hi per second */
#define EFR (FS / 200)
static double env_rate(const double *y, uint32_t n, double lo, double hi)
{
    static double e[4096];
    uint32_t m = n / EFR, i, k, best = 0;
    double mean = 0, bv = -1;
    if (m > 4096u)
        m = 4096u;
    for (i = 0; i < m; i++) {
        double s = 0;
        for (k = 0; k < EFR; k++)
            s += y[i * EFR + k] * y[i * EFR + k];
        e[i] = sqrt(s / EFR);
        mean += e[i] / m;
    }
    for (i = 0; i < m; i++)
        e[i] -= mean;
    static double ac[4096];
    uint32_t k0 = (uint32_t)(200 / hi), k1 = (uint32_t)(200 / lo);
    for (k = k0; k <= k1 && k < m / 2; k++) {
        double a = 0, b0 = 0, b1 = 0;
        for (i = 0; i + k < m; i++)
            a += e[i] * e[i + k], b0 += e[i] * e[i], b1 += e[i + k] * e[i + k];
        ac[k] = a / (sqrt(b0 * b1) + 1e-30);
        if (ac[k] > bv)
            bv = ac[k], best = k;
    }
    for (k = k0 + 1; k < best; k++)                     /* the shortest lag that repeats about as well: the period,
                                                         * not a multiple of it */
        if (ac[k] >= 0.85 * bv && ac[k] >= ac[k - 1] && ac[k] >= ac[k + 1])
            return 200.0 / k;
    return best ? 200.0 / best : 0;
}
static double rms(const double *y, uint32_t n, double *mean)
{
    double s = 0, m = 0;
    uint32_t i;
    for (i = 0; i < n; i++)
        m += y[i];
    m /= n;
    for (i = 0; i < n; i++)
        s += (y[i] - m) * (y[i] - m);
    if (mean)
        *mean = m;
    return sqrt(s / n);
}
/* the frequency of the strongest component of y around i0 (a 2048-point frame, zero-padded): a pitch track */
static double local_hz(const double *y, uint32_t i0, double lo, double hi)
{
    static double re[NFFT], im[NFFT], ps[NFFT / 2];
    uint32_t i;
    for (i = 0; i < NFFT; i++) {
        re[i] = i < 1024u ? y[i0 + i] * (0.5 - 0.5 * cos(2 * M_PI * i / 1024)) : 0;
        im[i] = 0;
    }
    fft(re, im, NFFT);
    for (i = 0; i < NFFT / 2; i++)
        ps[i] = re[i] * re[i] + im[i] * im[i];
    return peak_hz(ps, lo, hi);
}

#define NS (FS * 6)
static double Y[NS], PS[NFFT / 2];

/* ------------------------------------------------------------------ CMOS --- */
static void cmos(void)
{
    static const int16_t GATE[8] = {AN_CMOS, 64, 0, 64, 0, 0, 0, 0};   /* no sweep, RATE 0 (one long chirp) */
    double worst = 0, f, want, r, r2, early = 0, late = 0;
    uint32_t nt, k, nh = 0;
    char what[200];
    for (nt = 57; nt <= 93; nt += 12) {
        setup(-1, GATE);
        voice_render(nt, 0.3, Y, FS * 2);
        spectrum(Y, FS * 2, PS);
        want = 440.0 * pow(2, (nt + 12 - 69) / 12.0);
        f = peak_hz(PS, want * 0.8, want * 1.2);
        if (fabs(1200 * log2(f / want)) > fabs(worst))
            worst = 1200 * log2(f / want);
    }
    snprintf(what, sizeof what, "CMOS: the relaxation oscillator plays the key an octave up, A3 .. A6: worst %+.1f cents", worst);
    check(what, fabs(worst) <= 10);

    setup(preset_of("CMOS BIRD"), 0);
    voice_render(72, 0.2, Y, FS * 4);
    spectrum(Y, FS * 4, PS);
    f = peak_hz(PS, 300, 6000);
    r = env_rate(Y, FS * 4, 1.5, 12);
    for (k = 0; k + 2048 < FS * 4; k += 256) {         /* the hooks: in each chirp, its first 20 ms against its last */
        double e0 = rms(Y + k, 256, 0), e1 = rms(Y + k + 2205, 256, 0);
        if (e0 > 0.02 && k >= 256 && rms(Y + k - 256, 256, 0) < 0.3 * e0 && e1 > 0.02) {
            early += local_hz(Y, k, 300, 3000);
            late += local_hz(Y, k + 2205, 300, 3000);
            nh++;
            k += 4096;
        }
    }
    snprintf(what, sizeof what, "CMOS BIRD at C5: chirps at %.0f Hz (1.0 .. 1.3 kHz), %.2f a second (2.4 .. 4.5), hooks %.0f -> %.0f Hz 50 ms on (%u)",
             f, r, nh ? early / nh : 0, nh ? late / nh : 0, nh);
    check(what, f >= 1000 && f <= 1300 && r >= 2.4 && r <= 4.5 && nh >= 4 && early < late * 0.93);

    setup(preset_of("CMOS TRILL"), 0);
    voice_render(72, 0.2, Y, FS * 4);
    spectrum(Y, FS * 4, PS);
    f = peak_hz(PS, 300, 6000);
    r2 = env_rate(Y, FS * 4, 3, 16);
    snprintf(what, sizeof what, "CMOS TRILL at C5: %.2f chirps a second (8 .. 10) at %.0f Hz (1.0 .. 1.2 kHz)", r2, f);
    check(what, r2 >= 8 && r2 <= 10 && f >= 1000 && f <= 1200);
}

/* ----------------------------------------------------------------- PULSE --- */
static void pulse(void)
{
    double f, r, r14, r30, want;
    int16_t e[8];
    uint32_t i, pc = (uint32_t)preset_of("CRICKET");
    char what[200];
    setup((int32_t)pc, 0);
    voice_render(72, 0.1, Y, FS * 5);
    spectrum(Y, FS * 5, PS);
    f = peak_hz(PS, 200, 15000);
    r = env_rate(Y, FS * 5, 0.5, 6);
    snprintf(what, sizeof what, "CRICKET at C5: carrier %.0f Hz (4.3 .. 5.3 kHz), %.2f chirps a second (1 .. 3)", f, r);
    check(what, f >= 4300 && f <= 5300 && r >= 1 && r <= 3);
    setup(preset_of("CICADA"), 0);
    voice_render(72, 0.3, Y, FS * 3);
    spectrum(Y, FS * 3, PS);
    f = peak_hz(PS, 200, 15000);
    snprintf(what, sizeof what, "CICADA at C5: its abdomen's resonance %.0f Hz (3.8 .. 4.8 kHz)", f);
    check(what, f >= 3800 && f <= 4800);
    for (i = 0; i < 8u; i++)
        e[i] = ENG_ANIMAL.presets[pc].e[i];
    e[7] = 14;                                          /* 14 C */
    setup((int32_t)pc, e);
    voice_render(72, 0.1, Y, NS);
    r14 = env_rate(Y, NS, 0.3, 6);
    e[7] = 30;                                          /* 30 C */
    setup((int32_t)pc, e);
    voice_render(72, 0.1, Y, NS);
    r30 = env_rate(Y, NS, 0.3, 6);
    want = (double)an_temp(30) / an_temp(14);
    snprintf(what, sizeof what, "TEMP (Dolbear): the cricket chirps %.2f / s at 14 C, %.2f at 30 C (x%.2f, want x%.2f +-12 %%)", r14, r30,
             r30 / r14, want);
    check(what, r14 > 0 && fabs(r30 / r14 / want - 1) < 0.12);
}

/* ----------------------------------------------------------------- WHSTL --- */
static void whistle(void)
{
    int16_t e[8] = {AN_WHSTL, 100, 0, 64, 0, 60, 0, 22};   /* HOLD, no sweep, no chaos */
    double f[2], l[3];
    uint32_t k;
    static const int16_t PR[3] = {15, 60, 127};
    char what[200];
    for (k = 0; k < 2u; k++) {
        setup(-1, e);
        voice_render(69 + 12 * k, 0.3, Y, FS * 2);
        spectrum(Y, FS * 2, PS);
        f[k] = peak_hz(PS, 150, 3000);
    }
    snprintf(what, sizeof what, "WHSTL: the Hopf oscillator follows the key: %.1f / %.1f Hz at A4 / A5 (x%.3f; %+.0f cents)", f[0], f[1],
             f[1] / f[0], 1200 * log2(f[0] / 440));
    check(what, fabs(f[1] / f[0] - 2) < 0.02 && fabs(1200 * log2(f[0] / 440)) < 40);
    for (k = 0; k < 3u; k++) {
        e[1] = PR[k];
        setup(-1, e);
        voice_render(69, 0.4, Y, FS);
        l[k] = 20 * log10(rms(Y, FS, 0) + 1e-9);
    }
    snprintf(what, sizeof what, "WHSTL: born by bifurcation: PRESS 15 %.0f dB (below the threshold), 60 %.1f dB, 127 %.1f dB", l[0],
             l[1], l[2]);
    check(what, l[0] < -60 && l[1] > -30 && l[2] > l[1] + 1);
}

/* --------------------------------------------------------- every preset --- */
static void presets(void)
{
    static int32_t a[FS / 2], b[CTL], o[2 * CTL];
    uint32_t pi, i, k, m, same = 1, quiet = 0, ends = 1;
    double worst = 0, lo = 1;
    int32_t vmax = 0, omax = 0, worst_p = 0, lo_p = 0;
    char what[200];
    for (pi = 0; pi < ENG_ANIMAL.npresets; pi++) {
        double mean, r;
        setup((int32_t)pi, 0);
        voice_render(66, 0.05, Y, FS * 3);
        r = rms(Y, FS * 3, &mean);
        if (getenv("VERBOSE"))
            printf("animal:   %-12s RMS %6.1f dB  mean %+.5f\n", ENG_ANIMAL.presets[pi].name, 20 * log10(r + 1e-12), mean);
        if (fabs(mean) * 32768.0 / VOICE_FS > worst)
            worst = fabs(mean) * 32768.0 / VOICE_FS, worst_p = (int32_t)pi;
        if (r < lo)
            lo = r, lo_p = (int32_t)pi;
        quiet += r < 0.01;
        trk_note_off(&trk[0], 66);
        for (i = 0; i < 12u * FS / CTL; i++)            /* released: every voice free */
            track_render(&trk[0], b, CTL);
        for (k = 0; k < NVOICE; k++)
            ends &= !trk[0].v[k].active;
        /* a note from silence: the same samples twice */
        setup((int32_t)pi, 0);
        trk_note_on(&trk[0], 62, 100);
        for (i = 0; i < FS / 2 / CTL; i++) {
            track_render(&trk[0], b, CTL);
            for (k = 0; k < CTL; k++)
                a[i * CTL + k] = b[k];
        }
        trk_note_off(&trk[0], 62);
        for (i = 0; i < 12u * FS / CTL; i++)
            track_render(&trk[0], b, CTL);
        trk_note_on(&trk[0], 62, 100);
        for (i = 0; i < FS / 2 / CTL; i++) {
            track_render(&trk[0], b, CTL);
            for (k = 0; k < CTL; k++)
                same &= a[i * CTL + k] == b[k];
        }
    }
    snprintf(what, sizeof what, "every preset sounds: the quietest %s at %.1f dB (a voice's sum, 3 s with its gaps)",
             ENG_ANIMAL.presets[lo_p].name, 20 * log10(lo));
    check(what, !quiet);
    snprintf(what, sizeof what, "no DC: worst |mean| %.2f %% of a voice's full scale (%s)", worst * 100, ENG_ANIMAL.presets[worst_p].name);
    check(what, worst < 0.01);
    check("every preset's voices end after its release", ends);
    check("a note from silence renders the same samples again (every preset)", same);
    for (m = 0; m < AN_NMODE; m++)
        for (k = 0; k < 128u; k++) {                    /* the corners of the seven knobs */
            int16_t e[8];
            for (i = 1; i < 8u; i++)
                e[i] = (k >> (i - 1)) & 1u ? param_desc_of(eng_an(), P_E0 + i)->max : param_desc_of(eng_an(), P_E0 + i)->min;
            e[0] = (int16_t)m;
            setup(-1, e);
            trk[0].p[P_LEVEL] = 127;
            for (i = 0; i < 8u; i++)
                trk_note_on(&trk[0], 24 + 12 * i, 127);
            for (i = 0; i < FS / 2 / CTL; i++) {
                uint32_t j;
                track_render(&trk[0], b, CTL);
                for (j = 0; j < CTL; j++)
                    vmax = abs(b[j]) > vmax ? abs(b[j]) : vmax;
            }
            setup(-1, e);
            trk[0].p[P_LEVEL] = 127;
            for (i = 0; i < 8u; i++)
                trk_note_on(&trk[0], 24 + 12 * i, 127);
            for (i = 0; i < FS / 2 / CTL; i++) {
                uint32_t j;
                mix_block(o, CTL);
                for (j = 0; j < 2u * CTL; j++)
                    omax = abs(o[j]) > omax ? abs(o[j]) : omax;
            }
        }
    snprintf(what, sizeof what, "no clipping at the corners (every mode): voice sum peak %d (8 voices, limit %d), mix peak %d", vmax,
             8 * 2 * VOICE_FS, omax);
    check(what, vmax < 8 * 2 * VOICE_FS && omax < 32767);
}

/* ---------------------------------------------------------------- cost --- */
static double now_s(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + ts.tv_nsec * 1e-9;
}
static void cost(void)
{
    static int32_t o[2 * CTL];
    uint32_t m, i, k, nb = FS * 2u / CTL;
    printf("animal: cost, host ns per sample and voice (8 notes held, the idle mix taken off; information):");
    for (m = 0; m <= AN_NMODE; m++) {
        double t0, t[2];
        for (k = 0; k < 2u; k++) {
            int16_t e[8] = {(int16_t)(m < AN_NMODE ? m : 0), 100, 60, 64, 20, 90, 1, 22};
            setup(-1, e);
            for (i = 0; i < (k ? 8u : 0u); i++)
                trk_note_on(&trk[0], 48 + 3 * i, 100);
            for (i = 0; i < 64u; i++)
                mix_block(o, CTL);
            t0 = now_s();
            for (i = 0; i < nb; i++)
                mix_block(o, CTL);
            t[k] = (now_s() - t0) / (nb * CTL) * 1e9;
        }
        if (m < AN_NMODE)
            printf(" %s %.1f", N_ANIMAL_MODE[m], (t[1] - t[0]) / 8);
    }
    printf("\n");
}

/* --------------------------------------------------------------- demos --- */
static uint32_t demo_note(const preset_t *pr)
{
    static const struct { const char *n; uint8_t note; } H[] = {{"BULLFROG", 60}, {"OWL", 60}, {"WOLF", 67}, {"DOG BARK", 64},
                                                                  {"ROAR", 48}, {"PURR", 36}};
    uint32_t k;
    for (k = 0; k < NELEM(H); k++)
        if (str_eq(pr->name, H[k].n))
            return H[k].note;
    return 72;
}
static void demo(const char *dir, uint32_t pi)
{
    static int32_t o[2 * CTL];
    static const int8_t PH[4][2] = {{0, 22}, {5, 14}, {0, 18}, {-5, 26}};   /* the phrase: semitones, length (1/10 s) */
    const preset_t *pr = &ENG_ANIMAL.presets[pi];
    uint32_t home = demo_note(pr), s, i, frames = 0;
    char path[512], nm[32];
    FILE *w;
    for (i = 0; pr->name[i] && i < 31u; i++)
        nm[i] = pr->name[i] == ' ' ? '_' : pr->name[i];
    nm[i] = 0;
    snprintf(path, sizeof path, "%s/%02u_%s.wav", dir, pi, nm);
    if (!(w = fopen(path, "wb")))
        return;
    wav_hdr(w, 0);
    memset(trk, 0, sizeof trk);
    host_tracks_init();
    host_preset(&trk[0], eng_an(), pi);
    for (s = 0; s < 4u; s++) {                          /* each note a call, a breath between them */
        uint32_t note = home + PH[s][0], len = PH[s][1] * (FS / 10u);
        trk_note_on(&trk[0], note, 100);
        for (i = 0; i < len + FS / 4; i += CTL) {
            uint32_t j;
            if (i >= len && i < len + CTL)
                trk_note_off(&trk[0], note);
            mix_block(o, CTL);
            for (j = 0; j < CTL; j++)
                wav_put(w, o[2 * j], o[2 * j + 1]);
            frames += CTL;
        }
    }
    for (i = 0; i < FS; i += CTL) {                     /* the tail */
        uint32_t j;
        mix_block(o, CTL);
        for (j = 0; j < CTL; j++)
            wav_put(w, o[2 * j], o[2 * j + 1]);
        frames += CTL;
    }
    fseek(w, 0, SEEK_SET);
    wav_hdr(w, frames);
    fclose(w);
}

int main(int argc, char **argv)
{
    uint32_t pi;
    if (getenv("DEMO_ONLY") && argc > 1) {
        for (pi = 0; pi < ENG_ANIMAL.npresets; pi++)
            demo(argv[1], pi);
        return 0;
    }
    cmos();
    pulse();
    whistle();
    presets();
    cost();
    if (argc > 1) {
        for (pi = 0; pi < ENG_ANIMAL.npresets; pi++)
            demo(argv[1], pi);
        printf("animal: demos in %s (%u presets, a phrase each)\n", argv[1], (unsigned)ENG_ANIMAL.npresets);
    }
    printf("animal: %s\n", bad ? "FAILED" : "all checks ok");
    return bad != 0;
}
