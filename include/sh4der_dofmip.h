/* SH4DER-PVR: depth-driven focus blur by texture mip bias, one number a polygon

   include/sh4der_dofmip.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   RECOMMENDED (from the wiki ledger, one console; wiki/limits/header-recommended.md)
   Start with: SH4DER_DOFMIP_REACH = 1.0 and SH4DER_DOFMIP_CURVE = 1.0 (a straight ramp); a near and a far strength of
               1.0 .. 2.0 levels; range = the sharp zone's half depth in metres; one polygon header a code (12 codes,
               4 .. 15), the codes batched, never a header a polygon
   Cost:       not measured. ESTIMATE: prepare, once a frame, 22 thresholds of about 14 square roots and a few multiplies
               each (a straight ramp: none); a polygon or an object, one reciprocal (FSRRA), a few multiplies and a
               search of 4 compares; no pass, no video memory, no copy. The PVR's own cost of a blurrier bias is not
               measured (a SHARPER bias cost +1.0 to +1.5 ms over bilinear in the corridor, PVR-180, why OPEN)
   Status:     ADVISED. Nothing here has run on a console, and the level the PVR picks for a given bias code has
               never been read back (fakeaf's open item, PVR-180). To prove it: a textured plane in depth, the focus
               swept, the mip level read back, the PVR time with the codes against bilinear
   Do:         batch the polygons by code; combine with fakeaf's floor bias through sh4der_dofmip_combine
   Do not:     expect a soft silhouette or soft flat colour: only a texture can blur by mip level; do not call a
               blur proven before the level the PVR picks is read back
   Maturity:   PROTOTYPE v0.2 (host tested)

   WHAT IT DOES. A camera's focus pull as a number a polygon. The PVR picks a texture's mip level from its footprint
   and adds the polygon header's mip bias to it (a code of 1 .. 15, a quarter level each; 4 is 1.00, none; under 4 is
   sharper, over 4 is blurrier: the field fakeaf uses). This header gives each polygon, or each object, the code its
   distance from the focus plane asks for: nothing inside the sharp zone, then more blur toward the near side and the
   far side, each up to its own strength in levels (at most 2.75, code 15). The textures must have mip maps. Nothing is
   read back and nothing is drawn but the polygons already drawn: the whole effect is the polygon header's bias.

   THE RAMP. The blur rises with the change in 1 / z from the focus (what a thin lens's blur circle does), not with the
   change in z: a pull of a few metres blurs a far wall little and a near object a lot, the way a lens does. Let
   u = 1 / z. The sharp zone is from `focus - range` to `focus + range` (the near end held at SH4DER_DOFMIP_NEAR_MIN).
   Past the far end the share is (1 / (focus + range) - u) focus / reach; before the near end it is
   (u - 1 / (focus - range)) focus / reach; each is saturated at 1, raised to the power `curve` and times its side's
   strength (`near_strength` before the zone, `far_strength` past it). `reach` is how far in 1 / z, as a share of
   1 / focus, the blur takes to be full: 1.0 means full at infinity for a zero range, and full at half the focus
   distance on the near side. `curve` shapes the ramp between none and full: 1.0 is straight, above 1.0 holds the blur
   back near the zone and then rises (a late, sharp onset), under 1.0 gives most of the blur early. It is a real
   power, held to SH4DER_DOFMIP_CURVE_MIN .. SH4DER_DOFMIP_CURVE_MAX.

   THE POWER. SH4ZAM's shz_powf, shz_expf and shz_logf are coarse near 1 (shz_powf(0.99, 50) is 3.6, the true value is
   0.605: measured on the host, 2026-11-27), so this header does not use them. The power is taken as the exponent's
   whole part by repeated multiplication and its fraction, to 14 bits, by a chain of square roots (shz_sqrtf is
   exact): the exponent is off by at most 2 to the -15, so the result is within about 0.0006 of the true power at
   worst (for the smallest exponent taken, 1 / 50). A POLYGON does no power at all: prepare turns each of the 11 code steps of each side into the
   share at which that step starts (the share is the step's size over the strength, to the power 1 / curve), and a
   polygon's code is the number of those the polygon's share has passed, found in 4 compares.

   THE LIMITS. A texture only: a flat vertex colour, a silhouette and an edge stay sharp. A step of 0.25 level can
   show where two neighbouring polygons round to different codes (fakeaf's step, the same field). The most a bias
   gives is 2.75 levels, a blur of about 6 texels at the base level's size: not a wide bokeh. A strength asked for
   above that is held at 2.75 (the field's end). For a blur that takes in silhouettes, the frame has to be blurred
   (sh4der_focus, a render target); this header costs nothing and cannot.

   Nothing is kept between frames: prepare takes the frame's whole setting, so a track that jumps changes it in the
   frame it is given, and any easing is the caller's.

   The depth is the distance along the view (the dot of the point's offset from the eye with the eye's forward), the
   same z the PVR's 1 / w carries; sh4der_dofmip_depth gives it. All maths through sh4der_core.h (SH4ZAM on the
   console, plain C with SH4DER_CORE_NO_SH4ZAM). Host test: test/dofmip_test.c.

   Version: v0.2
   The effect: a texture that softens with distance from a focus plane.
   Developed for: a cutscene camera that keys a focus distance, a sharp range, a near and a far blur and a falloff.
   Use it for: close-ups with a soft background, a rack focus, a soft foreground, a tilt look at no cost.

   \author Jilted Valkyrie
*/

