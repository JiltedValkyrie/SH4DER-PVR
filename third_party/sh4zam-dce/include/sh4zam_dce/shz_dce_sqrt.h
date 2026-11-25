/* SH4ZAM-DCE: square roots and lengths

   shz_dce_sqrt.h
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.
*/

/** \file    shz_dce_sqrt.h
    \brief   SH4ZAM's guarded square roots and lengths (Falco Girgis) under shz_dce_ names. As
             built by default each routine here IS SH4ZAM's routine of the same name, called
             underneath: one implementation, SH4ZAM's. The reordered bodies described below (the
             zero test moved into the shadow of FSRRA; same signatures, same bits) are compiled
             only with SHZ_DCE_REWORK_ROOTS 1, for a console test: a console did not confirm
             their gain.

    SHZ_DCE_REWORK_ROOTS (1 or 0, default 0, set before the include):
    - 0: shz_dce_sqrtf(x) is shz_sqrtf(x), and so on for every routine of this header. SH4ZAM
      has these routines, so nothing here replaces them; the names exist so that code written
      against this header, and the lines of shz_dce_override.h, keep compiling. The barrier
      switches (SHZ_DCE_OPAQUE_...) do nothing in this build.
    - 1: the reordered bodies. Everything from here to the end of this comment describes them.

    SH4ZAM 0.10's guarded routines are written `x == 0 ? 0 : f(FSRRA(x))`: shz_sqrtf,
    shz_inv_sqrtf, shz_hypotf, the vector magnitudes, distances and safe normalisations. Compiled,
    the compare comes first, its branch waits two cycles for the T bit, and the SH-4 issues in
    order: FSRRA and everything after it wait behind that branch.

    Here FSRRA is issued first and the test is taken while it runs:

        r = f(FSRRA(x));  if (x == 0) r = 0;

    The operations and their operands are the ones SH4ZAM executes, so every result off the zero
    path has the same bits.

    What it is worth: two answers.
    - The pipeline model (cycles E a call in a loop over an array, SH4ZAM 0.10 -> here as the
      switches build it, -O2 and -O3 alike but where two figures are given): sqrtf 16 -> 14,
      inv_sqrtf 12 -> 11 / 9, hypotf 22 -> 20, vec3_magnitude 25 -> 23, vec3_magnitude_inv
      21 -> 20, vec3_distance 31 -> 29, vec3_normalize_safe 28 -> 25, vec4_magnitude 28 -> 26.
      With every barrier on, -O3 takes one or two more off most of them (sqrtf 12,
      vec3_magnitude 21). Four independent roots an element: 43 -> 35, and 39 with the barrier.
    - A console (single calls, cycles cached / uncached, reported by Falco Girgis; current
      SH4ZAM, this rework with the barrier off, with it on): inv_sqrtf 25 / 100, 26 / 88, 24 / 84;
      sqrtf 28 / 74, 29 / 102, 29 / 98; four roots in a row 65 / 126, 59 / 138, 66 / 156;
      vec2_normalize_safe 44 / 136, 40 / 138, 40 / 133; vec3 52 / 134, 60 / 212, 63 / 220;
      vec4 50 / 188, 65 / 250, 66 / 260. Current SH4ZAM's shz_sqrtf is FSQRT, which has no zero
      test to move.
    The console is the truth and the model was wrong about single calls. Take these routines one
    group at a time and measure: shz_dce_override.h has a switch a group.

    The zero path, and the barrier. SH4ZAM's source returns the constant 0. Built with
    -ffast-math the compiler is free to hand back the operand that compared equal to zero, and
    for SH4ZAM's routines it does (measured, -O2 and -O3): shz_sqrtf(-0) is -0, and a denormal,
    which the SH-4 compares as zero (FPSCR.DN = 1), comes back as that denormal. Each group here
    has a switch (SHZ_DCE_OPAQUE_SQRT, _INV_SQRT, _MAGNITUDE, _DISTANCE, _NORMALIZE2 / 3 / 4;
    shz_dce_cdefs.h has their defaults and what the barrier is):
    - on: the zero goes through shz_dce_opaquef(), the result is +0 in every build;
    - off: the zero is the plain constant, as in SH4ZAM's routine, and the compiler treats both
      alike.

    One more difference on the zero path: FSRRA is executed on the zero (+-0 gives +-infinity,
    FPSCR flag Z) and the product 0 x infinity is discarded (flag V). Both are quiet when the
    FPU's exception enables are off, which is how KallistiOS runs. A program that enables the
    divide-by-zero or the invalid-operation exception keeps SH4ZAM's routines.

    Alignment: none (values in registers).
    Error bound: SH4ZAM's own (FSRRA's); nothing is added.
    Bit-identical to the SH4ZAM 0.10 routine of the same name: YES in a strict build, for every
    input. In a -ffast-math build: YES for every input off the zero path; on it SH4ZAM's routine
    returns its operand (above); with the barrier on this one returns +0, with it off the
    operand, equal as numbers in both cases. Tests: memcmp on the SH-4, both states of every
    switch, over sweeps with zeros of both signs, denormals, squares that underflow, negatives,
    infinities. Where the compiler can prove the operand is not zero it removes the test from
    both forms and the code is the same.

    Use it for: call sites of the SH4ZAM 0.10 routines named above where a measurement on the
    console says it pays. shz_dce_override.h maps the SH4ZAM names onto these, a group at a time.

    Version: v0.10
*/

