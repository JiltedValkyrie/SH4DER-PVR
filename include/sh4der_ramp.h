/* SH4DER-PVR: one linear ramp between two distances, for effects that fade by distance

   sh4der_ramp.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   RECOMMENDED (from the wiki ledger; wiki/limits/header-recommended.md)
   Start with: no tuning macros; a ramp from the distance where the effect starts to the distance where it is full
               (a fade-out: sh4der_ramp_make(&r, start, end), then sh4der_ramp_down per point)
   Cost:       not measured. ESTIMATE: one FSRRA reciprocal a call (sh4der_ramp_in), or one a sh4der_ramp_make
               (the prepared form), then a multiply and a saturate a point
   Status:     ADVISED; to prove: the same view with a faded effect against a hard step, PVR time and frame time
               (no console run)
   Do:         prepare the ramp once for a loop over many points (sh4der_ramp_make, then _up / _down per point)
   Do not:     use a ramp for a strength the PVR must evaluate per pixel from a texture: this header is the CPU form
   Maturity:   v0.10, host tested

   A linear 0 .. 1 ramp between two distances, saturated at both ends. The same ramp serves a fake anisotropic
   filter's range weight, a shadow tile's strength by camera distance and a depth fade's edge. Two forms: one call
   with the two distances (one reciprocal a call), and a prepared ramp for a loop (the reciprocal taken once, a
   multiply a point). No divide either way.

   The maths is SH4ZAM through sh4der_core.h (the reciprocal is FSRRA, the saturate is shz_saturatef); define
   SH4DER_CORE_NO_SH4ZAM for the plain C path. Host test: test/ramp_test.c.

   Version: v0.10
   The effect: a strength that eases in or out between two distances (a fade by camera distance, a range weight).
   Developed for: shadow tiles and a fake anisotropic filter in the shadow demo.
   Use it for: distance fades of a shadow, a fog, a range of detail, a light's reach.

   \author Jilted Valkyrie
*/

#ifndef SH4DER_RAMP_H
#define SH4DER_RAMP_H

#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief The ramp as one call: 0 up to `from`, 1 from `from + len` on, linear between.
    len <= 0 is a step at `from`. One reciprocal (FSRRA on the SH-4) a call. */
static inline float sh4der_ramp_in(float from, float len, float d) {
    float f;

    if (!(len > 0.0f)) return d >= from ? 1.0f : 0.0f;
    f = (d - from) * sh4der_recip_pos_(len);
    return f <= 0.0f ? 0.0f : f >= 1.0f ? 1.0f : f;
}

/** \brief The same ramp prepared for many points: the start and 1 / length, taken once. */
typedef struct sh4der_ramp {
    float from, inv;        /**< \brief inv: 1 / length; 0 for a step. */
} sh4der_ramp_t;

/** \brief Prepare a ramp from `from` to `to` (to <= from is a step at `from`). One reciprocal. */
static inline void sh4der_ramp_make(sh4der_ramp_t *r, float from, float to) {
    r->from = from;
    r->inv = to > from ? sh4der_recip_pos_(to - from) : 0.0f;
}

/** \brief Rising: 0 up to `from`, 1 from `to` on. */
static inline float sh4der_ramp_up(const sh4der_ramp_t *r, float d) {
    if (!(r->inv > 0.0f)) return d >= r->from ? 1.0f : 0.0f;
    return sh4der_sat_((d - r->from) * r->inv);
}

/** \brief Falling: 1 up to `from`, 0 from `to` on (a strength that fades out with distance). */
static inline float sh4der_ramp_down(const sh4der_ramp_t *r, float d) { return 1.0f - sh4der_ramp_up(r, d); }

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_RAMP_H */
