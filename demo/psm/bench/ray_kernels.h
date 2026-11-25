/* SH4DER-PVR: the ray bench's kernels, shared by the console program (bench_ray.c) and its host test (ray_test.c).
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   THESIS_SH4_SHADOWS.md 5.3 (the walk) and 5.6 (the proxies), written twice each: a reference in plain C, and the kernel the
   bench times. The two must give the same bytes; both the host test and the console compare them. The maths is SH4ZAM's
   (shz_sincosf_deg, shz_sqrtf); with SH4ZAM's software back end the host runs the same steps.

   The grid is Junior's town grid (../town_ray_data.h, GENERATED): 64 x 64 cells of 1.5 m, 1 byte a cell, the tallest
   static caster over the cell in steps of TOWN_RAY_H / 255. The walk works in those byte units: a ray's height at t is
   y0 + t ybs, with y0 = p_y x 255 / H and ybs = sin e x 255 / H, so a cell's byte is compared with no conversion of the
   grid. The ray is clear once its height at a cell's exit is 255 or more (above the tallest caster).

   The test in a cell is 5.3's dark-side conservative one: shadowed if h > the ray's height where it enters the cell.
   A ray is counted one step for every cell it tests, the vertex's own cell included (n of 5.3).
*/

#ifndef RAY_KERNELS_H
#define RAY_KERNELS_H

#include <stdint.h>
#include <string.h>

#include "sh4zam/shz_sh4zam.h"
#include "../town_ray_data.h"

#define RAY_N 64
#define RAY_INV_C (1.0f / TOWN_RAY_CELL)
#define RAY_FAR 1e30f
#define RAY_LIT 255u

#if defined(__GNUC__)
#define RAY_NOINLINE __attribute__((noinline))
#else
#define RAY_NOINLINE
#endif

/* What every ray of one sun shares (5.3: "the rays are parallel, so these are shared"). */
typedef struct {
    float sx, sy, sz;           /* unit vector toward the sun, world (x east, y up, z) */
    float ybs;                  /* sin e x 255 / H: the ray's rise in grid bytes a metre of t */
    float kb;                   /* 255 / H */
    float inv_sx, inv_sz;       /* 1 / sx, 1 / sz; 0 where the ray runs along a wall */
    float dtx, dtz;             /* t from one x wall to the next, and z (RAY_FAR along a wall) */
    int si, sj;                 /* the step in i (x) and j (z): +1 or -1 */
} ray_sun_t;

/* Toward the sun: (sin az cos el, sin el, -cos az cos el), as psm_scene.h's f->sun and town_ray_data.h say. */
static inline void ray_sun_init(ray_sun_t *s, float az_deg, float el_deg) {
    const shz_sincos_t a = shz_sincosf_deg(az_deg), e = shz_sincosf_deg(el_deg);
    const float ax = a.sin * e.cos, az = -a.cos * e.cos;
    const float fx = ax < 0.0f ? -ax : ax, fz = az < 0.0f ? -az : az;

    s->sx = ax; s->sy = e.sin; s->sz = az;
    s->kb = 255.0f / TOWN_RAY_H;
    s->ybs = e.sin * s->kb;
    s->si = ax < 0.0f ? -1 : 1;
    s->sj = az < 0.0f ? -1 : 1;
    s->inv_sx = fx > 1e-6f ? 1.0f / ax : 0.0f;
    s->inv_sz = fz > 1e-6f ? 1.0f / az : 0.0f;
    s->dtx = fx > 1e-6f ? TOWN_RAY_CELL / fx : RAY_FAR;
    s->dtz = fz > 1e-6f ? TOWN_RAY_CELL / fz : RAY_FAR;
}

/* t from the point to the first wall of its cell in x (or z): the wall on the side the ray goes. */
static inline float ray_first_wall(int cell, int step, float pos, float origin, float inv) {
    const float wall = origin + (float)(cell + (step > 0)) * TOWN_RAY_CELL;

    return inv != 0.0f ? (wall - pos) * inv : RAY_FAR;
}

