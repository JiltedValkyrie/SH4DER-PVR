/* SH4DER-PVR: host test of the suite menu, look B (../suite_view.h), and of the hold bar's place (sh4der_suite.h)

   demo/psm/suite/test/suite_view_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Runs the menu as suite_menu.c does, with buttons from a table and a clock of its own, and draws its screens into a
   buffer: the two tabs' rows from the data, the cursor that never rests on a heading, a held direction that repeats,
   the stick that moves only the card's text, the row that starts nothing (NOT IN THE MENU) with its grey chips, a
   start by A and by START, the credits, the menu's place through the mail's word and back, a title too long for
   double size, and every card's text inside its columns. The glyphs are blocks (every pixel of a character's cell
   but its last column and row), so a text pixel is the text's colour exactly and a text box can be found.

   Build (from the repository's root):
     gcc -std=c99 -O2 -Wall -Wextra -Werror demo/psm/suite/test/suite_view_test.c -o build/demo_test/suite_view_test
   -ppm <dir> also writes the screens as PPM files (never over a file that is there). With
   -DUI_MINIFONT -I<KallistiOS>/kernel/arch/dreamcast/util the glyphs are KallistiOS's minifont, to look at (the
   console's font is the BIOS font through the page kit, a little smoother at the same size); the pixel checks of
   text then do not run. With -DVIEW_REAL -I<a folder with suite_list.h and suite_cards.h> the data are a build's own
   in place of this file's tables. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../suite_view.h"
#include "../../../../include/sh4der_suite.h"

#ifdef UI_MINIFONT
#include "minifont.h"
#endif

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static uint16_t fb[640 * 480], fb2[640 * 480];
static uint8_t glyph[96][16][8];
static const ui_font_t font = { &glyph[0][0][0], 8u, 16u };
static const char *ppm_dir;

#ifdef VIEW_REAL
#include "suite_list.h"
#include "suite_cards.h"
#else
#define SUITE_TAG "t1"
#define SUITE_TITLE "SH4DER-PVR SUITE"
#define SUITE_N 9u
static const suite_entry_t suite_list[SUITE_N] = {
    { "AO4.BIN", "ao4", "w", "r", 1u, 1u, 600000u, NAV_DEMOS, "Room shading, four ways", 0 },
    { "AOLAMP.BIN", "ao-lamp", "w", "r", 2u, 1u, 600000u, NAV_DEMOS, "Lamp with a lit shade", 0 },
    { "HERO1.BIN", "hero1", "w", "r", 3u, 1u, 600000u, NAV_DEMOS, "Space soldier on a street", 0 },     /* 25: too long for double size */
    { "PVRDMA.BIN", "pvr-dma", "w", "r", 1u, 1u, 400000u, NAV_TOOLS, "Copy video memory to RAM", 0 },
    { "CARTECH.BIN", "car-tech", "w", "r", 4u, 1u, 900000u, NAV_DEMOS, "Car in a city by day", 0 },
    { "CARHILL.BIN", "car-hills", "w", "r", 5u, 1u, 900000u, NAV_DEMOS, "Car on hills", 0 },
    { "DRVMAP.BIN", "drive-map", "w", "r", 0u, 1u, 900000u, NAV_DEMOS, "Drive a car in a city", 0 },
    { "SQUARE.BIN", "square", "w", "r", 0u, 1u, 900000u, NAV_DEMOS, "Walled town square", 0 },
    { "HEAT.BIN", "heat", "w", "r", 0u, 1u, 900000u, NAV_DEMOS, "Heat haze on a hot road", 0 },
};
#define SUITE_GROUPS_N 5u
static const char *const suite_group_name[SUITE_GROUPS_N] = { "DRIVING", "ROOMS AND LIGHT", "CHARACTERS", "TOWN", "TOOLS" };
static const char *const fx_room[] = { "Baked corner shading (main)", "Sky shading" };
static const char *const fx_lamp[] = { "Light through thin parts (main)", "Mesh bloom" };
static const char *const fx_hero[] = { "Sun shadow map (main)" };
static const char *const fx_dma[] = { "Video memory copy (main)" };
static const char *const fx_car[] = { "Planar reflection (main)", "Sun shadow map", "Road corner shading, baked by the host tool", "Heat haze (older layer, some stages)" };
static const char *const fx_hill[] = { "Sun shadow map (main)", "Planar reflection" };
static const char *const fx_heat[] = { "Heat haze (main)" };
static const suite_card_t suite_card[SUITE_N] = {
    { 1u, 2u, fx_room, "The same room four times side by side, each with a different way of darkening the corners.", "Automatic run, about 110 seconds. A+B+X+Y goes back to the menu.", "Measured on one console: the bake at load takes 254 ms for scene A and 393 ms for scene B." },
    { 1u, 2u, fx_lamp, "A lamp whose shade lets light through its thin parts, with a glow round the lamp.", "Automatic run, about 35 seconds.", 0 },
    { 2u, 1u, fx_hero, "A figure running down a long street with his shadow.", "Automatic run, about 35 seconds.", 0 },
    { 4u, 1u, fx_dma, "A fast copy of picture memory into RAM by the graphics chip, checked against the original.", "Automatic, about 2 seconds, then the result pages.", 0 },
    { 0u, 4u, fx_car, "An automatic run down a city street by day, with the car's reflection, its shadow and corner shading.", "Automatic run, about 56 seconds. A+B+X+Y goes back to the menu.", "Earlier build of the street demo, measured on one console: 18 to 26 ms a frame." },
    { 0u, 2u, fx_hill, "An automatic run with the car on a sloped road.", "Automatic run, about 63 seconds.", 0 },
    { 0u, 2u, fx_hill, "You drive the car down the city street.", "You drive. B+Y hides the panel.", 0 },
    { 3u, 1u, fx_room, "A walled square with stalls, a fountain and hens.", "Walk it. Y+A hides the panel.", 0 },
    { 0u, 1u, fx_heat, "A shimmer of hot air over the road ahead.", "X steps the heat: off, light, strong.", 0 },
};
#define SUITE_FX_N 5u
static const unsigned char fxd_0[] = { 0u, 7u }, fxd_1[] = { 8u, 4u }, fxd_2[] = { 1u }, fxd_3[] = { 4u, 6u, 5u }, fxd_4[] = { 5u, 2u, 4u, 6u };
static const suite_fx_t suite_fx[SUITE_FX_N] = {
    { "BAKED CORNER SHADING", 2u, fxd_0 }, { "HEAT HAZE", 2u, fxd_1 }, { "LIGHT THROUGH THIN PARTS", 1u, fxd_2 },
    { "PLANAR REFLECTION", 3u, fxd_3 }, { "SUN SHADOW MAP", 4u, fxd_4 },
};
#define SUITE_NIM_N 12u
static const char suite_nim_subtitle[] = "Not a demo: nothing to run.";
static const char suite_nim_intro[] = "These effects are in the library, but this menu has no demo for them. Each line says where else the effect is shown.";
static const char *const suite_nim_item[SUITE_NIM_N] = {
    "Shadow map on the SH-4 (also shown in: Town shadow demo)", "Shadow map by the PVR (also shown in: Town shadow demo)", "Corner darkening",
    "Mesh corner shading", "Soft character shadows (also shown in: Demo disc)", "Glowing tubes", "Screen bloom (also shown in: Demo disc)",
    "Screen spotlight (also shown in: Torch walk; Demo disc)", "Dark shafts of light (also shown in: Demo disc)", "Soft edges for light",
    "Sparks and tracers (also shown in: Street car demo; Match Light)", "Lightning",
};
#endif

