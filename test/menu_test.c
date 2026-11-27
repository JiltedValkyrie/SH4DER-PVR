/* SH4DER-PVR: host test of the menu logic (include/sh4der_menu.h)

   test/menu_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   What it checks:
     1 list     the cursor moves over headings and goes round at both ends; a list with no item does not move; the
                window follows the cursor, shows an item's heading above it at the top, and its last rows fill it
     2 tabs     each tab keeps its place; left and right go round; one tab does not change
     3 scroll   clamped at both ends; the bar: none when the text fits, at the track's ends at the scroll's ends, never
                shorter than its least length, never past the track
     4 repeat   one step at once, the next after the first wait, then one every repeat; a turn round steps at once;
                the clock's wrap is harmless
     5 hold     a tap, a hold that fires once and gives no tap after, a button already down that is ignored until it
                is let go, the progress in thousandths
     6 wrap     lines no longer than the columns, cut at spaces, a long word cut at the line's end, '\n', the words
                given back in order with nothing lost
     7 place    a place packs and unpacks to itself for every row of two lists; a word with no mark, a tab that is
                not there and a cursor on a heading are refused and change nothing

   Build (from the repository's root):
     cc -std=c99 -O2 -Wall -Wextra -Iinclude test/menu_test.c -o build/menu_test
*/

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "sh4der_menu.h"

static unsigned checks, fails;
#define CHECK(c) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

#define H { SH4DER_MENU_HEAD, 0u, 0u }
#define I(id) { SH4DER_MENU_ITEM, 0u, (id) }

/* 12 rows: H i i H H i i i H i H i (two headings in a row, a heading before the last item) */
static const sh4der_menu_row_t rows_a[] = { H, I(10), I(11), H, H, I(12), I(13), I(14), H, I(15), H, I(16) };
static const sh4der_menu_row_t rows_b[] = { H, I(20), H, I(21), I(22) };
static const sh4der_menu_row_t rows_none[] = { H, H };
static const sh4der_menu_list_t list_a = { rows_a, 12u }, list_b = { rows_b, 5u }, list_none = { rows_none, 2u }, list_empty = { 0, 0u };

static void t_list(void) {
    sh4der_menu_pos_t p = { 1u, 0u };
    unsigned i, seen;

    /* down over every item and round; never a heading; the window always holds the cursor */
    for (i = 0, seen = 0; i < 7u; i++) {
        CHECK(sh4der_menu_move(&list_a, &p, 1, 4u) == 1);
        CHECK(sh4der_menu_pickable(&list_a, p.cursor));
        CHECK(p.cursor >= p.first && p.cursor < p.first + 4u && p.first + 4u <= list_a.n);
        seen++;
    }
    CHECK(seen == 7u && p.cursor == 1u);                         /* seven items: back on the first */
    CHECK(p.first == 0u);                                        /* ... with its heading shown above it */
    CHECK(sh4der_menu_move(&list_a, &p, -1, 4u) == 1 && p.cursor == 11u && p.first == 8u);   /* up from the first: the last, the window's end at the list's */
    CHECK(sh4der_menu_move(&list_a, &p, -1, 4u) == 1 && p.cursor == 9u && p.first == 8u);
    CHECK(sh4der_menu_move(&list_a, &p, -1, 4u) == 1 && p.cursor == 7u && p.first == 7u);
    CHECK(sh4der_menu_move(&list_a, &p, -1, 4u) == 1 && p.cursor == 6u);
    CHECK(sh4der_menu_move(&list_a, &p, -1, 4u) == 1 && p.cursor == 5u && p.first == 3u);    /* the item under two headings: both shown */

    /* no item, no rows, no direction: nothing moves */
    p.cursor = 0; p.first = 0;
    CHECK(sh4der_menu_move(&list_none, &p, 1, 4u) == 0 && p.cursor == 0u);
    CHECK(sh4der_menu_move(&list_empty, &p, 1, 4u) == 0);
    CHECK(sh4der_menu_move(&list_a, &p, 0, 4u) == 0);
    CHECK(sh4der_menu_next_item(&list_none, 0u) == 2u && sh4der_menu_next_item(&list_a, 0u) == 1u && sh4der_menu_next_item(&list_a, 8u) == 9u);
    CHECK(sh4der_menu_next_item(&list_a, 11u) == 11u && sh4der_menu_next_item(&list_b, 2u) == 3u);

    /* a list shorter than the window: first is 0 whatever it was */
    p.cursor = 4u; p.first = 3u;
    sh4der_menu_keep(&list_b, &p, 19u);
    CHECK(p.first == 0u && p.cursor == 4u);
    /* a cursor past the end is pulled in; a window of 0 rows gives first 0 */
    p.cursor = 99u; p.first = 0u;
    sh4der_menu_keep(&list_a, &p, 4u);
    CHECK(p.cursor == 11u && p.first == 8u);
    sh4der_menu_keep(&list_a, &p, 0u);
    CHECK(p.first == 0u);

    /* seek: the first item of an id; an id that is not there leaves the place */
    p.cursor = 1u; p.first = 0u;
    CHECK(sh4der_menu_seek(&list_a, &p, 15u, 4u) == 1 && p.cursor == 9u && p.cursor >= p.first && p.cursor < p.first + 4u);
    CHECK(sh4der_menu_seek(&list_a, &p, 99u, 4u) == 0 && p.cursor == 9u);
    CHECK(sh4der_menu_seek(&list_a, &p, 0u, 4u) == 0);            /* a heading's id is not an item's */
}

