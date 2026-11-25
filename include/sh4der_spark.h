/* SH4DER-PVR: pooled additive sparks

   sh4der_spark.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
   The flash and tracer rules (one additive quad for a flash; a line from the muzzle
   to the hit whose muzzle end retreats, with a least width in pixels) follow
   Halo: Dreamcast Evolved's src/fx.c and src/bullet_tracer.h, same author.

   RECOMMENDED (from the wiki ledger, one console; wiki/limits/header-recommended.md)
   Start with: SH4DER_SPARK_MAX = 24 (96 vertices a frame at most), SH4DER_SPARK_MIN_PX = 0.8,
               SH4DER_SPARK_INSET = 0.0078125 for a 64 x 64 atlas (VQ: 3 KB); one additive header a frame
   Cost:       not measured: no ledger row names it (the street demo draws its halos with sh4der_spark_quad).
               ESTIMATE: 96 vertices at 142 to 260 ns a vertex sent (BUS-001, BUS-003), under 25 us of SH-4;
               the PVR's fill follows the quads' size on screen
   Status:     ADVISED; to prove: a stage with the pool full against empty, PVR and SH-4 apart
   Do:         give SH4DER_SPARK_KEEP only to what must never be pushed out (a flash, a tracer)
   Do not:     draw outside the translucent list or before the world; send your own quads without the
               area test (sh4der_tri_area_ok)
   Maturity:   PROTOTYPE v0.1
*/

/** \file    sh4der_spark.h
    \brief   A fixed pool of short-lived additive quads that face the camera: a
             billboard (a flash, a glint, a puff of light) and a streak (a tracer, a
             spark, a beam).

    The PVR adds for free: a quad on the translucent list with a ONE / ONE blend
    lights what is behind it and needs no sorting to look right. A muzzle flash, a
    tracer, an impact flash and its sparks are all that, a few quads for a few
    frames. This is the pool they live in and the two shapes they are drawn with.
    What a flash looks like (its size, colour, life, which cell of the atlas) is
    the caller's table; nothing here knows a weapon.

    Shapes (world space in, the view is given at draw time):
    - BILLBOARD: a centre, a half size, a spin. A square that faces the camera,
      turned about the view axis by the spin.
    - STREAK: a tail and a head. A quad along the line, turned about its own axis
      to face the camera, a width across. Its half width never falls under
      min_px / 2 pixels on screen, so a far tracer stays a line and does not
      flicker. Flags: RETREAT (the tail moves to the head over the life: a tracer
      that is swallowed by its hit), TAIL0 (the tail vertices are black: the
      streak fades along its length, no hard end), GRAVITY (the update adds the
      gravity vector to its velocity). A streak with a velocity moves whole.

    Brightness: colour x (1 - age / life), linear, in the vertex colour. The blend
    is additive with no alpha, so a dying quad just adds less. Give the birth
    colour already scaled (a tracer at 65 % is its colour x 0.65).

    The pool: SH4DER_SPARK_MAX quads, no allocation. When it is full a new quad
    takes the place of the one nearest its end among those without the KEEP flag;
    a new KEEP quad may also push out the KEEP quad nearest its end; a new quad
    without KEEP is dropped (counted) when everything in the pool is KEEP. So
    sparks die first and a flash or a tracer is never lost to a spark.

    The atlas: one texture cut into cols x rows equal cells; a quad names its
    cell. An additive atlas wants no alpha: black adds nothing (RGB565, VQ or
    not). A billboard maps the whole cell; a streak maps u along its length
    (tail to head), v across. The inset keeps the bilinear read inside the cell.

    Per frame: sh4der_spark_update(pool, dt, gravity), then inside the caller's
    translucent list sh4der_spark_draw (KallistiOS, src/sh4der_spark.c), or
    sh4der_spark_quad a quad for a client that submits its own vertices. Both
    guards of sh4der_guard.h are applied by the draw: a quad with no screen area
    is dropped, an uncompiled header draws nothing.

    Memory: the pool is SH4DER_SPARK_MAX x 52 B + 8 B (24: 1,256 B), the caller's;
    the draw holds one polygon header (32 B). VRAM: the caller's atlas (64 x 64
    VQ: 3 KB). Sound RAM (AICA): 0. TA: 4 vertices a quad, one header a frame.

    No allocation, no globals in the header. Floats in arrays the caller owns
    (4-byte aligned); nothing casts a byte pointer. The maths is SH4ZAM through
    sh4der_core.h (normalise, cross, dot, lerp, fused multiply-add, sine and
    cosine, divide); define SH4DER_CORE_NO_SH4ZAM for the plain C path. Integer
    by hand (no SH4ZAM equivalent): the colour byte scale, the pool search, the
    random step.

    Version: v0.10
    The effect: a gun flashes, a tracer draws to its hit and is gone, sparks fly
      off a wall and fall.
    Developed for: a third-person shooter's weapon effects.
    Use it for: muzzle flashes, tracers, impact sparks, a welding torch, a
      sparkler, fireflies, any few dozen bright short-lived things.

    \author Jilted Valkyrie

    PVR terms (TA, TR/OP list, EOL, twiddled, direct render): see the PVR glossary in README.md.
*/

