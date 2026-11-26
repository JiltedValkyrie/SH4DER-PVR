/* SH4DER-PVR: result pages for the console test programs: small text and graphs

   include/sh4der_page.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   RECOMMENDED (console: VID-015 and VID-016 in wiki/hardware/ledger-video.md; wiki/effects/page.md)
   Cost:       drawn once a page, after the PVR is shut down: no PVR state and no frame cost. RAM by the layout (not measured on
               the console): the glyphs 12 KB (96 x 16 x 8 B), the text lines 58 KB (900 x 65 B), 16 graph records of about 2.5 KB.
   Start with: the defaults (PG_BIG 0, PG_TOP 0, PG_LINES_MAX 900); a page of 64 x 22 characters in the title-safe area.
   Status:     MEASURED, one run each on one console (VID-015: CRC line; VID-016: 21 text pages and 4 graphs agree with their CRCs).
   Do:         put every number the ledger is written from on a text page, and type a photographed page into tools/page_crc.py.
   Do not:     draw a live view with it (it draws into the frame buffer after the PVR is shut down); take a graph's values as a ledger row.
   Maturity:   ALPHA v0.5
   Needs:      <kos.h> first (bfont_draw, vram_s, vid_clear). On the host, test/page_test.c stands in for those three names.

   The names are the page kit's own (pg_*, PG_*): a client includes this header. demo/psm/psm_page.h is a forward at the old path.

   The test programs print their numbers on pages that are photographed off a television. This file is those pages,
   shared: more text a page than the BIOS font gives, and graphs for the shape of a result beside its numbers.

   TEXT. The BIOS font is 12 x 24 pixels (KallistiOS's bfont): 50 characters by 17 lines inside a safe margin. Here each
   glyph is drawn once into RAM and reduced to 8 x 16 by a 3-to-2 box filter (an output pixel is the weighted cover of
   the 1.5 x 1.5 source pixels under it, 10 grey levels, brightened so a stroke one pixel wide stays bright). A page is
   PG_COLS x PG_ROWS = 64 x 22 characters under a header line, all inside the title-safe area of a 640 x 480 picture
   (64 pixels in from the sides, 48 from the top and bottom: x 64 .. 575, y 48 .. 431; the older pages started at
   x 20, y 16, outside it): 1.66 times the old page of 50 x 17. PG_BIG 1 before the include goes back to the BIOS
   font's own size (42 x 14 in the same area) if the small text does not photograph. PG_TOP 1 before the include
   keeps one more row free under the header line (a page is then 21 rows): sh4der_fxlabel.h's label line, when it is two.
   CRC. The header of every page ends with "CRC xxxxxxxx": the CRC-32 (the zip one) of the page's content, so a
   page typed from a photograph can be checked (tools/page_crc.py). A text page's content is each of its lines as
   shown, in order, each followed by one line feed, up to its last line that is not empty. A graph page's content is
   its title and a line feed, then for each series its name, a line feed, and each point's y value rounded to the
   nearest whole number, as decimal text followed by a space.
   A line longer than the page goes on, indented, on the next line; every line also goes to the serial line with the
   program's prefix.

   GRAPHS. A graph is a page of its own after the text pages: a title, two labelled axes with numbered ticks, up to
   PG_SERIES series as lines with point marks or as grouped bars, a legend. The axes start at 0 unless a series goes
   below it; the top is rounded up to 1, 2 or 5 times a power of ten. A graph shows shape; the exact values stay on
   the text pages, which are what the ledger is written from.

   Drawing is by the SH-4 into the frame buffer (vram_s, 640 x 480, RGB565) after the PVR is shut down: no PVR state.
   Use: #define PG_PREFIX "c4" (the serial line's prefix), include this after <kos.h>, call pg_init() once after the
   video mode is 640 x 480 RGB565, pg_say() for lines, pg_plot() .. for graphs, pg_draw(page, title, keys) to show a
   page; pg_pages() is the number of pages.
*/

