/* SH4DER-PVR: soft edges for light: a glow, a pool or a beam fades out where it meets a wall the SH-4 knows

   sh4der_softfade.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).

   RECOMMENDED (no console run of this header: nothing here is measured as this code; wiki/limits/header-recommended.md)
   Start with: SH4DER_SOFTFADE_DIST 0.30 m; the surfaces near the light only (SH4DER_SOFTFADE_MAX 8 a call);
               sh4der_softfade_near first, and the fade only when it says 1
   Cost:       NOT MEASURED. ESTIMATE, a surface: a plane is one 3-term dot, a subtract, a multiply, a saturate and a
               smooth step (3 multiplies); a wall is that and two more dots and two saturates. sh4der_softfade_near and
               sh4der_softfade_fit: once a light. sh4der_softfade_levels / _argb: once a vertex that has light, times
               the surfaces. No PVR pass, no video memory, no texture; the PVR's work is the halo's own triangles.
               The one number near it, MEASURED as another program's own code (one console, SH4-083, PVR-157): a lamp's
               halo of 129 vertices faded against a room's planes, 205 us of SH-4 a frame with the halo's build in it
   Status:     PROTOTYPE: host tested (test/softfade_test.c). NOT RUN on a console. The other route, a camera depth
               map the PVR draws (per pixel, any shape), ran on a console in a test program and is not in the library:
               5.1 ms of PVR for the map, 1.4 ms cut to its tiles (PVR-203, PVR-205), the hard line gone (PVR-204)
   Do:         give the fade a mesh with vertices where it acts (a halo as rings: sh4der_bloom_rings, 8 rings ran in
               the lamp test; a beam as sh4der_pool_beam with 3 or 4 rows); keep the walls few
   Do not:     expect a fade against a car, a person or anything curved (no depth is read: planes and rectangles
               only); use it where a wall cuts through the middle of a large triangle (the fade is per vertex)
   Maturity:   PROTOTYPE v0.1
*/

/** \file    sh4der_softfade.h
    \brief   How much of a light a point keeps near the surfaces the caller names: 0 on or behind a surface,
             1 from the fade distance in front of all of them, a smooth step between.

    A camera-facing glow that passes through a wall is cut on a hard line. So is a pool or a beam of light on the
    ground where a wall stands on it. The PVR cannot read the depth behind a pixel, so the cure here is the SH-4's:
    it knows where the walls are, and takes the light off the vertices near them. Three tiers, cheapest first:

    - NEAR (sh4der_softfade_near): is any surface within the light's radius and the fade distance? A 0 means
      draw the light plain and call nothing else.
    - FIT (sh4der_softfade_fit): a scale 0 .. 1 for a 4-vertex halo's radius and alpha, so the quad never
      reaches the surface. The glow shrinks near a wall; it does not fade into it. No extra vertices.
    - FADE (sh4der_softfade_point; sh4der_softfade_levels and sh4der_softfade_argb for a mesh): the keep at each
      vertex. The light thins smoothly to the wall. It needs vertices where it acts.

    Surfaces: a PLANE (unit normal toward the lit side, and d with dot(n, x) = d), endless; and a WALL, a plane
    that exists only over a rectangle in it (a centre, two unit axes, two half sizes), so a barrier does not fade
    a light far along its line. Past the rectangle's edge the wall's effect goes out over the fade distance.

    Limits. Only what the caller names: nothing against movers or curved shapes. Per vertex: between two
    vertices the PVR interpolates, so where a wall crosses a triangle a short hard edge of at most that triangle
    stays. A light within the fade distance in front of a wall is dimmed too: that is the look.

    No allocation, no globals, no PVR calls: maths only, it builds on the host. The maths is SH4ZAM through
    sh4der_core.h (the dot is shz_vec3_dot, the reciprocal FSRRA, taken once in sh4der_softfade_make); define
    SH4DER_CORE_NO_SH4ZAM for the plain C path. Host test: test/softfade_test.c.

    Version: v0.10
    The effect: a lamp's glow, a pool of light or a headlight's beam that dies softly at a wall, not on a line.
    Developed for: a lamp's halo in a room's corner, and headlights on a street with walls.
    Use it for: halos near walls and floors, pools and beams that run into a kerb, a barrier or a building front,
      a muzzle flash in a corridor.

    \author Jilted Valkyrie
*/

