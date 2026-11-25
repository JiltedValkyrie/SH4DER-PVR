/* SH4DER-PVR: eyes for small walkers: a fan of rays against the room's triangles, steering from what they hit

   demo/psm/ao/hen_eyes.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A hen (or any agent that walks on a floor) looks with rays in place of a collision sweep every frame:
     a FAN    of 5, 9 or 13 rays, level, at eye height, spread over the field of view (120 degrees); each gives the
              distance to the nearest thing: a triangle of the room, or another agent (a circle);
     DOWN     one or two rays straight down in front of it: no floor there is an edge;
     SIGHT    one ray to a viewer (the camera, a hero), when the viewer is inside the fan and in range: furniture in
              the way hides the viewer.
   The rays are cast on a TICK IN SECONDS (0.05, 0.1, 0.2 or 0.4), each agent with its own phase, so the agents are
   spread over the frames. Between ticks the agent keeps the last answer and turns toward it at a fixed rate; no ray is
   cast. Every frame a cheap PUSH (a circle against each object's footprint rectangle, and the floor's edge) keeps it
   out of the furniture whatever the rays missed. The fan reaches at least speed x tick x 2, plus the body.

   The agent's own walk stays the caller's (a hen's peck, step and run; its bounds). The eyes change three things only:
   the bearing (a turn), how much of the frame's step is kept (0 .. 1), and the place after the step (the push).

   THE WORLD (hen_world_t) is built once from triangles, an object at a time. Each object keeps its box, its triangles
   with those the level fan can meet FIRST (a triangle that does not span the eye's height cannot be hit by a level
   ray: for furniture that is most of them), and its footprint: the rectangle on the floor of what stands in the
   agent's height. A ray tests an object's box (two slabs), then only the triangles of a box it enters before its
   nearest hit so far. With room_data.h included first, hen_world_from_room() fills it from the room's casters.

   A FRAME, for agent i:
       slow = hen_eyes_pre(&world, &eyes[i], &cfg, dt, x, z, &bearing, top_speed, others, n, i, viewer, &stats);
       x0 = x; z0 = z;  ... the caller's own walk moves (x, z), and may change the bearing ...
       hen_eyes_post(&world, &cfg, x0, z0, &x, &z, slow, &stats);
   eyes[i].sees is 1 while the viewer is seen (cfg.react says what the eyes do about it; eyes[i].flee asks the
   caller's walk to run). hen_eyes_debug() gives the rays as line segments with a colour each, for a debug layer.

   Bearings are radians from +z, clockwise seen from above: forward is (sin b, 0, cos b). Metres, y up.

   COST. hen_stats_t counts what was done (rays, box tests, triangle tests, circle tests, pushes): exact. hen_cost_t
   turns the counts into cycles: the triangle test at the console's measured 62.49 cycles (any direction, early outs,
   triangles in the cache: SH4-066), the rest ESTIMATES until this module is timed on a console. The kernel here keeps
   the nearest hit, where the measured one answered any hit: one more compare a hit, and one reciprocal when a hit is
   nearer than the last. No divide in a loop. Maths through sh4der_core.h: SH4ZAM on the Dreamcast, plain C with
   SH4DER_CORE_NO_SH4ZAM.

   Host tested: test/hen_eyes_test.c. Nothing here has run on a Dreamcast. */

#ifndef HEN_EYES_H
#define HEN_EYES_H

#include <stdint.h>
#include <string.h>

#include "sh4der_core.h"

#define HEN_RAYS_MAX   13u
#define HEN_OBJS_MAX   32u
#define HEN_NEAR_MAX   16u          /* the agents near enough for a ray to reach, kept a tick; n_others up to 255 */
#define HEN_TRI_EPS    1e-4f        /* a hit nearer than this is the ray's own start, m (the bench's RAY_TRI_EPS) */
#define HEN_FAR        1e30f
#define HEN_PI         3.14159265f

enum { HEN_MISS = 0, HEN_HIT_ROOM = 1, HEN_HIT_AGENT = 2 };
enum { HEN_REACT_NONE = 0, HEN_REACT_FREEZE = 1, HEN_REACT_LOOK = 2, HEN_REACT_FLEE = 3 };
enum { HEN_SIGHT_NONE = 0, HEN_SIGHT_CLEAR = 1, HEN_SIGHT_BLOCKED = 2 };

/* A triangle as a corner and two edges, 48 bytes: the bench's ray_tri_t. */
typedef struct hen_tri {
    float a[3], e1[3], e2[3], pad[3];
} hen_tri_t;

