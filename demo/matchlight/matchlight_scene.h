/* SH4DER-PVR demo: "The Little Match Girl" (a 120 second short film for a narrator), the scene data and maths.
 * Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.
 *
 * What this file is: everything about the film that is a number or a pure calculation, with no KallistiOS
 * include, so the same code runs in the host test (test/matchlight_scene_test.c) and in the Dreamcast program
 * (matchlight.c). It holds the timeline (twelve shots, 120 s, looping), the camera of every shot, the placeholder set
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
#define ML_BAND_H        360            /* height of the 16:9 picture: the middle band of the frame, pixels. The rows above and below it
                                         * are black bars when ML_LETTERBOX is 1, so the television shows the framing the staging was judged in */
#define ML_SUB_MAX_GLYPHS 64u            /* the most letters one subtitle sentence may have (tools/narration.py keeps to it) */
#define ML_LETTERBOX     1              /* 1: black bars over the rows outside the 16:9 picture; 0: all 480 rows show (the one switch) */
#define ML_NEAR_Z        0.4f           /* near plane depth, metres: anything nearer is skipped, never sent */
#define ML_SHOTS         12             /* shots M1 .. M10, with M2b and M4b */
#define ML_FILM_SECONDS  120.0f         /* length of the film, seconds; the end credits follow (ML_LOOP_SECONDS) */
#define ML_DEG2RAD       0.01745329252f /* degrees to radians */
#define ML_DISSOLVE_S    0.5f           /* half of a 1 s dissolve: the fade out before a cut and the fade in after it, seconds */

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
    float in_s;                         /* the shot fades in from black over this long, seconds (0: it starts at once) */
    float out_s;                        /* the shot fades to black over its last this long, seconds (0: a hard cut out) */
    uint8_t flash;                      /* 1: the shot opens on the gold-white flash of a match being struck */
    float move_s;                       /* the camera move takes this long from the shot's start, then holds, seconds (0: the whole shot) */
} ml_shot_t;

/* Longest caption line the caption image can hold: "<name> <caption>", characters (256 texels at the font's widest 12). */
#define ML_CAPTION_CHARS 21

/* Girl standing place: she stops here for every shot after M2 (metres along the street). */
#define ML_GIRL_STAND_Z  15.0f
/* Girl at the start of her walk in M2, metres along the street. */
#define ML_GIRL_WALK_Z0  24.0f
/* Girl in the first shot: a dark shape in the fog at the far end of the street. */
#define ML_GIRL_FAR_Z    28.0f

/* Index of each shot in ml_shots. */
enum {
    ML_SHOT_M1, ML_SHOT_M2, ML_SHOT_M2B, ML_SHOT_M3, ML_SHOT_M4, ML_SHOT_M4B, ML_SHOT_M5, ML_SHOT_M6, ML_SHOT_M7, ML_SHOT_M8,
    ML_SHOT_M9, ML_SHOT_M10
};

/* The twelve shots of the 120 s film with a narrator: times, cuts, lens angles and staging. Lens heights: 1.6 m while she stands
 * (M1 .. M4; the low M2b is 0.45 m), then 1.0 to 1.4 m once she sits (the M6 crane runs 1.0 to 2.4 m), at 6 to 12 m. The
 * girl is 6 m or more from every lens. From M3 on the cameras stand on one line through her, 22 degrees left of the
 * street's axis, except M4b, which looks across the lane from the right. Every camera stands INSIDE the lane (|x| <= 4.5 m; the house fronts are 7 m or
 * more out), so no house can come between a lens and the girl or the tree.
 * Cuts: a "dissolve" of the timing table is a dip through black here, ML_DISSOLVE_S out and ML_DISSOLVE_S in (the PVR has
 * no second picture to blend with); a shot that opens on a struck match (flash = 1) is cut hard, on the flash. */
static const ml_shot_t ml_shots[ML_SHOTS] = {
    /* M1: static street, lit windows fading into fog, a dark shape far away. 60 degrees. Fades up from black over 1 s. */
    { "M1",   0.0f,  13.0f, { 0.0f, 1.6f, 0.0f}, { 0.0f, 1.0f, 30.0f}, { 0.0f, 1.6f, 0.0f}, { 0.0f, 1.0f, 30.0f}, 60.0f,
      -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, {255, 176, 96}, {0, 0, 0}, 0, 0, ML_GIRL_FAR_Z, ML_GIRL_FAR_Z, "street, far girl",
      1.0f, ML_DISSOLVE_S, 0, 0.0f },
    /* M2: static, the girl walks in as a silhouette, bare prints behind her. 45 degrees. */
    { "M2",  13.0f,  25.0f, {-4.5f, 1.6f, 4.0f}, { 0.0f, 0.8f, 17.0f}, {-4.5f, 1.6f, 4.0f}, { 0.0f, 0.8f, 17.0f}, 45.0f,
      -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, {255, 176, 96}, {0, 0, 0}, 0, 0, ML_GIRL_WALK_Z0, ML_GIRL_STAND_Z, "girl walks in",
      ML_DISSOLVE_S, ML_DISSOLVE_S, 0, 0.0f },
    /* M2b: low and static at snow level, looking past her along the trail of bare prints. 35 degrees. Hard cut out. */
    { "M2b", 25.0f,  32.0f, {-1.8f, 0.45f, 8.6f}, { 0.0f, 0.35f, 18.0f}, {-1.8f, 0.45f, 8.6f}, { 0.0f, 0.35f, 18.0f}, 35.0f,
      -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, {255, 176, 96}, {0, 0, 0}, 0, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "bare prints, low",
      ML_DISSOLVE_S, 0.0f, 0, 0.0f },
    /* M3: first match, struck on the cut; the stove vision in a 1.2 m pool. 35 degrees. */
    { "M3",  32.0f,  44.0f, {-2.28f, 1.6f, 9.45f}, { 0.0f, 0.6f, 15.0f}, {-2.28f, 1.6f, 9.45f}, { 0.0f, 0.6f, 15.0f}, 35.0f,
      0.0f, 12.0f, 0.15f, 0.4f, 1.2f, {255, 176, 96}, {230, 110, 50}, 0, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "first match: stove",
      0.0f, ML_DISSOLVE_S, 1, 0.0f },
    /* M4: the match goes out in the first 0.4 s, snow settles; slow pull back. 40 degrees. */
    { "M4",  44.0f,  50.0f, {-2.28f, 1.6f, 9.45f}, { 0.0f, 0.6f, 15.0f}, {-3.04f, 1.6f, 7.60f}, { 0.0f, 0.6f, 15.0f}, 40.0f,
      0.0f, 0.0f, 0.0f, 0.4f, 1.2f, {255, 176, 96}, {230, 110, 50}, 0, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "match goes out",
      ML_DISSOLVE_S, ML_DISSOLVE_S, 0, 0.0f },
    /* M4b: static, 7 m off, lens 1.4 m high: she sits in the snow. 34 degrees (the seated child about a third of the frame's
     * height). The camera stands on the right of the lane and looks across it, so the step between two house fronts of
     * the left side (z 23.5 / 24) stands behind her, left of the frame's middle: her corner. Hard cut out. */
    { "M4b", 50.0f,  58.0f, { 3.6f, 1.4f, 9.0f}, { 0.0f, 0.45f, 15.0f}, { 3.6f, 1.4f, 9.0f}, { 0.0f, 0.45f, 15.0f}, 34.0f,
      -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, {255, 176, 96}, {0, 0, 0}, 0, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "alone in the cold",
      ML_DISSOLVE_S, 0.0f, 0, 0.0f },
    /* M5: second match, struck on the cut; the goose table, a larger 1.6 m pool. 40 degrees. Hard cut out (into M6's strike). */
    { "M5",  58.0f,  67.0f, {-2.66f, 1.0f, 8.53f}, { 0.0f, 0.5f, 15.0f}, {-2.66f, 1.0f, 8.53f}, { 0.0f, 0.5f, 15.0f}, 40.0f,
      0.0f, 9.0f, 0.15f, 0.4f, 1.6f, {255, 176, 96}, {200, 140, 80}, 0, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "second: goose",
      0.0f, 0.0f, 1, 0.0f },
    /* M6: third match, struck on the cut; the tree (4.5 m: the lens is wider and aims higher to hold its top and the seated
     * girl together); crane from 1.0 m to 2.4 m (she sits: a higher crane would lose her). 62 degrees. */
    { "M6",  67.0f,  79.0f, {-2.28f, 1.0f, 9.45f}, { 1.8f, 1.85f, 17.0f}, {-2.28f, 2.4f, 9.45f}, { 1.8f, 1.85f, 17.0f}, 62.0f,
      0.0f, 12.0f, 0.15f, 0.4f, 1.6f, {255, 176, 96}, {120, 180, 120}, 1, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "tree, candles rise",
      0.0f, ML_DISSOLVE_S, 1, 0.0f },
    /* M7: no match; a star falls across the sky (camera tilted up). 50 degrees. Hard cut out (into M8's strike). */
    { "M7",  79.0f,  85.0f, {-2.28f, 1.0f, 9.45f}, { 0.0f, 7.0f, 30.0f}, {-2.28f, 1.0f, 9.45f}, { 0.0f, 7.0f, 30.0f}, 50.0f,
      -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, {255, 176, 96}, {0, 0, 0}, 0, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "falling star",
      ML_DISSOLVE_S, 0.0f, 0, 0.0f },
    /* M8: fifth match, struck on the cut; the grandmother, a soft figure in the pool. 40 degrees. Hard cut out (into M9's strike). */
    { "M8",  85.0f,  94.0f, {-3.04f, 1.0f, 7.60f}, { 0.0f, 0.5f, 15.0f}, {-3.04f, 1.0f, 7.60f}, { 0.0f, 0.5f, 15.0f}, 40.0f,
      0.0f, 9.0f, 0.15f, 0.4f, 1.4f, {255, 176, 96}, {250, 230, 200}, 0, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "grandmother",
      0.0f, 0.0f, 1, 0.0f },
    /* M9: the whole bundle, the largest pool (2.5 m radius, pale gold), struck on the cut. 50 degrees. The lens is level
     * (1.2 m): in the 640 x 360 middle band of the frame the staging is judged in, her ground is about 75 % down, her head
     * top about 58 %, and the spirits' heads end about 8 % under the band's top. Its way out is the
     * long dissolve: 1 s to black here and 1 s up in M10. */
    { "M9",  94.0f, 108.0f, {-3.42f, 1.2f, 6.68f}, { 0.0f, 1.2f, 15.0f}, {-3.42f, 1.2f, 6.68f}, { 0.0f, 1.2f, 15.0f}, 50.0f,
      0.0f, 14.0f, 0.05f, 0.4f, 2.5f, {255, 214, 150}, {250, 230, 200}, 0, 0, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "all matches flare",
      0.0f, 1.0f, 1, 0.0f },
    /* M10: dawn, cool blue, no warmth; lens 0.8 m high, a slow pull from 6 m to 9 m over 9 s, then a 3 s hold. 60 degrees.
     * She lies across the frame, about 70 % down the 16:9 picture at 6 m and 64 % at 9 m. Fades to black over its last second. */
    { "M10", 108.0f, 120.0f, {-2.28f, 0.8f, 9.45f}, { 0.0f, 0.89f, 15.0f}, {-3.42f, 0.8f, 6.68f}, { 0.0f, 0.93f, 15.0f}, 60.0f,
      -1.0f, 0.0f, 0.0f, 0.0f, 0.0f, {255, 176, 96}, {0, 0, 0}, 0, 1, ML_GIRL_STAND_Z, ML_GIRL_STAND_Z, "dawn, still girl",
      1.0f, 1.0f, 0, 9.0f },
};

