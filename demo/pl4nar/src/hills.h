/* The hills build (PL4NAR_HILLS, with PL4NAR_MAP): a height field under the loop, flattened at the structures, and the
 * heights of the meshes as they are drawn. Everything that stands on the ground takes its height from the triangles
 * (barycentric), not from the smooth field: on a 20 m facet the field and the triangle part by up to a few tenths of a
 * metre, so a car on the field would float or sink.
 * Included by demo.c (and the host tools) once; all static. SH4ZAM maths (shz_sincosf, shz_invf, shz_fmaf,
 * shz_saturatef); hand arithmetic is scalar a * b + c only. hills_init() runs once at load: the per frame calls are
 * table reads and one triangle a lookup. */
#ifndef HILLS_H
#define HILLS_H
#include "map.h"
#include "map_gen.h"
#include "sh4der_core.h"

#define HILLS_GX     18u           /* the ground grid of draw_road: 20 m cells, x -180 .. 180, z -260 .. 260 */
#define HILLS_GZ     26u
#define HILLS_X0     -180.0f
#define HILLS_Z0     -260.0f
#define HILLS_CELL   20.0f
#define HILLS_DROP   0.55f         /* the grid is let down this far round the road (the road's chord stays above it) */
#define HILLS_DROP_W 20.0f         /* past the band's edge, the drop goes to nothing over this */
#define HILLS_FALL   60.0f         /* a structure's flat ground goes back to the hills over this */
#define HILLS_MAX_FLAT 24u
#ifndef HILLS_FAR_GAIN
#define HILLS_FAR_GAIN 1.0f        /* the ground away from the road, x this (build flag: 1.5 gives about 3.5 m); the road is not touched */
#endif
#define HILLS_FAR_FROM 32.0f       /* the gain starts this far from the centre line (past the drop) */
#define HILLS_FAR_OVER 40.0f       /* and is whole this much further out */

extern const mdl_t mdl_map_overpass, mdl_map_footbridge, mdl_map_gantry, mdl_map_passage;

typedef struct hills_flat { float x, z, r; } hills_flat_t;

static float        hills_gy[HILLS_GZ + 1u][HILLS_GX + 1u];   /* the grid's corners, as drawn */
static float        hills_ry[MAP_ROAD_V];                     /* the road strip's vertices, as drawn */
static float        hills_iy[MAP_INST];                       /* each instance's base: the lowest ground under its box */
static float        hills_ly[MAP_LAMPS];                      /* the ground under each lamp head */
static hills_flat_t hills_flat[HILLS_MAX_FLAT];
static unsigned     hills_nflat;

static float hills_smooth(float t) { t = shz_saturatef(t); return t * t * shz_fmaf(-2.0f, t, 3.0f); }

/* The field before the flattening: two crossed waves and one on a diagonal, at most about 2.6 m, 120 .. 170 m long. */
static float hills_raw(float x, float z) {
    const shz_sincos_t a = shz_sincosf(shz_fmaf(x, 6.2831853f / 150.0f, 0.7f));
    const shz_sincos_t b = shz_sincosf(shz_fmaf(z, 6.2831853f / 170.0f, 0.3f));
    const shz_sincos_t c = shz_sincosf(shz_fmaf(shz_fmaf(x, 0.6f, z * 0.8f), 6.2831853f / 120.0f, 1.3f));
    return shz_fmaf(1.6f * a.sin, b.cos, 1.0f * c.sin);
}

/* 1 in the open, 0 on a structure's flat ground. */
static float hills_env(float x, float z) {
    float e = 1.0f;
    unsigned k;
    for (k = 0; k < hills_nflat; k++) {
        const float dx = x - hills_flat[k].x, dz = z - hills_flat[k].z;
        const float d = shz_sqrtf(shz_fmaf(dx, dx, dz * dz));
        const float f = hills_smooth((d - hills_flat[k].r) * (1.0f / HILLS_FALL));
        if (f < e) e = f;
    }
    return e;
}

