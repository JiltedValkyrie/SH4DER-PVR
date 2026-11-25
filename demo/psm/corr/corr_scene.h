/* SH4DER-PVR: corr-a, the texture filtering corridor: the corridor, the ring, the eye, the clip and the counters

   demo/psm/corr/corr_scene.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A long straight corridor of repeated 4 m segments that never ends: the eye walks forward and the segments wrap.

   THE CORRIDOR. 3 m wide (x -1.5 .. 1.5), 3 m high (y 0 .. 3), along +z. A segment is cut into 1 m cells: the floor and
   the ceiling 3 x 4, each wall 4 x 3, one quad a cell, the texture once a cell (so a grazing floor far off has many
   texels a pixel; the walls' tile is 2 m square, half a repeat a metre). Every surface's v runs along the corridor (the wall tile is turned in the bake for that:
   tools/corr_bake.py). There are CORR_IDS kinds of segment, fixed at load (corr_build): a doorway on the right wall in
   every 8th, a pair of pillars in every 4th, a torch on alternate walls in every other. The light is baked a vertex at
   load from every torch of the ring (the ring is periodic, so the bake has no seam): no light is computed in a frame.

   THE FAR CAP (corr-a2). A segment that starts CORR_CAP or more ahead is drawn as its far kind: one quad a surface
   in place of a quad a 1 m cell, the same texture repeats (uv in metres). That keeps the polygons that meet at the
   vanishing point few (the host count of the autorun path: test/corr_test.c). The light is then baked at the far
   quads' corners only, and where a near segment meets a far one the near cells' edge vertices are T-junctions.

   THE RING. CORR_RING segments are drawn: 1 behind the eye and the rest ahead. The eye's place s runs 0 .. CORR_WRAP
   (CORR_IDS segments) and wraps; segment b of the ring is the kind b & (CORR_IDS - 1), at z = 4 b - s from the eye.
   Nothing grows with time: s wraps, b stays within -1 .. 2 CORR_IDS (test/corr_test.c runs it for 10^6 frames).

   A FRAME. corr_frame: every quad of the ring into view space (the yaw a frame, one sin and cos), culled against the
   four sides of the view, clipped at the near plane (a quad becomes up to 5 points), projected (1 / z on FSRRA), into
   a list of polygons in strip order with their screen box. Transformed once: a split screen sends the same list twice.

   THE COUNTERS (CORE 26 count-only, Lead's call): the screen in 32 x 32 bins (20 x 15, as the PVR's tiles); for each
   filtered polygon its box adds one to every bin it covers; the most in one bin is what the page prints. A polygon's
   box is larger than the polygon, so this is an upper bound on what a tile holds. It drops nothing.

   Plain C on sh4der_core.h's maths (SH4ZAM on the console). No KallistiOS. */

#ifndef CORR_SCENE_H
#define CORR_SCENE_H

#include <stdint.h>
#include <string.h>

#include "sh4der_core.h"
#include "sh4der_guard.h"

#define CORR_SEG 4.0f                        /* metres a segment */
#define CORR_IDS 16u                         /* kinds of segment: a power of two */
#define CORR_WRAP (CORR_SEG * (float)CORR_IDS)
#define CORR_RING 16u                        /* segments drawn: 1 behind, 15 ahead (60 m) */
#define CORR_HALF_W 1.5f
#define CORR_H 3.0f
#define CORR_EYE_Y 1.6f
#define CORR_NEAR 0.10f
#define CORR_F 320.0f                        /* focal length in pixels: 90 degrees across 640 */
#define CORR_CX 320.0f
#define CORR_CY 240.0f
#define CORR_W 640u
#define CORR_HGT 480u
#define CORR_BIN 32u
#define CORR_BINS_X (CORR_W / CORR_BIN)
#define CORR_BINS_Y (CORR_HGT / CORR_BIN)
#define CORR_Q_MAX 80u                       /* quads a segment kind */
#define CORR_KINDS (2u * CORR_IDS)           /* near and far */
#define CORR_CAP 16.0f                       /* metres: a segment that starts this far ahead is drawn as its far kind */
#define CORR_NO_CAP 1.0e9f
#define CORR_POLY_MAX (CORR_RING * CORR_Q_MAX)

