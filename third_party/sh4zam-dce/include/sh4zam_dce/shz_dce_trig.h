/* SH4ZAM-DCE: inverse sine and cosine

   shz_dce_trig.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

/** \file    shz_dce_trig.h
    \brief   Reworks of SH4ZAM's shz_asinf and shz_acosf (Falco Girgis): one square root and one
             polynomial, about 850 times closer to the true angle, no NaN just outside [-1, 1].

    SH4ZAM computes asin(x) as atan(x / sqrt(1 - x x)) with its two-term arctangent, and acos as
    pi/2 - asin. That is FSRRA, a reciprocal (a second FSRRA) when the quotient is past 1, and a
    cubic whose own error is 5e-3 rad; for |x| > 1, which a dot product of two unit vectors
    reaches by rounding, 1 - x x is negative and the result is NaN.

    Here, with t = |x|:

        acos(t) = sqrt(1 - t) (pi/2 + b1 t + b2 t^2 + b3 t^3 + b4 t^4) + e(t),   0 <= t <= 1
        b1 = -0.214366547, b2 = 0.0860212715, b3 = -0.0382241586, b4 = 0.0100441489
        |e(t)| <= 5.6e-6 rad (minimax fit with the constant term held at pi/2, so that e(0) = 0)

        shz_dce_acosf(x) = x >= 0 ? acos(t) : pi - acos(t)
        shz_dce_asinf(x) = sign(x) (pi/2 - acos(t))

    The factor sqrt(1 - t) carries the infinite slope at t = 1, so the polynomial stays smooth
    and short (the form is Abramowitz and Stegun's 4.4.45, refitted with a fifth term). It is
    written as FSRRA(1 - t) x ((1 - t) x polynomial): one FSRRA, multiplied in last. With
    -ffast-math the compiler regroups the three factors by its own rules, so the last bit of a
    result can differ from one call site to another; the bound below holds for every grouping.
    asin(0) = 0 exactly, asin is odd, both are continuous across 0.

    Alignment: none (values in registers).
    Error bound: |result - true angle| <= 8e-6 rad for |x| <= 1 (the fit, FSRRA and the float
    rounding of a result near pi/2; measured worst 5.8e-6 on the SH-4, 5.9e-6 on the software
    back end). SH4ZAM's routines: 4.95e-3 rad measured. Near zero the error is absolute, not
    relative: for |x| < 1e-5 the result is within the bound of x and has the sign of x, but need
    not be closer to x than that (SH4ZAM's asin is 0.972 x there).
    Special cases: |x| >= 1 gives +-pi/2 (asin) and 0 or pi (acos), also past 1 (SH4ZAM: NaN).
    Bit-identical to SH4ZAM's routines: NO (another formula; the difference is SH4ZAM's error,
    up to 4.95e-3 rad).
    The FPU's divide-by-zero and invalid-operation exceptions must be off (KallistiOS's default):
    for |x| >= 1 the root of a zero or negative number is computed and thrown away.

    What it costs: two answers.
    - A console (single calls, cycles cached / uncached, reported by Falco Girgis; SH4ZAM, then
      this): asinf for |x| < 1: 63 / 228 -> 64 / 208; acosf for |x| < 1: 65 / 288 -> 64 / 252;
      asinf over [-pi, pi]: 57 / 254 -> 46 / 202; acosf over [-pi, pi]: 65 / 288 -> 54 / 256.
      About equal in range, faster past +-1 (the range test comes before the polynomial).
    - The pipeline model (cycles E a call in a loop over an array, -O2 and -O3 alike; SH4ZAM
      31.7 and 31.8): asinf 34.5, acosf 31.5, and 21 past +-1.
    The barrier. SHZ_DCE_OPAQUE_ASIN_ACOS 1 puts the product (1 - t) x polynomial behind
    shz_dce_opaquef(): the factors then stay as written and the polynomial stays in front of the
    range test (shz_dce_cdefs.h). The model gives 28.5 and 28.5 for that in range, 30 and 29 past
    +-1; the console gives 65 / 210 and 65 / 260 in range (no gain) and 65 / 210 and 68 / 270
    over [-pi, pi] (a loss). It is off by default.

    Not reworked, with the reason (loops of the same shape; tests/sh4/trig_bench.c has the rows):
    - shz_sincosf, shz_sinf, shz_cosf (16 to 17 E): one multiply, FTRC, FSCA; nothing to remove.
    - shz_tanf (28 E): FSCA, then sin x FSRRA(cos^2) with the sign put back; the sign-free form
      sin cos FSRRA(cos^2)^2 is one multiply longer (32 E).
    - shz_atanf (17 E on [-1, 1]): three operations; another grouping has the same depth.
    - shz_atan2f (37.4 E): not reworked for speed. 14 of its cycles are the quotient (add,
      square, FSRRA, multiply) and stay; the same operations with one quotient and no reflection
      subtract measure 36.5 E in C, because the compiler orders them as before. Evaluating the
      polynomial's terms while FSRRA runs would save about 8 cycles and needs an assembly block:
      not done. Reworked for accuracy in shz_dce_atan2.h (1.02e-2 rad to 2.5e-6, 9 cycles more).

    Use it for: angles from dot products and unit-vector components. shz_dce_override.h maps
    shz_asinf / shz_acosf onto these when the approximate overrides are asked for.

    Version: v0.10
*/

