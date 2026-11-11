/* SH4DER-PVR: the holo effect

   sh4der_holo.c
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE.
*/

#include "sh4der_holo.h"

static uint32_t channel(float c) {
    return (uint32_t)(sh4der_sat_(c) * 255.0f + 0.5f);
}

void sh4der_holo_params_default(sh4der_holo_params_t *p) {
    p->glow_base = SH4DER_HOLO_GLOW_BASE;
    p->glow_amp = SH4DER_HOLO_GLOW_AMP;
    p->core_base = SH4DER_HOLO_BASE;
    p->core_amp = SH4DER_HOLO_AMP;
    p->body = SH4DER_HOLO_BODY;
    p->add = SH4DER_HOLO_ADD;
    p->center[0] = SH4DER_HOLO_X;
    p->center[1] = SH4DER_HOLO_Y;
    p->center[2] = SH4DER_HOLO_Z;
    p->inner = SH4DER_HOLO_INNER;
    p->outer = SH4DER_HOLO_OUTER;
}

int sh4der_holo_params_valid(const sh4der_holo_params_t *p) {
    return p && sh4der_unit_(p->glow_base) && sh4der_unit_(p->glow_amp) && sh4der_unit_(p->core_base) && sh4der_unit_(p->core_amp) &&
           sh4der_unit_(p->body) && sh4der_unit_(p->add) && sh4der_finite_(p->center[0]) && sh4der_finite_(p->center[1]) &&
           sh4der_finite_(p->center[2]) && sh4der_finite_(p->inner) && sh4der_finite_(p->outer) && p->inner >= 0.0f && p->outer > p->inner;
}

void sh4der_holo_weights(const sh4der_holo_params_t *p, const float *xyz, size_t xyz_stride, uint8_t *out, unsigned count) {
    const uint8_t *v = (const uint8_t *)xyz;
    int ok = sh4der_holo_params_valid(p);
    float outer2 = ok ? p->outer * p->outer : 0.0f, inv = ok ? sh4der_recip_pos_(p->outer - p->inner) : 0.0f;
    unsigned i;

    for (i = 0; i < count; i++, v += xyz_stride) {
        const float *q = (const float *)v;
        float d2;

        out[i] = 0;
        if (!ok)
            continue;
        d2 = sh4der_len3_(q[0] - p->center[0], q[1] - p->center[1], q[2] - p->center[2]);
        if (d2 < outer2)
            out[i] = (uint8_t)(sh4der_falloff_(d2, p->inner, inv) * (float)(SH4DER_HOLO_LEVELS - 1) + 0.5f);
    }
}

int sh4der_holo_prepare(sh4der_holo_frame_t *f, const sh4der_holo_params_t *p, float breath, float fade) {
    float a, k;
    uint32_t alpha;
    int i;

    if (!sh4der_holo_params_valid(p)) {
        f->breath = f->glow = f->core = f->fade = 0.0f;
        for (i = 0; i < SH4DER_HOLO_LEVELS; i++)
            f->argb[i] = 0;
        return 0;
    }
    breath = sh4der_finite_(breath) ? sh4der_sat_(breath) : 0.5f;
    fade = sh4der_finite_(fade) ? sh4der_sat_(fade) : 0.0f;
    f->breath = breath;
    f->glow = p->glow_base + p->glow_amp * breath;
    f->core = p->core_base + p->core_amp * breath;
    f->fade = fade;
    alpha = channel(fade) << 24;
    a = f->glow * p->body;
    k = f->core * p->add * (1.0f / (float)(SH4DER_HOLO_LEVELS - 1));
    for (i = 0; i < SH4DER_HOLO_LEVELS; i++) {
        uint32_t g = channel(a + k * (float)i);

        f->argb[i] = alpha | g << 16 | g << 8 | g;
    }
    return 1;
}

void sh4der_holo_shade(const sh4der_holo_frame_t *f, const uint8_t *weight, uint32_t *out, size_t out_stride, unsigned count) {
    uint8_t *o = (uint8_t *)out;
    unsigned i;

    for (i = 0; i < count; i++, o += out_stride)
        *(uint32_t *)o = sh4der_holo_argb(f, weight[i]);
}
