/* SH4DER-PVR: the lens effects

   sh4der_lens.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).

   RECOMMENDED (no console run of the lens as its own test: nothing here is measured; wiki/limits/header-recommended.md)
   Start with: sh4der_lens_init, sh4der_lens_vignette_setup(w, h), then a frame sh4der_lens_vignette_draw and
               sh4der_lens_grain(frame, w, h): the first forms, at SH4DER_LENS_VIG_* and SH4DER_LENS_GRAIN_AMP 0.06.
               Reach for the variations only when a picture needs them: a grain of your own (sh4der_lens_grain_make,
               _load or _use, then sh4der_lens_grain_draw: a rectangle, a depth key, an amplitude, neutral on or
               off, a tile of 16 to 128 a side, mono / chroma / soft, jump / hold / drift / still) and a vignette of
               your own (sh4der_lens_vignette_shape, sh4der_lens_vignette_draw_at, sh4der_lens_vignette_mesh_draw:
               strength, radii, round, tint, mesh size, the centre off the middle, the falloff curve)
   Cost:       NOT MEASURED. Exact counts: video memory 2 B a texel, a tile of 16: 512 B, 32: 2 KB, 64: 8 KB (the first
               form), 128: 32 KB, a texture of your own handed to sh4der_lens_grain_use: 0 more; RAM 64 B a
               sh4der_lens_grain_t; TA a frame: grain 4 vertices and 1 header, 8 and 2 with neutral on; vignette
               (rings + 1) x 2 x (spokes + 1) vertices (594 at 32 x 8, 350 at 24 x 6) and 1 header.
               ESTIMATE: each grain layer over the whole screen is one translucent layer, about 3.1 ms of PVR at
               640 x 480 (PVR-002 is a plain layer's cost, MEASURED on one console; not this layer's); neutral off
               is one layer, not two; a rectangle costs its share of the screen; the tile's size, kind and movement
               change no fill. SH-4: a few vertex writes and one or two hashes a grain draw; the vignette's first
               draw rebuilds its bands every frame, sh4der_lens_vignette_mesh_draw copies a mesh built once
   Status:     ADVISED; host tested (test/lens_test.c: the first forms' bytes are pinned there). NOT RUN on a console
               as its own test. To prove: the same view with each overlay on and off, PVR time and frame time
   Do:         build a caller's vignette mesh once; turn neutral off where the picture can take the darkening (it
               saves a layer); keep the grain's amplitude under 0.5 (past it the boost cannot give the mean back)
   Do not:     draw any of it before sh4der_lens_init or the grain's own make (the draws refuse: an uncompiled
               header hangs a real console); fill a sh4der_lens_vig_t by hand (start from sh4der_lens_vig_default)
   Maturity:   PROTOTYPE v0.1
*/

/** \file    sh4der_lens.h
    \brief   The lens overlays: a vignette, film grain, the damage direction
             and the screen flash. All untextured but the grain tile, no frame read.

    - Vignette: a radial mesh of spokes and rings (SH4DER_LENS_VIG_SPOKES x
      SH4DER_LENS_VIG_RINGS), one vertex colour a grey multiplier. The rings
      stop at the screen edge along each spoke, so the mesh covers the screen
      and no more. sh4der_lens_vig_mesh gives the strips; the colour is
      sh4der_lens_vig_m (smoothstep from INNER to OUTER, STRENGTH deep).
      Blend: multiply (the look's MULTIPLY layer, see README).
    - Grain: a 64 x 64 ARGB4444 noise tile (8 KB VRAM), luma only, one
      4-bit level a texel, drawn as a multiply (1 - a) + a x tile, then a
      flat boost by the tile's own mean, so the mean brightness holds.
      sh4der_lens_grain_uv moves the tile each frame.
    - Damage direction: up to SH4DER_LENS_HIT_MAX wedges at the screen edge
      where the hit came from (camera space: right, forward). A wedge is a
      gradient quad: full alpha on the edge, none SH4DER_LENS_HIT_DEPTH in.
      A hit within SH4DER_LENS_HIT_MERGE_COS of a live one refreshes it.
      No trig: directions are unit vectors, merged by dot product.
    - Flash: one full-screen layer of a colour at a level, rising to a peak
      and decaying (sh4der_lens_flash_hit / _step). Additive or alpha.

    Variations (the first forms above stay as they are, byte for byte):
    - A grain of the caller's own (sh4der_lens_grain_t): where it is drawn (a
      rectangle and a depth key), how strong (an amplitude a draw), with or
      without the boost layer (neutral), the tile (16 to 128 a side, 1 or 2
      pixels a texel, a seed, MONO, CHROMA or SOFT), whose tile it is (made
      here, the caller's texels uploaded here, or a texture the caller already
      has in video memory) and how it moves (JUMP, HOLD, DRIFT, STILL).
    - A vignette of the caller's own: every field of sh4der_lens_vig_t is
      honoured by sh4der_lens_vignette_shape; the centre can sit off the middle
      (cx, cy) and the falloff can be SMOOTH, LINEAR or SQUARE; it can be drawn
      at an offset and a depth key, or from a mesh the caller built once.

    Every time step is first order, dt / (tau + dt), as in sh4der_look.h.
    Default tuning is SH4DER_LENS_* below. Screen units: pixels, y down,
    the screen origin at the top left.

    Version: v0.10
    The effect: adds what a camera lens and film add to a picture: darker
      corners, moving grain, a marker that points at where a hit came from, a
      white flash. None of it reads the frame.
    Developed for: a first-person shooter.
    Use it for: a horror game's dark corners, film grain in a cutscene, damage
      direction markers in a shooter, lightning or a flash grenade.

    \author Jilted Valkyrie
    \author Cortana (Claude, an AI by Anthropic): the effects' maths, the PVR paths, the tests

    PVR terms (TA, TR/OP list, EOL, twiddled, direct render): see the PVR glossary in README.md.
*/

#ifndef SH4DER_LENS_H
#define SH4DER_LENS_H

