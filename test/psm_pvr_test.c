/* SH4DER-PVR: perspective shadow map, PVR route, test

   test/psm_pvr_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of the route where the PVR draws the map (sh4der_psm_pvr.h): a caster
   triangle inside the map, outside it, and across the light's singularity (clipped, every
   output vertex with positive 1 / w and inside the map); the lookup coordinates of a
   ground vertex; the interpolation error of a ground triangle, which is 0 for a uniform
   map and grows with the cell and the warp; and the route against the SH-4 depth map
   (sh4der_psm_raster.h) for a box over a ground: how many ground points a silhouette
   target looked up through per-vertex coordinates (the PVR's interpolation) gets
   differently from the exact compare, for three cell sizes, uniform and warped.
   The PVR's own filtering is a plain bilinear read here.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../include/sh4der_psm_pvr.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

#define WARP_NEAR 4.0f
#define MAP_N 256u

static sh4der_psm_cam_t make_cam(void) {
    sh4der_psm_cam_t c;
    const float pitch = 0.12f;

    memset(&c, 0, sizeof c);
    c.eye[1] = 1.7f;
    c.fwd[1] = -sinf(pitch); c.fwd[2] = -cosf(pitch);
    c.up[1] = cosf(pitch); c.up[2] = -sinf(pitch);
    c.tan_half = tanf(0.5f * 1.0472f);
    c.aspect = 4.0f / 3.0f;
    c.znear = 0.3f; c.zfar = 60.0f;
    return c;
}

static unsigned ground_pts(const sh4der_psm_cam_t *c, float pts[][3]) {
    float corners[8][3];
    const float up[3] = { 0.0f, 1.0f, 0.0f };

    sh4der_psm_frustum(c, 2.0f, c->zfar, corners);
    return sh4der_psm_slab_pts((const float (*)[3])corners, up, 0.0f, 1e-3f, pts);
}

static int build(sh4der_psm_t *m, int warped, const float sun_in[3]) {
    const sh4der_psm_cam_t c = make_cam();
    float pts[32][3], sun[3] = { sun_in[0], sun_in[1], sun_in[2] };
    const unsigned n = ground_pts(&c, pts);

    sh4der_norm3_(sun);
    return warped ? sh4der_psm_build_warp(m, &c, WARP_NEAR, sun, &pts[0][0], n) : sh4der_psm_build_uniform(m, sun, &pts[0][0], n);
}

static const float SUNS[4][3] = { { 0.8f, 0.45f, -0.1f }, { 0.1f, 1.0f, 0.1f }, { 0.25f, 0.55f, 0.8f }, { -0.5f, 0.6f, -0.6f } };
static const char *SUN_NAMES[4] = { "side", "overhead", "behind camera", "ahead" };

/* ---- caster triangles ---------------------------------------------------- */

