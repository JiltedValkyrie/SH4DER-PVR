/* SH4DER-PVR: the mip depth test's arithmetic (mip_spike.c): the mip chain, the level's code, the floor, the three guesses

   demo/psm/ao/mip_model.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A MODEL of what the test asks the PVR, for the host test and for the "expected" side of the console's pages. It is
   not a measurement; where it and the console disagree the console is right. EVERYTHING here about which level the
   PVR picks is an ESTIMATE: the test exists to find the rule.

   THE TEXTURE. 256 x 256 with all its mip levels, twiddled, 16 bit. Level k (0: 256 x 256 .. 8: 1 x 1) is one flat
   grey, so a pixel's colour says which level the PVR read, and a blend of two levels reads between them. The grey is
   green 7 (k + 1) of 63 (red and blue 3 (k + 1) of 31): the frame is 565, and green's 7 steps a level is the most it
   can hold for 9 levels. A level's code as read back is that green: 7, 14 .. 63; 0 is the target's empty black.

   THE FLOOR. The eye MIP_H above a floor, looking level, focal length MIP_F pixels, in a 320 x 240 target: depth z
   lands on row 120 + MIP_F H / z. A tilt t lifts the floor t metres a metre of depth, about its near edge. One texture
   repeat is MIP_S metres both ways.

   THE GUESSES. Texels a pixel across the screen (rx) and down it (ry) at depth z; the level is floor(log2) of
     MAX   the larger of the two (the usual rule of a software rasteriser),
     MIN   the smaller,
     AREA  their geometric mean (a rule from the texture's area against the screen's). */

#ifndef MIP_MODEL_H
#define MIP_MODEL_H

#include <stdint.h>

#include "sh4der_core.h"                    /* the library's maths switch: SH4ZAM on the console, plain C on a host */

#define MIP_SIZE 256u
#define MIP_LEVELS 9u
#define MIP_BYTES 0x2aab0u                  /* the chain's bytes: the last level's offset + 256 x 256 x 2 */
#define MIP_STEP 7u                         /* green a level */

#define MIP_F 160.0f                        /* pixels */
#define MIP_H 0.75f                         /* metres: depth 1 m is the target's bottom row */
#define MIP_S 1.6f                          /* metres a texture repeat: then rx = z exactly */
#define MIP_W 8.0f                          /* the floor's half width, metres */
#define MIP_Z0 1.0f
#define MIP_Z1 60.0f
#define MIP_CX 160.0f
#define MIP_CY 120.0f

/* The byte offset of level k in a 16-bit twiddled mip chain (the PVR's layout: 1 x 1 first, at byte 6). */
static inline uint32_t mip_offset(unsigned k) {
    static const uint32_t at[MIP_LEVELS] = { 0xaab0u, 0x2ab0u, 0xab0u, 0x2b0u, 0xb0u, 0x30u, 0x10u, 0x8u, 0x6u };

    return at[k < MIP_LEVELS ? k : MIP_LEVELS - 1u];
}

static inline uint32_t mip_side(unsigned k) { return MIP_SIZE >> (k < MIP_LEVELS ? k : MIP_LEVELS - 1u); }

/* Level k's texel, RGB565. */
static inline uint16_t mip_texel(unsigned k) { return (uint16_t)(((3u * (k + 1u)) << 11) | ((MIP_STEP * (k + 1u)) << 5) | (3u * (k + 1u))); }

/* A frame pixel's code: its green, 0 .. 63. A whole level k reads MIP_STEP (k + 1); nothing drawn reads 0. */
static inline unsigned mip_code(uint16_t px) { return (px >> 5) & 63u; }

/* The chain into `out` (MIP_BYTES / 2 words, rounded up by the caller as it likes). */
static inline void mip_fill(uint16_t *out) {
    unsigned k;
    uint32_t i;

    out[0] = out[1] = out[2] = mip_texel(MIP_LEVELS - 1u);       /* the bytes before the 1 x 1 level */
    for (k = 0; k < MIP_LEVELS; k++) {
        const uint32_t n = mip_side(k) * mip_side(k), at = mip_offset(k) / 2u;

        for (i = 0; i < n; i++) out[at + i] = mip_texel(k);
    }
}

/* The floor: `hh` is MIP_H + tilt. A depth's row, and a row centre's depth. */
static inline float mip_floor_y(float z, float tilt) { return MIP_CY + MIP_F * (MIP_H + tilt) * sh4der_recip_pos_(z) - MIP_F * tilt; }
static inline float mip_floor_z(float y, float tilt) { return sh4der_div_(MIP_F * (MIP_H + tilt), y - MIP_CY + MIP_F * tilt); }

/* Texels a pixel at depth z: across (the same for a wall facing the eye, both ways), and down the floor. */
static inline float mip_rx(float z) { return (float)MIP_SIZE * z * (1.0f / (MIP_F * MIP_S)); }
static inline float mip_ry(float z, float tilt) {
    return (float)MIP_SIZE * sh4der_sqrt_(1.0f + tilt * tilt) * z * z * sh4der_recip_pos_(MIP_F * (MIP_H + tilt) * MIP_S);
}

/* floor(log2 r), held to 0 .. 8: the level a ratio of r texels a pixel asks for. */
static inline unsigned mip_ilog2(float r) {
    unsigned k = 0;

    while (k < MIP_LEVELS - 1u && r >= 2.0f) { r *= 0.5f; k++; }
    return k;
}

enum { MIP_MAX = 0, MIP_MIN, MIP_AREA, MIP_RULES };

/* The level a guess gives the floor at depth z. */
static inline unsigned mip_guess(unsigned rule, float z, float tilt) {
    const float rx = mip_rx(z), ry = mip_ry(z, tilt);

    if (rule == MIP_MAX) return mip_ilog2(rx > ry ? rx : ry);
    if (rule == MIP_MIN) return mip_ilog2(rx < ry ? rx : ry);
    return mip_ilog2(sh4der_sqrt_(rx * ry));
}

#endif /* MIP_MODEL_H */
