/* SH4DER-PVR: a ray against a triangle, one kernel for the bakes and the bench: the numerators with the sign taken off, the
   reciprocal form for a distance, the hit test with no divide, and a form prepared for one direction

   include/sh4der_raytri.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).

   RECOMMENDED (no console run of this kernel: its time is NOT MEASURED; wiki/effects/raytri.md)
   Cost:       one cross product and three dot products for the numerators, then the edge tests; a reciprocal only for a
               triangle that passed them (sh4der_raytri_t). NOT MEASURED on the SH-4. RAM: nothing kept (the numerators are
               on the stack); the prepared form is 64 bytes a triangle a direction.
   Start with: the caller's own tests and a reach; nothing to tune (no SH4DER_ macro here: the band 1e-9, the reach
               test 1e-4 are the constants the callers have always used)
   Status:     PROTOTYPE v0.1: host tested (test/raytri_test.c, plain C and SH4ZAM's software back end); the console NOT RUN
   Do:         take a ray's numerators once per triangle, then the distance or the hit from them
   Do not:     call it watertight (see Limits); pass a NULL recips counter to sh4der_raytri_t; use a NaN direction as a test
               of anything (it misses, by the arithmetic below, and the numerators are returned as numbers)
   Maturity:   PROTOTYPE v0.1

   Where it is used:
     include/sh4der_aomesh.h       sh4der_aomesh_tri_ (the mesh bake): num, then t (the reach and the recips counter)
     demo/psm/bench/ray_kernels.h  ray_tri (num, then the hit test with the bench's own |det| rule) and ray_tri_sun (the
                                   prepared form: dir_prep, dir_hit); the bench's _ref functions are untouched
   Not used by: tools/ao_bake_core.h (the host tool scales u, v and t by the reciprocal first, a different arithmetic; left
                alone, see wiki/effects/raytri.md).

   The arithmetic (one set of numerators, the sign taken off once):
     det  = e1 . (d x e2)          the band: refused when -1e-9 < det < 1e-9 (a NaN is not refused by the band)
     neg  = !(det > 0)             a negative det (and a NaN, as the else branch of the old test) takes the sign off:
                                   det, un, vn, wn are each negated ONCE, by a negate, never a multiply by a sign
     un   = s . (d x e2)           s = o - a; refused when un < 0 or un > det
     vn   = d . (s x e1)           refused when vn < 0 or un + vn > det (un + vn is formed once, as written)
     wn   = e2 . (s x e1)          the hit's numerator; t = wn / det
   Negating an IEEE float is exact and the sums are symmetric under the sign, so the flipped frame gives the bits of the old
   two-sided tests (aomesh's before this header: the same compares on the unflipped numbers).
   Two band rules, both kept on purpose for the bytes (do not make one match the other): sh4der_raytri_num refuses
   -1e-9 < det < 1e-9, so |det| == 1e-9 passes; sh4der_raytri_dir_prep and the bench's ray_tri take |det| > 1e-9, so
   |det| == 1e-9 is refused.

   Limits:
     Not watertight. Each triangle computes from its own a, e1, e2: a ray through a shared edge is taken by at least one of
     the two triangles on the test's sampled diagonal, but no proof covers every ray. A watertight test needs the edge
     functions with one shared ordering of the vertices, which this is not (a later option, NOT in this change).
     No far limit in sh4der_raytri_hit: it answers "is the hit beyond eps", for any distance. sh4der_raytri_t has a reach.
     A later option, NOT in this change: a reach test with no reciprocal (wn x sign < reach x |det|) would save the
     reciprocal, but compares in a different arithmetic and can move aomesh's bytes at the limit (the pin would warn).
     The reciprocal is sh4der_recip_pos_, which takes x > 0: a NaN det passes the band and reaches it (the result is a miss).
     The dir form takes 1 / det by sh4der_recip_pos_ (once a triangle a sun), like every reciprocal here. It was the
     bench's plain division until 2026-11-26; the bench's bytes were moved on purpose for that rule (see dir_prep).
     recips: sh4der_raytri_t counts into *recips and always dereferences it: the pointer must not be NULL. The callers that
     have no counter pass a local one. No test is added to the hot path to check it.
   */

#ifndef SH4DER_RAYTRI_H
#define SH4DER_RAYTRI_H

#include <stdint.h>
#include "sh4der_core.h"

/* The numerators of one ray against one triangle, in the frame where det > 0: det, un, vn and wn are the values with the
   sign of det taken off (a negative det negates each of them once). */
typedef struct sh4der_raytri_num {
    float det, un, vn, wn;
} sh4der_raytri_num_t;

/* The numerators for the ray from o along d against the triangle a, a + e1, a + e2 (e1 and e2 are the edges from a).
   Returns 0 when the band or the u or u + v tests refuse it (n is then not valid), 1 otherwise (n is valid: the hit still
   needs sh4der_raytri_t or sh4der_raytri_hit). A NaN direction or origin passes the band and the tests as numbers (a NaN
   compares false), so it returns 1 with NaN numerators: its hit and its distance are then misses. */