static void caster_tris(void) {
    sh4der_psm_t map;
    sh4der_psm_pvr_vtx_t out[SH4DER_PSM_PVR_MAX_TRIS][3];
    const float a[3] = { 0.0f, 0.0f, -12.0f }, b[3] = { 1.0f, 0.0f, -12.0f }, c[3] = { 0.5f, 3.0f, -12.0f };
    const float far_a[3] = { 900.0f, 0.0f, -12.0f }, far_b[3] = { 901.0f, 0.0f, -12.0f }, far_c[3] = { 900.5f, 3.0f, -12.0f };
    const float s0[3] = { -8.0f, 0.0f, 30.0f }, s1[3] = { 8.0f, 0.0f, 30.0f }, s2[3] = { 8.0f, 0.0f, -30.0f }, s3[3] = { -8.0f, 0.0f, -30.0f };
    unsigned n, i;
    int k;

    CHECK(build(&map, 0, SUNS[0]));
    n = sh4der_psm_pvr_tris(&map, MAP_N, a, b, c, out);
    CHECK(n == 1u);                                                           /* inside: one triangle */
    for (i = 0; i < n; i++) for (k = 0; k < 3; k++) CHECK(out[i][k].invw > 0.0f && out[i][k].x >= -1e-3f && out[i][k].x <= (float)MAP_N + 1e-3f);
    CHECK(sh4der_psm_pvr_tris(&map, MAP_N, far_a, far_b, far_c, out) == 0u);  /* far outside the fit: nothing */
    CHECK(sh4der_psm_pvr_tris(NULL, MAP_N, a, b, c, out) == 0u);
    CHECK(sh4der_psm_pvr_tris(&map, 1u, a, b, c, out) == 0u);

    CHECK(build(&map, 1, SUNS[2]));                                           /* sun behind the camera: the singularity runs across the view */
    n = sh4der_psm_pvr_tris(&map, MAP_N, s0, s1, s2, out);
    printf("  strip from 30 m behind to 30 m ahead, sun behind the camera: %u triangles for the first half\n", n);
    for (i = 0; i < n; i++)
        for (k = 0; k < 3; k++)
            CHECK(out[i][k].invw > 0.0f && sh4der_finite_(out[i][k].x) && out[i][k].x >= -1e-2f && out[i][k].x <= (float)MAP_N + 1e-2f &&
                  out[i][k].y >= -1e-2f && out[i][k].y <= (float)MAP_N + 1e-2f);
    n = sh4der_psm_pvr_tris(&map, MAP_N, s0, s2, s3, out);
    for (i = 0; i < n; i++) for (k = 0; k < 3; k++) CHECK(out[i][k].invw > 0.0f);
}

/* ---- lookup coordinates and the interpolation error ------------------------ */

static void lookup(void) {
    sh4der_psm_t map;
    float pts[32][3], u = 0.0f, v = 0.0f;
    const sh4der_psm_cam_t c = make_cam();
    unsigned n, i, s, w;

    n = ground_pts(&c, pts);
    for (s = 0; s < 4u; s++)
        for (w = 0; w < 2u; w++) {
            if (!build(&map, (int)w, SUNS[s])) continue;
            for (i = 0; i < n; i++) {
                CHECK(sh4der_psm_pvr_uv(&map, pts[i], &u, &v));
                CHECK(u >= -0.01f && u <= 1.01f && v >= -0.01f && v <= 1.01f);     /* every fitted point lands in the target */
            }
        }
}

static float tri_error(const sh4der_psm_t *map, float gx, float gz, float s) {
    const float a[3] = { gx, 0.0f, gz }, b[3] = { gx + s, 0.0f, gz }, c[3] = { gx + s, 0.0f, gz + s };

    return sh4der_psm_pvr_uv_error(map, MAP_N, a, b, c);
}

static void interpolation_error(void) {
    static const float cells[3] = { 4.0f, 1.0f, 0.25f };
    static const float zs[3] = { -6.0f, -20.0f, -50.0f };
    unsigned s, k, d;

    printf("  lookup error of one ground triangle, texels of a %u map (uniform / warp), by cell size and distance ahead\n", MAP_N);
    printf("  %-14s %-7s %-17s %-17s %-17s\n", "sun", "cell m", "6 m", "20 m", "50 m");
    for (s = 0; s < 4u; s++) {
        sh4der_psm_t uni, war;
        const int have_w = build(&war, 1, SUNS[s]);

        CHECK(build(&uni, 0, SUNS[s]));
        for (k = 0; k < 3u; k++) {
            printf("  %-14s %-7.2f", SUN_NAMES[s], cells[k]);
            for (d = 0; d < 3u; d++) {
                const float eu = tri_error(&uni, -1.0f, zs[d], cells[k]), ew = have_w ? tri_error(&war, -1.0f, zs[d], cells[k]) : -1.0f;

                CHECK(eu < 1e-2f);                                                  /* a uniform map is affine in world space: exact */
                printf(" %6.3f / %-8.3f", eu, ew);
            }
            printf("\n");
        }
        if (have_w) {                                                               /* the error falls with the cell */
            const float e4 = tri_error(&war, -1.0f, -20.0f, 4.0f), e1 = tri_error(&war, -1.0f, -20.0f, 1.0f), e025 = tri_error(&war, -1.0f, -20.0f, 0.25f);

            CHECK(e4 > e1 && e1 > e025 && e4 > 0.01f);
        }
    }
}

