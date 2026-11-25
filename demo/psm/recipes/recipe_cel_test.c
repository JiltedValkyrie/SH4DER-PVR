/* SH4DER-PVR: R1 cel ramp, host check

   demo/psm/recipes/recipe_cel_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The vertex's u (N.L, wrapped, held in 0..1) against a double reference; u rises with N.L; a vertex facing away
   reads the first texel; the band edges a point sample shows sit within half a texel of the asked ones under both
   texel-centre conventions, and within a whole texel when the ramp was built for the other one; the PVR's
   perspective-correct u is the world-space interpolation of the vertices' u (so a band edge is a straight line on
   the triangle in the world), which a screen-space interpolation is not; on a sphere the share of samples whose
   band differs from the true N.L falls as the mesh gets finer. Prints PASS and FAIL counts.
*/

#include <math.h>
#include <stdio.h>
#include "recipes.h"

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.00002f
#endif

static unsigned passes, fails;
#define CHECK(c) do { if (c) passes++; else { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static const float EDGES[2] = { 0.25f, 0.6f };   /* three bands: shade, mid, lit */

static void unit3(double v[3]) {
    const double l = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    v[0] /= l; v[1] /= l; v[2] /= l;
}

static unsigned band_of(float nl) {
    unsigned b = 0;
    while (b < 2u && nl >= EDGES[b]) b++;
    return b;
}

/* 1. u against the reference, over a sphere of normals and two wraps. */
static void t_u(void) {
    const float l[3] = { 0.48f, 0.64f, 0.6f };
    const float ws[2] = { 0.0f, 0.5f };
    unsigned i, j, k;
    float worst = 0.0f;
    for (k = 0; k < 2u; k++) {
        const float w = ws[k], inv = 1.0f / (1.0f + w);
        for (i = 0; i <= 32u; i++) {
            for (j = 0; j < 64u; j++) {
                const double th = M_PI * i / 32.0, ph = 2.0 * M_PI * j / 64.0;
                const float n[3] = { (float)(sin(th) * cos(ph)), (float)cos(th), (float)(sin(th) * sin(ph)) };
                double r = ((double)n[0] * l[0] + (double)n[1] * l[1] + (double)n[2] * l[2] + w) / (1.0 + w);
                const float u = rcp_cel_u(n, l, w, inv);
                r = r < 0.0 ? 0.0 : r > 1.0 ? 1.0 : r;
                if (fabs(u - r) > worst) worst = (float)fabs(u - r);
            }
        }
    }
    printf("R1 u against the reference: worst %.2e (tolerance %.0e)\n", worst, (double)SH4DER_TEST_TOL * 5.0);
    CHECK(worst <= SH4DER_TEST_TOL * 5.0f);
}

/* 2, 3. u rises with N.L; a vertex facing away reads texel 0. */
static void t_monotonic(void) {
    const float l[3] = { 0.0f, 1.0f, 0.0f };
    float prev = -1.0f;
    int rising = 1, away = 1;
    unsigned i;
    for (i = 0; i <= 2000u; i++) {
        const double a = M_PI * i / 2000.0;
        const float n[3] = { (float)sin(a), (float)cos(a), 0.0f };
        const float u = rcp_cel_u(n, l, 0.0f, 1.0f);
        if (u > prev + 1e-6f && i) rising = 0;          /* N.L falls as a grows, so u must not rise */
        if (cos(a) <= 0.0 && rcp_cel_texel(u, 8u, 0.0f) != 0u) away = 0;
        prev = u;
    }
    CHECK(rising);
    CHECK(away);
}

/* The first u (scanning up) where the ramp's band changes from b to b + 1, or -1. */
static float edge_seen(const uint8_t *ramp, unsigned n, float delta, unsigned b) {
    unsigned i;
    for (i = 0; i <= 100000u; i++) {
        const float u = (float)i * 1e-5f;
        if (ramp[rcp_cel_texel(u, n, delta)] > b) return u;
    }
    return -1.0f;
}

