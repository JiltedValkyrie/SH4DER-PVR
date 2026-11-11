/* SH4DER-PVR: the blade effect

   test/blade_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check: the shipped sword numbers at fixed ticks and points (the
   storyboard's reference maths), the early-outs against a plain formula
   with none, the period closing, the fade, bad params, and the shade and
   packed-colour loops against the per-vertex call.
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_blade.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0002f
#endif
#define TOL SH4DER_TEST_TOL

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int near(float a, float b, float tol) {
    return fabsf(a - b) <= tol;
}

/* The formula with no early-out and no table: every tube and blocker, plain C. */
static float reference(const sh4der_blade_params_t *p, uint32_t tick, float fade, float x, float y, float z) {
    float u = (float)(tick % p->period) / (float)p->period;
    float pulse = 0.5f * (1.0f - cosf(6.28318530718f * p->pulses * u));
    float glow = p->glow_base + p->glow_amp * pulse;
    float tube = 0.0f, mask = 1.0f, v;
    int i;

    for (i = 0; i < p->tube_count; i++) {
        const sh4der_tube_t *t = &p->tube[i];
        float a = 6.28318530718f * (t->turns * u + t->phase);
        float d = hypotf(y - (p->axis[0] + t->rho * cosf(a)), z - (p->axis[1] + t->rho * sinf(a)));
        float s = 0.0f;

        if (d <= t->inner) {
            s = 1.0f;
        } else if (d < t->outer) {
            float q = (d - t->inner) / (t->outer - t->inner);

            s = 1.0f - q * q * (3.0f - 2.0f * q);
        }
        s *= t->weight;
        if (s > tube)
            tube = s;
    }
    for (i = 0; i < p->blocker_count; i++) {
        const sh4der_blocker_t *k = &p->blocker[i];
        float w = k->passes * u + k->phase;
        float a = 6.28318530718f * k->turns * u;
        float bx = k->x0 + (k->x1 - k->x0) * (w - floorf(w));
        float d = sqrtf((x - bx) * (x - bx) + powf(y - (p->axis[0] + k->rho * cosf(a)), 2.0f) +
                        powf(z - (p->axis[1] + k->rho * sinf(a)), 2.0f));
        float s = 0.0f;

        if (d <= k->inner) {
            s = 1.0f;
        } else if (d < k->outer) {
            float q = (d - k->inner) / (k->outer - k->inner);

            s = 1.0f - q * q * (3.0f - 2.0f * q);
        }
        mask *= 1.0f - k->depth * s;
    }
    v = glow * (p->floor + (1.0f - p->floor) * tube) * mask;
    {   /* the base fade: smoothstep from fade_x0 to fade_x1 */
        float b = (x - p->fade_x0) / (p->fade_x1 - p->fade_x0);

        b = b < 0.0f ? 0.0f : b > 1.0f ? 1.0f : b;
        return (v > 1.0f ? 1.0f : v) * fade * b * b * (3.0f - 2.0f * b);
    }
}

