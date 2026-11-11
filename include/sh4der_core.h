/* SH4DER-PVR

   sh4der_core.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_core.h
    \brief   The parts every SH4DER-PVR effect shares.

    SH4DER-PVR is a set of per-vertex effects for the Dreamcast, each a module
    of its own (sh4der_blade.h, sh4der_holo.h, ...) built on this header:

    - The soft edge every effect uses is SH4DOW-DC's column feather
      (sh4dow_feather_in_ in sh4dow_dc.h). SH4DOW-DC ships with this library
      (the folder sh4dow-dc): put sh4dow-dc/include on the path next to this
      one.
    - The shapes: a tube (a soft column of light orbiting the effect's axis)
      and a blocker (a soft ball on a helix along it).
    - A breath tracker: a pose's own breathing turned into 0 (out) .. 1 (in).
    - A glint: a soft quad's brightness by camera depth.

    The square roots, sines, cosines, distances and clamps are SH4ZAM. Define
    SH4DER_CORE_NO_SH4ZAM to build the same steps in plain C (it also sets
    SH4DOW_DC_NO_SH4ZAM, so both libraries take the same path).

    \author Jilted Valkyrie
*/

#ifndef SH4DER_CORE_H
#define SH4DER_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifndef SH4DER_CORE_NO_SH4ZAM
#include <sh4zam/shz_sh4zam.h>
#else
#include <math.h>
#ifndef SH4DOW_DC_NO_SH4ZAM
#define SH4DOW_DC_NO_SH4ZAM   /* the two libraries take the same path */
#endif
#endif
/* SH4DOW-DC (included, sh4dow-dc/include). The feather of every tube, blocker and core is its own (sh4dow_feather_in_): SH4DER-PVR does not build
   without it. */
#include "sh4dow_dc.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SH4DER_TUBE_MAX    2    /**< \brief Tubes an effect frame holds. */
#define SH4DER_BLOCKER_MAX 2    /**< \brief Blockers an effect frame holds. */

/** \name The breath tracker (sh4der_breath_push())
    @{ */
#define SH4DER_BREATH_WINDOW 90     /**< \brief Frames the breath tracker remembers (one 3.0 s idle loop). */
#define SH4DER_BREATH_SPAN 0.0013f  /**< \brief Least chest travel that counts as a full breath. */
/** @} */

/** \name The glint: the original game's lens flare on a light (sh4der_glint())
    A soft additive quad at a marker: half size radius x scale, its
    brightness full within NEAR of the camera and gone past FAR.
    @{ */
#define SH4DER_GLINT_RADIUS    0.025f  /**< \brief Quad radius before the scale. */
#define SH4DER_GLINT_SCALE_X   4.0f    /**< \brief Horizontal scale (half width 0.10). */
#define SH4DER_GLINT_SCALE_Y   6.0f    /**< \brief Vertical scale (half height 0.15). */
#define SH4DER_GLINT_BRIGHTNESS 0.4f   /**< \brief Brightness at full. */
#define SH4DER_GLINT_NEAR      3.0f    /**< \brief Full brightness within this camera depth. */
#define SH4DER_GLINT_FAR       6.5f    /**< \brief No glint from this camera depth. */
#define SH4DER_GLINT_OFFSET    0.0075f /**< \brief Quad moved this far toward the viewer (depth tested there). */
/** @} */

/** \name The colour ramp (sh4der_ramp_colours(), sh4der_ramp_())
    From a texture's average colour: EDGE is its hue at saturation x EDGE_SAT,
    CORE at x CORE_SAT, both at full value. A vertex of level k takes
    mix(EDGE, CORE, k^2) x k: dim parts saturated, peaks white-hot (the
    energy sword's own texels run from saturation 0.55 to 0.09).
    @{ */
#define SH4DER_RAMP_EDGE_SAT   1.6f    /**< \brief EDGE saturation factor. */
#define SH4DER_RAMP_CORE_SAT   0.25f   /**< \brief CORE saturation factor. */
/** @} */

/** \brief A tube: a soft column of light parallel to the axis, orbiting it. */
typedef struct sh4der_tube {
    float rho;      /**< \brief Orbit radius. */
    float inner;    /**< \brief Full light within this distance of the tube's line. */
    float outer;    /**< \brief No light from this distance (> inner). */
    float weight;   /**< \brief Brightness 0..1. */
    float phase;    /**< \brief Start angle, in turns. */
    int8_t turns;   /**< \brief Whole turns a period; negative turns the other way. */
} sh4der_tube_t;

/** \brief A blocker: a soft ball that slides along the axis on a helix. */
typedef struct sh4der_blocker {
    float x0, x1;   /**< \brief Start and end of a pass along the axis. */
    float rho;      /**< \brief Helix radius. */
    float inner;    /**< \brief Full mask within this distance of the centre. */
    float outer;    /**< \brief No mask from this distance (> inner). */
    float depth;    /**< \brief Light taken at full mask, -1..1 (negative: added). */
    float phase;    /**< \brief Start, in passes. */
    uint8_t passes; /**< \brief Passes a period (at least 1). */
    int8_t turns;   /**< \brief Whole helix turns a period. */
} sh4der_blocker_t;

/** \brief One instance's breath tracker. Zero it (or sh4der_breath_reset) before the first push. */
typedef struct sh4der_breath {
    float ring[SH4DER_BREATH_WINDOW];
    uint16_t head, count;
} sh4der_breath_t;

/** \brief The breath signal of one pose: (chest - pelvis origin) . pelvis forward
    axis (unit), all in one space (world or model). */
float sh4der_breath_raw(const float chest[3], const float pelvis_origin[3], const float pelvis_forward[3]);

