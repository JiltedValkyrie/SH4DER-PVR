/* SH4DER-PVR: linear ramp test

   test/ramp_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of the distance ramp: the one-call form and the prepared form agree at many
   points; both ends are exact (0 and 1, rising; 1 and 0, falling); the middle is linear
   (a half at the midpoint); a length of 0 or less, and a prepared ramp with to <= from, is a
   step at `from`; the outputs stay in 0 .. 1 far outside the range; the prepared ramp's
   reciprocal is taken once (its fields are the start and 1 / length).
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_ramp.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.00002f
#endif

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR_(a, b, e) (fabsf((a) - (b)) <= (e))

static void ends_and_middle(void) {
    const float tol = 200.0f * SH4DER_TEST_TOL;
    sh4der_ramp_t r;

    /* the one-call form: 0 up to 40, 1 from 50 on, linear between */
    CHECK(sh4der_ramp_in(40.0f, 10.0f, 0.0f) == 0.0f);
    CHECK(sh4der_ramp_in(40.0f, 10.0f, 40.0f) == 0.0f);
    CHECK(sh4der_ramp_in(40.0f, 10.0f, 50.0f) == 1.0f);
    CHECK(sh4der_ramp_in(40.0f, 10.0f, 90.0f) == 1.0f);
    CHECK(NEAR_(sh4der_ramp_in(40.0f, 10.0f, 45.0f), 0.5f, tol));
    CHECK(NEAR_(sh4der_ramp_in(40.0f, 10.0f, 42.5f), 0.25f, tol));

    /* the prepared form, rising and falling: the same ends and middle */
    sh4der_ramp_make(&r, 40.0f, 50.0f);
    CHECK(sh4der_ramp_up(&r, 0.0f) == 0.0f && sh4der_ramp_up(&r, 40.0f) == 0.0f);
    CHECK(sh4der_ramp_up(&r, 50.0f) == 1.0f && sh4der_ramp_up(&r, 80.0f) == 1.0f);
    CHECK(NEAR_(sh4der_ramp_up(&r, 45.0f), 0.5f, tol));
    CHECK(sh4der_ramp_down(&r, 0.0f) == 1.0f && sh4der_ramp_down(&r, 40.0f) == 1.0f);
    CHECK(sh4der_ramp_down(&r, 50.0f) == 0.0f && sh4der_ramp_down(&r, 80.0f) == 0.0f);
    CHECK(NEAR_(sh4der_ramp_down(&r, 45.0f), 0.5f, tol));
    CHECK(NEAR_(sh4der_ramp_down(&r, 42.5f), 0.75f, tol));
}

static void forms_agree(void) {
    sh4der_ramp_t r;
    unsigned k;

    sh4der_ramp_make(&r, 5.0f, 25.0f);
    for (k = 0; k <= 400u; k++) {
        const float d = 0.25f * (float)k;                          /* 0 .. 100 m in quarter metres */
        CHECK(NEAR_(sh4der_ramp_up(&r, d), sh4der_ramp_in(5.0f, 20.0f, d), 200.0f * SH4DER_TEST_TOL));
        CHECK(NEAR_(sh4der_ramp_down(&r, d) + sh4der_ramp_in(5.0f, 20.0f, d), 1.0f, 200.0f * SH4DER_TEST_TOL));
    }
    CHECK(NEAR_(r.inv, 1.0f / 20.0f, 200.0f * SH4DER_TEST_TOL));    /* the reciprocal, taken once */
    CHECK(r.from == 5.0f);
}

static void steps(void) {
    sh4der_ramp_t r;

    CHECK(sh4der_ramp_in(5.0f, 0.0f, 4.9f) == 0.0f && sh4der_ramp_in(5.0f, 0.0f, 5.0f) == 1.0f);
    CHECK(sh4der_ramp_in(5.0f, -2.0f, 4.9f) == 0.0f && sh4der_ramp_in(5.0f, -2.0f, 5.0f) == 1.0f);
    sh4der_ramp_make(&r, 5.0f, 5.0f);                           /* to == from: a step */
    CHECK(r.inv == 0.0f);
    CHECK(sh4der_ramp_up(&r, 4.9f) == 0.0f && sh4der_ramp_up(&r, 5.0f) == 1.0f);
    CHECK(sh4der_ramp_down(&r, 4.9f) == 1.0f && sh4der_ramp_down(&r, 5.0f) == 0.0f);
    sh4der_ramp_make(&r, 5.0f, 1.0f);                           /* to < from: a step too */
    CHECK(sh4der_ramp_up(&r, 4.9f) == 0.0f && sh4der_ramp_up(&r, 5.0f) == 1.0f);
}

static void clamped(void) {
    sh4der_ramp_t r;
    const float far[] = { -1e6f, -10.0f, 0.0f, 1e6f };
    unsigned k;

    sh4der_ramp_make(&r, 10.0f, 20.0f);
    for (k = 0; k < sizeof(far) / sizeof(far[0]); k++) {
        const float a = sh4der_ramp_in(10.0f, 10.0f, far[k]);
        const float b = sh4der_ramp_up(&r, far[k]);
        CHECK(a >= 0.0f && a <= 1.0f && b >= 0.0f && b <= 1.0f);
        CHECK(sh4der_ramp_down(&r, far[k]) >= 0.0f && sh4der_ramp_down(&r, far[k]) <= 1.0f);
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    ends_and_middle();
    forms_agree();
    steps();
    clamped();
    if (fails) { fprintf(stderr, "ramp_test: %d FAILED\n", fails); return 1; }
    puts("ramp_test: ok");
    return 0;
}