typedef struct hen_obj {
    float lo[3], hi[3];             /* its box */
    float foot[4];                  /* its footprint: x0, x1, z0, z1 of what stands between the floor and the agent's top */
    uint16_t t0, tn, sn, has_foot;  /* triangles t0 .. t0 + tn - 1; the first sn span the eye's height */
} hen_obj_t;

typedef struct hen_world {
    hen_tri_t *tri;                 /* the caller's storage */
    unsigned cap, ntri, nobj;
    float eye_y, top_y;             /* the fan's height; the agent's top (footprints) */
    hen_obj_t obj[HEN_OBJS_MAX];
    hen_tri_t floor[2];             /* the floor, for the rays down */
    float floor_rect[4];            /* x0, x1, z0, z1 */
    int has_floor;
} hen_world_t;

typedef struct hen_eyes_cfg {
    unsigned rays;                  /* 5, 9 or 13 (any odd count from 3 to HEN_RAYS_MAX) */
    unsigned downs;                 /* 1 or 2 rays down */
    unsigned react;                 /* HEN_REACT_* when the viewer is seen */
    float fov;                      /* the fan's whole angle, radians */
    float range;                    /* the fan's least reach, m; the reach is never under speed x tick x 2 + the body */
    float radius, margin;           /* the body's circle; the gap it keeps */
    float tick;                     /* seconds between casts */
    float turn_rate;                /* radians a second toward the wanted bearing */
    float gain;                     /* how hard a near thing turns it */
    float edge_ahead;               /* the least distance ahead of the rays down, m */
    float see_range;                /* the viewer is not looked for further than this, m */
} hen_eyes_cfg_t;

typedef struct hen_eyes {
    float clock;                    /* seconds to the next tick */
    float want;                     /* the turn still wanted, radians (+ clockwise) */
    float slow;                     /* the part of a frame's step kept, 0 .. 1 */
    float reach;                    /* the fan's reach at the last tick, m */
    float dist[HEN_RAYS_MAX];       /* the nearest hit along each ray, or reach */
    float cast_at[4];               /* where the last tick was cast from: x, z, bearing, the rays down's distance ahead */
    float view[3];                  /* the viewer at the last tick */
    uint8_t hit[HEN_RAYS_MAX];      /* HEN_MISS, HEN_HIT_ROOM, HEN_HIT_AGENT */
    uint8_t floor_ok[2];            /* each ray down: 1 floor, 0 none */
    uint8_t sight;                  /* HEN_SIGHT_* at the last tick */
    uint8_t sees, flee, ticked;     /* the viewer is seen; the walk should run; this frame cast */
} hen_eyes_t;

typedef struct hen_stats {
    unsigned long frames, ticks;
    unsigned long ray_fan, ray_down, ray_sight;
    unsigned long box, tri, pair, push;     /* box tests, triangle tests, ray-circle tests, circle-rectangle tests */
    unsigned long sep;                      /* agent against agent, a pair (hen_agents_separate) */
    unsigned long near;                     /* another agent's distance looked at, once a tick, to leave the far ones out */
} hen_stats_t;

/* Cycles for each counted thing. tri is MEASURED (SH4-066: any direction, early outs, in the cache); every other
   figure is an ESTIMATE, to be replaced by this module's own console row. pair borrows SH4-055 (a receiver against
   a character proxy, 137.87 cycles): a ray against a circle is less work. mix is what a ray cost more when cast
   between other work in place of in one batch (SH4-053, SH4-062: 89 to 164 cycles a ray); 0 for a batch. */
typedef struct hen_cost {
    float tri, box, ray, pair, near, push, sep, steer, frame, mix;
} hen_cost_t;

#define HEN_CYCLES_PER_MS 200006.0f          /* 200.006 MHz, MEASURED (bench s2) */

static inline void hen_cost_default(hen_cost_t *c) {
    c->tri = 62.49f; c->box = 30.0f; c->ray = 74.0f; c->pair = 137.87f; c->near = 20.0f; c->push = 25.0f; c->sep = 20.0f; c->steer = 150.0f; c->frame = 40.0f; c->mix = 0.0f;
}

/* The cycles the counts stand for: ESTIMATE. frame is an agent's own work a frame without a tick (the turn, the step). */
static inline float hen_stats_cycles(const hen_stats_t *s, const hen_cost_t *c) {
    const float rays = (float)(s->ray_fan + s->ray_down + s->ray_sight);
    return (float)s->tri * c->tri + (float)s->box * c->box + rays * (c->ray + c->mix) + (float)s->pair * c->pair + (float)s->near * c->near +
           (float)s->push * c->push + (float)s->sep * c->sep + (float)s->ticks * c->steer + (float)s->frames * c->frame;
}

