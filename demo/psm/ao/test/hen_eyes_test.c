/* SH4DER-PVR: host test of hen_eyes.h and hen_sweep.h, and the cost tables that set them side by side

   demo/psm/ao/test/hen_eyes_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Build (from the repository's root), plain C:
     cc -O2 -std=gnu11 -Wall -Wextra -DSH4DER_CORE_NO_SH4ZAM -Iinclude -Idemo/psm/ao
        demo/psm/ao/test/hen_eyes_test.c -lm -o hen_eyes_test
   SH4ZAM's host path: drop -DSH4DER_CORE_NO_SH4ZAM and add -Ithird_party/sh4zam/include.
   With -DHEN_TEST_ROOM -I<dir of room_mesh.h> the room is room_data.h's in place of the scripted one.
   Run: hen_eyes_test            the checks; exit 1 when one fails
        hen_eyes_test --report   the checks, then the tables (markdown)

   Every count is exact and the same on every run. Every cycle figure is hen_cost_t's or hen_sweep_cost_t's model:
   ESTIMATE, not a console time. */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef HEN_TEST_ROOM
#define ROOM_NO_AO
#include "room_data.h"
#endif
#include "hen_eyes.h"
#include "hen_sweep.h"

static unsigned checks, failed;
#define CHECK(c, ...) do { checks++; if (!(c)) { failed++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static uint32_t rs = 12345u;
static float rnd(uint32_t *r) { *r = *r * 1664525u + 1013904223u; return (float)(*r >> 8) * (1.0f / 16777216.0f); }
static float rr(float lo, float hi) { return lo + (hi - lo) * rnd(&rs); }

/* ---- the scripted room: metres, y up, floor -3 .. 3 --------------------------------------------------------------- */

#define TRI_CAP 4096u
static hen_tri_t tri_store[TRI_CAP];
static hen_world_t W;
#define EYE_Y 0.20f
#define TOP_Y 0.35f

#ifndef HEN_TEST_ROOM
static float bv[512][3];
static uint16_t bt[768][3];
static unsigned nbv, nbt;

static void box(float x0, float x1, float y0, float y1, float z0, float z1) {
    static const uint8_t q[6][4] = { { 0, 2, 3, 1 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 6, 7, 3 }, { 0, 4, 6, 2 }, { 1, 3, 7, 5 } };
    unsigned k;

    for (k = 0; k < 8u; k++) { bv[nbv + k][0] = k & 1u ? x1 : x0; bv[nbv + k][1] = k & 2u ? y1 : y0; bv[nbv + k][2] = k & 4u ? z1 : z0; }
    for (k = 0; k < 6u; k++) {
        bt[nbt][0] = (uint16_t)(nbv + q[k][0]); bt[nbt][1] = (uint16_t)(nbv + q[k][1]); bt[nbt][2] = (uint16_t)(nbv + q[k][2]); nbt++;
        bt[nbt][0] = (uint16_t)(nbv + q[k][0]); bt[nbt][1] = (uint16_t)(nbv + q[k][2]); bt[nbt][2] = (uint16_t)(nbv + q[k][3]); nbt++;
    }
    nbv += 8u;
}
static void done(void) { CHECK(hen_world_add(&W, (const float (*)[3])bv, (const uint16_t (*)[3])bt, nbt), "object not added"); nbv = nbt = 0; }

static void legs(float cx, float cz, float hx, float hz, float half, float top) {
    int sx, sz;
    for (sx = -1; sx <= 1; sx += 2) for (sz = -1; sz <= 1; sz += 2) box(cx + sx * hx - half, cx + sx * hx + half, 0.0f, top, cz + sz * hz - half, cz + sz * hz + half);
}
#endif

static void scene(void) {
#ifdef HEN_TEST_ROOM
    CHECK(hen_world_from_room(&W, tri_store, TRI_CAP, EYE_Y, TOP_Y) > 0u, "no room objects");
#else
    hen_world_begin(&W, tri_store, TRI_CAP, EYE_Y, TOP_Y);
    box(-3.2f, -3.0f, 0.0f, 2.8f, -3.2f, 3.0f); done();                                       /* wall_left */
    box(-3.0f, 3.0f, 0.0f, 2.8f, -3.2f, -3.0f); done();                                       /* wall_right */
    legs(0.2f, -2.42f, 1.05f, 0.36f, 0.04f, 0.13f);                                          /* sofa: legs, base, back, arms */
    box(-0.95f, 1.35f, 0.12f, 0.48f, -2.885f, -1.955f); box(-0.75f, 1.15f, 0.31f, 0.86f, -2.885f, -2.62f);
    box(-0.95f, -0.72f, 0.31f, 0.65f, -2.885f, -1.955f); box(1.12f, 1.35f, 0.31f, 0.65f, -2.885f, -1.955f); done();
    legs(0.2f, -0.75f, 0.58f, 0.23f, 0.025f, 0.39f);                                         /* coffee table: legs, shelf, top */
    box(-0.40f, 0.80f, 0.14f, 0.17f, -1.00f, -0.50f); box(-0.45f, 0.85f, 0.385f, 0.42f, -1.05f, -0.45f); done();
    legs(-0.50f, 0.40f, 0.20f, 0.20f, 0.025f, 0.14f); box(-0.775f, -0.225f, 0.12f, 0.42f, 0.125f, 0.675f); done();   /* ottoman_a */
    legs(0.95f, 0.32f, 0.18f, 0.18f, 0.025f, 0.14f); box(0.695f, 1.205f, 0.12f, 0.42f, 0.065f, 0.575f); done();     /* ottoman_b */
    box(-2.27f, -2.23f, 0.0f, 1.2f, -2.27f, -2.23f); box(-2.50f, -2.00f, 1.36f, 1.70f, -2.50f, -2.00f); done();      /* lamp: a pole and a shade */
    box(2.12f, 2.48f, 0.0f, 0.42f, -2.53f, -2.17f); done();                                   /* plant_a's pot */
    box(-2.60f, -2.30f, 0.0f, 0.33f, 0.75f, 1.05f); done();                                   /* plant_b's pot */
    box(1.88f, 1.92f, 0.0f, 1.2f, 1.58f, 1.62f); done();                                      /* a thin post, 4 cm */
    hen_world_floor(&W, -3.0f, 3.0f, -3.0f, 3.0f, 0.0f);
#endif
}

