/* SH4DER-PVR: the baseline hen_eyes.h is measured against: a conventional swept-body collision test

   demo/psm/ao/hen_sweep.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The same job as hen_eyes.h, done the usual way, so the two can be set side by side on one scene: every frame (or on
   the same tick) each agent sweeps its body, a circle of radius + margin, along its heading for a look-ahead distance
   and tests that capsule against
     (a) the room: every triangle that stands in the agent's height, three ways:
           HEN_SWEEP_NAIVE  every triangle of every object;
           HEN_SWEEP_BOXES  only the objects whose footprint the capsule's box overlaps;
           HEN_SWEEP_GRID   only the triangles listed in the grid cells the capsule's box overlaps (a 2D grid on the
                            floor, built once);
     (b) every other agent's circle: the n x n pair test.
   It gives the same decision in the same fields of hen_eyes_t (want, slow), so hen_eyes_post, the push and the debug
   toggles are shared. The look-ahead is either the eyes' reach (look 1: the same warning distance) or speed x dt
   (look 0: the plain collision sweep, which only knows about what it is about to touch).

   A capsule against a triangle here is on the floor's plane (the triangle's shadow on the floor): the nearest
   approach of the path to each of its three edges, and the distance along the path where the body first touches.
   That is less work than a 3D swept sphere, so the baseline is not made dearer than it need be.

   COST. hen_sweep_stats_t counts the work: exact. hen_sweep_cost_t is ALL ESTIMATE: no capsule-triangle test has been
   timed on the console (the nearest measured row is a point against a character's capsule, 137.87 cycles: SH4-055).
   A console run of this code counts its own cycles.

   Host tested with hen_eyes.h: test/hen_eyes_test.c. Nothing here has run on a Dreamcast. */

#ifndef HEN_SWEEP_H
#define HEN_SWEEP_H

#include "hen_eyes.h"

enum { HEN_SWEEP_NAIVE = 0, HEN_SWEEP_BOXES = 1, HEN_SWEEP_GRID = 2 };

typedef struct hen_grid {
    float x0, z0, inv_cell;
    unsigned n;                     /* n x n cells */
    uint16_t *start;                /* n * n + 1: a cell's triangles are item[start[c]] .. item[start[c + 1] - 1] */
    uint16_t *item;
    uint16_t *stamp;                /* a mark a triangle, so one in several cells is tested once a cast */
    unsigned items;
    uint16_t mark;
} hen_grid_t;

typedef struct hen_sweep_stats {
    unsigned long frames, casts;
    unsigned long reject, tri;      /* a triangle turned away by its height or box; a full capsule-triangle test */
    unsigned long box, cell;        /* object footprints tested; grid cells read */
    unsigned long pair;             /* the capsule against another agent's circle */
} hen_sweep_stats_t;

typedef struct hen_sweep_cost {
    float tri, reject, box, cell, pair, steer, frame;
} hen_sweep_cost_t;

static inline void hen_sweep_cost_default(hen_sweep_cost_t *c) {       /* every figure an ESTIMATE; pair borrows SH4-055 */
    c->tri = 220.0f; c->reject = 20.0f; c->box = 25.0f; c->cell = 15.0f; c->pair = 137.87f; c->steer = 60.0f; c->frame = 40.0f;
}

static inline float hen_sweep_cycles(const hen_sweep_stats_t *s, const hen_sweep_cost_t *c) {
    return (float)s->tri * c->tri + (float)s->reject * c->reject + (float)s->box * c->box + (float)s->cell * c->cell +
           (float)s->pair * c->pair + (float)s->casts * c->steer + (float)s->frames * c->frame;
}

static inline int hen_tri_in_band_(const hen_world_t *w, const hen_tri_t *t) {
    const float y0 = t->a[1], y1 = y0 + t->e1[1], y2 = y0 + t->e2[1];
    const float lo = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2), hi = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
    return lo < w->top_y && hi > 0.0f;
}

static inline void hen_tri_rect_(const hen_tri_t *t, float r[4]) {
    const float x0 = t->a[0], x1 = x0 + t->e1[0], x2 = x0 + t->e2[0], z0 = t->a[2], z1 = z0 + t->e1[2], z2 = z0 + t->e2[2];
    r[0] = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2); r[1] = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
    r[2] = z0 < z1 ? (z0 < z2 ? z0 : z2) : (z1 < z2 ? z1 : z2); r[3] = z0 > z1 ? (z0 > z2 ? z0 : z2) : (z1 > z2 ? z1 : z2);
}