/* ---- the route against the exact compare ----------------------------------- */

static unsigned char sil[MAP_N * MAP_N];
static uint16_t dep[MAP_N * MAP_N];

static void fill_tri(const sh4der_psm_pvr_vtx_t t[3]) {
    float area = (t[1].x - t[0].x) * (t[2].y - t[0].y) - (t[2].x - t[0].x) * (t[1].y - t[0].y);
    sh4der_psm_pvr_vtx_t p0 = t[0], p1 = t[1], p2 = t[2];
    int x, y, x0 = (int)MAP_N, x1 = 0, y0 = (int)MAP_N, y1 = 0;

    if (area < 0.0f) { const sh4der_psm_pvr_vtx_t tmp = p1; p1 = p2; p2 = tmp; area = -area; }
    if (!(area > 1e-9f)) return;
    {
        const float xs[3] = { p0.x, p1.x, p2.x }, ys[3] = { p0.y, p1.y, p2.y };
        int k;

        for (k = 0; k < 3; k++) {
            if ((int)floorf(xs[k]) - 1 < x0) x0 = (int)floorf(xs[k]) - 1;
            if ((int)ceilf(xs[k]) + 1 > x1) x1 = (int)ceilf(xs[k]) + 1;
            if ((int)floorf(ys[k]) - 1 < y0) y0 = (int)floorf(ys[k]) - 1;
            if ((int)ceilf(ys[k]) + 1 > y1) y1 = (int)ceilf(ys[k]) + 1;
        }
    }
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)MAP_N) x1 = (int)MAP_N;
    if (y1 > (int)MAP_N) y1 = (int)MAP_N;
    for (y = y0; y < y1; y++)
        for (x = x0; x < x1; x++) {
            const float px = (float)x + 0.5f, py = (float)y + 0.5f;
            const float e0 = (p1.x - px) * (p2.y - py) - (p2.x - px) * (p1.y - py);
            const float e1 = (p2.x - px) * (p0.y - py) - (p0.x - px) * (p2.y - py);
            const float e2 = (p0.x - px) * (p1.y - py) - (p1.x - px) * (p0.y - py);

            if (e0 >= -1e-3f && e1 >= -1e-3f && e2 >= -1e-3f) sil[y * (int)MAP_N + x] = 255u;
        }
}

static float sil_at(int x, int y) {
    if (x < 0 || y < 0 || x >= (int)MAP_N || y >= (int)MAP_N) return 0.0f;
    return (float)sil[y * (int)MAP_N + x] * (1.0f / 255.0f);
}

/* the PVR's bilinear read of the target at (u, v) 0 .. 1 */
static float sil_read(float u, float v) {
    const float fx = u * (float)MAP_N - 0.5f, fy = v * (float)MAP_N - 0.5f;
    const int ix = (int)floorf(fx), iy = (int)floorf(fy);
    const float tx = fx - (float)ix, ty = fy - (float)iy;

    return (sil_at(ix, iy) * (1 - tx) + sil_at(ix + 1, iy) * tx) * (1 - ty) + (sil_at(ix, iy + 1) * (1 - tx) + sil_at(ix + 1, iy + 1) * tx) * ty;
}

typedef struct { float lo[3], hi[3]; } box_t;

