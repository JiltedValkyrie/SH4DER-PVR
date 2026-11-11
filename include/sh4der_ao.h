/* SH4DER-PVR: small-scale ambient occlusion

   sh4der_ao.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_ao.h
    \brief   Contact darkening without depth: a crease term at load or bake
             (no runtime cost) and a screen cavity term (one frame behind).

    The PVR has no per-pixel depth or normal read-back, so no SSAO. What the
    maths here can do:

    - Crease (sh4der_ao_edge_concave, sh4der_ao_crease_factor): at a concave
      edge, the face the other face rises from is darker. The caller sums the
      concave amount of a vertex's edges and multiplies the vertex colour by the
      factor. Done at bake time it costs nothing at runtime: the baked vertex
      colour already holds the light, so the factor multiplies into it.
    - Screen cavity (sh4der_ao_screen, sh4der_ao_grid): on an 80 x 60 luma grid
      of the frame on screen (the grid sh4der_look reads), a texel brighter than the mean of
      the texels above it is darkened by how much brighter. The darkened grid
      (sh4der_ao_texel565) is a multiply map drawn over the frame. This cannot
      tell a crease from an edge in the texture: it is a cue, not AO.

    The divisions and clamps are SH4ZAM (through the core), the luma sums are
    integer. Define SH4DER_CORE_NO_SH4ZAM for the plain C path.

    \author Jilted Valkyrie
    \author Cortana (Claude, an AI by Anthropic): the effects' maths, the PVR paths, the tests
*/

#ifndef SH4DER_AO_H
#define SH4DER_AO_H

#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Shipped tuning
    @{ */
#define SH4DER_AO_CREASE_K     0.5f    /**< \brief Vertex factor at a full concave crease is 1 - this (0.5: half the colour). */
#define SH4DER_AO_CREASE_FULL  2.0f    /**< \brief Sum of concave edge amounts at a vertex that counts as full (two right-angle edges). */
#define SH4DER_AO_SCR_GRID_X   80      /**< \brief Screen grid across (the grid sh4der_look reads). */
#define SH4DER_AO_SCR_GRID_Y   60      /**< \brief Screen grid down. */
#define SH4DER_AO_SCR_K        0.5f    /**< \brief Multiply at full cavity is 1 - this. */
#define SH4DER_AO_SCR_ENGAGE   0.2f    /**< \brief Brighter-than-above ratio below which nothing is darkened (the luma noise floor). */
#define SH4DER_AO_SCR_FULL     0.6f    /**< \brief Ratio (centre - above) / centre at full cavity. */
#define SH4DER_AO_SCR_FLOOR    4.0f    /**< \brief Luma (0..255) added to both before the ratio: black is no division by zero. */
/** @} */

/** \brief The concave amount of one edge, 0..1 (sin of the bend).
    n1: the unit normal of face 1. p: a point on the edge. e: the unit edge direction.
    o: a vertex of face 2 that is not on the edge. The offset to o is cut to the plane across the edge (no e part),
    so a right-angle wall gives 1 at any length. Concave (o rises in front of face 1, a wall on a floor): > 0.
    Flat or convex: 0. */
static inline float sh4der_ao_edge_concave(const float n1[3], const float p[3], const float e[3], const float o[3]) {
    float d[3] = { o[0] - p[0], o[1] - p[1], o[2] - p[2] };
    const float along = d[0] * e[0] + d[1] * e[1] + d[2] * e[2];
    d[0] -= along * e[0]; d[1] -= along * e[1]; d[2] -= along * e[2];
    sh4der_norm3_(d);   /* unit, or unchanged (zero) when o is on the edge's line */
    return sh4der_sat_(n1[0] * d[0] + n1[1] * d[1] + n1[2] * d[2]);
}

/** \brief A vertex's colour factor, 1 - K x sat(sum / FULL). `concave_sum`: the sum of sh4der_ao_edge_concave over the
    vertex's edges (each edge once, >= 0). No concave edge: 1. */
static inline float sh4der_ao_crease_factor(float concave_sum) {
    return 1.0f - SH4DER_AO_CREASE_K * sh4der_sat_(concave_sum * (1.0f / SH4DER_AO_CREASE_FULL));
}

/** \brief The screen cavity multiply for one texel. `centre` and `above` are luma 0..255: `above` is the mean of the
    three texels in the row above (the side the light comes from). A texel brighter than what is above it is darkened by
    the ratio (centre - above) / centre past SH4DER_AO_SCR_ENGAGE; a texel darker than above is left alone. */
static inline float sh4der_ao_screen(float centre, float above) {
    const float c = centre + SH4DER_AO_SCR_FLOOR, a = above + SH4DER_AO_SCR_FLOOR;
    const float ratio = sh4der_sat_(1.0f - sh4der_div_(a, c));   /* (c - a) / c: 0 when above is not darker */
    const float t = sh4der_sat_((ratio - SH4DER_AO_SCR_ENGAGE) * (1.0f / (SH4DER_AO_SCR_FULL - SH4DER_AO_SCR_ENGAGE)));
    return 1.0f - SH4DER_AO_SCR_K * t;
}

/** \brief Screen cavity over a whole grid: `luma` is w x h bytes (row-major, row 0 at the top, 0..255 each), `out` gets one
    multiply a texel (w x h floats). Row 0 and the first and last columns have no row above (or no up-left / up-right): 1.
    The above mean is (up-left + 2 up + up-right) / 4, integers, so the ring is the texel's angle toward the light. */
static inline void sh4der_ao_grid(const uint8_t *luma, unsigned w, unsigned h, float *out) {
    unsigned x, y;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            float m = 1.0f;
            if (y > 0 && x > 0 && x + 1 < w) {
                const unsigned up = (unsigned)luma[(y - 1) * w + x - 1] + 2u * luma[(y - 1) * w + x] + luma[(y - 1) * w + x + 1];
                m = sh4der_ao_screen((float)luma[y * w + x], (float)up * 0.25f);
            }
            out[y * w + x] = m;
        }
}

/** \brief A multiply texel, RGB565: m (0..1) per channel, 31 / 63 / 31 steps. 0xffff is 1 (no change). */
static inline uint16_t sh4der_ao_texel565(float m) {
    const unsigned r = (unsigned)(sh4der_sat_(m) * 31.0f + 0.5f);
    const unsigned g = (unsigned)(sh4der_sat_(m) * 63.0f + 0.5f);
    return (uint16_t)((r << 11) | (g << 5) | r);
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_AO_H */
