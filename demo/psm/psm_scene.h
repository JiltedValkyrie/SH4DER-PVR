/* SH4DER-PVR: the perspective shadow map demo's scene and frame logic

   demo/psm/psm_scene.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   Everything of the Dreamcast A-vs-B program (psm_dc.c) that is not a KallistiOS call, so the host can test it
   (test/psm_scene_test.c): the scene (a graded ground grid, ten poles, two cows), the pad's meaning, the map of each
   frame, the screen placement of the ground and the models (their cells and triangles with area, their colours,
   cached on the camera and the positions), the row-form transforms (XMTRX on the SH-4, plain C elsewhere), route B's
   fill and compare (the SH-4 draws a depth map: sh4der_psm_raster.h), route A's lookup coordinates
   (the PVR draws the map: sh4der_psm_pvr.h), the texel-over-pixel line and the readout's text.

   World: metres, y up, the camera starts at (0, 1.7, 0) looking along -z at yaw 0 and walks on the ground at that eye
   height. The sun: azimuth 0 is straight ahead (toward the camera), 90 on the right, 180 behind the camera.

   The ground is a polar grid round a centre near the eye, fixed in the world: rows are rings (0.25 m deep near the
   camera, growing to 2 m at 70 m and on), columns are spokes a fixed angle apart, and a frame draws the spokes the
   view covers. The centre is the eye snapped to a 0.25 m lattice, so a vertex does not move while the camera turns or
   walks inside a lattice cell, and moves by one whole lattice step when it crosses one: a shadow edge carried by
   vertices (route B) does not crawl. The window's width follows the pitch and the eye's offset from the centre.

   Walking (psm_walk_*, the pad's menu, the limits) is written so the town demo can take it whole: it knows nothing of
   the poles and cows but the circles psm_scene_init gives it.

   The cow is Spot (CC0) cut to about 400 triangles by tools/bake_cow.py into the build folder (PSM_HAVE_COW and
   psm_cow.h on the include path), or a stand-in made of boxes. In the field one mesh serves the screen and the map (its
   casters are a copy of its triangles), so the depth the SH-4 stores and the surface it compares are the same surface.

   The second scene is the town square (psm_town.h; PSM_HAVE_TOWN with tools/bake_town.py's psm_town_mesh.h on the include
   path). Its screen mesh is strips (a strip's triangle i is its vertices i, i + 1, i + 2), its casters a low mesh of their
   own in objects (a house, a prop, a mover), and an object whose box falls wholly off the map's square is not drawn into
   it (psm_frame_cull_: the map's own square, not the view, so a tower out of view still shades the square). Its movers'
   vertices are placed every frame (psm_movers_apply), the rest is cached as the field's is. Each scene carries its light,
   its colours, its fit slab by route, its ground texture, its walk area and its sky and fog table (psm_look_row_t).
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
#ifdef PSM_HAVE_TOWN
#include "psm_town_mesh.h"
#endif
#ifdef PSM_TOWN_NAME
#define PSM_TOWN_NAME_ PSM_TOWN_NAME            /* the square's own name (bake_dungeon.py writes one) */
#else
#define PSM_TOWN_NAME_ "town (the market square)"
#endif

/* ---- tuning ---------------------------------------------------------------- */
#define PSM_EYE_H        1.7f
#define PSM_PITCH        0.12217f     /* 7 degrees down: the boot pitch */
#define PSM_TAN_HALF     0.57735f     /* 60 degrees up and down */
#define PSM_ASPECT       (4.0f / 3.0f)
#define PSM_NEAR         0.3f
#define PSM_FAR          70.0f
#define PSM_FIT_NEAR     2.0f         /* the fit's points start here: the nearest ground in view is at about 2.3 m */
#define PSM_SLAB_H       0.5f         /* the receivers' slab: ground and a little over it */
#define PSM_ROW_R0       1.0f         /* the first ring: looking down the most (PSM_PITCH_DOWN), the nearest ground in view is 1.6 m out */
#define PSM_ROW_END      92.0f        /* the last ring is at or past this: the far plane's corners are 89 m out on the ground */
#define PSM_ROW_NEAR     0.25f        /* a ring's depth near the camera */
#define PSM_ROW_FAR      2.0f         /* and at 70 m */
#define PSM_ROWS_MAX     108u         /* rings */
#define PSM_COLS         64u          /* the most cells across the window (65 spokes); a frame takes what its pitch needs (psm_window_half) */
#define PSM_COL_STEP     0.0295f      /* radians between spokes: at 7 degrees down the screen's lower corners are 0.696 rad off the heading on the ground */
#define PSM_GV_MAX       (PSM_ROWS_MAX * (PSM_COLS + 1u))
#define PSM_SNAP         0.25f        /* the grid's centre is the eye snapped to this lattice (a near ring's depth) */
#define PSM_SNAP_OFF     0.1768f      /* the farthest the eye is from that centre: half the lattice's diagonal */
#define PSM_MV_MAX       12288u       /* model vertices: the field 240 for the poles and two cows; the town about 8,700 and its movers */
#define PSM_MT_MAX       8192u        /* model triangles: the field 120 for the poles and two cows of 400; the town about 5,900 */
#define PSM_CV_MAX       2048u        /* caster vertices (the field: its model vertices, copied) */
#define PSM_CT_MAX       2048u        /* caster triangles: the town's map, about 1,200 */
#define PSM_CO_MAX       64u          /* caster objects: a pole, a cow; a house, a prop, a mover */
#define PSM_PAL_MAX      64u          /* colours a scene */
#define PSM_MOVERS_MAX   16u          /* the town's eight people and four hens */
#define PSM_DYN_V_MAX    1536u        /* the movers' screen vertices */
#define PSM_DYN_CV_MAX   512u         /* the movers' caster vertices */
#define PSM_AMBIENT      0.42f        /* the share of the light that is not the sun's */
#define PSM_NORMAL_PUSH  0.06f        /* a model vertex is compared this far out along its normal, metres */
#define PSM_TURN         1.2f         /* radians a second at full stick */
#define PSM_MAP_SIZES    4u
#define PSM_MAP_STATIC   256u         /* the largest map whose buffers are static (psm_dc.c); a bigger one is allocated while chosen */
#define PSM_MAP_MAX_A    256u         /* route A's largest target (video memory): one line to raise */
#define PSM_MAP_MAX_B    512u         /* routes B and AB's largest map (RAM, 512 KB at 512): one line to raise */
#define PSM_SCR_W        640.0f       /* the screen: the ground and the models are placed on it */
#define PSM_SCR_H        480.0f
#define PSM_VTX_BUDGET   32768u       /* vertices the program's vertex buffer holds a frame (psm_dc.c: 1 MB at 32 B); the host test holds the worst frame under it */

/* walking: the torchwalk demo's controls (demo/torchwalk_ab.c: WALK_SPEED 1.2 wu of 3 m, TURN_SPEED, STICK_DEAD, PITCH_LIMIT) */
#define PSM_WALK_SPEED   3.6f         /* metres a second, a face button held */
#define PSM_LOOK_SPEED   2.2f         /* radians a second at full stick */
#define PSM_STICK_DEAD   (12.0f / 128.0f)
#define PSM_PITCH_UP     1.25f        /* looking up, as torchwalk's PITCH_LIMIT */
#define PSM_PITCH_DOWN   0.30f        /* looking down: the nearest ground in view stays past the fit's 2 m (PSM_FIT_NEAR) and the first ring */
#define PSM_TRIG_DEAD    0.1f         /* a trigger's share under which it is let go */
#define PSM_SUN_SPEED    30.0f        /* degrees of sun elevation a second at a full trigger */
#define PSM_WALK_BODY    0.5f         /* metres the eye keeps from a pole's or a cow's outline (the near plane is 0.3 m) */
#define PSM_AREA_X       15.0f        /* the walk area: x -15 .. 15, z -55 .. 8 (the poles run to z -43.6) */
#define PSM_AREA_Z0      (-55.0f)
#define PSM_AREA_Z1      8.0f
#define PSM_OBST_MAX     16u          /* circles the eye stays out of: one a pole, two a cow */
#define PSM_DIST_STEPS   22u          /* DRAW DISTANCE: 5 .. 50 m by 5, to 200 by 25, to 500 by 50; one step more is INFINITE */
#define PSM_DIST_INF     PSM_DIST_STEPS
#define PSM_DIST_FADE    0.75f        /* the fog starts at this share of the distance and is whole at it */
#define PSM_FOG_START    0.43f
#define PSM_HOLD_POS     2.0f         /* MAP HOLD, the warped map: its camera is taken again when the eye moves this many metres */
#define PSM_HOLD_DIR_COS 0.99756f     /* or turns 4 degrees */
#define PSM_HOLD_LIGHT_COS 0.99999f   /* both maps: the sun is taken again when it moves about a quarter of a degree */        /* with the air on (state atmos, demo/psm/psm_sky.h): the quadratic fog starts here */

/* hold to repeat (the menu's values, the sun's azimuth, the warp's near plane): one step on the press, then after the delay
   8 a second, rising to 30 a second at 2 s held; by time held, not by frames */
#define PSM_REPEAT_DELAY 0.4f
#define PSM_REPEAT_SLOW  8.0f
#define PSM_REPEAT_FAST  30.0f
#define PSM_REPEAT_RAMP  1.6f         /* seconds from the slow rate to the fast one */
#define PSM_REPEAT_CAP   4u           /* steps a frame at most: a long frame does not run a value away (the rest is dropped) */

static const unsigned psm_map_sizes[PSM_MAP_SIZES] = { 64u, 128u, 256u, 512u };
static const unsigned short psm_dist_m[PSM_DIST_STEPS] = { 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 75, 100, 125, 150, 175, 200, 250, 300, 350, 400, 450, 500 };
static const float psm_albedo[3][3] = { { 0.78f, 0.74f, 0.66f }, { 0.86f, 0.62f, 0.46f }, { 0.56f, 0.62f, 0.82f } };   /* pole, cow, cow */

/* the pad, as bits the program fills from the controller */
enum {
    PSM_BTN_A = 1, PSM_BTN_X = 2, PSM_BTN_Y = 4, PSM_BTN_B = 8, PSM_BTN_START = 16,
    PSM_BTN_UP = 32, PSM_BTN_DOWN = 64, PSM_BTN_LEFT = 128, PSM_BTN_RIGHT = 256, PSM_BTN_L = 512, PSM_BTN_R = 1024
};

enum { PSM_ROUTE_A = 0, PSM_ROUTE_B = 1 };

/* ---- scene ----------------------------------------------------------------- */

/* The sky's colour and the fog's at a solar hour: a scene's table, rows in hour order (one row: the same all day). */
typedef struct psm_look_row {
    float hour, sky[3], fog[3];
} psm_look_row_t;

static const psm_look_row_t psm_field_look[1] = { { 12.0f, { 0.55f, 0.68f, 0.85f }, { 0.55f, 0.68f, 0.85f } } };

/* A caster object: its caster vertices and triangles (each a run), and its box in the world (made at load; a mover's
   every frame). */
typedef struct psm_cobj {
    unsigned v0, vn, t0, tn;
    unsigned mover;
    float lo[3], hi[3];
} psm_cobj_t;

/* A mover: a template placed every frame at (x, z), turned to its bearing and tipped forward by tilt about its hip. */
typedef struct psm_mover {
    unsigned kind;                      /* 0 a person, 1 a hen */
    float x, z, bearing, tilt;          /* story metres (z north), radians from north clockwise, radians nose down */
    unsigned v0, vn, cv0, cvn, co;      /* its screen vertices, its caster vertices, its caster object */
    float t, b[4];                      /* a hen: the time left in its move, its bounds (story x0, x1, z0, z1) */
    int mode;
    uint32_t rng;
} psm_mover_t;

typedef struct psm_scene {
    float row_r[PSM_ROWS_MAX];
    unsigned rows;                      /* rings */
    float mv[PSM_MV_MAX][3], mn[PSM_MV_MAX][3];
    uint8_t mkind[PSM_MV_MAX];          /* the vertex's colour, an index into pal (the field: 0 a pole, 1 and 2 the cows) */
    uint16_t mt[PSM_MT_MAX][3];         /* in strip order: a strip's triangle i is its vertices i, i + 1, i + 2 */
    uint8_t mstart[PSM_MT_MAX];         /* 1: the triangle starts a strip (the field: every one) */
    uint8_t mtmov[PSM_MT_MAX];          /* 1: the triangle is a mover's (a cow; the town's people and hens), 0: static (a pole; the town's houses) */
    unsigned mvn, mtn;
    unsigned mdyn0, tdyn0;              /* the movers' first vertex and triangle (mvn and mtn: none): placed every frame */
    float cv[PSM_CV_MAX][3];            /* the casters: the map is drawn from these, never from the screen mesh */
    uint16_t ct[PSM_CT_MAX][3];
    uint8_t ctmov[PSM_CT_MAX];          /* 1: a mover's caster (CASTERS movers only keeps these) */
    unsigned cvn, ctn, cdyn0;
    psm_cobj_t co[PSM_CO_MAX];
    unsigned con;
    psm_mover_t mover[PSM_MOVERS_MAX];
    unsigned movern;
    float dloc[PSM_DYN_V_MAX][3], dnloc[PSM_DYN_V_MAX][3], dcloc[PSM_DYN_CV_MAX][3];   /* the movers' templates, local to their feet */
    float pal[PSM_PAL_MAX][3];
    unsigned paln;
    float amb[3], sunc[3];              /* the light: the ambient colour, and the sun's at N.L 1 */
    float slab[2];                      /* the fit's receiver slab, 0 .. this many metres, by route */
    float ground_uv;                    /* the ground texture's repeats a metre */
    int ground_tex;                     /* 0 the checker, 1 the cobbles */
    const psm_look_row_t *look;         /* the sky and fog table */
    unsigned lookn;
    float x0, x1, z0, z1;               /* the walk area, world x and z */
    int town;                           /* 0 the field, 1 the town */
    float obst[PSM_OBST_MAX][3];        /* x, z, radius: the eye stays out of each (psm_walk_limit) */
    unsigned obn;
} psm_scene_t;

/* The sky and the fog at `hour` from the scene's table. */
static inline void psm_look_at(const psm_scene_t *s, float hour, float sky[3], float fog[3]) {
    const psm_look_row_t *t = s->look ? s->look : psm_field_look;
    const unsigned n = s->look ? s->lookn : 1u;
    unsigned i = 0, k;
    float u = 0.0f;

    if (n > 1u && hour > t[0].hour) {
        while (i + 2u < n && hour > t[i + 1u].hour) i++;
        u = sh4der_clamp_((hour - t[i].hour) * sh4der_recip_pos_(t[i + 1u].hour - t[i].hour + 1e-6f), 0.0f, 1.0f);
    }
    for (k = 0; k < 3u; k++) {
        const float s1 = n > 1u ? t[i + 1u].sky[k] : t[0].sky[k], f1 = n > 1u ? t[i + 1u].fog[k] : t[0].fog[k];

        sky[k] = t[i].sky[k] + (s1 - t[i].sky[k]) * u;
        fog[k] = t[i].fog[k] + (f1 - t[i].fog[k]) * u;
    }
}

