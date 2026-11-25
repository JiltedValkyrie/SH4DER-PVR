/* SH4DER-PVR: the perspective shadow map demo's scene and frame logic

   demo/psm/psm_scene.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   Everything of the Dreamcast A-vs-B program (psm_dc.c) that is not a KallistiOS call, so the host can test it
   (test/psm_scene_test.c): the scene (a graded ground grid, ten poles, two cows), the pad's meaning, the map of each
   frame, the screen placement of the ground and the models (their cells and triangles with area, their colours,
   cached on the camera and the positions), the row-form transforms (XMTRX on the SH-4, plain C elsewhere), route B's
   fill and compare (the SH-4 draws a depth map: sh4der_psm_raster.h), route A's lookup coordinates
   (the PVR draws the map: sh4der_psm_pvr.h), the texel-over-pixel line and the readout's text.

   World: metres, y up, the camera at (0, 1.7, 0) looking along -z at yaw 0. The sun: azimuth 0 is straight ahead
   (toward the camera), 90 on the right, 180 behind the camera.

   The ground is a polar grid round the point under the eye, fixed in the world: rows are rings (0.25 m deep near the
   camera, growing to 2 m at 70 m and on), columns are spokes a fixed angle apart, and a frame draws the spokes the
   view covers. A vertex does not move when the camera turns, so a shadow edge carried by vertices (route B) does not
   crawl. Every vertex of the window is in front of the camera, so the ground needs no near clip.

   The cow is Spot (CC0) cut to about 400 triangles by tools/bake_cow.py into the build folder (PSM_HAVE_COW and
   psm_cow.h on the include path), or a stand-in made of boxes. One mesh serves the screen and the map, so the depth
   the SH-4 stores and the surface it compares are the same surface.
*/

#ifndef PSM_SCENE_H
#define PSM_SCENE_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include "sh4der_psm.h"
#include "sh4der_psm_raster.h"
#include "sh4der_psm_pvr.h"
#include "sh4der_guard.h"

#ifdef PSM_HAVE_COW
#include "psm_cow.h"
#endif

/* ---- tuning ---------------------------------------------------------------- */
#define PSM_EYE_H        1.7f
#define PSM_PITCH        0.12217f     /* 7 degrees down */
#define PSM_TAN_HALF     0.57735f     /* 60 degrees up and down */
#define PSM_ASPECT       (4.0f / 3.0f)
#define PSM_NEAR         0.3f
#define PSM_FAR          70.0f
#define PSM_FIT_NEAR     2.0f         /* the fit's points start here: the nearest ground in view is at about 2.3 m */
#define PSM_SLAB_H       0.5f         /* the receivers' slab: ground and a little over it */
#define PSM_ROW_R0       1.5f         /* the first ring */
#define PSM_ROW_END      92.0f        /* the last ring is at or past this: the far plane's corners are 89 m out on the ground */
#define PSM_ROW_NEAR     0.25f        /* a ring's depth near the camera */
#define PSM_ROW_FAR      2.0f         /* and at 70 m */
#define PSM_ROWS_MAX     104u         /* rings */
#define PSM_COLS         50u          /* cells across the window: 51 spokes */
#define PSM_COL_STEP     0.0295f      /* radians between spokes: the screen's lower corners are 0.696 rad off the heading on the ground, the window reaches 0.72 */
#define PSM_GV_MAX       (PSM_ROWS_MAX * (PSM_COLS + 1u))
#define PSM_MV_MAX       1024u        /* model vertices: 240 for the poles, two cows */
#define PSM_MT_MAX       1024u        /* model triangles: 120 for the poles, two cows of 400 */
#define PSM_AMBIENT      0.42f        /* the share of the light that is not the sun's */
#define PSM_NORMAL_PUSH  0.06f        /* a model vertex is compared this far out along its normal, metres */
#define PSM_TURN         1.2f         /* radians a second at full stick */
#define PSM_MAP_SIZES    3u
#define PSM_SCR_W        640.0f       /* the screen: the ground and the models are placed on it */
#define PSM_SCR_H        480.0f

static const unsigned psm_map_sizes[PSM_MAP_SIZES] = { 64u, 128u, 256u };
static const float psm_albedo[3][3] = { { 0.78f, 0.74f, 0.66f }, { 0.86f, 0.62f, 0.46f }, { 0.56f, 0.62f, 0.82f } };   /* pole, cow, cow */

/* the pad, as bits the program fills from the controller */
enum {
    PSM_BTN_A = 1, PSM_BTN_X = 2, PSM_BTN_Y = 4, PSM_BTN_B = 8, PSM_BTN_START = 16,
    PSM_BTN_UP = 32, PSM_BTN_DOWN = 64, PSM_BTN_LEFT = 128, PSM_BTN_RIGHT = 256, PSM_BTN_L = 512, PSM_BTN_R = 1024
};

enum { PSM_ROUTE_A = 0, PSM_ROUTE_B = 1 };

/* ---- scene ----------------------------------------------------------------- */

typedef struct psm_scene {
    float row_r[PSM_ROWS_MAX];
    unsigned rows;                      /* rings */
    float mv[PSM_MV_MAX][3], mn[PSM_MV_MAX][3];
    uint8_t mkind[PSM_MV_MAX];          /* 0 a pole, 1 and 2 the cows */
    uint16_t mt[PSM_MT_MAX][3];
    unsigned mvn, mtn;
} psm_scene_t;

