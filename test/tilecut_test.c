/* SH4DER-PVR: host test of include/sh4der_tilecut.h (moved from demo/psm/ao/tile_cut.h) (the table logic of the tile list cut)

   test/tilecut_test.c (was demo/psm/ao/test/tile_cut_test.c)
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Two tables built as KallistiOS's pvr_init_tile_matrix builds them (20 x 15 tiles, column by column, the last entry
   marked), in plain memory. Checked: the layout test takes a good table and refuses a spoiled one; a cut makes a walk
   of the table (entry after entry until the mark, as the PVR's ISP reads it) visit exactly the target's tiles, each
   with its own five list pointers; the end puts every word back; what a cut must refuse is refused with nothing
   written. A model of the table, not of the PVR: only a console run shows the PVR renders through it. */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../include/sh4der_tilecut.h"

#define TW 20u
#define TH 15u
#define WORDS (6u + 6u * TW * TH)

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t tab[2][WORDS + 6u], ref[2][WORDS + 6u];

static uint32_t list_ptr(unsigned k, unsigned list, unsigned x, unsigned y) {     /* list 1 and 3 are off, as with no modifier volumes */
    if (list == 1u || list == 3u) return 0x80000000u;
    return 0x00100000u * (k + 1u) + 0x10000u * list + 64u * (TW * y + x);
}

static void build(unsigned k) {
    uint32_t *t = tab[k];
    unsigned x, y, i;

    t[0] = 0x10000000u;
    for (i = 1; i < 6u; i++) t[i] = 0x80000000u;
    for (x = 0; x < TW; x++)
        for (y = 0; y < TH; y++) {
            uint32_t *e = t + 6u + 6u * (x * TH + y);

            e[0] = (y << 8) | (x << 2) | (1u << 29);                       /* with the presort bit, as KallistiOS may set it */
            for (i = 0; i < 5u; i++) e[1u + i] = list_ptr(k, i, x, y);
        }
    t[6u + 6u * (TW * TH - 1u)] |= 0x80000000u;
    for (i = WORDS; i < WORDS + 6u; i++) t[i] = 0xdeadbeefu;                /* what lies after the table */
    memcpy(ref[k], tab[k], sizeof tab[k]);
}

/* Walk table k as the ISP does. Returns the entries read; seen[x][y] counts the visits; *wrong: entries whose words are not their tile's. */
static unsigned walk(unsigned k, unsigned seen[TW][TH], unsigned *wrong) {
    const uint32_t *e = tab[k] + 6u;
    unsigned n = 0, i;

    memset(seen, 0, sizeof(unsigned) * TW * TH);
    *wrong = 0;
    for (;;) {
        const unsigned x = (e[0] >> 2) & 63u, y = (e[0] >> 8) & 63u;

        if (n >= TW * TH || x >= TW || y >= TH) { (*wrong)++; break; }
        seen[x][y]++;
        if (!(e[0] & (1u << 29))) (*wrong)++;
        for (i = 0; i < 5u; i++) if (e[1u + i] != list_ptr(k, i, x, y)) (*wrong)++;
        n++;
        if (e[0] & 0x80000000u) break;
        e += 6u;
    }
    return n;
}

static unsigned differ(void) {
    unsigned k, i, d = 0;

    for (k = 0; k < 2u; k++) for (i = 0; i < WORDS + 6u; i++) d += tab[k][i] != ref[k][i];
    return d;
}

