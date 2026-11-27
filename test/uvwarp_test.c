/* SH4DER-PVR

   test/uvwarp_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the per-vertex UV warp: a texel centre returns its texel, the middle of two texels their mean,
   both axes wrap (continuous across 1.0 and across negatives), the scroll moves the table, the offset is weight x
   table and never negative (unsigned: nothing re-centres), weight 0 gives 0, and a bad table, a non-finite input
   or a non-finite weight gives 0, 0.
*/

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include "../include/sh4der_uvwarp.h"

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int near1(float a, float b) { return fabsf(a - b) < 1.0e-4f; }

/* 4 x 2: R = 0, 64, 128, 255 on row 0 and 255, 128, 64, 0 on row 1; G = 255 - R. */
static const uint8_t TAB[4 * 2 * 2] = {
    0, 255, 64, 191, 128, 127, 255, 0,
    255, 0, 128, 127, 64, 191, 0, 255,
};

static int texels(void) {
    const sh4der_uvwarp_t t = { TAB, 4, 2 };
    float r, g;

    sh4der_uvwarp_sample(&t, 0.125f, 0.25f, &r, &g);               /* centre of texel (0, 0) */
    if (!near1(r, 0.0f) || !near1(g, 1.0f)) return fail("texel (0, 0) centre");
    sh4der_uvwarp_sample(&t, 0.875f, 0.75f, &r, &g);               /* centre of texel (3, 1) */
    if (!near1(r, 0.0f) || !near1(g, 1.0f)) return fail("texel (3, 1) centre");
    sh4der_uvwarp_sample(&t, 0.375f, 0.25f, &r, &g);               /* centre of texel (1, 0) */
    if (!near1(r, 64.0f / 255.0f) || !near1(g, 191.0f / 255.0f)) return fail("texel (1, 0) centre");
    sh4der_uvwarp_sample(&t, 0.25f, 0.25f, &r, &g);                /* between texels 0 and 1 of row 0 */
    if (!near1(r, 32.0f / 255.0f) || !near1(g, 223.0f / 255.0f)) return fail("middle of two texels is their mean");
    sh4der_uvwarp_sample(&t, 0.125f, 0.5f, &r, &g);                /* between rows 0 and 1 of column 0 */
    if (!near1(r, 127.5f / 255.0f) || !near1(g, 127.5f / 255.0f)) return fail("middle of two rows is their mean");
    return 0;
}

static int wrap(void) {
    const sh4der_uvwarp_t t = { TAB, 4, 2 };
    float r0, g0, r1, g1, e = 1.0e-4f;

    sh4der_uvwarp_sample(&t, 1.0f - e, 0.25f, &r0, &g0);           /* across u = 1.0: continuous, the mean of the last and the first column */
    sh4der_uvwarp_sample(&t, e, 0.25f, &r1, &g1);
    if (fabsf(r0 - r1) > 1.0e-2f || fabsf(g0 - g1) > 1.0e-2f) return fail("u wrap not continuous across 1.0");
    sh4der_uvwarp_sample(&t, 1.0f, 0.25f, &r0, &g0);
    if (!near1(r0, (255.0f + 0.0f) / 2.0f / 255.0f)) return fail("u = 1.0 is the mean of the last and first texel");
    sh4der_uvwarp_sample(&t, 0.3f, 0.7f, &r0, &g0);
    sh4der_uvwarp_sample(&t, 1.3f, 2.7f, &r1, &g1);
    if (!near1(r0, r1) || !near1(g0, g1)) return fail("period 1 on both axes");
    sh4der_uvwarp_sample(&t, -0.7f, -0.3f, &r1, &g1);              /* negatives wrap the same way */
    if (!near1(r0, r1) || !near1(g0, g1)) return fail("negative coordinates wrap");
    sh4der_uvwarp_sample(&t, 0.3f, 1.0f - e, &r0, &g0);            /* across v = 1.0 */
    sh4der_uvwarp_sample(&t, 0.3f, e, &r1, &g1);
    if (fabsf(r0 - r1) > 1.0e-2f || fabsf(g0 - g1) > 1.0e-2f) return fail("v wrap not continuous across 1.0");
    return 0;
}

