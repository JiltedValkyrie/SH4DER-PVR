/* SH4DER-PVR: light pool test

   test/pool_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of the light pools: the default numbers; a disc's centre and rim
   (count, radius, both axes, an ellipse, the plane it lies in), its alpha full
   at the centre and 0 on the rim, its colour kept; a wedge's apex and far arc
   (count, length, the half angle at both ends, symmetric about the direction),
   its alpha; every triangle of both fans has area (through the area guard);
   bad arguments write nothing.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_pool.h"
#include "../include/sh4der_guard.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.00002f
#endif

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR_(a, b, e) (fabsf((a) - (b)) <= (e))

static float dist(const sh4der_pool_vtx_t *a, const float c[3]) {
    return sqrtf((a->x - c[0]) * (a->x - c[0]) + (a->y - c[1]) * (a->y - c[1]) + (a->z - c[2]) * (a->z - c[2]));
}

/* A fan seen from straight above at height 20: every triangle has screen area. */
static int fan_ok(const sh4der_pool_vtx_t *p, unsigned n, int closed) {
    unsigned i;

    for (i = 0; i < n; i++) {
        const sh4der_pool_vtx_t *t[3] = { &p[0], &p[i + 1u], &p[closed ? (i + 1u) % n + 1u : i + 2u] };
        float v[3][3];
        int k;

        for (k = 0; k < 3; k++) { v[k][0] = t[k]->x; v[k][1] = t[k]->z; v[k][2] = 20.0f - t[k]->y; }
        if (!sh4der_tri_area_ok(v[0], v[1], v[2], 1.0f, SH4DER_GUARD_MIN_AREA2)) return 0;
    }
    return 1;
}

static void disc(void) {
    const float tol = 200.0f * SH4DER_TEST_TOL;
    const float c[3] = { 3.0f, 0.02f, -7.0f }, ux[3] = { 5.0f, 0.0f, 0.0f }, uz[3] = { 0.0f, 0.0f, 5.0f }, uw[3] = { 0.0f, 0.0f, 2.0f };
    sh4der_pool_vtx_t p[SH4DER_POOL_MAX_N + 2];
    unsigned k;

    CHECK(SH4DER_POOL_MAX_N == 16);
    CHECK(sizeof(sh4der_pool_vtx_t) == 16);
    CHECK(sh4der_pool_disc(c, ux, uz, 10u, 0x00FFC070u, 0.6f, p) == 11u);
    CHECK(p[0].x == c[0] && p[0].y == c[1] && p[0].z == c[2]);
    CHECK(p[0].argb == ((153u << 24) | 0x00FFC070u));                     /* 0.6 x 255 = 153 */
    for (k = 1; k <= 10u; k++) {
        CHECK(NEAR_(dist(&p[k], c), 5.0f, tol) && p[k].y == c[1]);        /* on the circle, in the plane */
        CHECK(p[k].argb == 0x00FFC070u);                                  /* the rim: alpha 0, the colour kept */
    }
    CHECK(NEAR_(p[1].x, c[0] + 5.0f, tol) && NEAR_(p[1].z, c[2], tol));    /* the first rim point is on the first axis */
    CHECK(fan_ok(p, 10u, 1));
    CHECK(sh4der_pool_disc(c, ux, uw, 8u, 0u, 2.0f, p) == 9u);             /* an ellipse; alpha clamped */
    CHECK((p[0].argb >> 24) == 255u);
    CHECK(NEAR_(p[1].x - c[0], 5.0f, tol) && NEAR_(p[3].z - c[2], 2.0f, tol) && NEAR_(p[5].x - c[0], -5.0f, tol) && NEAR_(p[7].z - c[2], -2.0f, tol));
    CHECK(sh4der_pool_disc(c, ux, uz, 3u, 0u, 1.0f, p) == 4u);
    CHECK(sh4der_pool_disc(c, ux, uz, SH4DER_POOL_MAX_N, 0u, 1.0f, p) == SH4DER_POOL_MAX_N + 1u);
    memset(p, 0x55, sizeof(p));
    CHECK(sh4der_pool_disc(c, ux, uz, 2u, 0u, 1.0f, p) == 0u);
    CHECK(sh4der_pool_disc(c, ux, uz, SH4DER_POOL_MAX_N + 1u, 0u, 1.0f, p) == 0u);
    CHECK(sh4der_pool_disc(NULL, ux, uz, 8u, 0u, 1.0f, p) == 0u);
    CHECK(sh4der_pool_disc(c, ux, uz, 8u, 0u, 1.0f, NULL) == 0u);
    CHECK(p[0].argb == 0x55555555u);                                      /* nothing written */
}

