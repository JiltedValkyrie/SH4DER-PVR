/* SH4DER-PVR: pl4nar test

   test/pl4nar_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of the planar reflection maths: the default numbers and the
   starting materials; a point mirrored about a plane; the view rows against a
   camera worked out by hand (the eye, the centre, right, up, the field of view)
   and the mirrored rows against the plain rows of the mirrored point; a view
   that has no direction refused; Fresnel's end points, its range and its
   equality with the water kernel; the exact planar case (a vertex in the plane
   looks up its own screen position, for any reach, and the flat call agrees);
   every lookup inside 0 .. 1 and every alpha inside 0 .. 255 over random
   vertices; a ray that ends behind the camera fading to the sky texel; the
   batch call against the single one through a stride; the flat triangle's
   split edge; the sphere against the plane; the half rate frames.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_pl4nar.h"
#include "../include/sh4der_water.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.00002f
#endif

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR_(a, b, e) (fabsf((a) - (b)) <= (e))

#define TW 256.0f
#define TH 128.0f
#define FOV 1.0f

static const sh4der_pl4nar_plane_t road = { { 0.0f, 1.0f, 0.0f }, 0.0f };
static const float eye0[3] = { 0.0f, 2.0f, 5.0f }, at0[3] = { 0.0f, 1.0f, 0.0f }, up0[3] = { 0.0f, 1.0f, 0.0f };

static unsigned rng_state = 12345u;
static float rnd(void) {                                    /* 0 .. 1 */
    rng_state = rng_state * 1664525u + 1013904223u;
    return (float)(rng_state >> 8) * (1.0f / 16777216.0f);
}

/* A world point through four rows, in plain C: the test's own ruler. */
static void through(const sh4der_pl4nar_row_t row[4], const float p[3], float out[4]) {
    int i;

    for (i = 0; i < 4; i++) out[i] = row[i].x * p[0] + row[i].y * p[1] + row[i].z * p[2] + row[i].w;
}

static void defaults(void) {
    sh4der_pl4nar_material_t m;

    CHECK(SH4DER_PL4NAR_F0_WATER == 0.0204f);
    CHECK(SH4DER_PL4NAR_F0_PAINT == 0.06f);
    CHECK(SH4DER_PL4NAR_F0_GLASS == 0.10f);
    CHECK(SH4DER_PL4NAR_F0_CHROME == 0.55f);
    CHECK(SH4DER_PL4NAR_F0_FLOOR == 0.05f);
    CHECK(SH4DER_PL4NAR_REACH == 18.0f);
    CHECK(SH4DER_PL4NAR_FADE_W == 4.5f);
    CHECK(SH4DER_PL4NAR_FOV_SCALE == 1.55f);
    CHECK(SH4DER_PL4NAR_SKY_U == 0.5f);
    CHECK(SH4DER_PL4NAR_SKY_V == 0.97f);
    CHECK(SH4DER_PL4NAR_GROUND_KEEP == 0.55f);
    CHECK(SH4DER_PL4NAR_W_MIN == 0.001f);
    CHECK(SH4DER_PL4NAR_SPLIT_RATIO == 1.25f);
    CHECK(sizeof(sh4der_pl4nar_material_t) == 24);
    CHECK(sizeof(sh4der_pl4nar_sample_t) == 16);
    CHECK(sizeof(sh4der_pl4nar_view_t) == 104 || sizeof(sh4der_pl4nar_view_t) == 112);   /* 112: a 16-byte aligned row */
    m = sh4der_pl4nar_preset(SH4DER_PL4NAR_PAINT);
    CHECK(m.f0 == 0.06f && m.gain == 1.0f && m.ground_keep == 0.55f && m.reach == 18.0f && m.tint == 0x00FFFFFFu && m.additive == 1u);
    m = sh4der_pl4nar_preset(SH4DER_PL4NAR_GLASS);
    CHECK(m.f0 == 0.10f && m.additive == 0u && m.reach == 18.0f);
    m = sh4der_pl4nar_preset(SH4DER_PL4NAR_CHROME);
    CHECK(m.f0 == 0.55f && m.additive == 0u);
    m = sh4der_pl4nar_preset(SH4DER_PL4NAR_WATER);
    CHECK(m.f0 == 0.0204f && m.gain == 1.0f && m.ground_keep == 1.0f && m.reach == 0.0f && m.additive == 0u);
    m = sh4der_pl4nar_preset(SH4DER_PL4NAR_WET_ROAD);
    CHECK(m.f0 == 0.0204f && m.gain == 0.7f && m.reach == 0.0f && m.additive == 0u);
    m = sh4der_pl4nar_preset(SH4DER_PL4NAR_FLOOR);
    CHECK(m.f0 == 0.05f && m.gain == 0.8f && m.reach == 0.0f && m.additive == 1u);
    m = sh4der_pl4nar_preset(99u);
    CHECK(m.f0 == 0.06f && m.additive == 1u);
}

