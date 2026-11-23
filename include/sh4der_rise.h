/* SH4DER-PVR: rise (points of light that lift off, drift up and settle as stars)

   sh4der_rise.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_rise.h
    \brief   Lights that sit still on something (candles on a tree, lamps, embers), then lift off one after
             another, drift upward with a little sideways sway, shrink and cool, and come to rest as stars
             that stay and twinkle faintly.

    COST (counts, not times; nothing here is measured on a console): RAM: the parameter block, 84 bytes,
    and 0 bytes per light. Video memory: 0 of its own (the caller's dot texture). Per light per frame: about
    50 multiplies or fused multiply-adds, 6 integer hashes and one sine-cosine pair. At most N quads = 2N
    triangles on the translucent list, additive. sh4der_rise_done costs the same hashes and a few
    multiplies for each of its n lights, so call it once per frame, not once per light.

    The stateless rule (the same idea as sh4der_motes.h): nothing is stored per light. The caller owns an
    array of N source positions; the state of light i at effect time t is a pure function of (i, t, params,
    source), so the same (i, t) always gives the same answer and a paused or scrubbed film shows the same
    frame. Every random pick (lift-off jitter, arrival spot, sway phase, twinkle phase) is an integer hash
    of (i, seed, salt).

    The life of one light, with u = (t - lift_off) / rise_time and s = smoothstep(u):
      - SEATED: t before this light's lift-off (and every t < 0): exactly at its source, start size, start
        colour, start brightness.
      - RISING: position = source + (arrival - source) x s, plus a sideways sway that is zero at both ends;
        size, colour and brightness are lerped from their start to their end values by s.
      - STAR: u >= 1. The light rests at its arrival spot (sky height plus a hashed share of the band, and
        a hashed x / z offset inside the sky spread), at the end size and colour, and its brightness
        twinkles between end_brightness x (1 - twinkle_depth) and end_brightness.
    Lift-off of light i is i x stagger + jitter x hash(i): a jitter larger than the stagger shuffles the order.
    Lights go UP when sky_y is above the source (the intended case); the height is monotone either way.

    The caller transforms a light's four corners (sh4der_rise_quad), culls and guards them (every triangle
    through sh4der_tri_area_ok; the polygon header it submits through sh4der_poly_hdr_ok), and draws them
    additively (source alpha / one) with a soft dot texture; the packed colour from sh4der_rise_argb carries
    the brightness in alpha.

    No allocation, no globals. Floats and integers in structs the caller owns (4-byte aligned). The maths is
    SH4ZAM through sh4der_core.h (fused multiply-add, lerp, saturate, divide, fraction, sine-cosine); define
    SH4DER_CORE_NO_SH4ZAM for the plain C path. By hand: the integer hash and the colour byte packing.

    Version: v0.10
    The effect: the candles of a tree lift off one by one and become stars.
    Developed for: a short film's closing shot.
    Use it for: lanterns released into the sky, fireflies leaving a bush, souls rising, campfire sparks that
      become stars, embers drifting off a pyre.

    \author Jilted Valkyrie

    PVR terms (TA, TR/OP list, EOL, twiddled, direct render): see the PVR glossary in README.md.
*/

#ifndef SH4DER_RISE_H
#define SH4DER_RISE_H

