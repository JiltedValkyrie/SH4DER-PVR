/* SH4DER-PVR: temporal anti-aliasing

   sh4der_taa.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_taa.h
    \brief   The maths of temporal anti-aliasing on the PVR, with no shader.

    Each frame the picture is drawn with a sub-pixel jitter (a Halton 2, 3
    sequence over SH4DER_TAA_PHASES phases, centred so a cycle sums to zero),
    then last frame's picture is blended in at a weight. The blend is the
    client's: the PVR has no shader, so the history is the frame on screen
    (read through the front buffer, 0 VRAM) drawn as translucent full-screen
    layers. This header is the numbers for those layers:

    - Jitter: sh4der_taa_jitter gives the phase's offset in pixels (added to
      the projection centre, screen y down); sh4der_taa_jitter_clip gives the
      same in clip units for a projection matrix.
    - Reprojection: the camera's yaw and pitch change since last frame moves
      the history. sh4der_taa_shift is the content's shift at the screen
      centre (one offset for a uniform quad); sh4der_taa_warp is the exact
      pure-rotation mapping of any pixel, for the history quad's corners.
    - Weight: sh4der_taa_weight, from the content's pixel speed (the centre
      shift squared): full while still, none at a fast turn. A turn of
      SH4DER_TAA_CUT_RAD in one frame is a cut: no history that frame.
    - Blend: sh4der_taa_keep_argb dims the frame to (1 - weight) and
      sh4der_taa_add_argb is the history's colour at weight, both alpha 0 (the
      layers the feedback path draws, the same blend the copy passes use).

    Order, per frame: sh4der_taa_step once (it fills the jitter, the shift and
    the weight). Apply the jitter to the projection, draw the scene, then the
    history layers, if the weight is not 0.

    Yaw turning right and pitch looking up are positive. A client whose yaw
    turns the other way passes the negated delta.

    Version: v0.50
    The effect: smooths jagged edges over time. The picture is shifted a
      fraction of a pixel each frame and each frame is blended with the ones
      before, so an edge averages out.
    Developed for: softer edges at 640 x 480 in Halo: Dreamcast Evolved.
    Use it for: smoother edges where the camera rests: cutscenes, menus with a
      3D backdrop, a photo mode, strategy and puzzle games.

    Recommended settings (the author's own game and demo disc):
    - Dynamic weight, not one fixed number. Camera still: the old frame's
      share is 0.675 (0.90 x SH4DER_TAA_WEIGHT_HI). Camera moving: 0.30.
      Between the two, mix by the slower of two measures:
          f = min(out.weight / SH4DER_TAA_WEIGHT_HI,          (turning)
                  1 - saturate(walk_speed / walk_full))        (walking)
          weight = out.cut ? 0 : lerp(0.30, still, f)
      with walk_full the walking speed at which the camera counts as moving
      (the demo: 1.5 world units a second, a little over half its walking
      pace). No smoothing over time: the weight follows the camera at once.
    - Never above 0.9: past it a ghost lasts ten frames and more.
    - Keep the blended result as the next frame's history (it accumulates:
      a ghost lasts about 1 / (1 - weight) frames, 3 at 0.675). Blending only
      the one frame before is sharper in motion and flickers more at rest.
    - A cut (out.cut) blends nothing that frame.
    - Use it with the 320 x 240 frame texture (sh4der_ca.h): two history
      textures are 320 KB there, 1,280 KB at 640 x 480.

    \author Jilted Valkyrie
    \author Cortana (Claude, an AI by Anthropic): the effects' maths, the PVR paths, the tests
*/

#ifndef SH4DER_TAA_H
#define SH4DER_TAA_H

#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_TAA_PHASES      8       /**< \brief Jitter phases in a cycle (Halton 2, 3). */
#define SH4DER_TAA_WEIGHT_HI   0.75f   /**< \brief History weight while the camera is still. */
#define SH4DER_TAA_WEIGHT_LO   0.0f    /**< \brief History weight at SPEED_HI and faster. */
#define SH4DER_TAA_SPEED_LO    1.0f    /**< \brief Content speed (pixels a frame) up to which the weight is full. */
#define SH4DER_TAA_SPEED_HI    6.0f    /**< \brief Content speed (pixels a frame) from which the weight is LO. */
#define SH4DER_TAA_CUT_RAD     0.35f   /**< \brief A turn of this many radians in one frame (about 20 degrees) is a cut. */
#define SH4DER_TAA_JITTER_AMP  1.0f    /**< \brief Scale on the jitter (1: the Halton range, centred). */
/** @} */

