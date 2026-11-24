/* SH4DER-PVR: a cloud deck you fly through, marched on the CPU into a small buffer

   sh4der_cloudvolume.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_cloudvolume.h
    \brief   Volumetric clouds for a first-person flight: a software ray march through a cloud
             slab into a small 8-bit buffer, which the PVR draws as one filtered quad over its
             own sky.

    Cost first (ESTIMATE from instruction counts, not measured on a console). RAM: the maps
    (sh4der_cloudvolume_maps_t, 66,560 B: a 128 x 128 cover map at 16 bits, a 32 x 32 x 32
    noise volume at 8 and a 32 x 32 map of the highest tops), the frame block (1,072 B), and the caller's buffers: one shade byte
    and one alpha byte a buffer pixel (24 KB at 128 x 96). Video memory: the caller's texture,
    one byte a pixel as a palette index (16 KB for a 128 x 128 texture) or two for RGB565.
    CPU: about 16 cycles for a sample in empty air, 42 for one that reads the cover map, 38
    more when it finds cloud, 52 a noise octave; the march counts its rays, samples, skips, hits
    and noise reads (sh4der_cloudvolume_frame_t counters), so a client
    shows the real numbers. PVR: one blended full-screen quad. Audio: nothing.

    The volume. A slab of air between `base_y` and `base_y + thickness`. A tiling cover map says
    how high the cloud stands over each point (cumulus billows); the density at a point is how
    far it is under that top and over the base, less an erosion read from a small tiling noise
    volume (0, 1 or 2 octaves) that moves on its own, so the shapes have overhangs and wisps
    and change as they drift.

    The march. For a buffer pixel: its ray is cut to the slab, then sampled `steps` times front
    to back, the steps growing with distance and starting at a per-pixel offset (an ordered
    4 x 4 pattern) so the layers do not show as bands. Each sample's opacity is
    1 - exp(-density x step length), from a table, so a different step count gives the same
    cloud. The march stops once 97 % is hidden. A ray that never meets the slab costs nothing.

    The light. `light_samples` 0: the cover map's second byte, baked by
    sh4der_cloudvolume_bake, is the height under which a point is in another cloud's shadow;
    the light falls off with the depth under it. 1 .. 4: that many density reads toward the sun
    a sample. Both: a table brightens rays that look toward the sun (forward scattering), and
    the light from the sky grows with height in the cloud.

    The output. A shade (0 shadow .. 255 sunlit) and an alpha a pixel. Distance pulls the shade
    toward a pale haze. sh4der_cloudvolume_index packs both
    into a palette index (96 shades of solid cloud, 13 alphas x 12 shades for the soft edges,
    ordered dither) for a PAL8 texture whose
    palette sh4der_cloudvolume_palette fills; sh4der_cloudvolume_rgb gives the colour for a
    client that composes its own RGB565 picture instead.

    What it is not: one slab (no tower above its top), no sorting against scene objects (the
    quad is a backdrop), no multiple scattering solve.

    No allocation, no globals. Floats through sh4der_core.h (SH4ZAM, or plain C with
    SH4DER_CORE_NO_SH4ZAM). By hand: the integer hash, the fixed-point map reads and the byte
    compositing (SH4ZAM has no integer raster routines); the exponential tables as running
    products and the step growth as a Newton root (shz_expf and shz_powf are within 3 %, too
    coarse for a table that must start at exactly 1).

    Technique references (ideas only, no code): Ikits, Kniss, Lefohn, Hansen, "Volume Rendering
    Techniques", GPU Gems ch. 39 (front-to-back compositing, opacity correction, noise volumes).

    Version: v0.50 (host prototype and Dreamcast demo; the console look is her check)
    The effect: a sunlit cumulus deck seen from a camera flying over and through it.
    Typical use: a sky fly-through; a backdrop of moving cloud behind a scene.

    \author Jilted Valkyrie
*/

#ifndef SH4DER_CLOUDVOLUME_H
#define SH4DER_CLOUDVOLUME_H