#ifndef SH4DER_SPARK_H
#define SH4DER_SPARK_H

#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_SPARK_MAX      24        /**< \brief Quads the pool holds (96 vertices a frame at most). */
#define SH4DER_SPARK_MIN_PX   0.8f      /**< \brief Least streak width on screen, pixels. */
#define SH4DER_SPARK_INSET    0.0078125f /**< \brief Cell edge kept out of the read, in texture units: half a texel of a 64 texel atlas. */
#define SH4DER_SPARK_LIFE_MAX 60.0f     /**< \brief Longest life, seconds (the age counter is 16 bit milliseconds). */
/** @} */

#define SH4DER_SPARK_FREE      0u   /**< \brief Shape of an empty slot. */
#define SH4DER_SPARK_BILLBOARD 1u   /**< \brief Shape of a camera-facing square sprite (a flash, a spark dot); q[0] half size, q[1] spin. */
#define SH4DER_SPARK_STREAK    2u   /**< \brief Shape of a line sprite from head to tail (a tracer, a spark's trail), turned to face the camera; q is the tail. */

#define SH4DER_SPARK_KEEP    1u     /**< \brief Never pushed out by a quad without KEEP (a flash, a tracer). */
#define SH4DER_SPARK_RETREAT 2u     /**< \brief Streak: the tail moves to the head over the life. */
#define SH4DER_SPARK_TAIL0   4u     /**< \brief Streak: the tail vertices are black (fades along its length). */
#define SH4DER_SPARK_GRAVITY 8u     /**< \brief The update adds gravity x dt to the velocity. */

/** \brief One quad of the pool (52 B). */
typedef struct sh4der_spark {
    float p[3];        /**< \brief Billboard: centre. Streak: head. World space. */
    float q[3];        /**< \brief Streak: tail. Billboard: q[0] half size, q[1] spin (radians), q[2] unused. */
    float v[3];        /**< \brief Velocity, world units a second (0 0 0: it stays). */
    float width;       /**< \brief Streak: width across, world units. */
    uint32_t argb;     /**< \brief Colour at birth (the alpha byte is not used). */
    uint16_t age_ms, life_ms;
    uint8_t shape, cell, flags, pad_;
} sh4der_spark_t;

/** \brief The pool. Zero it (or sh4der_spark_reset) before the first spawn. */
typedef struct sh4der_spark_pool {
    sh4der_spark_t s[SH4DER_SPARK_MAX];
    uint32_t rnd;                /**< \brief Random state (sh4der_spark_rand). */
    uint16_t live, dropped;      /**< \brief Quads alive; spawns refused since the reset (pool full of KEEP, or bad arguments). */
} sh4der_spark_pool_t;

/** \brief The camera and the atlas for a draw. right, up, fwd: the camera's unit axes in world space
    (x right, y up, z into the screen); eye: its position. */