#ifndef SHZ_DCE_SQRT_H
#define SHZ_DCE_SQRT_H

#include "shz_dce_cdefs.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief 0 (the default): every routine of this header calls SH4ZAM's routine of the same name. 1: the reordered bodies. */
#ifndef SHZ_DCE_REWORK_ROOTS
#define SHZ_DCE_REWORK_ROOTS 0
#endif

/* One line a public routine: the reordered body, or SH4ZAM's own routine. */
#if SHZ_DCE_REWORK_ROOTS
#define SHZ_DCE_ROOT_(reordered, sh4zam) (reordered)
#else
#define SHZ_DCE_ROOT_(reordered, sh4zam) (sh4zam)
#endif

#if SHZ_DCE_REWORK_ROOTS
/** \brief shz_dce_sqrtf() with the barrier chosen by the caller: `opaque` a constant, not 0 for
    a +0 behind shz_dce_opaquef() on the zero path, 0 for the plain constant. The public routines
    pass their group's switch; the tests run both. */
SHZ_FORCE_INLINE float shz_dce_sqrtf_o_(float x, int opaque) SHZ_NOEXCEPT {
#ifdef SHZ_GNUC
    if (__builtin_constant_p(x))
        return shz_sqrtf(x);
#endif
    const float r = shz_inv_sqrtf_fsrra(x) * x;
    return SHZ_UNLIKELY(x == 0.0f) ? SHZ_DCE_ZERO_(opaque) : r;
}

/** \brief shz_dce_inv_sqrtf() with the barrier chosen by the caller (see shz_dce_sqrtf_o_). */
SHZ_FORCE_INLINE float shz_dce_inv_sqrtf_o_(float x, int opaque) SHZ_NOEXCEPT {
#ifdef SHZ_GNUC
    if (__builtin_constant_p(x))
        return shz_inv_sqrtf(x);
#endif
    const float r = shz_inv_sqrtf_fsrra(x);
    return SHZ_UNLIKELY(x == 0.0f) ? SHZ_DCE_ZERO_(opaque) : r;
}

/** \brief shz_dce_vec2_normalize_safe() with the barrier chosen by the caller (see
    shz_dce_sqrtf_o_). */
SHZ_FORCE_INLINE shz_vec2_t shz_dce_vec2_normalize_safe_o_(shz_vec2_t vec, int opaque) SHZ_NOEXCEPT {
    const float sqr_mag = shz_vec2_magnitude_sqr(vec);
    const shz_vec2_t unit = shz_vec2_scale(vec, shz_inv_sqrtf_fsrra(sqr_mag));
    return SHZ_UNLIKELY(sqr_mag == 0.0f) ? shz_vec2_fill(SHZ_DCE_ZERO_(opaque)) : unit;
}

/** \brief See shz_dce_vec2_normalize_safe_o_. */
SHZ_FORCE_INLINE shz_vec3_t shz_dce_vec3_normalize_safe_o_(shz_vec3_t vec, int opaque) SHZ_NOEXCEPT {
    const float sqr_mag = shz_vec3_magnitude_sqr(vec);
    const shz_vec3_t unit = shz_vec3_scale(vec, shz_inv_sqrtf_fsrra(sqr_mag));
    return SHZ_UNLIKELY(sqr_mag == 0.0f) ? shz_vec3_fill(SHZ_DCE_ZERO_(opaque)) : unit;
}

/** \brief See shz_dce_vec2_normalize_safe_o_. */
SHZ_FORCE_INLINE shz_vec4_t shz_dce_vec4_normalize_safe_o_(shz_vec4_t vec, int opaque) SHZ_NOEXCEPT {
    const float sqr_mag = shz_vec4_magnitude_sqr(vec);
    const shz_vec4_t unit = shz_vec4_scale(vec, shz_inv_sqrtf_fsrra(sqr_mag));
    return SHZ_UNLIKELY(sqr_mag == 0.0f) ? shz_vec4_fill(SHZ_DCE_ZERO_(opaque)) : unit;
}
#endif /* SHZ_DCE_REWORK_ROOTS */

