/* SH4DER-PVR: baked ambient occlusion against exact shapes (spheres, boxes, a ground plane)

   include/sh4der_aobake.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).

   RECOMMENDED (from the wiki ledger, one console; wiki/limits/header-recommended.md)
   Start with: SH4DER_AOBAKE_RAYS 64, SH4DER_AOBAKE_REACH 3.0f, SH4DER_AOBAKE_PUSH 0.02f (defaults only: each call takes its own)
   Cost:       the load-time bake of demo/psm/ao scene A (2,142 vertices, 64 rays) 252 to 254 ms; scene B (2,544) 391 to 393 ms:
               MEASURED, one console, with the code this header moved unchanged (SH4-076, SH4-082). About 1.85 to 2.41 us
               a ray: DERIVED (SH4-076). Per vertex: ESTIMATE, 64 rays times the occluders a ray tests; not measured
   Status:     ADVISED for this copy: the host bytes match the code the bake was measured with (test/aobake_test.c checks
               the ray set; the ao4 scenes' bytes were checked before and after the move); nothing of this header is measured on a console
   Do:         bake at load, once per scene, not per frame; pass the reach your scene needs
   Do not:     bake a moving occluder at run time; read the byte as physical light (it is a multiply on the ambient light)
   Maturity:   ALPHA v0.5

   The bake, in one place. A point casts nrays cosine-weighted rays over its hemisphere (a Hammersley set, the same set
   as tools/ao_bake_core.h to 1e-6: test/aobake_test.c), each from `push` metres off its surface, against the exact
   occluders (spheres, boxes, and optionally a ground plane). A hit at t inside `reach` takes 1 - t / reach of a ray;
   a ray that misses takes nothing; a start inside a sphere or a box is 0 (closed). byte = 255 (1 - sum / nrays).

   How it differs from the other occlusion code:
     sh4der_ao.h        crease darkening from the angle between faces, and a screen cavity cue from a small map
                        (no rays, no occluders).
     tools/ao_bake_core.h  the same rule over any triangle list, on the host at build time (libc, a closed-object
                        inside test, a street wrap). Not for the console.
     sh4der_aomesh.h    the console bake over a triangle mesh with 12 golden-ratio rays and its own reach (1.6 m); its
                        bytes are its own: not merged with this header.

   No allocation, no globals. The caller owns the occluder arrays, the ray set and the output bytes.
   The maths goes through sh4der_core.h (SH4ZAM on the Dreamcast, plain C under SH4DER_CORE_NO_SH4ZAM).
   A ray set is built once per bake (sh4der_aobake_rays), not per vertex.

   Call order:
     1. At load: fill a sh4der_aobake_occl_t (pointers to your spheres and boxes; ground 1 at ground_y for a floor).
     2. sh4der_aobake_rays(dirs, n) into a dirs[n][3] array the caller owns.
     3. sh4der_aobake_bytes(...) over the vertices and normals, with an optional tick every 64 vertices (a heartbeat
        for a watchdog).
     4. Keep the bytes; multiply the ambient light by sh4der_aobake_open(byte, exaggerate) / 255-scaled value.

   Version: v0.5 (the bake code of demo/psm/ao/ao4_scene.h, moved here unchanged).
*/

/** \file    sh4der_aobake.h
    \brief   One ambient occlusion byte a vertex, by casting cosine-weighted rays at exact occluders.

    An occluder is a sphere, a box, or the ground plane y = ground_y. The rays are a fixed set (sh4der_aobake_rays),
    so the bytes of one scene are the same every bake. The byte is 255 open .. 0 closed. A start inside a sphere or a
    box is closed (0): a point sunk into a prop is dark, not a bright speck.

    The caller owns every array: the occluders (sph, box), the rays (dirs), the vertices (p, n) and the bytes (out).
    The caller also picks the defaults, through its own macros or through the call's arguments; the three
    SH4DER_AOBAKE_* macros here are only the start values.

    Each function is static inline. Maths: SH4ZAM on the Dreamcast, plain C under SH4DER_CORE_NO_SH4ZAM. */

