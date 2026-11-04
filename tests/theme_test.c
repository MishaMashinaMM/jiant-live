/* SPDX-License-Identifier: GPL-3.0-only */
/* The palettes and the real canvas/text renderer (src/gfx.c): historical identities select the same exact JIANT colors,
 * JIANT text contrast, the alpha blend, the ramp cache, the fonts' metrics and the ellipsis.
 * Optional: a palette sheet (PPM). */
#include <stdint.h>
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <string.h>
#define __attribute__(x)
static void lcd_sync(void) {}
static void lcd_power(uint32_t s) { (void)s; }   /* (MENU > SCREEN OFF: lcd.c) */
static void lcd_wake_now(void) {}
static void lcd_blit(uint32_t x, uint32_t y, uint32_t w, uint32_t h, const uint16_t *p)
{ (void)x; (void)y; (void)w; (void)h; (void)p; }
#include "../firmware/src/gfx.c"

#define SHEET_W 960u
#define SHEET_H (((NPALETTES + 3u) / 4u) * 124u)
static uint16_t sheet[SHEET_W * SHEET_H];
static unsigned channel(uint16_t c, unsigned n)
{ return n == 0 ? c >> 11 : n == 1 ? (c >> 5) & 63 : c & 31; }
static int gray(uint16_t c) { return channel(c, 0) == channel(c, 2) && channel(c, 1) == channel(c, 0) * 2u; }
/* MONO's blends: R = B, G within 2 of 2 R (blended per channel, the 5-6-5 rounding) */
static int neutral(uint16_t c)
{ return channel(c, 0) == channel(c, 2) && (int)channel(c, 1) - 2 * (int)channel(c, 0) >= -2 && (int)channel(c, 1) - 2 * (int)channel(c, 0) <= 2; }
static double luminance(uint16_t color)
{
    const double weight[] = {0.2126, 0.7152, 0.0722};
    double result = 0;
    for (unsigned c = 0; c < 3; c++) {
        double v = channel(color, c) / (c == 1 ? 63.0 : 31.0);
        result += weight[c] * (v <= 0.04045 ? v / 12.92 : pow((v + 0.055) / 1.055, 2.4));
    }
    return result;
}
static double contrast(uint16_t fg, uint16_t bg)
{
    double a = luminance(fg), b = luminance(bg);
    return a > b ? (a + 0.05) / (b + 0.05) : (b + 0.05) / (a + 0.05);
}

