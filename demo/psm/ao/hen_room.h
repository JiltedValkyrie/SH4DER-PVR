/* SH4DER-PVR: the hens in the room with eyes or with a sweep, for the hens-eyes entry (demo/psm/ao)

   demo/psm/ao/hen_room.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Up to HENR_MAX hens walking the room of room_data.h, steered one of four ways that do the same job, so their costs
   can be set side by side on one scene:

     HENR_EYES         hen_eyes.h: a fan of rays on a tick in seconds
     HENR_SWEEP_FRAME  hen_sweep.h: a capsule sweep every step, the grid as its broad phase, looking as far as the eyes
     HENR_SWEEP_TICK   the same sweep on the eyes' tick
     HENR_SWEEP_NAIVE  the sweep every step against every triangle: no broad phase

   THE WALK is the town's (../psm_town.h's psm_hen_step without the run from a walker, as ao4_hens.h has it), kept in
   the ROOM's metres here: x, z, and a bearing from +z clockwise, forward (sin b, cos b), which is what hen_eyes.h
   works in. The first four hens are ao4's: the town's homes, bounds and seeds through ao4_hens.h's mapping
   (room x = story z + AO4_HEN_DX, room z = story x - AO4_HEN_DZ, room bearing = pi / 2 - story bearing), so two keep
   to the pen and two roam. Hens five and on start at places from a fixed seed inside the roamers' bounds and roam
   them. The walk goes in steps of HENR_DT whatever a frame takes, so a run is the same run every time, in every mode
   up to what the mode itself decides.

   A STEP, a hen: the eyes or the sweep (the bearing's turn, how much of the step is kept), the walk, the bounds, the
   push out of the furniture; after every hen, the hens apart. HENR_CYC() is read round the two parts, so the
   includer's cycle counter times them: `pre` the eyes or the sweep alone, `post` the push and the hens apart. A host
   build leaves HENR_CYC undefined and the times are 0.

   Needs room_data.h, then hen_eyes.h and hen_sweep.h. Host tested: test/hen_room_test.c. */

#ifndef HEN_ROOM_H
#define HEN_ROOM_H

#ifndef HENR_CYC
#define HENR_CYC() 0ull
#endif

#define HENR_MAX   32u
#define HENR_DT    (1.0f / 60.0f)
#define HENR_SPEED 0.5f
#define HENR_DX    0.2f                     /* ao4_hens.h's AO4_HEN_DX and AO4_HEN_DZ */
#define HENR_DZ    7.05f
#define HENR_GRID  16u
#define HENR_GRID_CAP 4096u

enum { HENR_EYES = 0, HENR_SWEEP_FRAME = 1, HENR_SWEEP_TICK = 2, HENR_SWEEP_NAIVE = 3, HENR_MODES = 4 };

typedef struct henr_hen {
    float x, z, bearing, tilt;              /* room metres; radians from +z clockwise; radians nose down */
    float t, b[4];                          /* the time left in its move; its bounds (x0, x1, z0, z1) */
    int mode;                               /* 0 peck, 1 step */
    uint32_t rng;
} henr_hen_t;

typedef struct henr {
    henr_hen_t h[HENR_MAX];
    hen_eyes_t eyes[HENR_MAX];
    float pos[HENR_MAX][2];
    unsigned n, mode;
    float owed;
    hen_world_t world;
    hen_eyes_cfg_t cfg;
    hen_grid_t grid;
    hen_stats_t st;                         /* the eyes' counts, and the push and the hens apart in every mode */
    hen_sweep_stats_t sst;
    unsigned long steps;                    /* steps walked since the reset */
    unsigned long long cyc_pre, cyc_post;   /* HENR_CYC's sums since the reset */
    unsigned cyc_pre_worst, cyc_post_worst; /* the dearest step */
    unsigned objs, grid_items;
    hen_tri_t tris[ROOM_CT];
    uint16_t gstart[HENR_GRID * HENR_GRID + 1u], gitem[HENR_GRID_CAP], gstamp[ROOM_CT];
} henr_t;

static inline float henr_rand_(uint32_t *r) {
    *r = *r * 1664525u + 1013904223u;
    return (float)(*r >> 8) * (1.0f / 16777216.0f);
}

