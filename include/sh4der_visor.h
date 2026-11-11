/* SH4DER-PVR: visor modes (night vision, thermal, amber)

   sh4der_visor.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_visor.h
    \brief   The visor look: a tint, grain, bloom, vignette and scanlines,
             as one description, with a fade in and out.

    A mode is a description (sh4der_visor_desc_t) with named parts:
    - grade: a per-channel line (gain, lift, contrast, exposure), laid over the
      frame by sh4der_look_solve (the look's layers, the same ones the eye draws).
    - grain: a multiply noise tile (sh4der_visor_noise_grid), scrolled per frame.
    - bloom: an amount only. The client passes it to its bloom pass; this
      header does not draw it.
    - vignette: a dark feathered disc mesh (the focus mesh, black, alpha scaled).
    - scanline: a multiply tile of dark rows (sh4der_visor_scan_grid).

    THERMAL cannot be a per-channel line. A thermal ramp (cold blue, red,
    white-hot) maps luma to colour, and a per-channel line sees each channel
    on its own. So THERMAL draws a CPU map instead: the grid sh4der_look reads
    (80 x 60 RGB565) through a 64-entry luma LUT (sh4der_visor_lut) into a
    128 x 64 texture, drawn full screen over the frame with the fade as its
    alpha. The map is 80 x 60 samples of the previous frame, the coarse grid a
    thermal camera has. sh4der_visor_fit_ramp gives the closest affine
    (per-channel) version for comparison; the host test prints its error.

    Fade: sh4der_visor_fade eases an amount 0..1 with smoothstep over
    SH4DER_VISOR_FADE_IN_S (in) or SH4DER_VISOR_FADE_OUT_S (out).

    Order, per frame: sh4der_visor_fade for the amount, sh4der_visor_grade and
    sh4der_look_solve for the grade layers, then the draws for the parts the
    mode uses (a fade of 0 skips them).

    Version: v0.10
    The effect: shows the world as through a device: green night vision,
      amber, or a heat picture, with grain and scanlines, fading in and out.
    Developed for: Halo: Dreamcast Evolved.
    Use it for: night-vision goggles, a thermal scope, a security-camera or
      old-monitor look, a robot's point of view.

    \author Jilted Valkyrie
    \author Cortana (Claude, an AI by Anthropic): the effects' maths, the PVR paths, the tests
*/

#ifndef SH4DER_VISOR_H
#define SH4DER_VISOR_H

#include <stdint.h>
#include "sh4der_core.h"
#include "sh4der_look.h"
#include "sh4der_focus.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_VISOR_FADE_IN_S    0.25f   /**< \brief Seconds from off to on. */
#define SH4DER_VISOR_FADE_OUT_S   0.40f   /**< \brief Seconds from on to off. */
#define SH4DER_VISOR_LUT_BINS     SH4DER_LOOK_BINS   /**< \brief Thermal LUT entries (luma bins). */
#define SH4DER_VISOR_GRAIN_TEX    32      /**< \brief Grain tile, pixels (power of 2). */
#define SH4DER_VISOR_GRAIN_SPEED_U 0.37f  /**< \brief Grain scroll per frame, tile widths (U). */
#define SH4DER_VISOR_GRAIN_SPEED_V 0.61f  /**< \brief Grain scroll per frame, tile widths (V). */
#define SH4DER_VISOR_SCAN_TEX     8       /**< \brief Scanline tile, texels (power of 2). */
#define SH4DER_VISOR_SCAN_PX      2       /**< \brief Pixels per scanline texel row. */
#define SH4DER_VISOR_VIG_R        280.0f  /**< \brief Vignette: clear disc radius, pixels. */
#define SH4DER_VISOR_VIG_FEATHER  200.0f  /**< \brief Vignette falloff, pixels. */
#define SH4DER_VISOR_Z            1.0f    /**< \brief Depth for the overlays (the list's sort value is the client's). */
/** @} */

/** \name Sizes
    @{ */
