/* SH4DER-PVR: host test of include/sh4der_aosky.h (occlusion of the sky drawn into a shadow target)

   test/aosky_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   The maths cases of demo/psm/ao/test/ao_layer_test.c, moved here with the names changed (sh4der_aosky_*), then the
   sender's cases:
     1 the directions, the alphas and the depths;
     2 the slide matrix against a ray to the ground plane, and "slide then map" against the two steps apart;
     3 that a ground point is under a slid triangle exactly when a ray from it toward the sky direction hits the
       triangle (Moeller-Trumbore, written here): the slide is the occlusion test it stands for;
     4 a software model of the target (depth "greater" with write, drawn in order): with the union a texel holds
       sh4der_aosky_level(k, n) for the n directions blocked at it, k + 1 levels at most; without it never lighter,
       somewhere darker;
     5 sh4der_aosky_dir on a closed box caster into a 64 x 64 target: all 12 triangles sent with cull 0, and one side
       sends half each (the sum is the two-sided count); an open triangle is sent at either cull; the caller's scratch
       is the only memory the call writes (a sentinel past vn is untouched, and garbage in the scratch changes nothing).

   Runs on the plain C path (-DSH4DER_CORE_NO_SH4ZAM) and on the SH4ZAM software path (make test-shz). The demo's
   psm_ao2_send and its town test stay in demo/psm/ao/test/ao_layer_test.c.

   Build: gcc -O2 -std=c99 -Wall -Wextra -Werror -DSH4DER_CORE_NO_SH4ZAM -Iinclude
          test/aosky_test.c -lm */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/sh4der_aosky.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t rng = 0x1234567u;
static float rnd(void) { rng = rng * 1664525u + 1013904223u; return (float)(rng >> 8) * (1.0f / 16777216.0f); }
static float rnd_in(float a, float b) { return a + (b - a) * rnd(); }

static void xform(const float m[16], const float p[3], float h[4]) {
    unsigned c;

    for (c = 0; c < 4u; c++) h[c] = p[0] * m[c] + p[1] * m[4 + c] + p[2] * m[8 + c] + m[12 + c];
}

/* the reference: a ray from o along d against a triangle; 1 and *t when it hits in front */
static int ray_tri(const float o[3], const float d[3], const float a[3], const float b[3], const float c[3], double *t, double *edge) {
    double e1[3], e2[3], h[3], s[3], q[3], det, u, v;
    unsigned i;

    for (i = 0; i < 3u; i++) { e1[i] = (double)b[i] - a[i]; e2[i] = (double)c[i] - a[i]; s[i] = (double)o[i] - a[i]; }
    h[0] = d[1] * e2[2] - d[2] * e2[1]; h[1] = d[2] * e2[0] - d[0] * e2[2]; h[2] = d[0] * e2[1] - d[1] * e2[0];
    det = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2];
    *edge = 0.0;
    if (fabs(det) < 1e-9) return 0;
    u = (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]) / det;
    q[0] = s[1] * e1[2] - s[2] * e1[1]; q[1] = s[2] * e1[0] - s[0] * e1[2]; q[2] = s[0] * e1[1] - s[1] * e1[0];
    v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) / det;
    *t = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) / det;
    *edge = fmin(fmin(fabs(u), fabs(v)), fabs(1.0 - u - v));      /* how near an edge the hit is */
    return u >= 0.0 && v >= 0.0 && u + v <= 1.0 && *t > 0.0;
}

static int in_tri2(const float p[2], const float a[2], const float b[2], const float c[2]) {
    const double d0 = ((double)b[0] - a[0]) * ((double)p[1] - a[1]) - ((double)b[1] - a[1]) * ((double)p[0] - a[0]);
    const double d1 = ((double)c[0] - b[0]) * ((double)p[1] - b[1]) - ((double)c[1] - b[1]) * ((double)p[0] - b[0]);
    const double d2 = ((double)a[0] - c[0]) * ((double)p[1] - c[1]) - ((double)a[1] - c[1]) * ((double)p[0] - c[0]);

    return (d0 >= 0.0 && d1 >= 0.0 && d2 >= 0.0) || (d0 <= 0.0 && d1 <= 0.0 && d2 <= 0.0);
}

