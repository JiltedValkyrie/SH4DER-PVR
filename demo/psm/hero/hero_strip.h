/* SH4DER-PVR: hero1, the space stretch: the hero's run along it, his place, the shadow window that follows him, which
   objects are in view, and the near clip of a set triangle

   demo/psm/hero/hero_strip.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.
   The set's art (hero_strip_data.h, written by tools/bake_strip.py): Space Kit (2.0) by Kenney (www.kenney.nl), CC0 1.0.

   RECOMMENDED (nothing here is measured; wiki/limits/header-recommended.md)
   Cost:       not measured. Counts (exact, the set as baked): 2,671 triangles, 3,543 vertices, 21 objects. A frame:
               strip_run and strip_place (one sine and cosine each), a sphere test an object (strip_seen), one
               matrix product each for the camera and the shadow, a cut only where a triangle crosses the near plane
   Start with: the shadow window 2 x HERO_SPAN a side, centred on the hero every frame; STRIP_SPEED = 5.2f,
               STRIP_TURN_S = 0.8f; the camera at 5.4 m, as the autorun's stages
   Status:     ADVISED: host tested (test/hero_strip_test.c replays the autorun: hero and shadow never hidden)
   Do:         keep the eye in the street (strip_eye_ok; strip_yaw_max for a live view); cut at the near plane (strip_clip)
   Do not:     drop a set triangle that crosses the near plane; move the hero without moving the shadow's window
   Maturity:   PROTOTYPE v0.1

   Three hens stand beside the road, each over a flat blob of its own (strip_hen_*); they are not in the shadow target.

   The road runs along z; the hero runs it from z = STRIP_RUN to -STRIP_RUN and back at STRIP_SPEED, turning about at
   each end (strip_run: a pure function of the time, so a host test can replay a whole run). He is drawn through a
   place matrix (strip_place) multiplied into the camera's and the shadow's, so the clips stay as they are. The shadow
   target's window (2 x HERO_SPAN a side) is centred on him every frame (strip_shadow_matrix, strip_ground_uv): his
   shadow cannot leave it. The camera keeps to the street: its eye's |x| stays under STRIP_CLEAR (strip_eye_ok), so no
   building is between it and him. A set triangle that crosses the near plane is cut there (strip_clip), not dropped.

   Plain C through sh4der_core.h; the matrix product is SH4ZAM's XMTRX on the Dreamcast. Host tested:
   test/hero_strip_test.c replays the autorun and fails if the hero or his shadow is ever hidden. */

#ifndef HERO_STRIP_H
#define HERO_STRIP_H

#include "hero_view.h"

typedef struct strip_obj {
    uint16_t v0, vn, t0, tn, floor;              /* floor: the ground, it reads the shadow */
    float c[3], r;                               /* a sphere round it */
} strip_obj_t;

#include "hero_strip_data.h"

#define STRIP_SPEED   5.2f                       /* m/s: how fast the RUN clip's foot goes back while it is down (5.17 over its
                                                    contact frames, measured on the host from the clip): the feet do not slide */
#define STRIP_TURN_S  0.8f                       /* the turn about at an end, standing */
#define STRIP_LEG_S   (2.0f * STRIP_RUN / STRIP_SPEED)
#define STRIP_LAP_S   (2.0f * (STRIP_LEG_S + STRIP_TURN_S))
#define STRIP_PI      3.14159265f

/* The run at t seconds (t >= 0, it loops every STRIP_LAP_S): the hero's z (x is 0), his heading (0 faces +z, pi faces
   -z), the clip and the clip's own time. He starts at z = STRIP_RUN facing -z. */
