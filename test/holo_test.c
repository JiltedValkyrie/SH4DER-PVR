/* SH4DER-PVR: the holo effect

   test/holo_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the holo effect: weights, table entries and the breath tracker at
   fixed points (pinned from an offline reference script), the table order, the fade,
   bad params and the loop.
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_holo.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0002f
#endif
#define TOL SH4DER_TEST_TOL

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

/* Checks the default weights and table levels at fixed points, against pinned values from core_weight and core_ref
   in an offline reference script. */
static int shipped(void) {
    static const struct { float d; int want; } wpin[] = {{0.0f, 63}, {0.005f, 63}, {0.020f, 55}, {0.030f, 37}, {0.040f, 16}, {0.054f, 0}, {0.08f, 0}};
    static const struct { float breath; uint8_t w; uint32_t want; } tpin[] = {
        {0.0f, 0, 142}, {0.0f, 32, 173}, {0.0f, 63, 203}, {0.25f, 0, 146}, {0.25f, 32, 183}, {0.25f, 63, 218},
        {0.5f, 0, 150}, {0.5f, 32, 192}, {0.5f, 63, 232}, {1.0f, 0, 158}, {1.0f, 32, 210}, {1.0f, 63, 255},
    };
    const unsigned slack = TOL > 0.0003f ? 1u : 0u;   /* the SH4ZAM path's FSRRA may round a weight the other way */
    sh4der_holo_params_t p;
    sh4der_holo_frame_t f;
    float xyz[8][4];
    uint8_t w[8];
    unsigned i;

    sh4der_holo_params_default(&p);
    if (!sh4der_holo_params_valid(&p))
        return fail("default params invalid");
    for (i = 0; i < sizeof wpin / sizeof wpin[0]; i++) {
        xyz[i][0] = p.center[0] + wpin[i].d;
        xyz[i][1] = p.center[1];
        xyz[i][2] = p.center[2];
        xyz[i][3] = 99.0f;
    }
    sh4der_holo_weights(&p, xyz[0], sizeof xyz[0], w, sizeof wpin / sizeof wpin[0]);
    for (i = 0; i < sizeof wpin / sizeof wpin[0]; i++)
        if (w[i] + slack < (unsigned)wpin[i].want || w[i] > wpin[i].want + slack) {
            fprintf(stderr, "d %g: weight %u, want %d\n", wpin[i].d, w[i], wpin[i].want);
            return fail("weight");
        }
    for (i = 0; i < sizeof tpin / sizeof tpin[0]; i++) {
        uint32_t c, g;

        if (!sh4der_holo_prepare(&f, &p, tpin[i].breath, 1.0f))
            return fail("prepare");
        c = sh4der_holo_argb(&f, tpin[i].w);
        g = c & 255;
        if (c >> 24 != 255 || (c >> 8 & 255) != g || (c >> 16 & 255) != g || g != tpin[i].want) {
            fprintf(stderr, "breath %g weight %u: %08lx, want level %lu\n", tpin[i].breath, tpin[i].w, (unsigned long)c,
                    (unsigned long)tpin[i].want);
            return fail("table");
        }
    }
    if (sh4der_holo_argb(&f, 200) != f.argb[SH4DER_HOLO_LEVELS - 1])
        return fail("weight past the table");
    /* breath clamps; NaN is the middle */
    sh4der_holo_prepare(&f, &p, 3.0f, 1.0f);
    if (f.breath != 1.0f)
        return fail("breath clamp");
    sh4der_holo_prepare(&f, &p, NAN, 1.0f);
    if (f.breath != 0.5f || (sh4der_holo_argb(&f, 0) & 255) != 150)
        return fail("NaN breath");
    return 0;
}

/* Checks the breath tracker against pinned values (from the reference Breath.push) over a breathing chest
   that leans forward at frame 121 and stays there. */
static int tracker(void) {
    static const struct { unsigned frame; float want; } pin[] = {
        {0, 0.5f}, {1, 0.513415f}, {10, 0.623613f}, {45, 0.307809f}, {89, 0.473171f}, {100, 0.747226f}, {121, 1.0f}, {130, 0.916441f},
        {199, 1.0f},
    };
    static sh4der_breath_t b;   /* zeroed */
    unsigned i, k = 0;
    float c[3] = {0.3f, 2.0f, -1.0f}, o[3] = {0.1f, 1.0f, -1.5f}, fw[3] = {0.0f, 0.6f, 0.8f};

    if (fabsf(sh4der_breath_raw(c, o, fw) - 1.0f) > 1e-6f)
        return fail("breath raw");
    for (i = 0; i < 200; i++) {
        float raw = 0.0005f * sinf((float)i * 6.28318530718f / 90.0f) + (i > 120 ? 0.002f : 0.0f);
        float got = sh4der_breath_push(&b, raw);

        if (got < 0.0f || got > 1.0f)
            return fail("breath outside 0..1");
        if (k < sizeof pin / sizeof pin[0] && pin[k].frame == i) {
            if (fabsf(got - pin[k].want) > 2e-4f + TOL) {
                fprintf(stderr, "frame %u: breath %f, want %f\n", i, got, pin[k].want);
                return fail("tracker");
            }
            k++;
        }
    }
    if (b.count != SH4DER_BREATH_WINDOW)
        return fail("tracker window");
    if (sh4der_breath_push(&b, NAN) != 0.5f || b.count != SH4DER_BREATH_WINDOW)
        return fail("tracker NaN");
    sh4der_breath_reset(&b);
    if (b.count || sh4der_breath_push(&b, 7.0f) != 0.5f)
        return fail("tracker reset");
    b.head = 999;   /* garbage: starts again */
    if (sh4der_breath_push(&b, 1.0f) != 0.5f || b.count != 1)
        return fail("tracker garbage");
    return 0;
}

