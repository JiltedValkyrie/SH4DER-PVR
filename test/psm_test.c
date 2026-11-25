/* SH4DER-PVR: perspective shadow map test

   test/psm_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of the shadow map placement: the depth numbers the chapter's prose gives
   (Q 1.0002 at 1 and 4000, Q 1.02 at a ratio of 50, half the unit cube in the first
   metre, the inverse projection's 0, 1/2 and 1); the camera rows; the light kinds by the
   sign of w; every fitted point inside the map; two points on one light ray on one
   texel, for both signs of w and both sides of the light; the slab clip; and, on a
   ground plane at four sun angles, the ground a texel covers against the ground a pixel
   covers, for the uniform map and the warped one (printed, and the warp's gain asserted
   where the chapter says it holds). Bad input returns 0.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_psm.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR_(a, b, e) (fabsf((a) - (b)) <= (e))

/* the warp's near plane: a little beyond the nearest ground the camera sees (2.3 m at 1.7 m high, pitched down 7 degrees) */
#define WARP_NEAR 4.0f

/* world: y up, the camera looks along -z from 1.7 m, pitched down a little; ground is y = 0 */
static sh4der_psm_cam_t make_cam(void) {
    sh4der_psm_cam_t c;
    const float pitch = 0.12f;

    memset(&c, 0, sizeof c);
    c.eye[0] = 0.0f; c.eye[1] = 1.7f; c.eye[2] = 0.0f;
    c.fwd[0] = 0.0f; c.fwd[1] = -sinf(pitch); c.fwd[2] = -cosf(pitch);
    c.up[0] = 0.0f; c.up[1] = cosf(pitch); c.up[2] = -sinf(pitch);
    c.tan_half = tanf(0.5f * 1.0472f);          /* 60 degrees */
    c.aspect = 4.0f / 3.0f;
    c.znear = 0.3f; c.zfar = 150.0f;
    return c;
}

static void depth_numbers(void) {
    CHECK(NEAR_(sh4der_psm_q(1.0f, 4000.0f), 1.00025f, 2e-5f));               /* the page: 1.0002 */
    CHECK(NEAR_(sh4der_psm_q(1.0f, 50.0f), 1.0204f, 1e-3f));                  /* the page: 1.02 */
    CHECK(NEAR_(sh4der_psm_post_z(1.0f, 100.0f, 1.0f), 0.0f, 1e-6f));
    CHECK(NEAR_(sh4der_psm_post_z(1.0f, 100.0f, 100.0f), 1.0f, 1e-5f));
    CHECK(NEAR_(sh4der_psm_post_z(1.0f, 4000.0f, 2.0f), 0.5f, 1e-3f));        /* the first metre takes half the cube */
    CHECK(NEAR_(sh4der_psm_inverse_post_z(0.5f, -0.5f), 0.0f, 1e-6f));
    CHECK(NEAR_(sh4der_psm_inverse_post_z(0.5f, 0.5f), 1.0f, 1e-6f));
    CHECK(NEAR_(sh4der_psm_inverse_post_z(0.5f, 1e9f), 0.5f, 1e-6f));
    CHECK(NEAR_(sh4der_psm_inverse_post_z(0.5f, -1e9f), 0.5f, 1e-6f));
    CHECK(NEAR_(sh4der_psm_bias(0.02f, 1.5f, 0.1f), 0.17f, 1e-6f));
}

static void camera_rows(void) {
    const sh4der_psm_cam_t c = make_cam();
    float m[16], h[4], p[3];
    int i;

    CHECK(sh4der_psm_cam_rows(&c, 0.5f, m));
    for (i = 0; i < 3; i++) p[i] = c.eye[i] + c.fwd[i] * 10.0f;                /* on the axis, 10 m on */
    sh4der_psm_xform(m, p, 1.0f, h);
    CHECK(NEAR_(h[0], 0.0f, 1e-4f) && NEAR_(h[1], 0.0f, 1e-4f) && NEAR_(h[3], 10.0f, 1e-3f));
    CHECK(NEAR_(h[2] / h[3], sh4der_psm_post_z(0.5f, c.zfar, 10.0f), 1e-4f));
    for (i = 0; i < 3; i++) p[i] = c.eye[i] + c.fwd[i] * 0.5f;                 /* on the near plane */
    sh4der_psm_xform(m, p, 1.0f, h);
    CHECK(NEAR_(h[2] / h[3], 0.0f, 1e-4f));
    for (i = 0; i < 3; i++) p[i] = c.eye[i] + c.fwd[i] * c.zfar;               /* on the far plane */
    sh4der_psm_xform(m, p, 1.0f, h);
    CHECK(NEAR_(h[2] / h[3], 1.0f, 1e-4f));
    {
        float corners[8][3], hh[4];

        CHECK(sh4der_psm_frustum(&c, 0.5f, 20.0f, corners));
        for (i = 0; i < 8; i++) {                                              /* every corner is a corner of the unit cube */
            sh4der_psm_xform(m, corners[i], 1.0f, hh);
            CHECK(NEAR_(fabsf(hh[0] / hh[3]), 1.0f, 1e-3f) && NEAR_(fabsf(hh[1] / hh[3]), 1.0f, 1e-3f));
        }
    }
    CHECK(!sh4der_psm_cam_rows(&c, 0.0f, m));
    CHECK(!sh4der_psm_cam_rows(&c, 200.0f, m));
    CHECK(!sh4der_psm_cam_rows(NULL, 0.5f, m));
}

