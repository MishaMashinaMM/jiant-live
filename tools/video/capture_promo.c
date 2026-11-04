/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT 0.6.68) the presentation film's footage: a dark hypnotic techno / IDM piece in F minor at 128 BPM played by
 * the real engines (a huge mono FM6 bass with a long release and a driven glide, a punk lead in octaves on a sync saw
 * through FOLD, DRUM-X techno, LOFI blips made of step components, a choir and wind for the mysteries, a CLOUD reverb
 * that drifts), the screen captured at 30 fps while the script goes through what this version is.
 * Writes audio.wav, screen.rgb, frames.txt (HOME shown, each track's peak), scenes.txt (time, category, title lines,
 * subtitle lines, kind, the specimen's data lines: what tools/video/film.html annotates on the screen; kind 3 the list
 * of what is new, 0 with no title the outro) and list.txt (the list: title, words, the second its picture is taken at).
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/video5/capture_promo tools/video/capture_promo.c -lm
 *   build/video5/capture_promo build/video5/promo */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_VERSION "v0.8.6"
#define PF_MS 900u
#define UI_NO_DRIFT 0
#include "ui_test.c"

#define FPS 30u
#define BPM 128u
#define BARS 86u
#define BEAT_S (60.0 / BPM)
#define SECS (BARS * 4u * BEAT_S)
#define SPLASH_BEATS 8u
#define B(bar, q) ((bar) * 4u + (q))
enum { T_LEAD, T_BASS, T_BLIP, T_DRUM, T_CHOIR, T_WIND };
static FILE *wav, *rgb, *cap, *frm, *lst;
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
    s->comp = s->carg = 0;
    s->probability = 0;
}
static void drums(const char *const rows[8])      /* BD SD CP CH OH TM RS CB: 'x' a hit, 'X' accented, 'r' a ratchet */
{
    static const uint8_t NOTE[8] = {36, 38, 39, 42, 46, 45, 37, 56};
    uint32_t i, l;
    for (i = 0; i < 16u; i++) {
        uint8_t n[4]; uint32_t c = 0, acc = 0, rt = 0;
        for (l = 0; l < 8u; l++)
            if (rows[l] && rows[l][i] != '.' && c < 4u) { n[c++] = NOTE[l]; acc |= rows[l][i] == 'X'; rt |= rows[l][i] == 'r'; }
        put(T_DRUM, i, c, n, c ? ST_NOTE : ST_REST, (acc ? SF_ACCENT : 0u) | (rt ? (2u << SF_RATCH_SH) : 0u));
        step_to_grid(&trk[T_DRUM].step[i]);
    }
}
/* the drums: the pulse alone, the techno body (rolling hats, a clap, the rim pushing), the IDM one (ratchets, a broken
 * kick), the mystery (metal and air only), the peak */
static const char *const PULSE[8] = {"X...X...X...X...", 0, 0, "..x...x...x...x.", 0, 0, 0, 0};
static const char *const BODY[8] = {"X...X...X...X...", 0, "....X.......X...", "x.xxx.xxx.xxx.xx", "..x...x...x...x.", 0,
                                    "...x..x....x..x.", 0};
static const char *const IDM[8] = {"X..x..X...X.x...", "...........r..rr", "....X.......X...", "xrx.xrx.xxr.x.rx",
                                   "..x.......x.....", "..........x..x.x", "r..x..x..r.x..x.", 0};
static const char *const MYST[8] = {0, 0, 0, "x...x...x...x...", "......x.......x.", 0, "...x.....x..x...", "..........x....."};
static const char *const PEAK[8] = {"X...X...X...X..x", "............x...", "....X.......X...", "xxxxxxxxxxxxxxxx",
                                    "..x...x...x...x.", "..............xx", "...x..x....x..x.", "......x.........",};

