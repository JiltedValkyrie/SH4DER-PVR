/* SH4ZAM-DCE: square roots and lengths

   shz_dce_sqrt.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
   NOT affiliated with, endorsed by or maintained by Falco Girgis or the SH4ZAM
   project. Report problems with this library to SH4DER-PVR, not to them.
*/

/** \file    shz_dce_sqrt.h
    \brief   SH4ZAM's square roots and lengths (Falco Girgis) under shz_dce_ names: each routine
             here IS SH4ZAM's routine of the same name, called underneath. One implementation,
             SH4ZAM's.

    History. Against SH4ZAM 0.10 this header held reordered bodies (SHZ_DCE_REWORK_ROOTS 1): the
    zero test of `x == 0 ? 0 : f(FSRRA(x))` taken in FSRRA's shadow, with a value barrier on the
    zero. A console measured single calls (reported by Falco Girgis; doc/THESIS.md, section 7) and
    did not confirm the gain the pipeline model had promised. SH4ZAM master ff39547 (2026-11-22)
    then made every one of them a duplicate or a different job:
    - shz_inv_sqrtf, and with it shz_vecN_magnitude_inv, now computes FSRRA first and returns
      shz_opaquef(0) on the zero path: the reordered body with the barrier on;
    - shz_vecN_normalize_safe is shz_vecN_normalize_or(vec, 0), FSRRA first, the test after: the
      reordered body with the barrier off;
    - shz_sqrtf on the SH-4 is FSQRT, which has no zero test to move (and gives sqrt(-0) = -0,
      sqrt(+inf) = +inf, the correctly rounded root), and shz_vecN_magnitude and
      shz_vecN_distance use it; shz_hypotf scales inputs above 2^62 so the squares do not
      overflow. The FSRRA x x form of the rework was 29 cycles a call on that console against 28
      for FSQRT (cached).
    So the bodies were removed (the last tree with them is 691aa21; building with
    SHZ_DCE_REWORK_ROOTS 1 now stops with an error in shz_dce_cdefs.h).

    Alignment: none (values in registers).
    Error bound: SH4ZAM's own.
    Bit-identical to SH4ZAM's routine of the same name: YES (it is that routine; tests/host/wrap_test.c
    on the host, tests/sh4/sqrt_check.c on the SH-4, over zeros of both signs, denormals, squares
    that underflow and overflow, negatives, infinities).

    Use it for: code written against these names, and the lines of shz_dce_override.h. New code
    calls SH4ZAM's names.

    Version: v0.10
*/

#ifndef SHZ_DCE_SQRT_H
#define SHZ_DCE_SQRT_H

#include "shz_dce_cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief SH4ZAM's shz_sqrtf, called underneath. */
SHZ_FORCE_INLINE float shz_dce_sqrtf(float x) SHZ_NOEXCEPT {
    return shz_sqrtf(x);
}

/** \brief SH4ZAM's shz_inv_sqrtf, called underneath. */
SHZ_FORCE_INLINE float shz_dce_inv_sqrtf(float x) SHZ_NOEXCEPT {
    return shz_inv_sqrtf(x);
}

/** \brief SH4ZAM's shz_hypotf, called underneath. */
SHZ_FORCE_INLINE float shz_dce_hypotf(float x, float y) SHZ_NOEXCEPT {
    return shz_hypotf(x, y);
}

/** \brief SH4ZAM's shz_vec2_magnitude, called underneath. */
SHZ_FORCE_INLINE float shz_dce_vec2_magnitude(shz_vec2_t vec) SHZ_NOEXCEPT {
    return shz_vec2_magnitude(vec);
}

/** \brief SH4ZAM's shz_vec3_magnitude, called underneath. */
SHZ_FORCE_INLINE float shz_dce_vec3_magnitude(shz_vec3_t vec) SHZ_NOEXCEPT {
    return shz_vec3_magnitude(vec);
}

/** \brief SH4ZAM's shz_vec4_magnitude, called underneath. */
SHZ_FORCE_INLINE float shz_dce_vec4_magnitude(shz_vec4_t vec) SHZ_NOEXCEPT {
    return shz_vec4_magnitude(vec);
}

/** \brief SH4ZAM's shz_vec2_magnitude_inv, called underneath. */
SHZ_FORCE_INLINE float shz_dce_vec2_magnitude_inv(shz_vec2_t vec) SHZ_NOEXCEPT {
    return shz_vec2_magnitude_inv(vec);
}

/** \brief SH4ZAM's shz_vec3_magnitude_inv, called underneath. */
SHZ_FORCE_INLINE float shz_dce_vec3_magnitude_inv(shz_vec3_t vec) SHZ_NOEXCEPT {
    return shz_vec3_magnitude_inv(vec);
}

/** \brief SH4ZAM's shz_vec4_magnitude_inv, called underneath. */
SHZ_FORCE_INLINE float shz_dce_vec4_magnitude_inv(shz_vec4_t vec) SHZ_NOEXCEPT {
    return shz_vec4_magnitude_inv(vec);
}

/** \brief SH4ZAM's shz_vec2_distance, called underneath. */
SHZ_FORCE_INLINE float shz_dce_vec2_distance(shz_vec2_t vec1, shz_vec2_t vec2) SHZ_NOEXCEPT {
    return shz_vec2_distance(vec1, vec2);
}

/** \brief SH4ZAM's shz_vec3_distance, called underneath. */
SHZ_FORCE_INLINE float shz_dce_vec3_distance(shz_vec3_t vec1, shz_vec3_t vec2) SHZ_NOEXCEPT {
    return shz_vec3_distance(vec1, vec2);
}

/** \brief SH4ZAM's shz_vec4_distance, called underneath. */
SHZ_FORCE_INLINE float shz_dce_vec4_distance(shz_vec4_t vec1, shz_vec4_t vec2) SHZ_NOEXCEPT {
    return shz_vec4_distance(vec1, vec2);
}

/** \brief SH4ZAM's shz_vec2_normalize_safe, called underneath. */
SHZ_FORCE_INLINE shz_vec2_t shz_dce_vec2_normalize_safe(shz_vec2_t vec) SHZ_NOEXCEPT {
    return shz_vec2_normalize_safe(vec);
}

/** \brief SH4ZAM's shz_vec3_normalize_safe, called underneath. */
SHZ_FORCE_INLINE shz_vec3_t shz_dce_vec3_normalize_safe(shz_vec3_t vec) SHZ_NOEXCEPT {
    return shz_vec3_normalize_safe(vec);
}

/** \brief SH4ZAM's shz_vec4_normalize_safe, called underneath. */
SHZ_FORCE_INLINE shz_vec4_t shz_dce_vec4_normalize_safe(shz_vec4_t vec) SHZ_NOEXCEPT {
    return shz_vec4_normalize_safe(vec);
}

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_SQRT_H */
