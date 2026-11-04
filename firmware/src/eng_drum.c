/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant) */
/* DRUM: JIANT's drum engine, DRUM-X (drumx_voice.c, after Sonic Charge's Microtonic): a kit of 8 lanes, KICK, SNARE,
 * CLAP, HAT CL, HAT OP, TOM, RIM, BELL, each a synthesized sound with two patches A and B and a MORPH between them.
 * The kit is the section's (drumx_voice.c dx_kit), edited on EDIT > SOUND. Felucca's DRUM kits (STD and the model
 * kits 80 10 66 55 77, drum_voice.c) were retired in JIANT 0.1: a sound or project that picked one plays DRUM-X.
 * Before Felucca 1.0 this engine was PHYS's MODEL DRUM; those sounds load as it (drum_from_phys in core.h).
 *
 * Keys and notes: the keys play the General MIDI drum map with the kick on the first C (C KICK, C# RIM,
 * D SNARE, D# CLAP, F# HAT CL, A# HAT OP, F G A B C D TOMS, C# CYM, G# BELL; at every OCT: above it ratcheted, below it damped, 0.8.5), and a note
 * plays its GM drum on its lane (35..81: kicks, snares, toms, hats, cymbals and bells on BELL, congas on TOM, claves
 * on RIM, each at its pitch from the lane's; other notes as their octave of 36..47), so GM patterns and MIDI parts
 * play as drums.
 *
 * Parameters (stored as before: KIT's slot is MRPH, SNAP's NOIS, KICK's WARP): MRPH the MORPH A..B of every
 * lane; TUNE (+-12 semitones), TONE (COLOR), DECY (DECAY) and NOIS (NOISE) move every lane from its patch (64: as
 * the kit has it); FM ENV (JIANT 0.6.36, once ACC, then FM) the FM struck with each hit and gone with its pitch envelope: a click, a beater, a metal edge (drumx_voice.c dx_run); FOLD (JIANT 0.5, was WARP) every sound's oscillator through a wavefolder after its envelope: a bright attack folding back to the plain tail (0: as the kit has it); DRV a soft clip on every hit
 * (x1..x4, level kept). Each lane has its LEVEL (P_LN0..P_LN7, EDIT > LANES / LANES 2, #97: square law, 100 % the
 * default) on the voice amplitude after the knee. The knobs move a hit while it rings.
 *
 * Polyphony: a lane per drum and part, mono: a hit reuses its lane's voice even at another pitch, the 8 lanes ring
 * together. A sounding voice plays one lane (s[0]); the lane remembers its voice (owner), and a voice
 * whose lane was struck again by another voice, or has rung out (-84 dB), ends itself (drum_amp). The
 * voices come from the shared budget (voice.c), up to 8 per part. A closed hat chokes the open one.
 *
 * What applies (engine_t.oneshot): the hit is the envelope. VOICE (always POLY), GLIDE, the ADSR, ENV DEST,
 * LFO DEST PIT / FLT / SHP, the global TUNE and the matrix's PITCH CUT SHP do nothing; LEVEL, PAN, the
 * sends, DIST, the SLICER, LFO DEST AMP, velocity, the matrix's AMP and its E1..E8 (the knobs, per
 * block) do. A released key does not end a hit.
 *
 * State: 8 lanes per part (drum_kit: each lane's hit; .bss since JIANT 0.6.53). */
#include "drumx_voice.c"

typedef struct {
    dx_voice_t x;                /* the hit */
    uint8_t owner;               /* the voice playing the lane: index + 1, 0 = none */
    int8_t st;                   /* its semitones from the lane's pitch (the GM map) */
    uint8_t mg;                  /* its group mute's fade: DXG_FADE (sounding) .. 0 (muted, silent) */
    uint8_t age;                 /* blocks since the hit (255: and more): PUNCH's shape */
    int8_t rm, rt;               /* (0.5.1) X-MOD RAND: this hit's MORPH offset, its tune (1/16 semitones) */
    int16_t bc, bd, bs;          /* (0.6.82) a step's BEND on the hit: now, where to, a block's step (1/16 semitones) */
    uint8_t dm, fl, fv;          /* (0.8.5) its TOUCH 0..3 (DXO_*: SHORT GHOST FLAM, 0 as the kit has it); FLAM: blocks
                                  * to the hit itself (0 none), its velocity */
} drum_lane_t;
/* (JIANT 0.5.1) X-MOD (EDIT > X-MOD on a DRUM track): the kit's own modulator, saved with the kit (project.c, in its
 * reserved bytes): RATE (OFF, 4BAR 2BAR 1BAR 1/2 1/4 1/8 1/16 1/32: a tempo-synced LFO), DPTH (how far it moves
 * MORPH, +-64), SHPE (SINE TRI SAW RAMP S&H), RAND (each hit its own MORPH, up to +-64, and tune, up to +-1
 * semitone: no two hits alike). dx_mot_v: the LFO's MORPH offset this block (fx.c dx_mot_block) */
enum { XM_RATE, XM_DPTH, XM_SHPE, XM_RAND };
static uint8_t dx_mot[4];
static int32_t dx_mot_v;
static uint32_t dx_mot_rng = 0x9E3779B9u;

static drum_lane_t drum_kit[NPART][DV_NLANE];   /* (0.6.53: in .bss, was .pool: the pool kept for the FX lines) */
static drum_lane_t drum_kit12[NPART][4];        /* hit state for the four MIDI-only lanes (B2 S2 O2 T2) */

/* The group mutes (JIANT): KICK (the kick), SNARE (snare and clap), HAT (both hats), PERCS (tom, rim, bell), on every
 * DRUM track. A section's (project.c keeps them with its DRUM-X kit, the song stages them); set live on the
 * GLO layer (ui_layer.c). A muted group's new hits do not sound (voice.c trk_note_on); what rings when it is muted
 * fades out (drum_render: DXG_FADE blocks, ~5 ms, no click), then ends */
