/* SH4DER-PVR: R4 depth pulse and contours by the fog table, host check

   demo/psm/recipes/recipe_fogpulse_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The table index (16 entries an octave of the scaled 1/w, linear inside one) against an exact reference; its
   inverse; the model against c3b's two fog lines (the levels the console read, MEASURED once, 2026-11-25:
   compare/NOTES.md c3b 6a and 6b); the packed table read back; a pulse: where its peak falls, how much of it a
   narrow pulse loses between entries, how wide it is in metres; scene fog kept under it; contours by period.
   Prints PASS and FAIL counts.
*/

#include <math.h>
#include <stdio.h>
#include "recipes.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.00002f
#endif

static unsigned passes, fails;
#define CHECK(c) do { if (c) passes++; else { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static double log2lin_ref(double x) {
    int e;
    const double m = frexp(x, &e);
    return (e - 1) + (2.0 * m - 1.0);
}

/* 1, 2. The index against the reference, and its inverse. SH4ZAM's float reading of the bits keeps 24 of 30 bits. */
static void t_index(void) {
    const float tol = SH4DER_TEST_TOL > 0.0001f ? 0.0002f : 1e-6f;
    float worst = 0.0f, worst_inv = 0.0f;
    unsigned i;
    for (i = 0; i < 8192u; i++) {
        const float s = 1.0f + 255.0f * (float)i / 8192.0f;
        const float d = fabsf(rcp_log2lin_(s) - (float)log2lin_ref(s)), r = fabsf(rcp_exp2lin_(rcp_log2lin_(s)) / s - 1.0f);
        if (d > worst) worst = d;
        if (r > worst_inv) worst_inv = r;
    }
    printf("R4 index: worst %.2e octaves (%.2e entries) against the exact reading; inverse %.2e relative\n",
           (double)worst, 16.0 * worst, (double)worst_inv);
    CHECK(worst <= tol);
    CHECK(worst_inv <= 2.0f * tol);
}

static void pack_levels(uint32_t w[RCP_FOG_N], const uint8_t t[RCP_FOG_N + 1u], float k, float iw0, float iw1, const unsigned *xs, unsigned n, float *out) {
    unsigned i;
    rcp_fog_pack(w, t);
    for (i = 0; i < n; i++) out[i] = rcp_fog_level(w, rcp_fog_pos(k, iw0 + (iw1 - iw0) * ((float)xs[i] + 0.5f) / 256.0f));
}

/* 3. c3b: 6a a table of 2i across 0 <= 1/w <= 1 (scale 256); 6b the top octave a ramp across 1/w from 0.5 to 1. */
static void t_c3b(void) {
    static const unsigned xa[8] = { 1u, 3u, 7u, 15u, 31u, 63u, 127u, 255u }, ca[8] = { 12u, 52u, 89u, 125u, 158u, 190u, 223u, 255u };
    static const unsigned xb[5] = { 0u, 64u, 128u, 192u, 255u }, cb[5] = { 0u, 60u, 125u, 190u, 251u };
    uint8_t t[RCP_FOG_N + 1u];
    uint32_t w[RCP_FOG_N];
    float la[8], lb[5], worst_a = 0.0f, worst_b = 0.0f, worst_m = 0.0f;
    unsigned i;
    for (i = 0; i <= RCP_FOG_N; i++) t[i] = (uint8_t)(i < RCP_FOG_N ? 2u * i : 255u);
    pack_levels(w, t, 256.0f, 0.0f, 1.0f, xa, 8u, la);
    for (i = 0; i < 8u; i++) {
        const double s = xa[i] + 0.5;              /* sub_spike.c's own model of the same line */
        unsigned oct = 0;
        double m;
        while ((double)(2u << oct) <= s) oct++;
        m = 2.0 * (16.0 * oct + 16.0 * (s / (double)(1u << oct) - 1.0));
        if (m > 254.0) m = 127.0 + 0.5 * m;    /* the last entry blends 254 to 255, the table's top, where 2i would reach 256 */
        if (fabsf(la[i] - (float)m) > worst_m) worst_m = fabsf(la[i] - (float)m);
        if (fabsf(la[i] - (float)ca[i]) > worst_a) worst_a = fabsf(la[i] - (float)ca[i]);
    }
    for (i = 0; i <= RCP_FOG_N; i++) t[i] = (uint8_t)(i < 112u ? 0u : (i - 112u) * 255u / 16u);
    pack_levels(w, t, 256.0f, 0.5f, 1.0f, xb, 5u, lb);
    for (i = 0; i < 5u; i++) if (fabsf(lb[i] - (float)cb[i]) > worst_b) worst_b = fabsf(lb[i] - (float)cb[i]);
    printf("R4 c3b 6a model:");
    for (i = 0; i < 8u; i++) printf(" %.1f", (double)la[i]);
    printf(" (console %u %u %u %u %u %u %u %u)\n", ca[0], ca[1], ca[2], ca[3], ca[4], ca[5], ca[6], ca[7]);
    printf("R4 c3b 6b model:");
    for (i = 0; i < 5u; i++) printf(" %.1f", (double)lb[i]);
    printf(" (console %u %u %u %u %u)\n", cb[0], cb[1], cb[2], cb[3], cb[4]);
    printf("R4 worst against sub_spike's model %.3f; against the console 6a %.1f, 6b %.1f levels (one 6-bit step is 4)\n",
           (double)worst_m, (double)worst_a, (double)worst_b);
    CHECK(worst_m <= 0.01f);
    CHECK(worst_a <= 4.0f);                        /* the console read 6 bits: one step either way */
    CHECK(worst_b <= 4.0f);
}

