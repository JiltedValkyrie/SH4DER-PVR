/* SH4DER-PVR: a render target pass through a tile list its own size (from demo/psm/ao/tile_cut.h)

   include/sh4der_tilecut.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   RECOMMENDED (from the wiki ledger, one console; wiki/limits/header-recommended.md)
   Start with: one target pass cut to its own tiles (cw x ch), timed cut and uncut; SH4DER_TILECUT_TRIES = 3
   Cost:       PVR: 3,256 -> 344 us (ao4-h), 3,312 -> 375 us (c4), 5,075 -> 1,425 us (d1-d): MEASURED
               (PVR-156, PVR-092, PVR-205). SH-4 a pass: begin 126 us, end 130 us; the frame 1,869 us faster
               (d1-d): MEASURED (PVR-205). The first form's begin cost 3,930 us (d1-c, PVR-192)
   Status:     the PVR saving PROVEN (four runs, three programs; also ra2 at 128 and 256); the frame gain PROVEN
               in d1 alone (one run, held at 60 Hz); ADVISED elsewhere: the street demo lost 3.05 ms with one
               table of two cut (PVR-200), and route A's ra2 stalled from 16.6 to 20.0 ms (MEASURED); the cause —
               two sleeping pvr_wait_render_done ticks — is DERIVED from reading KallistiOS's source (to be tested
               by ra5). Single-table per-table state (sh4der_tilecut_begin_one_at, _end_one, _other, _active) is host
               tested (test/tilecut_test.c) and NOT RUN on a console. The need = 2 refusal has not run on a console
   Do:         wait for render done after every all-tables cut pass (use sh4der_pvrx_wait_render_done_poll to avoid
               the 10 ms sleep tick); cut only with every tile table found (PVR-200); when holding one table cut
               on a pass's own table, restore it before any frame without a pass, exit, or pvr_shutdown
   Do not:     cut a table while a render reads that table; carry on if sh4der_tilecut_end or _end_one returns bad
               (shut the PVR down and pvr_init again)
   Maturity:   ALPHA v0.5

   KallistiOS renders a target through the screen's whole tile table, 20 x 15 tiles whatever the target's size, and
   the PVR's ISP visits every one: about 3.2 ms a pass (wiki PVR-004). This makes the table end after the target's
   own tiles for the length of one pass and puts it back (demo/psm/TILE_CUT_SCOPE.md; PVR-092: 3,312 -> 375 us for a 20-tile
   target, MEASURED once, as a single last stage of the c4 image).

   STATUS: the table logic below is host tested (test/tilecut_test.c, run by make test). The
   per-pass use (rewrite before every target pass, restore after it) ran in ao4-h (PVR-156) and the single stage in
   c4 (PVR-092), one console each, in this file's FIRST form, which read both whole tables from video memory before
   every cut (the layout check), read the entries to save them and read them again to move them: about 5,500 uncached
   reads and 960 writes a pass at 10 x 8 tiles, and 960 writes and 960 reads to put them back. d1-c MEASURED that:
   +3.9 ms of SH-4 for the begin alone, more than the 3.7 ms of PVR it saved (PVR-192). THIS form (v0.5) keeps a copy
   of each table in RAM from the moment it is taken and only WRITES video memory in a cut: 438 words a table at
   10 x 8, as many to restore, two reads a table as a light check, and a read-back of the restore on the first
   SH4DER_TILECUT_VERIFY ends only. d1-d ran it, one console (PVR-205, MEASURED): begin 126 us, end 130 us a pass, the
   map's PVR time 5,075 -> 1,425 us, the frame 1,869 us shorter (it came to rest on the 16.6 ms of the display, so
   the gain may be larger).
   sh4der_tilecut_begin_cols is the cheapest cut: the first cw whole columns of the screen (cw x 15 tiles, more than the
   target needs), ONE word written a table and one to restore. d1-d, the same run (PVR-205, MEASURED): begin 5 us,
   end 3 us, the map's PVR time 5,075 -> 2,756 us, the frame 1,904 us shorter.
   Both ran BEFORE the rule that a cut wants every table found (need); that rule has not run on a console.

   THE TABLE, as KallistiOS builds it at pvr_init (pvr_buffers.c pvr_init_tile_matrix, read at commit 568c3404; the
   lines want a new check against the current master before anything is offered upstream): a first entry of six words
   (10000000, then five 80000000), then six words a tile, COLUMN BY COLUMN (x outer, y inner): a control word with
   (y << 8) | (x << 2), and five list pointers (80000000: no list). Bit 31 of the control word marks the last entry.
   One table a TA buffer: two.

   USE, round a target pass of cw x ch tiles (a 128 x 130 target: 4 x 5), with NO render running that reads the table:
       sh4der_tilecut_find(&cut);                  after a scene's render, until cut.n is 2 (Dreamcast only: reads the PVR's
                                             region base register and checks the table there before taking it)
       if (sh4der_tilecut_begin(&cut, cw, ch)) ..  the tables end after the target's tiles
       ... the target's scene, its render, the wait for render done ...
       bad = sh4der_tilecut_end(&cut);             every word back; read back (three tries) on the first ends; bad: words
                                             still different

   sh4der_tilecut_begin refuses (returns 0, nothing written) when not every table was found yet (c->need of them: two, as
   KallistiOS has; with one of two the passes through the other table ran uncut, every second frame: PVR-200. A
   program that knows it has one table sets need to 1 after sh4der_tilecut_init), a table's first or last entry no longer
   reads as laid out (two words read a table: all it looks at; sh4der_tilecut_check reads everything, for a load or a pause),
   the cut is larger than the screen or than SH4DER_TILECUT_MAX tiles, or a cut is on already. A table that is refused is
   left alone and the pass runs uncut. If sh4der_tilecut_end returns other than 0 the screen's next frame would be a corner
   of the picture: shut the PVR down and start it again (pvr_init builds the tables anew).

   Only the first cw x ch entries are written (and of those not the ones a cut leaves where they are: the first
   column's): what follows the end mark is not read by the PVR. The TA does not read
   this table (only the ISP does), so nothing of a scene being filled is disturbed. Only video memory is written.

   The caller's condition: no rewrite while a render that uses the table is running. A program that waits for render
   done after every pass holds it. One that fills the next scene while the last still renders does not; that wants a
   change inside KallistiOS (a short table a buffer for target passes), which this file is not. */

