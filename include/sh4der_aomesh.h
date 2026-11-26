/* SH4DER-PVR: baked ambient occlusion over a triangle mesh: a cell grid of its triangles, 12 rays a vertex, and a byte
   grid over the ground

   include/sh4der_aomesh.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).

   RECOMMENDED (no console run: nothing here is measured; wiki/limits/header-recommended.md)
   Cost:       at load, once a scene. On the town's mesh, counted on the host (exact): 190,884 rays, 1,594,130 ray-triangle
               tests, 69 ms on a desktop; the SH-4's time is NOT MEASURED. RAM: a byte a vertex and a byte a ground cell
               (70 x 103 there), and during the bake only the triangles' boxes, the cell table and its lists. A frame: one
               table read a vertex: ESTIMATE
   Start with: SH4DER_AOMESH_RAYS 12, SH4DER_AOMESH_REACH 1.6f, SH4DER_AOMESH_GCELL 0.5f; the bake at load, a lookup a frame
   Edges:      the edge tests are on the numerators against det; a reciprocal is taken only for a triangle that passed them:
               134,879 on the town's mesh, against one a test past the |det| check before (at most 1,594,130); counted on
               the host (exact). The time it saves is NOT MEASURED. The change moved 3 of the town's 15,907 plain C bytes (the
               largest by 21), and the pin was taken again on purpose (2026-11-26).
   Pin:        SH4ZAM software path against plain C on the host: 6 of 15,907 bytes differ, the largest by 21 (MEASURED on the host,
               2026-11-26; before the edge change 6 and 20, two of them closed and two new). The console's FSRRA path NOT RUN, so the
               console's bytes are neither. Cause OPEN, upstream of the edge tests (the two paths' ray directions differ by an ulp).
   Status:     PROTOTYPE v0.1: host tested (test/aomesh_test.c; the town's bytes pinned in demo/psm/test/psm_ao_test.c);
               the bake's time and the look on a console are NOT MEASURED
   Do:         multiply the ambient light only; give the bake a tick so a watchdog stays quiet over it
   Do not:     bake in a frame; pass the movers (people, hens): they stay open (255), so bake the static part only
   Maturity:   PROTOTYPE v0.1

   How it differs from the other occlusion code:
     sh4der_aobake.h    bakes against exact shapes (spheres, boxes, a plane), 64 Hammersley rays a point.
     sh4der_ao.h        crease darkening from the angle between faces, and a screen cavity cue (no rays, no mesh).
     sh4der_aomesh.h    bakes against a triangle mesh through a cell grid, 12 golden-ratio rays a point, a reach of 1.6 m,
                        and a byte grid over the ground (the ground has no vertices of its own when its mesh moves).
     tools/ao_bake_core.h  the same kind of bake on the host, at build time (libc only): not for the console.

   The method (demo/psm/psm_ao.h, which is now an adapter over this header): from each vertex a ray set over its hemisphere,
   each ray against the triangles of the cells its box touches; a hit at t metres closes the ray by 1 - t / reach; the byte
   is 255 open .. 0 closed. The optional ground grid is one byte a cell over a rectangle and a margin, with the normal up. A
   vertex with a zero normal is open and casts no rays. The plane y = 0 is an occluder for the vertices always (the floor the
   mesh stands on), and for the grid's cells never (they lie on it). A NULL rectangle gives no grid: the vertex bytes only.

   No malloc or free, no globals. The caller owns every buffer, sized by sh4der_aomesh_need; sh4der_aomesh_bake writes
   nothing and returns 0 when a buffer is too small, the mesh is empty or unusable, or the cell grid would be too big.

   Call order:
     1. At load: fill a sh4der_aomesh_tris_t (positions, their stride in floats (at least 3: 3 for x y z packed, 4 for a padded vertex), the index list, the
        triangle count). Positions and indices must be in range; the header does not check them.
     2. sh4der_aomesh_need(&mesh, ground_or_NULL, &need): the bytes of each buffer. Allocate them (box: need.box entries of
        float[6]; cell: need.cell uint32_t; list: need.list uint16_t; grid: need.grid bytes).
     3. sh4der_aomesh_bake(&mesh, normals, normal stride, vertex count, ground, &need, &work, vert_out, &grid, &stats, tick).
        A 1 is baked; a 0 wrote nothing (every byte the caller holds is as it was).
     4. The vertex bytes are vert_out; the ground's bytes are the grid: sh4der_aomesh_ground(&grid, x, z) (255 outside).
        Multiply the ambient light by the byte. A frame costs one table read.

   The maths goes through sh4der_core.h (SH4ZAM on the Dreamcast, plain C under SH4DER_CORE_NO_SH4ZAM). */