typedef struct sh4der_spark_view {
    float right[3], up[3], fwd[3], eye[3];
    float near_z;      /**< \brief Near plane depth (> 0): a billboard nearer is dropped, a streak is cut there. */
    float focal_px;    /**< \brief Pixels of screen x for x / z = 1 (half the width / tan of half the horizontal field). */
    float min_px;      /**< \brief Least streak width, pixels (SH4DER_SPARK_MIN_PX). */
    float inset;       /**< \brief Cell edge kept out of the read, texture units (SH4DER_SPARK_INSET). */
    uint8_t cols, rows;   /**< \brief Atlas cells across and down (at least 1 each). */
} sh4der_spark_view_t;

/** \brief One corner of a quad in view space (x right, y up, z depth > 0). Strip order: 0 1 2 3. */
typedef struct sh4der_spark_vtx {
    float x, y, z, u, v;
    uint32_t argb;     /**< \brief 0xFF and the colour x the life gain (black on a TAIL0 tail). */
} sh4der_spark_vtx_t;

/** \brief Empties the pool: every slot becomes free and the live count 0. Call once before the first spawn, or to clear all sparks. */
static inline void sh4der_spark_reset(sh4der_spark_pool_t *pool) {
    unsigned i;

    for (i = 0; i < SH4DER_SPARK_MAX; i++) pool->s[i].shape = SH4DER_SPARK_FREE;
    pool->rnd = 0x2545F491u;
    pool->live = pool->dropped = 0;
}

/** \brief The next random number 0 .. under 1 (24 bits; a linear congruential step, state in the pool). */
static inline float sh4der_spark_rand(sh4der_spark_pool_t *pool) {
    pool->rnd = pool->rnd * 1664525u + 1013904223u;
    return (float)(pool->rnd >> 8) * (1.0f / 16777216.0f);
}

static inline int sh4der_spark_vec_ok_(const float a[3]) {
    return a && sh4der_finite_(a[0]) && sh4der_finite_(a[1]) && sh4der_finite_(a[2]);
}

/* 1: a is nearer its end than b (age / life larger), by cross multiplying: no divide. */
static inline int sh4der_spark_older_(const sh4der_spark_t *a, const sh4der_spark_t *b) {
    return (uint32_t)a->age_ms * b->life_ms > (uint32_t)b->age_ms * a->life_ms;
}

/* The slot a new quad takes (the pool rule in the file comment), or NULL: dropped. */
static inline sh4der_spark_t *sh4der_spark_slot_(sh4der_spark_pool_t *pool, float life, unsigned flags) {
    sh4der_spark_t *plain = NULL, *kept = NULL, *s;
    unsigned i;

    if (!sh4der_finite_(life) || !(life > 0.0f) || life > SH4DER_SPARK_LIFE_MAX) { pool->dropped++; return NULL; }
    for (i = 0; i < SH4DER_SPARK_MAX; i++) {
        s = &pool->s[i];
        if (s->shape == SH4DER_SPARK_FREE) { pool->live++; goto take; }
        if (s->flags & SH4DER_SPARK_KEEP) { if (!kept || sh4der_spark_older_(s, kept)) kept = s; }
        else if (!plain || sh4der_spark_older_(s, plain)) plain = s;
    }
    s = plain ? plain : (flags & SH4DER_SPARK_KEEP) ? kept : NULL;
    if (!s) { pool->dropped++; return NULL; }
take:
    {
        const uint32_t ms = (uint32_t)sh4der_fma_(life, 1000.0f, 0.5f);

        s->life_ms = (uint16_t)(ms ? ms : 1u);
        s->age_ms = 0;
        s->flags = (uint8_t)flags;
        s->pad_ = 0;
        s->v[0] = s->v[1] = s->v[2] = 0.0f;
        s->width = 0.0f;
    }
    return s;
}

/** \brief A billboard: centre (world), half size (world units, > 0), spin (radians), colour at birth, life
    (seconds, > 0), atlas cell, flags (KEEP). Returns 1, or 0 when it was dropped (bad arguments, or a pool full
    of KEEP quads and no KEEP flag). */