static inline void hen_eyes_cfg_default(hen_eyes_cfg_t *c) {
    c->rays = 9u; c->downs = 2u; c->react = HEN_REACT_NONE;
    c->fov = 120.0f * (HEN_PI / 180.0f); c->range = 1.2f; c->radius = 0.15f; c->margin = 0.05f;
    c->tick = 0.1f; c->turn_rate = 4.0f; c->gain = 1.6f; c->edge_ahead = 0.35f; c->see_range = 6.0f;
}

/* The fan's reach for a top speed: the least reach, or two ticks of travel plus the body and its gap. */
static inline float hen_eyes_reach(const hen_eyes_cfg_t *c, float top_speed) {
    const float need = 2.0f * top_speed * c->tick + c->radius + c->margin;
    return need > c->range ? need : c->range;
}

/* phase: 0 .. 1, the agent's place in the tick (a random number an agent spreads them over the frames). */
static inline void hen_eyes_init(hen_eyes_t *e, const hen_eyes_cfg_t *c, float phase) {
    memset(e, 0, sizeof *e);
    e->clock = c->tick * sh4der_sat_(phase);
    e->slow = 1.0f;
}

/* ---- the world ---------------------------------------------------------------------------------------------------- */

static inline void hen_world_begin(hen_world_t *w, hen_tri_t *storage, unsigned cap, float eye_y, float top_y) {
    memset(w, 0, sizeof *w);
    w->tri = storage; w->cap = cap; w->eye_y = eye_y; w->top_y = top_y;
}

static inline void hen_tri_set_(hen_tri_t *t, const float a[3], const float b[3], const float c[3]) {
    unsigned k;
    for (k = 0; k < 3u; k++) { t->a[k] = a[k]; t->e1[k] = b[k] - a[k]; t->e2[k] = c[k] - a[k]; t->pad[k] = 0.0f; }
}

/* An object: nt triangles of corner indices into v. 0 when the world is full (the object is left out). */
static inline int hen_world_add(hen_world_t *w, const float (*v)[3], const uint16_t (*t)[3], unsigned nt) {
    hen_obj_t *o;
    unsigned pass, i, k, n = 0;

    if (w->nobj >= HEN_OBJS_MAX || w->ntri + nt > w->cap || nt == 0u || w->ntri + nt > 65535u) return 0;
    o = &w->obj[w->nobj];
    memset(o, 0, sizeof *o);
    o->t0 = (uint16_t)w->ntri; o->tn = (uint16_t)nt;
    for (k = 0; k < 3u; k++) { o->lo[k] = HEN_FAR; o->hi[k] = -HEN_FAR; }
    o->foot[0] = o->foot[2] = HEN_FAR; o->foot[1] = o->foot[3] = -HEN_FAR;
    for (pass = 0; pass < 2u; pass++)                      /* the triangles that span the eye's height first */
        for (i = 0; i < nt; i++) {
            const float *a = v[t[i][0]], *b = v[t[i][1]], *c = v[t[i][2]];
            float lo[3], hi[3];

            for (k = 0; k < 3u; k++) {
                lo[k] = a[k] < b[k] ? (a[k] < c[k] ? a[k] : c[k]) : (b[k] < c[k] ? b[k] : c[k]);
                hi[k] = a[k] > b[k] ? (a[k] > c[k] ? a[k] : c[k]) : (b[k] > c[k] ? b[k] : c[k]);
            }
            if ((lo[1] <= w->eye_y + HEN_TRI_EPS && hi[1] >= w->eye_y - HEN_TRI_EPS) != (pass == 0u)) continue;
            hen_tri_set_(&w->tri[o->t0 + n++], a, b, c);
            if (pass == 0u) o->sn++;
            for (k = 0; k < 3u; k++) { if (lo[k] < o->lo[k]) o->lo[k] = lo[k]; if (hi[k] > o->hi[k]) o->hi[k] = hi[k]; }
            if (lo[1] < w->top_y && hi[1] > 0.0f) {         /* it stands in the agent's height */
                o->has_foot = 1u;
                if (lo[0] < o->foot[0]) o->foot[0] = lo[0];
                if (hi[0] > o->foot[1]) o->foot[1] = hi[0];
                if (lo[2] < o->foot[2]) o->foot[2] = lo[2];
                if (hi[2] > o->foot[3]) o->foot[3] = hi[2];
            }
        }
    for (k = 0; k < 3u; k++) { o->lo[k] -= HEN_TRI_EPS; o->hi[k] += HEN_TRI_EPS; }
    w->ntri += nt; w->nobj++;
    return 1;
}