static inline void strip_run(float t, float *z, float *heading, unsigned *clip, float *clip_t) {
    const float u = sh4der_frac_(t * (1.0f / STRIP_LAP_S)) * STRIP_LAP_S;

    if (u < STRIP_LEG_S) { *z = STRIP_RUN - u * STRIP_SPEED; *heading = STRIP_PI; *clip = HERO_RUN_AIM; *clip_t = u; }
    else if (u < STRIP_LEG_S + STRIP_TURN_S) {
        const float k = (u - STRIP_LEG_S) * (1.0f / STRIP_TURN_S);

        *z = -STRIP_RUN; *heading = STRIP_PI * (1.0f + k); *clip = HERO_IDLE; *clip_t = u - STRIP_LEG_S;
    } else if (u < 2.0f * STRIP_LEG_S + STRIP_TURN_S) {
        const float w = u - STRIP_LEG_S - STRIP_TURN_S;

        *z = -STRIP_RUN + w * STRIP_SPEED; *heading = 0.0f; *clip = HERO_RUN_AIM; *clip_t = w;
    } else {
        const float k = (u - 2.0f * STRIP_LEG_S - STRIP_TURN_S) * (1.0f / STRIP_TURN_S);

        *z = STRIP_RUN; *heading = STRIP_PI * k; *clip = HERO_IDLE; *clip_t = u - 2.0f * STRIP_LEG_S - STRIP_TURN_S;
    }
}

/* The hero's place: a turn about y (the figure faces +z at rest) and a move to (x, 0, z). 4 x 4 column-major. */
static inline void strip_place(float heading, float x, float z, float m[16]) {
    float s, c;

    sh4der_sincos_(heading, &s, &c);
    memset(m, 0, 16u * sizeof m[0]);
    m[0] = c; m[2] = -s; m[5] = 1.0f; m[8] = s; m[10] = c; m[12] = x; m[14] = z; m[15] = 1.0f;
}

/* A world direction in the hero's own axes (the place's turn undone): the sun, the camera's right and up. */
static inline void strip_unturn(float heading, const float d[3], float out[3]) {
    float s, c;

    sh4der_sincos_(heading, &s, &c);
    out[0] = d[0] * c - d[2] * s; out[1] = d[1]; out[2] = d[0] * s + d[2] * c;
}

/* o = a b, 4 x 4 column-major (o apart from a and b, all aligned to 8). */
static inline void strip_mul(float o[16], const float a[16], const float b[16]) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    shz_xmtrx_load_4x4((const shz_mat4x4_t *)a);
    shz_xmtrx_apply_4x4((const shz_mat4x4_t *)b);
    shz_xmtrx_store_4x4((shz_mat4x4_t *)o);
#else
    hero_m4_mul_(o, a, b);
#endif
}

/* hero_shadow_matrix with the target's window centred on (cx, cz), and the ground's lookup into it. Outside the window
   the lookup leaves 0 .. 1: the target's border is white and the header clamps, so the ground there is unshadowed. */
static inline void strip_shadow_matrix(const float sun[3], float size, float cx, float cz, float m[16]) {
    const float k = size / (2.0f * HERO_SPAN);

    hero_shadow_matrix(sun, size, m);
    m[12] -= k * cx; m[13] -= k * cz;
}

static inline void strip_ground_uv(const float p[3], float cx, float cz, float uv[2]) {
    uv[0] = (p[0] - cx + HERO_SPAN) * (1.0f / (2.0f * HERO_SPAN));
    uv[1] = (p[2] - cz + HERO_SPAN) * (1.0f / (2.0f * HERO_SPAN));
}

/* The eye is in the street: between the buildings' fronts, over the ground, short of the ends' buildings. */
static inline int strip_eye_ok(const float eye[3]) {
    return eye[0] > -(STRIP_CLEAR - 0.3f) && eye[0] < STRIP_CLEAR - 0.3f && eye[1] > 0.3f && eye[2] > -(STRIP_END - 0.3f) && eye[2] < STRIP_END - 0.3f;
}

/* The most a live view's yaw may leave the street's line (0 or pi) at this distance and pitch, so strip_eye_ok holds
   at x: the eye's x is dist cos(pitch) sin(yaw). Radians, 0.1 .. 1.2. */
static inline float strip_yaw_max(float dist, float cos_pitch) {
    const float s = sh4der_div_(STRIP_CLEAR - 0.5f, dist * cos_pitch + 0.01f);

    return sh4der_clamp_(s, 0.1f, 0.93f);        /* the angle is under asin(s) while s <= 0.93: s itself is a safe angle */
}

