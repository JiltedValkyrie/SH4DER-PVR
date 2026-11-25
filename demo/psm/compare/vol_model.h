/* SH4DER-PVR: the modifier volume spike, the meshes and the volume builders

   demo/psm/compare/vol_model.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Shadow technique: John Brooks (the World Series Baseball 2K2 renderer): shadows as PVR modifier volumes extruded
   from the caster's own triangles. Used with his permission, credit required. This file is our own code for it.

   A shadow volume for a directional light is the caster swept away from the light. The PVR takes it as a closed
   set of triangles and marks the pixels of the scene that lie inside. Two ways to make that set from a mesh:

     per triangle   every triangle on the cap side becomes its own prism: itself, itself at the far end, and two
                    triangles a side: 8 a triangle. No table, no demand on the mesh. Prisms of neighbours share
                    sides, each sent twice; inside ONE volume those cancel if the PVR counts by parity.
     silhouette     needs the mesh's edges (each with its two triangles, built once) and a closed mesh. The cap
                    side's triangles, the same at the far end, and two triangles for each edge whose two triangles
                    face different ways.

   The cap side is a choice: the triangles facing the light (the lit faces lie ON the volume's near end) or the ones
   facing away (the dark faces do, and the lit faces are clear of it).

   Output is index triples into a vertex array of 2 x nv: i is the mesh's vertex i at the near end, i + nv the same
   vertex at the far end. Plain C, no library; the host test is test/vol_model_test.c.
*/

#ifndef VOL_MODEL_H
#define VOL_MODEL_H

#include <math.h>
#include <stdint.h>
#include <string.h>

#define VOL_MAX_V 320u
#define VOL_MAX_T 640u
#define VOL_MAX_E 960u
#define VOL_OUT_MAX (8u * VOL_MAX_T)       /* the most triangles a builder writes */

typedef struct vol_mesh {
    float (*p)[3];                         /* nv positions */
    uint16_t (*tri)[3];                    /* nt triangles, counter-clockwise seen from outside */
    uint16_t (*edge)[4];                   /* ne edges: vertex, vertex, triangle, triangle */
    unsigned nv, nt, ne;
} vol_mesh_t;

static inline float vol_dot(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

static inline void vol_cross(const float a[3], const float b[3], float out[3]) {
    out[0] = a[1] * b[2] - a[2] * b[1]; out[1] = a[2] * b[0] - a[0] * b[2]; out[2] = a[0] * b[1] - a[1] * b[0];
}

static inline void vol_unit(float v[3]) {
    const float l = sqrtf(vol_dot(v, v));

    if (l > 0.0f) { v[0] /= l; v[1] /= l; v[2] /= l; }
}

/* The triangle's normal, not unit: (b - a) x (c - a). */
static inline void vol_tri_normal(const vol_mesh_t *m, unsigned t, float n[3]) {
    const float *a = m->p[m->tri[t][0]], *b = m->p[m->tri[t][1]], *c = m->p[m->tri[t][2]];
    const float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };

    vol_cross(e1, e2, n);
}

/* Turn every triangle to face away from `centre` (right for a convex mesh round it). */
static inline void vol_orient(vol_mesh_t *m, const float centre[3]) {
    unsigned t;

    for (t = 0; t < m->nt; t++) {
        const float *a = m->p[m->tri[t][0]];
        const float out[3] = { a[0] - centre[0], a[1] - centre[1], a[2] - centre[2] };
        float n[3];

        vol_tri_normal(m, t, n);
        if (vol_dot(n, out) < 0.0f) { const uint16_t s = m->tri[t][1]; m->tri[t][1] = m->tri[t][2]; m->tri[t][2] = s; }
    }
}

/* Build the edge table. 1 when the mesh is closed: every edge has exactly two triangles. The mesh's `edge` must
   hold 3 nt / 2 entries. Done once, at load. */
static inline int vol_edges_build(vol_mesh_t *m) {
    enum { HASH = 4096 };
    static int16_t slot[HASH];
    unsigned t, k;

    memset(slot, -1, sizeof slot);
    m->ne = 0;
    for (t = 0; t < m->nt; t++) {
        for (k = 0; k < 3u; k++) {
            const unsigned a = m->tri[t][k], b = m->tri[t][(k + 1u) % 3u], lo = a < b ? a : b, hi = a < b ? b : a;
            unsigned h = (lo * 73u + hi * 151u) % HASH;

            for (;; h = (h + 1u) % HASH) {
                const int e = slot[h];

                if (e < 0) {
                    if (m->ne >= VOL_MAX_E) return 0;
                    slot[h] = (int16_t)m->ne;
                    m->edge[m->ne][0] = (uint16_t)a; m->edge[m->ne][1] = (uint16_t)b;   /* in the first triangle's order */
                    m->edge[m->ne][2] = (uint16_t)t; m->edge[m->ne][3] = 0xffffu;
                    m->ne++;
                    break;
                }
                if ((m->edge[e][0] == lo && m->edge[e][1] == hi) || (m->edge[e][0] == hi && m->edge[e][1] == lo)) {
                    if (m->edge[e][3] != 0xffffu) return 0;           /* a third triangle on one edge */
                    m->edge[e][3] = (uint16_t)t;
                    break;
                }
            }
        }
    }
    for (k = 0; k < m->ne; k++) if (m->edge[k][3] == 0xffffu) return 0;
    return 1;
}

