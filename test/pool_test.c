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

int main(void) {
    disc();
    wedge();
    if (fails) { fprintf(stderr, "pool_test: %d FAILED\n", fails); return 1; }
    puts("pool: PASS defaults, disc (circle, ellipse, alpha, colour, counts), wedge (arc, half angle, alpha), areas, bad arguments");
    return 0;
}
