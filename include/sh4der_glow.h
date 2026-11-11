/* SH4DER-PVR: the glow effect

   sh4der_glow.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_glow.h
    \brief   Screen bloom and god rays from one small bright map of the frame.

    The bright map is the frame's luma grid (SH4DER_GLOW_GRID_X x _Y samples,
    the same read the look's eye takes): each sample's brightness above
    SH4DER_GLOW_THRESHOLD, raised to SH4DER_GLOW_POWER, scales the sample's own
    colour ("darks die, brights stay"). It is packed to RGB565 in a
    power-of-two texture (the PVR takes 2^n sides only: 80 x 60 sits in 128 x 64).

    - Bloom: the map is drawn over the screen ADD (ONE / ONE) bilinear, with
      a 20 x 15 shrink of it for the wide halo. The split is
      sh4der_glow_halo: strength x (1 - radius) on the map, strength x radius
      on the shrink.
    - Darken: a multiply under the bloom, 1 - D, with D = S x map / frame
      (sh4der_glow_darken). A neutral scene keeps its mean: (1 - D) frame +
      S map = frame, unclamped. Hot texels lift the dark around them, the
      rest of the frame settles darker.
    - God rays: copies of the map, each scaled about the sun's screen point
      (sh4der_glow_ray_scale), added with weights that decay copy to copy
      (sh4der_glow_ray_weight). Each corner's colour fades with its distance
      from the sun (sh4der_glow_ray_colour, Gouraud across the quad) and with
      the sun's on-screen share and off-screen edge (sh4der_glow_sun_fade).

    The CPU path (sh4der_glow_upload) builds the map from the grid and loads
    it with pvr_txr_load. The GPU path (render the frame small, multiply it
    with itself) is costed in PLAN_GLOW.md; the maths here is the same.

    Every step here is integer or a short series; the luma weights are the
    look's (Rec. 601, 77 / 150 / 29). The smooth step on 0..1 is SH4DOW-DC's
    sh4dow_smooth01 (SH4ZAM's saturate and fused multiply-add; its
    shz_smoothstepf takes two edges and tests both, which fixed edges 0 and 1
    do not need). The rest goes through SH4ZAM's saturate / divide / length
    (sh4der_core.h).

    \author Jilted Valkyrie
*/

#ifndef SH4DER_GLOW_H
#define SH4DER_GLOW_H