#ifndef SH4DER_SOFTFADE_H
#define SH4DER_SOFTFADE_H

#include <stddef.h>
#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_SOFTFADE_DIST  0.30f   /**< \brief Metres in front of a surface over which the light comes back. */
#define SH4DER_SOFTFADE_MAX   8       /**< \brief Most planes, and most walls, one call takes; more are refused (no fade). */
/** @} */

/** \brief An endless plane: unit normal toward the lit side, and d with dot(n, x) = d. */
typedef struct sh4der_softfade_plane {
    float n[3], d;
} sh4der_softfade_plane_t;

/** \brief A wall: a plane over a rectangle. c: the rectangle's centre (on the plane); u, v: unit axes in the
    plane, square to each other; hu, hv: the half sizes along them, metres. */
typedef struct sh4der_softfade_wall {
    sh4der_softfade_plane_t p;
    float c[3], u[3], v[3], hu, hv;
} sh4der_softfade_wall_t;

/** \brief The fade distance, prepared: the distance and 1 / distance. */
typedef struct sh4der_softfade {
    float dist, inv;
} sh4der_softfade_t;

/** \brief Prepare the fade distance (one reciprocal: once at load or a frame, not a vertex). Returns 0, and a
    fade that is a hard step at the surface, for a distance that is not above 0. */
static inline int sh4der_softfade_make(sh4der_softfade_t *f, float dist) {
    if (!f) return 0;
    if (!(dist > 0.0f)) { f->dist = 0.0f; f->inv = 0.0f; return 0; }
    f->dist = dist; f->inv = sh4der_recip_pos_(dist);
    return 1;
}

/** \brief A plane from a point on it and its unit normal toward the lit side. */
static inline void sh4der_softfade_plane(sh4der_softfade_plane_t *p, const float point[3], const float normal[3]) {
    p->n[0] = normal[0]; p->n[1] = normal[1]; p->n[2] = normal[2];
    p->d = sh4der_dot3_(point, normal);
}

/** \brief A wall from its centre, its unit normal toward the lit side, one unit axis in it and the two half
    sizes; the other axis is normal x u. Returns 0 (nothing written) for a NULL or a half size not above 0. */
static inline int sh4der_softfade_wall(sh4der_softfade_wall_t *w, const float centre[3], const float normal[3],
                                       const float u[3], float hu, float hv) {
    if (!w || !centre || !normal || !u || !(hu > 0.0f) || !(hv > 0.0f)) return 0;
    sh4der_softfade_plane(&w->p, centre, normal);
    w->c[0] = centre[0]; w->c[1] = centre[1]; w->c[2] = centre[2];
    w->u[0] = u[0]; w->u[1] = u[1]; w->u[2] = u[2];
    sh4der_cross_(normal, u, w->v);
    w->hu = hu; w->hv = hv;
    return 1;
}

/* x saturated, then 3 x^2 - 2 x^3 (hand: three multiplies) */
static inline float sh4der_softfade_smooth_(float x) { x = sh4der_sat_(x); return x * x * (3.0f - 2.0f * x); }

/* the keep in front of a plane at signed distance s: 0 at or behind it, 1 from the fade distance on; a step for no distance */
static inline float sh4der_softfade_keep_(const sh4der_softfade_t *f, float s) {
    if (!(f->inv > 0.0f)) return s > 0.0f ? 1.0f : 0.0f;
    return sh4der_softfade_smooth_(s * f->inv);
}