/* ---- references in double ---------------------------------------------------------------------------------------- */

static double tri_double(const hen_tri_t *t, const double o[3], const double d[3]) {
    const double e1[3] = { t->e1[0], t->e1[1], t->e1[2] }, e2[3] = { t->e2[0], t->e2[1], t->e2[2] };
    const double h[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
    const double det = e1[0] * h[0] + e1[1] * h[1] + e1[2] * h[2], s[3] = { o[0] - t->a[0], o[1] - t->a[1], o[2] - t->a[2] };
    const double q[3] = { s[1] * e1[2] - s[2] * e1[1], s[2] * e1[0] - s[0] * e1[2], s[0] * e1[1] - s[1] * e1[0] };
    double u, v, w;

    if (fabs(det) < 1e-12) return 1e30;
    u = (s[0] * h[0] + s[1] * h[1] + s[2] * h[2]) / det; v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) / det;
    w = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) / det;
    return (u < 0 || u > 1 || v < 0 || u + v > 1 || w <= HEN_TRI_EPS) ? 1e30 : w;
}

static double world_double(const double o[3], const double d[3], double far) {
    double best = far;
    unsigned i;
    for (i = 0; i < W.ntri; i++) { const double t = tri_double(&W.tri[i], o, d); if (t < best) best = t; }
    return best;
}

/* ---- the hens' own walk: peck, step, run, inside their bounds (what the eyes must leave alone) -------------------- */

#define HENS_MAX 32u
#define WALK 0.5f
#define RUN 2.0f

typedef struct { float x, z, bearing, t, b[4]; int mode; uint32_t rng; } hen_t;

static void wander(hen_t *m, float dt, int flee) {
    float sb, cb;

    if (flee && m->mode != 2) { m->mode = 2; m->t = 0.6f; }
    m->t -= dt;
    switch (m->mode) {
        case 0: if (m->t <= 0.0f) { m->mode = 1; m->t = 0.4f; m->bearing += (rnd(&m->rng) - 0.5f) * 2.0f; } break;
        case 1: case 2:
            sh4der_sincos_(m->bearing, &sb, &cb);
            m->x += sb * (m->mode == 2 ? RUN : WALK) * dt; m->z += cb * (m->mode == 2 ? RUN : WALK) * dt;
            if (m->t <= 0.0f) { m->mode = 0; m->t = 0.4f; }
            break;
        default: m->mode = 0; m->t = 0.4f; break;
    }
    if (m->x < m->b[0] || m->x > m->b[1]) { m->x = sh4der_clamp_(m->x, m->b[0], m->b[1]); m->bearing = -m->bearing; }
    if (m->z < m->b[2] || m->z > m->b[3]) { m->z = sh4der_clamp_(m->z, m->b[2], m->b[3]); m->bearing = HEN_PI - m->bearing; }
    if (m->bearing > HEN_PI) m->bearing -= 2.0f * HEN_PI;
    if (m->bearing < -HEN_PI) m->bearing += 2.0f * HEN_PI;
}

/* how far a circle at (x, z) is into the room's footprints (0 or less: clear); *inside: its centre is in one */
static float room_pen(float x, float z, float rad, int *inside) {
    float worst = -1e30f;
    unsigned k;

    for (k = 0; k < W.nobj; k++) {
        const float *f = W.obj[k].foot;
        const float cx = sh4der_clamp_(x, f[0], f[1]), cz = sh4der_clamp_(z, f[2], f[3]), d = sqrtf((x - cx) * (x - cx) + (z - cz) * (z - cz));
        if (!W.obj[k].has_foot) continue;
        if (d < 1e-6f && inside) *inside = 1;
        if (rad - d > worst) worst = rad - d;
    }
    return worst;
}

static void place(hen_t *h, unsigned n, uint32_t seed) {
    unsigned i, j, tries;

    for (i = 0; i < n; i++) {
        for (tries = 0; tries < 10000u; tries++) {
            const float x = -2.6f + 5.2f * rnd(&seed), z = -2.6f + 5.2f * rnd(&seed);
            int ok = room_pen(x, z, 0.30f, NULL) < 0.0f;
            for (j = 0; j < i && ok; j++) ok = (h[j].x - x) * (h[j].x - x) + (h[j].z - z) * (h[j].z - z) > 0.45f * 0.45f;
            if (!ok) continue;
            memset(&h[i], 0, sizeof h[i]);
            h[i].x = x; h[i].z = z; h[i].bearing = (rnd(&seed) - 0.5f) * 2.0f * HEN_PI; h[i].t = 0.4f * rnd(&seed); h[i].rng = seed + i * 7919u;
            if (i % 3u == 2u) { h[i].b[0] = x - 0.6f; h[i].b[1] = x + 0.6f; h[i].b[2] = z - 0.6f; h[i].b[3] = z + 0.6f; }   /* some keep to a small area */
            else { h[i].b[0] = -2.8f; h[i].b[1] = 2.8f; h[i].b[2] = -2.8f; h[i].b[3] = 2.8f; }
            break;
        }
        CHECK(tries < 10000u, "no free place for hen %u", i);
    }
}

/* ---- one scripted run -------------------------------------------------------------------------------------------- */

enum { M_NONE, M_PUSH, M_RAYS, M_SWEEP };

typedef struct {
    int mode, sweep_mode, every_frame, look, react;
    unsigned n, rays;
    float tick, dt, secs;                     /* dt 0: frames of 23 .. 75 ms in a fixed pattern */
} sim_t;

typedef struct {
    hen_stats_t st;
    hen_sweep_stats_t sw;
    double cyc, cyc_worst, shared;            /* the mode's own cycles, total and the worst frame; the push and the separation */
    unsigned long frames, through, worst_ticks, seen;
    float pen_room, pen_hen, pushed, walked, turn_worst, secs;
    uint32_t hash;
} out_t;

