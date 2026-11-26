/* SH4DER-PVR: soft edges for light, test

   test/softfade_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of the fade at surfaces the caller names: the default numbers; a plane's keep (0 on and behind it,
   1 from the fade distance on, a half at half the distance, never falling as the point moves away); two planes
   multiply (a corner); a fade distance of 0 is a step; a wall fades over its rectangle and not beside it, and
   its effect goes out over the fade distance past its edge; NEAR and FIT; the two mesh forms (float levels, and
   the alpha byte of a pool's vertices) with the count of lit vertices and nothing spent on a dark one; bad
   arguments change nothing. The keep is also set against a lamp's halo fade written out by hand (a point and a
   normal a plane, a reciprocal a vertex): the library's form gives the same light to within one level of 255.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_softfade.h"
#include "../include/sh4der_pool.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.00002f
#endif

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR_(a, b, e) (fabsf((a) - (b)) <= (e))

static const float FLOOR_P[3] = { 0.0f, 0.0f, 0.0f }, FLOOR_N[3] = { 0.0f, 1.0f, 0.0f };
static const float WALL_P[3] = { 5.0f, 0.0f, 0.0f }, WALL_N[3] = { -1.0f, 0.0f, 0.0f };      /* a wall at x = 5, lit side x < 5 */

/* the hand form: a point and a normal a plane, the reciprocal taken a vertex, the product of smooth steps */
static float by_hand(const float p[3], const float o[][3], const float n[][3], unsigned np, float dist) {
    float f = 1.0f;
    unsigned i;

    for (i = 0; i < np; i++) {
        float x = ((p[0] - o[i][0]) * n[i][0] + (p[1] - o[i][1]) * n[i][1] + (p[2] - o[i][2]) * n[i][2]) / dist;

        x = x < 0.0f ? 0.0f : x > 1.0f ? 1.0f : x;
        f *= x * x * (3.0f - 2.0f * x);
    }
    return f;
}

static void planes(void) {
    const float tol = 200.0f * SH4DER_TEST_TOL;
    const float o[2][3] = { { 0.0f, 0.0f, 0.0f }, { 5.0f, 0.0f, 0.0f } }, n[2][3] = { { 0.0f, 1.0f, 0.0f }, { -1.0f, 0.0f, 0.0f } };
    sh4der_softfade_t f, step;
    sh4der_softfade_plane_t pl[2];
    float p[3] = { 0.0f, 0.0f, 0.0f }, was = -1.0f, y, worst = 0.0f;
    unsigned i;

    CHECK(SH4DER_SOFTFADE_DIST == 0.30f && SH4DER_SOFTFADE_MAX == 8);
    CHECK(sh4der_softfade_make(&f, SH4DER_SOFTFADE_DIST) == 1 && f.dist == 0.30f && NEAR_(f.inv * 0.30f, 1.0f, tol));
    sh4der_softfade_plane(&pl[0], FLOOR_P, FLOOR_N);
    sh4der_softfade_plane(&pl[1], WALL_P, WALL_N);
    CHECK(pl[0].d == 0.0f && pl[1].d == -5.0f && pl[1].n[0] == -1.0f);

    for (y = -0.2f; y < 0.6f; y += 0.0037f) {                            /* one plane: up from under the floor */
        float k;

        p[1] = y;
        k = sh4der_softfade_point(&f, pl, 1u, NULL, 0u, p);
        CHECK(k >= 0.0f && k <= 1.0f && k >= was);
        if (y <= 0.0f) CHECK(k == 0.0f);
        if (y >= 0.3001f) CHECK(k == 1.0f);
        was = k;
    }
    p[1] = 0.15f;
    CHECK(NEAR_(sh4der_softfade_point(&f, pl, 1u, NULL, 0u, p), 0.5f, tol));           /* a half at half the distance */
    p[0] = 4.85f;                                                                      /* the corner: both at a half */
    CHECK(NEAR_(sh4der_softfade_point(&f, pl, 2u, NULL, 0u, p), 0.25f, tol));
    p[0] = 5.2f; p[1] = 1.0f;                                                          /* behind the wall */
    CHECK(sh4der_softfade_point(&f, pl, 2u, NULL, 0u, p) == 0.0f);
    p[0] = 2.0f;
    CHECK(sh4der_softfade_point(&f, pl, 2u, NULL, 0u, p) == 1.0f);                     /* clear of both */
    CHECK(sh4der_softfade_point(&f, NULL, 0u, NULL, 0u, p) == 1.0f);                   /* no surfaces */

    for (i = 0; i < 4000u; i++) {                                        /* against the hand form, as a level of 255 */
        const float q[3] = { 4.4f + 0.0002f * (float)i, 0.5f * (float)(i % 97u) * (1.0f / 97.0f) - 0.05f, (float)(i % 13u) };
        const float e = fabsf(sh4der_softfade_point(&f, pl, 2u, NULL, 0u, q) - by_hand(q, o, n, 2u, 0.30f)) * 255.0f;

        if (e > worst) worst = e;
    }
    printf("softfade: against the hand form (a point and a normal, a divide a vertex), 4000 points: worst %.4f of a level of 255\n", (double)worst);
    CHECK(worst < 1.0f);

    CHECK(sh4der_softfade_make(&step, 0.0f) == 0 && sh4der_softfade_make(NULL, 0.3f) == 0);   /* no distance: a step */
    p[0] = 0.0f; p[1] = 0.001f;
    CHECK(sh4der_softfade_point(&step, pl, 1u, NULL, 0u, p) == 1.0f);
    p[1] = -0.001f;
    CHECK(sh4der_softfade_point(&step, pl, 1u, NULL, 0u, p) == 0.0f);

    p[1] = 0.0f;                                                         /* bad arguments: no fade */
    CHECK(sh4der_softfade_point(&f, pl, SH4DER_SOFTFADE_MAX + 1u, NULL, 0u, p) == 1.0f);
    CHECK(sh4der_softfade_point(&f, NULL, 1u, NULL, 0u, p) == 1.0f && sh4der_softfade_point(NULL, pl, 1u, NULL, 0u, p) == 1.0f);
    CHECK(sh4der_softfade_point(&f, pl, 1u, NULL, 1u, p) == 1.0f && sh4der_softfade_point(&f, pl, 1u, NULL, 0u, NULL) == 1.0f);
}