static inline float hills_h(float x, float z) { return hills_env(x, z) * hills_raw(x, z); }

/* The height on triangle a, b, c (x, y, z each) at (x, z); 0 when (x, z) is outside it. */
static int hills_tri(float x, float z, const float *a, const float *b, const float *c, float *y) {
    const float e1x = b[0] - a[0], e1z = b[2] - a[2], e2x = c[0] - a[0], e2z = c[2] - a[2], px = x - a[0], pz = z - a[2];
    const float det = shz_fmaf(e1x, e2z, -e2x * e1z);
    float u, v, inv;
    if (shz_fabsf(det) < 1e-6f) return 0;
    inv = shz_invf(det);
    u = shz_fmaf(px, e2z, -e2x * pz) * inv;
    v = shz_fmaf(e1x, pz, -px * e1z) * inv;
    if (u < -1e-4f || v < -1e-4f || u + v > 1.0001f) return 0;
    *y = shz_fmaf(u, b[1] - a[1], shz_fmaf(v, c[1] - a[1], a[1]));
    return 1;
}

/* The grid as draw_road splits it: corners q0 (x0, z1), q1 (x1, z1), q2 (x1, z0), q3 (x0, z0); triangles q0 q1 q2,
   q0 q2 q3 (demo.c quad()). */
static float hills_grid_y(float x, float z) {
    int i = (int)shz_floorf((x - HILLS_X0) * (1.0f / HILLS_CELL)), j = (int)shz_floorf((z - HILLS_Z0) * (1.0f / HILLS_CELL));
    float q[4][3], y;
    if (i < 0) i = 0;
    if (j < 0) j = 0;
    if (i > (int)HILLS_GX - 1) i = HILLS_GX - 1;
    if (j > (int)HILLS_GZ - 1) j = HILLS_GZ - 1;
    {
        const float x0 = HILLS_X0 + HILLS_CELL * (float)i, z0 = HILLS_Z0 + HILLS_CELL * (float)j;
        q[0][0] = x0;              q[0][1] = hills_gy[j + 1][i];     q[0][2] = z0 + HILLS_CELL;
        q[1][0] = x0 + HILLS_CELL; q[1][1] = hills_gy[j + 1][i + 1]; q[1][2] = z0 + HILLS_CELL;
        q[2][0] = x0 + HILLS_CELL; q[2][1] = hills_gy[j][i + 1];     q[2][2] = z0;
        q[3][0] = x0;              q[3][1] = hills_gy[j][i];         q[3][2] = z0;
    }
    if (hills_tri(x, z, q[0], q[1], q[2], &y)) return y;
    if (hills_tri(x, z, q[0], q[2], q[3], &y)) return y;
    return q[3][1];                                   /* off the grid: its corner */
}

/* Road quad k (rows 2k .. 2k + 3) as draw_road sends it: q0 = row 2k + 2, q1 = 2k + 3, q2 = 2k + 1, q3 = 2k. */
static int hills_road_quad_y(unsigned k, float x, float z, float *y) {
    float q[4][3];
    const unsigned r[4] = { 2u * k + 2u, 2u * k + 3u, 2u * k + 1u, 2u * k };
    unsigned c;
    if (2u * k + 3u >= MAP_ROAD_V) return 0;
    for (c = 0; c < 4u; c++) { q[c][0] = map_road[r[c]][0]; q[c][1] = hills_ry[r[c]]; q[c][2] = map_road[r[c]][1]; }
    return hills_tri(x, z, q[0], q[1], q[2], y) || hills_tri(x, z, q[0], q[2], q[3], y);
}

static unsigned hills_seg(float s) {
    unsigned lo = 0u, hi = MAP_PATH_N - 1u;
    s = map_wrap(s);
    while (hi - lo > 1u) {
        const unsigned mid = (lo + hi) >> 1;
        if (map_path[mid][0] <= s) lo = mid; else hi = mid;
    }
    return lo;
}

