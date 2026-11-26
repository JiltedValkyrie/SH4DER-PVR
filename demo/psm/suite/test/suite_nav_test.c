/* SH4DER-PVR: host test of the suite menu's screens and buttons (../suite_nav.h) and of the credits text's form

   demo/psm/suite/test/suite_nav_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   The list below is s7's sorting. Checked: the start screen (cold, a program back, a one-group disc), every button
   on every screen, RUN ALL's order a group (tools once through, demos round), the scrolling window, that every
   screen's rows keep to 64 columns and off the rows of the news line and the foot, and that ../../psm_credits.h is
   64 columns of plain ASCII with the owner's first line word for word. The keys: on every screen, with and without
   the TOP screen, each button acts exactly when the screen's legend names it (read from the legend's text, not from
   the table), the whole legend is on the screen, and two buttons in one pass do what they did before the table.
   The rows' kinds (what suite_ui.h draws under them): the title, the legend and the foot where they belong, one row
   picked, every line that belongs to an option under one; the chips' spans are the legend's keys; an entry with a
   plain title shows it alone, and its shows line and its read when picked; the fullest list still fits.

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
    { "AO4.BIN", "ao4", "AO side by side; clip guard; tile cut last. 110 s", "photo pages+graph; photo stages 16, 17 (CLIP GUARD label)", 1u, 1u, 600000u, NAV_DEMOS, 0, 0 },
    { "AOLAMP.BIN", "ao-lamp", "lamp look, movable split", "LT/RT move the split", 2u, 1u, 600000u, NAV_DEMOS, 0, 0 },
    { "HERO1.BIN", "hero1", "skinned hero, pass costs, room + hens. ~50 s", "photo every page; START = live (keys on the last page)", 3u, 1u, 600000u, NAV_DEMOS, 0, 0 },
    { "PL4NAR.BIN", "pl4nar", "street demo, new map", "photo the pages", 4u, 1u, 900000u, NAV_DEMOS, 0, 0 },
    { "D1.BIN", "d1", "depth-fade spike", "photo both passes", 1u, 1u, 500000u, NAV_TOOLS, 0, 0 },
    { "HENEYES.BIN", "hens-eyes", "hens: rays vs collision sweep, cycles. ~50 s", "photo every page + graph; START = live: X mode, Y hens", 2u, 1u, 500000u, NAV_TOOLS, 0, 0 },
    { "PVRDMA.BIN", "pvr-dma", "PVR-DMA download proof: VRAM to RAM. ~2 s", "PVR-DMA download proof: photo the page and the graph", 3u, 1u, 400000u, NAV_TOOLS, 0, 0 },
    { "AO1CLIVE.BIN", "ao1c-live", "TEST, no label: town walk, X AO OFF/1/2, Y CLAY", "photo pairs at one spot: CLAY, AO OFF/1/2; then TEXTURED", 0u, 1u, 900000u, NAV_TOOLS, 0, 0 },
    { "HUD1C2.BIN", "hud1c-2", "TEST, no label: hud1b, no DIAG box in opaque", "photo the bottom two lines; ABXY or START 3 s leaves", 0u, 0u, 900000u, NAV_TOOLS, 0, 0 },
};
#define N7 ((unsigned)(sizeof s7 / sizeof s7[0]))

static suite_entry_t many[20];

static void show(const char *what, const nav_t *v, const suite_entry_t *list, unsigned n, const char *note) {
    static nav_grid_t g;
    unsigned row, used = 0;

    char keys[96];

    nav_grid(&g, v, list, n, "PSM TEST SUITE s7", note, psm_credits_foot);
    nav_legend(keys, sizeof keys, v->screen, v->has_top);
    CHECK(keys[0] && !strcmp(g.text[NAV_KEYS_ROW], keys) && g.kind[NAV_KEYS_ROW] == NAV_K_KEYS);   /* the whole legend, not cut */
    CHECK(g.kind[0] == NAV_K_HEAD && g.kind[NAV_FOOT_ROW] == NAV_K_FOOT && g.kind[NAV_FOOT_ROW + 1u] == NAV_K_FOOT);
    {
        unsigned picks = 0, owner = 0;

        for (row = 0; row < NAV_ROWS; row++) {
            const unsigned k = g.kind[row];

            CHECK(k < NAV_KINDS && (g.text[row][0] || !k));
            picks += k == NAV_K_PICK;
            if (k == NAV_K_ITEM || k == NAV_K_PICK) owner = k;
            else if (k == NAV_K_ITEM_MORE) CHECK(owner == NAV_K_ITEM);
            else if (k == NAV_K_PICK_MORE || k == NAV_K_FLAG) CHECK(owner == NAV_K_PICK);
            else if (g.text[row][0]) owner = 0;
            CHECK((k == NAV_K_PICK) == (g.text[row][0] == '>'));            /* the cursor and the picked panel agree */
        }
        CHECK(picks == 1u);
    }
    {
        nav_span_t span[8];
        const unsigned n = nav_legend_spans(span, 8u, v->screen, v->has_top, NAV_COLS);
        unsigned i, j = 0;

        CHECK(n >= 3u);
        for (i = 0; i < n; i++) {
            while (j < NAV_KEYS_N && (nav_keys[j].screen != v->screen || !nav_keys[j].keys || (nav_keys[j].need_top && !v->has_top))) j++;
            CHECK(j < NAV_KEYS_N && !strncmp(keys + span[i].col, nav_keys[j].keys, span[i].len) && strlen(nav_keys[j].keys) == span[i].len);
            CHECK(span[i].buttons == nav_keys[j].buttons && (span[i].col == 0u || keys[span[i].col - 1u] == ' '));
            j++;
        }
    }
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