/* how much of a wall there is at p: 1 over its rectangle, 0 from `reach` past its edge (reach = 1 / inv; a step for inv 0) */
static inline float sh4der_softfade_over_(const sh4der_softfade_wall_t *w, const float p[3], float reach, float inv) {
    const float r[3] = { p[0] - w->c[0], p[1] - w->c[1], p[2] - w->c[2] };
    float a = sh4der_dot3_(r, w->u), b = sh4der_dot3_(r, w->v);

    if (a < 0.0f) a = -a;
    if (b < 0.0f) b = -b;
    if (!(inv > 0.0f)) return a <= w->hu && b <= w->hv ? 1.0f : 0.0f;
    return sh4der_sat_((w->hu + reach - a) * inv) * sh4der_sat_((w->hv + reach - b) * inv);
}

static inline int sh4der_softfade_args_(const sh4der_softfade_t *f, const sh4der_softfade_plane_t *planes, unsigned np,
                                        const sh4der_softfade_wall_t *walls, unsigned nw) {
    return f && np <= SH4DER_SOFTFADE_MAX && nw <= SH4DER_SOFTFADE_MAX && (planes || !np) && (walls || !nw);
}

/** \brief FADE: how much of its light the point p keeps, 0 .. 1: the product, over the planes and the walls, of
    each one's keep. 1 for no surfaces and for bad arguments (a count over SH4DER_SOFTFADE_MAX, a NULL): no fade. */
static inline float sh4der_softfade_point(const sh4der_softfade_t *f, const sh4der_softfade_plane_t *planes, unsigned np,
                                          const sh4der_softfade_wall_t *walls, unsigned nw, const float p[3]) {
    float k = 1.0f;
    unsigned i;

    if (!p || !sh4der_softfade_args_(f, planes, np, walls, nw)) return 1.0f;
    for (i = 0; i < np; i++) k *= sh4der_softfade_keep_(f, sh4der_dot3_(p, planes[i].n) - planes[i].d);
    for (i = 0; i < nw; i++) {
        const float keep = sh4der_softfade_keep_(f, sh4der_dot3_(p, walls[i].p.n) - walls[i].p.d);

        if (keep < 1.0f) k *= 1.0f - (1.0f - keep) * sh4der_softfade_over_(&walls[i], p, f->dist, f->inv);
    }
    return k;
}

/** \brief NEAR: 1 when any surface is within radius + the fade distance of the centre (or the centre is behind
    one), so the light may need a fade; 0: draw it plain. Bad arguments give 0. */
static inline int sh4der_softfade_near(const sh4der_softfade_t *f, const sh4der_softfade_plane_t *planes, unsigned np,
                                       const sh4der_softfade_wall_t *walls, unsigned nw, const float centre[3], float radius) {
    float reach;
    unsigned i;

    if (!centre || !sh4der_softfade_args_(f, planes, np, walls, nw)) return 0;
    reach = radius + f->dist;
    for (i = 0; i < np; i++)
        if (sh4der_dot3_(centre, planes[i].n) - planes[i].d < reach) return 1;
    for (i = 0; i < nw; i++) {
        const sh4der_softfade_wall_t *w = &walls[i];
        const float r[3] = { centre[0] - w->c[0], centre[1] - w->c[1], centre[2] - w->c[2] };
        float a = sh4der_dot3_(r, w->u), b = sh4der_dot3_(r, w->v);

        if (a < 0.0f) a = -a;
        if (b < 0.0f) b = -b;
        if (sh4der_dot3_(centre, w->p.n) - w->p.d < reach && a < w->hu + reach && b < w->hv + reach) return 1;
    }
    return 0;
}

/** \brief FIT: the scale 0 .. 1 for a round light of `radius` at `centre` so it stays in front of every
    surface: the least of distance / radius, 1 when nothing is nearer than the radius, 0 on or behind a surface.
    Multiply the halo's radius and its alpha by it. A wall counts over its rectangle and `radius` past its edge.
    One reciprocal (of the radius). 1 for bad arguments or a radius not above 0. */