enum { CORR_T_FLOOR, CORR_T_WALL, CORR_T_CEIL, CORR_T_DOOR, CORR_T_TEXTURES, CORR_T_FLAT = CORR_T_TEXTURES, CORR_T_FLAME };
#define CORR_TEXTURED(t) ((t) < CORR_T_TEXTURES)

typedef struct corr_quad {
    float p[4][3];                           /* segment space, strip order: 0 1 2, then 1 2 3 */
    float uv[4][2];
    uint32_t argb[4];                        /* the baked light */
    float n[3];                              /* unit normal, into the corridor */
    float c[3];                              /* the middle */
    uint8_t tex;                             /* CORR_T_* */
} corr_quad_t;

typedef struct corr_kind {
    corr_quad_t q[CORR_Q_MAX];
    unsigned n;
} corr_kind_t;

typedef struct corr_poly {
    float x[5], y[5], z[5], u[5], v[5];      /* screen x, y; z = 1 / depth; strip order */
    uint32_t argb[5];
    float ndv;                               /* |normal . unit direction to the eye| at the middle, 0 .. 1 */
    uint8_t n, tex;
    uint8_t bx0, by0, bx1, by1;              /* the bins its screen box covers, both ends counted */
} corr_poly_t;

typedef struct corr_eye {
    float s;                                 /* along the corridor, 0 .. CORR_WRAP */
    float x;                                 /* across, -1 .. 1 */
    float yaw;                               /* radians, + to the right */
} corr_eye_t;

typedef struct corr_list {
    corr_poly_t p[CORR_POLY_MAX];
    unsigned n;
    unsigned culled, clipped, dropped;       /* quads outside the view; cut at the near plane; no area (sh4der_tri_area_ok) */
} corr_list_t;

/* ---- the segment kinds --------------------------------------------------------------------------------------------- */

static inline uint32_t corr_argb_(float r, float g, float b) {
    const unsigned R = (unsigned)(sh4der_sat_(r) * 255.0f + 0.5f), G = (unsigned)(sh4der_sat_(g) * 255.0f + 0.5f), B = (unsigned)(sh4der_sat_(b) * 255.0f + 0.5f);

    return 0xff000000u | (R << 16) | (G << 8) | B;
}

/* A quad from its four corners in loop order a b c d, with their uv: stored in strip order a b d c. */
static inline void corr_put_(corr_kind_t *k, unsigned tex, const float n[3], const float a[3], const float b[3], const float c[3], const float d[3],
                             const float ta[2], const float tb[2], const float tc[2], const float td[2]) {
    corr_quad_t *q;
    const float *const P[4] = { a, b, d, c }, *const T[4] = { ta, tb, td, tc };
    unsigned i, j;

    if (k->n >= CORR_Q_MAX) return;
    q = &k->q[k->n++];
    for (i = 0; i < 4u; i++) {
        for (j = 0; j < 3u; j++) q->p[i][j] = P[i][j];
        q->uv[i][0] = T[i][0]; q->uv[i][1] = T[i][1];
        q->argb[i] = 0xffffffffu;
    }
    for (j = 0; j < 3u; j++) { q->n[j] = n[j]; q->c[j] = 0.25f * (a[j] + b[j] + c[j] + d[j]); }
    q->tex = (uint8_t)tex;
}

/* A box face: a rectangle on x = const (wall) between z0..z1, y0..y1. The wall texture is 2 m square: u = height from
   the ceiling, v along z, half a repeat a metre. */
static inline void corr_wall_cell_(corr_kind_t *k, unsigned tex, float x, float nx, float z0, float z1, float y0, float y1) {
    const float n[3] = { nx, 0.0f, 0.0f };
    const float a[3] = { x, y1, z0 }, b[3] = { x, y1, z1 }, c[3] = { x, y0, z1 }, d[3] = { x, y0, z0 };
    const float u1 = (CORR_H - y1) * 0.5f, u0 = (CORR_H - y0) * 0.5f;
    const float ta[2] = { u1, z0 * 0.5f }, tb[2] = { u1, z1 * 0.5f }, tc[2] = { u0, z1 * 0.5f }, td[2] = { u0, z0 * 0.5f };

    corr_put_(k, tex, n, a, b, c, d, ta, tb, tc, td);
}

