/* SH4DER-PVR: the perspective shadow map demo's second scene, the town square

   demo/psm/psm_town.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   The market square of TOWN_STORY.md (Hallowmere, Michaelmas, about 1420, 52.2 N), as TOWN_MOCKUP.md placed it: the
   mesh is tools/bake_town.py's (psm_town_mesh.h in the build folder, PSM_HAVE_TOWN), this file is the rest of it: the sun
   on the day's arc, the clock, the cast's walks, the hens, the presets, the walk area, the sky and fog table, and the
   scene's set-up. Included by psm_scene.h after psm_state_t; nothing here is a KallistiOS call.

   Story coordinates: metres, x east, z north, origin the open square's centre. The program's world has z south (the camera
   looks along -z at yaw 0, which is north), so a story point (x, z) is the world's (x, -z), and a compass bearing is the
   camera's yaw and the sun's azimuth as psm_scene.h takes them.

   The people are placeholders (grey blocks with the role's accent band) and the hens a procedural low hen: not final art.
*/

#ifndef PSM_TOWN_H
#define PSM_TOWN_H

#include <math.h>

#define PSM_TOWN_LAT      52.2f       /* degrees north */
#define PSM_TOWN_DEC      (-5.9f)     /* the sun's declination on the day (Julian 29 September = about 8 October) */
#define PSM_CLOCK_T0      9.0f        /* the clock's sweep, solar hours (after 16:00 the square is all shade) */
#define PSM_CLOCK_T1      16.0f
#define PSM_CLOCK_STEP    0.25f       /* a D-pad step: a quarter of an hour */
#define PSM_CLOCK_BOOT    14.0f
#define PSM_TIME_SPEEDS   4u
#define PSM_TOWN_PRESETS  6u
#define PSM_CAST          8u          /* people in the square at once */
#define PSM_HENS          4u
#define PSM_CAST_RATE     1.3f        /* metres a second, walking */
#define PSM_HEN_FLEE      1.5f        /* a walker (or the camera) this near sends a hen off */
#define PSM_TOWN_SLAB_A   0.5f        /* the fit's receivers: route A the ground only */
#define PSM_TOWN_SLAB_B   13.0f       /* route B the ground and the walls up to the guildhall's eave */
#define PSM_TOWN_GROUND_UV (1.0f / 3.0f)   /* the cobble texture: two 1.5 m cells a repeat */

static const unsigned short psm_time_speed[PSM_TIME_SPEEDS] = { 0u, 1u, 8u, 60u };   /* paused, x1, x8, x60 */

/* The sun at solar time `hour`: azimuth from north, clockwise, and elevation, degrees. The standard formula (hour angle,
   latitude, declination); no refraction, no equation of time, as TOWN_STORY.md's table. */
static inline void psm_sun_at(float hour, float *az, float *el) {
    const float d2r = 0.01745329f, lat = PSM_TOWN_LAT * d2r, dec = PSM_TOWN_DEC * d2r, h = (hour - 12.0f) * 15.0f * d2r;
    const float se = sinf(lat) * sinf(dec) + cosf(lat) * cosf(dec) * cosf(h);
    float a = atan2f(sinf(h), cosf(h) * sinf(lat) - tanf(dec) * cosf(lat)) * 57.29578f + 180.0f;

    if (a >= 360.0f) a -= 360.0f;
    *az = a;
    *el = asinf(sh4der_clamp_(se, -1.0f, 1.0f)) * 57.29578f;
}

/* ---- the cast: placeholders on the story's routes, from the clock alone ---------------------------------------- */

typedef struct psm_wp {
    float x, z, pause;                  /* story metres; seconds stood there */
} psm_wp_t;

/* TOWN_STORY.md's routes for the eight in the square at once (the boy and the friar are out of a1), kept off the pen,
   the counters and the cross. A route walks back to its first point, except the porter's, which leaves by the south street
   and comes in again behind the arch. */
