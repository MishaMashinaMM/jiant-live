/* SPDX-License-Identifier: GPL-3.0-only */
/* Receive routing without display dependencies: real queues, voices and ownership. */
#define main hostsim_main
#include "hostsim.c"
#undef main
#include <assert.h>

static void reset_rx(void)
{
    memset(trk, 0, sizeof trk); memset(&song, 0, sizeof song);
    memset(midi_ch, 0, sizeof midi_ch); memset(midi_sel_on, 0, sizeof midi_sel_on);
    memset(midi_owners, 0, sizeof midi_owners); memset(kb_on, 0, sizeof kb_on);
    memset(dxo_n, 0, sizeof dxo_n);
    memset(ml_table, 0, sizeof ml_table);
    host_tracks_init();
    for (uint32_t i = 0; i < NTRK; i++) {
        midi_rx[i] = (uint8_t)(i + 1u);
        trk[i].p[P_SUS] = 127;
        trk[i].p[P_VOICE] = V_POLY;
        trk[i].p[P_AMODE] = 0;
    }
    song.sel = 0; fm1_in.notes = kb_prev = 0;
    panic_req = transport_req = 0; mi_r = mi_w = midi_in_overflow = 0;
    midi_route_setup = 0;
    events_block(CTL);
}
static void send_rx(uint32_t st, uint32_t a, uint32_t b, uint32_t source)
{
    midi_enqueue((st >> 4) | st << 8 | a << 16 | b << 24, source);
    events_block(CTL);
}
static int gate_rx(uint32_t t, uint32_t note)
{
    for (uint32_t i = 0; i < NVOICE; i++)
        if (trk[t].v[i].active && trk[t].v[i].gate && trk[t].v[i].note == note) return 1;
    return 0;
}
int main(void)
{
    reset_rx();
    for (uint32_t ch = 1; ch <= 16; ch++) {
        midi_rx[0] = midi_rx[1] = (uint8_t)ch;
        send_rx(0x90 | (ch - 1u), 60, 100, ch & 1u ? 1 : 2);
        assert(gate_rx(0, 60) && gate_rx(1, 60));
        assert((mn_get(ch - 1u, 60) & 3u) == 3u);
        send_rx(0x80 | (ch - 1u), 60, 0, 2);
        assert(!gate_rx(0, 60) && !gate_rx(1, 60));
    }
    puts("RX: channels 1..16, shared Note On/Off, USB/TRS passed");
    reset_rx(); song.g[G_ROUTE] = 2u;
    for (uint32_t i = 0; i < NTRK; i++) midi_rx[i] = 14;
    send_rx(0x9D, 60, 100, 1);
    assert(mn_get(13, 60) == MIDI_TRACK_MASK);
    for (uint32_t i = 0; i < NTRK; i++) assert(gate_rx(i, 60) && midi_owners[i] == 1);
    send_rx(0x8D, 60, 0, 2);
    for (uint32_t i = 0; i < NTRK; i++) assert(!gate_rx(i, 60) && !midi_owners[i]);
    puts("RX: all six tracks sound and release on one shared channel");
    for (uint32_t note = 0; note < 128u; note++) send_rx(0x9D, note, 100, 1);
    for (uint32_t i = 0; i < NTRK; i++) assert(midi_owners[i] == 128 && midi_ch[13].owned[i] == 128);
    send_rx(0xBD, 64, 127, 1); send_rx(0xBD, 123, 0, 1);
    midi_forget_track(0);
    assert(!midi_owners[0] && midi_owners[1] == 128);
    send_rx(0xBD, 64, 0, 2);
    for (uint32_t i = 0; i < NTRK; i++) assert(!midi_owners[i] && !midi_ch[13].owned[i]);
    assert(!midi_ch[13].targets);
    puts("RX: 128 notes on six owners, sustain and partial forget keep counters consistent");

    reset_rx(); midi_rx[1] = 1;
    send_rx(0x90, 60, 100, 1); send_rx(0xB0, 64, 127, 2); send_rx(0x90, 60, 0, 1);
    assert(gate_rx(0, 60) && gate_rx(1, 60) && (mn_get(0, 60) & MIDI_PEDAL_NOTE));
    send_rx(0x90, 60, 90, 2);
    assert(midi_owners[0] == 1 && midi_owners[1] == 1 && !(mn_get(0, 60) & MIDI_PEDAL_NOTE));
    send_rx(0xB0, 64, 0, 1);
    assert(gate_rx(0, 60) && gate_rx(1, 60));
    send_rx(0xB0, 123, 0, 1);
    assert(!gate_rx(0, 60) && !gate_rx(1, 60));
    send_rx(0x90, 62, 100, 1); send_rx(0xB0, 64, 127, 1); send_rx(0x80, 62, 0, 1);
    panic_req = 1; events_block(CTL);
    assert(!(mn_get(0, 62) & 1u) && (mn_get(0, 62) & 2u) && midi_owners[1] == 1);
    send_rx(0xB0, 121, 0, 1);
    assert(!mn_get(0, 62) && !midi_owners[1]);
    puts("RX: sustain, retrigger, CC123/121 and isolated preset panic passed");

    reset_rx(); midi_rx[1] = 1; song.g[G_ROUTE] = 2u;
    send_rx(0x90, 60, 100, 1);
    midi_rx[0] = 10; events_block(CTL);
    assert(!gate_rx(0, 60) && gate_rx(1, 60) && mn_get(0, 60) == 2u);
    send_rx(0x80, 60, 0, 2); assert(!gate_rx(1, 60));
    midi_rx[4] = midi_rx[5] = 10;
    send_rx(0x99, 64, 100, 1); assert((mn_get(9, 64) & 49u) == 49u);
    trk_live = 4; events_block(CTL);
    assert(mn_get(9, 64) == 1u && !midi_owners[4] && !midi_owners[5]);
    send_rx(0x89, 64, 0, 1);
    send_rx(0x99, 65, 100, 1); assert(mn_get(9, 65) == 1u);
    song.g[G_ROUTE] = 1; song.sel = 2; events_block(CTL);
    assert(!mn_get(9, 65));
    send_rx(0x99, 67, 100, 2); song.sel = 1;
    send_rx(0x89, 67, 0, 1); assert(!gate_rx(2, 67) && !mn_get(9, 67));
    song.sel = 0; send_rx(0x9B, 69, 100, 1); song.sel = 1;
    midi_rx[0] = 12; events_block(CTL); /* assignments have no effect while SEL is active */
    assert(gate_rx(0, 69) && mn_get(11, 69) == 1u);
    send_rx(0x8B, 69, 0, 2); assert(!gate_rx(0, 69));
    puts("RX: assignment/ROUTE/TRACKS transitions and SEL ownership passed");

    reset_rx(); midi_rx[1] = 1;
    send_rx(0xE0, 127, 127, 1); assert(midi_bend_target[0] == 512 && midi_bend_target[1] == 512);
    send_rx(0xB0, 101, 0, 1); send_rx(0xB0, 100, 0, 1); send_rx(0xB0, 6, 12, 2);
    assert(midi_bend_target[0] == 3072 && midi_bend_target[1] == 3072);
    send_rx(0xB0, 7, 23, 1); send_rx(0xB0, 1, 75, 2); send_rx(0xD0, 88, 0, 1);
    assert(trk[0].p[P_LEVEL] == 23 && trk[1].p[P_LEVEL] == 23);
    assert(trk[0].mw == 75 && trk[1].mw == 75 && trk[0].at == 88 && trk[1].at == 88);
    ml_table[0] = 0x80u | 30u; ml_table[1] = 2; ml_table[2] = P_PAN + 1u;
    send_rx(0xB0, 30, 127, 1);
    assert(trk[2].p[P_PAN] == 63 && trk[0].p[P_PAN] != 63 && trk[1].p[P_PAN] != 63);
    send_rx(0x90, 60, 100, 1); send_rx(0xB0, 120, 0, 2);
    assert(!mn_get(0, 60) && !midi_owners[0] && !midi_owners[1]);
    memset((void *)midi_pc_req, 0, sizeof midi_pc_req);
    send_rx(0xC0, 7, 0, 1); assert(midi_pc_req[0] == 8 && midi_pc_req[1] == 8);
    midi_rx[0] = midi_rx[1] = 16;
    send_rx(0xCF, 2, 0, 1); assert(midi_song_req == 3 && midi_pc_req[0] == 8 && midi_pc_req[1] == 8);
    for (uint32_t i = 0; i < NTRK; i++) assert(trk_midi_ch(i) == i);
    puts("RX: controllers, bend/RPN, panic, Program Change and unchanged output channels passed");

    reset_rx(); trk[1].eng_req = trk[1].engine = ENGI_DRUM; midi_rx[1] = 1;
    send_rx(0x90, 36, 100, 1); send_rx(0xB0, 64, 127, 1); send_rx(0x80, 36, 0, 1);
    assert(mn_get(0, 36) == (1u | MIDI_PEDAL_NOTE) && !midi_owners[1]);
    send_rx(0xB0, 64, 0, 1); assert(!mn_get(0, 36));
    send_rx(0x90, 84, 100, 1); assert(dxo_n[1] && midi_owners[1]);
    send_rx(0x80, 84, 0, 1); assert(!dxo_n[1] && !midi_owners[1]);
    puts("RX: shared drum/synth pedal and drum octave release passed");

    reset_rx(); midi_rx[1] = 1;
    send_rx(0x90, 60, 100, 1);
    for (uint32_t i = 0; i <= MQ; i++) midi_enqueue(0x643C9009u, 1);
    events_block(CTL); assert(!midi_owners[0] && !midi_owners[1] && !mn_get(0, 60));
    puts("RX: shared ownership overflow recovery passed");

    reset_rx(); midi_rx[1] = 1;
    fm1_in.notes = 1u; events_block(CTL);
    uint32_t local = kb_note[0];
    send_rx(0x90, local, 100, 1); send_rx(0x80, local, 0, 1);
    assert(gate_rx(0, local) && !gate_rx(1, local));
    fm1_in.notes = 0; events_block(CTL); assert(!gate_rx(0, local));
    song.g[G_ROUTE] = 1; song.sel = 0;
    send_rx(0x91, 70, 100, 1); send_rx(0x92, 70, 100, 2);
    assert(midi_owners[0] == 2);
    send_rx(0x81, 70, 0, 1); assert(gate_rx(0, 70) && midi_owners[0] == 1);
    send_rx(0x82, 70, 0, 2); assert(!gate_rx(0, 70) && !midi_owners[0]);
    puts("RX: local-key overlap and independent SEL channel owners passed");

    reset_rx(); trk_live = 6; song.g[G_ROUTE] = 2;
    for (uint32_t i = 0; i < NTRK; i++) midi_rx[i] = (uint8_t)(i + 1u);
    events_block(CTL);
    for (uint32_t ch = 1; ch <= 6; ch++) {
        send_rx(0x90 | (ch - 1u), 60, 100, 1);
        assert(gate_rx(ch - 1u, 60));
        send_rx(0x80 | (ch - 1u), 60, 0, 1);
        assert(!gate_rx(ch - 1u, 60));
    }
    puts("RX: ROUTE 1-6 with TRACKS 6: all six tracks receive assigned-channel MIDI");

    reset_rx(); trk_live = 4; song.g[G_ROUTE] = 2;
    for (uint32_t i = 0; i < NTRK; i++) midi_rx[i] = (uint8_t)(i + 1u);
    events_block(CTL);
    for (uint32_t ch = 1; ch <= 6; ch++) {
        send_rx(0x90 | (ch - 1u), 60, 100, 1);
        assert((ch <= 4u) == gate_rx(ch - 1u, 60));
        send_rx(0x80 | (ch - 1u), 60, 0, 1);
    }
    puts("RX: ROUTE 1-6 with TRACKS 4: tracks 5-6 remain inactive");

    reset_rx(); trk_live = 6; song.g[G_ROUTE] = 2;
    midi_rx[0] = midi_rx[1] = 3;
    events_block(CTL);
    send_rx(0x92, 60, 100, 1);
    assert(gate_rx(0, 60) && gate_rx(1, 60) && (mn_get(2, 60) & 3u) == 3u);
    send_rx(0x82, 60, 0, 1);
    assert(!gate_rx(0, 60) && !gate_rx(1, 60));
    puts("RX: ROUTE 1-6 duplicate channel fans out to eligible tracks");

    reset_rx(); trk_live = 6; song.g[G_ROUTE] = 0;
    for (uint32_t i = 0; i < NTRK; i++) midi_rx[i] = (uint8_t)(i + 1u);
    events_block(CTL);
    send_rx(0x94, 60, 100, 1);
    assert(!gate_rx(4, 60) && !gate_rx(5, 60));
    send_rx(0x84, 60, 0, 1);
    puts("RX: ROUTE 1-4 with TRACKS 6: tracks 5-6 do not receive routed MIDI");

    reset_rx(); trk_live = 6; song.g[G_ROUTE] = 2; midi_rx[1] = 1;
    send_rx(0x90, 60, 100, 1); send_rx(0xB0, 64, 127, 1); send_rx(0x80, 60, 0, 1);
    assert(gate_rx(0, 60) && gate_rx(1, 60) && (mn_get(0, 60) & MIDI_PEDAL_NOTE));
    song.g[G_ROUTE] = 0; events_block(CTL);
    assert(gate_rx(0, 60) && gate_rx(1, 60));
    song.g[G_ROUTE] = 1; song.sel = 3; events_block(CTL);
    assert(!gate_rx(0, 60) && !gate_rx(1, 60));
    send_rx(0x93, 70, 100, 1); assert(gate_rx(3, 70));
    song.g[G_ROUTE] = 2; events_block(CTL);
    assert(gate_rx(3, 70));  /* T4 remains assigned to MIDI channel 4. */
    send_rx(0x83, 70, 0, 1);
    assert(!gate_rx(3, 70));
    puts("RX: ROUTE mode switches do not leave stuck or pedal-held notes");
    reset_rx();
    trk_live = 6;
    song.g[G_ROUTE] = 1;
    song.sel = 3;
    send_rx(0x90, 72, 100, 1);
    assert(gate_rx(3, 72));
    song.g[G_ROUTE] = 2;
    events_block(CTL);
    assert(!gate_rx(3, 72));

    /* Test 1 — RX channel reassignment with sustain held */
    reset_rx(); trk_live = 6; song.g[G_ROUTE] = 2u;
    midi_rx[0] = 1u;
    events_block(CTL);
    send_rx(0x90, 60, 100, 1);   /* Note On ch1 -> track 0 */
    assert(gate_rx(0, 60));
    send_rx(0xB0, 64, 127, 1);   /* CC64 down on ch1 */
    assert(midi_ch[0].pedal);
    send_rx(0x80, 60, 0, 1);     /* Note Off -> pedal-held, not released */
    assert(gate_rx(0, 60));
    assert(mn_get(0, 60) & MIDI_PEDAL_NOTE);
    midi_rx[0] = 3u;             /* Track 0 moves to channel 3 */
    events_block(CTL);           /* route update */
    assert(!gate_rx(0, 60));     /* pedal-held note released on route change */
    assert(!(mn_get(0, 60) & MIDI_TRACK_MASK));
    send_rx(0xB0, 64, 0, 1);     /* CC64 up on original ch1 */
    assert(!midi_ch[0].pedal);
    assert(!gate_rx(0, 60));
    send_rx(0x92, 64, 100, 1);   /* Note On ch3 -> track 0 */
    assert(gate_rx(0, 64));
    puts("RX: RX channel reassignment releases pedal-held note on route change");

    /* Test 2 — TRACKS 4 -> 6 -> 4 */
    reset_rx(); song.g[G_ROUTE] = 2u;
    for (uint32_t i = 0; i < NTRK; i++) midi_rx[i] = (uint8_t)(i + 1u);
    trk_live = 4; events_block(CTL);
    send_rx(0x94, 60, 100, 1);   /* ch5 -> track 4, but limit=4 */
    assert(!gate_rx(4, 60));
    send_rx(0x95, 61, 100, 1);   /* ch6 -> track 5, but limit=4 */
    assert(!gate_rx(5, 61));
    trk_live = 6; events_block(CTL);
    send_rx(0x94, 60, 100, 1);   /* ch5 -> track 4 now matches */
    assert(gate_rx(4, 60));
    send_rx(0x95, 61, 100, 1);   /* ch6 -> track 5 now matches */
    assert(gate_rx(5, 61));
    trk_live = 4; events_block(CTL);
    assert(!gate_rx(4, 60));
    assert(!gate_rx(5, 61));
    assert(!midi_owners[4] && !midi_owners[5]);
    puts("RX: TRACKS 4-6-4 transitions release and restore tracks");

    /* Test 3 — Pitch bend across ROUTE changes */
    reset_rx(); trk_live = 6; song.g[G_ROUTE] = 2u;
    midi_rx[0] = midi_rx[1] = 1u;
    events_block(CTL);
    send_rx(0xE0, 127, 127, 1);   /* max bend up on ch1 */
    assert(midi_bend_target[0] == 512 && midi_bend_target[1] == 512);
    song.g[G_ROUTE] = 1u; song.sel = 2; events_block(CTL);   /* SEL, track 2 only */
    assert(midi_bend_target[0] == 0 && midi_bend_target[1] == 0);
    song.g[G_ROUTE] = 2u; events_block(CTL);   /* back to 1-6 */
    assert(midi_bend_target[0] == 512 && midi_bend_target[1] == 512);
    send_rx(0xE0, 0, 0, 1);   /* maximum downward bend on ch1 */
    assert(midi_bend_target[0] == -512 && midi_bend_target[1] == -512);

    send_rx(0xE0, 0, 64, 1);  /* center bend on ch1 */
    assert(midi_bend_target[0] == 0 && midi_bend_target[1] == 0);
    puts("RX: pitch bend across ROUTE changes and center recovery");

    /* Test 4 — CC120 and CC123 channel isolation */
    reset_rx(); trk_live = 6; song.g[G_ROUTE] = 2u;
    midi_rx[0] = 1u; midi_rx[1] = 2u;
    events_block(CTL);
    send_rx(0x90, 60, 100, 1);   /* ch1 note 60 -> track 0 */
    send_rx(0x91, 60, 100, 1);   /* ch2 note 60 -> track 1 */
    assert(gate_rx(0, 60) && gate_rx(1, 60));
    send_rx(0xB0, 123, 0, 1);    /* CC123 on ch1 */
    assert(!gate_rx(0, 60) && gate_rx(1, 60));
    send_rx(0x90, 62, 100, 1);   /* ch1 note 62 -> track 0 */
    send_rx(0xB0, 64, 127, 1);   /* CC64 down on ch1 */
    send_rx(0x80, 62, 0, 1);     /* Note Off -> pedal held */
    assert(gate_rx(0, 62));
    assert(mn_get(0, 62) & MIDI_PEDAL_NOTE);
    send_rx(0xB0, 123, 0, 1);    /* CC123 on ch1 with pedal held */
    assert(gate_rx(0, 62));      /* pedal-held note not released by CC123 */
    send_rx(0xB0, 64, 0, 1);     /* CC64 up on ch1 */
    assert(!gate_rx(0, 62));     /* now released */
    send_rx(0x90, 64, 100, 1);   /* ch1 note 64 -> track 0 */
    send_rx(0x91, 64, 100, 1);   /* ch2 note 64 -> track 1 */
    assert(gate_rx(0, 64) && gate_rx(1, 64));
    send_rx(0xB0, 120, 0, 1);    /* CC120 on ch1 */
    assert(!gate_rx(0, 64) && gate_rx(1, 64));
    assert(!midi_owners[0] && midi_owners[1]);
    puts("RX: CC120 and CC123 channel isolation, pedal-aware CC123");

    /* Test 5 — Same-pitch notes on different MIDI channels */
    reset_rx(); trk_live = 6; song.g[G_ROUTE] = 2u;
    midi_rx[0] = 1u; midi_rx[1] = 2u;
    events_block(CTL);
    send_rx(0x90, 60, 100, 1);   /* ch1 note 60 vel 100 -> track 0 */
    send_rx(0x91, 60, 50, 1);    /* ch2 note 60 vel 50 -> track 1 */
    assert(gate_rx(0, 60) && gate_rx(1, 60));
    assert(mn_get(0, 60) == 1u && mn_get(1, 60) == 2u);
    send_rx(0x80, 60, 0, 1);     /* Note Off ch1 note 60 */
    assert(!gate_rx(0, 60) && gate_rx(1, 60));
    assert(!(mn_get(0, 60) & 1u) && (mn_get(1, 60) & 2u));
    send_rx(0x81, 60, 0, 1);     /* Note Off ch2 note 60 */
    assert(!gate_rx(1, 60));
    assert(!mn_get(0, 60) && !mn_get(1, 60));
    puts("RX: same-pitch notes on different channels, independent release");

    /* Test 6 — Shared-channel expression */
    reset_rx(); trk_live = 6; song.g[G_ROUTE] = 2u;
    midi_rx[0] = midi_rx[1] = 1u;
    midi_rx[2] = 2u;
    events_block(CTL);
    send_rx(0xB0, 7, 50, 1);     /* CC7 level on ch1 -> tracks 0 and 1 */
    assert(trk[0].p[P_LEVEL] == 50 && trk[1].p[P_LEVEL] == 50);
    assert(trk[2].p[P_LEVEL] == 104);   /* track 2 on ch2, unaffected */
    send_rx(0xB0, 1, 75, 1);     /* CC1 mod wheel on ch1 -> tracks 0 and 1 */
    assert(trk[0].mw == 75 && trk[1].mw == 75);
    send_rx(0xD0, 88, 0, 1);     /* channel pressure on ch1 -> tracks 0 and 1 */
    assert(trk[0].at == 88 && trk[1].at == 88);
    puts("RX: shared-channel CC7, CC1, channel pressure, track isolation");
    return 0;
}
