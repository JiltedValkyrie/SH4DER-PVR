/* SH4DER-PVR demo: "The Little Match Girl" (a wordless 60 second short film), the scene data and maths.
 * Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.
 *
 * What this file is: everything about the film that is a number or a pure calculation, with no KallistiOS
 * include, so the same code runs in the host test (test/matchlight_scene_test.c) and in the Dreamcast program
 * (matchlight.c). It holds the timeline (ten shots, 60 s, looping), the camera of every shot, the placeholder set
 * as tables (houses, windows, ground grid, the girl, a small tree), the match state of every shot, the sky and
 * ambient colours, a view and perspective projection, and the lighting of one ground vertex.
 *
 * World axes: x is across the street (right is +x), y is up, z runs along the street away from the start.
 * Units are metres. (The storyboard brief uses x along the street and z up; this file renames the axes:
 * brief x -> z, brief y -> x, brief z -> y. Brief positions are kept, the girl is moved 3 m down the street so that
 * she stands 9 m or more from every lens.)
 *
 * Placeholder status: every shape here is built from numbers. No art from any game or film is used.
 * The maths goes through the SH4ZAM wrappers of sh4der_core.h (dot, fused multiply-add, lerp, clamp, divide, sincos).
 */
#ifndef MATCHLIGHT_SCENE_H
#define MATCHLIGHT_SCENE_H

#include <stdint.h>
#include "sh4der_core.h"

/* ------------------------------------------------------------------------------------------------------------- */
/* Screen and timeline constants. */

#define ML_SCREEN_W      640            /* frame width in pixels (KallistiOS default video mode) */
#define ML_SCREEN_H      480            /* frame height in pixels */
#define ML_NEAR_Z        0.4f           /* near plane depth, metres: anything nearer is skipped, never sent */
#define ML_SHOTS         10             /* shots M1 .. M10 */
#define ML_FILM_SECONDS  60.0f          /* length of the film, seconds; the clock loops at this value */
#define ML_DEG2RAD       0.01745329252f /* degrees to radians */
#define ML_CUT_DIP_S     0.075f         /* half of the 0.15 s dip to black: it covers this long either side of a cut, seconds */

/* ------------------------------------------------------------------------------------------------------------- */
/* The shot table. */

/* One shot: when it runs, where the camera is, and what the match and the girl are doing. */
typedef struct ml_shot {
    const char *name;                   /* short label for the readout ("M1" ...) */
    float start_s, end_s;               /* the shot runs from start_s to end_s of the film, seconds */
    float eye0[3], target0[3];          /* camera position and look-at point at progress 0 */
    float eye1[3], target1[3];          /* the same at progress 1 (equal to the first pair: a static shot) */
    float hfov_deg;                     /* HORIZONTAL field of view, degrees (the spec's "Lens (h FOV)" column) */
    float match_on_s;                   /* seconds into the shot at which the match is lit; negative: no match this shot */
    float match_off_s;                  /* seconds into the shot after which the match dies down */
    float fade_in_s;                    /* seconds the flame takes to rise to full (0 = at once) */
    float fade_out_s;                   /* seconds the flame takes to die after match_off_s (0 = at once) */
    float pool_radius;                  /* radius of the warm pool on the snow, metres */
    uint8_t pool_rgb[3];                /* colour of the pool and the flame, 0..255 */
    uint8_t vision_rgb[3];              /* colour of the vision quad inside the pool, 0..255 */
    uint8_t has_tree;                   /* 1: the small tree stands in this shot (the tree vision) */
    uint8_t dawn;                       /* 1: the sky and the ambient light blend to dawn over this shot */
    float girl_z0, girl_z1;             /* the girl walks from girl_z0 to girl_z1 over the shot (equal: she stands) */
    const char *caption;                /* a few words saying what the shot shows, for the on-screen debug caption */
} ml_shot_t;

/* Longest caption line the caption image can hold: "<name> <caption>", characters (256 texels at the font's widest 12). */
#define ML_CAPTION_CHARS 21

/* Girl standing place: she stops here for every shot after M2 (metres along the street). */
#define ML_GIRL_STAND_Z  15.0f
/* Girl at the start of her walk in M2, metres along the street. */
#define ML_GIRL_WALK_Z0  24.0f
/* Girl in the first shot: a dark shape in the fog at the far end of the street. */
#define ML_GIRL_FAR_Z    28.0f

/* The ten shots. Times, lens angles and staging follow the film spec (cinematography section v5 / v4 and the
 * storyboard brief); camera heights are 1.6 m except the M6 crane (1.6 m up to 4.0 m). Girl 9 m or more from the lens.
 * Every camera stands INSIDE the lane (|x| <= 4.5 m; the house fronts are 7 m or more out), so no house can come between
 * a lens and the girl or the tree. (On the first build the cameras stood 6 to 12 m out, behind the house fronts.) */
