/* SH4DER-PVR: host test of the ray-triangle kernel (include/sh4der_raytri.h): edge cases, the shared diagonal on both
   windings, two random sets and a set at the limits (10,000 rays each). Plain C here; `make test-shz` runs it on SH4ZAM's software back end.

   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   What it checks:
     1 edge cases: a parallel ray is refused; a triangle and its reverse (det of both signs) take the same ray from above and
       from below; a ray through a triangle's edge at u = 0 and at u + v = 1 hits (and one a little past it misses); a
       distance is the reciprocal form's and counts the reciprocal; a distance under 1e-4 and beyond the reach are -1; a NaN
       direction or origin misses.
     2 the shared diagonal of a quad (two triangles, a ray down or up through every sample of it): at least one triangle
       takes it, on both windings. This is a sampled check, not a proof: the kernel is not watertight (sh4der_raytri.h, Limits).
     3 the random set: the hit with no divide (eps 1e-4) and the distance's reciprocal form agree; a disagreement is only
       where the distance is within 1e-5 (relative) of eps. The hit agrees with a double-precision test except within 1e-4 of
       an edge (the number that disagree is printed).
     4 the prepared direction form against the general numerators, with a tolerance (not bytes): the largest difference is
       printed; the hit disagreements are only within 1e-4 of an edge.
     5 the limit set: rays aimed within 1e-6 of each edge and within 1e-5 of eps, and numerators within 8 ulp of eps x det,
       so every disagreement branch runs: the same rules as 3 and 4 (hit vs distance within 1e-6 relative of eps), and each
       kind of disagreement must occur (the counts are printed).

   Build (from the repository's root):
     cc -std=c99 -O2 -Wall -Wextra -DSH4DER_CORE_NO_SH4ZAM -Iinclude test/raytri_test.c -lm -o build/raytri_test
*/

#include <float.h>
#include <math.h>
#include <stdint.h>
#include <stdio.h>

#include "sh4der_raytri.h"

static unsigned checks, fails;
#define CHECK(c, ...) do { checks++; if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t rs = 987654321u;
static float frand(float lo, float hi) {
    rs = rs * 1664525u + 1013904223u;
    return lo + (hi - lo) * (float)((rs >> 8) & 0xFFFFFFu) * (1.0f / 16777216.0f);
}
static void frand_unit(float d[3]) {
    float n = 0.0f;
    unsigned k;

    for (k = 0; k < 3u; k++) { d[k] = frand(-1.0f, 1.0f); n += d[k] * d[k]; }
    n = 1.0f / sqrtf(n);
    for (k = 0; k < 3u; k++) d[k] *= n;
}

/* Half the rays of the random sets start beside the triangle, toward a point near it (u, v a little outside 0 .. 1 on
   either side), so the set has hits and near misses; the other half are fully random. */
static void frand_ray(float o[3], float dir[3], const float a[3], const float e1[3], const float e2[3], unsigned near) {
    unsigned k;

    frand_unit(dir);
    if (near) {
        const float u = frand(-0.2f, 1.2f), v = frand(-0.2f, 1.2f), t = frand(0.05f, 3.0f);

        for (k = 0; k < 3u; k++) o[k] = a[k] + u * e1[k] + v * e2[k] - dir[k] * t;
    } else {
        for (k = 0; k < 3u; k++) o[k] = frand(-3.0f, 3.0f);
    }
}

/* The general kernel: 1 when the ray from o along d meets the triangle a, a + e1, a + e2 beyond eps (both faces). */
static unsigned hit_gen(const float o[3], const float d[3], const float a[3], const float e1[3], const float e2[3], float eps) {
    sh4der_raytri_num_t n;

    if (!sh4der_raytri_num(o, d, a, e1, e2, &n)) return 0u;
    return sh4der_raytri_hit(&n, eps);
}