#ifndef SH4DER_AOMESH_H
#define SH4DER_AOMESH_H

#include <stddef.h>
#include <stdint.h>
#include "sh4der_core.h"

/* Start values, not limits (README "Aomesh tuning"). The town's bytes are pinned for these values. */
#define SH4DER_AOMESH_RAYS   12         /* rays a vertex: a golden-ratio spiral over the hemisphere, cosine weighted */
#define SH4DER_AOMESH_REACH  1.6f       /* metres a ray looks for an occluder; a hit at t closes the ray by 1 - t / reach */
#define SH4DER_AOMESH_PUSH   0.03f      /* metres a ray starts off its surface */
#define SH4DER_AOMESH_CELL   2.0f       /* metres: the triangles' grid; not under the reach, so a ray touches 4 cells at most */
#define SH4DER_AOMESH_GCELL  0.5f       /* metres: the ground's grid of bytes */
#define SH4DER_AOMESH_MARGIN 8.0f       /* metres: the ground's grid goes this far past the rectangle */
#define SH4DER_AOMESH_GMAX   (256 * 256) /* the most cells of the ground's grid */

#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
_Static_assert(SH4DER_AOMESH_CELL >= SH4DER_AOMESH_REACH, "SH4DER_AOMESH_CELL must not be under SH4DER_AOMESH_REACH");
#endif

/* The mesh: vertex v at pos + v * stride (x, y, z; stride in floats, at least 3: the bake refuses less).
   Triangle t is the vertices idx[t][0..2]. Both sides of a triangle occlude. */
typedef struct sh4der_aomesh_tris {
    const float *pos;
    unsigned stride;
    const uint16_t (*idx)[3];
    unsigned ntri;
} sh4der_aomesh_tris_t;

/* The ground rectangle, world x and z. NULL, or one with x1 <= x0 or z1 <= z0, or too big for SH4DER_AOMESH_GMAX: no grid. */
typedef struct sh4der_aomesh_rect { float x0, z0, x1, z1; } sh4der_aomesh_rect_t;

/* The sizes of the buffers, in entries: what sh4der_aomesh_need gives, and what the caller allocated (the same struct). */
typedef struct sh4der_aomesh_need {
    unsigned box;     /* float[6] entries: a triangle's low and high corners */
    unsigned cell;    /* uint32_t entries: the triangle grid's cw * ch + 1 starts */
    unsigned list;    /* uint16_t entries: the triangles in the cells, one entry a reference (a triangle in 1 to 4 cells) */
    unsigned grid;    /* bytes: the ground's grid, 0 when there is none */
    unsigned cw, ch;  /* the triangle grid's size, in cells */
    float cx0, cz0;   /* the triangle grid's origin */
} sh4der_aomesh_need_t;

/* The scratch the bake uses. The first three are given back by the caller after the bake; the grid is kept (it is the
   output's bytes). Each pointer must have its need's entries. */
typedef struct sh4der_aomesh_work {
    float (*box)[6];
    uint32_t *cell;
    uint16_t *list;
    uint8_t *grid;
} sh4der_aomesh_work_t;

/* The ground's grid after the bake: its origin, its size and its bytes (row by row along x). g NULL: none. */
typedef struct sh4der_aomesh_grid {
    float x0, z0;
    unsigned gw, gh;
    uint8_t *g;
} sh4der_aomesh_grid_t;

/* The counts of the last bake (optional, may be NULL). */
typedef struct sh4der_aomesh_stats { uint32_t rays, tests, recips; } sh4der_aomesh_stats_t;   /* recips: reciprocals taken (a triangle that passed the edge tests) */

