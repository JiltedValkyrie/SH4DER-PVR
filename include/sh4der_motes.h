/* SH4DER-PVR: motes (a steady field of drifting specks of light in the air)

   sh4der_motes.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_motes.h
    \brief   Embers, dust, pollen, ash, snow flecks: a field of small square sprites that are born near an anchor in
             the world, drift and fall, fade as they age and are born again, drawn as hard 1-bit dots. Nothing is
             stored per speck: every value comes from (slot number, time), so the field costs no memory and a speck
             never needs updating.

    The stateless rule: an emitter owns `slots = ceil(rate x life_hi)` slots. Slot i has its own life between life_lo
    and life_hi and its own phase, so at time t it is `age` seconds into generation `gen` of its life. Every random
    pick (start box, velocity, acceleration, size) comes from an integer hash of (slot, generation, salt), so a slot
    starts somewhere new on every loop and the field never visibly repeats. Position is start + v x age + a x age^2 / 2.

    The look curves, as shares f = age / life: alpha rises from 0 to 1 over the first `alpha_in`; the mask opens as
    alpha passes `clip` (the dot grows from nothing, no pop, no alpha blending); the colour holds full until
    `col_hold` and falls to the fade colour at `col_end`; a speck dimmer than `cull_k` is not drawn at all.

    On the screen: the side of a sprite is its size x focal / depth pixels, held between `min_px` and `max_px`. A
    sprite smaller than min_px is drawn min_px wide with its brightness x (true / min_px)^2 (the same light in fewer
    pixels, no crawling single pixel); one wider than `big_px` is dimmed to `big_k` so a near speck does not read as a
    dull disc. An emitter further than `lod_cut` from the eye is not drawn. At most `max_draw` quads go out in a frame:
    over the cap the brightest (brightness x screen area) are kept.

    Per frame: sh4der_motes_collect(...) once, then sh4der_motes_draw_masked (punch-through list, one 16 x 16 mask,
    512 B) and, if wanted, sh4der_motes_draw_flat (opaque list, untextured, for sprites of `flat_px` pixels or less),
    both KallistiOS in src/sh4der_motes.c; or sh4der_motes_quad for a client that submits its own vertices.

    Memory: no state. The caller's collect buffer is max_draw x 32 B (3,072 B at 96; 4,096 B at the 128 limit);
    params 136 B; one emitter 16 B. VRAM: 512 B (the mask, optional). Sound RAM 0. SH4ZAM through sh4der_core.h.
    Floats and integers live in structs the caller owns, 4-byte aligned; nothing casts a byte pointer.

    Version: v0.10
    The effect: a steady drift of embers, dust or ash fixed in the world, hard and small.
    Developed for: embers over a burning deck.
    Use it for: embers, dust in a shaft, pollen, snow, ash, falling sparks, floating seeds.

    \author Jilted Valkyrie
    \author Junior (Claude, an AI by Anthropic): the effect

    PVR terms (TA, TR/OP list, EOL, twiddled, direct render): see the PVR glossary in README.md.
*/

#ifndef SH4DER_MOTES_H
#define SH4DER_MOTES_H