#define FRAMES_MAX 4096u
static float traj[2][FRAMES_MAX][HENS_MAX][2];
static uint16_t grid_start[32 * 32 + 1], grid_item[16384], grid_stamp[TRI_CAP];
static hen_grid_t G;
static const float viewer[3] = { 2.0f, 1.5f, 2.0f };

static void run(const sim_t *s, out_t *o, int keep) {
    static const float pattern[5] = { 0.023f, 0.033f, 0.050f, 0.075f, 0.040f };
    hen_t h[HENS_MAX];
    hen_eyes_t e[HENS_MAX];
    hen_eyes_cfg_t c;
    hen_cost_t cost;
    hen_sweep_cost_t scost;
    float pos[HENS_MAX][2], t = 0.0f;
    uint32_t seed = 4242u;
    unsigned i, j, f = 0;

    memset(o, 0, sizeof *o);
    hen_eyes_cfg_default(&c);
    hen_cost_default(&cost); hen_sweep_cost_default(&scost);
    c.rays = s->rays; c.tick = s->tick; c.react = (unsigned)s->react;
    place(h, s->n, 99u);
    for (i = 0; i < s->n; i++) hen_eyes_init(&e[i], &c, rnd(&seed));
    o->pen_room = o->pen_hen = -1e30f;
    while (t < s->secs && f < FRAMES_MAX) {
        const float dt = s->dt > 0.0f ? s->dt : pattern[f % 5u];
        const hen_stats_t st0 = o->st;
        const hen_sweep_stats_t sw0 = o->sw;
        hen_stats_t own, share;
        hen_sweep_stats_t swd;
        unsigned long ticks = 0;
        double fc;

        for (i = 0; i < s->n; i++) { pos[i][0] = h[i].x; pos[i][1] = h[i].z; }
        for (i = 0; i < s->n; i++) {
            const float x0 = h[i].x, z0 = h[i].z, b0 = h[i].bearing;
            float slow = 1.0f, d;

            if (s->mode == M_RAYS) slow = hen_eyes_pre(&W, &e[i], &c, dt, h[i].x, h[i].z, &h[i].bearing, RUN, (const float (*)[2])pos, s->n, i, viewer, &o->st);
            else if (s->mode == M_SWEEP) slow = hen_sweep_pre(&W, &G, s->sweep_mode, s->every_frame, s->look, &e[i], &c, dt, h[i].x, h[i].z, &h[i].bearing, RUN, (const float (*)[2])pos, s->n, i, &o->sw);
            d = sh4der_abs_(h[i].bearing - b0);
            if (d > HEN_PI) d = 2.0f * HEN_PI - d;
            if (d > o->turn_worst) o->turn_worst = d / dt;
            ticks += e[i].ticked;
            o->seen += e[i].sees;
            wander(&h[i], dt, e[i].flee);
            if (s->mode != M_NONE) {
                const float xa = x0 + (h[i].x - x0) * slow, za = z0 + (h[i].z - z0) * slow;
                hen_eyes_post(&W, &c, x0, z0, &h[i].x, &h[i].z, slow, &o->st);
                o->pushed += sqrtf((h[i].x - xa) * (h[i].x - xa) + (h[i].z - za) * (h[i].z - za));
            }
            o->walked += sqrtf((h[i].x - x0) * (h[i].x - x0) + (h[i].z - z0) * (h[i].z - z0));
            pos[i][0] = h[i].x; pos[i][1] = h[i].z;
        }
        if (s->mode != M_NONE) {
            hen_agents_separate(pos, s->n, c.radius, &o->st);
            for (i = 0; i < s->n; i++) {                       /* the separation must not put a body back into the room */
                const float x0 = pos[i][0], z0 = pos[i][1];
                h[i].x = x0; h[i].z = z0;
                hen_eyes_post(&W, &c, x0, z0, &h[i].x, &h[i].z, 1.0f, &o->st);
            }
        }
        for (i = 0; i < s->n; i++) {
            int inside = 0;
            const float p = room_pen(h[i].x, h[i].z, c.radius, &inside);

            if (p > o->pen_room) o->pen_room = p;
            o->through += (unsigned long)inside;
            for (j = 0; j < i; j++) {
                const float q = 2.0f * c.radius - sqrtf((h[i].x - h[j].x) * (h[i].x - h[j].x) + (h[i].z - h[j].z) * (h[i].z - h[j].z));
                if (q > o->pen_hen) o->pen_hen = q;
            }
            if (keep >= 0) { traj[keep][f][i][0] = h[i].x; traj[keep][f][i][1] = h[i].z; }
            { uint32_t a, b; memcpy(&a, &h[i].x, 4); memcpy(&b, &h[i].z, 4); o->hash = (o->hash ^ a) * 16777619u; o->hash = (o->hash ^ b) * 16777619u; }
        }
        /* the frame's cycles: the mode's own work, and apart from it what every mode pays (the push, the separation) */
        own = o->st; share = o->st;
        own.frames -= st0.frames; own.ticks -= st0.ticks; own.ray_fan -= st0.ray_fan; own.ray_down -= st0.ray_down; own.ray_sight -= st0.ray_sight;
        own.box -= st0.box; own.tri -= st0.tri; own.pair -= st0.pair; own.near -= st0.near; own.push = 0; own.sep = 0;
        memset(&share, 0, sizeof share); share.push = o->st.push - st0.push; share.sep = o->st.sep - st0.sep;
        swd = o->sw;
        swd.frames -= sw0.frames; swd.casts -= sw0.casts; swd.reject -= sw0.reject; swd.tri -= sw0.tri; swd.box -= sw0.box; swd.cell -= sw0.cell; swd.pair -= sw0.pair;
        fc = (double)hen_stats_cycles(&own, &cost) + (double)hen_sweep_cycles(&swd, &scost);
        o->cyc += fc; o->shared += (double)hen_stats_cycles(&share, &cost);
        if (fc > o->cyc_worst) o->cyc_worst = fc;
        if (ticks > o->worst_ticks) o->worst_ticks = ticks;
        t += dt; f++;
    }
    o->frames = f; o->secs = t;
}