static void t_tabs(void) {
    const sh4der_menu_list_t lists[2] = { { rows_a, 12u }, { rows_b, 5u } };
    sh4der_menu_t m;

    sh4der_menu_init(&m, lists, 2u, 4u);
    CHECK(m.tab == 0u && m.tabs == 2u && m.pos[0].cursor == 1u && m.pos[1].cursor == 1u && m.scroll == 0u);
    sh4der_menu_move(&lists[0], &m.pos[0], 1, 4u);
    sh4der_menu_move(&lists[0], &m.pos[0], 1, 4u);
    m.scroll = 3u;
    CHECK(sh4der_menu_tab(&m, 1) == 1 && m.tab == 1u && m.scroll == 0u);
    CHECK(m.pos[0].cursor == 5u && m.pos[1].cursor == 1u);       /* each keeps its place */
    CHECK(sh4der_menu_tab(&m, 1) == 1 && m.tab == 0u);           /* round */
    CHECK(sh4der_menu_tab(&m, -1) == 1 && m.tab == 1u);
    CHECK(sh4der_menu_tab(&m, 0) == 0 && m.tab == 1u);
    sh4der_menu_init(&m, lists, 1u, 4u);
    CHECK(sh4der_menu_tab(&m, 1) == 0 && m.tab == 0u);           /* one tab */
    sh4der_menu_init(&m, lists, 0u, 4u);
    CHECK(m.tabs == 1u);
}

static void t_scroll(void) {
    sh4der_menu_scroll_t s = { 0u, 40u, 15u };
    unsigned off = 99u, len = 99u, i;

    CHECK(sh4der_menu_scroll_max(&s) == 25u);
    CHECK(sh4der_menu_scroll_step(&s, -1) == 0 && s.pos == 0u);
    CHECK(sh4der_menu_scroll_step(&s, 1) == 1 && s.pos == 1u);
    CHECK(sh4der_menu_scroll_step(&s, 100) == 1 && s.pos == 25u);
    CHECK(sh4der_menu_scroll_step(&s, 1) == 0 && s.pos == 25u);
    CHECK(sh4der_menu_scroll_bar(&s, 240u, 16u, &off, &len) == 1 && len == 90u && off + len == 240u);    /* at the end: the thumb at the track's end */
    s.pos = 0;
    CHECK(sh4der_menu_scroll_bar(&s, 240u, 16u, &off, &len) == 1 && off == 0u);
    for (i = 0; i <= 25u; i++) {                                 /* never past the track, never going back */
        unsigned o2, l2;

        s.pos = (uint16_t)i;
        CHECK(sh4der_menu_scroll_bar(&s, 240u, 16u, &o2, &l2) == 1 && o2 + l2 <= 240u && o2 >= off);
        off = o2;
    }
    s.total = 2000u; s.pos = 0;                                  /* a very long text: the least length */
    CHECK(sh4der_menu_scroll_bar(&s, 240u, 16u, &off, &len) == 1 && len == 16u);
    s.total = 15u; s.pos = 7u;                                   /* a text that fits: no bar, and the place is pulled to 0 */
    CHECK(sh4der_menu_scroll_bar(&s, 240u, 16u, &off, &len) == 0);
    CHECK(sh4der_menu_scroll_step(&s, 0) == 1 && s.pos == 0u);
    s.total = 0u;
    CHECK(sh4der_menu_scroll_bar(&s, 240u, 16u, &off, &len) == 0 && sh4der_menu_scroll_max(&s) == 0u);
}