/* A box of half sizes h round c, turned by yaw about y, then scaled by s and moved to at: 24 vertices (a normal a face), 12 triangles. */
static inline void psm_scene_box_(psm_scene_t *s, const float c[3], const float h[3], float yaw, float scale, const float at[3], unsigned kind) {
    static const signed char face[6][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, -1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    float sn, cs;
    unsigned f, i;

    if (s->mvn + 24u > PSM_MV_MAX || s->mtn + 12u > PSM_MT_MAX) return;
    sh4der_sincos_(yaw, &sn, &cs);
    for (f = 0; f < 6u; f++) {
        const int ax = face[f][0] ? 0 : face[f][1] ? 1 : 2, u = (ax + 1) % 3, v = (ax + 2) % 3;
        const float sg = (float)face[f][ax];
        const unsigned base = s->mvn;

        for (i = 0; i < 4u; i++) {
            float p[3], n[3] = { 0.0f, 0.0f, 0.0f };

            p[ax] = c[ax] + sg * h[ax];
            p[u] = c[u] + ((i == 1u || i == 2u) ? h[u] : -h[u]);
            p[v] = c[v] + (i >= 2u ? h[v] : -h[v]);
            n[ax] = sg;
            s->mv[s->mvn][0] = at[0] + (p[0] * cs + p[2] * sn) * scale;
            s->mv[s->mvn][1] = at[1] + p[1] * scale;
            s->mv[s->mvn][2] = at[2] + (-p[0] * sn + p[2] * cs) * scale;
            s->mn[s->mvn][0] = n[0] * cs + n[2] * sn;
            s->mn[s->mvn][1] = n[1];
            s->mn[s->mvn][2] = -n[0] * sn + n[2] * cs;
            s->mkind[s->mvn] = (uint8_t)kind;
            s->mvn++;
        }
        s->mt[s->mtn][0] = (uint16_t)base; s->mt[s->mtn][1] = (uint16_t)(base + 1u); s->mt[s->mtn][2] = (uint16_t)(base + 2u); s->mtn++;
        s->mt[s->mtn][0] = (uint16_t)base; s->mt[s->mtn][1] = (uint16_t)(base + 2u); s->mt[s->mtn][2] = (uint16_t)(base + 3u); s->mtn++;
    }
}

/* A cow `size` metres long at (x, 0, z), turned by yaw. */
static inline void psm_scene_cow_(psm_scene_t *s, float x, float z, float size, float yaw, unsigned kind) {
    const float at[3] = { x, 0.0f, z };
#ifdef PSM_HAVE_COW
    float sn, cs;
    unsigned i, base = s->mvn;

    if (s->mvn + PSM_COW_V > PSM_MV_MAX || s->mtn + PSM_COW_T > PSM_MT_MAX) return;
    sh4der_sincos_(yaw, &sn, &cs);
    for (i = 0; i < PSM_COW_V; i++) {
        const float *p = psm_cow_v[i], *n = psm_cow_n[i];

        s->mv[s->mvn][0] = at[0] + (p[0] * cs + p[2] * sn) * size;
        s->mv[s->mvn][1] = p[1] * size;
        s->mv[s->mvn][2] = at[2] + (-p[0] * sn + p[2] * cs) * size;
        s->mn[s->mvn][0] = n[0] * cs + n[2] * sn;
        s->mn[s->mvn][1] = n[1];
        s->mn[s->mvn][2] = -n[0] * sn + n[2] * cs;
        s->mkind[s->mvn] = (uint8_t)kind;
        s->mvn++;
    }
    for (i = 0; i < PSM_COW_T; i++) {
        s->mt[s->mtn][0] = (uint16_t)(base + psm_cow_t[i][0]);
        s->mt[s->mtn][1] = (uint16_t)(base + psm_cow_t[i][1]);
        s->mt[s->mtn][2] = (uint16_t)(base + psm_cow_t[i][2]);
        s->mtn++;
    }
#else
    /* the stand-in: a body, a head and four legs, in a box 1 long (z), 0.62 high */
    static const float part[6][6] = {
        { 0.0f, 0.40f, 0.0f, 0.16f, 0.14f, 0.34f }, { 0.0f, 0.50f, 0.40f, 0.10f, 0.10f, 0.10f },
        { -0.11f, 0.13f, 0.24f, 0.04f, 0.13f, 0.04f }, { 0.11f, 0.13f, 0.24f, 0.04f, 0.13f, 0.04f },
        { -0.11f, 0.13f, -0.24f, 0.04f, 0.13f, 0.04f }, { 0.11f, 0.13f, -0.24f, 0.04f, 0.13f, 0.04f }
    };
    unsigned i;

    for (i = 0; i < 6u; i++) psm_scene_box_(s, &part[i][0], &part[i][3], yaw, size, at, kind);
#endif
}

/* The rings, the poles and the cows. Once, at load. */
static inline void psm_scene_init(psm_scene_t *s) {
    const float grow = (PSM_ROW_FAR - PSM_ROW_NEAR) / (PSM_FAR - PSM_FIT_NEAR);
    float r = PSM_ROW_R0;
    unsigned i;

    memset(s, 0, sizeof *s);
    while (s->rows < PSM_ROWS_MAX) {
        float step = PSM_ROW_NEAR + (r - PSM_FIT_NEAR) * grow;

        s->row_r[s->rows++] = r;
        if (r >= PSM_ROW_END) break;
        if (step < PSM_ROW_NEAR) step = PSM_ROW_NEAR;
        r += step;
    }
    for (i = 0; i < 10u; i++) {
        const float c[3] = { 0.0f, 2.5f, 0.0f }, h[3] = { 0.15f, 2.5f, 0.15f };
        const float at[3] = { (i & 1u) ? 2.6f : -2.6f, 0.0f, -4.0f - 4.4f * (float)i };

        psm_scene_box_(s, c, h, 0.0f, 1.0f, at, 0u);
    }
    psm_scene_cow_(s, 0.3f, -7.0f, 2.2f, 0.5f, 1u);
    psm_scene_cow_(s, -1.6f, -15.0f, 2.6f, -0.9f, 2u);
}

/* ---- state and pad ----------------------------------------------------------- */

typedef struct psm_state {
    int route, warp, inset, readout, pcf, split;
    unsigned size_i, preset;
    float az, el, warp_near, bias_a, bias_b, yaw;
    unsigned last;       /* the pad a frame ago */
    int start_used;      /* START was a shift this press, not a tap */
} psm_state_t;

/* The boot state: route A, the uniform map, 256, the host demo's sun. */
static inline void psm_state_default(psm_state_t *st) {
    memset(st, 0, sizeof *st);
    st->route = PSM_ROUTE_A;
    st->warp = 0;
    st->readout = 1;
    st->pcf = 1;
    st->size_i = PSM_MAP_SIZES - 1u;
    st->az = 120.0f; st->el = 35.0f;
    st->warp_near = 4.0f;
    st->bias_a = 0.02f; st->bias_b = 0.0f;
}

static inline float psm_step_(float x, float d, float lo, float hi) {
    x += d;
    return x < lo ? lo : x > hi ? hi : x;
}

/* One frame of the pad: `held` the PSM_BTN_ bits down now, `stick` -1 .. 1, dt seconds. */
static inline void psm_input(psm_state_t *st, unsigned held, float stick, float dt) {
    static const float preset[3][2] = { { 120.0f, 35.0f }, { 0.0f, 20.0f }, { 180.0f, 20.0f } };
    const unsigned down = held & ~st->last, up = st->last & ~held;

    if (down & PSM_BTN_START) st->start_used = 0;
    if (held & PSM_BTN_START) {
        const unsigned combo = down & ~(unsigned)PSM_BTN_START;

        if (combo) st->start_used = 1;
        if (down & PSM_BTN_A) st->size_i = (st->size_i + 1u) % PSM_MAP_SIZES;
        if (down & PSM_BTN_X) st->pcf = !st->pcf;
        if (down & PSM_BTN_Y) {
            st->preset = (st->preset + 1u) % 3u;
            st->az = preset[st->preset][0]; st->el = preset[st->preset][1];
        }
        if (down & PSM_BTN_UP) st->bias_a = psm_step_(st->bias_a, 0.005f, 0.0f, 0.2f);
        if (down & PSM_BTN_DOWN) st->bias_a = psm_step_(st->bias_a, -0.005f, 0.0f, 0.2f);
        if (down & PSM_BTN_R) st->bias_b = psm_step_(st->bias_b, 0.5f, 0.0f, 4.0f);
        if (down & PSM_BTN_L) st->bias_b = psm_step_(st->bias_b, -0.5f, 0.0f, 4.0f);
    } else {
        if ((up & PSM_BTN_START) && !st->start_used) st->readout = !st->readout;
        if (down & PSM_BTN_A) st->route = !st->route;
        if (down & PSM_BTN_X) st->warp = !st->warp;
        if (down & PSM_BTN_Y) st->inset = !st->inset;
        if (down & PSM_BTN_RIGHT) st->az += 5.0f;
        if (down & PSM_BTN_LEFT) st->az -= 5.0f;
        if (st->az >= 360.0f) st->az -= 360.0f;
        if (st->az < 0.0f) st->az += 360.0f;
        if (down & PSM_BTN_UP) st->el = psm_step_(st->el, 5.0f, 5.0f, 85.0f);
        if (down & PSM_BTN_DOWN) st->el = psm_step_(st->el, -5.0f, 5.0f, 85.0f);
        if (st->warp) {
            if (down & PSM_BTN_R) st->warp_near = psm_step_(st->warp_near, 0.5f, 1.0f, 12.0f);
            if (down & PSM_BTN_L) st->warp_near = psm_step_(st->warp_near, -0.5f, 1.0f, 12.0f);
        }
    }
    st->split = (held & PSM_BTN_B) != 0u;
    if (stick > 0.1f || stick < -0.1f) st->yaw += stick * PSM_TURN * dt;
    if (st->yaw > 6.2831853f) st->yaw -= 6.2831853f;
    if (st->yaw < 0.0f) st->yaw += 6.2831853f;
    st->last = held;
}

/* ---- frame ------------------------------------------------------------------- */

/* A row-form matrix applied to many points (p' = (x, y, z, 1) x M). On the SH-4 the matrix goes into XMTRX once, its rows
   being the matrix's columns, so FTRV gives each column's dot with the point: that is p x M, and one point is one FTRV.
   Between a load and its applies nothing else may use XMTRX. Plain C otherwise (SH4DER_CORE_NO_SH4ZAM). */
typedef struct psm_xf {
    float m[16];
} psm_xf_t;

static inline void psm_xf_load(psm_xf_t *x, const float m[16]) {
    unsigned i;

    for (i = 0; i < 16u; i++) x->m[i] = m[i];
#ifndef SH4DER_CORE_NO_SH4ZAM
    {
        const shz_vec4_t r0 = shz_vec4_init(m[0], m[4], m[8], m[12]);
        const shz_vec4_t r1 = shz_vec4_init(m[1], m[5], m[9], m[13]);
        const shz_vec4_t r2 = shz_vec4_init(m[2], m[6], m[10], m[14]);
        const shz_vec4_t r3 = shz_vec4_init(m[3], m[7], m[11], m[15]);

        shz_xmtrx_load_rows_4x4(&r0, &r1, &r2, &r3);
    }
#endif
}

/* h = (p, 1) x M */
static inline void psm_xf_apply(const psm_xf_t *x, const float p[3], float h[4]) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    const shz_vec4_t v = shz_xmtrx_transform_vec4(shz_vec4_init(p[0], p[1], p[2], 1.0f));

    (void)x;
    h[0] = v.x; h[1] = v.y; h[2] = v.z; h[3] = v.w;
#else
    sh4der_psm_xform(x->m, p, 1.0f, h);
#endif
}