static void mirror(void) {
    const sh4der_pl4nar_plane_t tilt = { { 0.6f, 0.8f, 0.0f }, 2.0f };
    const float on[3] = { 2.0f, 1.0f, 7.0f };               /* 0.6 x 2 + 0.8 x 1 = 2: in the plane */
    const float above[3] = { 2.0f + 0.6f * 3.0f, 1.0f + 0.8f * 3.0f, 7.0f };
    float o[3], back[3];

    sh4der_pl4nar_mirror_point(&tilt, on, o);
    CHECK(NEAR_(o[0], on[0], 1e-5f) && NEAR_(o[1], on[1], 1e-5f) && NEAR_(o[2], on[2], 1e-5f));
    sh4der_pl4nar_mirror_point(&tilt, above, o);
    CHECK(NEAR_(o[0], 2.0f - 0.6f * 3.0f, 1e-5f) && NEAR_(o[1], 1.0f - 0.8f * 3.0f, 1e-5f) && NEAR_(o[2], 7.0f, 1e-5f));
    sh4der_pl4nar_mirror_point(&tilt, o, back);
    CHECK(NEAR_(back[0], above[0], 1e-5f) && NEAR_(back[1], above[1], 1e-5f));
    sh4der_pl4nar_mirror_point(&tilt, o, o);                 /* in place */
    CHECK(NEAR_(o[0], above[0], 1e-5f) && NEAR_(o[1], above[1], 1e-5f));
    CHECK(sh4der_pl4nar_sphere_side(&road, above, 1.0f) == 1);
    CHECK(sh4der_pl4nar_sphere_side(&road, above, 5.0f) == 0);
    back[0] = 0.0f; back[1] = -3.0f; back[2] = 0.0f;
    CHECK(sh4der_pl4nar_sphere_side(&road, back, 1.0f) == -1);
    CHECK(sh4der_pl4nar_sphere_side(&road, back, 3.5f) == 0);
    CHECK(sh4der_pl4nar_due(0u, 0u) && sh4der_pl4nar_due(7u, 1u) && sh4der_pl4nar_due(4u, 2u) && !sh4der_pl4nar_due(5u, 2u));
}

