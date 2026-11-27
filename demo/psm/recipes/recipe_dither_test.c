/* SH4DER-PVR: R3 dither and dissolve by punch-through, host check

   demo/psm/recipes/recipe_dither_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The candidate rules for the punch-through test "texture alpha x vertex alpha >= reference" (two ways to widen a
   4-bit alpha, five ways to round the product) against what two console runs counted; the console lines that tell
   the rules still standing apart, for the vertex-alpha sweep page (PVR-064's open test); the fade table for each
   rule (the smallest vertex alpha that shows each number of cells); the 4 x 4 dither index. Prints PASS and FAIL
   counts, and the line list the console page should print.

   The console counts used (MEASURED, one console, one run each, 2026-11-25):
   - c4 test 6: a 4444 texture whose alpha nibble is x / 16 over 256 x 128 pixels, reference 128: vertex alpha 255
     drew 16,384, vertex alpha 128 drew 2,048 (compare/NOTES.md, c4 second run; PVR-060, PVR-064).
   - c3b 5a: the same ramp over 256 x 256 pixels, references 64 / 128 / 192: 49,152 / 32,768 / 16,384
     (compare/NOTES.md c3b). The vertex alpha there is taken as 255 (KallistiOS's white vertex; INF, not printed).
*/

#include <stdio.h>
#include "recipes.h"

static unsigned passes, fails;
#define CHECK(c) do { if (c) passes++; else { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static const char *const EXP_NAME[RCP_EXP_N] = { "replicate", "shift" };
static const char *const PT_NAME[RCP_PT_N] = { "plain", "vtx256", "both256", "round", "floor" };

#define NRULES (RCP_EXP_N * RCP_PT_N)

/* Nibbles 0..15 that pass: the ramp's drawn columns are 16 a nibble. */
static unsigned nibbles(unsigned a, unsigned ref, unsigned r) {
    return rcp_dither_cells(a, ref, r / RCP_PT_N, r % RCP_PT_N);
}

static int alive[NRULES];

/* 1. Which rules fit both console runs. */
static void t_console(void) {
    unsigned r, n = 0;
    printf("R3 rules against the console counts (c4 test 6: a 255 / 128 at ref 128; c3b 5a: refs 64 / 128 / 192):\n");
    for (r = 0; r < NRULES; r++) {
        const unsigned c4a = 128u * 16u * nibbles(255u, 128u, r), c4b = 128u * 16u * nibbles(128u, 128u, r);
        const unsigned b64 = 256u * 16u * nibbles(255u, 64u, r), b128 = 256u * 16u * nibbles(255u, 128u, r), b192 = 256u * 16u * nibbles(255u, 192u, r);
        alive[r] = c4a == 16384u && c4b == 2048u && b64 == 49152u && b128 == 32768u && b192 == 16384u;
        n += (unsigned)alive[r];
        printf("  %-9s x %-7s: c4 %5u %4u  c3b %5u %5u %5u  %s\n", EXP_NAME[r / RCP_PT_N], PT_NAME[r % RCP_PT_N],
               c4a, c4b, b64, b128, b192, alive[r] ? "fits" : "ruled out");
    }
    CHECK(n >= 1u);                                          /* some candidate fits what the console drew */
    CHECK(!alive[RCP_EXP_REPLICATE * RCP_PT_N + RCP_PT_PLAIN]);   /* the plain shift: 255 x 128 >> 8 = 127 fails ref 128 */
    for (r = 0; r < RCP_PT_N; r++) CHECK(!alive[RCP_EXP_SHIFT * RCP_PT_N + r]);   /* 240 x 128 is under 128 by every rounding */
}