/* A box of half sizes h round c, turned by yaw about y, then scaled by s and moved to at: 24 vertices (a normal a face), 12 triangles. */
static inline void psm_scene_box_(psm_scene_t *s, const float c[3], const float h[3], float yaw, float scale, const float at[3], unsigned kind, unsigned mover) {
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
        s->mtmov[s->mtn] = (uint8_t)mover;
        s->mt[s->mtn][0] = (uint16_t)base; s->mt[s->mtn][1] = (uint16_t)(base + 1u); s->mt[s->mtn][2] = (uint16_t)(base + 2u); s->mtn++;
        s->mtmov[s->mtn] = (uint8_t)mover;
        s->mt[s->mtn][0] = (uint16_t)base; s->mt[s->mtn][1] = (uint16_t)(base + 2u); s->mt[s->mtn][2] = (uint16_t)(base + 3u); s->mtn++;
    }
}

static inline void psm_scene_obst_(psm_scene_t *s, float x, float z, float r) {
    if (s->obn >= PSM_OBST_MAX) return;
    s->obst[s->obn][0] = x; s->obst[s->obn][1] = z; s->obst[s->obn][2] = r;
    s->obn++;
}

/* A cow `size` metres long at (x, 0, z), turned by yaw. Two circles along its length keep the eye out of it. */
static inline void psm_scene_cow_(psm_scene_t *s, float x, float z, float size, float yaw, unsigned kind) {
    const float at[3] = { x, 0.0f, z };
    float sn, cs;
#ifdef PSM_HAVE_COW
    unsigned i, base = s->mvn;

    sh4der_sincos_(yaw, &sn, &cs);
    psm_scene_obst_(s, x + 0.25f * size * sn, z + 0.25f * size * cs, 0.25f * size + PSM_WALK_BODY);
    psm_scene_obst_(s, x - 0.25f * size * sn, z - 0.25f * size * cs, 0.25f * size + PSM_WALK_BODY);
    if (s->mvn + PSM_COW_V > PSM_MV_MAX || s->mtn + PSM_COW_T > PSM_MT_MAX) return;
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
        s->mtmov[s->mtn] = 1u;
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

    sh4der_sincos_(yaw, &sn, &cs);
    psm_scene_obst_(s, x + 0.25f * size * sn, z + 0.25f * size * cs, 0.25f * size + PSM_WALK_BODY);
    psm_scene_obst_(s, x - 0.25f * size * sn, z - 0.25f * size * cs, 0.25f * size + PSM_WALK_BODY);
    for (i = 0; i < 6u; i++) psm_scene_box_(s, &part[i][0], &part[i][3], yaw, size, at, kind, 1u);
#endif
}

/* The scene cleared and its rings: both scenes have the same ground grid. */
static inline void psm_scene_rings_(psm_scene_t *s) {
    const float grow = (PSM_ROW_FAR - PSM_ROW_NEAR) / (PSM_FAR - PSM_FIT_NEAR);
    float r = PSM_ROW_R0;

    memset(s, 0, sizeof *s);
    while (s->rows < PSM_ROWS_MAX) {
        float step = PSM_ROW_NEAR + (r - PSM_FIT_NEAR) * grow;

        s->row_r[s->rows++] = r;
        if (r >= PSM_ROW_END) break;
        if (step < PSM_ROW_NEAR) step = PSM_ROW_NEAR;
        r += step;
    }
}

/* The caster objects' boxes, from their vertices. */
static inline void psm_scene_objects_(psm_scene_t *s) {
    unsigned k, i, c;

    for (k = 0; k < s->con; k++) {
        psm_cobj_t *o = &s->co[k];

        for (c = 0; c < 3u; c++) { o->lo[c] = 1e30f; o->hi[c] = -1e30f; }
        for (i = o->v0; i < o->v0 + o->vn; i++)
            for (c = 0; c < 3u; c++) {
                if (s->cv[i][c] < o->lo[c]) o->lo[c] = s->cv[i][c];
                if (s->cv[i][c] > o->hi[c]) o->hi[c] = s->cv[i][c];
            }
    }
}

/* A field object: the vertices and triangles made since (v0, t0), its casters the same (copied at the end of the field's
   init, so the indices are the same). */
static inline void psm_scene_obj_(psm_scene_t *s, unsigned v0, unsigned t0, unsigned mover) {
    if (s->con >= PSM_CO_MAX) return;
    s->co[s->con].v0 = v0; s->co[s->con].vn = s->mvn - v0;
    s->co[s->con].t0 = t0; s->co[s->con].tn = s->mtn - t0;
    s->co[s->con].mover = mover;
    s->con++;
}

/* The rings, the poles and the cows. Once, at load (or when the SCENE row picks the field). */
static inline void psm_scene_init(psm_scene_t *s) {
    unsigned i, v0, t0;

    psm_scene_rings_(s);
    for (i = 0; i < 10u; i++) {
        const float c[3] = { 0.0f, 2.5f, 0.0f }, h[3] = { 0.15f, 2.5f, 0.15f };
        const float at[3] = { (i & 1u) ? 2.6f : -2.6f, 0.0f, -4.0f - 4.4f * (float)i };

        v0 = s->mvn; t0 = s->mtn;
        psm_scene_box_(s, c, h, 0.0f, 1.0f, at, 0u, 0u);
        psm_scene_obj_(s, v0, t0, 0u);
        psm_scene_obst_(s, at[0], at[2], 0.2122f + PSM_WALK_BODY);       /* the pole's half diagonal */
    }
    v0 = s->mvn; t0 = s->mtn;
    psm_scene_cow_(s, 0.3f, -7.0f, 2.2f, 0.5f, 1u);
    psm_scene_obj_(s, v0, t0, 1u);
    v0 = s->mvn; t0 = s->mtn;
    psm_scene_cow_(s, -1.6f, -15.0f, 2.6f, -0.9f, 2u);
    psm_scene_obj_(s, v0, t0, 1u);
    for (i = 0; i < 3u; i++) memcpy(s->pal[i], psm_albedo[i], sizeof s->pal[i]);
    s->paln = 3u;
    memset(s->mstart, 1, s->mtn);
    s->mdyn0 = s->mvn; s->tdyn0 = s->mtn;
    s->cvn = s->mvn < PSM_CV_MAX ? s->mvn : PSM_CV_MAX;                   /* the casters: the screen mesh, copied */
    s->ctn = s->mtn < PSM_CT_MAX ? s->mtn : PSM_CT_MAX;
    memcpy(s->cv, s->mv, s->cvn * sizeof s->cv[0]);
    memcpy(s->ct, s->mt, s->ctn * sizeof s->ct[0]);
    memcpy(s->ctmov, s->mtmov, s->ctn);
    s->cdyn0 = s->cvn;
    for (i = 0; i < 3u; i++) { s->amb[i] = PSM_AMBIENT; s->sunc[i] = 1.0f - PSM_AMBIENT; }
    s->slab[PSM_ROUTE_A] = s->slab[PSM_ROUTE_B] = PSM_SLAB_H;
    s->ground_uv = 0.5f; s->ground_tex = 0;
    s->look = psm_field_look; s->lookn = 1u;
    s->x0 = -PSM_AREA_X; s->x1 = PSM_AREA_X; s->z0 = PSM_AREA_Z0; s->z1 = PSM_AREA_Z1;
    psm_scene_objects_(s);
}

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

/* o = (p, w) x M, the first three: w 1 a point, w 0 a direction. o may not be p. */
static inline void psm_xf_apply3(const psm_xf_t *x, const float p[3], float w, float o[3]) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    const shz_vec4_t v = shz_xmtrx_transform_vec4(shz_vec4_init(p[0], p[1], p[2], w));

    (void)x;
    o[0] = v.x; o[1] = v.y; o[2] = v.z;
#else
    float h[4];

    sh4der_psm_xform(x->m, p, w, h);
    o[0] = h[0]; o[1] = h[1]; o[2] = h[2];
#endif
}

/* A mover's matrix in row form (p' = (p, 1) x M): tipped by tilt about the hip (0.1 m up, about x), turned to the bearing
   (about y), at (x, -z). The rows are the images of x, y, z and the origin. */
static inline void psm_mover_rows(const psm_mover_t *m, float r[16]) {
    float sb, cb, st, ct;

    sh4der_sincos_(m->bearing, &sb, &cb);
    sh4der_sincos_(m->tilt, &st, &ct);
    r[0] = cb;            r[1] = 0.0f;                r[2] = sb;                       r[3] = 0.0f;
    r[4] = sb * st;       r[5] = ct;                  r[6] = -cb * st;                 r[7] = 0.0f;
    r[8] = -sb * ct;      r[9] = st;                  r[10] = cb * ct;                 r[11] = 0.0f;
    r[12] = m->x - 0.1f * st * sb; r[13] = 0.1f - 0.1f * ct; r[14] = -m->z + 0.1f * st * cb; r[15] = 1.0f;
}

/* The movers' vertices from their templates through their matrix (psm_mover_rows, one FTRV a point on the console; the
   normals with w 0); their casters the same, and their objects' boxes. Every frame, before the placement (which loads its
   own rows: XMTRX is left with the last mover's). */
static inline void psm_movers_apply(psm_scene_t *s) {
    unsigned k, i, c;

    for (k = 0; k < s->movern; k++) {
        const psm_mover_t *m = &s->mover[k];
        psm_cobj_t *o = &s->co[m->co];
        psm_xf_t xf;
        float r[16];

        psm_mover_rows(m, r);
        psm_xf_load(&xf, r);
        for (i = 0; i < m->vn; i++) {
            psm_xf_apply3(&xf, s->dloc[m->v0 + i - s->mdyn0], 1.0f, s->mv[m->v0 + i]);
            psm_xf_apply3(&xf, s->dnloc[m->v0 + i - s->mdyn0], 0.0f, s->mn[m->v0 + i]);
        }
        for (c = 0; c < 3u; c++) { o->lo[c] = 1e30f; o->hi[c] = -1e30f; }
        for (i = 0; i < m->cvn; i++) {
            float *p = s->cv[m->cv0 + i];

            psm_xf_apply3(&xf, s->dcloc[m->cv0 + i - s->cdyn0], 1.0f, p);
            for (c = 0; c < 3u; c++) {
                if (p[c] < o->lo[c]) o->lo[c] = p[c];
                if (p[c] > o->hi[c]) o->hi[c] = p[c];
            }
        }
    }
}

/* ---- walking: the camera on the ground (the town demo takes this part whole) ---- */

/* The walk area and the circles the eye stays out of. */
typedef struct psm_walk_limits {
    float x0, x1, z0, z1;
    const float (*circle)[3];           /* x, z, radius */
    unsigned n;
} psm_walk_limits_t;

/* A stick axis with the dead zone taken off (torchwalk's: under 12 of 128 is nothing, past it the value as it is). */
static inline float psm_axis_(float v) { return (v > PSM_STICK_DEAD || v < -PSM_STICK_DEAD) ? v : 0.0f; }

static inline float psm_wrap_2pi_(float a) {
    if (a >= 6.2831853f) a -= 6.2831853f;
    if (a < 0.0f) a += 6.2831853f;
    return a;
}

/* The eye held in the area and outside every circle. A few passes, so circles that overlap settle; the area last, so the eye
   never leaves it (the scene keeps its circles clear of the area's edge, the host test checks that). An eye on a circle's very
   centre goes out backwards along the heading (sy, cy). */
static inline void psm_walk_limit(float *x, float *z, float sy, float cy, const psm_walk_limits_t *lim) {
    unsigned pass, i;

    if (!lim) return;
    for (pass = 0; pass < 8u; pass++) {
        int moved = 0;

        *x = sh4der_clamp_(*x, lim->x0, lim->x1);
        *z = sh4der_clamp_(*z, lim->z0, lim->z1);
        for (i = 0; i < lim->n; i++) {
            const float *c = lim->circle[i];
            float dx = *x - c[0], dz = *z - c[1], d2 = dx * dx + dz * dz, k;

            if (d2 >= c[2] * c[2]) continue;
            if (d2 < 1e-8f) { dx = -sy; dz = cy; d2 = 1.0f; }
            k = c[2] * sh4der_recip_pos_(sh4der_sqrt_(d2)) * 1.0001f;    /* onto the circle, a hair outside */
            *x = c[0] + dx * k; *z = c[1] + dz * k;
            moved = 1;
        }
        if (!moved) break;
    }
    *x = sh4der_clamp_(*x, lim->x0, lim->x1);
    *z = sh4der_clamp_(*z, lim->z0, lim->z1);
}

/* One frame of walking: look_x, look_y the stick (-1 .. 1, the dead zone already off, y down positive as the pad gives it),
   fwd and side -1 .. 1 (the face buttons), dt seconds. Pitch is positive down. INVERT Y on: the stick up looks down. The walk
   is on the ground plane, along the heading's horizontal part only, then held by the limits. */
static inline void psm_walk(float *x, float *z, float *yaw, float *pitch, float look_x, float look_y, int invert_y, float fwd, float side,
                            float dt, const psm_walk_limits_t *lim) {
    float sy, cy;

    *yaw = psm_wrap_2pi_(*yaw + look_x * PSM_LOOK_SPEED * dt);
    *pitch = sh4der_clamp_(*pitch + (invert_y ? -1.0f : 1.0f) * look_y * PSM_LOOK_SPEED * dt, -PSM_PITCH_UP, PSM_PITCH_DOWN);
    sh4der_sincos_(*yaw, &sy, &cy);
    *x += (sy * fwd + cy * side) * PSM_WALK_SPEED * dt;            /* forward (sy, -cy), right (cy, sy) */
    *z += (-cy * fwd + sy * side) * PSM_WALK_SPEED * dt;
    psm_walk_limit(x, z, sy, cy, lim);
}

/* The nearest lattice index of v: the grid's centre is (ix, iz) x PSM_SNAP. */
static inline int psm_snap_(float v) {
    const float r = v * (1.0f / PSM_SNAP) + 0.5f;

    return (int)(r - sh4der_frac_(r));
}

/* ---- hold to repeat ------------------------------------------------------------ */

typedef struct psm_repeat {
    float t;             /* seconds held */
    unsigned done;       /* repeats given since the press */
    int on;
} psm_repeat_t;

/* The repeats due after t seconds held: the rate's integral past the delay (8 a second, rising evenly to 30 over the ramp). */
static inline float psm_repeat_due_(float t) {
    const float u = t - PSM_REPEAT_DELAY;

    if (!(u > 0.0f)) return 0.0f;
    if (u < PSM_REPEAT_RAMP) return PSM_REPEAT_SLOW * u + (PSM_REPEAT_FAST - PSM_REPEAT_SLOW) * u * u * (0.5f / PSM_REPEAT_RAMP);
    return PSM_REPEAT_RAMP * 0.5f * (PSM_REPEAT_SLOW + PSM_REPEAT_FAST) + PSM_REPEAT_FAST * (u - PSM_REPEAT_RAMP);
}

/* The steps this frame for a button `held` or not, dt seconds since the last frame: 1 on the press, then the repeats due,
   at most PSM_REPEAT_CAP a frame. A release starts it over. */
static inline unsigned psm_repeat_step(psm_repeat_t *r, int held, float dt) {
    unsigned due, n;

    if (!held) { r->on = 0; r->t = 0.0f; r->done = 0u; return 0u; }
    if (!r->on) { r->on = 1; r->t = 0.0f; r->done = 0u; return 1u; }
    r->t += dt > 0.0f ? dt : 0.0f;
    if (r->t > 3600.0f) r->t = 3600.0f;
    due = (unsigned)psm_repeat_due_(r->t);
    n = due > r->done ? due - r->done : 0u;
    r->done = due;
    return n < PSM_REPEAT_CAP ? n : PSM_REPEAT_CAP;
}