typedef struct psm_frame {
    sh4der_psm_cam_t cam;
    float sun[3];
    sh4der_psm_t map;
    int built;                  /* 0: no map this frame, everything is lit */
    int head_on;                /* the warp was asked for and had no fit: the uniform map stands in */
    unsigned size;
    float ratio[4];             /* ground a texel covers over ground a pixel covers at 4, 10, 25, 60 m; under 0 for none */
    float shadow_grey;          /* what a shadowed ground texel is multiplied by on route A: the ambient share */
    unsigned grows, gcols;      /* rings and spokes this frame */
    unsigned gvn;
    float rows[16];             /* the camera's world-to-clip rows at the render's near plane (zero for a bad camera) */
    int gpos_ok, gpos_k0;       /* the ground positions exist, for the spoke window that starts at gpos_k0 */
    unsigned gpos_rows;         /* rings the positions were made for */
    unsigned gpos_gen;          /* counts the position recomputes: the placement is keyed on it */
    float gpos[PSM_GV_MAX][3];
    uint8_t glit[PSM_GV_MAX];   /* route B: 255 lit .. 0 shadowed */
    float guv[PSM_GV_MAX][2];   /* route A: the map target's coordinates */
    uint8_t gok[PSM_GV_MAX];    /* route A: the vertex has coordinates on the fit's side of the light's singularity */
    uint8_t mlit[PSM_MV_MAX];   /* the sun's share at a model vertex, 0 .. 255: N.L, times the shadow on route B */
    float mh[PSM_MV_MAX][4];    /* route B's scratch: the casters in the map's homogeneous form (X, Y, K, W) */
    float ma[PSM_MV_MAX][4];    /* route A: the casters in the map's homogeneous form (X, Y, ., W), once a frame */
    /* the placement (psm_place): on the screen, the cells and the model triangles with area, and the colours */
    int place_ok;
    unsigned place_gen;         /* gpos_gen the placement was made for */
    float place_rows[16];       /* and the camera rows */
    float gclip[PSM_GV_MAX][3]; /* X, Y, W */
    float gscr[PSM_GV_MAX][3];  /* x, y, 1 / W */
    uint32_t gcol[PSM_GV_MAX];
    uint32_t glut[256];         /* the ground's colour by its shadow level */
    uint8_t cell[PSM_ROWS_MAX * PSM_COLS];   /* 1: both triangles of the cell have area and every corner is in front */
    float mclip[PSM_MV_MAX][3];
    float mscr[PSM_MV_MAX][3];
    uint32_t mcol[PSM_MV_MAX];
    uint8_t mtri[PSM_MT_MAX];   /* 1: the triangle is in front and has screen area */
    sh4der_psm_raster_t raster; /* route B's map */
    unsigned tris, drawn;       /* route B: caster triangles given and drawn */
} psm_frame_t;