/* 1 when a legend names `button`: its items are "keys word" two spaces apart, the keys split by '/'. */
static int named(const char *legend, unsigned button) {
    static const char *const name[6] = { "up", "down", "A", "B", "Y", "START" };   /* NAV_UP .. NAV_START, bit by bit */
    const char *want = 0, *s = legend;
    unsigned b;

    for (b = 0; b < 6u; b++) if (button == 1u << b) want = name[b];
    while (want && *s) {
        const char *end = strchr(s, ' ');
        const char *next = strstr(s, "  ");
        const size_t keys = end ? (size_t)(end - s) : strlen(s);
        size_t i = 0;

        while (i < keys) {
            size_t len = 0;

            while (i + len < keys && s[i + len] != '/') len++;
            if (len == strlen(want) && !strncmp(s + i, want, len)) return 1;
            i += len + 1u;
        }
        if (!next) break;
        s = next + 2;
    }
    return 0;
}

/* Each button alone on a copy of `start`: it acts exactly when the screen's legend names it. */
static void keys_match(const char *what, const nav_t *start, const suite_entry_t *list, unsigned n) {
    char keys[96];
    unsigned b;

    nav_legend(keys, sizeof keys, start->screen, start->has_top);
    for (b = NAV_UP; b <= NAV_START; b <<= 1) {
        nav_t v = *start;
        int index;
        const int acts = nav_key(&v, list, n, b, 7u, &index) != NAV_NONE;

        if (acts != named(keys, b)) { printf("FAIL keys, %s: button %u %s, legend \"%s\"\n", what, b, acts ? "acts" : "does nothing", keys); fails++; }
    }
    if (verbose) printf("keys, %s: %s\n", what, keys);
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
    CHECK(v.cursor == 12u && v.first == 12u - NAV_WINDOW);
    show("the same, the 12th picked: scrolled", &v, many, 20u, "");
    for (i = 0; i < 8u; i++) nav_key(&v, many, 20u, NAV_DOWN, 7u, &index);
    CHECK(v.cursor == 20u && v.first == 20u - NAV_WINDOW);
    show("the same, the last picked", &v, many, 20u, "RUN ALL is done (last: hens-eyes)");
    nav_key(&v, many, 20u, NAV_DOWN, 7u, &index);
    CHECK(v.cursor == 0u && v.first == 0u);
    nav_key(&v, many, 20u, NAV_UP, 7u, &index);
    CHECK(v.cursor == 20u && v.first == 20u - NAV_WINDOW);
    for (i = 0; i < 20u; i++) many[i].group = NAV_DEMOS;
    nav_init(&v, many, 20u, 19u);
    CHECK(v.screen == NAV_LIST && v.group == NAV_DEMOS && v.cursor == 20u && v.first == 20u - NAV_WINDOW && !v.has_top);

    /* the keys: shown = acts, on every screen, with and without TOP; the legend reads as the owner sees it */
    CHECK(named("up/down pick  A start  B/START back", NAV_START) && named("up/down pick", NAV_DOWN) && !named("Y credits", NAV_A));
    nav_init(&v, s7, N7, 0xffffffffu);
    keys_match("TOP", &v, s7, N7);
    nav_key(&v, s7, N7, NAV_A, 7u, &index);
    keys_match("LIST, RUN ALL picked", &v, s7, N7);
    nav_key(&v, s7, N7, NAV_DOWN, 7u, &index);
    keys_match("LIST, an entry picked", &v, s7, N7);
    nav_key(&v, s7, N7, NAV_Y, 7u, &index);
    keys_match("CREDITS from a LIST", &v, s7, N7);
    nav_init(&v, s7, N7, 0xffffffffu);
    nav_key(&v, s7, N7, NAV_Y, 7u, &index);
    keys_match("CREDITS from TOP", &v, s7, N7);
    nav_init(&v, many, 20u, 0xffffffffu);
    keys_match("LIST, one-group disc (no TOP)", &v, many, 20u);
    nav_key(&v, many, 20u, NAV_Y, 7u, &index);
    keys_match("CREDITS, one-group disc", &v, many, 20u);
    {
        char keys[96];

        nav_legend(keys, sizeof keys, NAV_TOP, 1);
        CHECK(!strcmp(keys, "up/down pick  A open  Y credits"));
        nav_legend(keys, sizeof keys, NAV_LIST, 1);
        CHECK(!strcmp(keys, "up/down pick  A start  B/START back  Y credits"));
        for (i = 0; i < NAV_GROUPS; i++) CHECK(strlen(nav_group_name[i]) + 2u + strlen(keys) <= NAV_COLS);
        nav_legend(keys, sizeof keys, NAV_LIST, 0);
        CHECK(!strcmp(keys, "up/down pick  A start  Y credits"));
        nav_legend(keys, sizeof keys, NAV_CREDITS, 1);
        CHECK(!strcmp(keys, "A next  B last  Y/START back"));
        nav_legend(keys, 8u, NAV_LIST, 1);                                                  /* a short buffer: cut, ended */
        CHECK(strlen(keys) == 7u);
    }
    /* two buttons in one pass: the order the menu had before the table */
    nav_init(&v, s7, N7, 0xffffffffu);
    nav_key(&v, s7, N7, NAV_A, 7u, &index);
    nav_key(&v, s7, N7, NAV_DOWN, 7u, &index);
    {
        nav_t w = v;

        CHECK(nav_key(&w, s7, N7, NAV_A | NAV_B, 7u, &index) == NAV_REDRAW && w.screen == NAV_TOP);   /* back before start */
        w = v;
        CHECK(nav_key(&w, s7, N7, NAV_Y | NAV_A, 7u, &index) == NAV_REDRAW && w.screen == NAV_CREDITS);
        w = v;
        CHECK(nav_key(&w, s7, N7, NAV_UP | NAV_DOWN, 7u, &index) == NAV_REDRAW && w.cursor == 0u);  /* up before down */
        w = v;
        CHECK(nav_key(&w, s7, N7, NAV_DOWN | NAV_START, 7u, &index) == NAV_REDRAW && w.screen == NAV_LIST && w.cursor == 2u);
        nav_key(&w, s7, N7, NAV_Y, 7u, &index);
        CHECK(nav_key(&w, s7, N7, NAV_A | NAV_START, 7u, &index) == NAV_REDRAW && w.screen == NAV_LIST);  /* leave before a page */
    }
    {
        nav_span_t span[8];

        CHECK(nav_legend_spans(span, 8u, NAV_LIST, 1, 20u) == 2u);                         /* a key past the columns is not counted */
        CHECK(nav_legend_spans(span, 1u, NAV_LIST, 1, NAV_COLS) == 1u);
    }

    /* a plain title and a shows line (the JSON's "title" and "shows"): the title alone, shows and read when picked */
    {
        static nav_grid_t g;
        suite_entry_t t[2];

        t[0] = s7[0]; t[1] = s7[4];
        t[0].title = "Room shading, four ways"; t[0].shows = "corner shading: none, baked, ground, texture";
        nav_init(&v, t, 2u, 0xffffffffu);
        nav_key(&v, t, 2u, NAV_A, 7u, &index);
        nav_grid(&g, &v, t, 2u, "SH4DER-PVR SUITE s12", "", psm_credits_foot);
        CHECK(!strcmp(g.text[4], " 1 Room shading, four ways") && !strstr(g.text[4], "ao4"));
        nav_key(&v, t, 2u, NAV_DOWN, 7u, &index);
        nav_grid(&g, &v, t, 2u, "SH4DER-PVR SUITE s12", "", psm_credits_foot);
        CHECK(!strcmp(g.text[4], ">1 Room shading, four ways") && !strcmp(g.text[5], "     shows: corner shading: none, baked, ground, texture"));
        CHECK(!strncmp(g.text[6], "     photo", 10u) && g.kind[4] == NAV_K_PICK && g.kind[5] == NAV_K_PICK_MORE && g.kind[6] == NAV_K_PICK_MORE);
        t[0].shows = "a line of features that runs on and on past what the field allows";
        nav_grid(&g, &v, t, 2u, "SH4DER-PVR SUITE s12", "", psm_credits_foot);
        CHECK(strlen(g.text[5]) <= NAV_COLS);
        nav_init(&v, t, 2u, 0xffffffffu);
        nav_grid(&g, &v, t, 2u, "SH4DER-PVR SUITE s12", "", psm_credits_foot);
        CHECK(g.kind[1] == NAV_K_INTRO && g.kind[2] == NAV_K_INTRO && !strncmp(g.text[1], "SHOWS", 5u) && !strncmp(g.text[2], "LOOK AT", 7u));
        CHECK(g.text[1][NAV_INTRO_LABEL - 1u] == ' ' && g.text[1][NAV_INTRO_LABEL] != ' ' && g.text[2][NAV_INTRO_LABEL] != ' ');
    }
    /* the fullest list: 20 entries with title, shows and NO RETURN, picked in the middle: the rows still fit */
    for (i = 0; i < 20u; i++) { many[i] = s7[8]; many[i].title = "Twenty-four letters long"; many[i].shows = "a shows line of the longest length, 49 letters.."; }
    nav_init(&v, many, 20u, 0xffffffffu);
    for (i = 0; i < 10u; i++) nav_key(&v, many, 20u, NAV_DOWN, 7u, &index);
    show("20 titled NO RETURN entries, the 10th picked", &v, many, 20u, "");

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