/* A double-precision test of the same triangle: the margin is how far (in the test's own units) the ray is from an edge. */
static int hit_double(const float o[3], const float d[3], const float a[3], const float e1[3], const float e2[3], double *margin) {
    const double E1[3] = { e1[0], e1[1], e1[2] }, E2[3] = { e2[0], e2[1], e2[2] }, D[3] = { d[0], d[1], d[2] };
    const double S[3] = { (double)o[0] - a[0], (double)o[1] - a[1], (double)o[2] - a[2] };
    const double H[3] = { D[1] * E2[2] - D[2] * E2[1], D[2] * E2[0] - D[0] * E2[2], D[0] * E2[1] - D[1] * E2[0] };
    const double Q[3] = { S[1] * E1[2] - S[2] * E1[1], S[2] * E1[0] - S[0] * E1[2], S[0] * E1[1] - S[1] * E1[0] };
    const double det = E1[0] * H[0] + E1[1] * H[1] + E1[2] * H[2];
    double u, v, w, m;

    *margin = 0.0;
    if (fabs(det) < 1e-9) return 0;
    u = (S[0] * H[0] + S[1] * H[1] + S[2] * H[2]) / det;
    v = (D[0] * Q[0] + D[1] * Q[1] + D[2] * Q[2]) / det;
    w = (E2[0] * Q[0] + E2[1] * Q[1] + E2[2] * Q[2]) / det;
    m = fabs(u);
    if (fabs(v) < m) m = fabs(v);
    if (fabs(1.0 - u - v) < m) m = fabs(1.0 - u - v);
    if (fabs(w - 1e-4) < m) m = fabs(w - 1e-4);
    *margin = m;
    return u >= 0.0 && v >= 0.0 && u + v <= 1.0 && w > 1e-4;
}

