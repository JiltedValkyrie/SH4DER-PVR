/* SH4ZAM-DCE: the angle of a vector

   shz_dce_atan2.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

/** \file    shz_dce_atan2.h
    \brief   A rework of SH4ZAM's shz_atan2f (Falco Girgis): the same reduction, a longer
             polynomial. 800 times closer to the true angle for 8 more cycles in the model
             (the default, 5 terms), 5000 times for 9, or 1.6 times as close for none. A trade
             of cycles for accuracy, not a faster routine.

    SH4ZAM's routine. With s = |y| it forms one quotient,

        x >  0:   r = (x - s) / (x + s),   angle = pi/4  - atan(r)
        x <= 0:   r = (x + s) / (s - x),   angle = 3pi/4 - atan(r)

    (tan(pi/4 - a) = (1 - tan a) / (1 + tan a): exact identities, r in [-1, 1]), takes the quotient
    by FSRRA of the squared denominator, and gives the result the sign of y. All of its error is
    in the arctangent, which it takes as 0.9817 r - 0.1963 r^3: 1.02e-2 rad at worst. That
    cubic has the right shape: its constants add up to pi/4, so it has no error at r = +-1,
    which are the axes. But among the cubics a r + (pi/4 - a) r^3 the best has a = 0.967482567:
    6.13e-3 rad. (The minimax cubic without that condition, the one in SH4ZAM's
    shz_atanf_unit, is 4.95e-3, all of it on the axes too: atan2(0, 1) would be 4.95e-3.)

    Here: the same quotient, and atan(r) = r P(r^2) with P of 2, 5, 6 or 7 terms, each the
    minimax fit on [-1, 1] with P(1) held at pi/4, as the original's, so that the polynomial
    has no error of its own on the axes. SHZ_DCE_ATAN2_TERMS picks one, before the include;
    the default is 5.

    Why 5: a console. Falco Girgis ran the forms in a 60 frames a second interpolation (Euler
    angles to quaternion, slerp, back to angles): with SH4ZAM's routine the result jitters,
    with 5 terms it does not ("a few extra cycles above the 2, all the precision we need").
    His cycle counts are not in this file; the E column below is still the model's.

        terms   degree   |result - atan2(y, x)| <=   worst found   cycles E
          2        3          6.2e-3 rad               6.14e-3       36.5
          5        9          1.4e-5                   1.28e-5       45.4
          6       11          2.5e-6                   2.1e-6        46.4
          7       13          1.0e-6                   5.3e-7        49.4
        SH4ZAM's   3          1.02e-2                  1.015e-2      37.5

    The bound is the fit's error (6.131e-3, 1.250e-5, 1.803e-6, 2.70e-7) plus 5e-7 for the rest:
    the quotient (FSRRA and four roundings: at most 2.2e-7 in the angle), the polynomial's own
    roundings (1e-7) and the rounding of a result near pi (1.5e-7). It holds for finite x and y
    with 2e-18 <= |x| + |y| <= 5e17 (the denominator is squared on the way, as in SH4ZAM's
    routine). "Worst found" is over four million points on the host and 90 thousand on the SH-4
    with FSRRA's own bits (tests/host/atan2_test.c, tests/sh4/atan2_check.c): the 5e-7 is a
    budget that the tests have not exceeded, not a proof.
    Reducing the argument to an octant instead (r within tan(pi/8), three or four terms) was
    tried: 3.8e-6 for 56 cycles and 4.0e-7 for 61; the extra compare and the unfolding cost
    more than the terms they save. So on this machine accuracy is bought with cycles: 8 of them
    for the default. Nothing here is faster than SH4ZAM's routine: the 2-term form is its code
    with another constant (the one cycle of the model is instruction order; a console may not
    show it).

    Special cases, as SH4ZAM's routine: atan2(+-0, +-0) is 0. The result has the sign of y, and
    a zero y of either sign counts as positive: y = -0, x < 0 gives +pi where C's atan2 gives
    -pi (the same direction). On the axes: x > 0, y = 0 gives a number within 4e-7 of zero;
    x < 0, y = 0 gives pi and x = 0 gives +-pi/2, within 6e-7.
    Alignment: none (values in registers).
    Bit-identical to shz_atan2f: NO (another polynomial; the difference is SH4ZAM's error).
    With -ffast-math the compiler may regroup the polynomial's terms: the last bits can differ
    from one call site to another, inside the bound.

    Cycles E are estimates from the pipeline model in a loop over an array, -O2 and -O3 alike.
    The model has been wrong about single calls before (doc/THESIS.md, section 7).
    shz_dce_override.h puts SH4ZAM's name on it (SHZ_DCE_USE_ATAN2, on by default).

    Use it for: angles that feed a camera or an orientation, where 1e-2 rad shows.

    Version: v0.10
*/

