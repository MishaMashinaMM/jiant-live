/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT 0.8.0) the tutorial film's footage (tools/video/tutorial.html, in Spanish): a dark, evolving, spacious but
 * percussive piece in F minor at 116 BPM played by the real engines (TECHNO RAW drums, a REESE bass, a DARK PAD, a
 * PD LEAD with its filter envelope and a dotted delay, an FM PLUCK arpeggio, METAL hits in the reverb), arranged by the
 * voice-over's chapters (tools/video/tutorial_voice.py: plan.txt, "seconds chapter cue" a line), while the screen goes
 * to what each line talks about. Writes audio.wav (the music alone: the voice is mixed in after), screen.rgb,
 * frames.txt (HOME shown, each track's peak) and scenes.txt (its header: what film_prep.py reads).
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/tut/capture_tutorial tools/video/capture_tutorial.c -lm
 *   build/tut/capture_tutorial build/tut */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_VERSION "v0.8.6"
#define PF_MS 900u
#define UI_NO_DRIFT 0
#include "ui_test.c"

#define FPS 30u
#define BPM 116u
#define BEAT_S (60.0 / BPM)
#define SPLASH_S 4.0
enum { T_DRUM, T_BASS, T_PAD, T_LEAD, T_ARP, T_METAL };
static FILE *wav, *rgb, *cap, *frm;
static uint32_t nsamp;
static double SECS;

/* ---- the plan: the voice-over's lines (their start, chapter, cue) */
#define NPLAN 128
static struct { double t; char ch[16]; int cue; } plan[NPLAN];
static uint32_t nplan;
static void plan_read(const char *dir)
{
    char p[512], id[16];
    double t;
    int cue;
    snprintf(p, sizeof p, "%s/plan.txt", dir);
    FILE *f = fopen(p, "r");
    if (!f) { fprintf(stderr, "no %s (tools/video/tutorial_voice.py first)\n", p); exit(1); }
    while (fscanf(f, "%lf %15s %d", &t, id, &cue) == 3) {
        if (!strcmp(id, "end")) { SECS = t; continue; }
        if (nplan < NPLAN) { plan[nplan].t = t; strcpy(plan[nplan].ch, id); plan[nplan].cue = cue; nplan++; }
    }
    fclose(f);
}
static const char *chapter_at(double t)                /* the chapter whose first line has begun (the intro before) */
{
    const char *c = "intro";
    uint32_t i;
    for (i = 0; i < nplan && plan[i].t <= t + 1.2; i++) c = plan[i].ch;   /* (the music turns a little ahead) */
    return c;
}
static int is(const char *a, const char *b) { return !strcmp(a, b); }

/* ---- the sounds and the notes */
static uint32_t eng_of(const char *name)
{
    uint32_t e;
    for (e = 0; e < NENGINES; e++) if (str_eq(ENGINES[e]->name, name)) return e;
    return 0;
}
static uint32_t pre_of(uint32_t e, const char *name)
{
    uint32_t i;
    for (i = 0; i < ENGINES[e]->npresets; i++) if (str_eq(ENGINES[e]->presets[i].name, name)) return i;
    return 0;
}
static void sound(uint32_t k, const char *eng, const char *pre)
{
    uint32_t e = eng_of(eng);
    set_engine_of(&trk[k], e);
    apply_preset_to(&trk[k], pre_of(e, pre));
    trk[k].engine = trk[k].eng_req;
}
static void put(uint32_t k, uint32_t i, uint32_t n, const uint8_t *notes, uint32_t time, uint32_t flags)
{
    step_t *s = &trk[k].step[i];
    uint32_t j;
    s->n = (uint8_t)n;
    for (j = 0; j < 4u; j++) s->note[j] = j < n ? notes[j] : 0;
    s->time = (uint8_t)time;
    s->flags = (uint8_t)flags;
    s->vel = n ? 100 : 0;
    s->comp = s->carg = 0;
    s->probability = 0;
}
static void line(uint32_t k, uint32_t at, uint8_t note, uint32_t len, uint32_t flags)   /* a note held len steps */
{
    uint32_t i;
    for (i = 0; i < len && at + i < 64u; i++) put(k, at + i, i ? 0u : 1u, &note, i ? ST_TIE : ST_NOTE, i ? 0u : flags);
}
static void rests(uint32_t k) { uint32_t i; for (i = 0; i < 64u; i++) put(k, i, 0, 0, ST_REST, 0); }
static void drums(const char *const rows[8])           /* BD SD CP CH OH TM RS CB: 'x' a hit, 'X' accented */
{
    static const uint8_t NOTE[8] = {36, 38, 39, 42, 46, 45, 37, 56};
    uint32_t i, l;
    for (i = 0; i < 16u; i++) {
        uint8_t n[4]; uint32_t c = 0, acc = 0;
        for (l = 0; l < 8u; l++)
            if (rows[l] && rows[l][i] != '.' && c < 4u) { n[c++] = NOTE[l]; acc |= rows[l][i] == 'X'; }
        put(T_DRUM, i, c, n, c ? ST_NOTE : ST_REST, acc ? SF_ACCENT : 0u);
        step_to_grid(&trk[T_DRUM].step[i]);
    }
}
static const char *const D_SPARSE[8] = {"X.......x.......", 0, 0, "..x...x...x...x.", 0, 0, "..........x.....", 0};
static const char *const D_GROOVE[8] = {"X...x...X...x...", 0, "....x.......x...", "..x...x...x...x.", "..............x.", 0,
                                        "...x......x..x..", 0};
static const char *const D_DRIVE[8] = {"X...x...X...x.x.", 0, "....X.......X...", "x.xxx.xxx.xxx.xx", "..x.......x.....",
                                       ".............x.x", "...x..x....x..x.", "......x.......x."};
/* F minor: A  Fm Dbmaj7 Abmaj7 Eb7, B  Fm7 Bbm7 Dbmaj7 C7 (the dominant's E: the tension) */
static const uint8_t ROOT[2][4] = {{41, 37, 44, 39}, {41, 46, 37, 36}};
static const uint8_t CHORD[2][4][4] = {{{65, 68, 72, 75}, {61, 65, 68, 72}, {56, 60, 63, 67}, {63, 67, 70, 73}},
                                       {{65, 68, 72, 75}, {58, 61, 65, 68}, {61, 65, 68, 72}, {60, 64, 67, 70}}};
static void pad(int pr)
{
    uint32_t b, i;
    for (b = 0; b < 4u; b++)
        for (i = 0; i < 16u; i++)
            put(T_PAD, b * 16u + i, i ? 0u : 3u, CHORD[pr][b], i ? (i < 15u ? ST_TIE : ST_REST) : ST_NOTE, 0);
}
static void bass(int pr, int big)                       /* syncopated, or long and deep (the drops) */
{
    static const uint8_t NORM[][3] = {{0, 0, 3}, {3, 12, 1}, {6, 0, 1}, {8, 0, 2}, {10, 12, 1}, {12, 0, 1}, {14, 7, 1}};
    static const uint8_t BIG[][3] = {{0, 0, 6}, {8, 0, 3}, {11, 0, 1}, {12, 12, 2}, {14, 10, 2}};
    uint32_t b, j;
    rests(T_BASS);
    for (b = 0; b < 4u; b++) {
        if (big) for (j = 0; j < sizeof BIG / sizeof BIG[0]; j++) line(T_BASS, b * 16u + BIG[j][0], (uint8_t)(ROOT[pr][b] + BIG[j][1]), BIG[j][2], j ? 0u : SF_ACCENT);
        else for (j = 0; j < sizeof NORM / sizeof NORM[0]; j++) line(T_BASS, b * 16u + NORM[j][0], (uint8_t)(ROOT[pr][b] + NORM[j][1]), NORM[j][2], j ? 0u : SF_ACCENT);
    }
}
static void lead(int alt)                               /* a phrase with room: notes with their tails, rests between */
{
    static const uint8_t A[][2] = {{72, 3}, {0, 1}, {75, 2}, {77, 2}, {75, 3}, {0, 1}, {72, 4},
                                   {73, 6}, {72, 2}, {68, 6}, {0, 2},
                                   {67, 3}, {0, 1}, {68, 2}, {72, 2}, {75, 6}, {0, 2},
                                   {70, 4}, {73, 4}, {72, 6}, {0, 2}};
    static const uint8_t B[][2] = {{77, 6}, {0, 2}, {80, 2}, {79, 2}, {77, 4},
                                   {77, 3}, {0, 1}, {73, 4}, {72, 6}, {0, 2},
                                   {73, 2}, {72, 2}, {68, 4}, {65, 6}, {0, 2},
                                   {67, 4}, {70, 4}, {72, 8}};
    const uint8_t (*L)[2] = alt ? B : A;
    uint32_t n = alt ? sizeof B / sizeof B[0] : sizeof A / sizeof A[0], j, p = 0;
    rests(T_LEAD);
    for (j = 0; j < n; j++) { if (L[j][0]) line(T_LEAD, p, L[j][0], L[j][1], 0); p += L[j][1]; }
}
static void arp(int pr)                                 /* 16ths up two octaves, every fourth left out: air */
{
    uint32_t b, i;
    for (b = 0; b < 4u; b++)
        for (i = 0; i < 16u; i++) {
            uint8_t n = (uint8_t)(CHORD[pr][b][i % 4u] + 12u * ((i / 4u) % 2u) - 12u);
            int on = i % 4u != 3u;
            put(T_ARP, b * 16u + i, on ? 1u : 0u, &n, on ? ST_NOTE : ST_REST, i % 8u == 0u ? SF_ACCENT : 0u);
        }
}
static void metal(void)
{
    uint32_t i;
    rests(T_METAL);
    for (i = 0; i < 64u; i++) {
        uint8_t n = (uint8_t)(i % 32u == 14u ? 70 : 63);
        if (i % 16u == 6u || i % 32u == 14u || i == 59u) put(T_METAL, i, 1, &n, ST_NOTE, i % 32u == 14u ? SF_ACCENT : 0u);
    }
}

static void mute(uint32_t k, int m) { trk[k].p[P_MUTE] = (int16_t)m; }
static void pad_sound(void)                             /* the space: slow, wide, a cathedral (again after the browsing) */
{
    track_t *t = &trk[T_PAD];
    int16_t m = t->p[P_MUTE];
    sound(T_PAD, "ANALOG", "DARK PAD");
    t->p[P_ROOT] = 5; t->p[P_SCALE] = 0; t->p[P_SLEN] = 64; t->p[P_MUTE] = m;
    t->p[P_LEVEL] = 76; t->p[P_ATK] = 70; t->p[P_REL] = 100; t->p[P_SUS] = 100;
    t->p[P_FCUT] = 64; t->p[P_FRES] = 30; t->p[P_LRATE] = 10; t->p[P_LD_FLT] = 22;
    t->p[P_CHOR] = 80; t->p[P_REV] = 118; t->p[P_DLY] = 10; t->p[P_APDEP] = 30; t->p[P_APRATE] = 12;
}
static void song_build(void)
{
    uint32_t k;
    ui_power_on();
    settings.palette = UI_JIANT_INDEX; palette_set(UI_JIANT_INDEX);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    song.g[G_BPM] = BPM;
    song.master_q12 = 4096;
    sound(T_DRUM, "DRUM", "TECHNO RAW");
    sound(T_BASS, "ANALOG", "REESE");
    sound(T_PAD, "ANALOG", "DARK PAD");
    sound(T_LEAD, "PHASE", "PD LEAD");
    sound(T_ARP, "FM6", "FM PLUCK");
    sound(T_METAL, "NOISE", "METAL");
    for (k = 0; k < NTRK; k++) {
        track_defaults_steps(&trk[k]);
        trk[k].p[P_ROOT] = 5; trk[k].p[P_SCALE] = 0;     /* F, chromatic: the chords as written */
        trk[k].p[P_SLEN] = 64;
        trk[k].p[P_MUTE] = 1;
    }
    trk[T_DRUM].p[P_SLEN] = 16;
    drums(D_SPARSE); bass(0, 0); pad(0); lead(0); arp(0); metal();
    {   track_t *t = &trk[T_BASS];                       /* big: mono, the envelope biting a low filter, a slow sweep */
        t->p[P_VOICE] = V_MONO; t->p[P_LEVEL] = 104; t->p[P_FCUT] = 58; t->p[P_FRES] = 34;
        t->p[P_ATK] = 0; t->p[P_DEC] = 46; t->p[P_SUS] = 70; t->p[P_REL] = 26; t->p[P_ED_FLT] = 30;
        t->p[P_LRATE] = 6; t->p[P_LD_FLT] = 10; t->p[P_DIST] = 22; t->p[P_DTYPE] = 5; t->p[P_CHOR] = 20; t->p[P_GLIDE] = 10; }
    pad_sound();
    {   track_t *t = &trk[T_LEAD];                       /* the lead: its envelope on the filter, a dotted delay */
        t->p[P_LEVEL] = 88; t->p[P_ATK] = 4; t->p[P_DEC] = 54; t->p[P_SUS] = 46; t->p[P_REL] = 70;
        t->p[P_FTYPE] = 0; t->p[P_FCUT] = 70; t->p[P_FRES] = 44; t->p[P_ED_FLT] = 40; t->p[P_GLIDE] = 14;
        t->p[P_LRATE] = 30; t->p[P_LD_PIT] = 2; t->p[P_DLY] = 70; t->p[P_REV] = 92; t->p[P_PAN] = 10; }
    {   track_t *t = &trk[T_ARP];                        /* the pluck: short, bright, to the left, into the delay */
        t->p[P_LEVEL] = 92; t->p[P_SGATE] = 30; t->p[P_REL] = 30; t->p[P_DLY] = 50; t->p[P_REV] = 60; t->p[P_PAN] = -22;
        t->p[P_FTYPE] = 0; t->p[P_FCUT] = 84; t->p[P_LRATE] = 4; t->p[P_LD_FLT] = 26; }
    {   track_t *t = &trk[T_METAL];                      /* the metal: hits far away in the reverb */
        t->p[P_LEVEL] = 127; t->p[P_SGATE] = 70; t->p[P_REV] = 110; t->p[P_DLY] = 40; t->p[P_PAN] = 24; }
    trk[T_DRUM].p[P_LEVEL] = 106; trk[T_DRUM].p[P_REV] = 24;
    dx_snd[2] = (uint8_t)(3u << 6 | 48u);                /* the clap and the rim into the reverb, the cowbell the delay */
    dx_snd[6] = (uint8_t)(3u << 6 | 40u);
    dx_snd[7] = (uint8_t)(2u << 6 | 40u);
    song.g[G_RTYPE] = 2; song.g[G_RSIZE] = 116; song.g[G_DTIME] = 3; song.g[G_DFDBK] = 50;   /* SHIMMER, a dotted delay */
    song.g[G_CDEPTH] = 80; song.g[G_CRATE] = 20;
    song.g[G_CLIP] = 20; song.g[G_PUNCH] = 50; song.g[G_DUCK] = 30;
    for (k = 0; k < 4u; k++) project_save(k);
    ui.msg_t = 0; ui.msg[0] = ui.msg2[0] = 0;
    transport_req = 1;
}

/* ---- the arrangement: what plays in each chapter, set on the bar */
static const char *arr_ch = "";
static int cue_now;                                    /* (the lab film's visuals: the arrangement turns with each line) */
static int arr_pr = -1, arr_big = -1, arr_alt = -1;
static const char *const *arr_dr;
static void arrange(uint32_t bar, double t)
{
    const char *c = chapter_at(t);
    int on[6] = {0}, pr = 0, big = 0, alt = 0;
    const char *const *dr = D_GROOVE;
    on[T_PAD] = 1;
    if (is(c, "intro")) { on[T_METAL] = bar >= 4u; dr = D_SPARSE; }
    else if (is(c, "install")) { on[T_DRUM] = on[T_METAL] = 1; dr = D_SPARSE; }
    else if (is(c, "panel")) { on[T_DRUM] = on[T_BASS] = on[T_METAL] = 1; dr = D_SPARSE; }
    else if (is(c, "screen")) { on[T_DRUM] = on[T_BASS] = on[T_ARP] = 1; }
    else if (is(c, "home")) { on[T_DRUM] = on[T_BASS] = on[T_ARP] = on[T_METAL] = 1; pr = 1; }
    else if (is(c, "layers")) { on[T_DRUM] = on[T_BASS] = on[T_ARP] = 1; }
    else if (is(c, "engines")) { on[T_DRUM] = on[T_BASS] = on[T_ARP] = on[T_LEAD] = 1; }
    else if (is(c, "signal")) { on[T_DRUM] = on[T_BASS] = on[T_LEAD] = on[T_METAL] = 1; dr = D_DRIVE; big = 1; pr = 1; alt = 1; }
    else if (is(c, "drumx")) { on[T_DRUM] = on[T_BASS] = on[T_ARP] = 1; dr = D_DRIVE; big = 1; }
    else if (is(c, "seq")) { on[T_DRUM] = on[T_BASS] = on[T_ARP] = on[T_LEAD] = on[T_METAL] = 1; }
    else if (is(c, "mod")) { on[T_LEAD] = on[T_ARP] = on[T_METAL] = 1; pr = 1; alt = 1; }
    else if (is(c, "fx")) { on[T_DRUM] = on[T_BASS] = on[T_ARP] = on[T_LEAD] = on[T_METAL] = 1; dr = D_DRIVE; big = 1; }
    else if (is(c, "gen")) { on[T_DRUM] = on[T_BASS] = on[T_ARP] = 1; }
    else if (is(c, "arrange")) { on[T_DRUM] = on[T_BASS] = on[T_ARP] = on[T_LEAD] = 1; dr = D_DRIVE; pr = 1; alt = 1; }
    else if (is(c, "usb")) { on[T_DRUM] = on[T_BASS] = on[T_LEAD] = on[T_METAL] = 1; }
    else if (is(c, "web")) { on[T_DRUM] = on[T_BASS] = on[T_METAL] = 1; dr = D_SPARSE; }
    else if (is(c, "outro")) { on[T_LEAD] = on[T_METAL] = 1; pr = 1; alt = 1; }
    /* (the lab film, news_script_en.py) */
    else if (is(c, "stems")) { on[T_DRUM] = on[T_BASS] = on[T_ARP] = on[T_METAL] = 1; }
    else if (is(c, "editor")) { on[T_DRUM] = on[T_BASS] = on[T_ARP] = on[T_LEAD] = 1; pr = 1; }
    else if (is(c, "micin")) { on[T_DRUM] = on[T_BASS] = on[T_LEAD] = on[T_METAL] = 1; dr = D_DRIVE; big = 1; pr = 1; alt = 1; }
    else if (is(c, "app")) { on[T_DRUM] = on[T_BASS] = on[T_ARP] = on[T_LEAD] = on[T_METAL] = 1; dr = D_DRIVE; big = 1; }
    else if (is(c, "next")) { on[T_DRUM] = on[T_BASS] = on[T_ARP] = on[T_LEAD] = 1; dr = D_GROOVE; big = cue_now >= 3; }
    else if (is(c, "visuals")) { on[T_DRUM] = on[T_BASS] = on[T_ARP] = on[T_LEAD] = on[T_METAL] = 1; dr = cue_now % 2 ? D_DRIVE : D_GROOVE; big = cue_now >= 3; pr = cue_now % 2; alt = cue_now >= 4; }
    if (!is(c, arr_ch) || pr != arr_pr || dr != arr_dr || big != arr_big || alt != arr_alt) {                /* (rewritten on a chapter's bar: the 4-bar phrase restarts) */
        if (pr != arr_pr) { pad(pr); arp(pr); }
        if (pr != arr_pr || big != arr_big) bass(pr, big);
        if (alt != arr_alt) lead(alt);
        if (dr != arr_dr) drums(dr);
        arr_ch = c; arr_pr = pr; arr_big = big; arr_alt = alt; arr_dr = dr;
    }
    {   uint32_t k;
        for (k = 0; k < NTRK; k++) if (!(ui.menu && k == T_DRUM)) mute(k, !on[k]);
    }
    /* the slow movement: the pad's filter breathing over minutes, the bass opening in the drops */
    trk[T_PAD].p[P_FCUT] = (int16_t)(60 + 16 * sin(t / 37.0));
    trk[T_BASS].p[P_FCUT] = (int16_t)((big ? 66 : 56) + 8 * sin(t / 23.0));
    trk[T_LEAD].p[P_ED_FLT] = (int16_t)(36 + 14 * sin(t / 29.0));
}

/* ---- the screen: each line's action */
static void page(uint32_t sel, const char *title) { song.sel = (uint8_t)sel; go_title(title); }
static void home(uint32_t sel) { song.sel = (uint8_t)sel; go_home(); }
static void hold_btn(uint32_t label, int on)
{
    if (on) { fm1_in.buttons |= 1u << panel.btn[label]; host_pressed |= 1u << panel.btn[label]; }
    else fm1_in.buttons &= ~(1u << panel.btn[label]);
}
static void enc_turn(uint32_t role, int32_t s) { host_enc[panel.enc[role]] += s * panel.dir[role]; }
static int held = -1;                                   /* the button held for a layer's map, let go on the next line */
static void hold_layer(uint32_t b) { hold_btn(b, 1); held = (int)b; }
static void let_go(void)                                /* (and no layer left latched: FX LOCKED, the macros' LATCH) */
{
    if (held >= 0) hold_btn((uint32_t)held, 0);
    if (held >= 0 || fx_lock || perf_latch_on || ui.layer) { fx_lock = 0; perf_latch_on = 0; ui.layer = 0; ui.ly = 0; ui.force = 1; }
    held = -1;
}
static uint32_t pad_pre;
static void menu_open(uint32_t row)
{
    if (!ui.menu) hold(B_HOME);
    ui.menu_sel = (uint8_t)row;
    ui.force = 1;
}
static void tut_menu_close(void) { menu_put(MI_SERIAL, 0u); ui.menu = 0; ui.force = 1; }
static void act(const char *c, int cue)
{
    let_go();
    perf_press(PF_R16, 0);
    if (ui.menu && !is(c, "usb") && !is(c, "stems") && !is(c, "micin")) tut_menu_close();
    if (ui.confirm && !is(c, "gen")) { ui.confirm = 0; ui.force = 1; }
    if (is(c, "intro")) home(cue % 2 ? T_LEAD : T_PAD);
    else if (is(c, "install")) home(T_DRUM);
    else if (is(c, "panel")) {
        if (cue == 0) home(T_PAD);
        if (cue == 2) page(T_PAD, "FILTER");
        if (cue == 3) page(T_LEAD, "ENV");
    } else if (is(c, "screen")) {
        if (cue <= 2) page(T_BASS, "FILTER");
        if (cue == 3) page(T_BASS, "MIXER");
    } else if (is(c, "home")) home(T_BASS);
    else if (is(c, "layers")) {
        if (cue == 0) page(T_ARP, "EDIT 1");
        if (cue == 1) { home(T_ARP); hold_layer(B_GLO); }
        if (cue == 2) hold_layer(B_FX);
    } else if (is(c, "engines")) {
        static const uint32_t K[5] = {T_BASS, T_ARP, T_LEAD, T_METAL, T_PAD};
        if (cue == 4) { pad_pre = 1; page(T_PAD, "PRESETS"); }
        else page(K[cue], cue == 1 ? "OPERATOR" : "EDIT 1");
    } else if (is(c, "signal")) {
        static const char *const P[5] = {"EDIT 1", "FILTER", "DIST", "FX", "MASTER"};
        page(T_LEAD, P[cue]);
    } else if (is(c, "drumx")) {
        if (cue == 0) page(T_DRUM, "EDIT 1");
        if (cue == 2) { page(T_DRUM, "SOUND"); dx_ui_lane = 1; }
    } else if (is(c, "seq")) {
        static const char *const P[5] = {"PATTERN", "STEP", "STEP", "COMP", "PLAYHEADS"};
        page(cue == 2 ? T_DRUM : cue == 0 ? T_ARP : T_LEAD, P[cue]);
        if (cue == 3) { trk[T_LEAD].step[0].comp = CP_WALK; trk[T_LEAD].step[0].carg = CP_WARG(0, 0, 1); ui.cursor = 0; ui.bank = 0; }
        if (cue == 4) { trk[T_LEAD].step[0].comp = 0; trk[T_LEAD].step[0].carg = 0; }
    } else if (is(c, "mod")) {
        if (cue == 0) page(T_PAD, "LFO");
        if (cue == 1) {                                 /* two routes: the LFO on the cutoff, the noise on the pan */
            trk[T_PAD].p[P_M1SRC] = MS_LFO; trk[T_PAD].p[P_M1DST] = MD_CUT; trk[T_PAD].p[P_M1AMT] = 30;
            trk[T_PAD].p[P_M1SRC + 3] = MS_NOISE; trk[T_PAD].p[P_M1DST + 3] = MD_PAN; trk[T_PAD].p[P_M1AMT + 3] = 40;
            page(T_PAD, "MOD");
        }
        if (cue == 2) { home(T_LEAD); hold_layer(B_LFO); }
    } else if (is(c, "fx")) {
        if (cue == 0) page(T_LEAD, "DLY");
        if (cue == 1) page(T_LEAD, "SLICER");
        if (cue == 2) { home(T_DRUM); hold_layer(B_FX); }
        if (cue == 3) page(T_DRUM, "PUNCH");
    } else if (is(c, "gen")) {
        if (cue == 0) { home(T_LEAD); confirm_open(CF_DICE_ALL, song.sel); gen_open(); }
        if (cue == 1) { ui.confirm = 0; page(T_ARP, "STEP"); }
        if (cue == 2) { home(T_ARP); hold_layer(B_GLO); }
    } else if (is(c, "arrange")) {
        if (cue == 0) page(T_LEAD, "PROJECT");
        if (cue == 1) {
            chain_config.count = 3;
            chain_config.row[0] = (chain_row_t){0, 8}; chain_config.row[1] = (chain_row_t){1, 8}; chain_config.row[2] = (chain_row_t){0, 4};
            page(T_LEAD, "ARRANGE"); ui.song_row = 1;
        }
        if (cue == 2) { home(T_LEAD); hold_layer(B_SAVE); }
    } else if (is(c, "usb")) {
        if (cue == 0) { menu_open(MI_SERIAL); menu_put(MI_SERIAL, 2u); }
        if (cue == 1) { menu_put(MI_SERIAL, 3u); ui.force = 1; }
        if (cue == 2) { menu_put(MI_SERIAL, 0u); ui.menu_sel = MI_MIDIOUT; ui.force = 1; }
    } else if (is(c, "stems")) {
        if (cue == 0) { menu_open(MI_SERIAL); menu_put(MI_SERIAL, 2u); ui.force = 1; }
        else { tut_menu_close(); home(T_DRUM); }
    } else if (is(c, "editor")) {
        static const char *const P[3] = {"EDIT 1", "OPERATOR", "PRESETS"};
        page(cue == 1 ? T_ARP : T_LEAD, P[cue % 3]);
    } else if (is(c, "micin")) {
        if (cue == 0 || cue == 2) { menu_open(MI_SERIAL); menu_put(MI_SERIAL, 4u); ui.force = 1; }
        else if (cue == 1) { tut_menu_close(); page(T_METAL, "FX"); }
        else { tut_menu_close(); home(T_METAL); }
    } else if (is(c, "app")) home(cue % 2 ? T_LEAD : T_BASS);
    else if (is(c, "visuals") || is(c, "next")) home(T_LEAD);
    else if (is(c, "web")) home(T_BASS);
    else if (is(c, "outro")) home(T_LEAD);
}
/* within a line: the knob being turned, the keys being played */
static void during(const char *c, int cue, double dt, uint32_t f)
{
    if (is(c, "panel") && cue == 1 && f % 20u == 0u && dt < 6.0) enc_turn(EN_ALGO, 1);
    if (is(c, "panel") && cue == 2 && dt > 1.0 && dt < 5.0 && f % 3u == 0u) enc_turn(EN_K2, dt < 3.0 ? 1 : -1);
    if (is(c, "screen") && cue == 1 && dt < 4.0 && f % 4u == 0u) enc_turn(EN_K2, dt < 2.0 ? 1 : -1);
    if (is(c, "drumx") && cue == 1 && f % 3u == 0u && dt < 8.0) enc_turn(EN_K1, dt < 4.0 ? 2 : -2);
    if (is(c, "engines") && cue == 4 && f % 45u == 0u && dt > 1.0 && dt < 6.0) enc_turn(EN_PRESET, 1);
    if (is(c, "engines") && cue == 4 && dt > 9.0 && pad_pre) { pad_sound(); pad_pre = 0; }   /* (the pad as it was) */
    if (is(c, "fx") && cue == 2 && dt > 3.0 && dt < 7.0) perf_press(PF_R16, fmod(dt, 1.0) < 0.5);
    if (is(c, "fx") && cue == 2 && dt >= 7.0) perf_press(PF_R16, 0);
    if (is(c, "usb") && cue == 1 && dt > 4.5 && dt < 4.6) { menu_put(MI_SERIAL, 4u); ui.force = 1; }
    if (is(c, "home") && cue == 2 && dt > 2.0 && dt < 5.0) mute(T_METAL, 1);
}

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
int main(int argc, char **argv)
{
    char p[512];
    const char *dir = argc > 1 ? argv[1] : "build/tut";
    uint32_t f, total, nwav, k, bar_done = 0xFFFFFFFFu, pi = 0;
    const char *cur = "intro";
    int cue = -1;
    double cue_t = 0;
    plan_read(dir);
    total = (uint32_t)(FPS * SECS); nwav = (uint32_t)(FS * SECS);
    snprintf(p, sizeof p, "%s/audio.wav", dir); wav = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/screen.rgb", dir); rgb = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/scenes.txt", dir); cap = fopen(p, "w");
    snprintf(p, sizeof p, "%s/frames.txt", dir); frm = fopen(p, "w");
    if (!wav || !rgb || !cap || !frm) { fprintf(stderr, "cannot write in %s\n", dir); return 1; }
    wav_hdr(wav, nwav);
    fprintf(cap, "# %u fps, %u BPM, %u bars, splash %.4f s, end %.4f s, beat %.6f s\n", FPS, BPM, (uint32_t)(SECS / BEAT_S / 4.0),
            SPLASH_S, SECS, BEAT_S);
    song_build();
    for (f = 0; f < total; f++) {
        double t = (double)f / FPS;
        uint32_t want = (uint32_t)((uint64_t)(f + 1u) * FS / FPS), bar = (uint32_t)(t / (4.0 * BEAT_S));
        if (bar != bar_done) { bar_done = bar; arrange(bar, t); }
        while (pi < nplan && plan[pi].t <= t) {          /* a line begins: its action */
            cur = plan[pi].ch; cue = plan[pi].cue; cue_t = plan[pi].t; cue_now = cue;
            if (t >= SPLASH_S) act(cur, cue);
            pi++;
        }
        if (t >= SPLASH_S && cue >= 0) during(cur, cue, t - cue_t, f);
        while (nsamp < want && nsamp < nwav) {
            int32_t o[2 * CTL];
            int16_t s[2 * CTL];
            uint32_t i, n = CTL;
            mix_block(o, CTL);
            for (i = 1; i < CTL; i += 2u)
                scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)clamp(o[2u * i], -32768, 32767);
            if (nsamp + n > nwav) n = nwav - nsamp;
            for (i = 0; i < 2u * n; i++) s[i] = (int16_t)clamp(o[i], -32768, 32767);
            fwrite(s, 2, 2u * n, wav);
            nsamp += n;
        }
        if ((f & 7u) == 0u) mutate_poll();
        fprintf(frm, "%d", t >= SPLASH_S && ui.home && !ui.menu && !ui.confirm);
        for (k = 0; k < NTRK; k++) fprintf(frm, " %d", trk[k].p[P_MUTE] ? 0 : (int)trk[k].peak);
        fprintf(frm, "\n");
        host_ticks += 1000000u / FPS; fm1_ms += 1000u / FPS;
        if (t < SPLASH_S) {
            if (f == 0) { spl_anim = 1; draw_splash(); }
            splash_write((uint32_t)(t * 1000.0));
        } else {
            static int first = 1;
            if (first) { ui.force = 1; first = 0; act(cur, cue < 0 ? 0 : cue); }
            ui_input();
            ui_draw();
        }
        for (k = 0; k < NTRK; k++) trk[k].peak = 0;
        put_screen();
    }
    fclose(wav); fclose(rgb); fclose(cap); fclose(frm);
    printf("%u frames, %u samples, %.1f s\n", total, nsamp, SECS);
    return 0;
}