/* A shot index that is safe to use on ml_shots: `shot` itself when it names a shot, else the nearest end of the table.
 * Every function below that takes a shot goes through it, so a wrong index can never read outside the table. */
static inline int ml_shot_clamp(int shot) {
    if (shot < 0) {
        return 0;
    }
    return shot >= ML_SHOTS ? ML_SHOTS - 1 : shot;
}

/* ------------------------------------------------------------------------------------------------------------- */
/* The end credits: silent-film intertitle cards after the film. Card k is fully shown for ML_CREDIT_HOLD_S; between two
 * cards lies a dissolve of ML_CREDIT_DISSOLVE_S (one fades out as the next fades in, over a near-black ground); the
 * first card comes up from black inside its first ML_CREDIT_IN_S, and the last fades to black over ML_CREDIT_OUT_S. */
#define ML_CREDIT_CARDS  8u
#define ML_CREDIT_HOLD_S 3.0f
#define ML_CREDIT_DISSOLVE_S 0.5f
#define ML_CREDIT_IN_S   1.0f
#define ML_CREDIT_OUT_S  1.0f
/* Length of the credits, seconds: 8 holds, 7 dissolves and the last fade = 28.5 s. */
#define ML_CREDITS_SECONDS ((float)ML_CREDIT_CARDS * ML_CREDIT_HOLD_S + (float)(ML_CREDIT_CARDS - 1u) * ML_CREDIT_DISSOLVE_S + ML_CREDIT_OUT_S)
/* The title card: shown full screen before the first shot, over the first shot's first frame (the card is slightly
 * clear, so the falling snow glimmers through it). It comes up from black over ML_TITLE_IN_S and goes down to black
 * over ML_TITLE_OUT_S; the first shot then fades up from that black. On the clock the card is the LAST stretch of the
 * loop and the program starts its clock there: title, film, credits, title, ... with every film time unchanged. */
#define ML_TITLE_SECONDS 7.0f
#define ML_TITLE_IN_S    1.0f
#define ML_TITLE_OUT_S   1.0f
/* Clock time at which the title card begins: after the film and the credits, seconds. */
#define ML_TITLE_START_S (ML_FILM_SECONDS + ML_CREDITS_SECONDS)
/* The whole loop: the film, the credits, the title card, seconds. The clock wraps at this value. */
#define ML_LOOP_SECONDS  (ML_TITLE_START_S + ML_TITLE_SECONDS)

/* How much of the title card shows at `s` seconds into it, 0 (black) .. 1. */
static inline float ml_title_alpha(float s) {
    const float in = sh4der_sat_(sh4der_div_(s, ML_TITLE_IN_S));
    const float out = sh4der_sat_(sh4der_div_(ML_TITLE_SECONDS - s, ML_TITLE_OUT_S));

    if (s < 0.0f) {
        return 0.0f;
    }
    return in < out ? in : out;
}

/* Colours of a card, 0..255: the near-black ground, the paper-white ink, the hairline border. The one accent, a short
 * rule under the label, is the match's amber (ML_WARM_*). */
#define ML_CREDIT_GROUND_R 14
#define ML_CREDIT_GROUND_G 12
#define ML_CREDIT_GROUND_B 10
#define ML_CREDIT_INK_R  232
#define ML_CREDIT_INK_G  226
#define ML_CREDIT_INK_B  214
#define ML_CREDIT_LINE_R 60
#define ML_CREDIT_LINE_G 56
#define ML_CREDIT_LINE_B 50
/* The card's own letterbox inside the 16:9 picture, its border and its accent rule, pixels. */
#define ML_CREDIT_BAR    46             /* black rows at the top and bottom of the 360-row picture */
#define ML_CREDIT_INSET  20             /* the hairline border lies this far inside the active picture */
#define ML_CREDIT_TICK   6              /* length of the tick that crosses each border corner */
#define ML_CREDIT_RULE_HALF 12          /* half the accent rule's length: 24 pixels, centred */

/* How strongly card `card` shows at `s` seconds into the credits, 0..1. */
static inline float ml_credit_alpha(unsigned card, float s) {
    const float start = (float)card * (ML_CREDIT_HOLD_S + ML_CREDIT_DISSOLVE_S);   /* the card is fully shown from here */
    const float end = start + ML_CREDIT_HOLD_S;                                    /* to here */
    float in, out;

    if (card >= ML_CREDIT_CARDS || s < 0.0f) {
        return 0.0f;
    }
    in = card == 0u ? sh4der_sat_(sh4der_div_(s, ML_CREDIT_IN_S)) : sh4der_sat_(sh4der_div_(s - (start - ML_CREDIT_DISSOLVE_S), ML_CREDIT_DISSOLVE_S));
    out = card == ML_CREDIT_CARDS - 1u ? sh4der_sat_(sh4der_div_(end + ML_CREDIT_OUT_S - s, ML_CREDIT_OUT_S))
                                       : sh4der_sat_(sh4der_div_(end + ML_CREDIT_DISSOLVE_S - s, ML_CREDIT_DISSOLVE_S));
    return in < out ? in : out;
}

/* How strongly the cards' shared frame (ground and border) shows at `s` seconds into the credits, 0..1: up with the
 * first card, down with the last, full in between (during a dissolve the ground does not dip). */
static inline float ml_credit_frame_alpha(float s) {
    if (s < 0.0f) {
        return 0.0f;
    }
    if (s < ML_CREDIT_IN_S) {
        return ml_credit_alpha(0u, s);
    }
    return s > ML_CREDITS_SECONDS - ML_CREDIT_OUT_S ? ml_credit_alpha(ML_CREDIT_CARDS - 1u, s) : 1.0f;
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Timeline. */

/* Where the film clock is: which shot, how far into it, and the wrapped clock itself. */
typedef struct ml_where {
    int shot;                           /* 0 .. ML_SHOTS - 1 */
    float progress;                     /* 0 .. 1 inside the shot */
    float seconds;                      /* seconds since the shot started */
    float clock;                        /* the clock wrapped into 0 .. ML_LOOP_SECONDS */
    float credits_s;                    /* seconds into the end credits; negative outside them */
    float title_s;                      /* seconds into the title card; negative outside it */
} ml_where_t;

/* Wraps a clock value (any float seconds, including negative) into 0 .. ML_LOOP_SECONDS: film, credits and title loop. */
static inline float ml_wrap_clock(float t) {
    const float loops = sh4der_div_(t, ML_LOOP_SECONDS);
    float r;

    if (t >= 0.0f && t < ML_LOOP_SECONDS) {
        return t;                        /* already inside the loop: returned exactly, so a cut's time stays that cut's time */
    }
    r = t - ML_LOOP_SECONDS * (loops - sh4der_frac_(loops));   /* less the whole loops (frac is x - floor(x): negatives wrap upward) */
    if (r < 0.0f) {                      /* rounding at a loop's very edge can leave it a hair outside: bring it in */
        r += ML_LOOP_SECONDS;
    }
    return r >= ML_LOOP_SECONDS ? r - ML_LOOP_SECONDS : r;
}

/* Finds the shot for clock time t (looping at ML_LOOP_SECONDS) and the 0 .. 1 progress inside it. During the end
 * credits the shot is the last one, at its end (a black frame), and credits_s says how far the credits are. During
 * the title card the shot is the first one at its first moment (the frame the card lies over), and title_s says how
 * far the card is.
 * In the film: Shots tile the film with no gap:
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
    w->credits_s = c < ML_TITLE_START_S ? c - ML_FILM_SECONDS : -1.0f;
    w->title_s = c >= ML_TITLE_START_S ? c - ML_TITLE_START_S : -1.0f;
    if (w->title_s >= 0.0f) {
        w->shot = 0;                     /* the card lies over the first shot's first frame */
    }
    w->seconds = w->title_s >= 0.0f ? 0.0f : (c < ML_FILM_SECONDS ? c : ML_FILM_SECONDS) - ml_shots[w->shot].start_s;
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
    s = &ml_shots[shot];                 /* in range: checked above */
    e = ml_ease(s->move_s > 0.0f ? sh4der_div_(progress * (s->end_s - s->start_s), s->move_s) : progress);   /* ml_ease holds at 1 past the move's end */
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
/* The closed fog of the shot where she sits down (M4b): it begins just behind her and is complete before the nearest
 * house front in frame, so the street behind her is fog only: no front, no edge, no sky. */
