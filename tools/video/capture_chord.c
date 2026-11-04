/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT 0.6.84) the story announcing that CHORD is back (1080 x 1920, tools/video/stories.html): a soft lo-fi song in
 * A minor at 96 BPM played by the real engines, a key at a time on the CHORD track, while the script shows what CHORD
 * does: one key one chord, always in key (TRIAD, 7TH, SUS4), VOICING LEAD, STRUM, recorded whole into the piano roll
 * and strummed back, the chords gliding into each other. Writes audio.wav, screen.rgb, frames.txt, scenes.txt as
 * capture_story.c, plus keys.txt (each frame: the key LEDs as the firmware lights them, the chord's name) and hud.txt.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/video6/capture_chord tools/video/capture_chord.c -lm
 *   build/video6/capture_chord build/video6/chord [en] */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_VERSION "v0.7.6"
#define PF_MS 900u
#define UI_NO_DRIFT 0
#include "ui_test.c"

#define FPS 30u
#define BPM 96u
#define BARS 23u
#define BEAT_S (60.0 / BPM)
#define SECS (BARS * 4u * BEAT_S)
#define SPLASH_BEATS 8u
#define B(bar, q) ((bar) * 4u + (q))
enum { T_KEYS, T_BASS, T_DRUM, T_AIR };
static FILE *wav, *rgb, *cap, *frm, *kfp;
static uint32_t nsamp;
static int en;
static char label[16];
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
    memset(s, 0, sizeof *s);
    s->n = (uint8_t)n;
    for (j = 0; j < 4u; j++) s->note[j] = j < n ? notes[j] : 0;
    s->time = (uint8_t)time;
    s->flags = (uint8_t)flags;
    s->vel = n ? 100 : 0;
}
static void drums(const char *const rows[8])      /* BD SD CP CH OH TM RS CB: 'x' a hit, 'X' accented */
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
static const char *const LOFI[8] = {"X......x..x.....", "....x.......x...", 0, "x.x.x.x.x.x.x.xx", 0, 0, "...........x....", 0};
static const char *const SOFT[8] = {"X...............", 0, 0, "....x.......x...", 0, 0, 0, 0};
/* A minor: i VI III VII (Am F C G), the keys A F C G: CHORD builds each chord from the scale */
static const uint8_t ROOT[4] = {33, 29, 36, 31};
static const uint8_t KEY[4] = {57, 53, 60, 55};
static const char *const NAME[3][4] = {{"Am", "F", "C", "G"}, {"Am7", "Fmaj7", "Cmaj7", "G7"}, {"Asus4", "Fsus4", "Csus4", "Gsus4"}};
static void bass(void)                            /* the roots in a lazy 8th pattern, a bar each */
{
    uint32_t b, i;
    for (b = 0; b < 4u; b++)
        for (i = 0; i < 16u; i++) {
            uint8_t n = (uint8_t)(ROOT[b] + (i == 10u ? 12u : 0u));
            int on = i == 0u || i == 6u || i == 10u;
            put(T_BASS, b * 16u + i, on ? 1u : 0u, &n, on ? ST_NOTE : (i == 1u || i == 7u ? ST_TIE : ST_REST), i ? 0u : SF_ACCENT);
        }
}
static void air(void)                             /* a far pad an octave up, one long note a bar */
{
    uint32_t b, i;
    for (b = 0; b < 4u; b++)
        for (i = 0; i < 16u; i++) {
            uint8_t n = (uint8_t)(KEY[b] + 24u);
            put(T_AIR, b * 16u + i, i ? 0u : 1u, &n, i ? (i < 15u ? ST_TIE : ST_REST) : ST_NOTE, 0);
        }
}
static uint32_t key_of(uint32_t note)             /* the key that plays note on the CHORD track (octave 0) */
{
    uint32_t k;
    for (k = 0; k < 27u; k++) if (kb_map(&trk[T_KEYS], k) == note) return k;
    return 0;
}
static void key(uint32_t note, int on)
{
    uint32_t b = 1u << key_of(note);
    if (on) { fm1_in.notes |= b; host_notes |= b; }
    else fm1_in.notes &= ~b;
}
static void keys_up(void) { fm1_in.notes = 0; label[0] = 0; }
static void chord(uint32_t c, uint32_t set)       /* key c of the progression down, its chord's name shown */
{
    keys_up();
    key(KEY[c & 3u], 1);
    str_cpy(label, NAME[set % 3u][c & 3u], sizeof label);
}
static void song_build(void)
{
    uint32_t k;
    ui_power_on();
    settings.palette = UI_JIANT_INDEX; palette_set(UI_JIANT_INDEX);
    song.g[G_BPM] = BPM;
    song.master_q12 = 4096;
    sound(T_KEYS, "FM6", "TINE EP");
    sound(T_BASS, "ANALOG", "SUB BASS");
    sound(T_DRUM, "DRUM", "DRUM-X");
    sound(T_AIR, "ANALOG", "SOFT PAD");
    for (k = 0; k < 4u; k++) {
        track_defaults_steps(&trk[k]);
        trk[k].p[P_ROOT] = 9; trk[k].p[P_SCALE] = 2;     /* A minor */
        trk[k].p[P_QUANT] = QN_OFF;
        trk[k].p[P_SLEN] = 64;
        trk[k].p[P_MUTE] = 1;
    }
    trk[T_KEYS].p[P_SLEN] = 32;
    trk[T_DRUM].p[P_SLEN] = 16;
    bass(); air(); drums(SOFT);
    {   track_t *t = &trk[T_KEYS];                       /* the CHORD track: an EP, POLY, chorus and a room */
        t->p[P_VOICE] = V_POLY; t->p[P_LEVEL] = 104; t->p[P_REV] = 70; t->p[P_CHOR] = 60; t->p[P_DLY] = 26;
        t->p[P_REL] = 60; t->p[P_SUS] = 110;
        t->p[P_CHRD] = CD_TRI; t->p[P_CVOI] = CV_CLOSE; t->p[P_CSTR] = 0; }
    trk[T_BASS].p[P_VOICE] = V_MONO; trk[T_BASS].p[P_LEVEL] = 100;
    trk[T_DRUM].p[P_LEVEL] = 92; trk[T_DRUM].p[P_E0] = 20; trk[T_DRUM].p[P_REV] = 26; trk[T_DRUM].p[P_DIST] = 18;
    trk[T_AIR].p[P_LEVEL] = 54; trk[T_AIR].p[P_REV] = 120; trk[T_AIR].p[P_ATK] = 70; trk[T_AIR].p[P_REL] = 100;
    song.g[G_RTYPE] = 2; song.g[G_RSIZE] = 104; song.g[G_DTIME] = 3; song.g[G_DFDBK] = 40;
    song.g[G_CLIP] = 20; song.g[G_PUNCH] = 40;
    song.sel = T_KEYS;
    ui.msg_t = 0; ui.msg[0] = ui.msg2[0] = 0;
    transport_req = 1;
}
static void caption(uint32_t q, const char *tag, const char *title, const char *sub, int kind, const char *data)
{
    fprintf(cap, "%.4f\t%s\t%s\t%s\t%d\t%s\n", q * BEAT_S, tag, title, sub, kind, data);
}
static void mute(uint32_t k, int m) { trk[k].p[P_MUTE] = (int16_t)m; }
#define S(es_, en_) (en ? (en_) : (es_))
static void beat(uint32_t q)
{
    uint32_t bar = q / 4u, i;
    if (q == B(0, 0)) { mute(T_DRUM, 0); mute(T_BASS, 0); mute(T_AIR, 0); }
    /* the progression on the keys, a bar a chord (bars 2..13), the chord's own set by the scene */
    if (bar >= 2u && bar < 14u && q % 4u == 0u) {
        uint32_t set = bar >= 6u && bar < 8u ? 1u : bar >= 8u && bar < 10u ? 2u : bar >= 10u ? 1u : 0u;
        chord(bar % 4u, set);
    }
    if (bar >= 2u && bar < 14u && q % 4u == 3u) keys_up();   /* (let go on the bar's last beat) */
    if (q == B(2, 0)) {
        mute(T_KEYS, 0); go_title("CHORD");
        caption(q, S("VOLVIÓ", "IT'S BACK"), "CHORD|MODE", S("una tecla, un acorde|como un Omnichord en el FM-1", "one key, one chord|an Omnichord in the FM-1"), 0,
                S("SCL > CHORD|liviano: casi nada de memoria", "SCL > CHORD|light: next to no memory"));
    }
    if (q == B(4, 0)) drums(LOFI);
    if (q == B(6, 0)) {
        trk[T_KEYS].p[P_CHRD] = CD_SEV;
        caption(q, S("ARMONÍA", "HARMONY"), S("SIEMPRE|EN TONO", "ALWAYS|IN KEY"),
                S("los acordes salen de la escala|nunca desafinan", "the chords come from the scale|never out of key"), 0,
                "TRIAD · 7TH · ADD9|SUS2 · SUS4|MAJ · MIN · 7 · POWER");
    }
    if (q == B(8, 0)) trk[T_KEYS].p[P_CHRD] = CD_SUS4;
    if (q == B(10, 0)) {
        trk[T_KEYS].p[P_CHRD] = CD_SEV; trk[T_KEYS].p[P_CVOI] = CV_LEAD;
        caption(q, S("VOICING", "VOICING"), "LEAD", S("cada acorde busca la inversión|más cercana al anterior", "each chord takes the inversion|nearest the last one"), 0,
                S("como el Orchid|CLOSE · LEAD · OPEN · +BASS", "as the Orchid does|CLOSE · LEAD · OPEN · +BASS"));
    }
    if (q == B(12, 0)) {
        trk[T_KEYS].p[P_CSTR] = 90;
        caption(q, "STRUM", S("COMO UN|ARPA", "LIKE A|HARP"), S("las notas entran una a una|de grave a agudo", "the notes come in one by one|low to high"), 0, "");
    }
    if (q == B(14, 0)) {                              /* recorded into the piano roll, strum and all */
        for (i = 0; i < NSTEP; i++) step_clear(&trk[T_KEYS].step[i]);
        song.rec = 1u << T_KEYS; go_title("STEP");
        caption(q, S("GRABACIÓN", "RECORDING"), S("AL PIANO|ROLL", "INTO THE|PIANO ROLL"),
                S("el acorde entero en su paso|y vuelve rasgueado", "the whole chord in its step|strummed back as played"), 0, "");
    }
    if (bar >= 14u && bar < 16u && q % 2u == 0u) chord((q - B(14, 0)) / 2u, 1u);
    if (bar >= 14u && bar < 16u && q % 2u == 1u) keys_up();
    if (q == B(16, 0)) { song.rec = 0; keys_up(); }
    if (q == B(18, 0)) {
        sound(T_KEYS, "ANALOG", "STRINGS");
        trk[T_KEYS].p[P_GLIDE] = 120; trk[T_KEYS].p[P_CSTR] = 0; trk[T_KEYS].p[P_VOICE] = V_POLY;
        trk[T_KEYS].p[P_REL] = 70; trk[T_KEYS].p[P_REV] = 90; trk[T_KEYS].p[P_LEVEL] = 96;
        drums(SOFT);
        caption(q, "GLIDE", S("ACORDES QUE|SE DESLIZAN", "CHORDS THAT|GLIDE"),
                S("con GLIDE cada acorde|se desliza al siguiente", "with GLIDE every chord|slides into the next"), 0,
                S("las notas nuevas aparecen|las teclas muestran el acorde", "new notes fade in|the keys show the chord"));
    }
    if (q == B(20, 0)) { go_home(); caption(q, "", "", "", 0, ""); }
    if (q == B(22, 0)) { mute(T_DRUM, 1); mute(T_BASS, 1); }
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
    const char *dir = argc > 1 ? argv[1] : "build/video6/chord";
    FILE *hud;
    en = argc > 2 && !strcmp(argv[2], "en");
    uint32_t f, total = (uint32_t)(FPS * SECS), beat_done = 0xFFFFFFFFu, nwav = (uint32_t)(FS * SECS), k;
    snprintf(p, sizeof p, "%s/audio.wav", dir); wav = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/screen.rgb", dir); rgb = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/scenes.txt", dir); cap = fopen(p, "w");
    snprintf(p, sizeof p, "%s/frames.txt", dir); frm = fopen(p, "w");
    snprintf(p, sizeof p, "%s/keys.txt", dir); kfp = fopen(p, "w");
    snprintf(p, sizeof p, "%s/hud.txt", dir); hud = fopen(p, "w");
    if (!wav || !rgb || !cap || !frm || !kfp || !hud) { fprintf(stderr, "cannot write in %s\n", dir); return 1; }
    fprintf(hud, "%u BPM · %s\n", BPM, en ? "A MINOR" : "LA MENOR"); fclose(hud);
    wav_hdr(wav, nwav);
    fprintf(cap, "# %u fps, %u BPM, %u bars, splash %.4f s, end %.4f s, beat %.6f s\n", FPS, BPM, BARS,
            SPLASH_BEATS * BEAT_S, SECS, BEAT_S);
    song_build();
    for (f = 0; f < total; f++) {
        double t = (double)f / FPS;
        uint32_t want = (uint32_t)((uint64_t)(f + 1u) * FS / FPS), bt = (uint32_t)(t / BEAT_S);
        if (bt != beat_done) { beat_done = bt; beat(bt); }
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
        {   uint32_t m = song.sel == T_KEYS ? key_leds(0) : 0u;   /* (the LEDs as the firmware lights them) */
            fprintf(kfp, "%u\t%s\n", m, label[0] ? label : "-"); }
        for (k = 0; k < NTRK; k++) trk[k].peak = 0;
        put_screen();
    }
    fclose(wav); fclose(rgb); fclose(cap); fclose(frm); fclose(kfp);
    printf("%u frames, %u samples\n", total, nsamp);
    return 0;
}