/* tools/sh4der_storyboard.py sh4der_ref, rounded to 4 places. */
static int shipped(void) {
    static const struct { uint32_t tick; float x, y, z, want; } pin[] = {
        {0, 0.20f, 0.0f, 0.06f, 0.4112f},   {0, 0.20f, 0.0f, -0.05f, 0.2200f},  {0, 0.33f, 0.0f, 0.01f, 0.2200f},
        {24, 0.20f, 0.0f, 0.06f, 0.3100f},  {24, 0.20f, 0.0f, -0.05f, 0.3100f}, {24, 0.33f, 0.0f, 0.01f, 0.3100f},
        {48, 0.20f, 0.0f, 0.06f, 1.0000f},  {48, 0.20f, 0.0f, -0.05f, 0.6099f}, {48, 0.33f, 0.0f, 0.01f, 0.4000f},
        {144, 0.20f, 0.0f, 0.06f, 0.4000f}, {144, 0.20f, 0.0f, -0.05f, 1.0000f}, {144, 0.33f, 0.0f, 0.01f, 0.4000f},
    };
    static const struct { uint32_t tick; float glow, light; } pulse[] = {
        {0, 0.55f, 0.85f}, {24, 0.775f, 0.925f}, {48, 1.0f, 1.0f}, {96, 0.55f, 0.85f}, {144, 1.0f, 1.0f},
    };
    sh4der_blade_params_t p;
    sh4der_blade_frame_t f;
    unsigned i;

    sh4der_blade_params_default(&p);
    if (!sh4der_blade_params_valid(&p))
        return fail("default params invalid");
    for (i = 0; i < sizeof pin / sizeof pin[0]; i++) {
        if (!sh4der_blade_prepare(&f, &p, pin[i].tick, 1.0f))
            return fail("shipped prepare");
        if (!near(sh4der_blade_intensity(&f, pin[i].x, pin[i].y, pin[i].z), pin[i].want, 0.0001f + TOL)) {
            fprintf(stderr, "tick %u (%g %g %g): %f, want %f\n", (unsigned)pin[i].tick, pin[i].x, pin[i].y, pin[i].z,
                    sh4der_blade_intensity(&f, pin[i].x, pin[i].y, pin[i].z), pin[i].want);
            return fail("shipped intensity");
        }
    }
    for (i = 0; i < sizeof pulse / sizeof pulse[0]; i++) {
        sh4der_blade_prepare(&f, &p, pulse[i].tick, 1.0f);
        if (!near(f.glow, pulse[i].glow, TOL) || !near(f.light, pulse[i].light, TOL))
            return fail("shipped glow / light");
    }
    /* the blocker at tick 48 is mid-blade: x 0.15 */
    sh4der_blade_prepare(&f, &p, 48, 1.0f);
    if (f.blocker_count != 1 || !near(f.blk_x[0], 0.15f, TOL))
        return fail("blocker position");
    return 0;
}

static uint32_t rng = 12345u;
static float rnd(float lo, float hi) {
    rng = rng * 1664525u + 1013904223u;
    return lo + (hi - lo) * (float)(rng >> 8) / 16777216.0f;
}

/* Early-outs and tables against the plain formula, over the sword's box and around it. */
static int against_reference(void) {
    sh4der_blade_params_t p;
    sh4der_blade_frame_t f;
    int n, worst_at = -1;
    float worst = 0.0f;

    sh4der_blade_params_default(&p);
    p.blocker_count = 2;
    p.blocker[1].phase = 0.5f;
    p.blocker[1].turns = -3;
    for (n = 0; n < 200000; n++) {
        uint32_t tick = (uint32_t)rnd(0.0f, 5000.0f);
        float x = rnd(-0.15f, 0.45f), y = rnd(-0.08f, 0.08f), z = rnd(-0.12f, 0.12f), fade = rnd(0.0f, 1.0f);
        float got, want, d;

        if (n % 1000 == 0) {
            p.blocker_count = (uint8_t)(n / 1000 % 3);
            p.tube_count = (uint8_t)(n / 3000 % 3);
        }
        sh4der_blade_prepare(&f, &p, tick, fade);
        got = sh4der_blade_intensity(&f, x, y, z);
        want = reference(&p, tick, fade, x, y, z);
        if (got < 0.0f || got > 1.0f)
            return fail("intensity outside 0..1");
        d = fabsf(got - want);
        if (d > worst) {
            worst = d;
            worst_at = n;
        }
    }
    if (worst > 2.0f * TOL) {
        fprintf(stderr, "worst %g at %d\n", worst, worst_at);
        return fail("reference");
    }
    return 0;
}