#define SH4DER_VISOR_GRAIN_BYTES  (SH4DER_VISOR_GRAIN_TEX * SH4DER_VISOR_GRAIN_TEX * 2)   /**< \brief 2048 B. */
#define SH4DER_VISOR_SCAN_BYTES   (SH4DER_VISOR_SCAN_TEX * SH4DER_VISOR_SCAN_TEX * 2)     /**< \brief 128 B. */
#define SH4DER_VISOR_LUT_BYTES    (SH4DER_VISOR_LUT_BINS * 2)                             /**< \brief 128 B RAM. */
/** @} */

enum {
    SH4DER_VISOR_OFF,
    SH4DER_VISOR_NIGHT,     /**< \brief Green phosphor night vision. */
    SH4DER_VISOR_THERMAL,   /**< \brief Luma through a cold-to-white-hot ramp (CPU map). */
    SH4DER_VISOR_AMBER,     /**< \brief Amber monochrome, low light. */
    SH4DER_VISOR_MODES
};

/** \brief Thermal ramp stops: cold, mid, hot, each RGB 0..1, at luma 0, 0.5 and 1. */
static const float sh4der_visor_thermal_stops[3][3] = {
    { 0.02f, 0.02f, 0.20f },
    { 0.95f, 0.25f, 0.05f },
    { 1.00f, 0.95f, 0.80f }
};

/** \brief A visor mode, as its named parts. */
typedef struct sh4der_visor_desc {
    float gain[3], lift[3];   /**< \brief Grade: per channel line (see sh4der_look_grade_t). */
    float contrast, exposure;   /**< \brief Grade: contrast about 0.5, exposure. See sh4der_look_grade_t. */
    float grain;              /**< \brief Grain depth, 0..1 (baked into the tile). */
    float bloom;              /**< \brief Bloom amount, 0..1 (the client's bloom pass). */
    float vignette;           /**< \brief Vignette strength, 0..1. */
    float scanline;           /**< \brief Scanline depth, 0..1 (baked into the tile). */
    int thermal;              /**< \brief 1: the CPU map path (the grade is unused). */
} sh4der_visor_desc_t;

/** \brief The shipped presets. THERMAL's gain and lift are filled by sh4der_visor_preset (the fit). */
static const sh4der_visor_desc_t sh4der_visor_presets[SH4DER_VISOR_MODES] = {
    /* OFF      */ { { 1, 1, 1 }, { 0, 0, 0 }, 1.0f, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0 },
    /* NIGHT    */ { { 0.30f, 1.00f, 0.35f }, { 0.00f, 0.05f, 0.00f }, 1.20f, 1.10f, 0.25f, 0.35f, 0.45f, 0.15f, 0 },
    /* THERMAL  */ { { 1, 1, 1 }, { 0, 0, 0 }, 1.00f, 1.00f, 0.10f, 0.25f, 0.30f, 0.00f, 1 },
    /* AMBER    */ { { 1.00f, 0.62f, 0.10f }, { 0.02f, 0.01f, 0.00f }, 1.10f, 0.95f, 0.20f, 0.20f, 0.40f, 0.25f, 0 }
};

/** \brief The thermal ramp at luma `l` (0..1), one channel, piecewise linear over the three stops. */
static inline float sh4der_visor_ramp_(int c, float l) {
    if (l <= 0.5f)
        return sh4der_lerp_(sh4der_visor_thermal_stops[0][c], sh4der_visor_thermal_stops[1][c], l * 2.0f);
    return sh4der_lerp_(sh4der_visor_thermal_stops[1][c], sh4der_visor_thermal_stops[2][c], (l - 0.5f) * 2.0f);
}

/** \brief Least-squares line per channel, out = gain x in + lift, fitted over 17 luma samples of the ramp. The closest a
    per-channel line gets to a luma ramp. */
