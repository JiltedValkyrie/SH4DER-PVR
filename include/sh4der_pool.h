/* SH4DER-PVR: light pools on a plane

   sh4der_pool.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_pool.h
    \brief   The light a lamp throws on the ground, as a small fan of triangles
             whose vertex alpha fades to nothing: a disc under a street lamp, a
             wedge in front of a headlight, and (the same disc, dark) a blob shadow.

    A big ground polygon cannot be lit per vertex: it has no vertices where the
    light falls. So the light is its own mesh, lying on the ground, drawn on the
    translucent list after it: additive (source alpha / one) for light, blended
    (source alpha / 1 - source alpha) with a dark colour for a shadow. The PVR
    sorts and blends it for free; the cost is the fill.

    - DISC: a centre and two axes in the plane (their lengths are the two radii:
      equal for a circle, unequal for the ellipse under a car). The centre has
      the full alpha, the rim 0. n triangles, n + 1 vertices.
    - WEDGE: an apex, a direction and a side axis in the plane, a length and a
      half angle. The apex has the full alpha, the far arc 0, so the light dies
      with distance. n triangles, n + 2 vertices.

    Both write world positions and packed colours; the caller transforms and
    submits them (fan order: a WEDGE's triangle k is vertices 0, k + 1, k + 2; a DISC's is
    0, k + 1, (k + 1) % n + 1, because its rim closes on itself), with its
    own area guard (sh4der_tri_area_ok) and a compiled header. Lift the fan a
    little off the ground along the plane's normal (2 to 4 cm at car scale) so
    the depth test does not fight the ground. The array a call writes must hold
    SH4DER_POOL_MAX_N + 2 vertices (or n + 2 for the n you pass).

    No allocation, no globals. Floats in arrays the caller owns (4-byte aligned).
    The maths is SH4ZAM through sh4der_core.h (sine and cosine, fused
    multiply-add, divide); define SH4DER_CORE_NO_SH4ZAM for the plain C path.
    By hand: the alpha byte, and one scale an arc vertex.

    Version: v0.10
    The effect: a pool of light on the road under a lamp, a cone of light ahead
      of a car, a soft dark patch under it.
    Developed for: a night street in a driving demo.
    Use it for: street lamps, headlights, torches on a floor, a spotlight on a
      stage, glowing pads, blob shadows.

    \author Jilted Valkyrie

    PVR terms (TA, TR/OP list, EOL, twiddled, direct render): see the PVR glossary in README.md.
*/

#ifndef SH4DER_POOL_H
#define SH4DER_POOL_H

#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_POOL_MAX_N   16     /**< \brief Most triangles in one fan. */
#define SH4DER_POOL_TAU     6.28318530718f   /**< \brief 2 pi: radians in a full turn. */
/** @} */

/** \brief One vertex of a fan: world position, packed colour (alpha in the top byte). */
typedef struct sh4der_pool_vtx {
    float x, y, z;
    uint32_t argb;
} sh4der_pool_vtx_t;

/* alpha 0 .. 1 over a 0x00RRGGBB colour (hand: one byte) */
static inline uint32_t sh4der_pool_argb_(uint32_t rgb, float alpha) {
    return ((uint32_t)(sh4der_sat_(alpha) * 255.0f + 0.5f) << 24) | (rgb & 0x00FFFFFFu);
}

/** \brief A disc: out[0] the centre (alpha), out[1 .. n] the rim (alpha 0), the rim closed by the caller
    (triangle k is 0, k + 1, (k + 1) % n + 1). u, v: the two axes in the plane, their lengths the radii.
    n: 3 .. SH4DER_POOL_MAX_N. Returns the vertices written, n + 1, or 0 for a bad n or pointer. */
static inline unsigned sh4der_pool_disc(const float centre[3], const float u[3], const float v[3], unsigned n,
                                        uint32_t rgb, float alpha, sh4der_pool_vtx_t *out) {
    float step;
    unsigned k;

    if (!centre || !u || !v || !out || n < 3u || n > SH4DER_POOL_MAX_N) return 0;
    step = sh4der_div_(SH4DER_POOL_TAU, (float)n);
    out[0].x = centre[0]; out[0].y = centre[1]; out[0].z = centre[2];
    out[0].argb = sh4der_pool_argb_(rgb, alpha);
    for (k = 0; k < n; k++) {
        float s, c;

        sh4der_sincos_(step * (float)k, &s, &c);
        out[k + 1u].x = sh4der_fma_(u[0], c, sh4der_fma_(v[0], s, centre[0]));
        out[k + 1u].y = sh4der_fma_(u[1], c, sh4der_fma_(v[1], s, centre[1]));
        out[k + 1u].z = sh4der_fma_(u[2], c, sh4der_fma_(v[2], s, centre[2]));
        out[k + 1u].argb = rgb & 0x00FFFFFFu;
    }
    return n + 1u;
}

/** \brief A wedge: out[0] the apex (alpha), out[1 .. n + 1] the far arc (alpha 0); triangle k is 0, k + 1, k + 2.
    dir, side: unit axes in the plane, along the light and across it. length: apex to arc. half_angle: radians,
    0 .. under a quarter turn. n: 1 .. SH4DER_POOL_MAX_N. Returns the vertices written, n + 2, or 0. */
static inline unsigned sh4der_pool_wedge(const float apex[3], const float dir[3], const float side[3], float length,
                                         float half_angle, unsigned n, uint32_t rgb, float alpha, sh4der_pool_vtx_t *out) {
    float per;
    unsigned k;

    if (!apex || !dir || !side || !out || n < 1u || n > SH4DER_POOL_MAX_N || !(length > 0.0f) ||
        !(half_angle > 0.0f) || !(half_angle < 1.5f)) return 0;
    per = sh4der_div_(half_angle + half_angle, (float)n);        /* the angle between two arc vertices, once */
    out[0].x = apex[0]; out[0].y = apex[1]; out[0].z = apex[2];
    out[0].argb = sh4der_pool_argb_(rgb, alpha);
    for (k = 0; k <= n; k++) {
        float s, c;

        sh4der_sincos_(sh4der_fma_(per, (float)k, -half_angle), &s, &c);
        s *= length; c *= length;                                /* hand: two scales */
        out[k + 1u].x = sh4der_fma_(dir[0], c, sh4der_fma_(side[0], s, apex[0]));
        out[k + 1u].y = sh4der_fma_(dir[1], c, sh4der_fma_(side[1], s, apex[1]));
        out[k + 1u].z = sh4der_fma_(dir[2], c, sh4der_fma_(side[2], s, apex[2]));
        out[k + 1u].argb = rgb & 0x00FFFFFFu;
    }
    return n + 2u;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_POOL_H */
