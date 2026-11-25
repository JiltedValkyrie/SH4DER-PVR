/* SH4DER-PVR: the depth fade spike's arithmetic (d1_spike.c): depth codes, the bump unit's level, the expected rows

   demo/psm/ao/d1_model.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A MODEL of what the spike asks the PVR to do, for the host test and for the "expected" side of the console's
   pages. It is not a measurement, and where it and the console disagree the console is right.

   THE DEPTH CODE. A camera depth map holds, a pixel, an 8-bit code of the scene's depth along the view:
       code = floor(256 x frac(depth / D1_TURN))
   It is cyclic: it comes round every D1_TURN metres. The map pass gets it from a 256-texel ramp that repeats: with
   the PVR's depth z = 1 / depth and the texture coordinate u = depth / D1_TURN at every vertex, the PVR's
   perspective-correct interpolation gives each pixel u of its OWN depth (it interpolates u z, a constant times 1
   here, and z, and divides), so the lookup is linear in depth and the repeat makes the turn.

   THE LEVEL. The bump unit read as a compare (wiki PVR-045; c4_model.h's table): level = K1 + K3 cos(2 pi d / 256),
   d = (M - Q) & 255, M the map's code and Q one value a polygon. With K1 0, K3 255 and Q = the halo's own code + 64,
   that is 255 sin(2 pi (M - halo) / 256): 0 where the scene is at the halo's depth, full a quarter turn (D1_FADE)
   behind it. Negative levels are taken as 0 here; what the PVR does with them is the console's to say (test T1).
   DESIGN CLAIM, ESTIMATE until T1 and T3 have run: past a quarter turn the level falls again and comes round every
   turn, so the level alone is not a fade; a second quad D1_FADE behind the halo, through the depth test, makes
   everything further than that full. */

#ifndef D1_MODEL_H
#define D1_MODEL_H

#include <stdint.h>

#include "sh4der_core.h"                    /* the library's maths switch: SH4ZAM on the console, plain C on a host */

#define D1_FADE 0.3f                        /* metres behind the halo over which it fades in: a quarter turn */
#define D1_TURN (4.0f * D1_FADE)            /* metres a turn of 256 codes: a code is 4.7 mm */
#define D1_Q_SHIFT 64u                      /* a quarter turn: the cosine read as a sine */

static inline float d1_u(float depth) { return depth * (1.0f / D1_TURN); }

static inline unsigned d1_code(float depth) {
    const float t = d1_u(depth);            /* depth >= 0: the cast is the floor */

    return (unsigned)((t - (float)(unsigned)t) * 256.0f) & 255u;
}

/* The Q a halo at `depth` gives the bump unit. */
static inline unsigned d1_q(float depth) { return (d1_code(depth) + D1_Q_SHIFT) & 255u; }

/* The level for difference d = (M - Q) & 255, held to 0 .. 255. */
static inline unsigned d1_level(unsigned k1, unsigned k3, unsigned d) {
    float sn, cs, v;

    sh4der_sincos_((float)(d & 255u) * (6.28318530718f / 256.0f), &sn, &cs);
    v = (float)k1 + (float)k3 * cs;

    return v <= 0.0f ? 0u : v >= 255.0f ? 255u : (unsigned)(v + 0.5f);
}

/* The alpha the fade gives a pixel: the scene `behind` metres behind the halo (negative: in front of it).
   with_far: the second quad is drawn. */
static inline unsigned d1_fade(float halo_depth, float behind, int with_far) {
    if (behind < 0.0f) return 0u;           /* the depth test: nothing is written in front of the halo */
    if (with_far && behind > D1_FADE) return 255u;
    return d1_level(0u, 255u, (d1_code(halo_depth + behind) - d1_q(halo_depth)) & 255u);
}

/* The depth at pixel centre x of a quad whose left edge (x0) is at depth d0 and right edge (x1) at d1: the PVR
   interpolates 1 / depth across the screen. */
static inline float d1_tilt_depth(float x0, float x1, float d0, float d1, float x) {
    const float t = sh4der_div_(x - x0, x1 - x0), i0 = sh4der_recip_pos_(d0), iw = i0 + (sh4der_recip_pos_(d1) - i0) * t;

    return sh4der_recip_pos_(iw);
}

/* The difference of two codes as -128 .. 127. */
static inline int d1_diff(unsigned a, unsigned b) { return (int)(((a - b + 128u) & 255u)) - 128; }

#endif /* D1_MODEL_H */
