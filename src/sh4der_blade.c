/* SH4DER-PVR: the blade effect

   sh4der_blade.c
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE.
*/

#include "sh4der_blade.h"

#define TWO_PI 6.28318530718f

static int soft_edge(float inner, float outer) {
    return sh4der_finite_(inner) && sh4der_finite_(outer) && inner >= 0.0f && outer > inner;
}

void sh4der_blade_params_default(sh4der_blade_params_t *p) {
    sh4der_tube_t *a = &p->tube[0], *b = &p->tube[1];
    sh4der_blocker_t *k = &p->blocker[0];

    p->period = SH4DER_BLADE_PERIOD;
    p->pulses = SH4DER_BLADE_PULSES;
    p->tube_count = 2;
    p->blocker_count = 1;
    p->axis[0] = 0.0f;
    p->axis[1] = SH4DER_BLADE_AXIS_Z;
    p->glow_base = SH4DER_BLADE_GLOW_BASE;
    p->glow_amp = SH4DER_BLADE_GLOW_AMP;
    p->floor = SH4DER_BLADE_FLOOR;
    p->fade_x0 = SH4DER_BLADE_FADE_X0;
    p->fade_x1 = SH4DER_BLADE_FADE_X1;
    p->light_swing = SH4DER_BLADE_LIGHT_SWING;
    p->ripple_freq = 0.0f;
    p->ripple_depth = 0.0f;   /* no ripples: the shipped sword */
    a->rho = SH4DER_BLADE_A_RHO;
    a->inner = SH4DER_BLADE_A_INNER;
    a->outer = SH4DER_BLADE_A_OUTER;
    a->weight = 1.0f;
    a->phase = 0.0f;
    a->turns = 1;
    b->rho = SH4DER_BLADE_B_RHO;
    b->inner = SH4DER_BLADE_B_INNER;
    b->outer = SH4DER_BLADE_B_OUTER;
    b->weight = SH4DER_BLADE_B_WEIGHT;
    b->phase = SH4DER_BLADE_B_PHASE;
    b->turns = -2;
    k->x0 = SH4DER_BLADE_K_X0;
    k->x1 = SH4DER_BLADE_K_X1;
    k->rho = SH4DER_BLADE_K_RHO;
    k->inner = SH4DER_BLADE_K_INNER;
    k->outer = SH4DER_BLADE_K_OUTER;
    k->depth = SH4DER_BLADE_K_DEPTH;
    k->phase = 0.0f;
    k->passes = SH4DER_BLADE_K_PASSES;
    k->turns = SH4DER_BLADE_K_PASSES;
    p->blocker[1] = *k;
}

void sh4der_blade_params_white(sh4der_blade_params_t *p) {
    sh4der_blade_params_default(p);
    p->glow_base = SH4DER_BLADE_WHITE_GLOW_BASE;
    p->glow_amp = SH4DER_BLADE_WHITE_GLOW_AMP;
    p->floor = SH4DER_BLADE_WHITE_FLOOR;
    p->blocker[0].depth = p->blocker[1].depth = SH4DER_BLADE_WHITE_K_DEPTH;
}

int sh4der_blade_params_valid(const sh4der_blade_params_t *p) {
    int i;

    if (!p || p->period == 0 || p->tube_count > SH4DER_TUBE_MAX || p->blocker_count > SH4DER_BLOCKER_MAX)
        return 0;
    if (!sh4der_finite_(p->axis[0]) || !sh4der_finite_(p->axis[1]) || !sh4der_unit_(p->glow_base) || !sh4der_unit_(p->glow_amp) ||
        !sh4der_unit_(p->floor) || !sh4der_unit_(p->light_swing) || !sh4der_unit_(p->ripple_depth) ||
        !(sh4der_finite_(p->ripple_freq) && p->ripple_freq >= 0.0f))
        return 0;
    if (!sh4der_finite_(p->fade_x0) || !sh4der_finite_(p->fade_x1) || !(p->fade_x1 > p->fade_x0))
        return 0;
    for (i = 0; i < p->tube_count; i++) {
        const sh4der_tube_t *t = &p->tube[i];

        if (!soft_edge(t->inner, t->outer) || !sh4der_finite_(t->rho) || !sh4der_unit_(t->weight) || !sh4der_finite_(t->phase))
            return 0;
    }
    for (i = 0; i < p->blocker_count; i++) {
        const sh4der_blocker_t *k = &p->blocker[i];

        if (!soft_edge(k->inner, k->outer) || !sh4der_finite_(k->rho) || !sh4der_finite_(k->x0) || !sh4der_finite_(k->x1) ||
            !(sh4der_finite_(k->depth) && k->depth >= -1.0f && k->depth <= 1.0f) || !sh4der_finite_(k->phase) || k->passes == 0)
            return 0;
    }
    return 1;
}

