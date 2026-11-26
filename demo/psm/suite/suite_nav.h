/* SH4DER-PVR: the suite disc's menu: which screen, which row, what a button does, what each screen says (pure C)

   demo/psm/suite/suite_nav.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   suite_menu.c draws and starts programs; everything it decides is here, so a host test (test/suite_nav_test.c)
   runs it and prints every screen as the text the console shows.

   THE SCREENS
     TOP      two lines on what the disc shows and what to look at, then two options in large letters: TECH DEMOS
              and DEVELOPER TOOLS. up / down pick, A opens, Y the credits.
              A disc whose entries are all of one group has no TOP screen: its one list is the menu, as before.
     LIST     one group's entries under its own RUN ALL. up / down pick, A starts, B or START goes back to TOP,
              Y the credits. More entries than NAV_WINDOW scroll, with a line that says how many are off screen.
     CREDITS  the notices (../psm_credits.h) as result pages; suite_menu.c turns them. Y or START goes back.

   RUN ALL is a group's own: its numbered entries in order. DEVELOPER TOOLS: once through, then the list. TECH DEMOS:
   a loop, the first again after the last, until B is pressed in the 5 s between two.

   A screen is NAV_ROWS rows of 64 characters (the page kit's small font over the title-safe area); a row marked big
   is drawn twice the size and takes two rows. Every screen: the title on row 0, the legend on NAV_KEYS_ROW, the
   copyright foot on the last two rows. Each row has a kind (NAV_K_), which is all suite_ui.h needs to draw the panels
   under the text. */

#ifndef SUITE_NAV_H
#define SUITE_NAV_H

#include <stdio.h>
#include <string.h>

typedef struct suite_entry {
    const char *file, *name, *what, *read;
    unsigned order;                         /* place in its group's RUN ALL, 1 ..; 0: not in it */
    unsigned returns;                       /* 1: built with psm_suite.h (the build found its mail) */
    unsigned bytes;
    unsigned group;                         /* NAV_TOOLS or NAV_DEMOS */
    const char *title;                      /* what it is, in plain words, up to 24 (the JSON's "title"), or 0: name and what */
    const char *shows;                      /* the features it shows, up to 49 (the JSON's "shows"), or 0 */
} suite_entry_t;

enum { NAV_TOOLS = 0, NAV_DEMOS = 1, NAV_GROUPS = 2 };
enum { NAV_TOP = 0, NAV_LIST = 1, NAV_CREDITS = 2 };
enum { NAV_NONE = 0, NAV_REDRAW = 1, NAV_LAUNCH = 2, NAV_RUN_ALL = 3 };   /* what a button asks of suite_menu.c */
enum { NAV_WHITE = 0, NAV_GREY = 1, NAV_YELLOW = 2 };

/* What a row is, for the panels under it (suite_ui.h). ITEM / PICK: an option, not picked / picked (a TOP group, RUN
   ALL, an entry); ITEM_MORE / PICK_MORE: a line that belongs to the option above it; FLAG: the NO RETURN line of the
   picked entry; MORE: "... more above / below"; INTRO: the TOP screen's two lines, a label then the text. */
enum { NAV_K_TEXT = 0, NAV_K_HEAD, NAV_K_INTRO, NAV_K_ITEM, NAV_K_ITEM_MORE, NAV_K_PICK, NAV_K_PICK_MORE, NAV_K_FLAG,
       NAV_K_MORE, NAV_K_HELP, NAV_K_NOTE, NAV_K_KEYS, NAV_K_FOOT, NAV_KINDS };

#define NAV_UP    1u                        /* the buttons, as suite_menu.c hands them over (pressed this pass) */
#define NAV_DOWN  2u
#define NAV_A     4u
#define NAV_B     8u
#define NAV_Y     16u
#define NAV_START 32u

/* THE KEYS: one table. nav_key acts from it and nav_legend writes each screen's legend from it, so a key shown is a
   key that works and a key that works is shown (test/suite_nav_test.c presses each button on each screen and reads
   the legend). A row: its screen; 1 when it needs the TOP screen; its rank (two buttons down in one pass: the lower
   rank wins, the order the menu had before the table); what it does; its buttons; the legend's keys and word (0: the
   row is shown by the row before it). The legend is in the table's order. */
