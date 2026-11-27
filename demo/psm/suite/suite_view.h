/* SH4DER-PVR: the suite disc's menu, look B: two tabs, a list on the left, the picked demo's card on the right
   (pure C, no KallistiOS)

   demo/psm/suite/suite_view.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   suite_menu.c reads the pad, starts programs and copies the frame to the screen; what the menu holds, what a button
   does and every pixel of it is here, so a host test (test/suite_view_test.c) runs it and draws every screen.
   The logic that is not this menu's own (a list with headings, tabs, a scrolled text, a held direction, the place in
   one word) is the library's: include/sh4der_menu.h.

   THE SCREEN (640 x 480, RGB565, drawn by the SH-4 into a RAM copy of the frame; no video memory, no texture)
     head     the disc's title and tag; the two tabs, BY DEMO and BY EFFECT. Left / right on the D-Pad change the tab.
     list     BY DEMO: the demos under their group (DRIVING, ROOMS AND LIGHT, ...). BY EFFECT: the demos under each
              effect they show, and a last row NOT IN THE MENU for the library's effects with no demo on the disc.
              Up / down move over the rows that can be picked (a heading cannot), round at the ends; a held
              direction repeats. VIEW_LIST_ROWS rows are on screen; a thin bar shows where a longer list is.
     card     the picked demo: its title, a chip "START restarts this demo" that never scrolls away, then the text
              the stick scrolls: WHAT IT IS, EFFECTS USED (a bullet each), CONTROLS, and COST only when the list has
              a measured one. A bar shows where a longer text is. The NOT IN THE MENU row's card is text only, and
              the A and START chips of the foot are grey while it is picked.
     foot     the keys as chips: D-Pad choose, Stick scroll, A run, START restart this demo, B back; under them one
              grey line: Y credits, the copyright line, the console's chip revisions (a note in its place, in
              yellow, when the menu has one to give).
   A and START start the picked demo. B on BY EFFECT goes back to BY DEMO; on BY DEMO it does nothing. Y opens the
   CREDITS pages (suite_menu.c draws them with the page kit): A / B turn them, Y or START goes back, as before.

   Cost: RAM 2 x VIEW_ROWS_MAX rows of 4 bytes and VIEW_LINES_MAX card lines of 8 bytes (16 bytes with 64-bit
   pointers), about 3.7 KB on the Dreamcast, beside the frame copy the menu already has. Drawn only after a key press
   or a stick step: no cost a frame. A stick step draws the lines of the card's text only (VIEW_TEXT_Y0 .. the card's
   end). Time on the console: NOT MEASURED (suite_menu.c prints each redraw's time on the serial line). */

#ifndef SUITE_VIEW_H
#define SUITE_VIEW_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "suite_ui.h"                           /* ui_text, ui_rect_, ui_clip, UI_RGB; suite_nav.h's suite_entry_t and NAV_ buttons */
#include "../../../include/sh4der_menu.h"

/* A demo's card (suite_cards.h, written by build.py: the same index as suite_list). effects: the bullet texts. cost:
   0 when the list has no measured one. */
typedef struct suite_card {
    unsigned char menu_group, n_effects;
    const char *const *effects;
    const char *what, *controls, *cost;
} suite_card_t;

/* An effect's heading on the BY EFFECT tab and the demos under it (indices into suite_list, best first). */
typedef struct suite_fx {
    const char *heading;
    unsigned char n;
    const unsigned char *demos;
} suite_fx_t;

/* Everything the menu shows, from suite_list.h and suite_cards.h (or a test's own tables). */
typedef struct view_data {
    const char *title, *tag;
    const suite_entry_t *list;
    const suite_card_t *card;
    unsigned n;
    const char *const *group_name;
    unsigned groups;
    const suite_fx_t *fx;
    unsigned nfx;
    const char *nim_subtitle, *nim_intro;
    const char *const *nim_item;
    unsigned nnim;
} view_data_t;

#define NAV_LEFT  64u                           /* two more buttons beside suite_nav.h's */
#define NAV_RIGHT 128u

enum { VIEW_TAB_DEMO = 0, VIEW_TAB_EFFECT = 1, VIEW_TABS = 2 };
enum { VIEW_LIST = 0, VIEW_CREDITS = 1 };
enum { VIEW_NONE = 0, VIEW_REDRAW, VIEW_REDRAW_TEXT, VIEW_LAUNCH };   /* what a pass asks of suite_menu.c */
enum { VIEW_L_HEAD = 0, VIEW_L_TEXT, VIEW_L_BULLET, VIEW_L_BULLET_MORE };