#ifndef SH4DER_TILECUT_H
#define SH4DER_TILECUT_H

#include <stdint.h>

#define SH4DER_TILECUT_TABLES 2u
#define SH4DER_TILECUT_MAX    96u                 /* the most tiles sh4der_tilecut_begin keeps */
#define SH4DER_TILECUT_TILES  300u                /* the most tiles of a table (20 x 15): the RAM copy's size */
#define SH4DER_TILECUT_TRIES  3u
#define SH4DER_TILECUT_VERIFY 8u                  /* ends that read their restore back; after them the restore is written and trusted */

typedef struct sh4der_tilecut {
    volatile uint32_t *table[SH4DER_TILECUT_TABLES];          /* each table's first word (its first entry) */
    unsigned n;                                         /* tables found */
    unsigned need;                                      /* tables a cut wants found: SH4DER_TILECUT_TABLES from sh4der_tilecut_init */
    unsigned tw, th;                                    /* the table's columns, and the rows a column holds (the screen's tiles) */
    unsigned row0;                                      /* the row number of a column's first entry: 0, or 1 in a table with its first row left out */
    unsigned cw, ch;                                    /* the cut that is on */
    unsigned ry;                                        /* the row number its first row sits at (sh4der_tilecut_begin_at) */
    int on;                                             /* 0 none, 1 begin_at / begin_one_at, 2 begin_cols / begin_one_cols (any table cut) */
    int tbl_on[SH4DER_TILECUT_TABLES];                  /* per table: 0 none, 1 begin_at / begin_one_at, 2 begin_cols / begin_one_cols */
    unsigned tbl_cw[SH4DER_TILECUT_TABLES];             /* per table: cut width, tiles */
    unsigned tbl_ch[SH4DER_TILECUT_TABLES];             /* per table: cut height, tiles */
    unsigned tbl_ry[SH4DER_TILECUT_TABLES];             /* per table: first row of the cut */
    unsigned verify;                                    /* ends still to be read back */
    uint32_t copy[SH4DER_TILECUT_TABLES][SH4DER_TILECUT_TILES * 6u];   /* each table's entries as they were when it was taken (RAM) */
} sh4der_tilecut_t;