#define ML_FOG_CLOSED_START_Z 8.0f      /* she is 7 m from that lens */
#define ML_FOG_CLOSED_END_Z   12.5f     /* the nearest house point in that frame is 13.1 m deep */
/* The fog's colour, 0..1 per channel: the fog table's colour and, in a closed-fog shot, the background too. */
#define ML_FOG_R         0.20f
#define ML_FOG_G         0.22f
#define ML_FOG_B         0.32f

/* 1 when the fog is closed in behind the girl in this shot. */
static inline int ml_fog_closed(int shot) {
    return shot == ML_SHOT_M4B;
}

/* The fog table's range in a shot: the depth where the fog begins and the depth where it is complete, metres. */
static inline void ml_fog_range(int shot, float *start, float *end) {
    *start = ml_fog_closed(shot) ? ML_FOG_CLOSED_START_Z : ML_FOG_START_Z;
    *end = ml_fog_closed(shot) ? ML_FOG_CLOSED_END_Z : ML_FOG_END_Z;
}

/* Share of its colour that a thing at depth z keeps in a shot: 1 at the start of the fog, 0 at the end (linear, like the table). */
static inline float ml_fog_keep(int shot, float z) {
    float start, end;

    ml_fog_range(shot, &start, &end);
    return sh4der_sat_(sh4der_div_(end - z, end - start));
}

/* ------------------------------------------------------------------------------------------------------------- */
/* The girl. A flat dark silhouette of four quads that always turns to face the camera about the vertical axis. */

#define ML_GIRL_QUADS    3              /* dress, shawl, hands (the head is a round fan, see ml_girl_head) */
#define ML_GIRL_HEIGHT   1.1f           /* total height, metres */
#define ML_GIRL_LAYER    0.01f          /* each quad sits this much nearer the camera than the one before, so none shares a plane, metres */
#define ML_STRIDE        0.5625f         /* distance between two bare footprints along her walk, metres */
#define ML_FOOT_SIDE     0.12f          /* each foot's row is this far to the side of her path, metres */
#define ML_WALK_DIST     (ML_GIRL_WALK_Z0 - ML_GIRL_STAND_Z)   /* length of the M2 walk, metres */
#define ML_STEPS_TOTAL   17u            /* footprints in the whole M2 walk: floor(ML_WALK_DIST / ML_STRIDE) + 1 */

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
    ml_flat_quad(pos, side, toward, 0.0f, 0.46f, 0.08f, 0.58f, 0.08f, 2.0f * ML_GIRL_LAYER, &out[2]);   /* hands */
}

/* The seated girl (from the shot where she stays out in the cold to the shot of the whole bundle): upright back, bowed head, knees drawn up, an arm down to
 * the knees; a flat silhouette with no face. Four quads in the camera-facing plane, and the head. */
#define ML_GIRL_SEATED_QUADS 4          /* skirt on the snow, back, knees, arm (the head is a round fan, see ml_girl_head) */
#define ML_GIRL_MAX_QUADS    4          /* the most quads either pose has */
#define ML_GIRL_HEAD_SIDES   8u         /* the head is a fan of this many triangles: an octagon, round at this size on screen */
#define ML_GIRL_HEAD_RADIUS  0.09f      /* head radius, metres */
#define ML_GIRL_HEAD_SEATED_X 0.06f     /* the seated head's centre across the camera-facing plane, metres: bowed toward the knees */
#define ML_GIRL_SEATED_HEIGHT 0.80f     /* top of her bowed head over the snow, metres */

/* The seated pose: per quad, four corners in strip order (bottom left, top left, bottom right, top right), each as
 * (metres across the camera-facing plane from her place, positive to the camera's right; metres up). She faces the
 * camera's left: the knees are on the left, the back on the right. */
static const float ml_girl_seated_pts[ML_GIRL_SEATED_QUADS][4][2] = {
    { {-0.44f, 0.00f}, {-0.44f, 0.08f}, { 0.26f, 0.00f}, { 0.26f, 0.08f} },   /* skirt and feet, flat on the snow */
    { {-0.04f, 0.00f}, { 0.00f, 0.59f}, { 0.24f, 0.00f}, { 0.20f, 0.59f} },   /* back and shawl, upright */
    { {-0.34f, 0.00f}, {-0.26f, 0.41f}, { 0.02f, 0.00f}, {-0.06f, 0.34f} },   /* knees drawn up */
    { {-0.24f, 0.34f}, {-0.22f, 0.42f}, { 0.06f, 0.45f}, { 0.08f, 0.53f} },   /* arm, from the shoulder down to the knees */
};

/* The girl lying in the snow (the last shot, the morning: she has died in the night): a long low shape, feet to the
 * camera's left, head to its right. Two quads and the head, in the same corner layout as the seated pose. */
#define ML_GIRL_LYING_QUADS  2          /* skirt and legs, body */
#define ML_GIRL_LYING_HEIGHT 0.22f      /* top of her head over the snow, metres */
#define ML_GIRL_HEAD_LYING_X 0.50f      /* the lying head's centre across the camera-facing plane, metres */
static const float ml_girl_lying_pts[ML_GIRL_LYING_QUADS][4][2] = {
    { {-0.55f, 0.00f}, {-0.55f, 0.10f}, {-0.05f, 0.00f}, {-0.05f, 0.18f} },   /* skirt and legs */
    { {-0.05f, 0.00f}, {-0.05f, 0.18f}, { 0.42f, 0.00f}, { 0.42f, 0.20f} },   /* body and shawl */
};

/* The girl's pose in a shot. */
enum { ML_POSE_STAND, ML_POSE_SEATED, ML_POSE_LYING };

/* Her pose by shot: she stands until M4; she sits down in M4b and stays in her corner through M9 (her body does not rise:
 * what rises in M9 is her spirit, see ml_spirit_build); in M10, the morning, she lies in the snow. */
static inline int ml_girl_pose(int shot) {
    if (shot < ML_SHOT_M4B) {
        return ML_POSE_STAND;
    }
    return shot == ML_SHOT_M10 ? ML_POSE_LYING : ML_POSE_SEATED;
}

/* 1 when the girl sits in this shot. */
static inline int ml_girl_seated(int shot) {
    return ml_girl_pose(shot) == ML_POSE_SEATED;
}

/* The top of her head over the snow in a pose, metres. */
static inline float ml_girl_pose_height(int pose) {
    return pose == ML_POSE_SEATED ? ML_GIRL_SEATED_HEIGHT : (pose == ML_POSE_LYING ? ML_GIRL_LYING_HEIGHT : ML_GIRL_HEIGHT);
}

/* Builds n quads of a pose table (corners as in ml_girl_seated_pts) at world position `pos`, turned to the camera. Each
 * quad sits ML_GIRL_LAYER nearer the camera than the one before, so none shares a plane. */
static inline void ml_girl_build_pts(const ml_camera_t *c, const float pos[3], const float pts[][4][2], int n, ml_quad_t out[]) {
    float side[3], toward[3];
    int q, i, k;

    ml_flat_basis(c, side, toward);
    for (q = 0; q < n; q++) {
        for (i = 0; i < 4; i++) {
            for (k = 0; k < 3; k++) {
                out[q].p[i][k] = sh4der_fma_(side[k], pts[q][i][0], sh4der_fma_(toward[k], (float)q * ML_GIRL_LAYER, pos[k]));
            }
            out[q].p[i][1] += pts[q][i][1];
        }
    }
}

/* Builds the seated girl's four quads at world position `pos` (where she sits), turned to the camera. */
static inline void ml_girl_build_seated(const ml_camera_t *c, const float pos[3], ml_quad_t out[ML_GIRL_SEATED_QUADS]) {
    ml_girl_build_pts(c, pos, ml_girl_seated_pts, ML_GIRL_SEATED_QUADS, out);
}

/* Builds the lying girl's two quads at world position `pos`, turned to the camera. */
static inline void ml_girl_build_lying(const ml_camera_t *c, const float pos[3], ml_quad_t out[ML_GIRL_LYING_QUADS]) {
    ml_girl_build_pts(c, pos, ml_girl_lying_pts, ML_GIRL_LYING_QUADS, out);
}