#ifndef SH4DER_PAGE_H
#define SH4DER_PAGE_H

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef PG_PREFIX
#define PG_PREFIX "pg"
#endif
#ifndef PG_BIG
#define PG_BIG 0
#endif
#ifndef PG_LINES_MAX
#define PG_LINES_MAX 900u
#endif
#ifndef PG_TOP
#define PG_TOP 0u                             /* rows kept free under the header line: sh4der_fxlabel.h's second label line (1) */
#endif

#if PG_BIG
#define PG_CW 12u
#define PG_CH 24u
#define PG_COLS 42u
#define PG_ROWS (14u - PG_TOP)
#else
#define PG_CW 8u
#define PG_CH 16u
#define PG_COLS 64u
#define PG_ROWS (22u - PG_TOP)
#endif
#define PG_X0 64u                           /* the title-safe area: 64 in from the sides, 48 from the top and bottom */
#define PG_Y0 48u

#define PG_RGB(r, g, b) ((uint16_t)((((r) >> 3) << 11) | (((g) >> 2) << 5) | ((b) >> 3)))
#define PG_WHITE PG_RGB(255, 255, 255)
#define PG_GREY PG_RGB(150, 150, 150)
#define PG_DIM PG_RGB(70, 70, 70)

/* ---- text ------------------------------------------------------------------------------ */

static char pg_lines[PG_LINES_MAX][PG_COLS + 1u];
static unsigned pg_nlines;
static uint8_t pg_glyph[96][PG_CH][PG_CW];  /* cover 0 .. 255 of each pixel of characters 32 .. 127 */
static int pg_ready;

/* The glyphs, once. bfont draws 12 x 24 into a buffer; the small size is its 3-to-2 reduction. */
static void pg_init(void) {
    static uint16_t src[24u * 12u];
    unsigned c, x, y;

    for (c = 32u; c < 127u; c++) {
        memset(src, 0, sizeof src);
        bfont_draw(src, 12u, true, c);
        for (y = 0; y < PG_CH; y++) {
            for (x = 0; x < PG_CW; x++) {
#if PG_BIG
                pg_glyph[c - 32u][y][x] = src[y * 12u + x] ? 255u : 0u;
#else
                /* output pixel (x, y) lies over source 1.5 x .. 1.5 x + 1.5: two source pixels, weights 2 and 1 (or
                   1 and 2) of 3, on each axis */
                const unsigned sx = (x / 2u) * 3u + (x & 1u), sy = (y / 2u) * 3u + (y & 1u);
                const unsigned wx0 = (x & 1u) ? 1u : 2u, wy0 = (y & 1u) ? 1u : 2u;
                unsigned v = 0;

                v += wy0 * wx0 * (src[sy * 12u + sx] != 0u);
                v += wy0 * (3u - wx0) * (src[sy * 12u + sx + 1u] != 0u);
                v += (3u - wy0) * wx0 * (src[(sy + 1u) * 12u + sx] != 0u);
                v += (3u - wy0) * (3u - wx0) * (src[(sy + 1u) * 12u + sx + 1u] != 0u);
                v *= 57u;                   /* 9 is full cover; twice as bright, so a thin stroke is not grey */
                pg_glyph[c - 32u][y][x] = (uint8_t)(v > 255u ? 255u : v);
#endif
            }
        }
    }
    pg_ready = 1;
}

/* A string at pixel (x, y), in a colour, over what is there (black is assumed under the text). */
static void pg_text(unsigned x, unsigned y, uint16_t col, const char *s) {
    const unsigned r = col >> 11, g = (col >> 5) & 63u, b = col & 31u;
    unsigned i, j;

    if (!pg_ready) pg_init();
    for (; *s && x + PG_CW <= 640u; s++, x += PG_CW) {
        const unsigned c = (unsigned char)*s;
        const uint8_t (*gl)[PG_CW] = pg_glyph[c >= 32u && c < 127u ? c - 32u : 0u];

        if (y + PG_CH > 480u) return;
        for (j = 0; j < PG_CH; j++)
            for (i = 0; i < PG_CW; i++) {
                const unsigned a = gl[j][i];

                if (a) vram_s[(y + j) * 640u + x + i] = (uint16_t)(((r * a / 255u) << 11) | ((g * a / 255u) << 5) | (b * a / 255u));
            }
    }
}