/** \brief Forget every frame (a new placement, a new map). */
void sh4der_breath_reset(sh4der_breath_t *breath);

/** \brief Push this frame's raw signal; returns the breath 0 (out) .. 1 (in).
    A non-finite raw is not remembered and returns 0.5. */
float sh4der_breath_push(sh4der_breath_t *breath, float raw);

/** \brief Load time: EDGE and CORE from a texture's average colour (0..1 each),
    then `white` (0..1) mixes both toward white. A black average gives cyan. */
void sh4der_ramp_colours(const float avg[3], float white, float edge[3], float core[3]);

/** \brief The glint's brightness 0..BRIGHTNESS at camera depth `depth` (along the
    view axis), times `fade` (clamped). Behind the camera or NaN: 0. */
float sh4der_glint(float depth, float fade);

/* ------------------------------------------------------------------------ */
/* The effects' shared steps (inline; a trailing underscore: not for callers). */

/* A bit test: under -ffast-math a float compare against NaN folds away. */
static inline int sh4der_finite_(float x) {
    union { float f; uint32_t u; } v;

    v.f = x;
    return (v.u & 0x7f800000u) != 0x7f800000u;
}

static inline int sh4der_unit_(float x) {
    return sh4der_finite_(x) && x >= 0.0f && x <= 1.0f;
}

#ifndef SH4DER_CORE_NO_SH4ZAM
static inline void sh4der_norm3_(float v[3]) {   /* unit length, or unchanged (zero) when it has none */
    const shz_vec3_t n = shz_vec3_normalize_safe(shz_vec3_init(v[0], v[1], v[2]));

    v[0] = n.x; v[1] = n.y; v[2] = n.z;
}
static inline void sh4der_cross_(const float a[3], const float b[3], float out[3]) {
    const shz_vec3_t c = shz_vec3_cross(shz_vec3_init(a[0], a[1], a[2]), shz_vec3_init(b[0], b[1], b[2]));

    out[0] = c.x; out[1] = c.y; out[2] = c.z;
}
static inline float sh4der_len2_(float a, float b) { return shz_vec2_magnitude_sqr(shz_vec2_init(a, b)); }
static inline float sh4der_len3_(float a, float b, float c) { return shz_mag_sqr3f(a, b, c); }
static inline float sh4der_min1_(float x) { return shz_fminf(x, 1.0f); }
static inline float sh4der_sat_(float x) { return shz_saturatef(x); }
static inline float sh4der_frac_(float x) { return shz_fractf(x); }   /* x - floor(x): 0..1, negatives too */
static inline float sh4der_lerp_(float a, float b, float t) { return shz_lerpf(a, b, t); }
static inline float sh4der_div_(float a, float b) { return shz_divf(a, b); }
static inline float sh4der_recip_pos_(float x) { return shz_invf_fsrra(x); }   /* x > 0: every caller checked the sign */
static inline void sh4der_sincos_(float radians, float *s, float *c) {
    const shz_sincos_t sc = shz_sincosf(radians);

    *s = sc.sin;
    *c = sc.cos;
}
#else
static inline void sh4der_norm3_(float v[3]) {
    const float m = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);

    if (m > 1e-12f) {
        v[0] /= m; v[1] /= m; v[2] /= m;
    }
}
static inline void sh4der_cross_(const float a[3], const float b[3], float out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1];
    out[1] = a[2] * b[0] - a[0] * b[2];
    out[2] = a[0] * b[1] - a[1] * b[0];
}
static inline float sh4der_len2_(float a, float b) { return a * a + b * b; }
static inline float sh4der_len3_(float a, float b, float c) { return a * a + b * b + c * c; }
static inline float sh4der_min1_(float x) { return x > 1.0f ? 1.0f : x; }
static inline float sh4der_sat_(float x) { return x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x; }
static inline float sh4der_frac_(float x) { return x - floorf(x); }
static inline float sh4der_lerp_(float a, float b, float t) { return a + (b - a) * t; }
static inline float sh4der_div_(float a, float b) { return a / b; }
static inline float sh4der_recip_pos_(float x) { return 1.0f / x; }
static inline void sh4der_sincos_(float radians, float *s, float *c) {
    *s = sinf(radians);
    *c = cosf(radians);
}
#endif

/* SH4DOW-DC's column feather: 1 inside the core, smoothstep down to 0 at the outer radius. Called only with d2 < outer2.
   Inside the core it answers before any root (FSRRA's 1 / sqrt(0) would make the root of 0 NaN); between the core and the
   outer radius it is sh4dow_feather_in_ itself, whose contract (inner2 < d2 < outer2) the two tests guarantee. */
static inline float sh4der_falloff_(float d2, float inner, float inv_feather) {
    if (d2 <= inner * inner)
        return 1.0f;
    return sh4dow_feather_in_(d2, inner, inv_feather);
}

/* sh4der_falloff_ for any d2: 0 from the outer radius on. */
static inline float sh4der_feather_(float d2, float inner, float outer) {
    if (d2 >= outer * outer)
        return 0.0f;
    return sh4der_falloff_(d2, inner, sh4der_recip_pos_(outer - inner));
}

/* One vertex of level k on the ramp: mix(edge, core, min(k,1)^2) x k, each channel 0..1. */
static inline void sh4der_ramp_(const float edge[3], const float core[3], float k, float out[3]) {
    float m = sh4der_sat_(k);
    int i;

    m *= m;
    for (i = 0; i < 3; i++)
        out[i] = sh4der_sat_(sh4der_lerp_(edge[i], core[i], m) * k);
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_CORE_H */