enum { DXG_KICK = 1, DXG_SNARE = 2, DXG_HAT = 4, DXG_PERC = 8, DXG_ALL = 15 };
static const uint8_t DXG_LANE[DV_NLANE] = {DXG_KICK, DXG_SNARE, DXG_SNARE, DXG_HAT, DXG_HAT, DXG_PERC, DXG_PERC, DXG_PERC};
/* group assignments for the four MIDI-only lanes: B2<-KICK, S2<-SNARE, O2<-HAT, T2<-PERC */
static const uint8_t DXG_LANE12[4] = {DXG_KICK, DXG_SNARE, DXG_HAT, DXG_PERC};
#define DXG_FADE 8u
/* .. and each sound's own mute (EDIT held + black keys 1..8 on a DRUM track, ui_layer.c): bit 8 + lane */
#define DXM_LANE(l) (1u << (8u + (l)))
#define DXM_ALL 0xFF0Fu
static volatile uint16_t dx_mute;                       /* DXG_* groups | DXM_LANE lanes (0-7) */
static volatile uint16_t dx_mute12;                     /* DXM_LANE lanes (8-11): bits 0-3 */
static void dx_mute_set(uint32_t m) { dx_mute = (uint16_t)(m & DXM_ALL); }
static int dx_lane_muted(uint32_t l)
{
    if (l < DV_NLANE)
        return (DXG_LANE[l] & dx_mute) || (dx_mute & DXM_LANE(l));
    if (l >= DV_NLANE12)
        return 1;                                          /* fail closed for an invalid voice lane */
    return (DXG_LANE12[l - DV_NLANE] & dx_mute) || (dx_mute12 & (1u << (l - DV_NLANE)));
}

static int16_t clip_eff, pnch_eff;   /* G_CLIP / G_PUNCH as the master plays them (mod.c macro_master) */
/* The drum bus (JIANT, MENU-less: FX > MASTER). PUNCH (G_PUNCH 0..100, no detector: each hit's own age), made to
 * fatten (JIANT 0.5; it was Drum Buss's +11 dB / -14 dB, which thinned everything out): the whole hit driven into the
 * voice's knee, its first ~3 ms up to x1.45, its body (~15 .. 115 ms) up to x1.7, back to unity by ~180 ms: the peak
 * held by the knee, the body denser and louder, the tail whole (the master's leveler does not pump on a spike). DUCK (G_DUCK, fx.c): a kick struck on any
 * DRUM track (its group not muted) ducks the other parts; duck_hit tells fx.c's mix_block */
static volatile uint8_t duck_hit;
#define DRUM_ATK_BLK 48u                                 /* (JIANT 0.4) ATK+ on a DRUM track: a hit's fade-in, blocks */
static int32_t punch_gain(uint32_t age, int32_t p)     /* Q15 (32768 = 1, up to x1.7), p 0..100 */
{
    int32_t up = 32768 + p * 150, body = 32768 + p * 230;
    return age < 4u ? up : age < 20u ? up + (body - up) * (int32_t)(age - 4u) / 16
                       : age < 160u ? body : age < 250u ? body + (32768 - body) * (int32_t)(age - 160u) / 90 : 32768;
}

/* General MIDI notes 35..81 -> the lane (DV_*) and semitones from its pitch */
static const int8_t DRUM_GM[47][2] = {
    {DV_KICK, -2}, {DV_KICK, 0}, {DV_RIM, 0}, {DV_SNARE, 0}, {DV_CLAP, 0}, {DV_S2, 0},              /* 35 */
    {DV_B2, 0}, {DV_HATC, 0}, {DV_O2, 0}, {DV_T2, 0}, {DV_TOM, 0}, {DV_HATO, 0},               /* 41 */
    {DV_TOM, 3}, {DV_TOM, 5}, {DV_BELL, 0}, {DV_TOM, 8}, {DV_BELL, -3}, {DV_BELL, 2},                 /* 47 */
    {DV_BELL, 5}, {DV_HATC, 5}, {DV_BELL, 4}, {DV_BELL, 0}, {DV_BELL, 1}, {DV_RIM, -12},              /* 53 */
    {DV_BELL, -2}, {DV_TOM, 10}, {DV_TOM, 7}, {DV_TOM, 5}, {DV_TOM, 5}, {DV_TOM, 0},                  /* 59 */
    {DV_TOM, 10}, {DV_TOM, 7}, {DV_BELL, 7}, {DV_BELL, 3}, {DV_HATC, 3}, {DV_HATC, 7},                /* 65 */
    {DV_RIM, 7}, {DV_RIM, 5}, {DV_HATC, -4}, {DV_HATC, -6}, {DV_RIM, 0}, {DV_RIM, -4},                /* 71 */
    {DV_RIM, -7}, {DV_TOM, 12}, {DV_TOM, 8}, {DV_BELL, 12}, {DV_BELL, 12},                            /* 77 */
};
static uint32_t drum_gm_ix(uint32_t note) { return (note >= 35u && note <= 81u ? note : 36u + (note + 120u - 36u) % 12u) - 35u; }

/* (JIANT 0.8.5) the octaves of the kit. OCT 0, where the keys play the GM map (key 7 the kick, 36), is the kit as it
 * always was. The other octaves play the same sounds on the same keys (drum_keys: OCT no longer moves the notes).
 * Above, repeated in time while held (seq.c dxo_tick; 0.9.13: half as fast, x2 already too fast to play; 0.9.17: half
 * again): OCT +1 the hit every other step (a hit recorded on every other step entered while held), +2 once a step (a
 * roll at the step rate, a hit recorded on every step entered), +3 x2 a step; recorded as the hit and its lane's mode
 * (none, none, x2; seq.c rec_lmode). Below, each hit with a
 * TOUCH (drum_lane_t.dm; the step's TOUCH component, core.h CP_TOUCH, when recorded): OCT -1 SHORT (half the decay),
 * -2 GHOST (short, darker, at ~45 % of its level: a ghost note), -3 FLAM (a soft grace hit, then the hit itself
 * DXO_FLAM later, ~20 ms whatever the tempo). MIDI keeps the GM map on 35..81, as ever; the notes around it, which
 * played their octave of 36..47 plain, are those octaves' bands: 23..34 -1, 11..22 -2, 0..10 -3, 82..93 +1, 94..105
 * +2, 106..127 +3, each by its pitch class (84 the kick x2, 24 the kick SHORT). dx_in_tch: a live hit's TOUCH,
 * track_t.dtch: a step's */