#define VIEW_ROWS_MAX  256u                     /* rows a tab; build.py refuses a list over 250 */
#define VIEW_LINES_MAX 200u                     /* lines of a card's text */
#define VIEW_ID_NIM    0xFFFFu                  /* the NOT IN THE MENU row's id */
#define VIEW_F_PLAIN   1u                       /* a row that is picked but starts nothing (NOT IN THE MENU) */

/* The layout, in pixels (measured from the approved pictures B_v7_*). */
#define VIEW_HEAD_Y1    61                      /* the head band: 0 .. 61, a rule 62 .. 63 */
#define VIEW_FOOT_Y0    416                     /* the foot band: 416 .. 479, a rule 414 .. 415 */
#define VIEW_TITLE_X    40
#define VIEW_TITLE_Y    24
#define VIEW_TAB_Y0     28
#define VIEW_TAB_Y1     55
#define VIEW_TAB_DEMO_X 440                     /* the tab's text; its fill reaches 8 past it on each side */
#define VIEW_TAB_FX_X   520
#define VIEW_LCARD_X0   32
#define VIEW_LCARD_X1   257
#define VIEW_RCARD_X0   268
#define VIEW_RCARD_X1   607
#define VIEW_CARD_Y0    72
#define VIEW_CARD_Y1    404
#define VIEW_LIST_ROWS  19u
#define VIEW_LIST_Y(k)  (80 + 17 * (int)(k))    /* row k of the list on screen */
#define VIEW_LIST_HEAD_X 40
#define VIEW_LIST_ITEM_X 46
#define VIEW_LIST_COLS  25u                     /* an item's title: 25 characters */
#define VIEW_CARD_X     280                     /* the card's text column */
#define VIEW_CARD_COLS  39u                     /* ... and its width in characters */
#define VIEW_BULLET_X   296
#define VIEW_BULLET_COLS 37u
#define VIEW_CTITLE_Y   88
#define VIEW_CTITLE_BIG 20u                     /* a title up to this long is drawn double size; a longer one normal size */
#define VIEW_CHIP_Y     126
#define VIEW_RULE_Y     150
#define VIEW_TEXT_Y0    158
#define VIEW_TEXT_ROWS  15u                     /* 240 lines of pixels: 158 .. 397 */
#define VIEW_TEXT_CH    16
#define VIEW_SECTION_GAP 8                      /* pixels between two sections of the text */
#define VIEW_KEYS_Y     428
#define VIEW_FOOT_Y     456

/* Look B's colours (the pictures' own, already RGB565 values). */
#define VIEW_C_BAND    UI_RGB(24, 28, 33)
#define VIEW_C_RULE    UI_RGB(41, 48, 66)
#define VIEW_C_BG      UI_RGB(16, 16, 24)
#define VIEW_C_CARD    UI_RGB(24, 32, 41)
#define VIEW_C_BORDER  UI_RGB(49, 56, 66)
#define VIEW_C_ACCENT  UI_RGB(123, 186, 255)
#define VIEW_C_PICK    UI_RGB(33, 73, 132)
#define VIEW_C_TEXT    UI_RGB(239, 243, 247)
#define VIEW_C_WHITE   UI_RGB(255, 255, 255)
#define VIEW_C_GREY    UI_RGB(148, 158, 173)
#define VIEW_C_CHIP    UI_RGB(66, 73, 90)
#define VIEW_C_CHIP_A  UI_RGB(181, 52, 49)
#define VIEW_C_CHIP_B  UI_RGB(41, 101, 198)
#define VIEW_C_YELLOW  UI_RGB(255, 220, 0)

typedef struct view_line {
    uint8_t kind, len;
    uint16_t y;                                 /* from the text's top, pixels */
    const char *s;
} view_line_t;

typedef struct view {
    const view_data_t *d;
    sh4der_menu_row_t rows[VIEW_TABS][VIEW_ROWS_MAX];
    sh4der_menu_list_t lists[VIEW_TABS];
    sh4der_menu_t m;
    sh4der_menu_scroll_t scroll;                /* the card's text, in lines of VIEW_TEXT_CH pixels */
    view_line_t line[VIEW_LINES_MAX];
    unsigned nlines;
    unsigned screen, page;                      /* VIEW_LIST or VIEW_CREDITS; the credits' page */
    sh4der_menu_repeat_t rep_list, rep_text;
    char note[65];                              /* the menu's one line of news, or "" */
} view_t;

