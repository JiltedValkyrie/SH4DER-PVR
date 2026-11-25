/* SH4DER-PVR: pass recipes, host reference models

   demo/psm/recipes/recipes.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../../../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).

   The SH-4 side of four recipes of demo/psm/RECIPES_NOTES.md, as plain functions a host test can check before any
   console run. Not part of the library (nothing in include/ or src/ uses it); for review.

   R1 cel ramp: the texture coordinate of a vertex (its N.L), and the ramp that point sampling reads with it.
   R3 dither and dissolve by punch-through: the candidate rules for "texture alpha x vertex alpha >= reference"
      (PVR-064: the product counts; how it rounds is open) and the vertex alpha that shows a given number of cells.
   R4 depth pulse and contours by the fog table: the table's index of a scaled 1/w (16 entries an octave, linear
      inside one), the table packed as the PVR takes it, the level a pixel gets.
   R5 soft particles against a known plane: a sprite vertex's alpha by its height over the plane.

   Maths through SH4ZAM (sh4der_core.h's helpers, and the few calls below); SH4DER_CORE_NO_SH4ZAM gives plain C.
   Every rule here that stands for the hardware is marked with where it was read. No allocation, no globals.
*/

#ifndef SH4DER_RECIPES_H
#define SH4DER_RECIPES_H

#include <stdint.h>
#include "sh4der_core.h"

#ifdef SH4DER_CORE_NO_SH4ZAM
#include <math.h>
#endif

/* ---- the few scalar calls sh4der_core.h has no helper for ------------------------------- */

static inline float rcp_floor_(float x) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_floorf(x);
#else
    return floorf(x);
#endif
}

static inline float rcp_fmax_(float a, float b) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_fmaxf(a, b);
#else
    return a > b ? a : b;
#endif
}

/* 0 at or under e0, 1 at or over e1, 3t^2 - 2t^3 between (e0 < e1). */
static inline float rcp_smooth_(float x, float e0, float e1) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_smoothstepf(x, e0, e1);
#else
    float t;
    if (x <= e0) return 0.0f;
    if (x >= e1) return 1.0f;
    t = (x - e0) / (e1 - e0);
    return t * t * (3.0f - 2.0f * t);
#endif
}

/* The piecewise linear log2: the exponent plus the mantissa's fraction, (e + m - 1) for x = m 2^e, 1 <= m < 2.
   x > 0. SH4ZAM's shz_log2f is this same reading of the float's bits, less 126.94269504 where this takes 127
   (it centres its error against the true log2); so this is shz_log2f plus that difference. */
static inline float rcp_log2lin_(float x) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_log2f(x) - 0.05730496f;
#else
    int e;
    const float m = frexpf(x, &e);         /* x = m 2^e, 0.5 <= m < 1 */
    return (float)(e - 1) + (2.0f * m - 1.0f);
#endif
}

/* The inverse of rcp_log2lin_: (1 + f) 2^e for y = e + f, 0 <= f < 1; y >= 0 (the fog table's range). Not
   shz_ldexpf: it takes a float exponent through shz_pow2f, a fast approximation, and was 2.9 % off here for whole
   exponents (host, SH4ZAM's software back end); shz_powif squares, exact for a whole power of two. */
static inline float rcp_exp2lin_(float y) {
    const float e = rcp_floor_(y);
#ifndef SH4DER_CORE_NO_SH4ZAM
    return (1.0f + (y - e)) * shz_powif(2.0f, (int)e);
#else
    return ldexpf(1.0f + (y - e), (int)e);
#endif
}

/* ---- R1: cel ramp ------------------------------------------------------------------------ */

/* A vertex's texture coordinate u for the ramp: its N.L, wrapped by w (0 = none; 0.5 lets light reach half way
   round), held in 0..1. inv_1w = 1 / (1 + w), once an object. n and l unit, the same space. */
static inline float rcp_cel_u(const float n[3], const float l[3], float w, float inv_1w) {
    return sh4der_clamp_((sh4der_dot3_(n, l) + w) * inv_1w, 0.0f, 1.0f);
}

/* The texel a point sample reads at u from a ramp n texels wide (PVR_UVCLAMP_U holds u in 0..1). delta is the
   texel-centre convention under test: 0 reads texel floor(u n), the texel whose span holds u (texels centred at
   (k + 0.5) / n); 0.5 reads floor(u n + 0.5), as if the centres sat on k / n. Which one the PVR uses for a
   texture coordinate is not measured; s2 / s3 put u at texel middles, where both agree (PVR-023). */
