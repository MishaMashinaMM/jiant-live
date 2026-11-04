/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT 0.6.69) the Instagram stories' footage (1080 x 1920, tools/video/stories.html): a dark, pompous synth-pop
 * song in D minor at 112 BPM, Depeche Mode's way (an octave bass in 8ths, FM brass stabs, a big choir, a melancholic
 * bell lead, stomping drums with metal and an anvil), played by the real engines, while one of two scripts runs:
 *   weird    the strange side: DICE ALL, the dice of each part with KEEP?, MUTATE, FUGUE, random components, the
 *            punch-in randoms, the macros' dice
 *   project  the project: what it is, what people say of it (comments on the YouTube video), its features, support
 * Writes audio.wav, screen.rgb, frames.txt and scenes.txt as capture_promo.c (kind 4: a quote, its lines the title).
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/video5/capture_story tools/video/capture_story.c -lm
 *   build/video5/capture_story build/video5/story_weird weird;  ... build/video5/story_project project */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_VERSION "v0.8.6"
#define PF_MS 900u
#define UI_NO_DRIFT 0
#include "ui_test.c"

#define FPS 30u
#define BPM 112u
#define BARS film_bars
#define BEAT_S (60.0 / BPM)
#define SECS (BARS * 4u * BEAT_S)
#define SPLASH_BEATS 8u
#define B(bar, q) ((bar) * 4u + (q))
enum { T_LEAD, T_BASS, T_BRASS, T_DRUM, T_CHOIR, T_ANVIL };
static FILE *wav, *rgb, *cap, *frm, *lst;
static uint32_t nsamp;
static uint32_t film_bars = 24u;
static int weird, es, yt;                              /* (es: the project's quotes translated) */
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
/* the drums: a stomp (the kick on one and three, a push before three), a huge snare on two and four, 16th hats, metal
 * on the offbeats */
static const char *const STOMP[8] = {"X.......X.x.....", "....X.......X...", "....x.......x...", "x.x.x.x.x.x.x.x.",
                                     "..............x.", 0, "......x.......x.", "...x.......x...."};
static const char *const STOMP2[8] = {"X.......X.x...x.", "....X.......X.xx", "....x.......x...", "xxx.xxx.xxx.xxx.",
                                      "..x...x...x...x.", "............x.x.", "......x.......x.", "...x.......x...."};
