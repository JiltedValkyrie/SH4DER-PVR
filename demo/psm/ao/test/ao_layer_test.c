/* SH4DER-PVR: host test of demo/psm/ao_layer.h (AO method 2: sky directions into route A's target)

   demo/psm/ao/test/ao_layer_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   What it checks, on the host, of the maths alone (the PVR's part is the ao0 image's):
     1 the directions, the alphas and the depths;
     2 the slide matrix against a ray to the ground plane, and "slide then map" against the two steps apart;
     3 that a ground point is under a slid triangle exactly when a ray from it toward the sky direction hits the
       triangle (Moeller-Trumbore, written here): the slide is the occlusion test it stands for;
     4 a software model of the target (depth "greater" with write, drawn in order): with the union a texel holds
       ao_level(k, n) for the n directions blocked at it, k + 1 levels at most; without it never lighter, somewhere
       darker.
   With -DAO_TEST_TOWN (and the town's include paths) it also runs psm_ao2_send over a small scene through a counting
   sink.

   Build: gcc -O2 -std=c99 -Wall -Wextra -Werror -DSH4DER_CORE_NO_SH4ZAM -Iinclude -Ish4dow-dc/include
          demo/psm/ao/test/ao_layer_test.c -lm */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef AO_TEST_TOWN
#include "../../psm_scene.h"
#include "../../psm_sky.h"
#include "../../psm_send.h"
#endif
#include "../../ao_layer.h"

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

    for (i = 0; i < sizeof ks / sizeof ks[0]; i++) CHECK(ao_count(ks[i]) == want[i], "ao_count(%u) = %u", ks[i], ao_count(ks[i]));
    for (k = 1; k <= 8u; k++) {
        CHECK(fabs(pow(1.0 - ao_alpha(k), (double)k) - AO_FLOOR) < 1e-6, "alpha(%u): all blocked leaves %f", k, pow(1.0 - ao_alpha(k), (double)k));
        CHECK(fabsf(ao_level(k, k) - AO_FLOOR) < 1e-5f && ao_level(k, 0) == 1.0f, "ao_level(%u)", k);
    }
    CHECK(ao_alpha(0) == 0.0f && ao_argb(0, 1.0f) == 0u, "k 0 draws nothing");
    CHECK(ao_argb(1, 1.0f) == 0x80000000u, "argb(1) %08x", (unsigned)ao_argb(1, 1.0f));
    CHECK(ao_argb(8, 1.0f) == (21u << 24) && ao_argb(8, 2.0f) == (21u << 24) && ao_argb(8, -1.0f) == 0u, "argb(8) %08x", (unsigned)ao_argb(8, 1.0f));
    CHECK((ao_argb(4, 0.5f) & 0x00ffffffu) == 0u, "the colour is black");
    for (k = 1; k <= 8u; k *= 2u) {
        float sum[2] = { 0.0f, 0.0f };

        for (i = 0; i < k; i++) {
            float s[2], t[2];

            ao_slope(k, i, s);
            CHECK(fabsf(sqrtf(s[0] * s[0] + s[1] * s[1]) - AO_COT_EL) < 1e-6f, "slope length k %u i %u", k, i);
            CHECK(fabsf(s[0]) > 0.2f && fabsf(s[1]) > 0.2f, "no direction along an axis");
            sum[0] += s[0]; sum[1] += s[1];
            for (j = 0; j < i; j++) {                           /* evenly round the sky: the angle between two is a multiple of 360 / k */
                double ang;

                ao_slope(k, j, t);
                ang = acos((s[0] * t[0] + s[1] * t[1]) / (AO_COT_EL * AO_COT_EL)) * (180.0 / 3.14159265358979) / (360.0 / k);
                CHECK(fabs(ang - floor(ang + 0.5)) < 1e-3 && ang > 0.5, "spacing k %u: %u and %u", k, i, j);
            }
        }
        if (k > 1u) CHECK(fabsf(sum[0]) < 1e-5f && fabsf(sum[1]) < 1e-5f, "k %u directions balance", k);
    }
    for (i = 0; i < AO_DIRS_MAX; i++) {
        CHECK(ao_depth(i) > 1.0f && ao_depth(i) < 2.0f, "depth %u between the casters and the border", i);
        if (i) CHECK(ao_depth(i) > ao_depth(i - 1u), "depth %u nearer than the one before", i);
    }
}