static inline unsigned hen_grid_cell_(const hen_grid_t *g, float v, float v0) {
    const float c = (v - v0) * g->inv_cell;
    return c <= 0.0f ? 0u : (c >= (float)g->n ? g->n - 1u : (unsigned)c);
}

/* The grid over the floor (the floor must be set): n x n cells, each with the triangles in the agent's height whose
   box touches it. start needs n * n + 1, stamp w->ntri, item `cap` entries. Returns the entries used, 0 when item is
   too small. */
static inline unsigned hen_grid_build(hen_grid_t *g, const hen_world_t *w, unsigned n, uint16_t *start, uint16_t *item, unsigned cap, uint16_t *stamp) {
    const float pad = 0.5f, x0 = w->floor_rect[0] - pad, z0 = w->floor_rect[2] - pad;
    const float sx = w->floor_rect[1] - w->floor_rect[0] + 2.0f * pad, sz = w->floor_rect[3] - w->floor_rect[2] + 2.0f * pad;
    unsigned pass, t, c, i, j;

    memset(g, 0, sizeof *g);
    g->x0 = x0; g->z0 = z0; g->n = n; g->start = start; g->item = item; g->stamp = stamp;
    g->inv_cell = (float)n * sh4der_recip_pos_(sx > sz ? sx : sz);
    memset(stamp, 0, w->ntri * sizeof *stamp);
    for (pass = 0; pass < 2u; pass++) {
        if (pass == 0u) memset(start, 0, (n * n + 1u) * sizeof *start);
        for (t = 0; t < w->ntri; t++) {
            float r[4];
            unsigned i0, i1, j0, j1;

            if (!hen_tri_in_band_(w, &w->tri[t])) continue;
            hen_tri_rect_(&w->tri[t], r);
            i0 = hen_grid_cell_(g, r[0], x0); i1 = hen_grid_cell_(g, r[1], x0); j0 = hen_grid_cell_(g, r[2], z0); j1 = hen_grid_cell_(g, r[3], z0);
            for (j = j0; j <= j1; j++)
                for (i = i0; i <= i1; i++) {
                    c = j * n + i;
                    if (pass == 0u) start[c + 1u]++;
                    else item[start[c]++] = (uint16_t)t;
                }
        }
        if (pass == 0u) {
            for (c = 0; c < n * n; c++) start[c + 1u] = (uint16_t)(start[c + 1u] + start[c]);
            g->items = start[n * n];
            if (g->items > cap) { g->items = 0; return 0; }
        } else {
            for (c = n * n; c > 0u; c--) start[c] = start[c - 1u];           /* the fill moved each start to its end */
            start[0] = 0;
        }
    }
    return g->items;
}

/* The path o + s d (s in 0 .. len, d unit) against the segment p .. q: when they pass within rad, the distance along
   the path where a body of that radius first touches (0 or more), nearer than *best. *px, *pz: the point touched. */
static inline int hen_sweep_edge_(float ox, float oz, float dx, float dz, float len, float rad, const float p[2], const float q[2],
                                  float *best, float *px, float *pz) {
    const float d1x = dx * len, d1z = dz * len, d2x = q[0] - p[0], d2z = q[1] - p[1], rx = ox - p[0], rz = oz - p[1];
    const float a = len * len, e = d2x * d2x + d2z * d2z, f = d2x * rx + d2z * rz, c = d1x * rx + d1z * rz;
    const float inv_a = sh4der_recip_pos_(a);
    float s, u, cx, cz, ex, ez, d2, t;

    if (e <= 1e-12f) { u = 0.0f; s = sh4der_sat_(-c * inv_a); }
    else {
        const float b = d1x * d2x + d1z * d2z, den = a * e - b * b, inv_e = sh4der_recip_pos_(e);

        s = den > 1e-12f ? sh4der_sat_((b * f - c * e) * sh4der_recip_pos_(den)) : 0.0f;
        u = (b * s + f) * inv_e;
        if (u < 0.0f) { u = 0.0f; s = sh4der_sat_(-c * inv_a); }
        else if (u > 1.0f) { u = 1.0f; s = sh4der_sat_((b - c) * inv_a); }
    }
    cx = p[0] + d2x * u; cz = p[1] + d2z * u;
    ex = ox + d1x * s - cx; ez = oz + d1z * s - cz;
    d2 = ex * ex + ez * ez;
    if (d2 >= rad * rad) return 0;
    t = s * len - sh4der_sqrt_(rad * rad - d2);
    if (t < 0.0f) t = 0.0f;
    if (!(t < *best)) return 0;
    *best = t; *px = cx; *pz = cz;
    return 1;
}