#ifndef SH4DER_DOFMIP_H
#define SH4DER_DOFMIP_H

#include <stdint.h>

#include "sh4der_core.h"                    /* the library's maths switch: SH4ZAM on the console, plain C on a host */

#define SH4DER_DOFMIP_CODE_NONE  4u         /* the PVR's code for a bias of 1.00: none (the same as FAF_BIAS_NONE) */
#define SH4DER_DOFMIP_CODE_MAX   15u        /* 3.75: the blurriest the field has, 2.75 levels past none */
#define SH4DER_DOFMIP_CODES      12u        /* the codes this header gives: 4 .. 15 */
#define SH4DER_DOFMIP_LEVELS_MAX 2.75f      /* levels past none at code 15 */
#define SH4DER_DOFMIP_REACH      1.0f       /* share of 1 / focus in 1 / z that takes the blur to full */
#define SH4DER_DOFMIP_CURVE      1.0f       /* the ramp's power: 1.0 is a straight ramp */
#define SH4DER_DOFMIP_CURVE_MIN  0.1f       /* the least power taken */
#define SH4DER_DOFMIP_CURVE_MAX  50.0f      /* the most power taken */
#define SH4DER_DOFMIP_NEAR_MIN   0.1f       /* metres: the sharp zone's near end is never closer than this */
#define SH4DER_DOFMIP_STEPS      11u        /* the code steps a side has above none: codes 5 .. 15 */
#define SH4DER_DOFMIP_POW_BITS   14u        /* bits of an exponent's fraction the power takes */

typedef struct sh4der_dofmip {
    float near_strength;                    /* levels of blur at full before the sharp zone, 0 .. SH4DER_DOFMIP_LEVELS_MAX */
    float far_strength;                     /* levels of blur at full past it */
    float curve;                            /* the ramp's power, held to CURVE_MIN .. CURVE_MAX */
    float k;                                /* focus / reach: turns a change in 1 / z into a share of full blur */
    float u_near;                           /* 1 / (focus - range): the sharp zone's near end, held at NEAR_MIN */
    float u_far;                            /* 1 / (focus + range): its far end */
    float step_near[SH4DER_DOFMIP_STEPS];   /* the share (0 .. 1, rising) at which code 5, 6 .. 15 starts before the zone; 2 when never */
    float step_far[SH4DER_DOFMIP_STEPS];    /* and past it */
    int ok;                                 /* 1 when prepared from good numbers; 0 gives no blur at all */
} sh4der_dofmip_t;

/* x to the power p, for x in 0 .. 1 and p from 0 to a few tens: the whole part of p by repeated multiplication, the
   fraction to SH4DER_DOFMIP_POW_BITS bits by a chain of square roots. Within about 0.0006 of the true power (see the
   header's note). 0 for x at or under 0 and p over 0; 1 for x at or over 1. */
static inline float sh4der_dofmip_pow_(float x, float p) {
    float r = 1.0f, s = x, f, b;
    unsigned n, m, i;

    if (!(x > 0.0f)) return p > 0.0f ? 0.0f : 1.0f;
    if (x >= 1.0f || !(p > 0.0f)) return 1.0f;
    n = (unsigned)p;
    f = p - (float)n + 0.5f / (float)(1u << SH4DER_DOFMIP_POW_BITS);     /* round to the nearest of the fraction's steps */
    if (f >= 1.0f) { n++; f -= 1.0f; }
    for (b = x, m = n; m; m >>= 1) { if (m & 1u) r *= b; b *= b; }
    for (i = 0; i < SH4DER_DOFMIP_POW_BITS; i++) {
        s = sh4der_sqrt_(s);                                           /* x to the 2 to the -(i + 1) */
        f *= 2.0f;
        if (f >= 1.0f) { r *= s; f -= 1.0f; }
    }
    return r;
}

