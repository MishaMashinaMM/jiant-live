/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT 0.6.11; 0.6.37 dark minimal techno) the presentation video's footage: the firmware's own UI and audio, from the
 * host build of the tests (tests/ui_test.c): a dark minimal techno track at 125 BPM in A minor (six tracks: low FM6
 * chords held for two bars, an acid line on ANALOG, a cyber line on PHASE, LOFI blips, DRUM-X, NOISE wind) played by
 * the real engines, its screen captured at 30 fps as the script below goes through what JIANT FM has, a feature and
 * its page at a time. The captions go to scenes.txt (time, category, title, subtitle, kind: 0 a feature on its page,
 * 2 the grid of pages whose titles the subtitle lists), from the same script.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/video/capture tools/video/capture.c -lm
 *   build/video/capture build/video        -> audio.wav (44.1 kHz stereo), screen.rgb (240 x 240 RGB24 a frame)
 * tools/video/compose.py makes the 1080p video from them. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_VERSION "v0.7.6"
#include "ui_test.c"

#define FPS 30u
#define BPM 125u
#define BARS 66u
#define BEAT_S (60.0 / BPM)
#define SECS (BARS * 4u * 60.0 / BPM)
#define SPLASH_BEATS 8u                             /* the power-on splash: the first two bars */
#define B(bar, q) ((bar) * 4u + (q))                /* the beat q of a bar */
enum { T_LEAD, T_BASS, T_ARP, T_DRUM, T_PAD, T_ZAP };   /* the six tracks */
static FILE *wav, *rgb, *cap;
static uint32_t nsamp;

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
}
static void drums(const char *const rows[8])      /* one row a lane (BD SD CP CH OH TM RS CB): 'x' a hit, 'X' accented */
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
/* the beats: the kick alone, the minimal groove (open hats on the offbeat, a rim that wanders), the clap and the
 * ticking hats, the peak; the breakdown keeps only the metal */
static const char *const KICK[8] = {"X...X...X...X...", 0, 0, 0, 0, 0, 0, 0};
static const char *const MIN1[8] = {"X...X...X...X...", 0, 0, 0, "..x...x...x...x.", 0, "......x..x....x.", 0};
static const char *const MIN2[8] = {"X...X...X...X...", 0, "....x.......x...", "x.xxx.xxx.xxx.xx", "..x...x...x...x.", 0,
                                    "......x..x....x.", 0};
static const char *const BREAK[8] = {0, 0, 0, 0, "..x...x...x...x.", 0, "...x.....x....x.", 0};
static const char *const PEAK[8] = {"X...X...X...X..x", 0, "....X.......X...", "x.xxx.xxx.xxx.xx", "..x...x...x...x.",
                                    0, "...x.....x....x.", "..........x....."};

/* the chords: four notes low, two bars each (A minor: Am9, Fmaj7#11; the breakdown Dm9, E7b9) */
static const uint8_t CHA[2][4] = {{45, 52, 59, 60}, {41, 48, 52, 59}};
static const uint8_t CHB[2][4] = {{38, 45, 52, 53}, {40, 47, 50, 56}};
static void chords(const uint8_t ch[2][4])
{
    uint32_t i;
    for (i = 0; i < 64u; i++)
        put(T_PAD, i, i % 32u == 0u ? 4u : 0u, ch[i / 32u], i % 32u == 0u ? ST_NOTE : i % 32u < 31u ? ST_TIE : ST_REST, 0);
}