/* A line for the pages and the serial line. A line over the page's width goes on, indented, on the next; it is cut
   at a space when there is one in the last half of the line, so a number is not split (the space goes with the cut). */
static void pg_say(const char *fmt, ...) {
    char big[320];
    const char *s = big;
    size_t left;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(big, sizeof big, fmt, ap);
    va_end(ap);
    printf(PG_PREFIX ": %s\n", big);
    for (left = strlen(big); pg_nlines < PG_LINES_MAX; ) {
        const int cont = s != big;
        const size_t room = cont ? PG_COLS - 3u : PG_COLS;
        size_t n = left < room ? left : room, skip = 0;

        if (left > room) {
            size_t k = room;

            while (k > room / 2u && s[k] != ' ') k--;
            if (s[k] == ' ') { n = k; skip = 1u; }
        }
        snprintf(pg_lines[pg_nlines++], PG_COLS + 1u, "%s%.*s", cont ? "   " : "", (int)n, s);
        s += n + skip; left -= n + skip;
        if (!left) break;
    }
}

/* The next line starts a new page. */
static inline void pg_break(void) {
    while (pg_nlines % PG_ROWS && pg_nlines < PG_LINES_MAX) pg_lines[pg_nlines++][0] = '\0';
}

/* ---- graphs ---------------------------------------------------------------------------- */

#define PG_PLOTS 16u
#define PG_SERIES 4u
#define PG_POINTS 64u
#define PG_CATS 12u

typedef struct pg_plot {
    char title[PG_COLS + 1u], xlabel[40], ylabel[40];
    char sname[PG_SERIES][28];
    char cat[PG_CATS][12];                  /* bar graphs: the name under each group */
    float x[PG_SERIES][PG_POINTS], y[PG_SERIES][PG_POINTS];
    uint8_t n[PG_SERIES];
    uint8_t nseries, bars, ncat;
} pg_plot_t;

static pg_plot_t pg_plots[PG_PLOTS];
static unsigned pg_nplots;
static pg_plot_t pg_spare;                  /* what a graph too many is written into */

static const uint16_t pg_colour[PG_SERIES] = { PG_RGB(255, 255, 255), PG_RGB(255, 220, 0), PG_RGB(0, 230, 255), PG_RGB(255, 90, 255) };

/* A new graph. bars: 0 lines with point marks (x is a number), 1 grouped bars (x is the group's index, 0 ..). */
static pg_plot_t *pg_plot(const char *title, const char *xlabel, const char *ylabel, int bars) {
    pg_plot_t *p = pg_nplots < PG_PLOTS ? &pg_plots[pg_nplots++] : &pg_spare;

    memset(p, 0, sizeof *p);
    snprintf(p->title, sizeof p->title, "%s", title);
    snprintf(p->xlabel, sizeof p->xlabel, "%s", xlabel);
    snprintf(p->ylabel, sizeof p->ylabel, "%s", ylabel);
    p->bars = (uint8_t)(bars != 0);
    return p;
}

/* A new series of the graph; the points that follow are its. */
static void pg_series(pg_plot_t *p, const char *name) {
    if (p->nseries < PG_SERIES) snprintf(p->sname[p->nseries++], sizeof p->sname[0], "%s", name);
}

static void pg_point(pg_plot_t *p, float x, float y) {
    const unsigned s = p->nseries ? p->nseries - 1u : 0u;

    if (!p->nseries || p->n[s] >= PG_POINTS) return;
    p->x[s][p->n[s]] = x; p->y[s][p->n[s]] = y; p->n[s]++;
}