static const psm_wp_t psm_cast_wp[] = {
    { -11.0f, -9.0f, 2.0f }, { -0.6f, -8.2f, 6.0f },                                               /* water girl: door, well */
    { -11.4f, 3.0f, 5.0f }, { -9.0f, 2.0f, 4.0f },                                                 /* alewife: tavern door, the cup */
    { -9.0f, 2.0f, 6.0f }, { 8.4f, -6.0f, 4.0f }, { 7.9f, -2.6f, 4.0f }, { 6.9f, 0.5f, 4.0f },     /* ale-conner: every stall */
    { 8.6f, 6.0f, 4.0f }, { 0.0f, 11.0f, 4.0f },
    { 11.0f, 2.0f, 8.0f }, { 11.0f, 0.9f, 5.0f },                                                  /* poulterer, under the awning */
    { 11.0f, 6.0f, 30.0f }, { 11.0f, 7.7f, 0.0f }, { 8.6f, 7.7f, 0.0f }, { 8.6f, 6.2f, 5.0f },     /* clothier: out to the front */
    { 8.6f, 7.7f, 0.0f }, { 11.0f, 7.7f, 0.0f },
    { 9.4f, 11.3f, 3.0f }, { 11.6f, 11.3f, 3.0f },                                                 /* sweeper, the doorstep */
    { 7.4f, 10.6f, 20.0f }, { 7.6f, 9.2f, 5.0f },                                                  /* roofer's mate at the ladder */
    { 0.0f, 19.0f, 0.0f }, { 0.0f, 13.0f, 0.0f }, { 8.3f, -3.6f, 2.0f }, { 1.5f, -16.0f, 0.0f },   /* porter: arch, the sacks, out */
    { 1.5f, -24.0f, 3.0f }
};
static const unsigned char psm_cast_route[PSM_CAST][3] = {   /* first point, points, 1: open (the last point jumps to the first) */
    { 0, 2, 0 }, { 2, 2, 0 }, { 4, 6, 0 }, { 10, 2, 0 }, { 12, 6, 0 }, { 18, 2, 0 }, { 20, 2, 0 }, { 22, 5, 1 }
};
static const char *const psm_cast_name[PSM_CAST] = {
    "water girl", "alewife", "ale-conner", "poulterer", "clothier", "sweeper", "roofer's mate", "porter"
};

/* a route's leg k: from point k to the next (the open route's last leg is a jump: no walk) */
static inline float psm_cast_leg_(unsigned who, unsigned k, float *ax, float *az, float *bx, float *bz) {
    const unsigned f = psm_cast_route[who][0], n = psm_cast_route[who][1];
    const psm_wp_t *a = &psm_cast_wp[f + k], *b = &psm_cast_wp[f + (k + 1u) % n];
    float dx = b->x - a->x, dz = b->z - a->z;

    *ax = a->x; *az = a->z; *bx = b->x; *bz = b->z;
    if (psm_cast_route[who][2] && k + 1u == n) return 0.0f;
    return sh4der_sqrt_(dx * dx + dz * dz) * (1.0f / PSM_CAST_RATE);
}

/* seconds a loop of the route takes */
static inline float psm_cast_period(unsigned who) {
    const unsigned f = psm_cast_route[who][0], n = psm_cast_route[who][1];
    float ax, az, bx, bz, t = 0.0f;
    unsigned k;

    for (k = 0; k < n; k++) t += psm_cast_wp[f + k].pause + psm_cast_leg_(who, k, &ax, &az, &bx, &bz);
    return t;
}

/* Where `who` is at t seconds of the cast's clock, story metres, and the bearing faced (radians, 0 north, clockwise). */
static inline void psm_cast_at(unsigned who, float t, float *x, float *z, float *bearing) {
    const unsigned f = psm_cast_route[who][0], n = psm_cast_route[who][1];
    const float period = psm_cast_period(who);
    float ax, az, bx, bz, leg;
    unsigned k;

    t -= (float)(int)(t / period) * period;
    if (t < 0.0f) t += period;
    for (k = 0; k < n; k++) {
        const float stand = psm_cast_wp[f + k].pause;

        leg = psm_cast_leg_(who, k, &ax, &az, &bx, &bz);
        if (t < stand) {                              /* standing: facing along the leg ahead (the one before, at an open end) */
            float px, pz, qx, qz;

            *x = ax; *z = az;
            if (leg <= 0.0f && k > 0u) { psm_cast_leg_(who, k - 1u, &px, &pz, &qx, &qz); *bearing = atan2f(qx - px, qz - pz); }
            else *bearing = atan2f(bx - ax, bz - az);
            return;
        }
        t -= stand;
        if (t < leg) {
            const float u = t * sh4der_recip_pos_(leg);

            *x = ax + (bx - ax) * u; *z = az + (bz - az) * u;
            *bearing = atan2f(bx - ax, bz - az);
            return;
        }
        t -= leg;
    }
    *x = psm_cast_wp[f].x; *z = psm_cast_wp[f].z; *bearing = 0.0f;
}

