/* SH4DER-PVR: the look effect

   sh4der_look.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_look.h
    \brief   The frame's look after it is drawn: an eye that adapts both ways,
             a grade (a straight line per channel), and glare in the fog.

    The PVR has no post-process shader. What it has: the CPU can read a few
    pixels of the frame on screen, a full-screen untextured quad can multiply,
    add or invert the frame per channel, and the fog table and colour are
    free per pixel. This header is the maths for those; the client reads the
    pixels, draws the quads and writes the fog.

    - Read: sample a grid of the frame on screen (RGB565), put each sample's
      luma in one of SH4DER_LOOK_BINS bins (sh4der_look_luma565), then
      sh4der_look_log_average gives the frame's log-average luma.
    - Eye: sh4der_look_eye_step eases an adapted level toward the scene (fast
      into light, slow into dark) and gives the gain to lay over the frame:
      a scene brighter than the eye reads over-bright until the eye catches
      up, a dark scene opens up a little once adapted (OPEN). The frame read
      already carries last frame's gain; the step divides it out.
    - Grade: out = clamp(A x + B) per channel. sh4der_look_solve turns gain,
      lift, contrast and exposure into at most SH4DER_LOOK_LAYERS full-screen
      layers (multiply, boost, add, invert), in drawing order.
    - Glare: sh4der_look_glare leans the fog colour toward white and thickens
      the fog where it already is, by an amount 0..1 (a bright light in view).

    The time steps are first order, dt / (tau + dt): 1 - e^-x to O(x^2) at a
    frame's x. A fast exp is off by percents at x = 0.01 .. 0.04, which is a
    whole factor in a step.

    The effect: makes the whole picture behave like an eye or a camera. It
      darkens after a moment in bright light and opens up in the dark, and it
      can tint, lift or fade the frame, as a few full-screen layers.
    Developed for: eye adaptation and colour grading in Halo: Dreamcast
      Evolved: walking out of a dark corridor into sunlight and back.
    Use it for: automatic exposure, a day / night or underwater tint, a fade
      to black or white, a red flash when hurt, a sepia flashback.

    Recommended settings (the author's own game and demo disc):
    - Eye strength 0.25: scale the shipped numbers by it, so the eye is felt
      and not seen:
          transient = SH4DER_LOOK_TRANSIENT x 0.25
          open      = SH4DER_LOOK_OPEN x 0.25
          gain_hi   = 1 + (SH4DER_LOOK_GAIN_HI - 1) x 0.25      (1.125)
          gain_lo   = 1 - (1 - SH4DER_LOOK_GAIN_LO) x 0.25      (0.9375)
      At any strength clamp gain_hi to 2.0 at most and gain_lo to 0.4 at
      least: RGB565 bands past that.
    - Into light 2 s, into dark 6 s (SH4DER_LOOK_LIGHT_S, SH4DER_LOOK_DARK_S).
    - Hold the eye (skip the step) while a flashlight is lit: the eye
      opening against its own lamp reads as a fault.
    - Hand the step the read with the gain of the frame it saw taken off
      (sh4der_frame.h's stamp and history), or the eye chases itself.

    \author Jilted Valkyrie
    \author Cortana (Claude, an AI by Anthropic): the effects' maths, the PVR paths, the tests
*/

#ifndef SH4DER_LOOK_H
#define SH4DER_LOOK_H

#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_LOOK_GRID_X     80       /**< \brief Samples across the frame. */
#define SH4DER_LOOK_GRID_Y     60       /**< \brief Samples down the frame. */
#define SH4DER_LOOK_BINS       64       /**< \brief Luma bins of the read. */
#define SH4DER_LOOK_FLOOR      0.004f   /**< \brief Luma added before the log: black is not minus infinity. */
#define SH4DER_LOOK_REF        0.18f    /**< \brief Middle grey OPEN pulls an adapted eye toward. */
#define SH4DER_LOOK_TRANSIENT  0.5f     /**< \brief Share of (scene - eye) that shows as over-bright or dark (log). */
#define SH4DER_LOOK_OPEN       0.3f     /**< \brief Share of (middle grey - eye) kept once adapted (log): dark opens up. */
#define SH4DER_LOOK_GAIN_LO    0.75f    /**< \brief Lowest gain laid over the frame. */
#define SH4DER_LOOK_GAIN_HI    1.5f     /**< \brief Highest gain (RGB565 bands past it). */
#define SH4DER_LOOK_LIGHT_S    2.0f     /**< \brief Seconds to 95 % when the scene gets brighter. */
#define SH4DER_LOOK_DARK_S     6.0f     /**< \brief Seconds to 95 % when it gets darker. */
#define SH4DER_LOOK_GLARE_TINT 0.6f     /**< \brief Fog colour's lean to white at full glare. */
#define SH4DER_LOOK_GLARE_FOG  1.5f     /**< \brief Fog thickening at full glare (x (1 + this)). */
#define SH4DER_LOOK_LAYERS     6        /**< \brief Layers sh4der_look_solve gives at most. */
/** @} */

