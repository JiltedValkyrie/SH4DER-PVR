/* SH4DER-PVR: software shadow map test

   test/psm_raster_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of the software shadow map: a caster over a ground grid, every grid vertex
   the camera sees compared with the shadow worked out by hand (the vertex's ray toward the
   light against the caster's rectangle), for the uniform map and the warped one at four
   sun angles, the sun behind the camera among them; the ground drawn into the map as a
   caster too (two triangles far larger than the map, across the camera's plane and the
   light's singularity) with no vertex shadowed by its own surface; a strip that runs
   behind the camera; a point just under and just over a caster; the four-texel blend; the
   byte form; bad input.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_psm_raster.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)
#define NEAR_(a, b, e) (fabsf((a) - (b)) <= (e))

#define MAP_N 256u
#define WARP_NEAR 4.0f

static uint16_t depth[MAP_N * MAP_N];

static sh4der_psm_cam_t make_cam(void) {
    sh4der_psm_cam_t c;
    const float pitch = 0.12f;

    memset(&c, 0, sizeof c);
    c.eye[0] = 0.0f; c.eye[1] = 1.7f; c.eye[2] = 0.0f;
    c.fwd[0] = 0.0f; c.fwd[1] = -sinf(pitch); c.fwd[2] = -cosf(pitch);
    c.up[0] = 0.0f; c.up[1] = cosf(pitch); c.up[2] = -sinf(pitch);
    c.tan_half = tanf(0.5f * 1.0472f);
    c.aspect = 4.0f / 3.0f;
    c.znear = 0.3f; c.zfar = 60.0f;
    return c;
}

static unsigned ground_pts(const sh4der_psm_cam_t *c, float zn, float pts[][3]) {
    float corners[8][3];
    const float up[3] = { 0.0f, 1.0f, 0.0f };

    sh4der_psm_frustum(c, zn, c->zfar, corners);
    return sh4der_psm_slab_pts((const float (*)[3])corners, up, 0.0f, 1e-3f, pts);
}

/* a horizontal rectangle at height h: the caster */
typedef struct { float cx, cz, hw, hd, h; } rect_t;

static void rect_corners(const rect_t *q, float p[4][3]) {
    int i;

    for (i = 0; i < 4; i++) {
        p[i][0] = q->cx + ((i == 1 || i == 2) ? q->hw : -q->hw);
        p[i][1] = q->h;
        p[i][2] = q->cz + (i >= 2 ? q->hd : -q->hd);
    }
}

static void rect_draw(sh4der_psm_raster_t *r, const rect_t *q) {
    float p[4][3];

    rect_corners(q, p);
    sh4der_psm_raster_tri(r, p[0], p[1], p[2]);
    sh4der_psm_raster_tri(r, p[0], p[2], p[3]);
}

/* Signed distance, on the ground, from g's shadow edge: under 0 is inside the rectangle's shadow. */
static float shadow_dist(const rect_t *q, const float sun[3], const float g[3]) {
    const float t = (q->h - g[1]) / sun[1];
    const float dx = fabsf(g[0] + sun[0] * t - q->cx) - q->hw, dz = fabsf(g[2] + sun[2] * t - q->cz) - q->hd;

    if (dx < 0.0f && dz < 0.0f) return dx > dz ? dx : dz;
    return sqrtf((dx > 0.0f ? dx * dx : 0.0f) + (dz > 0.0f ? dz * dz : 0.0f));
}

static int in_view(const float rows[16], const float p[3]) {
    float h[4];

    sh4der_psm_xform(rows, p, 1.0f, h);
    return h[3] > 0.0f && fabsf(h[0]) < 0.98f * h[3] && fabsf(h[1]) < 0.98f * h[3] && h[2] > 0.0f && h[2] < h[3];
}

/* One scene: the map (warped or uniform), the rectangle (and the ground, when asked) drawn in, the grid compared.
   Returns the number of shadowed vertices that were checked; -1 when the map could not be built. */
