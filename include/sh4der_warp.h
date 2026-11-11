/* SH4DER-PVR: frame warps

   sh4der_warp.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_warp.h
    \brief   The maths of warps on the frame as a texture: a shockwave, a
             heat haze column, an underwater wobble, a radial speed blur and a
             pixelate / dither pass.

    The frame is rendered into a 512 x 256 RGB565 texture, its 320 x 240
    picture in the top left.
    Every warp draws that texture through a mesh or a set of copies:

    - Warp: a grid of cells (SH4DER_WARP_GRID_X x _Y, or the fine grid). Each
      vertex keeps its place and gets a UV offset: the picture is sampled at
      p + the sum of the sources' offsets, clamped to the frame so no black is
      pulled in. Sources: a ring (a shockwave from a point), a haze column (a
      rect, a sideways wave that climbs), a wobble (the whole frame, a sideways
      wave moving on). The sideways waves depend on the row only, so they are
      one sine per source per row; only the ring is per vertex.
    - Blur: SH4DER_WARP_BLUR_COPIES copies of the frame zoomed about a centre,
      each at weight 1 / k, so the brightness holds. Every copy covers the
      screen (zoom >= 1: the edge is never pulled in dark).
    - Pixelate: a block per SH4DER_WARP_PIXEL_CELL, each drawn flat with
      its centre texel (point sampled, all four UVs the same). Dither: a
      4 x 4 ordered pattern multiplied over the frame (a screen door, not a
      palette: the PVR has no per-pixel quantiser).

    Order, per frame: fill a sh4der_warp_frame_t, then for each grid row call
    sh4der_warp_row (the mesh), or use sh4der_warp_blur_scale and
    sh4der_warp_blur_weight (the copies), or the pixelate and dither helpers.

    The draw code for the mesh and the copies is in src/sh4der_warp.c (KOS,
    under _arch_dreamcast). This header is the numbers: host tested.

    \author Jilted Valkyrie
    \author Cortana (Claude, an AI by Anthropic): the effects' maths, the PVR paths, the tests
*/

#ifndef SH4DER_WARP_H
#define SH4DER_WARP_H

