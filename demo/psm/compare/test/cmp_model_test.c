/* SH4DER-PVR: the PVR-side compare spike, host test of the model

   demo/psm/compare/test/cmp_model_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Host check of ../cmp_model.h (run by ../build.py). It does not test the PVR. It tests the model's arithmetic and the
   counters the console program uses, and it prints what each rounding rule WOULD give on the console, so that the
   console's numbers can be read against it. Everything it prints is the model's, marked so.

     - the multiply under each rule; saturation
     - BLEND with an 8-bit map: which rules give an exact compare (boundary error 0 on every row), which do not, and
       by how much
     - BLEND with a 6-bit map (a PVR-drawn RGB565 map): the error is the map's own step, never more
     - ALPHA: the same compare gives the same boundary
     - PUNCH: the product's boundary, and what it is worth as a depth compare in the log domain
     - VOLUME: the box's pixel count
     - the boundary counter and the difference counter against pictures with known faults
*/

#include <math.h>
#include <stdio.h>
#include "../cmp_model.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static uint8_t img[CMP_N * CMP_N], ref[CMP_N * CMP_N];

static void arithmetic(void) {
    unsigned x;

    for (x = 0; x < 256u; x++) {
        CHECK(cmp_mul(x, 255u, CMP_RULE_EXACT) == x && cmp_mul(x, 255u, CMP_RULE_TRUNC) == x && cmp_mul(x, 255u, CMP_RULE_SHIFT1) == x);
        CHECK(cmp_mul(x, 255u, CMP_RULE_SHIFT) == (x ? x - 1u : 0u));   /* the one rule under which 1.0 is not 1 */
        CHECK(cmp_mul(x, 0u, CMP_RULE_EXACT) == 0u && cmp_mul(0u, x, CMP_RULE_SHIFT1) == 0u);
        CHECK(cmp_map_level(x, 8u) == x);
        CHECK((cmp_map_level(x, 6u) >> 2) == (x >> 2) && cmp_map_level(x, 6u) <= 255u);
    }
    CHECK(cmp_map_level(255u, 6u) == 255u && cmp_map_level(0u, 6u) == 0u && cmp_map_level(4u, 6u) == 4u && cmp_map_level(128u, 6u) == 130u);
    CHECK(cmp_sat(255u) == 255u && cmp_sat(256u) == 255u && cmp_sat(510u) == 255u);
}

static void render_blend(unsigned doubles, unsigned bits, int rule) {
    unsigned x, y;

    for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) img[y * CMP_N + x] = (uint8_t)cmp_blend_pixel(x, y, doubles, bits, rule);
}

static void blend(void) {
    cmp_stats_t st;
    int rule;
    unsigned x, y;

    printf("  MODEL, not a measurement: the blend compare's boundary error by rounding rule (levels of 256)\n");
    for (rule = 0; rule < (int)CMP_RULES; rule++) {
        render_blend(8u, 8u, rule);
        cmp_boundary(img, CMP_N, 0u, 255u, 0u, &st);
        printf("    8-bit map, %-14s rows %u found %u ragged %u err %d..%d between %u\n", cmp_rule_name(rule), st.rows, st.found, st.ragged,
               st.min_err, st.max_err, st.between);
        CHECK(st.rows == 255u && st.ragged == 0u);
        if (rule != CMP_RULE_SHIFT) {
            /* with a multiply that keeps 1.0 as 1, the compare is exact: shadow from x = y + 1, two values only */
            CHECK(st.found == 255u && st.min_err == 0 && st.max_err == 0 && st.between == 0u);
            for (y = 0; y < CMP_N; y += 17u) for (x = 0; x < CMP_N; x += 13u) CHECK(img[y * CMP_N + x] == (x > y ? 255u : 0u));
        } else {
            /* under ab >> 8 the invert loses 1 and a doubling is 2 d - 1: a difference of 2 or less never grows. The
               boundary is two levels late, and the picture is not two-valued (the shadow tops out at 254 or less). */
            CHECK(st.max_err == 2 && st.min_err == 2 && st.found == 253u && st.between > 0u);
        }
    }
    for (rule = 0; rule < (int)CMP_RULES; rule++) {
        render_blend(8u, 6u, rule);
        cmp_boundary(img, CMP_N, 0u, 255u, 0u, &st);
        printf("    6-bit map, %-14s rows %u found %u ragged %u err %d..%d\n", cmp_rule_name(rule), st.rows, st.found, st.ragged, st.min_err, st.max_err);
        CHECK(st.ragged == 0u && st.min_err >= -3 && st.max_err <= 5);   /* the map's own step of 4, plus the rule's own error */
        if (rule != CMP_RULE_SHIFT) CHECK(st.min_err == -3 && st.max_err == 3);
    }

    /* fewer doublings: a soft edge. After n doublings the shadow is full where R - M >= 256 / 2^n */
    render_blend(3u, 8u, CMP_RULE_EXACT);
    CHECK(img[10u * CMP_N + 10u] == 0u && img[10u * CMP_N + 11u] == 8u && img[10u * CMP_N + 20u] == 80u && img[10u * CMP_N + 42u] == 255u && img[10u * CMP_N + 41u] == 248u);
    render_blend(0u, 8u, CMP_RULE_EXACT);                         /* none: the difference itself */
    CHECK(img[10u * CMP_N + 110u] == 100u && img[200u * CMP_N + 10u] == 0u);
}