/* Once: the world and the grid from the room. 0 when the room gave no object or the grid's storage is too small. */
static inline int henr_init(henr_t *r) {
    memset(r, 0, sizeof *r);
    hen_eyes_cfg_default(&r->cfg);
    r->objs = hen_world_from_room(&r->world, r->tris, ROOM_CT, 0.20f, 0.35f);
    if (!r->objs) return 0;
    r->grid_items = hen_grid_build(&r->grid, &r->world, HENR_GRID, r->gstart, r->gitem, HENR_GRID_CAP, r->gstamp);
    return r->grid_items > 0u && r->grid_items <= HENR_GRID_CAP;
}

/* n hens at their start, the counts and times at 0. The cfg (rays, tick) is kept. */
static inline void henr_reset(henr_t *r, unsigned n, unsigned mode) {
    static const float home[4][3] = { { 8.2f, 0.0f, 1.0f }, { 8.8f, 1.2f, 1.0f }, { 7.0f, -1.6f, 0.0f }, { 6.3f, 0.9f, 0.0f } };   /* story x, z; 1: the pen */
    static const float pen[4] = { 7.9f, 9.1f, -0.85f, 1.85f }, loose[4] = { 5.3f, 7.4f, -3.0f, 2.6f };                            /* story x0, x1, z0, z1 */
    uint32_t seed = 0x2545f491u;
    unsigned i;

    r->n = n > HENR_MAX ? HENR_MAX : n; r->mode = mode < HENR_MODES ? mode : HENR_EYES;
    r->owed = 0.0f; r->steps = 0; r->cyc_pre = r->cyc_post = 0; r->cyc_pre_worst = r->cyc_post_worst = 0;
    memset(&r->st, 0, sizeof r->st); memset(&r->sst, 0, sizeof r->sst);
    for (i = 0; i < r->n; i++) {
        henr_hen_t *m = &r->h[i];
        const float *bs = (i < 4u && home[i][2] > 0.0f) ? pen : loose;
        float sx, sz, sb;

        if (i < 4u) { sx = home[i][0]; sz = home[i][1]; sb = 1.3f * (float)i; }
        else {
            sx = loose[0] + (loose[1] - loose[0]) * henr_rand_(&seed); sz = loose[2] + (loose[3] - loose[2]) * henr_rand_(&seed);
            sb = 6.2831853f * henr_rand_(&seed);
        }
        m->x = sz + HENR_DX; m->z = sx - HENR_DZ; m->bearing = 0.5f * HEN_PI - sb; m->tilt = 0.0f;
        while (m->bearing > HEN_PI) m->bearing -= 2.0f * HEN_PI;
        while (m->bearing < -HEN_PI) m->bearing += 2.0f * HEN_PI;
        m->b[0] = bs[2] + HENR_DX; m->b[1] = bs[3] + HENR_DX; m->b[2] = bs[0] - HENR_DZ; m->b[3] = bs[1] - HENR_DZ;
        m->rng = 0x9e3779b9u * (i + 1u); m->t = 0.1f * (float)(i & 3u); m->mode = 0;
        r->pos[i][0] = m->x; r->pos[i][1] = m->z;
        {
            uint32_t ph = m->rng ^ 0x5bd1e995u;

            hen_eyes_init(&r->eyes[i], &r->cfg, henr_rand_(&ph));
        }
    }
}

/* The town's walk, one step, in room space. */
static inline void henr_walk_(henr_hen_t *m, float dt) {
    float sb, cb;

    m->t -= dt;
    if (m->mode == 0) {                     /* peck: the body tips forward and back */
        sh4der_sincos_(HEN_PI * sh4der_clamp_(1.0f - m->t * 2.5f, 0.0f, 1.0f), &sb, &cb);
        m->tilt = 0.7f * sb;
        if (m->t <= 0.0f) { m->mode = 1; m->t = 0.4f; m->tilt = 0.0f; m->bearing -= (henr_rand_(&m->rng) - 0.5f) * 2.0f; }
    } else {                                /* a step of 0.2 m */
        sh4der_sincos_(m->bearing, &sb, &cb);
        m->x += sb * HENR_SPEED * dt; m->z += cb * HENR_SPEED * dt;
        m->tilt = 0.0f;
        if (m->t <= 0.0f) { m->mode = 0; m->t = 0.4f; }
    }
    if (m->x < m->b[0] || m->x > m->b[1]) { m->x = sh4der_clamp_(m->x, m->b[0], m->b[1]); m->bearing = -m->bearing; }
    if (m->z < m->b[2] || m->z > m->b[3]) { m->z = sh4der_clamp_(m->z, m->b[2], m->b[3]); m->bearing = HEN_PI - m->bearing; }
    if (m->bearing > HEN_PI) m->bearing -= 2.0f * HEN_PI;
    if (m->bearing < -HEN_PI) m->bearing += 2.0f * HEN_PI;
}

