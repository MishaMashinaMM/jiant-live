/* SPDX-License-Identifier: GPL-3.0-only */
/* Real settings/panel serialization; presentation is stubbed so no fonts are needed. */
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#define __attribute__(x)
#define NPALETTES 20u
#define UI_JIANT_INDEX 19u
#define SKIN_AIR 3u
#define NENGINES 16u
#define UP_SLOTS 32u
static uint8_t fx_lowcut;
static int32_t fm1_enc_take(uint32_t i) { (void)i; return 0; }
static uint32_t palette_from_stored(uint32_t i) { return i % NPALETTES; }
static uint32_t palette_to_stored(uint32_t i) { return i; }
static void palette_set(uint32_t i) { (void)i; }
#include "../firmware/src/panel.c"
#include "../firmware/src/favorites.c"
#include "../firmware/src/settings_persist.c"

int main(void)
{
    persist_t p = {0}, q;
    panel = PANEL_DEFAULT;
    settings_init(); settings_fresh();
    for (uint32_t i = 0; i < 6u; i++) assert(midi_rx[i] == i + 1u);
    favorites.factory[15][28] = 0x80; /* TRACKS 6 */
    favorites.factory[1][2] = 0x9E;   /* learned CC */
    settings_export(&p);
    assert(p.magic == PERSIST_MAGIC && sizeof p == PERSIST_V5_SIZE + 68u);
    assert((uint8_t *)&p.midi_rx - (uint8_t *)&p == PERSIST_V4_SIZE);
    for (uint32_t i = 0; i < 6u; i++) midi_rx[i] = i < 3 ? 16 : 1;
    settings_export(&p);
    memset(midi_rx, 0, sizeof midi_rx);
    assert(settings_import(&p, sizeof p) == 1);
    for (uint32_t i = 0; i < 6u; i++) assert(midi_rx[i] == (i < 3 ? 16u : 1u));
    q = p; assert(settings_import(&q, sizeof q) == 1 && !memcmp(&q, &p, sizeof p));
    puts("RX settings: global defaults, shared channels, round-trip and idempotence passed");

    /* PER5's exact prefix is accepted and its RX assignments remain intact. */
    q = p; q.magic = PERSIST_MAGIC_V5;
    memset(q.ml_extra, 0xFF, sizeof q.ml_extra); /* not part of the old record */
    assert(settings_import(&q, PERSIST_V5_SIZE) == 2);
    for (uint32_t i = 0; i < 6u; i++) assert(midi_rx[i] == (i < 3 ? 16u : 1u));
    assert(q.magic == PERSIST_MAGIC && q.ml_extra[0] == 0 && q.ml_extra[65] == 0);
    puts("RX settings: PER5 migration preserves channels and clears new learned entries");

    q = p; q.magic = PERSIST_MAGIC_V4;
    memset(q.midi_rx, 0xFF, sizeof q.midi_rx); /* not present in PER4 */
    assert(settings_import(&q, PERSIST_V4_SIZE) == 2);
    for (uint32_t i = 0; i < 6u; i++) assert(midi_rx[i] == i + 1u);
    assert(q.favorites.factory[15][28] == 0x80 && q.favorites.factory[1][2] == 0x9E);
    assert(q.magic == PERSIST_MAGIC);
    q.midi_rx[0] = 0; q.midi_rx[1] = 17; q.midi_rx[2] = 16;
    assert(settings_import(&q, sizeof q) == 1);
    assert(midi_rx[0] == 1 && midi_rx[1] == 2 && midi_rx[2] == 16);
    puts("RX settings: PER4 prefix/settings preserved; invalid entries default individually");

    q = p; q.magic = 0x50455233u;
    assert(settings_import(&q, PERSIST_V4_SIZE - sizeof q.favorites) == 2);
    q = p; q.magic = 0x50455232u;
    assert(settings_import(&q, PERSIST_V4_SIZE - sizeof q.favorites - sizeof q.bold) == 2);
    q = p; q.magic = 0x50455231u;
    memcpy((uint8_t *)&q + 8, &PANEL_DEFAULT, sizeof panel);
    assert(settings_import(&q, 8 + sizeof panel) == 2);
    for (uint32_t i = 0; i < 6u; i++) assert(midi_rx[i] == i + 1u);
    q = p; q.magic = 0;
    midi_rx[0] = 12;
    assert(!settings_import(&q, sizeof q) && midi_rx[0] == 12);
    assert(!settings_import(&p, sizeof p - 1));
    puts("RX settings: PER1/2/3 migration and malformed-record rejection passed");
    return 0;
}