enum { NAV_ACT_TOGGLE = 1, NAV_ACT_OPEN, NAV_ACT_PICK_UP, NAV_ACT_PICK_DOWN, NAV_ACT_START, NAV_ACT_BACK, NAV_ACT_CREDITS,
       NAV_ACT_PAGE_NEXT, NAV_ACT_PAGE_PREV, NAV_ACT_LEAVE };

typedef struct nav_keydef {
    unsigned char screen, need_top, rank, act;
    unsigned buttons;
    const char *keys, *word;
} nav_keydef_t;

static const nav_keydef_t nav_keys[] = {
    { NAV_TOP,     0u, 1u, NAV_ACT_TOGGLE,    NAV_UP | NAV_DOWN, "up/down", "pick" },
    { NAV_TOP,     0u, 2u, NAV_ACT_OPEN,      NAV_A,             "A",       "open" },
    { NAV_TOP,     0u, 0u, NAV_ACT_CREDITS,   NAV_Y,             "Y",       "credits" },
    { NAV_LIST,    0u, 1u, NAV_ACT_PICK_UP,   NAV_UP,            "up/down", "pick" },
    { NAV_LIST,    0u, 2u, NAV_ACT_PICK_DOWN, NAV_DOWN,          0,         0 },
    { NAV_LIST,    0u, 4u, NAV_ACT_START,     NAV_A,             "A",       "start" },
    { NAV_LIST,    1u, 3u, NAV_ACT_BACK,      NAV_B | NAV_START, "B/START", "back" },
    { NAV_LIST,    0u, 0u, NAV_ACT_CREDITS,   NAV_Y,             "Y",       "credits" },
    { NAV_CREDITS, 0u, 1u, NAV_ACT_PAGE_NEXT, NAV_A,             "A",       "next" },
    { NAV_CREDITS, 0u, 2u, NAV_ACT_PAGE_PREV, NAV_B,             "B",       "last" },
    { NAV_CREDITS, 0u, 0u, NAV_ACT_LEAVE,     NAV_Y | NAV_START, "Y/START", "back" },
};
#define NAV_KEYS_N ((unsigned)(sizeof nav_keys / sizeof nav_keys[0]))
#define NAV_RANKS 5u

#define NAV_ROWS 24u
#define NAV_COLS 64u
#define NAV_WINDOW 8u                       /* entries of a list on screen at once: rows 4 .. 16 hold them, three detail lines and the two "more" lines */
#define NAV_NOTE_ROW 20u
#define NAV_KEYS_ROW 21u
#define NAV_FOOT_ROW 22u
#define NAV_INTRO_LABEL 9u                  /* the INTRO rows: the label's columns, then the text */

static const char *const nav_group_name[NAV_GROUPS] = { "DEVELOPER TOOLS", "TECH DEMOS" };

typedef struct nav {
    unsigned screen, group;
    unsigned top_cursor;                    /* TOP: 0 TECH DEMOS, 1 DEVELOPER TOOLS */
    unsigned cursor;                        /* LIST: 0 RUN ALL, 1 .. the group's entries */
    unsigned first;                         /* LIST: the first entry on screen (0 ..) */
    unsigned back;                          /* CREDITS: the screen to go back to */
    unsigned page;                          /* CREDITS: the page */
    int has_top;                            /* 0: every entry is of one group */
} nav_t;

typedef struct nav_grid {
    char text[NAV_ROWS][NAV_COLS + 1u];
    unsigned char colour[NAV_ROWS], big[NAV_ROWS], kind[NAV_ROWS];
} nav_grid_t;

/* One key of a legend as it sits in the legend's text: its first column, its length, its buttons (a chip's colour). */
typedef struct nav_span {
    unsigned char col, len;
    unsigned buttons;
} nav_span_t;

static inline unsigned nav_count(const suite_entry_t *list, unsigned n, unsigned group) {
    unsigned i, c = 0;

    for (i = 0; i < n; i++) c += list[i].group == group;
    return c;
}