static int scene(int warped, const float sun_in[3], const rect_t *q, int ground_too, int pcf, int *wrong, int *checked) {
    const sh4der_psm_cam_t c = make_cam();
    const float ax[3] = { 1.0f, 0.0f, 0.0f }, az[3] = { 0.0f, 0.0f, 1.0f };
    float sun[3], pts[32][3], rows[16], cp[4][3], lo, hi, glo = 0.0f, ghi = 0.0f, gx, gz;
    float big[4][3] = { { -300.0f, 0.0f, -300.0f }, { 300.0f, 0.0f, -300.0f }, { 300.0f, 0.0f, 300.0f }, { -300.0f, 0.0f, 300.0f } };
    sh4der_psm_t map;
    sh4der_psm_raster_t r = { 0 };                             /* begin below fills it; zero first so a refused begin reads clean */
    unsigned n;
    int dark = 0;

    sun[0] = sun_in[0]; sun[1] = sun_in[1]; sun[2] = sun_in[2];
    sh4der_norm3_(sun);
    n = ground_pts(&c, WARP_NEAR, pts);
    if (warped ? !sh4der_psm_build_warp(&map, &c, WARP_NEAR, sun, &pts[0][0], n) : !sh4der_psm_build_uniform(&map, sun, &pts[0][0], n)) return -1;
    rect_corners(q, cp);
    CHECK(sh4der_psm_raster_key_range(map.light, &cp[0][0], 3u * sizeof(float), 4u, &lo, &hi));
    CHECK(sh4der_psm_raster_key_range(map.light, &pts[0][0], 3u * sizeof(float), n, &glo, &ghi));   /* and the ground in view */
    if (glo < lo) lo = glo;
    if (ghi > hi) hi = ghi;
    CHECK(sh4der_psm_raster_begin(&r, &map, depth, MAP_N, lo - 0.5f, hi + 0.5f));
    rect_draw(&r, q);
    if (ground_too) {
        sh4der_psm_raster_tri(&r, big[0], big[1], big[2]);
        sh4der_psm_raster_tri(&r, big[0], big[2], big[3]);
        CHECK(r.drawn > 1u);                                   /* the ground reached the map */
    }
    CHECK(sh4der_psm_cam_rows(&c, WARP_NEAR, rows));
    *wrong = *checked = 0;
    for (gz = -58.0f; gz < -3.0f; gz += 0.25f)
        for (gx = -30.0f; gx <= 30.0f; gx += 0.25f) {
            const float g[3] = { gx, 0.0f, gz };
            float texel, bias, d, lit;

            if (!in_view(rows, g)) continue;
            texel = sh4der_psm_texel_m(&map, MAP_N, g, ax, az, SH4DER_PSM_RASTER_DELTA);
            if (!(texel < SH4DER_PSM_RASTER_TEXEL_MAX)) continue;
            bias = sh4der_psm_bias(SH4DER_PSM_BIAS_A, SH4DER_PSM_BIAS_B, texel);
            d = shadow_dist(q, sun, g);
            if (fabsf(d) < 3.0f * texel + 2.0f * bias / sun[1]) continue;      /* the edge: a texel either way is not an error */
            lit = sh4der_psm_raster_lit_plane(&r, g, ax, az, bias, pcf);
            (*checked)++;
            if (d < 0.0f) dark++;
            if ((d < 0.0f) != (lit < 0.5f) || (lit != 0.0f && lit != 1.0f)) (*wrong)++;
        }
    return dark;
}

static void grids(void) {
    static const float suns[4][3] = { { 0.8f, 0.45f, -0.1f }, { 0.1f, 1.0f, 0.1f }, { 0.25f, 0.55f, 0.8f }, { -0.5f, 0.6f, -0.6f } };
    static const char *names[4] = { "side", "overhead", "behind camera", "ahead" };
    const rect_t box = { 0.0f, -12.0f, 2.0f, 2.0f, 2.0f };
    const rect_t strip = { 0.5f, 0.0f, 1.0f, 30.0f, 2.5f };       /* from 30 m ahead to 30 m behind the camera */
    int s, w, g;

    for (s = 0; s < 4; s++)
        for (w = 0; w < 2; w++)
            for (g = 0; g < 2; g++) {
                int wrong = 0, checked = 0;
                const int dark = scene(w, suns[s], &box, g, g, &wrong, &checked);

                printf("  %-14s %-7s %-12s checked %5d, shadowed %4d, wrong %d\n", names[s], w ? "warp" : "uniform",
                       g ? "ground drawn" : "", checked, dark, wrong);
                CHECK(dark >= 0);                                  /* every one of these builds */
                CHECK(checked > 1000 && wrong == 0);
                if (w) CHECK(dark > 10);                           /* the warp resolves the box's shadow at 12 m */
            }
    for (w = 0; w < 2; w++) {
        int wrong = 0, checked = 0;
        const int dark = scene(w, suns[2], &strip, 1, 0, &wrong, &checked);

        printf("  strip behind   %-7s              checked %5d, shadowed %4d, wrong %d\n", w ? "warp" : "uniform", checked, dark, wrong);
        CHECK(dark >= 0 && wrong == 0);
        if (w) CHECK(dark > 20);
    }
}