static void t_edges(void) {
    static const float a[3] = { 0.0f, 0.0f, 0.0f }, ex[3] = { 1.0f, 0.0f, 0.0f }, ez[3] = { 0.0f, 0.0f, 1.0f };
    static const float down[3] = { 0.0f, -1.0f, 0.0f }, up[3] = { 0.0f, 1.0f, 0.0f }, along[3] = { 1.0f, 0.0f, 0.0f };
    static const float nan_d[3] = { NAN, 1.0f, 0.0f }, nan_o[3] = { NAN, 1.0f, 0.5f };
    const float o_top[3] = { 0.25f, 1.0f, 0.25f }, o_bot[3] = { 0.25f, -1.0f, 0.25f };
    const float o_u0[3] = { 0.0f, 1.0f, 0.5f }, o_uv1[3] = { 0.5f, 1.0f, 0.5f }, o_past[3] = { 0.5f, 1.0f, 0.501f };
    const float o_near[3] = { 0.0f, 1e-5f, 0.5f };
    const float o_par[3] = { 0.3f, 1.0f, 0.3f };
    sh4der_raytri_num_t nT, nR, n0;
    sh4der_raytri_dir_t p;
    uint32_t rc = 0;
    float t;

    /* a parallel ray: its det is exactly 0, refused by the band; the prepared form has no test for it either */
    CHECK(!sh4der_raytri_num(o_par, along, a, ex, ez, &n0), "a parallel ray is not refused");
    sh4der_raytri_dir_prep(&p, a, ex, ez, along);
    CHECK(sh4der_raytri_dir_hit(&p, o_par, 1e-4f) == 0u && p.ok == 0.0f, "a parallel ray hits the prepared form");

    /* a triangle and its reverse: opposite signs of det, both faces, from above and from below */
    {
        const float rev_e1[3] = { ez[0], ez[1], ez[2] }, rev_e2[3] = { ex[0], ex[1], ex[2] };
        unsigned hT, hR, bT, bR;

        CHECK(sh4der_raytri_num(o_top, down, a, ex, ez, &nT) == 1 && sh4der_raytri_num(o_top, down, a, rev_e1, rev_e2, &nR) == 1,
              "a ray from above is refused by a triangle or its reverse");
        CHECK(nT.det == nR.det && nT.det > 0.0f, "det of a triangle and its reverse differ in size (%g, %g)", nT.det, nR.det);
        hT = hit_gen(o_top, down, a, ex, ez, 1e-4f); hR = hit_gen(o_top, down, a, rev_e1, rev_e2, 1e-4f);
        bT = hit_gen(o_bot, up, a, ex, ez, 1e-4f); bR = hit_gen(o_bot, up, a, rev_e1, rev_e2, 1e-4f);
        CHECK(hT == 1u && hR == 1u && bT == 1u && bR == 1u, "a triangle or its reverse misses a ray (%u %u %u %u)", hT, hR, bT, bR);
    }

    /* u = 0 (the edge a to a + e2) hits; u + v = 1 (the edge a + e1 to a + e2) hits; a little past it misses */
    CHECK(hit_gen(o_u0, down, a, ex, ez, 1e-4f) == 1u, "a ray through the edge u = 0 misses");
    CHECK(hit_gen(o_uv1, down, a, ex, ez, 1e-4f) == 1u, "a ray through the edge u + v = 1 misses");
    CHECK(hit_gen(o_past, down, a, ex, ez, 1e-4f) == 0u, "a ray past the edge u + v = 1 hits");

    /* the distance: t = 1 exactly for the u = 0 ray (one reciprocal, counted); a reach under it is -1 */
    CHECK(sh4der_raytri_num(o_u0, down, a, ex, ez, &n0) == 1, "the u = 0 ray is refused");
    t = sh4der_raytri_t(&n0, 2.0f, &rc);
    CHECK(t == 1.0f && rc == 1u, "the u = 0 ray: t %g, reciprocals %u (want 1, 1)", t, rc);
    t = sh4der_raytri_t(&n0, 0.5f, &rc);
    CHECK(t == -1.0f && rc == 2u, "a reach of 0.5 m: t %g, reciprocals %u (want -1, 2)", t, rc);
    CHECK(sh4der_raytri_hit(&n0, 1e-4f) == 1u && sh4der_raytri_hit(&n0, 2.0f) == 0u, "the hit test ignores its eps");

    /* a distance under 1e-4 is -1 (the ray starts on the surface); the hit test follows eps, not the distance */
    CHECK(sh4der_raytri_num(o_near, down, a, ex, ez, &n0) == 1, "the near ray is refused");
    t = sh4der_raytri_t(&n0, 2.0f, &rc);
    CHECK(t == -1.0f, "a distance of 1e-5 m is not -1 (%g)", t);
    CHECK(sh4der_raytri_hit(&n0, 1e-6f) == 1u && sh4der_raytri_hit(&n0, 1e-4f) == 0u, "the hit test does not follow eps");

    /* a NaN direction or origin: the numbers are NaN, so the hit and the distance are misses */
    {
        sh4der_raytri_num_t nn;
        int v1 = sh4der_raytri_num(o_u0, nan_d, a, ex, ez, &nn);
        int v2 = sh4der_raytri_num(nan_o, down, a, ex, ez, &nn);

        CHECK(hit_gen(o_u0, nan_d, a, ex, ez, 1e-4f) == 0u && hit_gen(nan_o, down, a, ex, ez, 1e-4f) == 0u, "a NaN ray hits");
        sh4der_raytri_dir_prep(&p, a, ex, ez, nan_d);
        CHECK(sh4der_raytri_dir_hit(&p, o_u0, 1e-4f) == 0u, "a NaN direction hits the prepared form");
        if (v1) { t = sh4der_raytri_t(&nn, 2.0f, &rc); CHECK(t == -1.0f, "a NaN direction has a distance (%g)", t); }
        if (v2) { t = sh4der_raytri_t(&nn, 2.0f, &rc); CHECK(t == -1.0f, "a NaN origin has a distance (%g)", t); }
    }
}

/* The shared diagonal of the quad a, b = (1,0,0), c = (1,0,1), d = (0,0,1): the triangles (a, b, c) and (a, c, d), and the
   same two reversed. A ray down, and one up, through each sample (s, 0, s) of the diagonal: at least one triangle takes it. */
static void t_diagonal(void) {
    static const float a[3] = { 0.0f, 0.0f, 0.0f }, b[3] = { 1.0f, 0.0f, 0.0f }, c[3] = { 1.0f, 0.0f, 1.0f }, d[3] = { 0.0f, 0.0f, 1.0f };
    static const float down[3] = { 0.0f, -1.0f, 0.0f }, up[3] = { 0.0f, 1.0f, 0.0f };
    static const unsigned N = 4096u;
    unsigned k, win, miss = 0u, both = 0u, samples = 0u;

    for (win = 0; win < 2u; win++)
        for (k = 0; k <= N; k++) {
            const float s = (float)k / (float)N;
            const float o_t[3] = { s, 1.0f, s }, o_b[3] = { s, -1.0f, s };
            const float *dn = win ? up : down, *ray_o = win ? o_b : o_t;
            unsigned h;

            /* winding 0: (a, b, c) and (a, c, d); winding 1: both reversed */
            h = win == 0u ? hit_gen(ray_o, dn, a, b, c, 1e-4f) + hit_gen(ray_o, dn, a, c, d, 1e-4f)
                          : hit_gen(ray_o, dn, a, c, b, 1e-4f) + hit_gen(ray_o, dn, a, d, c, 1e-4f);
            samples++;
            if (h == 0u) miss++;
            if (h >= 2u) both++;
        }
    CHECK(miss == 0u, "the shared diagonal: %u of %u rays are taken by neither triangle", miss, samples);
    printf("raytri diagonal: %u rays along the shared diagonal of a quad, both windings, from above and below: %u missed by both, %u taken by both\n",
           samples, miss, both);
}