static inline int sh4der_spark_billboard(sh4der_spark_pool_t *pool, const float centre[3], float half, float spin,
                                         uint32_t argb, float life, unsigned cell, unsigned flags) {
    sh4der_spark_t *s;

    if (!sh4der_spark_vec_ok_(centre) || !sh4der_finite_(half) || !(half > 0.0f) || !sh4der_finite_(spin)) { pool->dropped++; return 0; }
    if (!(s = sh4der_spark_slot_(pool, life, flags & SH4DER_SPARK_KEEP))) return 0;
    s->p[0] = centre[0]; s->p[1] = centre[1]; s->p[2] = centre[2];
    s->q[0] = half; s->q[1] = spin; s->q[2] = 0.0f;
    s->argb = argb; s->cell = (uint8_t)cell; s->shape = SH4DER_SPARK_BILLBOARD;
    return 1;
}

/** \brief A streak from tail to head (world), a width across (world units, >= 0: the pixel floor still holds),
    a velocity (world units a second, NULL: it stays), colour at birth, life (seconds), atlas cell, flags (KEEP,
    RETREAT, TAIL0, GRAVITY). Returns 1, or 0 when it was dropped. */
static inline int sh4der_spark_streak(sh4der_spark_pool_t *pool, const float tail[3], const float head[3], const float vel[3],
                                      float width, uint32_t argb, float life, unsigned cell, unsigned flags) {
    sh4der_spark_t *s;

    if (!sh4der_spark_vec_ok_(tail) || !sh4der_spark_vec_ok_(head) || (vel && !sh4der_spark_vec_ok_(vel)) ||
        !sh4der_finite_(width) || width < 0.0f) { pool->dropped++; return 0; }
    if (!(s = sh4der_spark_slot_(pool, life, flags))) return 0;
    s->p[0] = head[0]; s->p[1] = head[1]; s->p[2] = head[2];
    s->q[0] = tail[0]; s->q[1] = tail[1]; s->q[2] = tail[2];
    if (vel) { s->v[0] = vel[0]; s->v[1] = vel[1]; s->v[2] = vel[2]; }
    s->width = width;
    s->argb = argb; s->cell = (uint8_t)cell; s->shape = SH4DER_SPARK_STREAK;
    return 1;
}

/** \brief A burst of n streaks from one point: each flies along dir (unit) turned by up to cone radians, at
    speed x (0.5 .. 1), len_min .. len_max long, with the GRAVITY flag. For impact sparks: dir is the shot
    reflected off the surface. Returns how many were made. */
static inline unsigned sh4der_spark_burst(sh4der_spark_pool_t *pool, const float at[3], const float dir[3], unsigned n, float cone,
                                          float speed, float len_min, float len_max, float width, uint32_t argb, float life,
                                          unsigned cell) {
    float d[3], t1[3], t2[3], ax[3] = { 1.0f, 0.0f, 0.0f };
    unsigned i, made = 0;

    if (!sh4der_spark_vec_ok_(at) || !sh4der_spark_vec_ok_(dir) || !sh4der_finite_(cone) || !sh4der_finite_(speed) ||
        !sh4der_finite_(len_min) || !sh4der_finite_(len_max)) { pool->dropped++; return 0; }
    d[0] = dir[0]; d[1] = dir[1]; d[2] = dir[2];
    sh4der_norm3_(d);
    if (d[0] > 0.9f || d[0] < -0.9f) { ax[0] = 0.0f; ax[1] = 1.0f; }      /* any axis not along d */
    sh4der_cross_(d, ax, t1);
    sh4der_norm3_(t1);
    sh4der_cross_(d, t1, t2);
    for (i = 0; i < n; i++) {
        float st, ct, sp, cp, u[3], head[3], vel[3];
        const float len = sh4der_lerp_(len_min, len_max, sh4der_spark_rand(pool));
        const float spd = speed * sh4der_fma_(sh4der_spark_rand(pool), 0.5f, 0.5f);
        int k;

        sh4der_sincos_(cone * sh4der_spark_rand(pool), &st, &ct);
        sh4der_sincos_(6.28318530718f * sh4der_spark_rand(pool), &sp, &cp);
        for (k = 0; k < 3; k++) {
            u[k] = sh4der_fma_(d[k], ct, sh4der_fma_(t1[k], cp, t2[k] * sp) * st);
            head[k] = sh4der_fma_(u[k], len, at[k]);
            vel[k] = u[k] * spd;
        }
        made += (unsigned)sh4der_spark_streak(pool, at, head, vel, width, argb, life, cell, SH4DER_SPARK_GRAVITY);
    }
    return made;
}