static int offsets(void) {
    const sh4der_uvwarp_t t = { TAB, 4, 2 };
    float du, dv, r, g;
    int i;

    sh4der_uvwarp_offset(&t, 0.375f, 0.25f, 0.0f, 0.0f, 0.0f, 0.5f, &du, &dv);
    if (!near1(du, 0.5f * 64.0f / 255.0f) || !near1(dv, 0.5f * 191.0f / 255.0f)) return fail("offset = weight x table");
    sh4der_uvwarp_offset(&t, 0.375f, 0.25f, 0.0f, 0.0f, 0.0f, 0.0f, &du, &dv);
    if (du != 0.0f || dv != 0.0f) return fail("weight 0 gives 0, 0");
    sh4der_uvwarp_offset(&t, 0.125f, 0.25f, 0.25f, 0.0f, 1.0f, 1.0f, &du, &dv);   /* scroll 0.25 u/s for 1 s: the texel one column on */
    sh4der_uvwarp_sample(&t, 0.375f, 0.25f, &r, &g);
    if (!near1(du, r) || !near1(dv, g)) return fail("the table scrolls with time");
    sh4der_uvwarp_offset(&t, 0.5f, 0.5f, -1.0f, 0.0f, 3.0f, 1.0f, &du, &dv);      /* a whole turn: the same as no scroll */
    sh4der_uvwarp_sample(&t, 0.5f, 0.5f, &r, &g);
    if (!near1(du, r) || !near1(dv, g)) return fail("a whole table turn is no scroll");
    for (i = 0; i < 200; i++) {                                    /* unsigned: never below 0, never above the weight */
        sh4der_uvwarp_offset(&t, (float)i * 0.0371f, (float)i * 0.0173f, -1.0f, -0.5f, (float)i * 0.033f, 0.8f, &du, &dv);
        if (du < 0.0f || dv < 0.0f || du > 0.8f + 1.0e-4f || dv > 0.8f + 1.0e-4f) return fail("offset outside 0 .. weight");
    }
    return 0;
}

static int guards(void) {
    const sh4der_uvwarp_t good = { TAB, 4, 2 };
    const sh4der_uvwarp_t nul = { 0, 4, 2 }, zw = { TAB, 0, 2 }, big = { TAB, 257, 2 };
    float du = 9.0f, dv = 9.0f;

    if (!sh4der_uvwarp_ok(&good) || sh4der_uvwarp_ok(&nul) || sh4der_uvwarp_ok(&zw) || sh4der_uvwarp_ok(&big) || sh4der_uvwarp_ok(0)) return fail("table check");
    sh4der_uvwarp_offset(&nul, 0.3f, 0.3f, 0.0f, 0.0f, 0.0f, 1.0f, &du, &dv);
    if (du != 0.0f || dv != 0.0f) return fail("bad table gives 0, 0");
    du = dv = 9.0f;
    sh4der_uvwarp_offset(&good, NAN, 0.3f, 0.0f, 0.0f, 0.0f, 1.0f, &du, &dv);
    if (du != 0.0f || dv != 0.0f) return fail("non-finite u gives 0, 0");
    du = dv = 9.0f;
    sh4der_uvwarp_offset(&good, 0.3f, 0.3f, INFINITY, 0.0f, 1.0f, 1.0f, &du, &dv);
    if (du != 0.0f || dv != 0.0f) return fail("non-finite scroll gives 0, 0");
    du = dv = 9.0f;
    sh4der_uvwarp_offset(&good, 0.3f, 0.3f, 0.0f, 0.0f, 0.0f, NAN, &du, &dv);
    if (du != 0.0f || dv != 0.0f) return fail("non-finite weight gives 0, 0");
    return 0;
}