/* ---- 1 ---- */
static void t_tables(void) {
    static const unsigned ks[] = { 0, 1, 2, 3, 4, 5, 7, 8, 9, 100 }, want[] = { 0, 1, 2, 2, 4, 4, 4, 8, 8, 8 };
    unsigned i, k, j;

    for (i = 0; i < sizeof ks / sizeof ks[0]; i++) CHECK(sh4der_aosky_count(ks[i]) == want[i], "sh4der_aosky_count(%u) = %u", ks[i], sh4der_aosky_count(ks[i]));
    for (k = 1; k <= 8u; k++) {
        CHECK(fabs(pow(1.0 - sh4der_aosky_alpha(k), (double)k) - SH4DER_AOSKY_FLOOR) < 1e-6, "alpha(%u): all blocked leaves %f", k, pow(1.0 - sh4der_aosky_alpha(k), (double)k));
        CHECK(fabsf(sh4der_aosky_level(k, k) - SH4DER_AOSKY_FLOOR) < 1e-5f && sh4der_aosky_level(k, 0) == 1.0f, "sh4der_aosky_level(%u)", k);
    }
    CHECK(sh4der_aosky_alpha(0) == 0.0f && sh4der_aosky_argb(0, 1.0f) == 0u, "k 0 draws nothing");
    CHECK(sh4der_aosky_argb(1, 1.0f) == 0x80000000u, "argb(1) %08x", (unsigned)sh4der_aosky_argb(1, 1.0f));
    CHECK(sh4der_aosky_argb(8, 1.0f) == (21u << 24) && sh4der_aosky_argb(8, 2.0f) == (21u << 24) && sh4der_aosky_argb(8, -1.0f) == 0u, "argb(8) %08x", (unsigned)sh4der_aosky_argb(8, 1.0f));
    CHECK((sh4der_aosky_argb(4, 0.5f) & 0x00ffffffu) == 0u, "the colour is black");
    for (k = 1; k <= 8u; k *= 2u) {
        float sum[2] = { 0.0f, 0.0f };

        for (i = 0; i < k; i++) {
            float s[2], t[2];

            sh4der_aosky_slope(k, i, s);
            CHECK(fabsf(sqrtf(s[0] * s[0] + s[1] * s[1]) - SH4DER_AOSKY_COT_EL) < 1e-6f, "slope length k %u i %u", k, i);
            CHECK(fabsf(s[0]) > 0.2f && fabsf(s[1]) > 0.2f, "no direction along an axis");
            sum[0] += s[0]; sum[1] += s[1];
            for (j = 0; j < i; j++) {                           /* evenly round the sky: the angle between two is a multiple of 360 / k */
                double ang;

                sh4der_aosky_slope(k, j, t);
                ang = acos((s[0] * t[0] + s[1] * t[1]) / (SH4DER_AOSKY_COT_EL * SH4DER_AOSKY_COT_EL)) * (180.0 / 3.14159265358979) / (360.0 / k);
                CHECK(fabs(ang - floor(ang + 0.5)) < 1e-3 && ang > 0.5, "spacing k %u: %u and %u", k, i, j);
            }
        }
        if (k > 1u) CHECK(fabsf(sum[0]) < 1e-5f && fabsf(sum[1]) < 1e-5f, "k %u directions balance", k);
    }
    for (i = 0; i < SH4DER_AOSKY_DIRS_MAX; i++) {
        CHECK(sh4der_aosky_depth(i) > 1.0f && sh4der_aosky_depth(i) < 2.0f, "depth %u between the casters and the border", i);
        if (i) CHECK(sh4der_aosky_depth(i) > sh4der_aosky_depth(i - 1u), "depth %u nearer than the one before", i);
    }
}