/* One triangle's box, from its three vertices (the order of the box entries: lo x, lo y, lo z, hi x, hi y, hi z). */
static inline void sh4der_aomesh_box_(const sh4der_aomesh_tris_t *m, unsigned t, float b[6]) {
    const float *a = m->pos + (size_t)m->idx[t][0] * m->stride, *q = m->pos + (size_t)m->idx[t][1] * m->stride;
    const float *r = m->pos + (size_t)m->idx[t][2] * m->stride;
    unsigned k;

    for (k = 0; k < 3u; k++) {
        const float p0 = a[k], p1 = q[k], p2 = r[k];

        b[k] = p0 < p1 ? (p0 < p2 ? p0 : p2) : (p1 < p2 ? p1 : p2);
        b[3u + k] = p0 > p1 ? (p0 > p2 ? p0 : p2) : (p1 > p2 ? p1 : p2);
    }
}

/* The ground grid's origin and size for a rectangle. 0: no grid (NULL, empty, or too big). */
static inline int sh4der_aomesh_gdims_(const sh4der_aomesh_rect_t *r, float *gx0, float *gz0, unsigned *gw, unsigned *gh) {
    const float ig = 1.0f / SH4DER_AOMESH_GCELL;
    float fw, fh;

    if (!r || !(r->x1 > r->x0) || !(r->z1 > r->z0)) return 0;
    fw = (r->x1 - r->x0 + 2.0f * SH4DER_AOMESH_MARGIN) * ig;
    fh = (r->z1 - r->z0 + 2.0f * SH4DER_AOMESH_MARGIN) * ig;
    if (!(fw < 65536.0f) || !(fh < 65536.0f)) return 0;
    *gw = (unsigned)fw + 1u; *gh = (unsigned)fh + 1u;
    if (*gw > (unsigned)SH4DER_AOMESH_GMAX / *gh) return 0;
    *gx0 = r->x0 - SH4DER_AOMESH_MARGIN; *gz0 = r->z0 - SH4DER_AOMESH_MARGIN;
    return 1;
}

/* The sizes for a mesh and a ground rectangle. 0 (and out zeroed) when the mesh is empty or unusable: no triangles, more
   than 65536 (a 16-bit index), or a cell grid over 65536 cells. */
static inline int sh4der_aomesh_need(const sh4der_aomesh_tris_t *m, const sh4der_aomesh_rect_t *ground, sh4der_aomesh_need_t *out) {
    const float ic = 1.0f / SH4DER_AOMESH_CELL;
    float lo[2] = { 1e9f, 1e9f }, hi[2] = { -1e9f, -1e9f }, b[6];
    float fx, fz, gx0, gz0;
    unsigned i, gw, gh, cw, ch, x0, x1, z0, z1, x, z;
    uint32_t refs = 0;

    if (out) { out->box = out->cell = out->list = out->grid = out->cw = out->ch = 0u; out->cx0 = out->cz0 = 0.0f; }
    if (!m || !out || !m->pos || !m->idx || !m->ntri || m->ntri > 65536u || m->stride < 3u) return 0;
    if (!(SH4DER_AOMESH_CELL >= SH4DER_AOMESH_REACH)) return 0;
    for (i = 0; i < m->ntri; i++) {
        sh4der_aomesh_box_(m, i, b);
        if (b[0] < lo[0]) lo[0] = b[0];
        if (b[2] < lo[1]) lo[1] = b[2];
        if (b[3] > hi[0]) hi[0] = b[3];
        if (b[5] > hi[1]) hi[1] = b[5];
    }
    fx = (hi[0] - lo[0]) * ic; fz = (hi[1] - lo[1]) * ic;
    if (!(fx < 65536.0f) || !(fz < 65536.0f)) return 0;
    cw = (unsigned)fx + 1u; ch = (unsigned)fz + 1u;
    if (cw > 65536u / ch) return 0;
    for (i = 0; i < m->ntri; i++) {                      /* the references: the triangle in every cell its box touches */
        sh4der_aomesh_box_(m, i, b);
        x0 = (unsigned)((b[0] - lo[0]) * ic); x1 = (unsigned)((b[3] - lo[0]) * ic);
        z0 = (unsigned)((b[2] - lo[1]) * ic); z1 = (unsigned)((b[5] - lo[1]) * ic);
        for (z = z0; z <= z1 && z < ch; z++)
            for (x = x0; x <= x1 && x < cw; x++) refs++;
    }
    out->box = m->ntri;
    out->cell = cw * ch + 1u;
    out->list = refs ? (unsigned)refs : 1u;
    out->cw = cw; out->ch = ch;
    out->cx0 = lo[0]; out->cz0 = lo[1];
    out->grid = sh4der_aomesh_gdims_(ground, &gx0, &gz0, &gw, &gh) ? gw * gh : 0u;
    return 1;
}