/* F minor: Fm Db Eb Cm, a bar each */
static const uint8_t ROOT[4] = {29, 25, 27, 24};
static void bass(int alt)                         /* the FM bass: big notes held into each other, mono, a glide */
{
    static const int8_t RIFF[16] = {0, -1, -1, 0, -1, -1, 12, -1, 0, -1, -1, 7, -1, 10, -1, -1};
    static const int8_t RIFF2[16] = {0, -1, 0, -1, 12, -1, -1, 0, -1, 3, -1, -1, 7, -1, 12, 10};
    uint32_t b, i;
    for (b = 0; b < 4u; b++)
        for (i = 0; i < 16u; i++) {
            int8_t r = (alt ? RIFF2 : RIFF)[i];
            uint8_t n = (uint8_t)(ROOT[b] + 12u + (r < 0 ? 0 : (uint32_t)r));
            put(T_BASS, b * 16u + i, r < 0 ? 0u : 1u, &n, r < 0 ? ST_TIE : ST_NOTE,
                (i == 0u ? SF_ACCENT : 0u) | (r == 12 || r == 10 ? SF_SLIDE : 0u));
        }
}
static void lead(int up)                          /* the punk line: 8ths in octaves, driving, falling */
{
    static const uint8_t L[32] = {72, 72, 72, 70, 68, 68, 67, 65,  68, 68, 68, 67, 65, 65, 63, 65,
                                  72, 72, 75, 73, 72, 70, 68, 67,  65, 0, 65, 0, 67, 68, 67, 65};
    uint32_t i;
    for (i = 0; i < 64u; i++) {
        uint8_t n = L[i / 2u], v[2];
        v[0] = (uint8_t)(n + (up ? 12u : 0u)); v[1] = (uint8_t)(v[0] - 12u);
        if (i % 2u || !n) put(T_LEAD, i, 0, v, i % 2u && n ? ST_TIE : ST_REST, 0);
        else put(T_LEAD, i, 2, v, ST_NOTE, i % 8u == 0u ? SF_ACCENT : 0u);
    }
}
static void choir(int dark)                       /* the choir: the chord held the bar */
{
    static const uint8_t CA[4][3] = {{65, 68, 72}, {61, 65, 68}, {63, 67, 70}, {60, 63, 67}};
    static const uint8_t CB[4][3] = {{61, 65, 72}, {61, 65, 68}, {60, 64, 67}, {60, 63, 67}};
    uint32_t b, i;
    for (b = 0; b < 4u; b++)
        for (i = 0; i < 16u; i++)
            put(T_CHOIR, b * 16u + i, i ? 0u : 3u, (dark ? CB : CA)[b], i ? (i < 15u ? ST_TIE : ST_REST) : ST_NOTE, 0);
}
static void blips(void)                           /* the IDM blips: a few notes, the components make the rest */
{
    static const uint8_t N[16] = {77, 0, 0, 80, 0, 84, 0, 0, 77, 0, 82, 0, 0, 80, 0, 75};
    uint32_t i;
    for (i = 0; i < 16u; i++) { uint8_t n = N[i]; put(T_BLIP, i, n ? 1u : 0u, &n, n ? ST_NOTE : ST_REST, 0); }
    trk[T_BLIP].step[0].comp = CP_ARPS; trk[T_BLIP].step[0].carg = CP_ARG(2, 3, 1);
    trk[T_BLIP].step[5].comp = CP_PULSE; trk[T_BLIP].step[5].carg = 2;
    trk[T_BLIP].step[8].comp = CP_RAND; trk[T_BLIP].step[8].carg = 4;
    trk[T_BLIP].step[10].comp = CP_TRIG; trk[T_BLIP].step[10].carg = 0;
    trk[T_BLIP].step[13].flags |= 2u << SF_RATCH_SH;
    trk[T_BLIP].step[15].comp = CP_OCT; trk[T_BLIP].step[15].carg = 1;
}