/* The list index of a group's k-th entry (0 ..), or -1. */
static inline int nav_entry(const suite_entry_t *list, unsigned n, unsigned group, unsigned k) {
    unsigned i;

    for (i = 0; i < n; i++) if (list[i].group == group && k-- == 0u) return (int)i;
    return -1;
}

/* The entry after place `order` in a group's RUN ALL, or -1. */
static inline int nav_next_in_order(const suite_entry_t *list, unsigned n, unsigned group, unsigned order) {
    int best = -1;
    unsigned i;

    for (i = 0; i < n; i++)
        if (list[i].group == group && list[i].order > order && list[i].returns && (best < 0 || list[i].order < list[best].order)) best = (int)i;
    return best;
}

/* What RUN ALL starts after entry `from` came back: the next of its group; TECH DEMOS go round. -1: the run is done. */
static inline int nav_next_chain(const suite_entry_t *list, unsigned n, unsigned from) {
    int next;

    if (from >= n) return -1;
    next = nav_next_in_order(list, n, list[from].group, list[from].order);
    if (next < 0 && list[from].group == NAV_DEMOS) next = nav_next_in_order(list, n, NAV_DEMOS, 0u);
    return next;
}

/* Keep the cursor's entry inside the window. */
static inline void nav_scroll_(nav_t *v) {
    if (v->cursor == 0u) { v->first = 0; return; }
    if (v->cursor - 1u < v->first) v->first = v->cursor - 1u;
    if (v->cursor - 1u >= v->first + NAV_WINDOW) v->first = v->cursor - NAV_WINDOW;
}

static inline void nav_open_(nav_t *v, unsigned group) { v->screen = NAV_LIST; v->group = group; v->cursor = 0; v->first = 0; }

/* The start. from: the list index of the program that came back, or anything >= n at a cold start. */
static inline void nav_init(nav_t *v, const suite_entry_t *list, unsigned n, unsigned from) {
    const unsigned demos = nav_count(list, n, NAV_DEMOS), tools = nav_count(list, n, NAV_TOOLS);
    unsigned i, k = 0;

    memset(v, 0, sizeof *v);
    v->has_top = demos && tools;
    if (from < n) {                         /* back on the row it was started from */
        nav_open_(v, list[from].group);
        v->top_cursor = list[from].group == NAV_DEMOS ? 0u : 1u;
        for (i = 0; i < from; i++) k += list[i].group == list[from].group;
        v->cursor = k + 1u;
        nav_scroll_(v);
    } else if (!v->has_top) nav_open_(v, demos ? NAV_DEMOS : NAV_TOOLS);
    else v->screen = NAV_TOP;
}

/* The table's row for the buttons `hit` on a screen (the lowest rank of those down), or 0: no key of that screen. */
static inline const nav_keydef_t *nav_keydef(unsigned screen, int has_top, unsigned hit) {
    unsigned r, i;

    for (r = 0; r < NAV_RANKS; r++)
        for (i = 0; i < NAV_KEYS_N; i++) {
            const nav_keydef_t *k = &nav_keys[i];

            if (k->rank == r && k->screen == screen && (!k->need_top || has_top) && (hit & k->buttons)) return k;
        }
    return 0;
}

/* A screen's legend from the table, "up/down pick  A start  B/START back  Y credits". */
static inline void nav_legend(char *out, size_t size, unsigned screen, int has_top) {
    size_t len = 0;
    unsigned i;

    if (!size) return;
    out[0] = 0;
    for (i = 0; i < NAV_KEYS_N && len < size; i++) {
        const nav_keydef_t *k = &nav_keys[i];

        if (k->screen != screen || !k->keys || (k->need_top && !has_top)) continue;
        len += (size_t)snprintf(out + len, size - len, "%s%s %s", len ? "  " : "", k->keys, k->word);
    }
}

/* Where each key of nav_legend's text is, for the chips: the spans in the legend's order, their count. A key past
   `cols` is not counted (the legend is cut there). */
static inline unsigned nav_legend_spans(nav_span_t *out, unsigned max, unsigned screen, int has_top, unsigned cols) {
    unsigned i, col = 0, n = 0;

    for (i = 0; i < NAV_KEYS_N && n < max; i++) {
        const nav_keydef_t *k = &nav_keys[i];
        size_t len;

        if (k->screen != screen || !k->keys || (k->need_top && !has_top)) continue;
        if (col) col += 2u;
        len = strlen(k->keys);
        if (col + len > cols) break;
        out[n].col = (unsigned char)col;
        out[n].len = (unsigned char)len;
        out[n].buttons = k->buttons;
        n++;
        col += (unsigned)(len + 1u + strlen(k->word));
    }
    return n;
}