/* The prepared form gives the plain one's answer over a grid of u, v, time, scroll and weight (to 2e-4 of the table range), and its guards give 0, 0. */
static int prepared(void) {
    const sh4der_uvwarp_t t = { TAB, 4, 2 };
    sh4der_uvwarp_prep_t p;
    const float us[] = { -0.7f, -0.05f, 0.0f, 0.13f, 0.5f, 0.999f, 1.0f, 2.3f, 7.9f };
    const float ts[] = { 0.0f, 0.37f, 1.0f, 4.2f, 90.0f };
    const float ss[] = { -1.0f, -0.93f, 0.0f, 0.5f, 3.0f };
    const float ws[] = { 0.0f, 0.25f, 1.0f };
    float worst = 0.0f, du, dv, pu, pv;
    unsigned i, j, k, l, m, n;

    for (i = 0; i < sizeof us / sizeof *us; i++)
        for (j = 0; j < sizeof us / sizeof *us; j++)
            for (k = 0; k < sizeof ts / sizeof *ts; k++)
                for (l = 0; l < sizeof ss / sizeof *ss; l++)
                    for (m = 0; m < sizeof ss / sizeof *ss; m++)
                        for (n = 0; n < sizeof ws / sizeof *ws; n++) {
                            sh4der_uvwarp_offset(&t, us[i], us[j], ss[l], ss[m], ts[k], ws[n], &du, &dv);
                            if (!sh4der_uvwarp_prepare(&p, &t, ss[l], ss[m], ts[k])) return fail("prepare refused a good table");
                            sh4der_uvwarp_offset_p(&p, us[i], us[j], ws[n], &pu, &pv);
                            if (fabsf(du - pu) > worst) worst = fabsf(du - pu);
                            if (fabsf(dv - pv) > worst) worst = fabsf(dv - pv);
                        }
    if (worst > 2.0e-4f) {
        fprintf(stderr, "prepared offset differs from the plain one by %g\n", (double)worst);
        return fail("prepared == plain");
    }
    sh4der_uvwarp_prepare(&p, &t, 0.0f, 0.0f, 0.0f);
    du = dv = 9.0f;
    sh4der_uvwarp_offset_p(&p, NAN, 0.3f, 1.0f, &du, &dv);
    if (du != 0.0f || dv != 0.0f) return fail("prepared: non-finite u gives 0, 0");
    du = dv = 9.0f;
    sh4der_uvwarp_offset_p(&p, -40.0f, 0.3f, 1.0f, &du, &dv);
    if (du != 0.0f || dv != 0.0f) return fail("prepared: u below -32 gives 0, 0");
    du = dv = 9.0f;
    sh4der_uvwarp_offset_p(&p, 0.3f, 0.3f, NAN, &du, &dv);
    if (du != 0.0f || dv != 0.0f) return fail("prepared: non-finite weight gives 0, 0");
    if (sh4der_uvwarp_prepare(&p, &t, 1.0f, 0.0f, 3.0e9f)) return fail("prepare refuses a huge scroll");
    du = dv = 9.0f;
    sh4der_uvwarp_offset_p(&p, 0.3f, 0.3f, 1.0f, &du, &dv);
    if (du != 0.0f || dv != 0.0f) return fail("prepared: a refused prepare gives 0, 0");
    {
        const sh4der_uvwarp_t bad = { NULL, 4, 2 };
        if (sh4der_uvwarp_prepare(&p, &bad, 0.0f, 0.0f, 0.0f)) return fail("prepare refuses a NULL table");
        if (sh4der_uvwarp_prepare(&p, NULL, 0.0f, 0.0f, 0.0f)) return fail("prepare refuses no table");
    }
    return 0;
}

/* Extreme coordinates and odd table sizes: the table sits in the middle of a 512 KB field of 0xFF bytes and holds ZEROS, so a read anywhere outside the table gives a non-zero
 * answer. A coordinate outside +- 2^23 (huge, infinite, NaN) gives 0, 0; every size from 1 x 1 to 256 x 256 reads inside its own w x h bytes. */