static inline void sh4der_tilecut_clear_(sh4der_tilecut_t *c) {
    unsigned k;

    c->n = 0; c->cw = c->ch = 0; c->ry = 0; c->on = 0;
    c->verify = SH4DER_TILECUT_VERIFY;
    for (k = 0; k < SH4DER_TILECUT_TABLES; k++) {
        c->table[k] = 0;
        c->tbl_on[k] = 0;
        c->tbl_cw[k] = c->tbl_ch[k] = c->tbl_ry[k] = 0;
    }
}

static inline void sh4der_tilecut_init(sh4der_tilecut_t *c, unsigned tw, unsigned th) {
    sh4der_tilecut_clear_(c);
    c->need = SH4DER_TILECUT_TABLES; c->tw = tw; c->th = th; c->row0 = 0;
}

/* The same for a table a program has compacted, with rows left out at the top (and the bottom): th rows a column, the first one
   the row number row0 (a port that keeps rows 1 to 13 of 15: th 13, row0 1). Call it right after sh4der_tilecut_init, before
   a table is taken. The control words keep their own row numbers (nothing is renumbered), so a tile's place in the table is
   x * th + (row - row0). */
static inline void sh4der_tilecut_init_rows(sh4der_tilecut_t *c, unsigned tw, unsigned th, unsigned row0) {
    sh4der_tilecut_init(c, tw, th);
    c->row0 = row0;
}

/* Forget every table taken and any cut that was on, and keep the layout (tw, th, row0, need). For a program whose tile tables
   are rebuilt (after pvr_shutdown and pvr_init, a video mode change: the tables may move and a compacting program rewrites
   them): call it once the PVR is up again, then sh4der_tilecut_find after the next render, until n is need again. A cut that
   was on is dropped without being written back: the table it cut no longer exists. */
static inline void sh4der_tilecut_reset(sh4der_tilecut_t *c) {
    sh4der_tilecut_clear_(c);
}

/* 1 when the table reads as laid out above for tw x th tiles, its rows numbered from row0. */
static inline int sh4der_tilecut_valid_rows(const volatile uint32_t *t, unsigned tw, unsigned th, unsigned row0) {
    unsigned x, y, i;

    if (t[0] != 0x10000000u) return 0;
    for (i = 1; i < 6u; i++) if (t[i] != 0x80000000u) return 0;
    for (x = 0; x < tw; x++)
        for (y = 0; y < th; y++) {
            const volatile uint32_t *e = t + 6u + 6u * (x * th + y);
            const int last = x == tw - 1u && y == th - 1u;

            if ((e[0] & 0x0000fffcu) != (((y + row0) << 8) | (x << 2))) return 0;
            if (((e[0] >> 31) != 0u) != last) return 0;
            for (i = 1; i < 6u; i++) if (e[i] != 0x80000000u && e[i] >= 0x00800000u) return 0;     /* a list pointer is inside video memory */
        }
    return 1;
}

/* 1 when the table reads as laid out above for tw x th tiles, from row 0. */
static inline int sh4der_tilecut_valid(const volatile uint32_t *t, unsigned tw, unsigned th) {
    return sh4der_tilecut_valid_rows(t, tw, th, 0u);
}

/* Take a table if it is valid and new: its entries are copied to RAM, once. 1: it is one of the tables now (taken, or
   known already). */
static inline int sh4der_tilecut_add(sh4der_tilecut_t *c, volatile uint32_t *t) {
    unsigned k, i;

    for (k = 0; k < c->n; k++) if (c->table[k] == t) return 1;
    if (c->on || c->n >= SH4DER_TILECUT_TABLES || !t || c->tw * c->th > SH4DER_TILECUT_TILES || !sh4der_tilecut_valid_rows(t, c->tw, c->th, c->row0)) return 0;
    for (i = 0; i < c->tw * c->th * 6u; i++) c->copy[c->n][i] = t[6u + i];
    c->table[c->n++] = t;
    return 1;
}

/* The light check a cut makes: the table's first word and its last entry's end mark, two reads. */
static inline int sh4der_tilecut_ends_ok_(const sh4der_tilecut_t *c, unsigned k) {
    const volatile uint32_t *t = c->table[k];

    return t[0] == 0x10000000u && t[6u + 6u * (c->tw * c->th - 1u)] == c->copy[k][6u * (c->tw * c->th - 1u)];
}