static void rows(void) {
    const float eye[3] = { 0.0f, 0.0f, 0.0f }, at[3] = { 0.0f, 0.0f, -1.0f };
    const float tol = 64.0f * SH4DER_TEST_TOL;               /* pixels x depth */
    sh4der_pl4nar_row_t r[4], rm[4], keep[4];
    float p[3], o[4], om[4], q[3];
    int i;

    CHECK(sh4der_pl4nar_viewproj_rows(r, NULL, eye, at, up0, FOV, 0.1f, TW, TH) == 1);
    through(r, eye, o);                                      /* the eye: no direction, the near constant */
    CHECK(NEAR_(o[0], 0.0f, tol) && NEAR_(o[1], 0.0f, tol) && NEAR_(o[3], 0.0f, tol) && o[2] == 0.1f);
    p[0] = 0.0f; p[1] = 0.0f; p[2] = -10.0f; through(r, p, o);        /* straight ahead: the centre, depth 10 */
    CHECK(NEAR_(o[3], 10.0f, tol) && NEAR_(o[0] / o[3], TW * 0.5f, tol) && NEAR_(o[1] / o[3], TH * 0.5f, tol));
    p[0] = 1.0f; through(r, p, o);                            /* to the right: larger x */
    CHECK(o[0] / o[3] > TW * 0.5f + 1.0f && NEAR_(o[1] / o[3], TH * 0.5f, tol));
    p[0] = 0.0f; p[1] = 10.0f * tanf(FOV * 0.5f); through(r, p, o);   /* the top of the field: y = 0 (y down) */
    CHECK(NEAR_(o[1] / o[3], 0.0f, 4.0f * tol) && NEAR_(o[0] / o[3], TW * 0.5f, tol));
    p[1] = -p[1]; through(r, p, o);                           /* the bottom: y = h */
    CHECK(NEAR_(o[1] / o[3], TH, 4.0f * tol));
    p[1] = 0.0f; p[0] = 10.0f * tanf(FOV * 0.5f) * (TW / TH); through(r, p, o);   /* the right edge: square pixels */
    CHECK(NEAR_(o[0] / o[3], TW, 8.0f * tol));

    CHECK(sh4der_pl4nar_viewproj_rows(r, NULL, eye0, at0, up0, FOV, 0.1f, TW, TH) == 1);
    CHECK(sh4der_pl4nar_viewproj_rows(rm, &road, eye0, at0, up0, FOV, 0.1f, TW, TH) == 1);
    for (i = 0; i < 500; i++) {                              /* mirrored rows of p = plain rows of the mirrored p */
        p[0] = rnd() * 40.0f - 20.0f; p[1] = rnd() * 12.0f - 2.0f; p[2] = rnd() * 40.0f - 30.0f;
        sh4der_pl4nar_mirror_point(&road, p, q);
        through(rm, p, om); through(r, q, o);
        CHECK(NEAR_(om[0], o[0], 40.0f * tol) && NEAR_(om[1], o[1], 40.0f * tol) && NEAR_(om[3], o[3], tol) && om[2] == 0.1f);
    }
    {
        const sh4der_pl4nar_plane_t tilt = { { 0.6f, 0.8f, 0.0f }, -1.0f };

        CHECK(sh4der_pl4nar_viewproj_rows(rm, &tilt, eye0, at0, up0, FOV, 0.1f, TW, TH) == 1);
        for (i = 0; i < 200; i++) {
            p[0] = rnd() * 20.0f - 10.0f; p[1] = rnd() * 12.0f - 2.0f; p[2] = rnd() * 20.0f - 15.0f;
            sh4der_pl4nar_mirror_point(&tilt, p, q);
            through(rm, p, om); through(r, q, o);
            CHECK(NEAR_(om[0], o[0], 60.0f * tol) && NEAR_(om[1], o[1], 60.0f * tol) && NEAR_(om[3], o[3], 2.0f * tol));
        }
    }
    /* refused, the rows untouched */
    memcpy(keep, r, sizeof(keep));
    CHECK(sh4der_pl4nar_viewproj_rows(r, NULL, eye0, eye0, up0, FOV, 0.1f, TW, TH) == 0);        /* eye on target */
    p[0] = 0.0f; p[1] = 9.0f; p[2] = 5.0f;
    CHECK(sh4der_pl4nar_viewproj_rows(r, NULL, eye0, p, up0, FOV, 0.1f, TW, TH) == 0);           /* up along the view */
    CHECK(sh4der_pl4nar_viewproj_rows(r, NULL, eye0, at0, up0, 0.0f, 0.1f, TW, TH) == 0);
    CHECK(sh4der_pl4nar_viewproj_rows(r, NULL, eye0, at0, up0, 3.2f, 0.1f, TW, TH) == 0);
    CHECK(sh4der_pl4nar_viewproj_rows(r, NULL, eye0, at0, up0, FOV, 0.1f, 0.0f, TH) == 0);
    CHECK(sh4der_pl4nar_viewproj_rows(r, NULL, eye0, at0, up0, FOV, 0.1f, TW, -1.0f) == 0);
    CHECK(memcmp(keep, r, sizeof(keep)) == 0);
    {
        sh4der_pl4nar_view_t v;

        CHECK(sh4der_pl4nar_view_build(&v, &road, eye0, at0, up0, FOV, 0.1f, TW, TH, 0.0f) == 0);
        CHECK(sh4der_pl4nar_view_build(&v, NULL, eye0, at0, up0, FOV, 0.1f, TW, TH, 4.5f) == 0);
        CHECK(sh4der_pl4nar_view_build(NULL, &road, eye0, at0, up0, FOV, 0.1f, TW, TH, 4.5f) == 0);
        CHECK(sh4der_pl4nar_view_build(&v, &road, eye0, at0, up0, FOV, 0.1f, TW, TH, 4.5f) == 1);
        CHECK(NEAR_(v.inv_w, 1.0f / TW, 1e-6f) && NEAR_(v.inv_h, 1.0f / TH, 1e-6f) && NEAR_(v.inv_fade, 1.0f / 4.5f, 1e-4f));
        CHECK(v.eye[1] == 2.0f && v.plane_n[1] == 1.0f);
    }
}