/* The ground's height at (s, lat) on the loop (the car, the traffic): the road quad that holds it, or its neighbours,
   else the grid (off the band). */
static inline float hills_y_sl(float s, float lat) {
    const map_pose_t p = map_at(s, lat);
    const unsigned k = hills_seg(s);
    float y;
    if (shz_fabsf(lat) <= MAP_BAND + 0.5f) {
        if (hills_road_quad_y(k, p.x, p.z, &y)) return y;
        if (k > 0u && hills_road_quad_y(k - 1u, p.x, p.z, &y)) return y;
        if (hills_road_quad_y(k + 1u < MAP_PATH_N - 1u ? k + 1u : 0u, p.x, p.z, &y)) return y;
    }
    return hills_grid_y(p.x, p.z);
}

/* The ground's height at any (x, z): a road quad (all of them: load time only), else the grid. */
static float hills_y_xz(float x, float z) {
    unsigned k;
    float y;
    for (k = 0; k + 1u < MAP_PATH_N; k++) if (hills_road_quad_y(k, x, z, &y)) return y;
    return hills_grid_y(x, z);
}

/* The distance from (x, z) to the centre line: load time only. */
static float hills_centre_d(float x, float z) {
    float best = 1e9f;
    unsigned k;
    for (k = 0; k + 1u < MAP_PATH_N; k++) {
        const float *a = map_path[k], *b = map_path[k + 1u];
        const float dx = b[1] - a[1], dz = b[2] - a[2], L2 = shz_fmaf(dx, dx, dz * dz);
        const float t = shz_saturatef(shz_fmaf(x - a[1], dx, (z - a[2]) * dz) * shz_invf(L2));
        const float ex = x - shz_fmaf(dx, t, a[1]), ez = z - shz_fmaf(dz, t, a[2]), d2 = shz_fmaf(ex, ex, ez * ez);
        if (d2 < best) best = d2;
    }
    return shz_sqrtf(best);
}

static void hills_init(void) {
    unsigned i, j;
    hills_nflat = 0u;
    for (i = 0; i < MAP_INST && hills_nflat < HILLS_MAX_FLAT; i++) {   /* the structures' flat ground */
        const mdl_t *m = map_models[map_inst[i].model];
        float r = 0.0f;
        if (m == &mdl_map_overpass) r = 34.0f;
        else if (m == &mdl_map_footbridge) r = 14.0f;
        else if (m == &mdl_map_gantry) r = 12.0f;
        else if (m == &mdl_map_passage) r = 14.0f;
        if (r > 0.0f) { hills_flat[hills_nflat].x = map_inst[i].x; hills_flat[hills_nflat].z = map_inst[i].z; hills_flat[hills_nflat].r = r; hills_nflat++; }
    }
    for (i = 0; i < MAP_ROAD_V; i++) hills_ry[i] = hills_h(map_road[i][0], map_road[i][1]);
    for (j = 0; j <= HILLS_GZ; j++)
        for (i = 0; i <= HILLS_GX; i++) {
            const float x = HILLS_X0 + HILLS_CELL * (float)i, z = HILLS_Z0 + HILLS_CELL * (float)j;
            const float d = hills_centre_d(x, z), near = 1.0f - hills_smooth((d - MAP_BAND) * (1.0f / HILLS_DROP_W));
            const float far = hills_smooth((d - HILLS_FAR_FROM) * (1.0f / HILLS_FAR_OVER));
            hills_gy[j][i] = hills_h(x, z) * shz_fmaf(HILLS_FAR_GAIN - 1.0f, far, 1.0f) - HILLS_DROP * near - 0.03f;   /* -0.03: the flat build's grid sat there too */
        }
    for (i = 0; i < MAP_INST; i++) {                  /* a base on the lowest ground under the box: nothing floats */
        const map_inst_t *p = &map_inst[i];
        const mdl_t *m = map_models[p->model];
        const shz_sincos_t sc = shz_sincosf(p->yaw);
        float lo = 1e9f;
        unsigned c;
        for (c = 0; c < 5u; c++) {                    /* the four corners and the middle */
            const float lx = c == 4u ? 0.5f * (m->lo[0] + m->hi[0]) : (c & 1u) ? m->hi[0] : m->lo[0];
            const float lz = c == 4u ? 0.5f * (m->lo[2] + m->hi[2]) : (c & 2u) ? m->hi[2] : m->lo[2];
            const float y = hills_y_xz(shz_fmaf(lx, sc.cos, shz_fmaf(lz, sc.sin, p->x)), shz_fmaf(-lx, sc.sin, shz_fmaf(lz, sc.cos, p->z)));
            if (y < lo) lo = y;
        }
        hills_iy[i] = lo;
    }
    for (i = 0; i < MAP_LAMPS; i++) hills_ly[i] = hills_y_xz(map_lamp[i][0], map_lamp[i][1]);
}