static void walls(void) {
    const float tol = 200.0f * SH4DER_TEST_TOL;
    const float c[3] = { 5.0f, 1.0f, 10.0f }, u[3] = { 0.0f, 0.0f, 1.0f };       /* a barrier at x = 5: 4 m along z, 2 m high */
    sh4der_softfade_t f;
    sh4der_softfade_wall_t w, bad;
    float p[3] = { 4.85f, 1.0f, 10.0f }, was = -1.0f, z;

    CHECK(sh4der_softfade_make(&f, 0.30f) == 1);
    CHECK(sh4der_softfade_wall(&w, c, WALL_N, u, 2.0f, 1.0f) == 1);
    CHECK(NEAR_(fabsf(w.v[1]), 1.0f, tol) && NEAR_(w.v[0], 0.0f, tol) && NEAR_(w.v[2], 0.0f, tol) && w.p.d == -5.0f);
    memset(&bad, 0x55, sizeof bad);
    CHECK(sh4der_softfade_wall(&bad, c, WALL_N, u, 0.0f, 1.0f) == 0 && sh4der_softfade_wall(NULL, c, WALL_N, u, 2.0f, 1.0f) == 0);
    CHECK(sh4der_softfade_wall(&bad, c, NULL, u, 2.0f, 1.0f) == 0 && ((const uint8_t *)&bad)[0] == 0x55u);

    CHECK(NEAR_(sh4der_softfade_point(&f, NULL, 0u, &w, 1u, p), 0.5f, tol));     /* over the rectangle: as a plane */
    p[0] = 5.1f;
    CHECK(sh4der_softfade_point(&f, NULL, 0u, &w, 1u, p) == 0.0f);               /* behind it */
    p[2] = 20.0f;
    CHECK(sh4der_softfade_point(&f, NULL, 0u, &w, 1u, p) == 1.0f);               /* behind its plane, far along its line: no wall there */
    p[0] = 4.85f; p[2] = 10.0f; p[1] = 3.0f;
    CHECK(sh4der_softfade_point(&f, NULL, 0u, &w, 1u, p) == 1.0f);               /* above it */
    p[1] = 1.0f; p[0] = 5.0f;                                                    /* on the plane, walking off the end: 0 to 1, never falling */
    for (z = 11.5f; z < 13.0f; z += 0.0041f) {
        float k;

        p[2] = z;
        k = sh4der_softfade_point(&f, NULL, 0u, &w, 1u, p);
        CHECK(k >= 0.0f && k <= 1.0f && k >= was);
        if (z <= 12.0f) CHECK(k == 0.0f);
        if (z >= 12.3001f) CHECK(k == 1.0f);
        was = k;
    }
}