#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Sizes
    @{ */
#define SH4DER_CLOUDVOLUME_MAP        128      /**< \brief Cover map side, texels (a power of two). */
#define SH4DER_CLOUDVOLUME_NOISE      32       /**< \brief Noise volume side, voxels (a power of two). */
#define SH4DER_CLOUDVOLUME_COARSE     32       /**< \brief Side of the coarse highest-top map, cells of 4 x 4 texels. */
#define SH4DER_CLOUDVOLUME_MAX_STEPS  64       /**< \brief Most march steps a ray. */
#define SH4DER_CLOUDVOLUME_MAX_LIGHT  4        /**< \brief Most light samples a march sample. */
#define SH4DER_CLOUDVOLUME_BAKE_TAPS  40       /**< \brief Map reads toward the sun a texel in the bake. */
/** @} */

/** \name Default tuning
    @{ */
#define SH4DER_CLOUDVOLUME_TILE       8192.0f  /**< \brief World units (metres) one cover map covers: 64 m a texel. */
#define SH4DER_CLOUDVOLUME_THICKNESS  1100.0f  /**< \brief Slab height, metres. */
#define SH4DER_CLOUDVOLUME_DENSITY    0.010f   /**< \brief Extinction a metre in full cloud (63 % hidden after 100 m). */
#define SH4DER_CLOUDVOLUME_COVERAGE   0.60f    /**< \brief Share of the cover map's range that is cloud. */
#define SH4DER_CLOUDVOLUME_EROSION    0.42f    /**< \brief How much the noise carves, 0 .. 1. */
#define SH4DER_CLOUDVOLUME_SHADOW     3.0f     /**< \brief Light lost over a full slab height of cloud toward the sun, as an exponent. */
#define SH4DER_CLOUDVOLUME_HAZE       9000.0f  /**< \brief Distance at which a cloud's shade is 63 % of the way to the haze, metres. */
#define SH4DER_CLOUDVOLUME_FAR        26000.0f /**< \brief March end, metres. */
#define SH4DER_CLOUDVOLUME_NEAR       12.0f    /**< \brief March start, metres. */
#define SH4DER_CLOUDVOLUME_STEPS      16       /**< \brief Default march steps. */
#define SH4DER_CLOUDVOLUME_GROWTH_LN  3.3322045f /**< \brief The natural logarithm of SH4DER_CLOUDVOLUME_GROWTH. */
#define SH4DER_CLOUDVOLUME_GROWTH     28.0f    /**< \brief Last step length over the first. */
#define SH4DER_CLOUDVOLUME_HAZE_SHADE 150      /**< \brief The shade far cloud fades to (a pale blue-grey on the ramp). */
#define SH4DER_CLOUDVOLUME_STOP       0.03f    /**< \brief The march stops when this share of the light still comes through. */
/** @} */

/** \brief The maps: 66,560 B, the caller's (static or aligned storage). */
typedef struct sh4der_cloudvolume_maps {
    uint16_t cover[SH4DER_CLOUDVOLUME_MAP * SH4DER_CLOUDVOLUME_MAP];   /**< \brief Low byte: cloud height 0 .. 255. High byte: shadow height (the bake). */
    uint8_t noise[SH4DER_CLOUDVOLUME_NOISE * SH4DER_CLOUDVOLUME_NOISE * SH4DER_CLOUDVOLUME_NOISE];   /**< \brief Erosion noise. */
    uint8_t reach[SH4DER_CLOUDVOLUME_COARSE * SH4DER_CLOUDVOLUME_COARSE];   /**< \brief The highest height byte in each 4 x 4 texel cell and one texel round it: air above it is empty. */
} sh4der_cloudvolume_maps_t;

/** \brief The clouds. Fill with sh4der_cloudvolume_init, then change any field. */
typedef struct sh4der_cloudvolume {
    float tile;             /**< \brief Metres one cover map covers. */
    float base_y;           /**< \brief Bottom of the slab, world height. */
    float thickness;        /**< \brief Slab height, metres. */
    float density;          /**< \brief Extinction a metre in full cloud. */
    float coverage;         /**< \brief 0.05 .. 0.95: more is more cloud. */
    float erosion;          /**< \brief 0 .. 1. */
    float shadow;           /**< \brief Light falloff exponent over one slab height. */
    float haze;             /**< \brief Metres. */
    float far_m;            /**< \brief March end, metres. */
    float sun[3];           /**< \brief Unit direction from the scene to the sun (y up). */
    float wind[2];          /**< \brief Drift of the cover map, metres a second (x, z). */
    float churn;            /**< \brief Rise of the noise volume, metres a second. */
    uint32_t steps;         /**< \brief 4 .. SH4DER_CLOUDVOLUME_MAX_STEPS. */
    uint32_t light_samples; /**< \brief 0 (baked) .. SH4DER_CLOUDVOLUME_MAX_LIGHT. */
    uint32_t detail;        /**< \brief Noise octaves: 0, 1 or 2. */
    uint32_t jitter;        /**< \brief 1: ray starts are offset by an ordered pattern. */
} sh4der_cloudvolume_t;

/** \brief A camera. */
typedef struct sh4der_cloudvolume_view {
    float eye[3];           /**< \brief World position, metres. */
    float yaw;              /**< \brief Radians, 0 looks along +z, positive turns toward +x. */
    float pitch;            /**< \brief Radians, positive looks up. */
    float roll;             /**< \brief Radians; NOT used by the march: the client turns the quad by it. */
    float tan_x;            /**< \brief Tangent of half the buffer's horizontal field of view (overscan included). */
    float tan_y;            /**< \brief The same, vertical. */
} sh4der_cloudvolume_view_t;

/** \brief One frame's numbers, from sh4der_cloudvolume_prepare. 1,072 B. */
typedef struct sh4der_cloudvolume_frame {
    float o[3];             /* eye in texel units; y above the slab base */
    float fwd[3], right[3], up[3];
    float sun[3];
    float tan_x, tan_y;
    float th;               /* slab height, texel units */
    float near_t, far_t;    /* texel units */
    float yn_scale;         /* 255 / th */
    float od_scale;         /* optical depth index a (density byte x texel of path) */
    float haze_scale;       /* haze table index a texel of path */
    float light_scale;      /* beer table index a (depth byte) */
    float tap_scale;        /* beer table index a (density byte x texel) of a light sample */
    float cover_off[2];     /* texels */
    float noise_off[3];     /* texels */
    float u[SH4DER_CLOUDVOLUME_MAX_STEPS + 1];   /* where each step starts, 0 .. 1 of the ray's stretch in the slab */
    int32_t cover_thr;      /* height byte under which there is no cloud */
    int32_t cover_gain;     /* 8.8: byte above the threshold -> top 0 .. 255 */
    int32_t erode;          /* 0 .. 256 */
    uint32_t steps, light_samples, detail, jitter, phase;
    uint8_t alpha_tab[256]; /* 1 - exp(-i / 32), x 255 */
    uint8_t beer_tab[256];  /* exp(-i / 64), x 255 */
    uint8_t haze_tab[64];   /* exp(-i / 16), x 255 */
    uint8_t phase_tab[64];  /* sun gain by cos(ray, sun): index (cos + 1) x 31.5 */
    uint32_t rays;          /**< \brief Counter: rays that met the slab. */
    uint32_t samples;       /**< \brief Counter: density reads that went to the cover map (light samples included). */
    uint32_t skips;         /**< \brief Counter: density reads answered by the coarse map alone (empty air). */
    uint32_t hits;          /**< \brief Counter: march samples that found cloud (lit and composited). */
    uint32_t noise_reads;   /**< \brief Counter: noise volume reads (8 bytes each). */
} sh4der_cloudvolume_frame_t;

/* square root (SH4ZAM, or plain C) */
#ifndef SH4DER_CORE_NO_SH4ZAM
static inline float sh4der_cloudvolume_sqrt_(float x) { return shz_sqrtf(x); }
#else
static inline float sh4der_cloudvolume_sqrt_(float x) { return sqrtf(x); }
#endif

/* The m-th root of g (g > 1, m 1 .. 63): Newton's method from 1 + ln(g) / m, the power by repeated multiply. Six rounds
   are exact to a float's last digits for the roots used here. (shz_powf and shz_expf are within 3 %: fine for a
   picture's gain, too coarse for a root this close to 1 or for a table that must start at exactly 1.) */
static inline float sh4der_cloudvolume_root_(float g, float ln_g, uint32_t m) {
    float r = 1.0f + sh4der_div_(ln_g, (float)m);
    unsigned round, k;

    for (round = 0; round < 6u; round++) {
        float p = 1.0f;

        for (k = 1u; k < m; k++) {
            p *= r;
        }
        r -= sh4der_div_(p * r - g, (float)m * p);
    }
    return r;
}

/* a 32-bit integer hash of three lattice coordinates and a seed (hand: integer arithmetic) */
static inline uint32_t sh4der_cloudvolume_hash_(uint32_t x, uint32_t y, uint32_t z, uint32_t seed) {
    uint32_t h = x * 0x8DA6B343u + y * 0xD8163841u + z * 0xCB1AB31Fu + seed * 0x165667B1u;

    h ^= h >> 13;
    h *= 0x5BD1E995u;
    h ^= h >> 15;
    h *= 0x27D4EB2Fu;
    h ^= h >> 13;
    return h;
}

/* smooth lattice noise 0 .. 1 that repeats every `cells` cells; fx, fy, fz in 0 .. 1 of the period */
static inline float sh4der_cloudvolume_lattice_(float fx, float fy, float fz, uint32_t cells, uint32_t seed) {
    const float gx = fx * (float)cells, gy = fy * (float)cells, gz = fz * (float)cells;
    const float ix = gx - sh4der_frac_(gx), iy = gy - sh4der_frac_(gy), iz = gz - sh4der_frac_(gz);
    const uint32_t x0 = (uint32_t)ix % cells, y0 = (uint32_t)iy % cells, z0 = (uint32_t)iz % cells;
    const uint32_t x1 = (x0 + 1u) % cells, y1 = (y0 + 1u) % cells, z1 = (z0 + 1u) % cells;
    float tx = gx - ix, ty = gy - iy, tz = gz - iz;
    float c[8];
    unsigned k;

    tx = tx * tx * (3.0f - 2.0f * tx);
    ty = ty * ty * (3.0f - 2.0f * ty);
    tz = tz * tz * (3.0f - 2.0f * tz);
    for (k = 0; k < 8u; k++) {
        c[k] = (float)(sh4der_cloudvolume_hash_((k & 1u) ? x1 : x0, (k & 2u) ? y1 : y0, (k & 4u) ? z1 : z0, seed) >> 8) * (1.0f / 16777216.0f);
    }
    return sh4der_lerp_(sh4der_lerp_(sh4der_lerp_(c[0], c[1], tx), sh4der_lerp_(c[2], c[3], tx), ty),
                        sh4der_lerp_(sh4der_lerp_(c[4], c[5], tx), sh4der_lerp_(c[6], c[7], tx), ty), tz);
}

/* a billow: 0 in the creases between puffs, round on top */
static inline float sh4der_cloudvolume_billow_(float n) {
    const float a = sh4der_abs_(n + n - 1.0f);

    return 1.0f - (1.0f - a) * (1.0f - a);
}

/** \brief Sets the clouds to the DEFAULTS: a 1,100 m deck from height 0, sun 12 degrees up and 8 degrees right of +z. */
static inline void sh4der_cloudvolume_init(sh4der_cloudvolume_t *c) {
    if (!c) {
        return;
    }
    c->tile = SH4DER_CLOUDVOLUME_TILE;
    c->base_y = 0.0f;
    c->thickness = SH4DER_CLOUDVOLUME_THICKNESS;
    c->density = SH4DER_CLOUDVOLUME_DENSITY;
    c->coverage = SH4DER_CLOUDVOLUME_COVERAGE;
    c->erosion = SH4DER_CLOUDVOLUME_EROSION;
    c->shadow = SH4DER_CLOUDVOLUME_SHADOW;
    c->haze = SH4DER_CLOUDVOLUME_HAZE;
    c->far_m = SH4DER_CLOUDVOLUME_FAR;
    c->sun[0] = 0.13613f;                                       /* cos 12 x sin 8 */
    c->sun[1] = 0.20791f;                                       /* sin 12 */
    c->sun[2] = 0.96863f;                                       /* cos 12 x cos 8 */
    c->wind[0] = 6.0f;
    c->wind[1] = -3.0f;
    c->churn = 4.0f;
    c->steps = SH4DER_CLOUDVOLUME_STEPS;
    c->light_samples = 0u;
    c->detail = 1u;
    c->jitter = 1u;
}

/** \brief 1 when every number is finite and in range, else 0 (also 0 for NULL). */
static inline int sh4der_cloudvolume_valid(const sh4der_cloudvolume_t *c) {
    float s2;

    if (!c) {
        return 0;
    }
    if (!sh4der_finite_(c->tile) || !(c->tile >= 256.0f) || !(c->tile <= 1.0e6f) ||
        !sh4der_finite_(c->base_y) || !(sh4der_abs_(c->base_y) <= 1.0e6f) ||
        !sh4der_finite_(c->thickness) || !(c->thickness >= 16.0f) || !(c->thickness <= 1.0e5f)) {
        return 0;
    }
    if (!sh4der_finite_(c->density) || !(c->density > 0.0f) || !(c->density <= 1.0f) ||
        !sh4der_finite_(c->coverage) || !(c->coverage >= 0.05f) || !(c->coverage <= 0.95f) ||
        !sh4der_unit_(c->erosion) || !sh4der_finite_(c->shadow) || !(c->shadow >= 0.0f) || !(c->shadow <= 64.0f)) {
        return 0;
    }
    if (!sh4der_finite_(c->haze) || !(c->haze >= 100.0f) || !(c->haze <= 1.0e7f) ||
        !sh4der_finite_(c->far_m) || !(c->far_m >= 100.0f) || !(c->far_m <= 1.0e6f)) {
        return 0;
    }
    if (!sh4der_finite_(c->sun[0]) || !sh4der_finite_(c->sun[1]) || !sh4der_finite_(c->sun[2]) ||
        !sh4der_finite_(c->wind[0]) || !sh4der_finite_(c->wind[1]) || !sh4der_finite_(c->churn) ||
        !(sh4der_abs_(c->wind[0]) <= 1000.0f) || !(sh4der_abs_(c->wind[1]) <= 1000.0f) || !(sh4der_abs_(c->churn) <= 1000.0f)) {
        return 0;
    }
    s2 = sh4der_len3_(c->sun[0], c->sun[1], c->sun[2]);
    if (!(s2 >= 0.81f) || !(s2 <= 1.21f)) {
        return 0;
    }
    return c->steps >= 4u && c->steps <= SH4DER_CLOUDVOLUME_MAX_STEPS && c->light_samples <= SH4DER_CLOUDVOLUME_MAX_LIGHT &&
           c->detail <= 2u && c->jitter <= 1u;
}

/** \brief Fills the cover map's heights and the noise volume from `seed`. Load time: about 1.6 million lattice reads.
    The shadow bytes are left 0: call sh4der_cloudvolume_bake after. Returns 1, or 0 for NULL. */
static inline int sh4der_cloudvolume_generate(sh4der_cloudvolume_maps_t *m, uint32_t seed) {
    const float inv = 1.0f / (float)SH4DER_CLOUDVOLUME_MAP;
    const float ninv = 1.0f / (float)SH4DER_CLOUDVOLUME_NOISE;
    unsigned x, y, z;

    if (!m) {
        return 0;
    }
    for (y = 0; y < SH4DER_CLOUDVOLUME_MAP; y++) {
        for (x = 0; x < SH4DER_CLOUDVOLUME_MAP; x++) {
            const float fx = (float)x * inv, fy = (float)y * inv;
            /* large clumps and clear lanes, then puffs on puffs */
            const float clump = sh4der_cloudvolume_lattice_(fx, fy, 0.0f, 3u, seed) * 0.6f + sh4der_cloudvolume_lattice_(fx, fy, 0.0f, 6u, seed + 1u) * 0.4f;
            const float puff = sh4der_cloudvolume_billow_(sh4der_cloudvolume_lattice_(fx, fy, 0.0f, 7u, seed + 2u)) * 0.52f +
                               sh4der_cloudvolume_billow_(sh4der_cloudvolume_lattice_(fx, fy, 0.0f, 13u, seed + 3u)) * 0.32f +
                               sh4der_cloudvolume_billow_(sh4der_cloudvolume_lattice_(fx, fy, 0.0f, 27u, seed + 4u)) * 0.16f;
            const float h = sh4der_sat_(clump * 0.62f + puff * 0.46f - 0.04f);

            m->cover[y * SH4DER_CLOUDVOLUME_MAP + x] = (uint16_t)(h * 255.0f + 0.5f);   /* hand: the byte */
        }
    }
    for (z = 0; z < SH4DER_CLOUDVOLUME_NOISE; z++) {
        for (y = 0; y < SH4DER_CLOUDVOLUME_NOISE; y++) {
            for (x = 0; x < SH4DER_CLOUDVOLUME_NOISE; x++) {
                const float fx = (float)x * ninv, fy = (float)y * ninv, fz = (float)z * ninv;
                const float n = sh4der_cloudvolume_billow_(sh4der_cloudvolume_lattice_(fx, fy, fz, 4u, seed + 7u)) * 0.6f +
                                sh4der_cloudvolume_billow_(sh4der_cloudvolume_lattice_(fx, fy, fz, 9u, seed + 8u)) * 0.4f;

                m->noise[(z * SH4DER_CLOUDVOLUME_NOISE + y) * SH4DER_CLOUDVOLUME_NOISE + x] = (uint8_t)(sh4der_sat_(n) * 255.0f + 0.5f);
            }
        }
    }
    for (y = 0; y < SH4DER_CLOUDVOLUME_COARSE; y++) {
        for (x = 0; x < SH4DER_CLOUDVOLUME_COARSE; x++) {
            unsigned best = 0u, i, j;

            for (j = 0; j < 6u; j++) {                          /* the cell's 4 x 4 texels and 1 more on every side (the blend reads a neighbour) */
                for (i = 0; i < 6u; i++) {
                    const unsigned tx = (x * 4u + i + SH4DER_CLOUDVOLUME_MAP - 1u) & (SH4DER_CLOUDVOLUME_MAP - 1u);
                    const unsigned ty = (y * 4u + j + SH4DER_CLOUDVOLUME_MAP - 1u) & (SH4DER_CLOUDVOLUME_MAP - 1u);
                    const unsigned hgt = m->cover[ty * SH4DER_CLOUDVOLUME_MAP + tx] & 255u;

                    best = hgt > best ? hgt : best;
                }
            }
            m->reach[y * SH4DER_CLOUDVOLUME_COARSE + x] = (uint8_t)best;
        }
    }
    return 1;
}

/* the cloud top 0 .. 255 of a height byte, for a coverage (threshold and 8.8 gain) */
static inline int32_t sh4der_cloudvolume_top_(int32_t h, int32_t thr, int32_t gain) {
    int32_t c = ((h - thr) * gain) >> 8;

    return c < 0 ? 0 : c > 255 ? 255 : c;
}

/* threshold and gain for a coverage */
static inline void sh4der_cloudvolume_cover_law_(float coverage, int32_t *thr, int32_t *gain) {
    const float t = 255.0f * (1.0f - coverage) * 0.72f;         /* the map's heights sit mostly in the middle of the range */

    *thr = (int32_t)t;
    *gain = (int32_t)(sh4der_div_(255.0f * 256.0f, 255.0f - t) * 1.45f);
}

/** \brief Bakes the shadow byte of rows y0 .. y1 - 1 of the cover map for the clouds' sun and coverage: the height
    (0 .. 255 of the slab) under which a point is in the shadow of cloud toward the sun. Call for the whole map at
    load and again, a few rows a frame, when the sun or the coverage changes. SH4DER_CLOUDVOLUME_BAKE_TAPS reads a texel.
    Returns 1, or 0 for a bad argument. */
static inline int sh4der_cloudvolume_bake(sh4der_cloudvolume_maps_t *m, const sh4der_cloudvolume_t *c, unsigned y0, unsigned y1) {
    const unsigned mask = SH4DER_CLOUDVOLUME_MAP - 1u;
    float hx, hz, hl, rise;
    int32_t thr, gain, dx, dz;
    unsigned x, y, k;

    if (!m || !sh4der_cloudvolume_valid(c) || y0 > y1 || y1 > SH4DER_CLOUDVOLUME_MAP) {
        return 0;
    }
    sh4der_cloudvolume_cover_law_(c->coverage, &thr, &gain);
    hl = sh4der_cloudvolume_sqrt_(sh4der_len2_(c->sun[0], c->sun[2]));
    if (!(hl > 1.0e-3f)) {                                      /* the sun overhead: every point's own top is its shadow height */
        hx = 0.0f;
        hz = 0.0f;
        rise = 255.0f;
    } else {
        const float texel = c->tile * (1.0f / (float)SH4DER_CLOUDVOLUME_MAP);
        const float stride = 1.25f;                             /* texels a tap */

        hx = sh4der_div_(c->sun[0], hl) * stride;
        hz = sh4der_div_(c->sun[2], hl) * stride;
        rise = sh4der_div_(c->sun[1], hl) * stride * texel * sh4der_div_(255.0f, c->thickness);   /* slab bytes the sun ray climbs a tap */
    }
    dx = (int32_t)(hx * 256.0f);
    dz = (int32_t)(hz * 256.0f);
    for (y = y0; y < y1; y++) {
        for (x = 0; x < SH4DER_CLOUDVOLUME_MAP; x++) {
            uint16_t *cell = &m->cover[y * SH4DER_CLOUDVOLUME_MAP + x];
            int32_t best = sh4der_cloudvolume_top_(*cell & 255, thr, gain);
            int32_t px = (int32_t)(x << 8) + 128, pz = (int32_t)(y << 8) + 128;
            float lift = 0.0f;

            for (k = 1u; k <= SH4DER_CLOUDVOLUME_BAKE_TAPS; k++) {
                int32_t t;

                px += dx;
                pz += dz;
                lift += rise;
                if (lift >= 255.0f) {
                    break;
                }
                t = sh4der_cloudvolume_top_(m->cover[(((uint32_t)pz >> 8) & mask) * SH4DER_CLOUDVOLUME_MAP + (((uint32_t)px >> 8) & mask)] & 255, thr, gain) - (int32_t)lift;
                best = t > best ? t : best;
            }
            *cell = (uint16_t)((*cell & 255u) | ((uint32_t)best << 8));
        }
    }
    return 1;
}

/** \brief Makes a frame's numbers from the clouds, a camera and a time in seconds. `phase` picks the jitter pattern's
    turn (the frame number). Returns 1, or 0 for a bad argument (the frame is then not usable). */
static inline int sh4der_cloudvolume_prepare(const sh4der_cloudvolume_t *c, const sh4der_cloudvolume_view_t *v, float time, uint32_t phase, sh4der_cloudvolume_frame_t *f) {
    float texel, inv_texel, sy, cy, sp, cp, r, lr;
    unsigned i;

    if (!c || !v || !f || !sh4der_cloudvolume_valid(c) || !sh4der_finite_(time) || !(sh4der_abs_(time) <= 1.0e6f)) {
        return 0;
    }
    if (!sh4der_finite_(v->eye[0]) || !sh4der_finite_(v->eye[1]) || !sh4der_finite_(v->eye[2]) || !sh4der_finite_(v->yaw) || !sh4der_finite_(v->pitch) ||
        !sh4der_finite_(v->tan_x) || !sh4der_finite_(v->tan_y) || !(v->tan_x > 0.01f) || !(v->tan_y > 0.01f) || !(v->tan_x < 8.0f) || !(v->tan_y < 8.0f) ||
        !(sh4der_abs_(v->eye[0]) <= 1.0e9f) || !(sh4der_abs_(v->eye[1]) <= 1.0e6f) || !(sh4der_abs_(v->eye[2]) <= 1.0e9f) ||
        !(sh4der_abs_(v->yaw) <= 1.0e4f) || !(sh4der_abs_(v->pitch) <= 1.5f)) {
        return 0;
    }
    texel = c->tile * (1.0f / (float)SH4DER_CLOUDVOLUME_MAP);
    inv_texel = sh4der_div_(1.0f, texel);

    /* the eye inside one tile (the maps repeat), in texels */
    f->o[0] = sh4der_frac_(sh4der_div_(v->eye[0], c->tile)) * (float)SH4DER_CLOUDVOLUME_MAP;
    f->o[1] = (v->eye[1] - c->base_y) * inv_texel;
    f->o[2] = sh4der_frac_(sh4der_div_(v->eye[2], c->tile)) * (float)SH4DER_CLOUDVOLUME_MAP;
    sh4der_sincos_(v->yaw, &sy, &cy);
    sh4der_sincos_(v->pitch, &sp, &cp);
    f->fwd[0] = sy * cp;  f->fwd[1] = sp;   f->fwd[2] = cy * cp;
    f->right[0] = cy;     f->right[1] = 0.0f; f->right[2] = -sy;
    f->up[0] = -sy * sp;  f->up[1] = cp;    f->up[2] = -cy * sp;
    f->sun[0] = c->sun[0]; f->sun[1] = c->sun[1]; f->sun[2] = c->sun[2];
    sh4der_norm3_(f->sun);
    f->tan_x = v->tan_x;
    f->tan_y = v->tan_y;
    f->th = c->thickness * inv_texel;
    f->near_t = SH4DER_CLOUDVOLUME_NEAR * inv_texel;
    f->far_t = c->far_m * inv_texel;
    f->yn_scale = sh4der_div_(255.0f, f->th);
    f->od_scale = c->density * texel * (32.0f / 255.0f);        /* alpha_tab index = optical depth x 32 */
    f->haze_scale = sh4der_div_(texel * 16.0f, c->haze);
    f->light_scale = c->shadow * (64.0f / 255.0f);
    f->tap_scale = c->density * texel * (64.0f / 255.0f) * 0.35f;   /* a light sample counts for less than a view sample: light scatters round */
    f->cover_off[0] = sh4der_frac_(sh4der_div_(time * c->wind[0], c->tile)) * (float)SH4DER_CLOUDVOLUME_MAP;
    f->cover_off[1] = sh4der_frac_(sh4der_div_(time * c->wind[1], c->tile)) * (float)SH4DER_CLOUDVOLUME_MAP;
    f->noise_off[0] = f->cover_off[0] * 0.6f;
    f->noise_off[1] = sh4der_frac_(sh4der_div_(time * c->churn, c->tile)) * (float)SH4DER_CLOUDVOLUME_MAP;
    f->noise_off[2] = f->cover_off[1] * 0.6f;
    sh4der_cloudvolume_cover_law_(c->coverage, &f->cover_thr, &f->cover_gain);
    f->erode = (int32_t)(c->erosion * 256.0f);
    f->steps = c->steps;
    f->light_samples = c->light_samples;
    f->detail = c->detail;
    f->jitter = c->jitter;
    f->phase = phase;

    /* step starts: each step r times the one before, the last SH4DER_CLOUDVOLUME_GROWTH times the first */
    r = sh4der_cloudvolume_root_(SH4DER_CLOUDVOLUME_GROWTH, SH4DER_CLOUDVOLUME_GROWTH_LN, c->steps - 1u);
    lr = 1.0f;
    f->u[0] = 0.0f;
    for (i = 0; i < c->steps; i++) {
        f->u[i + 1u] = f->u[i] + lr;
        lr *= r;
    }
    lr = sh4der_div_(1.0f, f->u[c->steps]);
    for (i = 1u; i <= c->steps; i++) {
        f->u[i] *= lr;
    }
    {   /* exp(-i / 32), exp(-i / 64), exp(-i / 16) as running products of the exact first step */
        float e32 = 1.0f, e64 = 1.0f, e16 = 1.0f;

        for (i = 0; i < 256u; i++) {
            f->alpha_tab[i] = (uint8_t)((1.0f - e32) * 255.0f + 0.5f);
            f->beer_tab[i] = (uint8_t)(e64 * 255.0f + 0.5f);
            e32 *= 0.96923323f;                                 /* exp(-1 / 32) */
            e64 *= 0.98449644f;                                 /* exp(-1 / 64) */
        }
        for (i = 0; i < 64u; i++) {
            const float cs = (float)i * (1.0f / 63.0f);         /* 0 away from the sun .. 1 straight at it */
            const float c2 = cs * cs, c8 = c2 * c2 * c2 * c2;

            f->haze_tab[i] = (uint8_t)(e16 * 255.0f + 0.5f);
            e16 *= 0.93941306f;                                 /* exp(-1 / 16) */
            f->phase_tab[i] = (uint8_t)((0.62f + 0.12f * cs + 0.26f * c8 * c8) * 255.0f + 0.5f);   /* a broad lobe and a tight one round the sun */
        }
    }
    f->rays = 0u;
    f->samples = 0u;
    f->skips = 0u;
    f->hits = 0u;
    f->noise_reads = 0u;
    return 1;
}

/* the cover map at (u, v), 16.16 texels: height byte and shadow byte, each blended between four texels (hand: fixed point) */
static inline void sh4der_cloudvolume_cover_(const sh4der_cloudvolume_maps_t *m, uint32_t u, uint32_t v, int32_t *height, int32_t *shadow) {
    const uint32_t mask = SH4DER_CLOUDVOLUME_MAP - 1u;
    const uint32_t x0 = (u >> 16) & mask, y0 = (v >> 16) & mask, x1 = (x0 + 1u) & mask, y1 = (y0 + 1u) & mask;
    const int32_t fx = (int32_t)((u >> 8) & 255u), fy = (int32_t)((v >> 8) & 255u);
    const uint32_t a = m->cover[y0 * SH4DER_CLOUDVOLUME_MAP + x0], b = m->cover[y0 * SH4DER_CLOUDVOLUME_MAP + x1];
    const uint32_t c = m->cover[y1 * SH4DER_CLOUDVOLUME_MAP + x0], d = m->cover[y1 * SH4DER_CLOUDVOLUME_MAP + x1];
    const int32_t h0 = (int32_t)(a & 255u) * 256 + ((int32_t)(b & 255u) - (int32_t)(a & 255u)) * fx;
    const int32_t h1 = (int32_t)(c & 255u) * 256 + ((int32_t)(d & 255u) - (int32_t)(c & 255u)) * fx;
    const int32_t s0 = (int32_t)(a >> 8) * 256 + ((int32_t)(b >> 8) - (int32_t)(a >> 8)) * fx;
    const int32_t s1 = (int32_t)(c >> 8) * 256 + ((int32_t)(d >> 8) - (int32_t)(c >> 8)) * fx;

    *height = (h0 * 256 + (h1 - h0) * fy) >> 16;
    *shadow = (s0 * 256 + (s1 - s0) * fy) >> 16;
}

/* the noise volume at (u, v, w), 16.16 voxels, blended between eight voxels (hand: fixed point) */
static inline int32_t sh4der_cloudvolume_noise_(const sh4der_cloudvolume_maps_t *m, uint32_t u, uint32_t v, uint32_t w) {
    const uint32_t mask = SH4DER_CLOUDVOLUME_NOISE - 1u, n = SH4DER_CLOUDVOLUME_NOISE;
    const uint32_t x0 = (u >> 16) & mask, y0 = (v >> 16) & mask, z0 = (w >> 16) & mask;
    const uint32_t x1 = (x0 + 1u) & mask, y1 = (y0 + 1u) & mask, z1 = (z0 + 1u) & mask;
    const int32_t fx = (int32_t)((u >> 8) & 255u), fy = (int32_t)((v >> 8) & 255u), fz = (int32_t)((w >> 8) & 255u);
    const uint8_t *r00 = &m->noise[(z0 * n + y0) * n], *r01 = &m->noise[(z0 * n + y1) * n];
    const uint8_t *r10 = &m->noise[(z1 * n + y0) * n], *r11 = &m->noise[(z1 * n + y1) * n];
    const int32_t a = r00[x0] * 256 + (r00[x1] - r00[x0]) * fx, b = r01[x0] * 256 + (r01[x1] - r01[x0]) * fx;
    const int32_t c = r10[x0] * 256 + (r10[x1] - r10[x0]) * fx, d = r11[x0] * 256 + (r11[x1] - r11[x0]) * fx;
    const int32_t ab = (a * 256 + (b - a) * fy) >> 8, cd = (c * 256 + (d - c) * fy) >> 8;

    return (ab * 256 + (cd - ab) * fz) >> 16;
}

/* a float position in texels as wrapping 16.16 (hand: the conversion; the value stays far inside the int range) */
static inline uint32_t sh4der_cloudvolume_fix_(float t) {
    return (uint32_t)(int32_t)(t * 65536.0f);
}

/* The density 0 .. 255 at a point (texels; y above the slab base). *depth gets the baked shadow depth 0 .. 255 above the point,
   *yn the height in the slab 0 .. 255. */
static inline int32_t sh4der_cloudvolume_density_(const sh4der_cloudvolume_maps_t *m, sh4der_cloudvolume_frame_t *f, float px, float py, float pz, int32_t *depth, int32_t *yn_out) {
    const int32_t yn = (int32_t)(py * f->yn_scale);
    int32_t h, s, top, d, base;
    uint32_t u, v;

    if (py < 0.0f || yn > 255) {
        return 0;
    }
    u = sh4der_cloudvolume_fix_(px + f->cover_off[0]);
    v = sh4der_cloudvolume_fix_(pz + f->cover_off[1]);
    if (yn >= sh4der_cloudvolume_top_(m->reach[((v >> 18) & (SH4DER_CLOUDVOLUME_COARSE - 1u)) * SH4DER_CLOUDVOLUME_COARSE + ((u >> 18) & (SH4DER_CLOUDVOLUME_COARSE - 1u))], f->cover_thr, f->cover_gain)) {
        f->skips++;                                             /* above every top near here */
        return 0;
    }
    f->samples++;
    sh4der_cloudvolume_cover_(m, u, v, &h, &s);
    top = sh4der_cloudvolume_top_(h, f->cover_thr, f->cover_gain);
    d = ((top << 4) - (int32_t)(py * f->yn_scale * 16.0f)) * 7 >> 4;   /* full a seventh of the slab under the top; the height at 12 bits so the edge has no steps */
    if (d <= 0) {
        return 0;
    }
    base = yn * 10;                                             /* a flat, quick base */
    d = d < base ? d : base;
    d = d > 255 ? 255 : d;
    if (f->detail && f->erode && d < 250) {                     /* deep inside, the carve cannot open the cloud: no read */
        /* the noise volume: 16 texels (1,024 m) across, then 4 texels (256 m) */
        const uint32_t nu = sh4der_cloudvolume_fix_(px + f->noise_off[0]), nv = sh4der_cloudvolume_fix_(py + f->noise_off[1]), nw = sh4der_cloudvolume_fix_(pz + f->noise_off[2]);
        int32_t n = sh4der_cloudvolume_noise_(m, nu << 1, nv << 1, nw << 1);

        f->noise_reads++;
        if (f->detail > 1u) {
            n = (n * 3 + sh4der_cloudvolume_noise_(m, (nu << 3) + 0x50000u, (nv << 3) + 0x90000u, nw << 3)) >> 2;
            f->noise_reads++;
        }
        d = ((d + (d >> 1)) * 256 - n * f->erode * 2) >> 8;     /* carve: the thin parts go first */
        if (d <= 0) {
            return 0;
        }
        d = d > 255 ? 255 : d;
    }
    s -= yn;
    *depth = s < 0 ? 0 : s;
    *yn_out = yn;
    return d;
}

/* The share of sunlight 0 .. 255 that reaches a point: baked depth, or light samples toward the sun. */
static inline int32_t sh4der_cloudvolume_light_(const sh4der_cloudvolume_maps_t *m, sh4der_cloudvolume_frame_t *f, float px, float py, float pz, int32_t depth) {
    static const float reach[SH4DER_CLOUDVOLUME_MAX_LIGHT + 1] = { 0.0f, 1.2f, 3.2f, 6.5f, 12.0f };   /* texels: 77, 205, 416, 768 m */
    float tau = 0.0f;
    unsigned k;
    int32_t i;

    if (!f->light_samples) {
        i = (int32_t)((float)depth * f->light_scale);
        return f->beer_tab[i > 255 ? 255 : i];
    }
    for (k = 1u; k <= f->light_samples; k++) {
        const float s = (reach[k - 1u] + reach[k]) * 0.5f;
        int32_t dd, yy;
        const int32_t d = sh4der_cloudvolume_density_(m, f, sh4der_fma_(f->sun[0], s, px), sh4der_fma_(f->sun[1], s, py), sh4der_fma_(f->sun[2], s, pz), &dd, &yy);

        tau = sh4der_fma_((float)d, reach[k] - reach[k - 1u], tau);
        if (k == f->light_samples && d > 0) {                   /* beyond the last sample: the bake's depth there */
            tau += (float)dd * sh4der_div_(f->light_scale, f->tap_scale) * 0.6f;
        }
    }
    i = (int32_t)(tau * f->tap_scale);
    return f->beer_tab[i > 255 ? 255 : i];
}

/* the ordered 4 x 4 pattern, 0 .. 15 */
static inline uint32_t sh4der_cloudvolume_bayer_(unsigned x, unsigned y) {
    static const uint8_t b[16] = { 0, 8, 2, 10, 12, 4, 14, 6, 3, 11, 1, 9, 15, 7, 13, 5 };

    return b[(x & 3u) | ((y & 3u) << 2)];
}

/** \brief Marches one buffer pixel: (x, y) of a w x h buffer (y 0 at the top). Writes its shade (0 shadow .. 255 sunlit)
    and its alpha (0 clear .. 255 solid). The frame's counters grow. */
static inline void sh4der_cloudvolume_pixel(const sh4der_cloudvolume_maps_t *m, sh4der_cloudvolume_frame_t *f, unsigned x, unsigned y, unsigned w, unsigned h, uint8_t *shade, uint8_t *alpha) {
    const float sx = (((float)x + 0.5f) * sh4der_div_(2.0f, (float)w) - 1.0f) * f->tan_x;
    const float sy = (1.0f - ((float)y + 0.5f) * sh4der_div_(2.0f, (float)h)) * f->tan_y;
    float d[3], len, t0, t1, cs;
    uint32_t trans = 65536u, s_acc = 0u, a_acc = 0u;            /* light still through (16.16), shade x alpha, alpha */
    int32_t gain;
    unsigned i;

    d[0] = f->fwd[0] + f->right[0] * sx + f->up[0] * sy;
    d[1] = f->fwd[1] + f->right[1] * sx + f->up[1] * sy;
    d[2] = f->fwd[2] + f->right[2] * sx + f->up[2] * sy;
    len = sh4der_cloudvolume_sqrt_(sh4der_len3_(d[0], d[1], d[2]));
    cs = sh4der_div_(sh4der_dot3_(d, f->sun), len);
    gain = f->phase_tab[(int32_t)(sh4der_sat_(cs) * 63.0f)];

    /* the ray's stretch inside the slab, as a distance along the view axis */
    if (sh4der_abs_(d[1]) < 1.0e-4f) {
        t0 = f->o[1] >= 0.0f && f->o[1] <= f->th ? f->near_t : f->far_t;
        t1 = f->far_t;
    } else {
        const float inv = sh4der_div_(1.0f, d[1]);
        const float a = -f->o[1] * inv, b = (f->th - f->o[1]) * inv;

        t0 = a < b ? a : b;
        t1 = a < b ? b : a;
        t0 = t0 < f->near_t ? f->near_t : t0;
        t1 = t1 > f->far_t ? f->far_t : t1;
    }
    if (t1 > t0) {
        const float range = t1 - t0;
        /* a 2 x 2 pattern of four starts that turns every frame: sh4der_cloudvolume_smooth's 1 2 1 weights average it away */
        const float jit = f->jitter ? (float)(((x & 1u) * 2u + ((x ^ y) & 1u) + f->phase) & 3u) * 0.25f + 0.125f : 0.5f;

        f->rays++;
        for (i = 0; i < f->steps; i++) {
            const float ua = f->u[i], ub = f->u[i + 1u];
            const float t = sh4der_fma_(range, sh4der_fma_(ub - ua, jit, ua), t0);
            const float px = sh4der_fma_(d[0], t, f->o[0]), py = sh4der_fma_(d[1], t, f->o[1]), pz = sh4der_fma_(d[2], t, f->o[2]);
            int32_t depth = 0, yn = 0;
            const int32_t dens = sh4der_cloudvolume_density_(m, f, px, py, pz, &depth, &yn);

            if (dens > 0) {
                const float path = range * (ub - ua) * len;     /* texels of ray this step stands for */
                int32_t ai = (int32_t)((float)dens * path * f->od_scale);
                int32_t hi = (int32_t)(t * len * f->haze_scale);
                const int32_t light = sh4der_cloudvolume_light_(m, f, px, py, pz, depth);
                uint32_t a, wgt, vis;
                int32_t sh, hz;

                f->hits++;

                a = f->alpha_tab[ai > 255 ? 255 : ai];
                wgt = (trans * a) >> 8;                         /* what this sample hides of what is left */
                hz = f->haze_tab[hi > 63 ? 63 : hi];            /* 255 near .. 0 far */
                vis = wgt;
                /* shade: sky light that grows with height, plus the sun through the cloud, stronger looking at the sun;
                   thin cloud (a low density) toward the sun lets more through */
                sh = 30 + (yn >> 2) + (((light + (((255 - light) * (255 - dens) * (gain - 100)) >> 16)) * gain * 3) >> 10);
                sh = sh < 0 ? 0 : sh > 255 ? 255 : sh;
                sh = SH4DER_CLOUDVOLUME_HAZE_SHADE + (((sh - SH4DER_CLOUDVOLUME_HAZE_SHADE) * hz) >> 8);   /* distance pulls every shade to the haze's */
                s_acc += (vis >> 4) * (uint32_t)sh;
                a_acc += vis;
                trans -= wgt;
                if (trans < (uint32_t)(SH4DER_CLOUDVOLUME_STOP * 65536.0f)) {
                    break;
                }
            }
        }
    }

    a_acc += a_acc >> 5;                                        /* the march stops with 3 % still through: count that as hidden */
    if (a_acc >= 256u) {
        *shade = (uint8_t)(sh4der_div_((float)s_acc * 16.0f, (float)a_acc));   /* hand: the byte */
        *alpha = (uint8_t)(a_acc >= 65536u ? 255u : a_acc >> 8);
    } else {
        *shade = 128u;
        *alpha = 0u;
    }
}

/** \brief Marches the pixels of a w x h buffer whose turn it is: `spread` 1 marches all, 2 a checkerboard half, 4 one
    pixel in four; `turn` counts frames. The other pixels keep what they hold. `shade` and `alpha` are w x h bytes.
    Returns the pixels marched, 0 for a bad argument. */
static inline unsigned sh4der_cloudvolume_march(const sh4der_cloudvolume_maps_t *m, sh4der_cloudvolume_frame_t *f, unsigned w, unsigned h, unsigned spread, unsigned turn, uint8_t *shade, uint8_t *alpha) {
    static const uint8_t order4[4] = { 0, 3, 1, 2 };
    unsigned x, y, n = 0;

    if (!m || !f || !shade || !alpha || w < 8u || h < 8u || w > 1024u || h > 1024u || (spread != 1u && spread != 2u && spread != 4u) || !f->steps) {
        return 0;
    }
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            if (spread == 2u && ((x + y + turn) & 1u)) {
                continue;
            }
            if (spread == 4u && ((x & 1u) | ((y & 1u) << 1)) != order4[turn & 3u]) {
                continue;
            }
            sh4der_cloudvolume_pixel(m, f, x, y, w, h, &shade[y * w + x], &alpha[y * w + x]);
            n++;
        }
    }
    return n;
}