/** \brief Once a frame: ages every quad by dt (seconds), frees the ones past their life, moves the ones with a
    velocity, adds gravity (world units a second squared, NULL: none) to the GRAVITY ones. A quad with under a
    sixteenth of this frame left is over, so a life of n frames is drawn n times. A dt that is not finite or not
    positive changes nothing. */
static inline void sh4der_spark_update(sh4der_spark_pool_t *pool, float dt, const float gravity[3]) {
    unsigned i;
    uint32_t ms;

    if (!sh4der_finite_(dt) || !(dt > 0.0f)) return;
    if (dt > 1.0f) dt = 1.0f;
    ms = (uint32_t)sh4der_fma_(dt, 1000.0f, 0.5f);
    if (!ms) ms = 1u;
    for (i = 0; i < SH4DER_SPARK_MAX; i++) {
        sh4der_spark_t *s = &pool->s[i];
        int k;

        if (s->shape == SH4DER_SPARK_FREE) continue;
        if ((uint32_t)s->age_ms + ms + (ms >> 4) >= s->life_ms) { s->shape = SH4DER_SPARK_FREE; pool->live--; continue; }   /* under 1/16 of a frame left: over (a 2 frame life at 30 Hz is 2 frames, whatever the rounding) */
        s->age_ms = (uint16_t)(s->age_ms + ms);
        if (s->shape != SH4DER_SPARK_STREAK) continue;
        if ((s->flags & SH4DER_SPARK_GRAVITY) && gravity)
            for (k = 0; k < 3; k++) s->v[k] = sh4der_fma_(gravity[k], dt, s->v[k]);
        for (k = 0; k < 3; k++) {
            s->p[k] = sh4der_fma_(s->v[k], dt, s->p[k]);
            s->q[k] = sh4der_fma_(s->v[k], dt, s->q[k]);
        }
    }
}

/* colour x gain (0..256) a channel, alpha 0xFF */
static inline uint32_t sh4der_spark_scale_(uint32_t argb, uint32_t gain) {
    const uint32_t r = (((argb >> 16) & 0xFFu) * gain) >> 8, g = (((argb >> 8) & 0xFFu) * gain) >> 8, b = ((argb & 0xFFu) * gain) >> 8;

    return 0xFF000000u | (r << 16) | (g << 8) | b;
}

static inline void sh4der_spark_to_view_(const sh4der_spark_view_t *vw, const float w[3], float out[3]) {
    const float d[3] = { w[0] - vw->eye[0], w[1] - vw->eye[1], w[2] - vw->eye[2] };

    out[0] = sh4der_dot3_(d, vw->right); out[1] = sh4der_dot3_(d, vw->up); out[2] = sh4der_dot3_(d, vw->fwd);
}

/** \brief The four corners of one quad in view space, strip order, with its atlas UV and its colour now.
    Returns 1, or 0 when there is nothing to draw: a free slot, a billboard nearer than the near plane, a streak
    wholly nearer, a bad view. A streak that crosses the near plane is cut there, and no corner is ever nearer than the
    near plane (an end at it is widened in the screen plane). The two triangles are
    (0 1 2) and (1 2 3): test each with sh4der_tri_area_ok before sending (sh4der_spark_draw does). */
