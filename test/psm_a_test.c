/* SH4DER-PVR: route A's pass, the maths and the accounting, test

   test/psm_a_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of include/sh4der_psm_a.h (the parts that need no KallistiOS): the fit over a square patch (held still: a
   patch that moves by less than a texel moves no texel, one that moves further moves whole texels); the accounting (four
   casters, 2,200 triangles, a caster's own cap, a refusal counts and costs nothing, nothing before a fit); a caster's
   colour by strength; a world triangle into the PVR's triangles (inside, outside, across the pass's origin); and, for a
   box caster over a ground patch, the whole route: the casters drawn into a size x size buffer the way the PVR would
   fill them (flat, pixel centres), each ground point looked up through its map coordinates, against the exact answer
   (a ray from the point toward the light against the box). Nothing here runs the PVR: the fill is this file's own. */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_psm_a.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

#define N SH4DER_PSM_A_SIZE
#define ROW 2u

static void light_dir(float elev_deg, float az_deg, float l[3]) {
    const float e = elev_deg * 0.0174532925f, a = az_deg * 0.0174532925f;

    l[0] = cosf(e) * sinf(a); l[1] = sinf(e); l[2] = cosf(e) * cosf(a);
}

/* An axis-aligned box as 12 triangles. */
static void make_box(const float lo[3], const float hi[3], float pos[8][3], uint16_t idx[12][3]) {
    static const uint16_t f[12][3] = { { 0, 1, 3 }, { 0, 3, 2 }, { 4, 6, 7 }, { 4, 7, 5 }, { 0, 4, 5 }, { 0, 5, 1 },
                                       { 2, 3, 7 }, { 2, 7, 6 }, { 0, 2, 6 }, { 0, 6, 4 }, { 1, 5, 7 }, { 1, 7, 3 } };
    unsigned i;

    for (i = 0; i < 8u; i++) { pos[i][0] = (i & 1u) ? hi[0] : lo[0]; pos[i][1] = (i & 2u) ? hi[1] : lo[1]; pos[i][2] = (i & 4u) ? hi[2] : lo[2]; }
    memcpy(idx, f, sizeof f);
}

/* The exact answer: does a ray from p toward the light meet the box? */
static int in_shadow(const float p[3], const float l[3], const float lo[3], const float hi[3]) {
    float t0 = 0.0f, t1 = 1.0e9f;
    unsigned k;

    for (k = 0; k < 3u; k++) {
        if (fabsf(l[k]) < 1.0e-9f) { if (p[k] < lo[k] || p[k] > hi[k]) return 0; continue; }
        {
            float a = (lo[k] - p[k]) / l[k], b = (hi[k] - p[k]) / l[k], t;

            if (a > b) { t = a; a = b; b = t; }
            if (a > t0) t0 = a;
            if (b < t1) t1 = b;
            if (t0 > t1) return 0;
        }
    }
    return 1;
}

/* The PVR's fill, flat, at pixel centres. map: 0 dark, 1 white, over rows y0 .. y0 + N of a taller buffer. */
static void fill_tri(unsigned char *buf, unsigned rows, const sh4der_psm_a_tri_t *t, float y0) {
    unsigned x, y;

    (void)rows;
    for (y = 0; y < N; y++)
        for (x = 0; x < N; x++) {
            const float px = (float)x + 0.5f, py = (float)y + 0.5f + y0;
            float w[3];
            unsigned k;
            int pos = 0, neg = 0;

            for (k = 0; k < 3u; k++) {
                const sh4der_psm_pvr_vtx_t *a = &t->v[k], *b = &t->v[(k + 1u) % 3u];

                w[k] = (b->x - a->x) * (py - a->y) - (b->y - a->y) * (px - a->x);
                if (w[k] > 0.0f) pos = 1;
                if (w[k] < 0.0f) neg = 1;
            }
            if (!(pos && neg)) buf[y * N + x] = 0u;
        }
}

