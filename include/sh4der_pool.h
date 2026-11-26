/* SH4DER-PVR: light pools on a plane

   sh4der_pool.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).

   RECOMMENDED (from the wiki ledger, one console; wiki/limits/header-recommended.md)
   Start with: a disc of 10 triangles a lamp, 8 for a blob shadow (the street demo); a wedge of 2 to 4;
               SH4DER_POOL_MAX_N = 16; the centre lifted 2 to 4 cm; a blob at alpha 0.50 by day down to 0.25 at
               night (the street demo's values)
   Cost:       not measured (the street demo's blob stage was not the pool alone: PVR-160, PVR-175). A pool is
               fill: ESTIMATE at most its share of the screen of a full-screen translucent layer, which is
               +3.1 ms of PVR at 640 x 480 (PVR-002)
   Status:     ADVISED; to prove: the same view with pools on and off, no key pressed, PVR time
   Do:         limit lamps by distance; spread translucent triangles, never stack them in a tile (256 in one
               tile 6.4 ms against 82 us spread: PVR-009, PROVEN)
   Do not:     write depth from a pool; draw it before the ground
   Maturity:   ALPHA v0.2 (the disc and the wedge)

   RECOMMENDED, the fade by distance (sh4der_pool_reach_*, sh4der_pool_beam; no console run: nothing here is measured)
   Start with: SH4DER_POOL_REACH 60 m, SH4DER_POOL_FADE_W 10 m (the street demo's lamps); a beam of
               SH4DER_POOL_BEAM_ROWS 3 arcs; sh4der_pool_reach_make once a frame, sh4der_pool_reach_fade a lamp
   Cost:       NOT MEASURED. ESTIMATE: sh4der_pool_reach_make one FSRRA reciprocal; sh4der_pool_reach_fade a
               subtract, a multiply and a saturate a lamp (the caller has the distance); sh4der_pool_reach_fade2 and
               sh4der_pool_reach_verts one FSRRA square root a vertex that has alpha, none past the reach; a beam of
               n x rows: 1 + rows (n + 1) vertices and n + 2 n (rows - 1) triangles (2 x 3: 10 vertices, 10
               triangles, against the wedge's 4 and 2). The fill is the wedge's: the same area
   Status:     PROTOTYPE; the reach fade's arithmetic is the street demo's lamp line (the same bits on the plain C
               path, test/pool_test.c); NOT RUN on a console as this code. To prove: lamps driven past the reach
               with the fade against a hard cut, and a far car's beam with 1 row against 3, on a console
   Do:         skip a pool whose fade is 0 before building it; give the fade to the halo too, so both go together
   Do not:     call sh4der_pool_reach_make a lamp; put a reach on the player's own headlights (they are always near)
   Maturity:   PROTOTYPE v0.1 (the fade by distance and the beam)
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

    - REACH: the fade by distance. A lamp far from the camera must not pop in: its pool's alpha is scaled by a ramp
      that is 1 inside the reach less a width and 0 at the reach (sh4der_pool_reach_make once, then
      sh4der_pool_reach_fade a lamp, or sh4der_pool_reach_verts a vertex of a long fan).
    - BEAM: a wedge with arcs along its length (sh4der_pool_beam, its triangles from sh4der_pool_beam_tris), for
      a headlight seen from outside: with a reach each vertex fades by its own distance from the eye, so the beam
      of a far car comes in from its near end.

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
#include "sh4der_ramp.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_POOL_MAX_N   16     /**< \brief Most triangles in one fan. */
#define SH4DER_POOL_TAU     6.28318530718f   /**< \brief 2 pi: radians in a full turn. */
#define SH4DER_POOL_REACH   60.0f  /**< \brief Metres from the camera (or the car) within which a lamp's pool is drawn (the street demo). */
#define SH4DER_POOL_FADE_W  10.0f  /**< \brief Metres: a pool fades to nothing over the last this of the reach, so it does not pop. */
#define SH4DER_POOL_BEAM_ROWS      3   /**< \brief Arcs along a beam (sh4der_pool_beam): where the fade by distance can act. */
#define SH4DER_POOL_BEAM_ROWS_MAX  4   /**< \brief Most arcs along a beam. */
#define SH4DER_POOL_BEAM_VTX_MAX   (1 + SH4DER_POOL_BEAM_ROWS_MAX * (SH4DER_POOL_MAX_N + 1))   /**< \brief Most vertices of a beam: 69. */
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