static void song_build(void)
{
    /* the acid (32 steps, two bars: note, 0 a rest; S slides into the next, A accented) */
#define S_ 0x100
#define A_ 0x200
    static const uint16_t ACID[32] = {
        33 | A_, 33, 45 | S_, 33, 0, 36, 33 | A_, 43 | S_, 45, 33, 0, 40, 33, 46 | S_, 45, 33,
        33 | A_, 33, 45, 36 | S_, 0, 33, 31 | A_, 33, 45 | S_, 44, 33, 0, 40, 33 | A_, 45 | S_, 43};
    /* the cyber line (64 steps: note, 0 rest, 1 hold), the leading tone G#4 at the end */
    static const uint8_t MEL[64] = {
        64, 1, 1, 0, 0, 0, 67, 1, 1, 1, 0, 0, 69, 1, 72, 1,
        71, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 67, 1, 64, 1,
        0, 0, 0, 0, 76, 1, 1, 0, 74, 1, 72, 1, 71, 1, 1, 1,
        68, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0, 0};
    static const uint8_t BLIP[16] = {0, 0, 0, 81, 0, 0, 0, 0, 0, 0, 76, 0, 0, 0, 0, 0};
    uint32_t i, k;
    ui_power_on();
    settings.palette = UI_JIANT_INDEX; palette_set(UI_JIANT_INDEX);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();               /* (0.6.30) TRACKS 6: all six play (4 the default) */
    song.g[G_BPM] = BPM;
    song.master_q12 = 4096;                                  /* (the pot: ui_power_on leaves 0) */
    sound(T_LEAD, "PHASE", "WIRE");
    sound(T_BASS, "ANALOG", "ACID");
    sound(T_ARP, "LOFI", "PULSE LD");
    sound(T_DRUM, "DRUM", "DRUM-X");
    sound(T_PAD, "FM6", "PAD");
    sound(T_ZAP, "NOISE", "WIND");
    for (k = 0; k < NTRK; k++) {
        track_defaults_steps(&trk[k]);
        trk[k].p[P_ROOT] = 9; trk[k].p[P_SCALE] = 2;          /* A minor */
        trk[k].p[P_SLEN] = 16;
        trk[k].p[P_MUTE] = 1;
    }
    trk[T_LEAD].p[P_SLEN] = 64;
    for (i = 0; i < 64u; i++) {
        uint8_t n = MEL[i];
        put(T_LEAD, i, n > 1u ? 1u : 0u, &n, n > 1u ? ST_NOTE : n ? ST_TIE : ST_REST, 0);
    }
    trk[T_LEAD].p[P_REV] = 70; trk[T_LEAD].p[P_DLY] = 60; trk[T_LEAD].p[P_LEVEL] = 76; trk[T_LEAD].p[P_PAN] = 12;
    trk[T_BASS].p[P_SLEN] = 32;
    for (i = 0; i < 32u; i++) {
        uint8_t n = (uint8_t)(ACID[i] & 0xFFu);
        put(T_BASS, i, n ? 1u : 0u, &n, n ? ST_NOTE : ST_REST,
            (ACID[i] & A_ ? SF_ACCENT : 0u) | (ACID[i] & S_ ? SF_SLIDE : 0u));
    }
    trk[T_BASS].p[P_E4] = 28; trk[T_BASS].p[P_E5] = 110; trk[T_BASS].p[P_E6] = 20;   /* closed, ringing */
    trk[T_BASS].p[P_DLY] = 20; trk[T_BASS].p[P_LEVEL] = 112;
    for (i = 0; i < 16u; i++) { uint8_t n = BLIP[i]; put(T_ARP, i, n ? 1u : 0u, &n, n ? ST_NOTE : ST_REST, 0); }
    trk[T_ARP].p[P_DLY] = 80; trk[T_ARP].p[P_REV] = 60; trk[T_ARP].p[P_LEVEL] = 80; trk[T_ARP].p[P_PAN] = -20;
    trk[T_PAD].p[P_SLEN] = 64; trk[T_PAD].p[P_SGATE] = 120;
    chords(CHA);
    trk[T_PAD].p[P_FTYPE] = 0; trk[T_PAD].p[P_FCUT] = 62; trk[T_PAD].p[P_FRES] = 20;   /* dark */
    trk[T_PAD].p[P_REV] = 90; trk[T_PAD].p[P_CHOR] = 50; trk[T_PAD].p[P_LEVEL] = 90;
    {   uint8_t n = 45; put(T_ZAP, 0, 1, &n, ST_NOTE, 0); for (i = 1; i < 16u; i++) put(T_ZAP, i, 0, &n, ST_TIE, 0); }
    trk[T_ZAP].p[P_LEVEL] = 60; trk[T_ZAP].p[P_REV] = 100;
    drums(KICK);
    trk[T_DRUM].p[P_E0] = 16; trk[T_DRUM].p[P_REV] = 14; trk[T_DRUM].p[P_LEVEL] = 84;
    trk[T_DRUM].p[P_LN2] = 80; trk[T_DRUM].p[P_LN3] = 52; trk[T_DRUM].p[P_LN4] = 62; trk[T_DRUM].p[P_LN6] = 76;
    trk[T_DRUM].p[P_LN7] = 60;                               /* (square law: the metal well under the kick) */
    dx_tone = 5u;                                            /* the hats a little narrower */
    song.g[G_RTYPE] = 0; song.g[G_RSIZE] = 110;
    song.g[G_DTIME] = 3;
    chain_config.count = 4;                                    /* (the SONG page: a chain to look at) */
    chain_config.row[0] = (chain_row_t){0, 4}; chain_config.row[1] = (chain_row_t){1, 8};
    chain_config.row[2] = (chain_row_t){2, 4}; chain_config.row[3] = (chain_row_t){0, 8};
    for (k = 0; k < 4u; k++) project_save(k);                 /* (ARRANGE's sections: stored, not EMPTY) */
    ui.msg_t = 0; ui.msg[0] = ui.msg2[0] = 0;
    transport_req = 1;
#undef S_
#undef A_
}

