/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * JIANT (FELUCCA TONIC): EDIT > SOUND and SOUND 2, a sound of the DRUM-X kit (docs/TONIC-DRUMX.md). */
/* Shown in the EDIT family on a DRUM track (ui.c page_visible). They edit the section's kit
 * (drumx_voice.c dx_kit, saved with the section: project.c FUNA):
 *   SOUND    LANE (the sound: BD .. CB; a key struck on the track picks its lane too), SIDE (patch A or B), PTCH, PMOD
 *   SOUND 2  DCAY, NOIS, COLR, MODE (the oscillator's wave, the noise filter, SNAP: 24 combinations)
 *   SOUND 3  (JIANT 0.4) LANE again, PMOD's mode (DECAY LONG NOISE SINE), DRIVE (OFF / ON), the wave alone
 * The values are the side's; the MORPH plays between the two (EDIT 2 MRPH). The panel shows the kit's specimen
 * with the sound's organ singled out (ui_plates.c graph_p_drum: the iris, its node lit). */
static uint8_t dx_ui_seen = 0xFFu;                   /* the last key note followed */
static const char *const DX_LANE_ABBR[DV_NLANE12] = {"BD", "SD", "CP", "CH", "OH", "TM", "RS", "CB", "B2", "S2", "O2", "T2"};

/* Keep the original serialized eight-lane arrays and Stage 9B1's separate instances. */
static dx_lane_t *dx_ui_patch(uint32_t l) { return l < DV_NLANE ? &dx_kit[l] : &dx_kit12[l - DV_NLANE]; }
static uint8_t *dx_ui_layer(uint32_t l) { return l < DV_NLANE ? &dx_lay[l] : &dx_lay12[l - DV_NLANE]; }
static uint8_t *dx_ui_send(uint32_t l) { return l < DV_NLANE ? &dx_snd[l] : &dx_snd12[l - DV_NLANE]; }

/* a key struck on the track (seq.c key_on: last_note) picks the sound it plays */
static void dx_follow(void)
{
    if (last_note != dx_ui_seen) {
        if (dx_ui_seen != 0xFFu)                     /* (the first look only takes note of it) */
            dx_ui_lane = (uint8_t)drum_lane(last_note);
        dx_ui_seen = last_note;
    }
}

/* the mode as one value 0..23: wave x 6 + filter x 2 + SNAP, and back */
static uint32_t dx_mode_ix(uint32_t m) { return (m & 3u) * 6u + ((m >> 2) & 3u) * 2u + ((m >> 4) & 1u); }
static uint8_t dx_mode_of(uint32_t ix) { return (uint8_t)(ix / 6u | (ix % 6u / 2u) << 2 | (ix & 1u) << 4); }
static void dx_mode_name(char *b, uint32_t m)        /* "FM BP+" (b: 8 bytes) */
{
    static const char *const W[4] = {"SIN", "FM", "MTL", "BEL"}, *const F[3] = {"LP", "BP", "HP"};
    str_cpy(b, W[m & 3u], 8);
    str_cpy(b + str_len(b), " ", 8 - str_len(b));
    str_cpy(b + str_len(b), F[((m >> 2) & 3u) % 3u], 8 - str_len(b));
    if ((m >> 4) & 1u)
        str_cpy(b + str_len(b), "+", 8 - str_len(b));
}

/* (JIANT 0.6.54) the pages, the whole kit first, then a sound: KIT (EDIT 1), KIT 2 (EDIT 2), KIT 3 (TONE's BP and FEEDBK,
 * IMAGE: dx_kit3_*), X-MOD, FILTER, LEVELS; then SOUND 1..4, KNOB 1 the sound on each:
 *   SOUND    LANE SIDE PITCH PMOD          SOUND 3  LANE MODE PM.TYPE DRIVE
 *   SOUND 2  LANE DECAY NOISE COLOR        SOUND 4  LANE LAYER FEEDBK SEND (the bus and how much on one knob) */
