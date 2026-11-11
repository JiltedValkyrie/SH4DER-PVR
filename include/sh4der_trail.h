/* SH4DER-PVR: motion blur and the history layers of the feed route

   sh4der_trail.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_trail.h
    \brief   Motion blur, and the KOS draw code of the feed route both it and
             sh4der_taa.h's history use.

    The feed route: last frame's picture is on screen in the front buffer. On
    the Dreamcast that buffer is read as a texture (KOS examples/dreamcast/pvr/
    fb_tex), so the history costs 0 VRAM. Two things make it work:
    - The column mask (8 x 8 ARGB1555, 128 B) puts the column parity into the
      destination alpha. The front buffer's two 64-bit halves are read in the
      wrong order on alternate columns, so each copy is two passes: the even
      columns (DESTALPHA) and the odd ones (INVDESTALPHA).
    - A copy is 1:1. It moves by whole pixels only: x by an EVEN count (the
      parity stays), y by any count.

    Preconditions, set once per load (sh4der_trail_setup): 640 x 480, no
    AA 2X, dithering off, vertical scale 1.0. The stride register is shared
    with the render to a texture (sh4der_ca.h): never both at once.

    Layers per frame: keep (1), mask (1), two per copy. Each layer is a
    full-screen translucent quad, z from the caller's base upward (z .. z + 1 +
    2 * copies). Autosort orders them under the HUD.

    Motion blur (this file): the history weight rises with the camera's speed
    (the TAA weight turned round): still, no blur; fast, the weight is
    SH4DER_TRAIL_WEIGHT_HI. The history is drawn as SH4DER_TRAIL_COPIES copies
    spread from no shift to the camera's motion, so a turn streaks. A cut
    (sh4der_trail_step) clears it.

    The maths is the host side (inline, both SH4ZAM and plain C). The KOS side
    is under _arch_dreamcast.

    \author Jilted Valkyrie
    \author Cortana (Claude, an AI by Anthropic): the effects' maths, the PVR paths, the tests
*/

#ifndef SH4DER_TRAIL_H
#define SH4DER_TRAIL_H