static int period_and_fade(void) {
    sh4der_blade_params_t p;
    sh4der_blade_frame_t f, g;
    uint32_t t;

    sh4der_blade_params_default(&p);
    for (t = 0; t < SH4DER_BLADE_PERIOD; t += 7) {
        sh4der_blade_prepare(&f, &p, t, 1.0f);
        sh4der_blade_prepare(&g, &p, t + 5u * SH4DER_BLADE_PERIOD, 1.0f);
        if (sh4der_blade_intensity(&f, 0.2f, 0.0f, 0.06f) != sh4der_blade_intensity(&g, 0.2f, 0.0f, 0.06f))
            return fail("period does not close");
    }
    /* the loop is seamless: the last tick is next to the first */
    sh4der_blade_prepare(&f, &p, SH4DER_BLADE_PERIOD - 1, 1.0f);
    sh4der_blade_prepare(&g, &p, 0, 1.0f);
    if (!near(f.glow, g.glow, 0.001f) || !near(f.tube_y[0], g.tube_y[0], 0.003f) || !near(f.tube_z[1], g.tube_z[1], 0.003f))
        return fail("seam");
    sh4der_blade_prepare(&f, &p, 48, 0.0f);
    if (sh4der_blade_intensity(&f, 0.2f, 0.0f, 0.06f) != 0.0f || f.light != 0.0f)
        return fail("fade 0");
    sh4der_blade_prepare(&f, &p, 48, 0.5f);
    if (!near(sh4der_blade_intensity(&f, 0.2f, 0.0f, 0.06f), 0.5f, TOL) || !near(f.light, 0.5f, TOL))
        return fail("fade 0.5");
    sh4der_blade_prepare(&f, &p, 48, 7.0f);
    if (f.fade != 1.0f)
        return fail("fade clamp");
    sh4der_blade_prepare(&f, &p, 48, NAN);
    if (f.fade != 0.0f)
        return fail("fade NaN");
    p.light_swing = 0.0f;
    sh4der_blade_prepare(&f, &p, 0, 1.0f);
    if (f.light != 1.0f)
        return fail("swing 0 is constant (OpenCE)");
    return 0;
}

/* Ripples along the axis: off by default (the shipped sword's intensities unchanged); on, a crest keeps the light and a trough takes `depth` of it. */
static int ripples(void) {
    sh4der_blade_params_t p;
    sh4der_blade_frame_t f, g;
    float x;

    sh4der_blade_params_default(&p);
    if (p.ripple_freq != 0.0f || p.ripple_depth != 0.0f)
        return fail("ripples on by default");
    sh4der_blade_prepare(&f, &p, 0, 1.0f);
    p.ripple_freq = 10.0f;   /* a ripple every 0.1 */
    p.ripple_depth = 0.5f;
    if (!sh4der_blade_prepare(&g, &p, 0, 1.0f))
        return fail("ripple params refused");
    for (x = 0.0f; x < 0.35f; x += 0.01f) {
        const float a = sh4der_blade_intensity(&f, x, 0.0f, 0.06f), b = sh4der_blade_intensity(&g, x, 0.0f, 0.06f);

        if (b > a + 1e-6f || b < 0.5f * a - 1e-6f)
            return fail("ripple outside 1 - depth .. 1");
    }
    if (!near(sh4der_blade_intensity(&g, 0.1f, 0.0f, 0.06f), sh4der_blade_intensity(&f, 0.1f, 0.0f, 0.06f), 1e-4f) ||
        !near(sh4der_blade_intensity(&g, 0.15f, 0.0f, 0.06f), 0.5f * sh4der_blade_intensity(&f, 0.15f, 0.0f, 0.06f), 1e-4f))
        return fail("ripple crest at 0.1, trough at 0.15 (tick 0)");
    return 0;
}

static int bad_params(void) {
    sh4der_blade_params_t p;
    sh4der_blade_frame_t f;
    int k;

    for (k = 0; k < 11; k++) {
        sh4der_blade_params_default(&p);
        switch (k) {
        case 0: p.period = 0; break;
        case 1: p.tube[0].outer = p.tube[0].inner; break;
        case 2: p.blocker[0].inner = -1.0f; break;
        case 3: p.tube_count = SH4DER_TUBE_MAX + 1; break;
        case 4: p.blocker_count = SH4DER_BLOCKER_MAX + 1; break;
        case 5: p.blocker[0].passes = 0; break;
        case 6: p.floor = 1.5f; break;
        case 7: p.axis[1] = NAN; break;
        case 8: p.tube[1].weight = INFINITY; break;
        case 9: p.ripple_depth = 1.5f; break;
        case 10: p.ripple_freq = -1.0f; break;
        }
        if (sh4der_blade_params_valid(&p) || sh4der_blade_prepare(&f, &p, 10, 1.0f))
            return fail("bad params accepted");
        if (sh4der_blade_intensity(&f, 0.2f, 0.0f, 0.06f) != 0.0f || f.light != 0.0f)
            return fail("bad params frame not dark");
    }
    if (sh4der_blade_params_valid(NULL))
        return fail("NULL params");
    return 0;
}