static const view_data_t data = { SUITE_TITLE, SUITE_TAG, suite_list, suite_card, SUITE_N, suite_group_name, SUITE_GROUPS_N, suite_fx, SUITE_FX_N,
                                  suite_nim_subtitle, suite_nim_intro, suite_nim_item, SUITE_NIM_N };
static const char rev[] = "HOLLY REV ---- SB --", foot[] = "Copyright (c) 2026 Jilted Valkyrie";

static void glyphs(void) {
    unsigned c, x, y;

#ifdef UI_MINIFONT
    for (c = 33u; c < 127u; c++)
        for (y = 0; y < 16u; y++)
            for (x = 0; x < 8u; x++) glyph[c - 32u][y][x] = (minifont_data[(c - 33u) * 16u + y] >> (7u - x)) & 1u ? 255u : 0u;
#else
    for (c = 33u; c < 127u; c++)
        for (y = 0; y < 15u; y++)
            for (x = 0; x < 7u; x++) glyph[c - 32u][y][x] = 255u;
#endif
}

static void draw(uint16_t *to, const view_t *v) {
    ui_clip(0, 479);
    view_draw(to, &font, v, rev, foot);
}

static void ppm(const char *name, const uint16_t *p) {
    char path[512];
    FILE *f;
    unsigned i;

    if (!ppm_dir) return;
    snprintf(path, sizeof path, "%s/%s.ppm", ppm_dir, name);
    f = fopen(path, "rb");
    if (f) { fclose(f); printf("suite_view_test: %s is there, not written again\n", path); return; }
    f = fopen(path, "wb");
    if (!f) { printf("suite_view_test: cannot write %s\n", path); return; }
    fprintf(f, "P6\n640 480\n255\n");
    for (i = 0; i < 640u * 480u; i++) {
        const unsigned r = (p[i] >> 11) & 31u, g = (p[i] >> 5) & 63u, b = p[i] & 31u;

        fputc((int)((r << 3) | (r >> 2)), f); fputc((int)((g << 2) | (g >> 4)), f); fputc((int)((b << 3) | (b >> 2)), f);
    }
    fclose(f);
}