/* The girl's head for a pose (ML_POSE_*): a round fan in the camera-facing plane, in front of her body. out[0] is the
 * centre, out[1 .. ML_GIRL_HEAD_SIDES] the rim; triangle k is (0, k + 1, (k + 1) % ML_GIRL_HEAD_SIDES + 1). Its top is
 * the pose's height (ml_girl_pose_height). */
static inline void ml_girl_head(const ml_camera_t *c, const float pos[3], int pose, float out[ML_GIRL_HEAD_SIDES + 1u][3]) {
    const float cx = pose == ML_POSE_SEATED ? ML_GIRL_HEAD_SEATED_X : (pose == ML_POSE_LYING ? ML_GIRL_HEAD_LYING_X : 0.0f);
    const float cy = ml_girl_pose_height(pose) - ML_GIRL_HEAD_RADIUS;
    float side[3], toward[3];
    unsigned i;
    int k;

    ml_flat_basis(c, side, toward);
    for (k = 0; k < 3; k++) {
        out[0][k] = sh4der_fma_(side[k], cx, sh4der_fma_(toward[k], (float)ML_GIRL_MAX_QUADS * ML_GIRL_LAYER, pos[k]));
    }
    out[0][1] += cy;
    for (i = 0; i < ML_GIRL_HEAD_SIDES; i++) {
        float sn, cs;

        sh4der_sincos_((float)i * (6.2831853f / (float)ML_GIRL_HEAD_SIDES), &sn, &cs);
        for (k = 0; k < 3; k++) {
            out[i + 1u][k] = sh4der_fma_(side[k], cs * ML_GIRL_HEAD_RADIUS, out[0][k]);
        }
        out[i + 1u][1] = sh4der_fma_(sn, ML_GIRL_HEAD_RADIUS, out[0][1]);
    }
}

/* The girl's position at a shot and progress (feet on the ground, centre of her body). Walks linearly. */
static inline void ml_girl_pos(int shot, float progress, float pos[3]) {
    const ml_shot_t *s = &ml_shots[ml_shot_clamp(shot)];

    pos[0] = 0.0f;
    pos[1] = 0.0f;
    pos[2] = sh4der_lerp_(s->girl_z0, s->girl_z1, sh4der_sat_(progress));
}

/* She creeps along, "shivering with cold and hunger": on the M2 walk her body is stooped by ML_WALK_STOOP and sinks a
 * further ML_WALK_DIP in the middle of every short step. Returns how far her drawn figure is lowered, metres (0 outside M2). */
#define ML_WALK_STOOP    0.06f
#define ML_WALK_DIP      0.04f
static inline float ml_girl_walk_drop(int shot, float progress) {
    float sn, cs;

    if (shot != ML_SHOT_M2) {
        return 0.0f;
    }
    sh4der_sincos_(sh4der_div_(ML_WALK_DIST * sh4der_sat_(progress), ML_STRIDE) * 3.14159265f, &sn, &cs);
    (void)cs;
    return ML_WALK_STOOP + ML_WALK_DIP * sn * sn;
}

/* The middle of her body (half her pose's height): the point a camera has to keep in frame. */
static inline void ml_girl_centre(int shot, float progress, float out[3]) {
    ml_girl_pos(shot, progress, out);
    out[1] = 0.5f * ml_girl_pose_height(ml_girl_pose(shot));
}

/* The print constants ml_foot_shape scales (the trail section below describes the pit). */
#define ML_PRINT_RADIUS  0.12f          /* radius of the largest stamp, metres: small enough that one print's pit ends before the
                                         * next begins (0.36 m of pit in a 0.5625 m step), so the trail is separate prints, not a channel */
#define ML_PRINT_HALF_GAP 0.06f         /* heel and toe stamps sit at most this far either side of the print's middle along the walk, metres */
#define ML_PRINT_DEPTH   0.20f          /* depth of the deepest pit's lowest point under the snow surface, metres */

/* ------------------------------------------------------------------------------------------------------------- */
/* The lift (M9): her spirit is taken away. Two pale figures of light, the grandmother leading the girl by the hand, come
 * out of the seated girl and rise into the night, while her body stays where it sits. Flat shapes drawn as added light,
 * brighter at the top than at the hem, with no face. */

#define ML_SPIRIT_QUADS  5              /* grandmother's robe and head, the girl's dress and head, their joined hands */
#define ML_LIFT_START_S  10.0f          /* seconds into M9 at which the spirits appear and the lights lift off: its last 4 s */
#define ML_LIFT_TIME_S   4.0f           /* seconds the rise takes */
#define ML_LIFT_RISE     1.7f           /* metres they rise in that time: the grandmother's head (1.5 m over her feet) ends 3.2 m up */
#define ML_LIFT_HOVER    0.0f           /* they begin this high over the snow, metres: they come out of her */
#define ML_SPIRIT_FADE_S 1.0f           /* seconds they take to appear */
#define ML_SPIRIT_R      250            /* their light: pale gold-white (250, 230, 200) */
#define ML_SPIRIT_G      230
#define ML_SPIRIT_B      200

/* How strongly the spirits show at `seconds` into `shot`, 0..1: nothing outside M9 or before the lift, then a fade in. */
static inline float ml_spirit_level(int shot, float seconds) {
    if (shot != ML_SHOT_M9 || seconds < ML_LIFT_START_S) {
        return 0.0f;
    }
    return sh4der_sat_(sh4der_div_(seconds - ML_LIFT_START_S, ML_SPIRIT_FADE_S));
}

/* How high the spirits' feet are over the snow at `seconds` into M9, metres: an eased rise from ML_LIFT_HOVER. */
static inline float ml_spirit_height(float seconds) {
    return sh4der_fma_(ml_ease(sh4der_div_(seconds - ML_LIFT_START_S, ML_LIFT_TIME_S)), ML_LIFT_RISE, ML_LIFT_HOVER);
}

/* Builds the five quads of the two spirits over the girl's place `pos`, their feet `height` metres up, turned to the
 * camera: the grandmother (1.5 m) on the camera's left, the girl's spirit (0.9 m) on its right, a band where their hands
 * join. Each quad's corners 0 and 2 are its lower edge, 1 and 3 its upper edge. */
static inline void ml_spirit_build(const ml_camera_t *c, const float pos[3], float height, ml_quad_t out[ML_SPIRIT_QUADS]) {
    const float base[3] = { pos[0], pos[1] + height, pos[2] };
    float side[3], toward[3];

    ml_flat_basis(c, side, toward);
    ml_flat_quad(base, side, toward, -0.34f, 0.00f, 0.30f, 1.25f, 0.14f, 0.20f, &out[0]);                   /* grandmother's robe: 46 % across the M9 frame */
    ml_flat_quad(base, side, toward, -0.34f, 1.28f, 0.09f, 1.50f, 0.09f, 0.20f + ML_GIRL_LAYER, &out[1]);   /* her head */
    ml_flat_quad(base, side, toward,  0.25f, 0.25f, 0.20f, 0.72f, 0.10f, 0.20f, &out[2]);                   /* the girl's dress: 53 % across */
    ml_flat_quad(base, side, toward,  0.25f, 0.75f, 0.075f, 0.92f, 0.075f, 0.20f + ML_GIRL_LAYER, &out[3]); /* her head, 0.6 m below the grandmother's */
    ml_flat_quad(base, side, toward, -0.04f, 0.60f, 0.19f, 0.68f, 0.19f, 0.20f + 2.0f * ML_GIRL_LAYER, &out[4]);   /* joined hands */
}

/* A repeatable "random" share 0 .. 1 for item n of kind `salt`: an integer hash (Wang's 32-bit mix), so the same film
 * always has the same footprints. Integer arithmetic only. */
static inline float ml_hash01(uint32_t n, uint32_t salt) {
    uint32_t h = n * 2654435761u + salt * 40503u + 0x9e3779b9u;

    h = (h ^ 61u) ^ (h >> 16);
    h *= 9u;
    h ^= h >> 4;
    h *= 0x27d4eb2du;
    h ^= h >> 15;
    return (float)(h >> 8) * (1.0f / 16777216.0f);     /* the top 24 bits: exact in a float */
}

/* No two footprints are alike: each is moved a little from its place on the two rows, and has its own depth, size,
 * length and turn of the foot, all from ml_hash01. These are the greatest changes. */
#define ML_FOOT_JITTER_X 0.025f         /* a print lies up to this far to either side of its row, metres */
#define ML_FOOT_JITTER_Z 0.04f          /* and up to this far along the walk from its even place, metres */
#define ML_FOOT_TURN     0.02f          /* the toe stamp is turned in or out by up to this much, metres across */
#define ML_FOOT_DEPTH_MIN 0.60f         /* the shallowest print's share of ML_PRINT_DEPTH (a light step, or snow that held) */
#define ML_FOOT_SIZE_MIN 0.85f          /* the smallest stamp's share of ML_PRINT_RADIUS */
#define ML_FOOT_GAP_MIN  0.75f          /* the shortest print's share of ML_PRINT_HALF_GAP */

/* Where footprint n (0 = the first, at the far end) lies: x, z on the snow. Feet alternate left and right; each print is
 * a little off its even place (see ML_FOOT_JITTER_*). */
