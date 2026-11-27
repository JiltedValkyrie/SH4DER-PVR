/* SH4DER-PVR: the logic of a menu, with nothing drawn: a list with headings, tabs, a scrolled text, a held button, a
   place that fits in one word

   include/sh4der_menu.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   A menu is a few small decisions made again in every program: which row the cursor lands on when some rows are
   headings, which rows are on screen, how far a long text is scrolled and where its bar sits, whether a button was
   tapped or held, and where the player was when the program comes back. They are here once, as plain C with no
   drawing, no pad read and no clock of its own: the caller hands over what the pad says and what time it is, and
   draws what the state says. So a host test runs all of it (test/menu_test.c).

   RECOMMENDED (no console run of this file: NOT RUN; nothing here is measured; wiki/effects/menu.md)
   Cost:       RAM: 4 bytes a row of a list (the caller's array), 20 bytes for the menu's state, 6 for a scroll, 16
               for a hold, 8 for a repeat. VRAM 0. No frame cost of its own: every call is a few compares; the
               scroll bar's place takes two integer divides, once a redraw. NOT MEASURED on the SH-4.
   Start with: SH4DER_MENU_HOLD_MS 2000, SH4DER_MENU_REPEAT_FIRST_MS 300, SH4DER_MENU_REPEAT_MS 80,
               SH4DER_MENU_STICK_DEAD 40 (README "Menu tuning")
   Status:     PROTOTYPE v0.1: host tested (test/menu_test.c); the console NOT RUN
   Do:         build each tab's rows once; call sh4der_menu_keep after anything that moves a cursor; give the hold
               and the repeat the time in ms from one clock
   Do not:     put more than SH4DER_MENU_ROWS_MAX rows in a list or more than SH4DER_MENU_TABS_MAX tabs in a menu (the
               packed place has 9 bits a row and 2 for the tab); read a packed word that sh4der_menu_unpack refused
   Maturity:   PROTOTYPE v0.1

   Where it is used:
     include/sh4der_suite.h          the hold of A on a test's last result page (suite_pager_step), the place in the mail
     demo/psm/suite/suite_view.h     the suite disc's menu: two tabs, the list, the card's text

   THE PARTS
     list     rows of a kind (a heading, or an item that can be picked) and an id that is the caller's. The cursor is
              only ever on an item: sh4der_menu_move steps over headings and goes round at the ends.
     window   `first` is the first row on screen; sh4der_menu_keep moves it as little as it can to keep the cursor on
              screen, and shows the headings just above the first row when the cursor stays on screen with them.
     tabs     each tab has its own list and keeps its own cursor and window; left / right go round.
     scroll   a text of `total` lines in a window of `window` lines: the place, clamped, and the bar's thumb.
     repeat   a direction held (a stick past its dead zone, a D-Pad button): one step at once, the next after
              SH4DER_MENU_REPEAT_FIRST_MS, then one every SH4DER_MENU_REPEAT_MS.
     hold     a button that does one thing on a tap and another when held: TAP on a release before the time, FIRED
              once at the time. It starts on a press only: a button already down when the hold is reset is ignored
              until it is let go.
     wrap     a text cut into lines at spaces, a word longer than a line cut where the line ends.
     place    tab, cursor, first row and scroll in one 32-bit word with a mark, for a program that is started again
              (the suite's mail has one spare word).
*/

#ifndef SH4DER_MENU_H
#define SH4DER_MENU_H

#include <stddef.h>
#include <stdint.h>

#define SH4DER_MENU_HOLD_MS         2000u   /* a button held this long fires (the suite: A on the last result page) */
#define SH4DER_MENU_REPEAT_FIRST_MS 300u    /* a direction held: the wait before its second step */
#define SH4DER_MENU_REPEAT_MS       80u     /* ... and between the steps after it */
#define SH4DER_MENU_STICK_DEAD      40      /* a stick axis (-128 .. 127) inside this is at rest */
#define SH4DER_MENU_TABS_MAX        4u
#define SH4DER_MENU_ROWS_MAX        511u    /* rows a list: the packed place has 9 bits for a row */

enum { SH4DER_MENU_HEAD = 0, SH4DER_MENU_ITEM = 1 };

/* A row. kind: SH4DER_MENU_HEAD or _ITEM. flags and id are the caller's (the suite: a grey row; the entry's index). */
typedef struct sh4der_menu_row {
    uint8_t kind, flags;
    uint16_t id;
} sh4der_menu_row_t;

typedef struct sh4der_menu_list {
    const sh4der_menu_row_t *row;
    unsigned n;
} sh4der_menu_list_t;