static void box_tris(const box_t *b, float t[12][3][3]) {
    static const int f[12][3] = { { 0, 1, 5 }, { 0, 5, 4 }, { 1, 2, 6 }, { 1, 6, 5 }, { 2, 3, 7 }, { 2, 7, 6 }, { 3, 0, 4 }, { 3, 4, 7 },
                                  { 4, 5, 6 }, { 4, 6, 7 }, { 0, 3, 2 }, { 0, 2, 1 } };
    float p[8][3];
    int i, k;

    for (i = 0; i < 8; i++) {
        p[i][0] = (i == 1 || i == 2 || i == 5 || i == 6) ? b->hi[0] : b->lo[0];
        p[i][1] = i >= 4 ? b->hi[1] : b->lo[1];
        p[i][2] = (i == 2 || i == 3 || i == 6 || i == 7) ? b->hi[2] : b->lo[2];
    }
    for (i = 0; i < 12; i++) for (k = 0; k < 3; k++) memcpy(t[i][k], p[f[i][k]], 12);
}

static int in_view(const float rows[16], const float g[3]) {
    float h[4];

    sh4der_psm_xform(rows, g, 1.0f, h);
    return h[3] > 0.0f && fabsf(h[0]) <= h[3] && fabsf(h[1]) <= h[3];
}

/* The route's shadow at a ground point: the three vertices of its cell triangle give (u, v), the PVR interpolates them in
   world space (perspective-correct, so linear in the ground), and reads the target. Returns -1 when a vertex has no lookup. */
static float route_lit(const sh4der_psm_t *map, float cell, const float g[3]) {
    const float cx = floorf(g[0] / cell), cz = floorf(g[2] / cell);
    const float fx = (g[0] - cx * cell) / cell, fz = (g[2] - cz * cell) / cell;
    float v00[3] = { cx * cell, 0.0f, cz * cell }, v10[3] = { (cx + 1.0f) * cell, 0.0f, cz * cell };
    float v11[3] = { (cx + 1.0f) * cell, 0.0f, (cz + 1.0f) * cell }, v01[3] = { cx * cell, 0.0f, (cz + 1.0f) * cell };
    const float *p0 = v00, *p1, *p2;
    float w0, w1, w2, uv[3][2], u, v;

    if (fx >= fz) { p1 = v10; p2 = v11; w0 = 1.0f - fx; w1 = fx - fz; w2 = fz; }
    else { p1 = v01; p2 = v11; w0 = 1.0f - fz; w1 = fz - fx; w2 = fx; }
    if (!sh4der_psm_pvr_uv(map, p0, &uv[0][0], &uv[0][1]) || !sh4der_psm_pvr_uv(map, p1, &uv[1][0], &uv[1][1]) ||
        !sh4der_psm_pvr_uv(map, p2, &uv[2][0], &uv[2][1])) return -1.0f;
    u = uv[0][0] * w0 + uv[1][0] * w1 + uv[2][0] * w2;
    v = uv[0][1] * w0 + uv[1][1] * w1 + uv[2][1] * w2;
    return 1.0f - sil_read(u, v);
}

