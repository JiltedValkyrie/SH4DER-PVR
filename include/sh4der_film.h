/** \file    sh4der_film.h
    \brief   The wear of old film laid over a finished picture: exposure
             flicker, vertical scratches, dust specks and gate weave.
             Untextured, no frame read; pairs with sh4der_lens.h, which has
             the vignette and the grain.

    - Flicker: the whole picture's brightness wanders a few per cent, as a
      hand-cranked projector's lamp and shutter do. One full-screen multiply
      rectangle a frame; its grey is sh4der_film_exposure. The wander is the
      sum of two sines whose rates have no common period, so it never
      repeats in a watchable time and never jumps.
    - Tint: the same rectangle can carry a tint (warm film base):
      sh4der_film_exposure_argb mixes it in by a strength.
    - Scratches: a few thin vertical lines, each alive for a fraction of a
      second, light (the emulsion scraped off) or dark (dirt in the groove),
      drifting sideways a little. sh4der_film_scratch gives slot k's line at
      a time, or nothing. A keep-out span of columns can be set, so no line
      runs through the picture's subject.
    - Specks: a handful of dust specks a frame, each at a new place, dark
      mostly. sh4der_film_specks lists them. A keep-out band of rows can be
      set with a cap, so a part of the picture that must stay readable gets
      at most that many.
    - Gate weave: the picture wanders by up to a pixel in x and y as film
      does in a worn gate. sh4der_film_weave gives the offset; the caller
      adds it to its projection centre, and leaves masks, captions and
      titles still, as a projector's own mask is.

    Everything is a pure function of the time and a seed: no state, so a
    paused, scrubbed or looped picture shows the same wear at the same
    moment, and the cost in RAM is the parameter block alone.
    Time is cut into ticks of 1 / tick_hz seconds (a display frame); a
    scratch's life and a speck's place are counted in ticks.

    Defaults (sh4der_film_init) are mild on purpose: the brightness swings
    by 5 % peak to peak (twice the flicker depth of 2.5 %) at under 3
    cycles a second, which stays far from a strobe. sh4der_film_valid
    refuses a block that could swing by more than 8 % peak to peak (a depth
    over SH4DER_FILM_FLICKER_MAX) or faster than SH4DER_FILM_RATE_MAX_HZ.

    Time: 0 .. SH4DER_FILM_TIME_MAX_S (an hour). A caller whose clock runs
    longer folds it into that range first; the wear after a fold is as
    good as any other.

    Maths: SH4ZAM through sh4der_core.h (sine, multiply-add, divide, lerp,
    saturate); define SH4DER_CORE_NO_SH4ZAM for the plain C path. By hand:
    the integer hash and the float-to-tick conversion (commented where they
    stand).

    Version: v0.1
    The effect: makes a clean rendered picture look like worn film in a
      projector.
    Typical use: a silent-film cutscene, a flashback, a title card, an old
      newsreel on a screen inside a scene.

    \author Jilted Valkyrie

    PVR terms (TA, TR/OP list, EOL): see the PVR glossary in README.md.
*/

#ifndef SH4DER_FILM_H
#define SH4DER_FILM_H

#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Limits and defaults */
/**@{*/
#define SH4DER_FILM_SCRATCH_SLOTS 3u      /**< \brief Scratches that can be alive at once (one a slot). */
#define SH4DER_FILM_SPECKS_MAX    12u     /**< \brief Most specks one tick can have. */
#define SH4DER_FILM_FLICKER_MAX   0.04f   /**< \brief Largest flicker depth sh4der_film_valid accepts: the brightness then swings by twice this, 8 % peak to peak. */
#define SH4DER_FILM_RATE_MAX_HZ   3.0f    /**< \brief Fastest rate a flicker sine may have, hertz: above it a flicker reads as a strobe. */
#define SH4DER_FILM_WEAVE_MAX_PX  2.0f    /**< \brief Largest weave sh4der_film_valid accepts, pixels. */
#define SH4DER_FILM_TIME_MAX_S    3600.0f /**< \brief Largest time the functions take, seconds (an hour); beyond it they give "no wear". The sines' turn count is a single-precision product: at an hour it is still exact to about 0.002 of a turn. */
#define SH4DER_FILM_TAU           6.2831853f /**< \brief 2 pi: radians in a full turn. */
#define SH4DER_FILM_HASH_MASK     0x00ffffffu /**< \brief The hash bits kept for a 0 .. 1 float (24: exact in a float). */
#define SH4DER_FILM_HASH_SCALE    (1.0f / 16777216.0f) /**< \brief 1 / 2^24. */
/**@}*/