/* Every word of every table against its layout and its RAM copy: 1 when all is as taken. For a load or a pause (it reads
   both tables from video memory: milliseconds), with no cut on. */
static inline int sh4der_tilecut_check(const sh4der_tilecut_t *c) {
    unsigned k, i;

    if (c->on) return 0;
    for (k = 0; k < c->n; k++) {
        if (!sh4der_tilecut_valid_rows(c->table[k], c->tw, c->th, c->row0)) return 0;
        for (i = 0; i < c->tw * c->th * 6u; i++) if (c->table[k][6u + i] != c->copy[k][i]) return 0;
    }
    return 1;
}

/* Index (0 .. c->n - 1) of table t among the known tables, or -1 when not known. */
static inline int sh4der_tilecut_index(const sh4der_tilecut_t *c, const volatile uint32_t *t) {
    unsigned k;

    if (!c || !t) return -1;
    for (k = 0; k < c->n; k++) if (c->table[k] == t) return (int)k;
    return -1;
}

/* Index (0 or 1) of the OTHER table when both tables are known and `in_flight` is one of them, or -1.
   KallistiOS flips tile tables at every render start: after pvr_wait_ready, PVR_ISP_TILEMAT_ADDR names the table of
   the render in flight, so the next scene renders through the other one. */
static inline int sh4der_tilecut_other(const sh4der_tilecut_t *c, const volatile uint32_t *in_flight) {
    const int cur = sh4der_tilecut_index(c, in_flight);

    if (cur < 0 || c->n != 2u || c->need != 2u) return -1;
    return 1 - cur;
}

/* 1 when table k is currently cut (begin_at / begin_one_at) to exactly cw x ch tiles at row ry. */
static inline int sh4der_tilecut_is_cut_at(const sh4der_tilecut_t *c, unsigned k, unsigned cw, unsigned ch, unsigned ry) {
    return c && k < SH4DER_TILECUT_TABLES && k < c->n && c->tbl_on[k] == 1 && c->tbl_cw[k] == cw && c->tbl_ch[k] == ch && c->tbl_ry[k] == ry;
}

/* 1 when table k is currently cut (begin_cols / begin_one_cols) to exactly cw columns. */
static inline int sh4der_tilecut_is_cut_cols(const sh4der_tilecut_t *c, unsigned k, unsigned cw) {
    return c && k < SH4DER_TILECUT_TABLES && k < c->n && c->tbl_on[k] == 2 && c->tbl_cw[k] == cw;
}

static inline void sh4der_tilecut_write_at_(sh4der_tilecut_t *c, unsigned k, unsigned cw, unsigned ch, unsigned ry) {
    volatile uint32_t *t = c->table[k] + 6u;
    const unsigned off = ry - c->row0;
    unsigned x, y, i, j;

    for (j = 0, x = 0; x < cw; x++)
        for (y = 0; y < ch; y++, j++) {                 /* entry j takes tile (x, ry + y)'s words */
            const unsigned src = x * c->th + off + y;
            const uint32_t *e = c->copy[k] + 6u * src;
            const int last = j + 1u == cw * ch;

            if (src == j && !last) continue;            /* in place already */
            for (i = 1; i < 6u; i++) t[6u * j + i] = e[i];
            t[6u * j] = (e[0] & 0x7fffffffu) | (last ? 0x80000000u : 0u);
        }
    c->tbl_on[k] = 1; c->tbl_cw[k] = cw; c->tbl_ch[k] = ch; c->tbl_ry[k] = ry;
}

static inline unsigned sh4der_tilecut_restore_one_(const sh4der_tilecut_t *c, unsigned k, int check) {
    volatile uint32_t *t = c->table[k] + 6u;
    unsigned i, j, x, y, bad = 0;

    if (c->tbl_on[k] == 2) {
        const unsigned w = 6u * (c->tbl_cw[k] * c->th - 1u);

        t[w] = c->copy[k][w];
        if (check) bad += t[w] != c->copy[k][w];
        return bad;
    }
    for (j = 0, x = 0; x < c->tbl_cw[k]; x++)
        for (y = 0; y < c->tbl_ch[k]; y++, j++) {       /* the entries sh4der_tilecut_write_at_ wrote */
            if (x * c->th + (c->tbl_ry[k] - c->row0) + y == j && j + 1u != c->tbl_cw[k] * c->tbl_ch[k]) continue;
            for (i = 0; i < 6u; i++) t[6u * j + i] = c->copy[k][6u * j + i];
        }
    if (check) for (i = 0; i < c->tbl_cw[k] * c->tbl_ch[k] * 6u; i++) bad += t[i] != c->copy[k][i];
    return bad;
}