/* The random set: the hit (no divide) and the reciprocal form agree; the double test agrees off the edges. */
static void t_random(void) {
    unsigned i, valid = 0, hits = 0, dis = 0, rcount = 0, dbl_dis = 0, dbl_hits = 0, dbl_edge = 0;
    double max_rel = 0.0, max_margin = 0.0;

    for (i = 0; i < 10000u; i++) {
        float a[3], e1[3], e2[3], o[3], dir[3];
        sh4der_raytri_num_t n;
        uint32_t rc = 0;
        unsigned hit, k;
        double margin, rel;
        float tt;
        int dv;

        for (k = 0; k < 3u; k++) { a[k] = frand(-2.0f, 2.0f); e1[k] = frand(-2.0f, 2.0f); e2[k] = frand(-2.0f, 2.0f); }
        frand_ray(o, dir, a, e1, e2, i & 1u);
        dv = sh4der_raytri_num(o, dir, a, e1, e2, &n);
        if (dv) {
            valid++;
            hit = sh4der_raytri_hit(&n, 1e-4f);
            tt = sh4der_raytri_t(&n, 1e30f, &rc);
            rcount += rc == 1u;
            hits += hit;
            if (hit != (unsigned)(tt > 0.0f)) {
                const double r = (double)n.wn / (double)n.det;

                dis++;
                rel = fabs(r - 1e-4) / 1e-4;
                if (rel > max_rel) max_rel = rel;
            }
        }
        {
            const unsigned g = hit_gen(o, dir, a, e1, e2, 1e-4f);
            const int dd = hit_double(o, dir, a, e1, e2, &margin);

            dbl_hits += g;
            if (g != (unsigned)dd) {
                dbl_dis++;
                if (margin > max_margin) max_margin = margin;
                if (margin < 1e-4) dbl_edge++;
            }
        }
    }
    CHECK(rcount == valid, "the random set: %u distances counted, %u valid rays", rcount, valid);
    CHECK(hits > 0u && hits < valid, "the random set: %u hits of %u valid rays (no spread)", hits, valid);
    CHECK(max_rel <= 1e-5, "the random set: a hit and a distance disagree off the limit (relative %g)", max_rel);
    CHECK(dbl_dis == dbl_edge, "the random set: %u hits differ from the double test, %u of them not within 1e-4 of an edge", dbl_dis, dbl_dis - dbl_edge);
    printf("raytri random: %u rays, %u valid, %u hits; hit vs distance: %u disagree (largest relative %.3g of eps); hit vs double test: %u hits, %u differ (%u within 1e-4 of an edge, largest margin %.3g)\n",
           10000u, valid, hits, dis, max_rel, dbl_hits, dbl_dis, dbl_edge, max_margin);
}