/* Whether a sphere can be in the camera's view (near plane and the four sides; no far plane). */
static inline int strip_seen(const hero_cam_t *cam, const float c[3], float r) {
    /* the side planes' unit normals in camera axes: (cos, sin) of the half angles, tan = 1 / (0.75 FOCAL) and 1 / FOCAL */
    const float tx = 1.0f / (0.75f * HERO_FOCAL), ty = 1.0f / HERO_FOCAL;
    const float nx = 0.82154f, sx = nx * tx, ny = 0.88698f, sy = ny * ty;   /* 1 / sqrt(1 + tan^2) for each */
    const float d[3] = { c[0] - cam->eye[0], c[1] - cam->eye[1], c[2] - cam->eye[2] };
    const float z = sh4der_dot3_(d, cam->fwd), x = sh4der_dot3_(d, cam->right), y = sh4der_dot3_(d, cam->up);

    if (z + r < HERO_NEAR) return 0;
    if (sh4der_abs_(x) * nx - z * sx > r) return 0;
    if (sh4der_abs_(y) * ny - z * sy > r) return 0;
    return 1;
}

/* A set vertex before the divide: X, Y, W of the camera's matrix, its lookup and its colour. */
typedef struct strip_cv { float x, y, w, u, v; uint32_t argb; } strip_cv_t;

static inline uint32_t strip_mix_(uint32_t a, uint32_t b, float t) {
    const unsigned k = (unsigned)(sh4der_sat_(t) * 256.0f);
    const uint32_t rb = (((a & 0x00ff00ffu) * (256u - k) + (b & 0x00ff00ffu) * k) >> 8) & 0x00ff00ffu;
    const uint32_t ag = ((((a >> 8) & 0x00ff00ffu) * (256u - k) + ((b >> 8) & 0x00ff00ffu) * k)) & 0xff00ff00u;

    return rb | ag;
}

/* A triangle cut at W = near: the part with W >= near, 0, 3 or 4 vertices in the triangle's own turn. */
static inline unsigned strip_clip(const strip_cv_t in[3], float near, strip_cv_t out[4]) {
    unsigned i, n = 0;

    for (i = 0; i < 3u; i++) {
        const strip_cv_t *a = &in[i], *b = &in[(i + 1u) % 3u];
        const int ia = a->w >= near, ib = b->w >= near;

        if (ia) out[n++] = *a;
        if (ia != ib) {
            const float t = sh4der_div_(near - a->w, b->w - a->w);
            strip_cv_t *o = &out[n++];

            o->x = sh4der_lerp_(a->x, b->x, t); o->y = sh4der_lerp_(a->y, b->y, t); o->w = near;
            o->u = sh4der_lerp_(a->u, b->u, t); o->v = sh4der_lerp_(a->v, b->v, t);
            o->argb = strip_mix_(a->argb, b->argb, t);
        }
    }
    return n;
}

/* ---- the hens: three beside the road, each with a flat blob under it (no shadow target: nothing to pop in or out) ---- */

#define STRIP_HENS      3u
#define STRIP_HEN_R     0.5f                     /* a sphere round a hen, about (x, STRIP_HEN_CY, z): what the cull and the test use */
#define STRIP_HEN_CY    0.25f
#define STRIP_BLOB_N    8u                       /* the blob: a fan of this many triangles, alpha STRIP_BLOB_A at its middle, 0 on its rim */
#define STRIP_BLOB_R    0.26f
#define STRIP_BLOB_Y    0.03f                    /* over the ground, so it is in front of it */
#define STRIP_BLOB_A    0x70u
static const float strip_hen_home[STRIP_HENS][3] = {   /* x, z, the way it faces: on the rock beside the road, clear of his run */
    { 2.7f, 9.0f, 0.4f }, { -2.8f, -3.0f, 2.2f }, { 2.5f, -13.0f, 4.0f },
};