static const float psm_probe_m[4] = { 4.0f, 10.0f, 25.0f, 60.0f };

/* The camera, the sun, this frame's ground vertices, the map (the warp, or the uniform one) and the texel-over-pixel line. */
static inline void psm_frame_setup(psm_frame_t *f, const psm_scene_t *s, const psm_state_t *st) {
    const float up[3] = { 0.0f, 1.0f, 0.0f }, gx[3] = { 1.0f, 0.0f, 0.0f }, gz[3] = { 0.0f, 0.0f, 1.0f };
    float sy, cy, sp, cp, sa, ca, se, ce, corners[8][3], pts[32][3];
    unsigned n, r, c;
    int k0;

    sh4der_sincos_(st->yaw, &sy, &cy);
    sh4der_sincos_(PSM_PITCH, &sp, &cp);
    memset(&f->cam, 0, sizeof f->cam);
    f->cam.eye[1] = PSM_EYE_H;
    f->cam.fwd[0] = sy * cp; f->cam.fwd[1] = -sp; f->cam.fwd[2] = -cy * cp;
    f->cam.up[0] = sy * sp; f->cam.up[1] = cp; f->cam.up[2] = -cy * sp;
    f->cam.tan_half = PSM_TAN_HALF; f->cam.aspect = PSM_ASPECT;
    f->cam.znear = PSM_NEAR; f->cam.zfar = PSM_FAR;
    if (!sh4der_psm_cam_rows(&f->cam, PSM_NEAR, f->rows)) memset(f->rows, 0, sizeof f->rows);

    sh4der_sincos_(st->az * 0.0174533f, &sa, &ca);
    sh4der_sincos_(st->el * 0.0174533f, &se, &ce);
    f->sun[0] = sa * ce; f->sun[1] = se; f->sun[2] = -ca * ce;
    f->shadow_grey = sh4der_div_(PSM_AMBIENT, PSM_AMBIENT + (1.0f - PSM_AMBIENT) * f->sun[1]);

    /* the spokes the view covers: the window starts on a whole spoke, so a vertex is the same point every frame. The
       positions change only when the window moves by a spoke; a still camera keeps them (and the placement on them). */
    k0 = (int)(st->yaw / PSM_COL_STEP + 0.5f) - (int)(PSM_COLS / 2u);
    f->grows = s->rows; f->gcols = PSM_COLS + 1u;
    f->gvn = f->grows * f->gcols;
    if (!f->gpos_ok || f->gpos_k0 != k0 || f->gpos_rows != f->grows) {
        for (c = 0; c < f->gcols; c++) {
            float sn, cs;

            sh4der_sincos_((float)(k0 + (int)c) * PSM_COL_STEP, &sn, &cs);
            for (r = 0; r < f->grows; r++) {
                float *p = f->gpos[r * f->gcols + c];

                p[0] = s->row_r[r] * sn; p[1] = 0.0f; p[2] = -s->row_r[r] * cs;
            }
        }
        f->gpos_ok = 1; f->gpos_k0 = k0; f->gpos_rows = f->grows; f->gpos_gen++;
    }

    f->size = psm_map_sizes[st->size_i % PSM_MAP_SIZES];
    f->built = 0; f->head_on = 0;
    n = 0;
    if (sh4der_psm_frustum(&f->cam, PSM_FIT_NEAR, PSM_FAR, corners))
        n = sh4der_psm_slab_pts((const float (*)[3])corners, up, 0.0f, PSM_SLAB_H, pts);
    if (n) {
        if (st->warp) {
            f->built = sh4der_psm_build_warp(&f->map, &f->cam, st->warp_near, f->sun, &pts[0][0], n);
            if (!f->built) f->head_on = 1;
        }
        if (!f->built) f->built = sh4der_psm_build_uniform(&f->map, f->sun, &pts[0][0], n);
    }
    for (c = 0; c < 4u; c++) {
        const float p[3] = { sy * psm_probe_m[c], 0.0f, -cy * psm_probe_m[c] };
        float t, px;

        f->ratio[c] = -1.0f;
        if (!f->built) continue;
        t = sh4der_psm_texel_m(&f->map, f->size, p, gx, gz, 0.05f);
        px = sh4der_psm_pixel_m(&f->cam, 480u, p, up);
        if (t < 1e29f && px > 0.0f) f->ratio[c] = sh4der_div_(t, px);
    }
    f->tris = f->drawn = 0u;
}

