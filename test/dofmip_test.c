/* SH4DER-PVR: host test of the depth-driven focus blur helper (include/sh4der_dofmip.h). Arithmetic only: no PVR.

   test/dofmip_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Compile: cc -std=c99 -Wall -Wextra -Werror -DSH4DER_CORE_NO_SH4ZAM -Iinclude
            test/dofmip_test.c -lm (make test; make test-shz builds the same with SH4ZAM) */

#include <math.h>
#include <stdio.h>

#include "sh4der_dofmip.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)
#define NEAR(a, b, e) (fabsf((a) - (b)) <= (e))

#define POW_TOL 2.0e-3f                     /* levels, at a strength of 2: the power's own error (its note in the header) */

int main(void) {
    sh4der_dofmip_t d, bad;
    const float eye[3] = { 1.0f, 2.0f, 3.0f }, fwd[3] = { 0.0f, 0.0f, 1.0f }, p[3] = { 5.0f, -1.0f, 13.0f };
    float z, prev, t;
    unsigned c;

    /* a good focus: sharp inside the zone, blurred outside, nothing sharper than none */
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 1.0f, 2.0f, 2.0f, 1.0f, 1.0f));
    CHECK(sh4der_dofmip_levels(&d, 4.0f) == 0.0f);
    CHECK(sh4der_dofmip_levels(&d, 3.01f) == 0.0f && sh4der_dofmip_levels(&d, 4.99f) == 0.0f);
    CHECK(sh4der_dofmip_levels(&d, 2.0f) > 0.0f && sh4der_dofmip_levels(&d, 9.0f) > 0.0f);
    CHECK(sh4der_dofmip_code(&d, 4.0f) == SH4DER_DOFMIP_CODE_NONE);

    /* it grows away from the zone on each side, to the strength, never past it */
    prev = 0.0f;
    for (z = 5.0f; z < 400.0f; z *= 1.3f) {
        const float l = sh4der_dofmip_levels(&d, z);

        CHECK(l >= prev - 1.0e-6f && l <= 2.0f + 1.0e-6f);
        prev = l;
    }
    prev = 0.0f;
    for (z = 3.0f; z > 0.2f; z *= 0.9f) {
        const float l = sh4der_dofmip_levels(&d, z);

        CHECK(l >= prev - 1.0e-6f && l <= 2.0f + 1.0e-6f);
        prev = l;
    }
    CHECK(NEAR(sh4der_dofmip_levels(&d, 1.0e5f), 1.6f, 1.0e-3f));         /* far end 5 m: 1/5 x focus 4 = 0.8 of full */

    /* the lens law: with no sharp range and reach 1, infinity is full blur and half the focus distance is full too */
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 2.0f, 2.0f, 1.0f, 1.0f));
    CHECK(NEAR(sh4der_dofmip_levels(&d, 1.0e6f), 2.0f, 1.0e-3f));
    CHECK(NEAR(sh4der_dofmip_levels(&d, 2.0f), 2.0f, 1.0e-4f));
    CHECK(NEAR(sh4der_dofmip_levels(&d, 8.0f), 1.0f, 1.0e-4f));           /* 1/8 vs 1/4: half of the way to full */
    CHECK(NEAR(sh4der_dofmip_levels(&d, 4.0f / 1.5f), 1.0f, 1.0e-4f));    /* 1/z = 1.5/4: half of the way on the near side */

    /* a bigger reach is a gentler pull; a bad reach takes the default */
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 2.0f, 2.0f, 1.0f, 2.0f));
    CHECK(NEAR(sh4der_dofmip_levels(&d, 1.0e6f), 1.0f, 1.0e-3f));
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 2.0f, 2.0f, 1.0f, 0.0f));
    CHECK(NEAR(sh4der_dofmip_levels(&d, 1.0e6f), 2.0f, 1.0e-3f));

    /* separate strengths: each side keeps its own, the other side is not touched */
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 0.5f, 2.0f, 1.0f, 1.0f));
    CHECK(NEAR(sh4der_dofmip_levels(&d, 2.0f), 0.5f, 1.0e-4f));           /* near, full */
    CHECK(NEAR(sh4der_dofmip_levels(&d, 1.0e6f), 2.0f, 1.0e-3f));         /* far, full */
    CHECK(NEAR(sh4der_dofmip_levels(&d, 8.0f), 1.0f, 1.0e-4f));           /* far, half */
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 0.0f, 2.0f, 1.0f, 1.0f));
    CHECK(sh4der_dofmip_levels(&d, 1.0f) == 0.0f && sh4der_dofmip_levels(&d, 100.0f) > 1.9f);   /* no near blur at all */
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 2.0f, 0.0f, 1.0f, 1.0f));
    CHECK(sh4der_dofmip_levels(&d, 1.0e6f) == 0.0f && sh4der_dofmip_levels(&d, 1.0f) > 1.9f);   /* no far blur at all */

    /* the curve is a real power of the share: the far side at 1/z = half of full, for a spread of exponents */
    {
        static const float pw[] = { 0.1f, 0.5f, 1.0f, 2.0f, 3.5f, 5.0f, 12.0f, 50.0f };
        unsigned i;

        for (i = 0; i < sizeof pw / sizeof pw[0]; i++) {
            CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 2.0f, 2.0f, pw[i], 1.0f));
            CHECK(NEAR(sh4der_dofmip_levels(&d, 8.0f), 2.0f * powf(0.5f, pw[i]), POW_TOL));
            CHECK(NEAR(sh4der_dofmip_levels(&d, 16.0f), 2.0f * powf(0.75f, pw[i]), POW_TOL));
            CHECK(NEAR(sh4der_dofmip_levels(&d, 1.0e6f), 2.0f, 3.0e-3f));     /* full is full whatever the power */
            CHECK(sh4der_dofmip_levels(&d, 4.0f) == 0.0f);                    /* the zone stays sharp */
            prev = 0.0f;
            for (z = 4.1f; z < 400.0f; z *= 1.2f) {                           /* still monotone and held to the strength */
                t = sh4der_dofmip_levels(&d, z);
                CHECK(t >= prev - 1.0e-3f && t <= 2.0f + 1.0e-3f);
                prev = t;
            }
            /* the code a polygon takes (4 compares) is the code its levels round to, but within the power's error of a step */
            for (z = 1.0f; z < 1.0e4f; z *= 1.07f) {
                const float lv = sh4der_dofmip_levels(&d, z);
                const unsigned a = sh4der_dofmip_code(&d, z), b = sh4der_dofmip_code_of(lv);
                const float edge = fmodf(lv * 4.0f + 0.5f, 1.0f);       /* how far into its code step the levels sit */

                CHECK(a == b || edge < 0.03f || edge > 0.97f);
                CHECK(a >= SH4DER_DOFMIP_CODE_NONE && a <= SH4DER_DOFMIP_CODE_MAX);
            }
        }
    }
    /* a power above 1 holds the blur back near the zone, under 1 gives it early */
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 2.0f, 2.0f, 4.0f, 1.0f));
    t = sh4der_dofmip_levels(&d, 8.0f);
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 2.0f, 2.0f, 0.25f, 1.0f));
    CHECK(sh4der_dofmip_levels(&d, 8.0f) > 1.0f && t < 1.0f);
    /* a curve at 0 or under takes 1.0; one past the ends is held */
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 2.0f, 2.0f, 0.0f, 1.0f) && NEAR(d.curve, SH4DER_DOFMIP_CURVE, 0.0f));
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 2.0f, 2.0f, -3.0f, 1.0f) && NEAR(d.curve, SH4DER_DOFMIP_CURVE, 0.0f));
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 2.0f, 2.0f, 0.001f, 1.0f) && NEAR(d.curve, SH4DER_DOFMIP_CURVE_MIN, 0.0f));
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 2.0f, 2.0f, 9000.0f, 1.0f) && NEAR(d.curve, SH4DER_DOFMIP_CURVE_MAX, 0.0f));

    /* the strengths are held to the field's most; the near end is held at NEAR_MIN */
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 99.0f, 99.0f, 1.0f, 1.0f));
    CHECK(sh4der_dofmip_code(&d, 1.0e6f) == SH4DER_DOFMIP_CODE_MAX && sh4der_dofmip_code(&d, 1.0f) == SH4DER_DOFMIP_CODE_MAX);
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, -3.0f, -3.0f, 1.0f, 1.0f));
    CHECK(sh4der_dofmip_levels(&d, 1.0e6f) == 0.0f && sh4der_dofmip_levels(&d, 1.0f) == 0.0f);
    CHECK(sh4der_dofmip_prepare(&d, 1.0f, 50.0f, 1.0f, 1.0f, 1.0f, 1.0f));
    CHECK(sh4der_dofmip_levels(&d, 0.5f) == 0.0f);                        /* near end at 0.1 m: 0.5 m is inside */
    CHECK(sh4der_dofmip_levels(&d, 0.05f) > 0.0f);

    /* refused numbers blur nothing and say so */
    CHECK(!sh4der_dofmip_prepare(&bad, 0.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f) && sh4der_dofmip_levels(&bad, 9.0f) == 0.0f);
    CHECK(!sh4der_dofmip_prepare(&bad, 4.0f, -1.0f, 1.0f, 1.0f, 1.0f, 1.0f));
    CHECK(!sh4der_dofmip_prepare(&bad, NAN, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f));
    CHECK(!sh4der_dofmip_prepare(&bad, 4.0f, 1.0f, NAN, 1.0f, 1.0f, 1.0f));
    CHECK(!sh4der_dofmip_prepare(&bad, 4.0f, 1.0f, 1.0f, NAN, 1.0f, 1.0f));
    CHECK(!sh4der_dofmip_prepare(&bad, 4.0f, 1.0f, 1.0f, 1.0f, NAN, 1.0f));
    CHECK(!sh4der_dofmip_prepare(&bad, 4.0f, 1.0f, 1.0f, 1.0f, 1.0f, NAN));
    CHECK(!sh4der_dofmip_prepare(&bad, INFINITY, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f));
    CHECK(!sh4der_dofmip_prepare(NULL, 4.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f));
    CHECK(sh4der_dofmip_levels(NULL, 9.0f) == 0.0f);
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 1.0f, 2.0f, 2.0f, 1.0f, 1.0f));
    CHECK(sh4der_dofmip_levels(&d, 0.0f) == 0.0f && sh4der_dofmip_levels(&d, -5.0f) == 0.0f && sh4der_dofmip_levels(&d, NAN) == 0.0f);

    /* nothing is kept between frames: a jump in the tracks is the next frame's whole setting */
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 2.0f, 2.0f, 1.0f, 1.0f));
    t = sh4der_dofmip_levels(&d, 10.0f);
    CHECK(sh4der_dofmip_prepare(&d, 20.0f, 0.0f, 1.0f, 1.0f, 3.0f, 1.0f));
    CHECK(sh4der_dofmip_levels(&d, 20.0f) == 0.0f && sh4der_dofmip_levels(&d, 10.0f) != t);
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 2.0f, 2.0f, 1.0f, 1.0f));
    CHECK(sh4der_dofmip_levels(&d, 10.0f) == t);                                                 /* the same tracks, the same answer */

    /* the power, against libm: the strong exponents and the shares near 1 where SH4ZAM's shz_powf is far out */
    {
        static const float xs[] = { 1.0e-6f, 0.001f, 0.01f, 0.1f, 0.25f, 0.5f, 0.75f, 0.9f, 0.99f, 0.9999f, 0.999996f };
        static const float ps[] = { 0.02f, 0.1f, 0.5f, 1.0f, 2.0f, 3.7f, 10.0f, 50.0f };
        unsigned i, j;

        for (i = 0; i < sizeof xs / sizeof xs[0]; i++)
            for (j = 0; j < sizeof ps / sizeof ps[0]; j++) {
                const float got = sh4der_dofmip_pow_(xs[i], ps[j]);

                CHECK(NEAR(got, powf(xs[i], ps[j]), 1.0e-3f));
                CHECK(got >= 0.0f && got <= 1.0f);
            }
        CHECK(sh4der_dofmip_pow_(0.0f, 2.0f) == 0.0f && sh4der_dofmip_pow_(0.0f, 0.0f) == 1.0f && sh4der_dofmip_pow_(1.0f, 7.0f) == 1.0f);
        CHECK(sh4der_dofmip_pow_(1.5f, 7.0f) == 1.0f && sh4der_dofmip_pow_(0.5f, 0.0f) == 1.0f);
        CHECK(NEAR(sh4der_dofmip_pow_(0.99f, 50.0f), 0.605f, 1.0e-3f));            /* shz_powf says 3.6 */
    }
    /* the steps: rising, in 0 .. 1, a step is never when its levels are over the strength */
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 2.0f, 0.3f, 2.5f, 1.0f));
    for (c = 1; c < SH4DER_DOFMIP_STEPS; c++) CHECK(d.step_near[c] >= d.step_near[c - 1u] && (d.step_near[c] <= 1.0f || d.step_near[c] == 2.0f));
    CHECK(d.step_far[0] <= 1.0f && d.step_far[1] == 2.0f && d.step_far[10] == 2.0f);                         /* 0.3 levels: code 5 only */
    CHECK(sh4der_dofmip_code(&d, 1.0e6f) == 5u && sh4der_dofmip_code(&d, 1.0f) == 12u);                    /* 2 levels before the zone: 4 + 8 */
    CHECK(sh4der_dofmip_prepare(&d, 4.0f, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f) && sh4der_dofmip_code(&d, 1.0e6f) == 4u && sh4der_dofmip_code(&d, 0.5f) == 4u);

    /* the codes: quarters past none, never under 4, never over 15; every bucket is hit once, in order */
    CHECK(sh4der_dofmip_code_of(0.0f) == 4u && sh4der_dofmip_code_of(-1.0f) == 4u && sh4der_dofmip_code_of(NAN) == 4u);
    CHECK(sh4der_dofmip_code_of(0.25f) == 5u && sh4der_dofmip_code_of(0.5f) == 6u && sh4der_dofmip_code_of(1.0f) == 8u);
    CHECK(sh4der_dofmip_code_of(2.75f) == 15u && sh4der_dofmip_code_of(50.0f) == 15u);
    for (c = SH4DER_DOFMIP_CODE_NONE; c <= SH4DER_DOFMIP_CODE_MAX; c++) CHECK(sh4der_dofmip_bucket((uint8_t)c) == c - SH4DER_DOFMIP_CODE_NONE);
    CHECK(sh4der_dofmip_bucket(1u) == 0u && sh4der_dofmip_bucket(200u) == SH4DER_DOFMIP_CODES - 1u);

    /* two biases on one polygon: a floor's sharper code and this one's blurrier one */
    CHECK(sh4der_dofmip_combine(4u, 4u) == 4u && sh4der_dofmip_combine(2u, 4u) == 2u && sh4der_dofmip_combine(4u, 9u) == 9u);
    CHECK(sh4der_dofmip_combine(2u, 9u) == 7u && sh4der_dofmip_combine(1u, 1u) == 1u && sh4der_dofmip_combine(15u, 15u) == 15u);

    /* the depth: the dot of the offset with the forward */
    CHECK(NEAR(sh4der_dofmip_depth(eye, fwd, p), 10.0f, 1.0e-5f));

    if (fails) { printf("dofmip_test: %d FAILED\n", fails); return 1; }
    printf("dofmip_test: ok\n");
    return 0;
}
