/* SH4DER-PVR

   test/spark_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the spark pool and its two shapes: the default numbers; a
   billboard is a square of the asked size facing the camera, its spin turns it,
   one nearer than the near plane is not drawn; a streak faces the camera about
   its own axis with the asked width, keeps the least width in pixels far away,
   is cut at the near plane and still drawn (no corner nearer than it), is
   refused by the area guard when seen end on, RETREAT moves the tail
   to the head, TAIL0 blacks the tail; the brightness falls in a line to the
   end of the life and the slot is then free; a velocity moves a streak whole
   and gravity bends it; the pool rule (sparks die first, a KEEP quad is never
   lost to a plain one, a plain one is dropped from a pool of KEEP); the atlas
   cells with their inset; bad arguments are dropped and counted; every quad
   the shapes give passes the area guard or is refused by it, never a NaN.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_spark.h"
#include "../include/sh4der_guard.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR_(a, b, e) (fabsf((a) - (b)) <= (e))

static sh4der_spark_view_t view_default(void) {
    sh4der_spark_view_t v;

    memset(&v, 0, sizeof(v));
    v.right[0] = 1.0f; v.up[1] = 1.0f; v.fwd[2] = 1.0f;          /* camera at the origin, looking down +z */
    v.near_z = 1.0f; v.focal_px = 320.0f; v.min_px = SH4DER_SPARK_MIN_PX; v.inset = 0.0f;
    v.cols = 2; v.rows = 2;
    return v;
}

static float dist(const sh4der_spark_vtx_t *a, const sh4der_spark_vtx_t *b) {
    return sqrtf((a->x - b->x) * (a->x - b->x) + (a->y - b->y) * (a->y - b->y) + (a->z - b->z) * (a->z - b->z));
}

/* The draw's rule: both triangles of the quad have screen area (sh4der_guard.h). */
static int guard_ok(const sh4der_spark_vtx_t q[4], float near_z) {
    float p[4][3];
    int k;

    for (k = 0; k < 4; k++) { p[k][0] = q[k].x; p[k][1] = q[k].y; p[k][2] = q[k].z; }
    return sh4der_tri_area_ok(p[0], p[1], p[2], near_z, SH4DER_GUARD_MIN_AREA2) && sh4der_tri_area_ok(p[1], p[2], p[3], near_z, SH4DER_GUARD_MIN_AREA2);
}

static void defaults(void) {
    CHECK(SH4DER_SPARK_MAX == 24);
    CHECK(SH4DER_SPARK_MIN_PX == 0.8f);
    CHECK(SH4DER_SPARK_INSET == 0.5f / 64.0f);
    CHECK(SH4DER_SPARK_LIFE_MAX == 60.0f);
    CHECK(sizeof(sh4der_spark_t) == 52);
    CHECK(sizeof(sh4der_spark_pool_t) == 52 * SH4DER_SPARK_MAX + 8);
}