static void fresnel(void) {
    int i;
    float last = 2.0f;

    CHECK(NEAR_(sh4der_pl4nar_fresnel(1.0f, 0.06f), 0.06f, 1e-5f));
    CHECK(NEAR_(sh4der_pl4nar_fresnel(0.0f, 0.06f), 1.0f, 1e-5f));
    CHECK(NEAR_(sh4der_pl4nar_fresnel(-0.5f, 0.06f), 1.0f, 1e-5f));          /* turned away: fully */
    CHECK(NEAR_(sh4der_pl4nar_fresnel(2.0f, 0.55f), 0.55f, 1e-5f));
    CHECK(NEAR_(sh4der_pl4nar_fresnel(0.5f, 0.10f), 0.10f + 0.90f * 0.03125f, 2e-5f));
    for (i = 0; i <= 100; i++) {
        const float x = (float)i * 0.01f, f = sh4der_pl4nar_fresnel(x, 0.14f);

        CHECK(NEAR_(sh4der_pl4nar_fresnel(x, SH4DER_PL4NAR_F0_WATER), sh4der_water_fresnel(x), 2e-5f));
        CHECK(f >= 0.14f - 1e-6f && f <= 1.0f + 1e-6f && f <= last + 1e-6f);
        last = f;
    }
}

