/* SH4DER-PVR: one linear ramp between two distances, shared by the tests that fade by distance

   demo/psm/psm_ramp.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   A test's helper, not yet the library's. The same saturated ramp is the fake AF's range weight (fakeaf.h, faf_in), the
   shadow tiles' strength by camera distance (psm_shtile.h) and the form of the depth fade's ramp (ao/d1_spike.c, where
   the PVR evaluates it a pixel from a texture). Two forms: one call with the two distances (one FSRRA reciprocal a call),
   and a prepared ramp for a loop (the reciprocal taken once, a multiply a point). No divide either way.

   Plain C on a host (SH4DER_CORE_NO_SH4ZAM); host test demo/psm/test/psm_shtile_test.c. */

#ifndef PSM_RAMP_H
#define PSM_RAMP_H

#include "sh4der_core.h"

/* 0 up to `from`, 1 from `from + len` on, linear between. len <= 0: a step at `from`. */
static inline float psm_ramp_in(float from, float len, float d) {
    float f;

    if (!(len > 0.0f)) return d >= from ? 1.0f : 0.0f;
    f = (d - from) * sh4der_recip_pos_(len);
    return f <= 0.0f ? 0.0f : f >= 1.0f ? 1.0f : f;
}

/* the same ramp prepared for many points */
typedef struct psm_ramp {
    float from, inv;        /* inv: 1 / len; 0 for a step */
} psm_ramp_t;

static inline void psm_ramp_make(psm_ramp_t *r, float from, float to) {
    r->from = from;
    r->inv = to > from ? sh4der_recip_pos_(to - from) : 0.0f;
}

/* rising: 0 up to from, 1 from `to` on */
static inline float psm_ramp_up(const psm_ramp_t *r, float d) {
    if (!(r->inv > 0.0f)) return d >= r->from ? 1.0f : 0.0f;
    return sh4der_sat_((d - r->from) * r->inv);
}

/* falling: 1 up to from, 0 from `to` on (a strength that fades out with distance) */
static inline float psm_ramp_down(const psm_ramp_t *r, float d) { return 1.0f - psm_ramp_up(r, d); }

#endif /* PSM_RAMP_H */
