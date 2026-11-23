/* SH4DER-PVR: a bank of mist as stacked translucent slices

   sh4der_mist.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_mist.h
    \brief   A fog bank you can see the shape of: a few upright translucent sheets
             standing one behind the other, each with a slowly drifting noise
             texture and a top edge that fades out.

    Cost first. RAM: the 64 B parameter block; nothing per slice. Video memory: one
    noise texture, the caller's: 64 x 64 at 16 bits is 8 KB (sh4der_mist_noise
    fills its alpha). Triangles: 4 a slice, so 48 for the default 12 slices, on the
    translucent list, blended. CPU: about 120 multiplies, 7 divides and one sine-cosine
    pair a slice a frame (a count, not a time; most of it is the per-slice alpha, which a caller may compute once). Fill is the real cost and is not measured:
    every slice covers its whole area on screen, so a bank that fills the frame is
    12 full-screen blended layers. Keep the bank low and partial (a band across the
    street, not a wall across the view), or use fewer slices.

    Table fog (the PVR's fog by depth) tints what is far away, but it has no shape:
    it cannot be a bank lying in a street with clear air above it. This module is
    the shape: `slices` sheets spaced through `depth` world units along a direction
    in the ground plane, each `2 x half_width` wide and standing from `base_y` to
    `top_y`. The lowest part of a sheet has the slice's full alpha; over the top
    `feather` world units the alpha falls to 0, so the bank has a soft top instead
    of a ruled line. The PVR's translucent sort puts every sheet in order with the
    rest of the scene for free, so a figure standing in the bank is hidden by the
    sheets in front of it and hides the ones behind.

    What moves: each sheet's texture slides sideways at its own speed and wobbles
    up and down a little (both hashed from the slice number and `seed`), so the
    bank seethes and no two sheets move together. To make a bank grow, shrink,
    thin or roll in, change the block's numbers over time (half_width, top_y,
    depth, opacity): every call recomputes from the block, there is no state.

    Close to the eye a sheet would cut across the picture as a hard plane, so a
    sheet fades out as the eye comes within `near_fade` world units of it, and one
    behind the eye is not drawn.

    The caller projects the six vertices of a slice (a strip, 4 triangles), runs
    its area guard (sh4der_tri_area_ok) and submits them with its own compiled
    header: the noise texture, blend source alpha / 1 - source alpha, the fog's
    colour in the vertex colour with the vertex alpha this module gives, depth
    test on, depth write off, no culling.

    No allocation, no globals. The maths is SH4ZAM through sh4der_core.h (fused
    multiply-add, blend, clamp, divide, sine and cosine); define
    SH4DER_CORE_NO_SH4ZAM for the plain C path. By hand: the integer hash and the
    byte values of the noise.

    Version: v0.10
    The effect: a low bank of mist with a soft top that drifts and seethes; things
      walk into it and are lost.
    Developed for: a foggy night street in a short film demo.
    Use it for: ground fog, a mist bank on water, steam over a vent, smoke under a
      ceiling (turn it over), dust in a valley, low cloud round a peak.

    \author Jilted Valkyrie

    PVR terms (TA, TR/OP list, EOL, twiddled, direct render): see the PVR glossary in README.md.
*/

#ifndef SH4DER_MIST_H
#define SH4DER_MIST_H

#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_MIST_MAX_SLICES   16       /**< \brief Most slices a bank may have. */
#define SH4DER_MIST_SLICES       12       /**< \brief Default slices. */
#define SH4DER_MIST_SLICE_VERTS  6        /**< \brief Vertices of one slice: a strip of 3 rows x 2 columns, 4 triangles. */
#define SH4DER_MIST_OPACITY      0.7f     /**< \brief Default share of the light the whole bank stops, looking through all of it. */
#define SH4DER_MIST_FEATHER      2.0f     /**< \brief Default height of the soft top, world units (metres). */
#define SH4DER_MIST_NEAR_FADE    3.0f     /**< \brief Default distance at which a slice starts to fade as the eye nears it, world units. */
#define SH4DER_MIST_DRIFT        0.02f    /**< \brief Default sideways slide of the texture, texture widths a second. */
#define SH4DER_MIST_SEETHE       0.03f    /**< \brief Default up-and-down wobble of the texture, texture heights. */
#define SH4DER_MIST_SEETHE_RATE  0.4f     /**< \brief Default wobble speed, radians a second. */
#define SH4DER_MIST_ALPHA_ROUNDS 5        /**< rief Newton rounds sh4der_mist_slice_alpha takes (each at most 16 multiplies and a divide). */
#define SH4DER_MIST_TILE         8.0f     /**< \brief Default world units one texture width covers across a slice. */
/** @} */