/* ---- the lists ------------------------------------------------------------------------------------------------- */

static inline void view_row_(view_t *v, unsigned tab, unsigned kind, unsigned flags, unsigned id) {
    sh4der_menu_row_t *r;

    if (v->lists[tab].n >= VIEW_ROWS_MAX) return;
    r = &v->rows[tab][v->lists[tab].n++];
    r->kind = (uint8_t)kind; r->flags = (uint8_t)flags; r->id = (uint16_t)id;
}

/* The two tabs' rows from the data: a group or an effect with no demo gets no heading. */
static inline void view_build_(view_t *v) {
    const view_data_t *d = v->d;
    unsigned g, i, k;

    for (k = 0; k < VIEW_TABS; k++) { v->lists[k].row = v->rows[k]; v->lists[k].n = 0; }
    for (g = 0; g < d->groups; g++) {
        unsigned any = 0;

        for (i = 0; i < d->n; i++) {
            if (d->card[i].menu_group != g) continue;
            if (!any) { view_row_(v, VIEW_TAB_DEMO, SH4DER_MENU_HEAD, 0u, g); any = 1u; }
            view_row_(v, VIEW_TAB_DEMO, SH4DER_MENU_ITEM, 0u, i);
        }
    }
    for (g = 0; g < d->nfx; g++) {
        if (!d->fx[g].n) continue;
        view_row_(v, VIEW_TAB_EFFECT, SH4DER_MENU_HEAD, 0u, g);
        for (k = 0; k < d->fx[g].n; k++) if (d->fx[g].demos[k] < d->n) view_row_(v, VIEW_TAB_EFFECT, SH4DER_MENU_ITEM, 0u, d->fx[g].demos[k]);
    }
    if (d->nnim) view_row_(v, VIEW_TAB_EFFECT, SH4DER_MENU_ITEM, VIEW_F_PLAIN, VIEW_ID_NIM);
}

/* The picked row, or 0 when the tab's list has no item. */
static inline const sh4der_menu_row_t *view_row(const view_t *v) {
    const sh4der_menu_list_t *l = &v->lists[v->m.tab];
    const unsigned c = v->m.pos[v->m.tab].cursor;

    return sh4der_menu_pickable(l, c) ? &l->row[c] : 0;
}

/* The picked demo's index in the list, or -1 (the NOT IN THE MENU row, or nothing picked). */
static inline int view_entry(const view_t *v) {
    const sh4der_menu_row_t *r = view_row(v);

    return r && !(r->flags & VIEW_F_PLAIN) && r->id < v->d->n ? (int)r->id : -1;
}

/* ---- the card's text ------------------------------------------------------------------------------------------- */

static inline void view_line_(view_t *v, unsigned kind, const char *s, size_t len, unsigned *y) {
    view_line_t *l;

    if (v->nlines >= VIEW_LINES_MAX) return;
    l = &v->line[v->nlines++];
    l->kind = (uint8_t)kind; l->len = (uint8_t)(len > 255u ? 255u : len); l->y = (uint16_t)*y; l->s = s;
    *y += VIEW_TEXT_CH;
}

static inline void view_wrap_(view_t *v, unsigned kind, unsigned more, const char *s, size_t cols, unsigned *y) {
    size_t take, step;
    unsigned first = 1u;

    while (s && (step = sh4der_menu_wrap(s, cols, &take)) != 0) {
        const char *t = s;

        while (*t == ' ') t++;
        view_line_(v, first ? kind : more, t, take, y);
        first = 0u;
        s += step;
    }
}

static inline void view_section_(view_t *v, const char *head, unsigned *y) {
    if (v->nlines) *y += VIEW_SECTION_GAP;
    view_line_(v, VIEW_L_HEAD, head, strlen(head), y);
}