static void caption(uint32_t q, const char *tag, const char *title, const char *sub, int kind)   /* (lines split by '|') */
{
    fprintf(cap, "%.4f\t%s\t%s\t%s\t%d\n", q * BEAT_S, tag, title, sub, kind);
}
static void page(uint32_t sel, const char *title) { song.sel = (uint8_t)sel; go_title(title); }
static void home(uint32_t sel) { song.sel = (uint8_t)sel; go_home(); }
static void slicer(uint32_t mode, uint32_t mask)
{
    uint32_t k;
    song.g[G_SLMODE] = (int16_t)mode; song.g[G_SLPAT] = 9; song.g[G_SLRATE] = 1; song.g[G_SLDEP] = 127;
    for (k = 0; k <= NTRK; k++) song.g[G_SLT1 + k] = (int16_t)((mask >> k) & 1u);
}
static void hold_btn(uint32_t label, int on)
{
    if (on) { fm1_in.buttons |= 1u << panel.btn[label]; host_pressed |= 1u << panel.btn[label]; }
    else fm1_in.buttons &= ~(1u << panel.btn[label]);
}
static void enc_turn(uint32_t role, int32_t s) { host_enc[panel.enc[role]] += s * panel.dir[role]; }
static void mute(uint32_t k, int m) { trk[k].p[P_MUTE] = (int16_t)m; }