/* Everything lit: a frame with no map, and what route A's models start from. */
static inline void psm_frame_unshadowed(psm_frame_t *f, const psm_scene_t *s) {
    unsigned i;

    memset(f->glit, 255, f->gvn);
    for (i = 0; i < s->mvn; i++) {
        const float ndl = sh4der_sat_(sh4der_dot3_(s->mn[i], f->sun));

        f->mlit[i] = (uint8_t)(ndl * 255.0f + 0.5f);
    }
}

/* Route B, step 1: the casters' depth range and a cleared map. `depth` holds size x size. 0: no map (draw unshadowed). */
static inline int psm_frame_b_begin(psm_frame_t *f, const psm_scene_t *s, uint16_t *depth) {
    float lo, hi;

    if (!f->built || !sh4der_psm_raster_key_range(f->map.light, &s->mv[0][0], 3u * sizeof(float), s->mvn, &lo, &hi)) return 0;
    return sh4der_psm_raster_begin(&f->raster, &f->map, depth, f->size, lo - 0.25f, hi + 0.25f);
}

/* Route B, step 2: the casters into the map. Each vertex is transformed once. */
static inline void psm_frame_b_casters(psm_frame_t *f, const psm_scene_t *s) {
    psm_xf_t xf;
    unsigned i;

    psm_xf_load(&xf, f->raster.m);                   /* the map's rows with the depth column: (X, Y, K, W) */
    for (i = 0; i < s->mvn; i++) psm_xf_apply(&xf, s->mv[i], f->mh[i]);
    for (i = 0; i < s->mtn; i++)
        sh4der_psm_raster_tri_h(&f->raster, f->mh[s->mt[i][0]], f->mh[s->mt[i][1]], f->mh[s->mt[i][2]]);
    f->tris = f->raster.tris; f->drawn = f->raster.drawn;
}