/* ---- the walk, reference: plain C, every quantity named, every bound checked as a signed index ---- */

/* Lit byte for the point (x, y, z): 0 shadowed by the grid, 255 lit. *steps = the cells tested. A point off the grid is
   lit with 0 steps. */
static RAY_NOINLINE unsigned ray_walk_ref(const uint8_t grid[RAY_N][RAY_N], const ray_sun_t *s, float x, float y, float z, unsigned *steps) {
    const float fx = (x - TOWN_RAY_X0) * RAY_INV_C, fz = (z - TOWN_RAY_Z0) * RAY_INV_C;
    const float y0 = y * s->kb;
    float tx, tz, t_entry = 0.0f, t_exit;
    int i, j;
    unsigned n = 0;

    *steps = 0;
    if (!(fx >= 0.0f && fz >= 0.0f && fx < (float)RAY_N && fz < (float)RAY_N)) return RAY_LIT;
    i = (int)fx; j = (int)fz;
    tx = ray_first_wall(i, s->si, x, TOWN_RAY_X0, s->inv_sx);
    tz = ray_first_wall(j, s->sj, z, TOWN_RAY_Z0, s->inv_sz);
    for (;;) {
        const float h = (float)grid[j][i];
        const float y_entry = y0 + t_entry * s->ybs;

        n++;
        if (h > y_entry) { *steps = n; return 0u; }
        t_exit = tx < tz ? tx : tz;
        if (y0 + t_exit * s->ybs >= 255.0f) { *steps = n; return RAY_LIT; }
        if (tx < tz) {
            i += s->si; t_entry = tx; tx += s->dtx;
            if (i < 0 || i >= RAY_N) { *steps = n; return RAY_LIT; }
        } else {
            j += s->sj; t_entry = tz; tz += s->dtz;
            if (j < 0 || j >= RAY_N) { *steps = n; return RAY_LIT; }
        }
    }
}

/* ---- the walk, the kernel the bench times: one pointer into the grid, the bounds as unsigned compares ---- */

/* The same arithmetic in the same order as ray_walk_ref (so the same bytes); no step count. */
static inline unsigned ray_walk(const uint8_t *grid, const ray_sun_t *s, float x, float y, float z) {
    const float fx = (x - TOWN_RAY_X0) * RAY_INV_C, fz = (z - TOWN_RAY_Z0) * RAY_INV_C;
    const float y0 = y * s->kb, ybs = s->ybs, dtx = s->dtx, dtz = s->dtz;
    const int si = s->si, sj = s->sj, dj = sj * RAY_N;
    float tx, tz, t_entry = 0.0f;
    const uint8_t *p;
    unsigned i, j;

    if (!(fx >= 0.0f && fz >= 0.0f && fx < (float)RAY_N && fz < (float)RAY_N)) return RAY_LIT;
    i = (unsigned)(int)fx; j = (unsigned)(int)fz;
    tx = ray_first_wall((int)i, si, x, TOWN_RAY_X0, s->inv_sx);
    tz = ray_first_wall((int)j, sj, z, TOWN_RAY_Z0, s->inv_sz);
    p = grid + j * RAY_N + i;
    for (;;) {
        if ((float)*p > y0 + t_entry * ybs) return 0u;
        if (tx < tz) {
            if (y0 + tx * ybs >= 255.0f) return RAY_LIT;
            i += (unsigned)si; p += si; t_entry = tx; tx += dtx;
            if (i >= (unsigned)RAY_N) return RAY_LIT;
        } else {
            if (y0 + tz * ybs >= 255.0f) return RAY_LIT;
            j += (unsigned)sj; p += dj; t_entry = tz; tz += dtz;
            if (j >= (unsigned)RAY_N) return RAY_LIT;
        }
    }
}

