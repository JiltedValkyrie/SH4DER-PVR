/* SH4DER-PVR: reusable directional-wave water and Fresnel; crest transmission via sh4der_sss.h.
 * Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.
 * SH4ZAM for all math on Dreamcast; core's plain-C mirror for host checks.
 */
#ifndef SH4DER_WATER_H
#define SH4DER_WATER_H
#include "sh4der_core.h"
#include "sh4der_sss.h"
#define SH4DER_WATER_TAU 6.28318530718f

typedef struct { float dx, dz, amplitude, wavelength, speed, phase; int ripple; } sh4der_water_wave_t;
typedef struct { float height, dx, dz; } sh4der_water_surface_t;

static inline float sh4der_water_exp(float x) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_expf(x);
#else
    return expf(x);
#endif
}
static inline float sh4der_water_pow5(float x) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    return shz_powif(x, 5);
#else
    return powf(x, 5);
#endif
}
/* footprint: world-space sampling interval. Smoothly suppress unresolved waves.
 * Ripple waves perturb normals only; long waves also displace geometry.
 * Wave directions must be unit, wavelength>0. Time is seconds.
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
static inline float sh4der_water_fresnel(float ndotv) {
    /* Air/water F0 about 0.0204. Schlick approximation. */
    return sh4der_fma_(0.9796f, sh4der_water_pow5(1-sh4der_sat_(ndotv)), 0.0204f);
}
static inline void sh4der_water_normal(sh4der_water_surface_t s, float n[3]) {
    n[0] = -s.dx; n[1] = 1; n[2] = -s.dz; sh4der_norm3_(n);
}
/* Unit normal/view, view points from surface to eye. Reflected sky direction. */
static inline void sh4der_water_reflect(const float n[3], const float view[3], float r[3]) {
#ifndef SH4DER_CORE_NO_SH4ZAM
    const shz_vec3_t o = shz_vec3_reflect(shz_vec3_init(-view[0],-view[1],-view[2]), shz_vec3_init(n[0],n[1],n[2]));
    r[0] = o.x; r[1] = o.y; r[2] = o.z;
#else
    const float d = n[0]*view[0]+n[1]*view[1]+n[2]*view[2];
    unsigned i; for (i=0; i<3; ++i) r[i] = 2*d*n[i]-view[i];
#endif
}
/* Absorption only: thin wrapper over sh4der_sss_absorb (kept for callers). */
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