int main(int argc, char **argv)
{
    static const aagh_t pg[1] = {{0, 48}};            /* one glyph 'A': 3 px, alpha 0, 8, 15 (bx 0, by 0, bw 3, bh 1) */
    static const uint32_t pp[1] = {1u << 10 | 3u << 19 | 1u << 26};
    static const uint8_t pd[] = {0x08, 0xf0};
    const aafont_t probe = {1, 1, 'A', 'A', 0, 0, pg, pp, pd, 0, 0, 0};
    double worst = 100;
    assert(NPALETTES == 11u && !strcmp(UI_PALETTES[0].name, "GREY") && !strcmp(UI_PALETTES[7].name, "HI-CON") &&
           !strcmp(UI_PALETTES[8].name, "NIGHT") && !strcmp(UI_PALETTES[9].name, "MONO") && UI_BW_INDEX == 9u &&
           !strcmp(UI_PALETTES[10].name, "JIANT") && UI_JIANT_INDEX == 10u);   /* (JIANT 0.1: appended) */
    assert(sizeof(UI_PALETTES) / sizeof(UI_PALETTES[0]) == 11u);
    assert(sizeof(ui_pal_t) == sizeof(const char *)); /* identities contain no rendering placeholders */
    static const char *const names[11] = {"GREY", "GREEN", "AMBER", "ICE", "VIOLET", "ROSE", "PAPER", "HI-CON", "NIGHT", "MONO", "JIANT"};
    static const uint8_t migrated[20] = {1,2,3,5,0,4,5,3,2,3,4,7,6,6,6,6,6,6,6,7};
    assert(palette_to_stored(UI_JIANT_INDEX) == 74u);
    for (unsigned p = 0; p < 11; p++) {
        assert(!strcmp(UI_PALETTES[p].name, names[p]));
        assert(palette_from_stored(64u + p) == p && palette_to_stored(p) == 64u + p);
    }
    for (unsigned v = 0; v < 256; v++) {
        assert(palette_stored_ok(v) == (v < 20u || (v >= 64u && v <= 74u)));
        assert(palette_from_stored(v) == (v < 20u ? migrated[v] : v >= 64u && v <= 74u ? v - 64u : 0u));
    }
    assert(sizeof(UI_JIANT_COLORS) == 5u * sizeof(uint16_t));
    assert(UI_JIANT_COLORS.bg == 0x0000 && UI_JIANT_COLORS.surf == 0x0841 &&
           UI_JIANT_COLORS.text == 0xf79d && UI_JIANT_COLORS.theme == 0xbb9f && UI_JIANT_COLORS.accent == 0xfbc0);
    static const uint16_t knobs[4] = {0x8b7f, 0xfe87, 0x5bbf, 0xfa49};
    assert(!memcmp(knobs, UI_JIANT_KNOBS, sizeof knobs) && UI_JIANT_RAISE_PCT == 4);
    ux.style = ST_FLAT;
    for (unsigned p = 0; p < NPALETTES; p++) {
        uint16_t *tok = &ux.bg;
        palette_set(p);
        assert(!ux.mono && !ux.light && ux.chamfer && T_BG == 0 && T_TEXT == 0xf79d && T_THEME == 0xbb9f);
        /* text and the things read on the screen; the generator checks the same (tools/gen_ui_palettes.py) */
        struct { uint16_t fg, bg; double min; } c[] = {
            {T_TEXT, T_BG, 12.0}, {T_TEXT, T_SURF, 9.0}, {T_MID, T_BG, 5.0}, {T_MID, T_SURF, 4.0},
            {T_THEME, T_BG, 4.5}, {T_THEME, T_SURF, 4.5}, {T_ACCENT, T_BG, 4.5}, {T_ACCENT, T_SURF, 4.5},
            {T_INK, T_SEL, 4.5}, {T_INK, T_THEME, 4.5}, {T_DIM, T_BG, 2.2}, {T_LINE, T_BG, 1.25}, {T_REC, T_BG, 3.0},
            {T_INK, T_KEY, 4.5}, {T_KEY, T_BG, 3.0}, {T_KEY, T_SURF, 2.5}, {T_INK, T_DIM, 1.8}, {T_INK, T_ACCENT, 4.5},
            {T_TEXT, T_RAISE, 7.0}};
        double min = 100;
        for (unsigned i = 0; i < sizeof c / sizeof c[0]; i++) {
            double r = contrast(c[i].fg, c[i].bg);
            if (r < c[i].min) printf("%s: pairing %u contrast %.2f < %.2f\n", UI_PALETTES[p].name, i, r, c[i].min);
            assert(r >= c[i].min);
            if (r / c[i].min < min) min = r / c[i].min;
        }
        if (min < worst) worst = min;
        printf("%-7s text %.1f:1, labels %.1f:1, values %.1f:1, selection %.1f:1\n", UI_PALETTES[p].name,
               contrast(T_TEXT, T_BG), contrast(T_MID, T_BG), contrast(T_THEME, T_BG), contrast(T_INK, T_SEL));
        assert(!ux.light);                         /* PAPER is the light one */
        assert(T_REC == 0xfa49);
        /* the blend: transparent, solid and an edge between the two, on two backgrounds */
        for (unsigned b = 0; b < 2; b++) {
            uint16_t under = b ? T_SEL : T_BG, ink = b ? T_INK : T_THEME;
            cv_begin(3, 1, under);
            cv_text_on(0, 0, &probe, "A", ink, under);
            assert(swap16(cv_px[0]) == under && swap16(cv_px[2]) == ink);
            for (unsigned k = 0; k < 3; k++) {
                unsigned a = channel(under, k), z = channel(ink, k), m = channel(swap16(cv_px[1]), k);
                assert(m >= (a < z ? a : z) && m <= (a > z ? a : z));   /* no halo */
            }
        }
        assert(text_w(&AF_S, UI_PALETTES[p].name) <= 60);     /* the COLOR row's value */
        cv_begin(240, 124, T_BG);
        cv_text(4, 2, &AF_M, UI_PALETTES[p].name, T_TEXT);
        for (unsigned k = 0; k < 5; k++) cv_rect(150 + k * 16, 4, 12, 12, tok[k]);
        cv_text(4, 26, &AF_S, "LABEL", T_MID);
        cv_text(70, 22, &AF_M, "VALUE 127", T_THEME);
        cv_text(170, 22, &AF_M, "HOT", T_ACCENT);
        cv_text(4, 46, &AF_S, "INACTIVE", T_DIM);
        cv_rect(80, 44, 150, 17, T_SEL);
        cv_text_on(86, 45, &AF_S, "SELECTED ROW", T_INK, T_SEL);
        cv_rect(4, 66, 232, 1, T_LINE);
        cv_rect(4, 72, 120, 3, T_LINE);
        cv_rect(4, 72, 70, 3, T_SEL);
        cv_rect(74, 70, 1, 7, T_THEME);
        cv_text(136, 66, &AF_S, "REC", T_REC);
        cv_text(4, 82, &AF_L, "124", T_THEME);
        cv_text(80, 92, &AF_S, "Mixed case text", T_TEXT);
        cv_key_hint(80, 108, KC_OCTUP, "LOAD", 1, T_BG);          /* keycaps: available, unavailable */
        cv_key_hint(150, 108, KC_OCTDN, "BACK", 0, T_BG);
        for (unsigned y = 0; y < 124; y++)
            for (unsigned x = 0; x < 240; x++) {
                uint16_t v = swap16(cv_px[y * 240 + x]);
                sheet[(p / 4 * 124 + y) * 960 + p % 4 * 240 + x] = v;
            }
    }
    /* Every alpha entry, on JIANT functional foreground/background pairs, exactly follows
     * the pre-cleanup dark-ink curve and per-channel rounding. Exercise cache eviction too. */
    static const uint8_t curve[16] = {0,24,43,62,80,97,114,130,147,163,178,194,210,225,240,255};
    uint16_t colors[] = {T_BG,T_SURF,T_TEXT,T_THEME,T_ACCENT,T_MID,T_DIM,T_RAISE,T_KEY,T_REC};
    for (unsigned f = 0; f < 10; f++) for (unsigned b = 0; b < 10; b++) {
        const uint16_t *r = ramp(colors[f], colors[b]);
        for (unsigned a = 0; a < 16; a++) {
            uint16_t fg = colors[f] == colors[b] ? T_BG : colors[f], out = 0;
            const unsigned shifts[3] = {11,5,0};
            for (unsigned k = 0; k < 3; k++) {
                int x = channel(colors[b],k), y = channel(fg,k);
                out |= (uint16_t)(x + (((y-x)*curve[a]+128)>>8)) << shifts[k];
            }
            assert(swap16(r[a]) == out);
        }
    }
    uint32_t generation = ux.gen;
    palette_set(UI_JIANT_INDEX);
    assert(ux.gen == generation + 1);
    (void)ramp(T_THEME,T_BG); assert(rc.gen == ux.gen);
    /* fonts: Chakra Petch (JIANT 0.5) S 12 px; (0.8.9) M Inter Tight SemiBold 15 px (the values, as the 0.6), L Michroma
     * 18 px; tabular digits; the ellipsis; L has capitals only */
    assert(AF_S.h == 16 && AF_S.asc == 12 && AF_M.h == 19 && AF_M.asc == 15 && AF_L.h == 26 && AF_L.asc == 21);
    assert(text_w(&AF_M, "0000") == text_w(&AF_M, "1111") && text_w(&AF_S, "1.25") == text_w(&AF_S, "8.75"));
    assert(glyph(&AF_S, (uint8_t)ELLIPSIS) != glyph(&AF_S, '?') && glyph(&AF_M, (uint8_t)ELLIPSIS) != glyph(&AF_M, '?'));
    assert(text_w(&AF_L, "abc") == text_w(&AF_L, "ABC"));
    {
        char b[32];
        assert(!text_fit(b, sizeof b, "KICK", &AF_S, 60) && !strcmp(b, "KICK"));
        assert(text_fit(b, sizeof b, "A VERY LONG PRESET NAME", &AF_S, 60) && text_w(&AF_S, b) <= 60);
        assert(b[strlen(b) - 1] == ELLIPSIS && b[strlen(b) - 2] != ' ');
        assert(text_fit(b, sizeof b, "WWWWWWWW", &AF_M, 1) && b[0] == ELLIPSIS && !b[1]);
    }
    if (argc > 1) {
        FILE *f = fopen(argv[1], "wb"); assert(f);
        fprintf(f, "P6\n%u %u\n255\n", (unsigned)SHEET_W, (unsigned)SHEET_H);
        for (unsigned i = 0; i < SHEET_W * SHEET_H; i++) {
            fputc(channel(sheet[i], 0) * 255 / 31, f);
            fputc(channel(sheet[i], 1) * 255 / 63, f);
            fputc(channel(sheet[i], 2) * 255 / 31, f);
        }
        fclose(f);
    }
    printf("JIANT: 11 historical identities, exact colors/knobs/text ramp, contrast (worst margin x%.2f), blending, ramp cache, font metrics and ellipsis passed.\n", worst);
    return 0;
}