/* The box of the pixels two frames differ in; 0 when they are the same. */
static int diff(const uint16_t *a, const uint16_t *b, int *x0, int *y0, int *x1, int *y1) {
    int x, y, any = 0;

    *x0 = 640; *y0 = 480; *x1 = -1; *y1 = -1;
    for (y = 0; y < 480; y++)
        for (x = 0; x < 640; x++)
            if (a[y * 640 + x] != b[y * 640 + x]) {
                any = 1;
                if (x < *x0) *x0 = x;
                if (x > *x1) *x1 = x;
                if (y < *y0) *y0 = y;
                if (y > *y1) *y1 = y;
            }
    return any;
}

static unsigned count(const uint16_t *p, int x0, int y0, int x1, int y1, uint16_t c) {
    unsigned n = 0;
    int x, y;

    for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) n += p[y * 640 + x] == c;
    return n;
}

static int key(view_t *v, unsigned hit, int dpad, int stick, uint32_t t, int *index) { return view_key(v, hit, dpad, stick, t, 3u, index); }

/* The lists: every item of the data is on its tab, a heading is never the cursor, the cursor goes round. */
static void t_lists(void) {
    static view_t v;
    unsigned tab, i, items, heads;
    int index;

    view_init(&v, &data, ~0u, 0u);
    CHECK(v.m.tab == VIEW_TAB_DEMO && v.screen == VIEW_LIST);
    for (tab = 0; tab < VIEW_TABS; tab++) {
        const sh4der_menu_list_t *l = &v.lists[tab];
        unsigned t = 1000u, seen = 0, start;

        for (i = 0, items = 0, heads = 0; i < l->n; i++) { items += l->row[i].kind == SH4DER_MENU_ITEM; heads += l->row[i].kind == SH4DER_MENU_HEAD; }
        CHECK(l->n <= VIEW_ROWS_MAX && items > 0u && heads > 0u);
        CHECK(l->row[0].kind == SH4DER_MENU_HEAD);                      /* a list starts with a heading */
        if (tab == VIEW_TAB_DEMO) CHECK(items == data.n);               /* every demo once */
        else CHECK(l->row[l->n - 1u].id == VIEW_ID_NIM && (l->row[l->n - 1u].flags & VIEW_F_PLAIN));
        while (v.m.tab != tab) key(&v, NAV_RIGHT, 0, 0, t, &index);
        start = v.m.pos[tab].cursor;
        CHECK(sh4der_menu_pickable(l, start));
        do {                                                            /* down through every item, one press each, back to the start */
            key(&v, 0u, 1, 0, t, &index); t += 20u;
            key(&v, 0u, 0, 0, t, &index); t += 20u;
            CHECK(sh4der_menu_pickable(l, v.m.pos[tab].cursor));
            CHECK(v.m.pos[tab].cursor >= v.m.pos[tab].first && v.m.pos[tab].cursor < v.m.pos[tab].first + VIEW_LIST_ROWS);
            seen++;
        } while (v.m.pos[tab].cursor != start && seen < 1000u);
        CHECK(seen == items);
    }
}