static const ml_shot_t ml_shots[ML_SHOTS] = {
    /* M1: static street, lit windows fading into fog, a dark shape far away. 60 degrees. */
    { "M1",  0.0f,  6.0f, { 0.0f, 1.6f, 0.0f}, { 0.0f, 1.0f, 30.0f}, { 0.0f, 1.6f, 0.0f}, { 0.0f, 1.0f, 30.0f}, 60.0f,
      -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, {255, 176, 96}, {0, 0, 0}, 0, 0, ML_GIRL_FAR_Z, ML_GIRL_FAR_Z, "street, far girl" },
    /* M2: static, the girl walks in as a silhouette, bare prints behind her. 45 degrees. */
    { "M2",  6.0f, 12.0f, {-4.5f, 1.6f, 4.0f}, { 0.0f, 0.8f, 17.0f}, {-4.5f, 1.6f, 4.0f}, { 0.0f, 0.8f, 17.0f}, 45.0f,
      -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, {255, 176, 96}, {0, 0, 0}, 0, 0, ML_GIRL_WALK_Z0, ML_GIRL_STAND_Z, "girl walks in" },
    /* M3: first match, the stove vision in a 1.2 m pool. 35 degrees. */
    { "M3", 12.0f, 20.0f, {-3.5f, 1.6f, 5.0f}, { 0.0f, 0.6f, 15.0f}, {-3.5f, 1.6f, 5.0f}, { 0.0f, 0.6f, 15.0f}, 35.0f,
      0.6f, 8.0f, 0.15f, 0.4f, 1.2f, {255, 176, 96}, {230, 110, 50}, 0, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "first match: stove" },
    /* M4: the match goes out in the first 0.4 s, snow settles; slow pull back. 40 degrees. */
    { "M4", 20.0f, 26.0f, {-3.5f, 1.6f, 5.0f}, { 0.0f, 0.6f, 15.0f}, {-4.5f, 1.6f, 3.0f}, { 0.0f, 0.6f, 15.0f}, 40.0f,
      0.0f, 0.0f, 0.0f, 0.4f, 1.2f, {255, 176, 96}, {230, 110, 50}, 0, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "match goes out" },
    /* M5: second match, the goose table, a larger 1.6 m pool. 40 degrees. */
    { "M5", 26.0f, 34.0f, {-4.0f, 1.6f, 5.0f}, { 0.0f, 0.6f, 15.0f}, {-4.0f, 1.6f, 5.0f}, { 0.0f, 0.6f, 15.0f}, 40.0f,
      0.5f, 8.0f, 0.15f, 0.4f, 1.6f, {255, 176, 96}, {200, 140, 80}, 0, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "second: goose" },
    /* M6: third match, the tree; crane from 1.6 m to 4.0 m. 50 degrees. */
    { "M6", 34.0f, 44.0f, {-4.0f, 1.6f, 5.0f}, { 3.0f, 3.0f, 17.0f}, {-4.0f, 4.0f, 5.0f}, { 3.0f, 3.0f, 17.0f}, 50.0f,
      0.4f, 10.0f, 0.15f, 0.4f, 1.6f, {255, 176, 96}, {120, 180, 120}, 1, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "tree, candles rise" },
    /* M7: no match; a star falls across the sky (camera tilted up). 50 degrees. */
    { "M7", 44.0f, 48.0f, {-4.0f, 1.6f, 5.0f}, { 0.0f, 7.0f, 30.0f}, {-4.0f, 1.6f, 5.0f}, { 0.0f, 7.0f, 30.0f}, 50.0f,
      -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, {255, 176, 96}, {0, 0, 0}, 0, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "falling star" },
    /* M8: fifth match, the grandmother, a soft figure in the pool. 40 degrees. */
    { "M8", 48.0f, 52.0f, {-4.5f, 1.6f, 5.0f}, { 0.0f, 0.8f, 15.0f}, {-4.5f, 1.6f, 5.0f}, { 0.0f, 0.8f, 15.0f}, 40.0f,
      0.2f, 4.0f, 0.15f, 0.4f, 1.4f, {255, 176, 96}, {250, 230, 200}, 0, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "grandmother" },
    /* M9: the whole bundle, the largest pool (2.5 m radius, pale gold), the gold-white flash at the strike. 50 degrees. */
    { "M9", 52.0f, 56.0f, {-4.5f, 1.6f, 4.5f}, { 0.0f, 2.0f, 15.0f}, {-4.5f, 1.6f, 4.5f}, { 0.0f, 2.0f, 15.0f}, 50.0f,
      0.0f, 4.0f, 0.05f, 0.4f, 2.5f, {255, 214, 150}, {250, 230, 200}, 0, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "all matches flare" },
    /* M10: dawn, cool blue, no warmth; slow pull back along the lane. 70 degrees. */
    { "M10", 56.0f, 60.0f, {-4.5f, 1.6f, 4.5f}, { 0.0f, 0.6f, 15.0f}, {-4.5f, 1.6f, 0.5f}, { 0.0f, 0.6f, 15.0f}, 70.0f,
      -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, {255, 176, 96}, {0, 0, 0}, 0, 1, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "dawn, still girl" },
};

/* ------------------------------------------------------------------------------------------------------------- */
/* Timeline. */

/* Where the film clock is: which shot, how far into it, and the wrapped clock itself. */
typedef struct ml_where {
    int shot;                           /* 0 .. ML_SHOTS - 1 */
    float progress;                     /* 0 .. 1 inside the shot */
    float seconds;                      /* seconds since the shot started */
    float clock;                        /* the clock wrapped into 0 .. ML_FILM_SECONDS */
} ml_where_t;

/* Wraps a clock value (any float seconds, including negative) into 0 .. ML_FILM_SECONDS: the film loops. */
static inline float ml_wrap_clock(float t) {
    const float turns = sh4der_frac_(sh4der_div_(t, ML_FILM_SECONDS));   /* 0 .. 1, negatives wrap upward */

    return turns * ML_FILM_SECONDS;
}

/* Finds the shot for film time t (looping at 60 s) and the 0 .. 1 progress inside it. Shots tile the film with no gap:
 * shot k ends exactly where shot k + 1 starts, so the first shot whose end is past the wrapped time is the one. */
static inline void ml_locate(float t, ml_where_t *w) {
    const float c = ml_wrap_clock(t);
    int k;

    w->clock = c;
    w->shot = ML_SHOTS - 1;
    for (k = 0; k < ML_SHOTS; k++) {
        if (c < ml_shots[k].end_s) {
            w->shot = k;
            break;
        }
    }
    w->seconds = c - ml_shots[w->shot].start_s;
    w->progress = sh4der_sat_(sh4der_div_(w->seconds, ml_shots[w->shot].end_s - ml_shots[w->shot].start_s));
}