static void song_build(void)
{
    uint32_t k;
    ui_power_on();
    settings.palette = UI_JIANT_INDEX; palette_set(UI_JIANT_INDEX);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    song.g[G_BPM] = BPM;
    song.master_q12 = 4096;
    sound(T_LEAD, "ANALOG", "SYNC LEAD");
    sound(T_BASS, "FM6", "FM BASS");
    sound(T_BLIP, "LOFI", "FLOAT ARP");
    sound(T_DRUM, "DRUM", "DRUM-X");
    sound(T_CHOIR, "VOICE", "CHOIR AAH");
    sound(T_WIND, "NOISE", "WIND");
    for (k = 0; k < NTRK; k++) {
        track_defaults_steps(&trk[k]);
        trk[k].p[P_ROOT] = 5; trk[k].p[P_SCALE] = 2;     /* F minor */
        trk[k].p[P_SLEN] = 64;
        trk[k].p[P_MUTE] = 1;
    }
    trk[T_DRUM].p[P_SLEN] = 16; trk[T_BLIP].p[P_SLEN] = 16;
    bass(0); lead(0); choir(0); blips(); drums(PULSE);
    {   uint8_t n = 41; put(T_WIND, 0, 1, &n, ST_NOTE, 0); for (k = 1; k < 64u; k++) put(T_WIND, k, 0, &n, ST_TIE, 0); }
    /* the bass: mono, legato, a long release, driven */
    trk[T_BASS].p[P_VOICE] = V_LEGATO; trk[T_BASS].p[P_GLIDE] = 30;
    trk[T_BASS].p[P_REL] = 90; trk[T_BASS].p[P_SUS] = 110;
    trk[T_BASS].p[P_DTYPE] = 0; trk[T_BASS].p[P_DIST] = 34; trk[T_BASS].p[P_LEVEL] = 106;
    trk[T_BASS].p[P_FTYPE] = 0; trk[T_BASS].p[P_FCUT] = 84; trk[T_BASS].p[P_FRES] = 30;
    /* the lead: sync saw through FOLD, delay and reverb */
    trk[T_LEAD].p[P_DTYPE] = 2; trk[T_LEAD].p[P_DIST] = 40; trk[T_LEAD].p[P_LEVEL] = 102;
    trk[T_LEAD].p[P_DLY] = 50; trk[T_LEAD].p[P_REV] = 60; trk[T_LEAD].p[P_PAN] = 8; trk[T_LEAD].p[P_VOICE] = V_POLY;
    {   track_t *t = &trk[T_LEAD];                       /* (sound design) the punk lead: a release, chorus, the LFO
                                                          * on the filter, the envelope biting it, a short glide */
        t->p[P_REL] = 52; t->p[P_GLIDE] = 12; t->p[P_CHOR] = 56; t->p[P_FTYPE] = 0; t->p[P_FCUT] = 92; t->p[P_FRES] = 40;
        t->p[P_ED_FLT] = 22; t->p[P_LRATE] = 30; t->p[P_LD_FLT] = 22; t->p[P_LD_PIT] = 2; }
    trk[T_BASS].p[P_ED_FLT] = 30; trk[T_BASS].p[P_LRATE] = 6; trk[T_BASS].p[P_LD_FLT] = 10;   /* (the bass breathing) */
    trk[T_BLIP].p[P_REL] = 40; trk[T_BLIP].p[P_CHOR] = 50; trk[T_BLIP].p[P_LRATE] = 44; trk[T_BLIP].p[P_LD_FLT] = 20;
    trk[T_CHOIR].p[P_ATK] = 60; trk[T_CHOIR].p[P_REL] = 100; trk[T_CHOIR].p[P_LRATE] = 10; trk[T_CHOIR].p[P_LD_FLT] = 16;
    trk[T_BLIP].p[P_LEVEL] = 112; trk[T_BLIP].p[P_DLY] = 80; trk[T_BLIP].p[P_REV] = 70; trk[T_BLIP].p[P_PAN] = -24;
    trk[T_CHOIR].p[P_LEVEL] = 66; trk[T_CHOIR].p[P_REV] = 120; trk[T_CHOIR].p[P_CHOR] = 60;
    trk[T_WIND].p[P_LEVEL] = 52; trk[T_WIND].p[P_REV] = 110;
    trk[T_DRUM].p[P_LEVEL] = 104; trk[T_DRUM].p[P_E0] = 24; trk[T_DRUM].p[P_REV] = 20;
    trk[T_DRUM].p[P_LN3] = 64; trk[T_DRUM].p[P_LN4] = 58; trk[T_DRUM].p[P_LN6] = 70;
    dx_snd[2] = (uint8_t)(3u << 6 | 44u);                /* the clap into the reverb */
    dx_snd[6] = (uint8_t)(2u << 6 | 30u);                /* the rim into the delay */
    /* the reverb: CLOUD, big, drifting (the chorus on it); the delay dotted */
    song.g[G_RTYPE] = 4; song.g[G_RSIZE] = 112; song.g[G_DTIME] = 3; song.g[G_DFDBK] = 55;
    song.g[G_CDEPTH] = 100; song.g[G_CRATE] = 20;
    song.g[G_CLIP] = 30; song.g[G_PUNCH] = 60;
    chain_config.count = 5;
    chain_config.row[0] = (chain_row_t){0, 4}; chain_config.row[1] = (chain_row_t){1, 8};
    chain_config.row[2] = (chain_row_t){2, 4}; chain_config.row[3] = (chain_row_t){3, 2}; chain_config.row[4] = (chain_row_t){1, 8};
    SONG_END(&chain_config) = SONG_FOREVER;
    for (k = 0; k < 4u; k++) project_save(k);
    ui.msg_t = 0; ui.msg[0] = ui.msg2[0] = 0;
    transport_req = 1;
}