/* Bar graphs: the name of the next group. */
static inline void pg_cat(pg_plot_t *p, const char *name) {
    if (p->ncat < PG_CATS) snprintf(p->cat[p->ncat++], sizeof p->cat[0], "%s", name);
}

static void pg_rect(int x0, int y0, int x1, int y1, uint16_t c) {
    int x, y;

    if (x0 > x1) { x = x0; x0 = x1; x1 = x; }
    if (y0 > y1) { y = y0; y0 = y1; y1 = y; }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > 639) x1 = 639;
    if (y1 > 479) y1 = 479;
    for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) vram_s[y * 640 + x] = c;
}

/* A line two pixels thick. */
static void pg_line(int x0, int y0, int x1, int y1, uint16_t c) {
    const int dx = x1 > x0 ? x1 - x0 : x0 - x1, dy = y1 > y0 ? y1 - y0 : y0 - y1, n = dx > dy ? dx : dy;
    int i;

    for (i = 0; i <= n; i++) {
        const int x = n ? x0 + (x1 - x0) * i / n : x0, y = n ? y0 + (y1 - y0) * i / n : y0;

        pg_rect(x, y, x + 1, y + 1, c);
    }
}

/* v as text: a whole number when it is one or is large, else one or two decimals. No float printf. */
static void pg_num(char *out, size_t size, float v) {
    const int neg = v < 0.0f;
    const float a = neg ? -v : v;
    const unsigned long whole = (unsigned long)a, c = (unsigned long)((a - (float)whole) * 100.0f + 0.5f);

    if (a >= 100.0f || c == 0u || c >= 100u) snprintf(out, size, "%s%lu", neg ? "-" : "", whole + (c >= 100u));
    else if (c % 10u == 0u) snprintf(out, size, "%s%lu.%lu", neg ? "-" : "", whole, c / 10u);
    else snprintf(out, size, "%s%lu.%02lu", neg ? "-" : "", whole, c);
}

/* The least of 1, 2, 5 times a power of ten that is not under v. */
static float pg_round_up(float v) {
    float p = 1.0f;
    unsigned k;

    if (!(v > 0.0f)) return 1.0f;
    for (k = 0; k < 40u && p > v; k++) p *= 0.1f;
    for (k = 0; k < 40u && p * 10.0f < v; k++) p *= 10.0f;
    return v <= p ? p : v <= 2.0f * p ? 2.0f * p : v <= 5.0f * p ? 5.0f * p : 10.0f * p;
}

#define PG_GX0 120
#define PG_GX1 572
#define PG_GTOP ((int)(PG_TOP * 16u))       /* a graph page keeps the rows under the header free too, as a text page does */
#define PG_GY0 (124 + PG_GTOP)
#define PG_GY1 (332 + PG_GTOP)