#ifndef SH4DER_AOBAKE_H
#define SH4DER_AOBAKE_H

#include <stdint.h>
#include "sh4der_core.h"

/* Start values, not limits: every call takes its own rays, reach and push (README "Aobake tuning"). */
#define SH4DER_AOBAKE_RAYS  64      /* rays a point: more is smoother and slower (the bake cost is linear in it) */
#define SH4DER_AOBAKE_REACH 3.0f    /* metres a ray looks for an occluder; a hit beyond it is open */
#define SH4DER_AOBAKE_PUSH  0.02f   /* metres a ray starts off its surface, so it does not hit the surface it leaves */

/* A sphere: centre and radius. Same layout as float[4]; the caller's array is kept by the caller. */
typedef struct sh4der_aobake_sph { float c[3], r; } sh4der_aobake_sph_t;
/* A box: its low and high corners. Same layout as float[6]. */
typedef struct sh4der_aobake_box { float lo[3], hi[3]; } sh4der_aobake_box_t;

/* The exact occluders. Each array is the caller's; a count of 0 is none. ground: 1 puts a plane at y = ground_y, which
   a ray from above it hits (a ray from below it does not). */
typedef struct sh4der_aobake_occl {
    const sh4der_aobake_sph_t *sph; unsigned nsph;
    const sh4der_aobake_box_t *box; unsigned nbox;
    int ground; float ground_y;
} sh4der_aobake_occl_t;

/* The fixed ray set over the hemisphere of +y: cosine-weighted, Hammersley (the set of tools/ao_bake_core.h to 1e-6).
   dirs[k] is unit length with y >= 0 (ray k's direction in the local frame: x, then y the normal, then z). n rays. */
static inline void sh4der_aobake_rays(float (*dirs)[3], unsigned n) {
    const float inv_n = sh4der_div_(1.0f, (float)n);
    unsigned k;

    if (!dirs) return;
    for (k = 0; k < n; k++) {
        const float u1 = ((float)k + 0.5f) * inv_n;
        float u2 = 0.0f, f = 0.5f, sn, cs, rr;
        unsigned b = k;

        while (b) { if (b & 1u) u2 += f; f *= 0.5f; b >>= 1; }   /* the base-2 radical inverse */
        rr = sh4der_sqrt_(u1);
        sh4der_sincos_(6.2831853f * u2, &sn, &cs);
        dirs[k][0] = rr * cs; dirs[k][1] = sh4der_sqrt_(1.0f - u1); dirs[k][2] = rr * sn;
    }
}

/* The nearest occluder along o + t d (d unit), or `reach` when none within it. 0 when o is inside a sphere or a box. */
static inline float sh4der_aobake_ray(const sh4der_aobake_occl_t *occl, const float o[3], const float d[3], float reach) {
    float best = reach;
    unsigned i, k;

    if (occl->ground && d[1] < -1.0e-4f && o[1] > occl->ground_y) {   /* the ground plane */
        const float t = sh4der_div_(occl->ground_y - o[1], d[1]);

        if (t < best) best = t;
    }
    for (i = 0; i < occl->nsph; i++) {
        const sh4der_aobake_sph_t *sp = &occl->sph[i];
        const float oc[3] = { o[0] - sp->c[0], o[1] - sp->c[1], o[2] - sp->c[2] };
        const float b = sh4der_dot3_(oc, d), cc = sh4der_dot3_(oc, oc) - sp->r * sp->r;
        float disc, t;

        if (cc < 0.0f) return 0.0f;
        if (b >= 0.0f) continue;
        disc = b * b - cc;
        if (disc <= 0.0f) continue;
        t = -b - sh4der_sqrt_(disc);
        if (t >= 0.0f && t < best) best = t;
    }
    for (i = 0; i < occl->nbox; i++) {
        const sh4der_aobake_box_t *bx = &occl->box[i];
        float t0 = 0.0f, t1 = best;
        int inside = 1, miss = 0;

        for (k = 0; k < 3u; k++) {
            if (!(o[k] > bx->lo[k] + 1.0e-3f && o[k] < bx->hi[k] - 1.0e-3f)) inside = 0;   /* on a face is outside */
            if (d[k] > 1.0e-6f || d[k] < -1.0e-6f) {
                const float inv = sh4der_div_(1.0f, d[k]);
                float a = (bx->lo[k] - o[k]) * inv, b = (bx->hi[k] - o[k]) * inv;

                if (a > b) { const float sw = a; a = b; b = sw; }
                if (a > t0) t0 = a;
                if (b < t1) t1 = b;
            } else if (o[k] < bx->lo[k] || o[k] > bx->hi[k]) miss = 1;
        }
        if (inside) return 0.0f;
        if (!miss && t0 + 1.0e-4f < t1 && t0 < best) best = t0;   /* a ray that only touches a face does not hit */
    }
    return best;
}

