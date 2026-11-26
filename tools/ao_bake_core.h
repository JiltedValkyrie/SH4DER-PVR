/* SH4DER-PVR: ao_bake_core.h, baked ambient occlusion over any triangle list (a host build tool's core)

   tools/ao_bake_core.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   RECOMMENDED (a host build tool, not a console header; wiki/limits/header-recommended.md, wiki/effects/ao-bake.md)
   Start with: AOB_RAYS 64 rays a point, AOB_R 2.5 m reach (the room and the street; the street's road bake uses
               10 m at a scale of AOB_R / 10), AOB_PUSH 0.02 m, AOB_INSIDE 0.25
   Cost:       host seconds: not measured for this core (no demo README or log states one). The console cost is the
               baked bytes the program keeps (one byte a vertex, the client's); the load-time bake of demo/psm/ao
               (sh4der_aobake_bytes, include/sh4der_aobake.h, SH4-076: 254 ms and 393 ms on the console) is a
               different routine, not this one
   Status:     ADVISED; to prove: the room's bytes against room_ao_v to 2 (test/ao_bake_test.c, the room data built
               by demo/psm/ao/build.py --room-only)
   Do:         bake at build time only, on the host, with the rays of one set (keep the room's and the street's
               rules the same); fill an inside point from its neighbours
   Do not:     ship it in a console build (libc, malloc, and the time it takes); change AOB_R without rebaking the
               scene; treat a byte as physical light (it is a multiply on the ambient light)
   Maturity:   ALPHA (host tool; the two clients build; no measured console result of its own)

   The rules of demo/psm/ao/tools/room_bake.c, for a mesh that is not the room: a point casts AOB_RAYS cosine-weighted
   rays over its hemisphere (u1 = (k + 0.5) / 64, u2 the base-2 radical inverse of k; local (r cos phi, sqrt(1 - u1),
   r sin phi), r = sqrt(u1), phi = 2 pi u2; the frame t = norm(n x ax), b = n x t, ax = x when |n.y| > 0.9 else y),
   from AOB_PUSH metres off its surface, against every occluder whose bounding sphere comes within AOB_R. A hit at t
   takes 1 - t / R of a ray, on either side of a triangle; a hit under 1e-4 is not one. byte = 255 (1 - sum / rays).

   Inside a solid: when more than AOB_INSIDE of the rays meet first the BACK of a triangle flagged closed, the point is
   inside a closed object (sunk into a wall, a prop pushed into a building) and its byte would be black. The caller
   then fills it from its neighbours (aob_fill on a grid) or bakes it a little way into its own triangles (aob_at_in).

   A street that repeats every `period` metres along z: aob_wrap adds the occluders within AOB_R of either end again
   at +period and -period. Bake a vertex on the seam once and give both ends its byte.

   Plain C, no SH4ZAM: it runs on the host at build time, never on the console. Header only; test:
   test/ao_bake_test.c (against room_bake.c's bytes on the room). Clients: demo/pl4nar/tools/map_ao.c and the psm
   demo tools that bake a mesh (none yet). Its ray set is the one of include/sh4der_aobake.h (sh4der_aobake_rays,
   the console's) to 1e-6: test/aobake_test.c checks it. The two bakes stay separate (a host tool with libc; this
   core does not include the library). */

#ifndef AO_BAKE_CORE_H
#define AO_BAKE_CORE_H

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define AOB_RAYS   64
#define AOB_R      2.5f                     /* metres: the reach */
#define AOB_PUSH   0.02f                    /* metres off the surface a ray starts */
#define AOB_INSIDE 0.25f                    /* the share of back hits on closed triangles that says "inside" */

typedef struct aob {
    unsigned n;                             /* occluder triangles */
    const float (*ot)[3][3];                /* the triangles (caller's, or aob_wrap's) */
    const uint8_t *closed;                  /* 1: the triangle belongs to a closed object wound one way */
    float (*tc)[3], *tr;                    /* each triangle's middle and the radius round it */
    unsigned *near_t;
    float dirs[AOB_RAYS][3];
} aob_t;

