/* SH4ZAM-DCE tests: shz_dce_trig.h on the host (SH4ZAM's software back end)

   trig_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked:
   - the fit itself, in double: sqrt(1 - t) (pi/2 + b1 t + .. + b4 t^4) against acos(t) on [0, 1], inside the 5.6e-6 the header states
   - shz_dce_asinf and shz_dce_acosf against the true angle (asin / acos in double) inside the header's bound, over two million
     arguments: even steps, random, dense near 0 and near +-1
   - the special cases: 0, the ends, arguments past +-1, the sign, the odd symmetry of the arcsine, acos(-x) = pi - acos(x)
   - SH4ZAM's routines on the same arguments: their worst error is printed beside the rework's
*/

#include <sh4zam_dce/shz_dce_trig.h>

#include "shz_dce_test.h"

#define BOUND 8e-6
#define PI_D 3.14159265358979323846

static volatile float vx;   /* run-time values: a constant argument takes SH4ZAM's compile-time path */

int main(void) {
    double worst_fit = 0.0, worst_as = 0.0, worst_ac = 0.0, worst_os = 0.0, worst_oc = 0.0;
    int k;

    for (k = 0; k <= 2000000; k++) {   /* the fit in exact arithmetic */
        const double t = (double)k / 2000000.0;
        const double p = PI_D / 2 + t * (-0.214366547 + t * (0.0860212715 + t * (-0.0382241586 + t * 0.0100441489)));
        const double e = fabs(sqrt(1.0 - t) * p - acos(t));
        if (e > worst_fit) worst_fit = e;
    }
    T_CHECK(worst_fit <= 5.6e-6, "the fit's error is %.3g, the header says 5.6e-6", worst_fit);

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
        as = shz_dce_asinf(x); ac = shz_dce_acosf(x);
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
            asm_ = shz_dce_asinf(m); acm = shz_dce_acosf(m);
            T_CHECK(asm_ == -as, "asinf is not odd at %.9g: %.9g and %.9g", (double)x, (double)as, (double)asm_);
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
    T_CHECK(t_bits(shz_dce_asinf(vx)) == 0u, "asinf(0) must be +0");
    T_CHECK(fabs(shz_dce_acosf(vx) - PI_D / 2) <= 1.2e-7, "acosf(0) must be pi/2");
    printf("fit %.3g; worst error against the true angle, rad: asinf %.3g (SH4ZAM %.3g), acosf %.3g (SH4ZAM %.3g); bound %.3g\n",
           worst_fit, worst_as, worst_os, worst_ac, worst_oc, BOUND);
    T_CHECK(worst_os > 100.0 * worst_as && worst_oc > 100.0 * worst_ac, "the header says the reworks are far closer than SH4ZAM's routines");
    return t_done("trig_test");
}