static void alpha(void) {
    cmp_stats_t st;
    int rule;
    unsigned x, y;

    for (rule = 0; rule < (int)CMP_RULES; rule++) {
        for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) img[y * CMP_N + x] = (uint8_t)cmp_alpha_pixel(x, y, 200u, rule);
        cmp_boundary(img, CMP_N, 200u, 0u, 2u, &st);
        CHECK(st.ragged == 0u && st.between == 0u);
        if (rule != CMP_RULE_SHIFT) CHECK(st.found == 255u && st.min_err == 0 && st.max_err == 0);
        else CHECK(st.min_err == 2 && st.max_err == 2);
    }
}

static void punch(void) {
    int rule;
    unsigned x, y, pass, worst = 0;

    printf("  MODEL: the punch-through product x (255 - y) >= 64: pixels drawn, by rounding rule\n");
    for (rule = 0; rule < (int)CMP_RULES; rule++) {
        for (pass = 0, y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) pass += (unsigned)cmp_punch_pixel(x, y, 64u, rule);
        printf("    %-14s %u\n", cmp_rule_name(rule), pass);
        CHECK(pass > 26000u && pass < 26800u);                    /* each rule gives its own count: the console's count names the rule */
    }
    CHECK(cmp_punch_pixel(255u, 0u, 64u, CMP_RULE_EXACT) && !cmp_punch_pixel(63u, 0u, 64u, CMP_RULE_EXACT) && cmp_punch_pixel(64u, 0u, 64u, CMP_RULE_EXACT));
    CHECK(!cmp_punch_pixel(255u, 200u, 64u, CMP_RULE_EXACT) && cmp_punch_pixel(128u, 127u, 64u, CMP_RULE_EXACT));

    /* As a depth compare the product needs depths in the log domain: texture alpha 255 e^(-k (1 - m)), vertex alpha
       255 e^(-k r), drawn where the product >= 255 e^(-k). With 8-bit alphas the smallest step of an alpha near its
       bottom is a large part of its value, so the depth error there is large: the largest, over all depths. */
    {
        const double k = log(255.0 / 8.0);                        /* alphas kept between 8 and 255 */
        double m, r, err;

        for (m = 0.0; m <= 1.0; m += 1.0 / 64.0) {
            for (r = 0.0; r <= 1.0; r += 1.0 / 512.0) {
                const unsigned ta = (unsigned)(255.0 * exp(-k * (1.0 - m)) + 0.5), va = (unsigned)(255.0 * exp(-k * r) + 0.5);
                const int drawn = cmp_mul(ta, va, CMP_RULE_EXACT) >= (unsigned)(255.0 * exp(-k) + 0.5), lit = m >= r;

                if (drawn != lit) { err = fabs(m - r); if ((unsigned)(err * 256.0) > worst) worst = (unsigned)(err * 256.0); }
            }
        }
        printf("  MODEL: punch-through as a depth compare (log codes, alphas 8..255): wrong up to %u levels of 256 from the edge\n", worst);
        CHECK(worst >= 4u && worst < 64u);                        /* much coarser than the blend compare's 0 */
    }
}

