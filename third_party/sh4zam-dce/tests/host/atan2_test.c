/* SH4ZAM-DCE tests: shz_dce_atan2.h on the host (SH4ZAM's software back end)

   atan2_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked:
   - the four polynomials themselves, in double: r P(r r) against atan(r) on [-1, 1], inside the fit errors the header states
   - the rework with 2, 5, 6 and 7 terms against the true angle (atan2 in double), each inside the header's bound, over four
     million points: directions spread evenly round the circle, random points, points next to both axes, lengths 2e-18 to 5e17
   - the special cases: two zeros of every sign, the sign of y (a zero y counts as positive), the axes; the public routine is the 6-term form by default
   - SH4ZAM's routine on the same points: its worst error is printed beside them
*/

#include <sh4zam_dce/shz_dce_atan2.h>

#include "shz_dce_test.h"

#define PI_D 3.14159265358979323846

static const double bound[4] = { 6.2e-3, 1.4e-5, 2.5e-6, 1.0e-6 };
static const double fit[4] = { 6.14e-3, 1.26e-5, 1.81e-6, 2.8e-7 };
static const double coef[4][7] = {
    { 0.967482567, -0.182084382 },
    { 0.999855518, -0.330125183, 0.179388285, -0.0839662105, 0.0202457625 },
    { 0.999975681, -0.332585186, 0.193293691, -0.11578197, 0.0519234873, -0.0114275254 },
    { 0.999995887, -0.333166063, 0.198008001, -0.132061511, 0.0791187882, -0.0331594534, 0.00666252105 },
};
static const int nterms[4] = { 2, 5, 6, 7 };
static volatile float vx, vy;   /* run-time values: constant arguments take the compile-time path */

static double angle_err(float got, double want) {
    double e = fabs((double)got - want);
    if (e > PI_D) e = fabs(e - 2.0 * PI_D);
    return e;
}

static float form(int v, float y, float x) {
    return v == 0 ? shz_dce_atan2f_n_(y, x, 2) : v == 1 ? shz_dce_atan2f_n_(y, x, 5) : v == 2 ? shz_dce_atan2f_n_(y, x, 6) : shz_dce_atan2f_n_(y, x, 7);
}

int main(void) {
    double worst[4] = { 0, 0, 0, 0 }, worst_shz = 0.0;
    int i, v, k;

    for (v = 0; v < 4; v++) {   /* the fits, in exact arithmetic */
        double w = 0.0;
        for (i = 0; i <= 1000000; i++) {
            const double r = (double)i / 1000000.0, u = r * r;
            double p = 0.0;
            for (k = nterms[v] - 1; k >= 0; k--) p = p * u + coef[v][k];
            if (fabs(r * p - atan(r)) > w) w = fabs(r * p - atan(r));
        }
        T_CHECK(w <= fit[v], "the %d-term fit's error is %.3g, the header says %.3g", nterms[v], w, fit[v]);
    }
    T_CHECK(SHZ_DCE_ATAN2_TERMS == 6, "the default number of terms");

    for (i = 0; i < 4000000; i++) {
        float x, y;
        double want;
        switch (i % 5) {
        case 0: { const double a = (i / 5) * (2.0 * PI_D / 800000.0), m = 0.01 + (double)(t_rand() % 1000u); x = (float)(m * cos(a)); y = (float)(m * sin(a)); break; }
        case 1: x = t_frand(-1.0f, 1.0f); y = t_frand(-1.0f, 1.0f); break;
        case 2: x = t_frand(-1.0f, 1.0f) * 1e-4f; y = t_frand(-50.0f, 50.0f); break;
        case 3: x = t_frand(-50.0f, 50.0f); y = t_frand(-1.0f, 1.0f) * 1e-4f; break;
        default: { const double a = t_frand(-3.14159f, 3.14159f); const float m = (i & 8) ? 2e-18f : 5e17f; x = (float)cos(a) * m; y = (float)sin(a) * m; break; }
        }
        vx = x; vy = y; x = vx; y = vy;
        if (x == 0.0f && y == 0.0f) continue;
        want = atan2((double)y, (double)x);
        for (v = 0; v < 4; v++) {
            const float got = form(v, y, x);
            const double e = angle_err(got, want);
            T_CHECK(e <= bound[v], "%d terms: atan2(%.9g, %.9g) = %.9g, true %.9g, off by %.3g", nterms[v], (double)y, (double)x, (double)got, want, e);
            T_CHECK((t_bits(got) >> 31) == (uint32_t)(y < 0.0f), "%d terms: atan2(%.9g, %.9g) = %.9g has not the sign of y", nterms[v], (double)y, (double)x, (double)got);
            if (e > worst[v]) worst[v] = e;
        }
        { const double e = angle_err(shz_atan2f(y, x), want); if (e > worst_shz) worst_shz = e; }
        T_CHECK(t_bits(shz_dce_atan2f(y, x)) == t_bits(form(2, y, x)) || fabs((double)shz_dce_atan2f(y, x) - form(2, y, x)) <= 5e-7, "the public routine is the 6-term form");
    }
    {   /* special cases */
        static const float sy[8] = { 0.0f, -0.0f, 0.0f, -0.0f, 3.0f, -3.0f, 0.0f, -0.0f };
        static const float sx[8] = { 0.0f, 0.0f, -0.0f, -0.0f, 0.0f, 0.0f, -2.0f, -2.0f };
        for (k = 0; k < 8; k++) {
            vy = sy[k]; vx = sx[k];
            for (v = 0; v < 4; v++) {
                const float got = form(v, vy, vx);
                if (k < 4) T_CHECK((t_bits(got) << 1) == 0u, "%d terms: atan2 of two zeros is %08x", nterms[v], (unsigned)t_bits(got));
                else if (k < 6) T_CHECK(fabs(fabs((double)got) - PI_D / 2) <= 6e-7 && (got < 0.0f) == (sy[k] < 0.0f), "%d terms: the y axis gives %.9g", nterms[v], (double)got);
                else T_CHECK(fabs(fabs((double)got) - PI_D) <= 6e-7 && got > 0.0f, "%d terms: the negative x axis gives %.9g (a zero y counts as positive)", nterms[v], (double)got);
            }
        }
        vy = 0.0f; vx = 5.0f;
        for (v = 0; v < 4; v++) T_CHECK(fabs((double)form(v, vy, vx)) <= 4e-7, "%d terms: the positive x axis gives %.9g", nterms[v], (double)form(v, vy, vx));
    }
    T_CHECK(fabs(shz_dce_atan2f(1.0f, 1.0f) - PI_D / 4) <= 1.2e-7, "constant arguments take the compile-time path");
    printf("worst error against the true angle, rad: 2 terms %.3g, 5 terms %.3g, 6 terms %.3g, 7 terms %.3g; SH4ZAM %.3g\n", worst[0], worst[1], worst[2], worst[3], worst_shz);
    T_CHECK(worst_shz > 1000.0 * worst[2], "the header says the default is far closer than SH4ZAM's routine");
    return t_done("atan2_test");
}
