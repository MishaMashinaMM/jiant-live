/* SPDX-License-Identifier: GPL-3.0-only
 * Copyright (C) 2026 Leo Kuroshita (@kurogedelic), Hügelton Instruments
 * Portions Copyright (C) 2026 Juan Ignacio Antonucci (Juan Jiant)
 * (JIANT 0.9.12: from Felucca 1.5's midi_learn.c, modified 2026-10-18: six tracks, three bytes an entry, ten of them,
 * MENU > MIDI > LEARN to arm it, LEARN CLEAR; the map, its CCs and its hook: midi_control.c) */
/* MIDI LEARN (Felucca 1.5, Discussion #170): a controller's CC set to any knob's parameter, on the device. MENU > MIDI >
 * LEARN ON (menu_items.c) arms it; then a knob turned on any page that edits a track's parameter (HOME, the EDIT and
 * sound pages, ENV, LFO, FX, MIXER: ui_input.c) picks that parameter of that track; the next CC that comes (midi_control.c
 * midi_learned: any channel ROUT hears, USB or TRS) is set to it, and stays until something else is learned on it. Then
 * turn the next knob. LEARN OFF (or the menu's LEARN CLEAR, every CC) ends it. One CC sets one parameter, a parameter
 * has one CC: learning one again replaces both. A CC with a meaning of its own is refused ("CC64 RESERVED"); with
 * ML_N learned, "LEARN FULL". Saved with the settings as each one changes (deferred while playing) */
static uint8_t *const ml_tab = &favorites.factory[ENGI_DIGITAL][2];
typedef char ml_row_fits[2u + 3u * ML_N <= sizeof favorites.factory[0] ? 1 : -1];

static uint32_t ml_code(uint32_t id) { return id >= P_E0 ? ML_E0 + id - P_E0 : id + 1u; }
static void ml_put(uint32_t i, uint32_t cc, uint32_t k, uint32_t code)   /* (the ISR reads the three: never part of one) */
{
    uint8_t *e = ml_tab + 3u * i;
    fm1_irq_off();
    e[0] = (uint8_t)cc;
    e[1] = (uint8_t)k;
    e[2] = (uint8_t)code;
    fm1_irq_on();
}
static int ml_used(uint32_t i) { return (ml_tab[3u * i] & 0x80u) != 0u; }
/* the CC of parameter id on track k, + 1; 0 none */
static uint32_t ml_cc_of(uint32_t k, uint32_t id)
{
    uint32_t i;
    for (i = 0; i < ML_N; i++)
        if (ml_used(i) && ml_tab[3u * i + 1u] == k && ml_tab[3u * i + 2u] == ml_code(id))
            return (ml_tab[3u * i] & 127u) + 1u;
    return 0;
}
static uint32_t ml_count(void)
{
    uint32_t i, n = 0;
    for (i = 0; i < ML_N; i++)
        n += (uint32_t)ml_used(i);
    return n;
}
/* cc -> parameter id of track k, in place of what either had; 1 learned, 0 no room (ML_N others), 2 a CC never learned */
static uint32_t ml_learn(uint32_t cc, uint32_t k, uint32_t id)
{
    uint32_t i, f = ML_N;
    if (!ml_free_cc(cc) || k >= NTRK || id >= P_COUNT)
        return 2;
    for (i = 0; i < ML_N; i++)
        if (ml_used(i) && ((ml_tab[3u * i] & 127u) == cc || (ml_tab[3u * i + 1u] == k && ml_tab[3u * i + 2u] == ml_code(id))))
            ml_put(i, 0, 0, 0);
    for (i = ML_N; i--;)
        if (!ml_used(i))
            f = i;
    if (f == ML_N)
        return 0;
    ml_put(f, 0x80u | cc, k, ml_code(id));
    return 1;
}
/* every learned CC cleared (MENU > MIDI > LEARN CLEAR), the settings saved; 1 = there were */
static int ml_clear_all(void)
{
    uint32_t i, n = ml_count();
    for (i = 0; i < ML_N; i++)
        ml_put(i, 0, 0, 0);
    if (n)
        settings_save();                             /* (deferred while playing) */
    return n != 0u;
}
/* MENU > MIDI > LEARN: on (armed, no parameter yet) / off */
static void ml_set(uint32_t on)
{
    ml.arm = ml.heard = 0;
    ml.pick = 0;
    ml.on = (uint8_t)(on != 0u);
    if (on)
        ui_message("LEARN: TURN A KNOB");
}
/* the picked parameter's name: "T2 CUT" (b: 16 bytes) */
static void ml_name(char *b)
{
    b[0] = 'T';
    b[1] = (char)('1' + ml.trk % NTRK);
    b[2] = ' ';
    str_cpy(b + 3, track_desc(&trk[ml.trk % NTRK], ml.id)->label, 12);
}
/* a knob turned parameter id of track t (ui_input.c): picked while learning; "LEARN T2 CUT: SEND A CC" (its CC if it
 * has one) */
static void ml_knob(const track_t *t, uint32_t id)
{
    char b[16], m[24];
    uint32_t cc;
    if (!ml.on || id >= P_COUNT || (ml.pick && ml.trk == trk_index(t) && ml.id == id))
        return;
    ml.trk = (uint8_t)trk_index(t);
    ml.id = (uint8_t)id;
    ml.pick = 1;
    ml.heard = 0;
    ml.arm = 1;
    ml_name(b);
    str_cpy(m, "LEARN ", sizeof m);
    str_cpy(m + 6, b, sizeof m - 6u);
    cc = ml_cc_of(ml.trk, id);
    if (cc) {
        str_cpy(b, " = CC", sizeof b);
        fmt_int(b + 5, (int32_t)cc - 1);
    } else {
        str_cpy(b, ": SEND A CC", sizeof b);
    }
    ui_say(m, b);
}
/* each frame (ui_input): a CC heard for the picked parameter is learned, "CC21 > T2 CUT" */
static void ml_poll(void)
{
    uint32_t cc = ml.heard, r;
    char a[12], b[16];
    if (!cc)
        return;
    ml.heard = 0;
    if (!ml.on || !ml.pick)
        return;
    cc--;
    a[0] = 'C';
    a[1] = 'C';
    fmt_int(a + 2, (int32_t)cc);
    r = ml_learn(cc, ml.trk, ml.id);
    if (r != 1u) {
        ui_say(a, r ? " RESERVED" : ": LEARN FULL");   /* (still waiting for one) */
        return;
    }
    str_cpy(a + str_len(a), " > ", 4);
    ml_name(b);
    ui_say(a, b);
    ml.pick = 0;
    ml.arm = 0;
    settings_save();                                 /* (deferred while playing) */
}