/* 5. The packed table read back at whole positions gives t itself. */
static void t_pack(void) {
    uint8_t t[RCP_FOG_N + 1u];
    uint32_t w[RCP_FOG_N];
    unsigned i, ok = 1;
    for (i = 0; i <= RCP_FOG_N; i++) t[i] = (uint8_t)((i * 37u + 11u) & 255u);
    rcp_fog_pack(w, t);
    for (i = 0; i <= RCP_FOG_N; i++) if (rcp_fog_level(w, (float)i) != (float)t[i]) ok = 0;
    CHECK(ok);
}

/* 4, 6. A pulse at depth d (scale 256: depths up to 256 m reach the table), half width h entries. */
static void t_pulse(void) {
    static const float ds[4] = { 4.0f, 20.0f, 60.0f, 150.0f }, hs[3] = { 1.0f, 2.0f, 4.0f };
    const float k = 256.0f;
    uint8_t base[RCP_FOG_N + 1u], t[RCP_FOG_N + 1u];
    uint32_t w[RCP_FOG_N];
    unsigned a, b, i, kept = 1;
    for (i = 0; i <= RCP_FOG_N; i++) base[i] = (uint8_t)(i < 64u ? 64u - i : 0u);   /* scene fog, thick far away */
    for (a = 0; a < 4u; a++) {
        for (b = 0; b < 3u; b++) {
            const float d = ds[a], h = hs[b], pd = rcp_fog_pos(k, 1.0f / d);
            float best = -1.0f, best_p = 0.0f, lo = -1.0f, hi = -1.0f;
            rcp_fog_pulse(t, NULL, k, d, h, 255.0f);
            rcp_fog_pack(w, t);
            for (i = 0; i <= 12800u; i++) {        /* positions 0..128 in steps of 0.01 */
                const float p = (float)i * 0.01f, v = rcp_fog_level(w, p);
                if (v > best) { best = v; best_p = p; }
            }
            for (i = 0; i <= 12800u; i++) {
                const float p = (float)i * 0.01f, v = rcp_fog_level(w, p);
                if (v >= 0.5f * best) { if (lo < 0.0f) lo = p; hi = p; }
            }
            {
                const float near_m = rcp_fog_depth(k, hi), far_m = rcp_fog_depth(k, lo);
                printf("R4 pulse at %5.1f m, half width %.0f: peak %5.1f at %.2f m; half level from %.2f to %.2f m, %.1f %% of its depth\n",
                       (double)d, (double)h, (double)best, (double)rcp_fog_depth(k, best_p), (double)near_m, (double)far_m,
                       100.0 * (far_m - near_m) / d);
                CHECK(fabsf(best_p - pd) <= 1.0f);            /* the peak within one entry of the pulse */
                if (h >= 2.0f) CHECK(best >= 255.0f * (1.0f - 0.5f / h) - 0.5f);
                CHECK(near_m < d && far_m > d);
            }
            rcp_fog_pulse(t, base, k, d, h, 255.0f);
            for (i = 0; i <= RCP_FOG_N; i++) if (t[i] < base[i]) kept = 0;
        }
    }
    CHECK(kept);
}

/* 7. Contours: a period of 8 entries is two lines an octave, 16 across the table. */
static void t_contours(void) {
    uint8_t t[RCP_FOG_N + 1u];
    unsigned i, lines = 0;
    rcp_fog_contours(t, 8.0f, 0.25f, 0.0f, 255u);
    for (i = 0; i < RCP_FOG_N; i++) lines += t[i] && (i == 0u || !t[i - 1u]);
    printf("R4 contours, period 8 entries: %u lines over 8 octaves\n", lines);
    CHECK(lines == 16u);
}

int main(void) {
    t_index();
    t_c3b();
    t_pack();
    t_pulse();
    t_contours();
    printf("recipe_fogpulse_test: PASS %u FAIL %u\n", passes, fails);
    return fails ? 1 : 0;
}