/* The doorway's cell, upright: the gate texture (2 m square, two gates side by side) with u along z from the doorway's
   start zd, v down from its top at 2 m: one gate over 1 m x 2 m. It has no stretched copy. */
static inline void corr_door_cell_(corr_kind_t *k, float x, float nx, float zd, float z0, float z1, float y0, float y1) {
    const float n[3] = { nx, 0.0f, 0.0f };
    const float a[3] = { x, y1, z0 }, b[3] = { x, y1, z1 }, c[3] = { x, y0, z1 }, d[3] = { x, y0, z0 };
    const float u0 = (z0 - zd) * 0.5f, u1 = (z1 - zd) * 0.5f, v1 = (2.0f - y1) * 0.5f, v0 = (2.0f - y0) * 0.5f;
    const float ta[2] = { u0, v1 }, tb[2] = { u1, v1 }, tc[2] = { u1, v0 }, td[2] = { u0, v0 };

    corr_put_(k, CORR_T_DOOR, n, a, b, c, d, ta, tb, tc, td);
}

/* A cell on y = const (floor or ceiling) between x0..x1, z0..z1. u across, v along z, a repeat a metre from the
   corridor's side and the segment's start: a cell 1 m or a whole 3 x 4 m surface (the far kind) repeats the same. */
static inline void corr_flat_cell_(corr_kind_t *k, unsigned tex, float y, float ny, float x0, float x1, float z0, float z1) {
    const float n[3] = { 0.0f, ny, 0.0f };
    const float a[3] = { x0, y, z0 }, b[3] = { x1, y, z0 }, c[3] = { x1, y, z1 }, d[3] = { x0, y, z1 };
    const float ta[2] = { x0 + CORR_HALF_W, z0 }, tb[2] = { x1 + CORR_HALF_W, z0 }, tc[2] = { x1 + CORR_HALF_W, z1 }, td[2] = { x0 + CORR_HALF_W, z1 };

    corr_put_(k, tex, n, a, b, c, d, ta, tb, tc, td);
}

/* A face on z = const facing -z (toward the eye walking in), x0..x1, y0..y1: the wall texture, u = height from the
   ceiling, v across, half a repeat a metre. */
static inline void corr_front_cell_(corr_kind_t *k, unsigned tex, float z, float x0, float x1, float y0, float y1) {
    const float n[3] = { 0.0f, 0.0f, -1.0f };
    const float a[3] = { x0, y1, z }, b[3] = { x1, y1, z }, c[3] = { x1, y0, z }, d[3] = { x0, y0, z };
    const float u1 = (CORR_H - y1) * 0.5f, u0 = (CORR_H - y0) * 0.5f;
    const float ta[2] = { u1, x0 * 0.5f }, tb[2] = { u1, x1 * 0.5f }, tc[2] = { u0, x1 * 0.5f }, td[2] = { u0, x0 * 0.5f };

    corr_put_(k, tex, n, a, b, c, d, ta, tb, tc, td);
}

static inline int corr_has_door(unsigned id) { return (id & 7u) == 4u; }
static inline int corr_has_pillars(unsigned id) { return (id & 3u) == 2u; }
static inline int corr_has_torch(unsigned id) { return (id & 1u) == 0u; }
static inline float corr_torch_side(unsigned id) { return (id & 3u) == 0u ? -1.0f : 1.0f; }   /* -1 the left wall */
#define CORR_TORCH_Z 0.5f
#define CORR_TORCH_Y 2.1f
#define CORR_PILLAR_Z 2.0f
#define CORR_PILLAR_X 1.15f
#define CORR_PILLAR_R 0.15f