static inline int sh4der_raytri_num(const float o[3], const float d[3], const float a[3], const float e1[3], const float e2[3],
                                    sh4der_raytri_num_t *n) {
    float h[3], s[3], q[3], det, un, vn, wn;
    int neg;

    sh4der_cross_(d, e2, h);
    det = sh4der_dot3_(e1, h);
    if (det > -1e-9f && det < 1e-9f) return 0;
    neg = !(det > 0.0f);
    s[0] = o[0] - a[0]; s[1] = o[1] - a[1]; s[2] = o[2] - a[2];
    un = sh4der_dot3_(s, h);
    if (neg) { det = -det; un = -un; }
    if (un < 0.0f || un > det) return 0;
    sh4der_cross_(s, e1, q);
    vn = sh4der_dot3_(d, q);
    if (neg) vn = -vn;
    if (vn < 0.0f || un + vn > det) return 0;
    wn = sh4der_dot3_(e2, q);
    if (neg) wn = -wn;
    n->det = det; n->un = un; n->vn = vn; n->wn = wn;
    return 1;
}

/* The distance t = wn / det of a valid n, in the reciprocal form: t when t > 1e-4 and t < reach, else -1. Counts the
   reciprocal in *recips (the pointer must not be NULL). Takes the reciprocal of det > 0 only (the band made det >= 1e-9 or
   a NaN, and a NaN gives a miss). */
static inline float sh4der_raytri_t(const sh4der_raytri_num_t *n, float reach, uint32_t *recips) {
    const float inv = sh4der_recip_pos_(n->det);
    const float t = n->wn * inv;

    (*recips)++;
    return t > 1e-4f && t < reach ? t : -1.0f;
}

/* The hit test with no divide: 1 when the ray meets the triangle further than eps along it, both faces, for any distance
   (no far limit here: a hit a kilometre off counts). Tested as wn > eps x det, the same as wn / det > eps for det > 0 in exact
   arithmetic; in floats the two can differ within a few ulp of eps (a multiply rounds unlike a divide at the limit). */
static inline unsigned sh4der_raytri_hit(const sh4der_raytri_num_t *n, float eps) {
    return (unsigned)(n->wn > eps * n->det);
}

/* A triangle prepared for one direction d (every ray in a loop shares d): h = (d x e2) / det, e2q = e2 / det, dq = d / det,
   det = e1 . (d x e2). ok = 0 for a triangle edge-on to d (no test). 64 bytes: two of the SH-4's 32-byte cache lines only when
   the array is 32-byte aligned (the bench's arrays are plain static arrays, alignment not set). The fields are the bench's
   layout (ray_tri_sun_t in demo/psm/bench/ray_kernels.h is this struct). */
typedef struct sh4der_raytri_dir {
    float a[3], e1[3], h[3], e2q[3];
    float dq[3];
    float ok;
} sh4der_raytri_dir_t;

/* Prepares p for the direction d. 1 / det is sh4der_recip_pos_ with the sign taken off (FSRRA through SH4ZAM; a plain
   1.0f / det only under SH4DER_CORE_NO_SH4ZAM), once a triangle a direction. It was the bench's plain divide until
   2026-11-26: the bench's bytes were moved on purpose, to follow the rule that every reciprocal goes through SH4ZAM
   (the difference is in the README section and the wiki page; the cost either way NOT MEASURED). */
static inline void sh4der_raytri_dir_prep(sh4der_raytri_dir_t *p, const float a[3], const float e1[3], const float e2[3],
                                          const float d[3]) {
    float h[3], det, ad, inv;
    unsigned k;

    sh4der_cross_(d, e2, h);
    det = sh4der_dot3_(e1, h);
    ad = det < 0.0f ? -det : det;
    inv = ad > 1e-9f ? (det > 0.0f ? sh4der_recip_pos_(det) : -sh4der_recip_pos_(-det)) : 0.0f;
    for (k = 0; k < 3u; k++) { p->a[k] = a[k]; p->e1[k] = e1[k]; p->e2q[k] = e2[k] * inv; p->dq[k] = d[k] * inv; p->h[k] = h[k] * inv; }
    p->ok = ad > 1e-9f ? 1.0f : 0.0f;
}

/* The hit of a prepared triangle from o: 1 when the ray meets it with its distance (in units of the ray's d) above eps.
   Tests u, then v, then u + v, each in the prepared units (u, v in 0 .. 1). No far limit, as the general test. */
static inline unsigned sh4der_raytri_dir_hit(const sh4der_raytri_dir_t *p, const float o[3], float eps) {
    const float s[3] = { o[0] - p->a[0], o[1] - p->a[1], o[2] - p->a[2] };
    const float u = sh4der_dot3_(s, p->h);
    float q[3], v;

    if (p->ok == 0.0f || u < 0.0f || u > 1.0f) return 0u;
    sh4der_cross_(s, p->e1, q);
    v = sh4der_dot3_(p->dq, q);
    if (v < 0.0f || u + v > 1.0f) return 0u;
    return (unsigned)(sh4der_dot3_(p->e2q, q) > eps);
}

#endif /* SH4DER_RAYTRI_H */
