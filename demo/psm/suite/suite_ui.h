/* SH4DER-PVR: the suite menu's panels: a skin of slots drawn under suite_nav.h's text (pure C, no KallistiOS)

   demo/psm/suite/suite_ui.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   suite_nav.h says what each row of a screen is (its kind); this draws the screen from that into a 640 x 480 RGB565
   frame: the background, a band for the title and one for the keys and the copyright, a panel under each option
   (an option and the lines that belong to it are one panel), a chip under each key of the legend, then the text,
   blended over what is under it. suite_menu.c gives it the frame buffer and the page kit's glyphs; the host test
   (test/suite_ui_test.c) gives it a buffer of its own and checks the screens it makes.

   A slot is a colour, or a 9-slice: the corners as they are, the edges stretched, the centre one pixel stretched.
   Pixels UI_CLEAR in a 9-slice are not drawn (the panel's rounded corners). suite_skin.h, written by
   ../tools/suite_skin.py from the UI Pack (Kenney, CC0), fills ui_skin's 9-slices; a slot it does not fill, or one
   too small for its 9-slice, is drawn in its colour.

   Speed: draw into a RAM copy of the frame (cached), then copy the lines drawn to the frame buffer by store queue.
   ui_dirty gives the lines two screens differ in and ui_clip keeps every call inside them, so a key press redraws a
   few rows, not the screen; a 9-slice is drawn as runs of one colour; text is mixed in shifts, never a divide.
   Cost: no video memory but the frame buffer; the skin is RAM (suite_skin.h says how much). It is drawn only when
   the menu redraws, after a key press: no cost a frame. */

#ifndef SUITE_UI_H
#define SUITE_UI_H

#include <stdint.h>
#include <string.h>

#include "suite_nav.h"

#define UI_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define UI_CLEAR 0xF81Fu                    /* a 9-slice pixel left as it is: suite_skin.py never writes it for a real one */

#define UI_X0 64                            /* the text's grid: suite_nav.h's 64 columns of 8 over the title-safe area */
#define UI_Y0 48
#define UI_CW 8
#define UI_CH 16
#define UI_ROW_Y(r) (UI_Y0 + UI_CH * (int)(r))
#define UI_PANEL_X0 (UI_X0 - 8)             /* a panel reaches 8 past the text on each side: inside the action-safe area */
#define UI_PANEL_X1 (640 - UI_X0 + 7)
#define UI_HEAD_Y1 (UI_ROW_Y(1) - 1)        /* the title's band, top of the screen to under row 0 */
#define UI_FOOT_Y0 (UI_ROW_Y(NAV_KEYS_ROW) - 2)

enum { UI_BG, UI_BAND, UI_RULE, UI_ROW, UI_ROW_PICK, UI_ROW_MARK, UI_FLAG, UI_CHIP_PAD, UI_CHIP_A, UI_CHIP_B, UI_CHIP_Y,
       UI_SLOTS };

typedef struct ui_slot {
    uint16_t flat;                          /* the colour: the centre of a 9-slice, all of a plain slot */
    const uint16_t *pix;                    /* 0: plain; else w x h RGB565, row by row */
    unsigned char w, h, l, r, t, b;         /* the 9-slice's size and its border widths */
} ui_slot_t;

/* The text's colours: suite_nav.h's NAV_WHITE, NAV_GREY, NAV_YELLOW; the grey on a picked panel; the INTRO label. */
enum { UI_TXT_WHITE, UI_TXT_GREY, UI_TXT_YELLOW, UI_TXT_GREY_PICK, UI_TXT_LABEL, UI_TXTS };

static const uint16_t ui_txt[UI_TXTS] = {
    UI_RGB(255, 255, 255), UI_RGB(160, 168, 180), UI_RGB(255, 220, 0), UI_RGB(205, 220, 235), UI_RGB(255, 170, 60)
};

/* The glyphs: 96 characters from 32, each ch rows of cw covers 0 .. 255 (the page kit's pg_glyph). */
typedef struct ui_font {
    const uint8_t *glyph;
    unsigned cw, ch;
} ui_font_t;

/* The lines drawn: every call below writes only inside ui_cy0 .. ui_cy1 (a redraw of the rows that changed). */
static int ui_cy0 = 0, ui_cy1 = 479;

static inline void ui_clip(int y0, int y1) { ui_cy0 = y0 < 0 ? 0 : y0; ui_cy1 = y1 > 479 ? 479 : y1; }

static inline void ui_run_(uint16_t *p, int n, uint16_t c) { while (n-- > 0) *p++ = c; }

static inline void ui_rect_(uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t c) {
    int y;

    if (x0 < 0) x0 = 0;
    if (y0 < ui_cy0) y0 = ui_cy0;
    if (x1 > 639) x1 = 639;
    if (y1 > ui_cy1) y1 = ui_cy1;
    for (y = y0; y <= y1 && x0 <= x1; y++) ui_run_(&fb[y * 640 + x0], x1 - x0 + 1, c);
}