/* depth resolution: with the sun straight up (no slope across a texel) 5 cm under a caster is shadowed and 5 cm over it
   is lit; with a slanted sun the same at 60 cm (the plain compare has the caster's slope across a texel in it) */
static void under_and_over(void) {
    const sh4der_psm_cam_t c = make_cam();
    const rect_t q = { 0.0f, -10.0f, 3.0f, 3.0f, 1.0f };
    static const float suns[2][3] = { { 0.0f, 1.0f, 0.0f }, { 0.3f, 0.9f, 0.2f } };
    static const float gaps[2] = { 0.05f, 0.6f };
    float sun[3], pts[32][3], under[3] = { 0.0f, 0.0f, -10.0f }, over[3] = { 0.0f, 0.0f, -10.0f }, cp[4][3], lo, hi;
    sh4der_psm_t map;
    sh4der_psm_raster_t r;
    unsigned n;
    int w, s;

    n = ground_pts(&c, WARP_NEAR, pts);
    for (s = 0; s < 2; s++)
    for (w = 0; w < 2; w++) {
        sun[0] = suns[s][0]; sun[1] = suns[s][1]; sun[2] = suns[s][2];
        sh4der_norm3_(sun);
        under[1] = q.h - gaps[s]; over[1] = q.h + gaps[s];
        CHECK(w ? sh4der_psm_build_warp(&map, &c, WARP_NEAR, sun, &pts[0][0], n) : sh4der_psm_build_uniform(&map, sun, &pts[0][0], n));
        rect_corners(&q, cp);
        CHECK(sh4der_psm_raster_key_range(map.light, &cp[0][0], 3u * sizeof(float), 4u, &lo, &hi));
        CHECK(sh4der_psm_raster_begin(&r, &map, depth, MAP_N, lo - 1.0f, hi + 1.0f));
        rect_draw(&r, &q);
        CHECK(r.tris == 2u && r.drawn == 2u);
        CHECK(sh4der_psm_raster_lit(&r, under, 0.01f, 0) == 0.0f);
        CHECK(sh4der_psm_raster_lit(&r, over, 0.01f, 0) == 1.0f);
        CHECK(sh4der_psm_raster_lit(&r, under, 0.01f, 1) == 0.0f);
        CHECK(sh4der_psm_raster_lit(&r, over, 0.01f, 1) == 1.0f);
    }
}

/* the blend runs 0 .. 1 across an edge and never leaves that range; the byte form is the same number */
static void blend_and_bytes(void) {
    const sh4der_psm_cam_t c = make_cam();
    const rect_t q = { 0.0f, -10.0f, 2.0f, 2.0f, 2.0f };
    const float ax[3] = { 1.0f, 0.0f, 0.0f }, az[3] = { 0.0f, 0.0f, 1.0f };
    float sun[3] = { 0.0f, 1.0f, 0.0f }, pts[32][3], line[64][3], lo = 2.0f, hi = -1.0f;
    uint8_t bytes[64];
    sh4der_psm_t map;
    sh4der_psm_raster_t r;
    unsigned n, i, part = 0;

    n = ground_pts(&c, WARP_NEAR, pts);
    CHECK(sh4der_psm_build_warp(&map, &c, WARP_NEAR, sun, &pts[0][0], n));
    CHECK(sh4der_psm_raster_begin(&r, &map, depth, MAP_N, -1.0f, 4.0f));       /* the sun straight up: the depth is the height */
    rect_draw(&r, &q);
    for (i = 0; i < 64u; i++) {                                    /* across the shadow's left edge at x = -2 */
        float v;

        line[i][0] = -2.4f + 0.0125f * (float)i; line[i][1] = 0.0f; line[i][2] = -10.0f;
        v = sh4der_psm_raster_lit(&r, line[i], 0.05f, 1);
        CHECK(v >= 0.0f && v <= 1.0f);
        if (v < lo) lo = v;
        if (v > hi) hi = v;
        if (v > 0.02f && v < 0.98f) part++;
    }
    CHECK(lo == 0.0f && hi == 1.0f && part > 0u);
    CHECK(sh4der_psm_raster_lit(&r, line[0], 0.05f, 1) == 1.0f && sh4der_psm_raster_lit(&r, line[63], 0.05f, 1) == 0.0f);
    sh4der_psm_raster_lit_n(&r, &line[0][0], 3u * sizeof(float), 64u, NULL, NULL, 0.05f, 0.0f, 1, bytes, 1u);
    for (i = 0; i < 64u; i++) CHECK(bytes[i] == (uint8_t)(sh4der_psm_raster_lit(&r, line[i], 0.05f, 1) * 255.0f + 0.5f));
    sh4der_psm_raster_lit_n(&r, &line[0][0], 3u * sizeof(float), 64u, ax, az, 0.02f, 1.5f, 0, bytes, 1u);
    CHECK(bytes[0] == 255u && bytes[63] == 0u);
}