/* The floor: a rectangle at height y. The rays down test its two triangles; the push keeps a body inside it. */
static inline void hen_world_floor(hen_world_t *w, float x0, float x1, float z0, float z1, float y) {
    const float p[4][3] = { { x0, y, z0 }, { x0, y, z1 }, { x1, y, z1 }, { x1, y, z0 } };
    hen_tri_set_(&w->floor[0], p[0], p[1], p[2]);
    hen_tri_set_(&w->floor[1], p[0], p[2], p[3]);
    w->floor_rect[0] = x0; w->floor_rect[1] = x1; w->floor_rect[2] = z0; w->floor_rect[3] = z1;
    w->has_floor = 1;
}

#ifdef ROOM_DATA_H
/* The room of room_data.h: every caster object (a wall's caster is a slab behind it), and the floor's surface.
   Returns the objects taken. storage needs ROOM_CT triangles. */
static inline unsigned hen_world_from_room(hen_world_t *w, hen_tri_t *storage, unsigned cap, float eye_y, float top_y) {
    unsigned k, s, taken = 0;

    hen_world_begin(w, storage, cap, eye_y, top_y);
    for (k = 0; k < ROOM_OBJS; k++)
        if ((room_obj[k].flags & ROOM_CASTER) && room_obj[k].ctn)
            taken += (unsigned)hen_world_add(w, room_cv, &room_ct[room_obj[k].ct0], room_obj[k].ctn);
    for (s = 0; s < ROOM_SURFS; s++)
        if (room_surf[s].kind == ROOM_FLOOR) {
            const room_surf_t *f = &room_surf[s];
            const float x1 = f->o[0] + f->du[0] + f->dv[0], z1 = f->o[2] + f->du[2] + f->dv[2];
            hen_world_floor(w, f->o[0] < x1 ? f->o[0] : x1, f->o[0] < x1 ? x1 : f->o[0], f->o[2] < z1 ? f->o[2] : z1, f->o[2] < z1 ? z1 : f->o[2], f->o[1]);
        }
    return taken;
}
#endif

/* ---- the kernels -------------------------------------------------------------------------------------------------- */

/* 1 / d for a slab test: the sign kept, a direction along the slab made a very large number. */
static inline float hen_inv_(float d) {
    const float a = sh4der_abs_(d);
    return a > 1e-6f ? (d < 0.0f ? -sh4der_recip_pos_(a) : sh4der_recip_pos_(a)) : 1e12f;
}

/* A ray against one axis of a box: narrows tn .. tf. */
static inline void hen_slab_(float lo, float hi, float o, float inv, float *tn, float *tf) {
    const float t1 = (lo - o) * inv, t2 = (hi - o) * inv, a = t1 < t2 ? t1 : t2, b = t1 < t2 ? t2 : t1;
    if (a > *tn) *tn = a;
    if (b < *tf) *tf = b;
}

/* Reference: the distance along o + t d to the triangle, every quantity worked out, one divide; HEN_FAR for none.
   Both faces count. */
static inline float hen_tri_ref(const hen_tri_t *t, const float o[3], const float d[3]) {
    float h[3], q[3], s[3], det, u, v, w;

    sh4der_cross_(d, t->e2, h);
    det = sh4der_dot3_(t->e1, h);
    s[0] = o[0] - t->a[0]; s[1] = o[1] - t->a[1]; s[2] = o[2] - t->a[2];
    sh4der_cross_(s, t->e1, q);
    u = sh4der_dot3_(s, h); v = sh4der_dot3_(d, q); w = sh4der_dot3_(t->e2, q);
    if (det < 0.0f) { det = -det; u = -u; v = -v; w = -w; }
    if (!(det > 1e-9f) || u < 0.0f || u > det || v < 0.0f || u + v > det || !(w > HEN_TRI_EPS * det)) return HEN_FAR;
    return sh4der_div_(w, det);
}

/* The kernel: the same arithmetic, leaving at the first test that fails. 1 and *best made the distance when the
   triangle is hit nearer than *best; the compare is by a multiply, the reciprocal only for a nearer hit. */
