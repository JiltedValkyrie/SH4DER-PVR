/* SH4DER-PVR: the lens effects

   sh4der_lens.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
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

    Every time step is first order, dt / (tau + dt), as in sh4der_look.h.
    Shipped tuning is SH4DER_LENS_* below. Screen units: pixels, y down,
    the screen origin at the top left.

    \author Jilted Valkyrie
*/

#ifndef SH4DER_LENS_H
#define SH4DER_LENS_H

#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Shipped tuning
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
#define SH4DER_LENS_FLASH_R      0.60f  /**< \brief Shield pop colour (blue-white). */
#define SH4DER_LENS_FLASH_G      0.80f
#define SH4DER_LENS_FLASH_B      1.00f
#define SH4DER_LENS_Z_VIG        430.0f /**< \brief Autosort z of each overlay (TR list). Under HUD (1000). */
#define SH4DER_LENS_Z_GRAIN      432.0f
#define SH4DER_LENS_Z_HIT        950.0f
#define SH4DER_LENS_Z_FLASH      960.0f
/** @} */

#define SH4DER_LENS_SPOKES_MAX 32   /**< \brief Spokes the maths takes at most. */
#define SH4DER_LENS_RINGS_MAX  16   /**< \brief Rings the maths takes at most. */

/** \brief A vertex: screen x, y and ARGB8888. The client copies it into its own vertex type. */
typedef struct sh4der_lens_vtx {
    float x, y;
    uint32_t argb;
} sh4der_lens_vtx_t;

/** \brief The vignette's shape, from sh4der_lens_vig_default. */
typedef struct sh4der_lens_vig {
    float w, h;             /**< \brief Screen size, pixels. */
    float kx;               /**< \brief Metric x scale: lerp(h / w, 1, ROUND). */
    float inner, outer, strength;
    float tint[3];
    unsigned spokes, rings;
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
}

/** \brief The metric radius of a point `dx, dy` from the screen centre (1 at the middle of a screen edge, ROUND 0). */
static inline float sh4der_lens_vig_r(const sh4der_lens_vig_t *v, float dx, float dy) {
    return sh4der_lens_sqrt_(sh4der_len2_(dx * v->kx, dy)) * sh4der_div_(2.0f, v->h);
}

/** \brief The vignette's grey multiplier 0..1 (before tint) at metric radius r: smoothstep from INNER to OUTER. */
static inline float sh4der_lens_vig_m(const sh4der_lens_vig_t *v, float r) {
    const float t = sh4der_sat_(sh4der_div_(r - v->inner, v->outer - v->inner));
    return 1.0f - v->strength * (t * t * (3.0f - 2.0f * t));
}

/* The vertex colour of a multiplier m: the tint times m, alpha 255 (the multiply blend ignores it). */
static inline uint32_t sh4der_lens_vig_argb(const sh4der_lens_vig_t *v, float m) {
    return sh4der_lens_argb_(1.0f, m * v->tint[0], m * v->tint[1], m * v->tint[2]);
}

/** \brief The vignette as strips: rings+1 bands of 2 x (spokes+1) vertices (A_j, B_j per spoke, the spoke 0 repeated last).
    Band b is a triangle strip from ring b to ring b+1, the last band runs from the outer ring to the screen edge. A ring b
    lies at metric radius min(INNER + (OUTER - INNER) b / rings, edge): the edge is where a spoke leaves the screen.
    Returns the vertex count (0 when cap is short or the shape is out of range). */
typedef struct sh4der_lens_spokes {
    unsigned n;
    float px[SH4DER_LENS_SPOKES_MAX + 1], py[SH4DER_LENS_SPOKES_MAX + 1];   /* each spoke's rim point, from the centre */
} sh4der_lens_spokes_t;

/** \brief The spoke table (once per screen size): spoke j's rim point, the rim a circumscribing polygon at 1.1 x the corner's
    metric radius, off the screen (a chord to the screen edge would cut the corners off: no spoke lands on a corner, and PVR clips
    what lies past the screen). 0 when the shape is out of range. */
static inline int sh4der_lens_vig_spokes(const sh4der_lens_vig_t *v, sh4der_lens_spokes_t *t) {
    const float rim = 1.1f * sh4der_lens_vig_r(v, 0.5f * v->w, 0.5f * v->h);
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
    const float hw = 0.5f * v->w, hh = 0.5f * v->h;
    const float rim = 1.1f * sh4der_lens_vig_r(v, hw, hh);
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

/** \brief The wedge as a strip of 4 (V0 V1 / V2 V3): V0, V1 on the screen edge where the direction leaves it (alpha
    alpha x HIT_ALPHA), V2, V3 DEPTH inward (alpha 0), the inner edge TAPER x WIDTH. The edge point is on the screen rectangle.
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

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_LENS_H */