static inline void ml_foot(unsigned n, float *x, float *z) {
    *x = ((n & 1u) ? ML_FOOT_SIDE : -ML_FOOT_SIDE) + (2.0f * ml_hash01(n, 1u) - 1.0f) * ML_FOOT_JITTER_X;
    *z = ML_GIRL_WALK_Z0 - (float)n * ML_STRIDE + (n > 0u && n + 1u < ML_STEPS_TOTAL ? (2.0f * ml_hash01(n, 2u) - 1.0f) * ML_FOOT_JITTER_Z : 0.0f);
}

/* The shape of footprint n: *depth of its lowest point under the snow, *radius of its two stamps, *half_gap between the
 * heel and toe stamps' middles and the print's own, and *turn, how far the toe stamp sits to the side of the heel
 * stamp (a foot turned in or out), all metres. The first and last prints of the walk keep their even place along it
 * (ml_foot), so the walk still starts and ends where the shot table says. */
static inline void ml_foot_shape(unsigned n, float *depth, float *radius, float *half_gap, float *turn) {
    *depth = ML_PRINT_DEPTH * sh4der_lerp_(ML_FOOT_DEPTH_MIN, 1.0f, ml_hash01(n, 3u));
    *radius = ML_PRINT_RADIUS * sh4der_lerp_(ML_FOOT_SIZE_MIN, 1.0f, ml_hash01(n, 4u));
    *half_gap = ML_PRINT_HALF_GAP * sh4der_lerp_(ML_FOOT_GAP_MIN, 1.0f, ml_hash01(n, 5u));
    *turn = (2.0f * ml_hash01(n, 6u) - 1.0f) * ML_FOOT_TURN;
}

/* How many footprints exist by this point of the film: none before M2, the walked ones during M2, all after it. */
static inline unsigned ml_steps_due(int shot, float progress) {
    float walked;
    unsigned n;

    if (shot < ML_SHOT_M2) {
        return 0u;
    }
    if (shot > ML_SHOT_M2) {
        return ML_STEPS_TOTAL;
    }
    walked = (ML_GIRL_WALK_Z0 - ML_GIRL_STAND_Z) * sh4der_sat_(progress);
    n = (unsigned)sh4der_div_(walked, ML_STRIDE) + 1u;
    return n > ML_STEPS_TOTAL ? ML_STEPS_TOTAL : n;
}

/* A foot does not appear in the snow, it sinks in, for as long as its footfall sound plays. These are the lengths of the
 * four footfall sounds (step0 .. step3), seconds, as tools/audio.py reports them for the sources in audio_public.json;
 * a change of those sounds changes this table. Every one is shorter than the time between two steps (the test checks). */
static const float ml_step_sound_s[4] = { 0.56f, 0.74f, 0.56f, 0.56f };

/* Which footfall sound goes with footprint n: the program plays this one as the print begins. */
static inline unsigned ml_step_sound(unsigned n) {
    return (n + 1u) % 4u;
}

/* How far footprint n is pressed in by this point of the film, 0 (untouched snow) .. 1 (full depth), eased at both ends.
 * Before M2 nothing is pressed; after it every print is complete. */
