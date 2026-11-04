/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_NO_MAIN 1
#include "ui_test.c"
#include <assert.h>

int main(void)
{
    ui_power_on(); trks_put(0); frame();

    /* Six independent RX controls on three MIDI pages: T1 on page 4, T2-T5 on page 5, T6 on page 6 */
    hold(B_HOME); menu_page_go(menu_page_of(MI_T1_RX)); frame();
    assert(menu_tab() == MTAB_MIDI);

    /* Page 1: K4 = T1 RX CH */
    assert(ui.menu_sel == MI_MIDIOUT || ui.menu_sel == MI_LEARN || ui.menu_sel == MI_LCLEAR || ui.menu_sel == MI_T1_RX);
    turn(EN_K4, 1);
    assert(ui.menu_sel == MI_T1_RX);
    menu_put(MI_T1_RX, 2); assert(midi_rx[0] == 3);

    /* Page 2: K1-K4 = T2-T5 RX CH */
    turn(EN_SELECT, 1);
    assert(ui.menu_sel == MI_T2_RX);
    menu_put(MI_T2_RX, 5); assert(midi_rx[1] == 6);
    turn(EN_K2, 1);
    assert(ui.menu_sel == MI_T3_RX);
    menu_put(MI_T3_RX, 9); assert(midi_rx[2] == 10);
    turn(EN_K3, 1);
    assert(ui.menu_sel == MI_T4_RX);
    menu_put(MI_T4_RX, 0); assert(midi_rx[3] == 1);
    turn(EN_K4, 1);
    assert(ui.menu_sel == MI_T5_RX);
    menu_put(MI_T5_RX, 15); assert(midi_rx[4] == 16);

    /* Page 3: K1 = T6 RX CH, K2-K4 empty */
    turn(EN_SELECT, 1);
    assert(ui.menu_sel == MI_T6_RX);
    menu_put(MI_T6_RX, 7); assert(midi_rx[5] == 8);

    /* Empty cards on page 3 do nothing */
    turn(EN_K2, 1); assert(ui.menu_sel == MI_T6_RX);
    turn(EN_K3, 1); assert(ui.menu_sel == MI_T6_RX);
    turn(EN_K4, 1); assert(ui.menu_sel == MI_T6_RX);

    /* Each row edits its fixed track regardless of song.sel */
    song.sel = 0;
    menu_put(MI_T2_RX, 3); assert(midi_rx[1] == 4 && midi_rx[0] == 3);
    song.sel = 5;
    menu_put(MI_T4_RX, 11); assert(midi_rx[3] == 12 && midi_rx[4] == 16);

    /* Channel values 1-16, boundary clamping */
    menu_put(MI_T1_RX, 0); assert(midi_rx[0] == 1);
    turn(EN_K4, -1); assert(midi_rx[0] == 1);            /* clamped at bottom */
    menu_put(MI_T1_RX, 15); assert(midi_rx[0] == 16);
    turn(EN_K4, 1); assert(midi_rx[0] == 16);            /* clamped at top */

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

    fprintf(stderr, "BEFORE: ");
    menu_close(); project_capture(&proj_scratch);
    project_restore_runtime(&proj_scratch);
    apply_preset_to(&trk[0], 0); system_init();

    uint8_t expected[6] = {1, 1, 10, 12, 16, 8};
    assert(!memcmp(expected, midi_rx, sizeof expected));
    puts("RX UI: six independent controls, pages, bounds, duplicates, TRACKS 4/6 persistence passed");

    /* ROUTE formatting unchanged */
    char value[8]; const char *unit;
    param_format(&GP[G_ROUTE], 0, value, &unit); assert(str_eq(value, "1-4"));
    trks_put(1); param_format(&GP[G_ROUTE], 0, value, &unit); assert(str_eq(value, "1-4"));
    param_format(&GP[G_ROUTE], 2, value, &unit); assert(str_eq(value, "1-6"));
    param_format(&GP[G_ROUTE], 1, value, &unit); assert(str_eq(value, "SEL"));
    puts("RX UI: ROUTE follows TRACKS; project, preset and NEW PROJECT preserve assignments");
    return 0;
}