/* A held direction: one step at once, the next after the first wait, then one every repeat. */
static void t_repeat(void) {
    static view_t v;
    unsigned t, moves = 0, was;
    int index;

    view_init(&v, &data, ~0u, 0u);
    was = v.m.pos[0].cursor;
    for (t = 0; t <= SH4DER_MENU_REPEAT_FIRST_MS + 2u * SH4DER_MENU_REPEAT_MS; t += 20u) {
        key(&v, 0u, 1, 0, 5000u + t, &index);
        if (v.m.pos[0].cursor != was) { moves++; was = v.m.pos[0].cursor; }
    }
    CHECK(moves == 4u);                                                  /* at 0, 300, 380 and 460 ms */
}

static void t_screens(void) {
    static view_t v, w;
    int x0, y0, x1, y1, index, act, ty0, ty1;
    unsigned t = 100u, i, long_row = ~0u;

    /* the cold start: BY DEMO, the first demo of the first group */
    view_init(&v, &data, ~0u, 0u);
    draw(fb, &v);
    ppm("B_1_by_demo_first", fb);
    CHECK(fb[30 * 640 + 436] == VIEW_C_PICK && fb[30 * 640 + 516] == VIEW_C_BAND);          /* the BY DEMO tab is the lit one */
    CHECK(count(fb, 0, 64, 31, 413, VIEW_C_BG) == 32u * 350u);                              /* nothing left of the cards */
    CHECK(count(fb, 608, 64, 639, 413, VIEW_C_BG) == 32u * 350u);                           /* ... or right of them */
    CHECK(fb[VIEW_LIST_Y(1) * 640 + 37] == VIEW_C_ACCENT && fb[(VIEW_LIST_Y(1) + 15) * 640 + 240] == VIEW_C_PICK);    /* row 1 is picked: its mark and fill */
    CHECK(fb[VIEW_LIST_Y(0) * 640 + 240] == VIEW_C_CARD);                                   /* the heading above it is not */
    CHECK(fb[430 * 640 + 274] == VIEW_C_CHIP_A && fb[430 * 640 + 538] == VIEW_C_CHIP_B);     /* A red, B blue */
    CHECK(fb[130 * 640 + 281] == VIEW_C_CHIP);                                              /* the card's START chip */

    /* the stick moves the card's text and nothing else, inside the lines suite_menu.c copies */
    for (i = 0; i < data.n; i++) {                                      /* a demo whose text is longer than the card */
        act = key(&v, 0u, 0, 127, t, &index); t += 20u;
        if (act == VIEW_REDRAW_TEXT) break;
        key(&v, 0u, 0, 0, t, &index); t += 20u;
        key(&v, 0u, 1, 0, t, &index); t += 20u;
        key(&v, 0u, 0, 0, t, &index); t += 20u;
        draw(fb, &v);
    }
    CHECK(i < data.n && v.scroll.pos == 1u && sh4der_menu_scroll_max(&v.scroll) >= 1u);
    draw(fb2, &v);
    ppm("B_2_by_demo_text_scrolled", fb2);
    view_text_lines(&ty0, &ty1);
    CHECK(diff(fb, fb2, &x0, &y0, &x1, &y1) && x0 > VIEW_RCARD_X0 && x1 < VIEW_RCARD_X1 && y0 >= ty0 && y1 <= ty1);
    for (i = 0; i < 400u; i++) { key(&v, 0u, 0, 127, t, &index); t += 20u; }                /* held down: to the end and no further */
    CHECK(v.scroll.pos == sh4der_menu_scroll_max(&v.scroll));
    draw(fb, &v);
    ppm("B_3_by_demo_text_end", fb);
    key(&v, 0u, 0, 0, t, &index); t += 20u;

    /* a list move puts the text back at its top and changes nothing in the head */
    memcpy(fb2, fb, sizeof fb);
    act = key(&v, 0u, 1, 0, t, &index); t += 20u;
    key(&v, 0u, 0, 0, t, &index); t += 20u;
    CHECK(act == VIEW_REDRAW && v.scroll.pos == 0u);
    draw(fb, &v);
    CHECK(diff(fb, fb2, &x0, &y0, &x1, &y1) && y0 > VIEW_HEAD_Y1 + 2 && y1 < VIEW_FOOT_Y0 - 2);

    /* A and START start the picked demo; the place round-trips through the mail's word */
    {
        const int want = view_entry(&v);
        uint32_t place;

        CHECK(want >= 0);
        CHECK(key(&v, NAV_A, 0, 0, t, &index) == VIEW_LAUNCH && index == want); t += 20u;
        CHECK(key(&v, NAV_START, 0, 0, t, &index) == VIEW_LAUNCH && index == want); t += 20u;
        place = view_place(&v);
        view_init(&w, &data, (unsigned)want, place);
        CHECK(w.m.tab == v.m.tab && w.m.pos[w.m.tab].cursor == v.m.pos[v.m.tab].cursor && w.scroll.pos == v.scroll.pos);
        view_init(&w, &data, (unsigned)want, 0u);                       /* a test from before the place: its row of BY DEMO */
        CHECK(w.m.tab == VIEW_TAB_DEMO && view_entry(&w) == want && w.scroll.pos == 0u);
        view_init(&w, &data, (unsigned)want == 0u ? 1u : 0u, place);    /* a place that is not that program's row: not used */
        CHECK(w.m.tab == VIEW_TAB_DEMO && view_entry(&w) == ((unsigned)want == 0u ? 1 : 0));
    }

    /* BY EFFECT: the other tab is lit; a place there, with a scroll, comes back as it was */
    CHECK(key(&v, NAV_RIGHT, 0, 0, t, &index) == VIEW_REDRAW && v.m.tab == VIEW_TAB_EFFECT); t += 20u;
    draw(fb, &v);
    ppm("B_4_by_effect", fb);
    CHECK(fb[30 * 640 + 516] == VIEW_C_PICK && fb[30 * 640 + 436] == VIEW_C_BAND);
    for (i = 0; i < 3u; i++) { key(&v, 0u, 1, 0, t, &index); t += 20u; key(&v, 0u, 0, 0, t, &index); t += 20u; }
    {
        const int want = view_entry(&v);
        const uint32_t place = view_place(&v);

        view_init(&w, &data, (unsigned)want, place);
        CHECK(want >= 0 && w.m.tab == VIEW_TAB_EFFECT && w.m.pos[1].cursor == v.m.pos[1].cursor && w.m.pos[1].first == v.m.pos[1].first);
        draw(fb, &v); draw(fb2, &w);
        CHECK(!diff(fb, fb2, &x0, &y0, &x1, &y1));                      /* the same screen */
    }

    /* NOT IN THE MENU: up from the first item goes round to it; it starts nothing; A and START are grey; text only */
    while (v.m.pos[1].cursor != sh4der_menu_next_item(&v.lists[1], 0u)) { key(&v, 0u, -1, 0, t, &index); t += 20u; key(&v, 0u, 0, 0, t, &index); t += 20u; }
    key(&v, 0u, -1, 0, t, &index); t += 20u; key(&v, 0u, 0, 0, t, &index); t += 20u;
    CHECK(view_row(&v) && view_row(&v)->id == VIEW_ID_NIM && view_entry(&v) < 0);
    CHECK(key(&v, NAV_A, 0, 0, t, &index) == VIEW_NONE && index < 0); t += 20u;
    CHECK(key(&v, NAV_START, 0, 0, t, &index) == VIEW_NONE && index < 0); t += 20u;
    draw(fb, &v);
    ppm("B_5_by_effect_not_in_the_menu", fb);
    CHECK(fb[430 * 640 + 274] == VIEW_C_RULE && fb[430 * 640 + 334] == VIEW_C_RULE && fb[430 * 640 + 538] == VIEW_C_CHIP_B);
    CHECK(count(fb, VIEW_CARD_X, VIEW_CHIP_Y - 2, VIEW_RCARD_X1 - 1, VIEW_CHIP_Y + 17, VIEW_C_CHIP) == 0u);     /* no START chip on this card */
    CHECK(sh4der_menu_scroll_max(&v.scroll) > 0u);

    /* B on BY EFFECT goes to BY DEMO; on BY DEMO it does nothing */
    CHECK(key(&v, NAV_B, 0, 0, t, &index) == VIEW_REDRAW && v.m.tab == VIEW_TAB_DEMO); t += 20u;
    CHECK(key(&v, NAV_B, 0, 0, t, &index) == VIEW_NONE && v.m.tab == VIEW_TAB_DEMO); t += 20u;

    /* Y: the credits and back; their pages go round */
    CHECK(key(&v, NAV_Y, 0, 0, t, &index) == VIEW_REDRAW && v.screen == VIEW_CREDITS && v.page == 0u);
    CHECK(key(&v, NAV_B, 0, 0, t, &index) == VIEW_REDRAW && v.page == 2u);
    CHECK(key(&v, NAV_A, 0, 0, t, &index) == VIEW_REDRAW && v.page == 0u);
    CHECK(key(&v, NAV_START, 0, 0, t, &index) == VIEW_REDRAW && v.screen == VIEW_LIST);

    /* a title too long for double size is drawn at normal size: the card's border is whole beside it */
    for (i = 0; i < data.n; i++) if (strlen(suite_list[i].title ? suite_list[i].title : suite_list[i].name) > VIEW_CTITLE_BIG) long_row = i;
    if (long_row < data.n) {
        view_init(&v, &data, long_row, 0u);
        CHECK(view_entry(&v) == (int)long_row);
        draw(fb, &v);
        ppm("B_6_long_title", fb);
        CHECK(count(fb, VIEW_RCARD_X1, VIEW_CARD_Y0, VIEW_RCARD_X1, VIEW_CARD_Y1, VIEW_C_BORDER) == (unsigned)(VIEW_CARD_Y1 - VIEW_CARD_Y0 + 1));
#ifndef UI_MINIFONT
        CHECK(count(fb, VIEW_CARD_X, VIEW_CTITLE_Y, VIEW_RCARD_X1 - 1, VIEW_CTITLE_Y + 7, VIEW_C_TEXT) == 0u);   /* nothing where a double-size title's top would be */
#endif
    }

    /* the note takes the grey line's place */
    view_init(&v, &data, ~0u, 0u);
    snprintf(v.note, sizeof v.note, "HEAT.BIN: could not be read from the disc");
    draw(fb, &v);
    CHECK(count(fb, 40, VIEW_FOOT_Y, 420, VIEW_FOOT_Y + 15, VIEW_C_YELLOW) > 0u);
}

