/* SH4DER-PVR: host test of the suite menu's screens and buttons (../suite_nav.h) and of the credits text's form

   demo/psm/suite/test/suite_nav_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   The list below is s7's sorting. Checked: the start screen (cold, a program back, a one-group disc), every button
   on every screen, RUN ALL's order a group (tools once through, demos round), the scrolling window, that every
   screen's rows keep to 64 columns and off the rows of the news line and the foot, and that ../../psm_credits.h is
   64 columns of plain ASCII with the owner's first line word for word.

   Build (from the repository's root):
     gcc -std=c99 -O2 -Wall -Wextra -Werror demo/psm/suite/test/suite_nav_test.c -o build/psm_suite/suite_nav_test
   -v prints every screen as the console shows it (a big row is marked [2x]). */

#include <stdio.h>
#include <string.h>

#include "../suite_nav.h"
#include "../../psm_credits.h"

static int fails, verbose;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static const suite_entry_t s7[] = {
    { "AO4.BIN", "ao4", "AO side by side; clip guard; tile cut last. 110 s", "photo pages+graph; photo stages 16, 17 (CLIP GUARD label)", 1u, 1u, 600000u, NAV_DEMOS },
    { "AOLAMP.BIN", "ao-lamp", "lamp look, movable split", "LT/RT move the split", 2u, 1u, 600000u, NAV_DEMOS },
    { "HERO1.BIN", "hero1", "skinned hero, pass costs, room + hens. ~50 s", "photo every page; START = live (keys on the last page)", 3u, 1u, 600000u, NAV_DEMOS },
    { "PL4NAR.BIN", "pl4nar", "street demo, new map", "photo the pages", 4u, 1u, 900000u, NAV_DEMOS },
    { "D1.BIN", "d1", "depth-fade spike", "photo both passes", 1u, 1u, 500000u, NAV_TOOLS },
    { "HENEYES.BIN", "hens-eyes", "hens: rays vs collision sweep, cycles. ~50 s", "photo every page + graph; START = live: X mode, Y hens", 2u, 1u, 500000u, NAV_TOOLS },
    { "PVRDMA.BIN", "pvr-dma", "PVR-DMA download proof: VRAM to RAM. ~2 s", "PVR-DMA download proof: photo the page and the graph", 3u, 1u, 400000u, NAV_TOOLS },
    { "AO1CLIVE.BIN", "ao1c-live", "TEST, no label: town walk, X AO OFF/1/2, Y CLAY", "photo pairs at one spot: CLAY, AO OFF/1/2; then TEXTURED", 0u, 1u, 900000u, NAV_TOOLS },
    { "HUD1C2.BIN", "hud1c-2", "TEST, no label: hud1b, no DIAG box in opaque", "photo the bottom two lines; ABXY or START 3 s leaves", 0u, 0u, 900000u, NAV_TOOLS },
};
#define N7 ((unsigned)(sizeof s7 / sizeof s7[0]))

static suite_entry_t many[20];

static void show(const char *what, const nav_t *v, const suite_entry_t *list, unsigned n, const char *note) {
    static nav_grid_t g;
    unsigned row, used = 0;

    nav_grid(&g, v, list, n, "PSM TEST SUITE s7", note, psm_credits_foot);
    for (row = 0; row < NAV_ROWS; row++) {
        CHECK(strlen(g.text[row]) <= NAV_COLS);
        if (g.big[row]) { CHECK(strlen(g.text[row]) <= NAV_COLS / 2u); CHECK(row + 1u < NAV_ROWS && !g.text[row + 1u][0]); }
        if (row >= 4u && row < 17u && g.text[row][0]) used = row;
    }
    CHECK(used < 17u);                                         /* the entries never reach the help lines */
    CHECK(!strcmp(g.text[NAV_FOOT_ROW], "SH4DER-PVR Copyright (c) 2026 Jilted Valkyrie"));
    CHECK(g.text[NAV_FOOT_ROW + 1u][0]);
    CHECK(!strncmp(g.text[17], "In a test: hold A+B+X+Y", 23u));
    if (!verbose) return;
    printf("\n--- %s\n", what);
    printf("+----------------------------------------------------------------+\n");
    for (row = 0; row < NAV_ROWS; row++) printf("|%-64s|%s\n", g.text[row], g.big[row] ? " [2x]" : g.colour[row] == NAV_YELLOW ? " yellow" : g.colour[row] == NAV_GREY ? " grey" : "");
    printf("+----------------------------------------------------------------+\n");
}