static void planar(void) {
    const float tol = 40.0f * SH4DER_TEST_TOL;               /* texture units: 0.2 of a texel at the SH4ZAM tolerance */
    sh4der_pl4nar_view_t v;
    sh4der_pl4nar_row_t plain[4];
    sh4der_pl4nar_material_t m = sh4der_pl4nar_preset(SH4DER_PL4NAR_WATER);
    sh4der_pl4nar_sample_t s, f;
    float p[3], o[4], w;
    int i, seen = 0;

    CHECK(sh4der_pl4nar_viewproj_rows(plain, NULL, eye0, at0, up0, FOV, 0.1f, TW, TH) == 1);
    CHECK(sh4der_pl4nar_view_build(&v, &road, eye0, at0, up0, FOV, 0.1f, TW, TH, SH4DER_PL4NAR_FADE_W) == 1);
    for (i = 0; i < 400; i++) {
        p[0] = rnd() * 16.0f - 8.0f; p[1] = 0.0f; p[2] = rnd() * 30.0f - 30.0f;
        through(plain, p, o);
        if (o[3] < 5.0f || o[0] < 0.0f || o[0] > o[3] * TW || o[1] < 0.0f || o[1] > o[3] * TH) continue;   /* on screen, past the fade */
        seen++;
        m.reach = (i & 1) ? 18.0f : 3.0f;                    /* any reach */
        sh4der_pl4nar_begin(&v);
        sh4der_pl4nar_shade(&v, p, road.n, &m, &s);
        CHECK(NEAR_(s.u, o[0] / o[3] / TW, tol) && NEAR_(s.v, o[1] / o[3] / TH, tol));
        w = sh4der_pl4nar_flat(&v, p, &m, &f);
        CHECK(NEAR_(w, o[3], 4.0f * tol));
        CHECK(NEAR_(f.u, o[0] / o[3] / TW, tol) && NEAR_(f.v, o[1] / o[3] / TH, tol));
        CHECK(NEAR_(f.fresnel, s.fresnel, 1e-5f) && f.down == 0.0f);
        CHECK(s.down == 0.0f);                              /* off the plane the ray goes up */
    }
    CHECK(seen > 50);
    {   /* the flat batch = the single call, through a 16 B stride, with and without the depths */
        float pts[8][4], wd[8];
        sh4der_pl4nar_sample_t fb[8];

        for (i = 0; i < 8; i++) { pts[i][0] = (float)i - 4.0f; pts[i][1] = 0.0f; pts[i][2] = -3.0f * (float)i; pts[i][3] = 99.0f; }
        CHECK(sh4der_pl4nar_flat_n(&v, pts[0], sizeof(pts[0]), &m, fb, wd, 8u) == 8u);
        for (i = 0; i < 8; i++) {
            w = sh4der_pl4nar_flat(&v, pts[i], &m, &f);
            CHECK(NEAR_(w, wd[i], 1e-5f) && NEAR_(f.u, fb[i].u, 1e-6f) && NEAR_(f.v, fb[i].v, 1e-6f) && NEAR_(f.fresnel, fb[i].fresnel, 1e-6f));
        }
        CHECK(sh4der_pl4nar_flat_n(&v, pts[0], sizeof(pts[0]), &m, fb, NULL, 8u) == 8u);
        CHECK(sh4der_pl4nar_flat_n(&v, pts[0], 6u, &m, fb, wd, 8u) == 0u);
        CHECK(sh4der_pl4nar_flat_n(&v, NULL, sizeof(pts[0]), &m, fb, wd, 8u) == 0u);
    }
    p[0] = 0.0f; p[1] = 0.0f; p[2] = 30.0f;                   /* behind the camera */
    w = sh4der_pl4nar_flat(&v, p, &m, &f);
    CHECK(w < 0.0f && f.fresnel == 0.0f && f.u == SH4DER_PL4NAR_SKY_U && f.v == SH4DER_PL4NAR_SKY_V);
    CHECK(sh4der_pl4nar_alpha(&f, &m) == 0u);
}