/** \brief Rework of SH4ZAM 0.10's shz_sqrtf (Falco Girgis): `x == 0 ? 0 : FSRRA(x) * x`, the test
    taken in FSRRA's shadow. Same special cases: 0 for a zero of either sign and for a denormal
    on the SH-4; NaN for a negative. Bit-identical to shz_sqrtf: YES (the zero path under
    -ffast-math: see the file comment). Barrier: SHZ_DCE_OPAQUE_SQRT. Cycles E: 16 -> 14; a
    console, single calls, cached: 29, against 28 for current SH4ZAM's FSQRT. */
SHZ_FORCE_INLINE float shz_dce_sqrtf(float x) SHZ_NOEXCEPT {
    return SHZ_DCE_ROOT_(shz_dce_sqrtf_o_(x, SHZ_DCE_OPAQUE_SQRT), shz_sqrtf(x));
}

/** \brief Rework of SH4ZAM 0.10's shz_inv_sqrtf (Falco Girgis): `x == 0 ? 0 : FSRRA(x)`, the test
    taken in FSRRA's shadow. Bit-identical to shz_inv_sqrtf: YES (as shz_dce_sqrtf). Barrier:
    SHZ_DCE_OPAQUE_INV_SQRT. Cycles E: 12 -> 11 (-O2), 9 (-O3); a console, single calls, cached:
    25 -> 24 with the barrier, 26 without. */
SHZ_FORCE_INLINE float shz_dce_inv_sqrtf(float x) SHZ_NOEXCEPT {
    return SHZ_DCE_ROOT_(shz_dce_inv_sqrtf_o_(x, SHZ_DCE_OPAQUE_INV_SQRT), shz_inv_sqrtf(x));
}

/** \brief Rework of SH4ZAM's shz_hypotf: the guarded root of x x + y y. Bit-identical to
    shz_hypotf: YES (as shz_dce_sqrtf). Barrier: SHZ_DCE_OPAQUE_SQRT. Cycles E: 22 -> 20 (-O2),
    23 -> 20 (-O3). */
SHZ_FORCE_INLINE float shz_dce_hypotf(float x, float y) SHZ_NOEXCEPT {
#ifdef SHZ_GNUC
    if (__builtin_constant_p(x) && __builtin_constant_p(y))
        return shz_hypotf(x, y);
#endif
    return SHZ_DCE_ROOT_(shz_dce_sqrtf_o_((x * x) + (y * y), SHZ_DCE_OPAQUE_SQRT), shz_hypotf(x, y));
}

/** \brief Reworks of SH4ZAM's shz_vec2_magnitude / shz_vec3_magnitude / shz_vec4_magnitude: the
    length, 0 for a vector whose squared length is zero. Bit-identical: YES. Barrier:
    SHZ_DCE_OPAQUE_MAGNITUDE. Cycles E (vec3): 25 -> 23. */
SHZ_FORCE_INLINE float shz_dce_vec2_magnitude(shz_vec2_t vec) SHZ_NOEXCEPT {
    return SHZ_DCE_ROOT_(shz_dce_sqrtf_o_(shz_vec2_magnitude_sqr(vec), SHZ_DCE_OPAQUE_MAGNITUDE), shz_vec2_magnitude(vec));
}

/** \brief See shz_dce_vec2_magnitude. */
SHZ_FORCE_INLINE float shz_dce_vec3_magnitude(shz_vec3_t vec) SHZ_NOEXCEPT {
    return SHZ_DCE_ROOT_(shz_dce_sqrtf_o_(shz_vec3_magnitude_sqr(vec), SHZ_DCE_OPAQUE_MAGNITUDE), shz_vec3_magnitude(vec));
}

/** \brief See shz_dce_vec2_magnitude. */
SHZ_FORCE_INLINE float shz_dce_vec4_magnitude(shz_vec4_t vec) SHZ_NOEXCEPT {
    return SHZ_DCE_ROOT_(shz_dce_sqrtf_o_(shz_vec4_magnitude_sqr(vec), SHZ_DCE_OPAQUE_MAGNITUDE), shz_vec4_magnitude(vec));
}

/** \brief Reworks of SH4ZAM's shz_vec2_magnitude_inv / shz_vec3_magnitude_inv /
    shz_vec4_magnitude_inv: one over the length, 0 for a vector whose squared length is zero.
    Bit-identical: YES. Barrier: SHZ_DCE_OPAQUE_MAGNITUDE. Cycles E (vec3): 21 -> 20. */
SHZ_FORCE_INLINE float shz_dce_vec2_magnitude_inv(shz_vec2_t vec) SHZ_NOEXCEPT {
    return SHZ_DCE_ROOT_(shz_dce_inv_sqrtf_o_(shz_vec2_magnitude_sqr(vec), SHZ_DCE_OPAQUE_MAGNITUDE), shz_vec2_magnitude_inv(vec));
}