/* A button. hit: the NAV_ buttons that went down this pass; pages: the credits' pages. For NAV_LAUNCH *index is the
   entry to start; for NAV_RUN_ALL it is the run's first entry, or -1 when no entry of the group can come back. */
static inline int nav_key(nav_t *v, const suite_entry_t *list, unsigned n, unsigned hit, unsigned pages, int *index) {
    const unsigned count = nav_count(list, n, v->group), p = pages ? pages : 1u;
    const nav_keydef_t *k = nav_keydef(v->screen, v->has_top, hit);

    *index = -1;
    if (!k) return NAV_NONE;
    switch (k->act) {
    case NAV_ACT_LEAVE:     v->screen = v->back; return NAV_REDRAW;
    case NAV_ACT_PAGE_NEXT: v->page = (v->page + 1u) % p; return NAV_REDRAW;
    case NAV_ACT_PAGE_PREV: v->page = (v->page + p - 1u) % p; return NAV_REDRAW;
    case NAV_ACT_CREDITS:   v->back = v->screen; v->screen = NAV_CREDITS; v->page = 0; return NAV_REDRAW;
    case NAV_ACT_TOGGLE:    v->top_cursor ^= 1u; return NAV_REDRAW;
    case NAV_ACT_OPEN:      nav_open_(v, v->top_cursor == 0u ? NAV_DEMOS : NAV_TOOLS); return NAV_REDRAW;
    case NAV_ACT_PICK_UP:   v->cursor = (v->cursor + count) % (count + 1u); nav_scroll_(v); return NAV_REDRAW;
    case NAV_ACT_PICK_DOWN: v->cursor = (v->cursor + 1u) % (count + 1u); nav_scroll_(v); return NAV_REDRAW;
    case NAV_ACT_BACK:      v->screen = NAV_TOP; return NAV_REDRAW;
    case NAV_ACT_START:
        if (v->cursor == 0u) { *index = nav_next_in_order(list, n, v->group, 0u); return NAV_RUN_ALL; }
        *index = nav_entry(list, n, v->group, v->cursor - 1u);
        return *index < 0 ? NAV_NONE : NAV_LAUNCH;
    default:                return NAV_NONE;
    }
}

static inline void nav_put_(nav_grid_t *g, unsigned row, unsigned colour, unsigned kind, const char *s) {
    size_t len = strlen(s);

    if (row >= NAV_ROWS) return;
    if (len > NAV_COLS) len = NAV_COLS;     /* a line over the screen is cut, as the page kit cuts it */
    memcpy(g->text[row], s, len);
    g->text[row][len] = 0;
    g->colour[row] = (unsigned char)colour;
    g->kind[row] = (unsigned char)kind;
}

/* The TOP or the LIST screen as text. title: the disc's ("SH4DER-PVR SUITE s12"); note: the menu's one line of news;
   foot: the two copyright lines. The CREDITS screen is not made here (it is result pages). An entry with a title
   shows it in place of its name and what, and when picked its shows line above its read. */