/** \brief Softens a marched buffer into `out_shade` / `out_alpha` (w x h bytes each, not the inputs): weights 1 2 1 across
    and down, the shade weighted by the alpha so clear pixels lend no colour. It removes the 2 x 2 start pattern of the
    jitter and the checkerboard a spread leaves. About 30 cycles a pixel (ESTIMATE). Returns 1, or 0 for a bad argument. */
static inline int sh4der_cloudvolume_smooth(const uint8_t *shade, const uint8_t *alpha, unsigned w, unsigned h, uint8_t *out_shade, uint8_t *out_alpha) {
    unsigned x, y;

    if (!shade || !alpha || !out_shade || !out_alpha || w < 8u || h < 8u || w > 1024u || h > 1024u) {
        return 0;
    }
    for (y = 0; y < h; y++) {
        const unsigned ya = y ? y - 1u : y, yb = y + 1u < h ? y + 1u : y;
        const uint8_t *a0 = alpha + ya * w, *a1 = alpha + y * w, *a2 = alpha + yb * w;
        const uint8_t *s0 = shade + ya * w, *s1 = shade + y * w, *s2 = shade + yb * w;

        for (x = 0; x < w; x++) {
            const unsigned xa = x ? x - 1u : x, xb = x + 1u < w ? x + 1u : x;
            /* hand: integer weights 1 2 1 / 2 4 2 / 1 2 1 (sum 16) */
            const uint32_t wa[9] = { a0[xa], 2u * a0[x], a0[xb], 2u * a1[xa], 4u * a1[x], 2u * a1[xb], a2[xa], 2u * a2[x], a2[xb] };
            const uint32_t at = wa[0] + wa[1] + wa[2] + wa[3] + wa[4] + wa[5] + wa[6] + wa[7] + wa[8];
            const uint32_t st = wa[0] * s0[xa] + wa[1] * s0[x] + wa[2] * s0[xb] + wa[3] * s1[xa] + wa[4] * s1[x] + wa[5] * s1[xb] +
                                wa[6] * s2[xa] + wa[7] * s2[x] + wa[8] * s2[xb];

            out_alpha[y * w + x] = (uint8_t)((at + 8u) >> 4);
            out_shade[y * w + x] = (uint8_t)(at ? st / at : s1[x]);
        }
    }
    return 1;
}