static void wedge(void) {
    const float tol = 200.0f * SH4DER_TEST_TOL;
    const float a[3] = { 1.0f, 0.03f, 2.0f }, dir[3] = { 0.0f, 0.0f, 1.0f }, side[3] = { 1.0f, 0.0f, 0.0f };
    const float half = 0.25f;
    sh4der_pool_vtx_t p[SH4DER_POOL_MAX_N + 2];
    unsigned k;

    CHECK(sh4der_pool_wedge(a, dir, side, 20.0f, half, 4u, 0x00FFF0C8u, 0.5f, p) == 6u);
    CHECK(p[0].x == a[0] && p[0].y == a[1] && p[0].z == a[2] && (p[0].argb >> 24) == 128u);
    for (k = 1; k <= 5u; k++) {
        CHECK(NEAR_(dist(&p[k], a), 20.0f, tol) && p[k].y == a[1] && p[k].argb == 0x00FFF0C8u);
    }
    CHECK(NEAR_(p[1].x - a[0], -20.0f * sinf(half), tol) && NEAR_(p[1].z - a[2], 20.0f * cosf(half), tol));   /* one edge */
    CHECK(NEAR_(p[5].x - a[0], 20.0f * sinf(half), tol) && NEAR_(p[5].z - a[2], 20.0f * cosf(half), tol));    /* the other */
    CHECK(NEAR_(p[3].x, a[0], tol) && NEAR_(p[3].z - a[2], 20.0f, tol));                                      /* the middle: straight ahead */
    CHECK(fan_ok(p, 4u, 0));
    CHECK(sh4der_pool_wedge(a, dir, side, 20.0f, half, 1u, 0u, 1.0f, p) == 3u);
    memset(p, 0x55, sizeof(p));
    CHECK(sh4der_pool_wedge(a, dir, side, 0.0f, half, 4u, 0u, 1.0f, p) == 0u);
    CHECK(sh4der_pool_wedge(a, dir, side, 20.0f, 0.0f, 4u, 0u, 1.0f, p) == 0u);
    CHECK(sh4der_pool_wedge(a, dir, side, 20.0f, 1.6f, 4u, 0u, 1.0f, p) == 0u);
    CHECK(sh4der_pool_wedge(a, dir, side, 20.0f, half, 0u, 0u, 1.0f, p) == 0u);
    CHECK(sh4der_pool_wedge(a, dir, side, 20.0f, half, SH4DER_POOL_MAX_N + 1u, 0u, 1.0f, p) == 0u);
    CHECK(sh4der_pool_wedge(a, NULL, side, 20.0f, half, 4u, 0u, 1.0f, p) == 0u);
    CHECK(p[0].argb == 0x55555555u);
}

