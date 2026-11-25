/* SH4DER-PVR: a render target pass through a tile list its own size (demo/psm/ao)

   demo/psm/ao/tile_cut.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   RECOMMENDED (from the wiki ledger, one console; wiki/limits/header-recommended.md)
   Start with: every render target pass, cut to its own tiles (cw x ch from the target's size); TILE_CUT_TRIES = 3
   Cost:       PVR: 3,256 -> 344 us (ao4-h), 3,312 -> 375 us (c4), 5,111 -> 1,442 us (d1-c): MEASURED
               (PVR-156, PVR-092, PVR-192). SH-4: tile_cut_begin +3,930 us a frame in d1-c, and the frame
               903 us slower: MEASURED (PVR-192)
   Status:     the PVR saving PROVEN (three passes); a frame gain ADVISED: the first form did not pay in d1-c
               (its begin read the whole tables from video memory); this form, tables kept in RAM, has not run
               on a console. To prove: the whole frame cut and uncut, with the begin and end timed apart
   Do:         wait for render done after every cut pass (the table must not change under a render)
   Do not:     cut while the next scene fills during a render; carry on if tile_cut_end returns bad
               (shut the PVR down and pvr_init again)
   Maturity:   ALPHA v0.5

   KallistiOS renders a target through the screen's whole tile table, 20 x 15 tiles whatever the target's size, and
   the PVR's ISP visits every one: about 3.2 ms a pass (wiki PVR-004). This makes the table end after the target's
   own tiles for the length of one pass and puts it back (../TILE_CUT_SCOPE.md; PVR-092: 3,312 -> 375 us for a 20-tile
   target, MEASURED once, as a single last stage of the c4 image).

   STATUS: the table logic below is host tested (demo/psm/ao/test/tile_cut_test.c; no make target runs it yet). The
   per-pass use (rewrite before every target pass, restore after it) ran in ao4-h (PVR-156) and the single stage in
   c4 (PVR-092), one console each, in this file's FIRST form, which read both whole tables from video memory before
   every cut (the layout check), read the entries to save them and read them again to move them: about 5,500 uncached
   reads and 960 writes a pass at 10 x 8 tiles, and 960 writes and 960 reads to put them back. d1-c MEASURED that:
   +3.9 ms of SH-4 for the begin alone, more than the 3.7 ms of PVR it saved (PVR-192). THIS form (v0.5) keeps a copy
   of each table in RAM from the moment it is taken and only WRITES video memory in a cut: 438 words a table at
   10 x 8, as many to restore, two reads a table as a light check, and a read-back of the restore on the first
   TILE_CUT_VERIFY ends only. NOT RUN ON A CONSOLE in this form; its SH-4 cost is not measured.
   tile_cut_begin_cols is the cheapest cut: the first cw whole columns of the screen (cw x 15 tiles, more than the
   target needs), ONE word written a table and one to restore. Not run on a console.

   THE TABLE, as KallistiOS builds it at pvr_init (pvr_buffers.c pvr_init_tile_matrix, read at commit 568c3404; the
   lines want a new check against the current master before anything is offered upstream): a first entry of six words
   (10000000, then five 80000000), then six words a tile, COLUMN BY COLUMN (x outer, y inner): a control word with
   (y << 8) | (x << 2), and five list pointers (80000000: no list). Bit 31 of the control word marks the last entry.
   One table a TA buffer: two.

   USE, round a target pass of cw x ch tiles (a 128 x 130 target: 4 x 5), with NO render running that reads the table:
       tile_cut_find(&cut);                  after a scene's render, until cut.n is 2 (Dreamcast only: reads the PVR's
                                             region base register and checks the table there before taking it)
       if (tile_cut_begin(&cut, cw, ch)) ..  the tables end after the target's tiles
       ... the target's scene, its render, the wait for render done ...
       bad = tile_cut_end(&cut);             every word back; read back (three tries) on the first ends; bad: words
                                             still different

   tile_cut_begin refuses (returns 0, nothing written) when no table was found, a table's first or last entry no longer
   reads as laid out (two words read a table: all it looks at; tile_cut_check reads everything, for a load or a pause),
   the cut is larger than the screen or than TILE_CUT_MAX tiles, or a cut is on already. A table that is refused is
   left alone and the pass runs uncut. If tile_cut_end returns other than 0 the screen's next frame would be a corner
   of the picture: shut the PVR down and start it again (pvr_init builds the tables anew).

   Only the first cw x ch entries are written (and of those not the ones a cut leaves where they are: the first
   column's): what follows the end mark is not read by the PVR. The TA does not read
   this table (only the ISP does), so nothing of a scene being filled is disturbed. Only video memory is written.

   The caller's condition: no rewrite while a render that uses the table is running. A program that waits for render
   done after every pass holds it. One that fills the next scene while the last still renders does not; that wants a
   change inside KallistiOS (a short table a buffer for target passes), which this file is not. */