/* The light at a point of kind `id`'s segment: ambient + every torch of the ring, the z distance taken round the ring. */
static inline uint32_t corr_light_(unsigned id, const float p[3], const float n[3]) {
    const float zw = (float)id * CORR_SEG + p[2];
    float k = 0.10f;
    unsigned t;

    for (t = 0; t < CORR_IDS; t++) {
        float dz, dx, dy, d2, l[3], facing;

        if (!corr_has_torch(t)) continue;
        dx = corr_torch_side(t) * (CORR_HALF_W - 0.2f) - p[0];
        dy = CORR_TORCH_Y - p[1];
        dz = (float)t * CORR_SEG + CORR_TORCH_Z - zw;
        if (dz > 0.5f * CORR_WRAP) dz -= CORR_WRAP;
        if (dz < -0.5f * CORR_WRAP) dz += CORR_WRAP;
        d2 = dx * dx + dy * dy + dz * dz;
        l[0] = dx; l[1] = dy; l[2] = dz;
        facing = sh4der_dot3_(n, l) * sh4der_recip_pos_(sh4der_sqrt_(d2) + 1e-3f);
        if (facing < 0.0f) continue;
        k += (0.35f + 0.65f * facing) * 1.4f * sh4der_recip_pos_(1.0f + d2 * (1.0f / 2.6f));
    }
    return corr_argb_(k * 1.00f, k * 0.80f, k * 0.58f);
}

/* Kind `id` (0 .. CORR_IDS - 1) of segment, lit. far: the far kind (CORR_CAP): one quad a surface (the walls round a
   doorway three, the gate one, a pillar's face one), the same texture repeats as the near kind's cells. */
static inline void corr_build_kind(corr_kind_t *k, unsigned id, int far) {
    const float W = CORR_HALF_W;
    unsigned i, j, c;

    k->n = 0;
    if (far) {
        corr_flat_cell_(k, CORR_T_FLOOR, 0.0f, 1.0f, -W, W, 0.0f, CORR_SEG);
        corr_flat_cell_(k, CORR_T_CEIL, CORR_H, -1.0f, -W, W, 0.0f, CORR_SEG);
        corr_wall_cell_(k, CORR_T_WALL, -W, 1.0f, 0.0f, CORR_SEG, 0.0f, CORR_H);
        if (corr_has_door(id)) {
            corr_wall_cell_(k, CORR_T_WALL, W, -1.0f, 0.0f, 1.0f, 0.0f, CORR_H);
            corr_wall_cell_(k, CORR_T_WALL, W, -1.0f, 2.0f, CORR_SEG, 0.0f, CORR_H);
            corr_wall_cell_(k, CORR_T_WALL, W, -1.0f, 1.0f, 2.0f, 2.0f, CORR_H);
            corr_door_cell_(k, W, -1.0f, 1.0f, 1.0f, 2.0f, 0.0f, 2.0f);
        } else corr_wall_cell_(k, CORR_T_WALL, W, -1.0f, 0.0f, CORR_SEG, 0.0f, CORR_H);
    } else {
        for (j = 0; j < 4u; j++)
            for (i = 0; i < 3u; i++) {
                corr_flat_cell_(k, CORR_T_FLOOR, 0.0f, 1.0f, -W + (float)i, -W + (float)i + 1.0f, (float)j, (float)j + 1.0f);
                corr_flat_cell_(k, CORR_T_CEIL, CORR_H, -1.0f, -W + (float)i, -W + (float)i + 1.0f, (float)j, (float)j + 1.0f);
            }
        for (j = 0; j < 4u; j++)
            for (i = 0; i < 3u; i++) {
                const float y0 = (float)i, y1 = (float)i + 1.0f;

                corr_wall_cell_(k, CORR_T_WALL, -W, 1.0f, (float)j, (float)j + 1.0f, y0, y1);
                if (corr_has_door(id) && j == 1u && i < 2u) corr_door_cell_(k, W, -1.0f, 1.0f, (float)j, (float)j + 1.0f, y0, y1);
                else corr_wall_cell_(k, CORR_T_WALL, W, -1.0f, (float)j, (float)j + 1.0f, y0, y1);
            }
    }
    if (corr_has_pillars(id))
        for (c = 0; c < 2u; c++) {
            const float sx = c ? 1.0f : -1.0f, xc = sx * CORR_PILLAR_X, r = CORR_PILLAR_R;
            const unsigned cells = far ? 1u : 3u;

            for (i = 0; i < cells; i++) {
                const float y0 = far ? 0.0f : (float)i, y1 = far ? CORR_H : (float)i + 1.0f;

                corr_front_cell_(k, CORR_T_WALL, CORR_PILLAR_Z - r, xc - r, xc + r, y0, y1);
                corr_wall_cell_(k, CORR_T_WALL, xc - sx * r, -sx, CORR_PILLAR_Z - r, CORR_PILLAR_Z + r, y0, y1);   /* the inner face */
            }
        }
    if (corr_has_torch(id)) {                 /* a bracket (flat, dark) and a flame (added): untextured */
        const float sx = corr_torch_side(id), x = sx * (W - 0.01f), z = CORR_TORCH_Z;
        const float n[3] = { -sx, 0.0f, 0.0f }, t0[2] = { 0, 0 };
        const float a[3] = { x, CORR_TORCH_Y - 0.05f, z - 0.05f }, b[3] = { x, CORR_TORCH_Y - 0.05f, z + 0.05f };
        const float cc[3] = { x, CORR_TORCH_Y - 0.40f, z + 0.05f }, d[3] = { x, CORR_TORCH_Y - 0.40f, z - 0.05f };
        const float fx = sx * (W - 0.12f), fn[3] = { 0.0f, 0.0f, -1.0f };
        const float fa[3] = { fx - 0.07f, CORR_TORCH_Y + 0.22f, z }, fb[3] = { fx + 0.07f, CORR_TORCH_Y + 0.22f, z };
        const float fc[3] = { fx + 0.07f, CORR_TORCH_Y - 0.04f, z }, fd[3] = { fx - 0.07f, CORR_TORCH_Y - 0.04f, z };

        corr_put_(k, CORR_T_FLAT, n, a, b, cc, d, t0, t0, t0, t0);
        corr_put_(k, CORR_T_FLAME, fn, fa, fb, fc, fd, t0, t0, t0, t0);
    }
    for (i = 0; i < k->n; i++) {
        corr_quad_t *q = &k->q[i];

        for (j = 0; j < 4u; j++)
            q->argb[j] = q->tex == CORR_T_FLAT ? 0xff201810u : q->tex == CORR_T_FLAME ? (j < 2u ? 0xffff9a30u : 0xffffd890u) : corr_light_(id, q->p[j], q->n);
    }
}