static inline void aob_cross_(const float *a, const float *b, float *o) { o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0]; }
static inline float aob_dot_(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static inline void aob_norm_(float *a) { const float l = sqrtf(aob_dot_(a, a)); if (l > 1e-12f) { a[0] /= l; a[1] /= l; a[2] /= l; } }

/* The occluders: n triangles and a closed flag each (NULL: none closed). The arrays stay the caller's. 0 on no memory. */
static inline int aob_init(aob_t *b, const float (*ot)[3][3], const uint8_t *closed, unsigned n) {
    unsigned i, k;

    memset(b, 0, sizeof *b);
    b->n = n; b->ot = ot; b->closed = closed;
    b->tc = (float (*)[3])malloc((n ? n : 1u) * sizeof b->tc[0]);
    b->tr = (float *)malloc((n ? n : 1u) * sizeof b->tr[0]);
    b->near_t = (unsigned *)malloc((n ? n : 1u) * sizeof b->near_t[0]);
    if (!b->tc || !b->tr || !b->near_t) return 0;
    for (k = 0; k < AOB_RAYS; k++) {
        const float u1 = ((float)k + 0.5f) / (float)AOB_RAYS;
        float u2 = 0.0f, fr = 0.5f, rr, ph;
        unsigned v = k;

        while (v) { if (v & 1u) u2 += fr; fr *= 0.5f; v >>= 1; }
        rr = sqrtf(u1); ph = 6.2831853f * u2;
        b->dirs[k][0] = rr * cosf(ph); b->dirs[k][1] = sqrtf(1.0f - u1); b->dirs[k][2] = rr * sinf(ph);
    }
    for (i = 0; i < n; i++) {
        float r2 = 0.0f;

        for (k = 0; k < 3u; k++) b->tc[i][k] = (ot[i][0][k] + ot[i][1][k] + ot[i][2][k]) * (1.0f / 3.0f);
        for (k = 0; k < 3u; k++) {
            const float d[3] = { ot[i][k][0] - b->tc[i][0], ot[i][k][1] - b->tc[i][1], ot[i][k][2] - b->tc[i][2] };

            if (aob_dot_(d, d) > r2) r2 = aob_dot_(d, d);
        }
        b->tr[i] = sqrtf(r2);
    }
    return 1;
}

static inline void aob_free(aob_t *b) { free(b->tc); free(b->tr); free(b->near_t); memset(b, 0, sizeof *b); }

/* The distance along o + t d to triangle k, or 1e30; *back: the ray met its back. */
static inline float aob_ray_(const aob_t *b, const float o[3], const float d[3], unsigned k, int *back) {
    const float *a = b->ot[k][0], *p = b->ot[k][1], *c = b->ot[k][2];
    const float e1[3] = { p[0] - a[0], p[1] - a[1], p[2] - a[2] }, e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
    const float s[3] = { o[0] - a[0], o[1] - a[1], o[2] - a[2] };
    float h[3], q[3], det, u, v, t;

    aob_cross_(d, e2, h);
    det = aob_dot_(e1, h);
    if (det > -1e-9f && det < 1e-9f) return 1e30f;
    *back = det < 0.0f;
    det = 1.0f / det;
    u = aob_dot_(s, h) * det;
    if (u < 0.0f || u > 1.0f) return 1e30f;
    aob_cross_(s, e1, q);
    v = aob_dot_(d, q) * det;
    if (v < 0.0f || u + v > 1.0f) return 1e30f;
    t = aob_dot_(e2, q) * det;
    return t > 1e-4f ? t : 1e30f;
}

/* The open share 0 .. 1 at p about the unit normal n; *inside as above. */
static inline float aob_at(aob_t *b, const float p[3], const float n[3], int *inside) {
    const float ax[3] = { fabsf(n[1]) > 0.9f ? 1.0f : 0.0f, fabsf(n[1]) > 0.9f ? 0.0f : 1.0f, 0.0f };
    const float o[3] = { p[0] + n[0] * AOB_PUSH, p[1] + n[1] * AOB_PUSH, p[2] + n[2] * AOB_PUSH };
    float t[3], bt[3], occl = 0.0f;
    unsigned k, i, nn = 0, in = 0;

    for (i = 0; i < b->n; i++) {
        const float d[3] = { b->tc[i][0] - o[0], b->tc[i][1] - o[1], b->tc[i][2] - o[2] };

        if (aob_dot_(d, d) < (AOB_R + b->tr[i]) * (AOB_R + b->tr[i])) b->near_t[nn++] = i;
    }
    aob_cross_(n, ax, t); aob_norm_(t);
    aob_cross_(n, t, bt);
    for (k = 0; k < AOB_RAYS; k++) {
        const float *q = b->dirs[k];
        const float d[3] = { t[0] * q[0] + n[0] * q[1] + bt[0] * q[2], t[1] * q[0] + n[1] * q[1] + bt[1] * q[2], t[2] * q[0] + n[2] * q[1] + bt[2] * q[2] };
        float best = AOB_R;
        int best_back = 0, back = 0;

        for (i = 0; i < nn; i++) {
            const float hit = aob_ray_(b, o, d, b->near_t[i], &back);

            if (hit < best) { best = hit; best_back = back && b->closed && b->closed[b->near_t[i]]; }
        }
        if (best < AOB_R) occl += 1.0f - best / AOB_R;
        in += (unsigned)best_back;
    }
    *inside = (float)in > AOB_INSIDE * (float)AOB_RAYS;
    return 1.0f - occl / (float)AOB_RAYS;
}

static inline uint8_t aob_byte(float a) { return (uint8_t)((a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a) * 255.0f + 0.5f); }

/* A vertex of an object found inside a solid: the mean of the points 0.3 of the way to the middles of the triangles
   it belongs to (tri: m index triples into v) that are outside, about its own normal. Returns 1 and *a if any was. */
static inline int aob_at_in(aob_t *b, unsigned vi, const float (*v)[3], const float n[3], const uint16_t (*tri)[3], unsigned m, float *a) {
    float acc = 0.0f;
    unsigned t, cnt = 0, c;
    int inside;

    for (t = 0; t < m; t++) {
        float q[3], x;

        if (tri[t][0] != vi && tri[t][1] != vi && tri[t][2] != vi) continue;
        for (c = 0; c < 3u; c++) q[c] = v[vi][c] + 0.3f * ((v[tri[t][0]][c] + v[tri[t][1]][c] + v[tri[t][2]][c]) * (1.0f / 3.0f) - v[vi][c]);
        x = aob_at(b, q, n, &inside);
        if (!inside) { acc += x; cnt++; }
    }
    if (!cnt) return 0;
    *a = acc / (float)cnt;
    return 1;
}

/* A w x h grid of bytes (every `stride`-th) with a flag a point: a flagged point takes the mean of its unflagged
   neighbours (the 8 round it), pass after pass. Returns how many were flagged. */
static inline unsigned aob_fill(uint8_t *v, uint8_t *bad, unsigned w, unsigned h, unsigned stride) {
    unsigned n = 0, left, x, y;
    uint8_t *next = (uint8_t *)malloc((size_t)w * h);

    if (!next) return 0;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) n += bad[y * w + x];
    for (left = n; left; ) {
        unsigned done = 0;

        memcpy(next, bad, (size_t)w * h);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                unsigned sum = 0, k = 0;
                int dx, dy;

                if (!bad[y * w + x]) continue;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++) {
                        const int xx = (int)x + dx, yy = (int)y + dy;

                        if (xx < 0 || yy < 0 || xx >= (int)w || yy >= (int)h || bad[(unsigned)yy * w + (unsigned)xx]) continue;
                        sum += v[((unsigned)yy * w + (unsigned)xx) * stride]; k++;
                    }
                if (k) { v[(y * w + x) * stride] = (uint8_t)((sum + k / 2u) / k); next[y * w + x] = 0; done++; }
            }
        memcpy(bad, next, (size_t)w * h);
        if (!done) break;
        left -= done;
    }
    free(next);
    return n;
}