static void billboard(void) {
    sh4der_spark_pool_t pool;
    sh4der_spark_view_t vw = view_default();
    sh4der_spark_vtx_t q[4];
    const float c[3] = { 3.0f, -2.0f, 50.0f }, behind[3] = { 0.0f, 0.0f, 0.5f };
    int k;

    sh4der_spark_reset(&pool);
    CHECK(sh4der_spark_billboard(&pool, c, 7.0f, 0.0f, 0x00FFC878u, 0.1f, 3, SH4DER_SPARK_KEEP) == 1 && pool.live == 1);
    CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 1);
    for (k = 0; k < 4; k++) CHECK(q[k].z == 50.0f && q[k].argb == 0xFFFFC878u);
    CHECK(NEAR_(dist(&q[0], &q[1]), 14.0f, 1e-4f) && NEAR_(dist(&q[0], &q[2]), 14.0f, 1e-4f) && NEAR_(dist(&q[0], &q[3]), 14.0f * 1.41421356f, 1e-3f));
    CHECK(NEAR_((q[0].x + q[3].x) * 0.5f, 3.0f, 1e-4f) && NEAR_((q[0].y + q[3].y) * 0.5f, -2.0f, 1e-4f));
    CHECK(q[0].u == 0.5f && q[0].v == 0.5f && q[3].u == 1.0f && q[3].v == 1.0f && q[1].u == 0.5f && q[1].v == 1.0f);   /* cell 3 of 2 x 2 */
    CHECK(NEAR_(q[0].x, -4.0f, 1e-4f) && NEAR_(q[0].y, 5.0f, 1e-4f));                 /* no spin: axis aligned, 0 = left top */
    /* The test writes the spark's spin angle (radians) straight into the pool slot. q[1] is the spin field of a billboard; no public
       call sets it after the spark is made, because a spin is fixed when the spark is created. Writing it here lets the test check the
       rotated corners without adding a setter just for the test. */
    pool.s[0].q[1] = 0.78539816f;                                                     /* an eighth of a turn (pi / 4): a diamond */
    CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 1);
    CHECK(NEAR_(dist(&q[0], &q[1]), 14.0f, 1e-3f) && NEAR_(q[0].x, 3.0f - 7.0f * 1.41421356f, 2e-3f) && NEAR_(q[0].y, -2.0f, 2e-3f));
    vw.inset = SH4DER_SPARK_INSET;
    CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 1 && NEAR_(q[0].u, 0.5f + SH4DER_SPARK_INSET, 1e-6f) && NEAR_(q[3].u, 1.0f - SH4DER_SPARK_INSET, 1e-6f));
    sh4der_spark_reset(&pool);
    CHECK(sh4der_spark_billboard(&pool, behind, 1.0f, 0.0f, 0xFFFFFFu, 0.1f, 0, 0) == 1);
    CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 0);                                 /* nearer than the near plane */
    /* a camera that is not at the origin, turned a quarter to the right: the world +x axis is its depth */
    vw = view_default();
    vw.right[0] = 0.0f; vw.right[2] = -1.0f; vw.fwd[0] = 1.0f; vw.fwd[2] = 0.0f; vw.eye[0] = -10.0f;
    {
        const float w[3] = { 30.0f, 4.0f, -5.0f };

        sh4der_spark_reset(&pool);
        sh4der_spark_billboard(&pool, w, 1.0f, 0.0f, 0xFFFFFFu, 0.1f, 0, 0);
        CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 1 && NEAR_(q[0].z, 40.0f, 1e-4f));
        CHECK(NEAR_((q[0].x + q[3].x) * 0.5f, 5.0f, 1e-4f) && NEAR_((q[0].y + q[3].y) * 0.5f, 4.0f, 1e-4f));
    }
}