static void t_repeat(void) {
    sh4der_menu_repeat_t r;
    uint32_t t;
    unsigned steps = 0;

    memset(&r, 0, sizeof r);
    CHECK(sh4der_menu_repeat(&r, 0, 0u, 300u, 80u) == 0);
    for (t = 1000u; t < 1000u + 300u + 80u * 3u + 1u; t += 10u) steps += sh4der_menu_repeat(&r, 1, t, 300u, 80u) == 1;
    CHECK(steps == 5u);                                          /* at 0, 300, 380, 460, 540 ms */
    CHECK(sh4der_menu_repeat(&r, -1, t, 300u, 80u) == -1);       /* a turn round: at once */
    CHECK(sh4der_menu_repeat(&r, -1, t + 10u, 300u, 80u) == 0);
    CHECK(sh4der_menu_repeat(&r, 0, t + 20u, 300u, 80u) == 0);   /* let go */
    CHECK(sh4der_menu_repeat(&r, 0, t + 5000u, 300u, 80u) == 0); /* nothing while at rest, however long */
    CHECK(sh4der_menu_repeat(&r, 5, t + 5010u, 300u, 80u) == 1); /* any positive value is 1 */

    /* the clock's wrap: a hold across 0xffffffff steps as anywhere else */
    memset(&r, 0, sizeof r);
    steps = 0;
    for (t = 0; t <= 300u + 80u; t += 10u) steps += sh4der_menu_repeat(&r, 1, 0xffffff00u + t, 300u, 80u) == 1;
    CHECK(steps == 3u);

    CHECK(sh4der_menu_stick_dir(0, 40) == 0 && sh4der_menu_stick_dir(40, 40) == 0 && sh4der_menu_stick_dir(41, 40) == 1);
    CHECK(sh4der_menu_stick_dir(-40, 40) == 0 && sh4der_menu_stick_dir(-41, 40) == -1 && sh4der_menu_stick_dir(-128, 40) == -1 && sh4der_menu_stick_dir(127, 40) == 1);
}

