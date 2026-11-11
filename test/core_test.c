/* SH4DER-PVR

   test/core_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the shared parts: the falloff matches sh4dow-dc's feather, the
   core answers before any root (d2 = 0, a zero core), the SH4ZAM steps match their
   plain C mirror (fraction of a negative, lerp, divide, saturate), and the breath
   tracker and glint refuse bad input.
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_core.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0002f
#endif
#define TOL SH4DER_TEST_TOL

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int near(float a, float b, float tol) {
    return fabsf(a - b) <= tol;
}

/* Between the core and the outer radius sh4der_falloff_ is SH4DOW-DC's feather (sh4dow_feather_, its clamped form); inside the core both are 1. */
static int feather_is_sh4dow(void) {
    static const float edge[3][2] = { { 0.012f, 0.035f }, { 0.010f, 0.028f }, { 0.0f, 0.055f } };
    int e, k;

    for (e = 0; e < 3; e++) {
        const float inner = edge[e][0], outer = edge[e][1], inv = 1.0f / (outer - inner);

        for (k = 0; k < 400; k++) {
            const float d = outer * (float)k / 400.0f, d2 = d * d;
            const float got = sh4der_falloff_(d2, inner, inv);

            if (!(got >= -TOL && got <= 1.0f + TOL))
                return fail("falloff NaN or out of range");
            if (!near(got, sh4dow_feather_(d2, inner, inv), TOL))
                return fail("falloff is not sh4dow-dc's feather");
        }
    }
    if (sh4der_falloff_(0.0f, 0.0f, 1.0f / 0.055f) != 1.0f || sh4der_falloff_(0.0f, 0.01f, 1.0f / 0.045f) != 1.0f)
        return fail("the core before the root");
    return 0;
}

/* Checks the SH4ZAM steps against their plain C results: fraction, lerp, divide, clamps, lengths and bit tests. */
static int steps(void) {
    if (!near(sh4der_frac_(-0.25f), 0.75f, 1e-6f) || !near(sh4der_frac_(2.5f), 0.5f, 1e-6f) || sh4der_frac_(3.0f) != 0.0f)
        return fail("fraction");
    if (!near(sh4der_lerp_(-0.1f, 0.4f, 0.5f), 0.15f, 1e-6f) || !near(sh4der_div_(48.0f, 192.0f), 0.25f, 1e-6f))
        return fail("lerp / divide");
    if (sh4der_sat_(-1.0f) != 0.0f || sh4der_sat_(2.0f) != 1.0f || sh4der_min1_(3.0f) != 1.0f || sh4der_min1_(0.5f) != 0.5f)
        return fail("clamps");
    if (!near(sh4der_len2_(3.0f, 4.0f), 25.0f, 1e-4f) || !near(sh4der_len3_(1.0f, 2.0f, 2.0f), 9.0f, 1e-4f))
        return fail("lengths");
    if (sh4der_finite_(NAN) || sh4der_finite_(INFINITY) || !sh4der_finite_(1.0f) || sh4der_unit_(1.5f) || !sh4der_unit_(0.5f))
        return fail("bit tests");
    return 0;
}

/* Checks that a NaN breath is not remembered and that glint refuses a negative, NaN or far depth. */
static int refusals(void) {
    sh4der_breath_t b = { { 0 }, 0, 0 };

    if (sh4der_breath_push(&b, NAN) != 0.5f || b.count != 0)
        return fail("a NaN breath is not remembered");
    if (sh4der_glint(-1.0f, 1.0f) != 0.0f || sh4der_glint(NAN, 1.0f) != 0.0f || sh4der_glint(SH4DER_GLINT_FAR + 1.0f, 1.0f) != 0.0f)
        return fail("glint refusals");
    if (!near(sh4der_glint(1.0f, 1.0f), SH4DER_GLINT_BRIGHTNESS, TOL))
        return fail("glint near");
    return 0;
}

int main(void) {
    if (feather_is_sh4dow() || steps() || refusals())
        return 1;
    printf("core_test: ok\n");
    return 0;
}
