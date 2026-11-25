/* SH4DER-PVR: the R3 page's host test (a model, not a measurement)

   demo/psm/compare/test/r3_model_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Checks, before a console runs r3_spike.c, that the page's own arithmetic (r3_model.h) gives the numbers of
   ../../recipes/R3_PAGE_SPEC.md for each of the four candidate rules: a picture is made the way a rule would draw it,
   and the page's derive, validity and rule naming are run on it. Also prints page 2's table for the notes.

   Build: demo/psm/compare/build.py --spike r3 --host-only (it compiles this with the library's include paths).
*/

#include <stdio.h>
#include <string.h>

#include "../r3_model.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static uint16_t px[R3_W * R3_H];

/* The 256 x 128 quad as a rule draws it. */
static void draw(unsigned a, unsigned ref, unsigned rule) {
    unsigned x, y;

    for (y = 0; y < R3_H; y++) for (x = 0; x < R3_W; x++) px[y * R3_W + x] = rcp_pt_pass(x / 16u, a, ref, RCP_EXP_REPLICATE, rule) ? 0xffffu : R3_BG;
}

int main(void) {
    /* the spec's table: n and lo of lines A, B, C, D, F for VTX256, BOTH256, ROUND, FLOOR */
    static const uint8_t want_n[4][5] = { { 8, 1, 9, 4, 0 }, { 8, 1, 9, 5, 1 }, { 8, 1, 10, 5, 1 }, { 8, 1, 10, 4, 1 } };
    static const uint8_t want_lo[4][5] = { { 8, 15, 7, 12, 16 }, { 8, 15, 7, 11, 15 }, { 8, 15, 6, 11, 15 }, { 8, 15, 6, 12, 15 } };
    /* page 2's rows where the rules differ (a: V, B, R, F) and where they agree */
    static const uint8_t differ[6][5] = { { 16, 0, 1, 1, 1 }, { 24, 5, 6, 6, 6 }, { 40, 9, 9, 10, 10 }, { 48, 10, 10, 11, 11 }, { 80, 12, 12, 13, 13 }, { 120, 13, 13, 14, 14 } };
    static const uint8_t agree[][2] = { { 0, 0 }, { 8, 0 }, { 32, 8 }, { 56, 11 }, { 64, 12 }, { 72, 12 }, { 88, 13 }, { 112, 13 }, { 128, 14 }, { 232, 14 }, { 240, 15 }, { 255, 15 } };
    unsigned r, k, i;

    for (r = RCP_PT_VTX256; r <= RCP_PT_FLOOR; r++) {
        r3_line_t l[R3_LINES];

        for (k = 0; k < R3_LINES; k++) { draw(r3_alpha[k], r3_ref[k], r); r3_derive(px, &l[k]); }
        for (k = 0; k < 5u; k++) {
            CHECK(l[k].n == want_n[r - RCP_PT_VTX256][k]);
            CHECK(l[k].lo == want_lo[r - RCP_PT_VTX256][k]);
            CHECK(l[k].step == 1u && l[k].px == 2048u * l[k].n);
        }
        CHECK(r3_valid(l));
        CHECK(r3_rule(l[R3_C].n, l[R3_D].n, l[R3_F].n) == r);
        printf("  MODEL R3 %-7s: A n %u lo %u, B n %u lo %u, C n %u lo %u, D n %u lo %u, F n %u lo %u -> %s\n", r3_rule_name(r), l[0].n, l[0].lo, l[1].n, l[1].lo,
               l[2].n, l[2].lo, l[3].n, l[3].lo, l[4].n, l[4].lo, r3_rule_name(r3_rule(l[R3_C].n, l[R3_D].n, l[R3_F].n)));
    }
    CHECK(r3_rule(9u, 5u, 0u) == RCP_PT_N);                        /* a result no candidate gives */
    CHECK(r3_rule(11u, 4u, 1u) == RCP_PT_N);

    {   /* a picture that is not a clean step, and a run whose A is not c4's, are void */
        r3_line_t l[R3_LINES];

        for (k = 0; k < R3_LINES; k++) { draw(r3_alpha[k], r3_ref[k], RCP_PT_ROUND); r3_derive(px, &l[k]); }
        CHECK(r3_valid(l));
        draw(40u, 16u, RCP_PT_ROUND); px[10u * R3_W + 200u] = R3_BG; r3_derive(px, &l[R3_C]);
        CHECK(l[R3_C].step == 0u && !r3_valid(l));
        draw(40u, 16u, RCP_PT_ROUND); r3_derive(px, &l[R3_C]);
        draw(255u, 128u, RCP_PT_PLAIN); r3_derive(px, &l[R3_A]);   /* the plain product draws less: 255 x 136 >> 8 = 135, 255 x 119 >> 8 = 118 */
        draw(128u, 128u, RCP_PT_PLAIN); r3_derive(px, &l[R3_B]);
        CHECK(l[R3_B].px == 0u && !r3_valid(l));
    }

    for (i = 0; i < 6u; i++)
        for (r = RCP_PT_VTX256; r <= RCP_PT_FLOOR; r++) CHECK(r3_model_n(differ[i][0], 16u, r) == differ[i][1u + r - RCP_PT_VTX256]);
    for (i = 0; i < sizeof agree / sizeof agree[0]; i++)
        for (r = RCP_PT_VTX256; r <= RCP_PT_FLOOR; r++) CHECK(r3_model_n(agree[i][0], 16u, r) == agree[i][1]);
    printf("  MODEL R3 page 2, reference 16, nibbles drawn (VTX256 BOTH256 ROUND FLOOR):\n");
    for (i = 0; i < R3_FADE; i++) {
        const unsigned a = r3_fade_alpha(i);

        printf("      a %3u: %2u %2u %2u %2u\n", a, r3_model_n(a, 16u, RCP_PT_VTX256), r3_model_n(a, 16u, RCP_PT_BOTH256), r3_model_n(a, 16u, RCP_PT_ROUND), r3_model_n(a, 16u, RCP_PT_FLOOR));
    }
    printf(fails ? "r3_model_test: %d FAILED\n" : "r3_model_test: ok\n", fails);
    return fails != 0;
}
