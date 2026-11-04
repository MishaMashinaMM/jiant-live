/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT 0.6.66) the v5 films' footage: a darkwave song in E minor at 132 BPM (a cold post-punk drum machine, an
 * 8th-note bass, chorused plucks, frozen strings, a choir and a melancholic lead: six tracks on six engines) played by
 * the real engines, the screen captured at 30 fps while a script goes through one film:
 *   feat  every feature of this version: six tracks, LOAD FULL / LITE / ECO, DRUM-X, the engines, modulation, the
 *         effects, performance, the dice with OK / CANCEL, the page jump, the song
 *   seq   the sequencing: the piano roll, step components, SHIFT, automation, MUTATE, FUGUE, the arp, MSEQ, and the
 *         details: DRUM-X's MORPH, its sends to the effects, its stereo IMAGE, MORPH on the synth engines
 * Besides audio.wav and screen.rgb it writes scenes.txt (the captions, as capture.c) and frames.txt (a line a frame:
 * HOME shown, then each track's peak since the last frame: the films' beings follow the tracks they stand for).
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/video5/capture_dark tools/video/capture_dark.c -lm
 *   build/video5/capture_dark build/video5/feat feat;  build/video5/capture_dark build/video5/seq seq
 * tools/video/film.mjs makes the 1080p films from them. */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_VERSION "v0.7.6"
#define PF_MS 900u                                   /* (the page's name shown, as on the device) */
#define UI_NO_DRIFT 0                                /* (the granules, as on the device) */
#include "ui_test.c"

#define FPS 30u
#define BPM 132u
#define BEAT_S (60.0 / BPM)
#define SPLASH_BEATS 8u
#define B(bar, q) ((bar) * 4u + (q))
enum { T_LEAD, T_BASS, T_ARP, T_DRUM, T_PAD, T_CHOIR };
static FILE *wav, *rgb, *cap, *frm;
static uint32_t nsamp, film_bars, film_seq;

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
/* the drum machine: the cold post-punk beat (the kick on one and the and of two, the snare on two and four, 8th hats),
 * its driving form (four on the floor, 16th hats), the break (rim and hats only) */
static const char *const POST[8] = {"X.....x.X.......", "....X.......X...", 0, "x.x.x.x.x.x.x.x.", "..............x.", 0, 0, 0};
static const char *const DRIVE[8] = {"X...X...X...X...", "....X.......X...", "............x...", "xxx.xxx.xxx.xxx.",
                                     "..x...x...x...x.", 0, 0, 0};
static const char *const FILLD[8] = {"X...X...X...X...", "....X.......X.xx", 0, "x.x.x.x.x.x.x.x.", 0, "........x.x.xx..", 0, 0};
static const char *const BRK[8] = {0, 0, 0, "x.x.x.x.x.x.x.x.", "..............x.", 0, "....x.......x...", 0};

/* E minor: the verse Em C G D, the chorus Am Em C B; a bar a chord */
static const uint8_t ROOT_V[4] = {40, 36, 43, 38}, ROOT_C[4] = {45, 40, 36, 47};
static const uint8_t CH_V[4][4] = {{52, 55, 59, 64}, {48, 55, 60, 64}, {50, 55, 59, 62}, {50, 54, 57, 62}};
static const uint8_t CH_C[4][4] = {{45, 52, 57, 60}, {52, 55, 59, 64}, {48, 55, 60, 64}, {47, 51, 54, 59}};
static const uint8_t ARPV[4][8] = {{64, 67, 71, 76, 71, 67, 71, 74}, {64, 67, 72, 76, 72, 67, 72, 74},
                                   {62, 67, 71, 74, 71, 67, 71, 74}, {62, 66, 69, 74, 69, 66, 69, 73}};
static const uint8_t ARPC[4][8] = {{69, 72, 76, 81, 76, 72, 76, 79}, {64, 67, 71, 76, 71, 67, 71, 74},
                                   {64, 67, 72, 76, 72, 67, 72, 74}, {63, 66, 71, 75, 71, 66, 71, 75}};