static void volume(void) {
    unsigned x, y, n = 0, x0 = 999, x1 = 0;

    for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) if (cmp_volume_pixel(x, y)) { n++; if (x < x0) x0 = x; if (x > x1) x1 = x; }
    printf("  MODEL: the volume holds %u pixels, x %u..%u\n", n, x0, x1);
    CHECK(n == 86u * 128u && x0 == 85u && x1 == 170u);
    CHECK(!cmp_volume_pixel(100u, 63u) && cmp_volume_pixel(100u, 64u) && cmp_volume_pixel(100u, 191u) && !cmp_volume_pixel(100u, 192u));
    CHECK(!cmp_volume_pixel(70u, 100u) && !cmp_volume_pixel(180u, 100u));   /* inside the box's outline, but the quad is in front of or behind it there */
}

static void counters(void) {
    cmp_stats_t st;
    unsigned x, y;

    for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) img[y * CMP_N + x] = x > y + 3u ? 255u : 0u;   /* a boundary 3 levels late */
    cmp_boundary(img, CMP_N, 0u, 255u, 0u, &st);
    CHECK(st.found == 252u && st.min_err == 3 && st.max_err == 3 && st.sum_err == 3 * 252 && st.ragged == 0u && st.between == 0u);
    for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) img[y * CMP_N + x] = x + 2u > y ? 255u : 0u;   /* 2 early */
    cmp_boundary(img, CMP_N, 0u, 255u, 0u, &st);
    CHECK(st.min_err == -2 && st.max_err == -1 && st.found == 255u);   /* rows 0 and 1 cannot start before x = 0 */
    memset(img, 0, sizeof img);                                   /* all lit: no boundary anywhere */
    cmp_boundary(img, CMP_N, 0u, 255u, 0u, &st);
    CHECK(st.found == 0u && st.lit == CMP_N * CMP_N && st.shadow == 0u);
    memset(img, 255, sizeof img);                                 /* all shadow: every row starts at 0 */
    cmp_boundary(img, CMP_N, 0u, 255u, 0u, &st);
    CHECK(st.found == 255u && st.max_err == -1 && st.min_err == -255 && st.shadow == CMP_N * CMP_N);
    render_blend(8u, 8u, CMP_RULE_EXACT);
    img[100u * CMP_N + 50u] = 255u;                               /* a stray shadow pixel in the lit part: that row is ragged */
    img[30u * CMP_N + 200u] = 77u;                                /* a value that is neither */
    cmp_boundary(img, CMP_N, 0u, 255u, 8u, &st);
    CHECK(st.ragged == 1u && st.between == 1u && st.min_err == -51 && st.max_err == 0);
    render_blend(8u, 8u, CMP_RULE_EXACT);
    for (x = 0; x < CMP_N * CMP_N; x++) img[x] = (uint8_t)(img[x] ? 248u : 6u);   /* as a 5-bit read gives it: tolerance takes it */
    cmp_boundary(img, CMP_N, 0u, 255u, 8u, &st);
    CHECK(st.found == 255u && st.max_err == 0 && st.min_err == 0 && st.between == 0u);

    for (y = 0; y < CMP_N; y++) for (x = 0; x < CMP_N; x++) ref[y * CMP_N + x] = (uint8_t)cmp_volume_pixel(x, y);
    memcpy(img, ref, sizeof img);
    CHECK(cmp_count_diff(img, ref) == 0u);
    img[0] = 1u; img[100u * CMP_N + 100u] = 0u;
    CHECK(cmp_count_diff(img, ref) == 2u);
}

int main(void) {
    arithmetic();
    blend();
    alpha();
    punch();
    volume();
    counters();
    if (fails) { fprintf(stderr, "cmp_model_test: %d failure(s)\n", fails); return 1; }
    printf("cmp_model_test: ok\n");
    return 0;
}