static void dx_lane_turn(int32_t s) { dx_ui_lane = (uint8_t)clamp((int32_t)dx_ui_lane + (s > 0 ? 1 : -1), 0, DV_NLANE12 - 1); }
/* SOUND 4's SEND as one value: 0 OFF, then CHORUS, DELAY, REVERB 16 amounts each (1..48) */
static int32_t dx_send_ix(uint32_t sn)
{
    int32_t a = (DXS_AMT(sn) + 1) / 4;
    return DXS_BUS(sn) ? (int32_t)(DXS_BUS(sn) - 1u) * 16 + (a < 1 ? 1 : a > 16 ? 16 : a) : 0;
}
static uint8_t dx_send_of(int32_t ix)
{
    return ix <= 0 ? 0u : (uint8_t)((uint32_t)((ix - 1) / 16 + 1) << 6 | (uint32_t)(((ix - 1) % 16 + 1) * 4 - 1));
}
/* KNOB k (0..3) turned s on a SOUND page or X-MOD */
static void dx_knob(const page_t *pg, uint32_t k, int32_t s)
{
    dx_lane_t *L = dx_ui_patch(dx_ui_lane);
    uint8_t *v = dx_ui_side ? L->b : L->a;
    uint32_t pn = dx_pagen(pg);
    if (pn == 4u) {                                     /* (JIANT 0.5.1) X-MOD: the kit's, not a sound's */
        static const uint8_t MX[4] = {8, 127, 4, 127};
        int32_t v0 = dx_mot[k & 3u];
        dx_mot[k & 3u] = (uint8_t)clamp(v0 + (MX[k & 3u] > 8 ? accel(EN_K1 + k, s, 127) : (s > 0 ? 1 : -1)), 0, MX[k & 3u]);
        return;
    }
    if (k == 0u) {                                      /* every SOUND page: KNOB 1 the sound */
        dx_lane_turn(s);
        return;
    }
    if (pn == 8u) {                                     /* SOUND 4: LAYER, FEEDBK, SEND */
        uint8_t *ly = dx_ui_layer(dx_ui_lane);
        int32_t la = (int32_t)DX_LAY_L(*ly), fb = (int32_t)DX_LAY_FB(*ly);
        if (k == 1u) la = clamp(la + (s > 0 ? 1 : -1), 0, 15);
        else if (k == 2u) fb = clamp(fb + (s > 0 ? 1 : -1), 0, 15);
        else *dx_ui_send(dx_ui_lane) = dx_send_of(clamp(dx_send_ix(*dx_ui_send(dx_ui_lane)) + (s > 0 ? 1 : -1), 0, 48));
        *ly = (uint8_t)(la | fb << 4);
        return;
    }
    if (pn == 3u) {                                     /* SOUND 3: MODE, the pitch modulation's type, DRIVE */
        if (k == 1u)
            L->mode = (uint8_t)((L->mode & 0xE0u) |
                                (dx_mode_of((uint32_t)clamp((int32_t)dx_mode_ix(L->mode) + (s > 0 ? 1 : -1), 0, 23)) & 0x1Fu));
        else if (k == 2u)
            L->mode = (uint8_t)((L->mode & ~0x60u) | (uint32_t)clamp((int32_t)((L->mode >> 5) & 3u) + (s > 0 ? 1 : -1), 0, 3) << 5);
        else
            L->mode = (uint8_t)(s > 0 ? L->mode | DX_DRIVE : L->mode & ~DX_DRIVE);
        return;
    }
    if (pn == 2u) {                                     /* SOUND 2: DECAY NOISE COLOR (the side's) */
        uint32_t i = k == 1u ? DXP_DECAY : k == 2u ? DXP_NOISE : DXP_COLOR;
        v[i] = (uint8_t)clamp(v[i] + accel(EN_K1 + k, s, 127), 0, 127);
        return;
    }
    if (k == 1u)                                        /* SOUND: SIDE, PITCH, PMOD */
        dx_ui_side = s > 0;
    else
        v[k == 2u ? DXP_PITCH : DXP_PMOD] = (uint8_t)clamp(v[k == 2u ? DXP_PITCH : DXP_PMOD] + accel(EN_K1 + k, s, 127), 0, 127);
}
/* KIT 3 (the whole kit): KNOB 1 BP, 2 FEEDBK (TONE's, dx_tone), (0.7.9) 3 HANDS (the CLAP's, dx_hands) */
static void dx_kit3_knob(uint32_t k, int32_t s)
{
    uint32_t bp = DX_TONE_BP(dx_tone), fb = DX_TONE_FB(dx_tone);
    if (k == 0u) bp = (uint32_t)clamp((int32_t)bp + (s > 0 ? 1 : -1), 0, 15);
    else if (k == 1u) fb = (uint32_t)clamp((int32_t)fb + (s > 0 ? 1 : -1), 0, 15);
    else if (k == 2u) dx_hands = (uint8_t)clamp((int32_t)dx_hands + (s > 0 ? 1 : -1), 0, 15);
    dx_tone = (uint8_t)(bp | fb << 4);
}
static void dx_kit3_cards(void)
{
    uint32_t bp = DX_TONE_BP(dx_tone), fb = DX_TONE_FB(dx_tone);
    char val[12];
    if (bp) {                                           /* (its centre, as the filter sets it) */
        uint32_t hz = CUTOFF_HZ[78u + bp * 2u];
        if (hz < 1000u) fmt_int(val, (int32_t)hz);
        else fmt_fix(val, (int32_t)(hz / 100u), 1);
    } else
        str_cpy(val, "OFF", sizeof val);
    draw_column(0, "BP", val, bp ? (CUTOFF_HZ[78u + bp * 2u] < 1000u ? "Hz" : "kHz") : "", bp ? VAL(0u) : T_DIM,
                (int32_t)bp * 1000 / 15, ICON_TONE);
    fmt_int(val, (int32_t)fb);
    draw_column(1, "FEEDBK", fb ? val : "OFF", "", fb ? VAL(1u) : T_DIM, (int32_t)fb * 1000 / 15, ICON_DRIVE);
    fmt_int(val, (int32_t)dx_hands);                    /* (0.7.9) the CLAP's HANDS */
    draw_column(2, "HANDS", dx_hands ? val : "OFF", "", dx_hands ? VAL(2u) : T_DIM, (int32_t)dx_hands * 1000 / 15, ICON_AUTO);
    draw_column(3, "", "", "", T_THEME, -1, ICON_NONE);
}