static inline int sh4der_spark_quad(const sh4der_spark_t *s, const sh4der_spark_view_t *vw, sh4der_spark_vtx_t out[4]) {
    float u0, v0, u1, v1, cw, ch;
    uint32_t col, gain;
    unsigned cell;
    int k;

    if (s->shape == SH4DER_SPARK_FREE || !vw->cols || !vw->rows || !(vw->near_z > 0.0f) || !(vw->focal_px > 0.0f) || !s->life_ms) return 0;
    cw = sh4der_div_(1.0f, (float)vw->cols); ch = sh4der_div_(1.0f, (float)vw->rows);
    cell = (unsigned)s->cell % ((unsigned)vw->cols * vw->rows);
    u0 = sh4der_fma_((float)(cell % vw->cols), cw, vw->inset); u1 = sh4der_fma_((float)(cell % vw->cols + 1u), cw, -vw->inset);
    v0 = sh4der_fma_((float)(cell / vw->cols), ch, vw->inset); v1 = sh4der_fma_((float)(cell / vw->cols + 1u), ch, -vw->inset);
    gain = 256u - ((uint32_t)s->age_ms * 256u) / s->life_ms;
    col = sh4der_spark_scale_(s->argb, gain);
    if (s->shape == SH4DER_SPARK_BILLBOARD) {
        float c[3], sn, cs, ax, ay;

        sh4der_spark_to_view_(vw, s->p, c);
        if (c[2] < vw->near_z) return 0;
        sh4der_sincos_(s->q[1], &sn, &cs);
        ax = cs * s->q[0]; ay = sn * s->q[0];                       /* the quad's x axis; its y axis is (-ay, ax) */
        out[0].x = c[0] - ax - ay; out[0].y = c[1] - ay + ax;
        out[1].x = c[0] - ax + ay; out[1].y = c[1] - ay - ax;
        out[2].x = c[0] + ax - ay; out[2].y = c[1] + ay + ax;
        out[3].x = c[0] + ax + ay; out[3].y = c[1] + ay - ax;
        for (k = 0; k < 4; k++) { out[k].z = c[2]; out[k].argb = col; out[k].u = k < 2 ? u0 : u1; out[k].v = (k & 1) ? v1 : v0; }
        return 1;
    } else {
        float a[3], b[3], d[3], m[3], side[3], flat[3], ha, hb, t;
        const float *sa, *sb;
        const float px = sh4der_div_(vw->min_px * 0.5f, vw->focal_px);   /* half the least width at depth 1 */
        uint32_t tail_col = (s->flags & SH4DER_SPARK_TAIL0) ? 0xFF000000u : col;

        sh4der_spark_to_view_(vw, s->q, a);
        sh4der_spark_to_view_(vw, s->p, b);
        if (s->flags & SH4DER_SPARK_RETREAT) {
            t = (float)s->age_ms * sh4der_div_(1.0f, (float)s->life_ms);
            for (k = 0; k < 3; k++) a[k] = sh4der_lerp_(a[k], b[k], t);
        }
        if (a[2] < vw->near_z && b[2] < vw->near_z) return 0;
        if (a[2] < vw->near_z) {                                   /* cut the tail at the near plane */
            t = sh4der_div_(vw->near_z - a[2], b[2] - a[2]);
            for (k = 0; k < 2; k++) a[k] = sh4der_lerp_(a[k], b[k], t);
            a[2] = vw->near_z;
        } else if (b[2] < vw->near_z) {
            t = sh4der_div_(vw->near_z - b[2], a[2] - b[2]);
            for (k = 0; k < 2; k++) b[k] = sh4der_lerp_(b[k], a[k], t);
            b[2] = vw->near_z;
        }
        for (k = 0; k < 3; k++) { d[k] = b[k] - a[k]; m[k] = (a[k] + b[k]) * 0.5f; }
        sh4der_cross_(d, m, side);                                 /* across the streak and across the line of sight */
        if (sh4der_len3_(side[0], side[1], side[2]) < 1.0e-12f) { side[0] = 1.0f; side[1] = 0.0f; side[2] = 0.0f; }   /* seen end on */
        sh4der_norm3_(side);
        ha = s->width * 0.5f; hb = ha;
        if (ha < px * a[2]) ha = px * a[2];
        if (hb < px * b[2]) hb = px * b[2];
        /* An end whose corner would fall nearer than the near plane (a cut end sits ON it) is widened in the screen plane
           instead: the same direction with no depth part, so no corner leaves the view volume and the quad is cut, not lost. */
        flat[0] = side[0]; flat[1] = side[1]; flat[2] = 0.0f;
        if (sh4der_len2_(flat[0], flat[1]) < 1.0e-12f) flat[0] = 1.0f;
        sh4der_norm3_(flat);
        t = side[2] < 0.0f ? -side[2] : side[2];
        sa = sh4der_fma_(-t, ha, a[2]) < vw->near_z ? flat : side;
        sb = sh4der_fma_(-t, hb, b[2]) < vw->near_z ? flat : side;
        out[0].x = sh4der_fma_(sa[0], ha, a[0]); out[0].y = sh4der_fma_(sa[1], ha, a[1]); out[0].z = sh4der_fma_(sa[2], ha, a[2]);
        out[1].x = sh4der_fma_(sa[0], -ha, a[0]); out[1].y = sh4der_fma_(sa[1], -ha, a[1]); out[1].z = sh4der_fma_(sa[2], -ha, a[2]);
        out[2].x = sh4der_fma_(sb[0], hb, b[0]); out[2].y = sh4der_fma_(sb[1], hb, b[1]); out[2].z = sh4der_fma_(sb[2], hb, b[2]);
        out[3].x = sh4der_fma_(sb[0], -hb, b[0]); out[3].y = sh4der_fma_(sb[1], -hb, b[1]); out[3].z = sh4der_fma_(sb[2], -hb, b[2]);
        for (k = 0; k < 4; k++) { out[k].argb = k < 2 ? tail_col : col; out[k].u = k < 2 ? u0 : u1; out[k].v = (k & 1) ? v1 : v0; }
        return 1;
    }
}