static inline int hen_tri_near(const hen_tri_t *t, const float o[3], const float d[3], float *best) {
    float h[3], q[3], s[3], det, u, v, w, sign = 1.0f;

    sh4der_cross_(d, t->e2, h);
    det = sh4der_dot3_(t->e1, h);
    s[0] = o[0] - t->a[0]; s[1] = o[1] - t->a[1]; s[2] = o[2] - t->a[2];
    u = sh4der_dot3_(s, h);
    if (det < 0.0f) { det = -det; u = -u; sign = -1.0f; }
    if (!(det > 1e-9f) || u < 0.0f || u > det) return 0;
    sh4der_cross_(s, t->e1, q);
    v = sign * sh4der_dot3_(d, q);
    if (v < 0.0f || u + v > det) return 0;
    w = sign * sh4der_dot3_(t->e2, q);
    if (!(w > HEN_TRI_EPS * det) || !(w < *best * det)) return 0;
    *best = w * sh4der_recip_pos_(det);
    return 1;
}

/* A level ray from (ox, eye, oz) along the unit (dx, 0, dz): the nearest of the room's triangles nearer than *best. */
static inline int hen_ray_level(const hen_world_t *w, float ox, float oz, float dx, float dz, float *best, hen_stats_t *st) {
    const float o[3] = { ox, w->eye_y, oz }, d[3] = { dx, 0.0f, dz }, ix = hen_inv_(dx), iz = hen_inv_(dz);
    unsigned k, i;
    int hit = 0;

    for (k = 0; k < w->nobj; k++) {
        const hen_obj_t *b = &w->obj[k];
        float tn = 0.0f, tf = *best;

        if (!b->sn) continue;
        st->box++;
        hen_slab_(b->lo[0], b->hi[0], ox, ix, &tn, &tf);
        hen_slab_(b->lo[2], b->hi[2], oz, iz, &tn, &tf);
        if (tn > tf) continue;
        st->tri += b->sn;
        for (i = 0; i < b->sn; i++) hit |= hen_tri_near(&w->tri[b->t0 + i], o, d, best);
    }
    return hit;
}

/* 1 when something of the room is between a and b (every triangle of an object whose box the segment enters). */
static inline int hen_blocked(const hen_world_t *w, const float a[3], const float b[3], hen_stats_t *st) {
    const float d[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] };
    const float inv[3] = { hen_inv_(d[0]), hen_inv_(d[1]), hen_inv_(d[2]) };
    unsigned k, i;

    for (k = 0; k < w->nobj; k++) {
        const hen_obj_t *o = &w->obj[k];
        float tn = 0.0f, tf = 1.0f;

        st->box++;
        hen_slab_(o->lo[0], o->hi[0], a[0], inv[0], &tn, &tf);
        hen_slab_(o->lo[1], o->hi[1], a[1], inv[1], &tn, &tf);
        hen_slab_(o->lo[2], o->hi[2], a[2], inv[2], &tn, &tf);
        if (tn > tf) continue;
        for (i = 0; i < o->tn; i++) {
            float best = 1.0f;                               /* the segment's end */
            st->tri++;
            if (hen_tri_near(&w->tri[o->t0 + i], a, d, &best)) return 1;
        }
    }
    return 0;
}

/* A level ray against a circle (another agent): the distance, when nearer than *best. */
static inline int hen_ray_circle(float ox, float oz, float dx, float dz, float cx, float cz, float r, float *best) {
    const float mx = ox - cx, mz = oz - cz, b = mx * dx + mz * dz, c = mx * mx + mz * mz - r * r;
    float disc, t;

    if (c <= 0.0f) { *best = 0.0f; return 1; }               /* inside it */
    if (b > 0.0f) return 0;                                  /* it is behind */
    disc = b * b - c;
    if (disc < 0.0f) return 0;
    t = -b - sh4der_sqrt_(disc);
    if (!(t < *best)) return 0;
    *best = t;
    return 1;
}

/* ---- a tick: cast, then the steering ------------------------------------------------------------------------------ */