static void section(int chorus)
{
    const uint8_t *root = chorus ? ROOT_C : ROOT_V;
    const uint8_t (*ch)[4] = chorus ? CH_C : CH_V, (*ar)[8] = chorus ? ARPC : ARPV;
    uint32_t b, i;
    for (b = 0; b < 4u; b++) {
        for (i = 0; i < 16u; i++) {                   /* the bass: 8ths on the root, the last one up an octave */
            uint8_t n = (uint8_t)(root[b] + (i == 14u ? 12u : 0u));
            put(T_BASS, b * 16u + i, i % 2u ? 0u : 1u, &n, i % 2u ? ST_REST : ST_NOTE, i == 0u ? SF_ACCENT : 0u);
        }
        for (i = 0; i < 16u; i++) {                   /* the plucks: 8ths through the chord, an octave up */
            uint8_t n = ar[b][i / 2u];
            put(T_ARP, b * 16u + i, i % 2u ? 0u : 1u, &n, i % 2u ? ST_REST : ST_NOTE, 0);
        }
        for (i = 0; i < 16u; i++)                     /* the strings: the chord held the bar */
            put(T_PAD, b * 16u + i, i ? 0u : 4u, ch[b], i ? (i < 15u ? ST_TIE : ST_REST) : ST_NOTE, 0);
        for (i = 0; i < 16u; i++) {                   /* the choir: the chord's top two, an octave up, the bar */
            uint8_t v[2] = {(uint8_t)(ch[b][2] + 12u), (uint8_t)(ch[b][3] + 12u)};
            put(T_CHOIR, b * 16u + i, i ? 0u : 2u, v, i ? (i < 15u ? ST_TIE : ST_REST) : ST_NOTE, 0);
        }
    }
    {   /* the lead: a falling, sighing line (note, 16ths; 0 a rest) */
        static const uint8_t LV[][2] = {{71, 6}, {69, 2}, {67, 4}, {66, 4}, {64, 8}, {67, 4}, {64, 4},
                                        {74, 6}, {71, 2}, {69, 4}, {67, 4}, {66, 12}, {0, 4}};
        static const uint8_t LC[][2] = {{72, 6}, {71, 2}, {69, 8}, {67, 6}, {71, 2}, {64, 8},
                                        {76, 4}, {74, 4}, {72, 4}, {71, 4}, {71, 12}, {0, 4}};
        const uint8_t (*m)[2] = chorus ? LC : LV;
        uint32_t nm = chorus ? 12u : 13u, j, p = 0;
        for (j = 0; j < nm; j++) {
            uint8_t n = m[j][0];
            for (i = 0; i < m[j][1] && p < 64u; i++, p++)
                put(T_LEAD, p, n && !i ? 1u : 0u, &n, !n ? ST_REST : i ? ST_TIE : ST_NOTE, 0);
        }
    }
}

static void song_build(void)
{
    uint32_t k;
    ui_power_on();
    settings.palette = UI_JIANT_INDEX; palette_set(UI_JIANT_INDEX);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();        /* TRACKS 6 */
    song.g[G_BPM] = BPM;
    song.master_q12 = 4096;
    sound(T_LEAD, "ANALOG", "SAW LEAD");
    sound(T_BASS, "ANALOG", "FAT BASS");
    sound(T_ARP, "FM6", "PLUCK");
    sound(T_DRUM, "DRUM", "DRUM-X");
    sound(T_PAD, "ANALOG", "STRINGS");
    sound(T_CHOIR, "VOICE", "CHOIR AAH");
    for (k = 0; k < NTRK; k++) {
        track_defaults_steps(&trk[k]);
        trk[k].p[P_ROOT] = 4; trk[k].p[P_SCALE] = 2;  /* E minor */
        trk[k].p[P_SLEN] = 64;
        trk[k].p[P_MUTE] = 1;
    }
    trk[T_DRUM].p[P_SLEN] = 16;
    section(0);
    drums(POST);
    trk[T_LEAD].p[P_LEVEL] = 70; trk[T_LEAD].p[P_DLY] = 70; trk[T_LEAD].p[P_REV] = 70; trk[T_LEAD].p[P_CHOR] = 60;
    trk[T_LEAD].p[P_PAN] = 10; trk[T_LEAD].p[P_GLIDE] = 20;
    trk[T_BASS].p[P_LEVEL] = 100; trk[T_BASS].p[P_CHOR] = 90; trk[T_BASS].p[P_FCUT] = 70;
    trk[T_ARP].p[P_LEVEL] = 104; trk[T_ARP].p[P_CHOR] = 110; trk[T_ARP].p[P_DLY] = 70; trk[T_ARP].p[P_REV] = 50;
    trk[T_ARP].p[P_PAN] = -22;
    trk[T_PAD].p[P_LEVEL] = 66; trk[T_PAD].p[P_REV] = 100; trk[T_PAD].p[P_CHOR] = 70; trk[T_PAD].p[P_FCUT] = 66;
    trk[T_PAD].p[P_SGATE] = 120;
    trk[T_CHOIR].p[P_LEVEL] = 58; trk[T_CHOIR].p[P_REV] = 120; trk[T_CHOIR].p[P_PAN] = 18;
    trk[T_DRUM].p[P_LEVEL] = 92; trk[T_DRUM].p[P_REV] = 40; trk[T_DRUM].p[P_E0] = 20;
    dx_snd[1] = (uint8_t)(3u << 6 | 40u);             /* the snare into the reverb: the gated 80s snare */
    dx_snd[3] = (uint8_t)(2u << 6 | 14u);             /* a little of the hats into the delay */
    song.g[G_RTYPE] = 0; song.g[G_RSIZE] = 116; song.g[G_DTIME] = 3; song.g[G_DFDBK] = 40;
    song.g[G_CDEPTH] = 90; song.g[G_CRATE] = 30;
    chain_config.count = 4;
    chain_config.row[0] = (chain_row_t){0, 4}; chain_config.row[1] = (chain_row_t){1, 8};
    chain_config.row[2] = (chain_row_t){2, 4}; chain_config.row[3] = (chain_row_t){0, 8};
    for (k = 0; k < 4u; k++) project_save(k);
    ui.msg_t = 0; ui.msg[0] = ui.msg2[0] = 0;
    transport_req = 1;
}