/* The fade by distance: the street demo's lamp line, moved here. On the plain C path the bits are the old line's. */
static void reach(void) {
    const float tol = 200.0f * SH4DER_TEST_TOL;
    const float c[3] = { 0.0f, 0.03f, 55.0f }, ux[3] = { 7.5f, 0.0f, 0.0f }, uz[3] = { 0.0f, 0.0f, 7.5f }, eye[3] = { 0.0f, 0.0f, 0.0f };
    sh4der_pool_reach_t r, st, bad;
    sh4der_pool_vtx_t p[SH4DER_POOL_MAX_N + 2];
    uint32_t h = 2166136261u, h_old = 2166136261u;
    float d, was = 2.0f;
    unsigned k;

    CHECK(SH4DER_POOL_REACH == 60.0f && SH4DER_POOL_FADE_W == 10.0f);
    CHECK(sh4der_pool_reach_make(&r, SH4DER_POOL_REACH, SH4DER_POOL_FADE_W) == 1);
    for (d = 0.0f; d < 75.0f; d += 0.0371f) {
        const float f = sh4der_pool_reach_fade(&r, d), old = sh4der_sat_((60.0f - d) * (1.0f / 10.0f));   /* the demo's line */
        const uint8_t b = (uint8_t)(f * 255.0f + 0.5f), b_old = (uint8_t)(old * 255.0f + 0.5f);

        CHECK(f >= 0.0f && f <= 1.0f && f <= was);                        /* never rises with distance */
        CHECK(NEAR_(f, old, tol));
#ifdef SH4DER_CORE_NO_SH4ZAM
        CHECK(f == old);                                                  /* the same bits */
#endif
        CHECK(NEAR_(sh4der_pool_reach_fade2(&r, d * d), f, tol));
        h = (h ^ b) * 16777619u; h_old = (h_old ^ b_old) * 16777619u;
        was = f;
    }
#ifdef SH4DER_CORE_NO_SH4ZAM
    CHECK(h == h_old);
#endif
    printf("pool reach: alpha bytes over 0 .. 75 m, fnv1a32 %08x (the old line: %08x)\n", (unsigned)h, (unsigned)h_old);
    CHECK(sh4der_pool_reach_fade(&r, 0.0f) == 1.0f && sh4der_pool_reach_fade(&r, 50.0f) == 1.0f);
    CHECK(NEAR_(sh4der_pool_reach_fade(&r, 55.0f), 0.5f, tol) && sh4der_pool_reach_fade(&r, 60.0f) == 0.0f && sh4der_pool_reach_fade(&r, 500.0f) == 0.0f);
    CHECK(sh4der_pool_reach_fade2(&r, 3600.0f) == 0.0f && sh4der_pool_reach_fade2(&r, 1e12f) == 0.0f && sh4der_pool_reach_fade2(&r, 0.0f) == 1.0f);
    CHECK(sh4der_pool_reach_make(&st, 60.0f, 0.0f) == 1);                 /* no width: a step at the reach */
    CHECK(sh4der_pool_reach_fade(&st, 59.9f) == 1.0f && sh4der_pool_reach_fade(&st, 60.1f) == 0.0f);
    CHECK(sh4der_pool_reach_make(&bad, 0.0f, 10.0f) == 0 && sh4der_pool_reach_make(NULL, 60.0f, 10.0f) == 0);
    CHECK(sh4der_pool_reach_fade(&bad, 0.0f) == 0.0f && sh4der_pool_reach_fade2(&bad, 0.0f) == 0.0f);

    /* per vertex: a disc whose centre is 55 m off keeps half its alpha; the rim stays 0; the colour is kept */
    CHECK(sh4der_pool_disc(c, ux, uz, 10u, 0x00FFC070u, 0.6f, p) == 11u);
    CHECK(sh4der_pool_reach_verts(&r, eye, p, 11u) == 1u);
    CHECK((p[0].argb >> 24) >= 76u && (p[0].argb >> 24) <= 77u && (p[0].argb & 0x00FFFFFFu) == 0x00FFC070u);   /* 153 x 0.5 */
    for (k = 1; k <= 10u; k++) CHECK(p[k].argb == 0x00FFC070u);
    CHECK(sh4der_pool_disc(c, ux, uz, 10u, 0x00FFC070u, 0.6f, p) == 11u);
    p[0].z = 70.0f;
    CHECK(sh4der_pool_reach_verts(&r, eye, p, 11u) == 0u && (p[0].argb >> 24) == 0u);   /* past the reach: nothing to send */
    CHECK(sh4der_pool_reach_verts(NULL, eye, p, 11u) == 0u && sh4der_pool_reach_verts(&r, NULL, p, 11u) == 0u && sh4der_pool_reach_verts(&r, eye, NULL, 11u) == 0u);
}