/* Every kind: the near ones at 0 .. CORR_IDS - 1, the far ones at CORR_IDS .. CORR_KINDS - 1. */
static inline void corr_build(corr_kind_t kinds[CORR_KINDS]) {
    unsigned id;

    for (id = 0; id < CORR_IDS; id++) { corr_build_kind(&kinds[id], id, 0); corr_build_kind(&kinds[CORR_IDS + id], id, 1); }
}

/* ---- the ring and the eye ------------------------------------------------------------------------------------------ */

/* The first segment of the ring (1 behind the eye's), as a whole number; the eye's s is kept in 0 .. CORR_WRAP. */
static inline int corr_ring_base(float s) { return (int)(s * (1.0f / CORR_SEG)) - 1; }

/* A step of the walk: s forward by v dt, wrapped. Returns the new s. */
static inline float corr_walk(float s, float v, float dt) {
    s += v * dt;
    while (s >= CORR_WRAP) s -= CORR_WRAP;
    while (s < 0.0f) s += CORR_WRAP;
    return s;
}

/* The slow sway the eye has by itself: about 6 degrees, 0.05 Hz (radians). */
static inline float corr_sway(float t) {
    float sn, cs;

    sh4der_sincos_(t * (2.0f * 3.14159265f * 0.05f), &sn, &cs);
    (void)cs;
    return 0.105f * sn;
}

/* ---- a frame ------------------------------------------------------------------------------------------------------- */

typedef struct corr_vtx_ { float x, y, z, u, v; uint32_t argb; } corr_vtx_t_;

static inline corr_vtx_t_ corr_lerp_(const corr_vtx_t_ *a, const corr_vtx_t_ *b, float t) {
    corr_vtx_t_ o;
    const unsigned ca = a->argb, cb = b->argb;
    unsigned s, c = 0xff000000u;

    o.x = a->x + (b->x - a->x) * t; o.y = a->y + (b->y - a->y) * t; o.z = a->z + (b->z - a->z) * t;
    o.u = a->u + (b->u - a->u) * t; o.v = a->v + (b->v - a->v) * t;
    for (s = 0; s < 24u; s += 8u) {
        const float x = (float)((ca >> s) & 255u), y = (float)((cb >> s) & 255u);

        c |= (unsigned)(x + (y - x) * t + 0.5f) << s;
    }
    o.argb = c;
    return o;
}