static const uint8_t DXO_RAT[4] = {1, 1, 1, 2};          /* OCT 0 +1 +2 +3: hits a step (0.9.13: was 1 2 4 8; 0.9.17:
                                                          * was 1 1 2 4, +1 now every other step: seq.c dxo_t.half) */
static const int8_t DXO_DEC[4] = {0, -14, -14, 0};       /* TOUCH 0..3: on DECAY (14: half the decay time, dx_tau) */
static const int8_t DXO_TON[4] = {0, 0, -24, 0};         /* .. on TONE (COLOR: the lane's filter, its drive) */
static const uint8_t DXO_VEL[4] = {128, 128, 58, 58};    /* .. on the velocity, /128 (FLAM: its grace hit's) */
#define DXO_FLAM ((FS * 20u / 1000u + CTL / 2u) / CTL)   /* FLAM: blocks from the grace hit to the hit (~20 ms) */
static uint8_t dx_in_tch, dx_in_rat;                     /* the live note's TOUCH / RATCH (seq.c dxo_on; 0 none) */
static int32_t drum_oct_of(uint32_t note)                /* a MIDI note's octave of the kit */
{
    if (note >= 35u && note <= 81u)
        return 0;
    if (note < 35u)
        return -(int32_t)((35u - note + 11u) / 12u);
    return note >= 106u ? 3 : note >= 94u ? 2 : 1;
}
static uint32_t drum_oct_base(uint32_t note) { return 36u + (note + 120u - 36u) % 12u; }   /* .. its sound (36..47) */
/* the MIDI note of key note at octave o (MIDI OUT): its band's note of the same pitch class (DRUM_GM's 48..55 are
 * their pitch class there) */
static uint32_t drum_oct_note(uint32_t note, int32_t o)
{
    int32_t lo, hi, n;
    if (!o)
        return note;
    lo = o > 0 ? 82 + 12 * (o > 3 ? 2 : o - 1) : o == -1 ? 23 : o == -2 ? 11 : 0;
    hi = o > 2 ? 127 : o == -3 ? 10 : lo + 11;
    n = lo + (((int32_t)drum_oct_base(note) - lo) % 12 + 12) % 12;
    return (uint32_t)(n > hi ? n - 12 >= 0 ? n - 12 : 11 : n);
}
static const char *drum_oct_name(int32_t o)              /* what OCT does on a DRUM track (ui_input.c) */
{
    static const char *const N[7] = {"FLAM", "GHOST", "SHORT", "DRUM-X KIT", "REPEAT 1/2", "REPEAT", "RATCHET x2"};
    return N[clamp(o, -3, 3) + 3];
}

/* ----------------------------------------------------- the grid's lanes --- */
/* A step's lane hits (step_t.hit / acc, the DRUM grid: SEQ > STEP on a DRUM track) play these GM notes, on any
 * engine: DRUM strikes its lanes, a synth plays the pitches. Each is the designed pitch of
 * its lane (st 0 in DRUM_GM) */
static const uint8_t DRUM_LANE_NOTE[NLANE] = {36, 38, 39, 42, 46, 45, 37, 56};

/* the lane a note strikes */
static uint32_t drum_lane(uint32_t note) { return (uint32_t)DRUM_GM[drum_gm_ix(note)][0]; }

/* a note of a muted group (dx_mute): voice.c trk_note_on drops it */
static int drum_muted(uint32_t note) { return dx_lane_muted(drum_lane(note)); }

/* the lane's name (5 characters at most) */
static const char *drum_lane_name(const track_t *t, uint32_t l)
{
    static const char *const N[12] = {"KICK", "SNARE", "CLAP", "HATCL", "HATOP", "TOM", "RIM", "BELL",
                                      "KICK2", "SNARE2", "HATOP2", "TOM2"};
    (void)t;
    return N[l < 12u ? l : 0];
}
/* .. in two letters, the drum machine way (the grid's lane column) */
static const char *drum_lane_abbr(const track_t *t, uint32_t l)
{
    static const char *const N[12] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB",
                                      "B2", "S2", "O2", "T2"};
    (void)t;
    return N[l < 12u ? l : 0];
}

/* the lanes step s strikes: its hits, and its notes on their lanes (a NOTE step only) */
static uint32_t step_lanes(const step_t *s)
{
    uint32_t m = 0, i;
    if (s->time != ST_NOTE)
        return 0;
    for (i = 0; i < s->n && i < 4u; i++) {
        uint32_t l = drum_lane(s->note[i]);
        if (l < DV_NLANE)
            m |= 1u << l;
    }
    return m | s->hit;
}

/* .. and which of them are accented */
static uint32_t step_accents(const step_t *s) { return s->flags & SF_ACCENT ? step_lanes(s) : s->acc & step_lanes(s); }

/* a step's notes that are a lane's note become that lane's hits (the same note, the same velocity: nothing
 * sounds different). Other notes (a tom variation 47, a crash 49, or MIDI-only new drums) stay notes, shown on their lane. A step accent
 * of a step left with hits only becomes the hits' accents. Pattern loads into a DRUM track, projects of
 * before the grid, the grid's edits */