/* 2. The console lines that tell the remaining rules apart: (vertex alpha, reference) -> nibbles drawn of the ramp. */
static void t_lines(void) {
    static const unsigned refs[4] = { 16u, 64u, 128u, 192u };
    int split[NRULES][NRULES] = { { 0 } };
    unsigned chosen = 0, r, s, open_pairs = 0, same = 0;
    for (;;) {
        unsigned best = 0, best_a = 0, best_ref = 0, a, k;
        for (k = 0; k < 4u; k++) {
            for (a = 1; a < 256u; a++) {
                unsigned gain = 0;
                for (r = 0; r < NRULES; r++) for (s = r + 1u; s < NRULES; s++)
                    if (alive[r] && alive[s] && !split[r][s] && nibbles(a, refs[k], r) != nibbles(a, refs[k], s)) gain++;
                if (gain > best) { best = gain; best_a = a; best_ref = refs[k]; }
            }
        }
        if (!best) break;
        chosen++;
        printf("R3 console line %u: vertex alpha %3u, reference %3u; nibbles drawn by rule:", chosen, best_a, best_ref);
        for (r = 0; r < NRULES; r++) if (alive[r]) printf(" %s/%s %u", EXP_NAME[r / RCP_PT_N], PT_NAME[r % RCP_PT_N], nibbles(best_a, best_ref, r));
        printf("\n");
        for (r = 0; r < NRULES; r++) for (s = r + 1u; s < NRULES; s++)
            if (alive[r] && alive[s] && nibbles(best_a, best_ref, r) != nibbles(best_a, best_ref, s)) split[r][s] = 1;
    }
    for (r = 0; r < NRULES; r++) for (s = r + 1u; s < NRULES; s++) {
        if (!alive[r] || !alive[s]) continue;
        if (split[r][s]) continue;
        open_pairs++;
        {   /* never split: the same on every line there is, so not a difference the console can show */
            unsigned a, k, differ = 0;
            for (k = 0; k < 256u; k++) for (a = 0; a < 256u; a++) differ += nibbles(a, k, r) != nibbles(a, k, s);
            if (!differ) { same++; printf("R3 %s/%s and %s/%s give the same pass for every alpha and reference\n",
                                          EXP_NAME[r / RCP_PT_N], PT_NAME[r % RCP_PT_N], EXP_NAME[s / RCP_PT_N], PT_NAME[s % RCP_PT_N]); }
        }
    }
    printf("R3 %u lines split every pair of the rules still standing that can be split\n", chosen);
    CHECK(open_pairs == same);
    CHECK(chosen <= 8u);                                     /* small enough for one photographed page */
}

/* 3. The fade table, per rule still standing, at reference 16. */
static void t_fade(void) {
    unsigned r;
    for (r = 0; r < NRULES; r++) {
        uint16_t tab[16];
        unsigned c, a, prev = 0, rising = 1, exact = 1, reach = 0;
        if (!alive[r]) continue;
        rcp_dither_table(tab, 16u, r / RCP_PT_N, r % RCP_PT_N);
        for (a = 0; a < 256u; a++) { const unsigned n = nibbles(a, 16u, r); if (n < prev) rising = 0; prev = n; }
        for (c = 0; c < 16u; c++) {
            if (tab[c] == 256u) continue;
            reach++;
            if (nibbles(tab[c], 16u, r) != c) exact = 0;
            if (tab[c] && nibbles(tab[c] - 1u, 16u, r) == c) exact = 0;   /* the smallest such alpha */
        }
        printf("R3 fade table (ref 16) for %s/%s: cells:alpha", EXP_NAME[r / RCP_PT_N], PT_NAME[r % RCP_PT_N]);
        for (c = 0; c < 16u; c++) if (tab[c] != 256u) printf(" %u:%u", c, tab[c]);
        printf("  (%u of 16 counts reachable)\n", reach);
        CHECK(rising);
        CHECK(exact);
        CHECK(tab[15] != 256u && tab[0] == 0u);              /* from nothing to 15 of 16 cells */
    }
}

/* 4. The dither index: each of 0..15 once in a 4 x 4 block; at the table's alpha for c cells the drawn cells are
   the c highest indices, so the pattern grows in the dither's own order. */
static void t_bayer(void) {
    unsigned seen = 0, x, y, c, ok = 1;
    uint16_t tab[16];
    for (y = 0; y < 4u; y++) for (x = 0; x < 4u; x++) seen |= 1u << rcp_bayer4(x, y);
    CHECK(seen == 0xffffu);
    rcp_dither_table(tab, 16u, RCP_EXP_REPLICATE, RCP_PT_ROUND);
    for (c = 0; c < 16u; c++) {
        if (tab[c] == 256u) continue;
        for (y = 0; y < 4u; y++) for (x = 0; x < 4u; x++) {
            const unsigned k = rcp_bayer4(x, y);
            if (rcp_pt_pass(k, tab[c], 16u, RCP_EXP_REPLICATE, RCP_PT_ROUND) != (k >= 16u - c)) ok = 0;
        }
    }
    CHECK(ok);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    t_console();
    t_lines();
    t_fade();
    t_bayer();
    printf("recipe_dither_test: PASS %u FAIL %u\n", passes, fails);
    return fails ? 1 : 0;
}
