/* SH4ZAM-DCE tests: shz_dce_trig.h on the host (SH4ZAM's software back end)

   trig_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Pinned to SH4ZAM master ff39547 (2026-11-22): every "SH4ZAM ff39547 claim" below is a promise of that commit, so a later SH4ZAM
   change that breaks one fails here by name. What is checked:
   - the wrappers: shz_dce_asinf / shz_dce_acosf give shz_asinf / shz_acosf bit for bit (each from its own function that is never
     inlined)
   - the fit SH4ZAM's routines are built on, in double: sqrt(1 - t) (pi/2 + b1 t + .. + b4 t^4) against acos(t) on [0, 1], inside
     5.6e-6 (the coefficients are SH4ZAM's, copied into this test only to check that number)
   - SH4ZAM's routines against the true angle inside SHZ_ASINF_ERROR_MAX (8e-6 rad), over two million arguments: even steps, random,
     dense near 0 and near +-1, at and past +-1
   - their special cases: past +-1 they give +-pi/2, 0 or pi; asin is odd and has the sign of x; acos(x) + acos(-x) = pi; asin(0) = +0
*/

#include <sh4zam_dce/shz_dce_trig.h>

#include "shz_dce_test.h"

#define PI_D 3.14159265358979323846
#define NOINLINE __attribute__((noinline))

static volatile float vx;   /* run-time values: a constant argument takes SH4ZAM's compile-time path */

static NOINLINE float d_asinf(float x) { return shz_dce_asinf(x); }
static NOINLINE float s_asinf(float x) { return shz_asinf(x); }
static NOINLINE float d_acosf(float x) { return shz_dce_acosf(x); }
static NOINLINE float s_acosf(float x) { return shz_acosf(x); }

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    double worst_fit = 0.0, worst_as = 0.0, worst_ac = 0.0;
    int k;

    T_CHECK(SHZ_ASINF_ERROR_MAX == 8.0e-6, "SH4ZAM ff39547 claim: SHZ_ASINF_ERROR_MAX is 8e-6");
    for (k = 0; k <= 2000000; k++) {   /* the fit in exact arithmetic */
        const double t = (double)k / 2000000.0;
        const double p = PI_D / 2 + t * (-0.214366547 + t * (0.0860212715 + t * (-0.0382241586 + t * 0.0100441489)));
        const double e = fabs(sqrt(1.0 - t) * p - acos(t));
        if (e > worst_fit) worst_fit = e;
    }
    T_CHECK(worst_fit <= 5.6e-6, "the fit's error is %.3g", worst_fit);

    for (k = 0; k < 2000000; k++) {
        float x, as, ac;
        double es, ec, c;
        switch (k % 5) {
        case 0: x = -1.0f + (float)k / 1000000.0f; break;
        case 1: x = t_frand(-1.0f, 1.0f); break;
        case 2: x = (k & 8 ? -1.0f : 1.0f) * (1.0f - t_float(0x33800000u + (t_rand() % 0x0c000000u))); break;   /* dense near +-1 */
        case 3: x = (k & 8 ? -1.0f : 1.0f) * t_float(0x1e000000u + (t_rand() % 0x21800000u)); break;           /* dense near 0 */
        default: x = (k & 8 ? -1.0f : 1.0f) * (1.0f + t_frand(0.0f, 1.0f) * (k % 3 ? 1e-6f : 3.0f)); break;    /* at and past +-1 */
        }
        vx = x; x = vx;
        as = s_asinf(x); ac = s_acosf(x);
        T_CHECK(t_bits(d_asinf(x)) == t_bits(as), "wrapper: asinf(%a): %08x, SH4ZAM's %08x", (double)x, (unsigned)t_bits(d_asinf(x)), (unsigned)t_bits(as));
        T_CHECK(t_bits(d_acosf(x)) == t_bits(ac), "wrapper: acosf(%a): %08x, SH4ZAM's %08x", (double)x, (unsigned)t_bits(d_acosf(x)), (unsigned)t_bits(ac));
        c = x > 1.0f ? 1.0 : x < -1.0f ? -1.0 : (double)x;
        es = fabs(as - asin(c)); ec = fabs(ac - acos(c));
        T_CHECK(es <= SHZ_ASINF_ERROR_MAX, "SH4ZAM ff39547 claim (SHZ_ASINF_ERROR_MAX): asinf(%.9g) = %.9g, true %.9g", (double)x, (double)as, asin(c));
        T_CHECK(ec <= SHZ_ASINF_ERROR_MAX, "SH4ZAM ff39547 claim (SHZ_ASINF_ERROR_MAX): acosf(%.9g) = %.9g, true %.9g", (double)x, (double)ac, acos(c));
        T_CHECK(as == 0.0f || (as < 0.0f) == (x < 0.0f), "SH4ZAM ff39547 claim: asinf(%.9g) = %.9g has the sign of x", (double)x, (double)as);
        T_CHECK(ac >= 0.0f && ac <= (float)PI_D, "SH4ZAM ff39547 claim: acosf(%.9g) = %.9g inside [0, pi]", (double)x, (double)ac);
        if (es > worst_as) worst_as = es;
        if (ec > worst_ac) worst_ac = ec;
        {
            float m, asm_, acm;
            vx = -x; m = vx;
            asm_ = s_asinf(m); acm = s_acosf(m);
            T_CHECK(asm_ == -as, "SH4ZAM ff39547 claim: asinf is odd at %.9g: %.9g and %.9g", (double)x, (double)as, (double)asm_);
            T_CHECK(fabs(((double)ac + acm) - PI_D) <= 5e-7, "SH4ZAM ff39547 claim: acosf(x) + acosf(-x) = %.9g at %.9g", (double)ac + acm, (double)x);
        }
        if (fabsf(x) >= 1.0f) {
            T_CHECK(t_bits(as) == t_bits(x > 0.0f ? (float)(PI_D / 2) : -(float)(PI_D / 2)), "SH4ZAM ff39547 claim: asinf at or past the end: %.9g", (double)as);
            T_CHECK(t_bits(ac) == t_bits(x > 0.0f ? 0.0f : (float)PI_D), "SH4ZAM ff39547 claim: acosf at or past the end: %.9g", (double)ac);
        }
    }
    vx = 0.0f;
    T_CHECK(t_bits(s_asinf(vx)) == 0u, "SH4ZAM ff39547 claim: asinf(0) is +0");
    T_CHECK(fabs(s_acosf(vx) - PI_D / 2) <= 1.2e-7, "SH4ZAM ff39547 claim: acosf(0) is pi/2");
    printf("wrapper: shz_dce_asinf / shz_dce_acosf == SH4ZAM's bit for bit, 2000000 arguments; fit %.3g; SH4ZAM ff39547's routines: worst error asinf %.3g, acosf %.3g rad, bound %.3g\n",
           worst_fit, worst_as, worst_ac, (double)SHZ_ASINF_ERROR_MAX);
    return t_done("trig_test");
}
