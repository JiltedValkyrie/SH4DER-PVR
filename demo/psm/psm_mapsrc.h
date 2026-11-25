/* SH4DER-PVR: the perspective shadow map demo's map source hook

   demo/psm/psm_mapsrc.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   The seam for a future route AB: the PVR draws the casters' depth into a render target, the picture is copied back
   to RAM, and the SH-4 compares it per vertex as route B does. That route's fill lands late (a render and a copy), so
   the compare must be able to read a map that somebody else filled, a frame ago. This file is only that seam. It
   holds no PVR code and no depth encoding.

   What the compare needs is a whole sh4der_psm_raster_t, not just the 16-bit buffer: the map's matrix, its depth
   scale and its size are the ones the buffer was filled with. So a source owns two of them (two slots), each with a
   buffer of its own:

     delay 0  one slot. A frame begins it, fills it and reads it: route B as it is, byte for byte.
     delay 1  two slots. A frame's fill goes into the slot that is not being read. The slot becomes the one read at
              the start of a later frame, and only once its fill is complete (the fill said so, or psm_mapsrc_ready
              was called). A fill still on its way is left alone: no new fill starts over it, and the compare keeps
              the last complete map. The slot being read is never the slot being written.

   The fill is a callback: it gets the slot, already begun (sh4der_psm_raster_begin: the matrix in, the buffer
   cleared), and writes depth into slot->depth by any means. The compare (sh4der_psm_raster_lit, _lit_plane, _lit_n)
   only reads the slot, so it does not know who filled it.

   The second half of the file is the demo's use of it: "ROUTE AB (STUB)". The fill there is route B's own SH-4
   raster standing in for the PVR, at delay 1, so the two slots, the swap and the one-frame lag run on the host and
   on the console now. It is not the PVR route and the readout says so.

   Cost: the second slot's buffer, size x size x 2 bytes of RAM (32 KB at 128 x 128, 128 KB at 256 x 256), and two
   sh4der_psm_raster_t. No allocation: the caller owns both buffers.
*/

#ifndef PSM_MAPSRC_H
#define PSM_MAPSRC_H

#include "sh4der_psm_raster.h"

/* ---- the hook ---------------------------------------------------------------- */

/* What a fill answers. */
enum {
    PSM_MAPSRC_FAIL = 0,       /* nothing usable was written: the slot is dropped */
    PSM_MAPSRC_DONE = 1,       /* the slot is complete now */
    PSM_MAPSRC_PENDING = 2     /* the fill has started and will finish later: psm_mapsrc_ready says when (delay 1 only) */
};

/* Fill `dst` (begun: matrix set, dst->depth cleared, dst->size texels a side). `user` is the source's own. */
typedef int (*psm_mapsrc_fill_fn)(sh4der_psm_raster_t *dst, void *user);

typedef struct psm_mapsrc {
    psm_mapsrc_fill_fn fill;
    void *user;
    unsigned delay;                 /* 0: read this frame's fill. 1: read the last complete one */
    sh4der_psm_raster_t slot[2];
    uint16_t *buf[2];               /* the caller's, each as large as the largest map; buf[1] is not used at delay 0 */
    unsigned read, write;           /* the slot the compare reads, the slot the last fill went into */
    int have;                       /* slot[read] holds a complete map */
    int pending;                    /* a fill was started in slot[write] and has not been taken as the read slot yet */
    int ready;                      /* that fill is complete */
    unsigned fills, swaps, skipped; /* counts: fills started, slots taken, frames with no new fill (one still on its way) */
} psm_mapsrc_t;

/* Set a source up. delay 0 needs buf0; delay 1 needs both, and they must be different. 0 for a bad input. */
static inline int psm_mapsrc_init(psm_mapsrc_t *src, psm_mapsrc_fill_fn fill, void *user, unsigned delay, uint16_t *buf0, uint16_t *buf1) {
    if (!src) return 0;
    memset(src, 0, sizeof *src);
    if (!fill || !buf0 || delay > 1u || (delay == 1u && (!buf1 || buf1 == buf0))) return 0;
    src->fill = fill; src->user = user; src->delay = delay;
    src->buf[0] = buf0; src->buf[1] = buf1;
    return 1;
}

/* Forget every map: the next frame starts as the first one did (a change of scene, or of route). */
static inline void psm_mapsrc_reset(psm_mapsrc_t *src) {
    src->read = src->write = 0u;
    src->have = src->pending = src->ready = 0;
}

/* The fill that answered PSM_MAPSRC_PENDING has finished: its slot may be read from the next frame on. */
static inline void psm_mapsrc_ready(psm_mapsrc_t *src) {
    if (src->pending) src->ready = 1;
}

/* One frame of the source: take a finished fill as the map to read, start this frame's fill, and return the map the
   compare reads this frame, or NULL when there is none yet (draw unshadowed). `map`, `size` and the depth range are
   this frame's (as for sh4der_psm_raster_begin); the map returned at delay 1 is the one of the frame its fill began. */