static void step_to_grid(step_t *s)
{
    uint32_t i, k = 0;
    if (s->time != ST_NOTE)
        return;
    for (i = 0; i < s->n && i < 4u; i++) {
        uint32_t l = drum_lane(s->note[i]);
        if (l < DV_NLANE && s->note[i] == DRUM_LANE_NOTE[l])
            s->hit |= (uint8_t)(1u << l);
        else
            s->note[k++] = s->note[i];
    }
    for (i = k; i < 4u; i++)
        s->note[i] = 0;
    s->n = (uint8_t)k;
    if (!k && (s->flags & SF_ACCENT)) {
        s->acc |= s->hit;
        s->flags &= (uint8_t)~SF_ACCENT;
    }
}

static drum_lane_t *drum_kit_of(const track_t *t)
{
    return t >= &trk[0] && t < &trk[NPART] ? drum_kit[t - trk] : 0;
}
static drum_lane_t *drum_kit12_of(const track_t *t)
{
    return t >= &trk[0] && t < &trk[NPART] ? drum_kit12[t - trk] : 0;
}

/* the lane voice v plays, 0 when it plays none (any more) */
static drum_lane_t *drum_lane_of(track_t *t, const voice_t *v)
{
    uint32_t i = (uint32_t)(v - t->v);
    if (i >= NVOICE)
        return 0;
    uint32_t lane = (uint32_t)v->s[0];
    if (lane >= DV_NLANE12)
        return 0;
    if (lane < DV_NLANE) {
        drum_lane_t *K = drum_kit_of(t);
        if (!K)
            return 0;
        drum_lane_t *L = &K[lane];
        return L->owner == i + 1u ? L : 0;
    }
    drum_lane_t *K = drum_kit12_of(t);
    if (!K)
        return 0;
    drum_lane_t *L = &K[lane - DV_NLANE];
    return L->owner == i + 1u ? L : 0;
}

/* Different GM pitches on a lane still share its one sounding voice. */
static voice_t *drum_reuse(track_t *t, uint32_t note)
{
    uint32_t lane = drum_lane(note);
    uint32_t owner;
    if (lane < DV_NLANE) {
        drum_lane_t *K = drum_kit_of(t);
        if (!K)
            return 0;
        owner = K[lane].owner;
    } else {
        drum_lane_t *K = drum_kit12_of(t);
        if (!K)
            return 0;
        owner = K[lane - DV_NLANE].owner;
    }
    if (!owner || owner > NVOICE)
        return 0;
    voice_t *v = &t->v[owner - 1u];
    return v->active && (uint32_t)v->s[0] == lane ? v : 0;
}

static void drum_note_on(track_t *t, voice_t *v)
{
    uint32_t i = (uint32_t)(v - t->v), lane;
    drum_lane_t *K, *K12, *L;
    if (i >= NVOICE)
        return;
    lane = drum_lane(v->note);
    K = drum_kit_of(t);
    K12 = drum_kit12_of(t);
    if (!K || !K12)
        return;
    /* release the lane this voice was playing */
    {
        uint32_t old_lane = (uint32_t)v->s[0];
        if (old_lane < DV_NLANE) {
            L = &K[old_lane];
        } else if (old_lane < DV_NLANE12) {
            L = &K12[old_lane - DV_NLANE];
        } else {
            L = 0;
        }
        if (L && L->owner == i + 1u)
            L->owner = 0;
    }
    /* claim the new lane */
    if (lane < DV_NLANE) {
        L = &K[lane];
    } else if (lane < DV_NLANE12) {
        L = &K12[lane - DV_NLANE];
    } else {
        return;
    }
    L->st = (int8_t)clamp(DRUM_GM[drum_gm_ix(v->note)][1] + t->pfx_rpit / 16 + t->dpit, -60, 60);   /* (pfx.c RANDOM;
                                                          * 0.6.82: the step's components, seq.c) */
    L->bc = 0;                                           /* (0.6.82) its BEND, from the step */
    L->bd = t->dbend;
    L->bs = (int16_t)(t->dbend ? ((t->dbend < 0 ? -t->dbend : t->dbend) + t->dbt) / (t->dbt ? t->dbt : 1u) : 0);
    L->owner = (uint8_t)(i + 1u);                        /* (its last voice, if another, ends: drum_amp) */
    L->dm = (uint8_t)((dx_in_tch ? dx_in_tch : t->dtch) & 3u);   /* (0.8.5) TOUCH: a key below the kit's octave, a step */
    L->fl = 0;
    if (L->dm) {
        L->fv = v->vel;
        v->vel = (uint8_t)((v->vel * DXO_VEL[L->dm] + 64u) >> 7 ? (v->vel * DXO_VEL[L->dm] + 64u) >> 7 : 1u);
        if (L->dm == 3u)
            L->fl = (uint8_t)(DXO_FLAM + 1u);            /* (FLAM: the grace hit now, the hit DXO_FLAM blocks on, drum_render) */
    }
    v->s[0] = (int32_t)lane;
    v->env_out = v->vel * 258;                           /* the hit starts at its level (no ramp from 0) */
    v->env = 1 << 24;                                    /* the ADSR held at full from here, not from drum_amp:
                                                          * a key-off before the first block (zero-length MIDI
                                                          * notes) would end a fresh voice at env 0 in env_tick */
    L->mg = DXG_FADE;
    L->age = 0;
    if (dx_mot[XM_RAND]) {                               /* (0.5.1) X-MOD RAND: this hit's own MORPH and tune */
        int32_t r = dx_mot[XM_RAND];
        dx_mot_rng = dx_mot_rng * 1664525u + 1013904223u;
        L->rm = (int8_t)(((int32_t)(dx_mot_rng >> 24) - 128) * r >> 8);
        L->rt = (int8_t)(((int32_t)((dx_mot_rng >> 16) & 255u) - 128) * r >> 10);
    } else {
        L->rm = L->rt = 0;
    }
    if (lane == DV_KICK || lane == DV_B2)
        duck_hit = 1;
    dx_trigger(&L->x);
    if (lane == DV_HATC) {
        dx_choke(&K[DV_HATO].x);
    }
}

