/* SH4DER-PVR: per-vertex UV warp

   sh4der_uvwarp.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_uvwarp.h
    \brief   A texture coordinate pushed by a small two-channel table: the offset a
             material would apply per pixel, applied per vertex instead.

    The PVR has no per-pixel UV offset (no bump offset, no distortion read). What it
    can do is move a vertex's UV. A mesh with enough vertices along the motion (an
    electric arc, a flag, a water strip, a banner) gets most of the wobble from
    that, and the table keeps the shape of the original distortion map.

    The warp is: offset(u, v) = weight x table(u + su t, v + sv t), table read
    bilinear with both axes wrapping, each channel 0..1 and unsigned (a positive
    push; nothing re-centres it, so the picture drifts the way the source map
    pushes it). weight is per vertex (a fade at the ends, a mask), the caller's,
    0..1 (saturated; a non-finite weight is 0). The vertex UV becomes uv + offset.
    The table scrolls with time at (su, sv) a second: a table coordinate, not a
    mesh one. Give the mesh ends a weight that fades to 0, else a non-zero weight at
    an end slides it off its anchor.

    Build the table from the source map (any two channels) by a box filter down to
    a small size (64 x 16 is 2 KB); the build tool owns that. Here is the runtime.

    The table is a byte array the caller owns: w x h pairs (R, G), row-major, v
    then u, one byte a channel. It is only ever read a byte at a time, so any
    pointer is fine (the SH-4 stops on an unaligned wide load; nothing here casts).
    The maths is SH4ZAM (shz_fractf, shz_truncf, shz_lerpf, shz_fmaf); define
    SH4DER_CORE_NO_SH4ZAM for the plain C path.

    Domain: u, v and the scroll sum u + su t are finite and under 2^23 in size;
    anything else, and a bad table, gives 0, 0 and reads nothing. Precision: the plain
    form takes frac(u + su t) from the float sum, whose spacing doubles every octave:
    it stays within 1e-3 of the table range while |u + su t| < 2^23 / (w x 256) and
    drifts past that (about 0.01 at 2^10, 0.08 at 2^13, 0.36 at 2^16 on a 64 wide
    table), so WRAP the time to the scroll's period now and then. The prepared form
    splits the scroll's whole part off first (its fraction is exact) and stays within
    1e-3 for any time that prepare accepts; it wants u and v from -32 up (a mesh UV is
    a small number).

    Accuracy: the offset is exact at a texel centre and linear between (a bilinear
    read of bytes), 1.1e-3 of the table range at worst (measured 1.02e-3 on a 256 wide
    table), 1e-4 on a 64 wide one.

    Version: v0.10
    The effect: an arc crawls, a banner ripples, a water strip shimmers, from one
      small table and one weight a vertex.
    Developed for: an electric arc in a cutscene.
    Use it for: anything with a distortion map in the original that the PVR cannot
      read per pixel.

    \author Jilted Valkyrie

    PVR terms (TA, TR/OP list, EOL, twiddled, direct render): see the PVR glossary in README.md.
*/

#ifndef SH4DER_UVWARP_H
#define SH4DER_UVWARP_H