/* Route B, step 3: the compare. The ground through the plane compare (its axes are x and z); a model vertex that
   faces the sun at a point a little out along its normal, one that faces away gets no sun and no lookup. */
static inline void psm_frame_b_receivers(psm_frame_t *f, const psm_scene_t *s, const psm_state_t *st) {
    const float gx[3] = { 1.0f, 0.0f, 0.0f }, gz[3] = { 0.0f, 0.0f, 1.0f };
    unsigned i;

    sh4der_psm_raster_lit_n(&f->raster, &f->gpos[0][0], 3u * sizeof(float), f->gvn, gx, gz, st->bias_a, st->bias_b, st->pcf, f->glit, 1u);
    for (i = 0; i < s->mvn; i++) {
        const float ndl = sh4der_dot3_(s->mn[i], f->sun);
        float p[3];

        if (!(ndl > 0.0f)) { f->mlit[i] = 0u; continue; }
        p[0] = s->mv[i][0] + s->mn[i][0] * PSM_NORMAL_PUSH;
        p[1] = s->mv[i][1] + s->mn[i][1] * PSM_NORMAL_PUSH;
        p[2] = s->mv[i][2] + s->mn[i][2] * PSM_NORMAL_PUSH;
        f->mlit[i] = (uint8_t)(sh4der_min1_(ndl) * sh4der_psm_raster_lit(&f->raster, p, st->bias_a, st->pcf) * 255.0f + 0.5f);
    }
}

/* Route A: the ground's coordinates in the map target. A vertex on the far side of the light's singularity (or at it)
   is marked: the PVR cannot interpolate across it, so the layer leaves out the cells that touch it. */
static inline void psm_frame_a_ground(psm_frame_t *f) {
    psm_xf_t xf;
    unsigned i;

    psm_xf_load(&xf, f->map.m);
    for (i = 0; i < f->gvn; i++) {
        float h[4], iw;

        f->gok[i] = 0u; f->guv[i][0] = f->guv[i][1] = 0.0f;
        if (!f->built) continue;
        psm_xf_apply(&xf, f->gpos[i], h);                       /* the map's homogeneous point; sh4der_psm_map's rule */
        if (!sh4der_finite_(h[3]) || !(h[3] > SH4DER_PSM_TRI_EPS)) continue;
        iw = sh4der_div_(1.0f, h[3]);
        f->guv[i][0] = 0.5f * (h[0] * iw) + 0.5f;
        f->guv[i][1] = 0.5f * (h[1] * iw) + 0.5f;
        f->gok[i] = 1u;
    }
}

/* Route A: the casters in the map's homogeneous form, once a frame (psm_pvr_tris_h draws them into the target). */
static inline void psm_frame_a_casters(psm_frame_t *f, const psm_scene_t *s) {
    psm_xf_t xf;
    unsigned i;

    if (!f->built) return;
    psm_xf_load(&xf, f->map.m);
    for (i = 0; i < s->mvn; i++) psm_xf_apply(&xf, s->mv[i], f->ma[i]);
}

/* The colour scale of a surface: the ambient share plus the sun's, `direct` 0 .. 1. */
static inline float psm_light(float direct) { return PSM_AMBIENT + (1.0f - PSM_AMBIENT) * direct; }

static inline uint32_t psm_pack_rgb(float r, float g, float b) {
    return 0xff000000u | ((uint32_t)(sh4der_sat_(r) * 255.0f + 0.5f) << 16) | ((uint32_t)(sh4der_sat_(g) * 255.0f + 0.5f) << 8)
         | (uint32_t)(sh4der_sat_(b) * 255.0f + 0.5f);
}

/* 1 when the cell (ring r, spoke c) can be sent: psm_place found both its triangles with area and every corner in front;
   for the layer, also every corner has map coordinates. */
static inline int psm_cell_ok(const psm_frame_t *f, unsigned r, unsigned c, int layer) {
    if (!f->cell[r * PSM_COLS + c]) return 0;
    if (layer) {
        const unsigned a = r * f->gcols + c, d = a + f->gcols;

        return f->gok[a] && f->gok[a + 1u] && f->gok[d] && f->gok[d + 1u];
    }
    return 1;
}

/* The spoke that ends the run of sendable cells which starts at (r, c): cells c .. end - 1 are sent, end is not (or is the last
   spoke). The strip is the same ring as the cells: outer and inner vertex per spoke. */
static inline unsigned psm_run_end(const psm_frame_t *f, unsigned r, unsigned c, int layer) {
    unsigned e = c + 1u;

    while (e + 1u < f->gcols && psm_cell_ok(f, r, e, layer)) e++;
    return e;
}

/* The triangles of one caster in the target's pixels, from its homogeneous vertices (psm_frame_a_casters): the same clip as
   sh4der_psm_pvr_tris, without its transforms. */
