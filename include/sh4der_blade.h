/* SH4DER-PVR: the blade effect

   sh4der_blade.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_blade.h
    \brief   Moving glow on a chosen material group, from tubes and blockers.

    Built on SH4DOW-DC's column. There a column darkens what lies inside it;
    here the same soft-edged column is a TUBE of light that orbits an axis
    (the effect's x axis), and SH4DOW-DC's limb ball is a BLOCKER that slides
    along the axis on a helix and masks the light as it passes through. A
    pulse breathes over the whole group. One period closes exactly, so the
    effect loops.

    The vertices are in the effect's own space: x along the axis, the axis
    line at (y, z) = params->axis. A weapon blade passes its node's space, so
    the effect needs no bone of its own.

    Call sh4der_blade_prepare() once per instance per frame with the game tick (plus
    a per-instance phase) and the fade, then per vertex sh4der_blade_intensity(), or
    sh4der_blade_shade() over a run. Multiply your colour by the intensity and draw
    it additively; sh4der_blade_argb() packs that for you. frame->light is the
    matching point-light intensity.

    Which parts get the effect is the client's choice, by material group
    (shader): this library sees only the vertices it is given.

    The tube and blocker shapes, the feather (SH4DOW-DC's, required) and the
    SH4ZAM steps are SH4DER-PVR's shared parts (sh4der_core.h).

    \author Jilted Valkyrie
*/

#ifndef SH4DER_BLADE_H
#define SH4DER_BLADE_H

