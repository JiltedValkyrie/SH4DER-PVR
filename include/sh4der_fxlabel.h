/* SH4DER-PVR: the effects on screen by their usual names, and how each is done: the label line

   include/sh4der_fxlabel.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   RECOMMENDED (console: VID-016 in wiki/hardware/ledger-video.md; wiki/effects/fxlabel.md)
   Cost:       none at run time: a table is formatted when the page or the view changes its mode. RAM: a text of 3 x 65 B, 16 spans
               of 3 B and the item list, about 0.3 KB. No VRAM. A live view's two lines are drawn by the program (512 x 16 blended
               pixels of fill: OFF in the measured stages).
   Start with: one table entry a line of each effect the program uses; the result page's label line is the same table.
   Status:     MEASURED on the console (VID-016): the labels on every page of eight programs on three discs, off in every measured
               stage, clear of the graph titles. Host: the rules and every program's table (test/fxlabel_test.c, demo/psm/test/fx_tables_test.c).
   Do:         say what is true, by the rules below: who and when are read from the code.
   Do not:     shorten a name to fit. A table that does not fit says so (fits = 0) and the program changes the table.
   Maturity:   ALPHA v0.4 (Scholar) / v0.5 (Parity), as in wiki/status/maturity.md

   The names are the label kit's own (fx_*, FX_*). demo/psm/psm_effects.h is a forward at the old path.

   Every demo screen (a live view, a result page) says in small text which effects are in use, by the name the field
   knows them by, and how each is done here:

     Ambient Occlusion  SH-4  Baked | Projected Shadow  SH-4/PVR  Realtime<<

   A program keeps one table of fx_entry_t for each of its modes and gives the table of the mode on screen to
   fx_format (one picture), fx_format_split (a screen cut in a left and a right half) or fx_page (a result page).
   The result is at most FX_LINES lines of FX_COLS characters: the page kit's small font (8 x 16) across the
   title-safe area. This header draws nothing on the PVR: a live view puts the lines through the text path it has.

   THE RULES OF A LABEL. A label must be true: who and when are read from the code, not guessed.
     name   The field's usual name, not a module's name.
              "Shadow Mapping" ONLY where a depth is compared against a map. Casters drawn into a render target and
              laid on the ground with no compare are a "Projected Shadow".
              "Bloom" ONLY for a bright map taken from the finished frame. A glow mesh round a light is a
              "Soft Halo" or "Glow".
     detail Optional (NULL): which one, when a program has two of a name ("Lightmap", "movers").
     who    Who does the work each frame: FX_SH4, FX_PVR, FX_BOTH. The PVR counts only when it does work that
            belongs to the effect: a render target pass, a texture that carries the effect, an alpha test. Drawing
            what the SH-4 computed (a vertex colour, an offset colour, an additive blend) is the SH-4's effect.
     when   When the value was computed. FX_BAKED: before the frame loop (at build or at load), kept a vertex.
            FX_TEXTURE: before the frame loop, kept in a texture the PVR samples. FX_REALTIME: every frame; it is
            marked "<<" in the text and has a span (fx_text_t.span) for a drawer that has a colour for it.
     FX_NONE as who: the name alone is printed ("No effect").

   THE TEXT
     one entry of a name            Name  WHO  When                (one space each when two do not fit)
                                    Name (detail)  WHO  When
     several of a name, or a split  Name: [L|R|L+R ][detail ]WHO When, [..] ...
     between names                   |
     too many for that              N1, N2, N3: WHO When           (the names that share who and when, in one group;
                                    fx_format_merged, and a result page, fall back to it; it has no L / R)
   A line breaks between names or after a comma, never inside an entry. Nothing is shortened: a table that does not
   fit gives fits = 0 (the text holds what did fit), and test/fxlabel_test.c and demo/psm/test/fx_tables_test.c run every table so that this is
   found at build time.

   WHERE IT SHOWS
     result page   fx_page after pg_draw: the row under the title (free in the page kit). A table of two lines needs
                   the program to define PG_TOP 1 before sh4der_page.h (the text then starts a row lower), one of three
                   lines PG_TOP 2 (a live view never has more than two). Details are
                   left out (a page covers the whole run: every mode's entries, merged by name). The line is not in
                   the page's CRC.
     live view     two lines, following the mode. OFF during measured autorun stages (a line of 512 x 16 blended
                   pixels is fill; the numbers stay comparable with runs before the labels), and a page line says so.
     hiding        each program names its own free chord in its controls; fx_hide_chord is the mechanism. Result
                   pages always show the line.

   Plain C, no KallistiOS, no maths. fx_page needs sh4der_page.h included first. */