static inline void sh4der_tilecut_sync_on_(sh4der_tilecut_t *c) {
    unsigned k;

    c->on = 0;
    for (k = 0; k < SH4DER_TILECUT_TABLES; k++)
        if (c->tbl_on[k]) {
            c->on = c->tbl_on[k]; c->cw = c->tbl_cw[k]; c->ch = c->tbl_ch[k]; c->ry = c->tbl_ry[k];
            return;
        }
}

/* Table k alone ends after the cw x ch tiles with their top left tile at column 0, row ry; the other table is untouched.
   0: refused, nothing written. For holding a cut on a pass's own table while the screen renders through the other table
   (NOT RUN on a console yet). Restore with sh4der_tilecut_end_one or sh4der_tilecut_end. */
static inline int sh4der_tilecut_begin_one_at(sh4der_tilecut_t *c, unsigned k, unsigned cw, unsigned ch, unsigned ry) {
    if (!c || k >= SH4DER_TILECUT_TABLES || k >= c->n || c->n < c->need || c->tbl_on[k]) return 0;
    if (!cw || !ch || cw > c->tw || ch > c->th || cw * ch > SH4DER_TILECUT_MAX) return 0;
    if (ry < c->row0 || ry - c->row0 + ch > c->th) return 0;
    if (!sh4der_tilecut_ends_ok_(c, k)) return 0;
    sh4der_tilecut_write_at_(c, k, cw, ch, ry);
    c->cw = cw; c->ch = ch; c->ry = ry; c->on = 1;
    return 1;
}

/* Table k alone ends after the cw x ch tiles of the table's first row (row0). */
static inline int sh4der_tilecut_begin_one(sh4der_tilecut_t *c, unsigned k, unsigned cw, unsigned ch) {
    return sh4der_tilecut_begin_one_at(c, k, cw, ch, c->row0);
}

/* Table k alone ends after the first cw whole columns; one word written. */
static inline int sh4der_tilecut_begin_one_cols(sh4der_tilecut_t *c, unsigned k, unsigned cw) {
    if (!c || k >= SH4DER_TILECUT_TABLES || k >= c->n || c->n < c->need || c->tbl_on[k] || !cw || cw > c->tw) return 0;
    if (!sh4der_tilecut_ends_ok_(c, k)) return 0;
    c->table[k][6u + 6u * (cw * c->th - 1u)] = c->copy[k][6u * (cw * c->th - 1u)] | 0x80000000u;
    c->tbl_on[k] = 2; c->tbl_cw[k] = cw; c->tbl_ch[k] = c->th; c->tbl_ry[k] = c->row0;
    c->cw = cw; c->ch = c->th; c->ry = c->row0; c->on = 2;
    return 1;
}

/* Table k alone back as it was; the other table is untouched. Returns words still different after SH4DER_TILECUT_TRIES
   tries (read back on the first SH4DER_TILECUT_VERIFY ends). */
static inline unsigned sh4der_tilecut_end_one(sh4der_tilecut_t *c, unsigned k) {
    const int check = c && c->verify > 0u;
    unsigned tries, bad = 0;

    if (!c || k >= SH4DER_TILECUT_TABLES || k >= c->n || !c->tbl_on[k]) return 0;
    for (tries = 0; tries < (check ? SH4DER_TILECUT_TRIES : 1u); tries++) {
        bad = sh4der_tilecut_restore_one_(c, k, check);
        if (!bad) break;
    }
    if (check) c->verify--;
    c->tbl_on[k] = 0; c->tbl_cw[k] = c->tbl_ch[k] = c->tbl_ry[k] = 0;
    sh4der_tilecut_sync_on_(c);
    return bad;
}

/* Every table ends after the cw x ch tiles with their top left tile at column 0, row ry: those tiles are written to the
   table's first entries in order, so the PVR visits only them (the target's scene must keep inside them: a pass drawn at
   y = 32 * ry). 0: refused, nothing written (ry is above the table's first row or the cut runs past its last). Written from
   the RAM copy: no entry is read from video memory. */