/** \brief Layer kinds, each a full-screen untextured quad of colour rgb over the frame x. */
enum {
    SH4DER_LOOK_MULTIPLY,   /**< \brief x * rgb: blend src ZERO, dst "other colour" (the source colour in the destination slot). */
    SH4DER_LOOK_BOOST,      /**< \brief x * (1 + rgb): src destination colour, dst ONE. */
    SH4DER_LOOK_ADD,        /**< \brief x + rgb: ONE, ONE. */
    SH4DER_LOOK_INVERT      /**< \brief 1 - x: white, src inverse destination colour, dst ZERO (rgb 1). */
};

typedef struct sh4der_look_layer {
    uint8_t kind;               /**< \brief SH4DER_LOOK_MULTIPLY, _BOOST, _ADD or _INVERT. */
    float rgb[3];               /**< \brief Colour of the layer, 0..1 per channel (see the kind). */
} sh4der_look_layer_t;

/** \brief The grade: per channel gain and lift (-1..1), contrast about 0.5, and the eye's gain on top. Neutral: 1, 0, 1, 1. */
typedef struct sh4der_look_grade {
    float gain[3];
    float lift[3];
    float contrast;
    float exposure;
} sh4der_look_grade_t;

typedef struct sh4der_look_params {
    float light_s, dark_s;    /**< \brief Seconds to 95 %, toward brighter and darker. */
    float transient, open;    /**< \brief See SH4DER_LOOK_TRANSIENT, SH4DER_LOOK_OPEN. */
    float gain_lo, gain_hi;   /**< \brief Lowest and highest gain the eye gives. */
} sh4der_look_params_t;

/** \brief One eye. Zero it (or sh4der_look_eye_reset) for a new place: the first step adapts at once. */
typedef struct sh4der_look_eye {
    float adapted;   /**< \brief Adapted level, natural log of luma. */
    float gain_ln;   /**< \brief The gain last given, log. */
    float gain;      /**< \brief The gain last given, linear. */
    int ready;       /**< \brief 0 until the first step. */
} sh4der_look_eye_t;

/** \brief The bins' logs, made once (sh4der_look_bins). */
typedef struct sh4der_look_bins {
    float ln[SH4DER_LOOK_BINS];
} sh4der_look_bins_t;

/* ------------------------------------------------------------------------ */
/* The look's steps (inline: the client calls them only where it uses them). */

/* Log and exp by short series, the same on both paths. Not shz_logf / shz_expf: their fast forms read log(1) = 0.040 and
   exp(-0.012) = 0.963 (host run of SH4ZAM's software path, 2026-11-10), a 3 % grey cast on a neutral frame and a drift where the
   eye divides its own gain back out. Hand loops because SH4ZAM has no exact form; the range is small. */
static inline float sh4der_look_ln_(float x) {   /* x > 0: x = m 2^k with m in [0.75, 1.5), then 2 atanh((m - 1) / (m + 1)) */
    float k = 0.0f, z, z2;
    if (!(x > 0.0f))
        return -88.0f;
    while (x >= 1.5f) { x *= 0.5f; k += 1.0f; }
    while (x < 0.75f) { x *= 2.0f; k -= 1.0f; }
    z = sh4der_div_(x - 1.0f, x + 1.0f);   /* |z| <= 0.2 */
    z2 = z * z;
    return k * 0.69314718f + 2.0f * z * (1.0f + z2 * (1.0f / 3.0f + z2 * (1.0f / 5.0f + z2 * (1.0f / 7.0f))));
}
static inline float sh4der_look_exp_(float x) {   /* |x| <= 1 (the gains are 0.75 .. 1.5): Taylor to x^6, error under 2e-4 at |x| = 1 */
    x = x < -1.0f ? -1.0f : x > 1.0f ? 1.0f : x;
    return 1.0f + x * (1.0f + x * (0.5f + x * (1.0f / 6.0f + x * (1.0f / 24.0f + x * (1.0f / 120.0f + x * (1.0f / 720.0f))))));
}