/* A place in a list: the cursor's row and the first row on screen. */
typedef struct sh4der_menu_pos {
    uint16_t cursor, first;
} sh4der_menu_pos_t;

/* A menu's state: the tab, a place a tab, the text's scroll. */
typedef struct sh4der_menu {
    uint8_t tab, tabs;
    uint16_t scroll;
    sh4der_menu_pos_t pos[SH4DER_MENU_TABS_MAX];
} sh4der_menu_t;

/* ---- list ------------------------------------------------------------------------------------------------------ */

static inline int sh4der_menu_pickable(const sh4der_menu_list_t *l, unsigned i) {
    return i < l->n && l->row[i].kind == SH4DER_MENU_ITEM;
}

/* The first item at or after row `from`, going round; l->n when the list has none. */
static inline unsigned sh4der_menu_next_item(const sh4der_menu_list_t *l, unsigned from) {
    unsigned k;

    for (k = 0; k < l->n; k++) {
        const unsigned i = (from + k) % l->n;

        if (l->row[i].kind == SH4DER_MENU_ITEM) return i;
    }
    return l->n;
}

/* Keep the cursor inside a window of `window` rows, moving `first` as little as that takes; the last rows fill the
   window when the list is longer than it; headings just above the window's first row are shown while the cursor
   stays on screen (so an item at the top has its heading over it). */
static inline void sh4der_menu_keep(const sh4der_menu_list_t *l, sh4der_menu_pos_t *p, unsigned window) {
    unsigned first = p->first, cursor = p->cursor;

    if (!window || !l->n) { p->first = 0; return; }
    if (cursor >= l->n) cursor = l->n - 1u;
    if (cursor < first) first = cursor;
    if (cursor >= first + window) first = cursor - window + 1u;
    if (l->n <= window) first = 0;
    else if (first > l->n - window) first = l->n - window;
    while (first && l->row[first - 1u].kind == SH4DER_MENU_HEAD && cursor - (first - 1u) < window) first--;   /* the headings just above */
    p->cursor = (uint16_t)cursor;
    p->first = (uint16_t)first;
}

/* The cursor to the next item down (dir > 0) or up (dir < 0), over headings, round at the ends. 1 when it moved. */
static inline int sh4der_menu_move(const sh4der_menu_list_t *l, sh4der_menu_pos_t *p, int dir, unsigned window) {
    const unsigned was = p->cursor;
    unsigned k, i = p->cursor;

    if (!l->n || !dir) return 0;
    for (k = 0; k < l->n; k++) {
        i = dir > 0 ? (i + 1u) % l->n : (i + l->n - 1u) % l->n;
        if (l->row[i].kind == SH4DER_MENU_ITEM) break;
    }
    if (k == l->n) return 0;
    p->cursor = (uint16_t)i;
    sh4der_menu_keep(l, p, window);
    return p->cursor != was;
}

/* The cursor on the first item whose id is `id`. 1 when there is one (else the place is left as it was). */
static inline int sh4der_menu_seek(const sh4der_menu_list_t *l, sh4der_menu_pos_t *p, unsigned id, unsigned window) {
    unsigned i;

    for (i = 0; i < l->n; i++)
        if (l->row[i].kind == SH4DER_MENU_ITEM && l->row[i].id == id) {
            p->cursor = (uint16_t)i;
            sh4der_menu_keep(l, p, window);
            return 1;
        }
    return 0;
}

/* ---- menu: tabs ------------------------------------------------------------------------------------------------ */

/* A menu of `tabs` tabs (1 .. SH4DER_MENU_TABS_MAX), each cursor on its list's first item. lists: one a tab. */
static inline void sh4der_menu_init(sh4der_menu_t *m, const sh4der_menu_list_t *lists, unsigned tabs, unsigned window) {
    unsigned t;

    m->tab = 0;
    m->tabs = (uint8_t)(tabs < 1u ? 1u : tabs > SH4DER_MENU_TABS_MAX ? SH4DER_MENU_TABS_MAX : tabs);
    m->scroll = 0;
    for (t = 0; t < SH4DER_MENU_TABS_MAX; t++) {
        m->pos[t].cursor = 0;
        m->pos[t].first = 0;
        if (t < m->tabs) {
            const unsigned i = sh4der_menu_next_item(&lists[t], 0u);

            m->pos[t].cursor = (uint16_t)(i < lists[t].n ? i : 0u);
            sh4der_menu_keep(&lists[t], &m->pos[t], window);
        }
    }
}