/* ---- the KallistiOS draw (src/sh4der_spark.c; nothing off the Dreamcast) ---- */

/** \brief The screen for the sprite draws (sh4der_spark_draw, sh4der_bolt_draw, sh4der_motes_draw_*): pixel x = cx + focal_px x / z,
    pixel y = cy - focal_y_px y / z, depth key = z_base + z_scale / z (larger is nearer, the KallistiOS convention).
    The depth pair lets a client put the sprites on the same depth scale as its own world geometry: a world that sends
    z_base + z_scale / depth (for instance z_scale 10) needs the same here, or the sprites sit far behind near geometry.
    z_base 0 and z_scale 1 (or 0, see below) give the plain 1 / z key. */
typedef struct sh4der_spark_screen {
    float cx, cy, focal_y_px;
    float z_base;                      /**< \brief Added to every depth key (0 = none). */
    float z_scale;                     /**< \brief Multiplies 1 / z (0 or a negative value = 1, so a record that was zero-filled or built before
                                           this field existed keeps the plain key). */
} sh4der_spark_screen_t;

/** \brief A screen record with the plain depth key 1 / z (z_base 0, z_scale 1). */
static inline sh4der_spark_screen_t sh4der_spark_screen_make(float cx, float cy, float focal_y_px) {
    sh4der_spark_screen_t s;

    s.cx = cx; s.cy = cy; s.focal_y_px = focal_y_px;
    s.z_base = 0.0f; s.z_scale = 1.0f;
    return s;
}

/** \brief The depth key of a vertex whose reciprocal depth is iz (= 1 / z): z_base + z_scale x iz, with z_scale 0 or less read as 1.
    Pure maths, host tested; the three Dreamcast draws send exactly this as the vertex z. */
static inline float sh4der_spark_depth(const sh4der_spark_screen_t *screen, float iz) {
    const float k = screen->z_scale > 0.0f ? screen->z_scale : 1.0f;

    return sh4der_fma_(k, iz, screen->z_base);
}

/** \brief Compile the polygon header for an atlas the caller loaded: texture pointer, its width and height
    (powers of two), its KallistiOS texture format word (PVR_TXRFMT_RGB565 | PVR_TXRFMT_VQ_ENABLE ...).
    Translucent list, ONE / ONE, depth test on, depth write off, no culling, bilinear. Call again after the
    texture moved. Returns 1, or 0 for a NULL texture (the draw then draws nothing). */
int sh4der_spark_init(void *texture, int width, int height, int txr_format);

/** \brief Inside the caller's translucent list: one header, then every live quad with screen area.
    Returns the quads sent. */
unsigned sh4der_spark_draw(const sh4der_spark_pool_t *pool, const sh4der_spark_view_t *view, const sh4der_spark_screen_t *screen);

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_SPARK_H */