static inline unsigned rcp_cel_texel(float u, unsigned n, float delta) {
    const float k = rcp_floor_(u * (float)n + delta);
    return k < 0.0f ? 0u : k >= (float)n ? n - 1u : (unsigned)k;
}

/* Build a ramp of n texels as band numbers: band b covers N.L from edge[b - 1] to edge[b] (edge rising, nb - 1 of
   them; band 0 under edge[0]). Each texel takes the band of the u at its own centre under the same delta. The edge
   a point sample then shows lies within half a texel (0.5 / n in u) of the asked one. */
static inline void rcp_cel_ramp(uint8_t *ramp, unsigned n, const float *edge, unsigned nb, float delta) {
    unsigned k, b;
    for (k = 0; k < n; k++) {
        const float uc = ((float)k + 0.5f - delta) * (1.0f / (float)n);
        for (b = 0; b + 1u < nb && uc >= edge[b]; b++) {}
        ramp[k] = (uint8_t)b;
    }
}

/* ---- R3: dither and dissolve by punch-through ------------------------------------------- */

/* How a 4-bit texel alpha becomes 8 bits. Not measured: c4 test 6 fits REPLICATE and rules SHIFT out (see the
   test). */
enum { RCP_EXP_REPLICATE, RCP_EXP_SHIFT, RCP_EXP_N };

/* How "texture alpha x vertex alpha" is rounded before the test against the reference. PVR-064 leaves it open.
   PLAIN:   (t a) >> 8
   VTX256:  (t (a + (a >> 7))) >> 8, the vertex's 255 read as 256 (skmp's blend factor rule, PVR-041, applied here)
   BOTH256: ((t + (t >> 7)) (a + (a >> 7))) >> 8, both 255s read as 256
   ROUND:   (t a + 127) / 255, the nearest
   FLOOR:   (t a) / 255, cut */
enum { RCP_PT_PLAIN, RCP_PT_VTX256, RCP_PT_BOTH256, RCP_PT_ROUND, RCP_PT_FLOOR, RCP_PT_N };

static inline unsigned rcp_pt_expand(unsigned nibble, unsigned exp) {
    return exp == RCP_EXP_REPLICATE ? nibble * 17u : nibble << 4;
}

static inline unsigned rcp_pt_product(unsigned t, unsigned a, unsigned rule) {
    switch (rule) {
    case RCP_PT_PLAIN:   return (t * a) >> 8;
    case RCP_PT_VTX256:  return (t * (a + (a >> 7))) >> 8;
    case RCP_PT_BOTH256: return ((t + (t >> 7)) * (a + (a >> 7))) >> 8;
    case RCP_PT_ROUND:   return (t * a + 127u) / 255u;
    default:             return (t * a) / 255u;
    }
}

/* 1 when a pixel of 4-bit texel alpha `nibble` and vertex alpha a passes the reference (alpha >= ref, PVR-060). */
static inline int rcp_pt_pass(unsigned nibble, unsigned a, unsigned ref, unsigned exp, unsigned rule) {
    return rcp_pt_product(rcp_pt_expand(nibble, exp), a, rule) >= ref;
}

/* The 4 x 4 ordered dither index 0..15 of screen pixel (x, y); a texel holds it as its alpha nibble. Cell 0 is
   never drawn at any vertex alpha, so a dither shows at most 15 of 16 cells; draw the object normally at full. */
static inline unsigned rcp_bayer4(unsigned x, unsigned y) {
    static const uint8_t b[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };
    return b[(y & 3u) * 4u + (x & 3u)];
}

/* How many of the 16 cells pass at vertex alpha a. */
static inline unsigned rcp_dither_cells(unsigned a, unsigned ref, unsigned exp, unsigned rule) {
    unsigned k, n = 0;
    for (k = 0; k < 16u; k++) n += (unsigned)rcp_pt_pass(k, a, ref, exp, rule);
    return n;
}

/* At load: for each count c of cells (0..15) the smallest vertex alpha that shows exactly c, or 256 when none
   does. Built once from the rule the console settles; at run time a fade is one table read, no divide. */
