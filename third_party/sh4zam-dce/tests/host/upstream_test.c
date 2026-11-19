/* SH4ZAM-DCE tests: doc/upstream/shz_asin_acos.h on the host (SH4ZAM's software back end)

   upstream_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   The file under doc/upstream is offered to SH4ZAM as it is, so it is tested as it is: it includes SH4ZAM only.
   What is checked, every claim of its comment:
   - shz_asinf_quartic and shz_acosf_quartic against the true angle (asin / acos in double) inside 8e-6 rad, over two million
     arguments: even steps, random, dense near 0 and near +-1
   - the special cases: +0 at a zero of either sign, the ends, arguments past +-1, a NaN, the sign, the odd symmetry of the arcsine,
     acos(-x) = pi - acos(x)
   - a constant argument takes the compile-time path and lands within a float of the true angle
   - SH4ZAM's routines on the same arguments: their worst error is printed beside it
*/

#include "../../doc/upstream/shz_asin_acos.h"

#include "shz_dce_test.h"

#define BOUND 8e-6
#define PI_D 3.14159265358979323846

static volatile float vx;   /* run-time values: a constant argument takes the compile-time path */

int main(void) {
    double worst_as = 0.0, worst_ac = 0.0, worst_os = 0.0, worst_oc = 0.0;
    int k;

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
        as = shz_asinf_quartic(x); ac = shz_acosf_quartic(x);
        c = x > 1.0f ? 1.0 : x < -1.0f ? -1.0 : (double)x;
        es = fabs(as - asin(c)); ec = fabs(ac - acos(c));
        T_CHECK(es <= BOUND, "asinf(%.9g) = %.9g, true %.9g", (double)x, (double)as, asin(c));
        T_CHECK(ec <= BOUND, "acosf(%.9g) = %.9g, true %.9g", (double)x, (double)ac, acos(c));
        T_CHECK(as == 0.0f || (as < 0.0f) == (x < 0.0f), "asinf(%.9g) = %.9g has the wrong sign", (double)x, (double)as);
        T_CHECK(ac >= 0.0f && ac <= (float)PI_D, "acosf(%.9g) = %.9g leaves [0, pi]", (double)x, (double)ac);
        if (es > worst_as) worst_as = es;
        if (ec > worst_ac) worst_ac = ec;
        {
            float m, asm_, acm;
            vx = -x; m = vx;
            asm_ = shz_asinf_quartic(m); acm = shz_acosf_quartic(m);
            T_CHECK(t_bits(asm_) == (t_bits(as) ^ 0x80000000u) || (as == 0.0f && asm_ == 0.0f), "asinf is not odd at %.9g: %.9g and %.9g", (double)x, (double)as, (double)asm_);
            T_CHECK(fabs(((double)ac + acm) - PI_D) <= 5e-7, "acosf(x) + acosf(-x) = %.9g at %.9g", (double)ac + acm, (double)x);
        }
        if (fabsf(x) < 1.0f) {
            const double eos = fabs(shz_asinf(x) - asin((double)x)), eoc = fabs(shz_acosf(x) - acos((double)x));
            if (eos > worst_os) worst_os = eos;
            if (eoc > worst_oc) worst_oc = eoc;
        } else {
            T_CHECK(t_bits(as) == t_bits(x > 0.0f ? (float)(PI_D / 2) : -(float)(PI_D / 2)), "asinf at or past the end: %.9g", (double)as);
            T_CHECK(t_bits(ac) == t_bits(x > 0.0f ? 0.0f : (float)PI_D), "acosf at or past the end: %.9g", (double)ac);
        }
    }
    vx = 0.0f;
    T_CHECK(t_bits(shz_asinf_quartic(vx)) == 0u, "asinf(+0) must be +0");
    T_CHECK(fabs(shz_acosf_quartic(vx) - PI_D / 2) <= 1.2e-7, "acosf(0) must be pi/2");
    vx = -0.0f;
    T_CHECK(t_bits(shz_asinf_quartic(vx)) == 0u, "asinf(-0) must be +0");
#ifndef __FAST_MATH__   /* -ffast-math tells the compiler there are no NaNs: the file promises nothing for one there */
    vx = t_float(0x7fc00000u);
    {
        const float ns = shz_asinf_quartic(vx), nc = shz_acosf_quartic(vx);
        T_CHECK((t_bits(ns) & 0x7fffffffu) > 0x7f800000u && (t_bits(nc) & 0x7fffffffu) > 0x7f800000u, "a NaN must give a NaN: %08x %08x", (unsigned)t_bits(ns), (unsigned)t_bits(nc));
    }
#endif
    /* constant arguments: the compile-time path */
    T_CHECK(fabs(shz_asinf_quartic(0.5f) - asin(0.5)) <= 1.2e-7 && fabs(shz_acosf_quartic(0.5f) - acos(0.5)) <= 1.2e-7, "constant 0.5");
    T_CHECK(fabs(shz_asinf_quartic(2.0f) - PI_D / 2) <= 1.2e-7 && shz_acosf_quartic(2.0f) == 0.0f && fabs(shz_acosf_quartic(-3.0f) - PI_D) <= 2.4e-7, "constants past the ends");
    printf("worst error against the true angle, rad: asinf %.3g (SH4ZAM %.3g), acosf %.3g (SH4ZAM %.3g); bound %.3g\n", worst_as, worst_os, worst_ac, worst_oc, BOUND);
    T_CHECK(worst_os > 100.0 * worst_as && worst_oc > 100.0 * worst_ac, "the file says the proposal is far closer than SH4ZAM's routines");
    return t_done("upstream_test");
}