/* ---- state and pad ----------------------------------------------------------- */

/* The settings panel's rows (START opens it): in three groups, a tab each; L / R pick the tab, D-pad up / down the row,
   left / right (and A) change it. */
enum {
    PSM_ROW_ROUTE, PSM_ROW_MAP, PSM_ROW_HOLD, PSM_ROW_CASTERS, PSM_ROW_DIST, PSM_ROW_SIZE, PSM_ROW_FILTER_A, PSM_ROW_PCF, PSM_ROW_BIAS_A, PSM_ROW_BIAS_B, PSM_ROW_WARP_NEAR, PSM_ROW_AZIMUTH,
    PSM_ROW_ELEVATION, PSM_ROW_INSET, PSM_ROW_READOUT, PSM_ROW_PRESET, PSM_ROW_INVY, PSM_ROW_SCENE, PSM_ROW_TIME, PSM_ROW_TSPEED, PSM_ROW_FREEZE, PSM_ROW_SEND, PSM_ROW_AO, PSM_ROWS
};
static const char *const psm_row_name[PSM_ROWS] = {
    "ROUTE", "MAP", "MAP HOLD", "CASTERS", "DRAW DISTANCE", "MAP SIZE", "A FILTER", "B FILTER", "BIAS CONST", "BIAS SLOPE", "WARP NEAR", "SUN AZIMUTH", "SUN ELEVATION", "INSET", "READOUT", "PRESETS", "INVERT Y",
    "SCENE", "TIME", "TIME SPEED", "FREEZE", "TA SEND", "AO"
};

/* The rows as data, for a panel that draws the table: the group (a tab), the kind, and for a slider its range and step in
   the value psm_row_num gives (the unit the row shows). A toggle has two states, a choice a few named ones; a held
   left / right repeats on the rows that say so. psm_row_change does the change itself (some rows move other state). */
enum { PSM_GRP_SHADOW, PSM_GRP_SUN, PSM_GRP_VIEW, PSM_GRPS };
enum { PSM_RK_TOGGLE, PSM_RK_CHOICE, PSM_RK_SLIDER };
typedef struct psm_row_def {
    uint8_t group, kind, repeats, wraps;
    float lo, hi, step;
} psm_row_def_t;
static const char *const psm_grp_name[PSM_GRPS] = { "SHADOWS", "SUN & TIME", "VIEW" };
static const psm_row_def_t psm_row_def[PSM_ROWS] = {
    { PSM_GRP_SHADOW, PSM_RK_CHOICE, 0, 1, 0.0f, 0.0f, 0.0f },        /* ROUTE */
    { PSM_GRP_SHADOW, PSM_RK_TOGGLE, 0, 1, 0.0f, 0.0f, 0.0f },        /* MAP */
    { PSM_GRP_SHADOW, PSM_RK_TOGGLE, 0, 1, 0.0f, 0.0f, 0.0f },        /* MAP HOLD */
    { PSM_GRP_SHADOW, PSM_RK_TOGGLE, 0, 1, 0.0f, 0.0f, 0.0f },        /* CASTERS */
    { PSM_GRP_VIEW,   PSM_RK_CHOICE, 1, 0, 0.0f, 0.0f, 0.0f },        /* DRAW DISTANCE */
    { PSM_GRP_SHADOW, PSM_RK_CHOICE, 1, 1, 0.0f, 0.0f, 0.0f },        /* MAP SIZE */
    { PSM_GRP_SHADOW, PSM_RK_TOGGLE, 0, 1, 0.0f, 0.0f, 0.0f },        /* A FILTER */
    { PSM_GRP_SHADOW, PSM_RK_TOGGLE, 0, 1, 0.0f, 0.0f, 0.0f },        /* B FILTER */
    { PSM_GRP_SHADOW, PSM_RK_SLIDER, 1, 0, 0.0f, 200.0f, 5.0f },      /* BIAS CONST, mm */
    { PSM_GRP_SHADOW, PSM_RK_SLIDER, 1, 0, 0.0f, 4.0f, 0.5f },        /* BIAS SLOPE, texels */
    { PSM_GRP_SHADOW, PSM_RK_SLIDER, 1, 0, 1.0f, 12.0f, 0.5f },       /* WARP NEAR, m */
    { PSM_GRP_SUN,    PSM_RK_SLIDER, 1, 1, 0.0f, 355.0f, 5.0f },      /* SUN AZIMUTH, degrees */
    { PSM_GRP_SUN,    PSM_RK_SLIDER, 1, 0, 5.0f, 85.0f, 5.0f },       /* SUN ELEVATION, degrees */
    { PSM_GRP_VIEW,   PSM_RK_TOGGLE, 0, 1, 0.0f, 0.0f, 0.0f },        /* INSET */
    { PSM_GRP_VIEW,   PSM_RK_CHOICE, 1, 1, 0.0f, 0.0f, 0.0f },        /* READOUT */
    { PSM_GRP_SUN,    PSM_RK_CHOICE, 1, 1, 0.0f, 0.0f, 0.0f },        /* PRESETS */
    { PSM_GRP_VIEW,   PSM_RK_TOGGLE, 0, 1, 0.0f, 0.0f, 0.0f },        /* INVERT Y */
    { PSM_GRP_VIEW,   PSM_RK_TOGGLE, 0, 1, 0.0f, 0.0f, 0.0f },        /* SCENE */
    { PSM_GRP_SUN,    PSM_RK_SLIDER, 1, 1, 0.0f, 1425.0f, 15.0f },    /* TIME, minutes of the day */
    { PSM_GRP_SUN,    PSM_RK_CHOICE, 0, 1, 0.0f, 0.0f, 0.0f },        /* TIME SPEED */
    { PSM_GRP_SUN,    PSM_RK_TOGGLE, 0, 1, 0.0f, 0.0f, 0.0f },        /* FREEZE */
    { PSM_GRP_VIEW,   PSM_RK_TOGGLE, 0, 1, 0.0f, 0.0f, 0.0f },        /* TA SEND */
    { PSM_GRP_VIEW,   PSM_RK_TOGGLE, 0, 1, 0.0f, 0.0f, 0.0f },        /* AO */
};
#define PSM_ROUTES 3                  /* A, B, AB (demo/psm/psm_mapsrc.h: PSM_ROUTE_AB 2) */

/* The rows of group g in table order into out; returns how many. */
static inline unsigned psm_grp_rows(int g, int out[PSM_ROWS]) {
    unsigned n = 0;
    int r;

    for (r = 0; r < (int)PSM_ROWS; r++)
        if (psm_row_def[r].group == g) out[n++] = r;
    return n;
}

/* The row's place in its group (0 for a row not in it). */
static inline unsigned psm_grp_index(int g, int row) {
    int rows[PSM_ROWS];
    const unsigned n = psm_grp_rows(g, rows);
    unsigned i;

    for (i = 0; i < n; i++)
        if (rows[i] == row) return i;
    return 0;
}
static const float psm_preset_sun[3][2] = { { 120.0f, 35.0f }, { 0.0f, 20.0f }, { 180.0f, 20.0f } };   /* default, head-on, behind */
static const char *const psm_preset_name[3] = { "default", "head-on", "behind" };

typedef struct psm_state {
    int route, warp, inset, readout, pcf, split, invert_y, menu, row;   /* pcf: B FILTER, 1 the 2 x 2 compare-then-blend */
    int filter_a;        /* A FILTER: 1 the PVR's bilinear on the target lookup, 0 point */
    int hold;            /* MAP HOLD: 1 the fit held still (sh4der_psm_build_*_held: texels do not swim), 0 fitted again each frame */
    unsigned size_refused_kb;   /* the last bigger map the program could not allocate (KB; 0: none), on the readout */
    int movers_only;     /* CASTERS: 0 every caster into the map, 1 only the movers' (the static casters are not drawn into it) */
    unsigned dist_i;     /* DRAW DISTANCE: psm_dist_m[dist_i] metres, PSM_DIST_INF for no cut and no fog */
    psm_repeat_t rep_l, rep_r, rep_wl, rep_wr;   /* the D-pad's left and right (and A in the panel); rep_wl / rep_wr unused */
    psm_repeat_t rep_u, rep_d;                    /* the D-pad's up and down in the panel */
    int grp;             /* the panel's tab (PSM_GRP_*); `row` is a row of it */
    unsigned size_i, preset;
    float az, el, warp_near, bias_a, bias_b, yaw, pitch, x, z;
    unsigned last;       /* the pad a frame ago */
    int start_used;      /* START was a shift this press (or closed the menu), not a tap */
    int scene;           /* SCENE: 0 the field, 1 the town (the program loads it when it differs from the scene it has) */
    float clock;         /* the town's solar time, hours */
    unsigned tspeed;     /* TIME SPEED: psm_time_speed's index */
    int sun_free;        /* 1: the sun was set by hand (azimuth, elevation), off the clock */
    int freeze;          /* FREEZE: the people and the hens stand still */
    int send_runs;       /* TA SEND: 1 the vertices into 8 KB of cache lines (MOVCA) and to the TA in store-queue runs, 0 one
                            pvr_dr store-queue write a vertex (psm_dc.c; the host draws nothing) */
    float cast_t;        /* the cast's clock, seconds */
    unsigned tpreset;    /* the town's last preset */
    int atmos;           /* 1: the sky band, the hills and the quadratic fog (demo/psm/psm_sky.h, TOWN_LOOK.md section 3); the town's
                            default, and the slot the LOOK row will set (LAUNCH 0, LATE 1) */
    int hide;            /* 1: nothing of the program's own is drawn over the picture (the readout, the panel, the inset, the
                            heartbeat): Y + A together, panel closed, turns it on and off; opening the panel shows all again */
    int ao;              /* AO: 1 the baked occlusion (psm_ao.h) darkens the ambient light; the default */
    unsigned ao_ms;      /* what the bake took at load, ms (the program's clock; 0: not timed, a host) */
    int ao_none;         /* 1: the bake did not run (no memory, or a scene with nothing static) */
} psm_state_t;

/* What the program reads from the controller: the PSM_BTN_ bits held, the stick -1 .. 1 (y down positive), the triggers 0 .. 1. */
typedef struct psm_pad {
    unsigned held;
    float sx, sy, lt, rt;
} psm_pad_t;

/* The boot state: route A, the uniform map, 256, the host demo's sun, the camera at the origin, menu closed, INVERT Y on. */
static inline void psm_state_default(psm_state_t *st) {
    memset(st, 0, sizeof *st);
    st->route = PSM_ROUTE_A;
    st->warp = 0;
    st->readout = 1;
    st->pcf = 1;
    st->filter_a = 1;
    st->hold = 1;
    st->send_runs = 1;
    st->ao = 1;
    st->invert_y = 1;
    st->size_i = 2u;                                   /* 256 (512 is a choice for route B and AB) */
    st->az = psm_preset_sun[0][0]; st->el = psm_preset_sun[0][1];
    st->warp_near = 4.0f;
    st->bias_a = 0.02f; st->bias_b = 0.0f;
    st->pitch = PSM_PITCH;
    st->dist_i = PSM_DIST_INF;
    st->clock = 14.0f;
    st->tspeed = 1u;
}

/* The trigonometry sh4der_core.h does not wrap (AGENTS.md "Maths and speed"): SH4ZAM's on the console, libm on the host's
   plain C path. psm_town.h and psm_sky.h use these. */
#ifndef SH4DER_CORE_NO_SH4ZAM
#include <sh4zam_dce/shz_dce_atan2.h>
/* shz_atan2f and shz_asinf are fast approximations (0.58 and 0.28 degrees worst on the host's software back end), too coarse
   for the story's sun table (0.1 degree in elevation): the atan2 is SH4ZAM-DCE's (0.0007 degrees), and asin goes through it. */
static inline float psm_sin_(float x) { return shz_sinf(x); }
static inline float psm_tan_(float x) { return shz_tanf(x); }
static inline float psm_atan2_(float y, float x) { return shz_dce_atan2f(y, x); }
static inline float psm_asin_(float x) { return shz_dce_atan2f(x, sh4der_sqrt_(1.0f - x * x)); }   /* |x| <= 1 */
#else
static inline float psm_sin_(float x) { return sinf(x); }
static inline float psm_tan_(float x) { return tanf(x); }
static inline float psm_asin_(float x) { return asinf(x); }
static inline float psm_atan2_(float y, float x) { return atan2f(y, x); }
#endif

#include "psm_town.h"

/* The draw distance in metres, 0 for INFINITE. */
/* the share of DRAW DISTANCE where the fog starts: linear from 0.75 (the field), quadratic from 0.43 with the air on */
static inline float psm_fog_start(const psm_state_t *st) { return st->atmos ? PSM_FOG_START : PSM_DIST_FADE; }
static inline float psm_dist(const psm_state_t *st) { return st->dist_i < PSM_DIST_STEPS ? (float)psm_dist_m[st->dist_i] : 0.0f; }

/* 1 for a row whose change is a value that steps (it repeats when held), 0 for a toggle (one change a press). */
/* the largest map side of a route, and the sizes it takes (the first n of psm_map_sizes) */
static inline unsigned psm_map_cap(int route) { return route == PSM_ROUTE_A ? PSM_MAP_MAX_A : PSM_MAP_MAX_B; }
static inline unsigned psm_map_sizes_n(int route) {
    unsigned n = 0;

    while (n < PSM_MAP_SIZES && psm_map_sizes[n] <= psm_map_cap(route)) n++;
    return n;
}

/* the index of the map side the route draws: the chosen one, or the route's largest under it */
static inline unsigned psm_map_index(const psm_state_t *st) {
    const unsigned n = psm_map_sizes_n(st->route), i = st->size_i % PSM_MAP_SIZES;

    return i < n ? i : n - 1u;
}
static inline unsigned psm_map_size(const psm_state_t *st) { return psm_map_sizes[psm_map_index(st)]; }

static inline int psm_row_repeats(int row) { return row >= 0 && row < (int)PSM_ROWS && psm_row_def[row].repeats; }

static inline float psm_step_(float x, float d, float lo, float hi) {
    return sh4der_clamp_(x + d, lo, hi);   /* the step, held in lo .. hi */
}

static inline float psm_wrap_360_(float a) {
    if (a >= 360.0f) a -= 360.0f;
    if (a < 0.0f) a += 360.0f;
    return a;
}

/* The camera and the sun a scene starts with: the town's first view, or the field's origin and its preset's sun. */
static inline void psm_scene_enter(psm_state_t *st) {
    st->atmos = st->scene;
    if (st->scene) { psm_town_preset_set(st, 0u); return; }
    st->x = st->z = st->yaw = 0.0f;
    st->pitch = PSM_PITCH;
    st->az = psm_preset_sun[st->preset % 3u][0]; st->el = psm_preset_sun[st->preset % 3u][1];
}

/* the clock a step on, held in the sweep; the sun back on it */
static inline void psm_clock_step_(psm_state_t *st, float d) {
    st->clock = psm_step_(st->clock, d, PSM_CLOCK_T0, PSM_CLOCK_T1);
    st->sun_free = 0;
    if (st->scene) psm_sun_at(st->clock, &st->az, &st->el);
}