/* the voice's amplitude: the hit, not the ADSR (which still runs, held at full so a release never ends
 * it). A voice that plays no lane any more, or whose drum has rung out, ends here */
static int32_t drum_amp(track_t *t, voice_t *v, int32_t adsr)
{
    const drum_lane_t *L = drum_lane_of(t, v);
    if (!v->active)                                      /* (taken for another part: env_tick ended it) */
        return 0;
    if (v->stage == 4u)                                  /* (0.6.74) given up: env_tick's fade, not held at full */
        return adsr;
    if (!L || (!L->x.live && !L->x.trig && !L->fl)) {   /* (0.8.5: a FLAM's hit to come keeps it) */
        v->active = v->gate = 0;
        v->stage = 0;
        v->env = 0;
        return 0;
    }
    v->env = 1 << 24;
    return 32767;
}

/* (JIANT 0.6.48) IMAGE (P_DXIMG, a track's) and each sound's SEND (dx_snd, the kit's: saved with it, project.c). The
 * part stays one mono sum through its chain (DRV, FILTER, DIST); each sound's pan comes out as what it takes from one
 * side (dx_sl / dx_sr: L = sum - dx_sl, R = sum - dx_sr, fx.c drum_mix), and its SEND as its own feed of a bus (dx_sb),
 * both from the hit as it leaves the voice: no stage run twice. DRV, DIST and the FILTER shape the sum only, so with
 * them the image narrows (dx_img_k), or the side taken would not match the side shaped. IMAGE 0 and no SEND: nothing
 * of this runs */
#define DXS_BUS(s) ((uint32_t)(s) >> 6)                  /* 0 none, 1 CHORUS, 2 DELAY, 3 REVERB */
#define DXS_AMT(s) ((int32_t)(s) & 63)                   /* 0..63 */
static uint8_t dx_snd[DV_NLANE];
/* (JIANT 0.7.8) a core's own (mod.c's mod_c the same way): the two cores render two parts at once (fx.c mix_block), and
 * with one set the other core's part cleared dx_xon and the sides mid-render: the image on and off block by block (a
 * grit at the block rate, like a downsampling) and clicks on the side a sound leaned to. A core renders its parts one
 * after another, so one set a core is enough */
typedef struct {
    int32_t sl[CTL], sr[CTL], sb[3][CTL];                /* what each side gives up; each bus's feed */
    int8_t lpan[DV_NLANE12];                           /* this block's pans, -64 (left) .. 64, at its end */
    int8_t lpan0[DV_NLANE12];                          /* (0.6.81) .. and at its start: each ramped across the block */
    uint8_t xon;                                         /* this part's render: bit 0 the image, bits 1..3 a bus fed */
} dx_core_t;
static dx_core_t dx_core[2];
#define DXC (dx_core[MC_CPU() & 1u])
#define dx_sl (DXC.sl)
#define dx_sr (DXC.sr)
#define dx_sb (DXC.sb)
#define dx_lpan (DXC.lpan)
#define dx_lpan0 (DXC.lpan0)
#define dx_xon (DXC.xon)
static int8_t dx_pcur[NTRK][DV_NLANE];                   /* (0.6.81) each part's pans as they are, slewed toward the
                                                           * knob's: a turn of IMAGE jumped a sound across (a click) */
static int8_t dx_pcur12[NTRK][4];                        /* pans for the four MIDI-only lanes */
#define DX_PSLEW 2                                       /* (a pan's step a block at most: across in ~45 ms) */
/* IMAGE img 1..127 -> lane l's pan, -64..64: each sound on a sine of its own rate over the knob (1 to 9 turns), so a
 * sweep goes through the kit's placings one after another, every one coming back at the same value; the first 16 open
 * it from the middle; the kick a quarter as far (the low end stays centred) */
static int32_t dx_img_pan(uint32_t l, int32_t img)
{
    static const uint8_t F[DV_NLANE12] = {1, 3, 5, 2, 7, 4, 6, 9, 1, 3, 2, 4};
    int32_t w = img >= 16 ? 64 : img * 4, s;
    if (img <= 0)
        return 0;
    s = (osc_sine((uint32_t)img * F[l] * 33554432u + l * 0x2E000000u) * w) >> 15;
    return l == DV_KICK || l == DV_B2 ? s / 4 : s;
}
/* how wide the image plays, Q8: all of it, less with DRV, DIST and the COMB (they shape the mono sum only) */
static int32_t dx_img_k(const track_t *t)
{
    int32_t k = 256 - clamp(t->p[P_E7], 0, 127) * 160 / 127, d = clamp(t->p[P_DIST], 0, 64);
    if (d)
        k = k * (256 - d * 3) >> 8;
    if (t->p[P_FTYPE] == FT_COMB)                        /* (0.6.81: the other filters shape the sides too, voice.c
                                                          * tf_sides: the image stays whole through them) */
        k >>= 2;
    return k;
}
/* a DRUM part's render starts (voice.c track_render): the pans of the block, the buffers that will be fed cleared */
static __attribute__((noinline)) void drum_side_begin(const track_t *t, uint32_t n)
{
    int32_t img = clamp(t->p[P_DXIMG], 0, 127), k = img ? dx_img_k(t) : 0;
    uint32_t l, i, on = 0;
    dx_core_t *dc = &DXC;                                /* (0.7.9: this core's, read once) */
    int8_t *pc = dx_pcur[(uint32_t)(t - trk) % NTRK];
    int8_t *pc12 = dx_pcur12[(uint32_t)(t - trk) % NTRK];
    for (l = 0; l < DV_NLANE; l++) {
        int32_t p = img ? dx_img_pan(l, img) * k >> 8 : 0, p0 = pc[l];
        p = p0 + clamp(p - p0, -DX_PSLEW, DX_PSLEW);
        pc[l] = (int8_t)p;
        dc->lpan0[l] = (int8_t)p0;
        dc->lpan[l] = (int8_t)p;
        if (p || p0)
            on |= 1u;
        if (DXS_BUS(dx_snd[l]) && DXS_AMT(dx_snd[l]))
            on |= 1u << DXS_BUS(dx_snd[l]);
    }
    for (l = 0; l < 4u; l++) {
        uint32_t ll = DV_NLANE + l;
        int32_t p = img ? dx_img_pan(ll, img) * k >> 8 : 0, p0 = pc12[l];
        p = p0 + clamp(p - p0, -DX_PSLEW, DX_PSLEW);
        pc12[l] = (int8_t)p;
        dc->lpan0[ll] = (int8_t)p0;
        dc->lpan[ll] = (int8_t)p;
        if (p || p0)
            on |= 1u;
        if (DXS_BUS(dx_snd12[l]) && DXS_AMT(dx_snd12[l]))
            on |= 1u << DXS_BUS(dx_snd12[l]);
    }
    dc->xon = (uint8_t)on;
    if (n > CTL)
        n = CTL;
    if (on & 1u)
        for (i = 0; i < n; i++)
            dc->sl[i] = dc->sr[i] = 0;
    for (l = 0; l < 3u; l++)
        if ((on >> (l + 1u)) & 1u)
            for (i = 0; i < n; i++)
                dc->sb[l][i] = 0;
}
/* lane l's sides and bus this block (drum_render's loop feeds them as it adds the hit to the part): 0 when none. (0.6.81)
 * g[0..3]: what it gives up of the left at the block's start and end, of the right at its start and end (a pan crossing
 * the middle within a block fades out of one side into the other) */