static inline void rcp_dither_table(uint16_t out[16], unsigned ref, unsigned exp, unsigned rule) {
    unsigned c, a;
    for (c = 0; c < 16u; c++) out[c] = 256u;
    for (a = 0; a < 256u; a++) {
        c = rcp_dither_cells(a, ref, exp, rule);
        if (c < 16u && out[c] == 256u) out[c] = (uint16_t)a;
    }
}

/* ---- R4: depth pulse and contours by the fog table --------------------------------------- */

#define RCP_FOG_N 128u                     /* entries; 16 an octave of the scaled 1/w (Sega 3.4.8.1; c3b 6a) */

/* The table position of a pixel whose 1/w is iw, under the fog scale k (s = k iw): 16 rcp_log2lin_(s), the far
   end (small s) first, held in 0..128. Read from c3b 6a (PVR-049) through the model in compare/sub_spike.c. */
static inline float rcp_fog_pos(float k, float iw) {
    const float s = k * iw;
    return s <= 1.0f ? 0.0f : sh4der_clamp_(16.0f * rcp_log2lin_(s), 0.0f, (float)RCP_FOG_N);
}

/* The view depth (w) at table position p: the inverse of rcp_fog_pos. */
static inline float rcp_fog_depth(float k, float p) {
    return k / rcp_exp2lin_(p * (1.0f / 16.0f));
}

/* The 128 words the table registers take from levels t[0..128]: entry i holds t[i] and t[i + 1] for the PVR to
   blend between (compare/sub_spike.c fog_load: (t[i] << 8) | t[i + 1]). Keep t: the registers do not read back. */
static inline void rcp_fog_pack(uint32_t out[RCP_FOG_N], const uint8_t t[RCP_FOG_N + 1u]) {
    unsigned i;
    for (i = 0; i < RCP_FOG_N; i++) out[i] = ((uint32_t)t[i] << 8) | t[i + 1u];
}

/* The level 0..255 a pixel at table position p gets from the packed table: the entry's two bytes blended by p's
   fraction (c3b 6b: one level a pixel across an octave, no step back). */
static inline float rcp_fog_level(const uint32_t w[RCP_FOG_N], float p) {
    const float pf = rcp_floor_(p);
    const unsigned i = pf >= (float)(RCP_FOG_N - 1u) ? RCP_FOG_N - 1u : (unsigned)pf;
    const float f = p - (float)i, a = (float)((w[i] >> 8) & 255u), b = (float)(w[i] & 255u);
    return a + (b - a) * (f > 1.0f ? 1.0f : f);
}

/* A pulse at view depth d: level peak at its table position, falling to 0 half_width entries either side, laid
   over base (scene fog; NULL for none) by the larger of the two. Writes t[0..128]. */
static inline void rcp_fog_pulse(uint8_t t[RCP_FOG_N + 1u], const uint8_t *base, float k, float d, float half_width, float peak) {
    const float ip = 16.0f * rcp_log2lin_(k / d), inv_h = sh4der_div_(1.0f, half_width);
    unsigned i;
    for (i = 0; i <= RCP_FOG_N; i++) {
        const float v = peak * rcp_fmax_(0.0f, 1.0f - sh4der_abs_((float)i - ip) * inv_h);
        const float b = base ? (float)base[i] : 0.0f, m = rcp_fmax_(v, b);
        t[i] = (uint8_t)(m >= 255.0f ? 255u : (unsigned)(m + 0.5f));
    }
}

/* Contours: level `peak` on the first `duty` (0..1) of every `period` entries, 0 elsewhere. Writes t[0..128]. */
static inline void rcp_fog_contours(uint8_t t[RCP_FOG_N + 1u], float period, float duty, float phase, uint8_t peak) {
    const float inv_p = sh4der_div_(1.0f, period);
    unsigned i;
    for (i = 0; i <= RCP_FOG_N; i++) {
        const float q = ((float)i + phase) * inv_p;
        t[i] = (q - rcp_floor_(q)) < duty ? peak : 0u;
    }
}

/* ---- R5: soft particles against a known plane -------------------------------------------- */

/* A sprite vertex's alpha factor at world point p, against the plane dot(n, x) = d (n unit, toward the open side):
   0 at or under the plane, 1 at r over it, smooth between. Multiply the factors of every plane near the sprite. */
static inline float rcp_soft_alpha(const float p[3], const float n[3], float d, float r) {
    return rcp_smooth_(sh4der_dot3_(p, n) - d, 0.0f, r);
}

#endif /* SH4DER_RECIPES_H */