#include <stddef.h>
#include "sh4der_spark.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Limits
    @{ */
#define SH4DER_MOTES_SLOTS_MAX 256u    /**< \brief Slots one emitter holds (rate x life_hi, rounded up, at most this). */
#define SH4DER_MOTES_DRAW_MAX  128u    /**< \brief Quads one collect call can hold (params.max_draw is at most this). */
#define SH4DER_MOTES_GEN_WRAP  4096u   /**< \brief Generations before the hash input repeats (a speck then repeats after hours). */
#define SH4DER_MOTES_GEN_PRIME 7919u   /**< \brief Prime that spreads a slot's generations across the hash input. */
#define SH4DER_MOTES_MASK_SIDE 16u     /**< \brief Side of the round mask texture, texels. */
/** @} */

/** \brief What one field looks like and how it moves (the caller's table row; 136 B). Distances in world units. */
typedef struct sh4der_motes_params {
    float rate;                        /**< \brief Specks born a second per emitter (> 0). */
    float life_lo, life_hi;            /**< \brief Life, seconds, uniform between (0 < lo <= hi). */
    float box_xy;                      /**< \brief Start box side in x and y, centred on the anchor. */
    float box_z_lo, box_z_hi;          /**< \brief Start height above the anchor, between. */
    float vel_lo[3], vel_hi[3];        /**< \brief Start velocity, units a second, uniform between per axis. */
    float acc_lo[3], acc_hi[3];        /**< \brief Acceleration, units a second squared, uniform between per axis. */
    float size_lo, size_hi;            /**< \brief Sprite side, units, uniform between (> 0). */
    float alpha_in;                    /**< \brief Share of the life over which alpha rises 0 -> 1 (0 < x <= 1). */
    float clip;                        /**< \brief Mask clip value: the dot opens as alpha passes it (0 <= x < 1). */
    float col_hold, col_end;           /**< \brief Life shares: colour full until hold, fade colour at end (hold < end <= 1). */
    float cull_k;                      /**< \brief Brightness under which a speck is not drawn (0 .. 1). */
    float lod_cut;                     /**< \brief Distance from the eye past which an emitter is not drawn. */
    float min_px, max_px;              /**< \brief Least and largest sprite side on the screen, pixels (0 < min <= max). */
    float big_px, big_k;               /**< \brief Sprites wider than big_px are dimmed to this share (0 < k <= 1). */
    float flat_px;                     /**< \brief Sprites this wide or less may use the untextured draw. */
    uint32_t max_draw;                 /**< \brief Quads a frame at most (1 .. SH4DER_MOTES_DRAW_MAX). */
    uint32_t color_argb;               /**< \brief Colour at full brightness (alpha byte not used). */
    uint32_t fade_argb;                /**< \brief Colour a dying speck fades to (the area's horizon colour; alpha byte not used). */
} sh4der_motes_params_t;

/** \brief One emitter: where it sits and a seed that makes its field differ from the others (16 B). */
typedef struct sh4der_motes_emitter {
    float anchor[3];                   /**< \brief World position. */
    uint32_t seed;                     /**< \brief Added to the slot number before hashing (give each emitter its own, 1000 apart). */
} sh4der_motes_emitter_t;

/** \brief One speck at one instant, in world space. */
typedef struct sh4der_motes_ember {
    float pos[3];                      /**< \brief World position. */
    float size;                        /**< \brief Sprite side, world units (already grown by the mask opening). */
    float k;                           /**< \brief Brightness 0..1 along the colour curve. */
} sh4der_motes_ember_t;

/** \brief One sprite ready to draw (32 B): the view space centre, half the side in view units, the colour now. */
typedef struct sh4der_motes_draw {
    float c[3];                        /**< \brief Centre in view space (x right, y up, z forward). */
    float half;                        /**< \brief Half the side, view units. */
    float score;                       /**< \brief brightness x screen area (the cap keeps the highest). */
    uint32_t argb;                     /**< \brief Vertex colour: the fade colour toward the speck colour by its brightness. */
    uint32_t flat;                     /**< \brief 1 when the sprite is flat_px wide or less. */
    uint32_t pad_;                     /**< \brief Keeps the record 32 B. */
} sh4der_motes_draw_t;

/** \brief Fills the params with the ember look (the embers over a deck; every value is a table row's to change). */
static inline void sh4der_motes_params_default(sh4der_motes_params_t *p) {
    static const float vel_lo[3] = { -50.0f, -10.0f, -10.0f }, vel_hi[3] = { -10.0f, 10.0f, -10.0f };
    static const float acc_lo[3] = { -30.0f, -10.0f, -70.0f }, acc_hi[3] = { -10.0f, 10.0f, -70.0f };
    int i;

    p->rate = 30.0f; p->life_lo = 4.5f; p->life_hi = 7.0f;
    p->box_xy = 4000.0f; p->box_z_lo = 500.0f; p->box_z_hi = 600.0f;
    for (i = 0; i < 3; i++) { p->vel_lo[i] = vel_lo[i]; p->vel_hi[i] = vel_hi[i]; p->acc_lo[i] = acc_lo[i]; p->acc_hi[i] = acc_hi[i]; }
    p->size_lo = 3.0f; p->size_hi = 20.0f;
    p->alpha_in = 0.19f; p->clip = 0.333f; p->col_hold = 0.30f; p->col_end = 0.60f; p->cull_k = 0.08f;
    p->lod_cut = 3500.0f;
    p->min_px = 2.0f; p->max_px = 12.0f; p->big_px = 9.0f; p->big_k = 0.8f; p->flat_px = 3.0f;
    p->max_draw = 96u;
    p->color_argb = 0xFFEBBCB8u;       /* 235, 188, 184 */
    p->fade_argb = 0xFF461810u;        /* 70, 24, 16 */
}

/** \brief Slots one emitter holds: ceil(rate x life_hi), 0 when the params are bad. */
static inline unsigned sh4der_motes_slots(const sh4der_motes_params_t *p) {
    float n;
    unsigned s;

    if (!p || !sh4der_finite_(p->rate) || !sh4der_finite_(p->life_hi) || !(p->rate > 0.0f) || !(p->life_hi > 0.0f)) return 0;
    n = p->rate * p->life_hi;
    if (n > (float)SH4DER_MOTES_SLOTS_MAX) return 0;
    s = (unsigned)n;                                 /* ceil without float rounding of the sum */
    return (float)s < n ? s + 1u : s;
}

/** \brief 1 when every value is finite and in range (see the struct), 0 otherwise. */
static inline int sh4der_motes_params_valid(const sh4der_motes_params_t *p) {
    const float *f;
    unsigned i;

    if (!p || !sh4der_motes_slots(p)) return 0;
    f = &p->rate;                                   /* the floats run unbroken from rate to flat_px */
    for (i = 0; i < offsetof(sh4der_motes_params_t, max_draw) / sizeof(float); i++) if (!sh4der_finite_(f[i])) return 0;
    for (i = 0; i < 3; i++) if (p->vel_lo[i] > p->vel_hi[i] || p->acc_lo[i] > p->acc_hi[i]) return 0;
    return p->life_lo > 0.0f && p->life_lo <= p->life_hi && p->box_xy >= 0.0f && p->box_z_lo <= p->box_z_hi &&
           p->size_lo > 0.0f && p->size_lo <= p->size_hi && p->alpha_in > 0.0f && p->alpha_in <= 1.0f && p->clip >= 0.0f && p->clip < 1.0f &&
           p->col_hold >= 0.0f && p->col_hold < p->col_end && p->col_end <= 1.0f && p->cull_k >= 0.0f && p->cull_k <= 1.0f &&
           p->lod_cut > 0.0f && p->min_px > 0.0f && p->min_px <= p->max_px && p->big_px > 0.0f && p->big_k > 0.0f && p->big_k <= 1.0f &&
           p->flat_px >= 0.0f && p->max_draw >= 1u && p->max_draw <= SH4DER_MOTES_DRAW_MAX;
}

/** \brief A stateless 0..1 value for a slot number and a salt (an integer hash; the same numbers on every machine). */
static inline float sh4der_motes_hash01(uint32_t i, uint32_t salt) {
    uint32_t x = i * 0x9E3779B1u + salt * 0x85EBCA6Bu;   /* two odd multipliers; unsigned wrap is defined */

    x ^= x >> 15;
    x *= 0x2C1B3C6Du;
    x ^= x >> 12;
    return (float)(x & 0xFFFFFFu) * (1.0f / 16777216.0f);
}

/** \brief Slot `slot` of an emitter at time t (seconds, >= 0). Fills `out` and returns 1 when the speck is drawn
    (its dot has opened and it is brighter than cull_k); 0 otherwise. Keep t small (wrap it every few hours). */
static inline int sh4der_motes_slot(const sh4der_motes_params_t *p, const sh4der_motes_emitter_t *e, unsigned slot, float t,
                                    sh4der_motes_ember_t *out) {
    const uint32_t id = e->seed + slot;
    const float life = sh4der_lerp_(p->life_lo, p->life_hi, sh4der_motes_hash01(id, 1u));
    const float u = sh4der_fma_(sh4der_motes_hash01(id, 2u), life, t);   /* t plus this slot's phase */
    const uint32_t gen = (uint32_t)sh4der_div_(u, life);
    const float age = sh4der_fma_(-(float)gen, life, u);
    const float f = sh4der_div_(age, life);
    const uint32_t j = id + (gen % SH4DER_MOTES_GEN_WRAP) * SH4DER_MOTES_GEN_PRIME;
    const float alpha = sh4der_sat_(sh4der_div_(f, p->alpha_in));
    const float grow = sh4der_sat_(sh4der_div_(alpha - p->clip, 1.0f - p->clip));
    const float k = sh4der_sat_(sh4der_div_(p->col_end - f, p->col_end - p->col_hold));
    float v, a;
    int i;

    if (!(grow > 0.0f) || k < p->cull_k || k <= 0.0f) return 0;
    out->pos[0] = e->anchor[0] + sh4der_lerp_(-0.5f * p->box_xy, 0.5f * p->box_xy, sh4der_motes_hash01(j, 3u));
    out->pos[1] = e->anchor[1] + sh4der_lerp_(-0.5f * p->box_xy, 0.5f * p->box_xy, sh4der_motes_hash01(j, 4u));
    out->pos[2] = e->anchor[2] + sh4der_lerp_(p->box_z_lo, p->box_z_hi, sh4der_motes_hash01(j, 5u));
    for (i = 0; i < 3; i++) {
        v = sh4der_lerp_(p->vel_lo[i], p->vel_hi[i], sh4der_motes_hash01(j, 6u + (uint32_t)i));
        a = sh4der_lerp_(p->acc_lo[i], p->acc_hi[i], sh4der_motes_hash01(j, 9u + (uint32_t)i));
        out->pos[i] = sh4der_fma_(v, age, out->pos[i]);
        out->pos[i] = sh4der_fma_(0.5f * a * age, age, out->pos[i]);
    }
    out->size = sh4der_lerp_(p->size_lo, p->size_hi, sh4der_motes_hash01(j, 12u)) * grow;
    out->k = k;
    return 1;
}

/** \brief Vertex colour: the fade colour toward the speck colour by brightness k (0..1). Alpha byte 0xFF. */
static inline uint32_t sh4der_motes_color_(const sh4der_motes_params_t *p, float k) {
    const int32_t g = (int32_t)(sh4der_sat_(k) * 256.0f);   /* 0..256 */
    uint32_t out = 0xFF000000u;
    int s;

    for (s = 16; s >= 0; s -= 8) {
        const int32_t c = (int32_t)((p->color_argb >> s) & 0xFFu), f = (int32_t)((p->fade_argb >> s) & 0xFFu);

        out |= (uint32_t)(f + (((c - f) * g) >> 8)) << s;
    }
    return out;
}

/** \brief Walks the slots of every emitter that is within lod_cut of the eye and keeps the brightest `max_draw`
    sprites in `out` (unsorted). `screen` gives the picture half size (cx, cy) so sprites off screen are dropped.
    Returns how many sprites were kept (0 .. max_draw); bad arguments give 0. */
static inline unsigned sh4der_motes_collect(const sh4der_motes_params_t *p, const sh4der_motes_emitter_t *em, unsigned n_em, float t,
                                            const sh4der_spark_view_t *view, const sh4der_spark_screen_t *screen,
                                            sh4der_motes_draw_t out[SH4DER_MOTES_DRAW_MAX]) {
    unsigned e, s, n = 0, low = 0, slots;

    if (!em || !view || !screen || !out || !sh4der_motes_params_valid(p) || !sh4der_finite_(t) || t < 0.0f ||
        !(view->near_z > 0.0f) || !(view->focal_px > 0.0f) || !(screen->focal_y_px > 0.0f)) return 0;
    slots = sh4der_motes_slots(p);
    for (e = 0; e < n_em; e++) {
        const float d[3] = { em[e].anchor[0] - view->eye[0], em[e].anchor[1] - view->eye[1], em[e].anchor[2] - view->eye[2] };

        if (sh4der_len3_(d[0], d[1], d[2]) > p->lod_cut * p->lod_cut) continue;   /* len3_ is the squared length */
        for (s = 0; s < slots; s++) {
            sh4der_motes_ember_t m;
            sh4der_motes_draw_t q;
            float c[3], true_px, px, k, ratio;

            if (!sh4der_motes_slot(p, &em[e], s, t, &m)) continue;
            sh4der_spark_to_view_(view, m.pos, c);
            if (c[2] < view->near_z) continue;
            true_px = sh4der_div_(m.size * view->focal_px, c[2]);
            px = true_px < p->min_px ? p->min_px : (true_px > p->max_px ? p->max_px : true_px);
            ratio = sh4der_div_(true_px, p->min_px);
            k = ratio < 1.0f ? m.k * ratio * ratio : m.k;   /* a sprite under the least size keeps its light */
            if (px > p->big_px) k *= p->big_k;
            if (k < p->cull_k) continue;
            if (sh4der_div_(c[0] * view->focal_px, c[2]) < -(screen->cx + px) || sh4der_div_(c[0] * view->focal_px, c[2]) > screen->cx + px ||
                sh4der_div_(c[1] * screen->focal_y_px, c[2]) < -(screen->cy + px) || sh4der_div_(c[1] * screen->focal_y_px, c[2]) > screen->cy + px) continue;
            q.c[0] = c[0]; q.c[1] = c[1]; q.c[2] = c[2];
            q.half = sh4der_div_(px * c[2], 2.0f * view->focal_px);
            q.score = k * px * px;
            q.argb = sh4der_motes_color_(p, k);
            q.flat = px <= p->flat_px;
            q.pad_ = 0u;
            if (n < p->max_draw) {
                out[n++] = q;
                if (n == p->max_draw) {                       /* the buffer is full: find the weakest to replace */
                    unsigned i;

                    for (low = 0, i = 1; i < n; i++) if (out[i].score < out[low].score) low = i;
                }
            } else if (q.score > out[low].score) {
                unsigned i;

                out[low] = q;
                for (low = 0, i = 1; i < n; i++) if (out[i].score < out[low].score) low = i;
            }
        }
    }
    return n;
}

/** \brief The four corners of one sprite in view space, strip order, with the full mask UV and its colour. The
    two triangles are (0 1 2) and (1 2 3). Returns 1, or 0 for a bad record. No corner is nearer than the centre
    minus half, and a sprite is flat to the screen (all four corners share the centre's depth). */
static inline int sh4der_motes_quad(const sh4der_motes_draw_t *d, sh4der_spark_vtx_t out[4]) {
    int k;

    if (!d || !(d->half > 0.0f) || !sh4der_finite_(d->half) || !(d->c[2] > 0.0f)) return 0;
    for (k = 0; k < 4; k++) {
        out[k].x = d->c[0] + ((k & 2) ? d->half : -d->half);
        out[k].y = d->c[1] + ((k & 1) ? -d->half : d->half);
        out[k].z = d->c[2];
        out[k].u = (k & 2) ? 1.0f : 0.0f;
        out[k].v = (k & 1) ? 1.0f : 0.0f;
        out[k].argb = d->argb;
    }
    return 1;
}

/** \brief Fills a 16 x 16 ARGB1555 mask (row by row, not twiddled): 0xFFFF inside a round dot, 0x0000 outside. The
    KallistiOS loader (pvr_txr_load_ex with PVR_TXRLOAD_16BPP) twiddles it. */
static inline void sh4der_motes_mask_make(uint16_t out[SH4DER_MOTES_MASK_SIDE * SH4DER_MOTES_MASK_SIDE]) {
    const float r = 0.5f * (float)SH4DER_MOTES_MASK_SIDE;   /* the dot's radius, texels: it fills the cell */
    unsigned x, y;

    for (y = 0; y < SH4DER_MOTES_MASK_SIDE; y++) {
        for (x = 0; x < SH4DER_MOTES_MASK_SIDE; x++) {
            const float dx = (float)x + 0.5f - r, dy = (float)y + 0.5f - r;

            out[y * SH4DER_MOTES_MASK_SIDE + x] = (dx * dx + dy * dy <= r * r) ? 0xFFFFu : 0x0000u;
        }
    }
}

/* ---- the KallistiOS draw (src/sh4der_motes.c; nothing off the Dreamcast) ---- */

/** \brief Compiles the punch-through header over a 16 x 16 mask the caller loaded into VRAM (ARGB1555 twiddled, 512 B).
    Hard dots: no filtering. Returns 1 when the header passes sh4der_poly_hdr_ok. */
int sh4der_motes_init_masked(void *texture, int txr_format);

/** \brief Compiles the untextured opaque header for the flat sprites. Returns 1 when it passes sh4der_poly_hdr_ok. */
int sh4der_motes_init_flat(void);

/** \brief Sends the collected sprites that are not flat (or all of them when `all`) inside the punch-through list.
    Depth test on, depth write on. Returns the quads sent. */
unsigned sh4der_motes_draw_masked(const sh4der_motes_draw_t *d, unsigned n, int all, const sh4der_spark_view_t *view,
                                  const sh4der_spark_screen_t *screen);

/** \brief Sends the collected flat sprites inside the opaque list, untextured. Returns the quads sent. */
unsigned sh4der_motes_draw_flat(const sh4der_motes_draw_t *d, unsigned n, const sh4der_spark_view_t *view,
                                const sh4der_spark_screen_t *screen);

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_MOTES_H */