static inline float ml_step_press(int shot, float progress, unsigned n) {
    float walked, since_s;

    if (shot < ML_SHOT_M2 || n >= ML_STEPS_TOTAL) {
        return 0.0f;
    }
    if (shot > ML_SHOT_M2) {
        return 1.0f;
    }
    walked = (ML_GIRL_WALK_Z0 - ML_GIRL_STAND_Z) * sh4der_sat_(progress);
    since_s = sh4der_div_((walked - (float)n * ML_STRIDE) * (ml_shots[ML_SHOT_M2].end_s - ml_shots[ML_SHOT_M2].start_s),
                          ML_GIRL_WALK_Z0 - ML_GIRL_STAND_Z);          /* seconds since her foot arrived at print n */
    return ml_ease(sh4der_sat_(sh4der_div_(since_s, ml_step_sound_s[ml_step_sound(n)])));
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

/* House tables: heights, widths and gables vary on purpose, so the street does not read as one repeated shape. The fronts
 * stand 7.0 to 7.6 m out: an open lane with the houses pushed back to its sides. */
static const ml_house_t ml_houses[ML_HOUSES] = {
    /* left side, wall faces +x */
    { -7.0f,  8.0f, 5.0f, 5.5f, 2.0f, 4.4f, 2, 1, 0x01u, 235u },
    { -7.4f, 13.5f, 4.0f, 6.5f, 2.6f, 4.4f, 2, 2, 0x05u, 220u },   /* the nearest left front M1 sees: two lit */
    { -7.0f, 18.0f, 5.5f, 5.0f, 1.8f, 4.4f, 3, 1, 0x05u, 245u },
    { -7.6f, 24.0f, 4.5f, 7.0f, 3.0f, 4.4f, 2, 2, 0x09u, 225u },
    { -7.2f, 29.0f, 6.0f, 5.8f, 2.2f, 4.4f, 3, 1, 0x02u, 240u },
    /* right side, wall faces -x */
    {  7.2f,  6.5f, 4.5f, 6.0f, 2.4f, 4.4f, 2, 2, 0x06u, 230u },
    {  7.0f, 11.5f, 5.5f, 5.2f, 2.0f, 4.4f, 3, 1, 0x06u, 245u },   /* the nearest right front M1 sees: two lit */
    {  7.5f, 17.5f, 4.0f, 7.0f, 2.8f, 4.4f, 2, 2, 0x05u, 222u },
    {  7.0f, 22.0f, 5.0f, 5.5f, 2.0f, 4.4f, 3, 1, 0x04u, 238u },
    {  7.4f, 27.5f, 4.5f, 6.4f, 2.6f, 4.4f, 2, 2, 0x0au, 228u },
};

/* Wall colour at full shade, 0..255 per channel: dark blue-grey. */
#define ML_WALL_R        40
#define ML_WALL_G        46
#define ML_WALL_B        66
/* Window colour, 0..255 per channel: warm amber ("lights were streaming from all the windows": other people's warmth,
 * which she is outside of). */
#define ML_WINDOW_R      255
#define ML_WINDOW_G      196
#define ML_WINDOW_B      120

/* One window rectangle of a house, as a quad in strip order in the plane in front of the wall. Returns 1 when window
 * `idx` (row * win_cols + col) exists, and sets *lit to 1 when it is lit. Returns 0 for an index past the grid. */
static inline int ml_house_window(const ml_house_t *h, unsigned idx, ml_quad_t *q, int *lit) {
    unsigned cols, col, row;
    float zc, y0, x;

    if (!h || !q || !lit || h->win_cols == 0u || idx >= (unsigned)h->win_cols * (unsigned)h->win_rows) {
        return 0;                        /* nothing to divide by, or past the grid */
    }
    cols = h->win_cols;
    col = idx % cols;
    row = idx / cols;
    zc = h->z0 + h->width * sh4der_div_((float)col + 0.5f, (float)cols);   /* window centre along the wall */
    y0 = sh4der_fma_((float)row, ML_WIN_ROW_STEP, ML_WIN_BASE_Y);
    x = h->face_x < 0.0f ? h->face_x + ML_WIN_LIFT : h->face_x - ML_WIN_LIFT;   /* toward the street */
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
#define ML_SNOW_DEPTH    0.30f          /* depth of the hard ground under the level snow: the deepest any dent can go, metres */

/* 1 when ground grid quad (column i, row j) is replaced by the trail patch and must not be drawn. */
static inline int ml_ground_quad_in_trail(unsigned i, unsigned j) {
    return i == ML_TRAIL_GROUND_COL && j >= ML_TRAIL_GROUND_ROW0 && j < ML_TRAIL_GROUND_ROW0 + ML_TRAIL_GROUND_ROWS;
}

/* The tree of the M6 vision ("the most magnificent Christmas tree": 4.5 m, more than five times the seated girl): a
 * camera-facing cut-out of a trunk and three tiers, its candle spots and three round ornaments. */
#define ML_TREE_X        3.0f           /* base of the tree, metres across */
#define ML_TREE_Z        17.0f          /* base of the tree, metres along */
#define ML_TREE_TRIS     5u             /* trunk quad (2) + three tier triangles */
#define ML_TREE_HEIGHT   4.5f           /* height of the tree, metres */
#define ML_TREE_CANDLE_ROWS 6u          /* rows of candles up the tree */
#define ML_TREE_CANDLE_COLS 8u          /* candles in a row */
#define ML_TREE_CANDLES  (ML_TREE_CANDLE_ROWS * ML_TREE_CANDLE_COLS)   /* 48 candle spots, for the star-lift hook */
#define ML_TREE_ORNAMENTS 3u            /* round ornaments */
#define ML_ORNAMENT_RADIUS 0.11f        /* an ornament's radius, metres */

/* Candle spot i in the tree's own plane: *across metres from the trunk, *up metres over the tree's foot. Rows 0.6 m
 * apart from 0.95 m up, each row as wide as the tree is at its height (inside the tiers), every second candle a little
 * higher so a row does not read as a ruled line. */
static inline void ml_tree_candle(unsigned i, float *across, float *up) {
    const unsigned row = (i / ML_TREE_CANDLE_COLS) % ML_TREE_CANDLE_ROWS;
    const unsigned col = i % ML_TREE_CANDLE_COLS;
    const float y = sh4der_fma_((float)row, 0.60f, 0.95f);
    const float hw = sh4der_fma_(1.25f, 1.0f - sh4der_div_(y, ML_TREE_HEIGHT), 0.05f);   /* half width of the row */

    *across = hw * (sh4der_div_(2.0f * (float)col + 1.0f, (float)ML_TREE_CANDLE_COLS) - 1.0f);
    *up = y + ((col & 1u) ? 0.07f : 0.0f);
}

/* The ornaments ("gay pictures" hang on the tale's tree; ours are three glass balls): across and up in the tree's plane,
 * metres, and colour 0..255. Red, gold, blue. */
static const float ml_tree_ornaments[ML_TREE_ORNAMENTS][2] = { {-0.60f, 1.55f}, { 0.48f, 2.35f}, {-0.18f, 3.25f} };
static const uint8_t ml_tree_ornament_rgb[ML_TREE_ORNAMENTS][3] = { { 230, 60, 50 }, { 250, 200, 90 }, { 90, 140, 240 } };

/* The tree tier triangles in the tree's plane: (half width at the bottom, bottom height, top height). */
static const float ml_tree_tier[3][3] = {
    {1.50f, 0.82f, 2.62f}, {1.20f, 1.87f, 3.67f}, {0.82f, 2.85f, 4.50f},
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
#define ML_FLAME_SEATED_DY 0.45f        /* the same height when she sits: her hand rests on her knees */
#define ML_FLAME_DZ      (-0.10f)

/* 1 when this shot has a match at all. */
static inline int ml_shot_lit(int shot) {
    return ml_shots[ml_shot_clamp(shot)].match_on_s >= 0.0f;
}

/* Flame level 0 .. 1 at `seconds` into the shot: 0 before the strike, a rise over fade_in_s, full until match_off_s, then
 * a fall over fade_out_s. A shot with no match is 0 throughout. */
static inline float ml_match_level(int shot, float seconds) {
    const ml_shot_t *s = &ml_shots[ml_shot_clamp(shot)];
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

/* The flame's world position for the girl of `shot` (standing or seated) at `girl`. */
static inline void ml_flame_pos(int shot, const float girl[3], float out[3]) {
    out[0] = girl[0] + ML_FLAME_DX;
    out[1] = girl[1] + (ml_girl_pose(shot) == ML_POSE_STAND ? ML_FLAME_DY : ML_FLAME_SEATED_DY);
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
    return ml_shots[ml_shot_clamp(shot)].dawn ? ml_ease(progress) : 0.0f;
}

/* Sky colour (0..1 per channel) at a shot and progress: night, blending to dawn over the dawn shot. In a closed-fog shot
 * there is no sky: the background is the fog's own colour, so a fogged wall has no outline against it. */
static inline void ml_sky(int shot, float progress, float rgb[3]) {
    const float d = ml_dawn_blend(shot, progress);

    if (ml_fog_closed(shot)) {
        rgb[0] = ML_FOG_R;
        rgb[1] = ML_FOG_G;
        rgb[2] = ML_FOG_B;
        return;
    }
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
/* Cuts: fades from and to black as each shot's in_s / out_s say, and the gold-white flash of a struck match. */

#define ML_FLASH_FRAME_S 0.0334f        /* one frame at 30 Hz, seconds */
#define ML_FLASH_PEAK    0.85f          /* flash alpha on the first frame of a struck-match shot */
#define ML_FLASH_TAIL_S  0.30f          /* the flash has faded away this long after the strike, seconds */
#define ML_FLASH_R       255            /* flash colour: gold-white (255, 236, 190) */
#define ML_FLASH_G       236
#define ML_FLASH_B       190

/* The full-screen overlay for a moment of a shot: *alpha 0 .. 1 and rgb 0..255. Black while the shot fades in (its first
 * in_s seconds) or out (its last out_s seconds). A shot with flash set begins with two frames of gold-white (the strike
 * replaces the cut) and a short tail. */
static inline void ml_overlay(int shot, float seconds, float *alpha, uint8_t rgb[3]) {
    const ml_shot_t *s = &ml_shots[ml_shot_clamp(shot)];
    const float to_end = s->end_s - s->start_s - seconds;
    const float a_in = s->in_s > 0.0f ? sh4der_sat_(1.0f - sh4der_div_(seconds, s->in_s)) : 0.0f;
    const float a_out = s->out_s > 0.0f ? sh4der_sat_(1.0f - sh4der_div_(to_end, s->out_s)) : 0.0f;
    const float dip = a_in > a_out ? a_in : a_out;
    float flash = 0.0f;

    rgb[0] = rgb[1] = rgb[2] = 0;
    if (s->flash) {
        if (seconds < ML_FLASH_FRAME_S) {
            flash = ML_FLASH_PEAK;
        } else if (seconds < 2.0f * ML_FLASH_FRAME_S) {
            flash = 0.5f * ML_FLASH_PEAK;
        } else {
            flash = 0.25f * sh4der_sat_(sh4der_div_(ML_FLASH_TAIL_S - seconds, ML_FLASH_TAIL_S));
        }
    }
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
#define ML_ATLAS_GLOW    0u             /* cell 0: soft round glow (flame glow, pool edge, vision quad) */
#define ML_ATLAS_STAR    1u             /* cell 1: four-point star */
#define ML_ATLAS_STREAK  2u             /* cell 2: a streak (u along its length, bright at the head) */
#define ML_ATLAS_WINDOW  3u             /* cell 3: soft window glow */

/* The grey value 0..1 of texel (tx, ty) (0 .. 63 each) of the atlas. Pure maths, no square root: the soft edges are
 * the library's feather of the squared distance. */
static inline float ml_atlas_value(unsigned tx, unsigned ty) {
    const unsigned cell = (ty / ML_ATLAS_CELL) * 2u + (tx / ML_ATLAS_CELL);   /* 0 .. 3 inside the atlas */
    const float u = sh4der_div_((float)(tx % ML_ATLAS_CELL) + 0.5f, (float)ML_ATLAS_CELL);   /* 0 .. 1 across the cell */
    const float v = sh4der_div_((float)(ty % ML_ATLAS_CELL) + 0.5f, (float)ML_ATLAS_CELL);
    const float cx = sh4der_fma_(u, 2.0f, -1.0f);   /* -1 .. 1 */
    const float cy = sh4der_fma_(v, 2.0f, -1.0f);
    const float r2 = sh4der_len2_(cx, cy);
    const float glow = sh4der_feather_(r2, 0.0f, 1.0f);

    if (tx >= (unsigned)ML_ATLAS_SIDE || ty >= (unsigned)ML_ATLAS_SIDE) {
        return 0.0f;                     /* outside the atlas: no light */
    }
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
/* Sound: the effects and when they play. Every time is seconds into a SHOT, not into the film, so the cues follow the
 * shots when the shot table is re-timed. The girl's footfalls are not in this table: the program plays one each time a
 * footprint is pressed. The wind bed and the flame's crackle are loops the program starts and stops by state. */

/* The sound effects. The program loads sound k from the disc file "sfx/<ml_sound_names[k]>.wav"; a file that is not on
 * the disc is a silent sound, never an error. */
enum {
    ML_SND_WIND,                        /* loop: the wind over the street, the whole film */
    ML_SND_CRACKLE,                     /* loop: the flame, while a match burns */
    ML_SND_STEP0,                       /* four footfalls in snow, played in turn */
    ML_SND_STEP1,
    ML_SND_STEP2,
    ML_SND_STEP3,
    ML_SND_STRIKE,                      /* a match is struck */
    ML_SND_HISS,                        /* a match goes out */
    ML_SND_LIGHTS,                      /* held: the candles flying off the tree, and the last lights and the spirits rising */
    ML_SND_STAR,                        /* the falling star */
    ML_SND_CARRIAGE,                    /* a horse and carriage rushing past: how she lost her slippers */
    ML_SND_SIZZLE,                      /* held: the roast goose */
    ML_SND_FLARE,                       /* the whole bundle is struck at once */
    ML_SND_BELL,                        /* one soft bell at dawn */
    ML_SND_SPIRIT,                      /* the two spirits appear */
    ML_SND_COUNT
};
#define ML_SND_STEPS     4u             /* footfall variants, ML_SND_STEP0 onward */

/* File names of the sounds (without folder or ".wav"), in enum order. */
static const char *const ml_sound_names[ML_SND_COUNT] = {
    "wind", "crackle", "step0", "step1", "step2", "step3", "strike", "hiss", "lights", "star", "carriage", "sizzle", "flare", "bell", "spirit",
};

/* Loudness of the two loops and of a footfall, 0..255 (the AICA channel volume). */
#define ML_VOL_WIND      90
#define ML_WIND_OUT_S    3.0f           /* the wind fades to silence over the film's last this many seconds, so the credits begin silent */
#define ML_VOL_CRACKLE   110
#define ML_VOL_STEP      170
#define ML_PAN_CENTRE    128            /* pan value for the middle (0 = left, 255 = right) */

/* The wind bed's volume at `seconds` into `shot`, 0 .. ML_VOL_WIND: full all through the film, down to nothing over the
 * last ML_WIND_OUT_S of the last shot. */
static inline int ml_wind_volume(int shot, float seconds) {
    const float to_end = ml_shots[ml_shot_clamp(shot)].end_s - ml_shots[ml_shot_clamp(shot)].start_s - seconds;

    if (shot != ML_SHOTS - 1) {
        return ML_VOL_WIND;
    }
    return (int)sh4der_fma_(sh4der_sat_(sh4der_div_(to_end, ML_WIND_OUT_S)), (float)ML_VOL_WIND, 0.5f);
}

/* A held sound: it starts at a moment of a shot, plays under an animation, and is faded out over ML_HOLD_FADE_S when
 * the animation is done (its file is longer than the animation). The fade may run on across the cut. */
typedef struct ml_hold {
    uint8_t shot;                       /* the shot it belongs to */
    float start_s;                      /* seconds into the shot at which it starts */
    float end_s;                        /* seconds into the shot at which its animation is done and the fade begins */
    uint8_t sound;                      /* ML_SND_* */
    uint8_t vol;                        /* volume while it holds, 0..255 */
    uint8_t pan;                        /* 0 = left, 128 = middle, 255 = right */
} ml_hold_t;

#define ML_HOLDS         3u             /* rows of ml_holds */
#define ML_HOLD_FADE_S   0.325f         /* the fade out, seconds */
/* Seconds into M6 at which the last of the tree's candles has come to rest as a star: lift-off at 4 s, the 48th leaves
 * 2.35 s later, up to 1 s of its own delay, 3 s of flight (the rise set-up in matchlight.c) = 10.35 s. */
#define ML_CANDLES_DONE_S 10.4f

/* The held sounds: the sizzle of the roast goose while its match burns (M5; faded out just before the cut to M6's
 * strike); the lights' sound while the candles fly off the tree (M6) and while the last lights and the two spirits rise
 * (M9: from 10 s in to the shot's end).
 * WHO PLAYS WHAT: a sound of at most 65,534 samples is an AICA sound effect (sfx/<name>.wav) and the program plays it
 * from ml_cues. A longer one cannot be (the KallistiOS limit): tools/audio.py lays it into the stream fx.wav at the
 * times of ml_cues and ml_holds, with the holds' fades, and the program only plays that stream. So the rows here and in
 * ml_cues are written with plain numbers, one row a line: the tool reads them. */
static const ml_hold_t ml_holds[ML_HOLDS] = {
    { ML_SHOT_M5,  0.5f,  8.675f, ML_SND_SIZZLE, 190, 150 },
    { ML_SHOT_M6,  4.0f,  10.4f,  ML_SND_LIGHTS, 150, 160 },
    { ML_SHOT_M9,  10.0f, 14.0f,  ML_SND_LIGHTS, 150, 128 },
};

/* One timed sound: which shot, how many seconds into it, which sound, how loud (0..255), where (pan 0..255). */
typedef struct ml_cue {
    uint8_t shot;
    float at_s;
    uint8_t sound;
    uint8_t vol;
    uint8_t pan;
} ml_cue_t;

#define ML_CUES          10u            /* rows of ml_cues */

/* The timed sounds. A strike sits on its shot's match_on_s, the cut itself (the test checks it); the carriage passes as
 * the narrator tells how she lost her slippers; the star's sound starts with the falling star; the spirits' sound as they
 * appear. (The lights' sound and the sizzle are held sounds: ml_holds.) */
static const ml_cue_t ml_cues[ML_CUES] = {
    { ML_SHOT_M2B, 0.3f,  ML_SND_CARRIAGE, 120, 90 },  /* the carriage that took her slippers, passing on the left: a memory, kept low */
    { ML_SHOT_M3,  0.0f,  ML_SND_STRIKE, 230, 128 },   /* first match */
    { ML_SHOT_M4,  0.0f,  ML_SND_HISS,   150, 128 },   /* it goes out */
    { ML_SHOT_M5,  0.0f,  ML_SND_STRIKE, 230, 128 },   /* second match */
    { ML_SHOT_M6,  0.0f,  ML_SND_STRIKE, 230, 128 },   /* third match */
    { ML_SHOT_M7,  0.6f,  ML_SND_STAR,   150, 110 },   /* the falling star, from the left */
    { ML_SHOT_M8,  0.0f,  ML_SND_STRIKE, 230, 128 },   /* the grandmother's match */
    { ML_SHOT_M9,  0.0f,  ML_SND_FLARE,  255, 128 },   /* the whole bundle */
    { ML_SHOT_M9,  10.0f, ML_SND_SPIRIT, 170, 128 },   /* the two spirits appear */
    { ML_SHOT_M10, 0.5f,  ML_SND_BELL,   150, 128 },   /* dawn */
};

/* 1 when cue c falls due on this frame: the film is in the cue's shot and the shot's clock went past the cue's time
 * since the last frame (prev_s < at_s <= s). The caller passes prev_s = -1 on the first frame of a shot (and after any
 * jump backward), so a cue at 0 s plays as its shot begins. */
static inline int ml_cue_due(const ml_cue_t *c, int shot, float prev_s, float s) {
    return c && (int)c->shot == shot && prev_s < c->at_s && c->at_s <= s;
}

/* ------------------------------------------------------------------------------------------------------------- */
/* The tale's props: flat cut-outs that face the lens, as the girl does. A prop is a table of parts; a part is a trapezoid
 * in the prop's camera-facing plane. The slipper and the burned matches are solid and dark; the visions (stove, goose,
 * grandmother) are drawn as added light and exist only while a match burns. */

/* One part of a prop. */
typedef struct ml_part {
    float cx;                           /* middle of the part across the prop's plane, metres from the prop's foot */
    float y0, hw0;                      /* lower edge: height over the foot and half width, metres */
    float y1, hw1;                      /* upper edge: the same */
    uint8_t rgb[3];                     /* colour, 0..255 */
} ml_part_t;

/* Builds part p of a prop standing at `base`, turned to the camera; `layer` lifts it that many ML_GIRL_LAYER steps toward
 * the lens so parts of one prop never share a plane. */
static inline void ml_part_build(const ml_camera_t *c, const float base[3], const ml_part_t *p, unsigned layer, ml_quad_t *q) {
    float side[3], toward[3];

    ml_flat_basis(c, side, toward);
    ml_flat_quad(base, side, toward, p->cx, p->y0, p->hw0, p->y1, p->hw1, (float)layer * ML_GIRL_LAYER, q);
}

/* Where the visions stand: beside the girl and a little behind her, inside the match's pool, metres. */
#define ML_VISION_X      0.95f
#define ML_VISION_Z      15.7f

/* M3, the stove of the tale: "a large iron stove, with polished brass feet, and brass shovel and tongs". Iron parts are a
 * dim warm grey (the light they are made of is the match's), the door glows, the brass is gold: four feet, tongs, shovel. */
#define ML_STOVE_PARTS   10u
#define ML_STOVE_BRASS   6u             /* of those, the last six are brass */
static const ml_part_t ml_stove_parts[ML_STOVE_PARTS] = {
    {  0.00f, 0.12f, 0.30f, 0.95f, 0.28f, {  78,  64,  58 } },   /* the iron body */
    {  0.00f, 0.95f, 0.33f, 1.01f, 0.33f, { 104,  86,  76 } },   /* its top plate */
    {  0.12f, 1.01f, 0.06f, 1.70f, 0.06f, {  64,  54,  50 } },   /* the pipe */
    {  0.00f, 0.30f, 0.16f, 0.62f, 0.16f, { 255, 150,  60 } },   /* the door: the fire behind it */
    { -0.27f, 0.00f, 0.04f, 0.12f, 0.03f, { 240, 196,  84 } },   /* four brass feet */
    { -0.10f, 0.00f, 0.04f, 0.12f, 0.03f, { 240, 196,  84 } },
    {  0.10f, 0.00f, 0.04f, 0.12f, 0.03f, { 240, 196,  84 } },
    {  0.27f, 0.00f, 0.04f, 0.12f, 0.03f, { 240, 196,  84 } },
    {  0.46f, 0.04f, 0.02f, 0.78f, 0.02f, { 240, 196,  84 } },   /* the tongs, leaning by the stove */
    {  0.57f, 0.04f, 0.05f, 0.72f, 0.02f, { 240, 196,  84 } },   /* the shovel: a wide blade, a thin handle */
};

/* M5, the table of the tale: "a snow-white tablecloth" and the dish the goose lies on. */
#define ML_TABLE_PARTS   2u
static const ml_part_t ml_table_parts[ML_TABLE_PARTS] = {
    {  0.00f, 0.00f, 0.52f, 0.55f, 0.46f, { 214, 216, 226 } },   /* the cloth, to the snow */
    {  0.00f, 0.55f, 0.30f, 0.60f, 0.32f, { 190, 168, 120 } },   /* the dish */
};

/* The roast goose: body, neck, head. It lies on the dish, then hops down and waddles toward the lens ("the goose jumped
 * down from the dish, and waddled along the ground ... up to the poor girl"). The knife and fork of the tale are not shown. */
#define ML_GOOSE_PARTS   3u
static const ml_part_t ml_goose_parts[ML_GOOSE_PARTS] = {
    {  0.00f, 0.00f, 0.17f, 0.22f, 0.13f, { 226, 150,  72 } },   /* the body, roasted brown */
    {  0.15f, 0.16f, 0.03f, 0.36f, 0.03f, { 226, 150,  72 } },   /* the neck */
    {  0.19f, 0.33f, 0.05f, 0.42f, 0.04f, { 236, 170,  90 } },   /* the head */
};
#define ML_GOOSE_DISH_Y  0.60f          /* the goose's foot while it lies on the dish, metres over the snow */
#define ML_GOOSE_HOP_S   6.9f           /* seconds into M5 at which it hops off: 2.1 s before the match is out */
#define ML_GOOSE_HOP_TIME_S 0.4f        /* the hop down takes this long, seconds */
#define ML_GOOSE_HOP_ARC 0.15f          /* and rises this far over the straight way down at its middle, metres */
#define ML_GOOSE_WALK_M  1.2f           /* how far it comes toward the lens, metres */
#define ML_GOOSE_WALK_TIME_S 2.0f       /* in this long from the hop's start, seconds */
#define ML_GOOSE_STEPS   5.0f           /* waddle steps on the way */
#define ML_GOOSE_BOB     0.04f          /* each step lifts it this far, metres */

/* Where the goose is at `seconds` into M5: *toward_m metres nearer the lens than the dish, its foot *up_m over the snow. */
static inline void ml_goose_place(float seconds, float *toward_m, float *up_m) {
    const float t = seconds - ML_GOOSE_HOP_S;
    const float hop = sh4der_sat_(sh4der_div_(t, ML_GOOSE_HOP_TIME_S));
    const float walk = sh4der_sat_(sh4der_div_(t, ML_GOOSE_WALK_TIME_S));
    float sn, cs;

    sh4der_sincos_(walk * ML_GOOSE_STEPS * 3.14159265f, &sn, &cs);
    (void)cs;
    *toward_m = walk * ML_GOOSE_WALK_M;
    *up_m = ML_GOOSE_DISH_Y * (1.0f - hop) + 4.0f * ML_GOOSE_HOP_ARC * hop * (1.0f - hop) + (hop >= 1.0f ? ML_GOOSE_BOB * sn * sn : 0.0f);
}

/* M8, the grandmother: "clear and shining like a spirit, yet looking so mild and loving". A robe, a head, both arms open.
 * No face. Pale gold-white, the spirits' colour. */
#define ML_GRANDMOTHER_PARTS 4u
static const ml_part_t ml_grandmother_parts[ML_GRANDMOTHER_PARTS] = {
    {  0.00f, 0.00f, 0.30f, 1.25f, 0.14f, { ML_SPIRIT_R, ML_SPIRIT_G, ML_SPIRIT_B } },   /* the robe */
    {  0.00f, 1.28f, 0.09f, 1.50f, 0.09f, { ML_SPIRIT_R, ML_SPIRIT_G, ML_SPIRIT_B } },   /* the head */
    { -0.34f, 0.92f, 0.20f, 1.04f, 0.18f, { ML_SPIRIT_R, ML_SPIRIT_G, ML_SPIRIT_B } },   /* her open arms */
    {  0.34f, 0.92f, 0.20f, 1.04f, 0.18f, { ML_SPIRIT_R, ML_SPIRIT_G, ML_SPIRIT_B } },
};

/* The vision a shot's match shows as a prop: its parts and their number, or NULL (the tree is a set piece of its own, and
 * M9's grandmother is one of the two spirits). */
static inline const ml_part_t *ml_vision_parts(int shot, unsigned *n) {
    if (shot == ML_SHOT_M3) {
        *n = ML_STOVE_PARTS;
        return ml_stove_parts;
    }
    if (shot == ML_SHOT_M5) {
        *n = ML_TABLE_PARTS;
        return ml_table_parts;
    }
    if (shot == ML_SHOT_M8) {
        *n = ML_GRANDMOTHER_PARTS;
        return ml_grandmother_parts;
    }
    *n = 0u;
    return (const ml_part_t *)0;
}

/* M2b, the slipper she lost crossing the road ("they were very large slippers"): it lies in the snow left of her trail,
 * in the foreground of the low shot. 0.26 m long. Shown while she stands (M1 .. M4). */
#define ML_SLIPPER_X     (-1.05f)
#define ML_SLIPPER_Z     13.4f
#define ML_SLIPPER_PARTS 2u
static const ml_part_t ml_slipper_parts[ML_SLIPPER_PARTS] = {
    {  0.00f, 0.00f, 0.13f, 0.06f, 0.12f, { 44, 30, 24 } },      /* the sole and sides */
    {  0.06f, 0.06f, 0.07f, 0.11f, 0.04f, { 58, 38, 28 } },      /* the toe cap */
};

/* M10, the burned matches by her hand ("still holding the matches, one bundle of which was burned"): three black stubs
 * on the snow, a little toward the lens from where she lies. 0.09 m long each (larger than life, to read at 6 m). */
#define ML_STUB_PARTS    3u
#define ML_STUB_LAYER    30u            /* they lie this many ML_GIRL_LAYER steps (0.3 m) toward the lens from her */
static const ml_part_t ml_stub_parts[ML_STUB_PARTS] = {
    { -0.20f, 0.00f, 0.045f, 0.022f, 0.045f, { 10, 8, 8 } },
    { -0.08f, 0.00f, 0.045f, 0.022f, 0.045f, { 10, 8, 8 } },
    {  0.03f, 0.01f, 0.045f, 0.032f, 0.045f, { 10, 8, 8 } },      /* this one lies across another */
};

/* The falling star of M7 ("left a long streak of fire"): its path across the sky, upper left to lower right, metres, and
 * its tail: ML_FALL_TAIL quads behind the head, each trailing the one before by ML_FALL_TAIL_GAP of the path, so the
 * streak is 64 % of the path long: more than 40 % of the frame's width (the test measures it). */
static const float ml_fall_from[3] = { -11.0f, 16.0f, 40.0f };
static const float ml_fall_to[3] = { 9.0f, 8.5f, 40.0f };
#define ML_FALL_TAIL     16u
#define ML_FALL_TAIL_GAP 0.04f

/* The light that fills the whole frame, not only the pool. M8: the grandmother's brightness is "all round": a steady warm
 * lift. M9: the bundle is "brighter than noonday": a peak held ML_LIFT_PEAK_S from the strike, which then settles over
 * ML_LIFT_SETTLE_S. Returns how much of the lift colour is ADDED to every pixel, 0..1, following the match's own level. */
#define ML_LIFT_M8       0.10f
#define ML_LIFT_M9_PEAK  0.45f
#define ML_LIFT_M9_REST  0.14f
#define ML_LIFT_PEAK_S   1.0f
#define ML_LIFT_SETTLE_S 1.0f
static inline float ml_lift_level(int shot, float seconds) {
    const float match = ml_match_level(shot, seconds);

    if (shot == ML_SHOT_M8) {
        return ML_LIFT_M8 * match;
    }
    if (shot == ML_SHOT_M9) {
        const float settle = ml_ease(sh4der_sat_(sh4der_div_(seconds - ML_LIFT_PEAK_S, ML_LIFT_SETTLE_S)));

        return sh4der_lerp_(ML_LIFT_M9_PEAK, ML_LIFT_M9_REST, settle) * (seconds < ML_LIFT_PEAK_S ? 1.0f : match);
    }
    return 0.0f;
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
    n += 2u * (ml_girl_seated(shot) ? ML_GIRL_SEATED_QUADS : ML_GIRL_QUADS) + ML_GIRL_HEAD_SIDES;   /* the girl (the seated pose has the most quads) and her head */
    n += shot == ML_SHOT_M9 ? 2u * ML_SPIRIT_QUADS : 0u;      /* the two spirits of the lift */
    n += shot <= ML_SHOT_M4 ? 2u * ML_SLIPPER_PARTS : 0u;     /* the lost slipper */
    n += shot == ML_SHOT_M10 ? 2u * ML_STUB_PARTS : 0u;       /* the burned matches */
    n += shot == ML_SHOT_M3 ? 2u * ML_STOVE_PARTS : 0u;       /* the stove */
    n += shot == ML_SHOT_M5 ? 2u * (ML_TABLE_PARTS + ML_GOOSE_PARTS) : 0u;   /* the table and the goose */
    n += shot == ML_SHOT_M8 ? 2u * ML_GRANDMOTHER_PARTS : 0u; /* the grandmother */
    n += shot >= ML_SHOT_M6 ? 2u * ML_TREE_CANDLES : 0u;      /* the candles, on the tree and then as stars */
    n += ml_shots[ml_shot_clamp(shot)].has_tree ? 2u * ML_TREE_ORNAMENTS : 0u;   /* the ornaments */
    n += shot == ML_SHOT_M7 ? 2u * (ML_FALL_TAIL + 1u) : 0u;  /* the falling star and its streak */
    n += (shot == ML_SHOT_M8 || shot == ML_SHOT_M9) ? 2u : 0u;   /* the whole-frame lift */
    n += ml_shots[ml_shot_clamp(shot)].has_tree ? ML_TREE_TRIS : 0u;         /* the tree */
    if (ml_shot_lit(shot)) {
        n += ML_POOL_FANS * ML_POOL_SEGMENTS;                 /* pool fans */
        n += 2u * 2u;                                         /* flame glow + hot core quads */
        n += 2u;                                              /* vision quad */
    }
    n += 2u * ML_GLOW_QUADS;                                  /* window glows */
    n += 2u * ML_SNOW_QUADS;                                  /* falling snow */
    n += 2u;                                                  /* the full-screen overlay (cuts, flash) */
    n += 2u;                                                  /* the debug caption */
    n += ML_LETTERBOX ? 4u : 0u;                              /* the two letterbox bars */
    n += 2u * ML_SUB_MAX_GLYPHS;                              /* the subtitle's letters */
    return n;
}

#endif /* MATCHLIGHT_SCENE_H */