/* A slice: k points (records of 4 floats: x, y, z, unused) to k lit bytes. */
static RAY_NOINLINE void ray_slice(const uint8_t *grid, const ray_sun_t *s, const float (*pts)[4], uint8_t *out, unsigned k) {
    unsigned r;

    for (r = 0; r < k; r++) out[r] = (uint8_t)ray_walk(grid, s, pts[r][0], pts[r][1], pts[r][2]);
}

static RAY_NOINLINE void ray_slice_ref(const uint8_t grid[RAY_N][RAY_N], const ray_sun_t *s, const float (*pts)[4], uint8_t *out, uint8_t *steps, unsigned k) {
    unsigned r, n;

    for (r = 0; r < k; r++) { out[r] = (uint8_t)ray_walk_ref(grid, s, pts[r][0], pts[r][1], pts[r][2], &n); if (steps) steps[r] = (uint8_t)n; }
}

/* ---- the points ---- */

static inline uint32_t ray_rand(uint32_t *seed) { *seed = *seed * 1664525u + 1013904223u; return *seed >> 8; }
static inline float ray_frand(uint32_t *seed, float lo, float hi) { return lo + (hi - lo) * (float)ray_rand(seed) * (1.0f / 16777216.0f); }

/* Receivers for the walk. A STAND-IN, not the town's vertices (those are in the baked mesh, which this bench does not
   load): x and z uniform over the static casters' box (town_ray_data.h: x -38.3 .. 18.0, z -18.0 .. 35.0); half on the
   ground (y 0.05 m), half at a height uniform over 0 .. 13 m. A point inside a building's column hits its own cell (5.3's
   self-hit), as a wall vertex would without the step out along its normal. */
static inline void ray_points(float (*pts)[4], unsigned k, uint32_t seed) {
    unsigned r;

    for (r = 0; r < k; r++) {
        pts[r][0] = ray_frand(&seed, -38.3f, 18.0f);
        pts[r][2] = ray_frand(&seed, -18.0f, 35.0f);
        pts[r][1] = (r & 1u) ? ray_frand(&seed, 0.0f, 13.0f) : 0.05f;
        pts[r][3] = 0.0f;
    }
}

/* Points for P1's rays of known n on a grid of zeros: anywhere over the grid, at a height uniform over 0 .. H. Every
   ray is lit; its n is set by its height (the climb to H) or by the grid's edge, and the reference counts it. */
static inline void ray_points_any(float (*pts)[4], unsigned k, uint32_t seed) {
    unsigned r;

    for (r = 0; r < k; r++) {
        pts[r][0] = ray_frand(&seed, TOWN_RAY_X0, TOWN_RAY_X0 + RAY_N * TOWN_RAY_CELL);
        pts[r][2] = ray_frand(&seed, TOWN_RAY_Z0, TOWN_RAY_Z0 + RAY_N * TOWN_RAY_CELL);
        pts[r][1] = ray_frand(&seed, 0.0f, TOWN_RAY_H);
        pts[r][3] = 0.0f;
    }
}

/* ---- the proxies (5.6): a capsule a person, a sphere a hen ---- */

/* tan of the light's angular radius: the real sun's, 0.00465 rad (5.6, FACT). */
#define RAY_TAN_ALPHA 0.00465f

/* The bench's proxy sizes (its choice, from the movers' boxes in town_ray_data.h): a person 0.46 x 1.70 x 0.30 m is a
   capsule of radius 0.20 m from 0.20 m to 1.50 m over the feet; a hen 0.22 x 0.27 x 0.41 m is a sphere of radius 0.15 m at
   the middle of its box. */
#define RAY_PERSON_R 0.20f
#define RAY_HEN_R 0.15f

typedef struct {
    float a[3], b[3];           /* the capsule's two ends; a sphere has a == b */
    float r;
    float mx, mz;               /* the feet, for the receivers */
    float run;                  /* shadow run at the sun used, m (town_ray_data.h, DERIVED) */
    unsigned kind;              /* 0 a person, 1 a hen */
} ray_proxy_t;