/* The prepared form against the general numerators: u, v and w by the tolerance, the hits by the edge margin. */
static void t_dir(void) {
    unsigned i, hits = 0, dis = 0, dis_edge = 0;
    double max_abs = 0.0, max_scaled = 0.0, max_edge = 0.0;

    for (i = 0; i < 10000u; i++) {
        float a[3], e1[3], e2[3], o[3], dir[3], s[3], h[3], q[3], det, un, vn, wn, ug, vg, wg, ud, vd, wd;
        sh4der_raytri_dir_t p;
        unsigned k;
        float sc;
        unsigned gh, dh;

        for (k = 0; k < 3u; k++) { a[k] = frand(-2.0f, 2.0f); e1[k] = frand(-2.0f, 2.0f); e2[k] = frand(-2.0f, 2.0f); }
        frand_ray(o, dir, a, e1, e2, i & 1u);
        for (k = 0; k < 3u; k++) s[k] = o[k] - a[k];
        sh4der_cross_(dir, e2, h);
        det = sh4der_dot3_(e1, h);
        if (!(fabsf(det) > 1e-9f)) continue;
        sh4der_cross_(s, e1, q);
        un = sh4der_dot3_(s, h); vn = sh4der_dot3_(dir, q); wn = sh4der_dot3_(e2, q);
        ug = un / det; vg = vn / det; wg = wn / det;
        sh4der_raytri_dir_prep(&p, a, e1, e2, dir);
        ud = sh4der_dot3_(s, p.h); vd = sh4der_dot3_(p.dq, q); wd = sh4der_dot3_(p.e2q, q);
        {   /* the scale of each ratio's rounding: the sum of the term magnitudes over |det| (a cancelled dot is not small) */
            const float dd[3] = { ug - ud, vg - vd, wg - wd };
            const float fdet = fabsf(det);
            const float bu = (fabsf(s[0] * h[0]) + fabsf(s[1] * h[1]) + fabsf(s[2] * h[2])) / fdet;
            const float bv = (fabsf(dir[0] * q[0]) + fabsf(dir[1] * q[1]) + fabsf(dir[2] * q[2])) / fdet;
            const float bw = (fabsf(e2[0] * q[0]) + fabsf(e2[1] * q[1]) + fabsf(e2[2] * q[2])) / fdet;
            const float bnd[3] = { bu, bv, bw };

            for (k = 0; k < 3u; k++) {
                const double ad = fabs((double)dd[k]), mag = fabs((double)bnd[k]) > 1.0 ? fabs((double)bnd[k]) : 1.0;

                if (ad > max_abs) max_abs = ad;
                if (ad / mag > max_scaled) max_scaled = ad / mag;
            }
        }
        sc = 1e-4f;
        gh = ug >= 0.0f && ug <= 1.0f && vg >= 0.0f && ug + vg <= 1.0f && wg > sc;
        dh = sh4der_raytri_dir_hit(&p, o, sc);
        hits += gh;
        if (gh != dh) {
            double edge = fabs((double)ug), mm;
            mm = fabs(1.0 - (double)ug); if (mm < edge) edge = mm;
            mm = fabs((double)vg); if (mm < edge) edge = mm;
            mm = fabs(1.0 - (double)ug - (double)vg); if (mm < edge) edge = mm;
            mm = fabs((double)wg - 1e-4); if (mm < edge) edge = mm;
            dis++;
            if (edge > max_edge) max_edge = edge;
            if (edge < 1e-4) dis_edge++;
        }
    }
    CHECK(max_scaled <= 1e-5, "the prepared form differs from the general by %g (scaled by the term magnitudes)", max_scaled);
    CHECK(dis == dis_edge, "the prepared form and the general test disagree on %u rays, %u not within 1e-4 of an edge", dis, dis - dis_edge);
    printf("raytri prepared vs general: %u hits, %u disagree (%u within 1e-4 of an edge, largest margin %.3g); largest |difference| %.3g, largest scaled by the term magnitudes %.3g (tolerance 1e-5)\n",
           hits, dis, dis_edge, max_edge, max_abs, max_scaled);
}

/* The limit set: 10,000 rays aimed at the limits, so the disagreement branches above really run. A quarter each at
   u = 0, v = 0 and u + v = 1 (within 1e-6 of the edge, either side), and at a distance within 1e-5 (relative) of eps.
   The same rules as the random sets: hit vs distance only near eps, hit vs double and prepared vs general only within
   1e-4 of a limit. The counts are printed; each kind of disagreement must occur at least once here. */