/* The beam: a wedge with arcs along it; one row and no reach is the wedge. */
static void beam(void) {
    const float tol = 200.0f * SH4DER_TEST_TOL;
    const float a[3] = { 1.0f, 0.03f, 2.0f }, dir[3] = { 0.0f, 0.0f, 1.0f }, side[3] = { 1.0f, 0.0f, 0.0f };
    const float half = 0.25f, eye[3] = { 1.0f, 1.2f, -43.0f };            /* 45 m behind the apex: the beam runs away from the eye */
    static sh4der_pool_vtx_t p[SH4DER_POOL_BEAM_VTX_MAX + 1], w[SH4DER_POOL_MAX_N + 2], q[SH4DER_POOL_BEAM_VTX_MAX];
    static uint16_t idx[3 * (SH4DER_POOL_MAX_N + 2 * SH4DER_POOL_MAX_N * (SH4DER_POOL_BEAM_ROWS_MAX - 1)) + 3];
    sh4der_pool_reach_t r;
    unsigned k, row, t, nt;

    CHECK(SH4DER_POOL_BEAM_ROWS == 3 && SH4DER_POOL_BEAM_ROWS_MAX == 4 && SH4DER_POOL_BEAM_VTX_MAX == 69);
    CHECK(sh4der_pool_wedge(a, dir, side, 20.0f, half, 4u, 0x00FFF0C8u, 0.5f, w) == 6u);
    CHECK(sh4der_pool_beam(a, dir, side, 20.0f, half, 4u, 1u, 0x00FFF0C8u, 0.5f, NULL, NULL, p) == 6u);
    for (k = 0; k < 6u; k++) CHECK(p[k].x == w[k].x && p[k].y == w[k].y && p[k].z == w[k].z && p[k].argb == w[k].argb);

    CHECK(sh4der_pool_beam(a, dir, side, 30.0f, half, 4u, 3u, 0x00FFF0C8u, 0.6f, NULL, NULL, p) == 16u);
    CHECK((p[0].argb >> 24) == 153u);
    for (row = 1; row <= 3u; row++)
        for (k = 0; k <= 4u; k++) {
            const sh4der_pool_vtx_t *v = &p[1u + (row - 1u) * 5u + k];

            CHECK(NEAR_(dist(v, a), 10.0f * (float)row, 2.0f * tol) && v->y == a[1] && (v->argb & 0x00FFFFFFu) == 0x00FFF0C8u);
            CHECK((v->argb >> 24) == (row == 1u ? 102u : row == 2u ? 51u : 0u));          /* 153 x 2/3, x 1/3, 0 */
        }
    CHECK(NEAR_(p[1u + 2u].x, a[0], tol) && NEAR_(p[1u + 10u + 2u].z - a[2], 30.0f, tol));   /* the middle: straight ahead */
    nt = sh4der_pool_beam_tris(4u, 3u, idx);
    CHECK(nt == 4u + 2u * 4u * 2u);
    for (t = 0; t < nt; t++) {                                            /* in range, with area, all wound one way */
        float v[3][3];
        int j;

        CHECK(idx[3u * t] < 16u && idx[3u * t + 1u] < 16u && idx[3u * t + 2u] < 16u);
        for (j = 0; j < 3; j++) { const sh4der_pool_vtx_t *s = &p[idx[3u * t + (unsigned)j]]; v[j][0] = s->x; v[j][1] = s->z; v[j][2] = 20.0f - s->y; }
        CHECK(sh4der_tri_area_ok(v[0], v[1], v[2], 1.0f, SH4DER_GUARD_MIN_AREA2));
        CHECK((v[1][0] - v[0][0]) * (v[2][1] - v[0][1]) - (v[1][1] - v[0][1]) * (v[2][0] - v[0][0]) < 0.0f);
    }
    CHECK(sh4der_pool_beam_tris(SH4DER_POOL_MAX_N, SH4DER_POOL_BEAM_ROWS_MAX, idx) == 112u);
    CHECK(sh4der_pool_beam_tris(4u, 1u, idx) == 4u);

    /* by distance: the eye 45 m behind the apex, reach 60, width 10. The apex (45 m) is whole; the arc at 55 m keeps
       about half of its own alpha; a beam from end to end past the reach writes nothing */
    CHECK(sh4der_pool_reach_make(&r, 60.0f, 10.0f) == 1);
    CHECK(sh4der_pool_beam(a, dir, side, 30.0f, half, 4u, 3u, 0x00FFF0C8u, 0.6f, NULL, NULL, q) == 16u);
    CHECK(sh4der_pool_beam(a, dir, side, 30.0f, half, 4u, 3u, 0x00FFF0C8u, 0.6f, &r, eye, p) == 16u);
    CHECK(p[0].argb == q[0].argb);
    CHECK((p[1u + 2u].argb >> 24) >= 50u && (p[1u + 2u].argb >> 24) <= 52u);          /* 102 at 55 m: half */
    CHECK((p[1u + 5u + 2u].argb >> 24) == 0u && (p[1u + 10u + 2u].argb >> 24) == 0u); /* 65 m and 75 m: gone */
    for (k = 0; k < 16u; k++) CHECK(p[k].x == q[k].x && p[k].z == q[k].z && (p[k].argb >> 24) <= (q[k].argb >> 24));
    memset(p, 0x55, sizeof(p));
    {
        const float far_eye[3] = { 1.0f, 1.2f, -200.0f };

        CHECK(sh4der_pool_beam(a, dir, side, 30.0f, half, 4u, 3u, 0u, 0.6f, &r, far_eye, p) == 0u);
    }
    CHECK(sh4der_pool_beam(a, dir, side, 30.0f, half, 4u, 3u, 0u, 0.6f, &r, NULL, p) == 0u);       /* a reach needs the eye */
    CHECK(sh4der_pool_beam(a, dir, side, 30.0f, half, 4u, 0u, 0u, 0.6f, NULL, NULL, p) == 0u);
    CHECK(sh4der_pool_beam(a, dir, side, 30.0f, half, 4u, SH4DER_POOL_BEAM_ROWS_MAX + 1u, 0u, 0.6f, NULL, NULL, p) == 0u);
    CHECK(sh4der_pool_beam(a, dir, side, 30.0f, half, 0u, 3u, 0u, 0.6f, NULL, NULL, p) == 0u);
    CHECK(sh4der_pool_beam(a, dir, side, 0.0f, half, 4u, 3u, 0u, 0.6f, NULL, NULL, p) == 0u);
    CHECK(sh4der_pool_beam(NULL, dir, side, 30.0f, half, 4u, 3u, 0u, 0.6f, NULL, NULL, p) == 0u);
    CHECK(sh4der_pool_beam(a, dir, side, 30.0f, half, 4u, 3u, 0u, 0.6f, NULL, NULL, NULL) == 0u);
    CHECK(p[0].argb == 0x55555555u && p[15].argb == 0x55555555u);        /* nothing written */
    CHECK(sh4der_pool_beam_tris(4u, 3u, NULL) == 0u && sh4der_pool_beam_tris(0u, 3u, idx) == 0u && sh4der_pool_beam_tris(4u, 5u, idx) == 0u);
    CHECK(sh4der_pool_beam(a, dir, side, 30.0f, half, SH4DER_POOL_MAX_N, SH4DER_POOL_BEAM_ROWS_MAX, 0u, 1.0f, NULL, NULL, p) == SH4DER_POOL_BEAM_VTX_MAX);
    CHECK(p[SH4DER_POOL_BEAM_VTX_MAX].argb == 0x55555555u);              /* not one vertex past the most */
}

int main(void) {
    disc();
    wedge();
    reach();
    beam();
    if (fails) { fprintf(stderr, "pool_test: %d FAILED\n", fails); return 1; }
    puts("pool: PASS reach (the demo's lamp line, a step, squared distance, per vertex), beam (rows, the wedge at one row, triangles, by distance)");
    puts("pool: PASS defaults, disc (circle, ellipse, alpha, colour, counts), wedge (arc, half angle, alpha), areas, bad arguments");
    return 0;
}