static void streak(void) {
    sh4der_spark_pool_t pool;
    sh4der_spark_view_t vw = view_default();
    sh4der_spark_vtx_t q[4];
    const float tail[3] = { -10.0f, 0.0f, 40.0f }, head[3] = { 10.0f, 0.0f, 40.0f };
    const float far_t[3] = { -100.0f, 0.0f, 4000.0f }, far_h[3] = { 100.0f, 0.0f, 4000.0f };
    const float in_t[3] = { 0.0f, 0.0f, -5.0f }, in_h[3] = { 0.0f, 3.0f, 9.0f };
    const float on_t[3] = { 0.0f, 0.0f, 10.0f }, on_h[3] = { 0.0f, 0.0f, 60.0f };
    int k;

    sh4der_spark_reset(&pool);
    CHECK(sh4der_spark_streak(&pool, tail, head, NULL, 2.0f, 0x808080u, 1.0f, 2, SH4DER_SPARK_TAIL0) == 1);
    CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 1);
    CHECK(NEAR_(dist(&q[0], &q[1]), 2.0f, 1e-4f) && NEAR_(dist(&q[2], &q[3]), 2.0f, 1e-4f) && NEAR_(dist(&q[0], &q[2]), 20.0f, 1e-4f));
    CHECK(NEAR_(q[0].x, -10.0f, 1e-4f) && NEAR_(q[2].x, 10.0f, 1e-4f) && NEAR_(fabsf(q[0].y), 1.0f, 1e-4f) && NEAR_(q[0].z, 40.0f, 1e-3f));   /* across = screen y: it faces the camera */
    CHECK(q[0].argb == 0xFF000000u && q[1].argb == 0xFF000000u && q[2].argb == 0xFF808080u && q[3].argb == 0xFF808080u);   /* TAIL0 */
    CHECK(q[0].u == 0.0f && q[2].u == 0.5f && q[0].v == 0.5f && q[1].v == 1.0f);       /* cell 2: u along, v across */
    /* far away: 2 units at depth 4,000 would be 0.16 px; the floor keeps 0.8 px */
    sh4der_spark_reset(&pool);
    sh4der_spark_streak(&pool, far_t, far_h, NULL, 2.0f, 0xFFFFFFu, 1.0f, 0, 0);
    CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 1);
    CHECK(NEAR_(dist(&q[0], &q[1]) * 320.0f / 4000.0f, 0.8f, 2e-3f));
    /* crossing the near plane: cut there, nothing behind it */
    sh4der_spark_reset(&pool);
    sh4der_spark_streak(&pool, in_t, in_h, NULL, 0.5f, 0xFFFFFFu, 1.0f, 0, 0);
    CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 1);
    for (k = 0; k < 4; k++) CHECK(q[k].z >= vw.near_z && sh4der_finite_(q[k].x) && sh4der_finite_(q[k].y));   /* no corner nearer than the near plane */
    CHECK(q[0].z == vw.near_z && q[1].z == vw.near_z);                                 /* the cut end sits on it */
    CHECK(NEAR_((q[0].y + q[1].y) * 0.5f, 3.0f * 6.0f / 14.0f, 1e-3f));               /* the cut point is on the line */
    CHECK(NEAR_(dist(&q[0], &q[1]), 0.5f, 1e-3f));                                     /* still the asked width */
    CHECK(guard_ok(q, vw.near_z));                                                     /* cut, not dropped: the quad is drawn */
    /* the same the other way round (the head is behind the camera) */
    sh4der_spark_reset(&pool);
    sh4der_spark_streak(&pool, in_h, in_t, NULL, 0.5f, 0xFFFFFFu, 1.0f, 0, 0);
    CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 1 && q[2].z == vw.near_z && q[3].z == vw.near_z && guard_ok(q, vw.near_z));
    /* a streak that dives at the camera off axis and is cut: every corner stays in front, drawn */
    {
        const float dv_t[3] = { 4.0f, 2.0f, -30.0f }, dv_h[3] = { 5.0f, 3.0f, 40.0f };

        sh4der_spark_reset(&pool);
        sh4der_spark_streak(&pool, dv_t, dv_h, NULL, 3.0f, 0xFFFFFFu, 1.0f, 0, 0);
        CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 1);
        for (k = 0; k < 4; k++) CHECK(q[k].z >= vw.near_z);
        CHECK(guard_ok(q, vw.near_z));
    }
    sh4der_spark_reset(&pool);
    sh4der_spark_streak(&pool, in_t, in_t, NULL, 0.5f, 0xFFFFFFu, 1.0f, 0, 0);
    CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 0);                                 /* wholly behind */
    /* seen end on (along the view axis through the eye): a finite quad, and the guard drops it (no area) */
    sh4der_spark_reset(&pool);
    sh4der_spark_streak(&pool, on_t, on_h, NULL, 1.0f, 0xFFFFFFu, 1.0f, 0, 0);
    CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 1);
    for (k = 0; k < 4; k++) CHECK(sh4der_finite_(q[k].x) && sh4der_finite_(q[k].y) && sh4der_finite_(q[k].z));
    CHECK(!guard_ok(q, vw.near_z));                                                    /* no screen area: the draw's guard refuses it */
    /* RETREAT: at half its life the tail is half way to the head */
    sh4der_spark_reset(&pool);
    sh4der_spark_streak(&pool, tail, head, NULL, 2.0f, 0xFFFFFFu, 1.0f, 0, SH4DER_SPARK_RETREAT | SH4DER_SPARK_KEEP);
    sh4der_spark_update(&pool, 0.5f, NULL);
    CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 1);
    CHECK(NEAR_((q[0].x + q[1].x) * 0.5f, 0.0f, 1e-3f) && NEAR_((q[2].x + q[3].x) * 0.5f, 10.0f, 1e-3f));
}