/* One side's steps: step j (1 .. 11) of the code starts at the levels (j - 1/2) / 4, that is the share
   (j - 1/2) / (4 strength) of the strength, which is a ramp value of that share to the power 1 / curve. */
static inline void sh4der_dofmip_steps_(float *step, float strength, float curve) {
    const float inv = sh4der_recip_pos_(curve);
    unsigned j;

    for (j = 0; j < SH4DER_DOFMIP_STEPS; j++) {
        const float levels = ((float)j + 0.5f) * 0.25f;

        if (!(strength > 0.0f) || levels > strength) step[j] = 2.0f;               /* never reached: a share is at most 1 */
        else step[j] = curve == 1.0f ? levels * sh4der_recip_pos_(strength) : sh4der_dofmip_pow_(levels * sh4der_recip_pos_(strength), inv);
    }
}

/* Take a frame's focus: `focus` metres to the sharp plane (at least SH4DER_DOFMIP_NEAR_MIN), `range` metres either side
   of it that stay sharp (0 or more), `near_strength` and `far_strength` levels of blur at full (each clamped to
   0 .. SH4DER_DOFMIP_LEVELS_MAX), `curve` the ramp's power (0 or under takes SH4DER_DOFMIP_CURVE; held to CURVE_MIN ..
   CURVE_MAX) and `reach` (0 or under takes SH4DER_DOFMIP_REACH). Once a frame, from the keyed tracks; nothing is kept
   between frames, so a track that jumps changes the settings in the frame it is given. Returns 1; 0 when a number
   cannot be used (not finite, focus too near, range under 0), and the focus then blurs nothing. */
static inline int sh4der_dofmip_prepare(sh4der_dofmip_t *d, float focus, float range, float near_strength, float far_strength,
                                        float curve, float reach) {
    float near_end;
    unsigned j;

    if (!d) return 0;
    d->ok = 0; d->near_strength = 0.0f; d->far_strength = 0.0f; d->curve = SH4DER_DOFMIP_CURVE; d->k = 0.0f; d->u_near = 0.0f; d->u_far = 0.0f;
    for (j = 0; j < SH4DER_DOFMIP_STEPS; j++) d->step_near[j] = d->step_far[j] = 2.0f;
    if (!(focus >= SH4DER_DOFMIP_NEAR_MIN) || !(focus < 1.0e6f) || !(range >= 0.0f) || !(range < 1.0e6f)) return 0;
    if (!(near_strength == near_strength) || !(far_strength == far_strength) || !(curve == curve) || !(reach == reach)) return 0;
    if (!(reach > 0.0f)) reach = SH4DER_DOFMIP_REACH;
    if (!(curve > 0.0f)) curve = SH4DER_DOFMIP_CURVE;
    d->curve = sh4der_clamp_(curve, SH4DER_DOFMIP_CURVE_MIN, SH4DER_DOFMIP_CURVE_MAX);
    d->near_strength = sh4der_clamp_(near_strength, 0.0f, SH4DER_DOFMIP_LEVELS_MAX);
    d->far_strength = sh4der_clamp_(far_strength, 0.0f, SH4DER_DOFMIP_LEVELS_MAX);
    near_end = focus - range;
    if (near_end < SH4DER_DOFMIP_NEAR_MIN) near_end = SH4DER_DOFMIP_NEAR_MIN;
    d->u_near = sh4der_recip_pos_(near_end);
    d->u_far = sh4der_recip_pos_(focus + range);
    d->k = focus * sh4der_recip_pos_(reach);
    sh4der_dofmip_steps_(d->step_near, d->near_strength, d->curve);
    sh4der_dofmip_steps_(d->step_far, d->far_strength, d->curve);
    d->ok = 1;
    return 1;
}

/* The depth along the view of a point: the dot of its offset from the eye with the eye's forward (unit). Negative or
   zero is behind the eye (the polygon is not drawn, or is cut). */