#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Limits
    @{ */
#define SH4DER_UVWARP_SIDE_MAX 256   /**< \brief Table width and height are 1 .. this (a 256 x 256 table is 128 KB: a build error, not a choice). */
#define SH4DER_UVWARP_UV_MAX   8388608.0f   /**< \brief 2^23: a coordinate at or past this is out of the domain (0, 0). */
/** @} */

/** \brief A warp table. rg points at w x h pairs of bytes (R, G), row-major, v rows of u. */
typedef struct {
    const uint8_t *rg;
    uint16_t       w, h;
} sh4der_uvwarp_t;

/** \brief 1: the table can be read (a pointer, w and h in 1 .. SH4DER_UVWARP_SIDE_MAX); 0: sample / offset return zeros. */
static inline int sh4der_uvwarp_ok(const sh4der_uvwarp_t *t) {
    return t && t->rg && t->w >= 1 && t->h >= 1 && t->w <= SH4DER_UVWARP_SIDE_MAX && t->h <= SH4DER_UVWARP_SIDE_MAX;
}

/* 1: x is a finite number inside +- 2^23. The finite test is a bit test (a float compare against NaN folds away under -ffast-math); the range after it is safe. */
static inline int sh4der_uvwarp_in_(float x) {
    return sh4der_finite_(x) && x > -SH4DER_UVWARP_UV_MAX && x < SH4DER_UVWARP_UV_MAX;
}

/** \brief The table at (u, v), both wrapping, bilinear; *r and *g 0..1. A bad table or a coordinate out of the domain gives 0, 0. */
static inline void sh4der_uvwarp_sample(const sh4der_uvwarp_t *t, float u, float v, float *r, float *g) {
    int x0, y0, x1, y1;
    float fx, fy, a, b, c, d;
    const uint8_t *p00, *p10, *p01, *p11;

    *r = 0.0f;
    *g = 0.0f;
    if (!sh4der_uvwarp_ok(t) || !sh4der_uvwarp_in_(u) || !sh4der_uvwarp_in_(v))
        return;
    {
        /* texel centres at (i + 0.5) / n: x in -0.5 .. n - 0.5, so x + 1 is positive and the cast is a floor */
        const float x = sh4der_frac_(u) * (float)t->w - 0.5f, y = sh4der_frac_(v) * (float)t->h - 0.5f;

        x0 = (int)sh4der_trunc_(x + 1.0f) - 1;
        y0 = (int)sh4der_trunc_(y + 1.0f) - 1;
        fx = x - (float)x0;
        fy = y - (float)y0;
    }
    x1 = x0 + 1;
    y1 = y0 + 1;
    if (x0 < 0) x0 = (int)t->w - 1;
    if (y0 < 0) y0 = (int)t->h - 1;
    if (x1 < 0) x1 = 0;                        /* never read before the table, whatever the float did */
    if (y1 < 0) y1 = 0;
    if (x1 >= (int)t->w) x1 = 0;
    if (y1 >= (int)t->h) y1 = 0;
    if (x0 >= (int)t->w) x0 = (int)t->w - 1;   /* frac() is 0 .. < 1, but a rounded 1.0 must not leave the row */
    if (y0 >= (int)t->h) y0 = (int)t->h - 1;
    p00 = t->rg + 2u * ((unsigned)y0 * t->w + (unsigned)x0);
    p10 = t->rg + 2u * ((unsigned)y0 * t->w + (unsigned)x1);
    p01 = t->rg + 2u * ((unsigned)y1 * t->w + (unsigned)x0);
    p11 = t->rg + 2u * ((unsigned)y1 * t->w + (unsigned)x1);
    a = sh4der_lerp_((float)p00[0], (float)p10[0], fx);
    b = sh4der_lerp_((float)p01[0], (float)p11[0], fx);
    *r = sh4der_lerp_(a, b, fy) * (1.0f / 255.0f);
    c = sh4der_lerp_((float)p00[1], (float)p10[1], fx);
    d = sh4der_lerp_((float)p01[1], (float)p11[1], fx);
    *g = sh4der_lerp_(c, d, fy) * (1.0f / 255.0f);
}

/** \brief The UV offset of one vertex: weight x table(u + su time, v + sv time) -> *du, *dv. weight 0 gives 0, 0, weight is saturated to 0..1 (non-finite: 0);
    a bad table or a coordinate out of the domain gives 0, 0. (su, sv): the table's scroll, table units a second. */
static inline void sh4der_uvwarp_offset(const sh4der_uvwarp_t *t, float u, float v, float su, float sv, float time, float weight, float *du, float *dv) {
    float r, g;

    sh4der_uvwarp_sample(t, sh4der_fma_(su, time, u), sh4der_fma_(sv, time, v), &r, &g);
    weight = sh4der_finite_(weight) ? sh4der_sat_(weight) : 0.0f;
    *du = r * weight;
    *dv = g * weight;
}

/** \name Prepared form: many vertices, one table, one time
    A mesh loop calls sh4der_uvwarp_prepare once, then sh4der_uvwarp_offset_p for each vertex. The scroll and the table size are
    hoisted, and the wrap is a bias plus a truncation (no floor call a vertex): the sum u + scroll + 33 is never negative, and adding
    a whole number does not change the fraction. It gives the same offset as sh4der_uvwarp_offset (to the accuracy above), and
    hoists the scroll and the table size out of the loop. sh4der_uvwarp_offset_pn is the same without the per-vertex input gates, for a loop that checked its
    inputs once (finite, u and v in the domain, weight 0..1).
    Domain: u and v from -32 up and under 2^23; anything outside, a non-finite input or a bad table gives 0, 0.
    @{ */
typedef struct {
    const uint8_t *rg;
    int            w, h;
    float          wf, hf;      /**< w and h as floats. */
    float          ou, ov;      /**< The scroll's fraction (-1 .. 1) plus 33: whole numbers do not change a fraction. */
    int            ok;
} sh4der_uvwarp_prep_t;

/** \brief Hoist what is the same for every vertex. su, sv: table units a second, time: seconds. Returns 1 when the warp can run;
    0 (a bad table, a non-finite or a huge scroll): sh4der_uvwarp_offset_p then gives 0, 0 for every vertex, so a caller may skip the loop. */
static inline int sh4der_uvwarp_prepare(sh4der_uvwarp_prep_t *p, const sh4der_uvwarp_t *t, float su, float sv, float time) {
    const float x = su * time, y = sv * time;

    p->rg = t ? t->rg : 0;
    p->w = t ? (int)t->w : 0;
    p->h = t ? (int)t->h : 0;
    p->wf = (float)p->w;
    p->hf = (float)p->h;
    p->ou = p->ov = 33.0f;
    p->ok = sh4der_uvwarp_ok(t) && sh4der_uvwarp_in_(x) && sh4der_uvwarp_in_(y);
    if (!p->ok)
        return 0;
    p->ou += x - sh4der_trunc_(x);
    p->ov += y - sh4der_trunc_(y);
    return 1;
}

/** \brief The offset of one vertex of a prepared warp WITHOUT the input gates: the caller guarantees prepare returned 1, u and v are finite and in -32 .. 2^23 - 34, and
    weight is 0..1. A mesh loop that knows its inputs (a UV from a fixed-point field, a weight from a byte) checks them once and calls this. Out-of-domain input is not defended. */
static inline void sh4der_uvwarp_offset_pn(const sh4der_uvwarp_prep_t *p, float u, float v, float weight, float *du, float *dv) {
    *du = 0.0f;
    *dv = 0.0f;
    if (weight == 0.0f)
        return;
    {
        const float fu = u + p->ou, fv = v + p->ov;

        {
            const float xu = fu - sh4der_trunc_(fu), xv = fv - sh4der_trunc_(fv);     /* the fraction, 0 .. < 1 */
            const float x = xu * p->wf - 0.5f, y = xv * p->hf - 0.5f;
            int x0 = (int)sh4der_trunc_(x + 1.0f) - 1, y0 = (int)sh4der_trunc_(y + 1.0f) - 1, x1, y1;
            const float fx = x - (float)x0, fy = y - (float)y0;
            const uint8_t *p00, *p10, *p01, *p11;
            float a, b, c, d;

            x1 = x0 + 1;
            y1 = y0 + 1;
            if (x0 < 0) x0 = p->w - 1;
            if (y0 < 0) y0 = p->h - 1;
            if (x1 >= p->w) x1 = 0;
            if (y1 >= p->h) y1 = 0;
            p00 = p->rg + 2 * (y0 * p->w + x0);
            p10 = p->rg + 2 * (y0 * p->w + x1);
            p01 = p->rg + 2 * (y1 * p->w + x0);
            p11 = p->rg + 2 * (y1 * p->w + x1);
            a = sh4der_lerp_((float)p00[0], (float)p10[0], fx);
            b = sh4der_lerp_((float)p01[0], (float)p11[0], fx);
            c = sh4der_lerp_((float)p00[1], (float)p10[1], fx);
            d = sh4der_lerp_((float)p01[1], (float)p11[1], fx);
            *du = sh4der_lerp_(a, b, fy) * (weight * (1.0f / 255.0f));
            *dv = sh4der_lerp_(c, d, fy) * (weight * (1.0f / 255.0f));
        }
    }
}

/** \brief The offset of one vertex of a prepared warp (see sh4der_uvwarp_offset; weight saturated to 0..1, non-finite 0; u, v out of the domain 0, 0). */
static inline void sh4der_uvwarp_offset_p(const sh4der_uvwarp_prep_t *p, float u, float v, float weight, float *du, float *dv) {
    *du = 0.0f;
    *dv = 0.0f;
    if (!p->ok || !sh4der_finite_(weight) || !sh4der_finite_(u) || !sh4der_finite_(v))
        return;
    {
        const float fu = u + p->ou, fv = v + p->ov;

        if (!(fu >= 0.0f && fu < SH4DER_UVWARP_UV_MAX && fv >= 0.0f && fv < SH4DER_UVWARP_UV_MAX))
            return;
    }
    sh4der_uvwarp_offset_pn(p, u, v, sh4der_sat_(weight), du, dv);
}
/** @} */

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_UVWARP_H */