static void bad_input(void) {
    const sh4der_psm_cam_t c = make_cam();
    float sun[3] = { 0.0f, 1.0f, 0.0f }, pts[32][3], lo, hi, far_out[3][3] = { { 900.0f, 5.0f, 0.0f }, { 901.0f, 5.0f, 0.0f }, { 900.0f, 5.0f, 1.0f } };
    float outside[3] = { 900.0f, 0.0f, 0.0f }, nan_tri[3] = { 0.0f, 0.0f, 0.0f };
    sh4der_psm_t map;
    sh4der_psm_raster_t r;
    unsigned n = ground_pts(&c, WARP_NEAR, pts), i, filled = 0;

    CHECK(sh4der_psm_build_uniform(&map, sun, &pts[0][0], n));
    CHECK(!sh4der_psm_raster_begin(NULL, &map, depth, MAP_N, 0.0f, 1.0f));
    CHECK(!sh4der_psm_raster_begin(&r, NULL, depth, MAP_N, 0.0f, 1.0f));
    CHECK(!sh4der_psm_raster_begin(&r, &map, NULL, MAP_N, 0.0f, 1.0f));
    CHECK(!sh4der_psm_raster_begin(&r, &map, depth, 1u, 0.0f, 1.0f));
    CHECK(!sh4der_psm_raster_begin(&r, &map, depth, SH4DER_PSM_RASTER_SIZE_MAX + 1u, 0.0f, 1.0f));
    CHECK(!sh4der_psm_raster_begin(&r, &map, depth, MAP_N, 1.0f, 1.0f));
    CHECK(!sh4der_psm_raster_key_range(sun, NULL, 12u, 4u, &lo, &hi));
    CHECK(!sh4der_psm_raster_key_range(sun, &pts[0][0], 12u, 0u, &lo, &hi));
    CHECK(sh4der_psm_raster_key_range(sun, &far_out[0][0], 12u, 3u, &lo, &hi) && NEAR_(lo, 5.0f, 1e-6f) && NEAR_(hi, 5.0f, 1e-6f));

    CHECK(sh4der_psm_raster_begin(&r, &map, depth, MAP_N, 0.0f, 10.0f));
    sh4der_psm_raster_tri(&r, far_out[0], far_out[1], far_out[2]);             /* wholly outside the map */
    nan_tri[0] = (float)NAN;
    sh4der_psm_raster_tri(&r, nan_tri, far_out[1], far_out[2]);                /* not a number: not drawn */
    sh4der_psm_raster_tri(&r, far_out[0], far_out[0], far_out[0]);             /* no area */
    for (i = 0; i < MAP_N * MAP_N; i++) filled += depth[i] != 0u;
    CHECK(r.tris == 3u && r.drawn == 0u && filled == 0u);
    CHECK(sh4der_psm_raster_lit(&r, outside, 0.0f, 0) == 1.0f);                /* outside the map: lit */
    CHECK(sh4der_psm_raster_lit(&r, outside, 0.0f, 1) == 1.0f);
    CHECK(sh4der_psm_raster_lit(&r, pts[0], 0.0f, 1) == 1.0f);                 /* an empty map shadows nothing */
}

/* the batch paths (`sh4der_psm_raster_xform_n`, `sh4der_psm_raster_lit_n` with axes; under SH4ZAM one FTRV a point with the
   matrix loaded once) against the scalar ones: the transforms to 1e-5 relative, the plane compare to one byte (b 0: no texel
   term); the texel term against `sh4der_psm_texel_m` loosely, since both are float forward differences over 2 cm and each
   carries a few per cent of cancellation far out */