#ifndef TILE_CUT_H
#define TILE_CUT_H

#include <stdint.h>

#define TILE_CUT_TABLES 2u
#define TILE_CUT_MAX    96u                 /* the most tiles tile_cut_begin keeps */
#define TILE_CUT_TILES  300u                /* the most tiles of a table (20 x 15): the RAM copy's size */
#define TILE_CUT_TRIES  3u
#define TILE_CUT_VERIFY 8u                  /* ends that read their restore back; after them the restore is written and trusted */

typedef struct tile_cut {
    volatile uint32_t *table[TILE_CUT_TABLES];          /* each table's first word (its first entry) */
    unsigned n;                                         /* tables found */
    unsigned tw, th;                                    /* the screen's tiles */
    unsigned cw, ch;                                    /* the cut that is on */
    int on;                                             /* 0 none, 1 tile_cut_begin's, 2 tile_cut_begin_cols' */
    unsigned verify;                                    /* ends still to be read back */
    uint32_t copy[TILE_CUT_TABLES][TILE_CUT_TILES * 6u];   /* each table's entries as they were when it was taken (RAM) */
} tile_cut_t;

static inline void tile_cut_init(tile_cut_t *c, unsigned tw, unsigned th) {
    c->n = 0; c->tw = tw; c->th = th; c->cw = c->ch = 0; c->on = 0;
    c->verify = TILE_CUT_VERIFY;
    c->table[0] = c->table[1] = 0;
}

/* 1 when the table reads as laid out above for tw x th tiles. */
static inline int tile_cut_valid(const volatile uint32_t *t, unsigned tw, unsigned th) {
    unsigned x, y, i;

    if (t[0] != 0x10000000u) return 0;
    for (i = 1; i < 6u; i++) if (t[i] != 0x80000000u) return 0;
    for (x = 0; x < tw; x++)
        for (y = 0; y < th; y++) {
            const volatile uint32_t *e = t + 6u + 6u * (x * th + y);
            const int last = x == tw - 1u && y == th - 1u;

            if ((e[0] & 0x0000fffcu) != ((y << 8) | (x << 2))) return 0;
            if (((e[0] >> 31) != 0u) != last) return 0;
            for (i = 1; i < 6u; i++) if (e[i] != 0x80000000u && e[i] >= 0x00800000u) return 0;     /* a list pointer is inside video memory */
        }
    return 1;
}

/* Take a table if it is valid and new: its entries are copied to RAM, once. 1: it is one of the tables now (taken, or
   known already). */
static inline int tile_cut_add(tile_cut_t *c, volatile uint32_t *t) {
    unsigned k, i;

    for (k = 0; k < c->n; k++) if (c->table[k] == t) return 1;
    if (c->on || c->n >= TILE_CUT_TABLES || !t || c->tw * c->th > TILE_CUT_TILES || !tile_cut_valid(t, c->tw, c->th)) return 0;
    for (i = 0; i < c->tw * c->th * 6u; i++) c->copy[c->n][i] = t[6u + i];
    c->table[c->n++] = t;
    return 1;
}

/* The light check a cut makes: the table's first word and its last entry's end mark, two reads. */
static inline int tile_cut_ends_ok_(const tile_cut_t *c, unsigned k) {
    const volatile uint32_t *t = c->table[k];

    return t[0] == 0x10000000u && t[6u + 6u * (c->tw * c->th - 1u)] == c->copy[k][6u * (c->tw * c->th - 1u)];
}

/* Every word of every table against its layout and its RAM copy: 1 when all is as taken. For a load or a pause (it reads
   both tables from video memory: milliseconds), with no cut on. */