/* The drawn ground triangles round (x, z) within r: the road quads from s - r - 6 to s + r + 6 (two triangles each, as
   draw_road splits them), then the grid cells under the box (the grid lies under the road: where both are, the road's
   depth hides the grid's). A triangle whose box misses the square is left out. Returns the count (at most max). */
#define HILLS_NEAR_MAX 48u
static unsigned hills_tris_near(float s, float x, float z, float r, float (*out)[3][3], unsigned max) {
    unsigned n = 0, k, c, steps;
    int i, j, i0, i1, j0, j1;
    float q[4][3];
    k = hills_seg(s - r - 6.0f);
    steps = (hills_seg(s + r + 6.0f) + (MAP_PATH_N - 1u) - k) % (MAP_PATH_N - 1u) + 1u;   /* across the loop's seam too */
    for (; steps > 0u; steps--, k = (k + 1u) % (MAP_PATH_N - 1u)) {
        const unsigned rr[4] = { 2u * k + 2u, 2u * k + 3u, 2u * k + 1u, 2u * k };
        float lx = 1e9f, hx = -1e9f, lz = 1e9f, hz = -1e9f;
        for (c = 0; c < 4u; c++) {
            q[c][0] = map_road[rr[c]][0]; q[c][1] = hills_ry[rr[c]]; q[c][2] = map_road[rr[c]][1];
            lx = shz_fminf(lx, q[c][0]); hx = shz_fmaxf(hx, q[c][0]); lz = shz_fminf(lz, q[c][2]); hz = shz_fmaxf(hz, q[c][2]);
        }
        if (hx < x - r || lx > x + r || hz < z - r || lz > z + r) continue;
        for (c = 0; c < 3u; c++) { if (n < max) { out[n][0][c] = q[0][c]; out[n][1][c] = q[1][c]; out[n][2][c] = q[2][c]; } }
        n += n < max;
        for (c = 0; c < 3u; c++) { if (n < max) { out[n][0][c] = q[0][c]; out[n][1][c] = q[2][c]; out[n][2][c] = q[3][c]; } }
        n += n < max;
    }
    i0 = (int)shz_floorf((x - r - HILLS_X0) * (1.0f / HILLS_CELL)); i1 = (int)shz_floorf((x + r - HILLS_X0) * (1.0f / HILLS_CELL));
    j0 = (int)shz_floorf((z - r - HILLS_Z0) * (1.0f / HILLS_CELL)); j1 = (int)shz_floorf((z + r - HILLS_Z0) * (1.0f / HILLS_CELL));
    if (i0 < 0) i0 = 0;
    if (j0 < 0) j0 = 0;
    if (i1 > (int)HILLS_GX - 1) i1 = HILLS_GX - 1;
    if (j1 > (int)HILLS_GZ - 1) j1 = HILLS_GZ - 1;
    for (j = j0; j <= j1; j++)
        for (i = i0; i <= i1; i++) {
            const float x0 = HILLS_X0 + HILLS_CELL * (float)i, z0 = HILLS_Z0 + HILLS_CELL * (float)j;
            q[0][0] = x0;              q[0][1] = hills_gy[j + 1][i];     q[0][2] = z0 + HILLS_CELL;
            q[1][0] = x0 + HILLS_CELL; q[1][1] = hills_gy[j + 1][i + 1]; q[1][2] = z0 + HILLS_CELL;
            q[2][0] = x0 + HILLS_CELL; q[2][1] = hills_gy[j][i + 1];     q[2][2] = z0;
            q[3][0] = x0;              q[3][1] = hills_gy[j][i];         q[3][2] = z0;
            for (c = 0; c < 3u; c++) { if (n < max) { out[n][0][c] = q[0][c]; out[n][1][c] = q[1][c]; out[n][2][c] = q[2][c]; } }
            n += n < max;
            for (c = 0; c < 3u; c++) { if (n < max) { out[n][0][c] = q[0][c]; out[n][1][c] = q[2][c]; out[n][2][c] = q[3][c]; } }
            n += n < max;
        }
    return n;
}