/* The picked row's text into v->line, and the scroll's size. The scroll's place is kept when `keep`, else the top. */
static inline void view_card(view_t *v, int keep) {
    const view_data_t *d = v->d;
    const sh4der_menu_row_t *r = view_row(v);
    unsigned y = 0, i;

    v->nlines = 0;
    if (r && (r->flags & VIEW_F_PLAIN)) {
        view_wrap_(v, VIEW_L_TEXT, VIEW_L_TEXT, d->nim_intro, VIEW_CARD_COLS, &y);
        y += VIEW_SECTION_GAP;
        for (i = 0; i < d->nnim; i++) view_wrap_(v, VIEW_L_BULLET, VIEW_L_BULLET_MORE, d->nim_item[i], VIEW_BULLET_COLS, &y);
    } else if (r && r->id < d->n) {
        const suite_card_t *c = &d->card[r->id];

        view_section_(v, "WHAT IT IS", &y);
        view_wrap_(v, VIEW_L_TEXT, VIEW_L_TEXT, c->what, VIEW_CARD_COLS, &y);
        if (c->n_effects) {
            view_section_(v, "EFFECTS USED", &y);
            for (i = 0; i < c->n_effects; i++) view_wrap_(v, VIEW_L_BULLET, VIEW_L_BULLET_MORE, c->effects[i], VIEW_BULLET_COLS, &y);
        }
        view_section_(v, "CONTROLS", &y);
        view_wrap_(v, VIEW_L_TEXT, VIEW_L_TEXT, c->controls, VIEW_CARD_COLS, &y);
        if (c->cost && c->cost[0]) {
            view_section_(v, "COST", &y);
            view_wrap_(v, VIEW_L_TEXT, VIEW_L_TEXT, c->cost, VIEW_CARD_COLS, &y);
        }
    }
    v->scroll.total = (uint16_t)((y + (unsigned)VIEW_TEXT_CH - 1u) / (unsigned)VIEW_TEXT_CH);
    v->scroll.window = (uint16_t)VIEW_TEXT_ROWS;
    if (!keep) v->scroll.pos = 0;
    sh4der_menu_scroll_step(&v->scroll, 0);
    v->m.scroll = v->scroll.pos;
}

/* ---- start, buttons -------------------------------------------------------------------------------------------- */

/* The start. from: the list index of the program that came back, or anything >= n at a cold start. place: the word
   that program handed back (the mail's spare): when it is this menu's place and its row is that program, the menu
   opens as it was left (tab, list, scroll); else on the program's row of BY DEMO. */
static inline void view_init(view_t *v, const view_data_t *d, unsigned from, uint32_t place) {
    memset(v, 0, sizeof *v);
    v->d = d;
    view_build_(v);
    sh4der_menu_init(&v->m, v->lists, VIEW_TABS, VIEW_LIST_ROWS);
    if (from < d->n) {
        const sh4der_menu_t cold = v->m;

        if (sh4der_menu_unpack(&v->m, v->lists, place, VIEW_LIST_ROWS) && view_entry(v) == (int)from) {
            v->scroll.pos = v->m.scroll;
            view_card(v, 1);
            return;
        }
        v->m = cold;
        sh4der_menu_seek(&v->lists[VIEW_TAB_DEMO], &v->m.pos[VIEW_TAB_DEMO], from, VIEW_LIST_ROWS);
    }
    view_card(v, 0);
}

/* The menu's place for the mail's spare word. */
static inline uint32_t view_place(view_t *v) {
    v->m.scroll = v->scroll.pos;
    return sh4der_menu_pack(&v->m);
}

/* One pass. hit: the NAV_ buttons that went down this pass. dpad: -1 while up is held, 1 while down, else 0. stick:
   the stick's up / down axis, -128 .. 127 (down is positive). now_ms: any clock in ms. pages: the credits' pages.
   VIEW_LAUNCH: *index is the entry to start. VIEW_REDRAW_TEXT: only the card's text moved. */