/* A sphere of `segs` x `stacks`: segs (stacks - 1) + 2 vertices, 2 segs (stacks - 1) triangles (20 x 16: 302, 600). */
static inline void vol_sphere(vol_mesh_t *m, unsigned segs, unsigned stacks, float r, const float c[3]) {
    const float pi = 3.14159265f;
    unsigned i, j, t = 0;

    m->nv = segs * (stacks - 1u) + 2u;
    m->p[0][0] = c[0]; m->p[0][1] = c[1] + r; m->p[0][2] = c[2];
    for (j = 1; j < stacks; j++) {
        const float th = pi * (float)j / (float)stacks;

        for (i = 0; i < segs; i++) {
            const float ph = 2.0f * pi * ((float)i + 0.25f) / (float)segs;
            float *p = m->p[1u + (j - 1u) * segs + i];

            p[0] = c[0] + r * sinf(th) * cosf(ph); p[1] = c[1] + r * cosf(th); p[2] = c[2] + r * sinf(th) * sinf(ph);
        }
    }
    m->p[m->nv - 1u][0] = c[0]; m->p[m->nv - 1u][1] = c[1] - r; m->p[m->nv - 1u][2] = c[2];
    for (i = 0; i < segs; i++) {
        const unsigned n = (i + 1u) % segs, last = 1u + (stacks - 2u) * segs;

        m->tri[t][0] = 0; m->tri[t][1] = (uint16_t)(1u + i); m->tri[t][2] = (uint16_t)(1u + n); t++;
        m->tri[t][0] = (uint16_t)(m->nv - 1u); m->tri[t][1] = (uint16_t)(last + i); m->tri[t][2] = (uint16_t)(last + n); t++;
    }
    for (j = 0; j + 2u < stacks; j++) {
        for (i = 0; i < segs; i++) {
            const unsigned n = (i + 1u) % segs, a = 1u + j * segs, b = a + segs;

            m->tri[t][0] = (uint16_t)(a + i); m->tri[t][1] = (uint16_t)(b + i); m->tri[t][2] = (uint16_t)(b + n); t++;
            m->tri[t][0] = (uint16_t)(a + i); m->tri[t][1] = (uint16_t)(b + n); m->tri[t][2] = (uint16_t)(a + n); t++;
        }
    }
    m->nt = t;
    vol_orient(m, c);
}

/* A box from `lo` to `hi`: 8 vertices (bit 0 x, bit 1 y, bit 2 z), 12 triangles. */
static inline void vol_box(vol_mesh_t *m, const float lo[3], const float hi[3]) {
    static const uint8_t q[6][4] = { {0,1,3,2}, {4,6,7,5}, {0,4,5,1}, {2,3,7,6}, {0,2,6,4}, {1,5,7,3} };
    const float c[3] = { 0.5f * (lo[0] + hi[0]), 0.5f * (lo[1] + hi[1]), 0.5f * (lo[2] + hi[2]) };
    unsigned i;

    m->nv = 8; m->nt = 12;
    for (i = 0; i < 8u; i++) {
        m->p[i][0] = (i & 1u) ? hi[0] : lo[0]; m->p[i][1] = (i & 2u) ? hi[1] : lo[1]; m->p[i][2] = (i & 4u) ? hi[2] : lo[2];
    }
    for (i = 0; i < 6u; i++) {
        m->tri[2u * i][0] = q[i][0]; m->tri[2u * i][1] = q[i][1]; m->tri[2u * i][2] = q[i][2];
        m->tri[2u * i + 1u][0] = q[i][0]; m->tri[2u * i + 1u][1] = q[i][2]; m->tri[2u * i + 1u][2] = q[i][3];
    }
    vol_orient(m, c);
}

/* side[t] = 1 where triangle t faces the light (`to_light`: from the surface toward the light). Returns how many. */
static inline unsigned vol_facing(const vol_mesh_t *m, const float to_light[3], uint8_t *side) {
    unsigned t, lit = 0;

    for (t = 0; t < m->nt; t++) {
        float n[3];

        vol_tri_normal(m, t, n);
        side[t] = vol_dot(n, to_light) > 0.0f;
        lit += side[t];
    }
    return lit;
}