/** \brief A bank. Fill it with sh4der_mist_init, then change any field. */
typedef struct sh4der_mist {
    float x, z;            /**< \brief Centre of the bank's NEAR face in the ground plane, world units. */
    float dir_x, dir_z;    /**< \brief Unit direction in the ground plane the slices are stacked along (near to far). */
    float half_width;      /**< \brief Half the bank's width across that direction, world units. */
    float base_y, top_y;   /**< \brief Bottom and top of the bank, world heights. */
    float feather;         /**< \brief Height of the soft top, world units; clamped to the bank's height. */
    float depth;           /**< \brief Distance from the first slice to the last, world units. */
    float opacity;         /**< \brief Share of light the whole bank stops, 0 .. 1 (1 - opacity is what comes through). */
    float near_fade;       /**< \brief A slice fades out as the eye comes within this distance of it, world units. */
    float drift;           /**< \brief Texture slide, widths a second; each slice takes its own multiple of it. */
    float seethe;          /**< \brief Texture wobble, heights. */
    float seethe_rate;     /**< \brief Wobble speed, radians a second. */
    float tile;            /**< \brief World units one texture width covers. */
    uint32_t slices;       /**< \brief Slices, 1 .. SH4DER_MIST_MAX_SLICES. */
    uint32_t seed;         /**< \brief Any number: another seed, another movement. */
} sh4der_mist_t;

/** \brief One vertex of a slice: world position, texture coordinate, alpha 0 .. 1. */
typedef struct sh4der_mist_vtx {
    float x, y, z, u, v, alpha;
} sh4der_mist_vtx_t;

/* a 32-bit integer hash (hand: integer arithmetic; a well-mixed word from a slice number and a salt) */
static inline uint32_t sh4der_mist_hash_(uint32_t a, uint32_t b) {
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA6Bu;

    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    h ^= h >> 12;
    h *= 0x297A2D39u;
    h ^= h >> 15;
    return h;
}

/* that hash as a float 0 .. 1 (hand: the top 24 bits, exact in a float) */
static inline float sh4der_mist_unit_(uint32_t a, uint32_t b) {
    return (float)(sh4der_mist_hash_(a, b) >> 8) * (1.0f / 16777216.0f);
}

/** \brief Sets a bank to the defaults: at the origin, stacked along +z, 6 m wide (half_width 3), 0 to 3 m high, 12 m deep. */
static inline void sh4der_mist_init(sh4der_mist_t *m) {
    if (!m) return;
    m->x = 0.0f;
    m->z = 0.0f;
    m->dir_x = 0.0f;
    m->dir_z = 1.0f;
    m->half_width = 3.0f;
    m->base_y = 0.0f;
    m->top_y = 3.0f;
    m->feather = SH4DER_MIST_FEATHER;
    m->depth = 12.0f;
    m->opacity = SH4DER_MIST_OPACITY;
    m->near_fade = SH4DER_MIST_NEAR_FADE;
    m->drift = SH4DER_MIST_DRIFT;
    m->seethe = SH4DER_MIST_SEETHE;
    m->seethe_rate = SH4DER_MIST_SEETHE_RATE;
    m->tile = SH4DER_MIST_TILE;
    m->slices = SH4DER_MIST_SLICES;
    m->seed = 1u;
}

/** \brief The alpha one slice needs so that all `slices` of them together stop `opacity` of the light.
    Looking through n slices of alpha a, (1 - a)^n comes through. The exact a is 1 - (1 - opacity)^(1 / n); this gives
    the answer of SH4DER_MIST_ALPHA_ROUNDS rounds of Newton's method from the first guess opacity / n, which is within
    0.01 of the exact value for opacity up to 0.9 and 4 to 16 slices (the test checks it; an n-th root would need a
    logarithm and an exponential for a difference the picture cannot show).
    Returns 0 for no slices or no opacity. */