static void unit(float v[3]) { sh4der_norm3_(v); }

static unsigned ground_pts(const sh4der_psm_cam_t *c, float zn, float pts[][3]) {
    float corners[8][3];
    const float up[3] = { 0.0f, 1.0f, 0.0f };

    sh4der_psm_frustum(c, zn, c->zfar, corners);
    return sh4der_psm_slab_pts((const float (*)[3])corners, up, 0.0f, 0.0f + 1e-3f, pts);
}

static void slab(void) {
    const sh4der_psm_cam_t c = make_cam();
    float pts[32][3];
    unsigned n, i;

    n = ground_pts(&c, c.znear, pts);
    CHECK(n >= 3u && n <= 32u);
    for (i = 0; i < n; i++) CHECK(pts[i][1] >= -1e-3f && pts[i][1] <= 2e-3f);
    {
        float corners[8][3];
        const float high[3] = { 0.0f, 1.0f, 0.0f };

        sh4der_psm_frustum(&c, 0.5f, 100.0f, corners);
        CHECK(sh4der_psm_slab_pts((const float (*)[3])corners, high, 500.0f, 600.0f, pts) == 0u);   /* nothing up there */
    }
}

/* the same texel for two points on one light ray, on either side: every kind, every sign of w */
static void light_ray_invariant(void) {
    const sh4der_psm_cam_t c = make_cam();
    float suns[5][3] = { { 0.6f, 0.7f, -0.3f }, { 0.0f, 1.0f, 0.0f }, { 0.3f, 0.6f, 0.7f }, { 0.4f, 0.5f, -0.8f }, { -0.7f, 0.5f, 0.2f } };
    float pts[32][3];
    unsigned n, s, i;
    int seen[4] = { 0, 0, 0, 0 };

    n = ground_pts(&c, c.znear, pts);
    for (s = 0; s < 5u; s++) {
        sh4der_psm_t u, w;
        float d[3];

        memcpy(d, suns[s], sizeof d);
        unit(d);
        CHECK(sh4der_psm_build_uniform(&u, d, &pts[0][0], n));
        CHECK(u.kind == SH4DER_PSM_UNIFORM);
        if (!sh4der_psm_build_warp(&w, &c, WARP_NEAR, d, &pts[0][0], n)) continue;
        seen[w.kind]++;
        for (i = 0; i < n; i++) {
            const sh4der_psm_t *maps[2] = { &u, &w };
            int k;

            for (k = 0; k < 2; k++) {
                float u0, v0, w0, u1, v1, w1, u2, v2, w2, q[3];
                const float t[3] = { 1.5f, 6.0f, 40.0f };
                int j;

                u0 = v0 = w0 = 0.0f;
                CHECK(sh4der_psm_map(maps[k], pts[i], &u0, &v0, &w0));
                CHECK(fabsf(u0) <= 1.0f / SH4DER_PSM_MARGIN + 1e-3f && fabsf(v0) <= 1.0f / SH4DER_PSM_MARGIN + 1e-3f);   /* inside the fit */
                for (j = 0; j < 3; j++) {                                      /* toward the light */
                    int a;
                    for (a = 0; a < 3; a++) q[a] = pts[i][a] + d[a] * t[j];
                    if (sh4der_psm_map(maps[k], q, &u1, &v1, &w1)) CHECK(NEAR_(u1, u0, 2e-3f * (1.0f + fabsf(u0))) && NEAR_(v1, v0, 2e-3f * (1.0f + fabsf(v0))));
                    for (a = 0; a < 3; a++) q[a] = pts[i][a] - d[a] * t[j];    /* away from it */
                    if (sh4der_psm_map(maps[k], q, &u2, &v2, &w2)) CHECK(NEAR_(u2, u0, 2e-3f * (1.0f + fabsf(u0))) && NEAR_(v2, v0, 2e-3f * (1.0f + fabsf(v0))));
                }
            }
            {
                const float a = sh4der_psm_depth(&w, pts[i]);                  /* depth grows toward the light */
                float q[3] = { pts[i][0] + d[0], pts[i][1] + d[1], pts[i][2] + d[2] };

                CHECK(sh4der_psm_depth(&w, q) > a + 0.99f);
            }
        }
    }
    CHECK(seen[SH4DER_PSM_POINT_FRONT] > 0 || seen[SH4DER_PSM_POINT_BEHIND] > 0);
    printf("  warp kinds over 5 suns: ortho %d, ahead %d, behind %d\n", seen[SH4DER_PSM_ORTHO], seen[SH4DER_PSM_POINT_FRONT], seen[SH4DER_PSM_POINT_BEHIND]);
}

