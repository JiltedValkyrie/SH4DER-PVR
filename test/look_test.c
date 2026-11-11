/* SH4DER-PVR

   test/look_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the look effect: the luma bins, the log average, the eye
   (into light over-bright then settled, into dark darker then opened up,
   the times, the gain's bounds, last frame's gain divided out), the grade's
   layers against clamp(A x + B), the layer colours and the glare.
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_look.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0002f
#endif

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

/* Checks the luma of 565 pixels, the log average of one bin and of a half-black frame, and the no-sample case. */
static int luma_and_average(void) {
    sh4der_look_bins_t t;
    uint16_t hist[SH4DER_LOOK_BINS] = { 0 };
    float avg;
    if (sh4der_look_luma565(0x0000u) != 0 || sh4der_look_luma565(0xffffu) != 62 || sh4der_look_luma565(0xf800u) >= sh4der_look_luma565(0x07e0u))
        return fail("luma565");
    sh4der_look_bins(&t);
    hist[31] = 100;
    avg = sh4der_look_log_average(&t, hist, 100);
    if (fabsf(avg - logf(31.5f / 64.0f + SH4DER_LOOK_FLOOR)) > 0.01f)
        return fail("log average of one bin");
    hist[0] = 100;   /* half black: the log average falls far below the plain mean */
    avg = sh4der_look_log_average(&t, hist, 200);
    if (!(expf(avg) < 0.1f))
        return fail("log average weighs the dark");
    if (sh4der_look_log_average(&t, hist, 0) > -5.0f)
        return fail("no samples");
    return 0;
}

/* The eye seeing a scene of luma `lum` for `frames` at 30 fps; the frame read carries the gain given the frame before. */
static float run(sh4der_look_eye_t *e, const sh4der_look_params_t *p, float lum, int frames) {
    float g = e->gain;
    int i;
    for (i = 0; i < frames; i++)
        g = sh4der_look_eye_step(e, p, logf(lum) + e->gain_ln, 1.0f / 30.0f);
    return g;
}

/* Checks the eye: middle grey reads 1, a dark room opens up, a bright step is over-bright at once and then settles,
   the ln and exp series, and reset. */
static int eye(void) {
    sh4der_look_params_t p;
    sh4der_look_eye_t e;
    float g0, g;
    sh4der_look_params_default(&p);
    sh4der_look_eye_reset(&e);
    g0 = run(&e, &p, SH4DER_LOOK_REF, 1);   /* adapted at once, to middle grey: no gain */
    if (fabsf(g0 - 1.0f) > 0.02f)
        return fail("middle grey reads 1");
    run(&e, &p, 0.02f, 600);   /* a dark room, 20 s: opened up, above 1 */
    g = e.gain;
    if (!(g > 1.1f && g <= SH4DER_LOOK_GAIN_HI + 1e-4f))
        return fail("dark opens up");
    g = run(&e, &p, 0.5f, 1);   /* step outside: over-bright at once */
    if (!(g >= SH4DER_LOOK_GAIN_HI - 0.01f))
        return fail("into light: over-bright");
    run(&e, &p, 0.5f, (int)(SH4DER_LOOK_LIGHT_S * 30.0f));   /* 95 % adapted after LIGHT_S */
    g = e.gain;
    {
        const float settled = expf(p.open * (logf(SH4DER_LOOK_REF) - logf(0.5f)));
        const float want = settled < SH4DER_LOOK_GAIN_LO ? SH4DER_LOOK_GAIN_LO : settled;
        if (!(g < 1.0f && fabsf(g - want) < 0.1f))   /* 95 %: the last 5 % of the jump (x TRANSIENT) is still over */
            return fail("into light: settled after LIGHT_S");
        run(&e, &p, 0.5f, (int)(2.0f * SH4DER_LOOK_LIGHT_S * 30.0f));
        if (!(fabsf(e.gain - want) < 0.01f))
            return fail("into light: settled");
    }
    if (fabsf(sh4der_look_ln_(1.0f)) > 1e-6f || fabsf(sh4der_look_exp_(0.0f) - 1.0f) > 1e-6f)
        return fail("ln 1 = 0, exp 0 = 1");
    {
        int i;
        for (i = 1; i <= 100; i++) {
            const float x = (float)i * 0.015f;
            if (fabsf(sh4der_look_ln_(x) - logf(x)) > 1e-4f || fabsf(sh4der_look_exp_(x - 0.75f) - expf(x - 0.75f)) > 3e-4f)
                return fail("ln / exp series");
        }
    }
    g = run(&e, &p, 0.02f, 1);   /* back into the dark: darker at once */
    if (!(g <= SH4DER_LOOK_GAIN_LO + 0.01f))
        return fail("into dark: darker");
    g = run(&e, &p, 0.02f, 30);   /* one second on: still adapting (DARK_S 6) */
    if (!(g < 1.0f))
        return fail("into dark: slow");
    sh4der_look_eye_reset(&e);
    if (e.ready || e.gain != 1.0f)
        return fail("reset");
    return 0;
}