static void life_and_motion(void) {
    sh4der_spark_pool_t pool;
    sh4der_spark_view_t vw = view_default();
    sh4der_spark_vtx_t q[4];
    const float c[3] = { 0.0f, 0.0f, 20.0f }, vel[3] = { 30.0f, 0.0f, 0.0f }, g[3] = { 0.0f, -100.0f, 0.0f };
    const float tail[3] = { 0.0f, 0.0f, 20.0f }, head[3] = { 1.0f, 0.0f, 20.0f };
    int i;

    sh4der_spark_reset(&pool);
    sh4der_spark_billboard(&pool, c, 1.0f, 0.0f, 0x00FF8040u, 0.2f, 0, 0);
    sh4der_spark_update(&pool, 0.1f, NULL);                                            /* half its life */
    CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 1 && q[0].argb == 0xFF7F4020u);
    sh4der_spark_update(&pool, 0.05f, NULL);
    CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 1 && q[0].argb == 0xFF3F2010u);     /* a quarter left */
    sh4der_spark_update(&pool, 0.05f, NULL);
    CHECK(pool.live == 0 && pool.s[0].shape == SH4DER_SPARK_FREE && sh4der_spark_quad(&pool.s[0], &vw, q) == 0);
    /* a 2 frame flash at 30 Hz: drawn twice (full, then half), gone on the third frame */
    sh4der_spark_billboard(&pool, c, 1.0f, 0.0f, 0x00FFFFFFu, 2.0f / 30.0f, 0, 0);
    CHECK(sh4der_spark_quad(&pool.s[0], &vw, q) == 1 && q[0].argb == 0xFFFFFFFFu);
    sh4der_spark_update(&pool, 1.0f / 30.0f, NULL);
    CHECK(pool.live == 1 && sh4der_spark_quad(&pool.s[0], &vw, q) == 1 && (q[0].argb & 0xFFu) >= 0x7Du && (q[0].argb & 0xFFu) <= 0x82u);
    sh4der_spark_update(&pool, 1.0f / 30.0f, NULL);
    CHECK(pool.live == 0);
    /* velocity moves both ends; gravity bends the path */
    sh4der_spark_streak(&pool, tail, head, vel, 0.5f, 0xFFFFFFu, 2.0f, 0, SH4DER_SPARK_GRAVITY);
    for (i = 0; i < 10; i++) sh4der_spark_update(&pool, 0.1f, g);
    CHECK(pool.live == 1 && NEAR_(pool.s[0].p[0], 31.0f, 1e-3f) && NEAR_(pool.s[0].q[0], 30.0f, 1e-3f));
    CHECK(NEAR_(pool.s[0].p[1], -55.0f, 1e-2f) && NEAR_(pool.s[0].q[1], -55.0f, 1e-2f));   /* sum of 10 steps of 0.1 s under 100: 55 */
    CHECK(NEAR_(pool.s[0].p[0] - pool.s[0].q[0], 1.0f, 1e-3f));                        /* its length is kept */
    sh4der_spark_update(&pool, 0.0f, g); sh4der_spark_update(&pool, -1.0f, g); sh4der_spark_update(&pool, NAN, g);
    CHECK(pool.live == 1 && pool.s[0].age_ms == 1000);                                 /* a bad dt changes nothing */
}