/** \brief One frame's numbers, from sh4der_taa_step. */
typedef struct sh4der_taa_frame {
    float weight;      /**< \brief History weight 0..WEIGHT_HI. 0: no history layer this frame (skip its passes). */
    float jitter[2];   /**< \brief Pixels, added to the projection centre (screen x, y down). */
    float shift[2];    /**< \brief The history's content shift at the screen centre, pixels (0 with no history). */
    int copy_px[2];    /**< \brief The history copy's whole offset: x even (sh4der_taa_even_px), y whole (sh4der_taa_whole_px). */
    int cut;           /**< \brief 1: first frame, or a cut (turn over SH4DER_TAA_CUT_RAD): no history. */
} sh4der_taa_frame_t;

/** \brief One camera's TAA state. Call sh4der_taa_init once at load (it fills the jitter table). */
typedef struct sh4der_taa {
    float jit[SH4DER_TAA_PHASES][2];   /**< \brief The cycle's jitter, pixels, centred (sums to zero). */
    float yaw, pitch;                  /**< \brief Last frame's camera angles, radians. */
    unsigned frame;                    /**< \brief Frames stepped: the phase is frame % PHASES. */
    int ready;                         /**< \brief 0 until the first step. */
} sh4der_taa_t;

/* ------------------------------------------------------------------------ */
/* The steps (inline: the client calls them only where it uses them). */

static inline float sh4der_taa_abs_(float x) { return x < 0.0f ? -x : x; }

/** \brief Radical inverse of `index` in `base`: the Halton value, 0..1. index 0 gives 0. */
static inline float sh4der_taa_halton(unsigned index, unsigned base) {
    const float inv = sh4der_div_(1.0f, (float)base);
    float f = inv, r = 0.0f;
    unsigned i = index;
    while (i) {
        r += f * (float)(i % base);
        i /= base;
        f *= inv;
    }
    return r;
}

/** \brief Angle a minus b, wrapped to [-pi, pi) (a turn past pi is a small step, not a whole turn). */
static inline float sh4der_taa_angle_delta(float a, float b) {
    const float pi = 3.14159265f, tau = 6.2831853f;
    return sh4der_frac_(sh4der_div_(a - b + pi, tau)) * tau - pi;
}

/** \brief The camera's history weight from the content's pixel speed squared (s2 = shift x^2 + shift y^2):
    WEIGHT_HI up to SPEED_LO, WEIGHT_LO from SPEED_HI, linear between. Never rises with speed. */
static inline float sh4der_taa_weight(float s2) {
    const float lo = SH4DER_TAA_SPEED_LO * SH4DER_TAA_SPEED_LO, hi = SH4DER_TAA_SPEED_HI * SH4DER_TAA_SPEED_HI;
    const float t = sh4der_sat_(sh4der_div_(s2 - lo, hi - lo));
    return sh4der_lerp_(SH4DER_TAA_WEIGHT_HI, SH4DER_TAA_WEIGHT_LO, t);
}

/** \brief A content shift as the feed copy's whole x offset: the nearest EVEN pixel count (the copy keeps the column parity). */
static inline int sh4der_taa_even_px(float s) {
    const int h = (int)(sh4der_taa_abs_(s) * 0.5f + 0.5f);
    return s < 0.0f ? -2 * h : 2 * h;
}

/** \brief A content shift as a whole pixel count, nearest (for y, where the copy has no parity rule). */
static inline int sh4der_taa_whole_px(float s) {
    return s < 0.0f ? -(int)(0.5f - s) : (int)(s + 0.5f);
}

/** \brief tan of an angle: sin / cos from the SH4ZAM sincos (the plain C path on the host). */
static inline float sh4der_taa_tan_(float x) {
    float s, c;
    sh4der_sincos_(x, &s, &c);
    return sh4der_div_(s, c);
}

/** \brief Jitter over the cycle: the table's phase, in clip units for a projection (ndc x += 2 jx / w, ndc y -= 2 jy / h). */
static inline void sh4der_taa_jitter_clip(const float jit[2], float w, float h, float clip[2]) {
    clip[0] = sh4der_div_(2.0f * jit[0], w);
    clip[1] = -sh4der_div_(2.0f * jit[1], h);
}

/** \brief The content shift at the screen centre for a camera change (dyaw, dpitch radians), f in pixels
    (fx, fy: the focal length in pixels, horizontal and vertical). Pure rotation: dx = -fx tan(dyaw),
    dy = fy tan(dpitch) / cos(dyaw). */
static inline void sh4der_taa_shift(float fx, float fy, float dyaw, float dpitch, float out[2]) {
    float s, c;
    sh4der_sincos_(dyaw, &s, &c);
    out[0] = -fx * sh4der_div_(s, c);
    out[1] = sh4der_div_(fy * sh4der_taa_tan_(dpitch), c);
}

/** \brief Where the history of pixel (x, y) is: the exact pure-rotation mapping, the pitch first, then the yaw
    (the order is second-order for small deltas). Pixel centre (cx, cy), focal lengths fx, fy in pixels. Draw the
    history quad's corner at (x, y) with its texture at out. A ray behind the camera gives (x, y). */