#include <stddef.h>
#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    The vignette's metric: radius r = 1 at the middle of a screen edge when
    ROUND is 0 (the ellipse follows the screen), a circle of radius 1 at the
    top and bottom middle when ROUND is 1. Corners sit at sqrt(2) when ROUND
    is 0.
    @{ */
#define SH4DER_LENS_VIG_STRENGTH 0.45f  /**< \brief Darkness at full (0..1): the multiplier falls to 1 - this. */
#define SH4DER_LENS_VIG_INNER    0.60f  /**< \brief Metric radius where the falloff starts. */
#define SH4DER_LENS_VIG_OUTER    1.10f  /**< \brief Metric radius at full strength (> INNER). */
#define SH4DER_LENS_VIG_ROUND    0.0f   /**< \brief 0: screen-shaped (follows the aspect), 1: a circle. */
#define SH4DER_LENS_VIG_SPOKES   32     /**< \brief Spokes round the screen (3..SH4DER_LENS_SPOKES_MAX). 16 gives 9.1 / 255 worst Gouraud error, 24 gives 4.0, 32 gives 2.3 (host sweep). */
#define SH4DER_LENS_VIG_RINGS    8      /**< \brief Rings across INNER..OUTER (1..SH4DER_LENS_RINGS_MAX). */
#define SH4DER_LENS_TINT_R       1.0f   /**< \brief Vignette tint: the multiplier per channel at full (1 = neutral). */
#define SH4DER_LENS_TINT_G       1.0f
#define SH4DER_LENS_TINT_B       1.0f
#define SH4DER_LENS_GRAIN_TILE   64     /**< \brief Noise tile, texels a side (power of two, 64 x 64 x 2 B = 8 KB VRAM). */
#define SH4DER_LENS_GRAIN_AMP    0.06f  /**< \brief Grain swing: the frame x (1 - a + a x noise), noise 0..1 (a = 0.06: about +-3 %). */
#define SH4DER_LENS_GRAIN_PX     1      /**< \brief Screen pixels a texel covers (1 fine, 2 coarse). */
#define SH4DER_LENS_GRAIN_SEED   0x9a1du
#define SH4DER_LENS_HIT_MAX      4      /**< \brief Wedges at once. */
#define SH4DER_LENS_HIT_ATTACK_S 0.05f  /**< \brief Seconds to full alpha. */
#define SH4DER_LENS_HIT_HOLD_S   1.0f   /**< \brief Full alpha this long, then fades. */
#define SH4DER_LENS_HIT_LIFE_S   1.5f   /**< \brief Gone at this age. */
#define SH4DER_LENS_HIT_MERGE_COS 0.9397f /**< \brief cos(20 degrees): a hit this close to a live wedge refreshes it. */
#define SH4DER_LENS_HIT_ALPHA    0.8f   /**< \brief Alpha on the edge at full. */
#define SH4DER_LENS_HIT_WIDTH    40.0f  /**< \brief Half width of the wedge on the edge, pixels. */
#define SH4DER_LENS_HIT_DEPTH    96.0f  /**< \brief Depth inward, pixels (alpha 0 there). */
#define SH4DER_LENS_HIT_TAPER    0.45f  /**< \brief The inner edge's half width as a share of WIDTH. */
#define SH4DER_LENS_HIT_R        1.0f   /**< \brief Wedge colour (red). */
#define SH4DER_LENS_HIT_G        0.10f
#define SH4DER_LENS_HIT_B        0.05f
#define SH4DER_LENS_FLASH_PEAK   0.35f  /**< \brief Highest level a hit reaches (0..1). */
#define SH4DER_LENS_FLASH_ATTACK_S 0.03f /**< \brief Level follows the target with this time constant. */
#define SH4DER_LENS_FLASH_DECAY_S  0.25f /**< \brief The target falls with this time constant. */
#define SH4DER_LENS_FLASH_R      0.60f  /**< \brief Flash colour (blue-white). */
#define SH4DER_LENS_FLASH_G      0.80f
#define SH4DER_LENS_FLASH_B      1.00f
#define SH4DER_LENS_Z_VIG        430.0f /**< \brief Autosort z of each overlay (TR list). Under HUD (1000). */
#define SH4DER_LENS_Z_GRAIN      432.0f
#define SH4DER_LENS_Z_HIT        950.0f
#define SH4DER_LENS_Z_FLASH      960.0f
#define SH4DER_LENS_GRAIN_TILE_MIN 16   /**< \brief A caller's grain tile: the least texels a side (512 B of video memory). */
#define SH4DER_LENS_GRAIN_TILE_MAX 128  /**< \brief And the most (32 KB). A power of two between the two. */
#define SH4DER_LENS_GRAIN_HOLD   2      /**< \brief Frames a HOLD grain keeps one offset (2: film at 30 a second on a 60 Hz screen). */
#define SH4DER_LENS_GRAIN_DRIFT  1      /**< \brief Texels a frame a DRIFT grain slides down. */
#define SH4DER_LENS_GRAIN_AMP_MAX 0.5f  /**< \brief The most a grain's amplitude may be: past it the boost cannot give the mean back. */
/** @} */

#define SH4DER_LENS_SPOKES_MAX 32   /**< \brief Spokes the maths takes at most. */
#define SH4DER_LENS_RINGS_MAX  16   /**< \brief Rings the maths takes at most. */

/** \brief What a grain tile's texels are. MONO: one level in R, G and B (the first form's tile). CHROMA: a level of
    its own in each of R, G, B (colour grain; the same video memory). SOFT: the mean of two levels (fewer extremes). */
enum { SH4DER_LENS_GRAIN_MONO = 0, SH4DER_LENS_GRAIN_CHROMA = 1, SH4DER_LENS_GRAIN_SOFT = 2 };
/** \brief How a grain moves. JUMP: a new offset every frame (the first form). HOLD: a new offset every `step`
    frames. DRIFT: it slides down `step` texels a frame. STILL: it does not move (dirt on the glass). */
enum { SH4DER_LENS_MOVE_JUMP = 0, SH4DER_LENS_MOVE_HOLD = 1, SH4DER_LENS_MOVE_DRIFT = 2, SH4DER_LENS_MOVE_STILL = 3 };
/** \brief The vignette's falloff across INNER .. OUTER. SMOOTH: a smooth step (the first form). LINEAR: straight.
    SQUARE: slow at first, then steep (a hard-edged tube or scope). */
enum { SH4DER_LENS_CURVE_SMOOTH = 0, SH4DER_LENS_CURVE_LINEAR = 1, SH4DER_LENS_CURVE_SQUARE = 2 };

/** \brief A vertex: screen x, y and ARGB8888. The client copies it into its own vertex type. */
typedef struct sh4der_lens_vtx {
    float x, y;
    uint32_t argb;
} sh4der_lens_vtx_t;

/** \brief The vignette's shape, from sh4der_lens_vig_default. */
typedef struct sh4der_lens_vig {
    float w, h;             /**< \brief Screen size, pixels. */
    float kx;               /**< \brief Metric x scale: lerp(h / w, 1, ROUND). */
    float inner, outer, strength;   /**< \brief Radii and depth, as the SH4DER_LENS_VIG_* values. */
    float tint[3];                  /**< \brief Multiplier per channel at full (1 = neutral). */
    unsigned spokes, rings;         /**< \brief Mesh size: spokes round the screen, rings across the falloff. */
    float cx, cy;                   /**< \brief The centre as a share of the size, 0 .. 1 (0.5, 0.5: the middle). */
    unsigned curve;                 /**< \brief SH4DER_LENS_CURVE_*: the falloff's shape (SMOOTH: the first form). */
} sh4der_lens_vig_t;

/** \brief A damage wedge: unit screen direction it comes from (x right, y down), its age in seconds, on. */
typedef struct sh4der_lens_hit {
    float dx, dy;
    float age;
    uint8_t on;
} sh4der_lens_hit_t;

/** \brief The flash: level drawn (0..1), target it follows (decays to 0). Zero it before use. */
typedef struct sh4der_lens_flash {
    float level, target;
} sh4der_lens_flash_t;

/* ------------------------------------------------------------------------ */
/* Helpers (inline; a trailing underscore: not for callers). */

static inline float sh4der_lens_sqrt_(float x) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_sqrtf(x);
#else
    return sqrtf(x);
#endif
}