/** \brief The cloud colour of a shade 0 .. 255, each channel 0 .. 1: cool blue-grey in shadow, through a soft grey, to a warm white in the sun. */
static inline void sh4der_cloudvolume_rgb(unsigned shade, float rgb[3]) {
    static const float lo[3] = { 0.38f, 0.43f, 0.57f }, mid[3] = { 0.70f, 0.72f, 0.81f }, hi[3] = { 1.00f, 0.96f, 0.88f };
    const float s = (float)(shade > 255u ? 255u : shade) * (1.0f / 255.0f);
    unsigned k;

    for (k = 0; k < 3u; k++) {
        rgb[k] = s < 0.5f ? sh4der_lerp_(lo[k], mid[k], s + s) : sh4der_lerp_(mid[k], hi[k], s + s - 1.0f);
    }
}

/** \brief Packs a shade and an alpha into a palette index for buffer pixel (x, y). The 256 entries are spent where the eye
    needs them: index 0 is clear; 1 .. 156 are 13 alphas (1/14 .. 13/14) x 12 shades for the soft edges; 160 .. 255 are 96
    shades of solid cloud. Both values are rounded through the ordered pattern so the levels do not band. */
static inline uint8_t sh4der_cloudvolume_index(unsigned shade, unsigned alpha, unsigned x, unsigned y) {
    const uint32_t b = sh4der_cloudvolume_bayer_(x, y);                 /* 0 .. 15 */
    uint32_t a = (alpha * 14u * 16u + ((b * 11u + 5u) & 15u) * 255u + 127u) / (255u * 16u);
    uint32_t s;

    a = alpha == 0u ? 0u : alpha >= 250u ? 14u : a > 14u ? 14u : a;
    if (a == 0u) {
        return 0u;
    }
    if (a == 14u) {
        s = (shade * 95u * 16u + b * 255u + 127u) / (255u * 16u);
        return (uint8_t)(160u + (s > 95u ? 95u : s));
    }
    s = (shade * 11u * 16u + b * 255u + 127u) / (255u * 16u);
    return (uint8_t)(1u + (a - 1u) * 12u + (s > 11u ? 11u : s));
}