#ifndef SHZ_DCE_ATAN2_H
#define SHZ_DCE_ATAN2_H

#include "shz_dce_cdefs.h"

/** \brief The number of terms of the arctangent polynomial of shz_dce_atan2f(): 2, 5, 6 or 7
    (see the table in the file comment). Set before the first SH4ZAM-DCE include. */
#ifndef SHZ_DCE_ATAN2_TERMS
#define SHZ_DCE_ATAN2_TERMS 5
#endif
#if SHZ_DCE_ATAN2_TERMS != 2 && SHZ_DCE_ATAN2_TERMS != 5 && SHZ_DCE_ATAN2_TERMS != 6 && SHZ_DCE_ATAN2_TERMS != 7
#error "SHZ_DCE_ATAN2_TERMS must be 2, 5, 6 or 7"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** \brief P(u) with atan(r) = r P(r r) on [-1, 1], u = r r: `terms` (a constant: 2, 5, 6 or 7)
    coefficients, the minimax fit with P(1) held at pi/4. Fit errors: 6.131e-3, 1.250e-5,
    1.803e-6, 2.70e-7 rad. Written in pairs (Estrin), so that the terms do not wait for each
    other. */
SHZ_FORCE_INLINE float shz_dce_atan_poly_(float u, int terms) SHZ_NOEXCEPT {
    const float u2 = u * u;

    if (terms == 2)
        return shz_fmaf(u, -0.182084382f, 0.967482567f);
    if (terms == 5)
        return shz_fmaf(u2, shz_fmaf(u2, 0.0202457625f, shz_fmaf(u, -0.0839662105f, 0.179388285f)), shz_fmaf(u, -0.330125183f, 0.999855518f));
    if (terms == 6)
        return shz_fmaf(u2, shz_fmaf(u2, shz_fmaf(u, -0.0114275254f, 0.0519234873f), shz_fmaf(u, -0.11578197f, 0.193293691f)),
                        shz_fmaf(u, -0.332585186f, 0.999975681f));
    return shz_fmaf(u2, shz_fmaf(u2, shz_fmaf(u2, 0.00666252105f, shz_fmaf(u, -0.0331594534f, 0.0791187882f)), shz_fmaf(u, -0.132061511f, 0.198008001f)),
                    shz_fmaf(u, -0.333166063f, 0.999995887f));
}

/** \brief shz_dce_atan2f() with the number of terms chosen by the caller (`terms` a constant: 2,
    5, 6 or 7). The public routine passes SHZ_DCE_ATAN2_TERMS; the tests run all four. */
SHZ_INLINE float shz_dce_atan2f_n_(float y, float x, int terms) SHZ_NOEXCEPT {
#ifdef SHZ_GNUC
    if (__builtin_constant_p(y) && __builtin_constant_p(x))
        return __builtin_atan2f(y, x);
#endif
    const float s = shz_fabsf(y);
    float num, den, base;

    if (x > 0.0f) {
        num = x - s; den = x + s; base = SHZ_F_PI_4;
    } else {
        if (SHZ_UNLIKELY(x == 0.0f && y == 0.0f))
            return 0.0f;
        num = x + s; den = s - x; base = 3.0f * SHZ_F_PI_4;
    }
    const float r = num * shz_invf_fsrra(den);
    return shz_copysignf(shz_fmaf(-r, shz_dce_atan_poly_(r * r, terms), base), y);
}

/** \brief Rework of SH4ZAM's shz_atan2f (Falco Girgis): the angle of (x, y) in radians,
    [-pi, pi]. Same signature and special cases. |error| <= 1.4e-5 rad with the default 5 terms
    (SH4ZAM: 1.02e-2); SHZ_DCE_ATAN2_TERMS 2, 6 or 7 for 6.2e-3, 2.5e-6 or 1.0e-6.
    Bit-identical to shz_atan2f: NO. Cycles E: 37.5 -> 45.4 (36.5, 46.4, 49.4). */
SHZ_INLINE float shz_dce_atan2f(float y, float x) SHZ_NOEXCEPT {
    return shz_dce_atan2f_n_(y, x, SHZ_DCE_ATAN2_TERMS);
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_ATAN2_H */