static inline float sh4der_lens_abs_(float x) { return x < 0.0f ? -x : x; }

/* The first-order step, as sh4der_look.h. */
static inline float sh4der_lens_step_(float tau, float dt) { return dt > 0.0f ? sh4der_div_(dt, tau + dt) : 0.0f; }

/* ARGB8888 from three channels 0..1 and an alpha 0..1. */
static inline uint32_t sh4der_lens_argb_(float a, float r, float g, float b) {
    return ((uint32_t)(sh4der_sat_(a) * 255.0f + 0.5f) << 24) | ((uint32_t)(sh4der_sat_(r) * 255.0f + 0.5f) << 16) |
           ((uint32_t)(sh4der_sat_(g) * 255.0f + 0.5f) << 8) | (uint32_t)(sh4der_sat_(b) * 255.0f + 0.5f);
}

/* ------------------------------------------------------------------------ */
/* Vignette. */

/** \brief The shipped vignette for a w x h screen. */
static inline void sh4der_lens_vig_default(sh4der_lens_vig_t *v, float w, float h) {
    const float round = SH4DER_LENS_VIG_ROUND;
    v->w = w;
    v->h = h;
    v->kx = (1.0f - round) * sh4der_div_(h, w) + round;   /* lerp(h / w, 1, ROUND) */
    v->inner = SH4DER_LENS_VIG_INNER;
    v->outer = SH4DER_LENS_VIG_OUTER;
    v->strength = SH4DER_LENS_VIG_STRENGTH;
    v->tint[0] = SH4DER_LENS_TINT_R;
    v->tint[1] = SH4DER_LENS_TINT_G;
    v->tint[2] = SH4DER_LENS_TINT_B;
    v->spokes = SH4DER_LENS_VIG_SPOKES;
    v->rings = SH4DER_LENS_VIG_RINGS;
    v->cx = 0.5f;
    v->cy = 0.5f;
    v->curve = SH4DER_LENS_CURVE_SMOOTH;
}

/** \brief The shape between the screen's own (round 0: the ellipse follows the aspect) and a circle (round 1). */
static inline void sh4der_lens_vig_round(sh4der_lens_vig_t *v, float round) {
    round = sh4der_sat_(round);
    v->kx = (1.0f - round) * sh4der_div_(v->h, v->w) + round;
}

/** \brief The centre moved off the middle: cx, cy as shares of the size, held to 0 .. 1 (a scope, a torch held to
    one side). The mesh still covers the whole screen: its rim follows the corner farthest from the centre. */
static inline void sh4der_lens_vig_centre(sh4der_lens_vig_t *v, float cx, float cy) {
    v->cx = sh4der_sat_(cx);
    v->cy = sh4der_sat_(cy);
}

/** \brief The metric radius of a point `dx, dy` from the vignette's centre (1 at the middle of a screen edge, ROUND 0,
    the centre in the middle). */
static inline float sh4der_lens_vig_r(const sh4der_lens_vig_t *v, float dx, float dy) {
    return sh4der_lens_sqrt_(sh4der_len2_(dx * v->kx, dy)) * sh4der_div_(2.0f, v->h);
}

/** \brief The vignette's grey multiplier 0..1 (before tint) at metric radius r: from INNER to OUTER along the
    shape's curve (SMOOTH: a smooth step; LINEAR; SQUARE). */
static inline float sh4der_lens_vig_m(const sh4der_lens_vig_t *v, float r) {
    const float t = sh4der_sat_(sh4der_div_(r - v->inner, v->outer - v->inner));
    if (v->curve == SH4DER_LENS_CURVE_LINEAR) return 1.0f - v->strength * t;
    if (v->curve == SH4DER_LENS_CURVE_SQUARE) return 1.0f - v->strength * (t * t);
    return 1.0f - v->strength * (t * t * (3.0f - 2.0f * t));
}