/* the four cards of a SOUND page or X-MOD */
static void dx_cards(const page_t *pg)
{
    static const char *const BUS[4] = {"OFF", "CHO", "DLY", "REV"};
    const dx_lane_t *L;
    const uint8_t *v;
    uint32_t pn = dx_pagen(pg);
    char val[12], ln[8];
    dx_follow();
    L = dx_ui_patch(dx_ui_lane);
    v = dx_ui_side ? L->b : L->a;
    if (pn == 4u) {                                     /* (JIANT 0.5.1) X-MOD */
        static const char *const RT[9] = {"OFF", "4BAR", "2BAR", "1BAR", "1/2", "1/4", "1/8", "1/16", "1/32"};
        static const char *const SH[5] = {"SINE", "TRI", "SAW", "RAMP", "S&H"};
        draw_column(0, "RATE", RT[dx_mot[XM_RATE] % 9u], "", dx_mot[XM_RATE] ? VAL(0u) : T_DIM, dx_mot[XM_RATE] * 125,
                    ICON_RATE);
        fmt_int(val, dx_mot[XM_DPTH]);
        draw_column(1, "DEPTH", val, "", dx_mot[XM_DPTH] ? VAL(1u) : T_DIM, dx_mot[XM_DPTH] * 1000 / 127, ICON_LEVEL);
        draw_column(2, "SHAPE", SH[dx_mot[XM_SHPE] % 5u], "", VAL(2u), dx_mot[XM_SHPE] * 250, ICON_WAVE);
        fmt_int(val, dx_mot[XM_RAND]);
        draw_column(3, "RAND", val, "", dx_mot[XM_RAND] ? VAL(3u) : T_DIM, dx_mot[XM_RAND] * 1000 / 127, ICON_NOISE);
        return;
    }
    str_cpy(ln, DX_LANE_ABBR[dx_ui_lane], sizeof ln);   /* KNOB 1: the sound (SOUND 2 its side too: the values') */
    if (pn == 2u) {
        ln[2] = ' ';
        ln[3] = dx_ui_side ? 'B' : 'A';
        ln[4] = 0;
    }
    draw_column(0, "SOUND", ln, "", VAL(0u), (int32_t)dx_ui_lane * 1000 / (DV_NLANE12 - 1), ICON_X_PATTERN);
    if (pn == 8u) {                                     /* SOUND 4 */
        uint32_t ly = *dx_ui_layer(dx_ui_lane), la = DX_LAY_L(ly), fb = DX_LAY_FB(ly), sn = *dx_ui_send(dx_ui_lane);
        int32_t ix = dx_send_ix(sn);
        fmt_int(val, (int32_t)la);
        draw_column(1, "LAYER", la ? val : "OFF", "", la ? VAL(1u) : T_DIM, (int32_t)la * 1000 / 15, ICON_WAVE);
        fmt_int(val, (int32_t)fb);
        draw_column(2, "FEEDBK", fb ? val : "OFF", "", fb ? VAL(2u) : T_DIM, (int32_t)fb * 1000 / 15, ICON_DRIVE);
        {
            char un[8];
            fmt_int(un, ix ? ((ix - 1) % 16 + 1) * 100 / 16 : 0);
            str_cpy(un + str_len(un), "%", 2);
            draw_column(3, "SEND", BUS[DXS_BUS(sn)], ix ? un : "", ix ? VAL(3u) : T_DIM, ix * 1000 / 48, ICON_MIX);
        }
        return;
    }
    if (pn == 3u) {                                     /* SOUND 3 */
        static const char *const PM[4] = {"DECAY", "LONG", "NOISE", "SINE"};
        dx_mode_name(val, L->mode);
        draw_column(1, "MODE", val, "", VAL(1u), (int32_t)dx_mode_ix(L->mode) * 1000 / 23, ICON_WAVE);
        draw_column(2, "PM.TYPE", PM[(L->mode >> 5) & 3u], "", VAL(2u), (int32_t)((L->mode >> 5) & 3u) * 333, ICON_ENV);
        draw_column(3, "DRIVE", L->mode & DX_DRIVE ? "ON" : "OFF", "", L->mode & DX_DRIVE ? VAL(3u) : T_DIM,
                    L->mode & DX_DRIVE ? 1000 : 0, ICON_DRIVE);
        return;
    }
    if (pn == 2u) {                                     /* SOUND 2 */
        fmt_int(val, v[DXP_DECAY]);
        draw_column(1, "DECAY", val, "", VAL(1u), v[DXP_DECAY] * 1000 / 127, ICON_DECAY);
        fmt_int(val, v[DXP_NOISE]);
        draw_column(2, "NOISE", val, "", VAL(2u), v[DXP_NOISE] * 1000 / 127, ICON_NOISE);
        fmt_int(val, v[DXP_COLOR]);
        draw_column(3, "COLOR", val, "", VAL(3u), v[DXP_COLOR] * 1000 / 127, ICON_TONE);
        return;
    }
    draw_column(1, "SIDE", dx_ui_side ? "B" : "A", "", VAL(1u), dx_ui_side ? 1000 : 0, ICON_MIX);   /* SOUND */
    fmt_int(val, v[DXP_PITCH]);
    draw_column(2, "PITCH", val, "", VAL(2u), v[DXP_PITCH] * 1000 / 127, ICON_PITCH);
    fmt_int(val, v[DXP_PMOD]);
    draw_column(3, "PMOD", val, "", VAL(3u), v[DXP_PMOD] * 1000 / 127, ICON_ENV);
}
/* what the cards and the panel show changed */
static uint32_t dx_sig(void)
{
    const dx_lane_t *L = dx_ui_patch(dx_ui_lane);
    uint32_t h = (uint32_t)dx_ui_lane * 31u + dx_ui_side * 977u + L->mode * 131u, i;
    h ^= (uint32_t)dx_mot[0] << 24 ^ (uint32_t)dx_mot[1] << 16 ^ (uint32_t)dx_mot[2] << 8 ^ dx_mot[3];   /* (X-MOD) */
    h = (h ^ dx_tone ^ (uint32_t)dx_hands << 8) * 2654435761u;   /* (TONE, HANDS) */
    for (i = 0; i < DV_NLANE12; i++)                      /* (0.6.48) the SENDs, (0.6.49) the LAYERs */
        h = (h ^ *dx_ui_send(i) ^ (uint32_t)*dx_ui_layer(i) << 8) * 16777619u;
    for (i = 0; i < DXP_N; i++)
        h = (h ^ (uint32_t)L->a[i] << 8 ^ L->b[i]) * 16777619u;
    return h;
}