static inline unsigned psm_pvr_tris_h(const float h0[4], const float h1[4], const float h2[4], unsigned size, sh4der_psm_pvr_vtx_t out[][3]) {
    const int pos = h0[3] > 0.0f || h1[3] > 0.0f || h2[3] > 0.0f;
    const int neg = h0[3] < 0.0f || h1[3] < 0.0f || h2[3] < 0.0f;
    unsigned n = 0;

    if (pos) n += sh4der_psm_pvr_part_(h0, h1, h2, 1.0f, size, out + n);
    if (neg) n += sh4der_psm_pvr_part_(h0, h1, h2, -1.0f, size, out + n);
    return n;
}

/* A point's place: clip (X, Y, W) and screen (x, y, 1 / W). 0 when it is not in front of the near plane (screen 0). */
static inline int psm_place_pt_(const psm_xf_t *xf, const float p[3], float clip[3], float scr[3]) {
    float h[4], iw;

    psm_xf_apply(xf, p, h);
    clip[0] = h[0]; clip[1] = h[1]; clip[2] = h[3];
    if (!(h[3] > PSM_NEAR)) { scr[0] = scr[1] = scr[2] = 0.0f; return 0; }
    iw = sh4der_recip_pos_(h[3]);
    scr[0] = (h[0] * iw * 0.5f + 0.5f) * PSM_SCR_W;
    scr[1] = (0.5f - h[1] * iw * 0.5f) * PSM_SCR_H;
    scr[2] = iw;
    return 1;
}

/* This frame's place on the screen and its colours. The ground and the models go through the camera's rows once each: clip
   and screen positions, the cells whose two strip triangles have area, the models' triangles with area. That depends on the
   camera and the ground positions only, so a still camera keeps it. The colours follow the sun and the shadows, so they are
   made every frame: the ground's from a 256 level table. */
static inline void psm_place(psm_frame_t *f, const psm_scene_t *s) {
    psm_xf_t xf;
    unsigned i, r, c;

    if (!f->place_ok || f->place_gen != f->gpos_gen || memcmp(f->place_rows, f->rows, sizeof f->rows) != 0) {
        psm_xf_load(&xf, f->rows);
        for (i = 0; i < f->gvn; i++) psm_place_pt_(&xf, f->gpos[i], f->gclip[i], f->gscr[i]);
        for (i = 0; i < s->mvn; i++) psm_place_pt_(&xf, s->mv[i], f->mclip[i], f->mscr[i]);
        /* a strip sends, for the cell (ring r, spoke c), the triangles (d, a, e) and (a, e, b): a = (r, c), b = (r, c + 1), d = (r + 1, c), e = (r + 1, c + 1) */
        for (r = 0; r + 1u < f->grows; r++)
            for (c = 0; c + 1u < f->gcols; c++) {
                const unsigned a = r * f->gcols + c, b = a + 1u, d = a + f->gcols, e = d + 1u;
                const int front = f->gclip[a][2] > PSM_NEAR && f->gclip[b][2] > PSM_NEAR && f->gclip[d][2] > PSM_NEAR && f->gclip[e][2] > PSM_NEAR;

                f->cell[r * PSM_COLS + c] = (uint8_t)(front
                    && sh4der_tri_area_ok(f->gclip[d], f->gclip[a], f->gclip[e], PSM_NEAR, SH4DER_GUARD_MIN_AREA2)
                    && sh4der_tri_area_ok(f->gclip[a], f->gclip[e], f->gclip[b], PSM_NEAR, SH4DER_GUARD_MIN_AREA2));
            }
        for (i = 0; i < s->mtn; i++) {
            const unsigned a = s->mt[i][0], b = s->mt[i][1], k = s->mt[i][2];
            const int front = f->mclip[a][2] > PSM_NEAR && f->mclip[b][2] > PSM_NEAR && f->mclip[k][2] > PSM_NEAR;

            f->mtri[i] = (uint8_t)(front && sh4der_tri_area_ok(f->mclip[a], f->mclip[b], f->mclip[k], PSM_NEAR, SH4DER_GUARD_MIN_AREA2));
        }
        f->place_ok = 1; f->place_gen = f->gpos_gen; memcpy(f->place_rows, f->rows, sizeof f->rows);
    }
    for (r = 0; r < 256u; r++) {
        const float l = psm_light(f->sun[1] * (float)r * (1.0f / 255.0f));

        f->glut[r] = psm_pack_rgb(l, l, l);
    }
    for (i = 0; i < f->gvn; i++) f->gcol[i] = f->glut[f->glit[i]];
    for (i = 0; i < s->mvn; i++) {
        const float l = psm_light((float)f->mlit[i] * (1.0f / 255.0f));
        const float *alb = psm_albedo[s->mkind[i] % 3u];

        f->mcol[i] = psm_pack_rgb(alb[0] * l, alb[1] * l, alb[2] * l);
    }
}

/* ---- the readout ------------------------------------------------------------- */

#define PSM_HUD_LINES 8u
#define PSM_HUD_COLS  64u            /* characters a line (a 512 texel line of the 8 x 16 font) */

/* What the program measured. A field shows "--" until `have` says the console gave a sample: nothing is filled in from a host. */
typedef struct psm_cost {
    int have;                        /* a one-second window has closed */
    float sh4_ms, pvr_ms, fps;
    unsigned ta_v, vram_kb;
    float part_ms[4];                /* B: build, clear, casters, receivers. A: build, clip, uv, (unused) */
    unsigned map_tris, layer_v;      /* route A: triangles sent to the target, vertices of the ground layer */
} psm_cost_t;