/* A row changed by d (-1 or +1): a toggle flips, a list steps round, a number steps and stops at its ends. */
static inline void psm_row_change(psm_state_t *st, int row, int d) {
    switch (row) {
        case PSM_ROW_ROUTE: st->route = (st->route + (d > 0 ? 1 : PSM_ROUTES - 1)) % PSM_ROUTES; break;   /* A, B, AB */
        case PSM_ROW_MAP: st->warp = !st->warp; break;
        case PSM_ROW_HOLD: st->hold = !st->hold; break;
        case PSM_ROW_CASTERS: st->movers_only = !st->movers_only; break;
        case PSM_ROW_DIST:                                  /* stops at 5 m and at INFINITE */
            if (d > 0 && st->dist_i < PSM_DIST_INF) st->dist_i++;
            if (d < 0 && st->dist_i > 0u) st->dist_i--;
            break;
        case PSM_ROW_SIZE: {                                /* round the route's own sizes */
            const unsigned n = psm_map_sizes_n(st->route);

            st->size_i = (psm_map_index(st) + (d > 0 ? 1u : n - 1u)) % n;
            st->size_refused_kb = 0u;
            break;
        }
        case PSM_ROW_FILTER_A: st->filter_a = !st->filter_a; break;
        case PSM_ROW_PCF: st->pcf = !st->pcf; break;
        case PSM_ROW_BIAS_A: st->bias_a = psm_step_(st->bias_a, 0.005f * (float)d, 0.0f, 0.2f); break;
        case PSM_ROW_BIAS_B: st->bias_b = psm_step_(st->bias_b, 0.5f * (float)d, 0.0f, 4.0f); break;
        case PSM_ROW_WARP_NEAR: st->warp_near = psm_step_(st->warp_near, 0.5f * (float)d, 1.0f, 12.0f); break;
        case PSM_ROW_AZIMUTH: st->az = psm_wrap_360_(st->az + 5.0f * (float)d); st->sun_free = 1; break;
        case PSM_ROW_ELEVATION: st->el = psm_step_(st->el, 5.0f * (float)d, 5.0f, 85.0f); st->sun_free = 1; break;
        case PSM_ROW_INSET: st->inset = !st->inset; break;
        case PSM_ROW_READOUT: st->readout = (st->readout + (d > 0 ? 1 : 2)) % 3; break;   /* off, on, on + the SH-4's split */
        case PSM_ROW_PRESET:
            if (st->scene) { psm_town_preset_set(st, st->tpreset + (d > 0 ? 1u : PSM_TOWN_PRESETS - 1u)); break; }
            st->preset = (st->preset + (d > 0 ? 1u : 2u)) % 3u;
            st->az = psm_preset_sun[st->preset][0]; st->el = psm_preset_sun[st->preset][1];
            break;
        case PSM_ROW_INVY: st->invert_y = !st->invert_y; break;
        case PSM_ROW_SCENE: st->scene = !st->scene; psm_scene_enter(st); break;
        case PSM_ROW_TIME: psm_clock_step_(st, PSM_CLOCK_STEP * (float)d); break;
        case PSM_ROW_TSPEED: st->tspeed = (st->tspeed + (d > 0 ? 1u : PSM_TIME_SPEEDS - 1u)) % PSM_TIME_SPEEDS; break;
        case PSM_ROW_FREEZE: st->freeze = !st->freeze; break;
        case PSM_ROW_SEND: st->send_runs = !st->send_runs; break;
        case PSM_ROW_AO: st->ao = !st->ao; break;
        default: break;
    }
    st->split = st->readout == 2;
}

/* The row's value as the menu line shows it. */
static inline const char *psm_row_value(const psm_state_t *st, int row, char *buf, size_t cap) {
    unsigned t;

    switch (row) {
        case PSM_ROW_ROUTE: return st->route == PSM_ROUTE_A ? "A  PVR draws the map" : st->route == PSM_ROUTE_B ? "B  SH-4 depth map"
                                   : "AB  stub, a frame late";          /* route AB: demo/psm/psm_mapsrc.h */
        case PSM_ROW_MAP: return st->warp ? "PSM (perspective)" : "uniform";
        case PSM_ROW_HOLD: return st->hold ? "ON  still texels" : "OFF  refit each frame";
        case PSM_ROW_CASTERS: return st->movers_only ? (st->scene ? "movers only (people, hens)" : "movers only (cows)") : "all";
        case PSM_ROW_DIST:
            if (st->dist_i >= PSM_DIST_INF) return "INF: no cut, no fog";
            snprintf(buf, cap, "%u m, fog from %u", (unsigned)psm_dist_m[st->dist_i], (unsigned)(psm_dist_m[st->dist_i] * psm_fog_start(st) + 0.5f));
            return buf;
        case PSM_ROW_SIZE:
            snprintf(buf, cap, "%u x %u%s", psm_map_size(st), psm_map_size(st), psm_map_size(st) > PSM_MAP_STATIC ? " (not speed)" : "");
            return buf;
        case PSM_ROW_FILTER_A: return st->filter_a ? "BILINEAR (the PVR)" : "POINT (the PVR)";
        case PSM_ROW_PCF: return st->pcf ? "BILINEAR 2x2 (B, AB)" : "POINT (B, AB)";
        case PSM_ROW_BIAS_A: snprintf(buf, cap, "%u mm (route B)", (unsigned)(st->bias_a * 1000.0f + 0.5f)); return buf;
        case PSM_ROW_BIAS_B: t = (unsigned)(st->bias_b * 10.0f + 0.5f); snprintf(buf, cap, "%u.%u (route B)", t / 10u, t % 10u); return buf;
        case PSM_ROW_WARP_NEAR: t = (unsigned)(st->warp_near * 10.0f + 0.5f); snprintf(buf, cap, "%u.%u m (PSM)", t / 10u, t % 10u); return buf;
        case PSM_ROW_AZIMUTH: snprintf(buf, cap, "%u deg", (unsigned)(st->az + 0.5f)); return buf;
        case PSM_ROW_ELEVATION: snprintf(buf, cap, "%u deg", (unsigned)(st->el + 0.5f)); return buf;
        case PSM_ROW_INSET: return st->inset ? "on" : "off";
        case PSM_ROW_READOUT: return st->readout == 0 ? "off" : st->readout == 1 ? "on" : "on + SH-4 split";
        case PSM_ROW_PRESET: return st->scene ? psm_town_preset[st->tpreset % PSM_TOWN_PRESETS].name : psm_preset_name[st->preset % 3u];
        case PSM_ROW_INVY: return st->invert_y ? "on: stick up looks down" : "off: stick up looks up";
        case PSM_ROW_SCENE: return st->scene ? PSM_TOWN_NAME_ : "field (poles, cows)";
        case PSM_ROW_TIME:
            t = (unsigned)(st->clock * 60.0f + 0.5f);
            snprintf(buf, cap, "%02u:%02u%s", t / 60u, t % 60u, st->sun_free ? " (sun free: az/el set)" : st->scene ? " (the sun on it)" : " (town only)");
            return buf;
        case PSM_ROW_TSPEED:
            if (!psm_time_speed[st->tspeed % PSM_TIME_SPEEDS]) return "paused";
            snprintf(buf, cap, "x%u", (unsigned)psm_time_speed[st->tspeed % PSM_TIME_SPEEDS]);
            return buf;
        case PSM_ROW_FREEZE: return st->freeze ? "on: people and hens stand" : "off";
        case PSM_ROW_SEND: return st->send_runs ? "SQ RUNS  8 KB a run" : "PVR_DR  a vertex a write";
        case PSM_ROW_AO:
            if (!st->ao) return "off";
            if (st->ao_none) return "ON  (not baked: all open)";
            if (!st->ao_ms) return "ON  baked a vertex (SH-4)";
            snprintf(buf, cap, "ON  SH-4 bake %u.%u s at load", st->ao_ms / 1000u, st->ao_ms % 1000u / 100u); return buf;
        default: return "?";
    }
}
/* A row's value as one whole number, for the fault catcher's breadcrumbs (psm_fault_menu): the field the row sets, the
   fractional ones in thousandths (biases), hundredths (warp near) or minutes (time). */
static inline int32_t psm_row_int(const psm_state_t *st, int row) {
    switch (row) {
        case PSM_ROW_ROUTE: return st->route;
        case PSM_ROW_MAP: return st->warp;
        case PSM_ROW_HOLD: return st->hold;
        case PSM_ROW_CASTERS: return st->movers_only;
        case PSM_ROW_DIST: return (int32_t)st->dist_i;
        case PSM_ROW_SIZE: return (int32_t)psm_map_size(st);
        case PSM_ROW_FILTER_A: return st->filter_a;
        case PSM_ROW_PCF: return st->pcf;
        case PSM_ROW_BIAS_A: return (int32_t)(st->bias_a * 1000.0f);
        case PSM_ROW_BIAS_B: return (int32_t)(st->bias_b * 1000.0f);
        case PSM_ROW_WARP_NEAR: return (int32_t)(st->warp_near * 100.0f);
        case PSM_ROW_AZIMUTH: return (int32_t)st->az;
        case PSM_ROW_ELEVATION: return (int32_t)st->el;
        case PSM_ROW_INSET: return st->inset;
        case PSM_ROW_READOUT: return st->readout;
        case PSM_ROW_PRESET: return (int32_t)(st->scene ? st->tpreset : st->preset);
        case PSM_ROW_INVY: return st->invert_y;
        case PSM_ROW_SCENE: return st->scene;
        case PSM_ROW_TIME: return (int32_t)(st->clock * 60.0f);
        case PSM_ROW_TSPEED: return (int32_t)st->tspeed;
        case PSM_ROW_FREEZE: return st->freeze;
        case PSM_ROW_SEND: return st->send_runs;
        case PSM_ROW_AO: return st->ao;
        default: return 0;
    }
}

/* One frame of the pad, torchwalk's single-stick scheme. The stick looks (yaw, pitch). Panel closed: Y / A held walk forward /
   back, X / B strafe left / right; START opens the settings panel; D-pad up steps the route (A, B, AB), down flips uniform /
   PSM, left / right step the clock a quarter of an hour in the town (turn the sun in the field); the triggers raise (R) and
   lower (L) the sun and set it free of the clock. Panel open: nothing walks; L / R pick the tab (round, its first row), D-pad
   up / down the row (round, held repeats), left / right change it and A steps it on (psm_row_change: a slider stops at its
   ends, a choice goes round), B or START close it. A value held repeats (psm_repeat_step) on the rows that say so
   (psm_row_def); a toggle changes once a press. What START held used to shift (the warp's near plane, the casters, the sun
   off the clock, the freeze, the presets) are rows. `lim` holds the walk (NULL: no limits). */
static inline void psm_input(psm_state_t *st, const psm_walk_limits_t *lim, const psm_pad_t *pad, float dt) {
    const unsigned held = pad->held, down = held & ~st->last;
    const unsigned nl = psm_repeat_step(&st->rep_l, (held & PSM_BTN_LEFT) != 0u, dt);
    const unsigned nr = psm_repeat_step(&st->rep_r, (held & (st->menu ? (unsigned)(PSM_BTN_RIGHT | PSM_BTN_A) : (unsigned)PSM_BTN_RIGHT)) != 0u, dt);
    const unsigned nu = psm_repeat_step(&st->rep_u, st->menu && (held & PSM_BTN_UP), dt);
    const unsigned nd = psm_repeat_step(&st->rep_d, st->menu && (held & PSM_BTN_DOWN), dt);
    float fwd = 0.0f, side = 0.0f, lt = pad->lt > PSM_TRIG_DEAD ? pad->lt : 0.0f, rt = pad->rt > PSM_TRIG_DEAD ? pad->rt : 0.0f;
    unsigned k;

    if (st->menu) {
        int rows[PSM_ROWS];
        unsigned n, at;

        if (st->grp < 0 || st->grp >= (int)PSM_GRPS || psm_row_def[st->row].group != st->grp) st->grp = psm_row_def[st->row].group;
        if (down & (PSM_BTN_L | PSM_BTN_R)) {
            st->grp = (st->grp + ((down & PSM_BTN_R) ? 1 : (int)PSM_GRPS - 1)) % (int)PSM_GRPS;
            psm_grp_rows(st->grp, rows);
            st->row = rows[0];
        }
        n = psm_grp_rows(st->grp, rows);
        at = psm_grp_index(st->grp, st->row);
        for (k = 0; k < nu; k++) at = (at + n - 1u) % n;
        for (k = 0; k < nd; k++) at = (at + 1u) % n;
        st->row = rows[at];
        if (psm_row_repeats(st->row)) {
            for (k = 0; k < nl; k++) psm_row_change(st, st->row, -1);
            for (k = 0; k < nr; k++) psm_row_change(st, st->row, 1);
        } else {
            if (down & PSM_BTN_LEFT) psm_row_change(st, st->row, -1);
            if (down & (PSM_BTN_RIGHT | PSM_BTN_A)) psm_row_change(st, st->row, 1);
        }
        if (down & (PSM_BTN_B | PSM_BTN_START)) st->menu = 0;
        lt = rt = 0.0f;                                         /* the triggers pick the tab here, not the sun */
    } else {
        if (down & PSM_BTN_START) { st->menu = 1; st->hide = 0; if (psm_row_def[st->row].group != st->grp) st->grp = psm_row_def[st->row].group; }
        if ((held & (PSM_BTN_Y | PSM_BTN_A)) == (PSM_BTN_Y | PSM_BTN_A) && (st->last & (PSM_BTN_Y | PSM_BTN_A)) != (PSM_BTN_Y | PSM_BTN_A))
            st->hide = !st->hide;                               /* Y + A (forward and back: no step): a clean picture, and back */
        if (down & PSM_BTN_UP) psm_row_change(st, PSM_ROW_ROUTE, 1);
        if (down & PSM_BTN_DOWN) st->warp = !st->warp;
        for (k = 0; k < nr; k++) { if (st->scene) psm_clock_step_(st, PSM_CLOCK_STEP); else st->az = psm_wrap_360_(st->az + 5.0f); }
        for (k = 0; k < nl; k++) { if (st->scene) psm_clock_step_(st, -PSM_CLOCK_STEP); else st->az = psm_wrap_360_(st->az - 5.0f); }
        if (held & PSM_BTN_Y) fwd += 1.0f;
        if (held & PSM_BTN_A) fwd -= 1.0f;
        if (held & PSM_BTN_X) side -= 1.0f;
        if (held & PSM_BTN_B) side += 1.0f;
    }
    if (lt > 0.0f || rt > 0.0f) { st->el = psm_step_(st->el, (rt - lt) * PSM_SUN_SPEED * dt, 5.0f, 85.0f); st->sun_free = 1; }
    psm_walk(&st->x, &st->z, &st->yaw, &st->pitch, psm_axis_(pad->sx), psm_axis_(pad->sy), st->invert_y, fwd, side, dt, lim);
    st->split = st->readout == 2;
    st->last = held;
}

/* ---- frame ------------------------------------------------------------------- */