static void pool_rule(void) {
    sh4der_spark_pool_t pool;
    const float c[3] = { 0.0f, 0.0f, 20.0f }, d[3] = { 0.0f, 1.0f, 0.0f };
    unsigned i, keep = 0;

    sh4der_spark_reset(&pool);
    for (i = 0; i < SH4DER_SPARK_MAX; i++) CHECK(sh4der_spark_billboard(&pool, c, 1.0f, 0.0f, i, 1.0f + 0.01f * (float)i, 0, 0) == 1);
    CHECK(pool.live == SH4DER_SPARK_MAX && pool.dropped == 0);
    sh4der_spark_update(&pool, 0.5f, NULL);                       /* slot 0 has the shortest life: nearest its end */
    CHECK(sh4der_spark_billboard(&pool, c, 2.0f, 0.0f, 0xABCDEFu, 1.0f, 0, SH4DER_SPARK_KEEP) == 1);
    CHECK(pool.live == SH4DER_SPARK_MAX && pool.s[0].argb == 0xABCDEFu && pool.s[0].age_ms == 0 && pool.s[1].argb == 1u);
    CHECK(sh4der_spark_billboard(&pool, c, 2.0f, 0.0f, 0x123456u, 1.0f, 0, 0) == 1 && pool.s[1].argb == 0x123456u);   /* a plain one takes the next oldest plain */
    for (i = 0; i < SH4DER_SPARK_MAX; i++) sh4der_spark_billboard(&pool, c, 1.0f, 0.0f, 0x777777u, 1.0f, 0, SH4DER_SPARK_KEEP);
    for (i = 0; i < SH4DER_SPARK_MAX; i++) keep += (pool.s[i].flags & SH4DER_SPARK_KEEP) ? 1u : 0u;
    CHECK(keep == SH4DER_SPARK_MAX && pool.live == SH4DER_SPARK_MAX && pool.dropped == 0);   /* KEEP pushed every plain one out, then the oldest KEEP */
    CHECK(sh4der_spark_billboard(&pool, c, 1.0f, 0.0f, 0x999999u, 1.0f, 0, 0) == 0 && pool.dropped == 1);   /* a plain one cannot push a KEEP out */
    CHECK(sh4der_spark_burst(&pool, c, d, 4, 0.7f, 100.0f, 10.0f, 22.0f, 0.8f, 0xFFFFFFu, 0.2f, 3) == 0 && pool.dropped == 5);
    /* bad arguments */
    sh4der_spark_reset(&pool);
    {
        const float bad[3] = { 0.0f, NAN, 0.0f };

        CHECK(sh4der_spark_billboard(&pool, bad, 1.0f, 0.0f, 0, 1.0f, 0, 0) == 0);
        CHECK(sh4der_spark_billboard(&pool, c, 0.0f, 0.0f, 0, 1.0f, 0, 0) == 0);
        CHECK(sh4der_spark_billboard(&pool, c, 1.0f, 0.0f, 0, 0.0f, 0, 0) == 0);
        CHECK(sh4der_spark_billboard(&pool, c, 1.0f, 0.0f, 0, 61.0f, 0, 0) == 0);
        CHECK(sh4der_spark_billboard(&pool, c, 1.0f, INFINITY, 0, 1.0f, 0, 0) == 0);
        CHECK(sh4der_spark_streak(&pool, c, bad, NULL, 1.0f, 0, 1.0f, 0, 0) == 0);
        CHECK(sh4der_spark_streak(&pool, c, c, bad, 1.0f, 0, 1.0f, 0, 0) == 0);
        CHECK(sh4der_spark_streak(&pool, c, c, NULL, -1.0f, 0, 1.0f, 0, 0) == 0);
        CHECK(pool.live == 0 && pool.dropped == 8);
    }
}

