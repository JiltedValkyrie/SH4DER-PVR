/* SH4DER-PVR: the frame read

   sh4der_frame.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_frame.h
    \brief   Reading the frame on screen back, and knowing which frame it was.

    The look and the glow work from the frame the CPU reads off the screen. That
    frame is one or two frames old, and which of the two changes from frame to
    frame. An effect that takes "last frame's" gain or glow off the read then
    corrects the wrong frame and pulses. So every frame carries a stamp:

    - sh4der_frame_next gives the frame its number (0..7) and keeps what it is
      drawn with: the eye's gain and the glow's strength.
    - sh4der_frame_stamp_draw lays a SH4DER_FRAME_STAMP_PX block of that
      number's colour (each channel full or none) at the top left, over
      everything.
    - sh4der_frame_read samples the frame on a grid and reads the stamp back:
      the history under that number is what the read frame carried.

    The read gives two grids: `raw`, one sample a cell (the eye's histogram), and
    `dark`, the darker of two samples a cell per channel (the glow's source: one
    hot pixel alone in its cell, a texture's grain or a letter's stroke, lights
    nothing).

    Order, per frame: sh4der_frame_next (the stamp, and what the frame is
    drawn with), the draw, then sh4der_frame_stamp_draw last so it sits over
    everything, then sh4der_frame_read once the scene is on the screen. The
    read's stamp names the frame it found.

    sh4der_frame_degain takes a gain off a grid (the glow wants the scene, not
    the scene times the eye). sh4der_frame_hot is the sun's "looked at" term.

    Version: v0.50
    The effect: lets a program see its own finished picture. The frame on
      screen is read back on a small grid, and every frame carries a small mark
      so a read knows which frame it saw.
    Developed for: the SH4DER demo disc, so the eye and the glow could read
      the frame without pulsing (the frame on screen is one or two frames old).
    Use it for: anything that must know what is on screen: automatic exposure,
      a bloom source, ambient light from the picture's average colour, rumble or
      music driven by brightness.

    \author Jilted Valkyrie
    \author Cortana (Claude, an AI by Anthropic): the effects' maths, the PVR paths, the tests
*/

#ifndef SH4DER_FRAME_H
#define SH4DER_FRAME_H

#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_FRAME_GRID_X   80   /**< \brief Samples across the frame (the look's and the glow's grid). */
#define SH4DER_FRAME_GRID_Y   60   /**< \brief Samples down the frame. */
#define SH4DER_FRAME_STAMP_PX 4    /**< \brief The stamp block's side, pixels. */
#define SH4DER_FRAME_HISTORY  8    /**< \brief Frames of history kept (the stamp's 3 bits). */
#define SH4DER_FRAME_HOT_FROM 0.85f   /**< \brief Cosine off the view axis from which a faced sun starts to grow (about 32 degrees). */
/** @} */

/** \brief What each of the last frames was drawn with, by stamp. */
typedef struct sh4der_frame_hist {
    unsigned id;                              /**< \brief This frame's stamp. */
    float gain[SH4DER_FRAME_HISTORY];         /**< \brief The eye's gain laid over that frame (1: none). */
    float gain_ln[SH4DER_FRAME_HISTORY];      /**< \brief Its log. */
    float glow[SH4DER_FRAME_HISTORY];         /**< \brief The glow strength drawn on that frame, all passes (0: none). */
} sh4der_frame_hist_t;

/** \brief The start: no gain, no glow on any frame. */
static inline void sh4der_frame_hist_reset(sh4der_frame_hist_t *h) {
    unsigned i;
    h->id = 0;
    for (i = 0; i < SH4DER_FRAME_HISTORY; i++) { h->gain[i] = 1.0f; h->gain_ln[i] = 0.0f; h->glow[i] = 0.0f; }
}

/** \brief The next frame: its stamp, and what it is being drawn with. Returns the stamp (0..7).
    \param h The history.
    \param gain The eye's gain laid over this frame (1: none).
    \param gain_ln Its natural log.
    \param glow Glow strength drawn on this frame, all passes (0: none). */