static inline void sh4der_taa_warp(float fx, float fy, float cx, float cy, float dyaw, float dpitch, float x, float y, float out[2]) {
    const float rx = sh4der_div_(x - cx, fx), ry = -sh4der_div_(y - cy, fy);
    float sa, ca, sb, cb, y1, z1, x2, y2, z2;
    sh4der_sincos_(dyaw, &sa, &ca);
    sh4der_sincos_(dpitch, &sb, &cb);
    y1 = cb * ry + sb;          /* ray (rx, ry, 1) turned by the pitch: y1 = cb ry + sb z, z1 = -sb ry + cb z */
    z1 = -sb * ry + cb;
    x2 = ca * rx + sa * z1;     /* then by the yaw */
    y2 = y1;
    z2 = -sa * rx + ca * z1;
    if (!(z2 > 0.0f)) { out[0] = x; out[1] = y; return; }
    out[0] = cx + fx * sh4der_div_(x2, z2);
    out[1] = cy - fy * sh4der_div_(y2, z2);
}

/** \brief Clear the history (a cut, a new place). The jitter table stays. */
static inline void sh4der_taa_reset(sh4der_taa_t *t) { t->yaw = 0.0f; t->pitch = 0.0f; t->frame = 0; t->ready = 0; }

/** \brief Load time: the jitter table (Halton 2 for x, 3 for y, each minus its cycle mean, so a cycle sums to zero). */
static inline void sh4der_taa_init(sh4der_taa_t *t) {
    float mx = 0.0f, my = 0.0f;
    unsigned k;
    for (k = 0; k < SH4DER_TAA_PHASES; k++) {
        mx += sh4der_taa_halton(k, 2);
        my += sh4der_taa_halton(k, 3);
    }
    mx = sh4der_div_(mx, (float)SH4DER_TAA_PHASES);
    my = sh4der_div_(my, (float)SH4DER_TAA_PHASES);
    for (k = 0; k < SH4DER_TAA_PHASES; k++) {
        t->jit[k][0] = SH4DER_TAA_JITTER_AMP * (sh4der_taa_halton(k, 2) - mx);
        t->jit[k][1] = SH4DER_TAA_JITTER_AMP * (sh4der_taa_halton(k, 3) - my);
    }
    sh4der_taa_reset(t);
}

/** \brief One frame: the camera's yaw and pitch (radians) and the focal lengths (pixels). Fills `out`, then remembers the angles. */
static inline void sh4der_taa_step(sh4der_taa_t *t, float yaw, float pitch, float fx, float fy, sh4der_taa_frame_t *out) {
    const unsigned ph = t->frame % SH4DER_TAA_PHASES;
    float dyaw, dpitch;
    out->jitter[0] = t->jit[ph][0];
    out->jitter[1] = t->jit[ph][1];
    out->shift[0] = 0.0f;
    out->shift[1] = 0.0f;
    out->copy_px[0] = 0;
    out->copy_px[1] = 0;
    if (!sh4der_finite_(yaw) || !sh4der_finite_(pitch)) {   /* not a number: a cut, and the angles are not kept */
        out->cut = 1;
        out->weight = 0.0f;
        t->frame++;
        t->ready = 0;
        return;
    }
    dyaw = sh4der_taa_angle_delta(yaw, t->yaw);
    dpitch = sh4der_taa_angle_delta(pitch, t->pitch);
    out->cut = !t->ready || sh4der_taa_abs_(dyaw) > SH4DER_TAA_CUT_RAD || sh4der_taa_abs_(dpitch) > SH4DER_TAA_CUT_RAD;
    if (out->cut) {
        out->weight = 0.0f;
    } else {
        sh4der_taa_shift(fx, fy, dyaw, dpitch, out->shift);
        out->weight = sh4der_taa_weight(sh4der_len2_(out->shift[0], out->shift[1]));
        out->copy_px[0] = sh4der_taa_even_px(out->shift[0]);
        out->copy_px[1] = sh4der_taa_whole_px(out->shift[1]);
    }
    t->yaw = yaw;
    t->pitch = pitch;
    t->frame++;
    t->ready = 1;
}

/** \brief The "keep" layer's colour at weight w: the frame dims to (1 - w) per channel, alpha 0 (the blend's dst alpha is cleared). */
static inline uint32_t sh4der_taa_keep_argb(float w) {
    const uint32_t v = (uint32_t)(sh4der_sat_(1.0f - w) * 255.0f + 0.5f);
    return (v << 16) | (v << 8) | v;
}

/** \brief The history layer's colour at weight w: the history is multiplied by w per channel, alpha 0. */
static inline uint32_t sh4der_taa_add_argb(float w) {
    const uint32_t v = (uint32_t)(sh4der_sat_(w) * 255.0f + 0.5f);
    return (v << 16) | (v << 8) | v;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_TAA_H */
