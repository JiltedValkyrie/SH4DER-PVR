/* SH4DER-PVR: back-lit transmission (a sub-surface scattering stand-in)

   sh4der_sss.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_sss.h
    \brief   Light that comes through a thin part of a surface toward the eye: a wave crest
             against a low sun, a backlit ear or leaf, wax, ice, a curtain.

    What it is: an ARTISTIC proxy, per vertex. A colour (tint) loses each channel by
    exp(-absorption x path) over a path-length the caller supplies (its own thickness proxy),
    then is weighted by how directly the light sits behind the surface as seen from the eye
    (the back-light term), how much of the surface is thin enough to pass light (coverage),
    the light that enters at all (entry, a Fresnel complement say) and whether the light
    reaches the surface (visibility, a shadow term). It is NOT a multiple-scattering solve: no
    diffusion profile, no blur across the surface, no energy conservation beyond the caller's
    weights.

    out = tint x exp(-absorption x path) x (entry x coverage x back x visibility x gain)

    back = saturate((wrap - (view . light + distortion x view . normal)) / (1 + wrap))

    view: unit, from the surface to the eye. light: unit, from the surface to the light.
    normal: unit (read only when distortion is not 0). With wrap 0 and distortion 0, back is
    saturate(-view . light): full when the light is straight behind the surface, 0 from
    side-on to front-lit. wrap widens the lit side (0.3 to 1 for skin, leaves); distortion
    bends the through direction along the normal (0.1 to 0.5 for curved thin shapes).

    Drawn: nothing by itself. Put the RGB in the PVR offset colour (the specular add of a
    textured header, pvr_poly_cxt_t gen.specular = 1) or add it to the vertex colour.
    Cost: VRAM 0, AICA 0, RAM: the 40-byte parameter block. CPU per vertex: two 3-term dots
    (one with distortion 0), three exp (SH4ZAM shz_expf), about 12 FMAC / multiplies.

    No allocation, no globals. SH4ZAM for all maths (sh4der_core helpers); define
    SH4DER_CORE_NO_SH4ZAM for the plain C path.

    Version: v0.50
    The effect: thin edges glow in the light's colour, deeper red-shifted (or the caller's
      absorption) as the path grows.
    Developed for: the sunset ocean's crest transmission.
    Use it for: water crests, backlit skin and ears, leaves, wax, ice, cloth.

    \author Jilted Valkyrie
*/

#ifndef SH4DER_SSS_H
#define SH4DER_SSS_H

#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \brief Material and light tuning. Fill with sh4der_sss_init (it precomputes the wrap terms). */
typedef struct {
    float tint[3];        /**< \brief Colour of the light entering, RGB 0..1 (the light's colour, say). */
    float absorption[3];  /**< \brief Loss per unit path, per channel (>= 0). Larger red = deeper goes blue-green. */
    float gain;           /**< \brief Overall strength. */
    float distortion;     /**< \brief Normal share in the through direction; 0 = straight through. */
    float wrap_scale;     /**< \brief 1 / (1 + wrap), set by sh4der_sss_init. */
    float wrap_offset;    /**< \brief wrap / (1 + wrap), set by sh4der_sss_init. */
} sh4der_sss_t;

static inline float sh4der_sss_exp_(float x) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_expf(x);
#else
    return expf(x);
#endif
}

/** \brief Fill p. wrap >= 0 (0: only light straight behind counts); distortion any (0: none). */
static inline void sh4der_sss_init(sh4der_sss_t *p, const float tint[3], const float absorption[3],
    float gain, float wrap, float distortion) {
    unsigned i;
    for (i = 0; i < 3; ++i) { p->tint[i] = tint[i]; p->absorption[i] = absorption[i]; }
    p->gain = gain;
    p->distortion = distortion;
    /* wrap 0 keeps scale exactly 1, offset exactly 0: back = saturate(-d) bit for bit */
    p->wrap_scale = wrap > 0 ? sh4der_recip_pos_(1 + wrap) : 1.0f;
    p->wrap_offset = wrap > 0 ? wrap * p->wrap_scale : 0.0f; /* hand: one product, once at init */
}

/** \brief Back-light term 0..1: how directly the light sits behind the surface, seen from the eye. */
static inline float sh4der_sss_back(const sh4der_sss_t *p, const float view[3], const float light[3], const float normal[3]) {
    float d = sh4der_dot3_(view, light);
    if (p->distortion != 0) d = sh4der_fma_(p->distortion, sh4der_dot3_(view, normal), d);
    return sh4der_sat_(sh4der_fma_(-d, p->wrap_scale, p->wrap_offset));
}

/** \brief tint x exp(-absorption x path) per channel; path < 0 counts as 0. */
static inline void sh4der_sss_absorb(const float tint[3], const float absorption[3], float path, float out[3]) {
    unsigned i;
    if (path < 0) path = 0;
    for (i = 0; i < 3; ++i) out[i] = tint[i] * sh4der_sss_exp_(-absorption[i] * path); /* hand: scalar products around SH4ZAM exp */
}

/** \brief Transmitted RGB for one vertex (see the file comment). path: the caller's thickness
    proxy (same unit as 1 / absorption); coverage, entry, visibility: 0..1. */
static inline void sh4der_sss_rgb(const sh4der_sss_t *p, const float view[3], const float light[3], const float normal[3],
    float path, float coverage, float entry, float visibility, float out[3]) {
    const float weight = entry * coverage * sh4der_sss_back(p, view, light, normal) * visibility * p->gain; /* hand: weight product, no SH4ZAM op */
    unsigned i;
    sh4der_sss_absorb(p->tint, p->absorption, path, out);
    for (i = 0; i < 3; ++i) out[i] *= weight; /* hand: scale, no fused form */
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_SSS_H */