/* ---- the hens: peck, step, flee -------------------------------------------------------------------------------- */

static const float psm_hen_home[PSM_HENS][4] = {   /* story x, z, and 1 for the pen (bounds the pen's inside), 0 loose */
    { 8.2f, 0.0f, 1.0f, 0.0f }, { 8.8f, 1.2f, 1.0f, 0.0f }, { 7.0f, -1.6f, 0.0f, 0.0f }, { 6.3f, 0.9f, 0.0f, 0.0f }
};

static inline float psm_rand_(uint32_t *r) {
    *r = *r * 1664525u + 1013904223u;
    return (float)(*r >> 8) * (1.0f / 16777216.0f);
}

/* one hen, dt seconds: `near` the walkers (story x, z pairs, n of them; the camera among them) */
static inline void psm_hen_step(psm_mover_t *m, float dt, const float (*near)[2], unsigned n) {
    const float pi = 3.14159265f;
    float fx = 0.0f, fz = 0.0f, best = PSM_HEN_FLEE * PSM_HEN_FLEE, sb, cb;
    unsigned i;

    for (i = 0; i < n; i++) {
        const float dx = m->x - near[i][0], dz = m->z - near[i][1], d2 = dx * dx + dz * dz;

        if (d2 < best) { best = d2; fx = dx; fz = dz; }
    }
    if (best < PSM_HEN_FLEE * PSM_HEN_FLEE && m->mode != 2) {           /* off, away from the nearest walker */
        m->mode = 2; m->t = 0.6f;
        m->bearing = (fx * fx + fz * fz > 1e-6f) ? atan2f(fx, fz) : m->bearing + pi;
    }
    m->t -= dt;
    switch (m->mode) {
        case 0:                                                          /* peck: the body tips forward and back, 0.4 s */
            m->tilt = 0.7f * sinf(pi * sh4der_clamp_(1.0f - m->t * 2.5f, 0.0f, 1.0f));
            if (m->t <= 0.0f) { m->mode = 1; m->t = 0.4f; m->tilt = 0.0f; m->bearing += (psm_rand_(&m->rng) - 0.5f) * 2.0f; }
            break;
        case 1: case 2:                                                  /* a step of 0.2 m, or a run */
            sh4der_sincos_(m->bearing, &sb, &cb);
            m->x += sb * (m->mode == 2 ? 2.0f : 0.5f) * dt;
            m->z += cb * (m->mode == 2 ? 2.0f : 0.5f) * dt;
            m->tilt = 0.0f;
            if (m->t <= 0.0f) { m->mode = 0; m->t = 0.4f; }
            break;
        default: m->mode = 0; m->t = 0.4f; break;
    }
    if (m->x < m->b[0] || m->x > m->b[1]) { m->x = sh4der_clamp_(m->x, m->b[0], m->b[1]); m->bearing = -m->bearing; }   /* the bounds: */
    if (m->z < m->b[2] || m->z > m->b[3]) { m->z = sh4der_clamp_(m->z, m->b[2], m->b[3]); m->bearing = pi - m->bearing; } /* off the wall */
    if (m->bearing > pi) m->bearing -= 2.0f * pi;
    if (m->bearing < -pi) m->bearing += 2.0f * pi;
}

/* ---- presets, the walk, the sky ----------------------------------------------------------------------------------- */

typedef struct psm_town_preset {
    const char *name;
    float hour, x, z, bearing, pitch;   /* solar hours; story metres; degrees (pitch down positive) */
} psm_town_preset_t;

/* TOWN_STORY.md "Where the shadows are, by preset" */
static const psm_town_preset_t psm_town_preset[PSM_TOWN_PRESETS] = {
    { "first view 14:00", 14.0f, 0.0f, -14.0f, 0.0f, 4.0f },
    { "morning 09:00", 9.0f, 0.0f, -14.0f, 0.0f, 4.0f },
    { "tower shadow 15:30", 15.5f, 0.0f, -14.0f, 0.0f, 4.0f },
    { "head-on 16:00", 16.0f, 6.0f, -9.0f, 236.0f, -6.0f },
    { "eave test 13:00", 13.0f, 3.0f, 9.0f, 315.0f, -25.0f },
    { "ladder test 14:00", 14.0f, 5.0f, 8.0f, 45.0f, -15.0f }
};