static void caption(uint32_t q, const char *tag, const char *title, const char *sub, int kind)
{
    fprintf(cap, "%.4f\t%s\t%s\t%s\t%d\n", q * BEAT_S, tag, title, sub, kind);
}
static void page(uint32_t sel, const char *title) { song.sel = (uint8_t)sel; go_title(title); }
static void home(uint32_t sel) { song.sel = (uint8_t)sel; go_home(); }
static void hold_btn(uint32_t label, int on)
{
    if (on) { fm1_in.buttons |= 1u << panel.btn[label]; host_pressed |= 1u << panel.btn[label]; }
    else fm1_in.buttons &= ~(1u << panel.btn[label]);
}
static void enc_turn(uint32_t role, int32_t s) { host_enc[panel.enc[role]] += s * panel.dir[role]; }
static void mute(uint32_t k, int m) { trk[k].p[P_MUTE] = (int16_t)m; }
static void band(int full)                        /* the arrangement's layers */
{
    mute(T_BASS, 0); mute(T_ARP, 0); mute(T_PAD, 0); mute(T_DRUM, 0); mute(T_LEAD, !full); mute(T_CHOIR, 0);
}
static void comp(uint32_t k, uint32_t i, uint32_t c, uint32_t a)
{
    trk[k].step[i].comp = (uint8_t)c; trk[k].step[i].carg = (uint8_t)a;
    if (k == song.sel) { ui.cursor = (uint8_t)i; ui.bank = (uint8_t)(i / 16u); }
}