/* A burst: n streaks inside the cone, the asked lengths, moving away; every quad of a busy pool over many frames is finite
   and either passes the area guard or is refused by it. */
static void burst_and_guard(void) {
    sh4der_spark_pool_t pool;
    sh4der_spark_view_t vw = view_default();
    const float at[3] = { 5.0f, -3.0f, 120.0f }, dir[3] = { 0.0f, 2.0f, 0.0f }, g[3] = { 0.0f, -300.0f, 0.0f };
    const float m0[3] = { 2.0f, -1.0f, 3.0f }, m1[3] = { 0.0f, 0.0f, 900.0f };
    unsigned i, drawn = 0, refused = 0;
    int f, k;

    sh4der_spark_reset(&pool);
    CHECK(sh4der_spark_burst(&pool, at, dir, 8, 0.7f, 200.0f, 10.0f, 22.0f, 0.8f, 0xFFC878u, 0.2f, 3) == 8 && pool.live == 8);
    for (i = 0; i < 8; i++) {
        const sh4der_spark_t *s = &pool.s[i];
        const float d[3] = { s->p[0] - s->q[0], s->p[1] - s->q[1], s->p[2] - s->q[2] };
        const float len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]), spd = sqrtf(s->v[0] * s->v[0] + s->v[1] * s->v[1] + s->v[2] * s->v[2]);

        CHECK(len >= 10.0f - 1e-3f && len <= 22.0f + 1e-3f && spd >= 100.0f - 1e-2f && spd <= 200.0f + 1e-2f);
        CHECK(d[1] / len >= cosf(0.7f) - 2e-3f);                   /* inside the cone about +y */
        CHECK(NEAR_(s->v[1] / spd, d[1] / len, 2e-3f) && (s->flags & SH4DER_SPARK_GRAVITY));
    }
    sh4der_spark_billboard(&pool, m0, 7.0f, 1.0f, 0xFFFFFFu, 0.066f, 0, SH4DER_SPARK_KEEP);
    sh4der_spark_streak(&pool, m0, m1, NULL, 1.2f, 0xA6A6A6u, 0.12f, 2, SH4DER_SPARK_KEEP | SH4DER_SPARK_RETREAT | SH4DER_SPARK_TAIL0);
    for (f = 0; f < 12; f++) {
        for (i = 0; i < SH4DER_SPARK_MAX; i++) {
            sh4der_spark_vtx_t q[4];
            float p[4][3];

            if (!sh4der_spark_quad(&pool.s[i], &vw, q)) continue;
            for (k = 0; k < 4; k++) {
                CHECK(sh4der_finite_(q[k].x) && sh4der_finite_(q[k].y) && sh4der_finite_(q[k].z) && (q[k].argb >> 24) == 0xFFu);
                CHECK(q[k].u >= 0.0f && q[k].u <= 1.0f && q[k].v >= 0.0f && q[k].v <= 1.0f);
                p[k][0] = q[k].x; p[k][1] = q[k].y; p[k][2] = q[k].z;
            }
            if (sh4der_tri_area_ok(p[0], p[1], p[2], vw.near_z, SH4DER_GUARD_MIN_AREA2) && sh4der_tri_area_ok(p[1], p[2], p[3], vw.near_z, SH4DER_GUARD_MIN_AREA2)) drawn++;
            else refused++;
        }
        sh4der_spark_update(&pool, 1.0f / 30.0f, g);
    }
    CHECK(drawn >= 8u * 5u + 2u + 3u && pool.live == 0);           /* sparks 6 frames, flash 2, tracer 4 */
    printf("spark_test: %u quads drawn, %u refused by the area guard\n", drawn, refused);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    defaults();
    billboard();
    streak();
    life_and_motion();
    pool_rule();
    burst_and_guard();
    if (fails) { fprintf(stderr, "spark_test: %d FAILED\n", fails); return 1; }
    printf("spark_test: ok\n");
    return 0;
}
