/** \file    sh4der_ca.h
    \brief   SH4DER-PVR: the frame texture, and chromatic aberration through it.

    The PVR cannot read the frame it is drawing, but it can draw a frame into
    a texture and then draw that texture. This module holds the texture and
    the first effect made with it:

    - The frame texture: the world is drawn into a render target, 640 x 480
      or 320 x 240 (KOS pvr_scene_begin_rtt with a size, a stride texture),
      then put on screen as one quad. A game's HUD goes on after, at full
      sharpness. 640 x 480 holds 640 KB of video memory, 320 x 240 holds
      160 KB (whole tile rows: 640 x 512 and 320 x 256 texels of 2 bytes).
    - Chromatic aberration: the texture is put on screen three times over a
      small mesh, once per colour channel (green opaque, red and blue added),
      each sampled at its own scale about the screen's centre. The difference
      grows with the square of the distance from the centre: none in the
      middle, SH4DER_CA_SPLIT_PX x strength pixels between red and blue in
      the corner. Every sample stays inside the picture, so no edge clamps.
    - No memory, no effect: sh4der_ca_setup() that cannot get its textures
      gives back what it took, returns 0 and says how many KB were short
      (sh4der_ca_gpu_t.short_kb), for one line on a log. Nothing else fails.

    Order, per frame: sh4der_ca_begin (the world is drawn into the texture),
    then sh4der_ca_quad, or the three sh4der_ca_channel calls for the
    aberration, then the HUD on the screen.

    Cost: the world is drawn once (into the texture); the screen pass is one
    quad, or with CA three meshes of SH4DER_CA_MESH_X x SH4DER_CA_MESH_Y
    cells. On the SH-4: 3 x 63 vertices, a multiply and an add each.

    The maths is a line per vertex; the square root of the half diagonal is
    SH4ZAM's (shz_sqrtf), once a mesh.

    Recommended settings (the author's own game and demo disc):
    - Strength 1.00: red and blue 3 pixels apart in the corner. Useful range
      0.25 .. 4.00 (12 pixels: a deliberate, damaged-lens look).
    - 640 x 480 for the aberration alone. 320 x 240 when a temporal blend
      (sh4der_taa.h) rides on it, or when video memory is short.
    - Off by default, and off without complaint when the memory is not
      there: print short_kb once, not every frame.
    - Draw a HUD after the three channels, at full sharpness.

    \author Jilted Valkyrie
    \author Cortana (Claude, an AI by Anthropic): the effects' maths, the PVR paths, the tests
*/

#ifndef SH4DER_CA_H
#define SH4DER_CA_H

#include <stdint.h>
#include "sh4der_core.h"
#ifdef SH4DER_CORE_NO_SH4ZAM
#include <math.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** \name Shipped tuning
    @{ */
#define SH4DER_CA_SPLIT_PX  3.0f    /**< \brief Strength 1: red and blue this many screen pixels apart in the corner. */
#define SH4DER_CA_MESH_X    8       /**< \brief Cells across a channel's mesh. */
#define SH4DER_CA_MESH_Y    6       /**< \brief Cells down a channel's mesh. */
#define SH4DER_CA_TEXTURES  2       /**< \brief Most frame textures held (two: a history for a temporal blend). */
/** @} */

/** \name The colour channels
    @{ */
#define SH4DER_CA_RED       0       /**< \brief Pushed outward most (added). */
#define SH4DER_CA_GREEN     1       /**< \brief Pushed outward half as far (the opaque base). */
#define SH4DER_CA_BLUE      2       /**< \brief Not moved (added). */
/** @} */

/** \brief A frame texture's size. */
typedef struct sh4der_ca_size {
    unsigned w, h;              /**< \brief What is rendered: 640 x 480 or 320 x 240. */
    unsigned tex_w, tex_h;      /**< \brief The power-of-two size its header names: 1024 x 512 or 512 x 256. */
    float scale;                /**< \brief w over the screen's 640: what the world's screen positions are multiplied by. */
} sh4der_ca_size_t;

/** \brief The size for full (half 0) or half (half not 0) resolution. */
static inline void sh4der_ca_size(int half, sh4der_ca_size_t *s) {
    s->w = half ? 320u : 640u;
    s->h = half ? 240u : 480u;
    s->tex_w = half ? 512u : 1024u;
    s->tex_h = half ? 256u : 512u;
    s->scale = half ? 0.5f : 1.0f;
}

/** \brief Video memory one frame texture holds, in bytes: w texels a row (the stride), tex_h rows (whole tile rows), RGB565. */
static inline unsigned sh4der_ca_bytes(const sh4der_ca_size_t *s) { return s->w * s->tex_h * 2u; }

/** \brief How many KB are short when `need` bytes are wanted and `have` are free (0: it fits). Rounded up. */
static inline unsigned sh4der_ca_short_kb(unsigned need, unsigned have) { return need > have ? (need - have + 1023u) / 1024u : 0u; }

static inline float sh4der_ca_sqrt_(float x) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_sqrtf(x);
#else
    return sqrtf(x);
#endif
}