static int32_t dx_dump[CTL];                             /* (the bus not fed: a sink, so the loop has no branch) */
static __attribute__((noinline)) int drum_side_of(dx_core_t *dc, uint32_t l, int32_t *g, int32_t **d2, int32_t *a)
{
    int32_t p0, p1;
    uint32_t b;
    if (l >= DV_NLANE12)
        return 0;
    p0 = dc->lpan0[l];
    p1 = dc->lpan[l];
    b = l < DV_NLANE ? DXS_BUS(dx_snd[l]) : DXS_BUS(dx_snd12[l - DV_NLANE]);
    *a = b ? DXS_AMT(l < DV_NLANE ? dx_snd[l] : dx_snd12[l - DV_NLANE]) : 0;
    if (!(dc->xon & 1u))
        p0 = p1 = 0;
    g[0] = p0 > 0 ? p0 : 0; g[1] = p1 > 0 ? p1 : 0;
    g[2] = p0 < 0 ? -p0 : 0; g[3] = p1 < 0 ? -p1 : 0;
    *d2 = *a ? dc->sb[b - 1u] : dx_dump;
    return p0 || p1 || *a;
}
/* the voice's output loop with them: the hit into the part, its side and its bus at once (the pan steady this block:
 * one side fed) */
static __attribute__((noinline)) void drum_side_add(int32_t *y, int32_t *out, uint32_t n, const vmod_t *m, int32_t *d1,
                                                    int32_t g, int32_t *d2, int32_t a)
{
    uint32_t i;
    for (i = 0; i < n; i++) {
        int32_t s = y[i], o = voice_amp(s + (s >> 2), m, i) << 1;
        out[i] += o;
        d1[i] += (o * g) >> 6;
        d2[i] += (o * a) >> 6;
    }
}
/* (0.6.81) .. its pan moving (IMAGE turned): both sides fed, each ramped across the block (only while it slews) */
static __attribute__((noinline)) void drum_side_ramp(dx_core_t *dc, int32_t *y, int32_t *out, uint32_t n,
                                                     const vmod_t *m, const int32_t *g, int32_t *d2, int32_t a)
{
    uint32_t i;
    int32_t gl = g[0] << CTL_LOG2, dgl = g[1] - g[0], gr = g[2] << CTL_LOG2, dgr = g[3] - g[2];   /* (Q6 + CTL_LOG2) */
    int32_t *sl = dc->sl, *sr = dc->sr;                  /* (this core's) */
    for (i = 0; i < n; i++, gl += dgl, gr += dgr) {
        int32_t s = y[i], o = voice_amp(s + (s >> 2), m, i) << 1;
        out[i] += o;
        sl[i] += (o * gl) >> (6 + CTL_LOG2);
        sr[i] += (o * gr) >> (6 + CTL_LOG2);
        d2[i] += (o * a) >> 6;
    }
}