#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Shipped tuning (estimates: not yet tuned on a real frame)
    @{ */
#define SH4DER_WARP_FRAME_W     320      /**< \brief The frame's width, pixels. */
#define SH4DER_WARP_FRAME_H     240      /**< \brief The frame's height, pixels. */
#define SH4DER_WARP_TEX_W       512      /**< \brief The texture's width. */
#define SH4DER_WARP_TEX_H       256      /**< \brief The texture's height. */
#define SH4DER_WARP_SOURCES     4        /**< \brief Warp sources a frame holds. */
#define SH4DER_WARP_GRID_X      20       /**< \brief Coarse grid: cells across (16 pixels). */
#define SH4DER_WARP_GRID_Y      15       /**< \brief Coarse grid: cells down. */
#define SH4DER_WARP_FINE_X      40       /**< \brief Fine grid: cells across (8 pixels). */
#define SH4DER_WARP_FINE_Y      30       /**< \brief Fine grid: cells down. */
#define SH4DER_WARP_RING_SPEED  180.0f   /**< \brief Shockwave ring, frame pixels a second. */
#define SH4DER_WARP_RING_WIDTH  14.0f    /**< \brief Ring band: pixels from its middle to no push. */
#define SH4DER_WARP_RING_AMP    5.0f     /**< \brief Ring push at full strength, pixels. */
#define SH4DER_WARP_HAZE_AMP    1.5f     /**< \brief Haze column's sideways wave, pixels. */
#define SH4DER_WARP_HAZE_PERIOD 24.0f    /**< \brief Haze wave length, pixels. */
#define SH4DER_WARP_HAZE_RISE   40.0f    /**< \brief Haze wave climb, pixels a second. */
#define SH4DER_WARP_HAZE_EDGE   16.0f    /**< \brief Haze column's soft edge, pixels. */
#define SH4DER_WARP_WOBBLE_AMP  1.2f     /**< \brief Full-frame wobble, pixels. */
#define SH4DER_WARP_WOBBLE_PERIOD 96.0f  /**< \brief Wobble wave length, pixels. */
#define SH4DER_WARP_WOBBLE_TURN 4.0f     /**< \brief Seconds for the wobble to move one wave length on. */
#define SH4DER_WARP_BLUR_COPIES 4        /**< \brief Radial blur copies (at most). */
#define SH4DER_WARP_BLUR_ZOOM   0.06f    /**< \brief Outermost copy's zoom at full strength (1 + this). */
#define SH4DER_WARP_PIXEL_CELL  16       /**< \brief Pixelate block, frame pixels (divides 320 and 240). */
#define SH4DER_WARP_DITHER      0.25f    /**< \brief Dither pattern: the darkest share at full. */
/** @} */

/** \brief Source kinds. */
enum {
    SH4DER_WARP_RING,     /**< \brief Shockwave: a ring from (cx, cy), expanding. */
    SH4DER_WARP_HAZE,     /**< \brief Heat haze: a column over rect (x0, y0)-(x1, y1), waves climbing. */
    SH4DER_WARP_WOBBLE    /**< \brief Underwater: the whole frame, a sideways wave. */
};

/** \brief One source. `strength` (0..1) is the caller's envelope (fade in and out); `age` is seconds since it started. */
typedef struct sh4der_warp_src {
    uint8_t kind;               /**< \brief SH4DER_WARP_RING, _HAZE or _WOBBLE. */
    float cx, cy;               /**< \brief Ring centre, frame pixels. */
    float x0, y0, x1, y1;       /**< \brief Haze rect, frame pixels. */
    float age;                  /**< \brief Seconds. */
    float strength;             /**< \brief 0..1. */
} sh4der_warp_src_t;

/** \brief The sources of one frame. Offsets sum; sources past SH4DER_WARP_SOURCES are not read. */
typedef struct sh4der_warp_frame {
    unsigned n;                 /**< \brief Sources in use (SH4DER_WARP_SOURCES at most). */
    sh4der_warp_src_t src[SH4DER_WARP_SOURCES];
} sh4der_warp_frame_t;

/** \brief One grid vertex: its place (frame pixels) and its sample (texture UV). */
typedef struct sh4der_warp_vtx {
    float x, y, u, v;
} sh4der_warp_vtx_t;

/* ------------------------------------------------------------------------ */
/* The steps (inline: the client calls them only where it uses them). */

/* 1 / sqrt(x) for x > 0: SH4ZAM's safe form, or plain C on the host path. */
static inline float sh4der_warp_rsqrt_(float x) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_inv_sqrtf(x);
#else
    return 1.0f / sqrtf(x);
#endif
}

static inline float sh4der_warp_clampf_(float v, float lo, float hi) { return v < lo ? lo : v > hi ? hi : v; }

/* The sideways wave of a source at row y (frame pixels), strength in: haze climbs with age, the wobble moves on by age.
   A ring has no row term. Two sines a row at most: the caller computes this once per row, not per vertex. */
static inline float sh4der_warp_wave_(const sh4der_warp_src_t *s, float y) {
    const float tau = 6.2831853f;
    const float k = sh4der_warp_clampf_(s->strength, 0.0f, 1.0f);
    float arg, sn, cs;
    if (s->kind == SH4DER_WARP_HAZE) {
        arg = sh4der_frac_(sh4der_div_(y + SH4DER_WARP_HAZE_RISE * s->age, SH4DER_WARP_HAZE_PERIOD));
        sh4der_sincos_(tau * arg, &sn, &cs);
        return SH4DER_WARP_HAZE_AMP * k * sn;
    }
    if (s->kind == SH4DER_WARP_WOBBLE) {
        arg = sh4der_frac_(sh4der_div_(y, SH4DER_WARP_WOBBLE_PERIOD) + sh4der_div_(s->age, SH4DER_WARP_WOBBLE_TURN));
        sh4der_sincos_(tau * arg, &sn, &cs);
        return SH4DER_WARP_WOBBLE_AMP * k * sn;
    }
    return 0.0f;
}