/* Hen i at t seconds: it stands where it is, looks about slowly and pecks now and then. A pure function of the time. */
static inline void strip_hen_pose(unsigned i, float t, float *bearing, float *tilt) {
    const float period = 2.2f + 0.5f * (float)i, u = sh4der_frac_(t * sh4der_div_(1.0f, period) + 0.37f * (float)i) * period;
    float s, c;

    sh4der_sincos_(6.2831853f * sh4der_frac_(t * 0.07f + 0.3f * (float)i), &s, &c);
    *bearing = strip_hen_home[i][2] + 0.6f * s;
    *tilt = 0.0f;
    if (u < 0.4f) { sh4der_sincos_(STRIP_PI * u * 2.5f, &s, &c); *tilt = 0.7f * s; }
}

/* A hen's place: a turn about y, a tilt about its own side axis with its feet kept down, a move to (x, 0, z). 4 x 4
   column-major (the town's psm_mover_rows, in this street's axes). */
static inline void strip_hen_place(float x, float z, float bearing, float tilt, float m[16]) {
    float sb, cb, st, ct;

    sh4der_sincos_(bearing, &sb, &cb); sh4der_sincos_(tilt, &st, &ct);
    m[0] = cb;       m[1] = 0.0f; m[2] = sb;        m[3] = 0.0f;
    m[4] = sb * st;  m[5] = ct;   m[6] = -cb * st;  m[7] = 0.0f;
    m[8] = -sb * ct; m[9] = st;   m[10] = cb * ct;  m[11] = 0.0f;
    m[12] = x - 0.1f * st * sb; m[13] = 0.1f - 0.1f * ct; m[14] = z + 0.1f * st * cb; m[15] = 1.0f;
}

/* The blob under a hen at (x, z): its middle, then STRIP_BLOB_N rim points; slid a little along the sun as a shadow is.
   Triangle k is 0, k + 1, (k + 1) % STRIP_BLOB_N + 1. */
static inline void strip_hen_blob(float x, float z, const float sun[3], float out[STRIP_BLOB_N + 1u][3]) {
    const float k = STRIP_HEN_CY * sh4der_recip_pos_(sun[1]), cx = x - sun[0] * k, cz = z - sun[2] * k;
    unsigned i;

    out[0][0] = cx; out[0][1] = STRIP_BLOB_Y; out[0][2] = cz;
    for (i = 0; i < STRIP_BLOB_N; i++) {
        float s, c;

        sh4der_sincos_(6.2831853f * (float)i * (1.0f / (float)STRIP_BLOB_N), &s, &c);
        out[i + 1u][0] = cx + STRIP_BLOB_R * c; out[i + 1u][1] = STRIP_BLOB_Y; out[i + 1u][2] = cz + STRIP_BLOB_R * s;
    }
}

/* ---- the autorun's script: the stages and the camera, here so the host test replays what the program runs ----------- */

/* A stage: which passes are on, whether he runs (0: he stands at the start), the camera's distance, its seconds. The
   opaque pass and the shadow are on in every stage: no stage hides the hero or his shadow. */
typedef struct strip_stage { uint8_t det, vis, run; float dist, secs; const char *name; } strip_stage_t;
#define STRIP_STAGES 5u
#define STRIP_AUTO_PITCH 0.24f
#define STRIP_AUTO_SWING 0.35f                   /* the camera swings this far either side of the street's line, radians */
static const strip_stage_t strip_stage[STRIP_STAGES] = {
    { 1, 1, 0, 5.4f, 4.0f, "all, IDLE" },
    { 1, 1, 1, 5.4f, 16.0f, "all, RUN a lap" },
    { 0, 1, 1, 5.4f, 5.0f, "run, no detail" },
    { 1, 0, 1, 5.4f, 5.0f, "run, no visor" },
    { 1, 1, 1, 8.0f, 5.0f, "run, far camera" },
};

/* The autorun's camera at t seconds since the run began, the hero at z: it stands up the street (+z) of him, looks
   down it and swings slowly from one side of the road to the other. */
static inline void strip_auto_cam(hero_cam_t *cam, float t, float dist, float hero_z) {
    const float target[3] = { 0.0f, 1.0f, hero_z };
    float s, c;

    sh4der_sincos_(6.2831853f * sh4der_frac_(t * (0.5f / 6.2831853f)), &s, &c);
    hero_cam_make(cam, STRIP_AUTO_SWING * s, STRIP_AUTO_PITCH, dist, target);
}

#endif /* HERO_STRIP_H */
