/* SH4DER-PVR: the holo effect

   sh4der_holo.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_holo.h
    \brief   A light inside a translucent body that follows the body's own breathing.

    A soft ball at one point (a chest marker) lifts the vertex colour around
    it, and the whole body brightens a little as the animation breathes in.
    No clock, no flicker term, no vertex movement: the alpha the table carries
    is constant (the fade), so the fill stays steady.

    The breath comes from the pose the model is skinned with, through the
    shared breath tracker (sh4der_core.h: sh4der_breath_raw, sh4der_breath_push).

    The core rides the body, so each vertex's weight is taken once, at load,
    from its bind position (sh4der_holo_weights: a byte, 0..LEVELS-1). Per
    frame sh4der_holo_prepare() fills a LEVELS-entry colour table from the
    breath; per vertex the colour is one table load (sh4der_holo_argb). Draw
    the body alpha blended with its texture modulated by that colour.

    Order: sh4der_holo_weights once at load. Per frame: sh4der_breath_push,
    then sh4der_holo_prepare with the breath, then sh4der_holo_shade or
    sh4der_holo_argb per vertex.

    The falloff is SH4DOW-DC's column feather (sh4der_core.h).

    \author Jilted Valkyrie
    \author Cortana (Claude, an AI by Anthropic): the effects' maths, the PVR paths, the tests
*/

#ifndef SH4DER_HOLO_H
#define SH4DER_HOLO_H

#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SH4DER_HOLO_LEVELS     64      /**< \brief Weight steps (table entries). */

/** \name Shipped tuning: a hologram's chest core (sh4der_holo_params_default())
    Lengths in vertex units (the bind model's world units).
    @{ */
#define SH4DER_HOLO_GLOW_BASE  0.90f   /**< \brief Body level factor, breath out. */
#define SH4DER_HOLO_GLOW_AMP   0.10f   /**< \brief Body level factor added at breath in. */
#define SH4DER_HOLO_BASE       0.60f   /**< \brief Core strength, breath out (never off). */
#define SH4DER_HOLO_AMP        0.40f   /**< \brief Core strength added at breath in. */
#define SH4DER_HOLO_BODY       0.62f   /**< \brief Vertex level away from the core (bake the texture 1 / this brighter). */
#define SH4DER_HOLO_ADD        0.40f   /**< \brief Level the core adds at its centre. */
#define SH4DER_HOLO_X          -0.00642f /**< \brief Core centre x (bind model space). */
#define SH4DER_HOLO_Y          0.0f    /**< \brief Core centre y. */
#define SH4DER_HOLO_Z          0.13477f /**< \brief Core centre z. */
#define SH4DER_HOLO_INNER      0.010f  /**< \brief Full core within this distance. */
#define SH4DER_HOLO_OUTER      0.055f  /**< \brief No core from this distance (> inner). */
/** @} */

typedef struct sh4der_holo_params {
    float glow_base, glow_amp;  /**< \brief Body factor = base + amp x breath (0..1). */
    float core_base, core_amp;  /**< \brief Core strength = base + amp x breath (0..1). */
    float body;                 /**< \brief Level away from the core, 0..1. */
    float add;                  /**< \brief Level the core adds at full weight, 0..1. */
    float center[3];            /**< \brief Core centre, bind model space. */
    float inner, outer;         /**< \brief Core feather (SH4DOW-DC's falloff). */
} sh4der_holo_params_t;

/** \brief One instance's frame: sh4der_holo_prepare()'s output. */
typedef struct sh4der_holo_frame {
    float breath;               /**< \brief 0 (out) .. 1 (in) this frame, as given (clamped). */
    float glow, core;           /**< \brief Body factor and core strength this frame. */
    float fade;                 /**< \brief 0..1, as given: the table's alpha. */
    uint32_t argb[SH4DER_HOLO_LEVELS];  /**< \brief Grey level x 255 in r, g, b; fade x 255 in a. */
} sh4der_holo_frame_t;

/** \brief The chest core's tuning (the SH4DER_HOLO_* values). */
void sh4der_holo_params_default(sh4der_holo_params_t *params);

/** \brief Validate params: 1 when sh4der_holo_prepare() would accept them. */
int sh4der_holo_params_valid(const sh4der_holo_params_t *params);

/** \brief Load time: each vertex's core weight 0..SH4DER_HOLO_LEVELS-1 from its
    bind position (rounded falloff). Bad params give weight 0 everywhere. */
void sh4der_holo_weights(const sh4der_holo_params_t *params, const float *xyz, size_t xyz_stride, uint8_t *out, unsigned count);

/** \brief Fill the frame's table for `breath` (0 out .. 1 in, clamped; NaN = 0.5);
    `fade` is clamped to 0..1. Returns 0 and leaves a clear table (every entry 0)
    on bad params. */
int sh4der_holo_prepare(sh4der_holo_frame_t *frame, const sh4der_holo_params_t *params, float breath, float fade);

/** \brief Packed colours of `count` vertices from their weights, out_stride bytes apart. */
void sh4der_holo_shade(const sh4der_holo_frame_t *frame, const uint8_t *weight, uint32_t *out, size_t out_stride, unsigned count);

/** \brief One vertex's packed colour (weights past the table read its last entry). */
static inline uint32_t sh4der_holo_argb(const sh4der_holo_frame_t *f, uint8_t weight) {
    return f->argb[weight < SH4DER_HOLO_LEVELS ? weight : SH4DER_HOLO_LEVELS - 1];
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_HOLO_H */