/* the centre in pixels, and the metric radius the rim stands at: 1.1 x the corner farthest from the centre */
static inline float sh4der_lens_vig_rim_(const sh4der_lens_vig_t *v, float *px, float *py) {
    const float fx = v->cx > 0.5f ? v->cx : 1.0f - v->cx, fy = v->cy > 0.5f ? v->cy : 1.0f - v->cy;
    *px = v->cx * v->w;
    *py = v->cy * v->h;
    return 1.1f * sh4der_lens_vig_r(v, fx * v->w, fy * v->h);
}

/* The vertex colour of a multiplier m: the tint times m, alpha 255 (the multiply blend ignores it). */
static inline uint32_t sh4der_lens_vig_argb(const sh4der_lens_vig_t *v, float m) {
    return sh4der_lens_argb_(1.0f, m * v->tint[0], m * v->tint[1], m * v->tint[2]);
}

/** \brief The spoke table: `n` spokes, and each spoke's rim point (px[j], py[j]) from the screen centre, in pixels. */
typedef struct sh4der_lens_spokes {
    unsigned n;
    float px[SH4DER_LENS_SPOKES_MAX + 1], py[SH4DER_LENS_SPOKES_MAX + 1];   /* each spoke's rim point, from the centre, pixels */
} sh4der_lens_spokes_t;

/** \brief The spoke table (once per screen size): spoke j's rim point, the rim a circumscribing polygon at 1.1 x the corner's
    metric radius, off the screen (a chord to the screen edge would cut the corners off: no spoke lands on a corner, and PVR clips
    what lies past the screen). 0 when the shape is out of range. */
static inline int sh4der_lens_vig_spokes(const sh4der_lens_vig_t *v, sh4der_lens_spokes_t *t) {
    float hw, hh;
    const float rim = sh4der_lens_vig_rim_(v, &hw, &hh);
    unsigned j;
    if (v->spokes < 3u || v->spokes > SH4DER_LENS_SPOKES_MAX || v->rings < 1u || v->rings > SH4DER_LENS_RINGS_MAX)
        return 0;
    t->n = v->spokes;
    for (j = 0; j <= t->n; j++) {   /* spoke j: its unit direction; the rim point is rim / metric(unit) along it */
        float s, c, u;
        sh4der_sincos_(6.28318531f * (float)j / (float)t->n, &s, &c);
        u = sh4der_lens_vig_r(v, c, s);
        t->px[j] = sh4der_div_(c * rim, u);
        t->py[j] = sh4der_div_(s * rim, u);
    }
    return 1;
}

/** \brief One band of the vignette as a strip: band b (0..rings) is 2 x (spokes+1) vertices, A_j at ring b and B_j at ring b+1 on
    spoke j (spoke 0 repeated last). Ring b lies at metric radius min(INNER + (OUTER - INNER) b / rings, rim); the last band runs
    from the outer ring out to the rim. Returns the vertex count (0 when cap is short). */
static inline unsigned sh4der_lens_vig_band(const sh4der_lens_vig_t *v, const sh4der_lens_spokes_t *t, unsigned b, sh4der_lens_vtx_t *out, unsigned cap) {
    const unsigned n = t->n, need = 2u * (n + 1u);
    float hw, hh;                                       /* the centre, pixels */
    const float rim = sh4der_lens_vig_rim_(v, &hw, &hh);
    float lam[2];
    uint32_t argb[2];
    unsigned j, k;
    if (v->rings < 1u || n > SH4DER_LENS_SPOKES_MAX || b > v->rings || need > cap)
        return 0;
    for (k = 0; k < 2u; k++) {   /* A = ring b, B = ring b + 1: a ring's radius and colour are the same on every spoke */
        const unsigned ring = b + k;
        float rho = ring <= v->rings ? v->inner + (v->outer - v->inner) * (float)ring / (float)v->rings : rim;
        if (rho > rim) rho = rim;
        lam[k] = sh4der_div_(rho, rim);
        argb[k] = sh4der_lens_vig_argb(v, sh4der_lens_vig_m(v, rho));
    }
    for (j = 0; j <= n; j++) {
        for (k = 0; k < 2u; k++) {
            out[j * 2u + k].x = hw + t->px[j] * lam[k];
            out[j * 2u + k].y = hh + t->py[j] * lam[k];
            out[j * 2u + k].argb = argb[k];
        }
    }
    return need;
}

/** \brief The whole vignette: rings+1 bands, back to back (band b starts at b x 2 x (spokes+1)). Returns the vertex count, 0 when cap is short. */
static inline unsigned sh4der_lens_vig_mesh(const sh4der_lens_vig_t *v, sh4der_lens_vtx_t *out, unsigned cap) {
    sh4der_lens_spokes_t t;
    const unsigned per = 2u * (v->spokes + 1u);
    unsigned b;
    if (!sh4der_lens_vig_spokes(v, &t) || (v->rings + 1u) * per > cap)
        return 0;
    for (b = 0; b <= v->rings; b++)
        sh4der_lens_vig_band(v, &t, b, out + b * per, per);
    return (v->rings + 1u) * per;
}

/* ------------------------------------------------------------------------ */
/* Grain. */

/** \brief The grain texel's level 0..15 at index i (luma only, a hash: the same tile every boot for one seed). */
static inline unsigned sh4der_lens_grain_level(uint32_t i, uint32_t seed) {
    uint32_t h = i * 2654435761u ^ (seed + 1u) * 0x9e3779b9u;
    h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12; h *= 0x297a2d39u; h ^= h >> 15;
    return (unsigned)(h >> 28);
}

/** \brief The texel's ARGB4444 (alpha 15, the same level in R G B). The colour is level / 15 (0..1). */
static inline uint16_t sh4der_lens_grain_argb4444(unsigned level) {
    return (uint16_t)(0xf000u | (level * 0x111u));
}