/* The loop's s nearest (x, z) (load time). */
static float hills_s_of(float x, float z) {
    float best = 1e18f, bs = 0.0f;
    unsigned k;
    for (k = 0; k + 1u < MAP_PATH_N; k++) {
        const float *a = map_path[k], *b = map_path[k + 1u];
        const float dx = b[1] - a[1], dz = b[2] - a[2];
        const float t = shz_saturatef(shz_fmaf(x - a[1], dx, (z - a[2]) * dz) * shz_invf(shz_fmaf(dx, dx, dz * dz)));
        const float ex = x - shz_fmaf(dx, t, a[1]), ez = z - shz_fmaf(dz, t, a[2]), d2 = shz_fmaf(ex, ex, ez * ez);
        if (d2 < best) { best = d2; bs = shz_lerpf(a[0], b[0], t); }
    }
    return bs;
}

/* A car's pose on the ground at (s, lat) facing yaw (world): y and the slopes along its length and across it, from four
   points on the triangles (1.3 m ahead and behind, 0.8 m to each side). */
typedef struct hills_tilt { float y, pitch, roll; } hills_tilt_t;   /* pitch: rise a metre forward; roll: rise a metre right */

static inline hills_tilt_t hills_car(float s, float lat, float yaw) {
    const map_pose_t p = map_at(s, 0.0f);
    const shz_sincos_t d = shz_sincosf(yaw - p.yaw);  /* the car's heading against the road's */
    hills_tilt_t t;
    /* in (s, lat), lat along the road's right (cos yaw, -sin yaw): the car's forward is (cos d, sin d) (demo_update:
       x grows with sin h), its right (cos wh, -sin wh) is (-sin d, cos d) */
    const float fs = d.cos, fl = d.sin, rs = -d.sin, rl = d.cos;
    const float yf = hills_y_sl(shz_fmaf(fs, 1.3f, s), shz_fmaf(fl, 1.3f, lat)), yb = hills_y_sl(shz_fmaf(fs, -1.3f, s), shz_fmaf(fl, -1.3f, lat));
    const float yr = hills_y_sl(shz_fmaf(rs, 0.8f, s), shz_fmaf(rl, 0.8f, lat)), yl = hills_y_sl(shz_fmaf(rs, -0.8f, s), shz_fmaf(rl, -0.8f, lat));
    t.y = 0.25f * (yf + yb + yr + yl);
    t.pitch = (yf - yb) * (1.0f / 2.6f);
    t.roll = (yr - yl) * (1.0f / 1.6f);
    return t;
}
#endif