static void near_and_fit(void) {
    const float tol = 200.0f * SH4DER_TEST_TOL;
    const float c[3] = { 5.0f, 1.0f, 10.0f }, u[3] = { 0.0f, 0.0f, 1.0f };
    sh4der_softfade_t f;
    sh4der_softfade_plane_t fl;
    sh4der_softfade_wall_t w;
    float p[3] = { 0.0f, 2.0f, 10.0f };

    sh4der_softfade_make(&f, 0.30f);
    sh4der_softfade_plane(&fl, FLOOR_P, FLOOR_N);
    sh4der_softfade_wall(&w, c, WALL_N, u, 2.0f, 1.0f);

    CHECK(sh4der_softfade_near(&f, &fl, 1u, &w, 1u, p, 0.5f) == 0);              /* 2 m up, 5 m from the wall, radius 0.5 */
    CHECK(sh4der_softfade_fit(&f, &fl, 1u, &w, 1u, p, 0.5f) == 1.0f);
    p[1] = 0.7f;
    CHECK(sh4der_softfade_near(&f, &fl, 1u, &w, 1u, p, 0.5f) == 1);              /* the floor: 0.7 < 0.5 + 0.3 */
    CHECK(sh4der_softfade_fit(&f, &fl, 1u, &w, 1u, p, 0.5f) == 1.0f);            /* but the quad does not reach it yet */
    p[1] = 0.25f;
    CHECK(NEAR_(sh4der_softfade_fit(&f, &fl, 1u, &w, 1u, p, 0.5f), 0.5f, tol));  /* 0.25 m up: half the radius */
    p[1] = -0.1f;
    CHECK(sh4der_softfade_fit(&f, &fl, 1u, &w, 1u, p, 0.5f) == 0.0f && sh4der_softfade_near(&f, &fl, 1u, &w, 1u, p, 0.5f) == 1);
    p[1] = 3.0f; p[0] = 4.8f;                                                    /* 0.2 m off the wall's plane but 1 m above its top: radius 0.5 and the fade do not reach */
    CHECK(sh4der_softfade_near(&f, NULL, 0u, &w, 1u, p, 0.5f) == 0);
    p[1] = 1.0f;
    CHECK(sh4der_softfade_near(&f, NULL, 0u, &w, 1u, p, 0.5f) == 1);
    CHECK(NEAR_(sh4der_softfade_fit(&f, NULL, 0u, &w, 1u, p, 0.5f), 0.4f, tol)); /* 0.2 m off, radius 0.5 */
    p[2] = 30.0f;                                                                /* far along its line: no wall */
    CHECK(sh4der_softfade_near(&f, NULL, 0u, &w, 1u, p, 0.5f) == 0 && sh4der_softfade_fit(&f, NULL, 0u, &w, 1u, p, 0.5f) == 1.0f);

    CHECK(sh4der_softfade_near(&f, NULL, 1u, NULL, 0u, p, 0.5f) == 0 && sh4der_softfade_near(&f, &fl, 1u, NULL, 0u, NULL, 0.5f) == 0);
    CHECK(sh4der_softfade_fit(&f, &fl, 1u, NULL, 0u, p, 0.0f) == 1.0f && sh4der_softfade_fit(NULL, &fl, 1u, NULL, 0u, p, 0.5f) == 1.0f);
}

