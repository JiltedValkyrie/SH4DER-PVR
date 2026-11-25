/* SH4DER-PVR: host test of demo/psm/ao/tile_cut.h (the table logic of the tile list cut)

   demo/psm/ao/test/tile_cut_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Two tables built as KallistiOS's pvr_init_tile_matrix builds them (20 x 15 tiles, column by column, the last entry
   marked), in plain memory. Checked: the layout test takes a good table and refuses a spoiled one; a cut makes a walk
   of the table (entry after entry until the mark, as the PVR's ISP reads it) visit exactly the target's tiles, each
   with its own five list pointers; the end puts every word back; what a cut must refuse is refused with nothing
   written. A model of the table, not of the PVR: only a console run shows the PVR renders through it. */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../tile_cut.h"

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
    static tile_cut_t c;
    unsigned k, x, y, q, n, wrong;

    build(0); build(1);
    tile_cut_init(&c, TW, TH);
    CHECK(tile_cut_valid(tab[0], TW, TH) && tile_cut_valid(tab[1], TW, TH), "a good table is refused");
    CHECK(!tile_cut_begin(&c, 4u, 5u) && differ() == 0u, "a cut with no table");
    tab[0][6u + 6u * 37u] ^= 4u;            /* one control word wrong */
    CHECK(!tile_cut_valid(tab[0], TW, TH) && !tile_cut_add(&c, tab[0]) && c.n == 0u, "a spoiled table is taken");
    tab[0][6u + 6u * 37u] ^= 4u;
    tab[0][6u + 6u * 100u] |= 0x80000000u;  /* an end mark in the middle */
    CHECK(!tile_cut_valid(tab[0], TW, TH), "an early end mark is not seen");
    tab[0][6u + 6u * 100u] &= 0x7fffffffu;
    tab[0][3] = 0u;                         /* the first entry */
    CHECK(!tile_cut_valid(tab[0], TW, TH), "a wrong first entry is not seen");
    tab[0][3] = 0x80000000u;
    CHECK(differ() == 0u, "the test's own repairs");
    CHECK(tile_cut_add(&c, tab[0]) && tile_cut_add(&c, tab[0]) && c.n == 1u && tile_cut_add(&c, tab[1]) && c.n == 2u, "taking the tables");
    CHECK(!tile_cut_add(&c, tab[0] + 6u) && c.n == 2u, "a third table");

    for (q = 0; q < 5u; q++) {
        const unsigned cw = cuts[q][0], ch = cuts[q][1];

        if (cw * ch > TILE_CUT_MAX) { CHECK(!tile_cut_begin(&c, cw, ch) && differ() == 0u, "a cut over TILE_CUT_MAX"); continue; }
        CHECK(tile_cut_begin(&c, cw, ch), "cut %u x %u refused", cw, ch);
        CHECK(!tile_cut_begin(&c, cw, ch), "a second cut on top");
        for (k = 0; k < 2u; k++) {
            n = walk(k, seen, &wrong);
            CHECK(n == cw * ch && wrong == 0u, "cut %u x %u, table %u: %u entries walked, %u wrong", cw, ch, k, n, wrong);
            for (x = 0; x < TW; x++) for (y = 0; y < TH; y++) CHECK(seen[x][y] == (x < cw && y < ch ? 1u : 0u), "cut %u x %u: tile %u, %u visited %u times", cw, ch, x, y, seen[x][y]);
            CHECK(tab[k][WORDS] == 0xdeadbeefu && tab[k][0] == 0x10000000u, "words outside the entries changed");
        }
        CHECK(tile_cut_end(&c) == 0u && differ() == 0u, "cut %u x %u: %u words not as before", cw, ch, differ());
        CHECK(tile_cut_end(&c) == 0u && differ() == 0u, "an end with no cut on");
    }
    CHECK(tile_cut_begin(&c, TW, TH) == 0 || (walk(0, seen, &wrong) == TW * TH && wrong == 0u), "the whole screen as a cut");
    tile_cut_end(&c);
    CHECK(differ() == 0u, "after the whole-screen cut");
    CHECK(!tile_cut_begin(&c, 21u, 5u) && !tile_cut_begin(&c, 4u, 16u) && !tile_cut_begin(&c, 0u, 5u) && differ() == 0u, "cuts larger than the screen, or empty");
    CHECK(tile_cut_check(&c), "the full check on tables as taken");
    tab[1][6u + 6u * 5u + 2u] = 0x00900000u;                                /* a table spoiled after it was taken: a pointer outside video memory */
    CHECK(!tile_cut_check(&c), "the full check does not see a spoiled pointer");
    tab[1][6u + 6u * 5u + 2u] = ref[1][6u + 6u * 5u + 2u];
    tab[1][0] = 0u;                                                         /* the two words a cut looks at: the first, and the last entry's */
    CHECK(!tile_cut_begin(&c, 4u, 5u) && !tile_cut_begin_cols(&c, 4u), "a cut on a table whose first word is wrong");
    tab[1][0] = ref[1][0];
    tab[1][6u + 6u * (TW * TH - 1u)] &= 0x7fffffffu;
    CHECK(!tile_cut_begin(&c, 4u, 5u) && !tile_cut_begin_cols(&c, 4u), "a cut on a table with no end mark");
    tab[1][6u + 6u * (TW * TH - 1u)] = ref[1][6u + 6u * (TW * TH - 1u)];
    CHECK(differ() == 0u, "a refused cut wrote something");
    for (q = 1; q <= TW; q += 3u) {                                         /* the column cut: the first q whole columns, one word a table */
        unsigned changed;

        CHECK(tile_cut_begin_cols(&c, q) && !tile_cut_begin(&c, 4u, 5u), "column cut %u", q);
        for (k = 0; k < 2u; k++) {
            n = walk(k, seen, &wrong);
            CHECK(n == q * TH && wrong == 0u, "column cut %u, table %u: %u entries walked, %u wrong", q, k, n, wrong);
            for (x = 0; x < TW; x++) for (y = 0; y < TH; y++) CHECK(seen[x][y] == (x < q ? 1u : 0u), "column cut %u: tile %u, %u visited %u times", q, x, y, seen[x][y]);
        }
        changed = differ();
        CHECK(changed == (q < TW ? 2u : 0u), "column cut %u changed %u words (one a table)", q, changed);
        CHECK(tile_cut_end(&c) == 0u && differ() == 0u, "column cut %u: not as before", q);
    }
    CHECK(!tile_cut_begin_cols(&c, 0u) && !tile_cut_begin_cols(&c, TW + 1u), "column cuts outside the screen");
    {                                                                       /* what a cut writes: the first column's entries stay where they are */
        unsigned w;

        CHECK(tile_cut_begin(&c, 10u, 8u), "the 10 x 8 cut");
        w = differ();
        CHECK(w <= 2u * 73u * 6u && w >= 2u * 72u, "the 10 x 8 cut changed %u words", w);
        CHECK(tile_cut_end(&c) == 0u && differ() == 0u, "the 10 x 8 cut: not as before");
        printf("  a 10 x 8 cut writes %u words a table (73 entries: the first column's first 7 stay) and restores as many; %u of them differ from the table\n", 73u * 6u, w / 2u);
    }
    {                                                                       /* the read-back runs on the first TILE_CUT_VERIFY ends only */
        static tile_cut_t d;

        tile_cut_init(&d, TW, TH);
        CHECK(tile_cut_add(&d, tab[0]) && d.verify == TILE_CUT_VERIFY, "a second cut state");
        for (q = 0; q < TILE_CUT_VERIFY + 3u; q++) { CHECK(tile_cut_begin(&d, 4u, 5u), "begin %u", q); CHECK(tile_cut_end(&d) == 0u, "end %u", q); }
        CHECK(d.verify == 0u && differ() == 0u, "the read-back count");
    }
    if (fails) { printf("tile_cut_test: %d FAILED\n", fails); return 1; }
    printf("tile_cut_test: all passed\n");
    return 0;
}