/* The next tab (dir > 0) or the one before, round at the ends; each tab keeps its place. The text's scroll goes back
   to its top (another row is picked). 1 when the tab changed. */
static inline int sh4der_menu_tab(sh4der_menu_t *m, int dir) {
    const unsigned was = m->tab;

    if (m->tabs < 2u || !dir) return 0;
    m->tab = (uint8_t)(dir > 0 ? (m->tab + 1u) % m->tabs : (m->tab + m->tabs - 1u) % m->tabs);
    m->scroll = 0;
    return m->tab != was;
}

/* ---- scroll ---------------------------------------------------------------------------------------------------- */

typedef struct sh4der_menu_scroll {
    uint16_t pos, total, window;
} sh4der_menu_scroll_t;

static inline unsigned sh4der_menu_scroll_max(const sh4der_menu_scroll_t *s) {
    return s->total > s->window ? (unsigned)(s->total - s->window) : 0u;
}

/* Scroll by d lines, clamped at both ends. 1 when the place changed. */
static inline int sh4der_menu_scroll_step(sh4der_menu_scroll_t *s, int d) {
    const int max = (int)sh4der_menu_scroll_max(s), was = s->pos;
    int p = was + d;

    if (p < 0) p = 0;
    if (p > max) p = max;
    s->pos = (uint16_t)p;
    return p != was;
}

/* The bar's thumb on a track of `track` pixels: its offset and length (at least `least`). 0 when all of the text is
   on screen (no bar is drawn). Two integer divides: once a redraw, never a frame. */
static inline int sh4der_menu_scroll_bar(const sh4der_menu_scroll_t *s, unsigned track, unsigned least, unsigned *off, unsigned *len) {
    const unsigned max = sh4der_menu_scroll_max(s);
    unsigned n;

    if (!max || !track) return 0;
    n = track * s->window / s->total;
    if (n < least) n = least;
    if (n > track) n = track;
    *len = n;
    *off = (track - n) * (s->pos > max ? max : s->pos) / max;
    return 1;
}

/* ---- repeat: a direction held ------------------------------------------------------------------------------------ */

typedef struct sh4der_menu_repeat {
    int8_t dir;
    uint8_t again;                          /* 1: the first repeat has been given */
    uint32_t next_ms;
} sh4der_menu_repeat_t;

/* dir: -1, 0 or 1, as held now. Returns the step for this pass: dir at once when it starts or turns round, then again
   after `first_ms`, then every `again_ms`; 0 otherwise. now_ms: any clock in ms (its wrap after 49 days is handled). */
static inline int sh4der_menu_repeat(sh4der_menu_repeat_t *r, int dir, uint32_t now_ms, uint32_t first_ms, uint32_t again_ms) {
    dir = dir > 0 ? 1 : dir < 0 ? -1 : 0;
    if (dir != r->dir) {
        r->dir = (int8_t)dir;
        r->again = 0;
        r->next_ms = now_ms + first_ms;
        return dir;
    }
    if (!dir || (int32_t)(now_ms - r->next_ms) < 0) return 0;
    r->again = 1;
    r->next_ms = now_ms + again_ms;
    return dir;
}

/* A stick axis (-128 .. 127) as a direction: 0 inside the dead zone. */
static inline int sh4der_menu_stick_dir(int value, int dead) {
    return value > dead ? 1 : value < -dead ? -1 : 0;
}

/* ---- hold: tap or held ------------------------------------------------------------------------------------------- */

enum { SH4DER_MENU_HOLD_NONE = 0, SH4DER_MENU_HOLD_TAP, SH4DER_MENU_HOLD_HOLDING, SH4DER_MENU_HOLD_FIRED };

typedef struct sh4der_menu_hold {
    uint8_t down;                           /* the button as it was on the last pass */
    uint8_t active;                         /* 1: a press is being timed */
    uint32_t t0_ms;
} sh4der_menu_hold_t;

/* Forget a press that is being timed. A button that is down now does nothing until it is let go and pressed again. */
static inline void sh4der_menu_hold_reset(sh4der_menu_hold_t *h, int down) {
    h->down = (uint8_t)(down != 0);
    h->active = 0;
    h->t0_ms = 0;
}

/* One pass. down: the button now. hold_ms: the time that fires. *permille (may be NULL): how far the hold is, 0 ..
   1000, written for HOLDING and FIRED.
     TAP      the button was let go before the time
     HOLDING  the button is down and being timed
     FIRED    the time is up: once; the press then does nothing more, a tap is not also given on its release */