static inline unsigned sh4der_frame_next(sh4der_frame_hist_t *h, float gain, float gain_ln, float glow) {
    h->id = (h->id + 1u) & (SH4DER_FRAME_HISTORY - 1u);
    h->gain[h->id] = gain;
    h->gain_ln[h->id] = gain_ln;
    h->glow[h->id] = glow;
    return h->id;
}

/** \brief The stamp's colour: bit 2 red, bit 1 green, bit 0 blue, each full or none; alpha 255. */
static inline uint32_t sh4der_frame_stamp_argb(unsigned id) {
    return 0xff000000u | ((id & 4u) ? 0xff0000u : 0u) | ((id & 2u) ? 0xff00u : 0u) | ((id & 1u) ? 0xffu : 0u);
}

/** \brief The stamp read back from an RGB565 pixel of the block. */
static inline unsigned sh4der_frame_stamp_id(uint16_t px) {
    return ((((unsigned)px >> 11) & 31u) > 15u ? 4u : 0u) | ((((unsigned)px >> 5) & 63u) > 31u ? 2u : 0u) | (((unsigned)px & 31u) > 15u ? 1u : 0u);
}

/** \brief The darker of two RGB565 pixels, per channel. */
static inline uint16_t sh4der_frame_min565(uint16_t p, uint16_t q) {
    const unsigned pr = (unsigned)p >> 11, qr = (unsigned)q >> 11, pg = ((unsigned)p >> 5) & 63u, qg = ((unsigned)q >> 5) & 63u, pb = (unsigned)p & 31u, qb = (unsigned)q & 31u;
    return (uint16_t)((pr < qr ? pr : qr) << 11 | (pg < qg ? pg : qg) << 5 | (pb < qb ? pb : qb));
}

/** \brief Take a gain off n RGB565 pixels: each channel / gain, clamped to its range. A gain within 1 % of 1 changes nothing;
    a gain under 1 / 256 (or not a number) is left alone: its inverse would not fit the integer maths. */
static inline void sh4der_frame_degain(uint16_t *px, unsigned n, float gain) {
    unsigned i, k;
    if (!(gain > 0.00390625f) || (gain > 0.99f && gain < 1.01f))
        return;
    k = (unsigned)(256.0f * sh4der_div_(1.0f, gain));
    for (i = 0; i < n; i++) {
        const unsigned q = px[i], r = ((q >> 11) * k) >> 8, g = (((q >> 5) & 63u) * k) >> 8, b = ((q & 31u) * k) >> 8;
        px[i] = (uint16_t)((r > 31u ? 31u : r) << 11 | (g > 63u ? 63u : g) << 5 | (b > 31u ? 31u : b));
    }
}

/** \brief How hard a light is looked at, 0..1: `facing` is the cosine between the view axis and the light. 0 up to
    SH4DER_FRAME_HOT_FROM, 1 dead on, squared between. A client grows and whitens its sun by it. */
static inline float sh4der_frame_hot(float facing) {
    const float t = sh4der_sat_((facing - SH4DER_FRAME_HOT_FROM) * (1.0f / (1.0f - SH4DER_FRAME_HOT_FROM)));
    return t * t;
}

#ifdef _arch_dreamcast
#include <dc/pvr.h>

/** \brief Read the frame on screen (the scan-out, fb_w x fb_h RGB565) on the grid. `raw`: one sample a cell at its centre.
    `dark` (may be NULL): the darker of that sample and one 3 pixels right and down. Returns the frame's stamp. About 4,800
    (9,600 with `dark`) uncached 16-bit reads. */
unsigned sh4der_frame_read(uint16_t raw[SH4DER_FRAME_GRID_X * SH4DER_FRAME_GRID_Y], uint16_t *dark, unsigned fb_w, unsigned fb_h);

/** \brief The stamp block at the top left, in the translucent list at depth z (over everything), replacing what is under it. */
void sh4der_frame_stamp_draw(unsigned id, float z);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_FRAME_H */