/** \brief How the film is worn. Fill with sh4der_film_init, then change what you need; lengths are pixels of the picture
    the wear is laid over, times are seconds. 92 bytes. */
typedef struct sh4der_film {
    float width;             /**< \brief Width of the picture the wear covers, pixels (4 .. 4096: a speck is up to 3 pixels a side). */
    float height;            /**< \brief Its height, pixels (4 .. 4096). */
    float tick_hz;           /**< \brief Ticks a second: lives and speck places change on a tick (10 .. 240). */
    float flicker;           /**< \brief Flicker depth: the brightness wanders between 1 - 2 x this and 1, so by 2 x this peak to peak (0 .. SH4DER_FILM_FLICKER_MAX). */
    float flicker_hz_a;      /**< \brief Rate of the first of the two sines, hertz (0 .. SH4DER_FILM_RATE_MAX_HZ). */
    float flicker_hz_b;      /**< \brief Rate of the second (0 .. SH4DER_FILM_RATE_MAX_HZ); not a simple multiple of the first. */
    float weave_px;          /**< \brief Gate weave: the offset stays within this many pixels in x and in y (0 .. SH4DER_FILM_WEAVE_MAX_PX). */
    float weave_hz;          /**< \brief Rate of the weave, hertz (0 .. 2 x SH4DER_FILM_RATE_MAX_HZ). */
    float scratch_light;     /**< \brief Share of scratches that are light, 0 .. 1; the rest are dark. */
    float scratch_alpha_lo;  /**< \brief Faintest scratch alpha, 0 .. 1. */
    float scratch_alpha_hi;  /**< \brief Strongest scratch alpha (>= lo, <= 1). */
    float scratch_drift;     /**< \brief A scratch drifts sideways by up to this many pixels a tick, either way (0 .. 4). */
    float keep_x0;           /**< \brief Columns keep_x0 .. keep_x1 get no scratch (keep_x1 <= keep_x0: no keep-out span). A span that leaves no whole column on either side leaves no scratch at all. */
    float keep_x1;
    float speck_dark;        /**< \brief Share of specks that are dark, 0 .. 1; the rest are white. */
    float keep_y0;           /**< \brief Rows keep_y0 .. keep_y1 take at most keep_specks specks a tick (keep_y1 <= keep_y0: no band). */
    float keep_y1;
    uint32_t scratch_min;    /**< \brief Shortest life of a scratch, ticks (1 .. scratch_max). */
    uint32_t scratch_max;    /**< \brief Longest life, ticks (<= 255). A slot's cycle is this long plus a gap of the same length at the most. */
    uint32_t specks_min;     /**< \brief Fewest specks a tick (<= specks_max). */
    uint32_t specks_max;     /**< \brief Most specks a tick (<= SH4DER_FILM_SPECKS_MAX). */
    uint32_t keep_specks;    /**< \brief Most specks a tick inside the keep-out band of rows. */
    uint32_t seed;           /**< \brief Any number: picks where and when the wear falls. */
} sh4der_film_t;

