/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT 0.7.2) the news film's footage (tools/video/update.html, in Spanish): the piece is the song generator's own
 * (POST PUNK, a seed, FORM CLUB: an arrangement with rows), played as a song by the real engines while the script goes
 * through what came in 0.6.99 .. 0.7.2: the generator, the presets, the DRUM-X kits, the punch-in sequencer, WALK /
 * CLIMB, the matrix on the FX buses, USB STEMS and USB LEVEL, the stems from the browser, the fixes, the JIANT names;
 * then the list, the outro with the author's words.
 * Writes audio.wav, screen.rgb, frames.txt, scenes.txt and list.txt as capture_promo.c does.
 *   cc -O2 -w -Ibuild/gen -Ifirmware/src -Itests -o build/video5/capture_update tools/video/capture_update.c -lm
 *   build/video5/capture_update build/video5/update */
#define UI_TEST_NO_MAIN 1
#define FELUCCA_VERSION "v0.8.6"
#define PF_MS 900u
#define UI_NO_DRIFT 0
#include "ui_test.c"

#define FPS 30u
#define BARS 76u
#define SPLASH_BEATS 8u
#define B(bar, q) ((bar) * 4u + (q))
static double BEAT_S = 60.0 / 130.0, SECS;              /* the generator's tempo, known after it runs */
static FILE *wav, *rgb, *cap, *frm, *lst;
static uint32_t nsamp;
static uint32_t T_DRUM_K, T_LEAD_K, T_PAD_K, T_BASS_K;  /* the roles' tracks, the generator's choice */

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
static void enc_turn(uint32_t role, int32_t s) { host_enc[panel.enc[role]] += s * panel.dir[role]; }
static void mute(uint32_t k, int m) { trk[k].p[P_MUTE] = (int16_t)m; }
static uint32_t genre_of(const char *name)
{
    uint32_t i;
    for (i = 0; i < NGENRE; i++) if (str_eq(GENRE[i].name, name)) return i;
    return 1;
}
static void menu_open(uint32_t row)                    /* MENU (HOME held) on a row */
{
    if (!ui.menu) hold(B_HOME);
    ui.menu_sel = (uint8_t)row;
    ui.force = 1;
}

static void song_build(void)
{
    uint32_t k;
    ui_power_on();
    settings.palette = UI_JIANT_INDEX; palette_set(UI_JIANT_INDEX);
    ui_rec_prefs |= PREF_SIX >> 8; rp_apply();
    go_home();
    frames(60);
    gp.genre = (uint8_t)genre_of("POST PUNK"); gp.seed = 2718; gp.energy = 64; gp.form = 2;   /* CLUB: an arrangement */
    for (k = 0; k < GP_N; k++) gp.amt[k] = GA_AUTO;
    gp.complex = 55; gp.swing = GA_AUTO; gp.human = 30;
    gen_go();
    BEAT_S = 60.0 / (double)song.g[G_BPM];
    SECS = BARS * 4u * BEAT_S;
    T_DRUM_K = T_LEAD_K = T_PAD_K = T_BASS_K = 0;
    for (k = 0; k < NTRK; k++) {
        if (trk[k].eng_req == ENGI_DRUM) T_DRUM_K = k;
        if (gs.role[k] == RL_LEAD) T_LEAD_K = k;
        if (gs.role[k] == RL_PAD) T_PAD_K = k;
        if (gs.role[k] == RL_BASS) T_BASS_K = k;
    }
    song.master_q12 = 4096;
    for (k = 0; k < 4u; k++) project_save(k);
    ui.msg_t = 0; ui.msg[0] = ui.msg2[0] = 0;
    if (chain_prepare())                               /* the song by its rows; else the live section */
        transport_req = 1;
}