/* The 12 movers at their load places (town_ray_data.h: x, z), with the 16:00 runs. */
static inline unsigned ray_proxies(ray_proxy_t *p) {
    unsigned m;

    for (m = 0; m < TOWN_RAY_MOVERS; m++) {
        const town_ray_mover_t *v = &town_ray_movers[m];

        memset(&p[m], 0, sizeof p[m]);
        p[m].kind = v->kind; p[m].mx = v->x; p[m].mz = v->z; p[m].run = v->run16;
        if (v->kind == 0) {
            p[m].r = RAY_PERSON_R;
            p[m].a[0] = p[m].b[0] = v->x; p[m].a[2] = p[m].b[2] = v->z;
            p[m].a[1] = RAY_PERSON_R; p[m].b[1] = v->hi[1] - RAY_PERSON_R;
        } else {
            p[m].r = RAY_HEN_R;
            p[m].a[0] = p[m].b[0] = v->x + 0.5f * (v->lo[0] + v->hi[0]);
            p[m].a[1] = p[m].b[1] = 0.5f * (v->lo[1] + v->hi[1]);
            p[m].a[2] = p[m].b[2] = v->z + 0.5f * (v->lo[2] + v->hi[2]);
        }
    }
    return TOWN_RAY_MOVERS;
}

static inline float ray_dot3(const float a[3], const float b[3]) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }

/* The closest approach of the ray from the point toward the sun to the proxy's axis: *t along the ray, the squared
   distance across it returned (5.6's capsule; a sphere is a capsule with a == b). */
static inline float ray_axis(const ray_proxy_t *c, const ray_sun_t *s, const float p[3], float *t) {
    const float sv[3] = { s->sx, s->sy, s->sz };
    const float a[3] = { c->a[0] - p[0], c->a[1] - p[1], c->a[2] - p[2] }, b[3] = { c->b[0] - p[0], c->b[1] - p[1], c->b[2] - p[2] };
    const float ta = ray_dot3(a, sv), tb = ray_dot3(b, sv);
    const float a1[3] = { a[0] - ta * sv[0], a[1] - ta * sv[1], a[2] - ta * sv[2] };
    const float e[3] = { (b[0] - tb * sv[0]) - a1[0], (b[1] - tb * sv[1]) - a1[1], (b[2] - tb * sv[2]) - a1[2] };
    const float ee = ray_dot3(e, e);
    float q = ee > 1e-12f ? -ray_dot3(a1, e) / ee : 0.0f;
    float d[3];

    q = q < 0.0f ? 0.0f : q > 1.0f ? 1.0f : q;
    d[0] = a1[0] + q * e[0]; d[1] = a1[1] + q * e[1]; d[2] = a1[2] + q * e[2];
    *t = ta + q * (tb - ta);
    return ray_dot3(d, d);
}

/* Lit byte from the squared distance across and the distance along: 5.6's linear ramp, u = (d - R) / wp. */
static inline unsigned ray_ramp(float d2, float t, float r) {
    const float wp = t * RAY_TAN_ALPHA;
    float d, u, lit;

    d2 = d2 > 0.0f ? d2 : 0.0f;
    d = shz_sqrtf(d2);
    u = (d - r) / wp;
    lit = 0.5f + 0.5f * u;
    lit = lit < 0.0f ? 0.0f : lit > 1.0f ? 1.0f : lit;
    return (unsigned)(lit * 255.0f + 0.5f);
}

/* Reference: the whole formula for every pair. */
static RAY_NOINLINE unsigned ray_proxy_ref(const ray_proxy_t *c, const ray_sun_t *s, const float p[3]) {
    float t;
    const float d2 = ray_axis(c, s, p, &t);

    if (t <= 0.0f) return RAY_LIT;
    return ray_ramp(d2, t, c->r);
}