/* D minor: Dm Bb Gm A, a bar each */
static const uint8_t ROOT[4] = {38, 34, 31, 33};
static const uint8_t CHORD[4][4] = {{50, 53, 57, 62}, {46, 50, 53, 58}, {43, 50, 55, 58}, {45, 49, 52, 57}};
static void bass(void)                            /* the octave bass in 8ths, the accent on the bar's one */
{
    uint32_t b, i;
    for (b = 0; b < 4u; b++)
        for (i = 0; i < 16u; i++) {
            uint8_t n = (uint8_t)(ROOT[b] + ((i / 2u) % 2u ? 12u : 0u));
            put(T_BASS, b * 16u + i, i % 2u ? 0u : 1u, &n, i % 2u ? ST_REST : ST_NOTE, i == 0u ? SF_ACCENT : 0u);
        }
}
static void brass(void)                           /* the stabs: on the one and the and of two, short */
{
    uint32_t b, i;
    for (b = 0; b < 4u; b++)
        for (i = 0; i < 16u; i++) {
            int on = i == 0u || i == 6u || (b == 3u && i == 12u);
            put(T_BRASS, b * 16u + i, on ? 3u : 0u, &CHORD[b][1], on ? ST_NOTE : (i == 1u || i == 7u ? ST_TIE : ST_REST),
                i == 0u ? SF_ACCENT : 0u);
        }
}
static void choir(void)
{
    uint32_t b, i;
    for (b = 0; b < 4u; b++)
        for (i = 0; i < 16u; i++) {
            uint8_t v[3] = {(uint8_t)(CHORD[b][1] + 12u), (uint8_t)(CHORD[b][2] + 12u), (uint8_t)(CHORD[b][3] + 12u)};
            put(T_CHOIR, b * 16u + i, i ? 0u : 3u, v, i ? (i < 15u ? ST_TIE : ST_REST) : ST_NOTE, 0);
        }
}
static void lead(void)                            /* the melancholic line */
{
    static const uint8_t L[][2] = {{69, 4}, {69, 2}, {67, 2}, {65, 4}, {64, 4},
                                   {74, 6}, {72, 2}, {70, 4}, {69, 4},
                                   {67, 4}, {70, 4}, {74, 4}, {72, 4},
                                   {73, 8}, {76, 4}, {69, 4}};
    uint32_t j, i, p = 0;
    for (j = 0; j < sizeof L / sizeof L[0]; j++) {
        uint8_t n = L[j][0];
        for (i = 0; i < L[j][1] && p < 64u; i++, p++) put(T_LEAD, p, i ? 0u : 1u, &n, i ? ST_TIE : ST_NOTE, 0);
    }
}
static void anvil(void)
{
    uint32_t i;
    for (i = 0; i < 64u; i++) {
        uint8_t n = 62;
        int on = i % 16u == 12u || i == 62u;
        put(T_ANVIL, i, on ? 1u : 0u, &n, on ? ST_NOTE : ST_REST, 0);
    }
}
static void song_build(void)
{
    uint32_t k;
    ui_power_on();
    settings.palette = UI_JIANT_INDEX; palette_set(UI_JIANT_INDEX);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    song.g[G_BPM] = BPM;
    song.master_q12 = 4096;
    sound(T_LEAD, "FM6", "BELL");
    sound(T_BASS, "ANALOG", "FAT BASS");
    sound(T_BRASS, "FM6", "BRASS");
    sound(T_DRUM, "DRUM", "DRUM-X");
    sound(T_CHOIR, "VOICE", "CHOIR AAH");
    sound(T_ANVIL, "NOISE", "METAL");
    for (k = 0; k < NTRK; k++) {
        track_defaults_steps(&trk[k]);
        trk[k].p[P_ROOT] = 2; trk[k].p[P_SCALE] = 2;     /* D minor */
        trk[k].p[P_SLEN] = 64;
        trk[k].p[P_MUTE] = 1;
    }
    trk[T_DRUM].p[P_SLEN] = 16;
    bass(); brass(); choir(); lead(); anvil(); drums(STOMP);
    trk[T_BASS].p[P_VOICE] = V_MONO; trk[T_BASS].p[P_LEVEL] = 104; trk[T_BASS].p[P_FCUT] = 76; trk[T_BASS].p[P_FRES] = 40;
    trk[T_BASS].p[P_DEC] = 40; trk[T_BASS].p[P_SUS] = 50; trk[T_BASS].p[P_REL] = 30; trk[T_BASS].p[P_CHOR] = 40;
    trk[T_BRASS].p[P_LEVEL] = 100; trk[T_BRASS].p[P_REV] = 70; trk[T_BRASS].p[P_SGATE] = 45; trk[T_BRASS].p[P_PAN] = -14;
    trk[T_LEAD].p[P_LEVEL] = 96; trk[T_LEAD].p[P_REV] = 90; trk[T_LEAD].p[P_DLY] = 60; trk[T_LEAD].p[P_PAN] = 12;
    trk[T_CHOIR].p[P_LEVEL] = 72; trk[T_CHOIR].p[P_REV] = 120; trk[T_CHOIR].p[P_CHOR] = 70;
    trk[T_ANVIL].p[P_LEVEL] = 116; trk[T_ANVIL].p[P_REV] = 90; trk[T_ANVIL].p[P_SGATE] = 30;
    trk[T_DRUM].p[P_LEVEL] = 108; trk[T_DRUM].p[P_E0] = 34; trk[T_DRUM].p[P_REV] = 30;
    /* sound design: every voice with its tail, its space, its movement */
    {   track_t *t = &trk[T_LEAD];                       /* the bell lead: a long release, chorus, a dotted delay, a
                                                          * cathedral; the LFO opening the filter, a hint of vibrato */
        t->p[P_ATK] = 6; t->p[P_REL] = 78; t->p[P_SUS] = 70; t->p[P_GLIDE] = 18;
        t->p[P_CHOR] = 64; t->p[P_DLY] = 62; t->p[P_REV] = 104;
        t->p[P_FTYPE] = 0; t->p[P_FCUT] = 88; t->p[P_FRES] = 34; t->p[P_ED_FLT] = 18;
        t->p[P_LRATE] = 34; t->p[P_LWAVE] = 0; t->p[P_LD_FLT] = 24; t->p[P_LD_PIT] = 2; }
    {   track_t *t = &trk[T_BRASS];                      /* the brass: a swell into the stab, a release, wide */
        t->p[P_ATK] = 10; t->p[P_REL] = 56; t->p[P_CHOR] = 70; t->p[P_DLY] = 30;
        t->p[P_FTYPE] = 0; t->p[P_FCUT] = 80; t->p[P_FRES] = 26; t->p[P_ED_FLT] = 26;
        t->p[P_LRATE] = 20; t->p[P_LD_FLT] = 14; t->p[P_APDEP] = 40; t->p[P_APRATE] = 30; }
    {   track_t *t = &trk[T_BASS];                       /* the bass: the envelope biting the filter, a slow sweep */
        t->p[P_ED_FLT] = 34; t->p[P_LRATE] = 8; t->p[P_LD_FLT] = 12; t->p[P_DIST] = 18; t->p[P_DTYPE] = 0; }
    {   track_t *t = &trk[T_CHOIR];                      /* the choir: a slow swell, a long tail, breathing */
        t->p[P_ATK] = 60; t->p[P_REL] = 96; t->p[P_LRATE] = 12; t->p[P_LD_FLT] = 18; t->p[P_LD_AMP] = 16; }
    dx_snd[1] = (uint8_t)(3u << 6 | 56u);                /* the snare into the big reverb */
    dx_snd[2] = (uint8_t)(3u << 6 | 40u);
    dx_snd[7] = (uint8_t)(2u << 6 | 30u);                /* the cowbell's metal into the delay */
    song.g[G_RTYPE] = 2; song.g[G_RSIZE] = 118; song.g[G_DTIME] = 3; song.g[G_DFDBK] = 45;   /* SHIMMER: the cathedral */
    song.g[G_CDEPTH] = 90; song.g[G_CRATE] = 25;
    song.g[G_CLIP] = 25; song.g[G_PUNCH] = 55;
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
static uint32_t tap_lbl, tap_n;                   /* a quick tap: let go a few frames later (a long OCT press shifts the octave) */
static void tap_btn(uint32_t label) { hold_btn(label, 1); tap_lbl = label; tap_n = 3; }
static void enc_turn(uint32_t role, int32_t s) { host_enc[panel.enc[role]] += s * panel.dir[role]; }
static void mute(uint32_t k, int m) { trk[k].p[P_MUTE] = (int16_t)m; }
static void slicer_unused(uint32_t mode, uint32_t mask)
{
    uint32_t k;
    song.g[G_SLMODE] = (int16_t)mode; song.g[G_SLPAT] = 9; song.g[G_SLRATE] = 1; song.g[G_SLDEP] = 127;
    for (k = 0; k <= NTRK; k++) song.g[G_SLT1 + k] = (int16_t)((mask >> k) & 1u);
}

static project_store_t film_song;
/* (the YouTube film, 16:9, tools/video/plate.html) the project, told in English over the song in full: an intro, the
 * verse, the mystery under the quotes, the choruses, a break, the end */
static const char *const BRK[8] = {0, 0, 0, "x...x...x...x...", "......x.......x.", 0, "...x.....x..x...", "..x.......x....."};
static void yt_beat(uint32_t q)
{
    if (q == B(0, 0)) { mute(T_CHOIR, 0); mute(T_BASS, 0); }
    if (q == B(2, 0)) {
        mute(T_DRUM, 0); mute(T_BRASS, 0); home(T_BASS);
        caption(q, "PROJECT", "JIANT FM", "a custom firmware for the M-Vave FM1|designed by one artist in Buenos Aires", 0,
                "hardware · M-Vave FM1|base · Felucca|author · JIANT, one person|license · free, open source");
    }
    if (q == B(6, 0)) {
        mute(T_LEAD, 0); mute(T_ANVIL, 0); page(T_BRASS, "OPERATOR");
        caption(q, "WHY IT EXISTS", "MORE INSIDE", "the FM1 is small and cheap|and holds far more than it shows", 0,
                "play whole songs|bend them live|sequence every change");
    }
    if (q == B(10, 0)) {
        drums(STOMP2); page(T_DRUM, "EDIT 1");
        caption(q, "FIELD NOTES", "THE RECEPTION", "people are installing it|and talking about it on YouTube", 0, "");
    }
    if (q == B(12, 0)) {
        drums(BRK); mute(T_BRASS, 1);
        caption(q, "FIELD NOTE 01 · YOUTUBE", "Been playing around with|Jiant and it's my|favorite so far.", "", 4, "");
    }
    if (q == B(14, 0)) caption(q, "FIELD NOTE 02 · YOUTUBE", "The level of destruction|through the FX alone|is amazing.", "", 4, "");
    if (q == B(16, 0)) caption(q, "FIELD NOTE 03 · YOUTUBE", "My FM-1 is in the mail and|I'm not sure I'm even gonna|try the default firmware|before installing this.", "", 4, "");
    if (q == B(19, 0)) {
        drums(STOMP2); mute(T_BRASS, 0); page(T_DRUM, "SOUND"); dx_ui_lane = 1;
        caption(q, "ANATOMY · A", "DRUM-X", "eight synthesized voices, no samples|MORPH from kit A to kit B", 0,
                "per-voice send to the reverb|stereo image on one knob");
    }
    if (q == B(21, 0)) {
        home(T_LEAD); hold_btn(B_FX, 1);
        caption(q, "ANATOMY · B", "PUNCH-IN FX", "hold FX and play the keys|repeats, octaves, stutters, live", 0, "the screen locks open|momentary effects");
    }
    if (q == B(21, 2)) hold_btn(B_FX, 0);
    if (q >= B(22, 0) && q < B(23, 0)) { perf_press(PF_R16, q % 2u == 0u); perf_press(PF_OCTU, q % 2u == 1u); }
    if (q == B(23, 0)) {
        perf_press(PF_R16, 0); perf_press(PF_OCTU, 0); hold_btn(B_FX, 1);
        chain_config.count = 4;
        chain_config.row[0] = (chain_row_t){0, 4}; chain_config.row[1] = (chain_row_t){1, 8};
        chain_config.row[2] = (chain_row_t){2, 4}; chain_config.row[3] = (chain_row_t){1, 8};
        caption(q, "ANATOMY · C", "SONG MODE", "sections, scenes, a whole set|played from the device", 0, "A · B · C · B|END x2..x8 · LOOP");
    }
    if (q == B(23, 1)) { hold_btn(B_FX, 0); go_title("ARRANGE"); ui.song_row = 1; }
    if (q == B(25, 0)) {
        trk[T_LEAD].step[0].comp = CP_RAND; trk[T_LEAD].step[0].carg = 4;
        trk[T_LEAD].step[16].comp = CP_ECHO; trk[T_LEAD].step[16].carg = 1;
        page(T_LEAD, "COMP"); ui.cursor = 16; ui.bank = 1;
        caption(q, "ANATOMY · D", "STEP COMPONENTS", "steps that arpeggiate, echo,|wander and jump", 0, "ARP · TRIG · RAND · ECHO|JUMP with TIMES");
    }
    if (q == B(27, 0)) {
        trk[T_BRASS].p[P_MPAT] = 1; trk[T_BRASS].p[P_MAMT] = 55; page(T_BRASS, "MUTATE");
        caption(q, "ANATOMY · E", "MUTATE · FUGUE", "tracks that evolve by themselves|playheads that make canons", 0, "ORBIT · DRIFT|three more heads in the scale");
    }
    if (q == B(29, 0)) {
        trk[T_BRASS].p[P_MPAT] = 0; home(T_LEAD);
        caption(q, "ANATOMY · F", "A LIVING SCREEN", "six beings, one for each track|everything follows the sound", 0, "nucleus = the level|membrane = the sound's shape");
    }
    if (q == B(31, 0)) {
        drums(BRK); go_title("SYSTEM");
        caption(q, "OPEN", "FREE FOR EVERYONE", "install it from the browser|no app, no account", 0,
                "juanjiant-bit.github.io/JIANT|github.com/juanjiant-bit/JIANT");
    }
    if (q == B(34, 0)) { drums(STOMP2); home(T_LEAD); caption(q, "", "", "", 0, ""); }
    if (q == B(38, 0)) { mute(T_DRUM, 1); mute(T_BRASS, 1); mute(T_ANVIL, 1); }
}
static void weird_beat(uint32_t q)
{
    uint32_t bar = q / 4u, k;
    if (q == B(0, 0)) { mute(T_CHOIR, 0); mute(T_BASS, 0); }
    if (q == B(2, 0)) {
        mute(T_DRUM, 0); mute(T_BRASS, 0); home(T_LEAD);
        caption(q, "RAREZAS", "LO RARO|DE JIANT FM", "dados, mutaciones y cabezales extra|todo adentro de un FM-1", 0,
                "6 pistas · 6 seres|todo reacciona al audio");
    }
    if (q == B(4, 0)) {
        mute(T_LEAD, 0); mute(T_ANVIL, 0);
        project_capture(&proj_scratch); proj_pack(&film_song, &proj_scratch);
        hold_btn(B_HOME, 1); enc_turn(EN_SELECT, 1);
        caption(q, "DADOS", "DICE ALL", "HOME + SELECT: un tema entero nuevo|escala, motores, partes, tempo", 0,
                "15 géneros|de SPACE a FRANTIC|GLO + G4: volvés atrás");
    }
    if (q == B(4, 1)) hold_btn(B_HOME, 0);
    if (q == B(4, 2)) { dice_force_style = 4; dice_force_energy = 2; tap_btn(B_OCTUP); }
    if (q == B(5, 0)) song.g[G_BPM] = BPM;
    if (q == B(7, 0)) {
        dice_force_style = dice_force_energy = -1;
        if (proj_import(&proj_scratch, &film_song, sizeof film_song) && !project_restore_runtime(&proj_scratch))
            transport_req = 1;
        song.g[G_BPM] = BPM; ui_message("UNDONE (AGAIN: REDO)");
        song.sel = T_LEAD; hold_btn(B_EDIT, 1); enc_turn(EN_SELECT, 1);
        caption(q, "DADOS", "UN SONIDO NUEVO", "EDIT + SELECT: lo escuchás al instante|OK lo deja · CANCEL lo devuelve", 0,
                "KEEP DICED SOUND?|SELECT otra vez: otra tirada");
    }
    if (q == B(7, 1)) hold_btn(B_EDIT, 0);
    if (q == B(8, 0)) enc_turn(EN_SELECT, 1);
    if (q == B(8, 2)) tap_btn(B_OCTDN);
    if (q == B(9, 0)) {
        song.sel = T_LEAD; hold_btn(B_SEQ, 1); enc_turn(EN_SELECT, 1);
        caption(q, "DADOS", "SECUENCIA · LFO · FX", "cada botón + SELECT tira sus dados|y siempre podés volver", 0,
                "SEQ: una melodía nueva en la escala|LFO: otra modulación|FX: otros efectos");
    }
    if (q == B(9, 1)) hold_btn(B_SEQ, 0);
    if (q == B(9, 3)) tap_btn(B_OCTDN);
    if (q == B(10, 0)) { song.sel = T_CHOIR; hold_btn(B_LFO, 1); enc_turn(EN_SELECT, 1); }
    if (q == B(10, 1)) hold_btn(B_LFO, 0);
    if (q == B(10, 2)) tap_btn(B_OCTUP);
    if (q == B(11, 0)) { song.sel = T_BRASS; hold_btn(B_FX, 1); enc_turn(EN_SELECT, 1); }
    if (q == B(11, 1)) hold_btn(B_FX, 0);
    if (q == B(11, 2)) tap_btn(B_OCTUP);
    if (q == B(12, 0)) {
        drums(STOMP2);
        trk[T_BRASS].p[P_MPAT] = 1; trk[T_BRASS].p[P_MSND] = 2; trk[T_BRASS].p[P_MAMT] = 70; trk[T_BRASS].p[P_MMODE] = 0;
        page(T_BRASS, "MUTATE");
        caption(q, "MUTACIÓN", "MUTATE", "la pista cambia sola, cada compás|ORBIT gira alrededor · DRIFT se aleja", 0,
                "SOUND cada 2 compases|PATTERN cada compás|AMOUNT 55 %");
    }
    if (q == B(14, 0)) trk[T_BRASS].p[P_MMODE] = 1;
    if (q == B(15, 0)) {
        trk[T_BRASS].p[P_MPAT] = 0; trk[T_BRASS].p[P_MSND] = 0;
        fg_set[T_LEAD][0] = (fg_set_t){7, FG_FWD, 4}; fg_set[T_LEAD][1] = (fg_set_t){3, FG_REV, -3};
        page(T_LEAD, "PLAYHEADS");
        caption(q, "CABEZALES", "FUGUE", "tres cabezales más leyendo la misma melodía|otra velocidad, otra dirección", 0,
                "x2 hacia adelante, una quinta arriba|1/2 hacia atrás|canon en la escala");
    }
    if (q == B(17, 0)) {
        memset(fg_set, 0, sizeof fg_set);
        trk[T_LEAD].step[0].comp = CP_RAND; trk[T_LEAD].step[0].carg = 4;
        trk[T_LEAD].step[16].comp = CP_ECHO; trk[T_LEAD].step[16].carg = 1;
        trk[T_LEAD].step[32].comp = CP_TRIG; trk[T_LEAD].step[32].carg = 0;
        trk[T_LEAD].step[48].comp = CP_ARPS; trk[T_LEAD].step[48].carg = CP_ARG(3, 2, 1);
        page(T_LEAD, "COMP"); ui.cursor = 0; ui.bank = 0;
        caption(q, "AZAR", "STEP COMPONENTS", "un paso que inventa notas|ecos, condiciones, arpegios", 0,
                "RAND: nota al azar|ECHO: rebota|TRIG 1:2: suena una vez cada dos");
    }
    if (q == B(18, 0)) { ui.cursor = 16; ui.bank = 1; }
    if (q == B(18, 2)) { ui.cursor = 48; ui.bank = 3; }
    if (q == B(19, 0)) {
        home(T_LEAD); hold_btn(B_FX, 1);
        caption(q, "EN VIVO", "PUNCH-IN AL AZAR", "notas al azar y arpegios|mientras tocás", 0,
                "FX mantenido: la pantalla queda|RND · ARP · REPEAT");
    }
    if (q == B(19, 2)) hold_btn(B_FX, 0);
    if (bar == 20u) { perf_press(PF_RND, q % 2u == 0u); perf_press(PF_ARP, q % 4u >= 2u); perf_press(PF_R16, q % 4u == 3u); }
    if (q == B(21, 0)) {
        perf_press(PF_RND, 0); perf_press(PF_ARP, 0); perf_press(PF_R16, 0); hold_btn(B_FX, 1);
        caption(q, "EN VIVO", "MACROS AL AZAR", "4 macros sobre lo que suena|cada tirada, rutas nuevas", 0, "M1 · M2 · M3 · M4");
    }
    if (q == B(21, 1)) { hold_btn(B_FX, 0); hold_btn(B_LFO, 1); }
    if (q == B(21, 2)) { macro_dice(1); ui_message("MACROS DICED"); }
    if (q >= B(21, 2) && q < B(22, 0)) for (k = 0; k < 4u; k++) macro_v[k] = (uint8_t)clamp((int32_t)((q - B(21, 1)) * (20u + 6u * k)), 0, 127);
    if (q == B(22, 0)) { hold_btn(B_LFO, 0); for (k = 0; k < 4u; k++) macro_v[k] = 0; caption(q, "", "", "", 0, ""); }
    if (q == B(22, 1)) home(T_LEAD);
}
static void project_beat(uint32_t q)
{
    if (q == B(0, 0)) { mute(T_CHOIR, 0); mute(T_BASS, 0); }
    if (q == B(2, 0)) {
        mute(T_DRUM, 0); mute(T_BRASS, 0); home(T_BASS);
        caption(q, "EL PROYECTO", "JIANT FM", "un firmware que diseñé|para el M-Vave FM-1", 0,
                "arte · código · sonido|un proyecto de una sola persona|gratis y open source");
    }
    if (q == B(4, 0)) {
        mute(T_LEAD, 0); mute(T_ANVIL, 0); page(T_DRUM, "EDIT 1");
        caption(q, "EN REDES", "EL RECIBIMIENTO", "lo están instalando y comentando|esto dicen en YouTube", 0, "");
    }
    if (q == B(6, 0)) caption(q, es ? "COMENTARIO EN YOUTUBE · TRADUCIDO" : "COMENTARIO EN YOUTUBE",
                              es ? "Estuve probando Jiant|y por ahora|es mi favorito." : "Been playing around with|Jiant and it's my|favorite so far.", "", 4, "");
    if (q == B(7, 2)) caption(q, es ? "COMENTARIO EN YOUTUBE · TRADUCIDO" : "COMENTARIO EN YOUTUBE",
                              es ? "El nivel de destrucción|solo con los FX|es increíble." : "The level of destruction|through the FX alone|is amazing.", "", 4, "");
    if (q == B(9, 0)) caption(q, es ? "COMENTARIO EN YOUTUBE · TRADUCIDO" : "COMENTARIO EN YOUTUBE",
                              es ? "Mi FM-1 está en camino|y no sé si voy a probar|el firmware original|antes de instalar esto." :
                                   "My FM-1 is in the mail and|I'm not sure I'm even gonna|try the default firmware|before installing this.", "", 4, "");
    if (q == B(11, 0)) { home(T_LEAD); caption(q, "QUÉ TIENE", "6 PISTAS · 6 MOTORES", "cada una con su ser|que reacciona al sonido", 0, "ANALOG · FM6 · PHASE|LOFI · VOICE · DRUM-X"); }
    if (q == B(12, 0)) { page(T_DRUM, "SOUND"); dx_ui_lane = 1; caption(q, "QUÉ TIENE", "DRUM-X", "8 voces sintetizadas|MORPH del kit A al B", 0, "sin samples|cada voz a su efecto"); }
    if (q == B(13, 0)) { page(T_LEAD, "OPERATOR"); caption(q, "QUÉ TIENE", "FM6", "seis operadores en el aparato|32 voces clásicas", 0, ""); }
    if (q == B(14, 0)) {
        trk[T_LEAD].step[0].comp = CP_RAND; trk[T_LEAD].step[0].carg = 4; trk[T_LEAD].step[16].comp = CP_ECHO; trk[T_LEAD].step[16].carg = 1;
        page(T_LEAD, "COMP"); ui.cursor = 16; ui.bank = 1;
        caption(q, "QUÉ TIENE", "STEP COMPONENTS", "pasos que arpegian, rebotan|inventan y saltan", 0, "");
    }
    if (q == B(15, 0)) { trk[T_BRASS].p[P_MPAT] = 1; trk[T_BRASS].p[P_MAMT] = 50; page(T_BRASS, "MUTATE"); caption(q, "QUÉ TIENE", "MUTATE · FUGUE", "pistas que evolucionan solas|cabezales que hacen cánones", 0, ""); }
    if (q == B(16, 0)) {
        trk[T_BRASS].p[P_MPAT] = 0; home(T_LEAD); hold_btn(B_FX, 1);
        caption(q, "QUÉ TIENE", "PUNCH-IN FX", "repeticiones, octavas, stutters|como un sampler, en vivo", 0, "");
    }
    if (q == B(16, 2)) hold_btn(B_FX, 0);
    if (q >= B(16, 3) && q < B(17, 0)) perf_press(PF_R16, 1);
    if (q == B(17, 0)) { perf_press(PF_R16, 0); hold_btn(B_FX, 1); }
    if (q == B(17, 1)) { hold_btn(B_FX, 0); song.g[G_RTYPE] = 4; page(T_CHOIR, "REVERB"); caption(q, "QUÉ TIENE", "5 REVERBS", "ROOM · SPRING · SHIMMER|RESO afinado · CLOUD granular", 0, ""); }
    if (q == B(18, 0)) {
        song.g[G_RTYPE] = 2; chain_config.count = 4;
        chain_config.row[0] = (chain_row_t){0, 4}; chain_config.row[1] = (chain_row_t){1, 8};
        chain_config.row[2] = (chain_row_t){2, 4}; chain_config.row[3] = (chain_row_t){1, 8};
        go_title("ARRANGE"); ui.song_row = 1;
        caption(q, "QUÉ TIENE", "MODO CANCIÓN", "secciones, escenas|un set entero desde el aparato", 0, "");
    }
    if (q == B(19, 0)) { home(T_LEAD); caption(q, "QUÉ TIENE", "DADOS CON OK / CANCEL", "ideas nuevas sin miedo|siempre podés volver", 0, ""); }
    if (q == B(20, 0)) caption(q, "", "", "", 0, "");
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
    const char *dir = argc > 1 ? argv[1] : "build/video5/story_weird";
    yt = argc > 2 && !strcmp(argv[2], "yt");
    if (yt) film_bars = 40u;
    weird = !yt && !(argc > 2 && (!strcmp(argv[2], "project") || !strcmp(argv[2], "project_es")));
    es = argc > 2 && !strcmp(argv[2], "project_es");
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
        if (beat != beat_done) { beat_done = beat; if (yt) yt_beat(beat); else if (weird) weird_beat(beat); else project_beat(beat); }
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
        if (tap_n && !--tap_n) hold_btn(tap_lbl, 0);
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