/* x with one decimal, by integers (no float printf on the console). */
static inline void psm_f1_(char *out, size_t cap, float x) {
    const int neg = x < 0.0f;
    const unsigned t = (unsigned)((neg ? -x : x) * 10.0f + 0.5f);

    snprintf(out, cap, "%s%u.%u", neg ? "-" : "", t / 10u, t % 10u);
}

static inline const char *psm_kind_name(int kind) {
    switch (kind) {
        case SH4DER_PSM_UNIFORM: return "UNIFORM";
        case SH4DER_PSM_ORTHO: return "ORTHO";
        case SH4DER_PSM_POINT_FRONT: return "POINT_FRONT";
        case SH4DER_PSM_POINT_BEHIND: return "POINT_BEHIND";
        default: return "?";
    }
}

/* one line: written whole, then cut to the line's width */
#define PSM_LINE_(i, ...) do { snprintf(big, sizeof big, __VA_ARGS__); memcpy(ln[i], big, PSM_HUD_COLS); ln[i][PSM_HUD_COLS] = 0; } while (0)

/* The eight lines: 0 the title, 1 texel over pixel, 2 .. 5 the readout, 6 the cost label, 7 the split of the SH-4's time. */
static inline void psm_hud(const psm_state_t *st, const psm_frame_t *f, const psm_cost_t *c, char ln[PSM_HUD_LINES][PSM_HUD_COLS + 1u]) {
    const unsigned kb = (f->size * f->size * 2u + 1023u) / 1024u;
    char a[4][16], big[192];
    unsigned i;

    memset(ln, 0, PSM_HUD_LINES * (PSM_HUD_COLS + 1u));
    PSM_LINE_(0, "%s   MAP %s%s", st->route == PSM_ROUTE_A ? "ROUTE A  PVR DRAWS THE MAP" : "ROUTE B  SH-4 DEPTH",
             !f->built ? "NONE" : !st->warp ? "UNIFORM" : f->head_on ? "PSM -> UNIFORM" : "PSM", f->head_on ? "  [HEAD-ON]" : "");
    for (i = 0; i < 4u; i++) {
        if (f->ratio[i] < 0.0f) snprintf(a[i], sizeof a[i], "--");
        else psm_f1_(a[i], sizeof a[i], f->ratio[i] > 999.0f ? 999.0f : f->ratio[i]);
    }
    PSM_LINE_(1, "TEXEL/PX  4m %s  10m %s  25m %s  60m %s", a[0], a[1], a[2], a[3]);
    PSM_LINE_(2, "MAP %ux%u  %u KB %s", f->size, f->size, kb, st->route == PSM_ROUTE_A ? "VRAM (render target)" : "RAM (16-bit depth)");
    psm_f1_(a[0], sizeof a[0], st->warp_near);
    PSM_LINE_(3, "WARP near %s m  kind %s  self-shadow: %s", st->warp ? a[0] : "--", f->built ? psm_kind_name(f->map.kind) : "--",
             st->route == PSM_ROUTE_A ? "no" : "yes");
    PSM_LINE_(4, "SUN az %u el %u   yaw %u", (unsigned)(st->az + 0.5f), (unsigned)(st->el + 0.5f), (unsigned)(st->yaw * 57.29578f + 0.5f));
    psm_f1_(a[1], sizeof a[1], st->bias_b);
    if (st->route == PSM_ROUTE_B)
        PSM_LINE_(5, "BIAS a %u mm  b %s   %s", (unsigned)(st->bias_a * 1000.0f + 0.5f), a[1], st->pcf ? "PCF 2x2" : "1 TEXEL");
    else
        PSM_LINE_(5, "LOOKUP per pixel, bilinear   (no bias: no depth compare)");
    if (c && c->have) {
        psm_f1_(a[0], sizeof a[0], c->sh4_ms);
        psm_f1_(a[1], sizeof a[1], c->pvr_ms);
        PSM_LINE_(6, "SH-4 %s ms  PVR %s ms  TA %u v  VRAM %u KB  FPS %u", a[0], a[1], c->ta_v, c->vram_kb, (unsigned)(c->fps + 0.5f));
        for (i = 0; i < 4u; i++) psm_f1_(a[i], sizeof a[i], c->part_ms[i]);
        if (st->route == PSM_ROUTE_B)
            PSM_LINE_(7, "B: build %s clear %s casters %s recv %s ms  tris %u/%u", a[0], a[1], a[2], a[3], f->drawn, f->tris);
        else
            PSM_LINE_(7, "A: build %s clip %s uv %s ms  map tris %u  layer v %u", a[0], a[1], a[2], c->map_tris, c->layer_v);
    } else {
        PSM_LINE_(6, "SH-4 -- ms  PVR -- ms  TA -- v  VRAM -- KB  FPS --");
        PSM_LINE_(7, "%s", st->route == PSM_ROUTE_B ? "B: build -- clear -- casters -- recv -- ms" : "A: build -- clip -- uv -- ms");
    }
}

#undef PSM_LINE_

#endif /* PSM_SCENE_H */
