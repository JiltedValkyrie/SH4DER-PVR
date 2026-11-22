/* SH4DER-PVR: reusable directional-wave water and Fresnel; crest transmission via sh4der_sss.h.
 * Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.
 * SH4ZAM for all math on Dreamcast; core's plain-C mirror for host checks.
 *
 * What it is: the maths of an ocean or pond surface, header only, no allocation, no globals, no PVR calls. A surface is
 * a sum of directional sine waves. For any point (x, z) at a time you get the surface height and its slope, from which
 * you build a normal for lighting (sh4der_water_normal), a mirror direction for the sky reflection
 * (sh4der_water_reflect), how much of the light reflects rather than enters (sh4der_water_fresnel), and how the light
 * that enters is tinted by the water (sh4der_water_transmit, sh4der_water_crest_sss).
 * Use it for: oceans, lakes, puddles, any rippling surface lit per vertex or per texel on the CPU.
 * Cost: RAM, the caller's wave table (7 values, 28 B, per wave); VRAM 0; AICA 0. CPU per wave per point: one sincos, a few
 * multiply-adds.
 * Axes: the surface lies in the x-z plane and y is up (the normal of a flat sea is (0, 1, 0)). Units: world units for
 * lengths, seconds for time, radians for phase.
 * "hand:" in a comment marks a line written as plain C because SH4ZAM has no equivalent instruction for it.
 * PVR terms (TA, TR/OP list, EOL, twiddled, direct render): see the PVR glossary in README.md.
 */
#ifndef SH4DER_WATER_H
#define SH4DER_WATER_H
#include "sh4der_core.h"
#include "sh4der_sss.h"
#define SH4DER_WATER_TAU 6.28318530718f   /* 2 pi: radians in one full wave cycle */

/* One directional wave: height = amplitude x sin(k x (dx x x + dz x z - speed x time) + phase), with k = 2 pi / wavelength. */
typedef struct {
    float dx, dz;       /* direction the wave travels along the surface; (dx, dz) must be a unit vector */
    float amplitude;    /* crest height above the mean level, world units (0 switches the wave off) */
    float wavelength;   /* distance between two crests, world units, must be > 0 (a wave with 0, negative or non-finite is skipped) */
    float speed;        /* how fast the pattern travels along (dx, dz), world units per second; negative runs it backwards */
    float phase;        /* start offset of the cycle, radians (0 .. 2 pi covers every case) */
    int ripple;         /* 0: a long swell wave that moves the geometry and the normal; nonzero: a small ripple that only bends the normal */
} sh4der_water_wave_t;

/* The surface at one point, the result of summing the waves. */
typedef struct {
    float height;       /* surface height above the mean level, world units (swell waves only; ripples add none) */
    float dx, dz;       /* slopes: height change per world unit along x and z (dimensionless, 0 = flat), the input of sh4der_water_normal */
} sh4der_water_surface_t;

/* exp(x): SH4ZAM's expf on the Dreamcast, the C library's on the host. */
static inline float sh4der_water_exp(float x) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_expf(x);
#else
    return expf(x);
#endif
}
/* x to the fifth power (the Schlick Fresnel exponent): SH4ZAM's integer power on the Dreamcast, powf on the host. */
static inline float sh4der_water_pow5(float x) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_powif(x, 5);
#else
    return powf(x, 5);
#endif
}
/* Sums count waves at the point (x, z) at time `time` (seconds) and returns the height and slopes there.
 * waves: the wave table; count: its length (0 gives a flat surface). x, z: world position in the surface plane.
 * footprint: the world-space distance between neighbouring samples (the vertex spacing or the texel size); a wave cannot
 *   be resolved when its wavelength is short against that, so it fades smoothly from full strength at 6 footprints to
 *   nothing at 2 footprints, which avoids shimmer.
 *   0 or negative turns the fade off (every wave counts fully).
 * swell, ripple: gains (0 .. 1, 1 = as authored) for the two kinds of wave: ripple == 0 waves are scaled by swell and
 *   ripple != 0 waves by ripple. A gain of 0 skips those waves.
 * Ripple waves only change the slopes; swell waves change the height as well as the slopes.
 * Wave directions must be unit and wavelength > 0 (others are skipped). No NULL check: waves must be valid for count entries.
 */