static int loops(void) {
    struct { float xyz[3]; uint8_t pad[12]; } v[40];
    uint32_t argb[40 * 2];
    float out[40], rgb[3] = {0.35f, 0.82f, 1.0f};
    sh4der_blade_params_t p;
    sh4der_blade_frame_t f;
    unsigned i;

    sh4der_blade_params_default(&p);
    sh4der_blade_prepare(&f, &p, 48, 1.0f);
    for (i = 0; i < 40; i++) {
        v[i].xyz[0] = -0.05f + 0.01f * i;
        v[i].xyz[1] = 0.0f;
        v[i].xyz[2] = (i & 1) ? 0.06f : -0.05f;
    }
    sh4der_blade_shade(&f, v[0].xyz, sizeof v[0], out, 40);
    sh4der_blade_argb(&f, rgb, 0x80, v[0].xyz, sizeof v[0], argb, 2 * sizeof argb[0], 40);
    for (i = 0; i < 40; i++) {
        float k = sh4der_blade_intensity(&f, v[i].xyz[0], v[i].xyz[1], v[i].xyz[2]);
        uint32_t c = argb[2 * i];

        if (out[i] != k)
            return fail("shade");
        if (c >> 24 != 0x80 || (c >> 16 & 255) != (uint32_t)(rgb[0] * k * 255.0f + 0.5f) ||
            (c >> 8 & 255) != (uint32_t)(rgb[1] * k * 255.0f + 0.5f) || (c & 255) != (uint32_t)(rgb[2] * k * 255.0f + 0.5f))
            return fail("argb");
    }
    return 0;
}

/* A vertex exactly on a tube's line or at a blocker's centre (d2 == 0): FSRRA's square root of 0 is NaN on the SH4ZAM path, so the
   core must answer before the root. Also a zero core radius. */
static int centres(void) {
    sh4der_blade_params_t p;
    sh4der_blade_frame_t f;
    uint32_t t;
    int k;

    for (k = 0; k < 2; k++) {
        sh4der_blade_params_default(&p);
        if (k) {
            p.tube[0].inner = p.tube[1].inner = p.blocker[0].inner = 0.0f;
        }
        for (t = 0; t < SH4DER_BLADE_PERIOD; t += 5) {
            float a, b, c;

            sh4der_blade_prepare(&f, &p, t, 1.0f);
            a = sh4der_blade_intensity(&f, 0.2f, f.tube_y[0], f.tube_z[0]);
            b = sh4der_blade_intensity(&f, 0.2f, f.tube_y[1], f.tube_z[1]);
            c = sh4der_blade_intensity(&f, f.blk_x[0], f.blk_y[0], f.blk_z[0]);
            if (!(a >= 0.0f && a <= 1.0f) || !(b >= 0.0f && b <= 1.0f) || !(c >= 0.0f && c <= 1.0f))
                return fail("NaN or range at a centre");
            if (!near(a, reference(&p, t, 1.0f, 0.2f, f.tube_y[0], f.tube_z[0]), 2.0f * TOL) ||
                !near(c, reference(&p, t, 1.0f, f.blk_x[0], f.blk_y[0], f.blk_z[0]), 2.0f * TOL))
                return fail("centre against the reference");
        }
    }
    return 0;
}

int main(void) {
    if (shipped() || against_reference() || period_and_fade() || ripples() || bad_params() || loops() || centres())
        return 1;
    printf("blade_test: ok\n");
    return 0;
}
