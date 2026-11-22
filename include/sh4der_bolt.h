/* SH4DER-PVR: branching bolts (electric discharge strands)

   sh4der_bolt.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_bolt.h
    \brief   Jagged, forking strands of light that flicker: lightning down a hull, an arc between two coils, a
             discharge across a gap. A few strands, each a polyline with forks, drawn as additive quads that keep a
             constant width on the screen, plus a wider and fainter halo along the main strands.

    The shape: each strand starts at an anchor (a root point, a direction, a length, all in world space). Its main
    polyline has `segs` segments that step along the direction and wander sideways by a share of their own length;
    `forks` side branches leave it at random points, each `fork_segs` segments long, turned away from the strand by
    an angle between fork_lo and fork_hi. Every point is stored as an offset from the root, so moving the anchor moves
    the whole bolt without a re-roll.

    The flicker: every `reroll_s` seconds the strand is re-rolled but keeps its first `keep_segs` segments, so each
    new picture shares part of its shape with the one before (a strand shifts, it does not all vanish). A fork that
    leaves the kept part of its strand is kept too; the others are drawn again. The brightness of the whole bolt takes
    a new gain 1 +- gain_jitter at each re-roll. A bolt is present in every picture (nothing is ever switched off).

    The width: a strand segment is drawn `width_px` pixels wide whatever its distance (the width in the world is worked
    out from the segment's depth), so a far strand stays a line and a near one is not a rope; the halo is halo_scale
    times wider at halo_gain of the brightness, on the main polyline only. The quads and their near plane handling are
    sh4der_spark_quad's.

    Per frame: sh4der_bolt_update(bolt, params, anchors, dt) once, then inside the caller's translucent list
    sh4der_bolt_draw (KallistiOS, src/sh4der_bolt.c), or sh4der_bolt_quad for a client that submits its own vertices.

    Memory: the bolt state is 3,328 B at the largest size (8 strands), the caller's; the draw holds one polygon header
    (32 B). Defaults (6 strands, 8 segments, 2 forks of 3): 84 core quads + 48 halo quads = 264 triangles, 528
    vertices a frame. VRAM 0 (untextured). Sound RAM 0. The random numbers are a linear congruential step kept in the
    state (no libm); the maths is SH4ZAM through sh4der_core.h (normalise, cross, fused multiply-add, divide).
    Floats in arrays the caller owns (4-byte aligned); nothing casts a byte pointer.

    Version: v0.10
    The effect: thin jagged strands run down a surface, branching, and the pattern shifts a few times a second.
    Developed for: lightning around a large ship.
    Use it for: lightning, an arc, a spark gap, a discharge, cracks of light, a lit root system.

    \author Jilted Valkyrie
    \author Junior (Claude, an AI by Anthropic): the effect
*/

#ifndef SH4DER_BOLT_H
#define SH4DER_BOLT_H