typedef struct hen_sweep_hit {
    float front;                    /* the distance along the path to the first touch, or the look-ahead */
    float px, pz;                   /* the point touched */
    int kind;                       /* HEN_MISS, HEN_HIT_ROOM, HEN_HIT_AGENT */
    int edge;                       /* 1: the path's end is off the floor */
} hen_sweep_hit_t;

static inline int hen_sweep_tri_(const hen_world_t *w, const hen_tri_t *t, const float cap[4], float ox, float oz, float dx, float dz,
                                 float len, float rad, hen_sweep_hit_t *h, hen_sweep_stats_t *st) {
    float r[4], p[3][2];
    int hit = 0;
    unsigned k;

    if (!hen_tri_in_band_(w, t)) { st->reject++; return 0; }
    hen_tri_rect_(t, r);
    if (r[0] > cap[1] || r[1] < cap[0] || r[2] > cap[3] || r[3] < cap[2]) { st->reject++; return 0; }
    st->tri++;
    p[0][0] = t->a[0]; p[0][1] = t->a[2];
    p[1][0] = t->a[0] + t->e1[0]; p[1][1] = t->a[2] + t->e1[2];
    p[2][0] = t->a[0] + t->e2[0]; p[2][1] = t->a[2] + t->e2[2];
    for (k = 0; k < 3u; k++) hit |= hen_sweep_edge_(ox, oz, dx, dz, len, rad, p[k], p[(k + 1u) % 3u], &h->front, &h->px, &h->pz);
    return hit;
}

/* One sweep from (x, z) along the unit (dx, dz) for len metres with a body of radius rad. */
static inline void hen_sweep_cast(const hen_world_t *w, hen_grid_t *g, int mode, float x, float z, float dx, float dz, float len, float rad,
                                  const float (*others)[2], unsigned n_others, unsigned self, float other_radius, hen_sweep_hit_t *h,
                                  hen_sweep_stats_t *st) {
    const float ex = x + dx * len, ez = z + dz * len;
    const float cap[4] = { (x < ex ? x : ex) - rad, (x < ex ? ex : x) + rad, (z < ez ? z : ez) - rad, (z < ez ? ez : z) + rad };
    unsigned k, i, j;

    st->casts++;
    h->front = len; h->px = ex; h->pz = ez; h->kind = HEN_MISS;
    h->edge = w->has_floor && (ex < w->floor_rect[0] || ex > w->floor_rect[1] || ez < w->floor_rect[2] || ez > w->floor_rect[3]);
    if (mode == HEN_SWEEP_GRID && g && g->items) {
        const unsigned i0 = hen_grid_cell_(g, cap[0], g->x0), i1 = hen_grid_cell_(g, cap[1], g->x0);
        const unsigned j0 = hen_grid_cell_(g, cap[2], g->z0), j1 = hen_grid_cell_(g, cap[3], g->z0);

        if (++g->mark == 0u) { memset(g->stamp, 0, w->ntri * sizeof *g->stamp); g->mark = 1u; }
        for (j = j0; j <= j1; j++)
            for (i = i0; i <= i1; i++) {
                const unsigned c = j * g->n + i;

                st->cell++;
                for (k = g->start[c]; k < g->start[c + 1u]; k++) {
                    const unsigned t = g->item[k];

                    if (g->stamp[t] == g->mark) continue;
                    g->stamp[t] = g->mark;
                    if (hen_sweep_tri_(w, &w->tri[t], cap, x, z, dx, dz, len, rad, h, st)) h->kind = HEN_HIT_ROOM;
                }
            }
    } else {
        for (k = 0; k < w->nobj; k++) {
            const hen_obj_t *o = &w->obj[k];

            if (mode != HEN_SWEEP_NAIVE) {
                st->box++;
                if (!o->has_foot || o->foot[0] > cap[1] || o->foot[1] < cap[0] || o->foot[2] > cap[3] || o->foot[3] < cap[2]) continue;
            }
            for (i = 0; i < o->tn; i++)
                if (hen_sweep_tri_(w, &w->tri[o->t0 + i], cap, x, z, dx, dz, len, rad, h, st)) h->kind = HEN_HIT_ROOM;
        }
    }
    for (j = 0; j < n_others; j++) {
        const float p[2] = { others[j][0], others[j][1] };

        if (j == self) continue;
        st->pair++;
        if (hen_sweep_edge_(x, z, dx, dz, len, rad + other_radius, p, p, &h->front, &h->px, &h->pz)) h->kind = HEN_HIT_AGENT;
    }
}

