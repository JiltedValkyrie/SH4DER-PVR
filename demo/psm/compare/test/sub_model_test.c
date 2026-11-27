/* SH4DER-PVR: the subtract spike, host test of the models

   demo/psm/compare/test/sub_model_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Checks ../sub_model.h. Everything printed here is a MODEL in plain C, not a measurement:
     - the two-digit (high, low) compare against r > m over every pair, for 5-bit and 6-bit digits and a sample of
       8-bit digits, for each rounding rule, with the smallest counted difference 1 and 2
     - the bump unit's level as a subtract: the test picture's boundary, with the threshold at 128 and at 127
*/

#include <stdio.h>

#include "../sub_model.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static unsigned long lex_wrong(unsigned bits, unsigned thr, int rule, unsigned step) {
    const unsigned n = 1u << (2u * bits);
    unsigned long wrong = 0;
    unsigned r, m;

    for (r = 0; r < n; r += step) for (m = 0; m < n; m += step) wrong += (unsigned long)(sub_lex_shadow(r, m, bits, thr, rule) != (r > m));
    return wrong;
}

static void lex(void) {
    unsigned thr;
    int rule;

    printf("  MODEL: the two-digit compare, pairs where it differs from r > m\n");
    for (thr = 1; thr <= 2u; thr++) {
        for (rule = 0; rule < (int)CMP_RULES; rule++) {
            const unsigned long w5 = lex_wrong(5u, thr, rule, 1u), w6 = lex_wrong(6u, thr, rule, 1u), w8 = lex_wrong(8u, thr, rule, 37u);

            printf("    smallest difference %u, %-14s 5-bit digits (all 1024^2): %lu   6-bit (all 4096^2): %lu   8-bit (every 37th): %lu\n",
                   thr, cmp_rule_name(rule), w5, w6, w8);
            if (rule != CMP_RULE_SHIFT) { CHECK(w5 == 0); CHECK(w6 == 0); }      /* digits of 5 or 6 bits are 4 or 8 levels apart */
            if (thr == 1u && rule != CMP_RULE_SHIFT) CHECK(w8 == 0);
        }
    }
}

static void bump(void) {
    static uint8_t img[CMP_N * CMP_N];
    unsigned x, y, thr;
    cmp_stats_t st;

    printf("  MODEL: the bump level as a subtract (K1 %u, K3 %u, texel = M - %u, Q = receiver)\n", SUB_K1, SUB_K3, SUB_TURN);
    printf("    level where M - Rcv is -2 -1 0 +1 +2 +64: %u %u %u %u %u %u\n", sub_bump_level(sub_bump_texel(100u), 102u),
           sub_bump_level(sub_bump_texel(100u), 101u), sub_bump_level(sub_bump_texel(100u), 100u), sub_bump_level(sub_bump_texel(101u), 100u),
           sub_bump_level(sub_bump_texel(102u), 100u), sub_bump_level(sub_bump_texel(164u), 100u));
    for (thr = 128; thr >= 127u; thr--) {
        for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) img[y * CMP_N + x] = sub_bump_level(sub_bump_texel(y), x) >= thr ? 0u : 255u;
        sub_diag(img, 0u, 255u, 8u, &st);
        printf("    lit where the level is %u or more: rows %u found %u ragged %u err %d..%d\n", thr, st.rows, st.found, st.ragged, st.min_err, st.max_err);
        CHECK(st.found == 255u && st.ragged == 0u && st.min_err == 0 && st.max_err == 0);
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    lex();
    bump();
    printf(fails ? "sub_model_test: %d FAILED\n" : "sub_model_test: ok\n", fails);
    return fails != 0;
}