static void meshes(void) {
    const float xyz[5][4] = { { 0.0f, 1.0f, 0.0f, 9.0f }, { 0.0f, 0.15f, 0.0f, 9.0f }, { 0.0f, -0.5f, 0.0f, 9.0f }, { 0.0f, 0.15f, 0.0f, 9.0f }, { 0.0f, 0.0f, 0.0f, 9.0f } };
    const float tol = 200.0f * SH4DER_TEST_TOL;
    const float a[3] = { 3.5f, 0.03f, 0.0f }, dir[3] = { 1.0f, 0.0f, 0.0f }, side[3] = { 0.0f, 0.0f, 1.0f };
    float lv[5] = { 0.8f, 0.8f, 0.8f, 0.0f, 0.8f }, keep[5];
    sh4der_softfade_t f;
    sh4der_softfade_plane_t fl, wl;
    sh4der_pool_vtx_t p[SH4DER_POOL_BEAM_VTX_MAX], q[SH4DER_POOL_BEAM_VTX_MAX];
    unsigned k, n;

    sh4der_softfade_make(&f, 0.30f);
    sh4der_softfade_plane(&fl, FLOOR_P, FLOOR_N);
    sh4der_softfade_plane(&wl, WALL_P, WALL_N);

    CHECK(sh4der_softfade_levels(&f, &fl, 1u, NULL, 0u, &xyz[0][0], 4u, lv, 5u) == 2u);      /* float levels, a stride of 4 */
    CHECK(lv[0] == 0.8f && NEAR_(lv[1], 0.4f, tol) && lv[2] == 0.0f && lv[3] == 0.0f && lv[4] == 0.0f);
    memcpy(keep, lv, sizeof keep);
    CHECK(sh4der_softfade_levels(&f, &fl, 1u, NULL, 0u, &xyz[0][0], 2u, lv, 5u) == 5u);      /* a bad stride: nothing changed */
    CHECK(sh4der_softfade_levels(&f, &fl, 9u, NULL, 0u, &xyz[0][0], 4u, lv, 5u) == 5u && memcmp(keep, lv, sizeof keep) == 0);
    CHECK(sh4der_softfade_levels(&f, &fl, 1u, NULL, 0u, NULL, 4u, lv, 5u) == 5u && sh4der_softfade_levels(&f, &fl, 1u, NULL, 0u, &xyz[0][0], 4u, NULL, 5u) == 5u);

    /* a beam 3 m long from x = 3.5 toward a wall at x = 5: it runs through the wall. The apex (1.5 m off) keeps
       its alpha; every vertex at or past the wall has none; nothing moved; the colour is kept */
    n = sh4der_pool_beam(a, dir, side, 3.0f, 0.2f, 4u, 4u, 0x00FFF0C8u, 0.6f, NULL, NULL, p);
    CHECK(n == 21u);
    memcpy(q, p, sizeof q);
    CHECK(sh4der_softfade_argb(&f, &wl, 1u, NULL, 0u, &p[0].x, sizeof p[0], &p[0].argb, sizeof p[0], n) >= 1u);
    CHECK(p[0].argb == q[0].argb);
    for (k = 0; k < n; k++) {
        CHECK(p[k].x == q[k].x && p[k].y == q[k].y && p[k].z == q[k].z && (p[k].argb & 0x00FFFFFFu) == 0x00FFF0C8u);
        CHECK((p[k].argb >> 24) <= (q[k].argb >> 24));
        if (p[k].x >= 5.0f) CHECK((p[k].argb >> 24) == 0u);
        if (p[k].x <= 4.69f) CHECK(p[k].argb == q[k].argb);
    }
    CHECK((q[1u + 5u + 2u].argb >> 24) > 0u && (p[1u + 5u + 2u].argb >> 24) == 0u);          /* the second arc's middle: x = 5.0, lit before, dark now */
    memcpy(q, p, sizeof q);
    CHECK(sh4der_softfade_argb(&f, &wl, 1u, NULL, 0u, &p[0].x, 6u, &p[0].argb, sizeof p[0], n) == n);   /* bad strides: nothing changed */
    CHECK(sh4der_softfade_argb(&f, &wl, 1u, NULL, 0u, &p[0].x, sizeof p[0], &p[0].argb, 2u, n) == n);
    CHECK(sh4der_softfade_argb(&f, &wl, 1u, NULL, 0u, NULL, sizeof p[0], &p[0].argb, sizeof p[0], n) == n && memcmp(p, q, sizeof q) == 0);
}

int main(void) {
    planes();
    walls();
    near_and_fit();
    meshes();
    if (fails) { fprintf(stderr, "softfade_test: %d FAILED\n", fails); return 1; }
    puts("softfade: PASS defaults, plane (keep, corner, step), wall (rectangle, edge), near, fit, levels, argb (a beam into a wall), bad arguments");
    return 0;
}