static void t_limit(void) {
    unsigned i, valid = 0, dis = 0, dbl_dis = 0, dbl_edge = 0, pdis = 0, pdis_edge = 0;
    double max_rel = 0.0, max_margin = 0.0, max_pedge = 0.0;

    for (i = 0; i < 10000u; i++) {
        float a[3], e1[3], e2[3], o[3], dir[3], u, v, t;
        const unsigned kind = i & 3u;
        const float tiny = frand(-1e-6f, 1e-6f);
        sh4der_raytri_num_t n;
        sh4der_raytri_dir_t p;
        uint32_t rc = 0;
        unsigned k, hit, g, dh;
        double margin;
        int dd;

        for (k = 0; k < 3u; k++) { a[k] = frand(-2.0f, 2.0f); e1[k] = frand(-2.0f, 2.0f); e2[k] = frand(-2.0f, 2.0f); }
        frand_unit(dir);
        u = frand(0.05f, 0.9f); v = frand(0.05f, 0.9f - u * 0.9f); t = frand(0.05f, 3.0f);
        if (kind == 0u) u = tiny;
        else if (kind == 1u) v = tiny;
        else if (kind == 2u) v = 1.0f - u + tiny;
        else t = 1e-4f * (1.0f + frand(-1e-5f, 1e-5f));
        for (k = 0; k < 3u; k++) o[k] = a[k] + u * e1[k] + v * e2[k] - dir[k] * t;

        if (sh4der_raytri_num(o, dir, a, e1, e2, &n)) {
            float tt;

            valid++;
            hit = sh4der_raytri_hit(&n, 1e-4f);
            tt = sh4der_raytri_t(&n, 1e30f, &rc);
            if (hit != (unsigned)(tt > 0.0f)) {
                const double rel = fabs((double)n.wn / (double)n.det - 1e-4) / 1e-4;

                dis++;
                if (rel > max_rel) max_rel = rel;
            }
        }
        g = hit_gen(o, dir, a, e1, e2, 1e-4f);
        dd = hit_double(o, dir, a, e1, e2, &margin);
        if (g != (unsigned)dd) {
            dbl_dis++;
            if (margin > max_margin) max_margin = margin;
            if (margin < 1e-4) dbl_edge++;
        }
        sh4der_raytri_dir_prep(&p, a, e1, e2, dir);
        dh = sh4der_raytri_dir_hit(&p, o, 1e-4f);
        if (dh != g) {
            pdis++;
            if (margin > max_pedge) max_pedge = margin;
            if (margin < 1e-4) pdis_edge++;
        }
    }
    /* A ray's float origin cannot put its distance within a few ulp of eps (the origin's own rounding is larger), so the
       hit vs distance limit is reached on the numerators themselves: 10,000 of them, wn within 8 ulp of eps x det. */
    for (i = 0; i < 10000u; i++) {
        sh4der_raytri_num_t n;
        uint32_t rc = 0;
        int st = (int)(i % 17u) - 8, s;
        float tt;

        n.det = frand(0.1f, 4.0f); n.un = 0.0f; n.vn = 0.0f;
        n.wn = 1e-4f * n.det;
        for (s = 0; s < (st < 0 ? -st : st); s++) n.wn = nextafterf(n.wn, st < 0 ? 0.0f : 1.0f);
        tt = sh4der_raytri_t(&n, 1e30f, &rc);
        if (sh4der_raytri_hit(&n, 1e-4f) != (unsigned)(tt > 0.0f)) {
            const double rel = fabs((double)n.wn / (double)n.det - 1e-4) / 1e-4;

            dis++;
            if (rel > max_rel) max_rel = rel;
        }
    }
    CHECK(max_rel <= 1e-6, "the limit set: a hit and a distance disagree off the limit (relative %g)", max_rel);
    CHECK(dbl_dis == dbl_edge, "the limit set: %u hits differ from the double test, %u not within 1e-4 of a limit", dbl_dis, dbl_dis - dbl_edge);
    CHECK(pdis == pdis_edge, "the limit set: the prepared form disagrees on %u rays, %u not within 1e-4 of a limit", pdis, pdis - pdis_edge);
    CHECK(dis > 0u && dbl_dis > 0u && pdis > 0u, "the limit set did not reach a limit (%u, %u, %u disagreements)", dis, dbl_dis, pdis);
    printf("raytri limit: %u rays at the edges and at eps, %u valid, and 10000 numerators within 8 ulp of eps; hit vs distance: %u disagree (largest relative %.3g of eps); hit vs double test: %u differ (%u within 1e-4, largest margin %.3g); prepared vs general: %u differ (%u within 1e-4, largest margin %.3g)\n",
           10000u, valid, dis, max_rel, dbl_dis, dbl_edge, max_margin, pdis, pdis_edge, max_pedge);
}

int main(void) {
    t_edges();
    t_diagonal();
    t_random();
    t_dir();
    t_limit();
    printf("raytri_test: %u checks, %u failed\n", checks, fails);
    return fails ? 1 : 0;
}