/* The walk: the open square and the south street to the churchyard (story x -11.4 .. 7.2, z -24 .. 11.4: off the stalls
   and the pen), round the well and the cross. In the world frame. */
#define PSM_TOWN_X0 (-11.4f)
#define PSM_TOWN_X1 7.2f
#define PSM_TOWN_ZS0 (-24.0f)
#define PSM_TOWN_ZS1 11.4f

/* the sky's colour and the fog's, by the clock: one table (TOWN_LOOK.md fills it; a1 has one provisional row, flat) */
#define PSM_TOWN_LOOK_ROWS 1u
static const psm_look_row_t psm_town_look[PSM_TOWN_LOOK_ROWS] = {
    { 14.0f, { 0.62f, 0.72f, 0.84f }, { 0.62f, 0.72f, 0.84f } }     /* PROVISIONAL: a2 brings the sky band and its fog */
};

#ifdef PSM_HAVE_TOWN

/* a template (screen strips and casters, local to the feet) appended as mover m: its vertices after the scene's */
static inline int psm_town_mover_(psm_scene_t *s, unsigned kind, const float (*v)[3], const signed char (*nv)[3], const unsigned char *c,
                                  unsigned nvn, const unsigned short *sv, const unsigned short *sl, unsigned ns,
                                  const float (*cv)[3], unsigned cvn, const unsigned short (*ct)[3], unsigned ctn, unsigned accent) {
    psm_mover_t *m;
    unsigned i, k, o;

    if (s->movern >= PSM_MOVERS_MAX || s->mvn + nvn > PSM_MV_MAX || s->mvn - s->mdyn0 + nvn > PSM_DYN_V_MAX
        || s->cvn + cvn > PSM_CV_MAX || s->cvn - s->cdyn0 + cvn > PSM_DYN_CV_MAX || s->ctn + ctn > PSM_CT_MAX || s->con >= PSM_CO_MAX) return 0;
    m = &s->mover[s->movern++];
    memset(m, 0, sizeof *m);
    m->kind = kind;
    m->v0 = s->mvn; m->vn = nvn;
    for (i = 0; i < nvn; i++) {
        const unsigned d = s->mvn - s->mdyn0;
        float n3[3] = { (float)nv[i][0], (float)nv[i][1], (float)nv[i][2] };

        sh4der_norm3_(n3);
        memcpy(s->dloc[d], v[i], sizeof s->dloc[d]);
        memcpy(s->dnloc[d], n3, sizeof n3);
        s->mkind[s->mvn] = (uint8_t)(c[i] == 255u ? accent : c[i]);
        s->mvn++;
    }
    for (k = 0, o = 0; k < ns; o += sl[k], k++)
        for (i = 0; i + 2u < sl[k]; i++) {
            if (s->mtn >= PSM_MT_MAX) return 0;
            s->mt[s->mtn][0] = (uint16_t)(m->v0 + sv[o + i]); s->mt[s->mtn][1] = (uint16_t)(m->v0 + sv[o + i + 1u]);
            s->mt[s->mtn][2] = (uint16_t)(m->v0 + sv[o + i + 2u]);
            s->mstart[s->mtn] = (uint8_t)(i == 0u);
            s->mtmov[s->mtn] = 1u;
            s->mtn++;
        }
    m->co = s->con;
    s->co[s->con].v0 = s->cvn; s->co[s->con].vn = cvn; s->co[s->con].t0 = s->ctn; s->co[s->con].tn = ctn; s->co[s->con].mover = 1u;
    s->con++;
    m->cv0 = s->cvn; m->cvn = cvn;
    for (i = 0; i < cvn; i++) { memcpy(s->dcloc[s->cvn - s->cdyn0], cv[i], sizeof cv[i]); s->cvn++; }
    for (i = 0; i < ctn; i++) {
        s->ct[s->ctn][0] = (uint16_t)(m->cv0 + ct[i][0]); s->ct[s->ctn][1] = (uint16_t)(m->cv0 + ct[i][1]); s->ct[s->ctn][2] = (uint16_t)(m->cv0 + ct[i][2]);
        s->ctmov[s->ctn] = 1u;
        s->ctn++;
    }
    return 1;
}