static void caption(uint32_t q, const char *tag, const char *title, const char *sub, int kind, const char *data)
{
    fprintf(cap, "%.4f\t%s\t%s\t%s\t%d\t%s\n", q * BEAT_S, tag, title, sub, kind, data);
}
static void item(const char *title, const char *words, uint32_t q)   /* the list: its picture taken at beat q + 1 */
{
    fprintf(lst, "%s\t%s\t%.4f\n", title, words, (q + 1u) * BEAT_S + 0.3);
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
static void slicer(uint32_t mode, uint32_t mask)
{
    uint32_t k;
    song.g[G_SLMODE] = (int16_t)mode; song.g[G_SLPAT] = 9; song.g[G_SLRATE] = 1; song.g[G_SLDEP] = 127;
    for (k = 0; k <= NTRK; k++) song.g[G_SLT1 + k] = (int16_t)((mask >> k) & 1u);
}

static void at_beat(uint32_t q)
{
    uint32_t bar = q / 4u, k;
    /* the mix's arc: the bass's filter opens, the drive grows over the peak */
    if (q >= B(4, 0) && q < B(20, 0)) trk[T_BASS].p[P_FCUT] = (int16_t)(60 + (q - B(4, 0)) * 40u / 64u);
    if (q == B(0, 0)) { mute(T_CHOIR, 0); mute(T_WIND, 0); mute(T_BASS, 0); trk[T_BASS].p[P_FCUT] = 56; }
    /* ---- intro: the organisms */
    if (q == B(4, 0)) {
        home(T_BASS);
        caption(q, "SPECIMEN", "JIANT FM|v0.6.68", "a custom firmware for the M-Vave FM1|six tracks living in one body", 0,
                "T1 · SYNC LEAD|T2 · FM6 BASS|T3 · LOFI BLIPS|T4 · DRUM-X|T5 · CHOIR|T6 · WIND");
    }
    if (q == B(8, 0)) {
        mute(T_DRUM, 0); drums(PULSE);
        caption(q, "SPECIMEN", "AUDIO-REACTIVE", "each being swells with its own track|what you see is what you hear", 0,
                "nucleus = the level|membrane = the sound's shape|grey = muted");
    }
    if (q == B(12, 0)) {
        drums(BODY); page(T_DRUM, "EDIT 1");
        caption(q, "DRUMS", "DRUM-X", "eight synthesized voices, no samples|MORPH glides from kit A to kit B", 0,
                "BD · SD · CP · CH|OH · TM · RS · CB|MORPH sweeping|per-voice SEND to the reverb");
    }
    if (q >= B(12, 0) && q < B(14, 0)) trk[T_DRUM].p[P_E0] = (int16_t)(24 + (q - B(12, 0)) * 6u);
    if (q == B(14, 0)) {
        page(T_DRUM, "STEREO");
        caption(q, "DRUMS", "STEREO IMAGE", "one knob places the whole kit|the kick stays in the middle", 0,
                "IMAGE 0 → 120|each voice on its own orbit|kick: a quarter as far");
    }
    if (q >= B(14, 0) && q < B(16, 0)) trk[T_DRUM].p[P_DXIMG] = (int16_t)(10 + (q - B(14, 0)) * 13u);
    if (q == B(16, 0)) {
        page(T_BASS, "OPERATOR");
        caption(q, "ENGINES", "FM6 · THE BASS", "six operators, mono and legato|a long release, a glide, a drive", 0,
                "ALG · RATIO · LEVEL|VOICE LEGATO|REL long · GLIDE 30|DIST SOFT 34");
    }
    if (q >= B(16, 0) && q < B(17, 0)) enc_turn(EN_K1, 1);
    if (q == B(18, 0)) {
        mute(T_LEAD, 0); page(T_LEAD, "DIST");
        caption(q, "ENGINES", "THE PUNK LEAD", "a sync saw in octaves|folded by the distortion", 0,
                "ANALOG · SYNC LEAD|two notes a step, an octave apart|DIST FOLD|delay · reverb");
    }
    if (q == B(20, 0)) {
        page(T_LEAD, "MORPH");
        caption(q, "ENGINES", "WAVE MORPH", "ANALOG · PHASE · LOFI|one knob through every wave", 0,
                "MORPH 0 → 120|SAW → SQR → TRI|its own page");
    }
    if (q >= B(20, 0) && q < B(22, 0)) trk[T_LEAD].p[P_MORPH] = (int16_t)((q - B(20, 0)) * 15u);
    /* ---- the sequencing */
    if (q == B(22, 0)) {
        trk[T_LEAD].p[P_MORPH] = 0; mute(T_BLIP, 0); page(T_BLIP, "COMP"); ui.cursor = 0; ui.bank = 0;
        caption(q, "SEQUENCER", "STEP COMPONENTS", "one a step, OP-Z style|the blips are five notes and their components", 0,
                "step 1 · ARP SCALE|step 6 · PULSE|step 9 · RAND|step 11 · TRIG 1:2|step 16 · OCT");
    }
    if (q == B(23, 0)) ui.cursor = 5;
    if (q == B(23, 2)) ui.cursor = 10;
    if (q == B(24, 0)) {
        trk[T_BASS].step[30].comp = CP_JUMP; trk[T_BASS].step[30].carg = (uint8_t)(16u | (1u << 6));
        page(T_BASS, "COMP"); ui.cursor = 30; ui.bank = 1;
        caption(q, "SEQUENCER", "JUMP · TIMES", "a step that sends the playhead back|ALWAYS · x1 · x2 · 1:2", 0,
                "step 31 → step 17|x1: once a pass|then on");
    }
    if (q == B(26, 0)) {
        trk[T_BLIP].p[P_MPAT] = 1; trk[T_BLIP].p[P_MAMT] = 60; page(T_BLIP, "MUTATE");
        caption(q, "SEQUENCER", "MUTATE", "the blips mutate on every bar|ORBIT around what you wrote", 0,
                "PATTERN · every bar|AMOUNT 47 %|ORBIT|a cleared track stays cleared");
    }
    if (q == B(28, 0)) {
        trk[T_BLIP].p[P_MPAT] = 0;
        fg_set[T_LEAD][0] = (fg_set_t){3, FG_REV, -3}; page(T_LEAD, "PLAYHEADS");
        caption(q, "SEQUENCER", "FUGUE", "three more playheads on the lead|rate · direction · interval in the scale", 0,
                "head 2 · 1/2 · REV · -3|counterpoint from one line");
    }
    /* ---- the IDM break */
    if (q == B(30, 0)) {
        memset(fg_set, 0, sizeof fg_set); drums(IDM); bass(1);
        slicer(2, (1u << T_DRUM) | (1u << T_BLIP));
        song.g[G_SLPLEN] = 4; song.g[G_SLP0] = 0; song.g[G_SLP0 + 1] = 12; song.g[G_SLP0 + 2] = -5; song.g[G_SLP0 + 3] = 7;
        page(T_DRUM, "SLICER");
        caption(q, "IDM", "SLICER BUS", "stutters and gates after the effects|quantized, on any track", 0,
                "drums + blips into the bus|pitch steps 0 · +12 · -5 · +7|ratchets x3 on the hats");
    }
    if (q == B(32, 0)) {
        slicer(0, 0); home(T_LEAD); hold_btn(B_FX, 1);
        caption(q, "IDM", "PUNCH-IN FX", "hold FX and let go: the screen stays|repeats, octaves, stutters while held", 0,
                "FX LOCKED|1/16 REPEAT|OCT+|STUTTER 1/16");
    }
    if (q == B(32, 2)) hold_btn(B_FX, 0);
    if (bar == 33u) { perf_press(PF_R16, q % 4u == 0u); perf_press(PF_OCTU, q % 4u == 1u); perf_press(PF_S16, q % 4u >= 2u); }
    if (bar == 34u) { perf_press(PF_S16, 0); perf_press(PF_R32, q % 2u == 0u); perf_press(PF_SHUF, q % 2u == 1u); }
    if (q == B(35, 0)) { perf_press(PF_R32, 0); perf_press(PF_SHUF, 0); perf_press(PF_R16, 0); perf_press(PF_OCTU, 0); hold_btn(B_FX, 1); }
    if (q == B(35, 1)) hold_btn(B_FX, 0);
    /* ---- the mystery */
    if (q == B(36, 0)) {
        drums(MYST); mute(T_LEAD, 1); mute(T_BLIP, 1); choir(1); bass(0); trk[T_BASS].p[P_FCUT] = 70;
        song.g[G_RSIZE] = 124; page(T_CHOIR, "REVERB");
        caption(q, "MYSTERY", "A DRIFTING REVERB", "CLOUD: a granular reverb that moves|SHIMMER · RESO tuned to the scale", 0,
                "TYPE CLOUD|SIZE 98 %|the choir inside it|the wind under it");
    }
    if (q == B(39, 0)) {
        trk[T_CHOIR].p[P_M1SRC] = MS_LFO; trk[T_CHOIR].p[P_M1DST] = MD_CUT; trk[T_CHOIR].p[P_M1AMT] = 30;
        page(T_CHOIR, "MOD");
        caption(q, "MYSTERY", "MOD MATRIX", "4 slots a track|LFO · envelopes · macros · MSEQ", 0, "SLOT 1 · LFO → CUT +30");
    }
    /* ---- the peak */
    if (q == B(42, 0)) {
        drums(PEAK); lead(1); bass(1); choir(0); mute(T_LEAD, 0); mute(T_BLIP, 0); song.g[G_RSIZE] = 112;
        trk[T_BASS].p[P_DIST] = 52; trk[T_BASS].p[P_FCUT] = 100;
        page(T_DRUM, "MASTER");
        caption(q, "PEAK", "MASTER", "a clipper, a drum punch|a sidechain duck, the leveler", 0,
                "CLIP 30|PUNCH 60|DUCK on the kick|never above 0 dB");
    }
    if (q == B(42, 1)) { song.g[G_CLIP] = 45; song.g[G_DUCK] = 35; }
    if (q == B(44, 0)) {
        page(T_DRUM, "EDIT 1");
        caption(q, "NEW", "SELECT: THE SECTION'S PAGES", "turn it alone: through EDIT, FX, SEQ …|the page's name below for a moment", 0,
                "EDIT 1 ▸ EDIT 2 ▸ KIT 3 ▸ …|it stays inside its section|on HOME: DICE EVERYTHING?");
    }
    if (q > B(44, 0) && q < B(46, 0)) enc_turn(EN_SELECT, 1);
    if (q == B(46, 0)) {
        song.sel = T_BLIP; hold_btn(B_SEQ, 1); enc_turn(EN_SELECT, 1);
        caption(q, "NEW", "A BUTTON + SELECT: ITS DICE", "HOME everything · EDIT the sound|SEQ · LFO · FX — KEEP? OK or CANCEL", 0,
                "SEQ + SELECT|a new sequence, heard at once|OCT+ OK · OCT- CANCEL");
    }
    if (q == B(46, 1)) hold_btn(B_SEQ, 0);
    if (q == B(47, 0)) enc_turn(EN_SELECT, 1);
    if (q == B(47, 2)) hold_btn(B_OCTDN, 1);           /* CANCEL: the blips as they were */
    if (q == B(47, 3)) hold_btn(B_OCTDN, 0);
    if (q == B(48, 0)) { song.sel = T_LEAD; hold_btn(B_EDIT, 1); enc_turn(EN_SELECT, 1); }
    if (q == B(48, 1)) hold_btn(B_EDIT, 0);
    if (q == B(48, 3)) hold_btn(B_OCTDN, 1);
    if (q == B(49, 0)) hold_btn(B_OCTDN, 0);
    if (q == B(50, 0)) {
        go_title("SYSTEM");
        caption(q, "SYSTEM", "TRACKS · LOAD", "4 or 6 tracks|FULL · LITE · ECO: you pick the weight", 0,
                "FULL: every reverb, the beings at full rate|LITE: lighter reverbs, ~20 fps|ECO: no slicer, 4 voices, ~14 fps");
    }
    if (q == B(51, 0) || q == B(51, 2)) enc_turn(EN_K2, -1);
    if (q == B(52, 0)) enc_turn(EN_K2, 2);
    if (q == B(52, 2)) {
        go_title("ARRANGE"); ui.song_row = 0;
        caption(q, "SONG", "ARRANGE", "sections in a column|bars · scenes · END x2..x8 · LOOP", 0,
                "A 4 · B 8 · C 4 · D 2 · B 8|LOOP");
    }
    if (q > B(52, 2) && q < B(54, 0)) ui.song_row = (uint8_t)((q - B(52, 2)) % 6u);
    /* ---- the list (the groove plays on), the outro */
    if (q == B(54, 0)) {
        home(T_DRUM);
        caption(q, "WHAT'S NEW", "WHAT JIANT FM GIVES YOUR FM1", "", 3, "");
        item("SIX TRACKS", "six engines at once, a being for each", B(4, 0));
        item("AUDIO-REACTIVE", "every being moves with its own sound", B(8, 0));
        item("DRUM-X", "8 synthesized voices, MORPH A to B", B(12, 0));
        item("STEREO IMAGE", "one knob spreads the whole kit", B(14, 0));
        item("FM6", "six operators, 32 classic voices", B(16, 0));
        item("DISTORTION", "SOFT HARD FOLD CRUSH RECT on every track", B(18, 0));
        item("WAVE MORPH", "ANALOG PHASE LOFI melt their waves", B(20, 0));
        item("STEP COMPONENTS", "ARP TRIG RAND PULSE ECHO BEND and more", B(22, 0));
        item("JUMP · TIMES", "phrases twice, then on", B(24, 0));
        item("MUTATE", "the track evolves on the bar", B(26, 0));
        item("FUGUE", "three extra playheads in the scale", B(28, 0));
        item("SLICER BUS", "quantized stutters and gates", B(30, 0));
        item("PUNCH-IN FX", "hold FX, the screen locks, play effects", B(32, 0));
        item("FIVE REVERBS", "ROOM SPRING SHIMMER RESO CLOUD", B(36, 0));
        item("MOD MATRIX", "LFO envelopes macros MSEQ", B(39, 0));
        item("MASTER", "clipper, drum punch, duck, leveler", B(42, 0));
        item("SELECT SCROLLS", "the section's pages on one knob", B(44, 0));
        item("DICE + KEEP?", "sound seq LFO FX dice, OK or CANCEL", B(46, 0));
        item("LOAD", "FULL LITE ECO: smooth on any set", B(50, 0));
        item("ARRANGE", "sections, scenes, a whole set", B(52, 2));
    }
    if (q == B(74, 0)) caption(q, "", "", "", 0, "");
    if (q == B(80, 0)) { mute(T_DRUM, 1); mute(T_BLIP, 1); }
    if (q == B(82, 0)) { mute(T_LEAD, 1); mute(T_BASS, 1); }
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
    const char *dir = argc > 1 ? argv[1] : "build/video5/promo";
    uint32_t f, total = (uint32_t)(FPS * SECS), beat_done = 0xFFFFFFFFu, nwav = (uint32_t)(FS * SECS), k;
    snprintf(p, sizeof p, "%s/audio.wav", dir); wav = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/screen.rgb", dir); rgb = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/scenes.txt", dir); cap = fopen(p, "w");
    snprintf(p, sizeof p, "%s/frames.txt", dir); frm = fopen(p, "w");
    snprintf(p, sizeof p, "%s/list.txt", dir); lst = fopen(p, "w");
    if (!wav || !rgb || !cap || !frm || !lst) { fprintf(stderr, "cannot write in %s\n", dir); return 1; }
    wav_hdr(wav, nwav);
    fprintf(cap, "# %u fps, %u BPM, %u bars, splash %.4f s, end %.4f s, beat %.6f s\n", FPS, BPM, BARS,
            SPLASH_BEATS * BEAT_S, SECS, BEAT_S);
    song_build();
    for (f = 0; f < total; f++) {
        double t = (double)f / FPS;
        uint32_t want = (uint32_t)((uint64_t)(f + 1u) * FS / FPS), beat = (uint32_t)(t / BEAT_S);
        if (beat != beat_done) { beat_done = beat; at_beat(beat); }
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
    fclose(wav); fclose(rgb); fclose(cap); fclose(frm); fclose(lst);
    printf("%u frames, %u samples\n", total, nsamp);
    return 0;
}