/* One step of HENR_DT for every hen. */
static inline void henr_step(henr_t *r) {
    unsigned long long pre = 0, post = 0, t0, t1, t2;
    unsigned i;

    for (i = 0; i < r->n; i++) {
        henr_hen_t *m = &r->h[i];
        float slow, x0, z0;

        m->x = r->pos[i][0]; m->z = r->pos[i][1];
        t0 = HENR_CYC();
        if (r->mode == HENR_EYES)
            slow = hen_eyes_pre(&r->world, &r->eyes[i], &r->cfg, HENR_DT, m->x, m->z, &m->bearing, HENR_SPEED, (const float (*)[2])r->pos, r->n, i, NULL, &r->st);
        else
            slow = hen_sweep_pre(&r->world, &r->grid, r->mode == HENR_SWEEP_NAIVE ? HEN_SWEEP_NAIVE : HEN_SWEEP_GRID, r->mode != HENR_SWEEP_TICK, 1, &r->eyes[i],
                                 &r->cfg, HENR_DT, m->x, m->z, &m->bearing, HENR_SPEED, (const float (*)[2])r->pos, r->n, i, &r->sst);
        t1 = HENR_CYC();
        x0 = m->x; z0 = m->z;
        henr_walk_(m, HENR_DT);
        t2 = HENR_CYC();
        hen_eyes_post(&r->world, &r->cfg, x0, z0, &m->x, &m->z, slow, &r->st);
        r->pos[i][0] = m->x; r->pos[i][1] = m->z;
        pre += t1 - t0; post += HENR_CYC() - t2;
    }
    t0 = HENR_CYC();
    hen_agents_separate(r->pos, r->n, r->cfg.radius, &r->st);
    post += HENR_CYC() - t0;
    for (i = 0; i < r->n; i++) { r->h[i].x = r->pos[i][0]; r->h[i].z = r->pos[i][1]; }
    r->steps++;
    r->cyc_pre += pre; r->cyc_post += post;
    if (pre > r->cyc_pre_worst) r->cyc_pre_worst = (unsigned)pre;
    if (post > r->cyc_post_worst) r->cyc_post_worst = (unsigned)post;
}

/* dt seconds of the walk in whole steps (at most 8 a call). Returns the steps walked. */
static inline unsigned henr_advance(henr_t *r, float dt) {
    unsigned n = 0;

    r->owed += dt;
    while (r->owed >= HENR_DT && n < 8u) { henr_step(r); r->owed -= HENR_DT; n++; }
    if (r->owed > HENR_DT) r->owed = 0.0f;
    return n;
}

/* A point and a normal of the town's hen template (-z its beak) where hen k stands: tipped about the hip (0.1 m up),
   turned so the beak points along the walk. */
static inline void henr_place(const henr_hen_t *m, float sb, float cb, float st, float ct, const float p[3], float o[3]) {
    const float y = p[1] * ct + p[2] * st + 0.1f - 0.1f * ct, z = -p[1] * st + p[2] * ct + 0.1f * st;

    o[0] = m->x - p[0] * cb - z * sb; o[1] = y; o[2] = m->z + p[0] * sb - z * cb;
}
static inline void henr_turn(float sb, float cb, float st, float ct, const float n[3], float o[3]) {
    const float y = n[1] * ct + n[2] * st, z = -n[1] * st + n[2] * ct;

    o[0] = -n[0] * cb - z * sb; o[1] = y; o[2] = n[0] * sb - z * cb;
}

#endif /* HEN_ROOM_H */