#ifndef SH4DER_FXLABEL_H
#define SH4DER_FXLABEL_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define FX_SH4  0
#define FX_PVR  1
#define FX_BOTH 2
#define FX_NONE 255

#define FX_BAKED    0
#define FX_TEXTURE  1
#define FX_REALTIME 2

#define FX_COLS 64u
#define FX_LINES 2u                         /* a live view's lines */
#define FX_LINES_MAX 3u                     /* a result page's, with PG_TOP 2 */
#define FX_MAX 16u                          /* entries a table (both halves of a split together) */
#define FX_MARK "<<"

typedef struct fx_entry {
    const char *name;
    uint8_t who, when;
    const char *detail;
} fx_entry_t;

typedef struct fx_span { uint8_t line, c0, c1; } fx_span_t;   /* columns c0 .. c1 - 1 of a line: a Realtime entry's "Realtime<<" */

typedef struct fx_text {
    char line[FX_LINES_MAX][FX_COLS + 1u];
    fx_span_t span[FX_MAX];
    unsigned lines, spans;
    unsigned placed, total;                 /* entries in the text, and entries there are */
    int fits;                               /* 1: all of them */
} fx_text_t;

#define FX_L 1u
#define FX_R 2u

typedef struct fx_item_ { const char *name, *detail; uint8_t who, when, side, used; } fx_item_t_;

static inline const char *fx_who_text(unsigned who) { return who == FX_SH4 ? "SH-4" : who == FX_PVR ? "PVR" : who == FX_BOTH ? "SH-4/PVR" : ""; }
static inline const char *fx_when_text(unsigned when) { return when == FX_BAKED ? "Baked" : when == FX_TEXTURE ? "Texture" : when == FX_REALTIME ? "Realtime" FX_MARK : ""; }

static inline int fx_same_(const char *a, const char *b) { return (!a && !b) || (a && b && !strcmp(a, b)); }

/* Add an entry to the items: one that is there already (name, detail, who, when) only gains the side. */
static inline unsigned fx_add_(fx_item_t_ *it, unsigned n, const fx_entry_t *e, unsigned side, int details) {
    const char *detail = details ? e->detail : NULL;
    unsigned i;

    if (!e->name) return n;
    for (i = 0; i < n; i++)
        if (!strcmp(it[i].name, e->name) && fx_same_(it[i].detail, detail) && it[i].who == e->who && it[i].when == e->when) { it[i].side |= (uint8_t)side; return n; }
    if (n >= FX_MAX) return n;
    it[n].name = e->name; it[n].detail = detail; it[n].who = e->who; it[n].when = e->when; it[n].side = (uint8_t)side; it[n].used = 0;
    return n + 1u;
}

/* An atom onto the text: after `sep` on the line it is on, or on the next line (a comma stays behind, a bar does
   not). 0 when there is no line left for it. */
static inline int fx_place_(fx_text_t *t, unsigned *line, unsigned maxl, const char *atom, int first, int realtime) {
    const unsigned alen = (unsigned)strlen(atom);
    unsigned len = (unsigned)strlen(t->line[*line]);
    const char *sep = !len ? "" : first ? " | " : ", ";

    if (len + strlen(sep) + alen > FX_COLS) {
        if (!first && len < FX_COLS) strcat(t->line[*line], ",");
        if (*line + 1u >= maxl || alen > FX_COLS) return 0;
        (*line)++; t->lines = *line + 1u; len = 0; sep = "";
    }
    strcat(t->line[*line], sep); strcat(t->line[*line], atom);
    if (realtime && t->spans < FX_MAX) {
        const unsigned end = len + (unsigned)strlen(sep) + alen, wl = (unsigned)strlen(fx_when_text(FX_REALTIME));

        t->span[t->spans].line = (uint8_t)*line; t->span[t->spans].c0 = (uint8_t)(end - wl); t->span[t->spans].c1 = (uint8_t)end;
        t->spans++;
    }
    return 1;
}