int sh4der_blade_prepare(sh4der_blade_frame_t *f, const sh4der_blade_params_t *p, uint32_t tick, float fade) {
    float u, s, c;
    int i;

    if (!sh4der_blade_params_valid(p)) {
        f->pulse = f->glow = f->light = f->fade = f->lo = f->hi = f->fade_x0 = f->inv_fade_len = 0.0f;
        f->ripple_k = f->ripple_phase = f->ripple_depth = 0.0f;
        f->tube_count = f->blocker_count = 0;
        return 0;
    }
    fade = sh4der_finite_(fade) ? sh4der_sat_(fade) : 0.0f;   /* the bit test first: -ffast-math folds a NaN compare away */
    u = sh4der_div_((float)(tick % p->period), (float)p->period);

    sh4der_sincos_(TWO_PI * sh4der_frac_((float)p->pulses * u), &s, &c);
    f->pulse = 0.5f * (1.0f - c);
    f->glow = p->glow_base + p->glow_amp * f->pulse;
    if (f->glow < SH4DER_BLADE_MIN_GLOW) f->glow = SH4DER_BLADE_MIN_GLOW;   /* the pulse never goes fully dark (AMOUNT 1 would reach 0) */
    f->light = (1.0f - p->light_swing * (1.0f - f->pulse)) * fade;
    f->fade = fade;
    f->lo = f->glow * p->floor;
    f->fade_x0 = p->fade_x0;
    f->inv_fade_len = sh4der_recip_pos_(p->fade_x1 - p->fade_x0);
    f->hi = f->glow * (1.0f - p->floor);
    f->ripple_k = TWO_PI * p->ripple_freq;
    f->ripple_phase = TWO_PI * sh4der_frac_((float)p->pulses * u);
    f->ripple_depth = p->ripple_depth;

    f->tube_count = p->tube_count;
    for (i = 0; i < p->tube_count; i++) {
        const sh4der_tube_t *t = &p->tube[i];

        sh4der_sincos_(TWO_PI * sh4der_frac_((float)t->turns * u + t->phase), &s, &c);
        f->tube_y[i] = p->axis[0] + t->rho * c;
        f->tube_z[i] = p->axis[1] + t->rho * s;
        f->tube_inner[i] = t->inner;
        f->tube_outer2[i] = t->outer * t->outer;
        f->tube_inv_feather[i] = sh4der_recip_pos_(t->outer - t->inner);
        f->tube_weight[i] = t->weight;
    }
    f->blocker_count = p->blocker_count;
    for (i = 0; i < p->blocker_count; i++) {
        const sh4der_blocker_t *k = &p->blocker[i];

        f->blk_x[i] = sh4der_lerp_(k->x0, k->x1, sh4der_frac_((float)k->passes * u + k->phase));
        sh4der_sincos_(TWO_PI * sh4der_frac_((float)k->turns * u), &s, &c);
        f->blk_y[i] = p->axis[0] + k->rho * c;
        f->blk_z[i] = p->axis[1] + k->rho * s;
        f->blk_inner[i] = k->inner;
        f->blk_outer2[i] = k->outer * k->outer;
        f->blk_inv_feather[i] = sh4der_recip_pos_(k->outer - k->inner);
        f->blk_depth[i] = k->depth;
    }
    return 1;
}

void sh4der_blade_shade(const sh4der_blade_frame_t *f, const float *xyz, size_t xyz_stride, float *out, unsigned count) {
    const uint8_t *v = (const uint8_t *)xyz;
    unsigned i;

    for (i = 0; i < count; i++, v += xyz_stride) {
        const float *p = (const float *)v;

        out[i] = sh4der_blade_intensity(f, p[0], p[1], p[2]);
    }
}

static uint32_t channel(float c, float k) {
    return (uint32_t)(sh4der_sat_(c * k) * 255.0f + 0.5f);   /* c and k are 0..1 (k from sh4der_blade_intensity, c the caller's colour) */
}

void sh4der_blade_argb(const sh4der_blade_frame_t *f, const float rgb[3], uint8_t a, const float *xyz, size_t xyz_stride,
                       uint32_t *out, size_t out_stride, unsigned count) {
    const uint8_t *v = (const uint8_t *)xyz;
    uint8_t *o = (uint8_t *)out;
    unsigned i;

    for (i = 0; i < count; i++, v += xyz_stride, o += out_stride) {
        const float *p = (const float *)v;
        float k = sh4der_blade_intensity(f, p[0], p[1], p[2]);

        *(uint32_t *)o = (uint32_t)a << 24 | channel(rgb[0], k) << 16 | channel(rgb[1], k) << 8 | channel(rgb[2], k);
    }
}

void sh4der_blade_argb_ramp(const sh4der_blade_frame_t *f, const float edge[3], const float core[3], uint8_t a, const float *xyz,
                            size_t xyz_stride, uint32_t *out, size_t out_stride, unsigned count) {
    const uint8_t *v = (const uint8_t *)xyz;
    uint8_t *o = (uint8_t *)out;
    unsigned i;

    for (i = 0; i < count; i++, v += xyz_stride, o += out_stride) {
        const float *p = (const float *)v;
        float c[3];

        sh4der_ramp_(edge, core, sh4der_blade_intensity(f, p[0], p[1], p[2]), c);
        *(uint32_t *)o = (uint32_t)a << 24 | channel(c[0], 1.0f) << 16 | channel(c[1], 1.0f) << 8 | channel(c[2], 1.0f);
    }
}