static inline int sh4der_tilecut_begin_at(sh4der_tilecut_t *c, unsigned cw, unsigned ch, unsigned ry) {
    unsigned k;

    if (c->on || !c->n || c->n < c->need || !cw || !ch || cw > c->tw || ch > c->th || cw * ch > SH4DER_TILECUT_MAX) return 0;
    if (ry < c->row0 || ry - c->row0 + ch > c->th) return 0;
    for (k = 0; k < c->n; k++) if (!sh4der_tilecut_ends_ok_(c, k)) return 0;
    for (k = 0; k < c->n; k++) sh4der_tilecut_write_at_(c, k, cw, ch, ry);
    c->cw = cw; c->ch = ch; c->ry = ry; c->on = 1;
    return 1;
}

/* Every table ends after the cw x ch tiles of the screen's top left corner (row 0, or the first row of a compacted table). */
static inline int sh4der_tilecut_begin(sh4der_tilecut_t *c, unsigned cw, unsigned ch) {
    return sh4der_tilecut_begin_at(c, cw, ch, c->row0);
}

/* The cheapest cut: every table ends after the screen's first cw whole columns (cw x th tiles: the target's columns, every
   row). One word written a table. 0: refused, nothing written. sh4der_tilecut_end puts it back. */
static inline int sh4der_tilecut_begin_cols(sh4der_tilecut_t *c, unsigned cw) {
    unsigned k;

    if (c->on || !c->n || c->n < c->need || !cw || cw > c->tw) return 0;
    for (k = 0; k < c->n; k++) if (!sh4der_tilecut_ends_ok_(c, k)) return 0;
    for (k = 0; k < c->n; k++) {
        c->table[k][6u + 6u * (cw * c->th - 1u)] = c->copy[k][6u * (cw * c->th - 1u)] | 0x80000000u;
        c->tbl_on[k] = 2; c->tbl_cw[k] = cw; c->tbl_ch[k] = c->th; c->tbl_ry[k] = c->row0;
    }
    c->cw = cw; c->ch = c->th; c->ry = c->row0; c->on = 2;
    return 1;
}

/* Every cut table as it was. Returns the words that do not read back as taken after SH4DER_TILECUT_TRIES tries (0: as before); read
   back only on the first SH4DER_TILECUT_VERIFY ends (c->verify counts them down; set it to read back again). */
static inline unsigned sh4der_tilecut_end(sh4der_tilecut_t *c) {
    const int check = c && c->verify > 0u;
    unsigned k, tries, bad = 0;

    if (!c || !c->on) return 0;
    for (tries = 0; tries < (check ? SH4DER_TILECUT_TRIES : 1u); tries++) {
        for (bad = 0, k = 0; k < c->n; k++)
            if (c->tbl_on[k]) bad += sh4der_tilecut_restore_one_(c, k, check);
        if (!bad) break;
    }
    if (check) c->verify--;
    for (k = 0; k < SH4DER_TILECUT_TABLES; k++) { c->tbl_on[k] = 0; c->tbl_cw[k] = c->tbl_ch[k] = c->tbl_ry[k] = 0; }
    c->on = 0;
    return bad;
}

#ifdef _arch_dreamcast
/* The table named by PVR_ISP_TILEMAT_ADDR right now (the render in flight after pvr_wait_ready, or the last render).
   0 when the register is 0. */
static inline volatile uint32_t *sh4der_tilecut_active(void) {
    const uint32_t a = PVR_GET(PVR_ISP_TILEMAT_ADDR) & 0x007ffffcu;

    return a ? (volatile uint32_t *)(uintptr_t)(0xa5000000u | a) : (volatile uint32_t *)0;
}

/* The table the last render went through: taken if it is valid and new. Call after a scene's render, with no cut on.
   Returns the tables known. */
static inline unsigned sh4der_tilecut_find(sh4der_tilecut_t *c) {
    if (!c->on && c->n < SH4DER_TILECUT_TABLES) {
        volatile uint32_t *t = sh4der_tilecut_active();

        if (t) sh4der_tilecut_add(c, t);
    }
    return c->n;
}
#endif

#endif /* SH4DER_TILECUT_H */
