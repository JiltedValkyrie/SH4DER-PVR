/* SH4ZAM-DCE: the angle of a vector

   shz_dce_atan2.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

/** \file    shz_dce_atan2.h
    \brief   SH4ZAM's shz_atan2f under a shz_dce_ name: it IS SH4ZAM's routine, called
             underneath. One implementation, SH4ZAM's.

    History. This header held a rework: SH4ZAM 0.10's quotient with a longer polynomial,
    atan(r) = r P(r^2), P the minimax fit with P(1) held at pi/4, of 2, 5, 6 or 7 terms
    (SHZ_DCE_ATAN2_TERMS). With 5 terms an interpolated rotation that jittered with SH4ZAM 0.10's
    cubic was steady on a console (reported by Falco Girgis; doc/THESIS.md, 2.5). SH4ZAM took it
    in commit cff97e8 (#92, 2026-11-18) with the same coefficients and the same evaluation:
    SHZ_ATAN2F_TERMS (2, 5, 6 or 7, default 5) and SHZ_ATAN2F_ERROR_MAX in shz_trig.h. Checked on
    the host before the bodies here were removed: the 5-term rework and SH4ZAM master ff39547 gave
    the same bits for 4,000,000 points (a third of them on the x axis with a zero y of either
    sign), strict and -ffast-math builds alike. The SH-4 side is tests/sh4/atan2_check.c. The last
    tree with the bodies is 691aa21.

    The number of terms is SH4ZAM's switch now: SHZ_ATAN2F_TERMS, set for the whole build (on the
    compiler's command line) as SH4ZAM's comment asks. SHZ_DCE_ATAN2_TERMS is still read: it must
    equal SHZ_ATAN2F_TERMS, or the build stops, so that a build that asked for 7 terms the old way
    does not get 5 without a word.

    What SH4ZAM's routine promises (master ff39547): |error| <= SHZ_ATAN2F_ERROR_MAX (1.4e-5 rad
    with 5 terms) for finite x and y; atan2(+-0, +-0) is 0; the result has the sign of y, a zero y
    of either sign counting as positive.
    Alignment: none (values in registers).
    Bit-identical to shz_atan2f: YES (it is that routine; tests/host/wrap_test.c,
    tests/sh4/atan2_check.c).

    Use it for: code written against this name. New code calls shz_atan2f.

    Version: v0.10
*/

#ifndef SHZ_DCE_ATAN2_H
#define SHZ_DCE_ATAN2_H

#include "shz_dce_cdefs.h"

/** \brief The number of terms of shz_atan2f()'s polynomial, as this layer used to name it: it
    follows SH4ZAM's SHZ_ATAN2F_TERMS and must equal it when set. */
#ifndef SHZ_DCE_ATAN2_TERMS
#define SHZ_DCE_ATAN2_TERMS SHZ_ATAN2F_TERMS
#endif
#if SHZ_DCE_ATAN2_TERMS != SHZ_ATAN2F_TERMS
#error "SHZ_DCE_ATAN2_TERMS is now SH4ZAM's SHZ_ATAN2F_TERMS: set that one, for the whole build"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** \brief SH4ZAM's shz_atan2f (Falco Girgis), called underneath: the angle of (x, y) in radians,
    [-pi, pi]. */
SHZ_INLINE float shz_dce_atan2f(float y, float x) SHZ_NOEXCEPT {
    return shz_atan2f(y, x);
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_ATAN2_H */