/* Checks the layer count, that the layers give clamp(A x + B) per channel, and the layer ARGB pack. */
static int grade(void) {
    static const sh4der_look_grade_t cases[] = {
        { { 1, 1, 1 }, { 0, 0, 0 }, 1.0f, 1.0f },          /* neutral: no layer */
        { { 1, 1, 1 }, { 0, 0, 0 }, 1.0f, 0.8f },          /* exposure down */
        { { 1, 1, 1 }, { 0, 0, 0 }, 1.0f, 1.4f },          /* exposure up */
        { { 1.2f, 1.0f, 0.8f }, { 0.05f, 0, -0.04f }, 1.0f, 1.0f },   /* warm, lifted red, crushed blue */
        { { 1, 1, 1 }, { 0, 0, 0 }, 1.3f, 1.0f },          /* contrast up */
        { { 1, 1, 1 }, { 0, 0, 0 }, 0.7f, 1.0f },          /* contrast down */
    };
    static const int layers[] = { 0, 1, 1, 0, 0, 0 };   /* 0: not checked */
    unsigned k;
    for (k = 0; k < sizeof cases / sizeof cases[0]; k++) {
        sh4der_look_layer_t l[SH4DER_LOOK_LAYERS];
        const int n = sh4der_look_solve(&cases[k], l);
        int c, i, s;
        if (n > SH4DER_LOOK_LAYERS || (layers[k] || k == 0 ? n != layers[k] : 0))
            return fail("layer count");
        for (c = 0; c < 3; c++)
            for (s = 0; s <= 20; s++) {
                const float x = (float)s / 20.0f;
                const float a = cases[k].gain[c] * cases[k].exposure * cases[k].contrast, b = cases[k].lift[c] + 0.5f * (1.0f - cases[k].contrast);
                float y = x, want = a * x + b;
                want = want < 0.0f ? 0.0f : want > 1.0f ? 1.0f : want;
                for (i = 0; i < n; i++)
                    y = sh4der_look_apply_(&l[i], c, y);
                if (fabsf(y - want) > 1.0f / 255.0f + SH4DER_TEST_TOL)
                    return fail("layers give clamp(A x + B)");
            }
    }
    {
        const sh4der_look_layer_t l = { SH4DER_LOOK_MULTIPLY, { 1.0f, 0.5f, 0.0f } };
        if (sh4der_look_argb(&l) != 0xffff8000u)
            return fail("argb");
    }
    return 0;
}

/* Checks that zero glare changes nothing, and full glare leans white and thickens where there is fog. */
static int glare(void) {
    float rgb[3] = { 0.2f, 0.4f, 0.6f }, table[4] = { 0.0f, 0.2f, 0.5f, 1.0f };
    sh4der_look_glare(0.0f, rgb, table, 4);
    if (rgb[0] != 0.2f || table[1] != 0.2f)
        return fail("glare 0 changes nothing");
    sh4der_look_glare(1.0f, rgb, table, 4);
    if (!(rgb[0] > 0.5f && rgb[2] > rgb[0]) || table[0] != 0.0f || !(table[1] > 0.45f) || table[3] != 1.0f)
        return fail("glare: white lean, thicker where fog is, none where none");
    return 0;
}

int main(void) {
    if (luma_and_average() || eye() || grade() || glare())
        return 1;
    printf("look_test: ok\n");
    return 0;
}