/* ---- 2 ---- */
static void t_slide(void) {
    unsigned n, c;

    for (n = 0; n < 2000u; n++) {
        const float y0 = rnd_in(-2.0f, 2.0f);
        float s[2], S[16], M[16], A[16], p[3], h[4], g[3], hm[4], ha[4];

        sh4der_aosky_slope(8u, n & 7u, s);
        p[0] = rnd_in(-40.0f, 40.0f); p[1] = y0 + rnd_in(0.0f, 12.0f); p[2] = rnd_in(-40.0f, 40.0f);
        sh4der_aosky_slide(s, y0, S);
        xform(S, p, h);
        g[0] = p[0] - s[0] * (p[1] - y0); g[1] = y0; g[2] = p[2] - s[1] * (p[1] - y0);     /* the ray to the plane */
        CHECK(fabsf(h[0] - g[0]) < 1e-4f && fabsf(h[1] - g[1]) < 1e-4f && fabsf(h[2] - g[2]) < 1e-4f && h[3] == 1.0f, "slide %u", n);
        for (c = 0; c < 16u; c++) M[c] = rnd_in(-1.0f, 1.0f);
        sh4der_aosky_slide_map(M, s, y0, A);
        xform(M, g, hm);
        xform(A, p, ha);
        for (c = 0; c < 4u; c++) CHECK(fabsf(hm[c] - ha[c]) < 2e-3f, "slide then map %u column %u: %f %f", n, c, hm[c], ha[c]);
    }
    {   /* a point on the ground stays where it is */
        float s[2], S[16], p[3] = { 3.0f, 0.5f, -7.0f }, h[4];

        sh4der_aosky_slope(4u, 1u, s);
        sh4der_aosky_slide(s, 0.5f, S);
        xform(S, p, h);
        CHECK(fabsf(h[0] - 3.0f) < 1e-6f && fabsf(h[1] - 0.5f) < 1e-6f && fabsf(h[2] + 7.0f) < 1e-6f, "the ground does not move");
    }
}

/* ---- 3 ---- */
static void t_occlusion(void) {
    unsigned n, tested = 0, hits = 0;

    for (n = 0; n < 20000u; n++) {
        float s[2], S[16], tri[3][3], sl[3][2], g[3], d[3], h[4], g2[2];
        double t, edge;
        unsigned i;
        int ray, flat;

        sh4der_aosky_slope(8u, n & 7u, s);
        sh4der_aosky_slide(s, 0.0f, S);
        for (i = 0; i < 3u; i++) {
            tri[i][0] = rnd_in(-3.0f, 3.0f); tri[i][1] = rnd_in(0.2f, 5.0f); tri[i][2] = rnd_in(-3.0f, 3.0f);
            xform(S, tri[i], h);
            sl[i][0] = h[0]; sl[i][1] = h[2];
        }
        g[0] = rnd_in(-6.0f, 6.0f); g[1] = 0.0f; g[2] = rnd_in(-6.0f, 6.0f);
        d[0] = s[0]; d[1] = 1.0f; d[2] = s[1];                      /* toward the sky; the length does not matter */
        ray = ray_tri(g, d, tri[0], tri[1], tri[2], &t, &edge);
        if (edge < 1e-3) continue;                                  /* on an edge: either answer is right */
        g2[0] = g[0]; g2[1] = g[2];
        flat = in_tri2(g2, sl[0], sl[1], sl[2]);
        tested++; hits += (unsigned)ray;
        CHECK(ray == flat, "occlusion %u: ray %d, slid triangle %d", n, ray, flat);
    }
    CHECK(tested > 15000u && hits > 300u, "occlusion: %u tested, %u hits", tested, hits);
}

/* ---- 4: the target as the PVR is asked to draw it ---- */
#define TS 96u
static float t_col[TS * TS], t_z[TS * TS];

static void target_clear(void) {
    unsigned i;

    for (i = 0; i < TS * TS; i++) { t_col[i] = 1.0f; t_z[i] = 0.1f; }
}