/* Clip a loop of 4 view-space points at z >= CORR_NEAR: up to 5 out, in loop order. */
static inline unsigned corr_clip_near(const corr_vtx_t_ in[4], corr_vtx_t_ out[5]) {
    unsigned i, n = 0;

    for (i = 0; i < 4u; i++) {
        const corr_vtx_t_ *a = &in[i], *b = &in[(i + 1u) & 3u];
        const int ia = a->z >= CORR_NEAR, ib = b->z >= CORR_NEAR;

        if (ia && n < 5u) out[n++] = *a;
        if (ia != ib && n < 5u) out[n++] = corr_lerp_(a, b, (CORR_NEAR - a->z) * sh4der_recip_pos_(sh4der_abs_(b->z - a->z)) * (b->z > a->z ? 1.0f : -1.0f));
    }
    return n;
}

/* Loop order -> strip order: 0, 1, n-1, 2, n-2, ... */
static inline unsigned corr_strip_index(unsigned k, unsigned n) {
    return k == 0u ? 0u : (k & 1u) ? (k + 1u) / 2u : n - k / 2u;
}

static inline unsigned corr_bin_(float v, unsigned bins) {
    const int b = (int)v / (int)CORR_BIN;

    return b < 0 ? 0u : b >= (int)bins ? bins - 1u : (unsigned)b;
}

/* The ring as the eye sees it into `out`. need_ndv: fill ndv (only the slope bias reads it). cap: a segment starting
   this far ahead or more is its far kind (CORR_CAP; CORR_NO_CAP: every segment near, the first design). */
