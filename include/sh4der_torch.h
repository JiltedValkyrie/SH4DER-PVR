/* SH4DER-PVR: the flashlight hotspot by carving

   sh4der_torch.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_torch.h
    \brief   A flashlight's hotspot on the PVR, with no shader: light near, carve the dark, boost the core, fade the reach.

    The frame is the product of four factors, each drawn by the client:

      out = base x (1 + near(vertex)) x carve(r) x (1 + boost(r)) x (1 - reach(depth))

    - near: a per-vertex add, by distance only (no cone maths): the walls within
      SH4DER_TORCH_NEAR_WU of the eye are lit. Done where the client lights vertices.
    - carve: a screen-space MULTIPLY layer, 1 inside the edge circle, MUL_OUT
      outside it, feathered over EDGE_IN .. EDGE_OUT. It darkens what `near`
      lit outside the spot. Scaled by the darkness gate (k = gate x power).
    - boost: a screen-space BOOST layer (x (1 + b), up to 2x) on the frame's own
      texels: a hot core and a soft corona. Brick stays brick. Runs in daylight
      too, mild (k = power).
    - reach: the fog table bent toward black with depth (sh4der_torch_fog_bend),
      so the lit near world fades out per pixel.

    Why a screen-space circle is exact: the beam starts at the eye, so its cone is
    a cone about the view axis. A cone about the optical axis projects to a circle
    about the principal point, of radius focal x tan(half angle), at any distance.
    sh4der_torch_radii gives the two radii (x and y differ only by the squeeze).

    Darkness gate: sh4der_torch_gate_step with the baked luminance (the
    flashlight's own low-passed sample), not the frame read: the carve darkens the
    frame, so a gate read from the frame would feed back on itself.

    Mesh: sh4der_torch_boost_mesh and sh4der_torch_carve_mesh write strips (one
    band per strip, EOL on each band's last vertex); the KOS draw code is in
    src/sh4der_torch.c. The client draws both layers under its own look
    (z below the look's layers, see the plan).

    \author Jilted Valkyrie
*/

#ifndef SH4DER_TORCH_H
#define SH4DER_TORCH_H