static void at_beat(uint32_t q)
{
    /* ---- the words */
    if (q == B(2, 0)) {
        home(T_LEAD_K);
        caption(q, "JIANT", "HELLO EVERYONE", "JIANT here. I have been working hard so you|can have the most powerful instrument|that fits in this little synth.|I read all your suggestions, and thanks to|the first ones who started supporting|the project on Ko-fi.", 0, "");
    }
    /* ---- the generator: this very piece */
    if (q == B(8, 0)) {
        confirm_open(CF_DICE_ALL, song.sel); gen_open();
        caption(q, "GEN", "SONG|GENERATOR", "the generator made this piece: POST PUNK|opens on RANDOM, PLAY and STOP work there|HOME closes the page", 0,
                "melodies that stay in their octave|phrases that end on the chord|automated fills and punch-in|presets and FX per genre");
    }
    if (q == B(9, 0) || q == B(10, 0)) enc_turn(EN_SELECT, 1);
    /* ---- the presets */
    if (q == B(12, 0)) {
        ui.confirm = 0; ui.force = 1;
        page(T_LEAD_K, "PRESETS");
        caption(q, "SOUNDS", "46 NEW PRESETS", "JUNO PAD · REESE · HOOVER · DUB STAB · CELLO|CZ PIANO · GLASS PAD · TALK BOX · DX KEYS · SEA|on every engine, one coherent palette", 0,
                "ANALOG 10 · PHASE 8 · LOFI 5|VOICE 3 · WHEEL 5 · NOISE 4 · FM6 7|browse them with PRESETS");
    }
    if (q >= B(13, 0) && q < B(16, 0) && q % 2u == 0u) enc_turn(EN_PRESET, 1);
    /* ---- the kits */
    if (q == B(16, 0)) {
        page(T_DRUM_K, "PRESETS");
        apply_preset(1);                               /* 909 CLUB */
        caption(q, "DRUMS", "9 DRUM-X KITS", "909 CLUB · 808 BOOM · LOFI DUST · INDUSTRIAL|ACOUSTIC · DUB ROOTS · TECHNO RAW · BREAKS|a preset loads the whole kit: 8 sounds, LAYER, SEND", 0,
                "909 CLUB: kick with drive|INDUSTRIAL: FM and glitch|DUB ROOTS: wet snare, rim to the delay");
    }
    if (q == B(18, 0)) apply_preset(4);                /* INDUSTRIAL */
    if (q == B(20, 0)) apply_preset(6);                /* DUB ROOTS */
    /* ---- the punch-in lane */
    if (q == B(22, 0)) {
        uint32_t i;
        song.g[G_PFXLEN] = 16; song.g[G_PFXRES] = 1;
        for (i = 0; i < 16u; i++) pfx_lane_put(i, i % 8u == 6u || i % 8u == 7u ? 6u : i == 12u ? 2u : 0u);
        page(T_DRUM_K, "PUNCH");
        caption(q, "FX", "SEQUENCED|PUNCH-IN", "record your FX performance in real time|LEN 1..64 steps · RES 1/8 1/16 1/32 · CLEAR|the lane repeats with the song", 0,
                "REC + PLAY records|hold FX + a key|STUTTER on 7 and 8, OCT+ on 13");
    }
    /* ---- WALK / CLIMB */
    if (q == B(26, 0)) {
        uint32_t i;
        for (i = 0; i < 16u; i++) pfx_lane_put(i, 0);
        trk[T_LEAD_K].step[0].comp = CP_WALK; trk[T_LEAD_K].step[0].carg = CP_WARG(0, 0, 1);
        page(T_LEAD_K, "COMP"); ui.cursor = 0; ui.bank = 0;
        caption(q, "SEQUENCE", "WALK · CLIMB", "a note climbs one scale degree per loop|every 1..16 loops · up, down, patterns|stackable: from 4 notes to a complex pattern", 0,
                "MODE UP · RANGE 5|EVERY 1 LOOP|like the OP-Z and the OP-XY");
    }
    /* ---- the matrix on the FX buses */
    if (q == B(30, 0)) {
        trk[T_LEAD_K].step[0].comp = 0; trk[T_LEAD_K].step[0].carg = 0;
        trk[T_PAD_K].p[P_M1SRC] = MS_STEP; trk[T_PAD_K].p[P_M1DST] = MD_DTIME; trk[T_PAD_K].p[P_M1AMT] = 40;
        page(T_PAD_K, "MOD");
        caption(q, "FX", "THE MATRIX REACHES|THE EFFECTS", "D.TIM D.FB D.TON D.MIX · R.SIZ R.DMP|C.RAT C.DEP: eight new destinations|the MOD SEQ sequences the delay time", 0,
                "SLOT 1 · STEP → D.TIM +40|an LFO on R.SIZ breathes|every track adds its slot");
    }
    /* ---- USB: the stems, each part alone for a bar (the mutes: what a DAW gets on its own channel), the level */
    if (q == B(34, 0)) {
        uint32_t k;
        trk[T_PAD_K].p[P_M1SRC] = 0; trk[T_PAD_K].p[P_M1DST] = 0; trk[T_PAD_K].p[P_M1AMT] = 0;
        menu_open(MI_SERIAL); menu_put(MI_SERIAL, 2u);
        for (k = 0; k < NTRK; k++) mute(k, k != T_DRUM_K);
        caption(q, "USB", "STEMS OVER USB", "MENU > SYSTEM > USB: STEMS, and the FM1 is an|8-channel USB audio interface: the 6 tracks mono + FX|each on its own channel in Ableton, Logic, Reaper, FL", 0,
                "hear them alone: drums · bass · lead · pad|channels 1–6 the tracks, with LEVEL and MUTE|7–8 delay, reverb and chorus in stereo");
    }
    if (q == B(35, 0)) { uint32_t k; for (k = 0; k < NTRK; k++) mute(k, k != T_BASS_K); }
    if (q == B(36, 0)) { uint32_t k; for (k = 0; k < NTRK; k++) mute(k, k != T_LEAD_K); }
    if (q == B(37, 0)) { uint32_t k; for (k = 0; k < NTRK; k++) mute(k, k != T_PAD_K); }
    if (q == B(38, 0)) {
        uint32_t k;
        for (k = 0; k < NTRK; k++) mute(k, 0);
        menu_put(MI_USB, 3u); ui.menu_sel = MI_USB; ui.force = 1;
        caption(q, "USB", "USB LEVEL", "MENU > AUDIO > USB LEVEL: −6 / −12 / −18 / −24 dB|USB fixed and attenuated, whatever the MASTER knob does|no clipping on a phone or a DAW input", 0,
                "FIXED: full level, as before|the DAC keeps the knob");
    }
    if (q == B(39, 2)) {
        caption(q, "USB", "STEMS FROM|THE BROWSER", "load your backup into the web emulator|each track alone with its effects, plus the MIX|a ZIP of WAVs, faster than real time", 0,
                "juanjiant-bit.github.io/JIANT|the whole song or the section|T1..T6 + MIX, 16 bit 44.1 kHz");
    }
    /* ---- the fixes and the names */
    if (q == B(42, 0)) {
        menu_put(MI_USB, 0u); menu_put(MI_SERIAL, 0u);
        ui.menu = 0; ui.force = 1;
        page(T_PAD_K, "CHORD");
        caption(q, "FIXES", "FIXES", "CHORD no longer resets when you change presets|STRETCH also stretches the repeats|MUTATE works with every engine", 0,
                "LED DIM HI more visible|MIDI CC 16–23 → K1–K8 · 102–105|the GEN without flicker");
    }
    if (q == B(46, 0)) {
        home(T_LEAD_K);
        caption(q, "JIANT", "IT IS ALL JIANT", "USB, credits, splash, console: JIANT FM|the editor and the installer take both names|ABOUT with the project info", 0,
                "USB: JIANT FM|editor: JIANT v0.7.2|ko-fi · cafecito · youtube");
    }
    /* ---- the list, the outro */
    if (q == B(50, 0)) {
        home(T_DRUM_K);
        caption(q, "NEWS", "NEW SINCE YESTERDAY", "", 3, "");
        item("GENERATOR", "coherent melodies, presets and FX per genre", B(8, 0));
        item("46 PRESETS", "pads, reeses, stabs, keys, textures", B(12, 0));
        item("9 DRUM-X KITS", "909, 808, lo-fi, industrial, acoustic, dub…", B(16, 0));
        item("FX PUNCH-IN", "LEN 1..64, RES, CLEAR, live recording", B(22, 0));
        item("WALK · CLIMB", "components that evolve every loop", B(26, 0));
        item("MATRIX → FX", "delay, reverb and chorus as destinations", B(30, 0));
        item("STEMS OVER USB", "an 8-channel USB audio interface: 6 tracks + FX", B(34, 0));
        item("USB LEVEL", "−6 to −24 dB to record without clipping", B(38, 0));
        item("WEB STEMS", "separate WAVs from a backup, in a ZIP", B(39, 2));
        item("FIXES", "CHORD, STRETCH, MUTATE, LEDs, MIDI CC", B(42, 0));
        item("IT IS ALL JIANT", "USB, credits and console with the name", B(46, 0));
        item("HOME CLOSES GEN", "and PLAY / STOP work on that page", B(9, 0));
    }
    if (q == B(66, 0)) caption(q, "", "", "", 0, "");
    if (q == B(72, 0)) { mute(T_DRUM_K, 1); }
    if (q == B(74, 0)) { uint32_t k; for (k = 0; k < NTRK; k++) if (k != T_PAD_K) mute(k, 1); }
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
    const char *dir = argc > 1 ? argv[1] : "build/video5/update";
    uint32_t f, total, beat_done = 0xFFFFFFFFu, nwav, k;
    snprintf(p, sizeof p, "%s/audio.wav", dir); wav = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/screen.rgb", dir); rgb = fopen(p, "wb");
    snprintf(p, sizeof p, "%s/scenes.txt", dir); cap = fopen(p, "w");
    snprintf(p, sizeof p, "%s/frames.txt", dir); frm = fopen(p, "w");
    snprintf(p, sizeof p, "%s/list.txt", dir); lst = fopen(p, "w");
    if (!wav || !rgb || !cap || !frm || !lst) { fprintf(stderr, "cannot write in %s\n", dir); return 1; }
    song_build();                                       /* (the tempo: the generator's) */
    total = (uint32_t)(FPS * SECS); nwav = (uint32_t)(FS * SECS);
    wav_hdr(wav, nwav);
    fprintf(cap, "# %u fps, %d BPM, %u bars, splash %.4f s, end %.4f s, beat %.6f s\n", FPS, song.g[G_BPM], BARS,
            SPLASH_BEATS * BEAT_S, SECS, BEAT_S);
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
    printf("%u frames, %u samples, %d BPM, drums T%u lead T%u pad T%u\n", total, nsamp, song.g[G_BPM], T_DRUM_K + 1u, T_LEAD_K + 1u, T_PAD_K + 1u);
    return 0;
}