/** \brief One scratch at one moment. */
typedef struct sh4der_film_scratch {
    float x;                 /**< \brief Its column: the left edge of a line one pixel wide, pixels from the picture's left edge. */
    float alpha;             /**< \brief 0 .. 1. */
    int light;               /**< \brief 1: white (blend it over the picture as white), 0: black. */
} sh4der_film_scratch_t;

/** \brief One dust speck of a tick. */
typedef struct sh4der_film_speck {
    float x, y;              /**< \brief Its top left corner, pixels from the picture's top left. */
    float size;              /**< \brief Its side, pixels: 1, 2 or 3. */
    float alpha;             /**< \brief 0 .. 1. */
    int light;               /**< \brief 1: white, 0: black. */
} sh4der_film_speck_t;

/* a 32-bit integer hash of three numbers (hand: integer arithmetic; a well-mixed word, the same on every machine) */
static inline uint32_t sh4der_film_hash_(uint32_t a, uint32_t b, uint32_t c) {
    uint32_t h = a * 2654435761u ^ (b + 0x9e3779b9u) * 0x85ebca6bu ^ (c + 0x7f4a7c15u) * 0xc2b2ae35u;

    h ^= h >> 15;
    h *= 0x2c1b3c6du;
    h ^= h >> 12;
    h *= 0x297a2d39u;
    h ^= h >> 15;
    return h;
}

/* that hash as a float 0 .. 1 (hand: the low 24 bits, exact in a float; never 1) */
static inline float sh4der_film_hash01_(uint32_t a, uint32_t b, uint32_t c) {
    return (float)(sh4der_film_hash_(a, b, c) & SH4DER_FILM_HASH_MASK) * SH4DER_FILM_HASH_SCALE;
}

/** \brief Fill `f` with the DEFAULTS for a width x height picture: 60 ticks a second; flicker 2.5 % deep from sines of
    1.3 and 2.1 hertz; weave 1 pixel at 2.3 hertz; scratches alive 6 to 20 ticks, 80 % of them light, alpha 0.20 to 0.40,
    drifting up to 1 pixel a tick, no keep-out span; 6 to 12 specks a tick, 75 % dark, no keep-out band; seed 1. */
static inline void sh4der_film_init(sh4der_film_t *f, float width, float height) {
    if (!f) {
        return;
    }
    f->width = width;
    f->height = height;
    f->tick_hz = 60.0f;
    f->flicker = 0.025f;
    f->flicker_hz_a = 1.3f;
    f->flicker_hz_b = 2.1f;
    f->weave_px = 1.0f;
    f->weave_hz = 2.3f;
    f->scratch_light = 0.8f;
    f->scratch_alpha_lo = 0.20f;
    f->scratch_alpha_hi = 0.40f;
    f->scratch_drift = 1.0f;
    f->keep_x0 = 0.0f;
    f->keep_x1 = 0.0f;
    f->speck_dark = 0.75f;
    f->keep_y0 = 0.0f;
    f->keep_y1 = 0.0f;
    f->scratch_min = 6u;
    f->scratch_max = 20u;
    f->specks_min = 6u;
    f->specks_max = 12u;
    f->keep_specks = 0u;
    f->seed = 1u;
}

/* 1 when v is finite and lo <= v <= hi */
static inline int sh4der_film_in_(float v, float lo, float hi) {
    return sh4der_finite_(v) && v >= lo && v <= hi;
}

/** \brief 1 when every field of `f` is finite and inside its stated range (see the struct), else 0. A flicker deeper than
    SH4DER_FILM_FLICKER_MAX or faster than SH4DER_FILM_RATE_MAX_HZ is refused: it would read as a strobe. Every function
    below gives "no wear" for a block this refuses. */