static inline void corr_frame(const corr_kind_t kinds[CORR_KINDS], const corr_eye_t *e, int need_ndv, float cap, corr_list_t *out) {
    const int base = corr_ring_base(e->s);
    float sn, cs;
    unsigned r;

    sh4der_sincos_(e->yaw, &sn, &cs);
    out->n = 0; out->culled = out->clipped = out->dropped = 0;
    for (r = 0; r < CORR_RING; r++) {
        const int b = base + (int)r;
        const float zoff = (float)b * CORR_SEG - e->s;
        const corr_kind_t *k = &kinds[((unsigned)b & (CORR_IDS - 1u)) + (zoff >= cap ? CORR_IDS : 0u)];
        unsigned i;

        for (i = 0; i < k->n; i++) {
            const corr_quad_t *q = &k->q[i];
            corr_vtx_t_ v4[4], lp[5];
            unsigned j, n, out_l = 0, out_r = 0, out_t = 0, out_b = 0, behind = 0;
            corr_poly_t *p;
            float x0 = 1e9f, x1 = -1e9f, y0 = 1e9f, y1 = -1e9f;
            int bad = 0;

            for (j = 0; j < 4u; j++) {
                static const unsigned loop[4] = { 0u, 1u, 3u, 2u };   /* strip order -> loop order */
                const unsigned s = loop[j];
                const float dx = q->p[s][0] - e->x, dy = q->p[s][1] - CORR_EYE_Y, dz = q->p[s][2] + zoff;
                corr_vtx_t_ *o = &v4[j];

                o->x = sh4der_fma_(dx, cs, -dz * sn);
                o->z = sh4der_fma_(dx, sn, dz * cs);
                o->y = dy;
                o->u = q->uv[s][0]; o->v = q->uv[s][1]; o->argb = q->argb[s];
                out_l += o->x < -o->z; out_r += o->x > o->z;
                out_t += o->y * CORR_F > o->z * CORR_CY; out_b += -o->y * CORR_F > o->z * CORR_CY;
                behind += o->z < CORR_NEAR;
            }
            if (behind == 4u || out_l == 4u || out_r == 4u || out_t == 4u || out_b == 4u || out->n >= CORR_POLY_MAX) { out->culled++; continue; }
            if (behind) { n = corr_clip_near(v4, lp); out->clipped++; if (n < 3u) { out->culled++; continue; } }
            else { for (j = 0; j < 4u; j++) lp[j] = v4[j]; n = 4u; }
            p = &out->p[out->n];
            for (j = 0; j < n; j++) {
                const corr_vtx_t_ *s = &lp[corr_strip_index(j, n)];
                const float w = sh4der_recip_pos_(s->z);

                p->x[j] = CORR_CX + CORR_F * s->x * w;
                p->y[j] = CORR_CY - CORR_F * s->y * w;
                p->z[j] = w;
                p->u[j] = s->u; p->v[j] = s->v; p->argb[j] = s->argb;
                if (p->x[j] < x0) x0 = p->x[j];
                if (p->x[j] > x1) x1 = p->x[j];
                if (p->y[j] < y0) y0 = p->y[j];
                if (p->y[j] > y1) y1 = p->y[j];
            }
            for (j = 0; j + 2u < n; j++) {       /* (c): each triangle of the strip; points on the plane z = 1 at x / F, y / F */
                const float a[3] = { p->x[j] * (1.0f / CORR_F), p->y[j] * (1.0f / CORR_F), 1.0f };
                const float bb[3] = { p->x[j + 1u] * (1.0f / CORR_F), p->y[j + 1u] * (1.0f / CORR_F), 1.0f };
                const float c[3] = { p->x[j + 2u] * (1.0f / CORR_F), p->y[j + 2u] * (1.0f / CORR_F), 1.0f };

                if (!sh4der_tri_area_ok(a, bb, c, 1.0f, SH4DER_GUARD_MIN_AREA2)) bad = 1;
            }
            if (bad) { out->dropped++; continue; }
            p->n = (uint8_t)n; p->tex = q->tex;
            p->bx0 = (uint8_t)corr_bin_(x0, CORR_BINS_X); p->bx1 = (uint8_t)corr_bin_(x1, CORR_BINS_X);
            p->by0 = (uint8_t)corr_bin_(y0, CORR_BINS_Y); p->by1 = (uint8_t)corr_bin_(y1, CORR_BINS_Y);
            p->ndv = 1.0f;
            if (need_ndv) {
                const float d[3] = { e->x - q->c[0], CORR_EYE_Y - q->c[1], -(q->c[2] + zoff) };
                const float l2 = sh4der_len3_(d[0], d[1], d[2]);

                p->ndv = l2 > 1e-6f ? sh4der_min1_(sh4der_abs_(sh4der_dot3_(q->n, d)) * sh4der_recip_pos_(sh4der_sqrt_(l2))) : 1.0f;
            }
            out->n++;
        }
    }
}

/* ---- the bin counter (count-only) ---------------------------------------------------------------------------------- */

typedef struct corr_bins { uint16_t c[CORR_BINS_Y][CORR_BINS_X]; } corr_bins_t;

static inline void corr_bins_clear(corr_bins_t *b) { memset(b, 0, sizeof *b); }

/* p's box into the bins, inside bin columns c0 .. c1 only (a split's half: the clip keeps the rest out). */
static inline void corr_bins_add_cols(corr_bins_t *b, const corr_poly_t *p, unsigned c0, unsigned c1) {
    const unsigned x0 = p->bx0 > c0 ? p->bx0 : c0, x1 = p->bx1 < c1 ? p->bx1 : c1;
    unsigned x, y;

    for (y = p->by0; y <= p->by1; y++)
        for (x = x0; x <= x1; x++) b->c[y][x]++;
}

static inline void corr_bins_add(corr_bins_t *b, const corr_poly_t *p) { corr_bins_add_cols(b, p, 0u, CORR_BINS_X - 1u); }

static inline unsigned corr_bins_max(const corr_bins_t *b) {
    unsigned x, y, m = 0;

    for (y = 0; y < CORR_BINS_Y; y++)
        for (x = 0; x < CORR_BINS_X; x++) if (b->c[y][x] > m) m = b->c[y][x];
    return m;
}

/* Of the polygons whose box lies wholly in bin columns c0 .. c1 or overlaps them: 1 when p touches those columns. */
static inline int corr_poly_in_cols(const corr_poly_t *p, unsigned c0, unsigned c1) { return p->bx1 >= c0 && p->bx0 <= c1; }

#endif /* CORR_SCENE_H */