/** \brief See shz_dce_vec2_magnitude_inv. */
SHZ_FORCE_INLINE float shz_dce_vec3_magnitude_inv(shz_vec3_t vec) SHZ_NOEXCEPT {
    return SHZ_DCE_ROOT_(shz_dce_inv_sqrtf_o_(shz_vec3_magnitude_sqr(vec), SHZ_DCE_OPAQUE_MAGNITUDE), shz_vec3_magnitude_inv(vec));
}

/** \brief See shz_dce_vec2_magnitude_inv. */
SHZ_FORCE_INLINE float shz_dce_vec4_magnitude_inv(shz_vec4_t vec) SHZ_NOEXCEPT {
    return SHZ_DCE_ROOT_(shz_dce_inv_sqrtf_o_(shz_vec4_magnitude_sqr(vec), SHZ_DCE_OPAQUE_MAGNITUDE), shz_vec4_magnitude_inv(vec));
}

/** \brief Reworks of SH4ZAM's shz_vec2_distance / shz_vec3_distance / shz_vec4_distance: the
    length of the difference. Bit-identical: YES. Barrier: SHZ_DCE_OPAQUE_DISTANCE. Cycles E
    (vec3): 31 -> 29. */
SHZ_FORCE_INLINE float shz_dce_vec2_distance(shz_vec2_t vec1, shz_vec2_t vec2) SHZ_NOEXCEPT {
    return SHZ_DCE_ROOT_(shz_dce_sqrtf_o_(shz_vec2_magnitude_sqr(shz_vec2_sub(vec1, vec2)), SHZ_DCE_OPAQUE_DISTANCE), shz_vec2_distance(vec1, vec2));
}

/** \brief See shz_dce_vec2_distance. */
SHZ_FORCE_INLINE float shz_dce_vec3_distance(shz_vec3_t vec1, shz_vec3_t vec2) SHZ_NOEXCEPT {
    return SHZ_DCE_ROOT_(shz_dce_sqrtf_o_(shz_vec3_magnitude_sqr(shz_vec3_sub(vec1, vec2)), SHZ_DCE_OPAQUE_DISTANCE), shz_vec3_distance(vec1, vec2));
}

/** \brief See shz_dce_vec2_distance. */
SHZ_FORCE_INLINE float shz_dce_vec4_distance(shz_vec4_t vec1, shz_vec4_t vec2) SHZ_NOEXCEPT {
    return SHZ_DCE_ROOT_(shz_dce_sqrtf_o_(shz_vec4_magnitude_sqr(shz_vec4_sub(vec1, vec2)), SHZ_DCE_OPAQUE_DISTANCE), shz_vec4_distance(vec1, vec2));
}

/** \brief Reworks of SH4ZAM's shz_vec2_normalize_safe / shz_vec3_normalize_safe /
    shz_vec4_normalize_safe: the unit vector, the zero vector for a vector whose squared length
    is zero. The scaling is done while the test resolves. Bit-identical: YES. Barrier:
    SHZ_DCE_OPAQUE_NORMALIZE2 / 3 / 4. Cycles E (vec3): 28 -> 25 (-O2), 27 -> 25 (-O3); a
    console, single calls, cached, barrier off: vec2 44 -> 40, vec3 52 -> 60, vec4 50 -> 65. */
SHZ_FORCE_INLINE shz_vec2_t shz_dce_vec2_normalize_safe(shz_vec2_t vec) SHZ_NOEXCEPT {
    return SHZ_DCE_ROOT_(shz_dce_vec2_normalize_safe_o_(vec, SHZ_DCE_OPAQUE_NORMALIZE2), shz_vec2_normalize_safe(vec));
}

/** \brief See shz_dce_vec2_normalize_safe. */
SHZ_FORCE_INLINE shz_vec3_t shz_dce_vec3_normalize_safe(shz_vec3_t vec) SHZ_NOEXCEPT {
    return SHZ_DCE_ROOT_(shz_dce_vec3_normalize_safe_o_(vec, SHZ_DCE_OPAQUE_NORMALIZE3), shz_vec3_normalize_safe(vec));
}

/** \brief See shz_dce_vec2_normalize_safe. */
SHZ_FORCE_INLINE shz_vec4_t shz_dce_vec4_normalize_safe(shz_vec4_t vec) SHZ_NOEXCEPT {
    return SHZ_DCE_ROOT_(shz_dce_vec4_normalize_safe_o_(vec, SHZ_DCE_OPAQUE_NORMALIZE4), shz_vec4_normalize_safe(vec));
}

#undef SHZ_DCE_ROOT_

#ifdef __cplusplus
}
#endif

#endif /* SHZ_DCE_SQRT_H */