/* The placement's options, a bit each (psm_frame_t.place_opt). The picture is the same with any of them: what they leave
   out is off the screen or has no area. What changes is the SH-4's work when the camera moves and the vertices sent.
     AREA    the area test of a cell's and a model's triangles from the 1 / W the placement already stored: no reciprocal
             a triangle, and the finite check once a vertex, not nine times a triangle (sh4der_tri_area_ok does both).
             On the console the stored 1 / W is FSRRA's and the guard's is a divide: a triangle within rounding of the
             area limit (0.01 square pixel) can fall on the other side. On a host the two are the same number.
     BLOCKS  the static models in blocks of whole strips with a box each: a block whose box is outside the view (behind
             the near plane, past one side of the screen, or wholly past DRAW DISTANCE) is not placed, tested or sent.
     SIDE    a model triangle whose three corners are past one side of the screen is not sent (the ground's cells
             always had this test: psm_cell_seen_). */
#define PSM_PLACE_AREA   1u
#define PSM_PLACE_BLOCKS 2u
#define PSM_PLACE_SIDE   4u
#define PSM_MB_TRIS      48u          /* a block closes at the first strip end at or past this many triangles */
#define PSM_MB_MAX       320u

typedef struct psm_mblock {
    uint16_t t0, tn;                  /* its triangles */
    float lo[3], hi[3];               /* the box of their vertices */
} psm_mblock_t;

typedef struct psm_frame {
    sh4der_psm_cam_t cam;
    float sun[3];
    sh4der_psm_t map;
    int built;                  /* 0: no map this frame, everything is lit */
    int head_on;                /* the warp was asked for and had no fit: the uniform map stands in */
    unsigned size;
    float ratio[4];             /* ground a texel covers over ground a pixel covers at 4, 10, 25, 60 m; under 0 for none */
    float shadow_grey;          /* what a shadowed ground texel is multiplied by on route A: the ambient share (green) */
    float shadow_rgb[3];        /* and by channel, for a tinted light */
    float slab;                 /* the fit's receiver slab this frame (the scene's, by route) */
    unsigned grows, gcols;      /* rings and spokes this frame */
    unsigned gvn;
    float rows[16];             /* the camera's world-to-clip rows at the render's near plane (zero for a bad camera) */
    int gpos_ok, gpos_k0;       /* the ground positions exist, for the spoke window that starts at gpos_k0 */
    int gpos_ix, gpos_iz;       /* round the centre (ix, iz) x PSM_SNAP */
    unsigned gpos_rows, gpos_cols;   /* rings and spokes the positions were made for */
    float gc[2];                /* this frame's grid centre, x and z */
    unsigned gpos_gen;          /* counts the position recomputes: the placement is keyed on it */
    float gpos[PSM_GV_MAX][3];
    uint8_t glit[PSM_GV_MAX];   /* route B: 255 lit .. 0 shadowed */
    float guv[PSM_GV_MAX][2];   /* route A: the map target's coordinates */
    uint8_t gok[PSM_GV_MAX];    /* route A: the vertex has coordinates on the fit's side of the light's singularity */
    uint8_t mlit[PSM_MV_MAX];   /* the sun's share at a model vertex, 0 .. 255: N.L, times the shadow on route B */
    float mh[PSM_CV_MAX][4];    /* route B's scratch: the casters in the map's homogeneous form (X, Y, K, W) */
    float ma[PSM_CV_MAX][4];    /* route A: the casters in the map's homogeneous form (X, Y, ., W), once a frame */
    uint8_t con[PSM_CO_MAX];    /* 1: the caster object's box touches the map's square (psm_frame_cull_) */
    sh4der_psm_hold_t hold;     /* MAP HOLD: what the held fit keeps between frames */
    unsigned hold_key;          /* the size, map, route and scene it was held for (another: let go) */
    unsigned cull_objs, cull_tris;   /* objects and their triangles left out by it this frame */
    uint8_t mvis[PSM_MV_MAX];   /* 1: the model vertex is a corner of a triangle that is sent (psm_place_geo) */
    int vis_ok;                 /* mvis is this frame's: route B compares those vertices only */
    /* the placement (psm_place): on the screen, the cells and the model triangles with area, and the colours */
    int place_ok;
    unsigned place_gen;         /* gpos_gen the placement was made for */
    float place_rows[16];       /* and the camera rows */
    float place_dist;           /* and the draw distance */
    float dist;                 /* DRAW DISTANCE this frame, metres of view depth; 0: INFINITE (no cut) */
    float gclip[PSM_GV_MAX][3]; /* X, Y, W */
    float gscr[PSM_GV_MAX][3];  /* x, y, 1 / W */
    uint32_t gcol[PSM_GV_MAX];
    uint32_t glut[256];         /* the ground's colour by its shadow level */
    uint8_t cell[PSM_ROWS_MAX * PSM_COLS];   /* 1: both triangles of the cell have area, every corner is in front, and it is not wholly off one side of the screen */
    float mclip[PSM_MV_MAX][3];
    float mscr[PSM_MV_MAX][3];
    uint32_t mcol[PSM_MV_MAX];
    uint8_t mtri[PSM_MT_MAX];   /* 1: the triangle is in front and has screen area; 2: it crosses the near plane (psm_clip_near cuts it) */
    sh4der_psm_raster_t raster; /* route B's map */
    unsigned tris, drawn;       /* caster triangles given to the map and drawn into it (B: rasterised; A: sent to the target, the program counts) */
    /* the placement's options (PSM_PLACE_*), at the end so no field above moves. 0: the placement as it always was */
    unsigned place_opt;         /* this frame's options */
    unsigned place_opt_done;    /* the options the kept placement was made with */
    uint8_t gbad[PSM_GV_MAX];   /* PSM_PLACE_AREA: 1 when the ground vertex's clip X, Y or W is not finite */
    uint8_t mbad[PSM_MV_MAX];   /* and the model vertex's */
    uint8_t mplaced[PSM_MV_MAX];   /* PSM_PLACE_BLOCKS: 1 when the static vertex was placed in this placement */
    psm_mblock_t mb[PSM_MB_MAX];   /* the static model triangles in runs of whole strips, each with its box (made once a scene) */
    unsigned mbn, mb_tdyn0, mb_mdyn0;
    int mb_town;
    unsigned mb_culled, mb_culled_tris;   /* blocks and their triangles the last placement left out */
    /* the colours' options (PSM_COLOUR_*). 0: every model vertex's colour worked out every frame, as it always was */
    unsigned colour_opt;
    int mlut_ok;                /* mlut is made for mlut_key and mlut_pal */
    unsigned mlut_n;            /* its rows */
    float mlut_key[6];          /* the light it was made for: amb, sunc */
    float mlut_pal[PSM_PAL_MAX][3];        /* and the palette */
    uint32_t mlut[PSM_PAL_MAX][256];       /* PSM_COLOUR_TABLE: a model vertex's colour by its palette entry and its sun share */
    /* the baked occlusion (psm_ao.h's psm_ao_frame sets these). mao NULL and gao_on 0: the colours as they always were */
    const uint8_t *mao;         /* a byte a model vertex, 255 open .. 0 closed; it multiplies the ambient light */
    int gao_on;                 /* gao holds this frame's ground vertices' bytes */
    unsigned gao_gen, gao_n;    /* gpos_gen and gvn they were read for */
    uint8_t gao[PSM_GV_MAX];
    uint32_t glut_a[256], glut_s[256];     /* the ground's ambient part by its byte, its sun part by its shadow level */
    uint32_t mlut_a[PSM_PAL_MAX][256];     /* PSM_COLOUR_TABLE with mao: a model vertex's ambient part by its palette entry and its byte */
    uint32_t mlut_s[PSM_PAL_MAX][256];     /* and its sun part by its palette entry and its sun share */
} psm_frame_t;

static const float psm_probe_m[4] = { 4.0f, 10.0f, 25.0f, 60.0f };

/* The scene's walk: the area and the circles round the poles and the cows (the town: the well and the cross). */
static inline void psm_scene_limits(const psm_scene_t *s, psm_walk_limits_t *lim) {
    lim->x0 = s->x0; lim->x1 = s->x1; lim->z0 = s->z0; lim->z1 = s->z1;
    lim->circle = (const float (*)[3])s->obst; lim->n = s->obn;
}

/* The spokes the window needs either side of the heading at this pitch (positive down). The widest ground the screen shows is
   at its lower corners (looking down) or at its horizon's ends (looking up): tan t = tan_half x aspect / the ray's forward part
   on the ground. The eye is up to PSM_SNAP_OFF from the grid's centre, which turns that angle by up to about off / r at the
   corner's distance r; one spoke more for the window's rounding to a whole spoke. */
static inline unsigned psm_window_half(float pitch) {
    const float side = PSM_TAN_HALF * PSM_ASPECT;
    float sp, cp, den, drop, t, r2, tm, tt, sn, cs;
    unsigned k;

    sh4der_sincos_(pitch, &sp, &cp);
    den = cp - PSM_TAN_HALF * sp;                               /* the lower corner ray: its forward part, its drop */
    drop = sp + PSM_TAN_HALF * cp;
    if (cp > 0.05f && sh4der_recip_pos_(cp) < den) den = sh4der_recip_pos_(cp);   /* the horizon's ray, when it is narrower */
    if (!(den > 0.05f)) return PSM_COLS / 2u;
    if (drop < 0.05f) drop = 0.05f;
    t = side * sh4der_recip_pos_(den);
    r2 = (PSM_EYE_H * PSM_EYE_H) * sh4der_recip_pos_(drop * drop) * (den * den + side * side);   /* the corner's distance, squared */
    tm = PSM_SNAP_OFF * sh4der_recip_pos_(sh4der_sqrt_(r2));
    if (!(t * tm < 0.95f)) return PSM_COLS / 2u;
    tt = (t + tm) * sh4der_recip_pos_(1.0f - t * tm);           /* tan of the two angles added */
    for (k = 1u; k < PSM_COLS / 2u; k++) {
        sh4der_sincos_((float)k * PSM_COL_STEP, &sn, &cs);
        if (sn >= tt * cs) break;
    }
    k++;
    return k < PSM_COLS / 2u ? k : PSM_COLS / 2u;
}

/* The camera, the sun, this frame's ground vertices, the map (the warp, or the uniform one) and the texel-over-pixel line. */
static inline void psm_frame_setup(psm_frame_t *f, const psm_scene_t *s, const psm_state_t *st) {
    const float up[3] = { 0.0f, 1.0f, 0.0f }, gx[3] = { 1.0f, 0.0f, 0.0f }, gz[3] = { 0.0f, 0.0f, 1.0f };
    float sy, cy, sp, cp, sa, ca, se, ce, corners[8][3], pts[32][3];
    unsigned n, r, c;
    int k0;

    unsigned half;
    int ix, iz;

    sh4der_sincos_(st->yaw, &sy, &cy);
    sh4der_sincos_(st->pitch, &sp, &cp);
    memset(&f->cam, 0, sizeof f->cam);
    f->cam.eye[0] = st->x; f->cam.eye[1] = PSM_EYE_H; f->cam.eye[2] = st->z;
    f->cam.fwd[0] = sy * cp; f->cam.fwd[1] = -sp; f->cam.fwd[2] = -cy * cp;
    f->cam.up[0] = sy * sp; f->cam.up[1] = cp; f->cam.up[2] = -cy * sp;
    f->cam.tan_half = PSM_TAN_HALF; f->cam.aspect = PSM_ASPECT;
    f->cam.znear = PSM_NEAR; f->cam.zfar = PSM_FAR;
    if (!sh4der_psm_cam_rows(&f->cam, PSM_NEAR, f->rows)) memset(f->rows, 0, sizeof f->rows);

    sh4der_sincos_(st->az * 0.0174533f, &sa, &ca);
    sh4der_sincos_(st->el * 0.0174533f, &se, &ce);
    f->sun[0] = sa * ce; f->sun[1] = se; f->sun[2] = -ca * ce;
    for (c = 0; c < 3u; c++) f->shadow_rgb[c] = sh4der_div_(s->amb[c], s->amb[c] + s->sunc[c] * f->sun[1]);
    f->shadow_grey = f->shadow_rgb[1];
    f->vis_ok = 0;

    /* the spokes the view covers round the eye's lattice point: the centre is a whole lattice step and the window starts on
       a whole spoke, so a vertex is the same point every frame. The positions change only when the window moves by a spoke,
       the centre by a lattice step or the pitch asks for a wider or narrower window; a still camera keeps them (and the
       placement on them). */
    ix = psm_snap_(st->x); iz = psm_snap_(st->z);
    half = psm_window_half(st->pitch);
    k0 = (int)(st->yaw * (1.0f / PSM_COL_STEP) + 0.5f) - (int)half;
    f->grows = s->rows; f->gcols = 2u * half + 1u;
    f->gvn = f->grows * f->gcols;
    f->gc[0] = (float)ix * PSM_SNAP; f->gc[1] = (float)iz * PSM_SNAP;
    if (!f->gpos_ok || f->gpos_k0 != k0 || f->gpos_rows != f->grows || f->gpos_cols != f->gcols || f->gpos_ix != ix || f->gpos_iz != iz) {
        for (c = 0; c < f->gcols; c++) {
            float sn, cs;

            sh4der_sincos_((float)(k0 + (int)c) * PSM_COL_STEP, &sn, &cs);
            for (r = 0; r < f->grows; r++) {
                float *p = f->gpos[r * f->gcols + c];

                p[0] = f->gc[0] + s->row_r[r] * sn; p[1] = 0.0f; p[2] = f->gc[1] - s->row_r[r] * cs;
            }
        }
        f->gpos_ok = 1; f->gpos_k0 = k0; f->gpos_rows = f->grows; f->gpos_cols = f->gcols; f->gpos_ix = ix; f->gpos_iz = iz; f->gpos_gen++;
    }

    f->size = psm_map_size(st);
    f->built = 0; f->head_on = 0;
    n = 0;
    f->slab = s->slab[st->route != PSM_ROUTE_A ? 1 : 0];          /* route B's slab for AB too: it compares the models */
    if (sh4der_psm_frustum(&f->cam, PSM_FIT_NEAR, PSM_FAR, corners))
        n = sh4der_psm_slab_pts((const float (*)[3])corners, up, 0.0f, f->slab, pts);
    {
        const unsigned key = 1u | (f->size << 1) | ((unsigned)(st->warp != 0) << 12) | ((unsigned)st->route << 13) | ((unsigned)(st->scene != 0) << 16)
                           | ((unsigned)(st->hold != 0) << 17);

        if (key != f->hold_key) { memset(&f->hold, 0, sizeof f->hold); f->hold_key = key; }
    }
    if (n) {
        if (st->warp) {
            f->built = st->hold ? sh4der_psm_build_warp_held(&f->map, &f->cam, st->warp_near, f->sun, &pts[0][0], n, f->size, &f->hold,
                                                             PSM_HOLD_POS, PSM_HOLD_DIR_COS, PSM_HOLD_LIGHT_COS)
                                : sh4der_psm_build_warp(&f->map, &f->cam, st->warp_near, f->sun, &pts[0][0], n);
            if (!f->built) f->head_on = 1;
        }
        if (!f->built) f->built = st->hold ? sh4der_psm_build_uniform_held(&f->map, f->sun, &pts[0][0], n, f->size, &f->hold, PSM_HOLD_LIGHT_COS)
                                           : sh4der_psm_build_uniform(&f->map, f->sun, &pts[0][0], n);
    }
    for (c = 0; c < 4u; c++) {
        const float p[3] = { st->x + sy * psm_probe_m[c], 0.0f, st->z - cy * psm_probe_m[c] };
        float t, px;

        f->ratio[c] = -1.0f;
        if (!f->built) continue;
        t = sh4der_psm_texel_m(&f->map, f->size, p, gx, gz, 0.05f);
        px = sh4der_psm_pixel_m(&f->cam, 480u, p, up);
        if (t < 1e29f && px > 0.0f) f->ratio[c] = sh4der_div_(t, px);
    }
    f->dist = psm_dist(st);
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

    if (!f->built || !sh4der_psm_raster_key_range(f->map.light, &s->cv[0][0], 3u * sizeof(float), s->cvn, &lo, &hi)) return 0;
    return sh4der_psm_raster_begin(&f->raster, &f->map, depth, f->size, lo - 0.25f, hi + 0.25f);
}