/* The layout merged by how: the names that share who and when as "N1, N2: WHO When". Details and sides are not in it. */
static inline void fx_layout_how_(fx_text_t *t, fx_item_t_ *it, unsigned n, unsigned maxl) {
    unsigned i, j, line = 0;

    memset(t, 0, sizeof *t);
    t->total = n; t->lines = n ? 1u : 0u;
    for (i = 0; i < n; i++) it[i].used = 0;
    for (i = 0; i < n; i++) {
        unsigned group = 0, seen = 0;

        if (it[i].used) continue;
        for (j = i; j < n; j++) if (it[j].who == it[i].who && it[j].when == it[i].when && it[i].who != FX_NONE) group++;
        if (it[i].who == FX_NONE) group = 1u;
        for (j = i; j < n; j++) {
            char atom[160];
            const int last = seen + 1u == group;

            if (j != i && (it[i].who == FX_NONE || it[j].who != it[i].who || it[j].when != it[i].when)) continue;
            it[j].used = 1;
            if (it[j].who == FX_NONE || !last) snprintf(atom, sizeof atom, "%s", it[j].name);
            else snprintf(atom, sizeof atom, "%s%s%s %s", it[j].name, group > 1u ? ": " : " ", fx_who_text(it[j].who), fx_when_text(it[j].when));
            if (!fx_place_(t, &line, maxl, atom, seen == 0u, last && it[j].who != FX_NONE && it[j].when == FX_REALTIME)) return;
            t->placed++; seen++;
        }
    }
    t->fits = 1;
}

/* One layout of the items; `wide`: two spaces inside a lone entry. */
static inline void fx_layout_(fx_text_t *t, fx_item_t_ *it, unsigned n, int wide, unsigned maxl) {
    const char *gap = wide ? "  " : " ";
    unsigned i, j, line = 0;

    memset(t, 0, sizeof *t);
    t->total = n; t->lines = n ? 1u : 0u;
    for (i = 0; i < n; i++) it[i].used = 0;
    for (i = 0; i < n; i++) {
        unsigned group = 0, first = 1;

        if (it[i].used) continue;
        for (j = i; j < n; j++) if (!strcmp(it[j].name, it[i].name)) group++;
        for (j = i; j < n; j++) {
            char atom[160];
            const char *when = fx_when_text(it[j].when), *who = fx_who_text(it[j].who);
            const char *side = it[j].side == FX_L ? "L" : it[j].side == FX_R ? "R" : it[j].side == (FX_L | FX_R) ? "L+R" : "";
            unsigned k = 0;

            if (strcmp(it[j].name, it[i].name)) continue;
            it[j].used = 1;
            if (group == 1u && !side[0]) {  /* a lone entry */
                k += (unsigned)snprintf(atom + k, sizeof atom - k, "%s", it[j].name);
                if (it[j].detail) k += (unsigned)snprintf(atom + k, sizeof atom - k, " (%s)", it[j].detail);
                if (it[j].who != FX_NONE) k += (unsigned)snprintf(atom + k, sizeof atom - k, "%s%s%s%s", gap, who, gap, when);
            } else {
                if (first) k += (unsigned)snprintf(atom + k, sizeof atom - k, "%s: ", it[j].name);
                if (side[0]) k += (unsigned)snprintf(atom + k, sizeof atom - k, "%s ", side);
                if (it[j].detail) k += (unsigned)snprintf(atom + k, sizeof atom - k, "%s ", it[j].detail);
                if (it[j].who != FX_NONE) k += (unsigned)snprintf(atom + k, sizeof atom - k, "%s %s", who, when);
                while (k && atom[k - 1u] == ' ') atom[--k] = 0;
            }
            if (!fx_place_(t, &line, maxl, atom, (int)first, it[j].when == FX_REALTIME && it[j].who != FX_NONE)) return;
            t->placed++; first = 0;
        }
    }
    t->fits = 1;
}

static inline int fx_items_(fx_text_t *t, fx_item_t_ *it, unsigned n, unsigned maxl, int merge) {
    fx_layout_(t, it, n, 1, maxl);
    if (!t->fits) fx_layout_(t, it, n, 0, maxl);
    if (!t->fits && merge) {                /* the last way: by how. When that does not fit either, the plain text is what stays */
        fx_text_t m;

        fx_layout_how_(&m, it, n, maxl);
        if (m.fits) *t = m;
    }
    return t->fits;
}