static inline void hen_eyes_tick(const hen_world_t *w, hen_eyes_t *e, const hen_eyes_cfg_t *c, float x, float z, float bearing,
                                 float top_speed, const float (*others)[2], unsigned n_others, unsigned self, const float *viewer,
                                 hen_stats_t *st) {
    const unsigned n = c->rays < 3u ? 3u : (c->rays > HEN_RAYS_MAX ? HEN_RAYS_MAX : c->rays | 1u), mid = (n - 1u) / 2u;
    const float reach = hen_eyes_reach(c, top_speed), inv_reach = sh4der_recip_pos_(reach), stop = c->radius + c->margin;
    const float step = c->fov * sh4der_recip_pos_((float)(n - 1u));
    const float ahead = 2.0f * top_speed * c->tick + c->radius > c->edge_ahead ? 2.0f * top_speed * c->tick + c->radius : c->edge_ahead;
    const float down[3] = { 0.0f, -1.0f, 0.0f };
    float sb, cb, left = 0.0f, right = 0.0f, push = 0.0f, front, side, want, slow;
    uint8_t near[HEN_NEAR_MAX];
    unsigned i, j, k, n_near = 0;

    st->ticks++;
    e->reach = reach;
    e->cast_at[0] = x; e->cast_at[1] = z; e->cast_at[2] = bearing; e->cast_at[3] = ahead;
    for (j = 0; j < n_others && n_near < HEN_NEAR_MAX; j++) {                /* the agents a ray can reach: looked up once */
        const float dx = others[j][0] - x, dz = others[j][1] - z, far = reach + c->radius;

        if (j == self) continue;
        st->near++;
        if (dx * dx + dz * dz < far * far) near[n_near++] = (uint8_t)j;
    }
    for (i = 0; i < n; i++) {                                                /* the fan */
        float s, co, best = reach;
        int kind = HEN_MISS;

        sh4der_sincos_(bearing - 0.5f * c->fov + step * (float)i, &s, &co);
        if (hen_ray_level(w, x, z, s, co, &best, st)) kind = HEN_HIT_ROOM;
        for (j = 0; j < n_near; j++) {
            st->pair++;
            if (hen_ray_circle(x, z, s, co, others[near[j]][0], others[near[j]][1], c->radius, &best)) kind = HEN_HIT_AGENT;
        }
        e->dist[i] = best; e->hit[i] = (uint8_t)kind;
        st->ray_fan++;
    }
    for (k = 0; k < (c->downs > 1u ? 2u : 1u); k++) {                        /* the rays down */
        float s, co, o[3], best = HEN_FAR;
        int ok = 0;

        sh4der_sincos_(bearing + (c->downs > 1u ? (k ? 0.5f : -0.5f) : 0.0f), &s, &co);
        o[0] = x + s * ahead; o[1] = w->eye_y; o[2] = z + co * ahead;
        if (w->has_floor) { st->tri += 2u; ok = hen_tri_near(&w->floor[0], o, down, &best) | hen_tri_near(&w->floor[1], o, down, &best); }
        e->floor_ok[k] = (uint8_t)ok;
        st->ray_down++;
    }
    if (c->downs <= 1u) e->floor_ok[1] = e->floor_ok[0];

    for (i = 0; i < mid; i++) left += e->dist[i];                            /* the steering */
    for (i = mid + 1u; i < n; i++) right += e->dist[i];
    side = right >= left ? 1.0f : -1.0f;                                     /* the freer side: + is clockwise */
    for (i = 0; i < n; i++) {
        const float close = 1.0f - e->dist[i] * inv_reach;
        const unsigned off = i < mid ? mid - i : i - mid;

        if (i == mid) push += side * close * 1.5f;
        else push += (i < mid ? 1.0f : -1.0f) * close * (1.0f - 0.5f * (float)off * sh4der_recip_pos_((float)mid));
    }
    want = c->gain * push * sh4der_recip_pos_((float)mid);
    front = e->dist[mid];
    if (e->dist[mid - 1u] < front) front = e->dist[mid - 1u];
    if (e->dist[mid + 1u] < front) front = e->dist[mid + 1u];
    slow = sh4der_sat_((front - stop) * sh4der_recip_pos_(reach - stop));
    if (front <= stop) { slow = 0.0f; if (sh4der_abs_(want) < 0.8f) want = side * 0.8f; }
    if (!e->floor_ok[0] && !e->floor_ok[1]) { slow = 0.0f; want = side * 2.4f; }            /* an edge ahead */
    else if (!e->floor_ok[0]) { want += 1.2f; slow *= 0.3f; }                                /* on the left: turn right */
    else if (!e->floor_ok[1]) { want -= 1.2f; slow *= 0.3f; }

    e->sight = HEN_SIGHT_NONE; e->sees = 0u; e->flee = 0u;                    /* the viewer */
    if (viewer) {
        const float dx = viewer[0] - x, dz = viewer[2] - z, d2 = dx * dx + dz * dz;
        float half_s, half_c;

        e->view[0] = viewer[0]; e->view[1] = viewer[1]; e->view[2] = viewer[2];
        sh4der_sincos_(bearing, &sb, &cb);
        sh4der_sincos_(0.5f * c->fov, &half_s, &half_c);
        if (d2 < c->see_range * c->see_range && d2 > 1e-6f) {
            const float inv = sh4der_recip_pos_(sh4der_sqrt_(d2)), dot = (dx * sb + dz * cb) * inv, cross = (dx * cb - dz * sb) * inv;

            if (dot >= half_c) {
                const float o[3] = { x, w->eye_y, z };

                st->ray_sight++;
                e->sight = hen_blocked(w, o, viewer, st) ? HEN_SIGHT_BLOCKED : HEN_SIGHT_CLEAR;
                if (e->sight == HEN_SIGHT_CLEAR) {
                    e->sees = 1u;
                    if (c->react == HEN_REACT_FREEZE) { slow = 0.0f; want = 0.0f; }
                    else if (c->react == HEN_REACT_LOOK) { slow = 0.0f; want = (cross < 0.0f ? -1.0f : 1.0f) * (1.0f - dot) * (0.5f * HEN_PI); }
                    else if (c->react == HEN_REACT_FLEE) { e->flee = 1u; want += (cross < 0.0f ? 1.0f : -1.0f) * (1.0f + dot) * (0.5f * HEN_PI); }
                }
            }
        }
    }
    e->want = sh4der_clamp_(want, -2.6f, 2.6f);
    e->slow = slow;
}