/* 1 when caster triangle i goes into the map: every one, or a mover's only (movers_only). The receivers do not change: with
   the static casters off, nothing shades a mover but the movers. */
static inline int psm_caster_on(const psm_scene_t *s, unsigned i, int movers_only) { return !movers_only || s->ctmov[i]; }

/* 1 when the box lo .. hi can touch the map: 0 only when its eight corners are all in front of the light's singularity
   (W > 0) and all past one side of the map's square. W is linear, so the whole box then has W > 0 and maps into the hull
   of its corners' places: nothing of it is in the square, and no caster in it draws a texel. A box with a corner at or
   behind the singularity is kept (route B draws both signs of W). */
static inline int psm_box_in_map(const sh4der_psm_t *map, const float lo[3], const float hi[3]) {
    int lx = 1, hx = 1, ly = 1, hy = 1;
    unsigned c;

    for (c = 0; c < 8u; c++) {
        const float p[3] = { (c & 1u) ? hi[0] : lo[0], (c & 2u) ? hi[1] : lo[1], (c & 4u) ? hi[2] : lo[2] };
        float h[4];

        sh4der_psm_xform(map->m, p, 1.0f, h);
        if (!sh4der_finite_(h[3]) || !(h[3] > SH4DER_PSM_TRI_EPS)) return 1;
        lx &= h[0] < -1.001f * h[3]; hx &= h[0] > 1.001f * h[3];
        ly &= h[1] < -1.001f * h[3]; hy &= h[1] > 1.001f * h[3];
    }
    return !(lx || hx || ly || hy);
}

/* This frame's caster objects against the map's own square, not the view: a caster out of view toward the sun stays. */
static inline void psm_frame_cull_(psm_frame_t *f, const psm_scene_t *s) {
    unsigned k;

    f->cull_objs = f->cull_tris = 0u;
    for (k = 0; k < s->con; k++) {
        f->con[k] = (uint8_t)(f->built && psm_box_in_map(&f->map, s->co[k].lo, s->co[k].hi));
        if (!f->con[k]) { f->cull_objs++; f->cull_tris += s->co[k].tn; }
    }
}

/* Route B, step 2: the casters into the map. Each vertex is transformed once; an object off the map's square is not
   rasterised. tris counts the casters CASTERS gives (culled or not), drawn those that drew a texel. */
static inline void psm_frame_b_casters(psm_frame_t *f, const psm_scene_t *s, int movers_only) {
    psm_xf_t xf;
    unsigned i, k, given = 0;

    psm_frame_cull_(f, s);
    psm_xf_load(&xf, f->raster.m);                   /* the map's rows with the depth column: (X, Y, K, W) */
    for (i = 0; i < s->cvn; i++) psm_xf_apply(&xf, s->cv[i], f->mh[i]);
    for (k = 0; k < s->con; k++)
        for (i = s->co[k].t0; i < s->co[k].t0 + s->co[k].tn; i++)
            if (psm_caster_on(s, i, movers_only)) {
                given++;
                if (f->con[k]) sh4der_psm_raster_tri_h(&f->raster, f->mh[s->ct[i][0]], f->mh[s->ct[i][1]], f->mh[s->ct[i][2]]);
            }
    f->tris = given; f->drawn = f->raster.drawn;
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

        if (f->vis_ok && !f->mvis[i]) continue;          /* not sent this frame: no compare (psm_place_geo ran first) */
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
        iw = sh4der_recip_pos_(h[3]);   /* h[3] > TRI_EPS (the guard above) */
        f->guv[i][0] = 0.5f * (h[0] * iw) + 0.5f;
        f->guv[i][1] = 0.5f * (h[1] * iw) + 0.5f;
        f->gok[i] = 1u;
    }
}

/* Route A: the casters in the map's homogeneous form, once a frame (psm_pvr_tris_h draws them into the target), and
   the objects against the map's square (f->con). */
static inline void psm_frame_a_casters(psm_frame_t *f, const psm_scene_t *s) {
    psm_xf_t xf;
    unsigned i;

    psm_frame_cull_(f, s);
    if (!f->built) return;
    psm_xf_load(&xf, f->map.m);
    for (i = 0; i < s->cvn; i++) psm_xf_apply(&xf, s->cv[i], f->ma[i]);
}

/* The colour scale of a surface: the ambient share plus the sun's, `direct` 0 .. 1 (the field's grey light). */
static inline float psm_light(float direct) { return PSM_AMBIENT + (1.0f - PSM_AMBIENT) * direct; }

/* channel c of the scene's light */
static inline float psm_light_c(const psm_scene_t *s, unsigned c, float direct) { return s->amb[c] + s->sunc[c] * direct; }

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
    scr[0] = sh4der_fma_(h[0] * iw, 0.5f * PSM_SCR_W, 0.5f * PSM_SCR_W);   /* the viewport: one multiply-add an axis */
    scr[1] = sh4der_fma_(h[1] * iw, -0.5f * PSM_SCR_H, 0.5f * PSM_SCR_H);
    scr[2] = iw;
    return 1;
}

/* 0 when the four corners (clip X, Y, W) are all past one side of the screen: the cell is not sent. */
static inline int psm_cell_seen_(const float a[3], const float b[3], const float d[3], const float e[3]) {
    if (a[0] < -a[2] && b[0] < -b[2] && d[0] < -d[2] && e[0] < -e[2]) return 0;
    if (a[0] > a[2] && b[0] > b[2] && d[0] > d[2] && e[0] > e[2]) return 0;
    if (a[1] < -a[2] && b[1] < -b[2] && d[1] < -d[2] && e[1] < -e[2]) return 0;
    if (a[1] > a[2] && b[1] > b[2] && d[1] > d[2] && e[1] > e[2]) return 0;
    return 1;
}

/* PSM_PLACE_AREA: sh4der_tri_area_ok's answer for three corners in front of the near plane, from their clip X, Y and the
   1 / W the placement stored (ia, ib, ic) and their not-finite marks: the same sums in the same order, no reciprocal. */
static inline int psm_area_ok_(const float a[3], const float b[3], const float c[3], float ia, float ib, float ic, unsigned bad) {
    const float ex = b[0] * ib - a[0] * ia, ey = b[1] * ib - a[1] * ia;
    const float fx = c[0] * ic - a[0] * ia, fy = c[1] * ic - a[1] * ia;

    if (bad) return 0;
    return sh4der_abs_(ex * fy - ey * fx) >= SH4DER_GUARD_MIN_AREA2;
}

/* A point's place as psm_place_pt_, and 1 when its clip X, Y or W is not finite (PSM_PLACE_AREA's mark). */
static inline uint8_t psm_place_pt_bad_(const psm_xf_t *xf, const float p[3], float clip[3], float scr[3]) {
    psm_place_pt_(xf, p, clip, scr);
    return (uint8_t)(!sh4der_finite_(clip[0]) || !sh4der_finite_(clip[1]) || !sh4der_finite_(clip[2]));
}

/* model triangle i's flag from its corners' places (and the draw distance) */
static inline void psm_place_tri_(psm_frame_t *f, const psm_scene_t *s, unsigned i) {
    const unsigned a = s->mt[i][0], b = s->mt[i][1], k = s->mt[i][2];
    const unsigned infront = (f->mclip[a][2] > PSM_NEAR) + (f->mclip[b][2] > PSM_NEAR) + (f->mclip[k][2] > PSM_NEAR);

    if (infront == 3u && (f->place_opt & PSM_PLACE_AREA))
        f->mtri[i] = (uint8_t)psm_area_ok_(f->mclip[a], f->mclip[b], f->mclip[k], f->mscr[a][2], f->mscr[b][2], f->mscr[k][2],
                                           (unsigned)f->mbad[a] | f->mbad[b] | f->mbad[k]);
    else
        f->mtri[i] = (uint8_t)(infront == 3u ? sh4der_tri_area_ok(f->mclip[a], f->mclip[b], f->mclip[k], PSM_NEAR, SH4DER_GUARD_MIN_AREA2) : infront ? 2 : 0);
    if (f->dist > 0.0f && f->mclip[a][2] > f->dist && f->mclip[b][2] > f->dist && f->mclip[k][2] > f->dist) f->mtri[i] = 0u;
    if ((f->place_opt & PSM_PLACE_SIDE) && f->mtri[i] && !psm_cell_seen_(f->mclip[a], f->mclip[b], f->mclip[k], f->mclip[k])) f->mtri[i] = 0u;
}

/* PSM_PLACE_BLOCKS: the static model triangles in blocks, once a scene: whole strips, in the mesh's order (a kit piece's
   strips follow each other), a block closed at the first strip end at or past PSM_MB_TRIS triangles; each with the box of
   its vertices. The static vertices never move, so the boxes stand. */
static inline void psm_blocks_fit_(psm_frame_t *f, const psm_scene_t *s) {
    unsigned i, k, c;

    if (f->mbn && f->mb_tdyn0 == s->tdyn0 && f->mb_mdyn0 == s->mdyn0 && f->mb_town == s->town) return;
    f->mbn = 0u; f->mb_tdyn0 = s->tdyn0; f->mb_mdyn0 = s->mdyn0; f->mb_town = s->town;
    for (i = 0; i < s->tdyn0; i++) {
        psm_mblock_t *b;

        if (!f->mbn || (s->mstart[i] && f->mb[f->mbn - 1u].tn >= PSM_MB_TRIS && f->mbn < PSM_MB_MAX)) {
            b = &f->mb[f->mbn++];
            b->t0 = (uint16_t)i; b->tn = 0u;
            for (c = 0; c < 3u; c++) { b->lo[c] = 1.0e30f; b->hi[c] = -1.0e30f; }
        }
        b = &f->mb[f->mbn - 1u];
        b->tn++;
        for (k = 0; k < 3u; k++)
            for (c = 0; c < 3u; c++) {
                const float v = s->mv[s->mt[i][k]][c];

                if (v < b->lo[c]) b->lo[c] = v;
                if (v > b->hi[c]) b->hi[c] = v;
            }
    }
}

/* 0 when the box is wholly outside the view: its eight corners all behind the near plane, all past one side of the
   screen, or (dist > 0) all past the draw distance. Each test is linear in the clip coordinates, so it holds for every
   point of the box: no triangle in it could be sent, or seen. A corner that is not finite keeps the box. */
static inline int psm_block_seen_(const psm_xf_t *xf, const psm_mblock_t *b, float dist) {
    unsigned c, behind = 1u, left = 1u, right = 1u, below = 1u, above = 1u, far = dist > 0.0f;

    for (c = 0; c < 8u; c++) {
        const float p[3] = { (c & 1u) ? b->hi[0] : b->lo[0], (c & 2u) ? b->hi[1] : b->lo[1], (c & 4u) ? b->hi[2] : b->lo[2] };
        float h[4];

        psm_xf_apply(xf, p, h);
        if (!sh4der_finite_(h[0]) || !sh4der_finite_(h[1]) || !sh4der_finite_(h[3])) return 1;
        behind &= !(h[3] > PSM_NEAR);
        left &= h[0] < -h[3]; right &= h[0] > h[3];
        below &= h[1] < -h[3]; above &= h[1] > h[3];
        far &= h[3] > dist;
    }
    return !(behind || left || right || below || above || far);
}

/* psm_place_geo's work when the camera moved, with the options of f->place_opt (not 0). The same clip and screen positions,
   cells and triangle flags as the plain placement for everything that can reach the screen. */
static inline void psm_place_geo_opt_(psm_frame_t *f, const psm_scene_t *s) {
    const unsigned opt = f->place_opt;
    psm_xf_t xf;
    unsigned i, r, c, k;

    psm_xf_load(&xf, f->rows);
    f->mb_culled = f->mb_culled_tris = 0u;
    if (opt & PSM_PLACE_AREA) for (i = 0; i < f->gvn; i++) f->gbad[i] = psm_place_pt_bad_(&xf, f->gpos[i], f->gclip[i], f->gscr[i]);
    else for (i = 0; i < f->gvn; i++) psm_place_pt_(&xf, f->gpos[i], f->gclip[i], f->gscr[i]);
    for (r = 0; r + 1u < f->grows; r++)
        for (c = 0; c + 1u < f->gcols; c++) {
            const unsigned a = r * f->gcols + c, b = a + 1u, d = a + f->gcols, e = d + 1u;
            const int front = f->gclip[a][2] > PSM_NEAR && f->gclip[b][2] > PSM_NEAR && f->gclip[d][2] > PSM_NEAR && f->gclip[e][2] > PSM_NEAR;
            int ok = front && psm_cell_seen_(f->gclip[a], f->gclip[b], f->gclip[d], f->gclip[e]);

            if (ok && (opt & PSM_PLACE_AREA))
                ok = psm_area_ok_(f->gclip[d], f->gclip[a], f->gclip[e], f->gscr[d][2], f->gscr[a][2], f->gscr[e][2], (unsigned)f->gbad[d] | f->gbad[a] | f->gbad[e])
                  && psm_area_ok_(f->gclip[a], f->gclip[e], f->gclip[b], f->gscr[a][2], f->gscr[e][2], f->gscr[b][2], (unsigned)f->gbad[a] | f->gbad[e] | f->gbad[b]);
            else if (ok)
                ok = sh4der_tri_area_ok(f->gclip[d], f->gclip[a], f->gclip[e], PSM_NEAR, SH4DER_GUARD_MIN_AREA2)
                  && sh4der_tri_area_ok(f->gclip[a], f->gclip[e], f->gclip[b], PSM_NEAR, SH4DER_GUARD_MIN_AREA2);
            if (ok && f->dist > 0.0f && f->gclip[a][2] > f->dist && f->gclip[b][2] > f->dist && f->gclip[d][2] > f->dist && f->gclip[e][2] > f->dist) ok = 0;
            f->cell[r * PSM_COLS + c] = (uint8_t)ok;
        }
    if (opt & PSM_PLACE_BLOCKS) {              /* the static models a block at a time; a block out of view costs eight points */
        psm_blocks_fit_(f, s);
        memset(f->mplaced, 0, s->mdyn0);
        for (k = 0; k < f->mbn; k++) {
            const psm_mblock_t *b = &f->mb[k];

            if (!psm_block_seen_(&xf, b, f->dist)) {
                memset(f->mtri + b->t0, 0, b->tn);
                f->mb_culled++; f->mb_culled_tris += b->tn;
                continue;
            }
            for (i = b->t0; i < (unsigned)b->t0 + b->tn; i++)
                for (c = s->mstart[i] ? 0u : 2u; c < 3u; c++) {      /* a strip's later triangles bring one new vertex each */
                    const unsigned v = s->mt[i][c];

                    if (f->mplaced[v]) continue;
                    f->mplaced[v] = 1u;
                    f->mbad[v] = psm_place_pt_bad_(&xf, s->mv[v], f->mclip[v], f->mscr[v]);
                }
            for (i = b->t0; i < (unsigned)b->t0 + b->tn; i++) psm_place_tri_(f, s, i);
        }
        for (i = s->mdyn0; i < s->mvn; i++) f->mbad[i] = psm_place_pt_bad_(&xf, s->mv[i], f->mclip[i], f->mscr[i]);
        for (i = s->tdyn0; i < s->mtn; i++) psm_place_tri_(f, s, i);
    } else {
        for (i = 0; i < s->mvn; i++) f->mbad[i] = psm_place_pt_bad_(&xf, s->mv[i], f->mclip[i], f->mscr[i]);
        for (i = 0; i < s->mtn; i++) psm_place_tri_(f, s, i);
    }
    f->place_ok = 1; f->place_gen = f->gpos_gen; memcpy(f->place_rows, f->rows, sizeof f->rows); f->place_dist = f->dist;
    f->place_opt_done = opt;
}