/* Smooth ease 0 .. 1 -> 0 .. 1 (3p^2 - 2p^3): the slow start and stop of a camera move. */
static inline float ml_ease(float p) {
    const float q = sh4der_sat_(p);

    return q * q * sh4der_fma_(q, -2.0f, 3.0f);
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Camera and projection. */

/* A camera at one instant: position, an orthonormal frame, and the lens in pixels. */
typedef struct ml_camera {
    float eye[3];                       /* position, metres */
    float target[3];                    /* the point it looks at */
    float right[3], up[3], fwd[3];      /* unit axes in world space (x right, y up, z into the screen) */
    float hfov_deg;                     /* horizontal field of view, degrees */
    float focal_px;                     /* pixels of screen x for x / z = 1: (width / 2) / tan(hfov / 2) */
} ml_camera_t;

/* One projected point: screen pixels and the PVR depth key 1 / w (larger is nearer, the KallistiOS convention). */
typedef struct ml_screen {
    float x, y, iz;
} ml_screen_t;

/* Builds the camera of `shot` at `progress` (0 .. 1): the eye and target move between their start and end poses with a
 * smooth ease; a static shot has equal poses. Returns 1, or 0 for a bad shot index or a degenerate look direction. */
static inline int ml_camera_at(int shot, float progress, ml_camera_t *c) {
    const ml_shot_t *s;
    const float up_world[3] = { 0.0f, 1.0f, 0.0f };
    float e, half_s, half_c;
    int k;

    if (shot < 0 || shot >= ML_SHOTS || !c) {
        return 0;
    }
    s = &ml_shots[shot];
    e = ml_ease(progress);
    for (k = 0; k < 3; k++) {
        c->eye[k] = sh4der_lerp_(s->eye0[k], s->eye1[k], e);
        c->target[k] = sh4der_lerp_(s->target0[k], s->target1[k], e);
        c->fwd[k] = c->target[k] - c->eye[k];
    }
    if (!(sh4der_len3_(c->fwd[0], c->fwd[1], c->fwd[2]) > 1.0e-6f)) {
        return 0;
    }
    sh4der_norm3_(c->fwd);
    sh4der_cross_(up_world, c->fwd, c->right);         /* left-handed frame: right = up x forward */
    sh4der_norm3_(c->right);
    sh4der_cross_(c->fwd, c->right, c->up);            /* already unit: both inputs are unit and square */
    c->hfov_deg = s->hfov_deg;
    sh4der_sincos_(s->hfov_deg * 0.5f * ML_DEG2RAD, &half_s, &half_c);
    c->focal_px = sh4der_div_(0.5f * (float)ML_SCREEN_W * half_c, half_s);
    return 1;
}

/* World point -> view space (x right, y up, z depth along the view). */
static inline void ml_to_view(const ml_camera_t *c, const float p[3], float v[3]) {
    const float d[3] = { p[0] - c->eye[0], p[1] - c->eye[1], p[2] - c->eye[2] };

    v[0] = sh4der_dot3_(d, c->right);
    v[1] = sh4der_dot3_(d, c->up);
    v[2] = sh4der_dot3_(d, c->fwd);
}

/* View-space point -> screen pixels and 1 / w. Returns 0 when the point is nearer than the near plane (the caller must
 * then skip any triangle that uses it: it is not clipped here), else 1. */
static inline int ml_view_to_screen(const ml_camera_t *c, const float v[3], ml_screen_t *out) {
    float iz;

    if (!(v[2] >= ML_NEAR_Z)) {
        return 0;
    }
    iz = sh4der_recip_pos_(v[2]);
    out->x = sh4der_fma_(v[0] * iz, c->focal_px, 0.5f * (float)ML_SCREEN_W);
    out->y = sh4der_fma_(v[1] * iz, -c->focal_px, 0.5f * (float)ML_SCREEN_H);
    out->iz = iz;
    return 1;
}

/* World point -> screen. Returns 0 for a point at or behind the near plane. */
static inline int ml_project(const ml_camera_t *c, const float p[3], ml_screen_t *out) {
    float v[3];

    ml_to_view(c, p, v);
    return ml_view_to_screen(c, v, out);
}

/* 1 when a screen point lies inside the 640 x 480 frame. */
static inline int ml_in_frame(const ml_screen_t *s) {
    return s->x >= 0.0f && s->x < (float)ML_SCREEN_W && s->y >= 0.0f && s->y < (float)ML_SCREEN_H;
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Fog: the street fades out with distance. Both the PVR fog table (matchlight.c) and the additive sprites (which the
 * table does not touch) use these two numbers. */

#define ML_FOG_START_Z   10.0f          /* depth where the fog begins, metres */
#define ML_FOG_END_Z     45.0f          /* depth where the fog is complete, metres */

/* Share of its colour that a thing at depth z keeps: 1 at the start of the fog, 0 at the end (linear, like the table). */
static inline float ml_fog_keep(float z) {
    return sh4der_sat_(sh4der_div_(ML_FOG_END_Z - z, ML_FOG_END_Z - ML_FOG_START_Z));
}

/* ------------------------------------------------------------------------------------------------------------- */
/* The girl. A flat dark silhouette of four quads that always turns to face the camera about the vertical axis. */

#define ML_GIRL_QUADS    4              /* dress, shawl, head, hands */
#define ML_GIRL_HEIGHT   1.1f           /* total height, metres */
#define ML_GIRL_LAYER    0.01f          /* each quad sits this much nearer the camera than the one before, so none shares a plane, metres */
#define ML_STRIDE        0.75f          /* distance between two bare footprints along her walk, metres */
#define ML_FOOT_SIDE     0.11f          /* each foot is this far to the side of her path, metres */
#define ML_WALK_DIST     (ML_GIRL_WALK_Z0 - ML_GIRL_STAND_Z)   /* length of the M2 walk, metres */
#define ML_STEPS_TOTAL   13u            /* footprints in the whole M2 walk: floor(ML_WALK_DIST / ML_STRIDE) + 1 */

/* One flat quad in strip order (0 1 2 3 gives triangles 0 1 2 and 1 2 3), world positions. */
typedef struct ml_quad {
    float p[4][3];
} ml_quad_t;

/* The horizontal facing of a camera: `side` is its right axis flattened onto the ground plane, `toward` the horizontal
 * direction pointing back at the camera. Used to turn flat cut-outs (the girl, the tree) to face the lens. */
static inline void ml_flat_basis(const ml_camera_t *c, float side[3], float toward[3]) {
    side[0] = c->right[0];
    side[1] = 0.0f;
    side[2] = c->right[2];
    sh4der_norm3_(side);
    toward[0] = -c->fwd[0];
    toward[1] = 0.0f;
    toward[2] = -c->fwd[2];
    sh4der_norm3_(toward);
}

/* A trapezoid in the camera-facing plane through `base`: bottom edge at height y0 with half width hw0, top edge at height
 * y1 with half width hw1, sitting `lift` metres toward the camera. `cx` shifts it sideways in the plane. */
static inline void ml_flat_quad(const float base[3], const float side[3], const float toward[3], float cx, float y0, float hw0,
                                float y1, float hw1, float lift, ml_quad_t *q) {
    static const float corner_y[4] = { 0.0f, 1.0f, 0.0f, 1.0f };      /* 0 = bottom edge, 1 = top edge, per corner */
    static const float corner_s[4] = { -1.0f, -1.0f, 1.0f, 1.0f };    /* -1 = left, +1 = right, per corner */
    int i, k;

    for (i = 0; i < 4; i++) {
        const float y = corner_y[i] > 0.5f ? y1 : y0;
        const float hw = corner_y[i] > 0.5f ? hw1 : hw0;

        for (k = 0; k < 3; k++) {
            q->p[i][k] = sh4der_fma_(side[k], sh4der_fma_(corner_s[i], hw, cx), sh4der_fma_(toward[k], lift, base[k]));
        }
        q->p[i][1] += y;
    }
}

/* Builds the girl's four quads at world position `pos` (her feet), turned to the camera. */
static inline void ml_girl_build(const ml_camera_t *c, const float pos[3], ml_quad_t out[ML_GIRL_QUADS]) {
    float side[3], toward[3];

    ml_flat_basis(c, side, toward);
    ml_flat_quad(pos, side, toward, 0.0f, 0.00f, 0.26f, 0.72f, 0.15f, 0.0f * ML_GIRL_LAYER, &out[0]);   /* dress */
    ml_flat_quad(pos, side, toward, 0.0f, 0.62f, 0.22f, 0.90f, 0.14f, 1.0f * ML_GIRL_LAYER, &out[1]);   /* shawl */
    ml_flat_quad(pos, side, toward, 0.0f, 0.92f, 0.07f, ML_GIRL_HEIGHT, 0.07f, 2.0f * ML_GIRL_LAYER, &out[2]);  /* head */
    ml_flat_quad(pos, side, toward, 0.0f, 0.46f, 0.08f, 0.58f, 0.08f, 3.0f * ML_GIRL_LAYER, &out[3]);   /* hands */
}

/* The girl's position at a shot and progress (feet on the ground, centre of her body). Walks linearly. */
static inline void ml_girl_pos(int shot, float progress, float pos[3]) {
    const ml_shot_t *s = &ml_shots[shot];

    pos[0] = 0.0f;
    pos[1] = 0.0f;
    pos[2] = sh4der_lerp_(s->girl_z0, s->girl_z1, sh4der_sat_(progress));
}

/* The middle of her body (half her height): the point a camera has to keep in frame. */
static inline void ml_girl_centre(int shot, float progress, float out[3]) {
    ml_girl_pos(shot, progress, out);
    out[1] = 0.5f * ML_GIRL_HEIGHT;
}

/* Where footprint n (0 = the first, at the far end) lies: x, z on the snow. Feet alternate left and right. */
static inline void ml_foot(unsigned n, float *x, float *z) {
    *x = (n & 1u) ? ML_FOOT_SIDE : -ML_FOOT_SIDE;
    *z = ML_GIRL_WALK_Z0 - (float)n * ML_STRIDE;
}

/* How many footprints exist by this point of the film: none before M2, the walked ones during M2, all after it. */
static inline unsigned ml_steps_due(int shot, float progress) {
    float walked;
    unsigned n;

    if (shot < 1) {
        return 0u;
    }
    if (shot > 1) {
        return ML_STEPS_TOTAL;
    }
    walked = (ML_GIRL_WALK_Z0 - ML_GIRL_STAND_Z) * sh4der_sat_(progress);
    n = (unsigned)sh4der_div_(walked, ML_STRIDE) + 1u;
    return n > ML_STEPS_TOTAL ? ML_STEPS_TOTAL : n;
}

/* ------------------------------------------------------------------------------------------------------------- */
/* The street: house fronts as boxes with a gable, pale cold windows, and the ground. */

/* One house front. The street-facing wall is the plane x = face_x, spanning width metres along z from z0. */
typedef struct ml_house {
    float face_x;                       /* x of the street-facing wall: negative on the left side, positive on the right */
    float z0, width;                    /* start along the street and width of the wall, metres */
    float height;                       /* eaves height, metres */
    float gable;                        /* extra height of the gable peak over the eaves, metres */
    float depth;                        /* how far the box runs back from the street, metres (its near side wall) */
    uint8_t win_cols, win_rows;         /* window grid on the wall */
    uint8_t lit_mask;                   /* bit (row * win_cols + col): that window is lit (drawn pale) */
    uint8_t shade;                      /* wall brightness 0..255: varies the fronts so they do not read as one repeated shape */
} ml_house_t;

#define ML_HOUSES        10             /* five fronts on each side */
#define ML_WIN_W         0.8f           /* window width along the wall, metres */
#define ML_WIN_H         1.1f           /* window height, metres */
#define ML_WIN_BASE_Y    1.0f           /* sill height of the lowest row, metres */
#define ML_WIN_ROW_STEP  2.2f           /* height between rows, metres */
#define ML_WIN_LIFT      0.03f          /* a window sits this far in front of its wall so it does not fight the wall's depth, metres */

/* Half the width of the open lane: no house front is nearer the street's middle than this, metres. */
#define ML_LANE_HALF     7.0f

/* House tables: heights, widths and gables vary on purpose (one repeated shape was a plates-review finding). The fronts
 * stand 7.0 to 7.6 m out: an open lane with the houses pushed back to its sides. */
static const ml_house_t ml_houses[ML_HOUSES] = {
    /* left side, wall faces +x */
    { -7.0f,  8.0f, 5.0f, 5.5f, 2.0f, 4.4f, 2, 1, 0x01u, 235u },
    { -7.4f, 13.5f, 4.0f, 6.5f, 2.6f, 4.4f, 2, 2, 0x04u, 220u },
    { -7.0f, 18.0f, 5.5f, 5.0f, 1.8f, 4.4f, 3, 1, 0x05u, 245u },
    { -7.6f, 24.0f, 4.5f, 7.0f, 3.0f, 4.4f, 2, 2, 0x09u, 225u },
    { -7.2f, 29.0f, 6.0f, 5.8f, 2.2f, 4.4f, 3, 1, 0x02u, 240u },
    /* right side, wall faces -x */
    {  7.2f,  6.5f, 4.5f, 6.0f, 2.4f, 4.4f, 2, 2, 0x06u, 230u },
    {  7.0f, 11.5f, 5.5f, 5.2f, 2.0f, 4.4f, 3, 1, 0x02u, 245u },
    {  7.5f, 17.5f, 4.0f, 7.0f, 2.8f, 4.4f, 2, 2, 0x05u, 222u },
    {  7.0f, 22.0f, 5.0f, 5.5f, 2.0f, 4.4f, 3, 1, 0x04u, 238u },
    {  7.4f, 27.5f, 4.5f, 6.4f, 2.6f, 4.4f, 2, 2, 0x0au, 228u },
};

/* Wall colour at full shade, 0..255 per channel: dark blue-grey. */
#define ML_WALL_R        40
#define ML_WALL_G        46
#define ML_WALL_B        66
/* Window colour, 0..255 per channel: pale and cold (no warm window: the match is the only warm colour). */
#define ML_WINDOW_R      205
#define ML_WINDOW_G      220
#define ML_WINDOW_B      245

/* One window rectangle of a house, as a quad in strip order in the plane in front of the wall. Returns 1 when window
 * `idx` (row * win_cols + col) exists, and sets *lit to 1 when it is lit. Returns 0 for an index past the grid. */
static inline int ml_house_window(const ml_house_t *h, unsigned idx, ml_quad_t *q, int *lit) {
    const unsigned cols = h->win_cols;
    const unsigned col = idx % cols;
    const unsigned row = idx / cols;
    const float zc = h->z0 + h->width * sh4der_div_((float)col + 0.5f, (float)cols);   /* window centre along the wall */
    const float y0 = sh4der_fma_((float)row, ML_WIN_ROW_STEP, ML_WIN_BASE_Y);
    const float x = h->face_x < 0.0f ? h->face_x + ML_WIN_LIFT : h->face_x - ML_WIN_LIFT;   /* toward the street */

    if (idx >= cols * (unsigned)h->win_rows) {
        return 0;
    }
    q->p[0][0] = x; q->p[0][1] = y0;              q->p[0][2] = zc - 0.5f * ML_WIN_W;
    q->p[1][0] = x; q->p[1][1] = y0 + ML_WIN_H;   q->p[1][2] = zc - 0.5f * ML_WIN_W;
    q->p[2][0] = x; q->p[2][1] = y0;              q->p[2][2] = zc + 0.5f * ML_WIN_W;
    q->p[3][0] = x; q->p[3][1] = y0 + ML_WIN_H;   q->p[3][2] = zc + 0.5f * ML_WIN_W;
    *lit = (h->lit_mask >> idx) & 1u;
    return 1;
}

/* Windows in the whole street (lit or not) and lit windows only: counted from the table. */
static inline unsigned ml_count_windows(int lit_only) {
    unsigned i, j, n = 0;

    for (i = 0; i < ML_HOUSES; i++) {
        for (j = 0; j < (unsigned)ml_houses[i].win_cols * ml_houses[i].win_rows; j++) {
            if (!lit_only || ((ml_houses[i].lit_mask >> j) & 1u)) {
                n++;
            }
        }
    }
    return n;
}

/* Ground plane: a grid of vertex-coloured quads, so the warm light and the snow dents have vertices to live on. */
#define ML_GROUND_COLS   25             /* quads across the street */
#define ML_GROUND_ROWS   40             /* quads along the street */
#define ML_GROUND_X0     (-12.5f)       /* x of the first column of vertices, metres: the path (x = 0) is the middle of column 12 */
#define ML_GROUND_Z0     (-2.0f)        /* z of the first row of vertices, metres */
#define ML_GROUND_DX     1.0f           /* column spacing, metres (25 columns span 25 m) */
#define ML_GROUND_DZ     1.0f           /* row spacing, metres (40 rows span 40 m) */
#define ML_GROUND_VERTS  ((ML_GROUND_COLS + 1) * (ML_GROUND_ROWS + 1))   /* 1,066 vertices */

/* ------------------------------------------------------------------------------------------------------------- */
/* The shape of the snow. The ground is not flat: the snow lies in banks against the house walls, in rolling drifts over
 * the lane and in a few piles. Only the girl's path down the middle is level, so her feet, the match pool and the trail
 * patch sit on a known height (0). ml_snow_height is the one source: the ground grid, the tree's foot and the pool read it. */

#define ML_PATH_FLAT     1.25f          /* the snow is level within this distance of the path's middle (x = 0), metres */
#define ML_PATH_EDGE     3.0f           /* the drifts reach their full height this far from the path's middle, metres */
#define ML_ROLL_HEIGHT   0.22f          /* the rolling drifts rise and fall by this much about their mean, metres (0 to 0.44 m deep) */
#define ML_BANK_START    3.5f           /* a wall bank begins this far from the path's middle, metres */
#define ML_BANK_HEIGHT   1.2f           /* a wall bank's greatest height, at the house fronts, metres */
#define ML_BANK_WAVE     0.25f          /* share of a bank's height that comes and goes along the street */
#define ML_MOUNDS        6u             /* snow piles in the lane */
#define ML_SLOPE_STEP    0.25f          /* the slope is measured over this distance either side of a point, metres */

/* The piles: centre across and along the street, radius, height, metres. None reaches the level path. */
static const float ml_mounds[ML_MOUNDS][4] = {
    {  5.2f,  5.5f, 1.6f, 0.75f }, {  4.2f, 11.0f, 1.8f, 0.85f }, { -3.8f, 19.0f, 2.0f, 0.90f },
    {  3.8f, 21.5f, 2.2f, 1.05f }, { -4.2f, 27.0f, 2.4f, 1.20f }, {  4.8f, 28.0f, 2.0f, 0.90f },
};

/* Height of the undisturbed snow surface at (x, z), metres over the level path. Never negative. The sum of:
 * - rolling drifts: two crossed waves, 0 to 2 x ML_ROLL_HEIGHT, faded in between ML_PATH_FLAT and ML_PATH_EDGE;
 * - the wall banks: an eased rise from ML_BANK_START to the house line (and level beyond it), its height waving along z;
 * - the piles: each a smooth round hill (the library's feather of the squared distance). */
static inline float ml_snow_height(float x, float z) {
    const float ax = x < 0.0f ? -x : x;                              /* distance from the path's middle */
    const float open = ml_ease(sh4der_div_(ax - ML_PATH_FLAT, ML_PATH_EDGE - ML_PATH_FLAT));
    const float bank = ml_ease(sh4der_div_(ax - ML_BANK_START, ML_LANE_HALF - ML_BANK_START));
    float s0, s1, s2, c, h;
    unsigned i;

    sh4der_sincos_(sh4der_fma_(x, 0.55f, z * 0.31f), &s0, &c);
    sh4der_sincos_(sh4der_fma_(x, -0.71f, sh4der_fma_(z, 0.23f, 1.3f)), &s1, &c);
    sh4der_sincos_(sh4der_fma_(z, 0.9f, x < 0.0f ? 0.0f : 1.7f), &s2, &c);   /* the two sides wave out of step */
    (void)c;
    h = open * ML_ROLL_HEIGHT * (1.0f + sh4der_fma_(s0, 0.6f, s1 * 0.4f));
    h = sh4der_fma_(bank * ML_BANK_HEIGHT, sh4der_fma_(s2, ML_BANK_WAVE, 1.0f - ML_BANK_WAVE), h);
    for (i = 0; i < ML_MOUNDS; i++) {
        const float d2 = sh4der_len2_(x - ml_mounds[i][0], z - ml_mounds[i][1]);

        h = sh4der_fma_(ml_mounds[i][3], sh4der_feather_(d2, 0.0f, ml_mounds[i][2]), h);
    }
    return h;
}

/* The slope of the undisturbed snow at (x, z): rise per metre along x and along z, by a centred difference. */
static inline void ml_snow_slope(float x, float z, float *sx, float *sz) {
    const float half = sh4der_div_(0.5f, ML_SLOPE_STEP);

    *sx = (ml_snow_height(x + ML_SLOPE_STEP, z) - ml_snow_height(x - ML_SLOPE_STEP, z)) * half;
    *sz = (ml_snow_height(x, z + ML_SLOPE_STEP) - ml_snow_height(x, z - ML_SLOPE_STEP)) * half;
}

/* The moon: a unit direction TOWARD the light. It stands far down the street and to the right, so a slope that faces the
 * cameras (which all look down the street from its near end) is in shade: drifts read as dark shapes with pale tops, and
 * the far wall of a footprint, the wall a camera sees, is dark. */
#define ML_MOON_X        0.45f
#define ML_MOON_Y        0.62f
#define ML_MOON_Z        0.64f
#define ML_SHADE_GAIN    1.5f           /* how strongly a slope's facing changes its brightness */
#define ML_SHADE_MIN     0.40f          /* the darkest a slope gets, as a share of level snow */
#define ML_SHADE_MAX     1.40f          /* and the brightest */

/* Brightness of snow whose surface rises sx per metre along x and sz along z, as a share of level snow (1 on the level):
 * 1 + gain x (N.L - L.y) with N the unit normal of (-sx, 1, -sz), kept inside ML_SHADE_MIN .. ML_SHADE_MAX. */
static inline float ml_snow_shade(float sx, float sz) {
    static const float moon[3] = { ML_MOON_X, ML_MOON_Y, ML_MOON_Z };
    float n[3] = { -sx, 1.0f, -sz };
    float shade;

    sh4der_norm3_(n);
    shade = sh4der_fma_(sh4der_dot3_(n, moon) - ML_MOON_Y, ML_SHADE_GAIN, 1.0f);
    return shade < ML_SHADE_MIN ? ML_SHADE_MIN : (shade > ML_SHADE_MAX ? ML_SHADE_MAX : shade);
}

/* ------------------------------------------------------------------------------------------------------------- */
/* The trail patch: a fine strip of ground along the girl's walk, in place of the ground grid's middle column there, so
 * her footprints are real pits in the mesh. (The grid's 1 m quads cannot show a 0.3 m pit: on the first build the prints
 * were flat decals, about 7 x 2 pixels at the M2 camera, and could not be seen.) */

#define ML_TRAIL_CELL    0.125f         /* spacing of the patch's vertices, metres: the dent grid's cell */
#define ML_TRAIL_COLS    8u             /* quads across: 1 m, exactly ground column ML_TRAIL_GROUND_COL */
#define ML_TRAIL_ROWS    88u            /* quads along: 11 m */
#define ML_TRAIL_X0      (-0.5f)        /* x of the patch's first column of vertices, metres */
#define ML_TRAIL_Z0      14.0f          /* z of its first row of vertices: 1 m in front of where she stops */
#define ML_TRAIL_VERTS   ((ML_TRAIL_COLS + 1u) * (ML_TRAIL_ROWS + 1u))   /* 801 vertices */
#define ML_TRAIL_GROUND_COL 12u         /* the ground grid column the patch replaces (x -0.5 .. 0.5) */
#define ML_TRAIL_GROUND_ROW0 16u        /* the first ground row it replaces (z 14 .. 15) */
#define ML_TRAIL_GROUND_ROWS 11u        /* ground rows replaced (z 14 .. 25) */
#define ML_TRAIL_TUCK    0.02f          /* the patch's outer vertices reach this far under the ground grid, metres, and */
#define ML_TRAIL_TUCK_Y  0.01f          /* sit this far below it, so the join shows no crack (the two meshes share no vertices) */

/* A bare foot's pit: each foot presses two overlapping round stamps (heel and toe). Deliberately large for a child, so the
 * prints read at 11 to 20 m: 0.32 m wide, 0.46 m long, 0.20 m deep, with a raised rim. */
#define ML_PRINT_RADIUS  0.16f          /* radius of one stamp, metres */
#define ML_PRINT_HALF_GAP 0.07f         /* heel and toe stamps sit this far either side of the print's middle along the walk, metres */
#define ML_PRINT_DEPTH   0.20f          /* depth of the pit's lowest point under the snow surface, metres */
#define ML_SNOW_DEPTH    0.30f          /* depth of the hard ground under the level snow: the deepest any dent can go, metres */

/* 1 when ground grid quad (column i, row j) is replaced by the trail patch and must not be drawn. */
static inline int ml_ground_quad_in_trail(unsigned i, unsigned j) {
    return i == ML_TRAIL_GROUND_COL && j >= ML_TRAIL_GROUND_ROW0 && j < ML_TRAIL_GROUND_ROW0 + ML_TRAIL_GROUND_ROWS;
}

/* The small tree of the M6 vision: a camera-facing cut-out of a trunk and three tiers, and its candle spots. */
#define ML_TREE_X        3.0f           /* base of the tree, metres across */
#define ML_TREE_Z        17.0f          /* base of the tree, metres along */
#define ML_TREE_TRIS     5u             /* trunk quad (2) + three tier triangles */
#define ML_TREE_CANDLES  12u            /* candle spots on the branches, for the star-lift hook */

/* Candle spots in the tree's own plane: (offset across in metres, height in metres). Four on each tier. */
static const float ml_tree_candles[ML_TREE_CANDLES][2] = {
    {-0.80f, 0.70f}, {-0.30f, 0.65f}, { 0.30f, 0.65f}, { 0.80f, 0.70f},
    {-0.60f, 1.40f}, {-0.20f, 1.35f}, { 0.20f, 1.35f}, { 0.60f, 1.40f},
    {-0.35f, 2.05f}, {-0.10f, 2.35f}, { 0.10f, 2.35f}, { 0.35f, 2.05f},
};

/* The tree tier triangles in the tree's plane: (half width at the bottom, bottom height, top height). */
static const float ml_tree_tier[3][3] = {
    {1.00f, 0.55f, 1.75f}, {0.80f, 1.25f, 2.45f}, {0.55f, 1.90f, 3.00f},
};

/* ------------------------------------------------------------------------------------------------------------- */
/* Match state, sky and light. */

/* Warm colour of the match as 0..1 floats: (255, 176, 96) / 255, the film's one accent. */
#define ML_WARM_R        1.0f
#define ML_WARM_G        0.690f
#define ML_WARM_B        0.376f

/* How strongly the match lights the snow: the lit snow is its own colour times the warm colour times this. */
#define ML_WARM_GAIN     1.4f
/* The warm light on the ground is full inside this share of the pool radius and gone at ML_WARM_REACH times the radius. */
#define ML_WARM_CORE     0.35f
#define ML_WARM_REACH    2.4f
/* How much a full dent (depth 1) darkens the snow. */
#define ML_DENT_DARKEN   0.8f

/* The flame's offset from the girl's feet: at her hands, a little toward the camera side, metres. */
#define ML_FLAME_DX      (-0.12f)
#define ML_FLAME_DY      0.52f
#define ML_FLAME_DZ      (-0.10f)

/* 1 when this shot has a match at all. */
static inline int ml_shot_lit(int shot) {
    return ml_shots[shot].match_on_s >= 0.0f;
}

/* Flame level 0 .. 1 at `seconds` into the shot: 0 before the strike, a rise over fade_in_s, full until match_off_s, then
 * a fall over fade_out_s. A shot with no match is 0 throughout. */
static inline float ml_match_level(int shot, float seconds) {
    const ml_shot_t *s = &ml_shots[shot];
    float up = 1.0f, down = 1.0f;

    if (s->match_on_s < 0.0f || seconds < s->match_on_s) {
        return 0.0f;
    }
    if (s->fade_in_s > 0.0f) {
        up = sh4der_sat_(sh4der_div_(seconds - s->match_on_s, s->fade_in_s));
    }
    if (seconds > s->match_off_s) {
        down = s->fade_out_s > 0.0f ? sh4der_sat_(1.0f - sh4der_div_(seconds - s->match_off_s, s->fade_out_s)) : 0.0f;
    }
    return up * down;
}

/* The flame's world position for a girl standing at `girl`. */
static inline void ml_flame_pos(const float girl[3], float out[3]) {
    out[0] = girl[0] + ML_FLAME_DX;
    out[1] = girl[1] + ML_FLAME_DY;
    out[2] = girl[2] + ML_FLAME_DZ;
}

/* Night sky colour, 0..1: (20, 28, 56). */
#define ML_SKY_NIGHT_R   (20.0f / 255.0f)
#define ML_SKY_NIGHT_G   (28.0f / 255.0f)
#define ML_SKY_NIGHT_B   (56.0f / 255.0f)
/* Dawn sky colour, 0..1: cool blue (120, 140, 170) scaled by 0.8, no warmth. */
#define ML_SKY_DAWN_R    (120.0f * 0.8f / 255.0f)
#define ML_SKY_DAWN_G    (140.0f * 0.8f / 255.0f)
#define ML_SKY_DAWN_B    (170.0f * 0.8f / 255.0f)

/* Night ambient light on the snow, per channel 0..1: snow (150, 165, 200) x this is about the brief's (60, 70, 100). */
#define ML_AMB_NIGHT_R   0.40f
#define ML_AMB_NIGHT_G   0.42f
#define ML_AMB_NIGHT_B   0.50f
/* Dawn ambient: cool, brighter, no warmth. */
#define ML_AMB_DAWN_R    0.70f
#define ML_AMB_DAWN_G    0.78f
#define ML_AMB_DAWN_B    0.92f

/* Blend 0 .. 1 toward dawn for a shot: 0 for every shot except the dawn shot, where it eases in over the shot. */
static inline float ml_dawn_blend(int shot, float progress) {
    return ml_shots[shot].dawn ? ml_ease(progress) : 0.0f;
}

/* Sky colour (0..1 per channel) at a shot and progress: night, blending to dawn over the dawn shot. */
static inline void ml_sky(int shot, float progress, float rgb[3]) {
    const float d = ml_dawn_blend(shot, progress);

    rgb[0] = sh4der_lerp_(ML_SKY_NIGHT_R, ML_SKY_DAWN_R, d);
    rgb[1] = sh4der_lerp_(ML_SKY_NIGHT_G, ML_SKY_DAWN_G, d);
    rgb[2] = sh4der_lerp_(ML_SKY_NIGHT_B, ML_SKY_DAWN_B, d);
}

/* Ambient light colour (0..1 per channel) at a shot and progress, the same blend. */
static inline void ml_ambient(int shot, float progress, float rgb[3]) {
    const float d = ml_dawn_blend(shot, progress);

    rgb[0] = sh4der_lerp_(ML_AMB_NIGHT_R, ML_AMB_DAWN_R, d);
    rgb[1] = sh4der_lerp_(ML_AMB_NIGHT_G, ML_AMB_DAWN_G, d);
    rgb[2] = sh4der_lerp_(ML_AMB_NIGHT_B, ML_AMB_DAWN_B, d);
}

/* Lights one ground vertex: snow (150, 165, 200) x ambient, plus the warm term of the match, darkened by a snow dent.
 * amb: ambient colour (ml_ambient). level: flame level 0 .. 1. d2: squared ground distance from the vertex to the flame,
 * metres squared. radius: the shot's pool radius. depth: sh4der_deform_depth at the vertex, 0 .. 1. rgb: result, 0..1. */
static inline void ml_snow_colour(const float amb[3], float level, float d2, float radius, float depth, float rgb[3]) {
    static const float snow[3] = { 150.0f / 255.0f, 165.0f / 255.0f, 200.0f / 255.0f };   /* snow albedo */
    static const float warm[3] = { ML_WARM_R, ML_WARM_G, ML_WARM_B };
    const float reach = radius * ML_WARM_REACH;
    const float f = level > 0.0f ? sh4der_feather_(d2, radius * ML_WARM_CORE, reach) * level : 0.0f;
    const float dark = sh4der_fma_(depth, -ML_DENT_DARKEN, 1.0f);
    int k;

    for (k = 0; k < 3; k++) {
        const float lit = sh4der_fma_(snow[k] * warm[k], f * ML_WARM_GAIN, snow[k] * amb[k]);

        rgb[k] = sh4der_sat_(lit * dark);
    }
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Cuts: a 0.15 s dip to black between shots, and the M9 gold-white flash. */

#define ML_FLASH_FRAME_S 0.0334f        /* one frame at 30 Hz, seconds */
#define ML_FLASH_PEAK    0.85f          /* flash alpha on the first frame of M9 */
#define ML_FLASH_TAIL_S  0.30f          /* the flash has faded away this long after the strike, seconds */
#define ML_FLASH_R       255            /* flash colour: gold-white (255, 236, 190) */
#define ML_FLASH_G       236
#define ML_FLASH_B       190
#define ML_FLASH_SHOT    8              /* index of M9, the shot whose first moments carry the flash */

/* The full-screen overlay for a moment of a shot: *alpha 0 .. 1 and rgb 0..255. A black dip covers ML_CUT_DIP_S either side of each cut (the very
 * first moment of M9 is a flash, not a dip: the strike replaces the cut). M9 begins with one or two frames of gold-white. */
static inline void ml_overlay(int shot, float seconds, float *alpha, uint8_t rgb[3]) {
    const float len = ml_shots[shot].end_s - ml_shots[shot].start_s;
    const float to_end = len - seconds;
    float a_in = sh4der_sat_(1.0f - sh4der_div_(seconds, ML_CUT_DIP_S));
    const float a_out = sh4der_sat_(1.0f - sh4der_div_(to_end, ML_CUT_DIP_S));
    float dip, flash = 0.0f;

    rgb[0] = rgb[1] = rgb[2] = 0;
    if (shot == ML_FLASH_SHOT) {
        a_in = 0.0f;
        if (seconds < ML_FLASH_FRAME_S) {
            flash = ML_FLASH_PEAK;
        } else if (seconds < 2.0f * ML_FLASH_FRAME_S) {
            flash = 0.5f * ML_FLASH_PEAK;
        } else {
            flash = 0.25f * sh4der_sat_(sh4der_div_(ML_FLASH_TAIL_S - seconds, ML_FLASH_TAIL_S));
        }
    }
    dip = a_in > a_out ? a_in : a_out;
    if (flash > dip) {
        rgb[0] = ML_FLASH_R; rgb[1] = ML_FLASH_G; rgb[2] = ML_FLASH_B;
        *alpha = flash;
    } else {
        *alpha = dip;
    }
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Sprite atlas: four 32 x 32 cells in a 64 x 64 texture, drawn as grey values the sprite colour tints. */

#define ML_ATLAS_SIDE    64             /* atlas width and height, texels */
#define ML_ATLAS_CELL    32             /* one cell's width and height, texels */
#define ML_ATLAS_GLOW    0u             /* cell 0: soft round glow (flame halo, pool edge, vision quad) */
#define ML_ATLAS_STAR    1u             /* cell 1: four-point star */
#define ML_ATLAS_STREAK  2u             /* cell 2: a streak (u along its length, bright at the head) */
#define ML_ATLAS_WINDOW  3u             /* cell 3: soft window glow */

/* The grey value 0..1 of texel (tx, ty) (0 .. 63 each) of the atlas. Pure maths, no square root: the soft edges are
 * the library's feather of the squared distance. */
static inline float ml_atlas_value(unsigned tx, unsigned ty) {
    const unsigned cell = (ty / ML_ATLAS_CELL) * 2u + (tx / ML_ATLAS_CELL);
    const float u = sh4der_div_((float)(tx % ML_ATLAS_CELL) + 0.5f, (float)ML_ATLAS_CELL);   /* 0 .. 1 across the cell */
    const float v = sh4der_div_((float)(ty % ML_ATLAS_CELL) + 0.5f, (float)ML_ATLAS_CELL);
    const float cx = sh4der_fma_(u, 2.0f, -1.0f);   /* -1 .. 1 */
    const float cy = sh4der_fma_(v, 2.0f, -1.0f);
    const float r2 = sh4der_len2_(cx, cy);
    const float glow = sh4der_feather_(r2, 0.0f, 1.0f);

    switch (cell) {
    case ML_ATLAS_GLOW:
        return glow * glow;
    case ML_ATLAS_STAR: {
        const float bar_h = sh4der_feather_(cy * cy * 36.0f, 0.0f, 1.0f) * sh4der_feather_(cx * cx, 0.0f, 1.0f);
        const float bar_v = sh4der_feather_(cx * cx * 36.0f, 0.0f, 1.0f) * sh4der_feather_(cy * cy, 0.0f, 1.0f);
        const float core = glow * glow * 0.7f;
        const float arm = bar_h > bar_v ? bar_h : bar_v;

        return arm > core ? arm : core;
    }
    case ML_ATLAS_STREAK:
        return sh4der_feather_(cy * cy * 4.0f, 0.0f, 1.0f) * u;
    default:
        return sh4der_feather_(r2 * 0.6f, 0.0f, 1.0f) * 0.8f;
    }
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Counted budget per shot. */

/* Quads the falling snow can send in a frame at most (the motes cap, params.max_draw). */
#define ML_SNOW_QUADS    96u
/* Quads of window glow: one per house. */
#define ML_GLOW_QUADS    ML_HOUSES
/* Triangles of one pool fan (16 segments), and the pool has an inner and an outer fan. */
#define ML_POOL_SEGMENTS 16u
#define ML_POOL_FANS     2u

/* Triangles a frame of `shot` sends at most, COUNTED from the tables (every quad two triangles, no culling applied,
 * no hardware measurement): ground, house walls (front 2 + gable 1 + side wall 2), windows drawn (lit ones only), the girl,
 * the tree, the trail patch, the pool fans, flame quads, vision quad, window glows, snow, and the cut overlay. */
static inline unsigned ml_tris_counted(int shot) {
    unsigned n = 2u * (ML_GROUND_COLS * ML_GROUND_ROWS - ML_TRAIL_GROUND_ROWS);   /* ground, less the quads the trail patch replaces */

    n += 2u * ML_TRAIL_COLS * ML_TRAIL_ROWS;                  /* the trail patch */

    n += 5u * ML_HOUSES;                                      /* house walls */
    n += 2u * ml_count_windows(1);                            /* lit windows */
    n += 2u * ML_GIRL_QUADS;                                  /* the girl */
    n += ml_shots[shot].has_tree ? ML_TREE_TRIS : 0u;         /* the tree */
    if (ml_shot_lit(shot)) {
        n += ML_POOL_FANS * ML_POOL_SEGMENTS;                 /* pool fans */
        n += 2u * 2u;                                         /* flame halo + hot core quads */
        n += 2u;                                              /* vision quad */
    }
    n += 2u * ML_GLOW_QUADS;                                  /* window glows */
    n += 2u * ML_SNOW_QUADS;                                  /* falling snow */
    n += 2u;                                                  /* the full-screen overlay (cuts, flash) */
    n += 2u;                                                  /* the debug caption */
    return n;
}

#endif /* MATCHLIGHT_SCENE_H */
