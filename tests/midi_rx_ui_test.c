/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>

int main(void)
{
    ui_power_on(); trks_put(0); frame();

    /* The device has one MIDI page; RX assignments live in GLO > MIXER.
     * Their stable MENU IDs remain for the editor protocol, but are not
     * reachable from the physical MENU's cards or page navigation. */
    hold(B_HOME); menu_page_go(mtab_page(MTAB_MIDI)); frame();
    assert(MP_COUNT == 8u);
    assert(menu_tab() == MTAB_MIDI);
    assert(ui.menu_sel == MI_MIDIOUT);
    assert(MPAGE[menu_page()].row[0] == MI_MIDIOUT);
    assert(MPAGE[menu_page()].row[1] == MI_LEARN);
    assert(MPAGE[menu_page()].row[2] == MI_LCLEAR);
    assert(MPAGE[menu_page()].row[3] == MI_NONE);
    for (uint32_t p = 0; p < MP_COUNT; p++)
        for (uint32_t k = 0; k < 4u; k++)
            assert(MPAGE[p].row[k] < MI_T1_RX || MPAGE[p].row[k] > MI_T6_RX);

    /* The empty MIDI card does not select or edit a hidden RX setting. */
    turn(EN_K4, 1);
    assert(ui.menu_sel == MI_MIDIOUT);
    turn(EN_SELECT, 1);
    assert(menu_tab() == MTAB_USB);
    turn(EN_SELECT, -1);
    assert(menu_tab() == MTAB_MIDI);

    /* The editor's six stable IDs still edit six independent tracks. */
    for (uint32_t i = 0; i < 6u; i++) {
        assert(mi_tab(MI_T1_RX + i) == MTAB_MIDI);
        assert(menu_n(MI_T1_RX + i) == 16u);
    }
    menu_put(MI_T1_RX, 2); assert(midi_rx[0] == 3);
    menu_put(MI_T2_RX, 5); assert(midi_rx[1] == 6);
    menu_put(MI_T3_RX, 9); assert(midi_rx[2] == 10);
    menu_put(MI_T4_RX, 0); assert(midi_rx[3] == 1);
    menu_put(MI_T5_RX, 15); assert(midi_rx[4] == 16);
    menu_put(MI_T6_RX, 7); assert(midi_rx[5] == 8);
    for (uint32_t i = 0; i < 6u; i++)
        assert(menu_get(MI_T1_RX + i) == midi_rx[i] - 1u);

    /* Each row edits its fixed track regardless of song.sel */
    song.sel = 0;
    menu_put(MI_T2_RX, 3); assert(midi_rx[1] == 4 && midi_rx[0] == 3);
    song.sel = 5;
    menu_put(MI_T4_RX, 11); assert(midi_rx[3] == 12 && midi_rx[4] == 16);

    /* Channel values 1-16, boundary clamping */
    menu_put(MI_T1_RX, 0); assert(midi_rx[0] == 1);
    assert(menu_get(MI_T1_RX) == 0);                  /* lower bound */
    menu_put(MI_T1_RX, 15); assert(midi_rx[0] == 16);
    assert(menu_get(MI_T1_RX) == 15);                 /* upper bound */

    /* Duplicate channel assignments allowed */
    menu_put(MI_T1_RX, 0); assert(midi_rx[0] == 1);
    menu_put(MI_T2_RX, 0); assert(midi_rx[1] == 1);
    assert(midi_rx[0] == midi_rx[1]);

    /* T5/T6 editable under TRACKS=4 (default) */
    assert(trk_live == 4);
    assert(midi_rx[4] == 16 && midi_rx[5] == 8);

    /* T5/T6 retained when switching TRACKS 4/6 */
    trks_put(1);                                          /* TRACKS=6 */
    assert(midi_rx[4] == 16 && midi_rx[5] == 8);
    trks_put(0);                                          /* TRACKS=4 */
    assert(midi_rx[4] == 16 && midi_rx[5] == 8);

    /* Editor IDs 25-30 retain their track associations */
    /* ED_MENU[id-25] = MI_T1_RX..MI_T6_RX, editor sets midi_rx[id-25] */
    menu_put(MI_T1_RX, 0); assert(midi_rx[0] == 1);

    menu_close(); project_capture(&proj_scratch);
    project_restore_runtime(&proj_scratch);
    apply_preset_to(&trk[0], 0); system_init();

    uint8_t expected[6] = {1, 1, 10, 12, 16, 8};
    assert(!memcmp(expected, midi_rx, sizeof expected));
    puts("RX UI: hidden MENU rows, six editor IDs, bounds, duplicates, TRACKS 4/6 persistence passed");

    /* ROUTE formatting unchanged */
    char value[8]; const char *unit;
    param_format(&GP[G_ROUTE], 0, value, &unit); assert(str_eq(value, "1-4"));
    trks_put(1); param_format(&GP[G_ROUTE], 0, value, &unit); assert(str_eq(value, "1-4"));
    param_format(&GP[G_ROUTE], 2, value, &unit); assert(str_eq(value, "1-6"));
    param_format(&GP[G_ROUTE], 1, value, &unit); assert(str_eq(value, "SEL"));
    /* Stage 11: 32 global learned assignments, full-table handling and PER6 persistence. */
    ml_clear_all();
    uint32_t cc = 2u, learned[ML_N];
    assert(ML_N == 32u);
    for (uint32_t i = 0; i < ML_N; i++) {
        while (cc < 120u && !ml_free_cc(cc)) cc++;
        assert(cc < 120u);
        learned[i] = cc;
        assert(ml_learn(cc++, i % NTRK, P_LEVEL + i / NTRK) == 1u);
    }
    assert(ml_count() == 32u);
    for (uint32_t i = 0; i < ML_N; i++)
        assert(ml_cc_of(i % NTRK, P_LEVEL + i / NTRK) == learned[i] + 1u);
    while (cc < 120u && !ml_free_cc(cc)) cc++;
    assert(cc < 120u && ml_learn(cc, 0, P_E0) == 0u);
    persist_t saved = {0};
    settings_export(&saved);
    assert(saved.magic == PERSIST_MAGIC);
    memset(ml_table, 0, sizeof ml_table);
    assert(settings_import(&saved, sizeof saved) == 1 && ml_count() == 32u);
    for (uint32_t i = 0; i < ML_N; i++)
        assert(ml_cc_of(i % NTRK, P_LEVEL + i / NTRK) == learned[i] + 1u);
    saved.magic = PERSIST_MAGIC_V5;
    assert(settings_import(&saved, PERSIST_V5_SIZE) == 2 && ml_count() == ML_OLD_N);
    puts("RX UI: 32 MIDI LEARN slots, capacity, PER6 round-trip and PER5 migration passed");

    puts("RX UI: ROUTE follows TRACKS; project, preset and NEW PROJECT preserve assignments");
    return 0;
}