/* Every row's card: its lines inside their columns, in the table's room, and the text never drawn outside the card. */
static void t_cards(void) {
    static view_t v;
    unsigned tab, i, k, most = 0;
    int index;

    view_init(&v, &data, ~0u, 0u);
    for (tab = 0; tab < VIEW_TABS; tab++) {
        unsigned t = 0;

        while (v.m.tab != tab) key(&v, NAV_RIGHT, 0, 0, 0u, &index);
        for (i = 0; i < v.lists[tab].n; i++) {
            key(&v, 0u, 1, 0, 100000u + t, &index); t += 20u;
            key(&v, 0u, 0, 0, 100000u + t, &index); t += 20u;
            CHECK(v.nlines > 0u && v.nlines < VIEW_LINES_MAX);
            if (v.nlines > most) most = v.nlines;
            for (k = 0; k < v.nlines; k++) {
                const view_line_t *l = &v.line[k];

                CHECK(l->len <= (l->kind == VIEW_L_BULLET || l->kind == VIEW_L_BULLET_MORE ? VIEW_BULLET_COLS : VIEW_CARD_COLS));
                CHECK(l->len > 0u && l->s[0] != ' ');
            }
            draw(fb, &v);
            CHECK(count(fb, VIEW_RCARD_X0, VIEW_CARD_Y0, VIEW_RCARD_X1, VIEW_CARD_Y0, VIEW_C_BORDER) == (unsigned)(VIEW_RCARD_X1 - VIEW_RCARD_X0 + 1));
            CHECK(count(fb, VIEW_RCARD_X0, VIEW_CARD_Y1, VIEW_RCARD_X1, VIEW_CARD_Y1, VIEW_C_BORDER) == (unsigned)(VIEW_RCARD_X1 - VIEW_RCARD_X0 + 1));
            CHECK(count(fb, VIEW_RCARD_X1, VIEW_CARD_Y0, VIEW_RCARD_X1, VIEW_CARD_Y1, VIEW_C_BORDER) == (unsigned)(VIEW_CARD_Y1 - VIEW_CARD_Y0 + 1));
            CHECK(count(fb, VIEW_LCARD_X1, VIEW_CARD_Y0, VIEW_LCARD_X1, VIEW_CARD_Y1, VIEW_C_BORDER) == (unsigned)(VIEW_CARD_Y1 - VIEW_CARD_Y0 + 1));
        }
    }
    printf("suite_view_test: %u + %u rows, the longest card %u lines of %u\n", v.lists[0].n, v.lists[1].n, most, VIEW_LINES_MAX);
}