/* The table never gets darker with more weight (no dark ring) or with more breath. */
static int order(void) {
    sh4der_holo_params_t p;
    sh4der_holo_frame_t f, g;
    unsigned i, k;

    sh4der_holo_params_default(&p);
    for (k = 0; k < 20; k++) {
        sh4der_holo_prepare(&f, &p, (float)k / 20.0f, 1.0f);
        sh4der_holo_prepare(&g, &p, (float)(k + 1) / 20.0f, 1.0f);
        for (i = 0; i < SH4DER_HOLO_LEVELS; i++) {
            if (i && (f.argb[i] & 255) < (f.argb[i - 1] & 255))
                return fail("table order");
            if ((g.argb[i] & 255) < (f.argb[i] & 255))
                return fail("breath order");
        }
    }
    return 0;
}

/* The fade is the alpha only: colours unchanged, alpha rounded, NaN and negative clear, above 1 clamped. */
static int fade(void) {
    sh4der_holo_params_t p;
    sh4der_holo_frame_t f, g;
    unsigned i;

    sh4der_holo_params_default(&p);
    sh4der_holo_prepare(&f, &p, 0.3f, 1.0f);
    sh4der_holo_prepare(&g, &p, 0.3f, 0.5f);
    for (i = 0; i < SH4DER_HOLO_LEVELS; i++)
        if ((g.argb[i] & 0xffffffu) != (f.argb[i] & 0xffffffu) || g.argb[i] >> 24 != 128)
            return fail("fade");
    sh4der_holo_prepare(&g, &p, 0.3f, NAN);
    if (g.argb[5] >> 24 != 0 || g.fade != 0.0f)
        return fail("NaN fade");
    sh4der_holo_prepare(&g, &p, 0.3f, -1.0f);
    if (g.argb[5] >> 24 != 0)
        return fail("negative fade");
    sh4der_holo_prepare(&g, &p, 0.3f, 7.0f);
    if (g.argb[5] >> 24 != 255)
        return fail("fade clamp");
    return 0;
}

/* Bad params: a clear table, zero weights. */
static int bad_params(void) {
    sh4der_holo_params_t p;
    sh4der_holo_frame_t g;
    float xyz[3] = {SH4DER_HOLO_X, SH4DER_HOLO_Y, SH4DER_HOLO_Z};
    uint8_t w = 9;

    sh4der_holo_params_default(&p);
    p.outer = p.inner;
    if (sh4der_holo_params_valid(&p) || sh4der_holo_prepare(&g, &p, 0.5f, 1.0f) || g.argb[0] || g.argb[SH4DER_HOLO_LEVELS - 1])
        return fail("bad params");
    sh4der_holo_weights(&p, xyz, sizeof xyz, &w, 1);
    if (w)
        return fail("bad params weights");
    sh4der_holo_params_default(&p);
    p.body = 1.5f;
    if (sh4der_holo_params_valid(&p) || sh4der_holo_params_valid(NULL))
        return fail("range");
    sh4der_holo_params_default(&p);
    p.center[1] = NAN;
    if (sh4der_holo_params_valid(&p))
        return fail("NaN centre");
    return 0;
}

/* The glint: full within 3, half at 4.75, gone at 6.5 and past, behind the camera and NaN 0, times the fade. */
static int glint(void) {
    static const struct { float depth, fade, want; } pin[] = {
        {0.5f, 1.0f, 0.4f}, {3.0f, 1.0f, 0.4f}, {4.75f, 1.0f, 0.2f}, {6.5f, 1.0f, 0.0f}, {9.0f, 1.0f, 0.0f}, {-1.0f, 1.0f, 0.0f},
        {0.0f, 1.0f, 0.0f}, {2.0f, 0.5f, 0.2f}, {2.0f, 3.0f, 0.4f}, {NAN, 1.0f, 0.0f}, {2.0f, NAN, 0.0f},
    };
    unsigned i;

    for (i = 0; i < sizeof pin / sizeof pin[0]; i++)
        if (fabsf(sh4der_glint(pin[i].depth, pin[i].fade) - pin[i].want) > 1e-6f) {
            fprintf(stderr, "depth %g fade %g: %f, want %f\n", pin[i].depth, pin[i].fade, sh4der_glint(pin[i].depth, pin[i].fade), pin[i].want);
            return fail("glint");
        }
    return 0;
}

/* The loop equals the per-vertex call, at its stride. */
static int loop(void) {
    sh4der_holo_params_t p;
    sh4der_holo_frame_t f;
    uint32_t out[16];
    uint8_t w[8];
    unsigned i;

    sh4der_holo_params_default(&p);
    sh4der_holo_prepare(&f, &p, 0.7f, 1.0f);
    for (i = 0; i < 8; i++)
        w[i] = (uint8_t)(i * 9);
    sh4der_holo_shade(&f, w, out, 2 * sizeof out[0], 8);
    for (i = 0; i < 8; i++)
        if (out[2 * i] != sh4der_holo_argb(&f, w[i]))
            return fail("shade");
    return 0;
}

int main(void) {
    if (shipped() || tracker() || order() || fade() || bad_params() || glint() || loop())
        return 1;
    printf("holo_test: ok\n");
    return 0;
}
