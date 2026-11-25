/* SH4ZAM-DCE tests: shz_dce_atan2.h on the host (SH4ZAM's software back end)

   atan2_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Pinned to SH4ZAM master ff39547 (2026-11-22): every "SH4ZAM ff39547 claim" below is a promise of that commit's shz_trig.h, so a
   later SH4ZAM change that breaks one fails here by name. What is checked:
   - the wrapper: shz_dce_atan2f gives shz_atan2f bit for bit (each from its own function that is never inlined)
   - SHZ_DCE_ATAN2_TERMS follows SHZ_ATAN2F_TERMS, and the default is 5
   - the polynomial of the default form, in double: r P(r r) against atan(r) on [-1, 1], inside 1.26e-5 (the fit SHZ_ATAN2F_ERROR_MAX
     is built on; the coefficients are SH4ZAM's, copied into this test only to check that number)
   - SH4ZAM's routine against the true angle (atan2 in double) inside SHZ_ATAN2F_ERROR_MAX over four million points: directions
     spread evenly round the circle, random points, points next to both axes, lengths 2e-18 to 5e17
   - its special cases: two zeros of every sign give a zero, the sign of y (a zero y counts as positive), the axes
*/

#include <sh4zam_dce/shz_dce_atan2.h>

#include "shz_dce_test.h"

#define PI_D 3.14159265358979323846
#define NOINLINE __attribute__((noinline))

static const double coef5[5] = { 0.999855518, -0.330125183, 0.179388285, -0.0839662105, 0.0202457625 };
static volatile float vx, vy;   /* run-time values: constant arguments take the compile-time path */

static NOINLINE float d_atan2f(float y, float x) { return shz_dce_atan2f(y, x); }
static NOINLINE float s_atan2f(float y, float x) { return shz_atan2f(y, x); }

static double angle_err(float got, double want) {
    double e = fabs((double)got - want);
    if (e > PI_D) e = fabs(e - 2.0 * PI_D);
    return e;
}

int main(void) {
    double worst = 0.0, wfit = 0.0;
    int i, k;

    T_CHECK(SHZ_DCE_ATAN2_TERMS == SHZ_ATAN2F_TERMS, "SHZ_DCE_ATAN2_TERMS follows SHZ_ATAN2F_TERMS");
    T_CHECK(SHZ_ATAN2F_TERMS == 5, "SH4ZAM ff39547 claim: the default number of terms is 5");
    T_CHECK(SHZ_ATAN2F_ERROR_MAX == 1.4e-5, "SH4ZAM ff39547 claim: SHZ_ATAN2F_ERROR_MAX is 1.4e-5 for 5 terms");
    for (i = 0; i <= 1000000; i++) {   /* the fit, in exact arithmetic */
        const double r = (double)i / 1000000.0, u = r * r;
        double p = 0.0;
        for (k = 4; k >= 0; k--) p = p * u + coef5[k];
        if (fabs(r * p - atan(r)) > wfit) wfit = fabs(r * p - atan(r));
    }
    T_CHECK(wfit <= 1.26e-5, "the 5-term fit's error is %.3g", wfit);

    for (i = 0; i < 4000000; i++) {
        float x, y, d, s;
        double want, e;
        switch (i % 5) {
        case 0: { const double a = (i / 5) * (2.0 * PI_D / 800000.0), m = 0.01 + (double)(t_rand() % 1000u); x = (float)(m * cos(a)); y = (float)(m * sin(a)); break; }
        case 1: x = t_frand(-1.0f, 1.0f); y = t_frand(-1.0f, 1.0f); break;
        case 2: x = t_frand(-1.0f, 1.0f) * 1e-4f; y = t_frand(-50.0f, 50.0f); break;
        case 3: x = t_frand(-50.0f, 50.0f); y = t_frand(-1.0f, 1.0f) * 1e-4f; break;
        default: { const double a = t_frand(-3.14159f, 3.14159f); const float m = (i & 8) ? 2e-18f : 5e17f; x = (float)cos(a) * m; y = (float)sin(a) * m; break; }
        }
        vx = x; vy = y; x = vx; y = vy;
        d = d_atan2f(y, x); s = s_atan2f(y, x);
        T_CHECK(t_bits(d) == t_bits(s), "wrapper: atan2(%a, %a): %08x, SH4ZAM's %08x", (double)y, (double)x, (unsigned)t_bits(d), (unsigned)t_bits(s));
        if (x == 0.0f && y == 0.0f) continue;
        want = atan2((double)y, (double)x);
        e = angle_err(s, want);
        T_CHECK(e <= SHZ_ATAN2F_ERROR_MAX, "SH4ZAM ff39547 claim (SHZ_ATAN2F_ERROR_MAX): atan2(%.9g, %.9g) = %.9g, true %.9g, off by %.3g", (double)y, (double)x, (double)s, want, e);
        T_CHECK((t_bits(s) >> 31) == (uint32_t)(y < 0.0f), "SH4ZAM ff39547 claim: atan2(%.9g, %.9g) = %.9g has the sign of y", (double)y, (double)x, (double)s);
        if (e > worst) worst = e;
    }
    {   /* special cases */
        static const float sy[8] = { 0.0f, -0.0f, 0.0f, -0.0f, 3.0f, -3.0f, 0.0f, -0.0f };
        static const float sx[8] = { 0.0f, 0.0f, -0.0f, -0.0f, 0.0f, 0.0f, -2.0f, -2.0f };
        for (k = 0; k < 8; k++) {
            float got;
            vy = sy[k]; vx = sx[k];
            got = s_atan2f(vy, vx);
            T_CHECK(t_bits(d_atan2f(vy, vx)) == t_bits(got), "wrapper: special case %d", k);
            if (k < 4) T_CHECK((t_bits(got) << 1) == 0u, "SH4ZAM ff39547 claim: atan2 of two zeros is %08x", (unsigned)t_bits(got));
            else if (k < 6) T_CHECK(fabs(fabs((double)got) - PI_D / 2) <= 6e-7 && (got < 0.0f) == (sy[k] < 0.0f), "SH4ZAM ff39547 claim: the y axis gives %.9g", (double)got);
            else T_CHECK(fabs(fabs((double)got) - PI_D) <= 6e-7 && got > 0.0f, "SH4ZAM ff39547 claim: the negative x axis gives %.9g (a zero y counts as positive)", (double)got);
        }
        vy = 0.0f; vx = 5.0f;
        T_CHECK(fabs((double)s_atan2f(vy, vx)) <= 4e-7, "SH4ZAM ff39547 claim: the positive x axis gives %.9g", (double)s_atan2f(vy, vx));
    }
    T_CHECK(fabs(shz_dce_atan2f(1.0f, 1.0f) - PI_D / 4) <= 1.2e-7, "constant arguments take the compile-time path");
    printf("wrapper: shz_dce_atan2f == shz_atan2f bit for bit, 4000000 points; SH4ZAM ff39547's routine (%d terms): worst error %.3g rad, bound %.3g; fit %.3g\n",
           SHZ_ATAN2F_TERMS, worst, (double)SHZ_ATAN2F_ERROR_MAX, wfit);
    return t_done("atan2_test");
}