static void drum_render(track_t *t, voice_t *v, int32_t *out, uint32_t n, const vmod_t *m)
{
    const int16_t *p = t->p;
    drum_lane_t *L = drum_lane_of(t, v);
    int32_t y[CTL], pb0 = 4096, pb1 = 4096;
    uint32_t lane = (uint32_t)v->s[0];
    int32_t lv = clamp(p[lane < DV_NLANE ? P_LN0 + lane : P_LN8 + lane - DV_NLANE], 0, 127);
    if (lane == DV_BELL)   /* (JIANT 0.6.49) the cowbell 4 dB under the others: it */
        lv = lv * 100 / 127;                                  /* stood out of every kit (its LEVEL at 100 % is this) */
    uint32_t i;
    int32_t lfo_pit, lfo_shp;
    vmod_t ml;                                           /* (only its amplitude ramp is read: voice_amp) */
    if (!L)
        return;
    if (dx_lane_muted((uint32_t)v->s[0]) || L->mg < DXG_FADE) {   /* its group muted:
                                                          * fading (to the end, if unmuted meanwhile), then ended */
        uint32_t g0 = L->mg;
        if (!g0) {
            L->x.live = L->x.trig = 0;                   /* (drum_amp ends the voice) */
            L->fl = 0;
            L->x.hl = 0;                                 /* (0.6.19: the next hit starts its upsampling from 0) */
            return;
        }
        L->mg = (uint8_t)(g0 - 1u);
        ml.amp0 = m->amp0 * (int32_t)g0 / (int32_t)DXG_FADE;
        ml.amp1 = m->amp1 * (int32_t)(g0 - 1u) / (int32_t)DXG_FADE;
        m = &ml;
    }
    if (L->fl == 2u)                                     /* (0.8.5) FLAM: its velocity back a block before the hit
                                                          * (voice.c takes the level before the render) */
        v->vel = L->fv;
    if (L->fl && !--L->fl) {                             /* .. the hit itself */
        dx_trigger(&L->x);
        L->age = 0;
        if (((uint32_t)v->s[0]) == DV_HATC) {
        dx_choke(&drum_kit_of(t)[DV_HATO].x);
    }
    }
    if (lv < 127) {                                      /* the lane's LEVEL (square law) on the block's amplitude
                                                          * ramp, not per sample; 100 %: the ramp as it was */
        int32_t gl = lv * lv * 2 + (lv * lv >> 6);       /* Q15, 127: 32508 (not used), 64: 8256 */
        ml.amp0 = (m->amp0 >> 4) * gl >> 11;
        ml.amp1 = (m->amp1 >> 4) * gl >> 11;
        m = &ml;
    }
    if (n > CTL)
        n = CTL;
    {   /* (0.6.81) the part's LFO on the kit (its hits never had a vmod's pitch or shape): PITCH tunes it, SHAPE moves
         * MORPH (both did nothing on DRUM) */
        int32_t lfo = mulq15(t->lfo_val, t->lfo_fade);
        lfo_pit = (lfo * p[P_LD_PIT] * 3) >> 15;
        lfo_shp = (lfo * p[P_LD_SHP]) >> 15;
    }
    if (L->bc != L->bd) {                                /* (0.6.82) BEND: towards its pitch over the step */
        int32_t d = L->bd - L->bc;
        L->bc = (int16_t)(L->bc + (d > 0 ? (d < L->bs ? d : L->bs) : (-d < L->bs ? d : -L->bs)));
    }
    dx_run(lane < DV_NLANE ? &dx_kit[lane] : &dx_kit12[lane - DV_NLANE], lane, &L->x,
           p[P_E0] + dx_mot_v + L->rm + lfo_shp,
           L->st * 16 + L->rt + L->bc + (p[P_E1] - 64) * 3 + t->pfx_pit + lfo_pit,
           p[P_E3] - 64 + DXO_DEC[L->dm & 3u], p[P_E2] - 64 + DXO_TON[L->dm & 3u], p[P_E4] - 64, p[P_E6], p[P_E5], y, n);
    {                                                    /* PUNCH: the boost before the knee (driven into it, as
                                                          * Drum Buss), the tail's cut on the amplitude ramp */
        int32_t pu = pnch_eff;
        if (pu > 0) {
            int32_t g0 = punch_gain(L->age, pu), g1 = punch_gain(L->age + 1u, pu);
            if (g0 < 32768 || g1 < 32768) {              /* (gains < 1: amp stays <= 32767) */
                int32_t a0 = (m->amp0 * (g0 < 32768 ? g0 : 32768)) >> 15;
                ml.amp1 = (m->amp1 * (g1 < 32768 ? g1 : 32768)) >> 15;
                ml.amp0 = a0;
                m = &ml;
            }
            pb0 = g0 > 32768 ? g0 >> 3 : 4096;           /* Q12 */
            pb1 = g1 > 32768 ? g1 >> 3 : 4096;
        }
        if ((t->pfx & PFX_ATK) && L->age < DRUM_ATK_BLK) {   /* (JIANT 0.4) ATK+: the hit faded in (~35 ms), softer */
            int32_t a0 = m->amp0 * (int32_t)L->age / (int32_t)DRUM_ATK_BLK;
            ml.amp1 = m->amp1 * (int32_t)(L->age + 1u) / (int32_t)DRUM_ATK_BLK;
            ml.amp0 = a0;
            m = &ml;
        }
        if (L->age < 255u)
            L->age++;
    }
    if (pb0 != 4096 || pb1 != 4096)                      /* (0.6.16: DRV and the knee on the kit's sum, drum_bus) */
        for (i = 0; i < n; i++)
            y[i] = (y[i] * (pb0 + (((pb1 - pb0) * (int32_t)i) >> CTL_LOG2))) >> 12;
    {
        int32_t *d2, g[4], a;
        dx_core_t *dc = &DXC;                            /* (0.7.8: this core's, read once) */
        if (dc->xon && drum_side_of(dc, (uint32_t)v->s[0], g, &d2, &a)) {   /* (0.6.48) IMAGE, SEND: the same loop */
            if (g[0] != g[1] || g[2] != g[3])
                drum_side_ramp(dc, y, out, n, m, g, d2, a);
            else
                drum_side_add(y, out, n, m, g[0] ? dc->sl : g[2] ? dc->sr : dx_dump, g[0] | g[2], d2, a);
            return;
        }
    }
    for (i = 0; i < n; i++) {                            /* x1.25 */
        int32_t s = y[i];
        out[i] += voice_amp(s + (s >> 2), m, i) << 1;
    }
}

/* (JIANT 0.6.16) the kit's sum, once a block (voice.c track_render): DRV x1..x4 into the soft clip, its level kept,
 * and the knee (were each hit's: eight lanes, two curves a sample each; now the bus's, a glue) */