/* A ray against a triangle, both sides: the distance, or a negative number for none within (1e-4, reach). The edge
   tests are on the numerators against det (no reciprocal, so its error cannot move a ray across a shared edge; the
   numerators' own rounding still can: the two host paths differ at 6 bytes); the reciprocal is taken only for a triangle that passed them, for t (*recips counts it). */
static inline float sh4der_aomesh_tri_(const float o[3], const float d[3], const float a[3], const float b[3], const float c[3], float reach,
                                       uint32_t *recips) {
    const float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
    float h[3], s[3], q[3], det, inv, un, vn, t;

    sh4der_cross_(d, e2, h);
    det = sh4der_dot3_(e1, h);
    if (det > -1e-9f && det < 1e-9f) return -1.0f;
    s[0] = o[0] - a[0]; s[1] = o[1] - a[1]; s[2] = o[2] - a[2];
    un = sh4der_dot3_(s, h);
    if (det > 0.0f ? (un < 0.0f || un > det) : (un > 0.0f || un < det)) return -1.0f;
    sh4der_cross_(s, e1, q);
    vn = sh4der_dot3_(d, q);
    if (det > 0.0f ? (vn < 0.0f || un + vn > det) : (vn > 0.0f || un + vn < det)) return -1.0f;
    inv = det > 0.0f ? sh4der_recip_pos_(det) : -sh4der_recip_pos_(-det);
    (*recips)++;
    t = sh4der_dot3_(e2, q) * inv;
    return t > 1e-4f && t < reach ? t : -1.0f;
}

/* The bake's state: the mesh, its boxes and cells, the counts. */
typedef struct sh4der_aomesh_ctx {
    const sh4der_aomesh_tris_t *m;
    float (*box)[6];
    const uint32_t *cell;
    const uint16_t *list;
    float cx0, cz0;
    unsigned cw, ch;
    uint32_t rays, tests, recips;
} sh4der_aomesh_ctx_t;

/* The closed share of one ray from o along d (unit): the nearest hit within the reach, against the ground (y 0, when
   `ground`) and the triangles of the cells its box touches. 0 (nothing) .. 1 (a hit at its start). */
static inline float sh4der_aomesh_ray_(sh4der_aomesh_ctx_t *c, const float o[3], const float d[3], int ground) {
    const float e[3] = { o[0] + d[0] * SH4DER_AOMESH_REACH, o[1] + d[1] * SH4DER_AOMESH_REACH, o[2] + d[2] * SH4DER_AOMESH_REACH };
    const float lo[3] = { o[0] < e[0] ? o[0] : e[0], o[1] < e[1] ? o[1] : e[1], o[2] < e[2] ? o[2] : e[2] };
    const float hi[3] = { o[0] > e[0] ? o[0] : e[0], o[1] > e[1] ? o[1] : e[1], o[2] > e[2] ? o[2] : e[2] };
    const float ic = 1.0f / SH4DER_AOMESH_CELL;
    int x0 = (int)((lo[0] - c->cx0) * ic), x1 = (int)((hi[0] - c->cx0) * ic), z0 = (int)((lo[2] - c->cz0) * ic), z1 = (int)((hi[2] - c->cz0) * ic), x, z;
    float best = SH4DER_AOMESH_REACH;

    c->rays++;
    if (ground && d[1] < -1e-6f) {
        const float t = o[1] * sh4der_recip_pos_(-d[1]);

        if (t < best) best = t > 0.0f ? t : 0.0f;
    }
    if (lo[0] < c->cx0) x0 = 0;
    if (lo[2] < c->cz0) z0 = 0;
    if (x1 >= (int)c->cw) x1 = (int)c->cw - 1;
    if (z1 >= (int)c->ch) z1 = (int)c->ch - 1;
    for (z = z0; z <= z1; z++)
        for (x = x0; x <= x1; x++) {
            const unsigned cc = (unsigned)z * c->cw + (unsigned)x;
            uint32_t k;

            for (k = c->cell[cc]; k < c->cell[cc + 1u]; k++) {
                const unsigned t = c->list[k];
                const float *b = c->box[t];
                const sh4der_aomesh_tris_t *m = c->m;
                float hit;

                if (b[0] > hi[0] || b[3] < lo[0] || b[1] > hi[1] || b[4] < lo[1] || b[2] > hi[2] || b[5] < lo[2]) continue;
                c->tests++;
                hit = sh4der_aomesh_tri_(o, d, m->pos + (size_t)m->idx[t][0] * m->stride, m->pos + (size_t)m->idx[t][1] * m->stride,
                                         m->pos + (size_t)m->idx[t][2] * m->stride, best, &c->recips);
                if (hit >= 0.0f) best = hit;
            }
        }
    return 1.0f - best * (1.0f / SH4DER_AOMESH_REACH);
}