static void pg_plot_draw(const pg_plot_t *p) {
    float xmin = 0.0f, xmax = 1.0f, ymin = 0.0f, ymax = 0.0f, ytop;
    unsigned s, i, any = 0, groups = 0;
    char num[24];

    for (s = 0; s < p->nseries; s++) {
        if (p->n[s] > groups) groups = p->n[s];
        for (i = 0; i < p->n[s]; i++) {
            if (!any || p->x[s][i] < xmin) xmin = p->x[s][i];
            if (!any || p->x[s][i] > xmax) xmax = p->x[s][i];
            if (p->y[s][i] > ymax) ymax = p->y[s][i];
            if (p->y[s][i] < ymin) ymin = p->y[s][i];
            any = 1;
        }
    }
    if (xmax <= xmin) xmax = xmin + 1.0f;
    ytop = pg_round_up(ymax);
    if (ymin < 0.0f) ymin = -pg_round_up(-ymin);
    pg_text(PG_X0, PG_Y0 + 32u + (unsigned)PG_GTOP, PG_WHITE, p->title);     /* row 2: row 1 is the effect labels' first line */
    pg_text(PG_X0, (unsigned)(PG_GY0 - 26), PG_GREY, p->ylabel);
    pg_text((unsigned)(PG_GX1 - (int)(strlen(p->xlabel) * PG_CW)), PG_GY1 + 30, PG_GREY, p->xlabel);
    for (i = 0; i <= 5u; i++) {            /* the y axis: six ticks, a faint line across at each */
        const float v = ymin + (ytop - ymin) * (float)i / 5.0f;
        const int y = PG_GY1 - (int)((float)(PG_GY1 - PG_GY0) * (float)i / 5.0f);

        pg_rect(PG_GX0, y, PG_GX1, y, i ? PG_DIM : PG_GREY);
        pg_num(num, sizeof num, v);
        pg_text((unsigned)(PG_GX0 - 8 - (int)(strlen(num) * PG_CW)), (unsigned)(y - 8), PG_GREY, num);
    }
    pg_rect(PG_GX0, PG_GY0, PG_GX0, PG_GY1, PG_GREY);
    if (!any) { pg_text(PG_GX0 + 16, 200u, PG_WHITE, "no data"); return; }
    if (p->bars) {
        const int gw = groups ? (PG_GX1 - PG_GX0) / (int)groups : 1, bw = (gw - 8) / (int)(p->nseries ? p->nseries : 1u);

        for (i = 0; i < groups; i++) {
            if (i < p->ncat) pg_text((unsigned)(PG_GX0 + (int)i * gw + 4), PG_GY1 + 8, PG_GREY, p->cat[i]);
            for (s = 0; s < p->nseries; s++) {
                const int x = PG_GX0 + (int)i * gw + 4 + (int)s * bw;
                const float t = i < p->n[s] ? (p->y[s][i] - ymin) / (ytop - ymin) : 0.0f;

                if (i < p->n[s]) pg_rect(x, PG_GY1 - (int)((float)(PG_GY1 - PG_GY0) * t), x + (bw > 3 ? bw - 2 : 1), PG_GY1, pg_colour[s]);
            }
        }
    } else {
        for (i = 0; i <= 4u; i++) {        /* the x axis: five ticks */
            const float v = xmin + (xmax - xmin) * (float)i / 4.0f;
            const int x = PG_GX0 + (PG_GX1 - PG_GX0) * (int)i / 4;

            pg_rect(x, PG_GY1, x, PG_GY1 + 4, PG_GREY);
            pg_num(num, sizeof num, v);
            {                               /* centred on the tick, and kept inside the safe area */
                const int w = (int)(strlen(num) * PG_CW), tx = x - w / 2;

                pg_text((unsigned)(tx + w > PG_GX1 + 4 ? PG_GX1 + 4 - w : tx), PG_GY1 + 8, PG_GREY, num);
            }
        }
        for (s = 0; s < p->nseries; s++) {
            int px = 0, py = 0;

            for (i = 0; i < p->n[s]; i++) {
                const int x = PG_GX0 + (int)((float)(PG_GX1 - PG_GX0 - 4) * (p->x[s][i] - xmin) / (xmax - xmin));
                const int y = PG_GY1 - (int)((float)(PG_GY1 - PG_GY0) * (p->y[s][i] - ymin) / (ytop - ymin));

                if (i) pg_line(px, py, x, y, pg_colour[s]);
                pg_rect(x - 2, y - 2, x + 3, y + 3, pg_colour[s]);
                px = x; py = y;
            }
        }
    }
    for (s = 0; s < p->nseries; s++) {     /* the legend: two a line under the graph */
        const unsigned lx = PG_X0 + (s & 1u) * 256u, ly = (unsigned)(PG_GY1 + 56) + (s / 2u) * 20u;

        pg_rect((int)lx, (int)ly + 4, (int)lx + 20, (int)ly + 11, pg_colour[s]);
        pg_text(lx + 28u, ly, PG_WHITE, p->sname[s]);
    }
}