/* ---- the checks -------------------------------------------------------------------------------------------------- */

static void test_kernels(void) {
    unsigned i, k, hits = 0;

    for (i = 0; i < 20000u; i++) {
        hen_tri_t t;
        float a[3], b[3], c[3], o[3], d[3], best = HEN_FAR, ref, len;
        double od[3], dd[3], want;

        for (k = 0; k < 3u; k++) { a[k] = rr(-2, 2); b[k] = a[k] + rr(-1, 1); c[k] = a[k] + rr(-1, 1); o[k] = rr(-3, 3); d[k] = (a[k] + b[k] + c[k]) / 3.0f - o[k] + rr(-0.5f, 0.5f); }
        len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
        for (k = 0; k < 3u; k++) { d[k] /= len; od[k] = o[k]; dd[k] = d[k]; }
        hen_tri_set_(&t, a, b, c);
        want = tri_double(&t, od, dd);
        ref = hen_tri_ref(&t, o, d);
        hen_tri_near(&t, o, d, &best);
        if (want < 1e29) hits++;
        if (want < 1e29 && want > 0.01 && fabs(want - (double)ref) > 2e-3 * (1.0 + want) && ref > 1e29f) continue;   /* on an edge: either answer */
        CHECK((ref > 1e29f) == (best > 1e29f), "kernel and reference differ on a hit, case %u", i);
        if (ref < 1e29f) CHECK(fabsf(ref - best) <= 1e-4f * (1.0f + ref), "kernel %.6f reference %.6f", (double)best, (double)ref);
        if (ref < 1e29f && want < 1e29) CHECK(fabs(want - (double)ref) <= 2e-3 * (1.0 + want), "reference %.6f double %.6f", (double)ref, want);
    }
    CHECK(hits > 2000u, "only %u of the kernel cases hit", hits);
}