static inline int sh4der_film_valid(const sh4der_film_t *f) {
    if (!f) {
        return 0;
    }
    if (!sh4der_film_in_(f->width, 4.0f, 4096.0f) || !sh4der_film_in_(f->height, 4.0f, 4096.0f) || !sh4der_film_in_(f->tick_hz, 10.0f, 240.0f)) {
        return 0;
    }
    if (!sh4der_film_in_(f->flicker, 0.0f, SH4DER_FILM_FLICKER_MAX) || !sh4der_film_in_(f->flicker_hz_a, 0.0f, SH4DER_FILM_RATE_MAX_HZ) ||
        !sh4der_film_in_(f->flicker_hz_b, 0.0f, SH4DER_FILM_RATE_MAX_HZ)) {
        return 0;
    }
    if (!sh4der_film_in_(f->weave_px, 0.0f, SH4DER_FILM_WEAVE_MAX_PX) || !sh4der_film_in_(f->weave_hz, 0.0f, 2.0f * SH4DER_FILM_RATE_MAX_HZ)) {
        return 0;
    }
    if (!sh4der_film_in_(f->scratch_light, 0.0f, 1.0f) || !sh4der_film_in_(f->scratch_alpha_lo, 0.0f, 1.0f) ||
        !sh4der_film_in_(f->scratch_alpha_hi, f->scratch_alpha_lo, 1.0f) || !sh4der_film_in_(f->scratch_drift, 0.0f, 4.0f)) {
        return 0;
    }
    if (!sh4der_film_in_(f->keep_x0, -8192.0f, 8192.0f) || !sh4der_film_in_(f->keep_x1, -8192.0f, 8192.0f) ||
        !sh4der_film_in_(f->keep_y0, -8192.0f, 8192.0f) || !sh4der_film_in_(f->keep_y1, -8192.0f, 8192.0f) || !sh4der_film_in_(f->speck_dark, 0.0f, 1.0f)) {
        return 0;
    }
    if (f->keep_x1 > f->keep_x0 && f->keep_x1 - f->keep_x0 >= f->width) {
        return 0;                                      /* a keep-out span as wide as the picture leaves no column for a scratch */
    }
    return f->scratch_min >= 1u && f->scratch_min <= f->scratch_max && f->scratch_max <= 255u && f->specks_min <= f->specks_max &&
           f->specks_max <= SH4DER_FILM_SPECKS_MAX;
}

/* 1 when t is a time the functions take: finite and 0 .. SH4DER_FILM_TIME_MAX_S */
static inline int sh4der_film_time_ok_(float t) {
    return sh4der_film_in_(t, 0.0f, SH4DER_FILM_TIME_MAX_S);
}

/* the tick of time t (hand: float to integer; t x tick_hz is at most 864,000 here, exact in a float and far inside 32 bits) */
static inline uint32_t sh4der_film_tick_(const sh4der_film_t *f, float t) {
    return (uint32_t)sh4der_trunc_(t * f->tick_hz);
}

/* sin(2 pi (hz t + phase)). The turn count hz x t + phase is a single-precision product, so it carries an error that grows
 * with t (about 0.002 of a turn at SH4DER_FILM_TIME_MAX_S and 6 hertz: the reason the time is capped); only its fraction
 * goes to the sine, which wants a small angle. */
static inline float sh4der_film_wave_(float hz, float t, float phase) {
    float s, c;

    sh4der_sincos_(sh4der_frac_(sh4der_fma_(hz, t, phase)) * SH4DER_FILM_TAU, &s, &c);
    (void)c;
    return s;
}

/** \brief The picture's brightness at time `t`, as a multiplier: 1 - flicker x (1 + w), with w -1 .. 1 the sum of two
    sines (0.6 of the first rate, 0.4 of the second, each with a phase from the seed). So it lies in 1 - 2 x flicker .. 1
    and its mean is 1 - flicker. Returns 1 (no flicker) for an invalid block or time. */
static inline float sh4der_film_exposure(const sh4der_film_t *f, float t) {
    float w;

    if (!sh4der_film_valid(f) || !sh4der_film_time_ok_(t)) {
        return 1.0f;
    }
    w = sh4der_fma_(0.6f, sh4der_film_wave_(f->flicker_hz_a, t, sh4der_film_hash01_(f->seed, 1u, 0u)),
                    0.4f * sh4der_film_wave_(f->flicker_hz_b, t, sh4der_film_hash01_(f->seed, 2u, 0u)));
    return 1.0f - f->flicker * (1.0f + sh4der_clamp_(w, -1.0f, 1.0f));
}