/** \brief Fills the 256 ARGB8888 palette entries sh4der_cloudvolume_index points into (157 .. 159 are unused: clear). */
static inline void sh4der_cloudvolume_palette(uint32_t pal[256]) {
    unsigned i;

    if (!pal) {
        return;
    }
    for (i = 0; i < 256u; i++) {
        float rgb[3];
        unsigned a = 0u, sh = 128u;

        if (i >= 160u) {
            a = 255u;
            sh = ((i - 160u) * 255u + 47u) / 95u;
        } else if (i >= 1u && i <= 156u) {
            a = (((i - 1u) / 12u + 1u) * 255u + 7u) / 14u;
            sh = (((i - 1u) % 12u) * 255u + 5u) / 11u;
        }
        sh4der_cloudvolume_rgb(sh, rgb);
        pal[i] = a << 24 | (uint32_t)(rgb[0] * 255.0f + 0.5f) << 16 | (uint32_t)(rgb[1] * 255.0f + 0.5f) << 8 | (uint32_t)(rgb[2] * 255.0f + 0.5f);
    }
}

/** \brief A flight path from a time in seconds, no state: forward along +z at `speed` metres a second, a slow weave,
    a height that dips into the cloud tops and climbs out again, a bank with the turn (`bank` 1, or 0 for a level horizon). */
static inline void sh4der_cloudvolume_path(const sh4der_cloudvolume_t *c, float time, float speed, int bank, sh4der_cloudvolume_view_t *v) {
    float s1, c1, s2, c2, s3, c3;

    if (!c || !v || !sh4der_finite_(time) || !sh4der_finite_(speed)) {
        return;
    }
    sh4der_sincos_(time * 0.13f, &s1, &c1);
    sh4der_sincos_(time * 0.42f + 3.9f, &s2, &c2);
    sh4der_sincos_(time * 0.071f + 1.0f, &s3, &c3);
    v->eye[0] = 700.0f * s1 + 300.0f * s3;
    v->eye[2] = time * speed;
    v->eye[1] = c->base_y + c->thickness * (0.70f + 0.20f * s2 + 0.05f * s3);
    v->yaw = 0.16f * c1 + 0.05f * c3;                           /* looks where the weave goes */
    v->pitch = -0.07f + 0.035f * c2;
    v->roll = bank ? 0.085f * s1 + 0.02f * s3 : 0.0f;           /* about 5 degrees at the most */
    (void)c2;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_CLOUDVOLUME_H */