static uint8_t field[512 * 1024];

static int extremes(void) {
    static const float bad[] = { 3.0e9f, -3.0e9f, 1.0e10f, -1.0e10f, 1.0e30f, -1.0e30f, 3.4e38f, -3.4e38f, 8388608.0f, -8388608.0f, 2147483648.0f, -2147483648.0f, -2147483700.0f };
    static const float edge[] = { 8388607.0f, -8388607.0f, 1.0e7f, -1.0e6f, 65535.5f, -33.0f, -32.5f, -1.0e-8f, 1.0e-8f, 0.0f };
    static const unsigned dims[][2] = { { 1, 1 }, { 3, 5 }, { 7, 2 }, { 256, 1 }, { 1, 256 }, { 5, 3 }, { 64, 16 }, { 255, 255 }, { 256, 256 } };
    const float nan = NAN, inf = INFINITY;
    unsigned d, i, j;
    float r, g, du, dv;

    for (d = 0; d < sizeof dims / sizeof *dims; d++) {
        const unsigned w = dims[d][0], h = dims[d][1];
        uint8_t *tab = field + sizeof field / 2 - (w * h);           /* inside the field, bytes before and after are 0xFF */
        sh4der_uvwarp_t t;
        sh4der_uvwarp_prep_t p;
        unsigned k;

        for (k = 0; k < sizeof field; k++) field[k] = 0xFF;
        for (k = 0; k < 2 * w * h; k++) tab[k] = 0;
        t.rg = tab; t.w = (uint16_t)w; t.h = (uint16_t)h;
        for (i = 0; i < sizeof bad / sizeof *bad + 2; i++) {
            const float u = i < sizeof bad / sizeof *bad ? bad[i] : (i == sizeof bad / sizeof *bad ? nan : inf);
            sh4der_uvwarp_sample(&t, u, 0.3f, &r, &g);
            if (r != 0.0f || g != 0.0f) return fail("u out of the domain: 0, 0 and no read outside the table");
            sh4der_uvwarp_sample(&t, 0.3f, u, &r, &g);
            if (r != 0.0f || g != 0.0f) return fail("v out of the domain: 0, 0 and no read outside the table");
            sh4der_uvwarp_offset(&t, 0.3f, 0.3f, u, 0.0f, 1.0f, 1.0f, &du, &dv);
            if (du != 0.0f || dv != 0.0f) return fail("scroll out of the domain: 0, 0");
            if (sh4der_uvwarp_prepare(&p, &t, u, 0.0f, 1.0f) && (sh4der_uvwarp_offset_p(&p, 0.3f, 0.3f, 1.0f, &du, &dv), du != 0.0f || dv != 0.0f)) return fail("prepared: scroll out of the domain");
            sh4der_uvwarp_prepare(&p, &t, 0.0f, 0.0f, 0.0f);
            sh4der_uvwarp_offset_p(&p, u, 0.3f, 1.0f, &du, &dv);
            if (du != 0.0f || dv != 0.0f) return fail("prepared: u out of the domain: 0, 0");
            sh4der_uvwarp_offset_p(&p, 0.3f, u, 1.0f, &du, &dv);
            if (du != 0.0f || dv != 0.0f) return fail("prepared: v out of the domain: 0, 0");
        }
        for (i = 0; i < sizeof edge / sizeof *edge; i++)
            for (j = 0; j < sizeof edge / sizeof *edge; j++) {         /* in the domain edge and just past it: always inside the table (zeros) */
                sh4der_uvwarp_sample(&t, edge[i], edge[j], &r, &g);
                if (r != 0.0f || g != 0.0f) return fail("edge coordinate read outside the w x h table");
                sh4der_uvwarp_prepare(&p, &t, 0.0f, 0.0f, 0.0f);
                sh4der_uvwarp_offset_p(&p, edge[i], edge[j], 1.0f, &du, &dv);
                if (du != 0.0f || dv != 0.0f) return fail("prepared edge coordinate read outside the w x h table");
            }
    }
    return 0;
}