/** \brief The tile's mean noise (0..1) from its mean level: sum(levels) / (15 x texels). Pass the sum the tile maker counted. */
static inline float sh4der_lens_grain_mean(uint32_t level_sum, uint32_t texels) {
    return sh4der_div_((float)level_sum, 15.0f * (float)texels);
}

/** \brief The boost (x (1 + boost)) that gives back the mean the multiply takes: 1 / (1 - a + a x mean) - 1. A tile of mean 0.5 at a = 0.06
    gives 0.0317. */
static inline float sh4der_lens_grain_boost(float amp, float mean) {
    return sh4der_div_(1.0f, 1.0f - amp + amp * mean) - 1.0f;
}

/** \brief The tile's offset this frame, texels: a hash of the frame. Writes uv[0..3] = u0, v0, u1, v1 (0..1 in the tile) for a w x h
    screen with GRAIN_PX pixels a texel. The offset is over the whole tile, so the pattern does not repeat in a short run. */
static inline void sh4der_lens_grain_uv(uint32_t frame, float w, float h, float uv[4]) {
    const uint32_t t = SH4DER_LENS_GRAIN_TILE;
    uint32_t a = frame * 2654435761u ^ 0x51u, b = frame * 0x9e3779b9u ^ 0xa7u;
    float ox, oy;
    a ^= a >> 13; a *= 0x2c1b3c6du; a ^= a >> 12;
    b ^= b >> 13; b *= 0x2c1b3c6du; b ^= b >> 12;
    ox = (float)(a & (t - 1u));
    oy = (float)(b & (t - 1u));
    uv[0] = ox / (float)t;
    uv[1] = oy / (float)t;
    uv[2] = uv[0] + sh4der_div_(w, (float)SH4DER_LENS_GRAIN_PX * (float)t);
    uv[3] = uv[1] + sh4der_div_(h, (float)SH4DER_LENS_GRAIN_PX * (float)t);
}

/* ---- Grain, the variations: a tile of the caller's size and kind, any tile's mean, the movement, the colours. ---- */

/** \brief 1 when `tile` is a side a grain tile may have: a power of two, SH4DER_LENS_GRAIN_TILE_MIN .. _MAX. */
static inline int sh4der_lens_grain_tile_ok(unsigned tile) {
    return tile >= SH4DER_LENS_GRAIN_TILE_MIN && tile <= SH4DER_LENS_GRAIN_TILE_MAX && !(tile & (tile - 1u));
}

/** \brief One texel of a tile of `kind` (SH4DER_LENS_GRAIN_*), ARGB4444 with alpha 15, and its three levels 0..15
    in lv (R, G, B). MONO is the first form's texel: sh4der_lens_grain_argb4444(sh4der_lens_grain_level(i, seed)). */
static inline uint16_t sh4der_lens_grain_texel(uint32_t i, uint32_t seed, unsigned kind, unsigned lv[3]) {
    lv[0] = sh4der_lens_grain_level(i, seed);
    if (kind == SH4DER_LENS_GRAIN_CHROMA) {
        lv[1] = sh4der_lens_grain_level(i, seed + 0x01000193u);
        lv[2] = sh4der_lens_grain_level(i, seed + 0x02000326u);
    } else {
        if (kind == SH4DER_LENS_GRAIN_SOFT) lv[0] = (lv[0] + sh4der_lens_grain_level(i, seed ^ 0x5bd1e995u) + (i & 1u)) >> 1;
        lv[1] = lv[2] = lv[0];
    }
    return (uint16_t)(0xf000u | (lv[0] << 8) | (lv[1] << 4) | lv[2]);
}

/** \brief Fill a caller's array with a tile of `tile` x `tile` texels (rows from the top), and give each channel's
    level sum in sum (R, G, B) for sh4der_lens_grain_mean. Returns the texels written, or 0 with nothing written for
    a NULL, a side that is not sh4der_lens_grain_tile_ok or a kind that does not exist. */
static inline unsigned sh4der_lens_grain_fill(uint16_t *px, unsigned tile, uint32_t seed, unsigned kind, uint32_t sum[3]) {
    const unsigned texels = tile * tile;
    unsigned i, lv[3];
    if (!px || !sum || !sh4der_lens_grain_tile_ok(tile) || kind > SH4DER_LENS_GRAIN_SOFT) return 0;
    sum[0] = sum[1] = sum[2] = 0u;
    for (i = 0; i < texels; i++) {
        px[i] = sh4der_lens_grain_texel(i, seed, kind, lv);
        sum[0] += lv[0]; sum[1] += lv[1]; sum[2] += lv[2];
    }
    return texels;
}

/** \brief The mean of ANY ARGB4444 tile, a channel (0..1), for the boost: a picture of dust or paper the caller made
    is not half grey. Returns 0 (and means of a half) for a NULL or no texels. */
static inline int sh4der_lens_grain_tile_mean(const uint16_t *px, unsigned texels, float mean[3]) {
    uint32_t s[3] = { 0u, 0u, 0u };
    unsigned i;
    if (!mean) return 0;
    mean[0] = mean[1] = mean[2] = 0.5f;
    if (!px || !texels) return 0;
    for (i = 0; i < texels; i++) { s[0] += (px[i] >> 8) & 15u; s[1] += (px[i] >> 4) & 15u; s[2] += px[i] & 15u; }
    for (i = 0; i < 3u; i++) mean[i] = sh4der_lens_grain_mean(s[i], texels);
    return 1;
}

/** \brief The tile's place this frame for a w x h rectangle: uv[0..3] = u0, v0, u1, v1. move: SH4DER_LENS_MOVE_*.
    step: HOLD's frames an offset (0 counts as 1), DRIFT's texels a frame; not read by JUMP and STILL. tile: texels
    a side (a power of two); px: screen pixels a texel (0 counts as 1). JUMP with SH4DER_LENS_GRAIN_TILE and
    SH4DER_LENS_GRAIN_PX is sh4der_lens_grain_uv. */