/* 4. Band edges: within half a texel under the convention the ramp was built for, within a whole one under the other. */
static void t_edges(void) {
    static const unsigned ns[3] = { 8u, 16u, 64u };
    unsigned a, b, c;
    for (a = 0; a < 3u; a++) {
        const unsigned n = ns[a];
        for (c = 0; c < 2u; c++) {
            const float delta = c ? 0.5f : 0.0f, other = c ? 0.0f : 0.5f;
            uint8_t ramp[64];
            float own = 0.0f, cross = 0.0f;
            rcp_cel_ramp(ramp, n, EDGES, 3u, delta);
            for (b = 0; b < 2u; b++) {
                const float e1 = edge_seen(ramp, n, delta, b), e2 = edge_seen(ramp, n, other, b);
                if (fabsf(e1 - EDGES[b]) > own) own = fabsf(e1 - EDGES[b]);
                if (fabsf(e2 - EDGES[b]) > cross) cross = fabsf(e2 - EDGES[b]);
            }
            printf("R1 ramp %2u texels, built for delta %.1f: edge off by %.4f (half a texel %.4f); read with delta %.1f: %.4f\n",
                   n, (double)delta, (double)own, 0.5 / n, (double)other, (double)cross);
            CHECK(own <= 0.5f / (float)n + 2e-5f);
            CHECK(cross <= 1.0f / (float)n + 2e-5f);
        }
    }
}

/* 5. Perspective-correct u is the world-space interpolation; screen-space is not. Camera at the origin looking down
   +z, focal 1: screen (x / z, y / z). */
static void t_perspective(void) {
    const double P[3][3] = { { -2.0, -1.0, 1.0 }, { 6.0, -4.0, 5.0 }, { -10.0, 12.0, 20.0 } };
    const double U[3] = { 0.0, 0.6, 1.0 };
    double S[3][2], nrm[3], e1[3], e2[3], worst_p = 0.0, worst_a = 0.0;
    unsigned i, j, k, inside = 0;
    for (k = 0; k < 3u; k++) { S[k][0] = P[k][0] / P[k][2]; S[k][1] = P[k][1] / P[k][2]; }
    for (k = 0; k < 3u; k++) { e1[k] = P[1][k] - P[0][k]; e2[k] = P[2][k] - P[0][k]; }
    nrm[0] = e1[1] * e2[2] - e1[2] * e2[1]; nrm[1] = e1[2] * e2[0] - e1[0] * e2[2]; nrm[2] = e1[0] * e2[1] - e1[1] * e2[0];
    for (j = 0; j < 200u; j++) {
        for (i = 0; i < 200u; i++) {
            const double sx = -2.0 + 4.0 * (i + 0.5) / 200.0, sy = -1.0 + 2.0 * (j + 0.5) / 200.0;
            const double den = (S[1][1] - S[2][1]) * (S[0][0] - S[2][0]) + (S[2][0] - S[1][0]) * (S[0][1] - S[2][1]);
            const double l0 = ((S[1][1] - S[2][1]) * (sx - S[2][0]) + (S[2][0] - S[1][0]) * (sy - S[2][1])) / den;
            const double l1 = ((S[2][1] - S[0][1]) * (sx - S[2][0]) + (S[0][0] - S[2][0]) * (sy - S[2][1])) / den;
            const double l2 = 1.0 - l0 - l1, L[3] = { l0, l1, l2 };
            double num = 0.0, dn = 0.0, ua = 0.0, t, X[3], w[3], uw;
            if (l0 < 0.0 || l1 < 0.0 || l2 < 0.0) continue;
            inside++;
            for (k = 0; k < 3u; k++) { num += L[k] * U[k] / P[k][2]; dn += L[k] / P[k][2]; ua += L[k] * U[k]; }
            /* the world point the pixel's ray meets, and its barycentric weights on the triangle */
            t = (nrm[0] * P[0][0] + nrm[1] * P[0][1] + nrm[2] * P[0][2]) / (nrm[0] * sx + nrm[1] * sy + nrm[2]);
            X[0] = sx * t; X[1] = sy * t; X[2] = t;
            {
                double v2[3], d00, d01, d11, d20, d21, dd;
                for (k = 0; k < 3u; k++) v2[k] = X[k] - P[0][k];
                d00 = e1[0] * e1[0] + e1[1] * e1[1] + e1[2] * e1[2]; d01 = e1[0] * e2[0] + e1[1] * e2[1] + e1[2] * e2[2];
                d11 = e2[0] * e2[0] + e2[1] * e2[1] + e2[2] * e2[2];
                d20 = v2[0] * e1[0] + v2[1] * e1[1] + v2[2] * e1[2]; d21 = v2[0] * e2[0] + v2[1] * e2[1] + v2[2] * e2[2];
                dd = d00 * d11 - d01 * d01;
                w[1] = (d11 * d20 - d01 * d21) / dd; w[2] = (d00 * d21 - d01 * d20) / dd; w[0] = 1.0 - w[1] - w[2];
            }
            uw = w[0] * U[0] + w[1] * U[1] + w[2] * U[2];
            if (fabs(num / dn - uw) > worst_p) worst_p = fabs(num / dn - uw);
            if (fabs(ua - uw) > worst_a) worst_a = fabs(ua - uw);
        }
    }
    printf("R1 perspective: %u pixels; perspective-correct u against the world interpolation %.2e; screen-space u off by up to %.3f\n",
           inside, worst_p, worst_a);
    CHECK(inside > 1000u);
    CHECK(worst_p <= 1e-9);
    CHECK(worst_a > 0.05);              /* the test can see the difference it guards against */
}