/** \brief The fade by distance: full inside the reach less the width, nothing at the reach, linear between
    (one ramp of sh4der_ramp.h, prepared once). */
typedef struct sh4der_pool_reach {
    float reach, reach2;        /**< \brief The reach and its square, metres. */
    sh4der_ramp_t edge;         /**< \brief The ramp over the last `width` of the reach. */
} sh4der_pool_reach_t;

/** \brief Prepare a reach: once a frame or at load, not a lamp (one reciprocal). width <= 0 is a step at the reach.
    Returns 0 (and a reach that fades everything out) for a NULL pointer or a reach that is not above 0. */
static inline int sh4der_pool_reach_make(sh4der_pool_reach_t *r, float reach, float width) {
    if (!r) return 0;
    if (!(reach > 0.0f)) { r->reach = r->reach2 = -1.0f; sh4der_ramp_make(&r->edge, 0.0f, 0.0f); return 0; }
    r->reach = reach; r->reach2 = reach * reach;
    sh4der_ramp_make(&r->edge, 0.0f, width);
    return 1;
}

/** \brief The alpha scale 0 .. 1 of a lamp's pool (or its halo) at `dist` metres: multiply the pool's alpha by it.
    A multiply and a saturate; no square root (the caller has the distance). */
static inline float sh4der_pool_reach_fade(const sh4der_pool_reach_t *r, float dist) {
    return sh4der_ramp_up(&r->edge, r->reach - dist);
}

/** \brief The same from a squared distance: 0 at or past the reach with no square root (skip the pool), else one
    square root. */
static inline float sh4der_pool_reach_fade2(const sh4der_pool_reach_t *r, float dist2) {
    if (!(dist2 < r->reach2)) return 0.0f;
    return sh4der_pool_reach_fade(r, sh4der_sqrt_(dist2 > 0.0f ? dist2 : 0.0f));
}

/** \brief Fade a fan's vertices by each one's own distance from `eye`: for a pool or a beam long enough that its
    far side passes the reach before its near side. Scales each alpha byte; a vertex at alpha 0 (a rim) costs nothing.
    Returns the vertices that still have alpha (0: do not send the fan). */
static inline unsigned sh4der_pool_reach_verts(const sh4der_pool_reach_t *r, const float eye[3], sh4der_pool_vtx_t *v, unsigned nv) {
    unsigned i, lit = 0;

    if (!r || !eye || !v) return 0;
    for (i = 0; i < nv; i++) {
        const uint32_t a = v[i].argb >> 24;
        const float dx = v[i].x - eye[0], dy = v[i].y - eye[1], dz = v[i].z - eye[2];
        uint32_t b;

        if (!a) continue;
        b = (uint32_t)((float)a * sh4der_pool_reach_fade2(r, sh4der_fma_(dx, dx, sh4der_fma_(dy, dy, dz * dz))) + 0.5f);
        v[i].argb = (b << 24) | (v[i].argb & 0x00FFFFFFu);
        lit += b != 0u;
    }
    return lit;
}

/** \brief A beam: a wedge with `rows` arcs along its length, so a headlight fades along the beam and by distance.
    out[0] is the apex; arc r (1 .. rows) is at length x r / rows, its vertex k (0 .. n) at out[1 + (r - 1)(n + 1) + k].
    Along the beam the alpha falls linearly from `alpha` at the apex to 0 on the last arc (as the wedge does); with a
    `reach` (and `eye`) each vertex is also scaled by sh4der_pool_reach_fade2 of its own distance from the eye, so a
    far car's beam fades in from its near end. reach NULL: no fade by distance (rows 1 is then sh4der_pool_wedge).
    n: 1 .. SH4DER_POOL_MAX_N. rows: 1 .. SH4DER_POOL_BEAM_ROWS_MAX. The array holds 1 + rows (n + 1) vertices
    (SH4DER_POOL_BEAM_VTX_MAX at most). Triangles: sh4der_pool_beam_tris. Returns the vertices written, or 0 with
    nothing written for a bad argument, and 0 too when a reach leaves no vertex with alpha (nothing to send). */