static inline int tile_cut_check(const tile_cut_t *c) {
    unsigned k, i;

    if (c->on) return 0;
    for (k = 0; k < c->n; k++) {
        if (!tile_cut_valid(c->table[k], c->tw, c->th)) return 0;
        for (i = 0; i < c->tw * c->th * 6u; i++) if (c->table[k][6u + i] != c->copy[k][i]) return 0;
    }
    return 1;
}

/* Every table ends after the cw x ch tiles of the screen's top left corner. 0: refused, nothing written. Written from the
   RAM copy: no entry is read from video memory. */
static inline int tile_cut_begin(tile_cut_t *c, unsigned cw, unsigned ch) {
    unsigned k, x, y, i, j;

    if (c->on || !c->n || !cw || !ch || cw > c->tw || ch > c->th || cw * ch > TILE_CUT_MAX) return 0;
    for (k = 0; k < c->n; k++) if (!tile_cut_ends_ok_(c, k)) return 0;
    for (k = 0; k < c->n; k++) {
        volatile uint32_t *t = c->table[k] + 6u;

        for (j = 0, x = 0; x < cw; x++)
            for (y = 0; y < ch; y++, j++) {             /* entry j takes tile (x, y)'s words */
                const uint32_t *e = c->copy[k] + 6u * (x * c->th + y);
                const int last = j + 1u == cw * ch;

                if (x * c->th + y == j && !last) continue;                  /* the first column's: in place already */
                for (i = 1; i < 6u; i++) t[6u * j + i] = e[i];
                t[6u * j] = (e[0] & 0x7fffffffu) | (last ? 0x80000000u : 0u);
            }
    }
    c->cw = cw; c->ch = ch; c->on = 1;
    return 1;
}

/* The cheapest cut: every table ends after the screen's first cw whole columns (cw x th tiles: the target's columns, every
   row). One word written a table. 0: refused, nothing written. tile_cut_end puts it back. */
static inline int tile_cut_begin_cols(tile_cut_t *c, unsigned cw) {
    unsigned k;

    if (c->on || !c->n || !cw || cw > c->tw) return 0;
    for (k = 0; k < c->n; k++) if (!tile_cut_ends_ok_(c, k)) return 0;
    for (k = 0; k < c->n; k++) c->table[k][6u + 6u * (cw * c->th - 1u)] = c->copy[k][6u * (cw * c->th - 1u)] | 0x80000000u;
    c->cw = cw; c->ch = c->th; c->on = 2;
    return 1;
}

/* Every table as it was. Returns the words that do not read back as taken after TILE_CUT_TRIES tries (0: as before); read
   back only on the first TILE_CUT_VERIFY ends (c->verify counts them down; set it to read back again). */
static inline unsigned tile_cut_end(tile_cut_t *c) {
    const int check = c->verify > 0u;
    unsigned k, i, j, x, y, tries, bad = 0;

    if (!c->on) return 0;
    for (tries = 0; tries < (check ? TILE_CUT_TRIES : 1u); tries++) {
        for (bad = 0, k = 0; k < c->n; k++) {
            volatile uint32_t *t = c->table[k] + 6u;

            if (c->on == 2) {
                const unsigned w = 6u * (c->cw * c->th - 1u);

                t[w] = c->copy[k][w];
                if (check) bad += t[w] != c->copy[k][w];
                continue;
            }
            for (j = 0, x = 0; x < c->cw; x++)
                for (y = 0; y < c->ch; y++, j++) {      /* the entries tile_cut_begin wrote */
                    if (x * c->th + y == j && j + 1u != c->cw * c->ch) continue;
                    for (i = 0; i < 6u; i++) t[6u * j + i] = c->copy[k][6u * j + i];
                }
            if (check) for (i = 0; i < c->cw * c->ch * 6u; i++) bad += t[i] != c->copy[k][i];
        }
        if (!bad) break;
    }
    if (check) c->verify--;
    c->on = 0;
    return bad;
}

#ifdef _arch_dreamcast
/* The table the last render went through: taken if it is valid and new. Call after a scene's render, with no cut on.
   Returns the tables known. */
static inline unsigned tile_cut_find(tile_cut_t *c) {
    if (!c->on && c->n < TILE_CUT_TABLES) {
        const uint32_t a = PVR_GET(PVR_ISP_TILEMAT_ADDR) & 0x007ffffcu;

        if (a) tile_cut_add(c, (volatile uint32_t *)(uintptr_t)(0xa5000000u | a));
    }
    return c->n;
}
#endif

#endif /* TILE_CUT_H */