static inline void sh4der_visor_fit_ramp(float gain[3], float lift[3]) {
    int c, k;
    for (c = 0; c < 3; c++) {
        float sx = 0.0f, sy = 0.0f, sxx = 0.0f, sxy = 0.0f;
        const float n = 17.0f;
        for (k = 0; k <= 16; k++) {
            const float l = (float)k * (1.0f / 16.0f);
            const float y = sh4der_visor_ramp_(c, l);
            sx += l; sy += y; sxx += l * l; sxy += l * y;
        }
        gain[c] = sh4der_div_(n * sxy - sx * sy, n * sxx - sx * sx);
        lift[c] = sh4der_div_(sy - gain[c] * sx, n);
    }
}

/** \brief A mode's description: the table, with THERMAL's grade from the fit. Returns 0, or -1 for a bad mode. */
static inline int sh4der_visor_preset(int mode, sh4der_visor_desc_t *d) {
    if (mode <= 0 || mode >= SH4DER_VISOR_MODES)
        return -1;
    *d = sh4der_visor_presets[mode];
    if (d->thermal)
        sh4der_visor_fit_ramp(d->gain, d->lift);
    return 0;
}

/** \brief The grade for `amount` (0 off, 1 full): each part lerped from neutral. */
static inline void sh4der_visor_grade(const sh4der_visor_desc_t *d, float amount, sh4der_look_grade_t *g) {
    int c;
    for (c = 0; c < 3; c++) {
        g->gain[c] = sh4der_lerp_(1.0f, d->gain[c], amount);
        g->lift[c] = sh4der_lerp_(0.0f, d->lift[c], amount);
    }
    g->contrast = sh4der_lerp_(1.0f, d->contrast, amount);
    g->exposure = sh4der_lerp_(1.0f, d->exposure, amount);
}

/** \brief Thermal LUT: luma bin (sh4der_look_luma565) to RGB565 of the ramp at the bin's centre. */
static inline void sh4der_visor_lut(uint16_t lut[SH4DER_VISOR_LUT_BINS]) {
    int i, c;
    for (i = 0; i < SH4DER_VISOR_LUT_BINS; i++) {
        const float l = ((float)i + 0.5f) * (1.0f / SH4DER_VISOR_LUT_BINS);
        float v[3];
        for (c = 0; c < 3; c++)
            v[c] = sh4der_sat_(sh4der_visor_ramp_(c, l));
        lut[i] = (uint16_t)(((unsigned)(v[0] * 31.0f + 0.5f) << 11) | ((unsigned)(v[1] * 63.0f + 0.5f) << 5) |
                            (unsigned)(v[2] * 31.0f + 0.5f));
    }
}

/** \brief The thermal map: the grid `src` (SRC_W x SRC_H RGB565) through the LUT into `dst` (TEX_W x TEX_H), padded. */
static inline void sh4der_visor_thermal_grid(const uint16_t lut[SH4DER_VISOR_LUT_BINS], const uint16_t *src, uint16_t *dst) {
    unsigned x, y;
    for (y = 0; y < SH4DER_FOCUS_SRC_H; y++)
        for (x = 0; x < SH4DER_FOCUS_SRC_W; x++)
            dst[y * SH4DER_FOCUS_TEX_W + x] = lut[sh4der_look_luma565(src[y * SH4DER_FOCUS_SRC_W + x])];
    sh4der_focus_pad_(dst);
}

/** \brief Grain tile (SH4DER_VISOR_GRAIN_TEX square, RGB565, multiply): value 1 - amp x r, r from a fixed LCG (the same tile
    every load). */
static inline void sh4der_visor_noise_grid(uint16_t *dst, float amp) {
    unsigned i;
    uint32_t seed = 0x2545f491u;
    const unsigned n = SH4DER_VISOR_GRAIN_TEX * SH4DER_VISOR_GRAIN_TEX;
    for (i = 0; i < n; i++) {
        float v, r;
        seed = seed * 1664525u + 1013904223u;
        r = (float)(seed >> 8) * (1.0f / 16777216.0f);
        v = sh4der_sat_(1.0f - amp * r);
        dst[i] = (uint16_t)(((unsigned)(v * 31.0f + 0.5f) << 11) | ((unsigned)(v * 63.0f + 0.5f) << 5) | (unsigned)(v * 31.0f + 0.5f));
    }
}