/* This frame's place on the screen. The ground and the models go through the camera's rows once each: clip and screen
   positions, the cells whose two strip triangles have area, the models' triangles with area. That depends on the camera
   and the ground positions only, so a still camera keeps it; the movers' part is made every frame. mvis marks the model
   vertices that are sent (route B compares those only). */
static inline void psm_place_geo(psm_frame_t *f, const psm_scene_t *s) {
    psm_xf_t xf;
    unsigned i, r, c;

    if (f->place_opt && (!f->place_ok || f->place_gen != f->gpos_gen || memcmp(f->place_rows, f->rows, sizeof f->rows) != 0
                         || f->place_dist != f->dist || f->place_opt_done != f->place_opt)) {
        psm_place_geo_opt_(f, s);              /* the same placement by the options' ways */
    } else if (!f->place_ok || f->place_gen != f->gpos_gen || memcmp(f->place_rows, f->rows, sizeof f->rows) != 0 || f->place_dist != f->dist
               || f->place_opt_done != f->place_opt) {
        f->place_opt_done = 0u; f->mb_culled = f->mb_culled_tris = 0u;
        psm_xf_load(&xf, f->rows);
        for (i = 0; i < f->gvn; i++) psm_place_pt_(&xf, f->gpos[i], f->gclip[i], f->gscr[i]);
        for (i = 0; i < s->mvn; i++) psm_place_pt_(&xf, s->mv[i], f->mclip[i], f->mscr[i]);
        /* a strip sends, for the cell (ring r, spoke c), the triangles (d, a, e) and (a, e, b): a = (r, c), b = (r, c + 1), d = (r + 1, c), e = (r + 1, c + 1) */
        for (r = 0; r + 1u < f->grows; r++)
            for (c = 0; c + 1u < f->gcols; c++) {
                const unsigned a = r * f->gcols + c, b = a + 1u, d = a + f->gcols, e = d + 1u;
                const int front = f->gclip[a][2] > PSM_NEAR && f->gclip[b][2] > PSM_NEAR && f->gclip[d][2] > PSM_NEAR && f->gclip[e][2] > PSM_NEAR;

                f->cell[r * PSM_COLS + c] = (uint8_t)(front && psm_cell_seen_(f->gclip[a], f->gclip[b], f->gclip[d], f->gclip[e])
                    && sh4der_tri_area_ok(f->gclip[d], f->gclip[a], f->gclip[e], PSM_NEAR, SH4DER_GUARD_MIN_AREA2)
                    && sh4der_tri_area_ok(f->gclip[a], f->gclip[e], f->gclip[b], PSM_NEAR, SH4DER_GUARD_MIN_AREA2));
            }
        for (i = 0; i < s->mtn; i++) psm_place_tri_(f, s, i);    /* DRAW DISTANCE: what is wholly past it in view depth is not sent */
        if (f->dist > 0.0f)
            for (r = 0; r + 1u < f->grows; r++)
                for (c = 0; c + 1u < f->gcols; c++) {
                    const unsigned a = r * f->gcols + c, b = a + 1u, d = a + f->gcols, e = d + 1u;

                    if (f->gclip[a][2] > f->dist && f->gclip[b][2] > f->dist && f->gclip[d][2] > f->dist && f->gclip[e][2] > f->dist)
                        f->cell[r * PSM_COLS + c] = 0u;
                }
        f->place_ok = 1; f->place_gen = f->gpos_gen; memcpy(f->place_rows, f->rows, sizeof f->rows); f->place_dist = f->dist;
    } else if (s->mdyn0 < s->mvn) {                              /* the movers: every frame */
        psm_xf_load(&xf, f->rows);
        if (f->place_opt & PSM_PLACE_AREA) for (i = s->mdyn0; i < s->mvn; i++) f->mbad[i] = psm_place_pt_bad_(&xf, s->mv[i], f->mclip[i], f->mscr[i]);
        else for (i = s->mdyn0; i < s->mvn; i++) psm_place_pt_(&xf, s->mv[i], f->mclip[i], f->mscr[i]);
        for (i = s->tdyn0; i < s->mtn; i++) psm_place_tri_(f, s, i);
    }
    memset(f->mvis, 0, s->mvn);
    for (i = 0; i < s->mtn; i++)
        if (f->mtri[i]) f->mvis[s->mt[i][0]] = f->mvis[s->mt[i][1]] = f->mvis[s->mt[i][2]] = 1u;
    f->vis_ok = 1;
}

/* The colours' options, psm_frame_t.colour_opt (0: as it always was).
   PSM_COLOUR_TABLE: a model vertex's colour is read from a table, a row a palette entry and a column a sun share: the
   same expression as the per-vertex one, worked out 256 times a palette entry when the light or the palette changes
   (the scene's amb, sunc and pal are compared every frame: under 1 KB) and not at all on the frames between. 64 KB of
   RAM in psm_frame_t; the town's palette uses a part of it.
   PSM_COLOUR_SEEN: only the vertices psm_place_geo marked as sent (mvis) get a colour; the others keep an old one,
   which nothing reads. Call psm_place_geo before psm_colour, as the frame does. */
#define PSM_COLOUR_TABLE 1u
#define PSM_COLOUR_SEEN  2u

static inline void psm_colour_table_(psm_frame_t *f, const psm_scene_t *s) {
    const unsigned n = s->paln < PSM_PAL_MAX ? (s->paln ? s->paln : 1u) : PSM_PAL_MAX;
    unsigned k, r;

    if (f->mlut_ok && f->mlut_n == n && memcmp(f->mlut_key, s->amb, sizeof s->amb) == 0 && memcmp(f->mlut_key + 3, s->sunc, sizeof s->sunc) == 0
        && memcmp(f->mlut_pal, s->pal, n * sizeof s->pal[0]) == 0) return;
    for (k = 0; k < n; k++) {
        const float *alb = s->pal[k];

        for (r = 0; r < 256u; r++) {
            const float d = (float)r * (1.0f / 255.0f);

            f->mlut[k][r] = psm_pack_rgb(alb[0] * psm_light_c(s, 0u, d), alb[1] * psm_light_c(s, 1u, d), alb[2] * psm_light_c(s, 2u, d));
            f->mlut_a[k][r] = psm_pack_rgb(alb[0] * s->amb[0] * d, alb[1] * s->amb[1] * d, alb[2] * s->amb[2] * d);
            f->mlut_s[k][r] = psm_pack_rgb(alb[0] * s->sunc[0] * d, alb[1] * s->sunc[1] * d, alb[2] * s->sunc[2] * d);
        }
    }
    memcpy(f->mlut_key, s->amb, sizeof s->amb); memcpy(f->mlut_key + 3, s->sunc, sizeof s->sunc);
    memcpy(f->mlut_pal, s->pal, n * sizeof s->pal[0]);
    f->mlut_n = n; f->mlut_ok = 1;
}

/* Two packed colours added a channel, each stopping at 255. For the occlusion's table form: colour = the ambient part
   by the byte + the sun part by the sun's share. */
static inline uint32_t psm_rgb_add_(uint32_t a, uint32_t b) {
    uint32_t rb = (a & 0x00ff00ffu) + (b & 0x00ff00ffu), g = (a & 0x0000ff00u) + (b & 0x0000ff00u);

    rb |= ((rb >> 8) & 0x00010001u) * 0xffu;
    g |= ((g >> 8) & 0x00000100u) * 0xffu;
    return 0xff000000u | (rb & 0x00ff00ffu) | (g & 0x0000ff00u);
}

/* This frame's colours: they follow the sun and the shadows, so they are made every frame, the ground's from a 256 level
   table. The light by channel (the field's is grey). With the baked occlusion (f->mao, f->gao_on: psm_ao.h) the ambient
   part is multiplied by the vertex's byte: colour = albedo x (ambient x ao + sun x direct). */
static inline void psm_colour(psm_frame_t *f, const psm_scene_t *s) {
    const int seen = (f->colour_opt & PSM_COLOUR_SEEN) && f->vis_ok;
    unsigned i, r;

    for (r = 0; r < 256u; r++) {
        const float d = f->sun[1] * (float)r * (1.0f / 255.0f);

        f->glut[r] = psm_pack_rgb(psm_light_c(s, 0u, d), psm_light_c(s, 1u, d), psm_light_c(s, 2u, d));
    }
    if (f->gao_on) {
        for (r = 0; r < 256u; r++) {
            const float a = (float)r * (1.0f / 255.0f), d = f->sun[1] * a;

            f->glut_a[r] = psm_pack_rgb(s->amb[0] * a, s->amb[1] * a, s->amb[2] * a);
            f->glut_s[r] = psm_pack_rgb(s->sunc[0] * d, s->sunc[1] * d, s->sunc[2] * d);
        }
        for (i = 0; i < f->gvn; i++) f->gcol[i] = psm_rgb_add_(f->glut_a[f->gao[i]], f->glut_s[f->glit[i]]);
    } else
        for (i = 0; i < f->gvn; i++) f->gcol[i] = f->glut[f->glit[i]];
    if (f->colour_opt & PSM_COLOUR_TABLE) {
        psm_colour_table_(f, s);
        if (f->mao) {
            for (i = 0; i < s->mvn; i++) {
                const unsigned k = s->mkind[i] < s->paln ? s->mkind[i] : 0u;

                if (seen && !f->mvis[i]) continue;
                f->mcol[i] = psm_rgb_add_(f->mlut_a[k][f->mao[i]], f->mlut_s[k][f->mlit[i]]);
            }
            return;
        }
        for (i = 0; i < s->mvn; i++) {
            if (seen && !f->mvis[i]) continue;
            f->mcol[i] = f->mlut[s->mkind[i] < s->paln ? s->mkind[i] : 0u][f->mlit[i]];
        }
        return;
    }
    if (f->mao) {
        for (i = 0; i < s->mvn; i++) {
            const float d = (float)f->mlit[i] * (1.0f / 255.0f), a = (float)f->mao[i] * (1.0f / 255.0f);
            const float *alb = s->pal[s->mkind[i] < s->paln ? s->mkind[i] : 0u];

            if (seen && !f->mvis[i]) continue;
            f->mcol[i] = psm_pack_rgb(alb[0] * (s->amb[0] * a + s->sunc[0] * d), alb[1] * (s->amb[1] * a + s->sunc[1] * d), alb[2] * (s->amb[2] * a + s->sunc[2] * d));
        }
        return;
    }
    for (i = 0; i < s->mvn; i++) {
        const float d = (float)f->mlit[i] * (1.0f / 255.0f);
        const float *alb = s->pal[s->mkind[i] < s->paln ? s->mkind[i] : 0u];

        if (seen && !f->mvis[i]) continue;
        f->mcol[i] = psm_pack_rgb(alb[0] * psm_light_c(s, 0u, d), alb[1] * psm_light_c(s, 1u, d), alb[2] * psm_light_c(s, 2u, d));
    }
}

/* The placement and the colours together (the host test's order: after the lit and the lookup). */
static inline void psm_place(psm_frame_t *f, const psm_scene_t *s) {
    psm_place_geo(f, s);
    psm_colour(f, s);
}

/* The last model triangle of the run that starts at i: i .. the returned one are in one strip and all placed with area
   (mtri 1), so one strip sends them: the first's corners 0 and 1, then each one's corner 2. */
static inline unsigned psm_model_run_end(const psm_frame_t *f, const psm_scene_t *s, unsigned i) {
    while (i + 1u < s->mtn && !s->mstart[i + 1u] && f->mtri[i + 1u] == 1u) i++;
    return i;
}

/* ---- a model triangle across the near plane ------------------------------------ */

/* A vertex of a cut model triangle: screen x, y, 1 / W, and its colour. */
typedef struct psm_cv {
    float x, y, iw;
    uint32_t argb;
} psm_cv_t;

static inline uint32_t psm_mix_argb_(uint32_t a, uint32_t b, float t) {
    uint32_t out = 0u;
    unsigned sh;

    for (sh = 0; sh < 32u; sh += 8u) {
        const float ca = (float)((a >> sh) & 255u), cb = (float)((b >> sh) & 255u);

        out |= (uint32_t)(ca + (cb - ca) * t + 0.5f) << sh;
    }
    return out;
}

/* Model triangle i, which crosses the near plane (mtri 2), cut at W = 1.01 x PSM_NEAR in clip space (X, Y, W and the colour
   are linear there): the part in front as screen vertices in strip order. 3 a triangle, 4 a quad as a strip of two (1, 0, 2, 3:
   the triangles 0 1 2 and 0 2 3), 0 when what is left has no area. A quad with one empty half sends the other half. A walking
   camera beside a pole or a cow needs it: without the cut, a face with one corner behind the eye drops whole. psm_place first. */
static inline unsigned psm_clip_near(const psm_frame_t *f, const psm_scene_t *s, unsigned i, psm_cv_t out[4]) {
    const float wc = PSM_NEAR * 1.01f;
    float poly[4][3];
    uint32_t col[4];
    unsigned n = 0, j, order[4], m = 0;

    for (j = 0; j < 3u; j++) {
        const unsigned p = s->mt[i][j], q = s->mt[i][(j + 1u) % 3u];
        const float *cp = f->mclip[p], *cq = f->mclip[q];
        const int pin = cp[2] >= wc, qin = cq[2] >= wc;

        if (pin) { poly[n][0] = cp[0]; poly[n][1] = cp[1]; poly[n][2] = cp[2]; col[n] = f->mcol[p]; n++; }
        if (pin != qin) {
            const float t = sh4der_div_(wc - cp[2], cq[2] - cp[2]);

            poly[n][0] = cp[0] + (cq[0] - cp[0]) * t; poly[n][1] = cp[1] + (cq[1] - cp[1]) * t; poly[n][2] = wc;
            col[n] = psm_mix_argb_(f->mcol[p], f->mcol[q], t);
            n++;
        }
    }
    if (n == 3u) {
        if (!sh4der_tri_area_ok(poly[0], poly[1], poly[2], PSM_NEAR, SH4DER_GUARD_MIN_AREA2)) return 0;
        order[0] = 0; order[1] = 1; order[2] = 2; m = 3;
    } else if (n == 4u) {
        const int lo = sh4der_tri_area_ok(poly[0], poly[1], poly[2], PSM_NEAR, SH4DER_GUARD_MIN_AREA2);
        const int hi = sh4der_tri_area_ok(poly[0], poly[2], poly[3], PSM_NEAR, SH4DER_GUARD_MIN_AREA2);

        if (lo && hi) { order[0] = 1; order[1] = 0; order[2] = 2; order[3] = 3; m = 4; }
        else if (lo) { order[0] = 0; order[1] = 1; order[2] = 2; m = 3; }
        else if (hi) { order[0] = 0; order[1] = 2; order[2] = 3; m = 3; }
    }
    for (j = 0; j < m; j++) {
        const float *c = poly[order[j]], iw = sh4der_recip_pos_(c[2]);

        out[j].x = (c[0] * iw * 0.5f + 0.5f) * PSM_SCR_W;
        out[j].y = (0.5f - c[1] * iw * 0.5f) * PSM_SCR_H;
        out[j].iw = iw;
        out[j].argb = col[order[j]];
    }
    return m;
}