/* The byte at point p with unit normal n (a zero normal: open, no rays). dirs: the golden-ratio set. */
static inline uint8_t sh4der_aomesh_at_(sh4der_aomesh_ctx_t *c, float dirs[][3], const float p[3], const float n[3], int ground) {
    const float up[3] = { 0.0f, 1.0f, 0.0f }, side[3] = { 1.0f, 0.0f, 0.0f };
    const float o[3] = { p[0] + n[0] * SH4DER_AOMESH_PUSH, p[1] + n[1] * SH4DER_AOMESH_PUSH, p[2] + n[2] * SH4DER_AOMESH_PUSH };
    float t[3], b[3], shut = 0.0f;
    unsigned i;

    if (!(n[0] * n[0] + n[1] * n[1] + n[2] * n[2] > 1e-12f)) return 255u;
    sh4der_cross_(n, n[1] > 0.9f || n[1] < -0.9f ? side : up, t);
    sh4der_norm3_(t);
    sh4der_cross_(n, t, b);
    for (i = 0; i < SH4DER_AOMESH_RAYS; i++) {
        const float *l = dirs[i];
        const float d[3] = { t[0] * l[0] + b[0] * l[1] + n[0] * l[2], t[1] * l[0] + b[1] * l[1] + n[1] * l[2], t[2] * l[0] + b[2] * l[1] + n[2] * l[2] };

        shut += sh4der_aomesh_ray_(c, o, d, ground);
    }
    return (uint8_t)(sh4der_sat_(1.0f - shut * (1.0f / (float)SH4DER_AOMESH_RAYS)) * 255.0f + 0.5f);
}

/* The bake. Returns 1: the bytes are written (vert_out: nvert bytes; grid: its origin, size and bytes, g NULL when none).
   Returns 0 and writes nothing when: nvert or nrm is 0 or NULL, the mesh is unusable (sh4der_aomesh_need), a buffer is
   smaller than need (cap is the caller's sizes, the same struct as need), a work pointer is NULL, or the cell size is
   under the reach (the header's own check). Normals: nvert unit vectors at nrm + i * nstride (floats). `tick` (may be NULL)
   is called every 256 vertices and every 8 rows of the ground: a heartbeat for a watchdog. */