/* The text of one table of n entries. 1 when all of it fits. */
static inline int fx_format(fx_text_t *t, const fx_entry_t *e, unsigned n) {
    fx_item_t_ it[FX_MAX];
    unsigned i, m = 0;

    for (i = 0; i < n; i++) m = fx_add_(it, m, &e[i], 0u, 1);
    return fx_items_(t, it, m, FX_LINES, 0);
}

/* As fx_format, and when the table does not fit that way, merged by how (the details are then left out). */
static inline int fx_format_merged(fx_text_t *t, const fx_entry_t *e, unsigned n) {
    fx_item_t_ it[FX_MAX];
    unsigned i, m = 0;

    for (i = 0; i < n; i++) m = fx_add_(it, m, &e[i], 0u, 1);
    return fx_items_(t, it, m, FX_LINES, 1);
}

/* The text of a screen cut in two: the left half's table and the right half's. An entry both have is "L+R"; a half
   with no entry is "No effect". 1 when all of it fits. */
static inline int fx_format_split(fx_text_t *t, const fx_entry_t *left, unsigned nl, const fx_entry_t *right, unsigned nr) {
    static const fx_entry_t none = { "No effect", FX_NONE, 0, NULL };
    fx_item_t_ it[FX_MAX];
    unsigned i, m = 0;

    for (i = 0; i < nl; i++) m = fx_add_(it, m, &left[i], FX_L, 1);
    for (i = 0; i < nr; i++) m = fx_add_(it, m, &right[i], FX_R, 1);
    if (!nl) m = fx_add_(it, m, &none, FX_L, 1);
    if (!nr) m = fx_add_(it, m, &none, FX_R, 1);
    return fx_items_(t, it, m, FX_LINES, 0);
}

/* The text for a result page of up to maxl lines: the program's whole table, the details left out, the same who and
   when of a name once; merged by how when it does not fit otherwise. */
static inline int fx_format_page_n(fx_text_t *t, const fx_entry_t *e, unsigned n, unsigned maxl) {
    fx_item_t_ it[FX_MAX];
    unsigned i, m = 0;

    for (i = 0; i < n; i++) m = fx_add_(it, m, &e[i], 0u, 0);
    return fx_items_(t, it, m, maxl > FX_LINES_MAX ? FX_LINES_MAX : maxl, 1);
}
static inline int fx_format_page(fx_text_t *t, const fx_entry_t *e, unsigned n) { return fx_format_page_n(t, e, n, FX_LINES); }

/* 1 when column c of a line is inside a Realtime span. */
static inline int fx_marked(const fx_text_t *t, unsigned line, unsigned c) {
    unsigned i;

    for (i = 0; i < t->spans; i++) if (t->span[i].line == line && c >= t->span[i].c0 && c < t->span[i].c1) return 1;
    return 0;
}

/* The hide chord's mechanism: `held` the program's own modifier is down, `hit` its own button went down this frame.
   Flips *hidden and gives 1 when both: the program then skips what that button does alone. */
static inline int fx_hide_chord(int held, int hit, int *hidden) {
    if (!held || !hit) return 0;
    *hidden = !*hidden;
    return 1;
}

#ifdef SH4DER_PAGE_H
#ifndef PG_TOP
#define PG_TOP 0u
#endif
#define FX_YELLOW PG_RGB(255, 220, 0)

/* The label line of a result page, under the title; call after pg_draw. A second line shows only with PG_TOP 1, a
   third with PG_TOP 2. Returns 0 when the table does not fit the rows the page kept. */
static inline int fx_page(const fx_entry_t *e, unsigned n) {
    fx_text_t t;
    unsigned l, c;
    const int ok = fx_format_page_n(&t, e, n, 1u + PG_TOP);

    for (l = 0; l < t.lines && l < 1u + PG_TOP; l++)
        for (c = 0; t.line[l][c]; c++) {
            const char one[2] = { t.line[l][c], 0 };

            pg_text(PG_X0 + c * PG_CW, PG_Y0 + PG_CH * (1u + l), fx_marked(&t, l, c) ? FX_YELLOW : PG_GREY, one);
        }
    return ok;
}
#endif

#endif /* SH4DER_FXLABEL_H */