/* The open share at a point with unit normal n: 1 open .. 0 closed. nrays 0, or reach not above 0, is open (1). */
static inline float sh4der_aobake_at(const sh4der_aobake_occl_t *occl, const float p[3], const float n[3],
                                     const float (*dirs)[3], unsigned nrays, float reach, float push) {
    const float ax[3] = { n[1] > 0.9f || n[1] < -0.9f ? 1.0f : 0.0f, n[1] > 0.9f || n[1] < -0.9f ? 0.0f : 1.0f, 0.0f };
    const float o[3] = { p[0] + n[0] * push, p[1] + n[1] * push, p[2] + n[2] * push };
    float t[3], b[3], occl_sum = 0.0f;
    unsigned k;

    if (!nrays || !(reach > 0.0f)) return 1.0f;
    {
        const float inv_reach = sh4der_div_(1.0f, reach), inv_n = sh4der_div_(1.0f, (float)nrays);

        sh4der_cross_(n, ax, t);
        sh4der_norm3_(t);
        sh4der_cross_(n, t, b);
        for (k = 0; k < nrays; k++) {
            const float d[3] = { t[0] * dirs[k][0] + n[0] * dirs[k][1] + b[0] * dirs[k][2],
                                 t[1] * dirs[k][0] + n[1] * dirs[k][1] + b[1] * dirs[k][2],
                                 t[2] * dirs[k][0] + n[2] * dirs[k][1] + b[2] * dirs[k][2] };
            const float hit = sh4der_aobake_ray(occl, o, d, reach);

            if (hit < reach) occl_sum += 1.0f - hit * inv_reach;
        }
        return 1.0f - occl_sum * inv_n;
    }
}

/* Every vertex's byte: out[i] = 255 open .. 0 closed. `tick` (may be NULL) is called every 64 vertices (at 0, 64, 128 ...),
   after the byte is written: a heartbeat for a watchdog. count 0 writes nothing. */
static inline void sh4der_aobake_bytes(const sh4der_aobake_occl_t *occl, const float (*p)[3], const float (*n)[3],
                                       unsigned count, const float (*dirs)[3], unsigned nrays, float reach, float push,
                                       uint8_t *out, void (*tick)(void)) {
    unsigned i;

    for (i = 0; i < count; i++) {
        out[i] = (uint8_t)(sh4der_sat_(sh4der_aobake_at(occl, p[i], n[i], dirs, nrays, reach, push)) * 255.0f + 0.5f);
        if (tick && (i & 63u) == 0u) tick();
    }
}

/* A stored byte as the light uses it: 0 .. 1 (open = 1), doubled under exaggerate: 1 - 2 (1 - a), not under 0. */
static inline float sh4der_aobake_open(unsigned byte, int exaggerate) {
    const float a = (float)byte * (1.0f / 255.0f);

    return exaggerate ? sh4der_sat_(2.0f * a - 1.0f) : a;
}

#endif /* SH4DER_AOBAKE_H */