static void t_hold(void) {
    sh4der_menu_hold_t h;
    unsigned pm = 7777u, fired = 0, t;
    int ev;

    /* a tap */
    sh4der_menu_hold_reset(&h, 0);
    CHECK(sh4der_menu_hold_step(&h, 0, 100u, 2000u, &pm) == SH4DER_MENU_HOLD_NONE && pm == 7777u);
    CHECK(sh4der_menu_hold_step(&h, 1, 120u, 2000u, &pm) == SH4DER_MENU_HOLD_HOLDING && pm == 0u);
    CHECK(sh4der_menu_hold_step(&h, 1, 1120u, 2000u, &pm) == SH4DER_MENU_HOLD_HOLDING && pm == 500u);
    CHECK(sh4der_menu_hold_step(&h, 0, 1140u, 2000u, &pm) == SH4DER_MENU_HOLD_TAP);
    CHECK(sh4der_menu_hold_step(&h, 0, 1160u, 2000u, &pm) == SH4DER_MENU_HOLD_NONE);

    /* a hold: HOLDING with the progress rising, FIRED once at the time, nothing after, no tap on the release */
    pm = 0;
    for (t = 0; t <= 3000u; t += 20u) {
        unsigned was = pm;

        ev = sh4der_menu_hold_step(&h, 1, 5000u + t, 2000u, &pm);
        if (t < 2000u) CHECK(ev == SH4DER_MENU_HOLD_HOLDING && pm >= was && pm < 1000u);
        else if (t == 2000u) { CHECK(ev == SH4DER_MENU_HOLD_FIRED && pm == 1000u); fired++; }
        else CHECK(ev == SH4DER_MENU_HOLD_NONE);
    }
    CHECK(fired == 1u);
    CHECK(sh4der_menu_hold_step(&h, 0, 9000u, 2000u, &pm) == SH4DER_MENU_HOLD_NONE);

    /* a button already down at the reset does nothing until it is let go and pressed again */
    sh4der_menu_hold_reset(&h, 1);
    for (t = 0; t <= 3000u; t += 500u) CHECK(sh4der_menu_hold_step(&h, 1, 20000u + t, 2000u, &pm) == SH4DER_MENU_HOLD_NONE);
    CHECK(sh4der_menu_hold_step(&h, 0, 24000u, 2000u, &pm) == SH4DER_MENU_HOLD_NONE);       /* its release is not a tap */
    CHECK(sh4der_menu_hold_step(&h, 1, 24020u, 2000u, &pm) == SH4DER_MENU_HOLD_HOLDING);
    CHECK(sh4der_menu_hold_step(&h, 0, 24040u, 2000u, &pm) == SH4DER_MENU_HOLD_TAP);

    /* a reset in the middle of a press forgets it; a NULL progress is allowed; the clock's wrap */
    CHECK(sh4der_menu_hold_step(&h, 1, 30000u, 2000u, 0) == SH4DER_MENU_HOLD_HOLDING);
    sh4der_menu_hold_reset(&h, 1);
    CHECK(sh4der_menu_hold_step(&h, 1, 33000u, 2000u, 0) == SH4DER_MENU_HOLD_NONE);
    sh4der_menu_hold_reset(&h, 0);
    CHECK(sh4der_menu_hold_step(&h, 1, 0xfffffc00u, 2000u, &pm) == SH4DER_MENU_HOLD_HOLDING);
    CHECK(sh4der_menu_hold_step(&h, 1, 0xfffffc00u + 1000u, 2000u, &pm) == SH4DER_MENU_HOLD_HOLDING && pm == 500u);
    CHECK(sh4der_menu_hold_step(&h, 1, 0xfffffc00u + 2000u, 2000u, &pm) == SH4DER_MENU_HOLD_FIRED);
}

/* The text wrapped at `cols`, its lines joined by one space: the words of the text in order. */
static void wrap_check(const char *text, size_t cols, unsigned want_lines) {
    char joined[512] = "", flat[512] = "";
    const char *s = text;
    size_t take, step, n = 0, i;
    unsigned lines = 0;

    while ((step = sh4der_menu_wrap(s, cols, &take)) != 0) {
        const char *t = s;

        while (*t == ' ') t++;
        CHECK(take > 0u && take <= cols && t[0] != ' ' && t[take - 1u] != ' ');
        if (n) joined[n++] = ' ';
        memcpy(joined + n, t, take); n += take; joined[n] = 0;
        s += step;
        lines++;
        if (lines > 100u) break;
    }
    for (i = 0, n = 0; text[i]; i++) {                           /* the text with runs of spaces and '\n' as one space, trimmed */
        const int sp = text[i] == ' ' || text[i] == '\n';

        if (sp) { if (n && flat[n - 1u] != ' ') flat[n++] = ' '; } else flat[n++] = text[i];
    }
    while (n && flat[n - 1u] == ' ') n--;
    flat[n] = 0;
    CHECK(strcmp(joined, flat) == 0);
    CHECK(lines == want_lines);
    CHECK(sh4der_menu_wrap_lines(text, cols) == lines);
}