#include <stdint.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Shipped tuning
    @{ */
#define SH4DER_TORCH_BOOST       0.6f    /**< \brief Core boost at the centre (x (1 + this) at full power, plus HOT). */
#define SH4DER_TORCH_HOT         0.4f    /**< \brief Extra hot centre, linear to 0 at HOT_U. */
#define SH4DER_TORCH_HOT_U       0.2f    /**< \brief Radius (share of the edge) the hot centre reaches. */
#define SH4DER_TORCH_TINT_G      0.96f   /**< \brief Green share of the boost (FLASHLIGHT_TINT_G). */
#define SH4DER_TORCH_TINT_B      0.88f   /**< \brief Blue share of the boost (FLASHLIGHT_TINT_B). */
#define SH4DER_TORCH_MUL_OUT     0.35f   /**< \brief Carve level outside the edge at full darkness. */
#define SH4DER_TORCH_EDGE_IN     0.9f    /**< \brief Carve starts (share of the edge radius). */
#define SH4DER_TORCH_EDGE_OUT    1.0f    /**< \brief Carve full (the edge). */
#define SH4DER_TORCH_UMAX        1.6f    /**< \brief Carve mesh reaches this share (screen corners at 1.25). */
#define SH4DER_TORCH_NEAR_K      0.425f  /**< \brief Near add at the eye, x gate x power (FLASHLIGHT_STRENGTH). */
#define SH4DER_TORCH_NEAR_WU     7.0f    /**< \brief Near add reach, world units. */
#define SH4DER_TORCH_FOG_NEAR    2.0f    /**< \brief Reach fade starts at this depth, world units. */
#define SH4DER_TORCH_FOG_FAR     6.0f    /**< \brief Reach fully black at this depth (x gate x power). */
#define SH4DER_TORCH_ON_LO       0.12f   /**< \brief Gate on below this baked luminance. */
#define SH4DER_TORCH_OFF_HI      0.22f   /**< \brief Gate off above this (hysteresis: the band between holds). */
#define SH4DER_TORCH_GATE_UP_S   0.4f    /**< \brief Gate time constant toward on. */
#define SH4DER_TORCH_GATE_DOWN_S 1.0f    /**< \brief Gate time constant toward off. */
#define SH4DER_TORCH_DARK_LO     0.30f   /**< \brief Baked luminance up to which the carve is full (sh4der_torch_dark). */
#define SH4DER_TORCH_DARK_HI     0.80f   /**< \brief Baked luminance from which there is no carve (daylight). */
#define SH4DER_TORCH_RISE_S      0.2f    /**< \brief Power: seconds to full (flashlight: 6 ticks at 30 Hz). */
#define SH4DER_TORCH_FALL_S      0.8f    /**< \brief Power: seconds to none (flashlight: 24 ticks at 30 Hz). */
#define SH4DER_TORCH_SPOKES      16      /**< \brief Spokes of both meshes (the unit table in sh4der_torch.c is 16). */
#define SH4DER_TORCH_RINGS       5       /**< \brief Boost bands, centre to edge. */
#define SH4DER_TORCH_BANDS       4       /**< \brief Carve bands, edge feather to UMAX. */
#define SH4DER_TORCH_VERTS       ((SH4DER_TORCH_RINGS + SH4DER_TORCH_BANDS) * 2 * (SH4DER_TORCH_SPOKES + 1))   /**< \brief Vertices both meshes hold. */
/** @} */

/** \brief A mesh vertex: screen x, y (client units), ARGB8888, eol on each strip's last vertex. */
typedef struct sh4der_torch_vert {
    float x, y;
    uint32_t argb;
    uint8_t eol;
} sh4der_torch_vert_t;

/** \brief The darkness gate: the share it is on (0..1) and the switch with hysteresis. Zero it (or sh4der_torch_gate_reset) per load. */
typedef struct sh4der_torch_gate {
    float d;
    int on;
} sh4der_torch_gate_t;

/** \brief sqrt for the one-off setup (SH4ZAM's root on the SH4ZAM path, sqrtf on the plain path). */
static inline float sh4der_torch_sqrt_(float x) {
#ifdef SH4DER_CORE_NO_SH4ZAM
    return sqrtf(x);
#else
    return shz_sqrtf(x);
#endif
}

/* The larger of two: SH4ZAM's on the SH4ZAM path. */
static inline float sh4der_torch_max_(float a, float b) {
#ifdef SH4DER_CORE_NO_SH4ZAM
    return a > b ? a : b;
#else
    return shz_fmaxf(a, b);
#endif
}

/* Smoothstep: SH4ZAM's on the SH4ZAM path, written out on the plain one. */
static inline float sh4der_torch_smooth_(float x, float e0, float e1) {
#ifdef SH4DER_CORE_NO_SH4ZAM
    const float t = sh4der_sat_(sh4der_div_(x - e0, e1 - e0));
    return t * t * (3.0f - 2.0f * t);
#else
    return shz_smoothstepf(x, e0, e1);
#endif
}

/** \brief tan of a half angle from its cos^2 (the flashlight's cos2 constants): sqrt((1 - c2) / c2). c2 in (0, 1]. */
static inline float sh4der_torch_tan_of_cos2(float c2) {
    if (!(c2 < 1.0f))
        return 0.0f;   /* 0 degrees (and the root of 0 is not taken: FSRRA of 0 is not a number) */
    return sh4der_torch_sqrt_(sh4der_div_(1.0f - c2, c2));
}

/** \brief The screen radii of a cone's circle: sx, sy are the projection's pixel scales (focal x xsq, focal, per axis); tan_r the tangent. */
static inline void sh4der_torch_radii(float sx, float sy, float tan_r, float *rx, float *ry) {
    *rx = sx * tan_r;
    *ry = sy * tan_r;
}

/** \brief The core boost at radius u (share of the edge radius), power k (0..1). Bright core, soft corona, 0 from the edge.
    Returns 0..1, the layer's grey (BOOST layer: frame x (1 + value)). */
static inline float sh4der_torch_boost_at_u(float u, float k, float hot_u) {
    float v = 0.0f, h;
    if (u < 1.0f) {
        const float q = 1.0f - u * u;
        v = SH4DER_TORCH_BOOST * q * q;
        h = hot_u > 0.0f ? 1.0f - sh4der_div_(u, hot_u) : 0.0f;
        if (h > 0.0f)
            v += SH4DER_TORCH_HOT * h;
    }
    return sh4der_sat_(v * k);
}

/** \brief sh4der_torch_boost_at_u with the shipped hot centre (SH4DER_TORCH_HOT_U). The _u form takes a client's hot centre radius (share of the edge). */
static inline float sh4der_torch_boost_at(float u, float k) { return sh4der_torch_boost_at_u(u, k, SH4DER_TORCH_HOT_U); }

/** \brief The carve at radius u, k = gate x power (0..1). 1 inside the feather, MUL_OUT at the edge and out, at k = 1. MULTIPLY layer. */
static inline float sh4der_torch_carve_at(float u, float k) {
    return 1.0f - k * (1.0f - SH4DER_TORCH_MUL_OUT) * sh4der_torch_smooth_(u, SH4DER_TORCH_EDGE_IN, SH4DER_TORCH_EDGE_OUT);
}

/** \brief The near add with its own strength and reach: near_k x k x (1 - dist2 x inv_r2), 0 past the reach or k <= 0. inv_r2 = 1 / reach^2. */
static inline float sh4der_torch_near_at(float dist2, float k, float near_k, float inv_r2) {
    const float t = dist2 * inv_r2;
    if (!(t < 1.0f) || k <= 0.0f)
        return 0.0f;
    return near_k * k * (1.0f - t);
}

/** \brief The near add of a vertex at squared distance dist2 from the eye, k = gate x power. Distance only; 0 past NEAR_WU. */
static inline float sh4der_torch_near(float dist2, float k) {
    return sh4der_torch_near_at(dist2, k, SH4DER_TORCH_NEAR_K, 1.0f / (SH4DER_TORCH_NEAR_WU * SH4DER_TORCH_NEAR_WU));
}

/** \brief The view depth fog table entry i stands for, for a far depth (the atmofog layout, atmofog_entry_depth). */
static inline float sh4der_torch_entry_depth(unsigned i, float far_depth) {
    const float s = (float)(1u << (i >> 4)) * (float)((i & 15u) + 16u) * (1.0f / 16.0f);
    return sh4der_div_(far_depth, s);
}

/** \brief One fog table entry bent toward black: table T (the fog amount at its depth), reach k = gate x power.
    Result 1 - (1 - T)(1 - R), R = k x smoothstep(FOG_NEAR, FOG_FAR, depth). Exact for black fog; see the plan for a tinted one. */
static inline float sh4der_torch_fog_bend(float table, float depth, float k) {
    const float r = k * sh4der_torch_smooth_(depth, SH4DER_TORCH_FOG_NEAR, SH4DER_TORCH_FOG_FAR);
    return 1.0f - (1.0f - table) * (1.0f - r);
}

/** \brief How much of the carve a place takes, from the baked luminance round the eye: 1 up to DARK_LO, smoothly to 0 at DARK_HI. No switch and no
    memory: a half-lit room gets a part of the carve at once (the gate below starts off and holds between its two thresholds, so such a room never
    got one). */
static inline float sh4der_torch_dark_range(float lum, float lo, float hi) { return 1.0f - sh4der_torch_smooth_(lum, lo, hi); }

/** \brief sh4der_torch_dark_range with the shipped ends (DARK_LO, DARK_HI). The _range form takes a client's own (hi above lo). */
static inline float sh4der_torch_dark(float lum) { return sh4der_torch_dark_range(lum, SH4DER_TORCH_DARK_LO, SH4DER_TORCH_DARK_HI); }

/** \brief The gate's start: off, d 0. */
static inline void sh4der_torch_gate_reset(sh4der_torch_gate_t *g) {
    g->d = 0.0f;
    g->on = 0;
}

/** \brief One frame: lum is the baked luminance of the near vertices (flashlight lum_avg, torch-invariant), dt seconds.
    Switches on below ON_LO, off above OFF_HI (between them it holds), eases d toward the switch. Returns d (0..1). */
static inline float sh4der_torch_gate_step_scaled(sh4der_torch_gate_t *g, float lum, float dt, float scale) {
    float tau, target;
    if (g->on) {
        if (lum > SH4DER_TORCH_OFF_HI * scale)
            g->on = 0;
    } else if (lum < SH4DER_TORCH_ON_LO * scale) {
        g->on = 1;
    }
    target = g->on ? 1.0f : 0.0f;
    tau = g->on ? SH4DER_TORCH_GATE_UP_S : SH4DER_TORCH_GATE_DOWN_S;
    if (dt > 0.0f)
        g->d += (target - g->d) * sh4der_div_(dt, tau + dt);
    return g->d;
}

/** \brief sh4der_torch_gate_step_scaled with both thresholds as shipped (scale 1). The scaled form is for a client whose own switch moves with a
    sensitivity: scale the gate by the same number so the carve comes on with the light. */
static inline float sh4der_torch_gate_step(sh4der_torch_gate_t *g, float lum, float dt) { return sh4der_torch_gate_step_scaled(g, lum, dt, 1.0f); }

/** \brief The power ramp: p (0..1) toward on or off, linear in dt (the flashlight's per-tick ramp at 30 Hz). */
static inline float sh4der_torch_power_step(float p, int on, float dt) {
    if (dt <= 0.0f)
        return p;
    p += on ? dt * (1.0f / SH4DER_TORCH_RISE_S) : -dt * (1.0f / SH4DER_TORCH_FALL_S);
    return sh4der_sat_(p);
}

/** \brief The boost mesh: SH4DER_TORCH_RINGS strips (one per band, centre to edge) about (cx, cy), radii rx, ry, power k. Returns the vertex count.
    `out` holds SH4DER_TORCH_VERTS. */
unsigned sh4der_torch_boost_mesh(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float power);

/** \brief sh4der_torch_boost_mesh in a client's tint (green and blue shares of the boost; the plain call uses SH4DER_TORCH_TINT_G / _B). */
unsigned sh4der_torch_boost_mesh_tint(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float power, float tint_g, float tint_b);

/** \brief sh4der_torch_boost_mesh_tint with a client's hot centre radius too (share of the edge; the others use SH4DER_TORCH_HOT_U). */
unsigned sh4der_torch_boost_mesh_core(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float power, float tint_g, float tint_b, float hot_u);

/** \brief The carve mesh: SH4DER_TORCH_BANDS strips from the feather's start out to UMAX (a hole inside: no fill there). k = gate x power.
    Returns the vertex count; `out` holds SH4DER_TORCH_VERTS. */
unsigned sh4der_torch_carve_mesh(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float k);

/** \brief sh4der_torch_carve_mesh out to `umax` (share of the edge radius) instead of SH4DER_TORCH_UMAX: a narrow beam or a wide screen puts the corners
    past 1.6. sh4der_torch_umax gives the share that covers a w x h screen. */
unsigned sh4der_torch_carve_mesh_to(sh4der_torch_vert_t out[SH4DER_TORCH_VERTS], float cx, float cy, float rx, float ry, float k, float umax);

/** \brief The carve's outer share that covers the screen (0, 0) .. (w, h) from the centre (cx, cy): the far corner over the radii, + 5 %; never under SH4DER_TORCH_UMAX. */
static inline float sh4der_torch_umax(float cx, float cy, float rx, float ry, float w, float h) {
    const float dx = sh4der_div_(sh4der_torch_max_(cx, w - cx), rx), dy = sh4der_div_(sh4der_torch_max_(cy, h - cy), ry);
    return sh4der_torch_max_(1.05f * sh4der_torch_sqrt_(dx * dx + dy * dy), SH4DER_TORCH_UMAX);
}

#ifdef _arch_dreamcast
#include <dc/pvr.h>

/** \brief The two headers: hdr[0] the carve (MULTIPLY), hdr[1] the boost (BOOST). Translucent list, fog off, no depth write. */
void sh4der_torch_headers(pvr_poly_hdr_t hdr[2]);

/** \brief Draw one mesh's strips at depth z of the translucent sort. */
void sh4der_torch_draw(const pvr_poly_hdr_t *hdr, const sh4der_torch_vert_t *v, unsigned n, float z);
#endif

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_TORCH_H */