static inline float sh4der_mist_slice_alpha(float opacity, uint32_t slices) {
    float a, through, target;
    int round;
    uint32_t k;

    if (slices < 1u || !(opacity > 0.0f)) return 0.0f;
    if (slices > SH4DER_MIST_MAX_SLICES) slices = SH4DER_MIST_MAX_SLICES;
    opacity = sh4der_sat_(opacity);
    if (slices == 1u) return opacity;
    target = 1.0f - opacity;                                    /* what must come through all the slices */
    a = sh4der_div_(opacity, (float)slices);
    for (round = 0; round < SH4DER_MIST_ALPHA_ROUNDS; round++) {
        float p = 1.0f;                                         /* (1 - a)^(n - 1), by repeated multiply: n is at most 16 */

        for (k = 1u; k < slices; k++) p *= 1.0f - a;
        through = p * (1.0f - a);                               /* (1 - a)^n */
        if (!(p > 1.0e-6f)) break;                              /* already opaque: no slope to follow */
        /* Newton on f(a) = (1 - a)^n - target, f'(a) = -n (1 - a)^(n - 1) */
        a = sh4der_sat_(a + sh4der_div_(through - target, (float)slices * p));
    }
    return a;
}

/** \brief The six vertices of slice i (0 = nearest the bank's near face) at time `time` seconds, seen from `eye`.
    Strip order: bottom-left, bottom-right, feather-left, feather-right, top-left, top-right: the two bottom rows carry
    the slice's alpha, the top row 0. `left` is the bank's left looking along its direction.
    Returns SH4DER_MIST_SLICE_VERTS, or 0 when there is nothing to draw: a bad argument, a slice the eye is level with
    or past (looking along the bank's direction), or one that has faded to nothing. Nothing is written when it returns 0. */
static inline unsigned sh4der_mist_slice(const sh4der_mist_t *m, uint32_t i, float time, const float eye[3], sh4der_mist_vtx_t out[SH4DER_MIST_SLICE_VERTS]) {
    float along, cx, cz, ahead, alpha, feather, mid_y, u0, u1, v_off, s, c, height, inv_h, v_mid;
    float lx, lz;
    unsigned k;

    if (!m || !eye || !out || m->slices < 1u || m->slices > SH4DER_MIST_MAX_SLICES || i >= m->slices) return 0;
    if (!(m->half_width > 0.0f) || !(m->top_y > m->base_y) || !(m->tile > 0.0f) || !(m->depth >= 0.0f)) return 0;
    if (!sh4der_finite_(time) || !sh4der_finite_(eye[0]) || !sh4der_finite_(eye[1]) || !sh4der_finite_(eye[2])) return 0;

    /* where this slice stands: evenly through the depth, the first on the near face */
    along = m->slices > 1u ? m->depth * sh4der_div_((float)i, (float)(m->slices - 1u)) : 0.0f;
    cx = sh4der_fma_(m->dir_x, along, m->x);
    cz = sh4der_fma_(m->dir_z, along, m->z);

    /* how far in front of the eye it is, measured along the bank's direction: behind or level = not drawn */
    ahead = sh4der_fma_(cx - eye[0], m->dir_x, (cz - eye[2]) * m->dir_z);
    if (!(ahead > 0.0f)) return 0;
    alpha = sh4der_mist_slice_alpha(m->opacity, m->slices);
    if (m->near_fade > 0.0f) alpha *= sh4der_sat_(sh4der_div_(ahead, m->near_fade));
    if (!(alpha > (1.0f / 255.0f))) return 0;                   /* under one step of an 8-bit alpha: invisible */

    height = m->top_y - m->base_y;
    feather = m->feather < 0.0f ? 0.0f : m->feather > height ? height : m->feather;
    mid_y = m->top_y - feather;                                 /* full alpha up to here, then the fade */
    inv_h = sh4der_div_(1.0f, height);
    v_mid = 1.0f - (mid_y - m->base_y) * inv_h;                 /* v runs 1 at the base to 0 at the top */

    /* the texture's place on this slice: its own slide speed (0.5 .. 1.5 of drift, half the slices the other way),
       its own start, and a small wobble up and down */
    u0 = sh4der_fma_(time * m->drift, (0.5f + sh4der_mist_unit_(i, m->seed)) * ((i & 1u) ? -1.0f : 1.0f), sh4der_mist_unit_(i, m->seed + 1u));
    u0 = sh4der_frac_(u0);                                      /* keep it small: a large u loses texel precision */
    u1 = u0 + sh4der_div_(m->half_width + m->half_width, m->tile);
    sh4der_sincos_(sh4der_fma_(time, m->seethe_rate, sh4der_mist_unit_(i, m->seed + 2u) * 6.2831853f), &s, &c);
    v_off = m->seethe * s;
    (void)c;

    lx = -m->dir_z;                                             /* the bank's left, looking along its direction: (-dir_z, dir_x) */
    lz = m->dir_x;
    for (k = 0; k < SH4DER_MIST_SLICE_VERTS; k++) {
        const float side = (k & 1u) ? -m->half_width : m->half_width;   /* even = left, odd = right */
        const unsigned row = k >> 1;                                   /* 0 base, 1 feather line, 2 top */

        out[k].x = sh4der_fma_(lx, side, cx);
        out[k].z = sh4der_fma_(lz, side, cz);
        out[k].y = row == 0u ? m->base_y : row == 1u ? mid_y : m->top_y;
        out[k].u = (k & 1u) ? u1 : u0;
        out[k].v = (row == 0u ? 1.0f : row == 1u ? v_mid : 0.0f) + v_off;
        out[k].alpha = row == 2u ? 0.0f : alpha;
    }
    return SH4DER_MIST_SLICE_VERTS;
}