static inline float sh4der_dofmip_depth(const float eye[3], const float forward[3], const float p[3]) {
    const float off[3] = { p[0] - eye[0], p[1] - eye[1], p[2] - eye[2] };

    return sh4der_dot3_(off, forward);
}

/* Where a point at depth z stands: 0 inside the sharp zone (or when nothing is prepared), 1 before it, 2 past it;
   *share is its straight ramp value, 0 .. 1. */
static inline int sh4der_dofmip_side_(const sh4der_dofmip_t *d, float z, float *share) {
    float u;

    *share = 0.0f;
    if (!d || !d->ok || !(z > 0.001f)) return 0;
    u = sh4der_recip_pos_(z);
    if (u > d->u_near) { *share = sh4der_sat_((u - d->u_near) * d->k); return 1; }
    if (u < d->u_far) { *share = sh4der_sat_((d->u_far - u) * d->k); return 2; }
    return 0;
}

/* The blur of a point at depth z, in levels past none: 0 inside the sharp zone, rising to the side's strength. A depth
   under 1 mm or an unprepared focus gives 0. */
static inline float sh4der_dofmip_levels(const sh4der_dofmip_t *d, float z) {
    float t;
    const int side = sh4der_dofmip_side_(d, z, &t);

    if (!side) return 0.0f;
    if (d->curve != 1.0f) t = sh4der_dofmip_pow_(t, d->curve);
    return (side == 1 ? d->near_strength : d->far_strength) * t;
}

/* The PVR's mip bias code for a number of levels past none: 4 + levels x 4, rounded, 4 .. 15. Never sharper than none. */
static inline uint8_t sh4der_dofmip_code_of(float levels) {
    unsigned c;

    if (!(levels > 0.0f)) return (uint8_t)SH4DER_DOFMIP_CODE_NONE;
    if (levels > SH4DER_DOFMIP_LEVELS_MAX) levels = SH4DER_DOFMIP_LEVELS_MAX;
    c = SH4DER_DOFMIP_CODE_NONE + (unsigned)(levels * 4.0f + 0.5f);
    return (uint8_t)(c > SH4DER_DOFMIP_CODE_MAX ? SH4DER_DOFMIP_CODE_MAX : c);
}

/* The code a polygon or an object at depth z takes: the usual call. For a polygon, give the depth of its middle (or the
   mean of its three vertices); for an object, its centre. No power is taken here: the steps prepare made are searched
   (4 compares). It gives the code sh4der_dofmip_code_of(sh4der_dofmip_levels(d, z)) gives, except within the power's
   own error of a step. */
static inline uint8_t sh4der_dofmip_code(const sh4der_dofmip_t *d, float z) {
    float t;
    const int side = sh4der_dofmip_side_(d, z, &t);
    const float *step;
    unsigned lo = 0u, hi = SH4DER_DOFMIP_STEPS;

    if (!side) return (uint8_t)SH4DER_DOFMIP_CODE_NONE;
    step = side == 1 ? d->step_near : d->step_far;
    while (lo < hi) {                                           /* the steps passed: the steps are rising */
        const unsigned mid = (lo + hi) >> 1;

        if (step[mid] <= t) lo = mid + 1u; else hi = mid;
    }
    return (uint8_t)(SH4DER_DOFMIP_CODE_NONE + lo);
}

/* The batch an code goes to: 0 .. SH4DER_DOFMIP_CODES - 1 (one polygon header a bucket). */
static inline unsigned sh4der_dofmip_bucket(uint8_t code) {
    return code <= SH4DER_DOFMIP_CODE_NONE ? 0u : (unsigned)(code > SH4DER_DOFMIP_CODE_MAX ? SH4DER_DOFMIP_CODE_MAX : code) - SH4DER_DOFMIP_CODE_NONE;
}

/* Two biases on one polygon, the codes' sum: a floor's sharper code from fakeaf's faf_bias_code (under 4) and this
   header's blurrier one (over 4). The result is the first plus the second, less one none, held to 1 .. 15. */
static inline uint8_t sh4der_dofmip_combine(uint8_t a, uint8_t b) {
    int c = (int)a + (int)b - (int)SH4DER_DOFMIP_CODE_NONE;

    return (uint8_t)(c < 1 ? 1 : c > (int)SH4DER_DOFMIP_CODE_MAX ? (int)SH4DER_DOFMIP_CODE_MAX : c);
}

#endif /* SH4DER_DOFMIP_H */