/* ---- a frame ------------------------------------------------------------------------------------------------------ */

/* Before the caller's walk: a tick when it is due (one a frame at the most), then the turn toward the wanted bearing.
   Returns the part of this frame's step to keep. others: every agent's (x, z), this one's index `self` skipped. */
static inline float hen_eyes_pre(const hen_world_t *w, hen_eyes_t *e, const hen_eyes_cfg_t *c, float dt, float x, float z, float *bearing,
                                 float top_speed, const float (*others)[2], unsigned n_others, unsigned self, const float *viewer,
                                 hen_stats_t *st) {
    const float most = c->turn_rate * dt;
    float turn;

    st->frames++;
    e->ticked = 0u;
    e->clock -= dt;
    if (e->clock <= 0.0f) {
        e->clock += c->tick;
        if (e->clock <= 0.0f) e->clock = c->tick;            /* a frame longer than the tick: one tick, not two */
        hen_eyes_tick(w, e, c, x, z, *bearing, top_speed, others, n_others, self, viewer, st);
        e->ticked = 1u;
    }
    turn = sh4der_clamp_(e->want, -most, most);
    *bearing += turn; e->want -= turn;
    if (*bearing > HEN_PI) *bearing -= 2.0f * HEN_PI;
    if (*bearing < -HEN_PI) *bearing += 2.0f * HEN_PI;
    return e->slow;
}

/* A circle against a rectangle on the floor: moved out of it. 1 when it moved. */
static inline int hen_push_rect_(const float r4[4], float rad, float *x, float *z) {
    const float cx = sh4der_clamp_(*x, r4[0], r4[1]), cz = sh4der_clamp_(*z, r4[2], r4[3]), dx = *x - cx, dz = *z - cz, d2 = dx * dx + dz * dz;

    if (d2 >= rad * rad) return 0;
    if (d2 > 1e-10f) {                                       /* outside it: straight out from the nearest point */
        const float d = sh4der_sqrt_(d2), k = (rad - d) * sh4der_recip_pos_(d);
        *x += dx * k; *z += dz * k;
    } else {                                                 /* inside it: out by the nearest side */
        const float l = *x - r4[0], r = r4[1] - *x, n = *z - r4[2], f = r4[3] - *z;
        if (l <= r && l <= n && l <= f) *x = r4[0] - rad;
        else if (r <= n && r <= f) *x = r4[1] + rad;
        else if (n <= f) *z = r4[2] - rad;
        else *z = r4[3] + rad;
    }
    return 1;
}

/* After the caller's walk from (x0, z0) to (*x, *z): keep `slow` of the step, then the push out of every footprint and
   back onto the floor. Up to four rounds, for a corner between two things. */
static inline void hen_eyes_post(const hen_world_t *w, const hen_eyes_cfg_t *c, float x0, float z0, float *x, float *z, float slow,
                                 hen_stats_t *st) {
    unsigned round, k;

    *x = x0 + (*x - x0) * slow; *z = z0 + (*z - z0) * slow;
    for (round = 0; round < 4u; round++) {
        int moved = 0;

        for (k = 0; k < w->nobj; k++) {
            if (!w->obj[k].has_foot) continue;
            st->push++;
            moved |= hen_push_rect_(w->obj[k].foot, c->radius, x, z);
        }
        if (w->has_floor) {
            const float fx = sh4der_clamp_(*x, w->floor_rect[0] + c->radius, w->floor_rect[1] - c->radius);
            const float fz = sh4der_clamp_(*z, w->floor_rect[2] + c->radius, w->floor_rect[3] - c->radius);
            if (fx != *x || fz != *z) { *x = fx; *z = fz; moved = 1; }
        }
        if (!moved) break;
    }
}