/* a triangle in target pixels at depth z, black at alpha a: unite 1 = test greater and write; 0 = test greater-or-equal, no write */
static void target_tri(const float a[2], const float b[2], const float c[2], float z, float alpha, int unite) {
    unsigned x, y;

    for (y = 0; y < TS; y++)
        for (x = 0; x < TS; x++) {
            const float p[2] = { (float)x + 0.5f, (float)y + 0.5f };
            const unsigned i = y * TS + x;

            if (!in_tri2(p, a, b, c)) continue;
            if (unite ? !(z > t_z[i]) : !(z >= t_z[i])) continue;
            t_col[i] *= 1.0f - alpha;
            if (unite) t_z[i] = z;
        }
}

#define BOXES 5u
static float box_v[BOXES][8][3];
static const uint8_t box_t[12][3] = { {0,1,2},{1,3,2},{4,6,5},{5,6,7},{0,4,1},{1,4,5},{2,3,6},{3,7,6},{0,2,4},{2,6,4},{1,5,3},{3,5,7} };

static void boxes_make(void) {
    unsigned b, i;

    for (b = 0; b < BOXES; b++) {
        const float cx = rnd_in(-4.0f, 4.0f), cz = rnd_in(-4.0f, 4.0f), w = rnd_in(0.4f, 1.5f), d = rnd_in(0.4f, 1.5f), hgt = rnd_in(0.5f, 4.0f);

        for (i = 0; i < 8u; i++) {
            box_v[b][i][0] = cx + ((i & 1u) ? w : -w);
            box_v[b][i][1] = (i & 4u) ? hgt : 0.0f;
            box_v[b][i][2] = cz + ((i & 2u) ? d : -d);
        }
    }
}

static void t_target(void) {
    /* the map: the ground's x, z in -8 .. 8 as -1 .. 1, w 1 (a uniform map looking straight down) */
    static const float map[16] = { 0.125f, 0, 0, 0,   0, 0, 0, 0,   0, 0.125f, 0, 0,   0, 0, 0, 1 };
    static float un[TS * TS];
    unsigned k, d, b, t, i, x, y;

    boxes_make();
    for (k = 1; k <= 8u; k *= 2u) {
        const float alpha = sh4der_aosky_alpha(k);
        unsigned levels[SH4DER_AOSKY_DIRS_MAX + 1u], bad = 0, darker = 0, lighter = 0, pass;

        for (pass = 0; pass < 2u; pass++) {                        /* 0: the union; 1: plain blending */
            target_clear();
            for (d = 0; d < k; d++) {
                float s[2], A[16];

                sh4der_aosky_slope(k, d, s);
                sh4der_aosky_slide_map(map, s, 0.0f, A);
                for (b = 0; b < BOXES; b++)
                    for (t = 0; t < 12u; t++) {
                        float xy[3][2], h[4];
                        int ok = 1;

                        for (i = 0; i < 3u; i++) { xform(A, box_v[b][box_t[t][i]], h); ok &= sh4der_aosky_px(h, 1.0f, 0.5f * (float)TS, xy[i]); }
                        CHECK(ok, "a box corner has no place in the target");
                        if (ok && sh4der_aosky_tri_ok(xy[0], xy[1], xy[2], (float)TS, 0)) target_tri(xy[0], xy[1], xy[2], sh4der_aosky_depth(d), alpha, !pass);
                    }
            }
            if (!pass) memcpy(un, t_col, sizeof un);
        }
        memset(levels, 0, sizeof levels);
        for (y = 0; y < TS; y++)
            for (x = 0; x < TS; x++) {
                /* the reference: the ground point of this texel, a ray toward each direction against every box triangle */
                const float g[3] = { ((float)x + 0.5f) * (16.0f / (float)TS) - 8.0f, 0.0f, ((float)y + 0.5f) * (16.0f / (float)TS) - 8.0f };
                unsigned blocked = 0;
                int near_edge = 0;

                for (d = 0; d < k; d++) {
                    float s[2], dir[3];
                    int hit = 0;

                    sh4der_aosky_slope(k, d, s);
                    dir[0] = s[0]; dir[1] = 1.0f; dir[2] = s[1];
                    for (b = 0; b < BOXES; b++)
                        for (t = 0; t < 12u; t++) {
                            double tt, edge;
                            const int h1 = ray_tri(g, dir, box_v[b][box_t[t][0]], box_v[b][box_t[t][1]], box_v[b][box_t[t][2]], &tt, &edge);

                            if (edge < 2e-3) near_edge = 1;
                            hit |= h1;
                        }
                    blocked += (unsigned)hit;
                }
                i = y * TS + x;
                if (t_col[i] < un[i] - 1e-6f) darker++;
                if (t_col[i] > un[i] + 1e-6f) lighter++;
                if (near_edge) continue;
                levels[blocked]++;
                if (fabsf(un[i] - sh4der_aosky_level(k, blocked)) > 1e-5f) bad++;
            }
        CHECK(bad == 0, "k %u: %u texels of the union are not sh4der_aosky_level(k, directions blocked)", k, bad);
        CHECK(lighter == 0, "k %u: plain blending is lighter than the union at %u texels", k, lighter);
        CHECK(darker > 0, "k %u: plain blending is nowhere darker: the boxes do not overlap themselves?", k);
        CHECK(levels[0] > 0 && levels[1] > 0, "k %u: the scene has lit and blocked texels", k);
        printf("  k %u: texels by directions blocked:", k);
        for (i = 0; i <= k; i++) printf(" %u", levels[i]);
        printf("; plain blending darker at %u\n", darker);
    }
}


