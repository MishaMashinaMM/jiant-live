/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* (0.9.14) MENU > ABOUT > PROFILE: where the audio's time goes, measured on the FM-1 itself (TIMER4, 24 MHz, read by
 * either core: hal/fm1_multicore.h fm1_mc_ticks), so the load is no longer estimated from the host's instruction
 * counts (they do not see the flash cache: 0.9.10's crackle, 0.9.13's EXPLORE budget). Each block (CTL samples,
 * 725 us = PRF_BLK ticks) the sections below are timed where they run; a second of them (344 halves) is summed and
 * kept, the last second while stopped and the last while playing apart (prf_win[0] / [1]): the mean and the worst
 * block of each, in % of a block (ui_menu.c prof_document). Off until PROFILE is first opened (prf_on): the marks
 * still read the timer (a load each), only the sums wait for it. The host has no timer: all 0 (tests set them) */
enum {
    PRF_SEQ,                     /* events_block, the FX layer's keys, the matrix's master, the split (mix_block's head) */
    PRF_T1,                      /* T1..T6: the part's render, mix and sends (fx.c mix_part_t), on the core it ran on */
    PRF_FX = PRF_T1 + NPART,     /* the buses (fx_run_t): */
    PRF_REV,                     /* .. of it the reverb (rev_run) */
    PRF_WAIT,                    /* core 0 waiting for core 1 (mc_wait) */
    PRF_POST,                    /* the FX layer, the SLICER, DJ FILTER, CLIP (after the parts) */
    PRF_MST,                     /* the leveler and the master (master_out) */
    PRF_IO,                      /* USB audio (the host's in, the tap out), the scope, the metronome, 24-bit (audio_block) */
    PRF_C1,                      /* core 1 busy (mc_job) */
    PRF_NB,                      /* (the sections a block) */
    PRF_IRQ = PRF_NB,            /* a half: TIMER5 nested in the audio ISR (the keys, USB) */
    PRF_HALF,                    /* .. the whole render (ABOUT's AUDIO LOAD, TIMER5 left out) */
    PRF_N
};
#define PRF_BLK ((uint32_t)CTL * 544u)              /* a block in TIMER4 ticks (audio.c DAC_TICKS) */
#define PRF_HALVES 344u                              /* a second (2.9 ms halves) */
typedef struct {
    uint32_t sum[PRF_N], max[PRF_N];                 /* ticks: the second's sum, its worst block (half) */
    uint32_t blocks, c1[NPART];                      /* blocks summed; each part's blocks on core 1 */
    uint32_t t5, lat, late, fxrest, frames;          /* TIMER5 in all (ticks); the ISR's latest start (ticks past its
                                                      * period); halves late; blocks the buses rested; UI frames */
} prf_win_t;
/* all of it one object (the compiler merges small globals into groups; new ones reshuffled them, +1.7 KB of code
 * elsewhere): this block's (either core writes its sections, core 0 sums them); this second; the last whole one
 * stopped [0] and playing [1]; measuring (PROFILE opened); a second published (the redraw); bit k: win[k] holds one;
 * prf_half's last start and halves; (ui_menu.c) CPU TEST's measurement */
static struct {
    uint32_t blk[PRF_NB];
    prf_win_t cur, win[2];
    uint32_t last, halves, cpt_ms, cpt_sum, cpt_n;
    volatile uint8_t on, seq;
    uint8_t have, cpt_arm, fxg_dly, rest_ph, msil;         /* (.. fx.c's: fx_rest's clearing, a part a block) */
} prf __attribute__((section(".pool")));             /* (zeroed at boot; an explicit section: never merged) */
#define prf_blk (prf.blk)
#define prf_cur (prf.cur)
#define prf_win (prf.win)
#define prf_on (prf.on)
#define prf_seq (prf.seq)
#define prf_have (prf.have)
#if FELUCCA_DUAL_CORE
#define PRF_T() fm1_mc_ticks()
#else
#ifndef PRF_T
#define PRF_T() 0u
#endif
#endif
/* (prf_block, prf_half: audio.c) */
