/* SH4DER-PVR: repro for SH4ZAM_NOTES.md (shz_ldexpf / shz_pow2f with whole exponents; shz_log2f's offset)

   demo/psm/recipes/shz_notes_repro.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE. SH4ZAM: Copyright (C) 2025, 2026 Falco Girgis (MIT).

   Host, SH4ZAM's software back end. Build from this folder:
     gcc -O2 -std=gnu11 -I../../../third_party/sh4zam/include shz_notes_repro.c \
         ../../../third_party/sh4zam/source/sw/shz_xmtrx_sw.c -lm -o shz_notes_repro
   Arguments pass through volatile, so gcc cannot fold them (SH4ZAM takes the exact builtin when it can: a constant
   argument gives a different answer from the same value at run time; the last block shows it).
*/

#include <math.h>
#include <stdio.h>
#include <sh4zam/shz_sh4zam.h>

static volatile float vx;
static volatile int vi;

static float run_f(float x) { vx = x; return vx; }

int main(void) {
    double worst = 0.0, worst_log = 0.0, off_min = 1e9, off_max = -1e9;
    int e, i;

    printf("shz_ldexpf(x, e), whole e, against ldexpf:\n");
    for (e = -4; e <= 4; e++) {
        vi = e;
        printf("  ldexpf(1, %2d) = %-10g shz_ldexpf = %-10g shz_pow2f(%2d) = %-10g  rel %+.5f\n", e, (double)ldexpf(1.0f, e),
               (double)shz_ldexpf(run_f(1.0f), (float)vi), e, (double)shz_pow2f((float)vi),
               (double)shz_ldexpf(run_f(1.0f), (float)vi) / ldexpf(1.0f, e) - 1.0);
    }
    for (e = -20; e <= 20; e++) {
        for (i = 0; i < 1024; i++) {
            const float x = 1.0f + (float)i / 1024.0f;
            vi = e;
            {
                const double r = fabs((double)shz_ldexpf(run_f(x), (float)vi) / ldexpf(x, e) - 1.0);
                if (r > worst) worst = r;
            }
        }
    }
    printf("  worst relative error, x in [1, 2), e in -20..20: %.5f\n", worst);
    printf("  the same through shz_powif(2, e): %.1e\n", fabs((double)shz_powif(run_f(2.0f), vi) / ldexpf(1.0f, vi) - 1.0));

    printf("shz_log2f(x) against the piecewise-linear log2 (exponent + mantissa - 1) and log2f:\n");
    for (i = 0; i < 65536; i++) {
        const float x = 1.0f + 255.0f * (float)i / 65536.0f;
        int ex;
        const float m = frexpf(x, &ex);
        const double lin = (double)(ex - 1) + (2.0 * m - 1.0), s = (double)shz_log2f(run_f(x));
        if (s - lin < off_min) off_min = s - lin;
        if (s - lin > off_max) off_max = s - lin;
        if (fabs(s - log2(x)) > worst_log) worst_log = fabs(s - log2(x));
    }
    printf("  shz_log2f - linear: %.8f .. %.8f (127 - 126.94269504 = %.8f)\n", off_min, off_max, 127.0 - 126.94269504);
    printf("  worst against log2f: %.4f\n", worst_log);
    printf("  at powers of two: shz_log2f(1) = %.6f, shz_log2f(2) = %.6f, shz_log2f(256) = %.6f\n",
           (double)shz_log2f(run_f(1.0f)), (double)shz_log2f(run_f(2.0f)), (double)shz_log2f(run_f(256.0f)));

    {   /* the functions built on shz_pow2f / shz_log2f, at run time, against libm */
        double pmin = 1e9, pmax = -1e9, emin = 1e9, emax = -1e9, e0 = 0.0, w_pow = 0.0, w_p10 = 0.0, w_log = 0.0, jump;
        for (i = 0; i <= 200000; i++) {             /* shz_pow2f over p in [-8, 8] */
            const float p = -8.0f + 16.0f * (float)i / 200000.0f;
            const double r = (double)shz_pow2f(run_f(p)) / exp2((double)p) - 1.0;
            if (r < pmin) pmin = r;
            if (r > pmax) pmax = r;
        }
        printf("shz_pow2f(p), p in [-8, 8]: relative %+.4f .. %+.4f\n", pmin, pmax);
        for (i = 0; i <= 200000; i++) {             /* shz_expf over [-10, 10] */
            const float x = -10.0f + 20.0f * (float)i / 200000.0f;
            const double r = (double)shz_expf(run_f(x)) / exp((double)x) - 1.0;
            if (r < emin) emin = r;
            if (r > emax) emax = r;
            if (fabs(x) < 0.05f && fabs(r) > e0) e0 = fabs(r);
        }
        printf("shz_expf(x), x in [-10, 10]: relative %+.4f .. %+.4f; near 0 (|x| < 0.05) up to %.4f; shz_expf(0) = %.5f, shz_expf(-0.012) = %.5f (exp: %.5f)\n",
               emin, emax, e0, (double)shz_expf(run_f(0.0f)), (double)shz_expf(run_f(-0.012f)), exp(-0.012));
        for (i = 0; i <= 400; i++) {                 /* shz_powf over x in [0.1, 10], p in [-3, 3] */
            int j;
            for (j = 0; j <= 60; j++) {
                const float x = 0.1f + 9.9f * (float)i / 400.0f, p = -3.0f + 0.1f * (float)j;
                const double r = fabs((double)shz_powf(run_f(x), run_f(p)) / pow((double)x, (double)p) - 1.0);
                if (r > w_pow) w_pow = r;
            }
        }
        for (i = 0; i <= 2000; i++) {
            const float p = -3.0f + 6.0f * (float)i / 2000.0f;
            const double r = fabs((double)shz_pow10f(run_f(p)) / pow(10.0, (double)p) - 1.0);
            if (r > w_p10) w_p10 = r;
        }
        for (i = 1; i <= 20000; i++) {
            const float x = 0.01f + 99.99f * (float)i / 20000.0f;
            const double r = fabs((double)shz_logf(run_f(x)) - log((double)x));
            if (r > w_log) w_log = r;
        }
        jump = (double)shz_expm1f(run_f(0.1001f)) - (double)shz_expm1f(run_f(0.0999f));
        printf("shz_powf, x in [0.1, 10], p in [-3, 3]: worst relative %.4f; shz_powf(2, 3) = %g at run time\n", w_pow,
               (double)shz_powf(run_f(2.0f), run_f(3.0f)));
        printf("shz_pow10f, p in [-3, 3]: worst relative %.4f; shz_pow10f(2) = %g at run time\n", w_p10, (double)shz_pow10f(run_f(2.0f)));
        printf("shz_logf, x in [0.01, 100]: worst absolute %.4f; shz_logf(1) = %.5f\n", w_log, (double)shz_logf(run_f(1.0f)));
        printf("shz_scalbnf(1, 3) = %g (it is shz_ldexpf)\n", (double)shz_scalbnf(run_f(1.0f), run_f(3.0f)));
        printf("shz_expm1f: 0.0999 -> %.6f, 0.1001 -> %.6f (expm1: %.6f, %.6f); step across the cutoff %.4f where expm1 rises 0.0002\n",
               (double)shz_expm1f(run_f(0.0999f)), (double)shz_expm1f(run_f(0.1001f)), expm1(0.0999), expm1(0.1001), jump);
    }

    printf("constant against run time (gcc folds constants to the builtin):\n");
    printf("  shz_pow2f(3.0f) constant = %g, at run time = %g\n", (double)shz_pow2f(3.0f), (double)shz_pow2f(run_f(3.0f)));
    printf("  shz_log2f(8.0f) constant = %g, at run time = %g\n", (double)shz_log2f(8.0f), (double)shz_log2f(run_f(8.0f)));
    return 0;
}