static inline void sh4der_lens_grain_uv_ex(unsigned move, uint32_t frame, unsigned step, unsigned tile, unsigned px, float w, float h, float uv[4]) {
    const uint32_t t = tile;
    float ox = 0.0f, oy = 0.0f;
    if (!px) px = 1u;
    if (move == SH4DER_LENS_MOVE_HOLD) frame /= step ? step : 1u;
    if (move == SH4DER_LENS_MOVE_JUMP || move == SH4DER_LENS_MOVE_HOLD) {
        uint32_t a = frame * 2654435761u ^ 0x51u, b = frame * 0x9e3779b9u ^ 0xa7u;
        a ^= a >> 13; a *= 0x2c1b3c6du; a ^= a >> 12;
        b ^= b >> 13; b *= 0x2c1b3c6du; b ^= b >> 12;
        ox = (float)(a & (t - 1u));
        oy = (float)(b & (t - 1u));
    } else if (move == SH4DER_LENS_MOVE_DRIFT) {
        oy = (float)((t - ((frame * step) & (t - 1u))) & (t - 1u));   /* the picture slides down: the tile's start goes up */
    }
    uv[0] = ox / (float)t;
    uv[1] = oy / (float)t;
    uv[2] = uv[0] + sh4der_div_(w, (float)px * (float)t);
    uv[3] = uv[1] + sh4der_div_(h, (float)px * (float)t);
}

/** \brief The three colours the grain's two quads carry. *argb: the multiply quad's vertex colour, the amplitude
    a (held to 0 .. SH4DER_LENS_GRAIN_AMP_MAX); *oargb: its offset colour, 1 - a; *boost: the boost quad's colour, a
    channel 1 / (1 - a + a x mean) - 1 for that channel's mean, so a tile with colour of its own stays neutral in
    each channel. Any pointer may be NULL. */
static inline void sh4der_lens_grain_colours(float amp, const float mean[3], uint32_t *argb, uint32_t *oargb, uint32_t *boost) {
    const float am = amp < 0.0f ? 0.0f : amp > SH4DER_LENS_GRAIN_AMP_MAX ? SH4DER_LENS_GRAIN_AMP_MAX : amp;
    const uint32_t a = (uint32_t)(am * 255.0f + 0.5f);
    if (argb) *argb = 0xff000000u | (a << 16) | (a << 8) | a;
    if (oargb) *oargb = 0xff000000u | ((255u - a) << 16) | ((255u - a) << 8) | (255u - a);
    if (boost) *boost = mean ? sh4der_lens_argb_(1.0f, sh4der_lens_grain_boost(am, mean[0]), sh4der_lens_grain_boost(am, mean[1]), sh4der_lens_grain_boost(am, mean[2]))
                             : 0xff000000u;
}

/** \brief A grain of the caller's own: its header, its tile and how it moves. Filled by sh4der_lens_grain_make,
    _load or _use (Dreamcast); the fields are for reading. 64 bytes on the Dreamcast, 32-byte aligned (the header is
    sent as it is). */
typedef struct sh4der_lens_grain {
    uint32_t hdr[8];        /**< \brief The multiply header for this tile, compiled (all zero: not ready). */
    void *tex;              /**< \brief The tile in video memory. */
    float mean[3];          /**< \brief The tile's mean a channel, 0..1. */
    uint16_t tile;          /**< \brief Texels a side. */
    uint8_t px;             /**< \brief Screen pixels a texel. */
    uint8_t kind;           /**< \brief SH4DER_LENS_GRAIN_* (a caller's own texels: MONO is written, not read). */
    uint8_t move, step;     /**< \brief SH4DER_LENS_MOVE_* and its step (sh4der_lens_grain_move). */
    uint8_t owned;          /**< \brief 1: the library allocated tex and sh4der_lens_grain_free gives it back. */
    uint8_t ready;          /**< \brief 1: it can be drawn. */
    uint32_t pad_[2];
} __attribute__((aligned(32))) sh4der_lens_grain_t;

/* ------------------------------------------------------------------------ */
/* Damage direction. */

/** \brief A hit from `right` and `fwd` (camera space, unit or not: the attacker's direction; fwd is the way the camera looks).
    Returns 1 when a wedge was made or refreshed, 0 for no direction. Merges with a live wedge within MERGE_COS, else takes a
    free slot, else the oldest. */
static inline int sh4der_lens_hit_add(sh4der_lens_hit_t *slots, unsigned n, float right, float fwd) {
    const float m = sh4der_lens_sqrt_(sh4der_len2_(right, fwd));
    float dx, dy, best = -2.0f;
    unsigned i, pick = 0, oldest = 0;
    int found = -1;
    if (!(m > 1e-6f) || n == 0u)
        return 0;
    dx = sh4der_div_(right, m);   /* screen: x right, y down, forward is up */
    dy = sh4der_div_(-fwd, m);
    for (i = 0; i < n; i++) {
        if (slots[i].on) {
            const float d = dx * slots[i].dx + dy * slots[i].dy;
            if (d >= SH4DER_LENS_HIT_MERGE_COS && d > best) { best = d; found = (int)i; }
        }
    }
    if (found >= 0) {
        slots[found].dx = dx; slots[found].dy = dy; slots[found].age = 0.0f;
        return 1;
    }
    for (i = 0; i < n; i++)
        if (!slots[i].on) break;
    if (i < n) {
        pick = i;
    } else {
        for (i = 1; i < n; i++)
            if (slots[i].age > slots[oldest].age) oldest = i;
        pick = oldest;
    }
    slots[pick].dx = dx; slots[pick].dy = dy; slots[pick].age = 0.0f; slots[pick].on = 1;
    return 1;
}

/** \brief Age every live wedge by dt; switch off at LIFE. */
static inline void sh4der_lens_hit_tick(sh4der_lens_hit_t *slots, unsigned n, float dt) {
    unsigned i;
    for (i = 0; i < n; i++) {
        if (!slots[i].on) continue;
        slots[i].age += dt;
        if (slots[i].age >= SH4DER_LENS_HIT_LIFE_S) slots[i].on = 0;
    }
}

