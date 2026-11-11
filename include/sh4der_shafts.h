/** \file    sh4der_shafts.h
    \brief   SH4DER-PVR: shadow shafts, the dark shapes of the frame cast away from a light.

    The glow's god rays are the frame's bright parts stretched away from the
    sun. These are the other half: the frame's DARK parts cast a darker copy
    of themselves, pushed away from the light, so things standing between
    the camera and the light seem to throw shade toward the viewer.

    - The read: the frame on a grid (sh4der_frame_read, 80 x 60 RGB565).
    - The shade map: a cell darker than SH4DER_SHAFTS_DARK_FROM casts, fully
      from SH4DER_SHAFTS_DARK_FULL down; the cast counts only near the light
      on screen (full within NEAR screen heights of it, gone at FAR) and only
      as the camera turns toward the light (sh4der_shafts_toward). A texel is
      what the frame under the pushed copy is multiplied by: white = no
      shade. Each upload moves a texel half way to its new value: the read
      is a frame or two old and carries its own shade, so it must not
      flicker or creep.
    - The draw: the map over the screen, multiplied (texel x frame), its
      texture coordinates scaled about the light so the copy lies further
      from the light than what cast it (sh4der_glow_ray_uv, per vertex).
    - Order, per frame: sh4der_shafts_map on the read, then sh4der_shafts_upload,
      then sh4der_shafts_draw. The light's screen point (su, sv) goes to the
      map and the draw; the draw's scale is sh4der_shafts_scale(strength).
    - No light in view: the client clears the map once and draws nothing.

    Cost: the map and its texture are the glow's size, 128 x 64 RGB565 =
    16 KB of video memory and 16 KB of RAM; 4800 cells a frame on the SH-4
    (a luma, a distance, a smooth step); one mesh of 8 x 6 cells on the PVR.

    The square root is SH4ZAM's (shz_sqrtf_fsrra: the argument is never 0);
    the smooth step is SH4DOW-DC's sh4dow_smooth01, the one both libraries
    share.

    Version: v0.50
    The effect: makes things that stand between the camera and a light seem to
      throw shade toward the viewer. The picture's dark shapes cast a darker
      copy of themselves, pushed away from the light.
    Developed for: the SH4DER demo disc, where a misplaced copy of the frame
      read threw dark ghosts that read as shadows; it was kept and made
      deliberate.
    Use it for: trees or pillars against a low sun, a figure in a bright
      doorway, window bars across a room.

    Recommended settings (the author's own game and demo disc):
    - Strength 0.50, off by default: a strong shade reads as a second,
      wrong shadow. Useful range 0.25 .. 3.00.
    - Draw it under the eye's layers and the glow (the demo: z 425, the
      eye from 435, the glow at 447).
    - Clear the map once when the light leaves the view, not every frame.

    \author Jilted Valkyrie
    \author Cortana (Claude, an AI by Anthropic): the effects' maths, the PVR paths, the tests
*/

#ifndef SH4DER_SHAFTS_H
#define SH4DER_SHAFTS_H

#include <stdint.h>
#include "sh4der_core.h"
#include "sh4der_glow.h"
#ifdef SH4DER_CORE_NO_SH4ZAM
#include <math.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_SHAFTS_DARK_FROM  0.30f  /**< \brief Luma (0..1) under which a cell casts. */
#define SH4DER_SHAFTS_DARK_FULL  0.10f  /**< \brief Luma from which down it casts fully. */
#define SH4DER_SHAFTS_DEPTH      0.45f  /**< \brief Light a full cast takes at strength 1. */
#define SH4DER_SHAFTS_DEPTH_MAX  0.9f   /**< \brief Most it ever takes. */
#define SH4DER_SHAFTS_PUSH       0.12f  /**< \brief How far the copy is pushed from the light at strength 1 (scale 1 - PUSH). */
#define SH4DER_SHAFTS_NEAR       0.25f  /**< \brief Within this of the light on screen (screen heights): full. */
#define SH4DER_SHAFTS_FAR        0.9f   /**< \brief From this far: none. */
#define SH4DER_SHAFTS_FACE_FROM  0.5f   /**< \brief Cosine between the view axis and the light where the shade starts (60 degrees off). */
#define SH4DER_SHAFTS_FACE_FULL  0.95f  /**< \brief Cosine from which it is full (18 degrees off). */
/** @} */

static inline float sh4der_shafts_sqrt_pos_(float x) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_sqrtf_fsrra(x);
#else
    return sqrtf(x);
#endif
}

/** \brief How far the camera has turned to the light, 0..1, from the cosine between the view axis and the light. */
static inline float sh4der_shafts_toward(float facing) {
    return sh4dow_smooth01((facing - SH4DER_SHAFTS_FACE_FROM) * (1.0f / (SH4DER_SHAFTS_FACE_FULL - SH4DER_SHAFTS_FACE_FROM)));
}

/** \brief The light a full cast takes this frame: DEPTH x strength (never over DEPTH_MAX) x toward. */
static inline float sh4der_shafts_depth(float strength, float toward) {
    const float d = SH4DER_SHAFTS_DEPTH * strength;
    return (d > SH4DER_SHAFTS_DEPTH_MAX ? SH4DER_SHAFTS_DEPTH_MAX : d < 0.0f ? 0.0f : d) * sh4der_sat_(toward);
}