static void body(void) {
    sh4der_pl4nar_view_t v;
    sh4der_pl4nar_material_t m = sh4der_pl4nar_preset(SH4DER_PL4NAR_PAINT);
    sh4der_pl4nar_sample_t s, batch[64];
    struct { float pos[3]; uint32_t colour; float n[3]; float pad; } vtx[64];      /* a client's vertex: 32 B */
    float p[3], n[3];
    unsigned a;
    int i;

    CHECK(sh4der_pl4nar_view_build(&v, &road, eye0, at0, up0, FOV * SH4DER_PL4NAR_FOV_SCALE, 0.1f, TW, TH, SH4DER_PL4NAR_FADE_W) == 1);
    for (i = 0; i < 4000; i++) {
        p[0] = rnd() * 6.0f - 3.0f; p[1] = rnd() * 1.5f; p[2] = rnd() * 6.0f - 3.0f;
        n[0] = rnd() * 2.0f - 1.0f; n[1] = rnd() * 2.0f - 1.0f; n[2] = rnd() * 2.0f - 1.0f;
        sh4der_norm3_(n);
        m.gain = rnd() * 3.0f; m.ground_keep = rnd(); m.f0 = rnd();
        sh4der_pl4nar_shade(&v, p, n, &m, &s);
        CHECK(s.u >= 0.0f && s.u <= 1.0f && s.v >= 0.0f && s.v <= 1.0f);
        CHECK(s.fresnel >= m.f0 - 1e-5f && s.fresnel <= 1.0f + 1e-5f && s.down >= 0.0f && s.down <= 1.0f);
        a = sh4der_pl4nar_alpha(&s, &m);
        CHECK(a <= 255u);
        m.tint = 0xAB123456u;
        CHECK(sh4der_pl4nar_argb(&s, &m) == ((a << 24) | 0x00123456u));
        if (i < 64) { memcpy(vtx[i].pos, p, sizeof(p)); memcpy(vtx[i].n, n, sizeof(n)); vtx[i].colour = 0u; vtx[i].pad = 0.0f; }
    }
    /* a ray that ends behind the camera: the vertex faces the eye, the ray comes back past it */
    m = sh4der_pl4nar_preset(SH4DER_PL4NAR_PAINT);
    p[0] = 0.0f; p[1] = 1.0f; p[2] = 0.0f;
    n[0] = eye0[0] - p[0]; n[1] = eye0[1] - p[1]; n[2] = eye0[2] - p[2];
    sh4der_norm3_(n);
    sh4der_pl4nar_shade(&v, p, n, &m, &s);
    CHECK(s.u == SH4DER_PL4NAR_SKY_U && s.v == SH4DER_PL4NAR_SKY_V);
    CHECK(NEAR_(s.fresnel, m.f0, 1e-4f));                    /* seen head on */
    CHECK(s.down == 0.0f);
    /* a ray straight down at the plane: ground_keep */
    n[0] = 0.0f; n[1] = -1.0f; n[2] = 0.0f;
    p[0] = eye0[0]; p[1] = 4.0f; p[2] = eye0[2];             /* straight above the eye, facing down */
    sh4der_pl4nar_shade(&v, p, n, &m, &s);
    CHECK(NEAR_(s.down, 1.0f, 1e-4f));
    s.fresnel = 1.0f; m.gain = 1.0f; m.ground_keep = 0.5f;
    CHECK(sh4der_pl4nar_alpha(&s, &m) == 128u);
    s.down = 0.0f;
    CHECK(sh4der_pl4nar_alpha(&s, &m) == 255u);
    m.gain = 0.0f;
    CHECK(sh4der_pl4nar_alpha(&s, &m) == 0u);

    /* the batch = the single call, through a 32 B stride */
    m = sh4der_pl4nar_preset(SH4DER_PL4NAR_GLASS);
    memset(batch, 0, sizeof(batch));
    CHECK(sh4der_pl4nar_shade_n(&v, vtx[0].pos, sizeof(vtx[0]), vtx[0].n, sizeof(vtx[0]), &m, batch, 64u) == 64u);
    for (i = 0; i < 64; i++) {
        sh4der_pl4nar_shade(&v, vtx[i].pos, vtx[i].n, &m, &s);
        CHECK(NEAR_(s.u, batch[i].u, 1e-6f) && NEAR_(s.v, batch[i].v, 1e-6f) && NEAR_(s.fresnel, batch[i].fresnel, 1e-6f) && NEAR_(s.down, batch[i].down, 1e-6f));
    }
    memset(batch, 0, sizeof(batch));
    CHECK(sh4der_pl4nar_shade_n(&v, vtx[0].pos, 30u, vtx[0].n, sizeof(vtx[0]), &m, batch, 64u) == 0u);      /* a stride off 4 */
    CHECK(sh4der_pl4nar_shade_n(&v, vtx[0].pos, sizeof(vtx[0]), vtx[0].n, 0u, &m, batch, 64u) == 0u);
    CHECK(sh4der_pl4nar_shade_n(&v, NULL, sizeof(vtx[0]), vtx[0].n, sizeof(vtx[0]), &m, batch, 64u) == 0u);
    CHECK(sh4der_pl4nar_shade_n(&v, vtx[0].pos, sizeof(vtx[0]), vtx[0].n, sizeof(vtx[0]), &m, batch, 0u) == 0u);
    CHECK(batch[0].u == 0.0f && batch[63].fresnel == 0.0f);                                                  /* nothing written */
}