/** \brief A channel's push for a screen of w x h pixels at `strength` (1 = the shipped split): the k in scale = 1 - k x r2.
    Red's corner shift is 2 k x the half diagonal = SPLIT_PX x strength pixels; green's half of it; blue's none. Once a mesh. */
static inline float sh4der_ca_push(int chan, float strength, float w, float h) {
    const float mult = chan == SH4DER_CA_RED ? 2.0f : chan == SH4DER_CA_GREEN ? 1.0f : 0.0f;
    const float half_diag = 0.5f * sh4der_ca_sqrt_(w * w + h * h);
    if (!(half_diag > 0.0f) || !(strength > 0.0f))
        return 0.0f;
    return sh4der_div_(mult * SH4DER_CA_SPLIT_PX * strength, 2.0f * half_diag);
}

/** \brief The squared distance of a screen position (cu, cv, 0..1) from the centre, 1 in the corner. `inv_diag2` = 4 / (w^2 + h^2), once a mesh. */
static inline float sh4der_ca_r2(float cu, float cv, float w, float h, float inv_diag2) {
    const float dx = (cu - 0.5f) * w, dy = (cv - 0.5f) * h;
    return sh4der_len2_(dx, dy) * inv_diag2;
}

/** \brief Where a screen position (cu, cv, 0..1) samples the picture (0..1 across it) for a push k: about the centre, scale 1 - k x r2.
    The scale is never above 1, so the sample never leaves the picture. */
static inline void sh4der_ca_sample(float k, float r2, float cu, float cv, float *pu, float *pv) {
    const float s = 1.0f - k * r2;
    *pu = 0.5f + (cu - 0.5f) * s;
    *pv = 0.5f + (cv - 0.5f) * s;
}

/** \brief A picture position (0..1) as a texture coordinate of the frame texture: texel centres, so a filtered sample never reads past the picture. */
static inline void sh4der_ca_uv(const sh4der_ca_size_t *s, float pu, float pv, float *u, float *v) {
    *u = sh4der_div_(0.5f + sh4der_sat_(pu) * ((float)s->w - 1.0f), (float)s->tex_w);
    *v = sh4der_div_(0.5f + sh4der_sat_(pv) * ((float)s->h - 1.0f), (float)s->tex_h);
}

/** \brief A channel's vertex colour (ARGB8888): full on that channel, zero on the other two, so the texel passes only that channel. */
static inline uint32_t sh4der_ca_argb(int chan) {
    return chan == SH4DER_CA_RED ? 0xffff0000u : chan == SH4DER_CA_GREEN ? 0xff00ff00u : 0xff0000ffu;
}

#ifdef _arch_dreamcast
/* ------------------------------------------------------------------------ */
/* The Dreamcast side (src/sh4der_ca.c): the textures, the two passes. */
#include <dc/pvr.h>

/** \brief The frame textures a client holds. Zero it before the first sh4der_ca_setup(). */
typedef struct sh4der_ca_gpu {
    pvr_ptr_t tex[SH4DER_CA_TEXTURES];  /**< \brief The render targets; NULL when not held. */
    unsigned n;                         /**< \brief How many are held. */
    int half;                           /**< \brief 1: 320 x 240. */
    sh4der_ca_size_t size;              /**< \brief Their size. */
    unsigned short_kb;                  /**< \brief After a setup that failed: the KB of video memory that were missing. */
} sh4der_ca_gpu_t;

/** \brief Hold `n` (0..SH4DER_CA_TEXTURES) frame textures at full or half size. Textures already held at another size or
    count are given back first. Sets the PVR's one stride register (every stride texture of the client shares it).
    Returns 1 when all are held. Returns 0 when the video memory is short: nothing is held, `short_kb` says by how much. */
int sh4der_ca_setup(sh4der_ca_gpu_t *g, int half, unsigned n);

/** \brief Give every texture back. */
void sh4der_ca_shutdown(sh4der_ca_gpu_t *g);

/** \brief Begin the scene that draws the world into texture `i` (in place of pvr_scene_begin). The client multiplies its
    screen positions by size.scale. Returns 0, or -1 when texture `i` is not held (nothing begun: draw to the screen). */
int sh4der_ca_begin(const sh4der_ca_gpu_t *g, unsigned i);

/** \brief Texture `i` as one quad over the screen moved by (dx, dy) pixels, the whole drawn at `scale` (1 to the screen,
    size.scale into another frame texture). `alpha` 1: opaque, in the opaque list. Under 1: blended at that share, in the
    translucent list (a temporal blend). Filtered at half size, 1 : 1 at full size. */
void sh4der_ca_quad(const sh4der_ca_gpu_t *g, unsigned i, float alpha, float dx, float dy, float z, float scale);

/** \brief One colour channel of texture `i` over a w x h screen at `strength`. SH4DER_CA_GREEN goes in the opaque list
    (the base); SH4DER_CA_RED and SH4DER_CA_BLUE in the translucent list (added). The vertices by KOS direct render. */
void sh4der_ca_channel(const sh4der_ca_gpu_t *g, unsigned i, int chan, float strength, float w, float h, float z);

#endif /* _arch_dreamcast */

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_CA_H */