/** \brief An RGB565 pixel's luma bin, 0..SH4DER_LOOK_BINS-1 (Rec. 601 weights 77 / 150 / 29). */
static inline unsigned sh4der_look_luma565(unsigned p) {
    const unsigned r = (p >> 11) << 3, g = ((p >> 5) & 63u) << 2, b = (p & 31u) << 3;
    return (77u * r + 150u * g + 29u * b) >> 10;   /* at most 62 */
}

/** \brief Fill the bins' logs. Once at load. */
static inline void sh4der_look_bins(sh4der_look_bins_t *t) {
    int i;
    for (i = 0; i < SH4DER_LOOK_BINS; i++)
        t->ln[i] = sh4der_look_ln_(((float)i + 0.5f) * (1.0f / SH4DER_LOOK_BINS) + SH4DER_LOOK_FLOOR);
}

/** \brief The log-average luma (natural log) of `n` samples counted in `hist`. n 0: the floor's log. */
static inline float sh4der_look_log_average(const sh4der_look_bins_t *t, const uint16_t hist[SH4DER_LOOK_BINS], unsigned n) {
    float acc = 0.0f;
    int i;
    if (!n)
        return sh4der_look_ln_(SH4DER_LOOK_FLOOR);
    for (i = 0; i < SH4DER_LOOK_BINS; i++)
        acc += (float)hist[i] * t->ln[i];
    return acc * sh4der_div_(1.0f, (float)n);
}

/** \brief The first-order step toward a target over dt with time constant tau. */
static inline float sh4der_look_step_(float tau, float dt) { return dt > 0.0f ? sh4der_div_(dt, tau + dt) : 0.0f; }

/** \brief The shipped eye and limits (the SH4DER_LOOK_* values). */
static inline void sh4der_look_params_default(sh4der_look_params_t *p) {
    p->light_s = SH4DER_LOOK_LIGHT_S; p->dark_s = SH4DER_LOOK_DARK_S;
    p->transient = SH4DER_LOOK_TRANSIENT; p->open = SH4DER_LOOK_OPEN;
    p->gain_lo = SH4DER_LOOK_GAIN_LO; p->gain_hi = SH4DER_LOOK_GAIN_HI;
}

/** \brief Zero the eye for a new place: the next step adapts at once. */
static inline void sh4der_look_eye_reset(sh4der_look_eye_t *e) { e->adapted = 0.0f; e->gain_ln = 0.0f; e->gain = 1.0f; e->ready = 0; }

/** \brief One frame: `read_ln` is the log-average of the frame on screen (drawn with the gain this eye gave last), dt seconds.
    Returns the gain to lay over this frame. */
static inline float sh4der_look_eye_step(sh4der_look_eye_t *e, const sh4der_look_params_t *p, float read_ln, float dt) {
    const float scene = read_ln - e->gain_ln;   /* the scene without last frame's gain */
    float g;
    if (!e->ready) {
        e->adapted = scene;
        e->ready = 1;
    } else {
        const float tau = (scene > e->adapted ? p->light_s : p->dark_s) * (1.0f / 3.0f);   /* 95 % in three time constants */
        e->adapted += (scene - e->adapted) * sh4der_look_step_(tau, dt);
    }
    g = p->transient * (scene - e->adapted) + p->open * (sh4der_look_ln_(SH4DER_LOOK_REF) - e->adapted);
    g = sh4der_look_exp_(g);
    g = g < p->gain_lo ? p->gain_lo : g > p->gain_hi ? p->gain_hi : g;
    e->gain = g;
    e->gain_ln = sh4der_look_ln_(g);
    return g;
}

/** \brief The grade's layers in drawing order into `out` (SH4DER_LOOK_LAYERS); returns how many. A layer that would change
    no channel by 1/255 is left out. The frame comes out clamp(A x + B), A = gain x exposure x contrast, B = lift + (1 - contrast) / 2. */