/** \brief The colour of the full-screen MULTIPLY rectangle at time `t` (blend: frame x this colour), as 0xFFRRGGBB: the
    exposure times a tint. `tint` is the film base's colour as multipliers 0 .. 1 a channel (NULL: none) and `strength`
    0 .. 1 how much of it is mixed in: each channel is exposure x lerp(1, tint, strength). */
static inline uint32_t sh4der_film_exposure_argb(const sh4der_film_t *f, float t, const float tint[3], float strength) {
    const float e = sh4der_film_exposure(f, t);
    const float k = sh4der_finite_(strength) ? sh4der_sat_(strength) : 0.0f;
    uint32_t argb = 0xff000000u;
    int i;

    for (i = 0; i < 3; i++) {
        const float m = (tint && sh4der_finite_(tint[i])) ? sh4der_lerp_(1.0f, sh4der_sat_(tint[i]), k) : 1.0f;

        argb |= (uint32_t)sh4der_fma_(sh4der_sat_(e * m), 255.0f, 0.5f) << (16 - 8 * i);   /* hand: the colour byte */
    }
    return argb;
}

/** \brief The gate weave at time `t`: *dx and *dy, each within +-weave_px, to add to the projection centre of the
    picture (not to its masks, captions or titles). x and y move at slightly different rates and phases, so the picture
    wanders and does not swing along one line. Gives 0, 0 for an invalid block or time. */
static inline void sh4der_film_weave(const sh4der_film_t *f, float t, float *dx, float *dy) {
    if (!dx || !dy) {
        return;
    }
    *dx = 0.0f;
    *dy = 0.0f;
    if (!sh4der_film_valid(f) || !sh4der_film_time_ok_(t)) {
        return;
    }
    *dx = f->weave_px * sh4der_film_wave_(f->weave_hz, t, sh4der_film_hash01_(f->seed, 3u, 0u));
    *dy = f->weave_px * sh4der_film_wave_(f->weave_hz * 1.37f, t, sh4der_film_hash01_(f->seed, 4u, 0u));   /* 1.37: no common period with x */
}

/** \brief The scratch of slot `slot` (0 .. SH4DER_FILM_SCRATCH_SLOTS - 1) at time `t`. Returns 1 and fills `out` when
    the slot has a scratch alive, 0 when it has none (between two scratches), the slot does not exist, the keep-out span leaves
    no column for it, or the block or time is invalid.
    How a slot runs: its time is cut into cycles of 2 x scratch_max ticks; in each cycle one scratch lives for
    scratch_min .. scratch_max ticks, starting somewhere in the cycle's first half, at a column, alpha and drift of its
    own. The slots' cycles are a third of a cycle apart. The line never leaves the picture. A column inside the keep-out
    span is moved to the nearer side of it that still lies in the picture (a span against the picture's edge has one
    such side; a span against both edges has none, and then there is no scratch). */