/* A slot over x0 .. x1, y0 .. y1 (both ends drawn), clipped. A 9-slice: each line is its left border, one run of one
   colour (the centre column of its 9-slice line) and its right border; no work a pixel in the middle. */
static inline void ui_fill(uint16_t *fb, const ui_slot_t *s, int x0, int y0, int x1, int y1) {
    const int w = x1 - x0 + 1, h = y1 - y0 + 1;
    int y, i;

    if (w <= 0 || h <= 0) return;
    if (!s->pix || w < s->l + s->r + 1 || h < s->t + s->b + 1 || s->w != s->l + s->r + 1 || s->h != s->t + s->b + 1) {
        ui_rect_(fb, x0, y0, x1, y1, s->flat);
        return;
    }
    for (y = y0 < ui_cy0 ? ui_cy0 : y0; y <= y1 && y <= ui_cy1; y++) {
        const int o = y - y0, sy = o < s->t ? o : o >= h - s->b ? s->h - (h - o) : s->t;
        const uint16_t *src = &s->pix[sy * s->w];
        uint16_t *row = &fb[y * 640];
        int a = x0 + s->l, b = x1 - s->r;

        for (i = 0; i < s->l; i++) if (x0 + i >= 0 && x0 + i <= 639 && src[i] != UI_CLEAR) row[x0 + i] = src[i];
        for (i = 0; i < s->r; i++) if (b + 1 + i >= 0 && b + 1 + i <= 639 && src[s->l + 1 + i] != UI_CLEAR) row[b + 1 + i] = src[s->l + 1 + i];
        if (a < 0) a = 0;
        if (b > 639) b = 639;
        if (a <= b && src[s->l] != UI_CLEAR) ui_run_(&row[a], b - a + 1, src[s->l]);
    }
}

/* Text at (x, y), `scale` times the glyph's size, at most n characters. A glyph pixel of full cover is the colour; a
   part-covered one is mixed with what is under it in shifts (no divide: the SH-4 has none in hardware). */
static inline void ui_text(uint16_t *fb, const ui_font_t *f, unsigned scale, int x, int y, uint16_t col, const char *s, unsigned n) {
    const int fr = col >> 11, fg = (col >> 5) & 63, fb_ = col & 31;
    const int w = (int)(f->cw * scale), h = (int)(f->ch * scale);
    int i, j, j0, j1;

    if (y < 0 || y + h > 480 || y + h - 1 < ui_cy0 || y > ui_cy1) return;
    j0 = ui_cy0 > y ? ui_cy0 - y : 0;
    j1 = ui_cy1 < y + h - 1 ? ui_cy1 - y : h - 1;
    for (; *s && n && x >= 0 && x + w <= 640; s++, n--, x += w) {
        const unsigned c = (unsigned char)*s;
        const uint8_t *gl;

        if (c == ' ') continue;
        gl = f->glyph + (c >= 32u && c < 127u ? c - 32u : 0u) * f->cw * f->ch;
        for (j = j0; j <= j1; j++) {
            const uint8_t *g = gl + (scale == 1u ? (unsigned)j : (unsigned)j / scale) * f->cw;
            uint16_t *p = &fb[(y + j) * 640 + x];

            for (i = 0; i < w; i++) {
                const int a = g[scale == 1u ? (unsigned)i : (unsigned)i / scale];

                if (!a) continue;
                if (a == 255) { p[i] = col; continue; }
                {
                    const int k = a + (a >> 7), br = p[i] >> 11, bg = (p[i] >> 5) & 63, bb = p[i] & 31;

                    p[i] = (uint16_t)(((br + (((fr - br) * k) >> 8)) << 11) | ((bg + (((fg - bg) * k) >> 8)) << 5) | (bb + (((fb_ - bb) * k) >> 8)));
                }
            }
        }
    }
}

/* The lines two screens differ in: from the first row that changed to the last (a big row is two), with the 2 lines
   a panel or a chip reaches past its rows. 0: nothing changed. */
static inline int ui_dirty(const nav_grid_t *a, const nav_grid_t *b, int *y0, int *y1) {
    int r, lo = -1, hi = -1;

    for (r = 0; r < (int)NAV_ROWS; r++)
        if (a->colour[r] != b->colour[r] || a->big[r] != b->big[r] || a->kind[r] != b->kind[r] || strcmp(a->text[r], b->text[r])) {
            if (lo < 0) lo = r;
            hi = r + (a->big[r] || b->big[r]);
        }
    if (lo < 0) return 0;
    *y0 = UI_ROW_Y(lo) - 2;
    *y1 = UI_ROW_Y(hi + 1) + 1;
    if (*y0 < 0) *y0 = 0;
    if (*y1 > 479) *y1 = 479;
    return 1;
}

static inline int ui_is_more_(unsigned kind) { return kind == NAV_K_ITEM_MORE || kind == NAV_K_PICK_MORE || kind == NAV_K_FLAG; }