/* ---- 2 ---- */
static void t_slide(void) {
    unsigned n, c;

    for (n = 0; n < 2000u; n++) {
        const float y0 = rnd_in(-2.0f, 2.0f);
        float s[2], S[16], M[16], A[16], p[3], h[4], g[3], hm[4], ha[4];

        ao_slope(8u, n & 7u, s);
        p[0] = rnd_in(-40.0f, 40.0f); p[1] = y0 + rnd_in(0.0f, 12.0f); p[2] = rnd_in(-40.0f, 40.0f);
        ao_slide(s, y0, S);
        xform(S, p, h);
        g[0] = p[0] - s[0] * (p[1] - y0); g[1] = y0; g[2] = p[2] - s[1] * (p[1] - y0);     /* the ray to the plane */
        CHECK(fabsf(h[0] - g[0]) < 1e-4f && fabsf(h[1] - g[1]) < 1e-4f && fabsf(h[2] - g[2]) < 1e-4f && h[3] == 1.0f, "slide %u", n);
        for (c = 0; c < 16u; c++) M[c] = rnd_in(-1.0f, 1.0f);
        ao_slide_map(M, s, y0, A);
        xform(M, g, hm);
        xform(A, p, ha);
        for (c = 0; c < 4u; c++) CHECK(fabsf(hm[c] - ha[c]) < 2e-3f, "slide then map %u column %u: %f %f", n, c, hm[c], ha[c]);
    }
    {   /* a point on the ground stays where it is */
        float s[2], S[16], p[3] = { 3.0f, 0.5f, -7.0f }, h[4];

        ao_slope(4u, 1u, s);
        ao_slide(s, 0.5f, S);
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

        ao_slope(8u, n & 7u, s);
        ao_slide(s, 0.0f, S);
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
        const float alpha = ao_alpha(k);
        unsigned levels[AO_DIRS_MAX + 1u], bad = 0, darker = 0, lighter = 0, pass;

        for (pass = 0; pass < 2u; pass++) {                        /* 0: the union; 1: plain blending */
            target_clear();
            for (d = 0; d < k; d++) {
                float s[2], A[16];

                ao_slope(k, d, s);
                ao_slide_map(map, s, 0.0f, A);
                for (b = 0; b < BOXES; b++)
                    for (t = 0; t < 12u; t++) {
                        float xy[3][2], h[4];
                        int ok = 1;

                        for (i = 0; i < 3u; i++) { xform(A, box_v[b][box_t[t][i]], h); ok &= ao_px(h, 1.0f, 0.5f * (float)TS, xy[i]); }
                        CHECK(ok, "a box corner has no place in the target");
                        if (ok && ao_tri_ok(xy[0], xy[1], xy[2], (float)TS, 0)) target_tri(xy[0], xy[1], xy[2], ao_depth(d), alpha, !pass);
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

                    ao_slope(k, d, s);
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
                if (fabsf(un[i] - ao_level(k, blocked)) > 1e-5f) bad++;
            }
        CHECK(bad == 0, "k %u: %u texels of the union are not ao_level(k, directions blocked)", k, bad);
        CHECK(lighter == 0, "k %u: plain blending is lighter than the union at %u texels", k, lighter);
        CHECK(darker > 0, "k %u: plain blending is nowhere darker: the boxes do not overlap themselves?", k);
        CHECK(levels[0] > 0 && levels[1] > 0, "k %u: the scene has lit and blocked texels", k);
        printf("  k %u: texels by directions blocked:", k);
        for (i = 0; i <= k; i++) printf(" %u", levels[i]);
        printf("; plain blending darker at %u\n", darker);
    }
}

#ifdef AO_TEST_TOWN
static psm_scene_t scene;
static psm_frame_t frame;

static void t_town(void) {
    psm_sink_t sink;
    unsigned b, i, k, sent;

    boxes_make();
    memset(&scene, 0, sizeof scene);
    memset(&frame, 0, sizeof frame);
    for (b = 0; b < BOXES; b++) {
        scene.co[b].v0 = b * 8u; scene.co[b].vn = 8u; scene.co[b].t0 = b * 12u; scene.co[b].tn = 12u;
        for (i = 0; i < 8u; i++) memcpy(scene.cv[b * 8u + i], box_v[b][i], sizeof scene.cv[0]);
        for (i = 0; i < 12u; i++) { scene.ct[b * 12u + i][0] = (uint16_t)(b * 8u + box_t[i][0]); scene.ct[b * 12u + i][1] = (uint16_t)(b * 8u + box_t[i][1]); scene.ct[b * 12u + i][2] = (uint16_t)(b * 8u + box_t[i][2]); }
        frame.con[b] = 1u;
    }
    scene.con = BOXES; scene.cvn = BOXES * 8u; scene.ctn = BOXES * 12u;
    frame.built = 1;
    frame.map.m[0] = 0.125f; frame.map.m[9] = 0.125f; frame.map.m[15] = 1.0f;
    for (k = 0; k <= 8u; k = k ? k * 2u : 1u) {
        psm_sink_reset(&sink, 0, 1, 0u, NULL);
        sent = psm_ao2_send(&sink, &frame, &scene, 256u, k);
        CHECK(sent % 3u == 0u && sent <= k * BOXES * 36u && (k == 0u ? sent == 0u : sent >= k * BOXES * 12u), "town k %u: %u vertices", k, sent);
        CHECK(sink.bad_short == 0u && sink.bad_open == 0u && sink.bad_coord == 0u, "town k %u: sink faults %u %u %u", k, sink.bad_short, sink.bad_open, sink.bad_coord);
        CHECK(sink.hdrs == (k ? 1u : 0u), "town k %u: %u headers", k, sink.hdrs);
        printf("  town k %u: %u vertices, %u bytes of TA (estimate)\n", k, sent, (unsigned)sink.bytes);
    }
    frame.con[0] = 0u;
    psm_sink_reset(&sink, 0, 1, 0u, NULL);
    i = psm_ao2_send(&sink, &frame, &scene, 256u, 1u);
    frame.con[0] = 1u; frame.built = 0;
    psm_sink_reset(&sink, 0, 1, 0u, NULL);
    CHECK(i > 0u && i <= (BOXES - 1u) * 36u, "an object off the map is left out: %u", i);
    CHECK(psm_ao2_send(&sink, &frame, &scene, 256u, 4u) == 0u, "no map, nothing sent");
}
#endif

int main(void) {
    t_tables();
    t_slide();
    t_occlusion();
    t_target();
#ifdef AO_TEST_TOWN
    t_town();
#endif
    if (fails) { printf("ao_layer_test: %d FAILED\n", fails); return 1; }
    printf("ao_layer_test: all passed\n");
    return 0;
}