static inline int sh4der_look_solve(const sh4der_look_grade_t *gr, sh4der_look_layer_t out[SH4DER_LOOK_LAYERS]) {
    const float eps = 1.0f / 255.0f;
    float a[3], b[3], mul[3], boost[3], add[3], sub[3];
    int c, n = 0, any_mul = 0, any_boost = 0, any_add = 0, any_sub = 0;
    for (c = 0; c < 3; c++) {
        a[c] = gr->gain[c] * gr->exposure * gr->contrast;
        b[c] = gr->lift[c] + 0.5f * (1.0f - gr->contrast);
        mul[c] = a[c] < 1.0f ? (a[c] > 0.0f ? a[c] : 0.0f) : 1.0f;
        boost[c] = a[c] > 1.0f ? (a[c] < 2.0f ? a[c] - 1.0f : 1.0f) : 0.0f;
        add[c] = b[c] > 0.0f ? sh4der_min1_(b[c]) : 0.0f;
        sub[c] = b[c] < 0.0f ? (a[c] > eps ? sh4der_min1_(sh4der_div_(-b[c], a[c])) : 1.0f) : 0.0f;   /* taken off first, before the scale: A (x - s / A) */
        any_mul |= mul[c] < 1.0f - eps; any_boost |= boost[c] > eps; any_add |= add[c] > eps; any_sub |= sub[c] > eps;
    }
    if (any_sub) {   /* first, so a boost cannot clip before it: x - s = 1 - ((1 - x) + s) */
        out[n].kind = SH4DER_LOOK_INVERT; for (c = 0; c < 3; c++) out[n].rgb[c] = 1.0f; n++;
        out[n].kind = SH4DER_LOOK_ADD; for (c = 0; c < 3; c++) out[n].rgb[c] = sub[c]; n++;
        out[n].kind = SH4DER_LOOK_INVERT; for (c = 0; c < 3; c++) out[n].rgb[c] = 1.0f; n++;
    }
    if (any_mul) { out[n].kind = SH4DER_LOOK_MULTIPLY; for (c = 0; c < 3; c++) out[n].rgb[c] = mul[c]; n++; }
    if (any_boost) { out[n].kind = SH4DER_LOOK_BOOST; for (c = 0; c < 3; c++) out[n].rgb[c] = boost[c]; n++; }
    if (any_add) { out[n].kind = SH4DER_LOOK_ADD; for (c = 0; c < 3; c++) out[n].rgb[c] = add[c]; n++; }
    return n;
}

/** \brief What one layer does to a channel value x (0..1), as the blender does it (the client's host check, and the tests). */
static inline float sh4der_look_apply_(const sh4der_look_layer_t *l, int c, float x) {
    float y;
    switch (l->kind) {
        case SH4DER_LOOK_MULTIPLY: y = x * l->rgb[c]; break;
        case SH4DER_LOOK_BOOST: y = x * (1.0f + l->rgb[c]); break;
        case SH4DER_LOOK_ADD: y = x + l->rgb[c]; break;
        default: y = 1.0f - x; break;
    }
    return sh4der_sat_(y);
}

/** \brief A layer's colour as ARGB8888, alpha 255. */
static inline uint32_t sh4der_look_argb(const sh4der_look_layer_t *l) {
    uint32_t v = 0xff000000u;
    int c;
    for (c = 0; c < 3; c++)
        v |= (uint32_t)(sh4der_sat_(l->rgb[c]) * 255.0f + 0.5f) << (16 - 8 * c);
    return v;
}

/** \brief Glare `t` (0..1): the fog colour leans to white, the fog table (fog amount 0..1 a depth) thickens where it is. */
static inline void sh4der_look_glare(float t, float rgb[3], float *table, unsigned n) {
    unsigned i;
    int c;
    t = sh4der_sat_(t);
    for (c = 0; c < 3; c++)
        rgb[c] = sh4der_lerp_(rgb[c], 1.0f, t * SH4DER_LOOK_GLARE_TINT);
    for (i = 0; i < n; i++)
        table[i] = sh4der_min1_(table[i] * (1.0f + t * SH4DER_LOOK_GLARE_FOG));
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_LOOK_H */