/* The chip's slot for a key of the legend: the face button's own colour, grey for the pad and START alone. */
static inline unsigned ui_chip_slot(unsigned buttons) {
    return buttons & NAV_A ? UI_CHIP_A : buttons & NAV_B ? UI_CHIP_B : buttons & NAV_Y ? UI_CHIP_Y : UI_CHIP_PAD;
}

/* A screen with no rows yet: the background and the two bands. suite_menu.c's loading and RUN ALL screens use it too. */
static inline void ui_frame(uint16_t *fb, const ui_slot_t *skin) {
    ui_rect_(fb, 0, 0, 639, 479, skin[UI_BG].flat);
    ui_fill(fb, &skin[UI_BAND], -8, -8, 647, UI_HEAD_Y1);
    ui_rect_(fb, 0, UI_HEAD_Y1 - 1, 639, UI_HEAD_Y1, skin[UI_RULE].flat);
    ui_fill(fb, &skin[UI_BAND], -8, UI_FOOT_Y0, 647, 487);
    ui_rect_(fb, 0, UI_FOOT_Y0, 639, UI_FOOT_Y0 + 1, skin[UI_RULE].flat);
}

/* The panel of the option on row r: down to the last row that belongs to it. Its last row is returned in *last. */
static inline void ui_panel_rows_(const nav_grid_t *g, unsigned r, unsigned *last, int *y1) {
    unsigned e = r + (g->big[r] ? 1u : 0u);

    while (e + 1u < NAV_ROWS && ui_is_more_(g->kind[e + 1u])) e++;
    *last = e;
    *y1 = UI_ROW_Y(e + 1u) - 3;              /* a line of the background between two panels */
}

/* A TOP or LIST screen (suite_nav.h's nav_grid) into fb. */
static inline void ui_screen(uint16_t *fb, const ui_slot_t *skin, const ui_font_t *f, const nav_grid_t *g, const nav_t *v) {
    nav_span_t span[8];
    unsigned r, i, n;

    ui_frame(fb, skin);
    for (r = 0; r < NAV_ROWS; r++) {        /* the panels */
        const unsigned k = g->kind[r];
        unsigned last;
        int y1;

        if (k != NAV_K_ITEM && k != NAV_K_PICK) continue;
        ui_panel_rows_(g, r, &last, &y1);
        ui_fill(fb, &skin[k == NAV_K_PICK ? UI_ROW_PICK : UI_ROW], UI_PANEL_X0, UI_ROW_Y(r) - 1, UI_PANEL_X1, y1);
        if (k == NAV_K_PICK) ui_rect_(fb, UI_PANEL_X0 - 6, UI_ROW_Y(r) - 1, UI_PANEL_X0 - 3, y1, skin[UI_ROW_MARK].flat);
        for (i = r + 1u; i <= last; i++)
            if (g->kind[i] == NAV_K_FLAG) {
                const char *t = g->text[i], *s = t;

                while (*s == ' ') s++;
                ui_fill(fb, &skin[UI_FLAG], UI_X0 + UI_CW * (int)(s - t) - 4, UI_ROW_Y(i) - 1, UI_X0 + UI_CW * (int)(s - t + 9) + 3, UI_ROW_Y(i) + UI_CH - 3);
            }
    }
    n = nav_legend_spans(span, 8u, v->screen, v->has_top, NAV_COLS);
    for (i = 0; i < n; i++)                 /* the chips */
        ui_fill(fb, &skin[ui_chip_slot(span[i].buttons)], UI_X0 + UI_CW * span[i].col - 4, UI_ROW_Y(NAV_KEYS_ROW) - 1,
                UI_X0 + UI_CW * (span[i].col + span[i].len) + 3, UI_ROW_Y(NAV_KEYS_ROW) + UI_CH);
    for (r = 0; r < NAV_ROWS; r++) {        /* the text */
        const unsigned k = g->kind[r], kc = g->colour[r] < 3u ? g->colour[r] : 0u;
        const int on_pick = k == NAV_K_PICK || k == NAV_K_PICK_MORE || k == NAV_K_FLAG;
        uint16_t col = ui_txt[kc == NAV_GREY && on_pick ? UI_TXT_GREY_PICK : kc];

        if (!g->text[r][0]) continue;
        if (k == NAV_K_INTRO) {
            ui_text(fb, f, 1u, UI_X0, UI_ROW_Y(r), ui_txt[UI_TXT_LABEL], g->text[r], NAV_INTRO_LABEL);
            if (strlen(g->text[r]) > NAV_INTRO_LABEL)
                ui_text(fb, f, 1u, UI_X0 + UI_CW * (int)NAV_INTRO_LABEL, UI_ROW_Y(r), col, g->text[r] + NAV_INTRO_LABEL, NAV_COLS);
            continue;
        }
        if (k == NAV_K_FLAG) col = ui_txt[UI_TXT_WHITE];
        ui_text(fb, f, g->big[r] ? 2u : 1u, UI_X0, UI_ROW_Y(r), col, g->text[r], NAV_COLS);
    }
}

#endif /* SUITE_UI_H */