/* 6. On a sphere under a light, the share of samples whose band (interpolated u, 64-texel ramp) differs from the
   band of the true N.L, by mesh size. */
static void t_sphere(void) {
    static const unsigned rings[4] = { 8u, 16u, 32u, 64u };
    const float l[3] = { 0.48f, 0.64f, 0.6f };
    uint8_t ramp[64];
    double share[4], mesh[4];
    unsigned r;
    rcp_cel_ramp(ramp, 64u, EDGES, 3u, 0.0f);
    for (r = 0; r < 4u; r++) {
        const unsigned R = rings[r], S = 2u * rings[r];
        unsigned i, j, a, b, total = 0, differ = 0, differ_mesh = 0;
        for (i = 0; i < R; i++) {
            for (j = 0; j < S; j++) {
                double p[4][3];
                float u[4];
                unsigned k;
                for (k = 0; k < 4u; k++) {
                    const double th = M_PI * (i + (k >> 1)) / R, ph = 2.0 * M_PI * (j + (k & 1u)) / S;
                    float n[3];
                    p[k][0] = sin(th) * cos(ph); p[k][1] = cos(th); p[k][2] = sin(th) * sin(ph);
                    n[0] = (float)p[k][0]; n[1] = (float)p[k][1]; n[2] = (float)p[k][2];
                    u[k] = rcp_cel_u(n, l, 0.0f, 1.0f);
                }
                for (k = 0; k < 2u; k++) {      /* two triangles: 0 1 3 and 0 3 2 */
                    const unsigned t0 = 0u, t1 = k ? 3u : 1u, t2 = k ? 2u : 3u;
                    for (a = 0; a < 8u; a++) {
                        for (b = 0; a + b < 8u; b++) {
                            const double w1 = (a + 1.0 / 3.0) / 8.0, w2 = (b + 1.0 / 3.0) / 8.0, w0 = 1.0 - w1 - w2;
                            double q[3];
                            unsigned m;
                            const float ui = (float)(w0 * u[t0] + w1 * u[t1] + w2 * u[t2]);
                            for (m = 0; m < 3u; m++) q[m] = w0 * p[t0][m] + w1 * p[t1][m] + w2 * p[t2][m];
                            unit3(q);
                            total++;
                            {
                                const unsigned truth = band_of((float)(q[0] * l[0] + q[1] * l[1] + q[2] * l[2]));
                                differ += ramp[rcp_cel_texel(ui, 64u, 0.0f)] != truth;   /* mesh and ramp */
                                differ_mesh += band_of(ui) != truth;                      /* mesh alone */
                            }
                        }
                    }
                }
            }
        }
        share[r] = (double)differ / total;
        mesh[r] = (double)differ_mesh / total;
        printf("R1 sphere %2u x %3u: in another band than the true N.L: %.3f %% from the mesh alone, %.3f %% with the 64-texel ramp\n",
               R, S, 100.0 * mesh[r], 100.0 * share[r]);
    }
    CHECK(mesh[0] > mesh[1] && mesh[1] > mesh[2] && mesh[2] > mesh[3]);   /* the mesh's part falls as it gets finer */
    CHECK(share[3] <= share[0]);                                         /* the ramp's half texel is the floor left */
}

int main(void) {
    t_u();
    t_monotonic();
    t_edges();
    t_perspective();
    t_sphere();
    printf("recipe_cel_test: PASS %u FAIL %u\n", passes, fails);
    return fails ? 1 : 0;
}