/* one octave of the noise: a hashed lattice of `cells` x `cells` values that wraps, blended smoothly between its points */
static inline float sh4der_mist_lattice_(float fx, float fy, uint32_t cells, uint32_t seed) {
    const float gx = fx * (float)cells, gy = fy * (float)cells;
    const float ix = gx - sh4der_frac_(gx), iy = gy - sh4der_frac_(gy);
    const uint32_t x0 = (uint32_t)ix % cells, y0 = (uint32_t)iy % cells, x1 = (x0 + 1u) % cells, y1 = (y0 + 1u) % cells;
    float tx = gx - ix, ty = gy - iy;
    float a, b;

    tx = tx * tx * (3.0f - 2.0f * tx);                          /* smoothstep: no crease at the lattice points */
    ty = ty * ty * (3.0f - 2.0f * ty);
    a = sh4der_lerp_(sh4der_mist_unit_(x0 + y0 * 131u, seed), sh4der_mist_unit_(x1 + y0 * 131u, seed), tx);
    b = sh4der_lerp_(sh4der_mist_unit_(x0 + y1 * 131u, seed), sh4der_mist_unit_(x1 + y1 * 131u, seed), tx);
    return sh4der_lerp_(a, b, ty);
}

/** \brief Fills a w x h image with soft cloudy noise that tiles in both directions, one byte a texel (0 clear .. 255
    thick), for the caller to pack as the alpha of its texture (white, or the fog's colour, in the colour bits).
    Three octaves (4, 8 and 16 cells across, weights 4 : 2 : 1). Run it once at load: about 30 multiplies a texel.
    Returns 1, or 0 for a bad argument (nothing is written). */
static inline int sh4der_mist_noise(uint8_t *out, unsigned w, unsigned h, uint32_t seed) {
    const float inv_w = w ? sh4der_div_(1.0f, (float)w) : 0.0f, inv_h = h ? sh4der_div_(1.0f, (float)h) : 0.0f;
    unsigned x, y;

    if (!out || w < 4u || h < 4u || w > 1024u || h > 1024u) return 0;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            const float fx = (float)x * inv_w, fy = (float)y * inv_h;
            const float n = (4.0f * sh4der_mist_lattice_(fx, fy, 4u, seed) + 2.0f * sh4der_mist_lattice_(fx, fy, 8u, seed + 1u) +
                             sh4der_mist_lattice_(fx, fy, 16u, seed + 2u)) * (1.0f / 7.0f);

            out[y * w + x] = (uint8_t)(sh4der_sat_(n) * 255.0f + 0.5f);   /* hand: the byte */
        }
    }
    return 1;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_MIST_H */