/* Odd sizes (1 x 1, 3 x 5, 255 x 255): the prepared form equals the plain one on a ramp table, weights are saturated, a negative weight gives no push. */
static int sizes(void) {
    static uint8_t tab[255 * 255 * 2];
    static const unsigned dims[][2] = { { 1, 1 }, { 3, 5 }, { 7, 2 }, { 255, 255 } };
    unsigned d, k, i;
    float du, dv, pu, pv, worst = 0.0f;

    for (d = 0; d < sizeof dims / sizeof *dims; d++) {
        sh4der_uvwarp_t t;
        sh4der_uvwarp_prep_t p;
        const unsigned w = dims[d][0], h = dims[d][1];

        for (k = 0; k < 2 * w * h; k++) tab[k] = (uint8_t)(k * 37u + 11u);
        t.rg = tab; t.w = (uint16_t)w; t.h = (uint16_t)h;
        sh4der_uvwarp_prepare(&p, &t, 0.31f, -0.17f, 2.5f);
        for (i = 0; i < 400; i++) {
            const float u = -3.0f + (float)i * 0.0173f, v = 5.0f - (float)i * 0.0291f;
            sh4der_uvwarp_offset(&t, u, v, 0.31f, -0.17f, 2.5f, 0.9f, &du, &dv);
            sh4der_uvwarp_offset_p(&p, u, v, 0.9f, &pu, &pv);
            if (fabsf(du - pu) > worst) worst = fabsf(du - pu);
            if (fabsf(dv - pv) > worst) worst = fabsf(dv - pv);
        }
        if (worst > 1.2e-3f) { fprintf(stderr, "size %u x %u: prepared vs plain %g\n", w, h, (double)worst); return fail("odd sizes: prepared == plain"); }
    }
    {                                                              /* the unchecked entry equals the gated one on in-domain input */
        sh4der_uvwarp_t t;
        sh4der_uvwarp_prep_t p;
        float worstn = 0.0f, qu, qv;

        for (k = 0; k < 2 * 255 * 255; k++) tab[k] = (uint8_t)(k * 37u + 11u);
        t.rg = tab; t.w = 255; t.h = 255;
        sh4der_uvwarp_prepare(&p, &t, 0.31f, -0.17f, 2.5f);
        for (i = 0; i < 400; i++) {
            const float u = -3.0f + (float)i * 0.0173f, v = 5.0f - (float)i * 0.0291f, w = (float)(i % 256) * (1.0f / 255.0f);
            sh4der_uvwarp_offset_p(&p, u, v, w, &pu, &pv);
            sh4der_uvwarp_offset_pn(&p, u, v, w, &qu, &qv);
            if (fabsf(pu - qu) > worstn) worstn = fabsf(pu - qu);
            if (fabsf(pv - qv) > worstn) worstn = fabsf(pv - qv);
        }
        if (worstn != 0.0f) return fail("offset_pn == offset_p on in-domain input");
    }
    {
        const sh4der_uvwarp_t t = { TAB, 4, 2 };
        sh4der_uvwarp_offset(&t, 0.375f, 0.25f, 0.0f, 0.0f, 0.0f, -2.0f, &du, &dv);
        if (du != 0.0f || dv != 0.0f) return fail("a negative weight is saturated to 0");
        sh4der_uvwarp_offset(&t, 0.375f, 0.25f, 0.0f, 0.0f, 0.0f, 3.0f, &du, &dv);
        if (!near1(du, 64.0f / 255.0f)) return fail("a weight over 1 is saturated to 1");
    }
    return 0;
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    if (texels() || wrap() || offsets() || guards() || prepared() || extremes() || sizes()) return 1;
    printf("uvwarp_test: PASS\n");
    return 0;
}
