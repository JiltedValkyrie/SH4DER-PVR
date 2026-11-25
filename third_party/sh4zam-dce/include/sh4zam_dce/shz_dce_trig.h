/* SH4ZAM-DCE: inverse sine and cosine

   shz_dce_trig.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
   NOT affiliated with, endorsed by or maintained by Falco Girgis or the SH4ZAM
   project. Report problems with this library to SH4DER-PVR, not to them.
*/

/** \file    shz_dce_trig.h
    \brief   SH4ZAM's shz_asinf and shz_acosf under shz_dce_ names: each IS SH4ZAM's routine,
             called underneath. One implementation, SH4ZAM's.

    History. This header held a rework of both: acos(t) = sqrt(1 - t) (pi/2 + b1 t + .. + b4 t^4),
    one FSRRA and one quartic, about 850 times closer to the true angle than SH4ZAM 0.10's
    atan(x / sqrt(1 - x x)), and defined past +-1. It was offered upstream
    (doc/upstream/shz_asin_acos.h) and SH4ZAM took it in commit cff97e8 (#92, 2026-11-18): the
    same polynomial, the same order of operations, the same special cases. Checked on the host
    before the bodies here were removed: the rework and SH4ZAM master ff39547 gave the same bits
    for 35,204,650 arguments each (every 61st float in [0, 2) and its negative), strict and
    -ffast-math builds alike. The SH-4 side is tests/sh4/trig_check.c. The last tree with the
    bodies is 691aa21; the barrier variant (SHZ_DCE_OPAQUE_ASIN_ACOS) went with them: a console
    had shown it no gain in range and a loss over [-pi, pi] (doc/THESIS.md, section 7).

    What SH4ZAM's routines promise (master ff39547, shz_trig.h): |error| <= SHZ_ASINF_ERROR_MAX
    (8e-6 rad) on [-1, 1]; |x| >= 1 gives +-pi/2 (asin), 0 or pi (acos). The FPU's
    divide-by-zero and invalid-operation exceptions must be off (KallistiOS's default): for
    |x| >= 1 the root of a zero or negative number is computed and thrown away.

    Alignment: none (values in registers).
    Bit-identical to shz_asinf / shz_acosf: YES (they are those routines; tests/host/wrap_test.c,
    tests/sh4/trig_check.c).

    Use it for: code written against these names. New code calls SH4ZAM's names.

    Version: v0.10
*/

#ifndef SHZ_DCE_TRIG_H
#define SHZ_DCE_TRIG_H

#include "shz_dce_cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief SH4ZAM's shz_acosf (Falco Girgis), called underneath: the arccosine in radians, [0, pi]. */
SHZ_INLINE float shz_dce_acosf(float x) SHZ_NOEXCEPT {
    return shz_acosf(x);
}

/** \brief SH4ZAM's shz_asinf (Falco Girgis), called underneath: the arcsine in radians, [-pi/2, pi/2]. */
SHZ_INLINE float shz_dce_asinf(float x) SHZ_NOEXCEPT {
    return shz_asinf(x);
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_TRIG_H */