static void split(void) {
    float w[3] = { 10.0f, 10.0f, 10.0f };

    CHECK(sh4der_pl4nar_tri_split(w, SH4DER_PL4NAR_SPLIT_RATIO) == -1);
    w[1] = 12.4f;
    CHECK(sh4der_pl4nar_tri_split(w, SH4DER_PL4NAR_SPLIT_RATIO) == -1);      /* 1.24: under the ratio */
    w[1] = 12.6f;
    CHECK(sh4der_pl4nar_tri_split(w, SH4DER_PL4NAR_SPLIT_RATIO) == 0);       /* edges 0 and 1 are the same ratio: the first */
    w[0] = 5.0f; w[1] = 10.0f; w[2] = 40.0f;
    CHECK(sh4der_pl4nar_tri_split(w, SH4DER_PL4NAR_SPLIT_RATIO) == 2);       /* 40 to 5 */
    w[0] = 40.0f; w[1] = 5.0f; w[2] = 10.0f;
    CHECK(sh4der_pl4nar_tri_split(w, SH4DER_PL4NAR_SPLIT_RATIO) == 0);
    w[0] = 10.0f; w[1] = 40.0f; w[2] = 5.0f;
    CHECK(sh4der_pl4nar_tri_split(w, SH4DER_PL4NAR_SPLIT_RATIO) == 1);
    CHECK(sh4der_pl4nar_tri_split(w, 10.0f) == -1);                          /* the caller's ratio */
    w[2] = 0.0f;
    CHECK(sh4der_pl4nar_tri_split(w, SH4DER_PL4NAR_SPLIT_RATIO) == -1);      /* at the camera: clip first */
    w[2] = -3.0f;
    CHECK(sh4der_pl4nar_tri_split(w, SH4DER_PL4NAR_SPLIT_RATIO) == -1);
    /* cut until done: a triangle from depth 2 to depth 40 ends, and no piece is over the ratio */
    {
        float stack[64][3], t[3];
        int top = 0, pieces = 0, guard = 0;

        stack[top][0] = 2.0f; stack[top][1] = 2.5f; stack[top][2] = 40.0f; top++;
        while (top > 0 && guard++ < 10000) {
            int e;

            top--; memcpy(t, stack[top], sizeof(t));
            e = sh4der_pl4nar_tri_split(t, SH4DER_PL4NAR_SPLIT_RATIO);
            if (e < 0) { pieces++; continue; }
            {
                const int a = e, b = (e + 1) % 3, c = (e + 2) % 3;
                const float mid = 0.5f * (t[a] + t[b]);      /* depth is straight along a world edge */

                CHECK(top + 2 <= 64);
                stack[top][0] = t[a]; stack[top][1] = mid; stack[top][2] = t[c]; top++;
                stack[top][0] = mid; stack[top][1] = t[b]; stack[top][2] = t[c]; top++;
            }
        }
        CHECK(top == 0 && pieces > 8 && pieces < 400);
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    defaults();
    mirror();
    rows();
    fresnel();
    planar();
    body();
    split();
    if (fails) { fprintf(stderr, "pl4nar_test: %d FAILED\n", fails); return 1; }
    puts("pl4nar: PASS defaults, materials, mirror, view rows, mirrored rows, refusals, Fresnel, the exact planar case, lookup and alpha ranges, lost rays, the batch, the split, the plane side");
    return 0;
}
