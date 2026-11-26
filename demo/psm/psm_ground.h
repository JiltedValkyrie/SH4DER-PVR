/* SH4DER-PVR: the square's ground texture, made at load: paving slabs, no chequer

   demo/psm/psm_ground.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   A 64 x 64 RGB565 texture (8 KB, non-twiddled) of stone slabs laid in rows: each row its own height, each slab its own
   width and tone, the rows offset so no joint runs on, a dark joint and a light / dark edge a texel wide on each slab,
   and a small grain over all. It tiles both ways (the rows sum to 64, a row's slabs wrap round). Every value comes from
   an integer hash of (row, slab) or (x, y), so the same seed gives the same texels on every machine. No float maths in
   a hot loop: 4,096 texels once at load. The town's two-stone chequer is gone (ground_tex 1).
*/

#ifndef PSM_GROUND_H
#define PSM_GROUND_H

#include <stdint.h>

#define PSM_GROUND_N 64u                     /* texels a side */

static inline uint32_t psm_ground_hash_(uint32_t a, uint32_t b) {
    uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca77u;
    h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12; h *= 0x297a2d39u; h ^= h >> 15;
    return h;
}

static inline uint16_t psm_ground_565_(int r, int g, int b) {   /* 0 .. 255 each */
    r = r < 0 ? 0 : r > 255 ? 255 : r; g = g < 0 ? 0 : g > 255 ? 255 : g; b = b < 0 ? 0 : b > 255 ? 255 : b;
    return (uint16_t)(((unsigned)(r >> 3) << 11) | ((unsigned)(g >> 2) << 5) | (unsigned)(b >> 3));
}

/* The slabs into px (PSM_GROUND_N x PSM_GROUND_N, row by row). base: the stone's colour, 0 .. 255 a channel. */
static inline void psm_ground_slabs(uint16_t *px, int base_r, int base_g, int base_b, uint32_t seed) {
    static const uint8_t row_h[6] = { 12u, 10u, 13u, 11u, 8u, 10u };   /* sums to 64 */
    unsigned row, y0 = 0u;

    for (row = 0u; row < 6u; y0 += row_h[row], row++) {
        const unsigned h = row_h[row], off = psm_ground_hash_(seed, row) % PSM_GROUND_N;
        uint8_t edge[PSM_GROUND_N], slab_of[PSM_GROUND_N];
        unsigned x = 0u, s = 0u, y;

        while (x < PSM_GROUND_N) {                   /* the slabs' widths, 10 .. 20, the last one fills the row */
            unsigned w = 10u + psm_ground_hash_(seed + 17u, row * 64u + s) % 11u, k;

            if (PSM_GROUND_N - x < w + 10u) w = PSM_GROUND_N - x;
            for (k = 0u; k < w; k++) {
                slab_of[(x + k + off) % PSM_GROUND_N] = (uint8_t)s;
                edge[(x + k + off) % PSM_GROUND_N] = (uint8_t)(k == 0u ? 1u : k == w - 1u ? 2u : 0u);
            }
            x += w; s++;
        }
        for (y = 0u; y < h; y++)
            for (x = 0u; x < PSM_GROUND_N; x++) {
                const uint32_t hs = psm_ground_hash_(seed + 101u, row * 64u + slab_of[x]), hg = psm_ground_hash_(seed + 7u, (y0 + y) * 64u + x);
                int tone = 230 + (int)(hs % 41u);                 /* the slab: 0.90 .. 1.06 of the base, in 1/256 */
                const int warm = (int)((hs >> 8) % 9u) - 4;        /* and a little warmer or cooler */
                int r, g, b;

                tone += (int)(hg % 13u) - 6;                       /* the grain: +-2 % */
                if (y == 0u || edge[x] == 1u) tone = tone * 268 / 256;   /* the slab's top and left edge catch the light */
                if (edge[x] == 2u) tone = tone * 220 / 256;          /* its right edge does not */
                if (y + 1u == h) tone = tone * 180 / 256;            /* the joint under the row */
                if (y + 1u == h && edge[x] == 2u) tone = tone * 160 / 256; /* the joint's corner */
                if (edge[x] == 2u && (hg & 3u) == 0u) tone = tone * 150 / 256;   /* a broken joint, a texel here and there */
                r = base_r * tone / 256 + warm; g = base_g * tone / 256; b = base_b * tone / 256 - warm;
                px[(y0 + y) * PSM_GROUND_N + x] = psm_ground_565_(r, g, b);
            }
    }
}

#endif /* PSM_GROUND_H */