static inline int sh4der_film_scratch(const sh4der_film_t *f, uint32_t slot, float t, sh4der_film_scratch_t *out) {
    uint32_t tick, cycle_len, cycle, at, life, start;
    float x, drift;

    if (!out || slot >= SH4DER_FILM_SCRATCH_SLOTS || !sh4der_film_valid(f) || !sh4der_film_time_ok_(t)) {
        return 0;
    }
    cycle_len = 2u * f->scratch_max;
    tick = sh4der_film_tick_(f, t) + slot * (cycle_len / SH4DER_FILM_SCRATCH_SLOTS);
    cycle = tick / cycle_len;
    at = tick % cycle_len;                                                          /* ticks into this cycle */
    life = f->scratch_min + sh4der_film_hash_(f->seed, 10u + slot, cycle) % (f->scratch_max - f->scratch_min + 1u);
    start = sh4der_film_hash_(f->seed, 20u + slot, cycle) % (f->scratch_max + 1u);  /* 0 .. scratch_max: the scratch ends inside the cycle */
    if (at < start || at >= start + life) {
        return 0;
    }
    x = sh4der_film_hash01_(f->seed, 30u + slot, cycle) * (f->width - 1.0f);
    drift = (2.0f * sh4der_film_hash01_(f->seed, 40u + slot, cycle) - 1.0f) * f->scratch_drift;
    x = sh4der_clamp_(sh4der_fma_(drift, (float)(at - start), x), 0.0f, f->width - 1.0f);   /* inside the picture first */
    if (f->keep_x1 > f->keep_x0 && x > f->keep_x0 - 1.0f && x < f->keep_x1) {       /* out of the keep-out span */
        const float left = f->keep_x0 - 1.0f;                                       /* the last column left of the span */
        const float right = f->keep_x1;                                             /* the first column right of it */
        const int left_ok = left >= 0.0f;
        const int right_ok = right <= f->width - 1.0f;

        if (!left_ok && !right_ok) {
            return 0;                                  /* the span reaches both edges: no column is left */
        }
        x = (left_ok && (!right_ok || x - f->keep_x0 < f->keep_x1 - x)) ? left : right;   /* the nearer side that is in the picture */
    }
    out->x = x;
    out->alpha = sh4der_lerp_(f->scratch_alpha_lo, f->scratch_alpha_hi, sh4der_film_hash01_(f->seed, 50u + slot, cycle));
    out->light = sh4der_film_hash01_(f->seed, 60u + slot, cycle) < f->scratch_light;
    return 1;
}

/** \brief The dust specks of the tick of time `t`: fills out[0 .. n - 1] and returns n, at most `cap` and at most
    specks_max (0 for an invalid block or time, or no room). Each tick has specks_min .. specks_max specks at new
    places, 1 to 3 pixels a side, the dark ones at alpha 0.40 .. 0.60 and the white ones at 0.50. A speck whose rows
    touch the keep-out band is dropped once the band has keep_specks of them. */
static inline unsigned sh4der_film_specks(const sh4der_film_t *f, float t, sh4der_film_speck_t *out, unsigned cap) {
    uint32_t tick, want, i;
    unsigned n = 0, in_band = 0;

    if (!out || cap == 0u || !sh4der_film_valid(f) || !sh4der_film_time_ok_(t)) {
        return 0;
    }
    tick = sh4der_film_tick_(f, t);
    want = f->specks_min + sh4der_film_hash_(f->seed, 100u, tick) % (f->specks_max - f->specks_min + 1u);
    for (i = 0; i < want && n < cap; i++) {
        sh4der_film_speck_t s;

        s.size = (float)(1u + sh4der_film_hash_(f->seed, 110u + i, tick) % 3u);
        s.x = sh4der_film_hash01_(f->seed, 130u + i, tick) * sh4der_clamp_(f->width - s.size, 0.0f, f->width);
        s.y = sh4der_film_hash01_(f->seed, 150u + i, tick) * sh4der_clamp_(f->height - s.size, 0.0f, f->height);
        s.light = !(sh4der_film_hash01_(f->seed, 170u + i, tick) < f->speck_dark);
        s.alpha = s.light ? 0.5f : sh4der_lerp_(0.40f, 0.60f, sh4der_film_hash01_(f->seed, 190u + i, tick));
        if (f->keep_y1 > f->keep_y0 && s.y + s.size > f->keep_y0 && s.y < f->keep_y1) {
            if (in_band >= f->keep_specks) {
                continue;                              /* the band has its share already */
            }
            in_band++;
        }
        out[n++] = s;
    }
    return n;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_FILM_H */