static void light_kinds(void) {
    const sh4der_psm_cam_t c = make_cam();
    float pts[32][3], d[3];
    sh4der_psm_t w;
    unsigned n = ground_pts(&c, c.znear, pts);

    d[0] = 0.2f; d[1] = 0.6f; d[2] = -0.8f; unit(d);                           /* toward -z, the way the camera looks: ahead */
    CHECK(sh4der_psm_build_warp(&w, &c, WARP_NEAR, d, &pts[0][0], n) && w.kind == SH4DER_PSM_POINT_FRONT);
    d[0] = 0.2f; d[1] = 0.6f; d[2] = 0.8f; unit(d);                            /* toward +z: behind the camera */
    CHECK(sh4der_psm_build_warp(&w, &c, WARP_NEAR, d, &pts[0][0], n) && w.kind == SH4DER_PSM_POINT_BEHIND);
    {                                                                          /* exactly across the view plane: a direction */
        float s[3] = { 0.7f, 0.0f, 0.0f };
        sh4der_psm_cam_t flat = make_cam();

        flat.fwd[0] = 0.0f; flat.fwd[1] = 0.0f; flat.fwd[2] = -1.0f;
        flat.up[0] = 0.0f; flat.up[1] = 1.0f; flat.up[2] = 0.0f;
        s[1] = 0.7f; unit(s);
        n = ground_pts(&flat, flat.znear, pts);
        CHECK(sh4der_psm_build_warp(&w, &flat, WARP_NEAR, s, &pts[0][0], n) && w.kind == SH4DER_PSM_ORTHO);
    }
}

static void tri_ok(void) {
    CHECK(sh4der_psm_tri_ok(1.0f, 2.0f, 0.5f));
    CHECK(sh4der_psm_tri_ok(-1.0f, -2.0f, -0.5f));
    CHECK(!sh4der_psm_tri_ok(1.0f, -2.0f, 0.5f));                             /* crosses the singularity */
    CHECK(!sh4der_psm_tri_ok(1.0f, 2.0f, 0.0f));                              /* on it */
}

/* ground a texel covers over the ground a pixel covers, near and far down the view */
static void texel_ratio(void) {
    const sh4der_psm_cam_t c = make_cam();
    const float ax[3] = { 1.0f, 0.0f, 0.0f }, az[3] = { 0.0f, 0.0f, 1.0f }, up[3] = { 0.0f, 1.0f, 0.0f };
    const char *names[4] = { "side", "overhead", "behind camera", "head on" };
    float suns[4][3] = { { 0.8f, 0.45f, -0.1f }, { 0.05f, 1.0f, -0.05f }, { 0.3f, 0.5f, 0.8f }, { 0.1f, 0.35f, -0.9f } };
    float pts[32][3], near_p[3] = { 0.0f, 0.0f, -4.0f }, far_p[3] = { 0.0f, 0.0f, -70.0f };
    unsigned n, s;

    printf("  texel m / pixel m on ground at 4 m and 70 m ahead (map 512, 480 px tall)\n");
    printf("  %-14s %-9s %-9s %-9s %-9s %s\n", "sun", "uni near", "uni far", "psm near", "psm far", "kind");
    n = ground_pts(&c, c.znear, pts);
    for (s = 0; s < 4u; s++) {
        sh4der_psm_t u, w;
        float d[3], un, uf, wn = 0, wf = 0, pn, pf;

        memcpy(d, suns[s], sizeof d); unit(d);
        CHECK(sh4der_psm_build_uniform(&u, d, &pts[0][0], n));
        pn = sh4der_psm_pixel_m(&c, 480u, near_p, up);
        pf = sh4der_psm_pixel_m(&c, 480u, far_p, up);
        CHECK(pn > 0.0f && pf > pn);
        un = sh4der_psm_texel_m(&u, 512u, near_p, ax, az, 0.05f) / pn;
        uf = sh4der_psm_texel_m(&u, 512u, far_p, ax, az, 0.05f) / pf;
        if (sh4der_psm_build_warp(&w, &c, WARP_NEAR, d, &pts[0][0], n)) {
            wn = sh4der_psm_texel_m(&w, 512u, near_p, ax, az, 0.05f) / pn;
            wf = sh4der_psm_texel_m(&w, 512u, far_p, ax, az, 0.05f) / pf;
            printf("  %-14s %-9.3f %-9.3f %-9.3f %-9.3f %d\n", names[s], un, uf, wn, wf, w.kind);
            if (s < 3u) CHECK(wn < un);                                        /* near ground gets more texels than the uniform map gives */
            if (s < 3u) CHECK(wf / wn > uf / un);                              /* and far over near comes nearer 1: the mismatch shrinks */
        } else {
            printf("  %-14s %-9.3f %-9.3f no warp (degenerate: use the uniform map)\n", names[s], un, uf);
            CHECK(s == 3u);                                                    /* only head on may fail */
        }
    }
}