/* The sweep in place of hen_eyes_pre: the same clock (cfg.tick; `every_frame` casts each frame whatever the tick),
   the same fields of e written, the same turn. look: 1 the eyes' reach, 0 speed x dt. */
static inline float hen_sweep_pre(const hen_world_t *w, hen_grid_t *g, int mode, int every_frame, int look, hen_eyes_t *e, const hen_eyes_cfg_t *c,
                                  float dt, float x, float z, float *bearing, float top_speed, const float (*others)[2], unsigned n_others,
                                  unsigned self, hen_sweep_stats_t *st) {
    const float most = c->turn_rate * dt;
    float turn;

    st->frames++;
    e->ticked = 0u;
    e->clock -= dt;
    if (every_frame || e->clock <= 0.0f) {
        const float rad = c->radius + c->margin, reach = hen_eyes_reach(c, top_speed);
        const float span = look ? reach - rad : top_speed * dt, len = span > 1e-3f ? span : 1e-3f;
        hen_sweep_hit_t h;
        float sb, cb, close, away, tcx, tcz, want;

        if (e->clock <= 0.0f) { e->clock += c->tick; if (e->clock <= 0.0f) e->clock = c->tick; }
        sh4der_sincos_(*bearing, &sb, &cb);
        hen_sweep_cast(w, g, mode, x, z, sb, cb, len, rad, others, n_others, self, c->radius, &h, st);
        e->ticked = 1u; e->reach = len;
        e->cast_at[0] = x; e->cast_at[1] = z; e->cast_at[2] = *bearing; e->cast_at[3] = len;
        e->dist[0] = h.front; e->hit[0] = (uint8_t)h.kind;
        close = h.kind != HEN_MISS ? 1.0f - h.front * sh4der_recip_pos_(len) : 0.0f;
        away = ((h.px - x) * cb - (h.pz - z) * sb) > 0.0f ? -1.0f : 1.0f;            /* the touch is on the right: turn left */
        want = c->gain * 1.5f * away * close;
        e->slow = h.kind != HEN_MISS ? sh4der_sat_(h.front * sh4der_recip_pos_(len)) : 1.0f;
        if (h.kind != HEN_MISS && h.front <= 1e-3f) { e->slow = 0.0f; if (sh4der_abs_(want) < 0.8f) want = away * 0.8f; }
        if (h.edge) {                                                               /* turn toward the floor's middle */
            tcx = 0.5f * (w->floor_rect[0] + w->floor_rect[1]) - x; tcz = 0.5f * (w->floor_rect[2] + w->floor_rect[3]) - z;
            e->slow = 0.0f; want = (tcx * cb - tcz * sb) > 0.0f ? 2.4f : -2.4f;
        }
        e->floor_ok[0] = e->floor_ok[1] = (uint8_t)!h.edge;
        e->want = sh4der_clamp_(want, -2.6f, 2.6f);
    }
    turn = sh4der_clamp_(e->want, -most, most);
    *bearing += turn; e->want -= turn;
    if (*bearing > HEN_PI) *bearing -= 2.0f * HEN_PI;
    if (*bearing < -HEN_PI) *bearing += 2.0f * HEN_PI;
    return e->slow;
}

#endif /* HEN_SWEEP_H */