/** \brief A wedge's alpha 0..1 at its age: ramps in over ATTACK, full to HOLD, then linear to 0 at LIFE. */
static inline float sh4der_lens_hit_alpha(float age) {
    if (age < 0.0f) return 0.0f;
    if (age < SH4DER_LENS_HIT_ATTACK_S) return sh4der_div_(age, SH4DER_LENS_HIT_ATTACK_S);
    if (age < SH4DER_LENS_HIT_HOLD_S) return 1.0f;
    return sh4der_sat_(sh4der_div_(SH4DER_LENS_HIT_LIFE_S - age, SH4DER_LENS_HIT_LIFE_S - SH4DER_LENS_HIT_HOLD_S));
}

/** \brief The wedge as a strip of 4 (V0 V1 / V2 V3): V0, V1 on the screen edge where the direction leaves it (alpha =
    the `alpha` argument x HIT_ALPHA), V2, V3 DEPTH inward (alpha 0), the inner edge TAPER x WIDTH. The edge point is on the screen rectangle.
    `w`, `h` the screen size. Returns 4. */
static inline unsigned sh4der_lens_hit_quad(const sh4der_lens_hit_t *hit, float w, float h, float alpha, sh4der_lens_vtx_t out[4]) {
    const float hw = 0.5f * w, hh = 0.5f * h;
    const float ax = sh4der_lens_abs_(hit->dx), ay = sh4der_lens_abs_(hit->dy);
    float dist = 1e9f, px, py, tx, ty, a;
    uint32_t edge_col, inner_col;
    if (ax > 1e-6f) dist = sh4der_div_(hw, ax);
    if (ay > 1e-6f && sh4der_div_(hh, ay) < dist) dist = sh4der_div_(hh, ay);
    px = hw + hit->dx * dist;   /* the edge point, screen pixels */
    py = hh + hit->dy * dist;
    tx = -hit->dy;              /* tangent along the edge (unit) */
    ty = hit->dx;
    a = sh4der_sat_(alpha) * SH4DER_LENS_HIT_ALPHA;
    edge_col = sh4der_lens_argb_(a, SH4DER_LENS_HIT_R, SH4DER_LENS_HIT_G, SH4DER_LENS_HIT_B);
    inner_col = sh4der_lens_argb_(0.0f, SH4DER_LENS_HIT_R, SH4DER_LENS_HIT_G, SH4DER_LENS_HIT_B);
    out[0].x = px + tx * SH4DER_LENS_HIT_WIDTH; out[0].y = py + ty * SH4DER_LENS_HIT_WIDTH; out[0].argb = edge_col;
    out[1].x = px - tx * SH4DER_LENS_HIT_WIDTH; out[1].y = py - ty * SH4DER_LENS_HIT_WIDTH; out[1].argb = edge_col;
    out[2].x = px + tx * SH4DER_LENS_HIT_WIDTH * SH4DER_LENS_HIT_TAPER - hit->dx * SH4DER_LENS_HIT_DEPTH;
    out[2].y = py + ty * SH4DER_LENS_HIT_WIDTH * SH4DER_LENS_HIT_TAPER - hit->dy * SH4DER_LENS_HIT_DEPTH; out[2].argb = inner_col;
    out[3].x = px - tx * SH4DER_LENS_HIT_WIDTH * SH4DER_LENS_HIT_TAPER - hit->dx * SH4DER_LENS_HIT_DEPTH;
    out[3].y = py - ty * SH4DER_LENS_HIT_WIDTH * SH4DER_LENS_HIT_TAPER - hit->dy * SH4DER_LENS_HIT_DEPTH; out[3].argb = inner_col;
    return 4;
}

/* ------------------------------------------------------------------------ */
/* Flash. */

/** \brief A hit: the target rises to at least `peak` (0..1). */
static inline void sh4der_lens_flash_hit(sh4der_lens_flash_t *f, float peak) {
    if (peak > f->target) f->target = sh4der_sat_(peak);
}

/** \brief One frame: the level follows the target over ATTACK; the target falls over DECAY, but only once the level has
    caught up (95 %), so a hit reaches its peak before it fades. */
static inline void sh4der_lens_flash_step(sh4der_lens_flash_t *f, float dt) {
    f->level += (f->target - f->level) * sh4der_lens_step_(SH4DER_LENS_FLASH_ATTACK_S, dt);
    if (f->level >= 0.95f * f->target)
        f->target -= f->target * sh4der_lens_step_(SH4DER_LENS_FLASH_DECAY_S, dt);
    if (f->target < 1.0f / 512.0f) f->target = 0.0f;
    if (f->level < 1.0f / 512.0f) f->level = 0.0f;
}

/** \brief The layer's vertex colour at `level`: additive (ONE, ONE) takes the colour x level with alpha 0; alpha blend takes
    the colour with alpha = level. */
static inline uint32_t sh4der_lens_flash_argb(float level, const float rgb[3], int additive) {
    if (additive)
        return sh4der_lens_argb_(0.0f, rgb[0] * level, rgb[1] * level, rgb[2] * level);
    return sh4der_lens_argb_(level, rgb[0], rgb[1], rgb[2]);
}

#ifdef _arch_dreamcast
/* The Dreamcast draw side (src/sh4der_lens.c). */

/* Once per PVR start: makes the first form's grain tile (8 KB of video memory) and the polygon headers. Returns 1 once the
   headers are compiled, ALSO when video memory was short and there is no tile (the lens then draws what it can, without
   grain): ask sh4der_lens_ready for the tile. A second call does nothing and returns 1. */
int sh4der_lens_init(void);
/* 1 when sh4der_lens_init has run and its grain tile is in video memory (sh4der_lens_grain will draw); 0 otherwise. */
int sh4der_lens_ready(void);
/* The untextured headers alone (vignette, boost, hit, flash), no video memory: for a client that wants no grain, or
   only a grain of its own. sh4der_lens_init and the grain makers call it. A second call does nothing. */