/* The town, once, at load (or when the SCENE row picks it). The cast and the hens start where 14:00 has them. */
static inline void psm_scene_init_town(psm_scene_t *s) {
    unsigned i, k, o;

    psm_scene_rings_(s);
    s->town = 1;
    for (i = 0; i < PSM_TOWN_PAL && i < PSM_PAL_MAX; i++) memcpy(s->pal[i], psm_town_pal[i], sizeof s->pal[i]);
    s->paln = i;
    for (i = 0; i < PSM_TOWN_V && i < PSM_MV_MAX; i++) {
        float n3[3] = { (float)psm_town_n[i][0], (float)psm_town_n[i][1], (float)psm_town_n[i][2] };

        sh4der_norm3_(n3);
        memcpy(s->mv[i], psm_town_v[i], sizeof s->mv[i]);
        memcpy(s->mn[i], n3, sizeof n3);
        s->mkind[i] = psm_town_c[i];
    }
    s->mvn = i;
    for (k = 0, o = 0; k < PSM_TOWN_S; o += psm_town_sl[k], k++)
        for (i = 0; i + 2u < psm_town_sl[k] && s->mtn < PSM_MT_MAX; i++) {
            s->mt[s->mtn][0] = psm_town_sv[o + i]; s->mt[s->mtn][1] = psm_town_sv[o + i + 1u]; s->mt[s->mtn][2] = psm_town_sv[o + i + 2u];
            s->mstart[s->mtn] = (uint8_t)(i == 0u);
            s->mtn++;
        }
    for (i = 0; i < PSM_TOWN_CV && i < PSM_CV_MAX; i++) memcpy(s->cv[i], psm_town_cv[i], sizeof s->cv[i]);
    s->cvn = i;
    for (i = 0; i < PSM_TOWN_CT && i < PSM_CT_MAX; i++) memcpy(s->ct[i], psm_town_ct[i], sizeof s->ct[i]);
    s->ctn = i;
    for (i = 0; i < PSM_TOWN_CO && i < PSM_CO_MAX; i++) {
        s->co[i].v0 = psm_town_co[i][0]; s->co[i].vn = psm_town_co[i][1]; s->co[i].t0 = psm_town_co[i][2]; s->co[i].tn = psm_town_co[i][3];
        s->co[i].mover = 0u;
    }
    s->con = i;
    s->mdyn0 = s->mvn; s->tdyn0 = s->mtn; s->cdyn0 = s->cvn;
    for (i = 0; i < PSM_CAST; i++) {
        psm_mover_t *m = &s->mover[s->movern];

        if (!psm_town_mover_(s, 0u, psm_town_person_v, psm_town_person_n, psm_town_person_c, PSM_TOWN_PERSON_V, psm_town_person_sv, psm_town_person_sl,
                             PSM_TOWN_PERSON_S, psm_town_person_cv, PSM_TOWN_PERSON_CV, psm_town_person_ct, PSM_TOWN_PERSON_CT, psm_town_acc[i])) break;
        psm_cast_at(i, 23.0f * (float)i, &m->x, &m->z, &m->bearing);   /* where psm_town_update puts them at cast time 0 */
    }
    for (i = 0; i < PSM_HENS; i++) {
        psm_mover_t *m = &s->mover[s->movern];

        if (!psm_town_mover_(s, 1u, psm_town_hen_v, psm_town_hen_n, psm_town_hen_c, PSM_TOWN_HEN_V, psm_town_hen_sv, psm_town_hen_sl, PSM_TOWN_HEN_S,
                             psm_town_hen_cv, PSM_TOWN_HEN_CV, psm_town_hen_ct, PSM_TOWN_HEN_CT, 0u)) break;
        m->x = psm_hen_home[i][0]; m->z = psm_hen_home[i][1]; m->bearing = 1.3f * (float)i;
        m->rng = 0x9e3779b9u * (i + 1u); m->t = 0.1f * (float)i;
        if (psm_hen_home[i][2] > 0.0f) { m->b[0] = 7.9f; m->b[1] = 9.1f; m->b[2] = -0.85f; m->b[3] = 1.85f; }
        else { m->b[0] = 5.3f; m->b[1] = 7.4f; m->b[2] = -3.0f; m->b[3] = 2.6f; }
    }
    s->amb[0] = 0.48f * 0.80f; s->amb[1] = 0.48f * 0.88f; s->amb[2] = 0.48f * 1.06f;   /* TOWN_MOCKUP.md: shade colour x 0.48 x (0.80, 0.88, 1.06) */
    s->sunc[0] = 1.04f; s->sunc[1] = 0.98f; s->sunc[2] = 0.88f;                         /* sun + N.L x (1.04, 0.98, 0.88) */
    s->slab[PSM_ROUTE_A] = PSM_TOWN_SLAB_A; s->slab[PSM_ROUTE_B] = PSM_TOWN_SLAB_B;
    s->ground_uv = PSM_TOWN_GROUND_UV; s->ground_tex = 1;
    s->look = psm_town_look; s->lookn = PSM_TOWN_LOOK_ROWS;
    s->x0 = PSM_TOWN_X0; s->x1 = PSM_TOWN_X1; s->z0 = -PSM_TOWN_ZS1; s->z1 = -PSM_TOWN_ZS0;
    psm_scene_obst_(s, -2.0f, 7.0f, 1.0f + PSM_WALK_BODY);          /* the well at story (-2, -7): its roof's half width */
    psm_scene_obst_(s, 4.0f, 0.0f, 1.42f + PSM_WALK_BODY);          /* the cross's step, 2 x 2 m: its half diagonal */
    psm_scene_objects_(s);
    psm_movers_apply(s);                                            /* the movers' vertices placed once, so the mesh is whole from load */
}