/* ---- pages ------------------------------------------------------------------------------ */

static unsigned pg_extra_pages;            /* pages the program draws itself after these (counted in the header's N) */

static inline unsigned pg_text_pages(void) { return (pg_nlines + PG_ROWS - 1u) / PG_ROWS; }

static inline unsigned pg_pages(void) {
    const unsigned n = pg_text_pages() + pg_nplots;

    return n ? n : 1u;
}

/* CRC-32 (reflected, polynomial edb88320, as zip's), carried on: start with 0. */
static uint32_t pg_crc(uint32_t crc, const void *data, size_t n) {
    const uint8_t *p = data;
    unsigned k;

    crc = ~crc;
    while (n--) { crc ^= *p++; for (k = 0; k < 8u; k++) crc = (crc & 1u) ? 0xedb88320u ^ (crc >> 1) : crc >> 1; }
    return ~crc;
}

/* The CRC of a page's content (the file comment says what the content is). */
static uint32_t pg_page_crc(unsigned page) {
    const unsigned tp = pg_text_pages();
    uint32_t crc = 0;
    unsigned i, s, last = 0;
    char num[24];

    if (page < tp) {
        for (i = 0; i < PG_ROWS && page * PG_ROWS + i < pg_nlines; i++) if (pg_lines[page * PG_ROWS + i][0]) last = i + 1u;
        for (i = 0; i < last; i++) { crc = pg_crc(crc, pg_lines[page * PG_ROWS + i], strlen(pg_lines[page * PG_ROWS + i])); crc = pg_crc(crc, "\n", 1u); }
    } else if (page - tp < pg_nplots) {
        const pg_plot_t *p = &pg_plots[page - tp];

        crc = pg_crc(crc, p->title, strlen(p->title)); crc = pg_crc(crc, "\n", 1u);
        for (s = 0; s < p->nseries; s++) {
            crc = pg_crc(crc, p->sname[s], strlen(p->sname[s])); crc = pg_crc(crc, "\n", 1u);
            for (i = 0; i < p->n[s]; i++) {
                snprintf(num, sizeof num, "%ld ", (long)(p->y[s][i] < 0.0f ? p->y[s][i] - 0.5f : p->y[s][i] + 0.5f));
                crc = pg_crc(crc, num, strlen(num));
            }
        }
    }
    return crc;
}

/* Page `page` of pg_pages(): the header (title, page n/N, the keys, the page's CRC), then the text or the graph. */
static void pg_draw(unsigned page, const char *title, const char *keys) {
    const unsigned tp = pg_text_pages();
    char head[192], crc[16];
    unsigned i;

    vid_clear(0, 0, 0);
    snprintf(head, sizeof head, "%.80s p%u/%u%s A/B %.80s", title, page + 1u, pg_pages() + pg_extra_pages, page >= tp && pg_nplots ? " GRAPH" : "", keys);
    head[PG_COLS] = '\0';                   /* a long title or key line is cut at the page's width, on purpose */
    pg_text(PG_X0, PG_Y0, PG_WHITE, head);
    snprintf(crc, sizeof crc, "CRC %08lx", (unsigned long)pg_page_crc(page));
    pg_text(PG_X0 + (PG_COLS - 12u) * PG_CW, PG_Y0, PG_GREY, crc);
    if (page < tp) {
        for (i = 0; i < PG_ROWS && page * PG_ROWS + i < pg_nlines; i++)
            pg_text(PG_X0, PG_Y0 + PG_CH * (i + 2u + PG_TOP), PG_WHITE, pg_lines[page * PG_ROWS + i]);
    } else if (page - tp < pg_nplots) {
        pg_plot_draw(&pg_plots[page - tp]);
    }
}

#endif /* SH4DER_PAGE_H */