#include <stdint.h>
#include "sh4der_core.h"
#include "sh4der_taa.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Shipped tuning
    @{ */
#define SH4DER_TRAIL_COPIES    3      /**< \brief History copies along the motion, 2 or 3 (copy k at k / (COPIES - 1) of the motion). */
#define SH4DER_TRAIL_WEIGHT_LO 0.0f   /**< \brief History weight while still (no blur). */
#define SH4DER_TRAIL_WEIGHT_HI 0.5f   /**< \brief History weight at SPEED_HI and faster: the cap (the HUD's last frame ghosts at most this). */
#define SH4DER_TRAIL_SPEED_LO  1.0f   /**< \brief Content speed (pixels a frame) below which there is no blur. */
#define SH4DER_TRAIL_SPEED_HI  8.0f   /**< \brief Content speed (pixels a frame) from which the weight is full. */
#define SH4DER_TRAIL_SHIFT_MAX 8      /**< \brief Longest copy offset, pixels (the streak's length cap). */
#define SH4DER_TRAIL_CUT_RAD   0.35f  /**< \brief A turn of this many radians in one frame is a cut: no history. */
/** @} */

/** \brief One frame's numbers, from sh4der_trail_step. */
typedef struct sh4der_trail_frame {
    float weight;        /**< \brief Total history weight, 0..WEIGHT_HI. 0: no history layers this frame. */
    float copy_w;        /**< \brief Weight of each copy: weight / copies. */
    int copies;          /**< \brief Copies drawn this frame: COPIES, or 0 (no weight, or a cut). */
    int dx[SH4DER_TRAIL_COPIES];   /**< \brief Copy k's x offset, pixels, EVEN. */
    int dy[SH4DER_TRAIL_COPIES];   /**< \brief Copy k's y offset, pixels. */
    int cut;             /**< \brief 1: first frame, or a cut: no history. */
} sh4der_trail_frame_t;

/** \brief One camera's motion state. Zero it with sh4der_trail_reset at load. */
typedef struct sh4der_trail {
    float yaw, pitch;   /**< \brief Last frame's camera angles, radians. */
    unsigned frame;     /**< \brief Frames stepped. */
    int ready;          /**< \brief 0 until the first step. */
} sh4der_trail_t;

/* ------------------------------------------------------------------------ */
/* The maths (inline). */

/** \brief The motion blur weight from the content's pixel speed squared: 0 up to SPEED_LO, rising to WEIGHT_HI at SPEED_HI.
    Never falls with speed (the TAA weight turned round). */
static inline float sh4der_trail_weight(float s2) {
    const float lo = SH4DER_TRAIL_SPEED_LO * SH4DER_TRAIL_SPEED_LO, hi = SH4DER_TRAIL_SPEED_HI * SH4DER_TRAIL_SPEED_HI;
    const float t = sh4der_sat_(sh4der_div_(s2 - lo, hi - lo));
    return sh4der_lerp_(SH4DER_TRAIL_WEIGHT_LO, SH4DER_TRAIL_WEIGHT_HI, t);
}

/** \brief Zero the motion state. The next step is a cut: no history that frame. */
static inline void sh4der_trail_reset(sh4der_trail_t *t) { t->yaw = 0.0f; t->pitch = 0.0f; t->frame = 0; t->ready = 0; }

/** \brief One frame: the camera's yaw and pitch (radians), the focal lengths (pixels). Fills `out`, then remembers the angles.
    The copies run from no shift (copy 0) to the full motion (copy COPIES - 1), each clamped to SHIFT_MAX. */
static inline void sh4der_trail_step(sh4der_trail_t *t, float yaw, float pitch, float fx, float fy, sh4der_trail_frame_t *out) {
    float dyaw, dpitch, shift[2];
    int k;
    if (!sh4der_finite_(yaw) || !sh4der_finite_(pitch)) {   /* not a number: a cut, and the angles are not kept */
        out->cut = 1;
        out->weight = 0.0f;
        out->copy_w = 0.0f;
        out->copies = 0;
        for (k = 0; k < SH4DER_TRAIL_COPIES; k++) { out->dx[k] = 0; out->dy[k] = 0; }
        t->frame++;
        t->ready = 0;
        return;
    }
    dyaw = sh4der_taa_angle_delta(yaw, t->yaw);
    dpitch = sh4der_taa_angle_delta(pitch, t->pitch);
    out->cut = !t->ready || sh4der_taa_abs_(dyaw) > SH4DER_TRAIL_CUT_RAD || sh4der_taa_abs_(dpitch) > SH4DER_TRAIL_CUT_RAD;
    out->weight = 0.0f;
    out->copy_w = 0.0f;
    out->copies = 0;
    for (k = 0; k < SH4DER_TRAIL_COPIES; k++) { out->dx[k] = 0; out->dy[k] = 0; }
    if (!out->cut) {
        sh4der_taa_shift(fx, fy, dyaw, dpitch, shift);
        out->weight = sh4der_trail_weight(sh4der_len2_(shift[0], shift[1]));
        if (out->weight > 0.0f) {
            out->copies = SH4DER_TRAIL_COPIES;
            out->copy_w = sh4der_div_(out->weight, (float)SH4DER_TRAIL_COPIES);
            for (k = 0; k < SH4DER_TRAIL_COPIES; k++) {
                const float f = sh4der_div_((float)k, (float)(SH4DER_TRAIL_COPIES - 1));
                int dx = sh4der_taa_even_px(shift[0] * f), dy = sh4der_taa_whole_px(shift[1] * f);
                dx = dx > SH4DER_TRAIL_SHIFT_MAX ? SH4DER_TRAIL_SHIFT_MAX : dx < -SH4DER_TRAIL_SHIFT_MAX ? -SH4DER_TRAIL_SHIFT_MAX : dx;
                dy = dy > SH4DER_TRAIL_SHIFT_MAX ? SH4DER_TRAIL_SHIFT_MAX : dy < -SH4DER_TRAIL_SHIFT_MAX ? -SH4DER_TRAIL_SHIFT_MAX : dy;
                out->dx[k] = dx;
                out->dy[k] = dy;
            }
        }
    }
    t->yaw = yaw;
    t->pitch = pitch;
    t->frame++;
    t->ready = 1;
}

/* ------------------------------------------------------------------------ */
/* The feed route's KOS draw code (src/sh4der_trail.c). Dreamcast only. */

#ifdef _arch_dreamcast
#include <dc/pvr.h>

#define SH4DER_TRAIL_MASK_BYTES 128   /**< \brief The mask's size in VRAM. */

/** \brief The source of the copies: the frame on screen, as the copy's texture (front buffer, its 64-bit half). */
typedef struct sh4der_trail_src {
    pvr_ptr_t fb;       /**< \brief The front buffer in VRAM: the copies' source. */
    int hi_chip;        /**< \brief Which 64-bit half of the front buffer the copy reads (set by sh4der_trail_src_get). */
    int flat;   /**< \brief 1: no chip offsets on the copy (a setup where they shift the picture instead; set after sh4der_trail_src_get). */
} sh4der_trail_src_t;

/** \brief Load time, once per load: the mask (128 B VRAM, NULL: no VRAM) and the preconditions (dithering off, vertical scale 1.0). */
pvr_ptr_t sh4der_trail_setup(void);

/** \brief Frame: the front buffer as the copies' source, and the stride (640). Call once a frame, before the layers. */
void sh4der_trail_src_get(sh4der_trail_src_t *s);

/** \brief Keep layer: the frame multiplied by argb's channels (sh4der_taa_keep_argb(1 - w)). Clears the destination alpha, so it comes first. Draws z. */
void sh4der_trail_keep(uint32_t argb, float z);

/** \brief Mask layer: the column parity into the destination alpha. Draws z. */
void sh4der_trail_mask(pvr_ptr_t mask, float z);

/** \brief One copy of the history at (dx, dy) pixels (dx EVEN), each channel weighted by argb (alpha ignored, add). Draws z and z + 1. */
void sh4der_trail_copy(const sh4der_trail_src_t *s, int dx, int dy, uint32_t argb, float z);

/** \brief The whole effect for a motion blur frame: keep, mask, the copies. Draws z .. z + 1 + 2 * copies. Nothing when weight is 0. */
void sh4der_trail_draw(const sh4der_trail_src_t *s, pvr_ptr_t mask, const sh4der_trail_frame_t *f, float z);

/** \brief The same for TAA: keep at (1 - weight), one copy at the history's offset, weight. Nothing when weight is 0. */
void sh4der_trail_draw_taa(const sh4der_trail_src_t *s, pvr_ptr_t mask, const sh4der_taa_frame_t *f, float z);

#endif /* _arch_dreamcast */

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_TRAIL_H */