/* The haze column's mask at a pixel: 1 inside the rect, a linear soft edge of HAZE_EDGE pixels, 0 outside. */
static inline float sh4der_warp_haze_mask_(const sh4der_warp_src_t *s, float x, float y) {
    const float inv = 1.0f / SH4DER_WARP_HAZE_EDGE;   /* a constant: folded at compile time */
    return sh4der_sat_((x - s->x0) * inv) * sh4der_sat_((s->x1 - x) * inv) *
           sh4der_sat_((y - s->y0) * inv) * sh4der_sat_((s->y1 - y) * inv);
}

/* The shockwave's push at a pixel: a band of RING_WIDTH either side of the ring's radius (RING_SPEED x age), smooth (e^2),
   pushing the picture OUT from the centre (the sample comes from inside). Zero at the centre and outside the band. */
static inline void sh4der_warp_ring_(const sh4der_warp_src_t *s, float x, float y, float out[2]) {
    const float ex = x - s->cx, ey = y - s->cy;
    const float d2 = sh4der_len2_(ex, ey);
    float inv_d, d, u, e, k;
    out[0] = 0.0f;
    out[1] = 0.0f;
    if (!(d2 > 1e-6f))
        return;
    inv_d = sh4der_warp_rsqrt_(d2);   /* once: the distance and the unit direction both come from it */
    d = d2 * inv_d;
    u = (d - SH4DER_WARP_RING_SPEED * s->age) * (1.0f / SH4DER_WARP_RING_WIDTH);
    if (u <= -1.0f || u >= 1.0f)
        return;
    e = 1.0f - u * u;
    e *= e;
    k = -SH4DER_WARP_RING_AMP * sh4der_sat_(s->strength) * e * inv_d;   /* (ex, ey) / d, times the push */
    out[0] = k * ex;
    out[1] = k * ey;
}

/** \brief The sample position of pixel (x, y): the frame's own point plus every source's offset, each sample clamped to the
   frame's middle (0.5 .. W - 0.5), so the edge repeats and no black is pulled in. `wave` holds each source's row term. */
static inline void sh4der_warp_point(const sh4der_warp_frame_t *f, const float wave[SH4DER_WARP_SOURCES], float x, float y, float out[2]) {
    float sx = x, sy = y, d[2];
    unsigned i;
    const unsigned n = f->n < SH4DER_WARP_SOURCES ? f->n : SH4DER_WARP_SOURCES;
    for (i = 0; i < n; i++) {
        const sh4der_warp_src_t *s = &f->src[i];
        if (s->kind == SH4DER_WARP_RING) {
            sh4der_warp_ring_(s, x, y, d);
            sx += d[0];
            sy += d[1];
        } else if (s->kind == SH4DER_WARP_HAZE) {
            sx += wave[i] * sh4der_warp_haze_mask_(s, x, y);
        } else {
            sx += wave[i];
        }
    }
    out[0] = sh4der_warp_clampf_(sx, 0.5f, (float)SH4DER_WARP_FRAME_W - 0.5f);
    out[1] = sh4der_warp_clampf_(sy, 0.5f, (float)SH4DER_WARP_FRAME_H - 0.5f);
}

/** \brief Grid row j (0 .. ny) of an nx x ny grid, nx + 1 vertices: positions on the grid (integer division: exact for the shipped
   grids, 320 / 20 = 16, 240 / 15 = 16, 320 / 40 = 8, 240 / 30 = 8) and the warped UVs. The geometry does not move: only the
   sample does. nx <= SH4DER_WARP_FINE_X. */