static void batch_paths(void) {
    const sh4der_psm_cam_t c = make_cam();
    const rect_t q = { 0.0f, -10.0f, 2.0f, 2.0f, 2.0f };
    const float ax[3] = { 1.0f, 0.0f, 0.0f }, az[3] = { 0.0f, 0.0f, 1.0f };
    float sun[3] = { 0.25f, 0.55f, 0.8f }, pts[32][3], gp[64][3], hb[64][4], hs[4], cp[4][3], lo, hi, worst_xf = 0.0f;
    static float rp[4000][3];
    static uint8_t rb[4000];
    uint32_t seed = 12345u;
    sh4der_psm_t map;
    sh4der_psm_raster_t r;
    unsigned n, i, worst_lsb = 0u;

    n = ground_pts(&c, WARP_NEAR, pts);
    sh4der_norm3_(sun);
    CHECK(sh4der_psm_build_warp(&map, &c, WARP_NEAR, sun, &pts[0][0], n));
    rect_corners(&q, cp);
    CHECK(sh4der_psm_raster_key_range(map.light, &cp[0][0], 3u * sizeof(float), 4u, &lo, &hi));
    CHECK(sh4der_psm_raster_begin(&r, &map, depth, MAP_N, lo - 1.0f, hi + 1.0f));
    rect_draw(&r, &q);
    for (i = 0; i < 64u; i++) {                                    /* an 8 x 8 grid of ground points in view, 2 m to 30 m out */
        gp[i][0] = -6.0f + 1.7f * (float)(i % 8u);
        gp[i][1] = 0.0f;
        gp[i][2] = -2.0f - 4.0f * (float)(i / 8u);
    }
    sh4der_psm_raster_xform_n(&r, &gp[0][0], 3u * sizeof(float), 64u, hb);
    for (i = 0; i < 64u; i++) {
        float d;

        sh4der_psm_raster_xform(&r, gp[i], hs);
        d = fabsf(hb[i][0] - hs[0]) + fabsf(hb[i][1] - hs[1]) + fabsf(hb[i][2] - hs[2]) + fabsf(hb[i][3] - hs[3]);
        if (d > worst_xf) worst_xf = d;
        CHECK(fabsf(hb[i][3] - hs[3]) <= 1e-5f * (1.0f + fabsf(hs[3])));
        CHECK(fabsf(hb[i][0] - hs[0]) <= 1e-5f * (1.0f + fabsf(hs[0])));
    }
    CHECK(worst_xf <= 1e-3f);
    for (i = 0; i < 4000u; i++) {                                  /* 4000 points around the caster's shadow edge: a plane compare near its depth edge shows an error */
        seed = seed * 1664525u + 1013904223u;
        rp[i][0] = -3.0f + 6.0f * (float)(seed >> 8) / 16777216.0f;
        seed = seed * 1664525u + 1013904223u;
        rp[i][1] = 0.0f;
        rp[i][2] = -15.0f + 6.0f * (float)(seed >> 8) / 16777216.0f;
    }
    sh4der_psm_raster_lit_n(&r, &rp[0][0], 3u * sizeof(float), 4000u, ax, az, 0.02f, 0.0f, 1, rb, 1u);
    for (i = 0; i < 4000u; i++) {
        const unsigned s = (unsigned)(sh4der_psm_raster_lit_plane(&r, rp[i], ax, az, 0.02f, 1) * 255.0f + 0.5f);
        const unsigned b = rb[i];
        const unsigned dl = b > s ? b - s : s - b;

        if (dl > worst_lsb) worst_lsb = dl;
    }
    CHECK(worst_lsb <= 1u);
    for (i = 0; i < 64u; i++) {                                    /* the texel term: same texel to a few per cent, the bias in metres alike */
        const float *x = gp[i];
        const float h_t = sh4der_psm_texel_m(&map, MAP_N, x, ax, az, SH4DER_PSM_RASTER_DELTA);
        float h[4], g1[4], g2[4], tn;

        sh4der_psm_xform(r.m, x, 1.0f, h);
        sh4der_psm_xform(r.m, ax, 0.0f, g1);
        sh4der_psm_xform(r.m, az, 0.0f, g2);
        if (!(h_t < 1e29f) || !(h[3] > SH4DER_PSM_TRI_EPS)) continue;
        tn = sh4der_psm_raster_texel_(&r, h, 1.0f / h[3], g1, g2, SH4DER_PSM_RASTER_DELTA);
        CHECK(fabsf(tn - h_t) <= 0.05f * h_t);
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    grids();
    under_and_over();
    blend_and_bytes();
    batch_paths();
    bad_input();
    if (fails) { fprintf(stderr, "psm_raster_test: %d failure(s)\n", fails); return 1; }
    printf("psm_raster_test: ok\n");
    return 0;
}