static inline const sh4der_psm_raster_t *psm_mapsrc_frame(psm_mapsrc_t *src, const sh4der_psm_t *map, unsigned size, float key_lo, float key_hi) {
    if (!src || !src->fill) return NULL;
    if (src->delay == 0u) {
        src->read = src->write = 0u;
        src->have = 0;
        if (sh4der_psm_raster_begin(&src->slot[0], map, src->buf[0], size, key_lo, key_hi)) {
            src->fills++;
            src->have = src->fill(&src->slot[0], src->user) == PSM_MAPSRC_DONE;   /* a fill that is not complete now is no map this frame */
        }
        return src->have ? &src->slot[0] : NULL;
    }
    if (src->pending && src->ready) {                          /* a fill has landed: its slot is the one to read from now on */
        src->read = src->write;
        src->have = 1;
        src->pending = src->ready = 0;
        src->swaps++;
    }
    if (src->pending) {
        src->skipped++;                                        /* still on its way: its slot is not touched, nor the one being read */
    } else {
        const unsigned w = src->have ? src->read ^ 1u : 0u;    /* never the slot being read */

        if (sh4der_psm_raster_begin(&src->slot[w], map, src->buf[w], size, key_lo, key_hi)) {
            const int r = src->fill(&src->slot[w], src->user);

            src->write = w;
            src->fills++;
            if (r == PSM_MAPSRC_DONE) { src->pending = 1; src->ready = 1; }
            else if (r == PSM_MAPSRC_PENDING) { src->pending = 1; src->ready = 0; }
        }
    }
    return src->have ? &src->slot[src->read] : NULL;
}

/* ---- the demo's route AB stub -------------------------------------------------- */

#include "psm_scene.h"

#define PSM_ROUTE_AB 2             /* beside PSM_ROUTE_A and PSM_ROUTE_B: the hook, with the SH-4's raster as the fill */

/* The stand-in fill's own: the frame whose casters go in. */
typedef struct psm_ab_stub {
    psm_frame_t *f;
    const psm_scene_t *s;
    int movers_only;
} psm_ab_stub_t;

/* The stand-in for the PVR: route B's SH-4 raster draws the casters into the slot. Complete when it returns. */
static inline int psm_ab_stub_fill(sh4der_psm_raster_t *dst, void *user) {
    psm_ab_stub_t *u = (psm_ab_stub_t *)user;

    u->f->raster = *dst;                                       /* route B's step draws into the frame's raster: point it at the slot */
    psm_frame_b_casters(u->f, u->s, u->movers_only);
    *dst = u->f->raster;                                       /* the slot keeps the counts of what was drawn */
    return PSM_MAPSRC_DONE;
}

/* Route AB, step 1 (after psm_frame_setup): the casters' depth range and one frame of the source. Returns the map to
   compare against this frame, or NULL (no map: call psm_frame_unshadowed). */
static inline const sh4der_psm_raster_t *psm_frame_ab_fill(psm_frame_t *f, const psm_scene_t *s, psm_mapsrc_t *src) {
    float lo, hi;

    if (!f->built || !sh4der_psm_raster_key_range(f->map.light, &s->mv[0][0], 3u * sizeof(float), s->mvn, &lo, &hi)) return NULL;
    return psm_mapsrc_frame(src, &f->map, f->size, lo - 0.25f, hi + 0.25f);
}

/* Route AB, step 2: route B's compare against `map` (from psm_frame_ab_fill). The frame's raster becomes that map, so the
   inset and the readout show what was compared against. */
static inline void psm_frame_ab_compare(psm_frame_t *f, const psm_scene_t *s, const psm_state_t *st, const sh4der_psm_raster_t *map) {
    f->raster = *map;
    psm_frame_b_receivers(f, s, st);
}

/* The route after a pad frame, for a program that keeps three routes while psm_input knows two: psm_input flips
   st->route between 0 and 1 (and turns a 2 into 0); a flip moves `route3` one on: A, B, AB. Returns the route to put
   back in st->route. */
static inline int psm_ab_route_step(int *route3, int before, int after) {
    if (after != before) *route3 = (*route3 + 1) % 3;
    return *route3;
}

/* The readout for route AB: route B's lines (the compare is route B's), with a title that says what fills the map. */
static inline void psm_ab_hud(const psm_state_t *st, const psm_frame_t *f, const psm_cost_t *c, const psm_mapsrc_t *src,
                              char ln[PSM_HUD_LINES][PSM_HUD_COLS + 1u]) {
    psm_state_t b = *st;
    char rest[PSM_HUD_COLS + 1u], big[192];
    const char *map;

    memset(big, 0, sizeof big);                                /* the whole line is copied: nothing left over after the text */
    b.route = PSM_ROUTE_B;
    psm_hud(&b, f, c, ln);
    map = strstr(ln[0], "   MAP ");
    snprintf(rest, sizeof rest, "%s", map ? map : "");
    snprintf(big, sizeof big, "AB STUB: SH-4 fill, %s%s", src && src->delay ? "1 frame late" : "same frame", rest);
    memcpy(ln[0], big, PSM_HUD_COLS);
    ln[0][PSM_HUD_COLS] = 0;
    memset(big, 0, sizeof big);                                /* two maps in RAM: the one filled and the one read */
    snprintf(big, sizeof big, "MAP %ux%u  2 x %u KB RAM (16-bit depth)  text %u KB VRAM", f->size, f->size, (f->size * f->size * 2u + 1023u) / 1024u,
             PSM_TEXT_KB);
    memcpy(ln[2], big, PSM_HUD_COLS);
    ln[2][PSM_HUD_COLS] = 0;
    if (st->row % PSM_ROWS == PSM_ROW_ROUTE) {                 /* the menu's ROUTE row reads AB, not the B it was drawn as */
        char v[48];

        memset(big, 0, sizeof big);
        snprintf(big, sizeof big, "%s: %s   (D-pad row/change, B close)", psm_row_name[PSM_ROW_ROUTE], psm_row_value(st, PSM_ROW_ROUTE, v, sizeof v));
        memcpy(ln[8], big, PSM_HUD_COLS);
        ln[8][PSM_HUD_COLS] = 0;
    }
}

#endif /* PSM_MAPSRC_H */