static inline sh4der_water_surface_t sh4der_water_sample(const sh4der_water_wave_t *waves,
    unsigned count, float x, float z, float time, float footprint, float swell, float ripple) {
    sh4der_water_surface_t out = {0, 0, 0};
    const float inv_footprint = footprint > 0 ? sh4der_recip_pos_(footprint) : 0;
    const float at[3] = {x, z, time};
    unsigned i;
    for (i = 0; i < count; ++i) {
        const sh4der_water_wave_t *w = &waves[i];
        const float dir[3] = {w->dx, w->dz, -w->speed};
        float a, s, c, k, ak, filter = 1;
        if (!(w->wavelength > 0) || !sh4der_finite_(w->wavelength)) continue;
        k = SH4DER_WATER_TAU * sh4der_recip_pos_(w->wavelength); /* hand: constant x reciprocal, no fused op */
        if (footprint > 0) filter = sh4dow_smooth01(sh4der_fma_(w->wavelength*0.25f, inv_footprint, -0.5f)); /* hand: constant scale feeds FMAC */
        a = w->amplitude * (w->ripple ? ripple : swell) * filter; /* hand: scalar product, no SH4ZAM op */
        if (a == 0) continue;
        /* phase = k (d . (x, z, time)) + phase0: one dot, one FMAC */
        sh4der_sincos_(sh4der_fma_(k, sh4der_dot3_(dir, at), w->phase), &s, &c);
        if (!w->ripple) out.height = sh4der_fma_(a, s, out.height);
        ak = a * k; /* hand: scalar product, no SH4ZAM op */
        out.dx = sh4der_fma_(ak*w->dx, c, out.dx);
        out.dz = sh4der_fma_(ak*w->dz, c, out.dz);
    }
    return out;
}
/* Fresnel reflectance: the share of light (0.0204 .. 1) that reflects off the water instead of entering it. ndotv is
 * the cosine between the surface normal and the direction to the eye, clamped to 0..1: 1 looking straight down gives
 * about 2 %, 0 at a grazing angle gives 1 (a mirror). */
static inline float sh4der_water_fresnel(float ndotv) {
    /* Air/water F0 about 0.0204. Schlick approximation. */
    return sh4der_fma_(0.9796f, sh4der_water_pow5(1-sh4der_sat_(ndotv)), 0.0204f);
}
/* Turns a slope pair into a unit surface normal n (y up): (-dx, 1, -dz) normalised. s is a sh4der_water_sample result. */
static inline void sh4der_water_normal(sh4der_water_surface_t s, float n[3]) {
    n[0] = -s.dx; n[1] = 1; n[2] = -s.dz; sh4der_norm3_(n);
}
/* The direction in which the eye sees the reflection: r (unit) is the view ray mirrored about the surface normal, the
 * direction to look up in the sky. n: unit normal; view: unit, pointing from the surface to the eye. */
static inline void sh4der_water_reflect(const float n[3], const float view[3], float r[3]) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    const shz_vec3_t o = shz_vec3_reflect(shz_vec3_init(-view[0],-view[1],-view[2]), shz_vec3_init(n[0],n[1],n[2]));
    r[0] = o.x; r[1] = o.y; r[2] = o.z;
#else
    const float d = n[0]*view[0]+n[1]*view[1]+n[2]*view[2];
    unsigned i; for (i=0; i<3; ++i) r[i] = 2*d*n[i]-view[i];
#endif
}
/* Colours the light that enters the water: incident (RGB 0..1) loses each channel by exp(-absorption x path), so a larger
 * absorption (per world unit) or a longer path (world units) leaves less of that channel. No scattering, no back-light
 * term: a thin wrapper over sh4der_sss_absorb (kept for callers). Writes the result to out. */
static inline void sh4der_water_transmit(const float incident[3], const float absorption[3], float path, float out[3]) {
    sh4der_sss_absorb(incident, absorption, path, out);
}
/* Crest transmission: the water's thickness proxy on top of sh4der_sss_rgb.
 * crest 0..1 (thin wave top = 1) is the coverage; the path runs from thick_path
 * (crest 0) to thin_path (crest 1). entry: light into the water (1 - Fresnel).
 * An artistic proxy, NOT a multiple-scattering solve (see sh4der_sss.h). */
static inline void sh4der_water_crest_sss(const sh4der_sss_t *p, const float view[3], const float light[3], const float normal[3],
    float crest, float thin_path, float thick_path, float entry, float visibility, float out[3]) {
    sh4der_sss_rgb(p, view, light, normal, sh4der_lerp_(thick_path, thin_path, crest), crest, entry, visibility, out);
}
#endif