/** \brief Scanline tile (SH4DER_VISOR_SCAN_TEX square, multiply): the top two texel rows at 1 - depth, the rest white. The
    strength is baked: a fade skips the pass (the client's rule). */
static inline void sh4der_visor_scan_grid(uint16_t *dst, float depth) {
    unsigned x, y;
    for (y = 0; y < SH4DER_VISOR_SCAN_TEX; y++) {
        const float v = y < 2 ? sh4der_sat_(1.0f - depth) : 1.0f;
        const uint16_t p = (uint16_t)(((unsigned)(v * 31.0f + 0.5f) << 11) | ((unsigned)(v * 63.0f + 0.5f) << 5) | (unsigned)(v * 31.0f + 0.5f));
        for (x = 0; x < SH4DER_VISOR_SCAN_TEX; x++)
            dst[y * SH4DER_VISOR_SCAN_TEX + x] = p;
    }
}

/** \brief The vignette's alpha: the focus vertex's alpha (1 - sharp) scaled by strength x amount; colour black. */
static inline uint32_t sh4der_visor_vignette_argb(uint32_t focus_argb, float strength, float amount) {
    const float a = (float)(focus_argb >> 24) * (1.0f / 255.0f) * sh4der_sat_(strength) * sh4der_sat_(amount);
    return (uint32_t)(sh4der_sat_(a) * 255.0f + 0.5f) << 24;
}

/** \brief The fade envelope: t 0..1 eases toward on (1) or off (0) over the in / out time; returns the smoothstep of t. */
typedef struct sh4der_visor_fade {
    float t;
} sh4der_visor_fade_t;

/** \brief One frame of the fade: `on` 1 eases toward on, 0 toward off, over `dt` seconds. Returns the smoothstep of t (0..1). */
static inline float sh4der_visor_fade(sh4der_visor_fade_t *f, int on, float dt) {
    if (on)
        f->t = sh4der_sat_(f->t + sh4der_div_(dt, SH4DER_VISOR_FADE_IN_S));
    else
        f->t = sh4der_sat_(f->t - sh4der_div_(dt, SH4DER_VISOR_FADE_OUT_S));
    return f->t * f->t * (3.0f - 2.0f * f->t);
}

#ifdef _arch_dreamcast
/* KOS draw code (src/sh4der_visor.c). */
#include <dc/pvr.h>

typedef struct sh4der_visor_gpu {
    sh4der_focus_gpu_t thermal;      /**< \brief The thermal map (the focus texture type). */
    pvr_poly_hdr_t layer[4];         /**< \brief One untextured header per look layer kind (MULTIPLY, BOOST, ADD, INVERT). */
    pvr_ptr_t grain_tex, scan_tex;   /**< \brief Grain tile and scanline tile, linear RGB565. */
    pvr_poly_hdr_t grain_hdr, scan_hdr, vig_hdr;
} sh4der_visor_gpu_t;

/** \brief Allocate and compile everything; 0 or -1 (no VRAM). The LUT and grain tile are the caller's to fill and upload. */
int sh4der_visor_init(sh4der_visor_gpu_t *g);
/** \brief The grade's layers, full screen, in the TR list (between pvr_list_begin / end). */
void sh4der_visor_draw_grade(const sh4der_visor_gpu_t *g, const sh4der_look_layer_t *l, int n);
/** \brief The thermal map over the frame with `amount` as alpha. */
void sh4der_visor_draw_thermal(const sh4der_visor_gpu_t *g, float amount);
/** \brief Grain, scrolled by frame, multiplied over the frame. */
void sh4der_visor_draw_grain(const sh4der_visor_gpu_t *g, unsigned frame);
/** \brief Scanlines, multiplied over the frame. */
void sh4der_visor_draw_scanline(const sh4der_visor_gpu_t *g);
/** \brief Vignette: the dark disc mesh, strength x amount. */
void sh4der_visor_draw_vignette(const sh4der_visor_gpu_t *g, float strength, float amount);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_VISOR_H */