static void t_wrap(void) {
    size_t take = 9u;

    wrap_check("An automatic run down a city street by day, with the car's reflection, its shadow and corner shading.", 39u, 3u);
    wrap_check("one", 39u, 1u);
    wrap_check("", 39u, 0u);
    wrap_check("    ", 39u, 0u);
    wrap_check("a  b   c", 3u, 3u);                                 /* runs of spaces: "a", "b", "c" (no two fit in 3 with their spaces) */
    wrap_check("exactly ten", 11u, 1u);
    wrap_check("exactly ten!", 11u, 2u);
    wrap_check("first\nsecond line", 39u, 2u);
    CHECK(sh4der_menu_wrap(0, 39u, &take) == 0u && take == 0u);
    CHECK(sh4der_menu_wrap("abc", 0u, &take) == 0u);
    /* a word longer than the line is cut at the line's end, with nothing lost */
    {
        const char *s = "abcdefghij kl";
        size_t step = sh4der_menu_wrap(s, 4u, &take);

        CHECK(step == 4u && take == 4u && !memcmp(s, "abcd", 4u));
        s += step; step = sh4der_menu_wrap(s, 4u, &take);
        CHECK(step == 4u && take == 4u && !memcmp(s, "efgh", 4u));
        s += step; step = sh4der_menu_wrap(s, 4u, &take);
        CHECK(take == 2u && !memcmp(s, "ij", 2u));
        s += step; step = sh4der_menu_wrap(s, 4u, &take);
        CHECK(take == 2u && !memcmp(s, "kl", 2u));
        s += step;
        CHECK(sh4der_menu_wrap(s, 4u, &take) == 0u);
    }
}

static void t_place(void) {
    const sh4der_menu_list_t lists[2] = { { rows_a, 12u }, { rows_b, 5u } };
    sh4der_menu_t m, r;
    unsigned tab, i, scroll, n = 0;

    for (tab = 0; tab < 2u; tab++)
        for (i = 0; i < lists[tab].n; i++)
            for (scroll = 0; scroll < 300u; scroll += 97u) {
                uint32_t w;

                if (!sh4der_menu_pickable(&lists[tab], i)) continue;
                sh4der_menu_init(&m, lists, 2u, 4u);
                m.tab = (uint8_t)tab; m.pos[tab].cursor = (uint16_t)i;
                sh4der_menu_keep(&lists[tab], &m.pos[tab], 4u);
                m.scroll = (uint16_t)scroll;
                w = sh4der_menu_pack(&m);
                sh4der_menu_init(&r, lists, 2u, 4u);
                CHECK(sh4der_menu_unpack(&r, lists, w, 4u) == 1);
                CHECK(r.tab == tab && r.pos[tab].cursor == i && r.pos[tab].first == m.pos[tab].first && r.scroll == (scroll > 255u ? 255u : scroll));
                n++;
            }
    CHECK(n == 10u * 4u);

    /* refused: no mark, a tab that is not there, a cursor on a heading or past the list; m is as it was */
    sh4der_menu_init(&m, lists, 2u, 4u);
    r = m;
    CHECK(sh4der_menu_unpack(&m, lists, 0u, 4u) == 0 && !memcmp(&m, &r, sizeof m));
    CHECK(sh4der_menu_unpack(&m, lists, 0xffffffffu, 4u) == 0 && !memcmp(&m, &r, sizeof m));
    CHECK(sh4der_menu_unpack(&m, lists, SH4DER_MENU_PLACE_MARK | (2u << 26) | (1u << 17), 4u) == 0 && !memcmp(&m, &r, sizeof m));
    CHECK(sh4der_menu_unpack(&m, lists, SH4DER_MENU_PLACE_MARK | (0u << 26) | (3u << 17), 4u) == 0 && !memcmp(&m, &r, sizeof m));
    CHECK(sh4der_menu_unpack(&m, lists, SH4DER_MENU_PLACE_MARK | (1u << 26) | (5u << 17), 4u) == 0 && !memcmp(&m, &r, sizeof m));
    /* a first row past the list is put right, not refused */
    CHECK(sh4der_menu_unpack(&m, lists, SH4DER_MENU_PLACE_MARK | (0u << 26) | (9u << 17) | (400u << 8), 4u) == 1);
    CHECK(m.pos[0].cursor == 9u && m.pos[0].cursor >= m.pos[0].first && m.pos[0].cursor < m.pos[0].first + 4u);
}

int main(void) {
    t_list();
    t_tabs();
    t_scroll();
    t_repeat();
    t_hold();
    t_wrap();
    t_place();
    printf("menu_test: %u checks, %u failed\n", checks, fails);
    return fails ? 1 : 0;
}
