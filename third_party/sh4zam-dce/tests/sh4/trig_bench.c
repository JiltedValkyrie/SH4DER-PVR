/* SH4ZAM-DCE tests: shz_dce_trig.h, the loops its cycle ESTIMATES are made from

   trig_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the fast-math flags and run by tests/sh4run/sh4bench: every bench_ function is
   `void f(void *out, const void *in0, const void *in1, size_t count)` on buffers of random floats in [-1, 1).
   SH4ZAM's routine (the _shz rows, before) and the rework (after); then the routines the header says it does NOT rework, with
   the variant that was tried, so its "no gain" is a figure. An ESTIMATE from the interpreter's pipeline model, not a console's.
*/

#include <sh4zam_dce/shz_dce_trig.h>

#define R SHZ_RESTRICT
#define F1(name, expr) void name(float *R out, const float *R a, const float *R b, size_t n) { (void)b; for (size_t i = 0; i < n; i++) { const float x = a[i]; out[i] = (expr); } }
#define F2(name, expr) void name(float *R out, const float *R a, const float *R b, size_t n) { for (size_t i = 0; i < n; i++) { const float x = a[i], y = b[i]; out[i] = (expr); } }

F1(bench_asinf_shz, shz_asinf(x))
F1(bench_asinf, shz_dce_asinf(x))
F1(bench_acosf_shz, shz_acosf(x))
F1(bench_acosf, shz_dce_acosf(x))

/* every argument past 1 (x + 2: the range test's other side; the add is the same in every row). The wrappers are SH4ZAM's routines
   (master ff39547), so the _shz rows and the plain rows must cost the same */
F1(bench_asinf_past1, shz_dce_asinf(x + 2.0f))
F1(bench_acosf_past1, shz_dce_acosf(x + 2.0f))

/* ---- not reworked: the figures behind the header's list ---- */

F1(bench_sinf_shz, shz_sinf(x))
F1(bench_tanf_shz, shz_tanf(x))
F1(bench_atanf_shz, shz_atanf(x))
F2(bench_atan2f_shz, shz_atan2f(x, y))

/* tan without the sign test: sin cos / cos^2, one multiply longer */
SHZ_FORCE_INLINE float try_tanf(float radians) {
    const shz_sincos_t sc = shz_sincosf(radians);
    const float i = shz_inv_sqrtf_fsrra(sc.cos * sc.cos);
    return sc.sin * sc.cos * i * i;
}
F1(bench_try_tanf, try_tanf(x))

/* atan2 with one quotient q = (|x| - |y|) / (|x| + |y|) and the reflection folded into its sign: the same operations as SH4ZAM's in
   another order; the compiler orders both alike */
SHZ_FORCE_INLINE float try_atan2f(float y, float x) {
    const float ax = shz_fabsf(x), ay = shz_fabsf(y), s = ax + ay;
    float d = ax - ay, a = SHZ_F_PI_4;
    if (x <= 0.0f) { d = -d; a = 3.0f * SHZ_F_PI_4; }
    const float q = d * shz_inv_sqrtf_fsrra(s * s);
    a = shz_fmaf(q * q, 0.1963f * q, shz_fmaf(q, -0.9817f, a));
    if (SHZ_UNLIKELY(s == 0.0f)) return 0.0f;
    a = shz_fabsf(a);
    return (y < 0.0f) ? -a : a;
}
F2(bench_try_atan2f, try_atan2f(x, y))