static inline int view_key(view_t *v, unsigned hit, int dpad, int stick, uint32_t now_ms, unsigned pages, int *index) {
    const unsigned p = pages ? pages : 1u;
    sh4der_menu_list_t *l = &v->lists[v->m.tab];
    int step, act = VIEW_NONE;

    *index = -1;
    if (v->screen == VIEW_CREDITS) {
        if (hit & (NAV_Y | NAV_START)) { v->screen = VIEW_LIST; return VIEW_REDRAW; }
        if (hit & NAV_A) { v->page = (v->page + 1u) % p; return VIEW_REDRAW; }
        if (hit & NAV_B) { v->page = (v->page + p - 1u) % p; return VIEW_REDRAW; }
        return VIEW_NONE;
    }
    if (hit & NAV_Y) { v->screen = VIEW_CREDITS; v->page = 0; return VIEW_REDRAW; }
    if (hit & (NAV_LEFT | NAV_RIGHT)) {
        if (sh4der_menu_tab(&v->m, hit & NAV_RIGHT ? 1 : -1)) { view_card(v, 0); return VIEW_REDRAW; }
    }
    if ((hit & NAV_B) && v->m.tab != VIEW_TAB_DEMO) { v->m.tab = VIEW_TAB_DEMO; view_card(v, 0); return VIEW_REDRAW; }
    if (hit & (NAV_A | NAV_START)) {
        *index = view_entry(v);
        return *index < 0 ? VIEW_NONE : VIEW_LAUNCH;
    }
    step = sh4der_menu_repeat(&v->rep_list, dpad, now_ms, SH4DER_MENU_REPEAT_FIRST_MS, SH4DER_MENU_REPEAT_MS);
    if (step && sh4der_menu_move(l, &v->m.pos[v->m.tab], step, VIEW_LIST_ROWS)) { view_card(v, 0); act = VIEW_REDRAW; }
    step = sh4der_menu_repeat(&v->rep_text, sh4der_menu_stick_dir(stick, SH4DER_MENU_STICK_DEAD), now_ms, SH4DER_MENU_REPEAT_FIRST_MS, SH4DER_MENU_REPEAT_MS);
    if (step && sh4der_menu_scroll_step(&v->scroll, step)) { v->m.scroll = v->scroll.pos; if (act == VIEW_NONE) act = VIEW_REDRAW_TEXT; }
    return act;
}

/* The lines of the screen a VIEW_REDRAW_TEXT changes (the card from its rule down). */
static inline void view_text_lines(int *y0, int *y1) { *y0 = VIEW_RULE_Y + 1; *y1 = VIEW_CARD_Y1 - 1; }

/* ---- drawing --------------------------------------------------------------------------------------------------- */

static inline void view_box_(uint16_t *fb, int x0, int y0, int x1, int y1) {    /* a card: a 1 pixel border round its fill */
    ui_rect_(fb, x0, y0, x1, y1, VIEW_C_BORDER);
    ui_rect_(fb, x0 + 1, y0 + 1, x1 - 1, y1 - 1, VIEW_C_CARD);
}

/* A key's chip and its word at x; returns the x after them (16 pixels of gap). */
static inline int view_chip_(uint16_t *fb, const ui_font_t *f, int x, int y, const char *key, const char *word, uint16_t chip, int grey) {
    const int kw = (int)strlen(key) * UI_CW, ww = (int)strlen(word) * UI_CW;

    ui_rect_(fb, x, y - 2, x + kw + 7, y + 17, grey ? VIEW_C_RULE : chip);
    ui_text(fb, f, 1u, x + 4, y, grey ? VIEW_C_GREY : VIEW_C_WHITE, key, 16u);
    ui_text(fb, f, 1u, x + kw + 12, y, grey ? VIEW_C_GREY : VIEW_C_TEXT, word, 40u);
    return x + kw + 12 + ww + 16;
}

static inline void view_tab_(uint16_t *fb, const ui_font_t *f, int x, const char *s, int on) {
    if (on) ui_rect_(fb, x - 8, VIEW_TAB_Y0, x + (int)strlen(s) * UI_CW + 7, VIEW_TAB_Y1, VIEW_C_PICK);
    ui_text(fb, f, 1u, x, VIEW_TAB_Y0 + 6, on ? VIEW_C_WHITE : VIEW_C_GREY, s, 16u);
}

/* The screen (the LIST screen: the credits are the page kit's) into fb, inside ui_clip's lines. rev: the console's
   revisions as text ("" for none). foot: the copyright line (up to 37 characters are shown), or 0. */
