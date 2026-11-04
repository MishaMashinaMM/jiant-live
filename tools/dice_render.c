/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT 0.6.80) DICE ALL heard: pieces from the generator (firmware/src/song_dice.c) on the host build of the tests,
 * the real engines playing each at its tempo, written as WAV files, to compare the generator before and after a
 * change by ear. Each argument is STYLE:ENERGY:SEED (the genre's index in GENRE (gen_tables.c), the energy 0..2, any seed: the same
 * seed, the same throw); the files are DIR/NN_<style>_e<energy>.wav, SECONDS long each (default 20). (0.6.81) A throw
 * that made parts plays its song, as PLAY in SONG mode.
 *   cc -O1 -w -Ibuild/gen -Ifirmware/src -Itests -o build/dice_render tools/dice_render.c -lm
 *   build/dice_render DIR SECONDS 6:1:11 12:1:22 ... */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"

int main(int argc, char **argv)
{
    const char *dir = argc > 1 ? argv[1] : "build/dice_render";
    double secs = argc > 2 ? atof(argv[2]) : 20.0;
    int a;
    if (secs <= 0) secs = 20.0;
    for (a = 3; a < argc; a++) {
        int st = 0, en = 1;
        unsigned seed = 1;
        char path[600], name[64];
        uint32_t nsamp = 0, want, f = 0, i;
        FILE *wav;
        if (sscanf(argv[a], "%d:%d:%u", &st, &en, &seed) < 2) { fprintf(stderr, "STYLE:ENERGY:SEED, not %s\n", argv[a]); return 1; }
        ui_power_on();
        settings.palette = UI_JIANT_INDEX; palette_set(UI_JIANT_INDEX);
        ui_rec_prefs |= PREF_SIX >> 8; rp_apply();      /* TRACKS 6 */
        song.master_q12 = 4096;
        host_ticks = seed * 7919u;
        rng_state = seed * 2654435761u | 1u;
        dice_force_style = (int8_t)st; dice_force_energy = (int8_t)en;
        dice_all();
        dice_force_style = dice_force_energy = -1;
        str_cpy(name, GENRE[(uint32_t)st % NGENRE].name, sizeof name);
        for (i = 0; name[i]; i++) if (name[i] == ' ') name[i] = '_';
        snprintf(path, sizeof path, "%s/%02d_%s_e%d_s%u.wav", dir, a - 2, name, en, seed);
        if (!(wav = fopen(path, "wb"))) { fprintf(stderr, "cannot write %s\n", path); return 1; }
        wav_hdr(wav, 0);
        if (live.mode) chain_play_ui();                 /* (0.6.81: its parts in ARRANGE: PLAY plays the song) */
        else transport_req = 1;
        want = (uint32_t)(secs * FS);
        while (nsamp < want) {                          /* a frame (1/30 s) of the main loop, then its audio */
            uint32_t to = (uint32_t)((uint64_t)(++f) * FS / 30u);
            ui_input();
            song_poll();                                /* (the main loop's: the song's next section staged) */
            while (nsamp < to && nsamp < want) {
                int32_t o[2 * CTL];
                int16_t s[2 * CTL];
                mix_block(o, CTL);
                for (i = 0; i < 2u * CTL; i++) s[i] = (int16_t)clamp(o[i], -32768, 32767);
                fwrite(s, 2, 2u * CTL, wav);
                nsamp += CTL;
            }
            host_ticks += 1000000u / 30u; fm1_ms += 1000u / 30u;
        }
        transport_req = 2; ui_input();
        fclose(wav);
        if ((wav = fopen(path, "r+b"))) { wav_hdr(wav, nsamp); fclose(wav); }
        printf("%s  %s energy %d, %d BPM\n", path, GENRE[(uint32_t)st % NGENRE].name, en, song.g[G_BPM]);
    }
    return 0;
}