/* Once a frame, after every agent's post: agents that overlap are moved apart, half each. Every pair is tested: n (n - 1) / 2.
   Both the eyes and a collision sweep need this to keep two bodies apart, so it is counted apart from either. */
static inline void hen_agents_separate(float (*pos)[2], unsigned n, float radius, hen_stats_t *st) {
    unsigned i, j;

    for (i = 0; i + 1u < n; i++)
        for (j = i + 1u; j < n; j++) {
            const float dx = pos[j][0] - pos[i][0], dz = pos[j][1] - pos[i][1], d2 = dx * dx + dz * dz, need = 2.0f * radius;

            st->sep++;
            if (d2 >= need * need || d2 < 1e-10f) continue;
            {
                const float d = sh4der_sqrt_(d2), k = 0.5f * (need - d) * sh4der_recip_pos_(d);
                pos[i][0] -= dx * k; pos[i][1] -= dz * k; pos[j][0] += dx * k; pos[j][1] += dz * k;
            }
        }
}

/* ---- the debug layer ---------------------------------------------------------------------------------------------- */

#define HEN_DEBUG_SEGS (HEN_RAYS_MAX + 3u)
#define HEN_ARGB_MISS   0xff30c040u      /* a fan ray that met nothing: green */
#define HEN_ARGB_ROOM   0xffe03020u      /* it met the room: red */
#define HEN_ARGB_AGENT  0xffe0c020u      /* it met another agent: yellow */
#define HEN_ARGB_FLOOR  0xff2060e0u      /* a ray down with floor under it: blue */
#define HEN_ARGB_EDGE   0xffe020c0u      /* a ray down with none: magenta */
#define HEN_ARGB_SEEN   0xffffffffu      /* the line to the viewer, clear: white */
#define HEN_ARGB_HIDDEN 0xff606060u      /* the line to the viewer, blocked: grey */

/* The last tick's rays as segments (seg[i][0] to seg[i][1]) with a colour each: at most HEN_DEBUG_SEGS. Draw each as
   a thin quad that faces the camera (sh4der_spark_quad), untextured and additive, about 1.5 pixels wide. */
static inline unsigned hen_eyes_debug(const hen_world_t *w, const hen_eyes_t *e, const hen_eyes_cfg_t *c, float (*seg)[2][3], uint32_t *argb) {
    const unsigned n = c->rays < 3u ? 3u : (c->rays > HEN_RAYS_MAX ? HEN_RAYS_MAX : c->rays | 1u);
    const float step = c->fov * sh4der_recip_pos_((float)(n - 1u)), x = e->cast_at[0], z = e->cast_at[1], b = e->cast_at[2];
    unsigned i, k, m = 0;

    for (i = 0; i < n; i++, m++) {
        float s, co;
        sh4der_sincos_(b - 0.5f * c->fov + step * (float)i, &s, &co);
        seg[m][0][0] = x; seg[m][0][1] = w->eye_y; seg[m][0][2] = z;
        seg[m][1][0] = x + s * e->dist[i]; seg[m][1][1] = w->eye_y; seg[m][1][2] = z + co * e->dist[i];
        argb[m] = e->hit[i] == HEN_HIT_ROOM ? HEN_ARGB_ROOM : e->hit[i] == HEN_HIT_AGENT ? HEN_ARGB_AGENT : HEN_ARGB_MISS;
    }
    for (k = 0; k < (c->downs > 1u ? 2u : 1u); k++, m++) {
        float s, co;
        sh4der_sincos_(b + (c->downs > 1u ? (k ? 0.5f : -0.5f) : 0.0f), &s, &co);
        seg[m][0][0] = seg[m][1][0] = x + s * e->cast_at[3]; seg[m][0][2] = seg[m][1][2] = z + co * e->cast_at[3];
        seg[m][0][1] = w->eye_y; seg[m][1][1] = 0.0f;
        argb[m] = e->floor_ok[k] ? HEN_ARGB_FLOOR : HEN_ARGB_EDGE;
    }
    if (e->sight != HEN_SIGHT_NONE) {
        seg[m][0][0] = x; seg[m][0][1] = w->eye_y; seg[m][0][2] = z;
        seg[m][1][0] = e->view[0]; seg[m][1][1] = e->view[1]; seg[m][1][2] = e->view[2];
        argb[m++] = e->sight == HEN_SIGHT_CLEAR ? HEN_ARGB_SEEN : HEN_ARGB_HIDDEN;
    }
    return m;
}

#endif /* HEN_EYES_H */