/* Returns the share of checked ground points the route and the exact compare light differently; -1 when no map. */
static float route_vs_exact(int warped, const float sun_in[3], float cell, int *checked) {
    const sh4der_psm_cam_t c = make_cam();
    const box_t box = { { -2.0f, 0.0f, -14.0f }, { 2.0f, 3.0f, -10.0f } };
    sh4der_psm_t map;
    sh4der_psm_raster_t r;
    float tris[12][3][3], rows[16], lo, hi, gx, gz, corners[8][3];
    unsigned i, k, diff = 0;

    if (!build(&map, warped, sun_in)) return -1.0f;
    box_tris(&box, tris);
    memset(sil, 0, sizeof sil);
    for (i = 0; i < 12u; i++) {
        sh4der_psm_pvr_vtx_t out[SH4DER_PSM_PVR_MAX_TRIS][3];
        const unsigned n = sh4der_psm_pvr_tris(&map, MAP_N, tris[i][0], tris[i][1], tris[i][2], out);

        for (k = 0; k < n; k++) fill_tri(out[k]);
    }
    for (i = 0; i < 8u; i++) {
        corners[i][0] = (i == 1 || i == 2 || i == 5 || i == 6) ? box.hi[0] : box.lo[0];
        corners[i][1] = i >= 4u ? box.hi[1] : box.lo[1];
        corners[i][2] = (i == 2 || i == 3 || i == 6 || i == 7) ? box.hi[2] : box.lo[2];
    }
    if (!sh4der_psm_raster_key_range(map.light, &corners[0][0], 3u * sizeof(float), 8u, &lo, &hi)) return -1.0f;
    if (!sh4der_psm_raster_begin(&r, &map, dep, MAP_N, lo - 0.5f, hi + 0.5f)) return -1.0f;
    for (i = 0; i < 12u; i++) sh4der_psm_raster_tri(&r, tris[i][0], tris[i][1], tris[i][2]);
    CHECK(sh4der_psm_cam_rows(&c, WARP_NEAR, rows));
    *checked = 0;
    for (gz = -58.0f; gz < -3.0f; gz += 0.25f)
        for (gx = -30.0f; gx <= 30.0f; gx += 0.25f) {
            const float g[3] = { gx, 0.0f, gz };
            float exact, lit;

            if (!in_view(rows, g)) continue;
            lit = route_lit(&map, cell, g);
            if (lit < 0.0f) continue;
            exact = sh4der_psm_raster_lit(&r, g, 0.05f, 1);
            (*checked)++;
            if ((lit < 0.5f) != (exact < 0.5f)) diff++;
        }
    return *checked ? (float)diff / (float)*checked : 0.0f;
}

static void route_agreement(void) {
    static const float cells[3] = { 4.0f, 1.0f, 0.25f };
    unsigned s, k;

    printf("  share of ground points the silhouette route lights differently from the exact compare (%% of checked)\n");
    printf("  %-14s %-8s %-17s %-17s %-17s\n", "sun", "map", "cell 4 m", "cell 1 m", "cell 0.25 m");
    for (s = 0; s < 4u; s++) {
        float uni[3], war[3];
        int checked = 0, have_w = 1;

        for (k = 0; k < 3u; k++) {
            uni[k] = route_vs_exact(0, SUNS[s], cells[k], &checked);
            war[k] = route_vs_exact(1, SUNS[s], cells[k], &checked);
            if (war[k] < 0.0f) have_w = 0;
            CHECK(uni[k] >= 0.0f);
        }
        printf("  %-14s uniform  %6.2f%%            %6.2f%%            %6.2f%%\n", SUN_NAMES[s], 100.0f * uni[0], 100.0f * uni[1], 100.0f * uni[2]);
        if (have_w) {
            printf("  %-14s warp     %6.2f%%            %6.2f%%            %6.2f%%\n", "", 100.0f * war[0], 100.0f * war[1], 100.0f * war[2]);
            CHECK(war[2] <= war[0] + 1e-4f);                                       /* finer ground never makes it worse */
        }
        CHECK(uni[0] <= uni[2] + 0.02f);                                           /* a uniform map does not care about the cell */
    }
}

static void bad_input(void) {
    sh4der_psm_t map;
    sh4der_psm_pvr_vtx_t out[SH4DER_PSM_PVR_MAX_TRIS][3];
    const float a[3] = { 0.0f, 0.0f, -12.0f };
    float u, v;

    CHECK(build(&map, 0, SUNS[0]));
    CHECK(sh4der_psm_pvr_uv(&map, a, &u, &v));
    CHECK(sh4der_psm_pvr_uv_error(&map, MAP_N, a, a, a) < 1e-3f);                 /* a point: nothing to interpolate */
    CHECK(sh4der_psm_pvr_tris(&map, MAP_N, a, a, NULL, out) == 0u);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    caster_tris();
    lookup();
    interpolation_error();
    route_agreement();
    bad_input();
    if (fails) { fprintf(stderr, "psm_pvr_test: %d failure(s)\n", fails); return 1; }
    printf("psm_pvr_test: ok\n");
    return 0;
}