/* ---- the readout ------------------------------------------------------------- */

#define PSM_HUD_LINES 16u           /* lines 0 .. 10 psm_hud, 11 the frame and the vertex buffer (psm_send.h, psm_dc.c), 12 .. 13
                                       the panel's title and tabs (psm_panel_lines); the text texture's sixteen, no more VRAM */
#define PSM_HUD_COLS  64u            /* characters a line (a 512 texel line of the 8 x 16 font) */
#define PSM_TEXT_W    512u           /* the text texture: sixteen lines of the 8 x 16 font, fourteen used, ARGB1555 */
#define PSM_TEXT_H    256u
#define PSM_TEXT_KB   (PSM_TEXT_W * PSM_TEXT_H * 2u / 1024u)

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

/* The eleven lines: 0 the title, 1 texel over pixel and the fit's slab, 2 .. 5 the readout (4: the sun and the camera, the
   town's clock; 5: the map's casters and the objects culled), 6 the cost label, 7 the split of the SH-4's time, 8 the menu's
   row (torchwalk's line), 9 the controls, 10 the draw distance (with the readout; the town: the placeholders). */
static inline void psm_hud(const psm_state_t *st, const psm_frame_t *f, const psm_cost_t *c, char ln[PSM_HUD_LINES][PSM_HUD_COLS + 1u]) {
    const unsigned kb = (f->size * f->size * 2u + 1023u) / 1024u;
    char a[4][16], big[192], v[48];
    unsigned i;

    memset(ln, 0, PSM_HUD_LINES * (PSM_HUD_COLS + 1u));
    PSM_LINE_(0, "%s   MAP %s%s%s", st->route == PSM_ROUTE_A ? "ROUTE A  PVR DRAWS THE MAP" : "ROUTE B  SH-4 DEPTH",
             !f->built ? "NONE" : !st->warp ? "UNIFORM" : f->head_on ? "PSM -> UNIFORM" : "PSM", f->head_on ? "  [HEAD-ON]" : "", st->scene ? "  TOWN" : "");
    for (i = 0; i < 4u; i++) {
        if (f->ratio[i] < 0.0f) snprintf(a[i], sizeof a[i], "--");
        else psm_f1_(a[i], sizeof a[i], f->ratio[i] > 999.0f ? 999.0f : f->ratio[i]);
    }
    psm_f1_(v, sizeof v, f->slab);
    PSM_LINE_(1, "TEXEL/PX  4m %s  10m %s  25m %s  60m %s  SLAB 0-%s", a[0], a[1], a[2], a[3], v);
    if (st->size_refused_kb)                                    /* a bigger map was chosen and could not be had */
        PSM_LINE_(2, "MAP %ux%u  %u KB  bigger REFUSED: no RAM for %u KB", f->size, f->size, kb, st->size_refused_kb);
    else if (f->size > PSM_MAP_STATIC)                          /* 512: a look, not speed (it misses the SH-4's 16 KB data cache) */
        PSM_LINE_(2, "MAP %ux%u  %u KB RAM  NOT FOR SPEED: 16 KB cache, fill/texel", f->size, f->size, kb);
    else
        PSM_LINE_(2, "MAP %ux%u  %u KB %s  text %u KB VRAM", f->size, f->size, kb, st->route == PSM_ROUTE_A ? "VRAM (render target)" : "RAM (16-bit depth)",
                 PSM_TEXT_KB);
    psm_f1_(a[0], sizeof a[0], st->warp_near);
    PSM_LINE_(3, "WARP %s m  %s  self-shadow %s  CASTERS %s", st->warp ? a[0] : "--", f->built ? psm_kind_name(f->map.kind) : "--",
             st->route == PSM_ROUTE_A ? "no" : "yes", st->movers_only ? "MOVERS" : "ALL");
    psm_f1_(a[2], sizeof a[2], st->x);
    psm_f1_(a[3], sizeof a[3], st->z);
    if (st->scene) {                                             /* the town: the clock, the camera in story metres (z north) */
        const unsigned t = (unsigned)(st->clock * 60.0f + 0.5f);

        psm_f1_(a[3], sizeof a[3], -st->z);
        snprintf(a[0], sizeof a[0], "%02u:%02u", t / 60u, t % 60u);
        snprintf(a[1], sizeof a[1], "x%u", (unsigned)psm_time_speed[st->tspeed % PSM_TIME_SPEEDS]);
        PSM_LINE_(4, "%s %s SUN %u/%u  CAM x %s z %s  hdg %u pitch %d", st->sun_free ? "free " : a[0],
                 psm_time_speed[st->tspeed % PSM_TIME_SPEEDS] ? a[1] : "||", (unsigned)(st->az + 0.5f), (unsigned)(st->el + 0.5f), a[2], a[3],
                 (unsigned)(st->yaw * 57.29578f + 0.5f) % 360u, (int)(-st->pitch * 57.29578f + (st->pitch > 0.0f ? -0.5f : 0.5f)));
    } else
        PSM_LINE_(4, "SUN az %u el %u   CAM x %s z %s  hdg %u  pitch %d", (unsigned)(st->az + 0.5f), (unsigned)(st->el + 0.5f), a[2], a[3],
                 (unsigned)(st->yaw * 57.29578f + 0.5f) % 360u, (int)(-st->pitch * 57.29578f + (st->pitch > 0.0f ? -0.5f : 0.5f)));
    psm_f1_(a[1], sizeof a[1], st->bias_b);
    if (st->route == PSM_ROUTE_B)
        PSM_LINE_(5, "BIAS a %u mm b %s  FILTER A %s B %s  TRIS %u/%u cull %u", (unsigned)(st->bias_a * 1000.0f + 0.5f), a[1],
                 st->filter_a ? "BILIN" : "POINT", st->pcf ? "2x2" : "POINT",
                 f->drawn, f->tris, f->cull_objs);
    else
        PSM_LINE_(5, "LOOKUP per pixel  FILTER A %s B %s  TRIS %u/%u cull %u", st->filter_a ? "BILIN" : "POINT", st->pcf ? "2x2" : "POINT",
                 f->drawn, f->tris, f->cull_objs);
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
    PSM_LINE_(8, "%s: %s   (D-pad row/change, B close)", psm_row_name[st->row % PSM_ROWS], psm_row_value(st, st->row % PSM_ROWS, v, sizeof v));
    if (st->scene && st->dist_i >= PSM_DIST_INF) PSM_LINE_(10, "DIST INF   people (8), hens (4): PLACEHOLDER blocks");
    else if (st->scene) PSM_LINE_(10, "DIST %u m, fog from %u   people, hens: PLACEHOLDER", (unsigned)psm_dist_m[st->dist_i],
                                  (unsigned)(psm_dist_m[st->dist_i] * psm_fog_start(st) + 0.5f));
    else if (st->dist_i >= PSM_DIST_INF) PSM_LINE_(10, "DIST INF: no cut, no fog   map casters: all distances");
    else PSM_LINE_(10, "DIST %u m: cut, fog from %u m   map casters: all distances", (unsigned)psm_dist_m[st->dist_i],
                   (unsigned)(psm_dist_m[st->dist_i] * psm_fog_start(st) + 0.5f));
    PSM_LINE_(9, "%s", st->menu ? "L/R TAB  UP/DN ROW  </> A ADJUST  B CLOSE"
                                : st->scene ? "stick look YAXB walk Up route Dn map START menu L/R time Y+A HUD"
                                : "stick look YAXB walk Up route Dn map START menu L/R sun Y+A HUD");
}

#undef PSM_LINE_

/* ---- the settings panel ---------------------------------------------------------------------------------------------- */
/* START opens it. The layout, the navigation and the colours are the owner's own menu (demo/psm/CREDITS.md), set for our
   8 x 16 font: a dark panel down the left, the title, the tabs (one a group: the one open white and underlined, the others
   grey), a hairline, the rows (the label at the left, "< VALUE >" at the right, the chosen one on a bar in white, a
   chevron left out at a slider's end), a scrollbar when a group has more rows than show, a hairline and the button hints.
   Its text is lines of the readout's texture: open, the panel's lines take 1 .. 8 (the readout's, not shown then), 9 is
   the hints and 12, 13 the title and the tabs; lines 0 and 11 stay at the top. psm_send_panel (psm_send.h) draws it. */
#define PSM_PANEL_ROWS   8u           /* rows shown at once */
#define PSM_PANEL_COLS   50u          /* characters a line in the panel (400 pixels) */
#define PSM_PANEL_LABEL  14u          /* the label's column width (the longest name is 13) */
#define PSM_PANEL_X0     0.0f         /* the panel: x 0 .. 432, y 80 .. 344 */
#define PSM_PANEL_X1     432.0f
#define PSM_PANEL_Y0     80.0f
#define PSM_PANEL_Y1     344.0f
#define PSM_PANEL_TX     16.0f        /* the text's left edge */
#define PSM_PANEL_TITLE_Y 90.0f
#define PSM_PANEL_TABS_Y 112.0f
#define PSM_PANEL_HAIR1_Y 136.0f
#define PSM_PANEL_ROW_Y  144.0f       /* the first row's top; a row's pitch */
#define PSM_PANEL_PITCH  20.0f
#define PSM_PANEL_HAIR2_Y 312.0f
#define PSM_PANEL_FOOT_Y 320.0f
#define PSM_PANEL_LN_TITLE 12u
#define PSM_PANEL_LN_TABS  13u
#define PSM_PANEL_LN_FOOT  9u
#define PSM_PANEL_FILL   0xB2000000u
#define PSM_PANEL_TEXT   0xFFDEE4E8u
#define PSM_PANEL_WHITE  0xFFF4F7F9u
#define PSM_PANEL_MUTE   0xFF808E98u
#define PSM_PANEL_FAINT  0xFF5E6A74u
#define PSM_PANEL_HAIR   0x5A96A5B2u
#define PSM_PANEL_BAR    0x4D8FB8D8u
#define PSM_PANEL_ACCENT 0xFF8FB8D8u
#define PSM_PANEL_TRACK  0x733C5A73u

/* the panel's numbers this frame (no text): the open group's rows, the window on them, the tabs' columns */
typedef struct psm_panel {
    int rows[PSM_ROWS];
    unsigned n, sel, top, shown;          /* rows in the group, the chosen one's place, the first shown, how many shown */
    unsigned tab_c0[PSM_GRPS], tab_c1[PSM_GRPS];   /* each tab's first column and one past its last on line 13 */
} psm_panel_t;

static inline void psm_panel_layout(const psm_state_t *st, psm_panel_t *p) {
    const int g = st->grp >= 0 && st->grp < (int)PSM_GRPS ? st->grp : 0;
    unsigned i, c = 0;

    p->n = psm_grp_rows(g, p->rows);
    p->sel = psm_grp_index(g, st->row);
    p->top = p->sel < PSM_PANEL_ROWS ? 0u : p->sel - PSM_PANEL_ROWS + 1u;
    p->shown = p->n - p->top < PSM_PANEL_ROWS ? p->n - p->top : PSM_PANEL_ROWS;
    for (i = 0; i < PSM_GRPS; i++) {
        p->tab_c0[i] = c;
        c += (unsigned)strlen(psm_grp_name[i]);
        p->tab_c1[i] = c;
        c += 3u;                                                  /* three spaces between tabs */
    }
}

/* the value's chevrons: a slider at its end leaves that one out (a ghost chevron, here a space) */
static inline int psm_panel_end_(const psm_state_t *st, int row, int hi) {
    const psm_row_def_t *d = &psm_row_def[row];
    float v;

    if (d->kind != PSM_RK_SLIDER || d->wraps) return 0;
    switch (row) {
        case PSM_ROW_BIAS_A: v = st->bias_a * 1000.0f; break;
        case PSM_ROW_BIAS_B: v = st->bias_b; break;
        case PSM_ROW_WARP_NEAR: v = st->warp_near; break;
        case PSM_ROW_ELEVATION: v = st->el; break;
        default: return 0;
    }
    return hi ? v >= d->hi - d->step * 0.5f : v <= d->lo + d->step * 0.5f;
}

/* The panel's lines into ln (after psm_hud or psm_ab_hud): 1 .. 8 the rows shown, 12 the title, 13 the tabs, 9 the hints. */
static inline void psm_panel_lines(const psm_state_t *st, const psm_panel_t *p, char ln[PSM_HUD_LINES][PSM_HUD_COLS + 1u]) {
    char v[48], val[PSM_PANEL_COLS + 8u];
    unsigned i, w;

    for (i = 0; i < PSM_PANEL_ROWS; i++) memset(ln[1u + i], 0, PSM_HUD_COLS + 1u);
    for (i = 0; i < p->shown; i++) {
        const int row = p->rows[p->top + i];
        const char *s = psm_row_value(st, row, v, sizeof v);
        const unsigned room = PSM_PANEL_COLS - PSM_PANEL_LABEL - 4u;   /* the value between "< " and " >" */
        char cut[PSM_PANEL_COLS + 1u];

        if (strlen(s) > room) { memcpy(cut, s, room - 3u); memcpy(cut + room - 3u, "...", 4u); s = cut; }
        snprintf(val, sizeof val, "%c %s %c", psm_panel_end_(st, row, 0) ? ' ' : '<', s, psm_panel_end_(st, row, 1) ? ' ' : '>');
        w = PSM_PANEL_COLS - PSM_PANEL_LABEL;
        snprintf(ln[1u + i], PSM_HUD_COLS + 1u, "%-*.*s%*.*s", (int)PSM_PANEL_LABEL, (int)PSM_PANEL_LABEL, psm_row_name[row], (int)w, (int)w, val);
    }
    memset(ln[PSM_PANEL_LN_TITLE], 0, PSM_HUD_COLS + 1u);
    snprintf(ln[PSM_PANEL_LN_TITLE], PSM_HUD_COLS + 1u, "%-*s%*s", (int)(PSM_PANEL_COLS - 12u), "SETTINGS", 12, "");
    snprintf(v, sizeof v, "%u / %u", p->sel + 1u, p->n);
    memcpy(ln[PSM_PANEL_LN_TITLE] + PSM_PANEL_COLS - strlen(v), v, strlen(v));
    memset(ln[PSM_PANEL_LN_TABS], 0, PSM_HUD_COLS + 1u);
    for (i = 0; i < PSM_GRPS; i++) {
        memset(ln[PSM_PANEL_LN_TABS] + (i ? p->tab_c1[i - 1u] : 0u), ' ', p->tab_c0[i] - (i ? p->tab_c1[i - 1u] : 0u));
        memcpy(ln[PSM_PANEL_LN_TABS] + p->tab_c0[i], psm_grp_name[i], p->tab_c1[i] - p->tab_c0[i]);
    }
}

#endif /* PSM_SCENE_H */