static inline void nav_grid(nav_grid_t *g, const nav_t *v, const suite_entry_t *list, unsigned n, const char *title, const char *note, const char *const foot[2]) {
    char s[160], keys[96];
    unsigned row, k;

    memset(g, 0, sizeof *g);
    nav_legend(keys, sizeof keys, v->screen, v->has_top);
    nav_put_(g, 0u, NAV_WHITE, NAV_K_HEAD, title);
    if (v->screen == NAV_TOP) {
        nav_put_(g, 1u, NAV_WHITE, NAV_K_INTRO, "SHOWS    the library's effects, and the tests behind them.");
        nav_put_(g, 2u, NAV_WHITE, NAV_K_INTRO, "LOOK AT  each row: what it is, and the features it shows.");
        for (k = 0; k < 2u; k++) {
            const unsigned group = k == 0u ? NAV_DEMOS : NAV_TOOLS, count = nav_count(list, n, group), on = v->top_cursor == k;

            snprintf(s, sizeof s, "%c %s", on ? '>' : ' ', nav_group_name[group]);
            nav_put_(g, 4u + 4u * k, on ? NAV_YELLOW : NAV_WHITE, on ? NAV_K_PICK : NAV_K_ITEM, s); g->big[4u + 4u * k] = 1;
            snprintf(s, sizeof s, "    %u on this disc: %s", count, group == NAV_DEMOS ? "the effects, to look at and compare" : "tests, proofs and debug views");
            nav_put_(g, 6u + 4u * k, NAV_GREY, on ? NAV_K_PICK_MORE : NAV_K_ITEM_MORE, s);
        }
    } else {
        const unsigned count = nav_count(list, n, v->group);

        nav_put_(g, 1u, NAV_WHITE, NAV_K_TEXT, nav_group_name[v->group]); g->big[1] = 1;
        snprintf(s, sizeof s, "%c RUN ALL: %s", v->cursor == 0u ? '>' : ' ',
                 v->group == NAV_DEMOS ? "the numbered demos, round and round; B stops" : "the numbered tests in order, 20 s a page");
        nav_put_(g, 3u, v->cursor == 0u ? NAV_YELLOW : NAV_WHITE, v->cursor == 0u ? NAV_K_PICK : NAV_K_ITEM, s);
        row = 4u;
        if (v->first) { snprintf(s, sizeof s, "     ... %u more above", v->first); nav_put_(g, row++, NAV_GREY, NAV_K_MORE, s); }
        for (k = v->first; k < count && k < v->first + NAV_WINDOW; k++) {
            const suite_entry_t *e = &list[nav_entry(list, n, v->group, k)];
            const unsigned on = v->cursor == k + 1u;
            char ord[4] = " ";

            if (e->order && e->returns) snprintf(ord, sizeof ord, "%u", e->order % 10u);
            if (e->title && e->title[0]) snprintf(s, sizeof s, "%c%s %.60s", on ? '>' : ' ', ord, e->title);
            else snprintf(s, sizeof s, "%c%s %-10.10s %.49s", on ? '>' : ' ', ord, e->name, e->what);
            nav_put_(g, row++, on ? NAV_YELLOW : NAV_WHITE, on ? NAV_K_PICK : NAV_K_ITEM, s);
            if (on) {
                if (e->shows && e->shows[0]) {
                    snprintf(s, sizeof s, "     shows: %.52s", e->shows);
                    nav_put_(g, row++, NAV_WHITE, NAV_K_PICK_MORE, s);
                }
                snprintf(s, sizeof s, "     %.59s", e->read);
                nav_put_(g, row++, NAV_GREY, NAV_K_PICK_MORE, s);
                if (!e->returns) nav_put_(g, row++, NAV_YELLOW, NAV_K_FLAG, "     NO RETURN: switch the console off after this test");
            }
        }
        if (v->first + NAV_WINDOW < count) { snprintf(s, sizeof s, "     ... %u more below", count - v->first - NAV_WINDOW); nav_put_(g, row++, NAV_GREY, NAV_K_MORE, s); }
    }
    nav_put_(g, 17u, NAV_GREY, NAV_K_HELP, "In a test: hold A+B+X+Y on its result pages for this menu.");
    nav_put_(g, 18u, NAV_GREY, NAV_K_HELP, "A fault page tries the menu after 60 s; if it stays, power off.");
    nav_put_(g, 19u, NAV_GREY, NAV_K_HELP, "No reset button is needed. Every test starts from a full restart.");
    if (note && note[0]) nav_put_(g, NAV_NOTE_ROW, NAV_YELLOW, NAV_K_NOTE, note);
    nav_put_(g, NAV_KEYS_ROW, NAV_WHITE, NAV_K_KEYS, keys);
    nav_put_(g, NAV_FOOT_ROW, NAV_GREY, NAV_K_FOOT, foot[0]);
    nav_put_(g, NAV_FOOT_ROW + 1u, NAV_GREY, NAV_K_FOOT, foot[1]);
}

#endif /* SUITE_NAV_H */