/* ---------------------------------------------------------------- the features film */
static project_store_t film_song;
static void feat_beat(uint32_t q)
{
    uint32_t bar = q / 4u, k;
    if (q == B(0, 0)) { mute(T_PAD, 0); mute(T_CHOIR, 0); }
    if (q == B(2, 0)) {
        mute(T_BASS, 0); home(T_BASS);
        caption(q, "SYSTEM", "SIX LIVE TRACKS", "six engines at once|a being for every track", 0);
    }
    if (q == B(4, 0)) {
        mute(T_DRUM, 0); mute(T_ARP, 0);
        caption(q, "SYSTEM", "AUDIO-REACTIVE", "every being moves with its own sound|what you see is what you hear", 0);
    }
    if (q == B(6, 0)) { go_title("SYSTEM"); caption(q, "SYSTEM", "TRACKS 4 · 6", "four tracks light, six for the band|T5 · T6 asleep cost nothing", 0); }
    if (q == B(8, 0)) caption(q, "LOAD", "FULL", "every reverb · the GRAIN delay · SLICER|the beings at full frame rate", 0);
    if (q == B(10, 0)) { enc_turn(EN_K2, -1); caption(q, "LOAD", "LITE", "SHIMMER · CLOUD as ROOM · FM6 two voices|the screen at ~20 fps, lighter outlines", 0); }
    if (q == B(11, 0)) home(T_DRUM);
    if (q == B(12, 0)) { go_title("SYSTEM"); enc_turn(EN_K2, -1); caption(q, "LOAD", "ECO", "every reverb SPRING · no SLICER · no chorus|4 voices a synth · the screen at ~14 fps", 0); }
    if (q == B(13, 0)) home(T_DRUM);
    if (q == B(14, 0)) { go_title("SYSTEM"); enc_turn(EN_K2, 2); caption(q, "LOAD", "YOU PICK THE WEIGHT", "FULL again|smooth on the stage, light on the CPU", 0); }
    /* DRUM-X */
    if (q == B(16, 0)) { mute(T_LEAD, 0); page(T_DRUM, "EDIT 1"); caption(q, "DRUMS", "DRUM-X", "8 synthesized voices, no samples|MORPH from kit A to kit B", 0); }
    if (q >= B(16, 0) && q < B(18, 0)) trk[T_DRUM].p[P_E0] = (int16_t)(20 + (q - B(16, 0)) * 11u);
    if (q == B(18, 0)) { trk[T_DRUM].p[P_E0] = 30; dx_ui_lane = 1; page(T_DRUM, "SOUND"); caption(q, "DRUMS", "EVERY VOICE ITS OWN", "pitch · decay · noise · color · wave|layer · feedback · its own send", 0); }
    if (q == B(19, 0)) dx_ui_lane = 3;
    if (q == B(20, 0)) { page(T_DRUM, "STEREO"); caption(q, "DRUMS", "STEREO IMAGE", "one knob places the whole kit|each voice on its own orbit", 0); }
    if (q >= B(20, 0) && q < B(22, 0)) trk[T_DRUM].p[P_DXIMG] = (int16_t)(10 + (q - B(20, 0)) * 14u);
    /* the engines */
    if (q == B(22, 0)) { page(T_BASS, "EDIT 1"); caption(q, "ENGINES", "ANALOG", "saw · square · sync · ring|TRIO: three oscillators", 0); }
    if (q == B(24, 0)) { page(T_LEAD, "MORPH"); caption(q, "ENGINES", "WAVE MORPH", "ANALOG · PHASE · LOFI|the wave melts into the next", 0); }
    if (q >= B(24, 0) && q < B(26, 0)) trk[T_LEAD].p[P_MORPH] = (int16_t)((q - B(24, 0)) * 15u);
    if (q == B(26, 0)) { trk[T_LEAD].p[P_MORPH] = 0; page(T_ARP, "OPERATOR"); caption(q, "ENGINES", "FM6", "six operators on the device|32 classic voices in a bank", 0); }
    if (q >= B(26, 0) && q < B(27, 0)) enc_turn(EN_K1, 1);
    if (q == B(27, 0)) page(T_ARP, "OP ENV");
    if (q == B(28, 0)) { page(T_CHOIR, "EDIT 1"); caption(q, "ENGINES", "VOICE", "a formant choir|vowels you can play", 0); }
    if (q == B(30, 0)) { page(T_BASS, "FILTER"); caption(q, "ENGINES", "FILTERS", "LP · HP · BP · COMB|resonance and drive, on every engine", 0); }
    if (q >= B(30, 1) && q < B(31, 2)) trk[T_BASS].p[P_FTYPE] = (int16_t)((q - B(30, 1)) % 4u);
    if (q == B(31, 2)) trk[T_BASS].p[P_FTYPE] = 0;
    /* modulation */
    if (q == B(32, 0)) {
        section(1);
        trk[T_PAD].p[P_M1SRC] = MS_LFO; trk[T_PAD].p[P_M1DST] = MD_CUT; trk[T_PAD].p[P_M1AMT] = 24;
        page(T_PAD, "MOD"); caption(q, "MODULATION", "MOD MATRIX", "4 slots a track|LFO · envelopes · macros · MSEQ", 0);
    }
    if (q == B(34, 0)) {
        trk[T_BASS].p[P_M2SRC] = MS_STEP; trk[T_BASS].p[P_M2DST] = MD_CUT; trk[T_BASS].p[P_M2AMT] = 26;
        trk[T_BASS].p[P_MSLEN] = 8; trk[T_BASS].p[P_MSDIV] = 3;
        for (k = 0; k < 16u; k++) trk[T_BASS].p[P_MS0 + k] = (int16_t)((k * 53u + 10u) % 128u);
        page(T_BASS, "MSEQ"); caption(q, "MODULATION", "MSEQ", "a 16-step modulation sequencer|here on the bass's cutoff", 0);
    }
    if (q == B(36, 0)) { page(T_PAD, "LFO"); caption(q, "MODULATION", "FREE LFO", "synced or free running|its own rate, as the delay", 0); }
    /* the effects */
    if (q == B(38, 0)) { song.g[G_RTYPE] = 3; trk[T_LEAD].p[P_REV] = 100; page(T_LEAD, "REVERB"); caption(q, "EFFECTS", "FIVE REVERBS", "ROOM · SPRING · SHIMMER|RESO tuned to the scale · CLOUD", 0); }
    if (q == B(39, 0)) song.g[G_RTYPE] = 4;
    if (q == B(40, 0)) { song.g[G_RTYPE] = 0; song.g[G_DPIT] = 7; page(T_LEAD, "DLY 2"); caption(q, "EFFECTS", "GRAIN DELAY", "repeats pitched ±12 semitones|a spray of grains", 0); }
    if (q == B(42, 0)) { song.g[G_DPIT] = 0; trk[T_BASS].p[P_DIST] = 40; page(T_BASS, "DIST"); caption(q, "EFFECTS", "FIVE DISTORTIONS", "SOFT · HARD · FOLD · CRUSH · RECT|on every track", 0); }
    if (q >= B(42, 0) && q < B(44, 0) && q % 2u == 0u) trk[T_BASS].p[P_DTYPE] = (int16_t)((q - B(42, 0)) / 2u % 5u);
    if (q == B(44, 0)) { trk[T_BASS].p[P_DIST] = 0; song.g[G_CSTER] = 60; page(T_ARP, "CHORUS"); caption(q, "EFFECTS", "STEREO CHORUS", "from mono to twice as wide|and AUTOPAN on every track", 0); }
    /* performance */
    if (q == B(46, 0)) {
        section(0); drums(DRIVE); home(T_LEAD); hold_btn(B_FX, 1);
        caption(q, "PERFORMANCE", "PUNCH-IN FX", "hold FX, let go: the screen stays|the keys play the effects", 0);
    }
    if (q == B(46, 2)) hold_btn(B_FX, 0);            /* (held past HOLD, nothing used: locked open) */
    if (bar == 47u) { perf_press(PF_R16, q % 4u == 0u); perf_press(PF_OCTU, q % 4u == 1u); perf_press(PF_S16, q % 4u >= 2u); }
    if (bar == 48u) { perf_press(PF_S16, 0); perf_press(PF_SHUF, q % 4u < 3u); perf_press(PF_R8, q % 4u == 3u); }
    if (q == B(49, 0)) { perf_press(PF_SHUF, 0); perf_press(PF_R8, 0); hold_btn(B_FX, 1); }   /* FX again: unlocked */
    if (q == B(49, 1)) hold_btn(B_FX, 0);
    if (q == B(50, 0)) { hold_btn(B_LFO, 1); caption(q, "PERFORMANCE", "MACROS", "4 macros over what's playing|their dice roll new routes", 0); }
    if (q >= B(50, 0) && q < B(51, 0)) for (k = 0; k < 4u; k++) macro_v[k] = (uint8_t)clamp((int32_t)((q - B(50, 0) + 1u) * (10u + 4u * k)), 0, 127);
    if (q == B(50, 2)) { macro_dice(1); ui_message("MACROS DICED"); }
    if (q == B(51, 0)) { hold_btn(B_LFO, 0); for (k = 0; k < 4u; k++) macro_v[k] = 0; }
    if (q == B(51, 1)) hold_btn(B_LFO, 1);
    if (q == B(51, 2)) hold_btn(B_LFO, 0);
    if (q == B(52, 0)) { song.g[G_CLIP] = 40; song.g[G_PUNCH] = 70; song.g[G_DUCK] = 30; page(T_DRUM, "MASTER"); caption(q, "PERFORMANCE", "MASTER", "clipper · drum punch|sidechain duck · leveler", 0); }
    /* new in this version */
    if (q == B(54, 0)) {
        project_capture(&proj_scratch); proj_pack(&film_song, &proj_scratch);
        page(T_LEAD, "EDIT 1"); hold_btn(B_EDIT, 1); enc_turn(EN_SELECT, 1);
        caption(q, "NEW", "SOUND DICE · OK / CANCEL", "EDIT + SELECT: a new sound, heard at once|CANCEL brings yours back", 0);
    }
    if (q == B(54, 1)) hold_btn(B_EDIT, 0);
    if (q == B(55, 0)) enc_turn(EN_SELECT, 1);         /* again */
    if (q == B(55, 2)) hold_btn(B_OCTDN, 1);           /* CANCEL */
    if (q == B(55, 3)) hold_btn(B_OCTDN, 0);
    if (q == B(56, 0)) { home(T_LEAD); caption(q, "NEW", "SELECT: EVERY PAGE", "turn it alone: page after page|the page's name below for a moment", 0); }
    if (q > B(56, 0) && q < B(58, 0)) enc_turn(EN_SELECT, 1);
    if (q == B(58, 0)) {
        hold_btn(B_SEQ, 1); enc_turn(EN_SELECT, 1);
        caption(q, "NEW", "A BUTTON + SELECT: ITS DICE", "HOME everything · EDIT the sound|SEQ · LFO · FX: KEEP? OK or CANCEL", 0);
    }
    if (q == B(58, 1)) hold_btn(B_SEQ, 0);
    if (q == B(59, 0)) enc_turn(EN_SELECT, 1);          /* (SELECT again: another sequence) */
    if (q == B(59, 2)) hold_btn(B_OCTUP, 1);
    if (q == B(59, 3)) hold_btn(B_OCTUP, 0);
    if (q == B(60, 0)) { song.sel = T_PAD; hold_btn(B_LFO, 1); enc_turn(EN_SELECT, 1); }
    if (q == B(60, 1)) hold_btn(B_LFO, 0);
    if (q == B(60, 2)) hold_btn(B_OCTUP, 1);
    if (q == B(60, 3)) hold_btn(B_OCTUP, 0);
    if (q == B(61, 0)) { song.sel = T_ARP; hold_btn(B_FX, 1); enc_turn(EN_SELECT, 1); }
    if (q == B(61, 1)) hold_btn(B_FX, 0);
    if (q == B(61, 3)) hold_btn(B_OCTDN, 1);           /* (CANCEL: the effects as they were) */
    if (q == B(62, 0)) hold_btn(B_OCTDN, 0);
    /* the song */
    if (q == B(62, 0)) { section(1); drums(POST); go_title("MIXER"); caption(q, "SONG", "6-CHANNEL MIXER", "level · pan · send · mute|thermal meters", 0); }
    if (q == B(64, 0)) {
        chain_config.count = 5;
        chain_config.row[0] = (chain_row_t){0, 4}; chain_config.row[1] = (chain_row_t){1, 8};
        chain_config.row[2] = (chain_row_t){2, 4}; chain_config.row[3] = (chain_row_t){3, 2}; chain_config.row[4] = (chain_row_t){1, 8};
        SONG_END(&chain_config) = SONG_FOREVER;
        go_title("ARRANGE"); ui.song_row = 0;
        caption(q, "SONG", "ARRANGE", "sections in a column|bars · scenes · END x2..x8 · LOOP", 0);
    }
    if (q > B(64, 0) && q < B(66, 0)) ui.song_row = (uint8_t)((q - B(64, 0)) < 5u ? q - B(64, 0) : 6u);
    if (q == B(66, 0)) {
        project_capture(&proj_scratch); proj_pack(&film_song, &proj_scratch);
        home(T_DRUM); hold_btn(B_HOME, 1); enc_turn(EN_SELECT, 1);
        caption(q, "SONG", "DICE ALL", "HOME + SELECT: a whole new piece|15 genres · SPACE to FRANTIC", 0);
    }
    if (q == B(66, 1)) hold_btn(B_HOME, 0);
    if (q == B(66, 2)) { dice_force_style = 4; dice_force_energy = 1; hold_btn(B_OCTUP, 1); }   /* DARK ELECTRO */
    if (q == B(66, 3)) hold_btn(B_OCTUP, 0);
    if (q == B(67, 0)) song.g[G_BPM] = BPM;
    if (q == B(68, 0)) {
        dice_force_style = dice_force_energy = -1;
        if (proj_import(&proj_scratch, &film_song, sizeof film_song) && !project_restore_runtime(&proj_scratch))
            transport_req = 1;
        song.g[G_BPM] = BPM; ui_message("UNDONE (AGAIN: REDO)");
        caption(q, "SONG", "UNDO", "GLO + G4: back one step|again: forward", 0);
    }
    if (q == B(70, 0)) {
        band(1); home(T_LEAD);
        caption(q, "", "ONE SMALL SCREEN", "DRUM-X|STEREO IMAGE|WAVE MORPH|FM6|MSEQ|FIVE REVERBS|MASTER|ARRANGE", 2);
    }
    if (q == B(74, 0)) caption(q, "", "", "", 0);
    if (q == B(78, 0)) { mute(T_DRUM, 1); mute(T_BASS, 1); mute(T_ARP, 1); }
    if (q == B(80, 0)) mute(T_LEAD, 1);
}