#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Constants
    @{ */
#define SH4DER_RISE_TAU        6.28318530718f   /**< \brief 2 pi: radians in a full turn. */
#define SH4DER_RISE_INV255     0.00392156863f   /**< \brief 1 / 255: a colour byte to a 0..1 share. */
#define SH4DER_RISE_TWINKLE_IN 1.0f             /**< \brief Seconds over which the twinkle depth fades in after arrival (so brightness has no jump). */
#define SH4DER_RISE_HASH_MASK  0xFFFFFFu        /**< \brief The 24 hash bits kept: they fit a float mantissa exactly. */
#define SH4DER_RISE_HASH_SCALE 5.9604645e-8f    /**< \brief 1 / 2^24: turns the 24 kept bits into 0..1. */
/** @} */

/** \name Hash salts (one per hashed pick, so the picks do not correlate)
    @{ */
#define SH4DER_RISE_SALT_LIFT  1u   /**< \brief Lift-off jitter. */
#define SH4DER_RISE_SALT_DX    2u   /**< \brief Arrival offset along x. */
#define SH4DER_RISE_SALT_DZ    3u   /**< \brief Arrival offset along z. */
#define SH4DER_RISE_SALT_SKY   4u   /**< \brief Arrival height inside the band. */
#define SH4DER_RISE_SALT_SWAY  5u   /**< \brief Sway phase. */
#define SH4DER_RISE_SALT_TWK   6u   /**< \brief Twinkle phase. */
/** @} */

/** \brief Which part of its life a light is in. */
typedef enum sh4der_rise_phase {
    SH4DER_RISE_SEATED = 0,   /**< \brief Still on its source. */
    SH4DER_RISE_RISING = 1,   /**< \brief Lifting off and drifting up. */
    SH4DER_RISE_STAR   = 2    /**< \brief Arrived: at rest, twinkling. */
} sh4der_rise_phase_t;

/** \brief The tuning block for a field of lights (84 bytes). Set it with sh4der_rise_init, then change what you need. */
typedef struct sh4der_rise_params {
    float stagger;          /**< \brief Seconds between one light's lift-off and the next in index order (>= 0). */
    float jitter;           /**< \brief Seconds of hashed extra delay per light (>= 0); above the stagger the order shuffles. */
    float rise_time;        /**< \brief Seconds from lift-off to arrival (> 0). */
    float sky_y;            /**< \brief World y where the stars rest (the bottom of the band). */
    float sky_band;         /**< \brief Height of the star band above sky_y, world units (>= 0). */
    float sky_spread;       /**< \brief Largest x / z distance of a star from its source, world units (>= 0). */
    float sway_amp;         /**< \brief Sideways drift amplitude while rising, world units (>= 0). */
    float sway_freq;        /**< \brief Sway cycles per second (>= 0). */
    float size0;            /**< \brief Quad side while seated, world units (> 0): a candle flame. */
    float size1;            /**< \brief Quad side as a star, world units (> 0): much smaller. */
    float col0[3];          /**< \brief Start colour r, g, b, each 0..1 (warm). */
    float col1[3];          /**< \brief End colour r, g, b, each 0..1 (cool white). */
    float bright0;          /**< \brief Start brightness 0..1. */
    float bright1;          /**< \brief End (star) brightness 0..1. */
    float twinkle_depth;    /**< \brief Share 0..1 of the star brightness the twinkle can remove. */
    float twinkle_rate;     /**< \brief Twinkle cycles per second (>= 0). */
    uint32_t seed;          /**< \brief Mixed into every hash: a different seed gives a different sky. */
} sh4der_rise_params_t;

/** \brief One light at one time: world position, quad side, colour 0..1, brightness 0..1 and phase. */
typedef struct sh4der_rise_light {
    float x, y, z;          /**< \brief Centre, world units. */
    float size;             /**< \brief Side of the square quad, world units. */
    float r, g, b;          /**< \brief Colour, each 0..1. */
    float alpha;            /**< \brief Brightness 0..1 (the additive draw's alpha). */
    uint32_t phase;         /**< \brief A sh4der_rise_phase_t. */
} sh4der_rise_light_t;

/** \brief One corner of a light's quad: world position and texture coordinates. */
typedef struct sh4der_rise_vtx {
    float x, y, z;          /**< \brief World position. */
    float u, v;             /**< \brief Texture coordinates 0..1. */
} sh4der_rise_vtx_t;

/** \brief Fill `p` with the defaults: stagger 0.4 s, jitter 3 s, rise 6 s, sky 60 units up with a 20 band,
    spread 40, sway 1.5 units at 0.3 Hz, size 2 to 0.5, colour (255, 176, 96) to (220, 230, 255), brightness
    1 to 0.8, twinkle depth 0.3 at 0.7 Hz, seed 0. A NULL pointer does nothing. */
static inline void sh4der_rise_init(sh4der_rise_params_t *p) {
    if (!p) {
        return;
    }
    p->stagger = 0.4f;
    p->jitter = 3.0f;
    p->rise_time = 6.0f;
    p->sky_y = 60.0f;
    p->sky_band = 20.0f;
    p->sky_spread = 40.0f;
    p->sway_amp = 1.5f;
    p->sway_freq = 0.3f;
    p->size0 = 2.0f;
    p->size1 = 0.5f;
    p->col0[0] = 255.0f * SH4DER_RISE_INV255;
    p->col0[1] = 176.0f * SH4DER_RISE_INV255;
    p->col0[2] = 96.0f * SH4DER_RISE_INV255;
    p->col1[0] = 220.0f * SH4DER_RISE_INV255;
    p->col1[1] = 230.0f * SH4DER_RISE_INV255;
    p->col1[2] = 255.0f * SH4DER_RISE_INV255;
    p->bright0 = 1.0f;
    p->bright1 = 0.8f;
    p->twinkle_depth = 0.3f;
    p->twinkle_rate = 0.7f;
    p->seed = 0u;
}

/** \brief 1 when every number of `p` is finite and in its documented range, else 0 (also 0 for NULL). */
static inline int sh4der_rise_valid(const sh4der_rise_params_t *p) {
    int k;

    if (!p) {
        return 0;
    }
    if (!sh4der_finite_(p->stagger) || !(p->stagger >= 0.0f) || !sh4der_finite_(p->jitter) || !(p->jitter >= 0.0f) ||
        !sh4der_finite_(p->rise_time) || !(p->rise_time > 0.0f) || !sh4der_finite_(p->sky_y) ||
        !sh4der_finite_(p->sky_band) || !(p->sky_band >= 0.0f) || !sh4der_finite_(p->sky_spread) || !(p->sky_spread >= 0.0f) ||
        !sh4der_finite_(p->sway_amp) || !(p->sway_amp >= 0.0f) || !sh4der_finite_(p->sway_freq) || !(p->sway_freq >= 0.0f) ||
        !sh4der_finite_(p->size0) || !(p->size0 > 0.0f) || !sh4der_finite_(p->size1) || !(p->size1 > 0.0f) ||
        !sh4der_finite_(p->twinkle_rate) || !(p->twinkle_rate >= 0.0f)) {
        return 0;
    }
    if (!sh4der_unit_(p->bright0) || !sh4der_unit_(p->bright1) || !sh4der_unit_(p->twinkle_depth)) {
        return 0;
    }
    for (k = 0; k < 3; k++) {
        if (!sh4der_unit_(p->col0[k]) || !sh4der_unit_(p->col1[k])) {
            return 0;
        }
    }
    return 1;
}

/** \brief A stateless 0..1 value for a light number, a seed and a salt (an integer hash; the same on every machine). */
static inline float sh4der_rise_hash01(uint32_t i, uint32_t seed, uint32_t salt) {
    uint32_t x = (i + seed) * 0x9E3779B1u + salt * 0x85EBCA6Bu;   /* two odd multipliers; unsigned wrap is defined */

    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    return (float)(x & SH4DER_RISE_HASH_MASK) * SH4DER_RISE_HASH_SCALE;
}

/** \brief Effect time (seconds) at which light i lifts off: i x stagger + jitter x hash(i). Not validated: call it with valid params. */
static inline float sh4der_rise_liftoff(const sh4der_rise_params_t *p, uint32_t i) {
    return sh4der_fma_((float)i, p->stagger, p->jitter * sh4der_rise_hash01(i, p->seed, SH4DER_RISE_SALT_LIFT));
}

/** \brief Light i of a field, source position `source` (x, y, z), at effect time t seconds (t < 0: all seated).
    Fills `out` and returns 1, or returns 0 and writes nothing for a NULL pointer, invalid params, a source or a
    t that is not finite. */
static inline int sh4der_rise_light(const sh4der_rise_params_t *p, uint32_t i, const float source[3], float t,
                                    sh4der_rise_light_t *out) {
    float lift, u, s, ang, sn, cs, dx, dz, ex, ey, ez, size, bright;
    float col[3];
    uint32_t phase;
    int k;

    if (!p || !source || !out || !sh4der_rise_valid(p) || !sh4der_finite_(t) || !sh4der_finite_(source[0]) ||
        !sh4der_finite_(source[1]) || !sh4der_finite_(source[2])) {
        return 0;
    }
    lift = sh4der_rise_liftoff(p, i);
    if (t < 0.0f || t < lift) {
        u = 0.0f;
        phase = SH4DER_RISE_SEATED;
    } else {
        phase = (t - lift >= p->rise_time) ? SH4DER_RISE_STAR : SH4DER_RISE_RISING;
        u = sh4der_sat_(sh4der_div_(t - lift, p->rise_time));
    }
    s = u * u * sh4der_fma_(-2.0f, u, 3.0f);                    /* smoothstep: slow start, gentle settle */

    /* Arrival spot: a hashed point of the square [-1, 1]^2; a point outside the unit disc is halved (a corner is at
       most sqrt 2 away, so it lands within 0.71), then scaled by the spread. No root needed. */
    dx = sh4der_fma_(2.0f, sh4der_rise_hash01(i, p->seed, SH4DER_RISE_SALT_DX), -1.0f);
    dz = sh4der_fma_(2.0f, sh4der_rise_hash01(i, p->seed, SH4DER_RISE_SALT_DZ), -1.0f);
    if (sh4der_len2_(dx, dz) > 1.0f) {
        dx *= 0.5f;
        dz *= 0.5f;
    }
    ex = sh4der_fma_(dx, p->sky_spread, source[0]);
    ez = sh4der_fma_(dz, p->sky_spread, source[2]);
    ey = sh4der_fma_(p->sky_band, sh4der_rise_hash01(i, p->seed, SH4DER_RISE_SALT_SKY), p->sky_y);

    out->x = sh4der_lerp_(source[0], ex, s);
    out->y = sh4der_lerp_(source[1], ey, s);
    out->z = sh4der_lerp_(source[2], ez, s);
    size = sh4der_lerp_(p->size0, p->size1, s);
    bright = sh4der_lerp_(p->bright0, p->bright1, s);
    for (k = 0; k < 3; k++) {
        col[k] = sh4der_lerp_(p->col0[k], p->col1[k], s);
    }

    if (phase == SH4DER_RISE_RISING) {
        /* Sway: a sideways circle whose radius is 4 u (1 - u) x amplitude, zero at lift-off and at arrival. */
        const float env = 4.0f * u * (1.0f - u) * p->sway_amp;

        ang = sh4der_frac_(sh4der_fma_(p->sway_freq, t - lift, sh4der_rise_hash01(i, p->seed, SH4DER_RISE_SALT_SWAY))) *
              SH4DER_RISE_TAU;
        sh4der_sincos_(ang, &sn, &cs);
        out->x = sh4der_fma_(sn, env, out->x);
        out->z = sh4der_fma_(cs, env, out->z);
    } else if (phase == SH4DER_RISE_STAR) {
        /* Twinkle: brightness x (1 - depth x (0.5 + 0.5 sin)), the depth fading in over a second after arrival. */
        const float age = t - lift - p->rise_time;
        const float depth = p->twinkle_depth * sh4der_sat_(sh4der_div_(age, SH4DER_RISE_TWINKLE_IN));

        ang = sh4der_frac_(sh4der_fma_(p->twinkle_rate, age, sh4der_rise_hash01(i, p->seed, SH4DER_RISE_SALT_TWK))) *
              SH4DER_RISE_TAU;
        sh4der_sincos_(ang, &sn, &cs);
        bright *= sh4der_fma_(-depth, sh4der_fma_(0.5f, sn, 0.5f), 1.0f);
    }
    out->size = size;
    out->r = col[0];
    out->g = col[1];
    out->b = col[2];
    out->alpha = sh4der_sat_(bright);
    out->phase = phase;
    return 1;
}

/** \brief 1 once every one of the n lights has arrived at time t (the last arrival is the largest lift-off plus
    rise_time), else 0; also 0 for n == 0, a NULL pointer, invalid params or a t that is not finite. */
static inline int sh4der_rise_done(const sh4der_rise_params_t *p, uint32_t n, float t) {
    float last = 0.0f;
    uint32_t i;

    if (!p || n == 0u || !sh4der_rise_valid(p) || !sh4der_finite_(t)) {
        return 0;
    }
    for (i = 0; i < n; i++) {
        const float arrive = sh4der_rise_liftoff(p, i) + p->rise_time;

        if (arrive > last) {
            last = arrive;
        }
    }
    return (t - last) >= 0.0f;
}

/** \brief The four world corners of a light's camera-facing quad, in strip order: top-left, top-right,
    bottom-left, bottom-right (triangles 0 1 2 and 1 3 2), u / v 0..1 with v down. right and up are the camera's
    unit axes. The square has side light->size and is centred on the light. Returns 1, or 0 writing nothing for
    a NULL pointer or a size that is not positive and finite. */
static inline int sh4der_rise_quad(const sh4der_rise_light_t *light, const float right[3], const float up[3],
                                   sh4der_rise_vtx_t out[4]) {
    /* the four corners as signs on (right, up) and their texture coordinates, in strip order */
    static const float sx[4] = { -1.0f, 1.0f, -1.0f, 1.0f };
    static const float sy[4] = { 1.0f, 1.0f, -1.0f, -1.0f };
    static const float tu[4] = { 0.0f, 1.0f, 0.0f, 1.0f };
    static const float tv[4] = { 0.0f, 0.0f, 1.0f, 1.0f };
    float h;
    unsigned k;

    if (!light || !right || !up || !out || !sh4der_finite_(light->size) || !(light->size > 0.0f)) {
        return 0;
    }
    h = 0.5f * light->size;
    for (k = 0; k < 4u; k++) {
        const float a = sx[k] * h;
        const float b = sy[k] * h;

        out[k].x = sh4der_fma_(up[0], b, sh4der_fma_(right[0], a, light->x));
        out[k].y = sh4der_fma_(up[1], b, sh4der_fma_(right[1], a, light->y));
        out[k].z = sh4der_fma_(up[2], b, sh4der_fma_(right[2], a, light->z));
        out[k].u = tu[k];
        out[k].v = tv[k];
    }
    return 1;
}

/** \brief The light's colour packed as 0xAARRGGBB for an additive draw: alpha = brightness, each byte rounded
    (by hand: byte packing). */
static inline uint32_t sh4der_rise_argb(const sh4der_rise_light_t *light) {
    const uint32_t a = (uint32_t)(sh4der_sat_(light->alpha) * 255.0f + 0.5f);
    const uint32_t r = (uint32_t)(sh4der_sat_(light->r) * 255.0f + 0.5f);
    const uint32_t g = (uint32_t)(sh4der_sat_(light->g) * 255.0f + 0.5f);
    const uint32_t b = (uint32_t)(sh4der_sat_(light->b) * 255.0f + 0.5f);

    return (a << 24) | (r << 16) | (g << 8) | b;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_RISE_H */