int main(void) {
    static sh4der_psm_a_t a;
    static unsigned char map[N * N];
    float light[3], centre[3] = { 0.0f, 0.0f, 0.0f }, shadow[3] = { 0.4f, 0.4f, 0.5f };
    float lo[3] = { -0.25f, 0.0f, -0.25f }, hi[3] = { 0.25f, 1.8f, 0.25f }, pos[8][3];
    uint16_t idx[12][3];
    sh4der_psm_a_caster_t c;
    unsigned i, j, k;

    light_dir(64.0f, 30.0f, light);
    make_box(lo, hi, pos, idx);
    c.pos = (const float (*)[3])pos; c.idx = (const uint16_t (*)[3])idx; c.nt = 12u; c.strength = 1.0f; c.max_tris = 0u;

    /* the accounting: nothing before a fit; four casters; the triangle budget; a caster's own cap */
    sh4der_psm_a_init(&a, N, ROW);
    CHECK(a.y0 == 64.0f && a.size == N);
    sh4der_psm_a_begin_frame(&a, shadow);
    CHECK(!sh4der_psm_a_accept(&a, 12u, 0u) && a.refused == 1u && a.casters == 0u);                 /* no map yet */
    CHECK(sh4der_psm_a_fit(&a, light, centre, 4.0f, -0.1f, 0.3f) && a.built);
    CHECK(sh4der_psm_a_accept(&a, c.nt, 0u) && sh4der_psm_a_accept(&a, c.nt, c.nt) && a.casters == 2u && a.tris == 24u);
    CHECK(!sh4der_psm_a_accept(&a, 13u, 12u) && a.refused == 2u && a.casters == 2u && a.tris == 24u);   /* over its cap */
    CHECK(!sh4der_psm_a_accept(&a, 0u, 0u));
    CHECK(sh4der_psm_a_accept(&a, 1000u, 0u) && sh4der_psm_a_accept(&a, 1000u, 0u) && a.casters == 4u && a.tris == 2024u);
    CHECK(!sh4der_psm_a_accept(&a, 1u, 0u) && a.casters == 4u);                                      /* a fifth */
    sh4der_psm_a_begin_frame(&a, shadow);
    CHECK(a.casters == 0u && a.tris == 0u && a.refused == 0u && !a.built);
    sh4der_psm_a_fit(&a, light, centre, 4.0f, -0.1f, 0.3f);
    CHECK(sh4der_psm_a_accept(&a, 2200u, 0u) && !sh4der_psm_a_accept(&a, 1u, 0u));                 /* the triangle budget */
    CHECK(!sh4der_psm_a_fit(&a, light, centre, 0.0f, 0.0f, 1.0f) && !a.built);                        /* bad inputs */
    CHECK(!sh4der_psm_a_fit(&a, light, centre, 4.0f, 1.0f, 0.0f));
    CHECK(!sh4der_psm_a_fit(&a, NULL, centre, 4.0f, 0.0f, 1.0f) && !sh4der_psm_a_fit(&a, light, NULL, 4.0f, 0.0f, 1.0f));

    /* a caster's colour by strength: white at 0, the shadow level at 1, the middle between */
    {
        const uint32_t w = sh4der_psm_a_argb(&a, 0.0f), d = sh4der_psm_a_argb(&a, 1.0f), m = sh4der_psm_a_argb(&a, 0.5f);

        CHECK(w == 0xffffffffu);
        CHECK(((d >> 16) & 255u) == 102u && ((d >> 8) & 255u) == 102u && (d & 255u) == 128u);          /* 0.4, 0.4, 0.5 */
        CHECK(((m >> 16) & 255u) == 179u && (m & 255u) == 191u);
        CHECK(sh4der_psm_a_argb(&a, -3.0f) == w && sh4der_psm_a_argb(&a, 9.0f) == d);
        CHECK(fabsf(sh4der_psm_a_shadow_level(0.3f, 0.7f) - 0.3f) < 1.0e-6f && sh4der_psm_a_shadow_level(0.0f, 0.0f) == 1.0f);
    }

    /* the held fit: a patch that moves by less than a texel moves no texel; further, whole texels */
    {
        static sh4der_psm_a_t h;
        float u0 = 0.0f, v0 = 0.0f, u1 = 0.0f, v1 = 0.0f, p[3] = { 1.0f, 0.0f, 0.7f }, c2[3];

        sh4der_psm_a_init(&h, N, ROW);
        sh4der_psm_a_begin_frame(&h, shadow);
        CHECK(sh4der_psm_a_fit(&h, light, centre, 4.0f, -0.1f, 0.3f) && sh4der_psm_a_texel_m(&h) > 0.0f);
        sh4der_psm_a_uv(&h, p, &u0, &v0);
        for (k = 0; k < 40u; k++) {
            c2[0] = 0.013f * (float)k; c2[1] = 0.0f; c2[2] = -0.011f * (float)k;
            sh4der_psm_a_begin_frame(&h, shadow);
            CHECK(sh4der_psm_a_fit(&h, light, c2, 4.0f, -0.1f, 0.3f));
            sh4der_psm_a_uv(&h, p, &u1, &v1);
            {
                const float du = (u1 - u0) * (float)N, dv = (v1 - v0) * (float)N;      /* the point's move, texels */

                CHECK(fabsf(du - floorf(du + 0.5f)) < 2.0e-3f && fabsf(dv - floorf(dv + 0.5f)) < 2.0e-3f);   /* whole texels */
            }
        }
        printf("  map texel %.4f m on a perpendicular to the light (8 m patch, sun 64 degrees, %u steps of the hold)\n", (double)sh4der_psm_a_texel_m(&h), h.hold.steps);
    }

    /* a world triangle into the PVR's: inside the map, with the pass's origin in y; outside; with no area; the order of the origin */
    {
        static sh4der_psm_a_t t;
        sh4der_psm_a_tri_t out[SH4DER_PSM_PVR_MAX_TRIS];
        const float in0[3] = { 0.0f, 0.0f, 0.0f }, in1[3] = { 0.5f, 0.0f, 0.0f }, in2[3] = { 0.0f, 0.5f, 0.0f };
        const float far0[3] = { 300.0f, 0.0f, 300.0f }, far1[3] = { 300.5f, 0.0f, 300.0f }, far2[3] = { 300.0f, 0.5f, 300.0f };
        const float line2[3] = { 1.0f, 0.0f, 0.0f };
        unsigned n;

        sh4der_psm_a_init(&t, N, ROW);
        sh4der_psm_a_begin_frame(&t, shadow);
        CHECK(sh4der_psm_a_tri(&t, in0, in1, in2, out) == 0u);                           /* not fitted: nothing */
        sh4der_psm_a_fit(&t, light, centre, 4.0f, -0.1f, 0.3f);
        n = sh4der_psm_a_tri(&t, in0, in1, in2, out);
        CHECK(n >= 1u && t.sent_tris == n);
        for (i = 0; i < n; i++) for (j = 0; j < 3u; j++) CHECK(out[i].v[j].x >= -0.01f && out[i].v[j].x <= (float)N + 0.01f
                                                               && out[i].v[j].y >= 64.0f - 0.01f && out[i].v[j].y <= 64.0f + (float)N + 0.01f && out[i].v[j].invw > 0.0f);
        CHECK(sh4der_psm_a_tri(&t, far0, far1, far2, out) == 0u && t.dropped == 1u);        /* outside the map */
        CHECK(sh4der_psm_a_tri(&t, in0, in1, line2, out) == 0u || 1);                        /* a thin one: any answer, no crash */
        CHECK(sh4der_psm_a_tri(&t, in0, in0, in0, out) == 0u);                              /* no area */
    }

    /* the whole route: a box's shadow on the ground, drawn into the buffer, each ground point looked up, against the exact answer */
    {
        static const float azs[3] = { 30.0f, 200.0f, 75.0f }, els[3] = { 64.0f, 40.0f, 80.0f };
        unsigned s;

        for (s = 0; s < 3u; s++) {
            unsigned total = 0, wrong = 0, dark = 0, covered = 0, found = 0;
            sh4der_psm_a_tri_t out[SH4DER_PSM_PVR_MAX_TRIS];
            float l2[3];

            light_dir(els[s], azs[s], l2);
            sh4der_psm_a_init(&a, N, ROW);
            sh4der_psm_a_begin_frame(&a, shadow);
            CHECK(sh4der_psm_a_fit(&a, l2, centre, 4.0f, -0.1f, 0.3f));
            memset(map, 255, sizeof map);
            CHECK(sh4der_psm_a_accept(&a, 12u, 0u));
            for (i = 0; i < 12u; i++) {
                const unsigned n = sh4der_psm_a_tri(&a, pos[idx[i][0]], pos[idx[i][1]], pos[idx[i][2]], out);

                for (j = 0; j < n; j++) fill_tri(map, 0u, &out[j], a.y0);                /* the buffer is the texture: the triangles carry the pass's origin */
            }
            for (i = 0; i < 80u; i++)
                for (j = 0; j < 80u; j++) {
                    const float p[3] = { -3.9f + 0.1f * (float)i, 0.0f, -3.9f + 0.1f * (float)j };
                    float u, v;
                    int lit, truth;

                    if (!sh4der_psm_a_uv(&a, p, &u, &v)) continue;
                    if (!sh4der_psm_a_covers(&a, p)) continue;
                    covered++;
                    {
                        const unsigned tx = (unsigned)(u * (float)N), ty = (unsigned)(v * (float)N);

                        lit = map[ty * N + tx] != 0u;
                    }
                    truth = !in_shadow(p, l2, lo, hi);
                    total++; dark += !truth; wrong += lit != truth; found += !lit && !truth;
                }
            printf("  sun %2.0f degrees up, azimuth %3.0f: %u of 6400 ground points in the map, %u in the box's shadow (%u of them dark in the map), %u read wrong (%.2f %%)\n",
                   (double)els[s], (double)azs[s], covered, dark, found, wrong, total ? 100.0 * (double)wrong / (double)total : 0.0);
            CHECK(found * 100u >= dark * 80u);                                              /* the shadow is really there: the rest is the edge, a texel wide */
            CHECK(covered == 6400u && dark > 40u);
            CHECK(wrong * 100u <= total * 1u);                                              /* the edges, a texel off: 1 % of the ground at most */
        }
    }

    if (fails) { fprintf(stderr, "psm_a_test: %d FAILED\n", fails); return 1; }
    printf("psm_a_test: all passed\n");
    return 0;
}
