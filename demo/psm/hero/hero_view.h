/* SH4DER-PVR: hero1, the orbit camera, the sun's shadow matrix and the clay set

   demo/psm/hero/hero_view.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   The camera orbits a point over the hero: yaw, pitch, distance. Its matrix takes a world point to (X, Y, Z, W) with
   X / W, Y / W the pixel on a 640 x 480 screen and 1 / W the PVR's depth; one FTRV a point. The shadow matrix takes a
   world point along the sun onto the ground (y 0) and into the shadow target's pixels; the ground reads the target at
   the same place (hero_ground_uv). The clay set: a ground and two walls in the room's place (x = -SPAN, z = -SPAN), cut
   into cells so a vertex colour can light them, wound counter-clockwise seen from the inside. Header only. */

#ifndef HERO_VIEW_H
#define HERO_VIEW_H

#include "hero_model.h"

#define HERO_SPAN      3.0f                  /* the ground: x, z in -SPAN .. SPAN (the room's floor) */
#define HERO_WALL_H    3.0f
#define HERO_CELLS     12u                   /* ground cells a side; walls the same along, half up */
#define HERO_NEAR      0.1f
#define HERO_FOCAL     1.921f                /* 1 / tan(27.5 degrees): 55 degrees high */
#define HERO_SET_V     ((HERO_CELLS + 1u) * (HERO_CELLS + 1u) + 2u * (HERO_CELLS + 1u) * (HERO_CELLS / 2u + 1u))

typedef struct hero_cam {
    float eye[3], right[3], up[3], fwd[3];
    float vp[16] __attribute__((aligned(8)));   /* column-major, world -> (X, Y, Z = 1, W) */
} hero_cam_t;

static inline void hero_cam_make(hero_cam_t *c, float yaw, float pitch, float dist, const float target[3]) {
    const float wup[3] = { 0.0f, 1.0f, 0.0f };
    float sy, cy, sp, cp, v[16] __attribute__((aligned(8))), p[16] __attribute__((aligned(8)));
    unsigned k;

    sh4der_sincos_(yaw, &sy, &cy); sh4der_sincos_(pitch, &sp, &cp);
    c->eye[0] = target[0] + dist * cp * sy; c->eye[1] = target[1] + dist * sp; c->eye[2] = target[2] + dist * cp * cy;
    for (k = 0; k < 3u; k++) c->fwd[k] = target[k] - c->eye[k];
    sh4der_norm3_(c->fwd);
    sh4der_cross_(c->fwd, wup, c->right); sh4der_norm3_(c->right);
    sh4der_cross_(c->right, c->fwd, c->up);
    memset(v, 0, sizeof v);
    for (k = 0; k < 3u; k++) { v[k * 4u] = c->right[k]; v[k * 4u + 1u] = c->up[k]; v[k * 4u + 2u] = -c->fwd[k]; }
    v[12] = -sh4der_dot3_(c->right, c->eye); v[13] = -sh4der_dot3_(c->up, c->eye); v[14] = sh4der_dot3_(c->fwd, c->eye); v[15] = 1.0f;
    memset(p, 0, sizeof p);                     /* X = 320 f / aspect x - 320 z, Y = -240 f y - 240 z, Z = 1, W = -z */
    p[0] = 320.0f * HERO_FOCAL * 0.75f; p[5] = -240.0f * HERO_FOCAL;
    p[8] = -320.0f; p[9] = -240.0f; p[11] = -1.0f; p[14] = 1.0f;
#ifndef SH4DER_CORE_NO_SH4ZAM
    shz_xmtrx_load_4x4((const shz_mat4x4_t *)p);
    shz_xmtrx_apply_4x4((const shz_mat4x4_t *)v);
    shz_xmtrx_store_4x4((shz_mat4x4_t *)c->vp);
#else
    hero_m4_mul_(c->vp, p, v);
#endif
}

/* The sun's shadow: a world point slid along the unit direction toward the sun (sun[1] > 0) down to y 0, then into
   the target's pixels: x -SPAN .. SPAN to 0 .. size, z the same to the rows. Z = 1, W = 1. */
static inline void hero_shadow_matrix(const float sun[3], float size, float m[16]) {
    const float k = size / (2.0f * HERO_SPAN), iy = sh4der_recip_pos_(sun[1]);

    memset(m, 0, 16u * sizeof m[0]);
    m[0] = k; m[4] = -k * sun[0] * iy; m[12] = k * HERO_SPAN;
    m[5] = -k * sun[2] * iy; m[9] = k; m[13] = k * HERO_SPAN;
    m[14] = 1.0f; m[15] = 1.0f;
}