/* ---------------------------------------------------------------- the sequencing film */
static void seq_beat(uint32_t q)
{
    uint32_t bar = q / 4u, k;
    (void)bar;
    if (q == B(0, 0)) { mute(T_PAD, 0); mute(T_CHOIR, 0); }
    if (q == B(2, 0)) {
        mute(T_BASS, 0); mute(T_LEAD, 0); page(T_LEAD, "STEP");
        caption(q, "SEQUENCER", "PIANO ROLL", "64 steps · up to 4 notes a step|accents · slides · ties", 0);
    }
    if (q == B(4, 0)) {
        mute(T_DRUM, 0); mute(T_ARP, 0);
        for (k = 0; k < 64u; k += 4u) if (trk[T_ARP].step[k].n) step_set_chance(&trk[T_ARP].step[k], 60);
        trk[T_ARP].step[6].flags |= SF_RATCH; page(T_ARP, "CHANCE");
        caption(q, "SEQUENCER", "CHANCE · RATCHETS", "a chance on every step|ratchets x2 · x3 · x4", 0);
    }
    if (q == B(6, 0)) { trk[T_DRUM].p[P_SSWING] = 60; page(T_DRUM, "PATTERN"); caption(q, "SEQUENCER", "PATTERN", "length · division · swing · gate|each track its own", 0); }
    /* step components */
    if (q == B(8, 0)) {
        page(T_PAD, "COMP"); comp(T_PAD, 0, CP_ARPN, CP_ARG(0, 2, 1)); comp(T_PAD, 16, CP_ARPN, CP_ARG(2, 2, 1));
        caption(q, "STEP COMPONENTS", "ARP NOTES", "a step plays an arpeggio of its own chord|order · rate · range", 0);
    }
    if (q == B(10, 0)) {
        comp(T_PAD, 32, CP_ARPS, CP_ARG(0, 3, 2)); comp(T_PAD, 48, CP_ARPQ, CP_ARG(3, 2, 0));
        caption(q, "STEP COMPONENTS", "ARP SEQ · ARP SCALE", "through the pattern's notes|or up the scale from the step", 0);
    }
    if (q == B(12, 0)) {
        page(T_ARP, "COMP"); comp(T_ARP, 8, CP_TRIG, 0); comp(T_ARP, 24, CP_TRIG, 7); comp(T_ARP, 40, CP_TRIG, 2);
        caption(q, "STEP COMPONENTS", "TRIG", "1:2 · 3:4 · 1:3 …|a step plays on some loops only", 0);
    }
    if (q == B(14, 0)) {
        comp(T_ARP, 4, CP_RAND, 3); comp(T_ARP, 12, CP_OCT, 1); comp(T_ARP, 20, CP_TONAL, 9);
        caption(q, "STEP COMPONENTS", "RAND · OCT · TONAL", "notes that wander|octaves that jump · moves in the scale", 0);
    }
    if (q == B(16, 0)) {
        page(T_LEAD, "COMP"); comp(T_LEAD, 0, CP_BEND, 7); comp(T_LEAD, 16, CP_ECHO, 1); comp(T_LEAD, 32, CP_PULSE, 2);
        caption(q, "STEP COMPONENTS", "BEND · ECHO · PULSE", "a bend into the note|echoes · pulses in its length", 0);
    }
    if (q == B(18, 0)) {
        page(T_BASS, "COMP"); comp(T_BASS, 62, CP_REV, 2); comp(T_BASS, 30, CP_JUMP, 16u | (1u << 6));
        caption(q, "STEP COMPONENTS", "JUMP · REV with TIMES", "ALWAYS · x1 · x2 · 1:2|a phrase twice, then on", 0);
    }
    if (q == B(20, 0)) {
        page(T_DRUM, "COMP"); comp(T_DRUM, 15, CP_KILL, 0); comp(T_DRUM, 12, CP_FILL, 0);
        caption(q, "STEP COMPONENTS", "FILL · KILL", "a fill when you hold it|a step that stops the rest", 0);
    }
    if (q == B(22, 0)) {
        trk[T_ARP].p[P_CLEN] = 12; page(T_ARP, "SHIFT");
        caption(q, "SEQUENCER", "COMPONENT LANE", "C.LEN: the components on a length of their own|polymeters from one pattern", 0);
    }
    if (q == B(24, 0)) { caption(q, "SEQUENCER", "SHIFT", "slide the sequence in time and in the scale|automatable", 0); }
    if (q >= B(24, 0) && q < B(26, 0)) { trk[T_ARP].p[P_SOFS] = (int16_t)((q - B(24, 0)) % 4u); trk[T_ARP].p[P_POFS] = (int16_t)((q - B(24, 0)) / 2u % 3u); }
    if (q == B(26, 0)) {
        trk[T_ARP].p[P_SOFS] = 0; trk[T_ARP].p[P_POFS] = 0; page(T_PAD, "FILTER"); song.rec = 1u << T_PAD;
        caption(q, "SEQUENCER", "AUTOMATION", "knobs recorded into the steps while it plays|parameter locks on any step", 0);
    }
    if (q > B(26, 0) && q < B(27, 0)) enc_turn(EN_K2, 6);
    if (q >= B(27, 0) && q < B(27, 3)) enc_turn(EN_K2, -5);
    if (q == B(27, 3)) { song.rec = 0; page(T_PAD, "AUTOMATION"); }
    if (q == B(28, 0)) { section(1); trk[T_CHOIR].p[P_AMODE] = 1; page(T_CHOIR, "ARP"); caption(q, "SEQUENCER", "ARP", "modes · octaves · rate|and its own transpose", 0); }
    if (q == B(30, 0)) {
        trk[T_CHOIR].p[P_AMODE] = 0;
        trk[T_BASS].p[P_M2SRC] = MS_STEP; trk[T_BASS].p[P_M2DST] = MD_CUT; trk[T_BASS].p[P_M2AMT] = 26;
        trk[T_BASS].p[P_MSLEN] = 8; trk[T_BASS].p[P_MSDIV] = 3;
        for (k = 0; k < 16u; k++) trk[T_BASS].p[P_MS0 + k] = (int16_t)((k * 53u + 10u) % 128u);
        page(T_BASS, "MSEQ"); caption(q, "SEQUENCER", "MSEQ", "a modulation sequencer on every track|steps to any destination", 0);
    }
    /* MUTATE, FUGUE */
    if (q == B(32, 0)) {
        trk[T_ARP].p[P_MSND] = 2; trk[T_ARP].p[P_MPAT] = 1; trk[T_ARP].p[P_MAMT] = 70; trk[T_ARP].p[P_MMODE] = 0;
        page(T_ARP, "MUTATE");
        caption(q, "EVOLUTION", "MUTATE", "every N bars the sound or the pattern mutates|ORBIT around it · DRIFT away", 0);
    }
    if (q == B(34, 0)) trk[T_ARP].p[P_MMODE] = 1;
    if (q == B(36, 0)) {
        trk[T_ARP].p[P_MSND] = 0; trk[T_ARP].p[P_MPAT] = 0;
        fg_set[T_LEAD][0] = (fg_set_t){7, FG_FWD, 4}; fg_set[T_LEAD][1] = (fg_set_t){3, FG_REV, -3};
        page(T_LEAD, "PLAYHEADS");
        caption(q, "EVOLUTION", "FUGUE", "three more playheads on a track|RATE · DIRECTION · INTERVAL in the scale", 0);
    }
    if (q == B(38, 0)) { fg_set[T_LEAD][2] = (fg_set_t){5, FG_PING, 7}; caption(q, "EVOLUTION", "CANONS FROM ONE LINE", "x2 forward a fifth up · 1/2 backwards|ping-pong an octave up", 0); }
    if (q == B(40, 0)) {
        memset(fg_set, 0, sizeof fg_set); section(0); page(T_LEAD, "SCL");
        caption(q, "EVOLUTION", "ONE SCALE", "every track, the components, FUGUE|the RESO reverb follow it", 0);
    }
    /* the details */
    if (q == B(42, 0)) { drums(DRIVE); page(T_DRUM, "EDIT 1"); caption(q, "DETAILS", "DRUM-X MORPH", "two kits in one|MORPH glides from A to B", 0); }
    if (q >= B(42, 0) && q < B(44, 0)) trk[T_DRUM].p[P_E0] = (int16_t)(20 + (q - B(42, 0)) * 12u);
    if (q == B(44, 0)) {
        trk[T_DRUM].p[P_E0] = 30; dx_ui_lane = 1; dx_snd[1] = (uint8_t)(3u << 6 | 63u); page(T_DRUM, "SOUND 4");
        caption(q, "DETAILS", "DRUM SENDS", "each drum voice to CHORUS · DELAY · REVERB|its own amount: the snare to the hall", 0);
    }
    if (q == B(45, 0)) { dx_ui_lane = 3; dx_snd[3] = (uint8_t)(2u << 6 | 40u); }
    if (q == B(46, 0)) { dx_snd[1] = (uint8_t)(3u << 6 | 40u); page(T_DRUM, "STEREO"); caption(q, "DETAILS", "STEREO IMAGE", "one knob spreads the kit|the kick stays in the middle", 0); }
    if (q >= B(46, 0) && q < B(48, 0)) trk[T_DRUM].p[P_DXIMG] = (int16_t)(10 + (q - B(46, 0)) * 14u);
    if (q == B(48, 0)) { page(T_LEAD, "MORPH"); caption(q, "DETAILS", "MORPH ON THE ENGINES", "ANALOG · PHASE · LOFI|one knob through every wave", 0); }
    if (q >= B(48, 0) && q < B(50, 0)) trk[T_LEAD].p[P_MORPH] = (int16_t)((q - B(48, 0)) * 15u);
    if (q == B(50, 0)) {
        trk[T_LEAD].p[P_MORPH] = 0; home(T_LEAD); hold_btn(B_FX, 1); song.rec = 1u << T_LEAD;
        caption(q, "DETAILS", "PUNCH-IN LANE", "effects recorded into the section|played back with it", 0);
    }
    if (q == B(50, 2)) hold_btn(B_FX, 0);
    if (q >= B(51, 0) && q < B(52, 0)) { perf_press(PF_R16, q % 2u == 0u); perf_press(PF_OCTU, q % 2u == 1u); }
    if (q == B(52, 0)) { perf_press(PF_R16, 0); perf_press(PF_OCTU, 0); hold_btn(B_FX, 1); song.rec = 0; }
    if (q == B(52, 1)) hold_btn(B_FX, 0);
    if (q == B(53, 0)) {
        chain_config.count = 5;
        chain_config.row[0] = (chain_row_t){0, 4}; chain_config.row[1] = (chain_row_t){1, 8};
        chain_config.row[2] = (chain_row_t){2, 4}; chain_config.row[3] = (chain_row_t){3, 2}; chain_config.row[4] = (chain_row_t){1, 8};
        SONG_END(&chain_config) = 2;
        go_title("ARRANGE"); ui.song_row = 0;
        caption(q, "SONG", "ARRANGE", "sections · bars · scenes|END x2..x8 · LOOP", 0);
    }
    if (q > B(53, 0) && q < B(55, 0)) ui.song_row = (uint8_t)((q - B(53, 0)) < 5u ? q - B(53, 0) : 5u);
    if (q == B(55, 0)) { page(T_DRUM, "STEP"); caption(q, "SEQUENCER", "SEQ TOOLS", "reverse · shift · random · cook|beat · clear the automation", 0); }
    if (q == B(55, 1)) hold_btn(B_SEQ, 1);
    if (q == B(56, 3)) hold_btn(B_SEQ, 0);
    if (q == B(57, 0)) {
        hold_btn(B_SEQ, 1); enc_turn(EN_SELECT, 1);
        caption(q, "SEQUENCER", "SEQ + SELECT", "a new sequence for the track, heard at once|KEEP? OK or CANCEL", 0);
    }
    if (q == B(57, 1)) hold_btn(B_SEQ, 0);
    if (q == B(57, 3)) hold_btn(B_OCTDN, 1);           /* (CANCEL: the line as it was) */
    if (q == B(58, 0)) {
        hold_btn(B_OCTDN, 0);
        section(1); band(1); home(T_LEAD);
        caption(q, "", "EVERY STEP ALIVE", "PIANO ROLL|TRIG|SHIFT|AUTOMATION|MUTATE|FUGUE|DRUM SENDS|STEREO IMAGE", 2);
    }
    if (q == B(62, 0)) caption(q, "", "", "", 0);
    if (q == B(66, 0)) { mute(T_DRUM, 1); mute(T_BASS, 1); mute(T_ARP, 1); }
    if (q == B(68, 0)) mute(T_LEAD, 1);
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
    const char *dir = argc > 1 ? argv[1] : "build/video5/feat";
    double secs;
    uint32_t f, total, beat_done = 0xFFFFFFFFu, nwav, k;
    film_seq = argc > 2 && !strcmp(argv[2], "seq");
    film_bars = film_seq ? 72u : 84u;
    secs = film_bars * 4u * BEAT_S;
    total = (uint32_t)(FPS * secs); nwav = (uint32_t)(FS * secs);
    snprintf(p, sizeof p, "%s/audio.wav", dir); wav = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/screen.rgb", dir); rgb = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/scenes.txt", dir); cap = fopen(p, "w");
    snprintf(p, sizeof p, "%s/frames.txt", dir); frm = fopen(p, "w");
    if (!wav || !rgb || !cap || !frm) { fprintf(stderr, "cannot write in %s\n", dir); return 1; }
    wav_hdr(wav, nwav);
    fprintf(cap, "# %u fps, %u BPM, %u bars, splash %.4f s, end %.4f s, beat %.6f s\n", FPS, BPM, film_bars,
            SPLASH_BEATS * BEAT_S, secs, BEAT_S);
    song_build();
    for (f = 0; f < total; f++) {
        double t = (double)f / FPS;
        uint32_t want = (uint32_t)((uint64_t)(f + 1u) * FS / FPS), beat = (uint32_t)(t / BEAT_S);
        if (beat != beat_done) { beat_done = beat; if (film_seq) seq_beat(beat); else feat_beat(beat); }
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
        fprintf(frm, "%d", t >= SPLASH_BEATS * BEAT_S && ui.home && !ui.menu && !ui.confirm);
        for (k = 0; k < NTRK; k++) fprintf(frm, " %d", trk[k].p[P_MUTE] ? 0 : (int)trk[k].peak);
        fprintf(frm, "\n");
        host_ticks += 1000000u / FPS; fm1_ms += 1000u / FPS;
        if (t < SPLASH_BEATS * BEAT_S) {
            if (f == 0) { spl_anim = 1; draw_splash(); }
            splash_write((uint32_t)(t * 1000.0));
        } else {
            static int first = 1;
            if (first) { ui.force = 1; first = 0; }
            ui_input();
            ui_draw();
        }
        for (k = 0; k < NTRK; k++) trk[k].peak = 0;
        put_screen();
    }
    fclose(wav); fclose(rgb); fclose(cap); fclose(frm);
    printf("%u frames, %u samples\n", total, nsamp);
    return 0;
}