/* the near plane the warp is built with: raising it gives the near ground more texels? it gives the FAR ground more and the near less */
static void warp_near(void) {
    const sh4der_psm_cam_t c = make_cam();
    const float ax[3] = { 1.0f, 0.0f, 0.0f }, az[3] = { 0.0f, 0.0f, 1.0f };
    float d[3] = { 0.8f, 0.45f, -0.1f }, pts[32][3], far_p[3] = { 0.0f, 0.0f, -70.0f };
    float lo_far, hi_far;
    sh4der_psm_t a, b;
    unsigned n;

    unit(d);
    n = ground_pts(&c, c.znear, pts);
    CHECK(sh4der_psm_build_warp(&a, &c, 2.0f, d, &pts[0][0], n));
    CHECK(sh4der_psm_build_warp(&b, &c, 8.0f, d, &pts[0][0], n));
    lo_far = sh4der_psm_texel_m(&a, 512u, far_p, ax, az, 0.05f);
    hi_far = sh4der_psm_texel_m(&b, 512u, far_p, ax, az, 0.05f);
    printf("  far ground texel: warp near 2 m -> %.3f m, 8 m -> %.3f m\n", lo_far, hi_far);
    CHECK(hi_far < lo_far);                                                   /* a larger near leaves texels for the far */
}

static void near_and_light_depth(void) {
    const sh4der_psm_cam_t c = make_cam();
    float pts[32][3], m[16], lh[4], zero_w = 0.0f;
    const float zn = 3.0f;
    unsigned n = ground_pts(&c, c.znear, pts);
    float near_z = sh4der_psm_near_from_pts(&c, &pts[0][0], n, 20.0f);

    (void)zero_w;
    CHECK(near_z > 2.0f && near_z < 3.0f);                                    /* the nearest ground the camera sees */
    CHECK(NEAR_(sh4der_psm_near_from_pts(&c, &pts[0][0], n, 1.0f), 1.0f, 1e-6f));   /* held to the limit */
    CHECK(sh4der_psm_near_from_pts(&c, &pts[0][0], n, 0.2f) == 0.0f);         /* a limit under the camera's near: no answer */
    CHECK(sh4der_psm_near_from_pts(&c, NULL, 0u, 20.0f) == 0.0f);
    CHECK(sh4der_psm_cam_rows(&c, zn, m));
    sh4der_psm_xform(m, c.fwd, 0.0f, lh);                                     /* a light along the view direction: depth Q, w 1 */
    CHECK(NEAR_(lh[2] / lh[3], sh4der_psm_q(zn, c.zfar), 1e-5f) && NEAR_(lh[3], 1.0f, 1e-5f));
}

static void bad_input(void) {
    const sh4der_psm_cam_t c = make_cam();
    sh4der_psm_t w;
    float pts[32][3], d[3] = { 0.0f, 1.0f, 0.0f };
    unsigned n = ground_pts(&c, c.znear, pts);
    float zero[3] = { 0.0f, 0.0f, 0.0f }, behind[3] = { 0.0f, 0.0f, 5.0f };

    CHECK(!sh4der_psm_build_uniform(NULL, d, &pts[0][0], n));
    CHECK(!sh4der_psm_build_uniform(&w, d, &pts[0][0], 0u));
    CHECK(!sh4der_psm_build_uniform(&w, d, &pts[0][0], SH4DER_PSM_MAX_PTS + 1u));
    CHECK(!sh4der_psm_build_uniform(&w, zero, &pts[0][0], n));                 /* no light direction */
    CHECK(!sh4der_psm_build_warp(&w, &c, WARP_NEAR, d, behind, 1u));                /* a point behind the camera */
    CHECK(!sh4der_psm_build_warp(&w, &c, -1.0f, d, &pts[0][0], n));
    CHECK(!sh4der_psm_build_uniform(&w, d, behind, 1u));                       /* one point has no extent: refused */
}

int main(void) {
    depth_numbers();
    camera_rows();
    slab();
    light_kinds();
    light_ray_invariant();
    tri_ok();
    texel_ratio();
    warp_near();
    near_and_light_depth();
    bad_input();
    if (fails) { fprintf(stderr, "psm_test: %d failure(s)\n", fails); return 1; }
    printf("psm_test: ok\n");
    return 0;
}