static inline void hero_ground_uv(const float p[3], float uv[2]) {
    uv[0] = (p[0] + HERO_SPAN) * (1.0f / (2.0f * HERO_SPAN));
    uv[1] = (p[2] + HERO_SPAN) * (1.0f / (2.0f * HERO_SPAN));
}

/* A world point on screen: x, y pixels, z = 1 / W; 0 if it is nearer than HERO_NEAR (or behind). */
static inline int hero_view_pt(const float vp[16], const float p[3], float out[3]) {
    float o[4];

    hero_xf_(vp, p, o);
    if (o[3] < HERO_NEAR) return 0;
    {
        const float iw = sh4der_recip_pos_(o[3]);

        out[0] = o[0] * iw; out[1] = o[1] * iw; out[2] = iw;
    }
    return 1;
}

/* Twice a screen triangle's signed area: under 0 when it faces the camera (counter-clockwise from its front, y down). */
static inline float hero_area2(const float a[3], const float b[3], const float c[3]) {
    return (b[0] - a[0]) * (c[1] - a[1]) - (c[0] - a[0]) * (b[1] - a[1]);
}

/* ---- the clay set: vertices, light, strips (one strip a row of cells) ---------------------------------------------- */

typedef struct hero_set {
    unsigned vn, rows;                           /* rows: strips; the first HERO_CELLS are the ground */
    float p[HERO_SET_V][3], n[HERO_SET_V][3];
    uint16_t row_v0[2u * HERO_CELLS + HERO_CELLS], row_nu[2u * HERO_CELLS + HERO_CELLS], row_wall[2u * HERO_CELLS + HERO_CELLS];
} hero_set_t;

static inline void hero_set_grid_(hero_set_t *s, const float o[3], const float du[3], const float dv[3], unsigned nu, unsigned nv, const float n[3], unsigned wall) {
    const unsigned v0 = s->vn;
    unsigned i, j, k;

    for (j = 0; j <= nv; j++)
        for (i = 0; i <= nu; i++, s->vn++)
            for (k = 0; k < 3u; k++) {
                s->p[s->vn][k] = o[k] + du[k] * (float)i / (float)nu + dv[k] * (float)j / (float)nv;
                s->n[s->vn][k] = n[k];
            }
    for (j = 0; j < nv; j++, s->rows++) {
        s->row_v0[s->rows] = (uint16_t)(v0 + j * (nu + 1u));
        s->row_nu[s->rows] = (uint16_t)nu;
        s->row_wall[s->rows] = (uint16_t)wall;
    }
}

/* Ground, then the wall at x = -SPAN (wall 1), then the wall at z = -SPAN (wall 2). A row's strip: for each column i,
   vertex (i, j + 1) then (i, j): counter-clockwise about n when du x dv points along n. */
static inline void hero_set_build(hero_set_t *s) {
    const float S = HERO_SPAN, H = HERO_WALL_H;
    const float go[3] = { -S, 0, S }, gu[3] = { 2 * S, 0, 0 }, gv[3] = { 0, 0, -2 * S }, gn[3] = { 0, 1, 0 };
    const float ao[3] = { -S, 0, -S }, au[3] = { 0, 0, 2 * S }, av[3] = { 0, H, 0 }, an[3] = { 1, 0, 0 };
    const float bo[3] = { -S, 0, -S }, bu[3] = { 2 * S, 0, 0 }, bv[3] = { 0, H, 0 }, bn[3] = { 0, 0, 1 };

    memset(s, 0, sizeof *s);
    hero_set_grid_(s, go, gu, gv, HERO_CELLS, HERO_CELLS, gn, 0u);
    hero_set_grid_(s, ao, au, av, HERO_CELLS, HERO_CELLS / 2u, an, 1u);
    hero_set_grid_(s, bo, bu, bv, HERO_CELLS, HERO_CELLS / 2u, bn, 2u);
}

/* Whether a wall's inside is toward the eye (else it would hide the hero: it is not drawn). */
static inline int hero_wall_faces(unsigned wall, const float eye[3]) {
    return wall == 1u ? eye[0] > -HERO_SPAN : wall == 2u ? eye[2] > -HERO_SPAN : 1;
}

#endif