#endif /* PSM_HAVE_TOWN */

/* The town's state at a preset: the clock, the camera, the sun back on the clock. */
static inline void psm_town_preset_set(psm_state_t *st, unsigned k) {
    const psm_town_preset_t *p = &psm_town_preset[k % PSM_TOWN_PRESETS];

    st->tpreset = k % PSM_TOWN_PRESETS;
    st->clock = p->hour;
    st->sun_free = 0;
    st->x = p->x; st->z = -p->z;
    st->yaw = p->bearing * 0.01745329f;
    st->pitch = p->pitch * 0.01745329f;
    psm_sun_at(st->clock, &st->az, &st->el);
}

/* The boot state of the town image: the field's defaults, the town, the first view at 14:00. */
static inline void psm_state_town(psm_state_t *st) {
    psm_state_default(st);
    st->scene = 1;
    st->atmos = 1;
    st->tspeed = 1u;
    psm_town_preset_set(st, 0u);
}

/* One frame of the town's clock and movers: the sun follows the clock (unless the sun was set free), the cast walks on the
   cast's clock, the hens peck and flee (the camera among the walkers). START + B froze them: the sun's clock goes on. */
static inline void psm_town_update(psm_scene_t *s, psm_state_t *st, float dt) {
    float near[PSM_CAST + 1u][2];
    unsigned i;

    if (!s->town) return;
    if (!(dt > 0.0f)) dt = 0.0f;
    st->clock += dt * (float)psm_time_speed[st->tspeed % PSM_TIME_SPEEDS] * (1.0f / 3600.0f);
    if (st->clock > PSM_CLOCK_T1) st->clock = PSM_CLOCK_T0;          /* the sweep round again */
    if (!st->sun_free) psm_sun_at(st->clock, &st->az, &st->el);
    if (!st->freeze) st->cast_t += dt;
    for (i = 0; i < s->movern; i++) {
        psm_mover_t *m = &s->mover[i];

        if (m->kind == 0u && i < PSM_CAST) {
            psm_cast_at(i, st->cast_t + 23.0f * (float)i, &m->x, &m->z, &m->bearing);
            near[i][0] = m->x; near[i][1] = m->z;
        }
    }
    near[PSM_CAST][0] = st->x; near[PSM_CAST][1] = -st->z;
    if (!st->freeze)
        for (i = 0; i < s->movern; i++)
            if (s->mover[i].kind == 1u) psm_hen_step(&s->mover[i], dt > 0.1f ? 0.1f : dt, (const float (*)[2])near, PSM_CAST + 1u);
    psm_movers_apply(s);
}

#endif /* PSM_TOWN_H */