static inline int sh4der_menu_hold_step(sh4der_menu_hold_t *h, int down, uint32_t now_ms, uint32_t hold_ms, unsigned *permille) {
    const int was = h->down;
    int ev = SH4DER_MENU_HOLD_NONE;

    down = down != 0;
    h->down = (uint8_t)down;
    if (down && !was) { h->active = 1; h->t0_ms = now_ms; }
    if (!h->active) return SH4DER_MENU_HOLD_NONE;
    if (down) {
        const uint32_t dt = now_ms - h->t0_ms;

        if (dt >= hold_ms) {
            h->active = 0;
            if (permille) *permille = 1000u;
            return SH4DER_MENU_HOLD_FIRED;
        }
        /* dt < hold_ms: the quotient is under 1000. A divide a pass while a button is held, not in a vertex loop. */
        if (permille) *permille = hold_ms ? (unsigned)((uint64_t)dt * 1000u / hold_ms) : 1000u;
        ev = SH4DER_MENU_HOLD_HOLDING;
    } else {
        h->active = 0;
        ev = SH4DER_MENU_HOLD_TAP;
    }
    return ev;
}

/* ---- wrap -------------------------------------------------------------------------------------------------------- */

/* The next line of the text s in `cols` columns: *take characters are the line (no space at its end), and the return
   is how far to step s for the line after (the line and the spaces cut with it). 0 at the end of the text. A word
   longer than a line is cut at the line's end. A '\n' ends a line. */
static inline size_t sh4der_menu_wrap(const char *s, size_t cols, size_t *take) {
    size_t n = 0, cut = 0, step;

    *take = 0;
    if (!s || !cols) return 0;
    while (*s == ' ') { s++; cut++; }       /* spaces a line starts with are dropped */
    if (!*s) return 0;
    while (s[n] && s[n] != '\n' && n < cols) n++;
    if (s[n] && s[n] != '\n' && s[n] != ' ') {          /* the line ends inside a word: back to the space before it */
        size_t k = n;

        while (k && s[k - 1u] != ' ') k--;
        if (k) n = k;
    }
    step = n;
    while (n && s[n - 1u] == ' ') n--;
    while (s[step] == ' ') step++;
    if (s[step] == '\n') step++;
    *take = n;
    return cut + step;
}

/* The number of lines sh4der_menu_wrap makes of s. */
static inline unsigned sh4der_menu_wrap_lines(const char *s, size_t cols) {
    size_t take, step;
    unsigned n = 0;

    while (s && (step = sh4der_menu_wrap(s, cols, &take)) != 0) { n++; s += step; }
    return n;
}

/* ---- place: the menu's state in one word ----------------------------------------------------------------------- */

#define SH4DER_MENU_PLACE_MARK 0xA0000000u  /* the top 4 bits of a packed place: a word without them is not one */

/* tab (2 bits), the tab's cursor and first row (9 bits each), the scroll (8 bits: 255 lines at the most; more is
   stored as 255). */
static inline uint32_t sh4der_menu_pack(const sh4der_menu_t *m) {
    const sh4der_menu_pos_t *p = &m->pos[m->tab < SH4DER_MENU_TABS_MAX ? m->tab : 0u];
    const uint32_t scroll = m->scroll > 255u ? 255u : m->scroll;

    return SH4DER_MENU_PLACE_MARK | ((uint32_t)(m->tab & 3u) << 26) | ((uint32_t)(p->cursor & 511u) << 17) | ((uint32_t)(p->first & 511u) << 8) | scroll;
}

/* A packed place back into m (set up by sh4der_menu_init for the same lists). Refused, 0 and m as it was, when the
   word has no mark, the tab is not one of m's, or the cursor is not on an item of that tab's list. */
static inline int sh4der_menu_unpack(sh4der_menu_t *m, const sh4der_menu_list_t *lists, uint32_t word, unsigned window) {
    const unsigned tab = (word >> 26) & 3u, cursor = (word >> 17) & 511u, first = (word >> 8) & 511u;

    if ((word & 0xF0000000u) != SH4DER_MENU_PLACE_MARK || tab >= m->tabs || !sh4der_menu_pickable(&lists[tab], cursor)) return 0;
    m->tab = (uint8_t)tab;
    m->pos[tab].cursor = (uint16_t)cursor;
    m->pos[tab].first = (uint16_t)(first < lists[tab].n ? first : 0u);
    sh4der_menu_keep(&lists[tab], &m->pos[tab], window);
    m->scroll = (uint16_t)(word & 255u);
    return 1;
}

#endif /* SH4DER_MENU_H */