int main(int argc, char **argv) {
    nav_t v;
    int index;
    unsigned i, guard;

    verbose = argc > 1 && !strcmp(argv[1], "-v");

    /* the start */
    nav_init(&v, s7, N7, 0xffffffffu);
    CHECK(v.screen == NAV_TOP && v.has_top && v.top_cursor == 0u);
    show("TOP, cold start", &v, s7, N7, "");
    CHECK(nav_count(s7, N7, NAV_DEMOS) == 4u && nav_count(s7, N7, NAV_TOOLS) == 5u);

    /* TOP's buttons */
    CHECK(nav_key(&v, s7, N7, NAV_B, 7u, &index) == NAV_NONE && v.screen == NAV_TOP);
    CHECK(nav_key(&v, s7, N7, NAV_START, 7u, &index) == NAV_NONE && v.screen == NAV_TOP);
    CHECK(nav_key(&v, s7, N7, NAV_DOWN, 7u, &index) == NAV_REDRAW && v.top_cursor == 1u);
    show("TOP, DEVELOPER TOOLS picked", &v, s7, N7, "");
    CHECK(nav_key(&v, s7, N7, NAV_UP, 7u, &index) == NAV_REDRAW && v.top_cursor == 0u);
    CHECK(nav_key(&v, s7, N7, NAV_A, 7u, &index) == NAV_REDRAW && v.screen == NAV_LIST && v.group == NAV_DEMOS && v.cursor == 0u);
    show("LIST of TECH DEMOS, RUN ALL picked", &v, s7, N7, "");

    /* a list's buttons */
    CHECK(nav_key(&v, s7, N7, NAV_A, 7u, &index) == NAV_RUN_ALL && index == 0);
    CHECK(nav_key(&v, s7, N7, NAV_DOWN, 7u, &index) == NAV_REDRAW && v.cursor == 1u);
    CHECK(nav_key(&v, s7, N7, NAV_A, 7u, &index) == NAV_LAUNCH && index == 0);
    CHECK(nav_key(&v, s7, N7, NAV_UP, 7u, &index) == NAV_REDRAW && v.cursor == 0u);
    CHECK(nav_key(&v, s7, N7, NAV_UP, 7u, &index) == NAV_REDRAW && v.cursor == 4u);         /* round to the last */
    CHECK(nav_key(&v, s7, N7, NAV_A, 7u, &index) == NAV_LAUNCH && index == 3);
    show("LIST of TECH DEMOS, the last picked", &v, s7, N7, "");
    CHECK(nav_key(&v, s7, N7, NAV_DOWN, 7u, &index) == NAV_REDRAW && v.cursor == 0u);
    CHECK(nav_key(&v, s7, N7, NAV_B, 7u, &index) == NAV_REDRAW && v.screen == NAV_TOP);
    CHECK(nav_key(&v, s7, N7, NAV_DOWN, 7u, &index) == NAV_REDRAW);
    CHECK(nav_key(&v, s7, N7, NAV_A, 7u, &index) == NAV_REDRAW && v.screen == NAV_LIST && v.group == NAV_TOOLS);
    CHECK(nav_key(&v, s7, N7, NAV_A, 7u, &index) == NAV_RUN_ALL && index == 4);
    for (i = 0; i < 5u; i++) nav_key(&v, s7, N7, NAV_DOWN, 7u, &index);
    CHECK(v.cursor == 5u && nav_key(&v, s7, N7, NAV_A, 7u, &index) == NAV_LAUNCH && index == 8);
    show("LIST of DEVELOPER TOOLS, a NO RETURN entry picked, with news", &v, s7, N7, "last: pvr-dma ended");
    CHECK(nav_key(&v, s7, N7, NAV_START, 7u, &index) == NAV_REDRAW && v.screen == NAV_TOP && v.top_cursor == 1u);

    /* the credits, from either screen, and back to it */
    CHECK(nav_key(&v, s7, N7, NAV_Y, 7u, &index) == NAV_REDRAW && v.screen == NAV_CREDITS && v.page == 0u);
    CHECK(nav_key(&v, s7, N7, NAV_B, 7u, &index) == NAV_REDRAW && v.page == 6u);
    CHECK(nav_key(&v, s7, N7, NAV_A, 7u, &index) == NAV_REDRAW && v.page == 0u);
    CHECK(nav_key(&v, s7, N7, NAV_A, 7u, &index) == NAV_REDRAW && v.page == 1u);
    CHECK(nav_key(&v, s7, N7, NAV_UP, 7u, &index) == NAV_NONE);
    CHECK(nav_key(&v, s7, N7, NAV_Y, 7u, &index) == NAV_REDRAW && v.screen == NAV_TOP);
    nav_key(&v, s7, N7, NAV_A, 7u, &index);
    CHECK(v.screen == NAV_LIST);
    CHECK(nav_key(&v, s7, N7, NAV_Y, 7u, &index) == NAV_REDRAW && v.screen == NAV_CREDITS);
    CHECK(nav_key(&v, s7, N7, NAV_START, 7u, &index) == NAV_REDRAW && v.screen == NAV_LIST && v.group == NAV_TOOLS);
    CHECK(nav_key(&v, s7, N7, NAV_A, 0u, &index) != NAV_NONE);                              /* no page count does not divide by 0 */

    /* a program came back: its group's list, its row */
    nav_init(&v, s7, N7, 6u);
    CHECK(v.screen == NAV_LIST && v.group == NAV_TOOLS && v.cursor == 3u && v.top_cursor == 1u);
    nav_init(&v, s7, N7, 2u);
    CHECK(v.screen == NAV_LIST && v.group == NAV_DEMOS && v.cursor == 3u && v.top_cursor == 0u);

    /* RUN ALL: tools once through, demos round; an entry with order 0 or no return is never in it */
    CHECK(nav_next_chain(s7, N7, 4u) == 5 && nav_next_chain(s7, N7, 5u) == 6 && nav_next_chain(s7, N7, 6u) == -1);
    CHECK(nav_next_chain(s7, N7, 0u) == 1 && nav_next_chain(s7, N7, 3u) == 0);
    CHECK(nav_next_chain(s7, N7, 99u) == -1);
    for (i = 0, guard = 0, index = 0; guard < 9u; guard++) { index = nav_next_chain(s7, N7, (unsigned)index); CHECK(index >= 0 && s7[index].group == NAV_DEMOS); i += index == 0; }
    CHECK(i == 2u);                                                                         /* nine steps: twice past the first */

    /* a disc of one group: no TOP, B does nothing (the menu the owner ran before) */
    for (i = 0; i < 20u; i++) { many[i] = s7[5]; many[i].order = i + 1u; }
    nav_init(&v, many, 20u, 0xffffffffu);
    CHECK(v.screen == NAV_LIST && !v.has_top && v.group == NAV_TOOLS);
    CHECK(nav_key(&v, many, 20u, NAV_B, 7u, &index) == NAV_NONE && v.screen == NAV_LIST);
    show("a one-group disc of 20: the list is the menu", &v, many, 20u, "");
    for (i = 0; i < 12u; i++) nav_key(&v, many, 20u, NAV_DOWN, 7u, &index);
    CHECK(v.cursor == 12u && v.first == 3u);
    show("the same, the 12th picked: scrolled", &v, many, 20u, "");
    for (i = 0; i < 8u; i++) nav_key(&v, many, 20u, NAV_DOWN, 7u, &index);
    CHECK(v.cursor == 20u && v.first == 11u);
    show("the same, the last picked", &v, many, 20u, "RUN ALL is done (last: hens-eyes)");
    nav_key(&v, many, 20u, NAV_DOWN, 7u, &index);
    CHECK(v.cursor == 0u && v.first == 0u);
    nav_key(&v, many, 20u, NAV_UP, 7u, &index);
    CHECK(v.cursor == 20u && v.first == 11u);
    for (i = 0; i < 20u; i++) many[i].group = NAV_DEMOS;
    nav_init(&v, many, 20u, 19u);
    CHECK(v.screen == NAV_LIST && v.group == NAV_DEMOS && v.cursor == 20u && v.first == 11u && !v.has_top);

    /* the credits text: the owner's first line, 64 columns, plain ASCII, a page break alone on its line */
    CHECK(!strcmp(psm_credits_foot[0], "SH4DER-PVR Copyright (c) 2026 Jilted Valkyrie"));
    CHECK(PSM_CREDITS_N > 60u);
    for (i = 0; i < PSM_CREDITS_N; i++) {
        const char *s = psm_credits_text[i];

        CHECK(strlen(s) <= NAV_COLS);
        if (s[0] == '\f') { CHECK(!s[1]); continue; }
        for (; *s; s++) CHECK(*s >= 32 && *s < 127);
    }
    for (i = 0; i < 2u; i++) CHECK(strlen(psm_credits_foot[i]) <= NAV_COLS);

    if (fails) printf("suite_nav_test: %d FAILED\n", fails); else printf("suite_nav_test: ok\n");
    return fails ? 1 : 0;
}