#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Shipped tuning
    @{ */
#define SH4DER_GLOW_GRID_X      80      /**< \brief Bright-map cells across the frame. */
#define SH4DER_GLOW_GRID_Y      60      /**< \brief Bright-map cells down the frame. */
#define SH4DER_GLOW_TEX_W       128     /**< \brief Power-of-two texture side holding the map. */
#define SH4DER_GLOW_TEX_H       64      /**< \brief Power-of-two texture side holding the map. */
#define SH4DER_GLOW_SMALL_X     20      /**< \brief Shrink of the map across (cells of 4 x 4). */
#define SH4DER_GLOW_SMALL_Y     15      /**< \brief Shrink of the map down. */
#define SH4DER_GLOW_SMALL_TEX_W 32      /**< \brief Texture side of the shrink. */
#define SH4DER_GLOW_SMALL_TEX_H 16      /**< \brief Texture side of the shrink. */
#define SH4DER_GLOW_THRESHOLD   0.62f   /**< \brief Luma (0..1) where the glow starts. */
#define SH4DER_GLOW_POWER       2       /**< \brief k in ((luma - threshold) / (1 - threshold))^k. Whole, 1..3. */
#define SH4DER_GLOW_STRENGTH    0.45f   /**< \brief Glow at full bloom (one draw: 0..1). */
#define SH4DER_GLOW_RADIUS      0.5f    /**< \brief Share of the strength on the wide 20 x 15 halo. */
#define SH4DER_GLOW_DARKEN_MAX  0.25f   /**< \brief Most the darken layer takes off (D cap). */
#define SH4DER_GLOW_RAYS        4       /**< \brief Ray copies (3..6). */
#define SH4DER_GLOW_RAY_STEP    0.12f   /**< \brief Scale step per copy about the sun (copy i at 1 - i x step). */
#define SH4DER_GLOW_RAY_WEIGHT  0.35f   /**< \brief First copy's weight. */
#define SH4DER_GLOW_RAY_DECAY   0.6f    /**< \brief Weight ratio from one copy to the next. */
#define SH4DER_GLOW_ASPECT      1.3333333f /**< \brief Screen width over height: distances are in screen heights. */
#define SH4DER_GLOW_RAY_REACH   0.8f    /**< \brief Distance from the sun (screen heights) where a ray is gone. */
#define SH4DER_GLOW_SUN_EDGE    0.25f   /**< \brief Sun this far off screen (screen heights): no rays. */
#define SH4DER_GLOW_MESH_X      8       /**< \brief Cells across a ray or shaft copy (the fade is per vertex). */
#define SH4DER_GLOW_MESH_Y      6       /**< \brief Cells down a ray or shaft copy. */
/** @} */

/** \brief The RGB565 texel's 8-bit luma (Rec. 601 77 / 150 / 29, 0..255). */
static inline unsigned sh4der_glow_luma8_(uint16_t p) {
    const unsigned r = (p >> 11) & 31u, g = (p >> 5) & 63u, b = p & 31u;
    const unsigned r8 = (r << 3) | (r >> 2), g8 = (g << 2) | (g >> 4), b8 = (b << 3) | (b >> 2);
    return (77u * r8 + 150u * g8 + 29u * b8) >> 8;   /* 255 at white */
}

/** \brief Threshold and power: brightness 0..1 (luma 0..1) to the map's weight 0..1. */
static inline float sh4der_glow_bright_(float y) {
    const float t = sh4der_sat_(sh4der_div_(y - SH4DER_GLOW_THRESHOLD, 1.0f - SH4DER_GLOW_THRESHOLD));
    float w = t;
    int k;
    for (k = 1; k < SH4DER_GLOW_POWER; k++) w *= t;
    return w;
}

/** \brief The weight table: weight (0..255) for an 8-bit luma. Made once at load. */
static inline void sh4der_glow_lut(uint8_t lut[256]) {
    int i;
    for (i = 0; i < 256; i++)
        lut[i] = (uint8_t)(255.0f * sh4der_glow_bright_((float)i * (1.0f / 255.0f)) + 0.5f);
}

/** \brief The weight table for a client's own threshold (0..1) and ramp (squared: 1, straight: 0). sh4der_glow_lut is this at SH4DER_GLOW_THRESHOLD, squared. */
static inline void sh4der_glow_curve(uint8_t lut[256], float threshold, int squared) {
    const float span = threshold < 0.999f ? 1.0f - threshold : 0.001f;   /* a threshold of 1 does not divide by zero */
    int i;
    for (i = 0; i < 256; i++) {
        const float t = sh4der_sat_(sh4der_div_((float)i * (1.0f / 255.0f) - threshold, span));
        lut[i] = (uint8_t)(255.0f * (squared ? t * t : t) + 0.5f);
    }
}

/** \brief Take the glow a read frame carried off the read: grid (w x h RGB565) minus `big` x the map (map_stride texels a row) minus `small_w` x the shrink
    (one texel a 4 x 4 block, small_stride a row), per channel, never under 0. big, small_w: the weights that frame's glow was drawn with (sh4der_glow_halo of
    its strength, all passes together). Nearest texel, where the PVR drew bilinear: an estimate, good enough to stop a bright spot feeding itself.
    The map holds h rows of map_stride texels, the shrink (h + 3) / 4 rows of small_stride: with more than one row a stride under the row's width does nothing. */
static inline void sh4der_glow_unfeed(uint16_t *grid, unsigned w, unsigned h, const uint16_t *map, unsigned map_stride, const uint16_t *small, unsigned small_stride, float big, float small_w) {
    const unsigned wb = (unsigned)(sh4der_sat_(big) * 256.0f), ws = (unsigned)(sh4der_sat_(small_w) * 256.0f);   /* a weight is 0..1: never a negative into unsigned */
    unsigned x, y;
    if (!grid || !map || !small || (h > 1u && w > map_stride) || (h > 4u && ((w + 3u) >> 2) > small_stride))
        return;
    for (y = 0; y < h; y++)
        for (x = 0; x < w; x++) {
            const unsigned p = grid[y * w + x], m = map[y * map_stride + x], s = small[(y >> 2) * small_stride + (x >> 2)];
            int r, g, b;
            if (!(m | s))
                continue;
            r = (int)(p >> 11) - (int)((((m >> 11) & 31u) * wb + ((s >> 11) & 31u) * ws) >> 8);
            g = (int)((p >> 5) & 63u) - (int)((((m >> 5) & 63u) * wb + ((s >> 5) & 63u) * ws) >> 8);
            b = (int)(p & 31u) - (int)(((m & 31u) * wb + (s & 31u) * ws) >> 8);
            grid[y * w + x] = (uint16_t)((r > 0 ? r : 0) << 11 | (g > 0 ? g : 0) << 5 | (b > 0 ? b : 0));
        }
}

/** \brief RGB565 from 8-bit channels (truncated to 5 / 6 / 5). */
static inline uint16_t sh4der_glow_pack565(unsigned r8, unsigned g8, unsigned b8) {
    return (uint16_t)(((r8 >> 3) << 11) | ((g8 >> 2) << 5) | (b8 >> 3));
}

/** \brief A map texel: the source pixel times its weight, RGB565. The weight is lut[luma8], 0..255; (c w + 127) / 255 rounds. */
static inline uint16_t sh4der_glow_texel(uint16_t p, const uint8_t lut[256]) {
    const unsigned r = (p >> 11) & 31u, g = (p >> 5) & 63u, b = p & 31u;
    const unsigned r8 = (r << 3) | (r >> 2), g8 = (g << 2) | (g >> 4), b8 = (b << 3) | (b >> 2);
    const unsigned w = lut[sh4der_glow_luma8_(p)];
    return sh4der_glow_pack565((r8 * w + 127u) / 255u, (g8 * w + 127u) / 255u, (b8 * w + 127u) / 255u);
}

/** \brief The shrink: each dst texel is the mean of a 4 x 4 block of src (RGB565, src_stride texels a row). Integer, rounds. */
static inline void sh4der_glow_shrink4(const uint16_t *src, unsigned src_stride, uint16_t *dst, unsigned dst_stride) {
    unsigned x, y, i, j;
    for (y = 0; y < SH4DER_GLOW_SMALL_Y; y++)
        for (x = 0; x < SH4DER_GLOW_SMALL_X; x++) {
            unsigned sr = 0, sg = 0, sb = 0;
            for (j = 0; j < 4u; j++)
                for (i = 0; i < 4u; i++) {
                    const uint16_t p = src[(4u * y + j) * src_stride + 4u * x + i];
                    sr += (p >> 11) & 31u; sg += (p >> 5) & 63u; sb += p & 31u;
                }
            dst[y * dst_stride + x] = (uint16_t)((((sr + 8u) >> 4) << 11) | (((sg + 8u) >> 4) << 5) | ((sb + 8u) >> 4));
        }
}

/** \brief Mean luma (0..1) of n RGB565 texels: the map's Mg in the darken balance. */
static inline float sh4der_glow_mean_luma(const uint16_t *px, unsigned n) {
    unsigned long sum = 0;
    unsigned i;
    if (!n)
        return 0.0f;
    for (i = 0; i < n; i++) sum += sh4der_glow_luma8_(px[i]);
    return sh4der_div_((float)sum, 255.0f * (float)n);
}

/** \brief The bloom split: `big` on the map, `small` on the shrink (big + small = strength when strength <= 1). */
static inline void sh4der_glow_halo(float strength, float radius, float *big, float *small) {
    const float s = sh4der_sat_(strength), r = sh4der_sat_(radius);
    *big = s * (1.0f - r);
    *small = s * r;
}

/** \brief Darken D (0..DARKEN_MAX) so the glow's mean goes back into the frame: D = S x map / frame, with S the
    strength, `map_mean` the map's mean luma and `frame_mean` the frame's (both 0..1). A black frame: 0. */
static inline float sh4der_glow_darken(float frame_mean, float map_mean, float strength) {
    float d;
    if (!(frame_mean > 0.001f))
        return 0.0f;
    d = sh4der_div_(strength * map_mean, frame_mean);
    return d > SH4DER_GLOW_DARKEN_MAX ? SH4DER_GLOW_DARKEN_MAX : d < 0.0f ? 0.0f : d;
}

/** \brief 3t^2 - 2t^3 on 0..1, clamped: SH4DOW-DC's sh4dow_smooth01, the one smooth step both libraries share. */
static inline float sh4der_glow_smooth_(float t) { return sh4dow_smooth01(t); }

/** \brief Copy i (1..SH4DER_GLOW_RAYS) scale about the sun: the copy samples sun + (screen - sun) x scale. */
static inline float sh4der_glow_ray_scale(int i) { return 1.0f - (float)i * SH4DER_GLOW_RAY_STEP; }

/** \brief Copy i's weight: WEIGHT x DECAY^(i - 1). */
static inline float sh4der_glow_ray_weight(int i) {
    float w = SH4DER_GLOW_RAY_WEIGHT;
    int k;
    for (k = 1; k < i; k++) w *= SH4DER_GLOW_RAY_DECAY;
    return w;
}

/** \brief Texture coordinate of a map position m (0..1 across the frame, clamped): the texel centre of the cell, in a
    `tex`-texel side with `cells` cells, so bilinear never reads past the map's last cell. */
static inline float sh4der_glow_tex_(float m, int cells, int tex) {
    return sh4der_div_(sh4der_sat_(m) * (float)(cells - 1) + 0.5f, (float)tex);
}

/** \brief Texture u, v of a screen corner (cu, cv, 0..1) on the map with copy scale s about the sun (su, sv).
    s = 1: the plain map (the halo). */
static inline void sh4der_glow_ray_uv(float s, float su, float sv, float cu, float cv, float *u, float *v) {
    const float mu = su + (cu - su) * s, mv = sv + (cv - sv) * s;
    *u = sh4der_glow_tex_(mu, SH4DER_GLOW_GRID_X, SH4DER_GLOW_TEX_W);
    *v = sh4der_glow_tex_(mv, SH4DER_GLOW_GRID_Y, SH4DER_GLOW_TEX_H);
}

/** \brief Sun off screen: 1 on screen, 0 once it is SH4DER_GLOW_SUN_EDGE (screen heights) past the edge; smooth between. */
static inline float sh4der_glow_sun_fade(float su, float sv) {
    const float ox = su < 0.0f ? -su : su > 1.0f ? su - 1.0f : 0.0f;
    const float oy = sv < 0.0f ? -sv : sv > 1.0f ? sv - 1.0f : 0.0f;
    const float ex = ox * SH4DER_GLOW_ASPECT;
    return 1.0f - sh4der_glow_smooth_(sh4der_div_(sh4der_len2_(ex, oy), SH4DER_GLOW_SUN_EDGE * SH4DER_GLOW_SUN_EDGE));
}

/** \brief A corner's colour weight in one ray copy: weight(i) x distance fade x sun fade x share, clamped 0..1.
    The distance is the corner's from the sun in screen heights, fading to 0 at SH4DER_GLOW_RAY_REACH.
    `sun_fade` from sh4der_glow_sun_fade, `share` 0..1 (the lit share of the sun's probe). */
static inline float sh4der_glow_ray_colour(int i, float su, float sv, float cu, float cv, float sun_fade, float share) {
    const float dx = (cu - su) * SH4DER_GLOW_ASPECT, dy = cv - sv;
    const float t = sh4der_div_(sh4der_len2_(dx, dy), SH4DER_GLOW_RAY_REACH * SH4DER_GLOW_RAY_REACH);
    return sh4der_sat_(sh4der_glow_ray_weight(i) * (1.0f - sh4der_glow_smooth_(t)) * sun_fade * sh4der_sat_(share));
}

/** \brief A 0..1 colour as ARGB8888 with alpha 255 (grey: the texture's own colour is modulated, the add is on all three). */
static inline uint32_t sh4der_glow_argb(float c) {
    const uint32_t v = (uint32_t)(sh4der_sat_(c) * 255.0f + 0.5f);
    return 0xff000000u | (v << 16) | (v << 8) | v;
}

#ifdef _arch_dreamcast
/* ------------------------------------------------------------------------ */
/* The Dreamcast side (src/sh4der_glow.c): the textures, the headers, the quads. */

#include <dc/pvr.h>

typedef struct sh4der_glow {
    pvr_ptr_t big, small;                      /**< \brief The map (128 x 64) and the shrink (32 x 16) in texture RAM. */
    pvr_poly_hdr_t hdr_big, hdr_small, hdr_darken;   /**< \brief Textured ADD, textured ADD (shrink), MULTIPLY colour. */
    uint8_t lut[256];                          /**< \brief The weights (sh4der_glow_lut). */
    uint16_t stage[SH4DER_GLOW_TEX_W * SH4DER_GLOW_TEX_H];           /**< \brief RAM staging, 16 KB. */
    uint16_t stage_small[SH4DER_GLOW_SMALL_TEX_W * SH4DER_GLOW_SMALL_TEX_H];   /**< \brief RAM staging, 1 KB. */
    uint8_t keep;                              /**< \brief Share (of 256) of the map before kept by each upload: 0 = none (the start), 128 = half. The read is of a frame
                                                    that carries the glow and is one or two frames old; an eased map does not pulse with it. */
} sh4der_glow_t;

/** \brief Load time: the two textures and the three headers. Returns 0 when VRAM is short (nothing held). */
int sh4der_glow_init(sh4der_glow_t *g);

/** \brief Free both textures. */
void sh4der_glow_free(sh4der_glow_t *g);

/** \brief Each frame, after the list's wait: grid (GRID_X x GRID_Y RGB565, the frame's luma read) to the map and the shrink.
    Returns the map's mean luma (0..1), the Mg of sh4der_glow_darken. */
float sh4der_glow_upload(sh4der_glow_t *g, const uint16_t grid[SH4DER_GLOW_GRID_X * SH4DER_GLOW_GRID_Y]);

/** \brief The darken layer and the bloom over the rectangle (x, y, w, h), layers at z, z + 0.25, z + 0.5 (autosort). */
void sh4der_glow_draw_halo(const sh4der_glow_t *g, float x, float y, float w, float h, float z, float strength, float radius, float darken);

/** \brief The god rays over the rectangle: `rays` copies (1..6) about the sun at screen (su, sv), 0..1; `share` 0..1 lit. */
void sh4der_glow_draw_rays(const sh4der_glow_t *g, float x, float y, float w, float h, float z, float su, float sv, float share, int rays);

/** \brief A shaft: ONE copy of the map scaled by `scale` (under 1: pushed out from the anchor) about the anchor (au, av), 0..1 of the rectangle, at `weight` 0..1.
    One big step keeps the bright pattern's shape, so its dark gaps read as shadows cast through the light. */
void sh4der_glow_draw_shaft(const sh4der_glow_t *g, float x, float y, float w, float h, float z, float au, float av, float scale, float weight);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_GLOW_H */