#include "sh4der_spark.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Limits and default tuning
    @{ */
#define SH4DER_BOLT_STRANDS_MAX   8       /**< \brief Strands a bolt holds. */
#define SH4DER_BOLT_SEGS_MAX      12      /**< \brief Segments of a main polyline. */
#define SH4DER_BOLT_FORKS_MAX     3       /**< \brief Forks a strand holds. */
#define SH4DER_BOLT_FORK_SEGS_MAX 6       /**< \brief Segments of a fork. */
#define SH4DER_BOLT_STRANDS       6       /**< \brief Default strands (a visible branch count of 4 - 8 with the forks). */
#define SH4DER_BOLT_SEGS          8       /**< \brief Default main segments. */
#define SH4DER_BOLT_FORKS         2       /**< \brief Default forks a strand. */
#define SH4DER_BOLT_FORK_SEGS     3       /**< \brief Default fork segments. */
#define SH4DER_BOLT_KEEP_SEGS     4       /**< \brief Main segments a re-roll keeps (half of a strand). */
#define SH4DER_BOLT_WANDER        0.45f   /**< \brief Sideways step as a share of the segment length, each of the two sideways axes. */
#define SH4DER_BOLT_SEG_JITTER    0.25f   /**< \brief Segment length varies by +- this share. */
#define SH4DER_BOLT_FORK_LEN      0.35f   /**< \brief Fork length as a share of the strand length. */
#define SH4DER_BOLT_FORK_LO       0.50f   /**< \brief Least fork angle from the strand, radians (about 29 degrees). */
#define SH4DER_BOLT_FORK_HI       0.90f   /**< \brief Largest fork angle, radians (about 52 degrees). */
#define SH4DER_BOLT_WIDTH_PX      2.0f    /**< \brief Strand width on the screen, pixels. */
#define SH4DER_BOLT_HALO_SCALE    3.0f    /**< \brief Halo width as a multiple of the strand width. */
#define SH4DER_BOLT_HALO_GAIN     0.35f   /**< \brief Halo brightness as a share of the strand's. */
#define SH4DER_BOLT_FORK_GAIN     0.70f   /**< \brief Fork brightness as a share of the main strand's. */
#define SH4DER_BOLT_REROLL_S      0.1333f /**< \brief Seconds between re-rolls: 4 pictures at 30 a second. */
#define SH4DER_BOLT_GAIN_JITTER   0.25f   /**< \brief Brightness of a re-roll is 1 +- this. */
#define SH4DER_BOLT_CORE_ARGB     0xFFFFEBF0u   /**< \brief Peak colour of the core strand (255, 235, 240). */
#define SH4DER_BOLT_HALO_ARGB     0xFFFF968Cu   /**< \brief Peak colour of the halo (255, 150, 140). */
/** @} */

/** \brief The look of a bolt. Fill it with sh4der_bolt_params_default and change what you need. */
typedef struct sh4der_bolt_params {
    uint32_t strands, segs, forks, fork_segs, keep_segs;
    float wander, seg_jitter, fork_len, fork_lo, fork_hi;
    float width_px, halo_scale, halo_gain, fork_gain;
    float reroll_s, gain_jitter;
    uint32_t core_argb, halo_argb;   /**< \brief Peak colours (the alpha byte is not used); the gain scales them down from their peak. */
} sh4der_bolt_params_t;

/** \brief Where one strand starts, in world space: root, unit direction it runs along, length. Pass strands of them. */
typedef struct sh4der_bolt_anchor {
    float root[3];
    float dir[3];
    float length;
} sh4der_bolt_anchor_t;

/** \brief The state of a bolt: the shape of every strand as offsets from its root, the random state, the clock. */
typedef struct sh4der_bolt {
    float main[SH4DER_BOLT_STRANDS_MAX][SH4DER_BOLT_SEGS_MAX + 1][3];   /**< \brief Main points, offset from the root; point 0 is 0. */
    float fork[SH4DER_BOLT_STRANDS_MAX][SH4DER_BOLT_FORKS_MAX][SH4DER_BOLT_FORK_SEGS_MAX + 1][3];   /**< \brief Fork points, offset from the root; point 0 sits on the main strand. */
    uint8_t fork_at[SH4DER_BOLT_STRANDS_MAX][SH4DER_BOLT_FORKS_MAX];    /**< \brief The main point a fork leaves from. */
    uint32_t rnd;      /**< \brief Random state. */
    uint32_t rolls;    /**< \brief Re-rolls so far (the first roll is 0). */
    float clock;       /**< \brief Seconds since the last re-roll. */
    float gain;        /**< \brief Brightness gain of the current roll, 1 +- gain_jitter. */
} sh4der_bolt_t;

/** \brief The default look: the values above. */
static inline void sh4der_bolt_params_default(sh4der_bolt_params_t *p) {
    p->strands = SH4DER_BOLT_STRANDS; p->segs = SH4DER_BOLT_SEGS; p->forks = SH4DER_BOLT_FORKS;
    p->fork_segs = SH4DER_BOLT_FORK_SEGS; p->keep_segs = SH4DER_BOLT_KEEP_SEGS;
    p->wander = SH4DER_BOLT_WANDER; p->seg_jitter = SH4DER_BOLT_SEG_JITTER; p->fork_len = SH4DER_BOLT_FORK_LEN;
    p->fork_lo = SH4DER_BOLT_FORK_LO; p->fork_hi = SH4DER_BOLT_FORK_HI;
    p->width_px = SH4DER_BOLT_WIDTH_PX; p->halo_scale = SH4DER_BOLT_HALO_SCALE; p->halo_gain = SH4DER_BOLT_HALO_GAIN;
    p->fork_gain = SH4DER_BOLT_FORK_GAIN; p->reroll_s = SH4DER_BOLT_REROLL_S; p->gain_jitter = SH4DER_BOLT_GAIN_JITTER;
    p->core_argb = SH4DER_BOLT_CORE_ARGB; p->halo_argb = SH4DER_BOLT_HALO_ARGB;
}

/** \brief 1 when every field is in range and finite: strands 1 .. 8, segs 3 .. 12, forks 0 .. 3, fork_segs 1 .. 6 (forks 0 ignores it),
    keep_segs 0 .. segs, shares in 0 .. 1 (wander 0 .. 2), fork angles 0 < lo <= hi < pi / 2 + 0.5, width_px 0.1 .. 16,
    halo_scale 1 .. 16, reroll_s above 0. */
static inline int sh4der_bolt_params_valid(const sh4der_bolt_params_t *p) {
    if (!p || p->strands < 1 || p->strands > SH4DER_BOLT_STRANDS_MAX || p->segs < 3 || p->segs > SH4DER_BOLT_SEGS_MAX ||
        p->forks > SH4DER_BOLT_FORKS_MAX || p->keep_segs > p->segs) return 0;
    if (p->forks && (p->fork_segs < 1 || p->fork_segs > SH4DER_BOLT_FORK_SEGS_MAX)) return 0;
    if (!sh4der_finite_(p->wander) || p->wander < 0.0f || p->wander > 2.0f) return 0;
    if (!sh4der_finite_(p->seg_jitter) || p->seg_jitter < 0.0f || p->seg_jitter > 0.9f) return 0;
    if (!sh4der_finite_(p->fork_len) || p->fork_len <= 0.0f || p->fork_len > 1.0f) return 0;
    if (!sh4der_finite_(p->fork_lo) || !sh4der_finite_(p->fork_hi) || !(p->fork_lo > 0.0f) || p->fork_lo > p->fork_hi || p->fork_hi > 2.0f) return 0;
    if (!sh4der_finite_(p->width_px) || p->width_px < 0.1f || p->width_px > 16.0f) return 0;
    if (!sh4der_finite_(p->halo_scale) || p->halo_scale < 1.0f || p->halo_scale > 16.0f) return 0;
    if (!sh4der_finite_(p->halo_gain) || p->halo_gain < 0.0f || p->halo_gain > 1.0f) return 0;
    if (!sh4der_finite_(p->fork_gain) || p->fork_gain < 0.0f || p->fork_gain > 1.0f) return 0;
    if (!sh4der_finite_(p->reroll_s) || !(p->reroll_s > 0.0f)) return 0;
    if (!sh4der_finite_(p->gain_jitter) || p->gain_jitter < 0.0f || p->gain_jitter > 0.9f) return 0;
    return 1;
}

/** \brief The next random number 0 .. under 1 (24 bits; a linear congruential step, state in the bolt). */
static inline float sh4der_bolt_rand_(sh4der_bolt_t *b) {
    b->rnd = b->rnd * 1664525u + 1013904223u;
    return (float)(b->rnd >> 8) * (1.0f / 16777216.0f);
}

/* A random number -1 .. 1. */
static inline float sh4der_bolt_srand_(sh4der_bolt_t *b) {
    return sh4der_fma_(sh4der_bolt_rand_(b), 2.0f, -1.0f);
}

/* Two unit vectors across the unit direction d (any pair that, with d, is a frame). */
static inline void sh4der_bolt_basis_(const float d[3], float t1[3], float t2[3]) {
    float ax[3] = { 1.0f, 0.0f, 0.0f };

    if (d[0] > 0.9f || d[0] < -0.9f) { ax[0] = 0.0f; ax[1] = 1.0f; }      /* any axis not along d */
    sh4der_cross_(d, ax, t1);
    sh4der_norm3_(t1);
    sh4der_cross_(d, t1, t2);
    sh4der_norm3_(t2);
}

/* One step of a wandering polyline: out = from + dir x step + (t1 x srand + t2 x srand) x wander x step. */
static inline void sh4der_bolt_step_(sh4der_bolt_t *b, const float from[3], const float dir[3], const float t1[3], const float t2[3],
                                     float step, float wander, float out[3]) {
    const float s1 = sh4der_bolt_srand_(b) * wander * step, s2 = sh4der_bolt_srand_(b) * wander * step;
    int k;

    for (k = 0; k < 3; k++) out[k] = sh4der_fma_(dir[k], step, sh4der_fma_(t1[k], s1, sh4der_fma_(t2[k], s2, from[k])));
}

/* Rolls strand i. The main points from index `from` on are drawn again (points 0 .. from - 1 stay); a fork whose branch point lies in the kept part
   stays, every other fork is drawn again. */
static inline void sh4der_bolt_roll_strand_(sh4der_bolt_t *b, const sh4der_bolt_params_t *p, const sh4der_bolt_anchor_t *a, unsigned i, unsigned from) {
    float d[3], t1[3], t2[3];
    unsigned s, f, k;
    const float seg = sh4der_div_(a->length, (float)p->segs);

    d[0] = a->dir[0]; d[1] = a->dir[1]; d[2] = a->dir[2];
    sh4der_norm3_(d);
    sh4der_bolt_basis_(d, t1, t2);
    if (from < 1) {
        b->main[i][0][0] = b->main[i][0][1] = b->main[i][0][2] = 0.0f;
        from = 1;
    }
    for (s = from; s <= p->segs; s++)
        sh4der_bolt_step_(b, b->main[i][s - 1], d, t1, t2, seg * sh4der_fma_(sh4der_bolt_srand_(b), p->seg_jitter, 1.0f), p->wander, b->main[i][s]);
    for (f = 0; f < p->forks; f++) {
        float u[3], sp, cp, st, ct, ang, fseg;
        unsigned at;

        if (b->fork_at[i][f] >= 1 && b->fork_at[i][f] < p->segs && b->fork_at[i][f] <= from - 1) continue;   /* leaves the kept part: kept */
        at = 1u + (unsigned)(sh4der_bolt_rand_(b) * (float)(p->segs - 2u));   /* a main point 1 .. segs - 2 (the strand has a tail after it) */
        if (at > p->segs - 2u) at = p->segs - 2u;
        b->fork_at[i][f] = (uint8_t)at;
        ang = sh4der_lerp_(p->fork_lo, p->fork_hi, sh4der_bolt_rand_(b));
        sh4der_sincos_(ang, &st, &ct);
        sh4der_sincos_(6.28318530718f * sh4der_bolt_rand_(b), &sp, &cp);
        for (k = 0; k < 3; k++) u[k] = sh4der_fma_(d[k], ct, sh4der_fma_(t1[k], cp, t2[k] * sp) * st);
        fseg = sh4der_div_(a->length * p->fork_len, (float)p->fork_segs);
        for (k = 0; k < 3; k++) b->fork[i][f][0][k] = b->main[i][at][k];
        for (s = 1; s <= p->fork_segs; s++) sh4der_bolt_step_(b, b->fork[i][f][s - 1], u, t1, t2, fseg, p->wander * 0.5f, b->fork[i][f][s]);
    }
}

/** \brief Starts a bolt: clears it, seeds the random state (any value), rolls every strand for the anchors. Returns 1, or 0 on bad params or
    anchors (a non-finite value, a zero direction, a length that is not above 0), leaving the bolt cleared. */
static inline int sh4der_bolt_init(sh4der_bolt_t *b, const sh4der_bolt_params_t *p, const sh4der_bolt_anchor_t *anchors, uint32_t seed) {
    unsigned i, j, k;

    if (!b) return 0;
    for (i = 0; i < SH4DER_BOLT_STRANDS_MAX; i++) {
        for (j = 0; j <= SH4DER_BOLT_SEGS_MAX; j++) b->main[i][j][0] = b->main[i][j][1] = b->main[i][j][2] = 0.0f;
        for (j = 0; j < SH4DER_BOLT_FORKS_MAX; j++) {
            b->fork_at[i][j] = 0;
            for (k = 0; k <= SH4DER_BOLT_FORK_SEGS_MAX; k++) b->fork[i][j][k][0] = b->fork[i][j][k][1] = b->fork[i][j][k][2] = 0.0f;
        }
    }
    b->rnd = seed ? seed : 0x2545F491u;
    b->rolls = 0; b->clock = 0.0f; b->gain = 1.0f;
    if (!sh4der_bolt_params_valid(p) || !anchors) return 0;
    for (i = 0; i < p->strands; i++) {
        const sh4der_bolt_anchor_t *a = &anchors[i];

        if (!sh4der_finite_(a->root[0]) || !sh4der_finite_(a->root[1]) || !sh4der_finite_(a->root[2]) || !sh4der_finite_(a->dir[0]) ||
            !sh4der_finite_(a->dir[1]) || !sh4der_finite_(a->dir[2]) || !sh4der_finite_(a->length) || !(a->length > 0.0f) ||
            sh4der_len3_(a->dir[0], a->dir[1], a->dir[2]) < 1.0e-12f) return 0;
    }
    for (i = 0; i < p->strands; i++) sh4der_bolt_roll_strand_(b, p, &anchors[i], i, 0);
    return 1;
}

/** \brief Once a frame: adds dt seconds to the clock; when it reaches reroll_s the bolt is re-rolled (the first keep_segs segments of every strand stay,
    the rest and the forks that left the rest are drawn again) with a new brightness gain. At most one re-roll a call (a long pause is one flicker, not a
    burst). A dt that is not finite or not above 0 changes nothing. */
static inline void sh4der_bolt_update(sh4der_bolt_t *b, const sh4der_bolt_params_t *p, const sh4der_bolt_anchor_t *anchors, float dt) {
    unsigned i;

    if (!b || !anchors || !sh4der_bolt_params_valid(p) || !sh4der_finite_(dt) || !(dt > 0.0f)) return;
    b->clock += dt;
    if (b->clock < p->reroll_s) return;
    b->clock -= p->reroll_s;
    if (b->clock >= p->reroll_s) b->clock = 0.0f;
    for (i = 0; i < p->strands; i++) sh4der_bolt_roll_strand_(b, p, &anchors[i], i, p->keep_segs + 1u);
    b->gain = sh4der_fma_(sh4der_bolt_srand_(b), p->gain_jitter, 1.0f);
    b->rolls++;
}

/** \brief Segments a bolt draws (core quads): strands x (segs + forks x fork_segs). */
static inline unsigned sh4der_bolt_segments(const sh4der_bolt_params_t *p) {
    return p->strands * (p->segs + p->forks * p->fork_segs);
}

/** \brief Segment k (0 .. sh4der_bolt_segments - 1) in world space: the tail and the head, whether it belongs to a fork. Returns 0 for k out of range. */
static inline int sh4der_bolt_segment(const sh4der_bolt_t *b, const sh4der_bolt_params_t *p, const sh4der_bolt_anchor_t *anchors, unsigned k,
                                      float tail[3], float head[3], int *is_fork) {
    const unsigned per = p->segs + p->forks * p->fork_segs;
    unsigned i, j;
    const float *t, *h;
    int c;

    if (k >= sh4der_bolt_segments(p)) return 0;
    i = k / per; j = k % per;
    if (j < p->segs) {
        t = b->main[i][j]; h = b->main[i][j + 1];
        *is_fork = 0;
    } else {
        const unsigned f = (j - p->segs) / p->fork_segs, m = (j - p->segs) % p->fork_segs;

        t = b->fork[i][f][m]; h = b->fork[i][f][m + 1];
        *is_fork = 1;
    }
    for (c = 0; c < 3; c++) { tail[c] = t[c] + anchors[i].root[c]; head[c] = h[c] + anchors[i].root[c]; }
    return 1;
}

/** \brief The four corners of one quad in view space (sh4der_spark_quad's, strip order, triangles (0 1 2) and (1 2 3)): segment k, the halo ribbon of it
    when `halo` is set (main segments only; a fork has no halo and returns 0), the colour at the bolt's current gain, the width the params ask for in pixels at
    this segment's depth. `view` is the spark view (cols and rows 1: nothing is textured). Returns 1, or 0 when there is nothing to draw (a bad k, a fork's
    halo, the segment wholly nearer than the near plane, a bad view). Test each triangle with sh4der_tri_area_ok before sending (sh4der_bolt_draw does). */
static inline int sh4der_bolt_quad(const sh4der_bolt_t *b, const sh4der_bolt_params_t *p, const sh4der_bolt_anchor_t *anchors, unsigned k, int halo,
                                   const sh4der_spark_view_t *view, sh4der_spark_vtx_t out[4]) {
    sh4der_spark_t s;
    float mid[3], z, level;
    int fork, c;
    uint32_t peak, g;

    if (!view || !(view->focal_px > 0.0f) || !sh4der_bolt_segment(b, p, anchors, k, s.q, s.p, &fork)) return 0;
    if (halo && fork) return 0;
    for (c = 0; c < 3; c++) mid[c] = (s.p[c] + s.q[c]) * 0.5f - view->eye[c];
    z = sh4der_dot3_(mid, view->fwd);
    if (!(z > 0.0f)) return 0;                                       /* behind the eye: sh4der_spark_quad would cut it, but its width has no meaning */
    s.v[0] = s.v[1] = s.v[2] = 0.0f;
    s.width = sh4der_div_(p->width_px * z, view->focal_px) * (halo ? p->halo_scale : 1.0f);
    level = b->gain * sh4der_div_(1.0f, 1.0f + p->gain_jitter);      /* the peak colour is the brightest roll (gain 1 + jitter) */
    if (fork) level *= p->fork_gain;
    if (halo) level *= p->halo_gain;
    g = (uint32_t)sh4der_fma_(sh4der_sat_(level), 256.0f, 0.0f);
    peak = halo ? p->halo_argb : p->core_argb;
    s.argb = sh4der_spark_scale_(peak, g > 256u ? 256u : g);
    s.age_ms = 0; s.life_ms = 1000u;                                 /* full gain: the spark's own age fade is not used */
    s.shape = SH4DER_SPARK_STREAK; s.cell = 0; s.flags = 0; s.pad_ = 0;
    return sh4der_spark_quad(&s, view, out);
}

/* ---- the KallistiOS draw (src/sh4der_bolt.c; nothing off the Dreamcast) ---- */

/** \brief Compile the polygon header: translucent list, untextured, ONE / ONE (additive), depth test on, depth write off, no culling. Call once (and
    again if the list setup changed). Returns 1, or 0 when the header did not compile (the draw then draws nothing). */
int sh4der_bolt_init_draw(void);

/** \brief Inside the caller's translucent list: one header, the halo ribbons, then every core quad that has screen area. Returns the quads sent. */
unsigned sh4der_bolt_draw(const sh4der_bolt_t *b, const sh4der_bolt_params_t *p, const sh4der_bolt_anchor_t *anchors, const sh4der_spark_view_t *view,
                          const sh4der_spark_screen_t *screen);

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_BOLT_H */