static __attribute__((noinline)) void drum_bus(const track_t *t, int32_t *out, uint32_t n)
{
    int32_t drv = t->p[P_E7], g = 0, mk = 0, k = 4096, *sl = dx_sl, *sr = dx_sr;
    uint32_t i, side = (dx_xon & 1u) != 0u;
    if (drv > 0) {
        g = 4096 + drv * 3 * 4096 / 127;
        mk = (int32_t)((19661u << 15) / (uint32_t)softclip((19661 * g) >> 12));
        k = (g * mk) >> 15;                              /* (the curve's slope at 0: a quiet sum's gain) */
    }
    for (i = 0; i < n; i++) {
        int32_t s = out[i] >> 1, y = s;
        if (g)                                           /* (0.6.19: s held to +-131071: s * g fits; several loud hits
                                                         * at once with DRV high wrapped it: a full-scale click) */
            y = (softclip((clamp(s, -131071, 131071) * g) >> 12) * mk) >> 15;
        y = soft_knee(y, 24000);
        out[i] = y << 1;
        if (side && (g || y != s)) {                     /* (0.7.9: the curve flat here, nothing to scale) */                                      /* (0.7.8) IMAGE: what each side gives up through the same
                                                         * gain the curve gave the sum here (it was taken raw from the
                                                         * squashed sum: a loud hit's side over-taken, clicks and grit
                                                         * on the side it leaned to), so L and R are the curve's exactly */
            if ((s > 256 || s < -256) && !(i & 3u))      /* (0.7.9: the gain every 4th sample: a divide's cost) */
                k = clamp((y << 12) / s, 0, 16384);   /* (y within +-41000: y << 12 fits 32 bits) */
            sl[i] = ((sl[i] >> 2) * k) >> 10;
            sr[i] = ((sr[i] >> 2) * k) >> 10;
        }
    }
}

/* the GM drum map, the first C (key 7) is the kick (C2, 36), at every OCT (0.8.5: the octaves, above) */
static int32_t drum_keys(const track_t *t, uint32_t k)
{
    (void)t;
    return clamp(29 + (int32_t)k, 0, 127);                /* (0.8.5: every octave; OCT ratchets / damps, seq.c dxo_on) */
}

/* {MRPH, TUNE, TONE, DECY, NOIS, FM, FOLD, DRV}; the kit suggests the BEAT pattern (GM notes) */
static const preset_t DRUM_PRESETS[] = {                /* (0.7.2) one a factory kit: drumx_voice.c DX_KITS, this order */
    {"DRUM-X", DRUM_KIT_E, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 20), PAT(12)},   /* (core.h: SAMPLE PERC's too) */
    {"909 CLUB", {60, 64, 76, 64, 64, 0, 0, 10}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 16), PAT(12)},
    {"808 BOOM", {58, 60, 56, 76, 60, 0, 0, 0}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 24), PAT(12)},
    {"LOFI DUST", {70, 64, 50, 54, 76, 0, 0, 20}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(8, 0, 0, 18), PAT(12)},
    {"INDUSTRIAL", {72, 66, 88, 70, 74, 20, 0, 40}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(16, 0, 10, 30), PAT(12)},
    {"ACOUSTIC", {56, 64, 58, 62, 64, 0, 0, 0}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 0, 34), PAT(12)},
    {"DUB ROOTS", {60, 60, 52, 70, 60, 0, 0, 0}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(0, 0, 18, 36), PAT(12)},
    {"TECHNO RAW", {66, 64, 80, 62, 66, 10, 0, 30}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(10, 0, 0, 14), PAT(12)},
    {"BREAKS", {62, 66, 72, 62, 70, 0, 0, 10}, {0, 100, 127, 100}, 0, TAG(DRUM), FX(4, 0, 0, 22), PAT(12)},
};
_Static_assert(sizeof DRUM_PRESETS / sizeof DRUM_PRESETS[0] == sizeof DX_KITS / sizeof DX_KITS[0], "a DRUM preset a kit");
/* (0.7.2) a factory kit into the section's: the sounds, their LAYERs, their SENDs (the project saves them) */
static void dx_kit_load(uint32_t k)
{
    const dx_kit_t *K = &DX_KITS[k % DX_NKIT];
    memcpy(dx_kit, K->lane, sizeof dx_kit);
    memcpy(dx_lay, K->lay, sizeof dx_lay);
    memcpy(dx_snd, K->snd, sizeof dx_snd);
    /* Each additional instance begins with the *selected preset kit*'s corresponding sound. */
    {
        static const uint8_t SRC[4] = {DV_KICK, DV_SNARE, DV_HATO, DV_TOM};
        uint32_t i;
        for (i = 0; i < 4u; i++) {
            dx_kit12[i] = dx_kit[SRC[i]];
            dx_lay12[i] = dx_lay[SRC[i]];
            dx_snd12[i] = dx_snd[SRC[i]];
        }
    }
}

static const engine_t ENG_DRUM = {
    .name = "DRUM",
    .page_title = {"KIT", "KIT 2"},                     /* (0.6.54: both the whole kit's, was HIT) */
    .edit = {
        {"MORPH", F_PCT, 0, 127, 64, 0, 0},
        {"TUNE", F_PCT, 0, 127, 64, 0, 0},
        {"TONE", F_PCT, 0, 127, 64, 0, 0},
        {"DECAY", F_PCT, 0, 127, 64, 0, 0},
        {"NOISE", F_PCT, 0, 127, 64, 0, 0},
        {"FM ENV", F_PCT, 0, 127, 0, 0, 0},             /* (JIANT: was ACC; 0.6.36: was FM, a fixed FM: the FM struck
                                                         * with the hit, gone with the pitch envelope) */
        {"FOLD", F_PCT, 0, 127, 0, 0, 0},              /* (JIANT 0.5: a wavefolder, was WARP) */
        {"DRIVE", F_PCT, 0, 127, 0, 0, 0},
    },
    .presets = DRUM_PRESETS,
    .npresets = NELEM(DRUM_PRESETS),
    .note_on = drum_note_on,
    .render = drum_render,
    .amp = drum_amp,
    .knob = {P_E0, P_E1, P_E2, P_E3},
    .poly = DV_NLANE12,
    .oneshot = 1,
    .keys = drum_keys,
};
