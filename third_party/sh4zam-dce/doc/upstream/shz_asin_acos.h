/* SH4ZAM: arcsine and arccosine by one reciprocal root and one quartic (a proposal)

   shz_asin_acos.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License.
*/

/** \file    shz_asin_acos.h
    \brief   Replacements for shz_asinf() and shz_acosf(): about 850 times closer to the true
             angle, defined past +-1, for the same cycles or fewer.

    Self-contained: it needs SH4ZAM's own headers and nothing else, and it has SH4ZAM's
    signatures. The two routines carry the suffix _quartic only so that this file compiles next
    to the routines it would replace: to take them, copy the three bodies into shz_trig.inl.h
    and drop the suffix.

    What SH4ZAM 0.10 does: asin(x) = atan(x / sqrt(1 - x x)) with its two-term arctangent, acos =
    pi/2 - asin. The arctangent's own error is 4.95e-3 rad, and for |x| > 1, which a dot product
    of two unit vectors reaches by rounding, 1 - x x is negative and the result is NaN.

    What this does, with t = |x|:

        acos(t) = sqrt(1 - t) (pi/2 + b1 t + b2 t^2 + b3 t^3 + b4 t^4) + e(t),   0 <= t <= 1
        b1 = -0.214366547, b2 = 0.0860212715, b3 = -0.0382241586, b4 = 0.0100441489
        |e(t)| <= 5.51e-6 rad

        acosf(x) = x >= 0 ? acos(t) : pi - acos(t)
        asinf(x) = sign(x) |pi/2 - acos(t)|

    acos(1 - s) = sqrt(2 s) (1 + s/12 + ...), so acos(t) / sqrt(1 - t) is smooth on [0, 1] and a
    short polynomial fits it; the factor sqrt(1 - t) carries the infinite slope at 1. The
    coefficients are a minimax fit with weight sqrt(1 - t) and the constant term held at pi/2,
    so that the error vanishes at 0: asin(0) is exactly 0 and both functions are continuous
    there. (Abramowitz and Stegun 4.4.45 is the cubic of this form: 6.7e-5.) The root is
    sqrt(1 - t) = FSRRA(1 - t) (1 - t): one FSRRA, no divide, no second root.

    Error: |result - true angle| <= 8e-6 rad on [-1, 1] (the fit 5.51e-6, FSRRA, and the float
    rounding of a result near pi/2). Measured worst: 5.8e-6 on the SH-4 (FSRRA's own bits),
    5.9e-6 on the software back end, over 2 x 10^6 arguments dense near 0 and near +-1. Near
    zero the error is absolute, not relative: for |x| < 1e-5 the result has the sign of x and is
    within the bound of it (SH4ZAM 0.10's asin is 0.972 x there).
    Special cases: asinf of a zero of either sign is +0. |x| >= 1 gives +-pi/2 (asinf) and 0 or
    pi (acosf), for every x past the ends too. Away from zero asinf is odd bit for bit. A NaN
    gives a NaN, unless the build tells the compiler there are none (-ffinite-math-only, which
    -ffast-math includes): then the result for a NaN is whatever the compiler makes of it.
    The reciprocal root of a zero or negative number is computed and thrown away for |x| >= 1:
    the FPU's divide-by-zero and invalid-operation exceptions must be off, as KallistiOS has
    them.
    With -ffast-math the compiler may regroup the three factors FSRRA(1 - t), (1 - t) and the
    polynomial, so the last bit of a result can differ from one call site to another. The bound
    holds for every grouping.

    Cost, on a console (single calls, cycles cached / uncached, measured and reported by Falco
    Girgis; SH4ZAM's routine -> this one): asinf for |x| < 1: 63 / 228 -> 64 / 208; acosf for
    |x| < 1: 65 / 288 -> 64 / 252; asinf over [-pi, pi]: 57 / 254 -> 46 / 202; acosf over
    [-pi, pi]: 65 / 288 -> 54 / 256.

    How it was tested (SH4ZAM-DCE, tests/host/upstream_test.c and tests/sh4/upstream_check.c):
    the bound and every special case above, on the host with strict and with -ffast-math flags,
    and on the SH-4 code of sh-elf-gcc 15.2 (-O2 and -O3, strict and -ffast-math) run
    instruction by instruction with bit-exact FSRRA.
*/

#ifndef SHZ_ASIN_ACOS_H
#define SHZ_ASIN_ACOS_H

#include <sh4zam/shz_sh4zam.h>

#ifdef __cplusplus
extern "C" {
#endif

/** \brief (1 - t) times the quartic of the file comment, for t = |x| and one_minus = 1 - t. */
SHZ_FORCE_INLINE float shz_acosf_quartic_poly_(float t, float one_minus) SHZ_NOEXCEPT {
    const float t2 = t * t;
    const float low = shz_fmaf(t, -0.214366547f, SHZ_F_PI_2);
    const float high = shz_fmaf(t2, 0.0100441489f, shz_fmaf(t, -0.0382241586f, 0.0860212715f));
    return one_minus * shz_fmaf(t2, high, low);
}

/** \brief The arccosine in radians, [0, pi]. |error| <= 8e-6 rad. x >= 1 gives 0, x <= -1 gives pi. */
SHZ_INLINE float shz_acosf_quartic(float x) SHZ_NOEXCEPT {
#ifdef SHZ_GNUC
    if (__builtin_constant_p(x))
        return __builtin_acosf(x > 1.0f ? 1.0f : x < -1.0f ? -1.0f : x);
#endif
    const float t = shz_fabsf(x);
    const float one_minus = 1.0f - t;
    float r = shz_inv_sqrtf_fsrra(one_minus) * shz_acosf_quartic_poly_(t, one_minus);
    if (SHZ_UNLIKELY(t >= 1.0f))
        r = 0.0f;
    return (x < 0.0f) ? SHZ_F_PI - r : r;
}

/** \brief The arcsine in radians, [-pi/2, pi/2]. |error| <= 8e-6 rad. Odd, +0 at a zero. |x| >= 1 gives +-pi/2. */
SHZ_INLINE float shz_asinf_quartic(float x) SHZ_NOEXCEPT {
#ifdef SHZ_GNUC
    if (__builtin_constant_p(x))
        return __builtin_asinf(x > 1.0f ? 1.0f : x < -1.0f ? -1.0f : x);
#endif
    const float t = shz_fabsf(x);
    const float one_minus = 1.0f - t;
    /* fabs: a last-bit excess of the product near t = 0 must not flip the sign */
    float r = shz_fabsf(shz_fmaf(-shz_inv_sqrtf_fsrra(one_minus), shz_acosf_quartic_poly_(t, one_minus), SHZ_F_PI_2));
    if (SHZ_UNLIKELY(t >= 1.0f))
        r = SHZ_F_PI_2;
    return (x < 0.0f) ? -r : r;
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_ASIN_ACOS_H */