/* The kernel: 5.6's two early outs before the square root; the same bytes. */
static inline unsigned ray_proxy(const ray_proxy_t *c, const ray_sun_t *s, const float p[3]) {
    float t, wp, hi, lo;
    const float d2 = ray_axis(c, s, p, &t);

    if (t <= 0.0f) return RAY_LIT;
    wp = t * RAY_TAN_ALPHA;
    hi = c->r + wp;
    if (d2 >= hi * hi) return RAY_LIT;
    lo = c->r - wp;
    if (lo > 0.0f && d2 <= lo * lo) return 0u;
    return ray_ramp(d2, t, c->r);
}

/* The receivers of one proxy: the 0.5 m ground map's points (5.3, a world-fixed map; y 0.02 m) in the box swept from
   the feet away from the sun to the end of the shadow run, widened by the radius and 0.5 m. Returns how many were
   written (at most max). */
static inline unsigned ray_run_points(const ray_proxy_t *c, const ray_sun_t *s, float (*pts)[4], unsigned max) {
    const float hl = shz_sqrtf(s->sx * s->sx + s->sz * s->sz);
    const float dx = -s->sx / hl, dz = -s->sz / hl;            /* the shadow's way on the ground: away from the sun */
    const float ex = c->mx + c->run * dx, ez = c->mz + c->run * dz, pad = c->r + 0.5f;
    const float x0 = (c->mx < ex ? c->mx : ex) - pad, x1 = (c->mx > ex ? c->mx : ex) + pad;
    const float z0 = (c->mz < ez ? c->mz : ez) - pad, z1 = (c->mz > ez ? c->mz : ez) + pad;
    const int i0 = (int)((x0 + 100.0f) * 2.0f), i1 = (int)((x1 + 100.0f) * 2.0f);
    const int j0 = (int)((z0 + 100.0f) * 2.0f), j1 = (int)((z1 + 100.0f) * 2.0f);
    unsigned n = 0;
    int i, j;

    for (j = j0; j <= j1; j++)
        for (i = i0; i <= i1; i++) {
            const float x = (float)i * 0.5f - 100.0f + 0.25f, z = (float)j * 0.5f - 100.0f + 0.25f;
            /* inside the swept box: along the run 0 .. run, across within pad */
            const float along = (x - c->mx) * dx + (z - c->mz) * dz, across = (x - c->mx) * dz - (z - c->mz) * dx;

            if (along < -pad || along > c->run + pad || across < -pad || across > pad) continue;
            if (n < max) { pts[n][0] = x; pts[n][1] = 0.02f; pts[n][2] = z; pts[n][3] = 0.0f; }
            n++;
        }
    return n < max ? n : max;
}

/* The proxy pass: each proxy against its own receivers only (5.6: the cost is the sum of V_j x C_pair). out[] takes the
   min with what is there (Lit_total = min of the casters). first[m] .. first[m + 1] are proxy m's receivers. */
static RAY_NOINLINE void ray_proxy_pass(const ray_proxy_t *c, unsigned np, const unsigned *first, const ray_sun_t *s, const float (*pts)[4], uint8_t *out) {
    unsigned m, r;

    for (m = 0; m < np; m++)
        for (r = first[m]; r < first[m + 1u]; r++) {
            const unsigned l = ray_proxy(&c[m], s, pts[r]);

            if (l < out[r]) out[r] = (uint8_t)l;
        }
}

static RAY_NOINLINE void ray_proxy_pass_ref(const ray_proxy_t *c, unsigned np, const unsigned *first, const ray_sun_t *s, const float (*pts)[4], uint8_t *out) {
    unsigned m, r;

    for (m = 0; m < np; m++)
        for (r = first[m]; r < first[m + 1u]; r++) {
            const unsigned l = ray_proxy_ref(&c[m], s, pts[r]);

            if (l < out[r]) out[r] = (uint8_t)l;
        }
}

#endif /* RAY_KERNELS_H */