static inline void sh4der_warp_row(const sh4der_warp_frame_t *f, unsigned nx, unsigned ny, unsigned j, sh4der_warp_vtx_t out[]) {
    float wave[SH4DER_WARP_SOURCES] = { 0.0f, 0.0f, 0.0f, 0.0f };
    const float y = ny ? (float)((j * SH4DER_WARP_FRAME_H) / ny) : 0.0f;
    const unsigned n = f->n < SH4DER_WARP_SOURCES ? f->n : SH4DER_WARP_SOURCES;
    float p[2];
    unsigned i, k;
    for (k = 0; k < n; k++)
        wave[k] = sh4der_warp_wave_(&f->src[k], y);
    if (!nx || !ny)   /* a grid of no cells: nothing (no division by zero) */
        return;
    for (i = 0; i <= nx; i++) {
        const float x = (float)((i * SH4DER_WARP_FRAME_W) / nx);
        sh4der_warp_point(f, wave, x, y, p);
        out[i].x = x;
        out[i].y = y;
        out[i].u = p[0] * (1.0f / SH4DER_WARP_TEX_W);
        out[i].v = p[1] * (1.0f / SH4DER_WARP_TEX_H);
    }
}

/** \brief Copy i of k (0 .. k-1) zoomed about the centre: 1 for the sharp copy, up to 1 + ZOOM x strength for the outermost.
    Never below 1, so every copy covers the screen. */
static inline float sh4der_warp_blur_scale(unsigned i, unsigned k, float strength) {
    if (k < 2u)
        return 1.0f;
    return 1.0f + SH4DER_WARP_BLUR_ZOOM * sh4der_sat_(strength) * (float)i * sh4der_div_(1.0f, (float)(k - 1u));
}

/** \brief Copy i's blend weight out of 255 (k copies): the integer share, the shares summing to exactly 255 over i < k. */
static inline unsigned sh4der_warp_blur_weight(unsigned i, unsigned k) {
    if (k == 0u || i >= k)
        return 0u;
    return (255u * (i + 1u)) / k - (255u * i) / k;
}

/** \brief The pixelate block cell (bx, by): the UV of its centre texel, all four corners of the block take it (point sampled, flat). */
static inline void sh4der_warp_pixel_uv(unsigned bx, unsigned by, float out[2]) {
    const unsigned c = SH4DER_WARP_PIXEL_CELL;
    const unsigned px = bx * c + c / 2u, py = by * c + c / 2u;
    out[0] = ((float)px + 0.5f) * (1.0f / SH4DER_WARP_TEX_W);
    out[1] = ((float)py + 0.5f) * (1.0f / SH4DER_WARP_TEX_H);
}

/* 4 x 4 ordered (Bayer) threshold, 0..15, row-major. */
static const uint8_t sh4der_warp_bayer_[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

/** \brief The dither multiply at screen pixel (x, y): 1 - DITHER x (threshold share), 1 - DITHER .. 1. */
static inline float sh4der_warp_dither_mul(unsigned x, unsigned y) {
    const float t = ((float)sh4der_warp_bayer_[(y & 3u) * 4u + (x & 3u)] + 0.5f) * (1.0f / 16.0f);
    return 1.0f - SH4DER_WARP_DITHER * t;
}

/** \brief PVR twiddle index of texel (x, y) in an 8 x 8 texture: y bit b at 2b, x bit b at 2b + 1. Matches the feed mask's
    pre-twiddled data (fb_tex.c: alpha 1 on even columns). */
static inline unsigned sh4der_warp_twiddle8(unsigned x, unsigned y) {
    unsigned r = 0, b;
    for (b = 0; b < 3; b++)
        r |= (((y >> b) & 1u) << (2u * b)) | (((x >> b) & 1u) << (2u * b + 1u));
    return r;
}

/** \brief The 8 x 8 ARGB4444 dither tile, twiddled, into out[64]: the 4 x 4 pattern repeated (alpha 15, grey levels 0..15). */
static inline void sh4der_warp_dither_tile(uint16_t out[64]) {
    unsigned x, y;
    for (y = 0; y < 8; y++)
        for (x = 0; x < 8; x++) {
            const unsigned l = (unsigned)(15.0f * sh4der_warp_dither_mul(x, y) + 0.5f);
            out[sh4der_warp_twiddle8(x, y)] = (uint16_t)(0xf000u | (l << 8) | (l << 4) | l);
        }
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_WARP_H */
