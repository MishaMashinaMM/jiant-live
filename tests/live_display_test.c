/* SPDX-License-Identifier: GPL-3.0-only */
#define UI_TEST_CUSTOM_MAIN 1
#include "ui_test.c"
#include <assert.h>

int main(void)
{
    ui_power_on(); frame();
    assert(test_roll() == 0); /* immediate parameter feedback with decorative animation disabled */
    assert(test_menu_tabs() == 0);
    assert(test_large() == 0);
    ui_power_on(); frame();
    for (uint32_t p = 0; p < MP_COUNT; p++)
        for (uint32_t k = 0; k < 4; k++)
            assert(MPAGE[p].row[k] != MI_COLOR && MPAGE[p].row[k] != MI_SKIN && MPAGE[p].row[k] != MI_STYLE && MPAGE[p].row[k] != MI_LARGE && MPAGE[p].row[k] != MI_ANIM);
    hold(B_HOME);
    for (uint32_t p = 0; p < MP_COUNT; p++) {
        assert(menu_page() == p && ui.menu_sel == MPAGE[p].row[0]);
        for (uint32_t k = 0; k < 4; k++) {
            turn(EN_K1 + k, 1);
            assert(ui.menu_sel != MI_COLOR && ui.menu_sel != MI_SKIN && ui.menu_sel != MI_STYLE && ui.menu_sel != MI_LARGE && ui.menu_sel != MI_ANIM);
        }
        if (p + 1 < MP_COUNT) turn(EN_SELECT, 1);
    }
    menu_close();
    for (uint32_t flags = 0; flags < 256; flags++) {
        persist_t p = {0}, q;
        p.magic = PERSIST_MAGIC;
        p.favorites.factory[15][30] = (uint8_t)flags;
        assert(settings_import(&p, sizeof p) == 1);
        assert((ui_prefs & 34u) == 34u && ANIM_SNAP() && UI_LARGE_ON);
        assert((ui_prefs & ~34u) == (flags & ~34u));
        assert(menu_get(MI_LARGE) == 1 && menu_get(MI_ANIM) == 1);
        q = p;
        assert(settings_import(&q, sizeof q) == 1 && !memcmp(&p, &q, sizeof p));
        for (uint32_t v = 0; v < 2; v++) {
            menu_put(MI_LARGE, v); menu_put(MI_ANIM, v);
            assert(menu_get(MI_LARGE) == 1 && menu_get(MI_ANIM) == 1);
        }
        project_capture(&proj_scratch); project_restore_runtime(&proj_scratch);
        assert((ui_prefs & 34u) == 34u && ANIM_SNAP() && UI_LARGE_ON);
    }
    puts("Live display: navigation, all legacy flag bytes, immutable settings and project restore passed");
    return 0;
}