int main(void) {
    static const unsigned cuts[5][2] = { { 4, 5 }, { 8, 9 }, { 1, 1 }, { 4, 15 }, { 20, 4 } };
    static unsigned seen[TW][TH];
    static sh4der_tilecut_t c;
    unsigned k, x, y, q, n, wrong;

    build(0); build(1);
    sh4der_tilecut_init(&c, TW, TH);
    CHECK(sh4der_tilecut_valid(tab[0], TW, TH) && sh4der_tilecut_valid(tab[1], TW, TH), "a good table is refused");
    CHECK(!sh4der_tilecut_begin(&c, 4u, 5u) && differ() == 0u, "a cut with no table");
    tab[0][6u + 6u * 37u] ^= 4u;            /* one control word wrong */
    CHECK(!sh4der_tilecut_valid(tab[0], TW, TH) && !sh4der_tilecut_add(&c, tab[0]) && c.n == 0u, "a spoiled table is taken");
    tab[0][6u + 6u * 37u] ^= 4u;
    tab[0][6u + 6u * 100u] |= 0x80000000u;  /* an end mark in the middle */
    CHECK(!sh4der_tilecut_valid(tab[0], TW, TH), "an early end mark is not seen");
    tab[0][6u + 6u * 100u] &= 0x7fffffffu;
    tab[0][3] = 0u;                         /* the first entry */
    CHECK(!sh4der_tilecut_valid(tab[0], TW, TH), "a wrong first entry is not seen");
    tab[0][3] = 0x80000000u;
    CHECK(differ() == 0u, "the test's own repairs");
    CHECK(sh4der_tilecut_add(&c, tab[0]) && sh4der_tilecut_add(&c, tab[0]) && c.n == 1u && sh4der_tilecut_add(&c, tab[1]) && c.n == 2u, "taking the tables");
    CHECK(!sh4der_tilecut_add(&c, tab[0] + 6u) && c.n == 2u, "a third table");

    for (q = 0; q < 5u; q++) {
        const unsigned cw = cuts[q][0], ch = cuts[q][1];

        if (cw * ch > SH4DER_TILECUT_MAX) { CHECK(!sh4der_tilecut_begin(&c, cw, ch) && differ() == 0u, "a cut over SH4DER_TILECUT_MAX"); continue; }
        CHECK(sh4der_tilecut_begin(&c, cw, ch), "cut %u x %u refused", cw, ch);
        CHECK(!sh4der_tilecut_begin(&c, cw, ch), "a second cut on top");
        for (k = 0; k < 2u; k++) {
            n = walk(k, seen, &wrong);
            CHECK(n == cw * ch && wrong == 0u, "cut %u x %u, table %u: %u entries walked, %u wrong", cw, ch, k, n, wrong);
            for (x = 0; x < TW; x++) for (y = 0; y < TH; y++) CHECK(seen[x][y] == (x < cw && y < ch ? 1u : 0u), "cut %u x %u: tile %u, %u visited %u times", cw, ch, x, y, seen[x][y]);
            CHECK(tab[k][WORDS] == 0xdeadbeefu && tab[k][0] == 0x10000000u, "words outside the entries changed");
        }
        CHECK(sh4der_tilecut_end(&c) == 0u && differ() == 0u, "cut %u x %u: %u words not as before", cw, ch, differ());
        CHECK(sh4der_tilecut_end(&c) == 0u && differ() == 0u, "an end with no cut on");
    }
    CHECK(sh4der_tilecut_begin(&c, TW, TH) == 0 || (walk(0, seen, &wrong) == TW * TH && wrong == 0u), "the whole screen as a cut");
    sh4der_tilecut_end(&c);
    CHECK(differ() == 0u, "after the whole-screen cut");
    CHECK(!sh4der_tilecut_begin(&c, 21u, 5u) && !sh4der_tilecut_begin(&c, 4u, 16u) && !sh4der_tilecut_begin(&c, 0u, 5u) && differ() == 0u, "cuts larger than the screen, or empty");
    CHECK(sh4der_tilecut_check(&c), "the full check on tables as taken");
    tab[1][6u + 6u * 5u + 2u] = 0x00900000u;                                /* a table spoiled after it was taken: a pointer outside video memory */
    CHECK(!sh4der_tilecut_check(&c), "the full check does not see a spoiled pointer");
    tab[1][6u + 6u * 5u + 2u] = ref[1][6u + 6u * 5u + 2u];
    tab[1][0] = 0u;                                                         /* the two words a cut looks at: the first, and the last entry's */
    CHECK(!sh4der_tilecut_begin(&c, 4u, 5u) && !sh4der_tilecut_begin_cols(&c, 4u), "a cut on a table whose first word is wrong");
    tab[1][0] = ref[1][0];
    tab[1][6u + 6u * (TW * TH - 1u)] &= 0x7fffffffu;
    CHECK(!sh4der_tilecut_begin(&c, 4u, 5u) && !sh4der_tilecut_begin_cols(&c, 4u), "a cut on a table with no end mark");
    tab[1][6u + 6u * (TW * TH - 1u)] = ref[1][6u + 6u * (TW * TH - 1u)];
    CHECK(differ() == 0u, "a refused cut wrote something");
    for (q = 1; q <= TW; q += 3u) {                                         /* the column cut: the first q whole columns, one word a table */
        unsigned changed;

        CHECK(sh4der_tilecut_begin_cols(&c, q) && !sh4der_tilecut_begin(&c, 4u, 5u), "column cut %u", q);
        for (k = 0; k < 2u; k++) {
            n = walk(k, seen, &wrong);
            CHECK(n == q * TH && wrong == 0u, "column cut %u, table %u: %u entries walked, %u wrong", q, k, n, wrong);
            for (x = 0; x < TW; x++) for (y = 0; y < TH; y++) CHECK(seen[x][y] == (x < q ? 1u : 0u), "column cut %u: tile %u, %u visited %u times", q, x, y, seen[x][y]);
        }
        changed = differ();
        CHECK(changed == (q < TW ? 2u : 0u), "column cut %u changed %u words (one a table)", q, changed);
        CHECK(sh4der_tilecut_end(&c) == 0u && differ() == 0u, "column cut %u: not as before", q);
    }
    CHECK(!sh4der_tilecut_begin_cols(&c, 0u) && !sh4der_tilecut_begin_cols(&c, TW + 1u), "column cuts outside the screen");
    {                                                                       /* what a cut writes: the first column's entries stay where they are */
        unsigned w;

        CHECK(sh4der_tilecut_begin(&c, 10u, 8u), "the 10 x 8 cut");
        w = differ();
        CHECK(w <= 2u * 73u * 6u && w >= 2u * 72u, "the 10 x 8 cut changed %u words", w);
        CHECK(sh4der_tilecut_end(&c) == 0u && differ() == 0u, "the 10 x 8 cut: not as before");
        printf("  a 10 x 8 cut writes %u words a table (73 entries: the first column's first 7 stay) and restores as many; %u of them differ from the table\n", 73u * 6u, w / 2u);
    }
    {                                                                       /* the read-back runs on the first SH4DER_TILECUT_VERIFY ends only */
        static sh4der_tilecut_t d;

        sh4der_tilecut_init(&d, TW, TH);
        CHECK(sh4der_tilecut_add(&d, tab[0]) && d.verify == SH4DER_TILECUT_VERIFY && d.need == SH4DER_TILECUT_TABLES, "a second cut state");
        CHECK(!sh4der_tilecut_begin(&d, 4u, 5u) && !sh4der_tilecut_begin_cols(&d, 4u) && !d.on && differ() == 0u, "a cut with one of the two tables found");
        CHECK(sh4der_tilecut_end(&d) == 0u && differ() == 0u, "an end after a refused cut");
        d.need = 1u;                                                        /* a program with one table says so */
        for (q = 0; q < SH4DER_TILECUT_VERIFY + 3u; q++) { CHECK(sh4der_tilecut_begin(&d, 4u, 5u), "begin %u", q); CHECK(sh4der_tilecut_end(&d) == 0u, "end %u", q); }
        CHECK(d.verify == 0u && differ() == 0u, "the read-back count");
    }
    {                                                                       /* a table with rows 1 to 13 kept (a port that shows a 640 x 360 band), cut at a row origin */
        static uint32_t ctab[2][WORDS + 6u], cref[2][WORDS + 6u];
        static sh4der_tilecut_t d;
        const unsigned row0 = 1u, rows = 13u;
        unsigned t, i, w, kept = 0;

        for (t = 0; t < 2u; t++) {
            uint32_t *out = ctab[t] + 6u;
            const uint32_t *full = tab[t] + 6u;

            memcpy(ctab[t], tab[t], sizeof ctab[t]);                        /* the stale words behind the new last entry stay */
            for (i = 0; i < TW * TH; i++) {
                const uint32_t *e = full + 6u * i;
                const unsigned ey = (e[0] >> 8) & 0x3fu;

                if (ey < row0 || ey >= row0 + rows) continue;
                out[0] = e[0] & ~0x80000000u; out[1] = e[1]; out[2] = e[2]; out[3] = e[3]; out[4] = e[4]; out[5] = e[5];
                out += 6; kept++;
            }
            out[-6] |= 0x80000000u;
            memcpy(cref[t], ctab[t], sizeof ctab[t]);
        }
        kept /= 2u;
        CHECK(kept == TW * rows, "the compacted table keeps %u entries", kept);
        sh4der_tilecut_init_rows(&d, TW, rows, row0);
        CHECK(!sh4der_tilecut_valid(ctab[0], TW, rows) && sh4der_tilecut_valid_rows(ctab[0], TW, rows, row0), "a compacted table's layout test");
        CHECK(!sh4der_tilecut_valid_rows(ctab[0], TW, rows, 0u) && !sh4der_tilecut_valid_rows(ctab[0], TW, rows, 2u), "a compacted table with the wrong first row");
        CHECK(sh4der_tilecut_add(&d, ctab[0]) && sh4der_tilecut_add(&d, ctab[1]) && d.n == 2u, "taking the compacted tables");
        {
            static const unsigned at[5][3] = { { 4, 4, 2 }, { 8, 8, 2 }, { 4, 4, 9 }, { 2, 3, 1 }, { 4, 4, 10 } };      /* cw, ch, first row */

            for (q = 0; q < 5u; q++) {
                const unsigned cw = at[q][0], ch = at[q][1], ry = at[q][2];

                CHECK(sh4der_tilecut_begin_at(&d, cw, ch, ry), "compacted cut %u x %u at row %u refused", cw, ch, ry);
                for (t = 0; t < 2u; t++) {
                    const uint32_t *e = ctab[t] + 6u;
                    unsigned n2 = 0, bad = 0, seen2[TW][TH];

                    memset(seen2, 0, sizeof seen2);
                    for (;;) {
                        const unsigned ex = (e[0] >> 2) & 63u, ey = (e[0] >> 8) & 63u;

                        if (n2 >= TW * TH || ex >= TW || ey >= TH) { bad++; break; }
                        seen2[ex][ey]++;
                        for (i = 0; i < 5u; i++) if (e[1u + i] != list_ptr(t, i, ex, ey)) bad++;
                        n2++;
                        if (e[0] & 0x80000000u) break;
                        e += 6u;
                    }
                    CHECK(n2 == cw * ch && bad == 0u, "compacted cut %u x %u at %u, table %u: %u entries, %u wrong", cw, ch, ry, t, n2, bad);
                    for (x = 0; x < TW; x++) for (y = 0; y < TH; y++)
                        CHECK(seen2[x][y] == (x < cw && y >= ry && y < ry + ch ? 1u : 0u), "compacted cut %u x %u at %u: tile %u, %u visited %u times", cw, ch, ry, x, y, seen2[x][y]);
                }
                CHECK(sh4der_tilecut_end(&d) == 0u, "compacted cut %u: end", q);
                for (w = 0, t = 0; t < 2u; t++) for (i = 0; i < WORDS + 6u; i++) w += ctab[t][i] != cref[t][i];
                CHECK(w == 0u, "compacted cut %u x %u at %u: %u words not as before", cw, ch, ry, w);
            }
        }
        CHECK(!sh4der_tilecut_begin_at(&d, 4u, 4u, 0u) && !sh4der_tilecut_begin_at(&d, 4u, 4u, 11u) && !sh4der_tilecut_begin_at(&d, 4u, 14u, 1u)
              && !sh4der_tilecut_begin_at(&d, 4u, 4u, 14u), "a cut that starts above the table or runs past its end");
        CHECK(sh4der_tilecut_begin(&d, 4u, 4u) && sh4der_tilecut_end(&d) == 0u, "begin on a compacted table starts at its first row");
        CHECK(sh4der_tilecut_begin_cols(&d, 4u) && sh4der_tilecut_end(&d) == 0u, "a column cut on a compacted table");
        for (w = 0, t = 0; t < 2u; t++) for (i = 0; i < WORDS + 6u; i++) w += ctab[t][i] != cref[t][i];
        CHECK(w == 0u, "the compacted tables are as before");
    }
    {                                                                       /* a reset: tables forgotten, layout kept, a cut dropped without a write */
        static sh4der_tilecut_t r;

        sh4der_tilecut_init_rows(&r, TW, 13u, 1u);
        r.need = 1u;
        r.verify = 2u;
        r.n = 1u; r.on = 1; r.cw = 4u; r.ch = 4u; r.ry = 2u; r.table[0] = tab[0];
        r.tbl_on[0] = 1; r.tbl_cw[0] = 4u; r.tbl_ch[0] = 4u; r.tbl_ry[0] = 2u;
        sh4der_tilecut_reset(&r);
        CHECK(r.n == 0u && r.on == 0 && r.tbl_on[0] == 0 && r.tbl_on[1] == 0 && r.table[0] == 0 && r.table[1] == 0 && r.verify == SH4DER_TILECUT_VERIFY, "the reset state");
        CHECK(r.tw == TW && r.th == 13u && r.row0 == 1u && r.need == 1u, "the reset keeps the layout");
        CHECK(!sh4der_tilecut_begin(&r, 4u, 4u) && sh4der_tilecut_end(&r) == 0u && differ() == 0u, "a cut after a reset, before the tables are found again");
    }
    {                                                                       /* per-table cut state: index, other, single-table cut and restore, held-cut parity shift */
        static sh4der_tilecut_t h;
        unsigned i, d0, d1, f, ta_target = 0u, in_flight = 1u;
        unsigned breaks = 0u;

        sh4der_tilecut_init(&h, TW, TH);
        CHECK(sh4der_tilecut_index(&h, tab[0]) == -1 && sh4der_tilecut_other(&h, tab[0]) == -1, "index/other with no tables");
        CHECK(sh4der_tilecut_add(&h, tab[0]), "add table 0 for per-table test");
        CHECK(sh4der_tilecut_index(&h, tab[0]) == 0 && sh4der_tilecut_other(&h, tab[0]) == -1, "other refuses with one table");
        CHECK(!sh4der_tilecut_begin_one_at(&h, 0u, 4u, 4u, 2u) && differ() == 0u, "single-table cut refused before both tables found");
        h.need = 1u;
        CHECK(sh4der_tilecut_other(&h, tab[0]) == -1, "other refuses when need is 1");
        h.need = SH4DER_TILECUT_TABLES;
        CHECK(sh4der_tilecut_add(&h, tab[1]) && h.n == 2u, "add table 1 for per-table test");
        CHECK(sh4der_tilecut_index(&h, tab[0]) == 0 && sh4der_tilecut_index(&h, tab[1]) == 1 && sh4der_tilecut_index(&h, NULL) == -1 && sh4der_tilecut_index(&h, tab[0] + 6u) == -1, "index of both tables");
        CHECK(sh4der_tilecut_other(&h, tab[0]) == 1 && sh4der_tilecut_other(&h, tab[1]) == 0 && sh4der_tilecut_other(&h, NULL) == -1, "other table");

        /* cut table 0 alone: table 1 must stay untouched */
        CHECK(sh4der_tilecut_begin_one_at(&h, 0u, 4u, 4u, 2u), "single-table cut table 0 at row 2");
        CHECK(h.on == 1 && h.tbl_on[0] == 1 && h.tbl_on[1] == 0, "per-table flags after cutting table 0");
        CHECK(sh4der_tilecut_is_cut_at(&h, 0u, 4u, 4u, 2u) && !sh4der_tilecut_is_cut_at(&h, 0u, 4u, 4u, 0u) && !sh4der_tilecut_is_cut_at(&h, 1u, 4u, 4u, 2u), "is_cut_at query");
        CHECK(!sh4der_tilecut_begin_one_at(&h, 0u, 4u, 4u, 2u) && !sh4der_tilecut_begin_at(&h, 4u, 4u, 2u), "double cut on table 0 or all-table cut while one is cut refused");
        CHECK(!sh4der_tilecut_begin_one_at(&h, 2u, 4u, 4u, 2u) && !sh4der_tilecut_check(&h), "out-of-range table or full check while cut refused");
        n = walk(0u, seen, &wrong);
        CHECK(n == 16u && wrong == 0u, "single-table walk table 0: %u entries, %u wrong", n, wrong);
        for (x = 0; x < TW; x++) for (y = 0; y < TH; y++) CHECK(seen[x][y] == (x < 4u && y >= 2u && y < 6u ? 1u : 0u), "table 0 tile %u, %u", x, y);
        for (d1 = 0, i = 0; i < WORDS + 6u; i++) d1 += tab[1][i] != ref[1][i];
        CHECK(d1 == 0u, "table 1 was touched (%u words) while only table 0 was cut", d1);
        n = walk(1u, seen, &wrong);
        CHECK(n == TW * TH && wrong == 0u, "uncut table 1 walk: %u entries, %u wrong", n, wrong);

        /* cut table 1 independently with a different size, then restore table 0 alone: table 1 stays cut */
        CHECK(sh4der_tilecut_begin_one(&h, 1u, 8u, 8u) && h.tbl_on[0] == 1 && h.tbl_on[1] == 1, "independent cut on table 1");
        CHECK(sh4der_tilecut_is_cut_at(&h, 0u, 4u, 4u, 2u) && sh4der_tilecut_is_cut_at(&h, 1u, 8u, 8u, 0u), "both tables track their own shape");
        CHECK(sh4der_tilecut_end_one(&h, 0u) == 0u && h.tbl_on[0] == 0 && h.tbl_on[1] == 1 && h.on == 1, "restore table 0 alone keeps table 1 cut");
        for (d0 = 0, i = 0; i < WORDS + 6u; i++) d0 += tab[0][i] != ref[0][i];
        CHECK(d0 == 0u, "table 0 restored cleanly (%u words left)", d0);
        n = walk(1u, seen, &wrong);
        CHECK(n == 64u && wrong == 0u, "table 1 stays 8x8 after table 0 restored");
        CHECK(sh4der_tilecut_end(&h) == 0u && h.on == 0 && h.tbl_on[0] == 0 && h.tbl_on[1] == 0 && differ() == 0u, "end restores remaining cut table");
        CHECK(sh4der_tilecut_end_one(&h, 0u) == 0u && sh4der_tilecut_end_one(&h, 2u) == 0u, "end_one on uncut or invalid table");

        /* single-table column cut on table 1 alone */
        CHECK(sh4der_tilecut_begin_one_cols(&h, 1u, 4u) && sh4der_tilecut_is_cut_cols(&h, 1u, 4u) && !sh4der_tilecut_is_cut_cols(&h, 0u, 4u), "single-table cols cut");
        for (d0 = 0, i = 0; i < WORDS + 6u; i++) d0 += tab[0][i] != ref[0][i];
        CHECK(d0 == 0u && differ() == 1u, "single-table cols cut wrote 1 word in table 1 only");
        CHECK(sh4der_tilecut_end_one(&h, 1u) == 0u && h.on == 0 && differ() == 0u, "single-table cols restore");

        /* simulate KallistiOS double-buffered flip with held cut, a released no-pass frame (parity shift), and an order break */
        for (f = 0; f < 10u; f++) {
            if (f == 4u) {                                                  /* frame without a pass: release before screen, parity shifts */
                CHECK(sh4der_tilecut_end(&h) == 0u && differ() == 0u, "release before no-pass frame");
                in_flight = ta_target; ta_target ^= 1u;                     /* screen renders on ta_target */
                n = walk(in_flight, seen, &wrong);
                CHECK(n == TW * TH && wrong == 0u, "no-pass frame screen renders full table");
                continue;
            }
            if (f == 7u) {                                                  /* simulate caller forgetting release on a no-pass frame */
                in_flight = ta_target; ta_target ^= 1u;
                continue;
            }
            {
                const int cur = sh4der_tilecut_index(&h, tab[in_flight]);
                const int next = sh4der_tilecut_other(&h, tab[in_flight]);

                CHECK(cur == (int)in_flight && next == (int)ta_target, "frame %u: other matches ta_target", f);
                if (h.tbl_on[cur]) {                                        /* order broke: recover by restoring cur */
                    breaks++;
                    CHECK(sh4der_tilecut_end_one(&h, (unsigned)cur) == 0u, "frame %u: recover broken table", f);
                }
                if (!sh4der_tilecut_is_cut_at(&h, (unsigned)next, 4u, 4u, 2u))
                    CHECK(sh4der_tilecut_begin_one_at(&h, (unsigned)next, 4u, 4u, 2u), "frame %u: cut pass table %d", f, next);
                /* pass renders on ta_target */
                in_flight = ta_target; ta_target ^= 1u;
                n = walk(in_flight, seen, &wrong);
                CHECK(n == 16u && wrong == 0u, "frame %u: pass walks 4x4 on table %u", f, in_flight);
                /* screen renders on next ta_target (the uncut table) */
                in_flight = ta_target; ta_target ^= 1u;
                n = walk(in_flight, seen, &wrong);
                CHECK(n == TW * TH && wrong == 0u, "frame %u: screen walks full table %u", f, in_flight);
            }
        }
        CHECK(breaks == 1u, "order break detected once (got %u)", breaks);
        CHECK(sh4der_tilecut_end(&h) == 0u && differ() == 0u, "final held-cut restore");
    }
    if (fails) { printf("tilecut_test: %d FAILED\n", fails); return 1; }
    printf("tilecut_test: all passed\n");
    return 0;
}