/* Silhouette volume: the triangles whose side is `cap`, the same at the far end, and two triangles a silhouette edge.
   Needs the edge table. Returns the triangles written; *sil the silhouette edges. */
static inline unsigned vol_extrude_sil(const vol_mesh_t *m, const uint8_t *side, int cap, uint16_t (*out)[3], unsigned *sil) {
    const uint16_t nv = (uint16_t)m->nv;
    unsigned t, e, n = 0, s = 0;

    for (t = 0; t < m->nt; t++) {
        const uint16_t *v = m->tri[t];

        if (side[t] != cap) continue;
        out[n][0] = v[0]; out[n][1] = v[1]; out[n][2] = v[2]; n++;
        out[n][0] = (uint16_t)(v[0] + nv); out[n][1] = (uint16_t)(v[2] + nv); out[n][2] = (uint16_t)(v[1] + nv); n++;
    }
    for (e = 0; e < m->ne; e++) {
        const uint16_t a = m->edge[e][0], b = m->edge[e][1];

        if (side[m->edge[e][2]] == side[m->edge[e][3]]) continue;
        out[n][0] = a; out[n][1] = b; out[n][2] = (uint16_t)(b + nv); n++;
        out[n][0] = a; out[n][1] = (uint16_t)(b + nv); out[n][2] = (uint16_t)(a + nv); n++;
        s++;
    }
    if (sil) *sil = s;
    return n;
}

/* Per-triangle volume: a prism for every triangle whose side is `cap`, 8 triangles each. No edge table. */
static inline unsigned vol_extrude_tri(const vol_mesh_t *m, const uint8_t *side, int cap, uint16_t (*out)[3]) {
    const uint16_t nv = (uint16_t)m->nv;
    unsigned t, k, n = 0;

    for (t = 0; t < m->nt; t++) {
        const uint16_t *v = m->tri[t];

        if (side[t] != cap) continue;
        out[n][0] = v[0]; out[n][1] = v[1]; out[n][2] = v[2]; n++;
        out[n][0] = (uint16_t)(v[0] + nv); out[n][1] = (uint16_t)(v[2] + nv); out[n][2] = (uint16_t)(v[1] + nv); n++;
        for (k = 0; k < 3u; k++) {
            const uint16_t a = v[k], b = v[(k + 1u) % 3u];

            out[n][0] = a; out[n][1] = b; out[n][2] = (uint16_t)(b + nv); n++;
            out[n][0] = a; out[n][1] = (uint16_t)(b + nv); out[n][2] = (uint16_t)(a + nv); n++;
        }
    }
    return n;
}

/* The far end of a vertex: `p` moved away from the light until it is on the plane y = plane_y (to_light[1] > 0). */
static inline void vol_far_plane(const float p[3], const float to_light[3], float plane_y, float out[3]) {
    const float t = (p[1] - plane_y) / to_light[1];

    out[0] = p[0] - to_light[0] * t; out[1] = plane_y; out[2] = p[2] - to_light[2] * t;
}

/* ---- a camera ---------------------------------------------------------------------- */

typedef struct vol_cam {
    float eye[3], right[3], up[3], fwd[3];
    float focal, cx, cy, near;             /* focal length and centre in pixels; depth under `near` is refused */
} vol_cam_t;

static inline void vol_cam_look(vol_cam_t *c, const float eye[3], const float target[3], float focal, float cx, float cy) {
    static const float world_up[3] = { 0.0f, 1.0f, 0.0f };

    memcpy(c->eye, eye, sizeof c->eye);
    c->fwd[0] = target[0] - eye[0]; c->fwd[1] = target[1] - eye[1]; c->fwd[2] = target[2] - eye[2];
    vol_unit(c->fwd);
    vol_cross(world_up, c->fwd, c->right);
    vol_unit(c->right);
    vol_cross(c->fwd, c->right, c->up);
    c->focal = focal; c->cx = cx; c->cy = cy; c->near = 0.1f;
}

/* World to screen: x, y in pixels, z = 1 / depth as the PVR wants it. 0 when the point is nearer than `near`. */
static inline int vol_project(const vol_cam_t *c, const float p[3], float out[3]) {
    const float d[3] = { p[0] - c->eye[0], p[1] - c->eye[1], p[2] - c->eye[2] };
    const float z = vol_dot(d, c->fwd);
    float inv;

    if (z < c->near) return 0;
    inv = 1.0f / z;
    out[0] = c->cx + c->focal * vol_dot(d, c->right) * inv;
    out[1] = c->cy - c->focal * vol_dot(d, c->up) * inv;
    out[2] = inv;
    return 1;
}

#endif /* VOL_MODEL_H */