/** \brief The copy's scale about the light at a strength: under 1, the copy lies further from the light than its caster. */
static inline float sh4der_shafts_scale(float strength) { return 1.0f - SH4DER_SHAFTS_PUSH * sh4der_sat_(strength * (1.0f / 3.0f)) * 3.0f; }

/** \brief How much a cell at screen position (cu, cv, 0..1) counts by its distance from the light at (su, sv): 1 near, 0 far. */
static inline float sh4der_shafts_near(float cu, float cv, float su, float sv) {
    const float dx = (cu - su) * SH4DER_GLOW_ASPECT, dy = cv - sv;
    const float d = sh4der_shafts_sqrt_pos_(sh4der_len2_(dx, dy) + 1e-6f);
    return 1.0f - sh4dow_smooth01((d - SH4DER_SHAFTS_NEAR) * (1.0f / (SH4DER_SHAFTS_FAR - SH4DER_SHAFTS_NEAR)));
}

/** \brief How much a cell of luma (0..1) casts: 0 from DARK_FROM up, 1 from DARK_FULL down. */
static inline float sh4der_shafts_cast(float luma) {
    return sh4der_sat_((SH4DER_SHAFTS_DARK_FROM - luma) * (1.0f / (SH4DER_SHAFTS_DARK_FROM - SH4DER_SHAFTS_DARK_FULL)));
}

/** \brief A map texel moved half way from `was` to the grey that keeps `keep` (0..1) of the frame's light. RGB565 grey. */
static inline uint16_t sh4der_shafts_texel(uint16_t was, float keep) {
    const unsigned g6 = (unsigned)(sh4der_sat_(keep) * 63.0f + 0.5f), old = ((unsigned)was >> 5) & 63u, now = (old + g6 + 1u) >> 1;
    return (uint16_t)((now >> 1) << 11 | now << 5 | (now >> 1));
}

/** \brief The whole map from a read: `grid` is SH4DER_GLOW_GRID_X x SH4DER_GLOW_GRID_Y RGB565 (the frame read, untouched),
    `map` rows of `stride` texels (at least SH4DER_GLOW_GRID_X; the glow's texture is 128). (su, sv): the light on screen, 0..1.
    `depth` from sh4der_shafts_depth. The map must have been cleared (sh4der_shafts_clear) before its first use. */
static inline void sh4der_shafts_map(uint16_t *map, unsigned stride, const uint16_t *grid, float su, float sv, float depth) {
    unsigned x, y;
    if (!map || !grid || stride < (unsigned)SH4DER_GLOW_GRID_X)
        return;
    for (y = 0; y < (unsigned)SH4DER_GLOW_GRID_Y; y++)
        for (x = 0; x < (unsigned)SH4DER_GLOW_GRID_X; x++) {
            const float l = (float)sh4der_glow_luma8_(grid[y * SH4DER_GLOW_GRID_X + x]) * (1.0f / 255.0f);
            const float cu = ((float)x + 0.5f) * (1.0f / (float)SH4DER_GLOW_GRID_X), cv = ((float)y + 0.5f) * (1.0f / (float)SH4DER_GLOW_GRID_Y);
            const float cast = sh4der_shafts_cast(l) * sh4der_shafts_near(cu, cv, su, sv);
            map[y * stride + x] = sh4der_shafts_texel(map[y * stride + x], 1.0f - depth * cast);
        }
}

/** \brief White: no shade anywhere. `rows` rows of `stride` texels. */
static inline void sh4der_shafts_clear(uint16_t *map, unsigned stride, unsigned rows) {
    unsigned i;
    if (!map)
        return;
    for (i = 0; i < stride * rows; i++) map[i] = 0xffffu;
}

#ifdef _arch_dreamcast
/* ------------------------------------------------------------------------ */
/* The Dreamcast side (src/sh4der_shafts.c): the texture, the multiply mesh. */
#include <dc/pvr.h>

/** \brief The shade map's texture and header. */
typedef struct sh4der_shafts_gpu {
    pvr_ptr_t tex;          /**< \brief SH4DER_GLOW_TEX_W x SH4DER_GLOW_TEX_H RGB565, 16 KB; NULL when not held. */
    pvr_poly_hdr_t hdr;     /**< \brief Translucent list, multiply: the frame x the texel. */
} sh4der_shafts_gpu_t;

/** \brief The texture and the header. Returns 0, or -1 with no video memory (nothing held). */
int sh4der_shafts_init(sh4der_shafts_gpu_t *g);

/** \brief Give the texture back. */
void sh4der_shafts_shutdown(sh4der_shafts_gpu_t *g);

/** \brief Load a map (SH4DER_GLOW_TEX_W x SH4DER_GLOW_TEX_H texels, 32-byte aligned) into the texture. Not between a
    list's begin and finish while direct-render vertices are being written. */
void sh4der_shafts_upload(const sh4der_shafts_gpu_t *g, const uint16_t *map);

/** \brief The shade over a w x h screen: the light at (su, sv, 0..1), the copy's scale from sh4der_shafts_scale. One mesh
    of SH4DER_GLOW_MESH_X x SH4DER_GLOW_MESH_Y cells in the translucent list, the vertices by KOS direct render. */
void sh4der_shafts_draw(const sh4der_shafts_gpu_t *g, float su, float sv, float scale, float w, float h, float z);

#endif /* _arch_dreamcast */

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_SHAFTS_H */