/* ---- 5: sh4der_aosky_dir: the caller's casters and scratch ---- */
#define DS 64u                              /* the test's target, in pixels a side */
#define DIR_VN 8u

static float box_p[8][3];
static uint16_t box_tri[12][3];
static float dxy[DIR_VN + 1u][2];           /* the caller's scratch: one more than vn, the last is a sentinel */
static uint8_t dok[DIR_VN + 1u];
static const float dmap[16] = { 0.125f, 0, 0, 0,   0, 0, 0, 0,   0, 0.125f, 0, 0,   0, 0, 0, 1 };   /* ground -8 .. 8 to -1 .. 1 */

/* A unit-ish box from -1 .. 1 in x and z, 0 .. 2 in y, with each triangle wound outward (the test checks the winding
   against the box's own centre, so the caster is closed and one-way). */
static void box_make(void) {
    static const uint8_t quad[6][4] = { {0,1,3,2}, {4,6,7,5}, {0,4,5,1}, {2,3,7,6}, {0,2,6,4}, {1,5,7,3} };
    static const float lo[3] = { -1.0f, 0.0f, -1.0f }, hi[3] = { 1.0f, 2.0f, 1.0f }, cen[3] = { 0.0f, 1.0f, 0.0f };
    unsigned i, f, h, t = 0, k;

    for (i = 0; i < 8u; i++) {
        box_p[i][0] = (i & 1u) ? hi[0] : lo[0];
        box_p[i][1] = (i & 4u) ? hi[1] : lo[1];
        box_p[i][2] = (i & 2u) ? hi[2] : lo[2];
    }
    for (f = 0; f < 6u; f++)
        for (h = 0; h < 2u; h++) {
            const uint8_t *q = quad[f];
            const uint8_t ia = q[0], ib = h ? q[2] : q[1], ic = h ? q[3] : q[2];
            const float *a = box_p[ia], *b = box_p[ib], *c = box_p[ic];
            float e1[3], e2[3], n[3], fc[3], dd;

            for (k = 0; k < 3u; k++) { e1[k] = b[k] - a[k]; e2[k] = c[k] - a[k]; fc[k] = (a[k] + b[k] + c[k]) / 3.0f - cen[k]; }
            n[0] = e1[1] * e2[2] - e1[2] * e2[1]; n[1] = e1[2] * e2[0] - e1[0] * e2[2]; n[2] = e1[0] * e2[1] - e1[1] * e2[0];
            dd = n[0] * fc[0] + n[1] * fc[1] + n[2] * fc[2];
            box_tri[t][0] = ia;
            box_tri[t][1] = dd > 0.0f ? ib : ic;
            box_tri[t][2] = dd > 0.0f ? ic : ib;
            t++;
        }
}