#ifndef SHZ_DCE_TRIG_H
#define SHZ_DCE_TRIG_H

#include "shz_dce_cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief (1 - t) times the quartic of the file comment, for t = |x| and one_minus = 1 - t.
    `opaque` (a constant): not 0 puts the product behind shz_dce_opaquef(), which keeps the
    factors as written and the polynomial in front of the range test that follows; 0 leaves both
    to the compiler. */
SHZ_FORCE_INLINE float shz_dce_acos_poly_(float t, float one_minus, int opaque) SHZ_NOEXCEPT {
    const float t2 = t * t;
    const float low = shz_fmaf(t, -0.214366547f, SHZ_F_PI_2);
    const float high = shz_fmaf(t2, 0.0100441489f, shz_fmaf(t, -0.0382241586f, 0.0860212715f));
    const float product = one_minus * shz_fmaf(t2, high, low);
    return opaque ? shz_dce_opaquef(product) : product;
}

/** \brief shz_dce_acosf() with the barrier chosen by the caller (`opaque` a constant; see
    shz_dce_acos_poly_). The public routine passes SHZ_DCE_OPAQUE_ASIN_ACOS; the tests run both. */
SHZ_INLINE float shz_dce_acosf_o_(float x, int opaque) SHZ_NOEXCEPT {
#ifdef SHZ_GNUC
    if (__builtin_constant_p(x))
        return shz_acosf(x);
#endif
    const float t = shz_fabsf(x);
    const float one_minus = 1.0f - t;
    float r = shz_inv_sqrtf_fsrra(one_minus) * shz_dce_acos_poly_(t, one_minus, opaque);
    if (SHZ_UNLIKELY(t >= 1.0f))
        r = 0.0f;
    return (x < 0.0f) ? SHZ_F_PI - r : r;
}

/** \brief shz_dce_asinf() with the barrier chosen by the caller (see shz_dce_acosf_o_). */
SHZ_INLINE float shz_dce_asinf_o_(float x, int opaque) SHZ_NOEXCEPT {
#ifdef SHZ_GNUC
    if (__builtin_constant_p(x))
        return shz_asinf(x);
#endif
    const float t = shz_fabsf(x);
    const float one_minus = 1.0f - t;
    /* fabs: a last-bit excess of the product near t = 0 must not flip the sign */
    float r = shz_fabsf(shz_fmaf(-shz_inv_sqrtf_fsrra(one_minus), shz_dce_acos_poly_(t, one_minus, opaque), SHZ_F_PI_2));
    if (SHZ_UNLIKELY(t >= 1.0f))
        r = SHZ_F_PI_2;
    return (x < 0.0f) ? -r : r;
}

/** \brief Rework of SH4ZAM's shz_acosf (Falco Girgis): the arccosine in radians, [0, pi].
    |error| <= 8e-6 rad (SH4ZAM: 4.9e-3). x >= 1 gives 0, x <= -1 gives pi, also past +-1.
    Bit-identical to shz_acosf: NO. A console, single calls, cached: 65 -> 64 in range, 65 -> 54
    over [-pi, pi]. Cycles E: 31.8 -> 31.5 (28.5 with SHZ_DCE_OPAQUE_ASIN_ACOS 1). */
SHZ_INLINE float shz_dce_acosf(float x) SHZ_NOEXCEPT {
    return shz_dce_acosf_o_(x, SHZ_DCE_OPAQUE_ASIN_ACOS);
}

/** \brief Rework of SH4ZAM's shz_asinf (Falco Girgis): the arcsine in radians, [-pi/2, pi/2].
    |error| <= 8e-6 rad (SH4ZAM: 4.9e-3). Odd, 0 at 0, the sign of x. |x| >= 1 gives +-pi/2, also
    past +-1. Bit-identical to shz_asinf: NO. A console, single calls, cached: 63 -> 64 in range,
    57 -> 46 over [-pi, pi]. Cycles E: 31.7 -> 34.5 (28.5 with SHZ_DCE_OPAQUE_ASIN_ACOS 1). */
SHZ_INLINE float shz_dce_asinf(float x) SHZ_NOEXCEPT {
    return shz_dce_asinf_o_(x, SHZ_DCE_OPAQUE_ASIN_ACOS);
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_TRIG_H */