static inline void view_draw(uint16_t *fb, const ui_font_t *f, const view_t *v, const char *rev, const char *foot) {
    const view_data_t *d = v->d;
    const sh4der_menu_list_t *l = &v->lists[v->m.tab];
    const sh4der_menu_pos_t *pos = &v->m.pos[v->m.tab];
    const sh4der_menu_row_t *picked = view_row(v);
    const int plain = picked && (picked->flags & VIEW_F_PLAIN), nothing = !picked;
    unsigned k, off, len;
    int x;

    /* the bands and the two cards */
    ui_rect_(fb, 0, 0, 639, VIEW_HEAD_Y1, VIEW_C_BAND);
    ui_rect_(fb, 0, VIEW_HEAD_Y1 + 1, 639, VIEW_HEAD_Y1 + 2, VIEW_C_RULE);
    ui_rect_(fb, 0, VIEW_HEAD_Y1 + 3, 639, VIEW_FOOT_Y0 - 3, VIEW_C_BG);
    ui_rect_(fb, 0, VIEW_FOOT_Y0 - 2, 639, VIEW_FOOT_Y0 - 1, VIEW_C_RULE);
    ui_rect_(fb, 0, VIEW_FOOT_Y0, 639, 479, VIEW_C_BAND);
    view_box_(fb, VIEW_LCARD_X0, VIEW_CARD_Y0, VIEW_LCARD_X1, VIEW_CARD_Y1);
    view_box_(fb, VIEW_RCARD_X0, VIEW_CARD_Y0, VIEW_RCARD_X1, VIEW_CARD_Y1);

    /* the head */
    ui_text(fb, f, 2u, VIEW_TITLE_X, VIEW_TITLE_Y, VIEW_C_TEXT, d->title, 20u);
    ui_text(fb, f, 1u, VIEW_TITLE_X + (int)strlen(d->title) * 2 * UI_CW + 8, VIEW_TITLE_Y + 11, VIEW_C_GREY, d->tag, 8u);
    ui_text(fb, f, 1u, VIEW_TAB_DEMO_X - 40, VIEW_TAB_Y0 + 6, VIEW_C_GREY, "< >", 3u);
    view_tab_(fb, f, VIEW_TAB_DEMO_X, "BY DEMO", v->m.tab == VIEW_TAB_DEMO);
    view_tab_(fb, f, VIEW_TAB_FX_X, "BY EFFECT", v->m.tab == VIEW_TAB_EFFECT);

    /* the list */
    for (k = 0; k < VIEW_LIST_ROWS && pos->first + k < l->n; k++) {
        const unsigned i = pos->first + k;
        const sh4der_menu_row_t *r = &l->row[i];
        const int y = VIEW_LIST_Y(k), on = i == pos->cursor && r->kind == SH4DER_MENU_ITEM;

        if (on) {
            ui_rect_(fb, VIEW_LCARD_X0 + 7, y - 1, VIEW_LCARD_X1 - 10, y + 16, VIEW_C_PICK);
            ui_rect_(fb, VIEW_LCARD_X0 + 4, y - 1, VIEW_LCARD_X0 + 6, y + 16, VIEW_C_ACCENT);
        }
        if (r->kind == SH4DER_MENU_HEAD)
            ui_text(fb, f, 1u, VIEW_LIST_HEAD_X, y, VIEW_C_ACCENT, v->m.tab == VIEW_TAB_DEMO ? d->group_name[r->id] : d->fx[r->id].heading, VIEW_LIST_COLS + 1u);
        else if (r->flags & VIEW_F_PLAIN)
            ui_text(fb, f, 1u, VIEW_LIST_HEAD_X, y, on ? VIEW_C_WHITE : VIEW_C_ACCENT, "NOT IN THE MENU", VIEW_LIST_COLS + 1u);
        else {
            const suite_entry_t *e = &d->list[r->id];

            ui_text(fb, f, 1u, VIEW_LIST_ITEM_X, y, on ? VIEW_C_WHITE : VIEW_C_TEXT, e->title && e->title[0] ? e->title : e->name, VIEW_LIST_COLS);
        }
    }
    {
        sh4der_menu_scroll_t ls;

        ls.pos = pos->first; ls.total = (uint16_t)l->n; ls.window = (uint16_t)VIEW_LIST_ROWS;
        if (sh4der_menu_scroll_bar(&ls, (unsigned)(VIEW_CARD_Y1 - VIEW_CARD_Y0 - 9), 12u, &off, &len)) {
            ui_rect_(fb, VIEW_LCARD_X1 - 6, VIEW_CARD_Y0 + 5, VIEW_LCARD_X1 - 3, VIEW_CARD_Y1 - 5, VIEW_C_BORDER);
            ui_rect_(fb, VIEW_LCARD_X1 - 6, VIEW_CARD_Y0 + 5 + (int)off, VIEW_LCARD_X1 - 3, VIEW_CARD_Y0 + 4 + (int)(off + len), VIEW_C_ACCENT);
        }
    }

    /* the card: its title, the chip (or the plain row's line), the rule, the text and its bar */
    if (!nothing) {
        const char *title = plain ? "Not in the menu" : d->list[picked->id].title && d->list[picked->id].title[0] ? d->list[picked->id].title : d->list[picked->id].name;

        if (strlen(title) <= VIEW_CTITLE_BIG) ui_text(fb, f, 2u, VIEW_CARD_X, VIEW_CTITLE_Y, VIEW_C_TEXT, title, VIEW_CTITLE_BIG);
        else ui_text(fb, f, 1u, VIEW_CARD_X, VIEW_CTITLE_Y + 8, VIEW_C_TEXT, title, VIEW_CARD_COLS);
        if (plain) ui_text(fb, f, 1u, VIEW_CARD_X, VIEW_CHIP_Y, VIEW_C_GREY, d->nim_subtitle, VIEW_CARD_COLS);
        else view_chip_(fb, f, VIEW_CARD_X, VIEW_CHIP_Y, "START", "restarts this demo", VIEW_C_CHIP, 0);
    }
    ui_rect_(fb, VIEW_CARD_X, VIEW_RULE_Y, VIEW_RCARD_X1 - 16, VIEW_RULE_Y, VIEW_C_BORDER);
    for (k = 0; k < v->nlines; k++) {
        const view_line_t *ln = &v->line[k];
        const int yy = (int)ln->y - (int)v->scroll.pos * VIEW_TEXT_CH, y = VIEW_TEXT_Y0 + yy;

        if (yy < 0 || yy + VIEW_TEXT_CH > (int)VIEW_TEXT_ROWS * VIEW_TEXT_CH) continue;
        if (ln->kind == VIEW_L_BULLET) ui_rect_(fb, VIEW_CARD_X + 4, y + 6, VIEW_CARD_X + 8, y + 10, VIEW_C_ACCENT);
        ui_text(fb, f, 1u, ln->kind == VIEW_L_BULLET || ln->kind == VIEW_L_BULLET_MORE ? VIEW_BULLET_X : VIEW_CARD_X, y,
                ln->kind == VIEW_L_HEAD ? VIEW_C_ACCENT : VIEW_C_TEXT, ln->s, ln->len);
    }
    if (sh4der_menu_scroll_bar(&v->scroll, (unsigned)VIEW_TEXT_ROWS * (unsigned)VIEW_TEXT_CH - 1u, 16u, &off, &len)) {
        ui_rect_(fb, VIEW_RCARD_X1 - 10, VIEW_TEXT_Y0, VIEW_RCARD_X1 - 7, VIEW_TEXT_Y0 + (int)VIEW_TEXT_ROWS * VIEW_TEXT_CH - 2, VIEW_C_BORDER);
        ui_rect_(fb, VIEW_RCARD_X1 - 10, VIEW_TEXT_Y0 + (int)off, VIEW_RCARD_X1 - 7, VIEW_TEXT_Y0 + (int)(off + len) - 1, VIEW_C_ACCENT);
    }

    /* the foot: the keys, then the grey line (or the note) */
    x = view_chip_(fb, f, 40, VIEW_KEYS_Y, "D-Pad", "choose", VIEW_C_CHIP, 0);
    x = view_chip_(fb, f, x, VIEW_KEYS_Y, "Stick", "scroll", VIEW_C_CHIP, 0);
    x = view_chip_(fb, f, x, VIEW_KEYS_Y, "A", "run", VIEW_C_CHIP_A, plain || nothing);
    x = view_chip_(fb, f, x, VIEW_KEYS_Y, "START", "restart this demo", VIEW_C_CHIP, plain || nothing);
    view_chip_(fb, f, x, VIEW_KEYS_Y, "B", "back", VIEW_C_CHIP_B, 0);
    if (v->note[0]) ui_text(fb, f, 1u, 40, VIEW_FOOT_Y, VIEW_C_YELLOW, v->note, 48u);
    else {
        ui_text(fb, f, 1u, 40, VIEW_FOOT_Y, VIEW_C_GREY, "Y credits", 9u);
        if (foot) ui_text(fb, f, 1u, 40 + 11 * UI_CW, VIEW_FOOT_Y, VIEW_C_GREY, foot, 37u);
    }
    if (rev && rev[0]) ui_text(fb, f, 1u, 600 - (int)strlen(rev) * UI_CW, VIEW_FOOT_Y, VIEW_C_GREY, rev, 24u);
}

#endif /* SUITE_VIEW_H */