void sh4der_lens_init_headers(void);
/* Builds the vignette's rim table for a screen of w x h pixels. Call once after init and again whenever the screen size
   changes. Returns 1 when the table is ready, 0 on a bad size (the vignette then draws nothing). */
int sh4der_lens_vignette_setup(float w, float h);
/* Draws the vignette (the darkened screen edge) on the translucent list. No arguments; draws nothing until
   sh4der_lens_vignette_setup has returned 1. Its depth key lies under the HUD. */
void sh4der_lens_vignette_draw(void);
/* Draws the film grain over a w x h screen on the translucent list; frame is the frame counter, which moves the tile
   so the grain changes each frame. Draws nothing when init found no video memory for the tile. */
void sh4der_lens_grain(uint32_t frame, float w, float h);
/* Draws the damage wedges that are on (sh4der_lens_hit_add, _tick) over a w x h screen on the translucent list. */
void sh4der_lens_hits_draw(const sh4der_lens_hit_t *slots, unsigned n, float w, float h);
/* Draws the flash at its level in the colour rgb over a w x h screen on the translucent list: additive 1 adds it, 0
   blends it. Nothing at level 0. */
void sh4der_lens_flash_draw(const sh4der_lens_flash_t *f, const float rgb[3], int additive, float w, float h);
/* At a PVR start (the reset clears video memory): forgets the first form's tile and the vignette's table. Call
   sh4der_lens_init and sh4der_lens_vignette_setup again. */
void sh4der_lens_forget(void);

/* ---- The variations. Every draw is for the translucent list, returns the triangles it sent, and sends nothing (0)
   when its header was never compiled: a draw before its setup is refused, not sent as an empty header. ---- */

/* The vignette's shape from the caller's own sh4der_lens_vig_t (start from sh4der_lens_vig_default, then change
   strength, inner, outer, tint, spokes, rings, and sh4der_lens_vig_round / _centre / .curve). It replaces what
   sh4der_lens_vignette_setup set. Returns 1 when the rim table is ready, 0 for a NULL or a mesh size out of range. */
int sh4der_lens_vignette_shape(const sh4der_lens_vig_t *shape);
/* The vignette set by _setup or _shape, moved by (ox, oy) pixels and at the depth key z: a picture band or a window
   that does not start at the screen's corner. (0, 0, SH4DER_LENS_Z_VIG) is sh4der_lens_vignette_draw. */
unsigned sh4der_lens_vignette_draw_at(float ox, float oy, float z);
/* A mesh the caller built once with sh4der_lens_vig_mesh, through the library's multiply header: strips of `per`
   vertices (2 x (spokes + 1)), moved by (ox, oy), at z. Nothing is rebuilt a frame. 0 when verts is not a whole
   number of strips, per is under 4, or the headers are not compiled (call sh4der_lens_init, or make a grain, first). */
unsigned sh4der_lens_vignette_mesh_draw(const sh4der_lens_vtx_t *mesh, unsigned verts, unsigned per, float ox, float oy, float z);

/* A grain the library makes: a tile of `tile` texels a side (sh4der_lens_grain_tile_ok), `px` screen pixels a texel
   (1 or 2), from `seed`, of `kind` (SH4DER_LENS_GRAIN_*). Allocates tile x tile x 2 bytes of video memory, fills and
   uploads it, compiles the header. It moves by JUMP until sh4der_lens_grain_move says otherwise. Returns 1, or 0
   (the grain not ready, nothing allocated) for bad arguments or no video memory. Does not need sh4der_lens_init. */
int sh4der_lens_grain_make(sh4der_lens_grain_t *g, unsigned tile, unsigned px, uint32_t seed, unsigned kind);
/* The same from the caller's own texels: tile x tile ARGB4444, rows from the top (dust, paper, a scanned grain). The
   library uploads them and works out the tile's mean a channel, so neutral stays right for a tile that is not half
   grey. The array is not kept. Returns 1, or 0 as sh4der_lens_grain_make. */
int sh4der_lens_grain_load(sh4der_lens_grain_t *g, const uint16_t *texels, unsigned tile, unsigned px);
/* The same for a texture already in video memory (ARGB4444, not twiddled, tile x tile): the library compiles the
   header and allocates nothing; mean is the tile's mean a channel (NULL: a half). sh4der_lens_grain_free will not
   free it. Returns 1, or 0 for bad arguments. */
int sh4der_lens_grain_use(sh4der_lens_grain_t *g, void *tex, unsigned tile, unsigned px, const float mean[3]);
/* How the grain moves from now on: move is SH4DER_LENS_MOVE_*, step the frames a HOLD keeps an offset
   (SH4DER_LENS_GRAIN_HOLD) or the texels a frame a DRIFT slides (SH4DER_LENS_GRAIN_DRIFT), at most 255. */
void sh4der_lens_grain_move(sh4der_lens_grain_t *g, unsigned move, unsigned step);
/* Draws the grain over the rectangle (x0, y0) .. (x1, y1) at the depth key z with the amplitude amp (0 ..
   SH4DER_LENS_GRAIN_AMP_MAX). neutral 1: the boost quad follows, so the mean brightness holds (two layers); 0: the
   multiply alone, one layer, the picture darker by amp x (1 - the tile's mean). Returns 2 or 4 triangles, 0 when the
   grain is not ready or the rectangle is empty. */
unsigned sh4der_lens_grain_draw(const sh4der_lens_grain_t *g, uint32_t frame, float x0, float y0, float x1, float y1, float z, float amp, int neutral);
/* Gives back the video memory the library allocated for g (after waiting for the PVR to finish with it) and marks g
   not ready. A grain from sh4der_lens_grain_use keeps its texture. */
void sh4der_lens_grain_free(sh4der_lens_grain_t *g);
/* At a PVR start (the reset clears video memory): marks g not ready without freeing anything. Make it again. */
void sh4der_lens_grain_forget(sh4der_lens_grain_t *g);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_LENS_H */