static void count_emit(void *user, const float a[2], const float b[2], const float c[2], float z, uint32_t argb, int blend) {
    (void)a; (void)b; (void)c; (void)z; (void)argb; (void)blend;
    (*(unsigned *)user)++;
}

static unsigned dir_emit(const sh4der_aosky_casters_t *cs, const float slope[2], int cull, float (*xy)[2], uint8_t *ok) {
    unsigned n = 0;

    sh4der_aosky_dir(cs, dmap, slope, 0.0f, DS, sh4der_aosky_depth(0), 0x80000000u, cull, 1, xy, ok, count_emit, &n);
    return n;
}

static void t_dir(void) {
    static const float tv[3][3] = { { -1.0f, 0.5f, -1.0f }, { 1.0f, 0.5f, -1.0f }, { 0.0f, 0.5f, 1.0f } };
    static const uint16_t tt[1][3] = { { 0, 1, 2 } };
    static const uint8_t open1[1] = { 1 };
    sh4der_aosky_casters_t box = { box_p, 8u, box_tri, 12u, NULL }, tri = { tv, 3u, tt, 1u, NULL }, tri_open = { tv, 3u, tt, 1u, open1 };
    float s[2], zero[2] = { 0.0f, 0.0f };
    unsigned n0, n1, nm, t_all, t_plus, t_minus, i;

    box_make();
    sh4der_aosky_slope(4u, 1u, s);

    /* a closed box: all 12 triangles in the target, one side half and half */
    n0 = dir_emit(&box, s, 0, dxy, dok);
    n1 = dir_emit(&box, s, 1, dxy, dok);
    nm = dir_emit(&box, s, -1, dxy, dok);
    CHECK(n0 == 12u, "box: %u triangles sent two-sided, want 12", n0);
    CHECK(n1 + nm == n0, "box: one side %u + %u is not the two-sided %u", n1, nm, n0);
    CHECK(n1 == 6u && nm == 6u, "box: one side sends %u and %u, want 6 and 6", n1, nm);
    printf("  box: %u two-sided, %u cull +1, %u cull -1\n", n0, n1, nm);

    /* the scratch is the caller's: garbage in it changes nothing, and nothing past vn is written */
    for (i = 0; i <= DIR_VN; i++) { dxy[i][0] = 1.0e9f; dxy[i][1] = -1.0e9f; dok[i] = 0xABu; }
    {
        const unsigned again = dir_emit(&box, s, 0, dxy, dok);

        CHECK(again == n0, "box: a second call with garbage scratch sends %u, want %u", again, n0);
        CHECK(dok[DIR_VN] == 0xABu, "box: the call wrote past its vertices (sentinel %u)", dok[DIR_VN]);
    }

    /* an open triangle is sent at either cull; a closed one sends its single side */
    t_all = dir_emit(&tri_open, zero, 0, dxy, dok);
    t_plus = dir_emit(&tri_open, zero, 1, dxy, dok);
    t_minus = dir_emit(&tri_open, zero, -1, dxy, dok);
    CHECK(t_all == 1u && t_plus == 1u && t_minus == 1u, "open triangle: %u, %u, %u at cull 0, 1, -1; want 1, 1, 1", t_all, t_plus, t_minus);
    t_plus = dir_emit(&tri, zero, 1, dxy, dok);
    t_minus = dir_emit(&tri, zero, -1, dxy, dok);
    CHECK(t_plus + t_minus == 1u, "closed triangle: one side sends %u + %u, want 1", t_plus, t_minus);
    printf("  open triangle: %u at cull 0, 1 and -1; closed: %u + %u\n", t_all, t_plus, t_minus);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    t_tables();
    t_slide();
    t_occlusion();
    t_target();
    t_dir();
    if (fails) { printf("aosky_test: %d FAILED\n", fails); return 1; }
    printf("aosky_test: ok\n");
    return 0;
}