static inline float sh4der_softfade_fit(const sh4der_softfade_t *f, const sh4der_softfade_plane_t *planes, unsigned np,
                                        const sh4der_softfade_wall_t *walls, unsigned nw, const float centre[3], float radius) {
    float least = radius;
    unsigned i;

    if (!centre || !(radius > 0.0f) || !sh4der_softfade_args_(f, planes, np, walls, nw)) return 1.0f;
    for (i = 0; i < np; i++) {
        const float s = sh4der_dot3_(centre, planes[i].n) - planes[i].d;

        if (s < least) least = s;
    }
    for (i = 0; i < nw; i++) {
        const sh4der_softfade_wall_t *w = &walls[i];
        const float r[3] = { centre[0] - w->c[0], centre[1] - w->c[1], centre[2] - w->c[2] };
        const float s = sh4der_dot3_(centre, w->p.n) - w->p.d;
        float a = sh4der_dot3_(r, w->u), b = sh4der_dot3_(r, w->v);

        if (a < 0.0f) a = -a;
        if (b < 0.0f) b = -b;
        if (s < least && a < w->hu + radius && b < w->hv + radius) least = s;
    }
    if (!(least > 0.0f)) return 0.0f;
    return least < radius ? least * sh4der_recip_pos_(radius) : 1.0f;
}

/** \brief FADE over a mesh whose light is a float level a vertex (a halo from sh4der_bloom_rings): level[i] is
    multiplied by the keep at vertex i. xyz: vertex i at xyz + i * stride floats (x, y, z; stride at least 3).
    A vertex at level 0 costs nothing. Returns the vertices that still have light (0: do not send the mesh);
    bad arguments return nv and change nothing (no fade). */
static inline unsigned sh4der_softfade_levels(const sh4der_softfade_t *f, const sh4der_softfade_plane_t *planes, unsigned np,
                                              const sh4der_softfade_wall_t *walls, unsigned nw,
                                              const float *xyz, unsigned stride, float *level, unsigned nv) {
    unsigned i, lit = 0;

    if (!xyz || !level || stride < 3u || !sh4der_softfade_args_(f, planes, np, walls, nw)) return nv;
    for (i = 0; i < nv; i++) {
        if (!(level[i] > 0.0f)) continue;
        level[i] *= sh4der_softfade_point(f, planes, np, walls, nw, xyz + (size_t)i * stride);
        lit += level[i] > 0.0f;
    }
    return lit;
}

/** \brief FADE over a mesh whose light is the alpha byte of a packed colour (a pool, a beam: sh4der_pool_vtx_t
    with xyz = &v->x, argb = &v->argb and both strides sizeof *v). xyz_stride, argb_stride: bytes from one vertex
    to the next (multiples of 4). A vertex at alpha 0 costs nothing. Returns the vertices that still have alpha;
    bad arguments return nv and change nothing. */
static inline unsigned sh4der_softfade_argb(const sh4der_softfade_t *f, const sh4der_softfade_plane_t *planes, unsigned np,
                                            const sh4der_softfade_wall_t *walls, unsigned nw,
                                            const float *xyz, size_t xyz_stride, uint32_t *argb, size_t argb_stride, unsigned nv) {
    unsigned i, lit = 0;

    if (!xyz || !argb || xyz_stride < 12u || (xyz_stride & 3u) || argb_stride < 4u || (argb_stride & 3u) ||
        !sh4der_softfade_args_(f, planes, np, walls, nw)) return nv;
    for (i = 0; i < nv; i++) {
        uint32_t *c = (uint32_t *)(void *)((uint8_t *)argb + (size_t)i * argb_stride);
        const uint32_t a = *c >> 24;
        uint32_t b;

        if (!a) continue;
        b = (uint32_t)((float)a * sh4der_softfade_point(f, planes, np, walls, nw,
                                                        (const float *)(const void *)((const uint8_t *)xyz + (size_t)i * xyz_stride)) + 0.5f);
        *c = (b << 24) | (*c & 0x00FFFFFFu);
        lit += b != 0u;
    }
    return lit;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_SOFTFADE_H */