/* The last result page's line and bar (sh4der_suite.h): where they are, and the bar's fill. */
static void t_bar(void) {
    char s[48];
    int x;

    memset(fb, 0, sizeof fb);
    ui_clip(0, 479);
    ui_text(fb, &font, 1u, 64, 48, VIEW_C_WHITE, "Car in a city by day p4/4 A/B ABXY menu", 64u);
    ui_text(fb, &font, 1u, 64, 80, VIEW_C_GREY, "[the test's own result lines and graphs: 22 rows, down to y = 431]", 64u);
    ui_text(fb, &font, 1u, 64, 80 + 16 * 21, VIEW_C_GREY, "[the page's last row]", 64u);
    suite_bar_paint(fb, 0, 0u);
    ui_text(fb, &font, 1u, SUITE_BAR_X0 + 8, SUITE_BAR_Y0 + 2, VIEW_C_WHITE, SUITE_HINT_TEXT, 64u);
    ppm("B_7_last_page_line", fb);
    CHECK(SUITE_BAR_Y0 >= 48 + 16 * 24 && SUITE_BAR_Y1 < 480 && strlen(SUITE_HINT_TEXT) * 8u + 8u <= (unsigned)(SUITE_BAR_X1 - SUITE_BAR_X0));
    suite_bar_paint(fb, 1, 600u);
    suite_hold_text(s, sizeof s, 600u);
    CHECK(!strcmp(s, "Hold A ... 1.2 s of 2 s"));
    ui_text(fb, &font, 1u, SUITE_BAR_X0 + 8, SUITE_BAR_Y0 + 2, VIEW_C_WHITE, s, 64u);
    ppm("B_8_last_page_hold_a", fb);
    x = suite_bar_fill_x(600u);
    CHECK(x > SUITE_BAR_X0 && x < SUITE_BAR_X1);
    CHECK(fb[(SUITE_BAR_Y1 - 1) * 640 + x] == 0x2b38u && fb[(SUITE_BAR_Y1 - 1) * 640 + x + 1] == 0x2988u);
    CHECK(fb[SUITE_BAR_Y0 * 640 + SUITE_BAR_X0] == 0x94f5u && fb[SUITE_BAR_Y1 * 640 + SUITE_BAR_X1] == 0x94f5u);
    CHECK(suite_bar_fill_x(0u) == SUITE_BAR_X0 && suite_bar_fill_x(1000u) == SUITE_BAR_X1 - 1 && suite_bar_fill_x(5000u) == SUITE_BAR_X1 - 1);
    suite_hold_text(s, sizeof s, 0u);
    CHECK(!strcmp(s, "Hold A ... 0.0 s of 2 s"));
    suite_hold_text(s, sizeof s, 999u);
    CHECK(!strcmp(s, "Hold A ... 1.9 s of 2 s"));
}

int main(int argc, char **argv) {
    int i;

    (void)suite_mail_find;
    for (i = 1; i + 1 < argc; i++) if (!strcmp(argv[i], "-ppm")) ppm_dir = argv[i + 1];
    glyphs();
    t_lists();
    t_repeat();
    t_screens();
    t_cards();
    t_bar();
    printf("suite_view_test: %s\n", fails ? "FAILED" : "ok");
    return fails ? 1 : 0;
}