#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Shipped tuning: the energy sword (sh4der_blade_params_default())
    Lengths are in vertex units (Halo world units on the sword's node), the
    period in ticks.
    @{ */
#define SH4DER_BLADE_PERIOD          192     /**< \brief Ticks a period: one turn of tube A (6.4 s at 30 Hz). */
#define SH4DER_BLADE_PULSES          2       /**< \brief Glow pulses a period. */
#define SH4DER_BLADE_GLOW_BASE       0.55f   /**< \brief Glow at the bottom of a pulse. */
#define SH4DER_BLADE_GLOW_AMP        0.45f   /**< \brief Glow added at the top of a pulse. */
#define SH4DER_BLADE_FLOOR           0.40f   /**< \brief Share of the glow away from every tube. */
#define SH4DER_BLADE_LIGHT_SWING     0.15f   /**< \brief Point light dip at the bottom of a pulse. */
#define SH4DER_BLADE_MIN_GLOW        0.25f   /**< \brief Lowest glow a pulse reaches (the blade never goes fully transparent; a pulse AMOUNT of 1 would reach 0). */
#define SH4DER_BLADE_AXIS_Z          0.008f  /**< \brief Axis height (the blades' node origin). */
#define SH4DER_BLADE_A_RHO           0.055f  /**< \brief Tube A orbit radius. */
#define SH4DER_BLADE_A_INNER         0.012f  /**< \brief Tube A core radius. */
#define SH4DER_BLADE_A_OUTER         0.035f  /**< \brief Tube A outer radius. */
#define SH4DER_BLADE_B_RHO           0.040f  /**< \brief Tube B orbit radius. */
#define SH4DER_BLADE_B_INNER         0.010f  /**< \brief Tube B core radius. */
#define SH4DER_BLADE_B_OUTER         0.028f  /**< \brief Tube B outer radius. */
#define SH4DER_BLADE_B_WEIGHT        0.6f    /**< \brief Tube B brightness. */
#define SH4DER_BLADE_B_PHASE         0.25f   /**< \brief Tube B start, in turns. */
#define SH4DER_BLADE_K_X0            -0.10f  /**< \brief Blocker start along the axis (behind the hilt). */
#define SH4DER_BLADE_K_X1            0.40f   /**< \brief Blocker end along the axis (past the tips). */
#define SH4DER_BLADE_K_RHO           0.030f  /**< \brief Blocker helix radius. */
#define SH4DER_BLADE_K_INNER         0.020f  /**< \brief Blocker core radius. */
#define SH4DER_BLADE_K_OUTER         0.055f  /**< \brief Blocker outer radius. */
#define SH4DER_BLADE_K_DEPTH         0.85f   /**< \brief Light the blocker core takes away. */
#define SH4DER_BLADE_K_PASSES        2       /**< \brief Blocker passes a period. */
#define SH4DER_BLADE_FADE_X0         -0.065f /**< \brief The blade's base along the axis: intensity 0 here (no hard edge at the cut). */
#define SH4DER_BLADE_FADE_X1         0.0f    /**< \brief Full intensity from here on (smoothstep between). */
/** @} */

/** \name The MAX WHITE test preset (sh4der_blade_params_white())
    The same tubes, blocker and pulse run white-hot: the base lifted and the
    blocker inverted (a negative depth brightens: its knot is the brightest
    part). Draw it with a white ramp. Never a flat silhouette.
    @{ */
#define SH4DER_BLADE_WHITE_GLOW_BASE 0.80f   /**< \brief Glow at the bottom of a pulse. */
#define SH4DER_BLADE_WHITE_GLOW_AMP  0.20f   /**< \brief Glow added at the top. */
#define SH4DER_BLADE_WHITE_FLOOR     0.75f   /**< \brief Share of the glow with no tube near. */
#define SH4DER_BLADE_WHITE_K_DEPTH   -0.6f   /**< \brief The blocker brightens by this much at its core. */
/** @} */

typedef struct sh4der_blade_params {
    uint32_t period;        /**< \brief Ticks a period (at least 1). */
    uint8_t pulses;         /**< \brief Glow pulses a period. */
    uint8_t tube_count;     /**< \brief 0..SH4DER_TUBE_MAX. */
    uint8_t blocker_count;  /**< \brief 0..SH4DER_BLOCKER_MAX. */
    float axis[2];          /**< \brief The axis line's (y, z). */
    float glow_base, glow_amp;  /**< \brief Glow = base + amp x pulse (0..1). */
    float floor;            /**< \brief Share of the glow a vertex gets with no tube near, 0..1. */
    float light_swing;      /**< \brief Light = 1 - swing x (1 - pulse); 0 keeps it constant. */
    float fade_x0, fade_x1; /**< \brief The intensity fades in from 0 at fade_x0 to full at fade_x1 along the axis (fade_x1 > fade_x0). */
    float ripple_freq;      /**< \brief Ripples along the axis, cycles a unit (>= 0); they travel one ripple a pulse. */
    float ripple_depth;     /**< \brief Light a ripple's trough takes away, 0..1; 0 (the default) turns them off. */
    sh4der_tube_t tube[SH4DER_TUBE_MAX];
    sh4der_blocker_t blocker[SH4DER_BLOCKER_MAX];
} sh4der_blade_params_t;

/** \brief One instance's frame: sh4der_blade_prepare()'s output. */
typedef struct sh4der_blade_frame {
    float pulse;            /**< \brief 0..1 this frame. */
    float glow;             /**< \brief Glow this frame (before the fade). */
    float light;            /**< \brief Point-light intensity this frame, fade applied. */
    float fade;             /**< \brief 0..1, as given. */
    float lo, hi;           /**< \brief glow x floor, glow x (1 - floor). */
    float fade_x0, inv_fade_len;    /**< \brief The base fade (params' fade_x0, 1 / (fade_x1 - fade_x0)). */
    float ripple_k, ripple_phase, ripple_depth;   /**< \brief 2 pi x ripple_freq, this frame's travel (radians), the depth (0: none). */
    int tube_count, blocker_count;
    float tube_y[SH4DER_TUBE_MAX], tube_z[SH4DER_TUBE_MAX];
    float tube_inner[SH4DER_TUBE_MAX], tube_outer2[SH4DER_TUBE_MAX];
    float tube_inv_feather[SH4DER_TUBE_MAX], tube_weight[SH4DER_TUBE_MAX];
    float blk_x[SH4DER_BLOCKER_MAX], blk_y[SH4DER_BLOCKER_MAX], blk_z[SH4DER_BLOCKER_MAX];
    float blk_inner[SH4DER_BLOCKER_MAX], blk_outer2[SH4DER_BLOCKER_MAX];
    float blk_inv_feather[SH4DER_BLOCKER_MAX], blk_depth[SH4DER_BLOCKER_MAX];
} sh4der_blade_frame_t;

/** \brief The energy sword's tuning (the SH4DER_* values). */
void sh4der_blade_params_default(sh4der_blade_params_t *params);

/** \brief The MAX WHITE test preset: the shipped sword with the SH4DER_BLADE_WHITE_* values. */
void sh4der_blade_params_white(sh4der_blade_params_t *params);

/** \brief Validate params: 1 when sh4der_blade_prepare() would accept them. */
int sh4der_blade_params_valid(const sh4der_blade_params_t *params);

/** \brief Evaluate one instance at `tick` (add the instance's phase yourself).
    `fade` is clamped to 0..1 (1 - active camouflage). Returns 0 and leaves an
    all-dark frame (every intensity 0) on bad params. */
int sh4der_blade_prepare(sh4der_blade_frame_t *frame, const sh4der_blade_params_t *params, uint32_t tick, float fade);

/** \brief Intensities of `count` vertices into out[] (floats, 0..1). */
void sh4der_blade_shade(const sh4der_blade_frame_t *frame, const float *xyz, size_t xyz_stride, float *out, unsigned count);

/** \brief Packed colours of `count` vertices: rgb x intensity, alpha `a`, at
    out_stride bytes apart. rgb is 0..1. */
void sh4der_blade_argb(const sh4der_blade_frame_t *frame, const float rgb[3], uint8_t a, const float *xyz, size_t xyz_stride,
                       uint32_t *out, size_t out_stride, unsigned count);

/** \brief Packed colours on the ramp (sh4der_ramp_colours()): each vertex
    mix(edge, core, k^2) x k, alpha `a`, out_stride bytes apart. */
void sh4der_blade_argb_ramp(const sh4der_blade_frame_t *frame, const float edge[3], const float core[3], uint8_t a, const float *xyz,
                            size_t xyz_stride, uint32_t *out, size_t out_stride, unsigned count);

/* ------------------------------------------------------------------------ */

/** \brief Intensity 0..1 of one vertex, fade applied. */
static inline float sh4der_blade_intensity(const sh4der_blade_frame_t *f, float x, float y, float z) {
    float tube = 0.0f, mask = 1.0f, v;
    int i;

    for (i = 0; i < f->tube_count; i++) {
        const float d2 = sh4der_len2_(y - f->tube_y[i], z - f->tube_z[i]);

        if (d2 < f->tube_outer2[i]) {
            float s = f->tube_weight[i] * sh4der_falloff_(d2, f->tube_inner[i], f->tube_inv_feather[i]);

            if (s > tube)
                tube = s;
        }
    }
    for (i = 0; i < f->blocker_count; i++) {
        const float d2 = sh4der_len3_(x - f->blk_x[i], y - f->blk_y[i], z - f->blk_z[i]);

        if (d2 < f->blk_outer2[i])
            mask *= 1.0f - f->blk_depth[i] * sh4der_falloff_(d2, f->blk_inner[i], f->blk_inv_feather[i]);
    }
    v = sh4der_min1_((f->lo + f->hi * tube) * mask);
    if (f->ripple_depth > 0.0f) {   /* ripples along the blade: 1 at a crest, 1 - depth at a trough */
        float s, c;

        sh4der_sincos_(f->ripple_k * x - f->ripple_phase, &s, &c);
        v *= 1.0f - f->ripple_depth * 0.5f * (1.0f - c);
    }
    return v * f->fade * sh4dow_smooth01((x - f->fade_x0) * f->inv_fade_len);   /* the base fade: no hard edge where the blade's shape is cut */
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_BLADE_H */