static inline int sh4der_aomesh_bake(const sh4der_aomesh_tris_t *m, const float *nrm, unsigned nstride, unsigned nvert,
                                     const sh4der_aomesh_rect_t *ground, const sh4der_aomesh_need_t *cap,
                                     const sh4der_aomesh_work_t *w, uint8_t *vert_out, sh4der_aomesh_grid_t *grid,
                                     sh4der_aomesh_stats_t *stats, void (*tick)(void)) {
    const float ic = 1.0f / SH4DER_AOMESH_CELL;
    static const float up_n[3] = { 0.0f, 1.0f, 0.0f };
    sh4der_aomesh_need_t need;
    sh4der_aomesh_ctx_t c;
    float dirs[SH4DER_AOMESH_RAYS][3], gx0 = 0.0f, gz0 = 0.0f;
    unsigned i, pass, gw = 0u, gh = 0u, x, z;

    if (!m || !nrm || nstride < 3u || !nvert || !vert_out || !grid || !cap || !w || !(SH4DER_AOMESH_CELL >= SH4DER_AOMESH_REACH)) return 0;
    if (!sh4der_aomesh_need(m, ground, &need)) return 0;
    if (cap->box < need.box || cap->cell < need.cell || cap->list < need.list || cap->grid < need.grid) return 0;
    if ((need.box && !w->box) || (need.cell && !w->cell) || (need.list && !w->list) || (need.grid && !w->grid)) return 0;

    if (need.grid) sh4der_aomesh_gdims_(ground, &gx0, &gz0, &gw, &gh);
    if (stats) stats->rays = stats->tests = stats->recips = 0u;
    c.m = m; c.box = w->box; c.cell = w->cell; c.list = w->list;
    c.cx0 = need.cx0; c.cz0 = need.cz0; c.cw = need.cw; c.ch = need.ch; c.rays = c.tests = c.recips = 0u;

    for (i = 0; i < SH4DER_AOMESH_RAYS; i++) {          /* golden-ratio spiral, cosine weighted: the same for every vertex */
        const float r2 = ((float)i + 0.5f) * (1.0f / (float)SH4DER_AOMESH_RAYS);
        float sn, cs;

        sh4der_sincos_(6.2831853f * sh4der_frac_((float)i * 0.61803399f), &sn, &cs);
        dirs[i][0] = sh4der_sqrt_(r2) * cs; dirs[i][1] = sh4der_sqrt_(r2) * sn; dirs[i][2] = sh4der_sqrt_(1.0f - r2);
    }
    for (i = 0; i < m->ntri; i++) sh4der_aomesh_box_(m, i, w->box[i]);
    for (i = 0; i < need.cell; i++) w->cell[i] = 0u;
    for (pass = 0; pass < 2u; pass++) {                 /* count, then fill: a triangle into every cell its box touches */
        if (pass) {
            uint32_t sum = 0;

            for (i = 0; i < need.cw * need.ch; i++) { const uint32_t n = w->cell[i]; w->cell[i] = sum; sum += n; }
            w->cell[need.cw * need.ch] = sum;
        }
        for (i = 0; i < m->ntri; i++) {
            const float *bx = w->box[i];
            const unsigned x0 = (unsigned)((bx[0] - c.cx0) * ic), x1 = (unsigned)((bx[3] - c.cx0) * ic);
            const unsigned z0 = (unsigned)((bx[2] - c.cz0) * ic), z1 = (unsigned)((bx[5] - c.cz0) * ic);
            unsigned xx, zz;

            for (zz = z0; zz <= z1 && zz < c.ch; zz++)
                for (xx = x0; xx <= x1 && xx < c.cw; xx++) {
                    if (pass) w->list[w->cell[zz * c.cw + xx]++] = (uint16_t)i;
                    else w->cell[zz * c.cw + xx]++;
                }
        }
    }
    for (i = need.cw * need.ch; i > 0u; i--) w->cell[i] = w->cell[i - 1u];   /* the fill moved each start to its end: back */
    w->cell[0] = 0u;

    for (i = 0; i < nvert; i++) {                       /* the vertices */
        vert_out[i] = sh4der_aomesh_at_(&c, dirs, m->pos + (size_t)i * m->stride, nrm + (size_t)i * nstride, 1);
        if (tick && (i & 255u) == 255u) tick();
    }
    grid->x0 = gx0; grid->z0 = gz0; grid->gw = gw; grid->gh = gh; grid->g = need.grid ? w->grid : NULL;
    if (need.grid) {                                    /* the ground: the cells' centres, the normal up, no ground plane */
        for (z = 0; z < gh; z++) {
            for (x = 0; x < gw; x++) {
                const float p[3] = { gx0 + ((float)x + 0.5f) * SH4DER_AOMESH_GCELL, 0.0f, gz0 + ((float)z + 0.5f) * SH4DER_AOMESH_GCELL };

                w->grid[z * gw + x] = sh4der_aomesh_at_(&c, dirs, p, up_n, 0);
            }
            if (tick && (z & 7u) == 7u) tick();
        }
    }
    if (stats) { stats->rays = c.rays; stats->tests = c.tests; stats->recips = c.recips; }
    return 1;
}

/* The ground's byte at (x, z): the cell's, 255 outside the grid (or with no bytes). */
static inline uint8_t sh4der_aomesh_ground(const sh4der_aomesh_grid_t *g, float x, float z) {
    const float ig = 1.0f / SH4DER_AOMESH_GCELL, fx = (x - g->x0) * ig, fz = (z - g->z0) * ig;

    if (!g->g || !(fx >= 0.0f) || !(fz >= 0.0f) || !(fx < (float)g->gw) || !(fz < (float)g->gh)) return 255u;
    return g->g[(unsigned)fz * g->gw + (unsigned)fx];
}

#endif /* SH4DER_AOMESH_H */