/* A street repeating every `period` along z over [0, period): the n triangles, then each that comes within AOB_R of
   an end again on the far side (z + period near z 0, z - period near the top). Writes up to `cap` triangles to out
   and their flags to out_closed (closed may be NULL); returns how many. */
static inline unsigned aob_wrap(const float (*ot)[3][3], const uint8_t *closed, unsigned n, float period, float (*out)[3][3], uint8_t *out_closed, unsigned cap) {
    unsigned i, m = 0, s, k;

    for (i = 0; i < n && m < cap; i++, m++) { memcpy(out[m], ot[i], sizeof out[0]); out_closed[m] = closed ? closed[i] : 0u; }
    for (s = 0; s < 2u; s++)
        for (i = 0; i < n && m < cap; i++) {
            float lo = 1e30f, hi = -1e30f;
            const float shift = s ? -period : period;

            for (k = 0; k < 3u; k++) { if (ot[i][k][2] < lo) lo = ot[i][k][2]; if (ot[i][k][2] > hi) hi = ot[i][k][2]; }
            if (s ? lo < period - AOB_R : hi > AOB_R) continue;   /* within R of the end it is copied across */
            memcpy(out[m], ot[i], sizeof out[0]);
            for (k = 0; k < 3u; k++) out[m][k][2] += shift;
            out_closed[m] = closed ? closed[i] : 0u;
            m++;
        }
    return m;
}

#endif