/* the script: at each beat q (bar q / 4); the captions go with it */
static project_store_t film_song;
static void at_beat(uint32_t q)
{
    uint32_t bar = q / 4u, k;
    /* ---- the acid opens slowly from its entry to the breakdown, again over the peak */
    if (q >= B(8, 0) && q < B(28, 0)) trk[T_BASS].p[P_E4] = (int16_t)(28 + (q - B(8, 0)) * 34u / 80u);
    if (q >= B(36, 0) && q < B(48, 0)) trk[T_BASS].p[P_E4] = (int16_t)(44 + (q - B(36, 0)) * 30u / 48u);

    /* ---- the intro: the chords and the wind under the splash */
    if (q == B(0, 0)) { mute(T_PAD, 0); mute(T_ZAP, 0); }
    if (q == B(3, 0)) {
        home(T_PAD);
        caption(q, "SYSTEM", "SIX LIVE TRACKS", "eight synthesis engines|no samples", 0);
    }
    /* ---- the drums */
    if (q == B(4, 0)) {
        mute(T_DRUM, 0); page(T_DRUM, "EDIT 1");
        caption(q, "DRUMS", "DRUM-X", "8 synthesized voices|morph from kit A to kit B", 0);
    }
    if (q >= B(4, 0) && q < B(6, 0)) trk[T_DRUM].p[P_E0] = (int16_t)(16 + (q - B(4, 0)) * 6u);
    if (q == B(6, 0)) {
        drums(MIN1); dx_ui_lane = 0; page(T_DRUM, "SOUND");
        caption(q, "DRUMS", "EVERY VOICE ITS OWN", "pitch · decay · noise|color · wave", 0);
    }
    if (q == B(7, 0)) dx_ui_lane = 4;
    /* ---- the synths */
    if (q == B(8, 0)) {
        mute(T_BASS, 0); page(T_BASS, "EDIT 2");
        caption(q, "SYNTHESIS", "ACID ANALOG", "cutoff · resonance · drive|slides and accents in the steps", 0);
    }
    if (q == B(10, 0)) {
        page(T_BASS, "FILTER");
        caption(q, "SYNTHESIS", "FILTERS", "LP · HP · BP · COMB|on every engine", 0);
    }
    if (q >= B(10, 1) && q < B(11, 2)) trk[T_BASS].p[P_FTYPE] = (int16_t)((q - B(10, 1)) % 4u);   /* LP HP BP COMB */
    if (q == B(11, 2)) trk[T_BASS].p[P_FTYPE] = 0;
    if (q == B(12, 0)) {
        mute(T_ARP, 0); page(T_PAD, "OPERATOR");
        caption(q, "SYNTHESIS", "FM6 OPERATORS", "six operators on the device|ratio · level · envelopes", 0);
    }
    if (q >= B(12, 0) && q < B(13, 0)) enc_turn(EN_K1, 1);
    if (q == B(13, 0)) page(T_PAD, "OP ENV");
    if (q >= B(13, 0) && q < B(14, 0)) enc_turn(EN_K2, 1);
    if (q == B(14, 0)) {
        page(T_ARP, "EDIT 1");
        caption(q, "SYNTHESIS", "LOFI BYTEBEAT", "formulas as oscillators|bytebeat · floatbeat · wave RAM", 0);
    }
    /* ---- the groove: X-MOD, TONE, the cyber line */
    if (q == B(16, 0)) {
        drums(MIN2);
        dx_mot[XM_RATE] = 5; dx_mot[XM_DPTH] = 60; dx_mot[XM_SHPE] = 1; dx_mot[XM_RAND] = 25;
        page(T_DRUM, "X-MOD"); caption(q, "DRUMS", "X-MOD", "the kit's own LFO on MORPH|a random push on every hit", 0);
    }
    if (q == B(18, 0)) { page(T_DRUM, "KIT 3"); caption(q, "DRUMS", "KIT 3", "band-pass on hats · rim · bell|feedback on every voice", 0); }
    if (q >= B(18, 0) && q < B(20, 0)) dx_tone = (uint8_t)((q >= B(19, 0) ? 0x20u : 0u) | (2u + (q - B(18, 0)) * 3u / 2u));
    if (q == B(20, 0)) {
        dx_tone = 0x18u; mute(T_LEAD, 0); page(T_LEAD, "EDIT 1");
        caption(q, "SYNTHESIS", "PHASE", "phase distortion|two waves · DCW · feedback", 0);
    }
    /* ---- modulation */
    if (q == B(22, 0)) {
        trk[T_BASS].p[P_M1SRC] = MS_LFO; trk[T_BASS].p[P_M1DST] = MD_CUT; trk[T_BASS].p[P_M1AMT] = 18;
        page(T_BASS, "MOD"); caption(q, "MODULATION", "MOD MATRIX", "4 slots a track|LFO · envelopes · macros · MSEQ", 0);
    }
    if (q == B(24, 0)) {
        trk[T_BASS].p[P_M2SRC] = MS_STEP; trk[T_BASS].p[P_M2DST] = MD_CUT; trk[T_BASS].p[P_M2AMT] = 26;
        trk[T_BASS].p[P_MSLEN] = 8; trk[T_BASS].p[P_MSDIV] = 3;
        for (k = 0; k < 16u; k++) trk[T_BASS].p[P_MS0 + k] = (int16_t)((k * 53u + 10u) % 128u);
        page(T_BASS, "MSEQ"); caption(q, "MODULATION", "MSEQ", "a 16-step modulation sequencer|here on the acid's cutoff", 0);
    }
    if (q == B(26, 0)) {
        trk[T_PAD].p[P_ELOOP] = 1; trk[T_PAD].p[P_ED_FLT] = 30; page(T_PAD, "ENV DEST");
        caption(q, "MODULATION", "ENVELOPES THAT LOOP", "an ADSR held cycles|like a shaped LFO", 0);
    }
    /* ---- the breakdown: the kick out, the other chords, the effects */
    if (q == B(28, 0)) {
        drums(BREAK); chords(CHB);
        page(T_LEAD, "STEP"); caption(q, "SEQUENCING", "PIANO ROLL", "chance · ratchets · slides|parameter locks", 0);
    }
    if (q == B(28, 2)) page(T_LEAD, "CHANCE");
    if (q == B(29, 0)) page(T_BASS, "STEP");
    if (q == B(29, 2)) page(T_LEAD, "AUTOMATION");
    if (q == B(30, 0)) {
        song.g[G_RTYPE] = 3; song.g[G_RDAMP] = 1; song.g[G_RPRE] = 2; trk[T_LEAD].p[P_REV] = 100;
        page(T_LEAD, "REVERB"); caption(q, "EFFECTS", "FIVE REVERBS", "ROOM · SPRING · SHIMMER|RESO tuned to the scale · CLOUD", 0);
    }
    if (q == B(31, 0)) song.g[G_RTYPE] = 2;
    if (q == B(31, 2)) song.g[G_RTYPE] = 4;
    if (q == B(32, 0)) {
        song.g[G_RTYPE] = 3; song.g[G_DPIT] = 7; song.g[G_DFDBK] = 70; page(T_LEAD, "DLY 2");
        caption(q, "EFFECTS", "GRAIN DELAY", "repeats pitched ±12 semitones|a spray of grains", 0);
    }
    if (q == B(33, 0)) song.g[G_DPIT] = 12;
    if (q == B(34, 0)) {
        song.g[G_DPIT] = 0; song.g[G_DFDBK] = 55; trk[T_BASS].p[P_DIST] = 50; page(T_BASS, "DIST");
        caption(q, "EFFECTS", "FIVE DISTORTIONS", "SOFT · HARD · FOLD · CRUSH · RECT|on every track", 0);
    }
    if (q >= B(34, 0) && q < B(36, 0) && q % 2u == 0u) trk[T_BASS].p[P_DTYPE] = (int16_t)((q - B(34, 0)) / 2u % 5u);
    /* ---- the peak */
    if (q == B(36, 0)) {
        trk[T_BASS].p[P_DTYPE] = 2; trk[T_BASS].p[P_DIST] = 26; trk[T_LEAD].p[P_REV] = 70;
        drums(PEAK); chords(CHA); trk[T_DRUM].p[P_E0] = 40;
        slicer(2, (1u << T_DRUM) | (1u << T_BASS));
        song.g[G_SLPLEN] = 4; song.g[G_SLP0] = 0; song.g[G_SLP0 + 1] = 12; song.g[G_SLP0 + 2] = 7; song.g[G_SLP0 + 3] = -5;
        page(T_DRUM, "SLICER"); caption(q, "PERFORMANCE", "SLICER BUS", "stutter and gate after the effects|on any of the 6 tracks", 0);
    }
    if (q == B(37, 0)) page(T_DRUM, "SL PITCH");
    if (q == B(38, 0)) { slicer(0, 0); home(T_LEAD); hold_btn(B_FX, 1); caption(q, "PERFORMANCE", "PUNCH-IN FX", "hold FX, play the keys|repeat · octave · stutter", 0); }
    if (bar == 38u && q % 4u == 3u) { perf_press(PF_R16, 1); lys.used = 1; }   /* (0.6.80: used, as the keys mark it:
                                                         * let go, FX does not lock open, 0.6.44) */
    if (bar == 39u) { perf_press(PF_R16, q % 4u == 0u); perf_press(PF_OCTU, q % 4u == 1u || q % 4u == 2u); perf_press(PF_S16, q % 4u >= 2u); }
    if (q == B(40, 0)) {
        hold_btn(B_FX, 0); perf_press(PF_S16, 0); perf_press(PF_OCTU, 0); perf_press(PF_R16, 0);
        song.g[G_CLIP] = 50; song.g[G_PUNCH] = 80; song.g[G_DUCK] = 40;
        page(T_DRUM, "MASTER"); caption(q, "PERFORMANCE", "MASTER", "clipper · drum punch|sidechain duck · leveler", 0);
    }
    if (q == B(42, 0)) { home(T_BASS); hold_btn(B_LFO, 1); caption(q, "PERFORMANCE", "MACROS", "4 macros over what's playing|their dice roll new routes", 0); }
    if (q >= B(42, 0) && q < B(43, 0)) for (k = 0; k < 4u; k++) macro_v[k] = (uint8_t)clamp((int32_t)((q - B(42, 0) + 1u) * (10u + 4u * k)), 0, 127);
    if (q == B(42, 2)) { macro_dice(1); ui_message("MACROS DICED"); }
    if (q == B(42, 3)) { hold_btn(B_LFO, 0); for (k = 0; k < 4u; k++) macro_v[k] = 0; }
    if (q == B(43, 0)) hold_btn(B_LFO, 1);                     /* (LFO again: DONE) */
    if (q == B(43, 1)) hold_btn(B_LFO, 0);
    /* ---- the song */
    if (q == B(44, 0)) { go_title("MIXER"); caption(q, "SONG", "6-CHANNEL MIXER", "level · pan · send · mute|thermal meters", 0); }
    if (q == B(46, 0)) {
        chain_config.count = 5;
        chain_config.row[0] = (chain_row_t){0, 4}; chain_config.row[1] = (chain_row_t){1, 8};
        chain_config.row[2] = (chain_row_t){2, 4}; chain_config.row[3] = (chain_row_t){3, 2}; chain_config.row[4] = (chain_row_t){1, 8};
        SONG_END(&chain_config) = SONG_FOREVER;                 /* END: LOOP */
        go_title("ARRANGE"); ui.song_row = 0;
        caption(q, "SONG", "ARRANGE", "sections in a column|bars · scenes · END x2..x8 · LOOP", 0);
    }
    if (q > B(46, 0) && q < B(48, 0)) ui.song_row = (uint8_t)((q - B(46, 0)) < 5u ? q - B(46, 0) : 6u);
    if (q == B(48, 0)) {                                       /* DICE ALL: the song packed first, UNDO after */
        project_capture(&proj_scratch); proj_pack(&film_song, &proj_scratch);
        home(T_DRUM); enc_turn(EN_SELECT, 1);
        caption(q, "SONG", "DICE ALL", "HOME + SELECT: a whole new piece|15 genres · SPACE to FRANTIC", 0);
    }
    if (q == B(48, 2)) { dice_force_style = 2; dice_force_energy = 1; hold_btn(B_OCTUP, 1); }   /* DARK MINIMAL */
    if (q == B(48, 3)) hold_btn(B_OCTUP, 0);
    if (q == B(49, 0)) song.g[G_BPM] = BPM;                    /* (the film keeps its tempo) */
    if (q == B(50, 0)) {
        dice_force_style = dice_force_energy = -1;
        if (!undo_do()) ui_message("UNDONE (AGAIN: REDO)");
        if (proj_import(&proj_scratch, &film_song, sizeof film_song) && !project_restore_runtime(&proj_scratch))
            transport_req = 1;
        song.g[G_BPM] = BPM;
        caption(q, "SONG", "UNDO", "GLO + G4: back one step|again: forward", 0);
    }
    /* ---- the system */
    if (q == B(51, 0)) { go_title("SYSTEM"); caption(q, "SYSTEM", "TRACKS · LOAD", "4 or 6 tracks|ECO · LITE · FULL: you pick the weight", 0); }
    if (q == B(51, 1) || q == B(51, 2)) enc_turn(EN_K2, -1);    /* LOAD: LITE, ECO */
    if (q == B(51, 3)) enc_turn(EN_K2, 2);                      /* .. FULL again */
    if (q == B(52, 0)) { page(T_LEAD, "SCL"); caption(q, "SYSTEM", "ONE SCALE", "every track, the reverb|and the arp follow it", 0); }
    if (q == B(52, 1) || q == B(52, 2)) enc_turn(EN_K1, 1);
    if (q == B(52, 3)) enc_turn(EN_K1, -2);
    if (q == B(53, 0)) { home(T_PAD); hold_btn(B_ENV, 1); caption(q, "SYSTEM", "PAGE JUMP", "hold a page button|turn PRESETS: straight to a page", 0); }
    if (q > B(53, 0) && q < B(54, 3)) enc_turn(EN_PRESET, q % 2u ? 1 : -1);
    if (q == B(54, 3)) hold_btn(B_ENV, 0);
    if (q == B(55, 0)) { page(T_DRUM, "STEP"); caption(q, "SEQUENCING", "SEQ TOOLS", "reverse · shift · random · cook|beat · clear automation", 0); }
    if (q == B(55, 1)) hold_btn(B_SEQ, 1);
    if (q == B(55, 3)) hold_btn(B_SEQ, 0);
    /* ---- the grid of pages (compose.py replays them), the beings under it; the outro */
    if (q == B(56, 0)) {
        home(T_DRUM);
        caption(q, "", "ONE SMALL SCREEN", "DRUM-X|ACID ANALOG|FM6 OPERATORS|TONE|MSEQ|SLICER BUS|PUNCH-IN FX|ARRANGE", 2);
    }
    if (q == B(60, 0)) { caption(q, "", "", "", 0); slicer(2, 1u << T_DRUM); }
    if (q == B(60, 2)) slicer(0, 0);
    if (q == B(62, 0)) { mute(T_DRUM, 1); mute(T_BASS, 1); mute(T_ARP, 1); trk[T_PAD].p[P_REV] = 110; }
    if (q == B(64, 0)) mute(T_LEAD, 1);
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
#ifndef CAPTURE_NO_MAIN                             /* (0.6.89: capture_shots.c takes the song alone) */
int main(int argc, char **argv)
{
    char p[512];
    uint32_t f, total = (uint32_t)(FPS * SECS), beat_done = 0xFFFFFFFFu, nwav = (uint32_t)(FS * SECS);
    const char *dir = argc > 1 ? argv[1] : "build/video";
    snprintf(p, sizeof p, "%s/audio.wav", dir); wav = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/screen.rgb", dir); rgb = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/scenes.txt", dir); cap = fopen(p, "w");
    if (!wav || !rgb || !cap) { fprintf(stderr, "cannot write in %s\n", dir); return 1; }
    wav_hdr(wav, nwav);                         /* (hostsim.c) */
    fprintf(cap, "# %u fps, %u BPM, %u bars, splash %.4f s, end %.4f s, beat %.6f s\n", FPS, BPM, BARS, SPLASH_BEATS * BEAT_S,
            SECS, BEAT_S);
    song_build();
    for (f = 0; f < total; f++) {
        double t = (double)f / FPS;
        uint32_t want = (uint32_t)((uint64_t)(f + 1u) * FS / FPS), beat = (uint32_t)(t / BEAT_S);
        if (beat != beat_done) {
            beat_done = beat; at_beat(beat);
            if (getenv("FILM_SOLO")) {                  /* (a check: only these tracks sound, a bit each) */
                uint32_t m = (uint32_t)strtoul(getenv("FILM_SOLO"), 0, 0), k;
                for (k = 0; k < NTRK; k++) if (!((m >> k) & 1u)) trk[k].p[P_MUTE] = 1;
            }
        }
        while (nsamp < want && nsamp < nwav) {          /* the audio up to this frame's end */
            int32_t o[2 * CTL];
            int16_t s[2 * CTL];
            uint32_t i, n = CTL;
            mix_block(o, CTL);
            for (i = 1; i < CTL; i += 2u)                /* (the scope, as audio.c audio_block fills it) */
                scope_buf[scope_w++ & (SCOPE_N - 1u)] = (int16_t)clamp(o[2u * i], -32768, 32767);
            if (nsamp + n > nwav) n = nwav - nsamp;
            for (i = 0; i < 2u * n; i++) s[i] = (int16_t)clamp(o[i], -32768, 32767);
            fwrite(s, 2, 2u * n, wav);
            nsamp += n;
        }
        host_ticks += 1000000u / FPS; fm1_ms += 1000u / FPS;
        if (t < SPLASH_BEATS * BEAT_S) {                 /* the power-on splash: the signature written */
            if (f == 0) { spl_anim = 1; draw_splash(); }
            splash_write((uint32_t)(t * 1000.0));
        } else {
            static int first = 1;
            if (first) { ui.force = 1; first = 0; }
            ui_input();
            ui_draw();
        }
        put_screen();
    }
    fclose(wav); fclose(rgb); fclose(cap);
    printf("%u frames, %u samples\n", total, nsamp);
    return 0;
}
#endif