static void test_fan_and_sight(void) {
    hen_stats_t st;
    unsigned i, differ = 0, blocked = 0;

    memset(&st, 0, sizeof st);
    for (i = 0; i < 4000u; i++) {                              /* a level ray: boxes and the eye's triangles against every triangle */
        const float x = rr(-2.9f, 2.9f), z = rr(-2.9f, 2.9f), a = rr(-HEN_PI, HEN_PI), s = sinf(a), c = cosf(a);
        const double o[3] = { x, EYE_Y, z }, d[3] = { s, 0, c };
        const double want = world_double(o, d, 3.0);
        float best = 3.0f;

        hen_ray_level(&W, x, z, s, c, &best, &st);
        if (fabs(want - (double)best) > 2e-3) differ++;
    }
    CHECK(differ <= 4u, "the fan's ray differs from every-triangle in %u of 4000 (a few on an edge are allowed)", differ);
    differ = 0;
    for (i = 0; i < 4000u; i++) {                              /* a sight line against every triangle */
        const float a[3] = { rr(-2.9f, 2.9f), EYE_Y, rr(-2.9f, 2.9f) }, b[3] = { rr(-2.9f, 2.9f), rr(0.3f, 1.8f), rr(-2.9f, 2.9f) };
        const double o[3] = { a[0], a[1], a[2] }, d[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
        const double t = world_double(o, d, 1.0);
        const int want = t < 1.0, got = hen_blocked(&W, a, b, &st);

        if (fabs(t - 1.0) < 1e-3 || t < 2e-4) continue;
        blocked += (unsigned)got;
        if (want != got) differ++;
    }
    CHECK(differ <= 2u, "sight differs from every-triangle in %u of 4000", differ);
    CHECK(blocked > 200u && blocked < 3800u, "sight blocked in %u of 4000: the test has no spread", blocked);
#ifndef HEN_TEST_ROOM
    {   /* the sofa hides a viewer; from the side it does not */
        const float hen[3] = { -1.6f, EYE_Y, -2.4f }, low[3] = { 2.6f, 0.6f, -2.4f }, high[3] = { 2.6f, 1.6f, -2.4f }, front[3] = { -1.6f, 1.5f, 1.0f };
        hen_eyes_t e;
        hen_eyes_cfg_t c;
        float pos[1][2] = { { -1.6f, -2.4f } };

        CHECK(hen_blocked(&W, hen, low, &st) == 1, "the sofa does not hide a low viewer");
        CHECK(hen_blocked(&W, hen, high, &st) == 1, "the sofa does not hide a standing viewer behind its length");
        CHECK(hen_blocked(&W, hen, front, &st) == 0, "an open line is blocked");
        hen_eyes_cfg_default(&c); c.react = HEN_REACT_FREEZE;
        hen_eyes_init(&e, &c, 0.0f);
        hen_eyes_tick(&W, &e, &c, -1.6f, -2.4f, 0.5f * HEN_PI, RUN, (const float (*)[2])pos, 1u, 0u, low, &st);     /* facing +x, at the viewer */
        CHECK(e.sight == HEN_SIGHT_BLOCKED && !e.sees, "facing a hidden viewer: sight %u sees %u", e.sight, e.sees);
        hen_eyes_tick(&W, &e, &c, -1.6f, -2.4f, 0.0f, RUN, (const float (*)[2])pos, 1u, 0u, front, &st);            /* facing +z, at the open viewer */
        CHECK(e.sight == HEN_SIGHT_CLEAR && e.sees && e.slow == 0.0f, "facing an open viewer: sight %u sees %u slow %.2f", e.sight, e.sees, (double)e.slow);
        hen_eyes_tick(&W, &e, &c, -1.6f, -2.4f, HEN_PI, RUN, (const float (*)[2])pos, 1u, 0u, front, &st);          /* its back to the viewer */
        CHECK(e.sight == HEN_SIGHT_NONE && !e.sees, "a viewer behind is seen");
        c.react = HEN_REACT_FLEE;
        hen_eyes_tick(&W, &e, &c, -1.6f, -2.4f, 0.0f, RUN, (const float (*)[2])pos, 1u, 0u, front, &st);
        CHECK(e.flee && e.sees, "flee is not asked for");
        {
            float seg[HEN_DEBUG_SEGS][2][3];
            uint32_t argb[HEN_DEBUG_SEGS];
            CHECK(hen_eyes_debug(&W, &e, &c, seg, argb) == c.rays + c.downs + 1u, "debug segments");
            CHECK(argb[c.rays + c.downs] == HEN_ARGB_SEEN, "the sight line's colour");
        }
    }
#endif
}

static void test_sweeps_agree(void) {
    hen_sweep_stats_t st[3];
    unsigned i, m, differ = 0, hits = 0;

    memset(st, 0, sizeof st);
    for (i = 0; i < 4000u; i++) {
        const float x = rr(-2.8f, 2.8f), z = rr(-2.8f, 2.8f), a = rr(-HEN_PI, HEN_PI), len = rr(0.05f, 1.2f);
        hen_sweep_hit_t h[3];

        for (m = 0; m < 3u; m++) hen_sweep_cast(&W, &G, (int)m, x, z, sinf(a), cosf(a), len, 0.2f, NULL, 0u, 0u, 0.15f, &h[m], &st[m]);
        hits += h[0].kind != HEN_MISS;
        if (h[0].kind != h[1].kind || h[0].kind != h[2].kind || fabsf(h[0].front - h[1].front) > 1e-5f || fabsf(h[0].front - h[2].front) > 1e-5f) differ++;
    }
    CHECK(differ == 0u, "the three sweeps differ in %u of 4000", differ);
    CHECK(hits > 400u, "only %u sweeps touch", hits);
    CHECK(st[1].tri == st[0].tri && st[2].tri == st[0].tri, "full tests %lu %lu %lu: a broad phase dropped one", st[0].tri, st[1].tri, st[2].tri);
    CHECK(st[1].reject < st[0].reject && st[2].reject < st[1].reject, "rejects %lu %lu %lu", st[0].reject, st[1].reject, st[2].reject);
}

static void test_runs(void) {
    static const unsigned rays[3] = { 5u, 9u, 13u };
    static const float ticks[4] = { 0.05f, 0.1f, 0.2f, 0.4f }, dts[2] = { 1.0f / 30.0f, 0.0f };
    unsigned r, k, d;
    out_t o, o2, none, push;
    sim_t s = { M_NONE, HEN_SWEEP_GRID, 0, 1, HEN_REACT_NONE, 8u, 9u, 0.1f, 1.0f / 30.0f, 60.0f };
    hen_eyes_cfg_t c;

    hen_eyes_cfg_default(&c);
    run(&s, &none, -1);
    CHECK(none.through > 0u, "with no eyes and no push the hens never cross furniture: the scene tests nothing");
    s.mode = M_PUSH; run(&s, &push, -1);
    CHECK(push.through == 0u && push.pen_room <= 1e-3f, "push alone: through %lu, into the room %.4f", push.through, (double)push.pen_room);
    for (r = 0; r < 3u; r++) for (k = 0; k < 4u; k++) for (d = 0; d < 2u; d++) {
        s.mode = M_RAYS; s.rays = rays[r]; s.tick = ticks[k]; s.dt = dts[d];
        run(&s, &o, -1); run(&s, &o2, -1);
        c.tick = ticks[k];
        CHECK(o.through == 0u && o.pen_room <= 1e-3f, "rays %u tick %.2f: through %lu, into the room %.4f", rays[r], (double)ticks[k], o.through, (double)o.pen_room);
        CHECK(o.pen_hen < 0.15f, "rays %u tick %.2f: a hen %.3f m into another", rays[r], (double)ticks[k], (double)o.pen_hen);
        CHECK(o.hash == o2.hash && o.st.tri == o2.st.tri, "rays %u tick %.2f: two runs differ", rays[r], (double)ticks[k]);
        CHECK(o.st.ray_fan == o.st.ticks * rays[r] && o.st.ray_down == o.st.ticks * 2u && o.st.ray_sight <= o.st.ticks, "ray counts");
        CHECK(o.st.frames == o.frames * s.n, "frames %lu", o.st.frames);
        CHECK(o.turn_worst <= c.turn_rate * 1.001f, "the eyes turned %.2f rad/s", (double)o.turn_worst);
        CHECK(hen_eyes_reach(&c, RUN) >= 2.0f * RUN * ticks[k], "the reach is under two ticks of travel");
        if (d == 0u && ticks[k] >= s.dt) {
            const double want = (double)s.n * (double)o.secs / (double)ticks[k];
            CHECK(fabs((double)o.st.ticks - want) <= (double)s.n + 1.0, "ticks %lu, expected %.0f", o.st.ticks, want);
        } else CHECK(o.st.ticks <= o.st.frames, "more ticks than frames");
        CHECK(o.walked > 0.25f * none.walked, "rays %u tick %.2f: the hens stand still (%.1f m against %.1f)", rays[r], (double)ticks[k], (double)o.walked, (double)none.walked);
        if (r == 1u && k == 1u && d == 0u) {
            CHECK(o.worst_ticks < s.n && o.worst_ticks >= 1u, "the worst frame cast %lu of %u hens: not spread", o.worst_ticks, s.n);
            CHECK(o.pushed < push.pushed, "with eyes the push moved %.2f m, without %.2f m: the eyes do not steer", (double)o.pushed, (double)push.pushed);
        }
    }
    s.rays = 9u; s.tick = 0.1f; s.dt = 1.0f / 30.0f; s.mode = M_SWEEP;
    for (k = 0; k < 3u; k++) for (d = 0; d < 2u; d++) {
        s.sweep_mode = (int)k; s.every_frame = 1; s.look = (int)d;
        run(&s, &o, -1);
        CHECK(o.through == 0u && o.pen_room <= 1e-3f && o.pen_hen < 0.15f, "sweep %u look %u: through %lu, room %.4f, hen %.3f", k, d, o.through, (double)o.pen_room, (double)o.pen_hen);
        CHECK(o.sw.casts == o.frames * s.n, "a sweep a hen a frame");
    }
    s.every_frame = 0; s.look = 1; s.sweep_mode = HEN_SWEEP_GRID;
    run(&s, &o, -1);
    CHECK(o.through == 0u && o.pen_room <= 1e-3f, "sweep on the tick: through %lu, room %.4f", o.through, (double)o.pen_room);
}

/* ---- the tables -------------------------------------------------------------------------------------------------- */

static const char *mode_name(const sim_t *s) {
    static char b[96];
    static const char *const bp[3] = { "every triangle", "object boxes", "grid" };
    if (s->mode == M_RAYS) snprintf(b, sizeof b, "eyes: %u rays, tick %.2f s", s->rays, (double)s->tick);
    else if (s->mode == M_SWEEP) snprintf(b, sizeof b, "sweep, %s, %s, look %s", bp[s->sweep_mode], s->every_frame ? "every frame" : "tick 0.10 s", s->look ? "= reach" : "= speed x dt");
    else snprintf(b, sizeof b, "%s", s->mode == M_PUSH ? "push only" : "nothing");
    return b;
}

static void row(const sim_t *s, double frame_ms) {
    out_t o;
    run(s, &o, -1);
    {
        const double an = (double)o.frames * s->n, us = o.cyc / (double)o.frames / HEN_CYCLES_PER_MS * 1000.0, usw = o.cyc_worst / HEN_CYCLES_PER_MS * 1000.0;
        printf("| %s | %u | %.2f | %.1f | %.1f | %.2f | %.0f | %.0f | %.0f | %.2f %% | %.2f %% | %.0f |\n", mode_name(s), s->n,
               (double)(o.st.ray_fan + o.st.ray_down + o.st.ray_sight + o.sw.casts) / an, (double)(o.st.tri + o.sw.tri) / an,
               (double)(o.st.box + o.sw.box + o.sw.cell + o.sw.reject) / an, (double)(o.st.pair + o.st.near + o.sw.pair) / an,
               o.cyc / an, us, usw, us / (frame_ms * 10.0), usw / (frame_ms * 10.0), o.shared / (double)o.frames / HEN_CYCLES_PER_MS * 1000.0);
    }
}

static void outcome(const sim_t *s) {
    out_t o;
    run(s, &o, -1);
    printf("| %s | %u | %lu | %.4f | %.3f | %.2f | %.1f |\n", mode_name(s), s->n, o.through, (double)(o.pen_room > 0 ? o.pen_room : 0), (double)(o.pen_hen > 0 ? o.pen_hen : 0), (double)o.pushed, (double)o.walked);
}

/* one walker straight at something: how early it reacts, how near it gets, how much the push had to do */
#ifndef HEN_TEST_ROOM
static void approach(const char *what, const sim_t *s, float x, float z, float bearing, float speed, float top) {
    hen_eyes_t e;
    hen_eyes_cfg_t c;
    hen_stats_t st;
    hen_sweep_stats_t sw;
    float pos[1][2], b = bearing, t = 0.0f, first = -1.0f, turned = -1.0f, gap = 1e30f, pushed = 0.0f, sb, cb;

    memset(&st, 0, sizeof st); memset(&sw, 0, sizeof sw);
    hen_eyes_cfg_default(&c); c.rays = s->rays; c.tick = s->tick;
    hen_eyes_init(&e, &c, 0.5f);
    while (t < 6.0f) {
        const float dt = 1.0f / 30.0f, x0 = x, z0 = z;
        float slow, xa, za, g;

        pos[0][0] = x; pos[0][1] = z;
        if (s->mode == M_RAYS) slow = hen_eyes_pre(&W, &e, &c, dt, x, z, &b, top, (const float (*)[2])pos, 1u, 0u, NULL, &st);
        else slow = hen_sweep_pre(&W, &G, s->sweep_mode, s->every_frame, s->look, &e, &c, dt, x, z, &b, top, (const float (*)[2])pos, 1u, 0u, &sw);
        g = -room_pen(x, z, c.radius, NULL);
        if (first < 0.0f && (sh4der_abs_(e.want) > 0.05f || slow < 0.95f)) first = g;
        sh4der_sincos_(b, &sb, &cb);
        x += sb * speed * dt; z += cb * speed * dt;
        xa = x0 + (x - x0) * slow; za = z0 + (z - z0) * slow;
        hen_eyes_post(&W, &c, x0, z0, &x, &z, slow, &st);
        pushed += sqrtf((x - xa) * (x - xa) + (z - za) * (z - za));
        g = -room_pen(x, z, c.radius, NULL);
        if (g < gap) gap = g;
        t += dt;
        { float d = sh4der_abs_(b - bearing); if (d > HEN_PI) d = 2.0f * HEN_PI - d; if (turned < 0.0f && d > 0.785f) turned = t; }
    }
    printf("| %s | %s | %.1f | ", what, mode_name(s), (double)speed);
    if (first >= 0.0f) printf("%.2f | ", (double)first); else printf("never | ");
    printf("%.3f | %.3f | ", (double)gap, (double)pushed);
    if (turned >= 0.0f) printf("%.2f |\n", (double)turned); else printf("did not turn 45 degrees |\n");
}

#endif

static void report(void) {
    static const unsigned agents[4] = { 4u, 8u, 16u, 32u };
    static const double frames_ms[3] = { 33.333, 23.0, 75.0 };
    sim_t s = { M_RAYS, HEN_SWEEP_GRID, 1, 1, HEN_REACT_NONE, 8u, 9u, 0.1f, 1.0f / 30.0f, 60.0f };
    hen_cost_t cost;
    hen_sweep_cost_t sc;
    unsigned a, k, slice = 0, band = 0;
    out_t o;

    hen_cost_default(&cost); hen_sweep_cost_default(&sc);
    for (k = 0; k < W.nobj; k++) slice += W.obj[k].sn;
    for (k = 0; k < W.ntri; k++) band += (unsigned)hen_tri_in_band_(&W, &W.tri[k]);
    printf("\n## The scene\n\n");
    printf("- %u objects, %u triangles (%u bytes at 48 a triangle).\n", W.nobj, W.ntri, W.ntri * 48u);
    printf("- %u triangles span the eye's height (%.2f m): the only ones a level ray tests, %u bytes.\n", slice, (double)EYE_Y, slice * 48u);
    printf("- %u triangles stand in the body's height (0 to %.2f m): the ones a sweep tests. The grid: %u x %u cells, %u entries.\n", band, (double)TOP_Y, G.n, G.n, G.items);
    printf("\n## The cost model (cycles; ESTIMATE unless said)\n\n");
    printf("| Thing counted | Cycles | Status |\n|---|---|---|\n");
    printf("| eyes: a ray against a triangle | %.2f | MEASURED for any-hit in the cache (SH4-066); the nearest-hit form here is not timed |\n", (double)cost.tri);
    printf("| eyes: a ray against an object's box | %.0f | ESTIMATE |\n| eyes: a ray's set-up | %.0f | ESTIMATE (the grid ray's measured set-up, SH4-052, borrowed) |\n", (double)cost.box, (double)cost.ray);
    printf("| eyes: a ray against another agent | %.2f | ESTIMATE (SH4-055's point-against-capsule pair, borrowed; a circle is less work) |\n", (double)cost.pair);
    printf("| eyes: another agent's distance, once a tick | %.0f | ESTIMATE |\n", (double)cost.near);
    printf("| eyes: the steering, a tick | %.0f | ESTIMATE |\n| sweep: a capsule against a triangle | %.0f | ESTIMATE |\n", (double)cost.steer, (double)sc.tri);
    printf("| sweep: a triangle turned away by height or box | %.0f | ESTIMATE |\n| sweep: an object's footprint / a grid cell | %.0f / %.0f | ESTIMATE |\n", (double)sc.reject, (double)sc.box, (double)sc.cell);
    printf("| sweep: the capsule against another agent | %.2f | ESTIMATE (SH4-055 borrowed) |\n| sweep: the steering, a cast | %.0f | ESTIMATE |\n", (double)sc.pair, (double)sc.steer);
    printf("| both: an agent's own work a frame | %.0f | ESTIMATE |\n| shared: the push (circle against a footprint) / the separation (a pair) | %.0f / %.0f | ESTIMATE |\n", (double)cost.frame, (double)cost.push, (double)cost.sep);
    printf("| 1 ms | %.0f | MEASURED (200.006 MHz) |\n", (double)HEN_CYCLES_PER_MS);

    for (k = 0; k < 3u; k++) {
        printf("\n## Delta against a collision sweep: frames of %.1f ms, 60 s scripted run\n\n", frames_ms[k]);
        printf("Per agent per frame (mean): casts, triangle tests, broad-phase tests, agent-pair tests, cycles. Then all agents: us a frame (mean, worst frame), the share of the frame, and the shared push + separation.\n\n");
        printf("| Mode | Agents | Casts | Triangle tests | Broad-phase tests | Pair tests | Cycles an agent a frame | us a frame | us worst frame | Share, mean | Share, worst | Shared us a frame |\n|---|---|---|---|---|---|---|---|---|---|---|---|\n");
        for (a = 0; a < 4u; a++) {
            s.n = agents[a]; s.dt = (float)(frames_ms[k] / 1000.0);
            s.mode = M_RAYS; s.tick = 0.1f;
            s.rays = 5u; row(&s, frames_ms[k]); s.rays = 9u; row(&s, frames_ms[k]); s.rays = 13u; row(&s, frames_ms[k]);
            s.rays = 9u; s.mode = M_SWEEP;
            s.every_frame = 1; s.look = 1; s.sweep_mode = HEN_SWEEP_NAIVE; row(&s, frames_ms[k]);
            s.sweep_mode = HEN_SWEEP_BOXES; row(&s, frames_ms[k]);
            s.sweep_mode = HEN_SWEEP_GRID; row(&s, frames_ms[k]);
            s.look = 0; row(&s, frames_ms[k]);
            s.every_frame = 0; s.look = 1; row(&s, frames_ms[k]);
        }
    }

    printf("\n## The eyes alone: rays and tick (8 agents, frames of 33.3 ms)\n\n");
    printf("| Mode | Agents | Casts | Triangle tests | Broad-phase tests | Pair tests | Cycles an agent a frame | us a frame | us worst frame | Share, mean | Share, worst | Shared us a frame |\n|---|---|---|---|---|---|---|---|---|---|---|---|\n");
    s.mode = M_RAYS; s.n = 8u; s.dt = 1.0f / 30.0f;
    { static const float tk[4] = { 0.05f, 0.1f, 0.2f, 0.4f }; static const unsigned ry[3] = { 5u, 9u, 13u };
      for (a = 0; a < 3u; a++) for (k = 0; k < 4u; k++) { s.rays = ry[a]; s.tick = tk[k]; row(&s, 33.333); } }

    printf("\n## One tick of one agent (9 rays, 0.1 s; means over the 8-agent run)\n\n");
    s.rays = 9u; s.tick = 0.1f; run(&s, &o, -1);
    {
        const double tk = (double)o.st.ticks, rays = (double)(o.st.ray_fan + o.st.ray_down + o.st.ray_sight) / tk;
        hen_stats_t one = o.st;
        double cyc;
        one.push = 0; one.sep = 0; one.frames = 0;
        cyc = (double)hen_stats_cycles(&one, &cost) / tk;
        printf("- Rays a tick: %.2f (9 in the fan, 2 down, %.2f to the viewer).\n- Box tests: %.1f. Triangle tests: %.1f. Agent distance looks: %.1f; ray-against-agent tests: %.1f.\n", rays, (double)o.st.ray_sight / tk, (double)o.st.box / tk, (double)o.st.tri / tk, (double)o.st.near / tk, (double)o.st.pair / tk);
        printf("- Cycles a tick: %.0f = %.1f us (ESTIMATE). With the rays cast between other work, add up to 164 cycles a ray (SH4-062): %.0f cycles, %.1f us.\n", cyc, cyc / 200.006, cyc + rays * 164.0, (cyc + rays * 164.0) / 200.006);
        printf("- Ticks that fit in 1 ms: %.0f. Agents that fit in 1 ms a frame at 30 fps, mean (0.3 ticks an agent a frame): %.0f; if every agent ticked on one frame: %.0f.\n", 200006.0 / cyc, 200006.0 / (cyc * 0.3333 + cost.frame), 200006.0 / (cyc + cost.frame));
        printf("- Worst frame seen with 8 agents: %lu of them ticked together.\n", o.worst_ticks);
    }

    printf("\n## Outcome on the same scripted run (60 s, frames of 33.3 ms)\n\n");
    printf("Through: frames in which a body's centre was inside a footprint. Into the room / a hen: the deepest overlap left after the push, m. Pushed: how far the push moved bodies in all, m (the less, the better the steering). Walked: the distance covered in all, m.\n\n");
    printf("| Mode | Agents | Through | Into the room | Into a hen | Pushed | Walked |\n|---|---|---|---|---|---|---|\n");
    for (a = 0; a < 4u; a++) {
        s.n = agents[a]; s.dt = 1.0f / 30.0f; s.tick = 0.1f; s.rays = 9u;
        s.mode = M_NONE; outcome(&s); s.mode = M_PUSH; outcome(&s);
        s.mode = M_RAYS; s.rays = 5u; outcome(&s); s.rays = 9u; outcome(&s); s.rays = 13u; outcome(&s);
        s.mode = M_SWEEP; s.sweep_mode = HEN_SWEEP_GRID; s.every_frame = 1; s.look = 1; outcome(&s);
        s.look = 0; outcome(&s); s.every_frame = 0; s.look = 1; outcome(&s);
    }

    printf("\n## Paths: eyes against the sweep, the same hens and seed (8 agents)\n\n");
    {
        out_t p, q;
        double sum = 0.0, worst = 0.0, at5 = 0.0;
        unsigned f, i;

        s.n = 8u; s.mode = M_RAYS; s.rays = 9u; s.tick = 0.1f; run(&s, &p, 0);
        s.mode = M_SWEEP; s.every_frame = 1; s.look = 1; s.sweep_mode = HEN_SWEEP_GRID; run(&s, &q, 1);
        for (f = 0; f < p.frames; f++) for (i = 0; i < 8u; i++) {
            const double d = sqrt(pow(traj[0][f][i][0] - traj[1][f][i][0], 2) + pow(traj[0][f][i][1] - traj[1][f][i][1], 2));
            sum += d; if (d > worst) worst = d; if (f == 150u) at5 += d / 8.0;
        }
        printf("- Mean distance between a hen's place under the eyes and under the sweep: %.2f m after 5 s; %.2f m over the run; %.2f m at the most.\n", at5, sum / ((double)p.frames * 8.0), worst);
        printf("- The walk has random turns, so two steering rules part for good once one of them turns a hen: the paths are not the same paths. What is the same is the outcome table above.\n");
        printf("- Distance walked: eyes %.1f m, sweep %.1f m.\n", (double)p.walked, (double)q.walked);
    }

#ifndef HEN_TEST_ROOM
    printf("\n## One walker straight at something (frames of 33.3 ms)\n\n");
    printf("First reaction: the gap to the nearest footprint when it first turns or slows, m. Least gap: the nearest it came (0: touching). Pushed: what the push had to move it, m. Time to turn 45 degrees, s.\n\n");
    printf("| Case | Mode | Speed m/s | First reaction at | Least gap | Pushed | Turned 45 degrees after |\n|---|---|---|---|---|---|---|\n");
    {
        static const struct { const char *what; float x, z, b, v; } cs[4] = {
            { "a wall, walking", -1.4f, -0.3f, -0.5f * HEN_PI, 0.5f }, { "a wall, running", -0.8f, -0.3f, -0.5f * HEN_PI, 2.0f },
            { "a 4 cm post, 10 cm off the path", 2.0f, 0.2f, 0.0f, 0.5f }, { "a 4 cm post, 10 cm off the path, running", 2.0f, -0.6f, 0.0f, 2.0f } };
        for (k = 0; k < 4u; k++) {
            s.mode = M_RAYS; s.tick = 0.1f;
            s.rays = 5u; approach(cs[k].what, &s, cs[k].x, cs[k].z, cs[k].b, cs[k].v, RUN);
            s.rays = 9u; approach(cs[k].what, &s, cs[k].x, cs[k].z, cs[k].b, cs[k].v, RUN);
            s.rays = 13u; approach(cs[k].what, &s, cs[k].x, cs[k].z, cs[k].b, cs[k].v, RUN);
            s.rays = 9u; s.tick = 0.4f; approach(cs[k].what, &s, cs[k].x, cs[k].z, cs[k].b, cs[k].v, RUN);
            s.tick = 0.1f; s.mode = M_SWEEP; s.sweep_mode = HEN_SWEEP_GRID;
            s.every_frame = 1; s.look = 1; approach(cs[k].what, &s, cs[k].x, cs[k].z, cs[k].b, cs[k].v, RUN);
            s.look = 0; approach(cs[k].what, &s, cs[k].x, cs[k].z, cs[k].b, cs[k].v, RUN);
            s.every_frame = 0; s.look = 1; approach(cs[k].what, &s, cs[k].x, cs[k].z, cs[k].b, cs[k].v, RUN);
        }
    }
#endif
}

int main(int argc, char **argv) {
    scene();
    CHECK(hen_grid_build(&G, &W, 16u, grid_start, grid_item, 16384u, grid_stamp) > 0u, "the grid is empty");
    test_kernels();
    test_fan_and_sight();
    test_sweeps_agree();
    test_runs();
    printf("hen_eyes_test: %u checks, %u failed\n", checks, failed);
    if (argc > 1 && !strcmp(argv[1], "--report")) report();
    return failed ? 1 : 0;
}