static inline unsigned sh4der_pool_beam(const float apex[3], const float dir[3], const float side[3], float length,
                                        float half_angle, unsigned n, unsigned rows, uint32_t rgb, float alpha,
                                        const sh4der_pool_reach_t *reach, const float eye[3], sh4der_pool_vtx_t *out) {
    float per, step;
    unsigned k, r, nv;

    if (!apex || !dir || !side || !out || n < 1u || n > SH4DER_POOL_MAX_N || rows < 1u || rows > SH4DER_POOL_BEAM_ROWS_MAX ||
        !(length > 0.0f) || !(half_angle > 0.0f) || !(half_angle < 1.5f) || (reach && !eye)) return 0;
    nv = 1u + rows * (n + 1u);
    if (reach) {                                                 /* past the reach from end to end: nothing written */
        const float ax = apex[0] - eye[0], ay = apex[1] - eye[1], az = apex[2] - eye[2];
        const float far = reach->reach + length;

        if (!(reach->reach > 0.0f) || ax * ax + ay * ay + az * az >= far * far) return 0;
    }
    per = sh4der_div_(half_angle + half_angle, (float)n);        /* the angle between two arc vertices, once */
    step = sh4der_div_(1.0f, (float)rows);                       /* an arc's share of the length, once */
    out[0].x = apex[0]; out[0].y = apex[1]; out[0].z = apex[2];
    out[0].argb = sh4der_pool_argb_(rgb, alpha);
    for (k = 0; k <= n; k++) {
        float s, c;

        sh4der_sincos_(sh4der_fma_(per, (float)k, -half_angle), &s, &c);
        s *= length; c *= length;                                /* hand: two scales */
        for (r = 1u; r <= rows; r++) {
            sh4der_pool_vtx_t *o = &out[1u + (r - 1u) * (n + 1u) + k];
            const float t = r == rows ? 1.0f : step * (float)r, sr = s * t, cr = c * t;

            o->x = sh4der_fma_(dir[0], cr, sh4der_fma_(side[0], sr, apex[0]));
            o->y = sh4der_fma_(dir[1], cr, sh4der_fma_(side[1], sr, apex[1]));
            o->z = sh4der_fma_(dir[2], cr, sh4der_fma_(side[2], sr, apex[2]));
            o->argb = r == rows ? rgb & 0x00FFFFFFu : sh4der_pool_argb_(rgb, alpha * (1.0f - t));
        }
    }
    if (reach && !sh4der_pool_reach_verts(reach, eye, out, nv)) return 0;
    return nv;
}

/** \brief A beam's triangles as indices into its vertices: the fan from the apex to the first arc, then two
    triangles a cell between each pair of arcs, all wound the same way. idx holds 3 x (n + 2 n (rows - 1)) entries
    (3 x 112 at most). Returns the triangles written, or 0 for a bad argument. */
static inline unsigned sh4der_pool_beam_tris(unsigned n, unsigned rows, uint16_t *idx) {
    unsigned k, r, t = 0;

    if (!idx || n < 1u || n > SH4DER_POOL_MAX_N || rows < 1u || rows > SH4DER_POOL_BEAM_ROWS_MAX) return 0;
    for (k = 0; k < n; k++, t++) { idx[3u * t] = 0u; idx[3u * t + 1u] = (uint16_t)(k + 1u); idx[3u * t + 2u] = (uint16_t)(k + 2u); }
    for (r = 1u; r < rows; r++)
        for (k = 0; k < n; k++) {
            const uint16_t a = (uint16_t)(1u + (r - 1u) * (n + 1u) + k), c = (uint16_t)(a + n + 1u);

            idx[3u * t] = a; idx[3u * t + 1u] = c; idx[3u * t + 2u] = (uint16_t)(c + 1u); t++;
            idx[3u * t] = a; idx[3u * t + 1u] = (uint16_t)(c + 1u); idx[3u * t + 2u] = (uint16_t)(a + 1u); t++;
        }
    return t;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_POOL_H */
