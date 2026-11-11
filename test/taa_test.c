/* SH4DER-PVR

   test/taa_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of temporal anti-aliasing: the Halton values, the jitter cycle,
   the reprojection (pixel shifts, the corner warp against a matrix product
   in double), the weight, the frame step, the angle wrap, the layer colours
   and the feed copy offsets.
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_taa.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0002f
#endif

#define W 640.0f
#define H 480.0f
#define CX 320.0f
#define CY 240.0f

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int near(float a, float b, float tol) {
    return fabsf(a - b) <= tol;
}

/* Focal length in pixels for a 35 degree half horizontal field of view: focal = 320 / tan 35 degrees. */
static double focal35(void) {
    return 320.0 / tan(35.0 * 3.14159265358979 / 180.0);
}

/* Halton values in base 2 and base 3 against hand-worked fractions. */
static int halton_values(void) {
    if (!near(sh4der_taa_halton(0, 2), 0.0f, SH4DER_TEST_TOL) || !near(sh4der_taa_halton(1, 2), 0.5f, SH4DER_TEST_TOL) ||
        !near(sh4der_taa_halton(2, 2), 0.25f, SH4DER_TEST_TOL) || !near(sh4der_taa_halton(3, 2), 0.75f, SH4DER_TEST_TOL) ||
        !near(sh4der_taa_halton(4, 2), 0.125f, SH4DER_TEST_TOL))
        return fail("halton base 2");
    if (!near(sh4der_taa_halton(1, 3), 1.0f / 3.0f, SH4DER_TEST_TOL) || !near(sh4der_taa_halton(2, 3), 2.0f / 3.0f, SH4DER_TEST_TOL) ||
        !near(sh4der_taa_halton(3, 3), 1.0f / 9.0f, SH4DER_TEST_TOL) || !near(sh4der_taa_halton(4, 3), 4.0f / 9.0f, SH4DER_TEST_TOL))
        return fail("halton base 3");
    return 0;
}

/* The jitter table: phases are distinct, sum to zero over a cycle, and stay under 0.75 px. */
static int jitter_cycle(void) {
    sh4der_taa_t t;
    float sx = 0.0f, sy = 0.0f, mx = 0.0f;
    int i, j;
    sh4der_taa_init(&t);
    for (i = 0; i < SH4DER_TAA_PHASES; i++) {
        sx += t.jit[i][0];
        sy += t.jit[i][1];
        if (fabsf(t.jit[i][0]) > mx)
            mx = fabsf(t.jit[i][0]);
        if (fabsf(t.jit[i][1]) > mx)
            mx = fabsf(t.jit[i][1]);
        for (j = 0; j < i; j++)
            if (near(t.jit[i][0], t.jit[j][0], 1e-6f) && near(t.jit[i][1], t.jit[j][1], 1e-6f))
                return fail("jitter phases repeat");
    }
    if (!near(sx, 0.0f, 1e-5f) || !near(sy, 0.0f, 1e-5f))
        return fail("jitter cycle does not sum to zero");
    if (!(mx < 0.75f))
        return fail("jitter range past 0.75 px");
    return 0;
}

/* The jitter in clip units: the centre plus the jitter lands on the pixel, with y down. */
static int jitter_clip(void) {
    const float jit[2] = { 0.25f, -0.375f };
    float clip[2];
    sh4der_taa_jitter_clip(jit, W, H, clip);
    /* ndc x = 0 is the centre; the pixel is (1 + ndc) W / 2, the row (1 - ndc) H / 2 (y down) */
    if (!near((1.0f + clip[0]) * W * 0.5f, W * 0.5f + jit[0], 1e-3f) || !near((1.0f - clip[1]) * H * 0.5f, H * 0.5f + jit[1], 1e-3f))
        return fail("jitter clip units");
    return 0;
}

/* Reprojection: yaw and pitch shifts in pixels, their signs, the warp at the centre, the corners and a wide yaw. */
static int reproject(void) {
    const double f = focal35();
    const float fx = (float)f, fy = (float)f;
    float s[2], p[2];
    const float a = 0.5f * 3.14159265f / 180.0f;   /* a half degree of yaw */
    int i, j;
    sh4der_taa_shift(fx, fy, a, 0.0f, s);
    if (!near(s[0], (float)(-f * tan(a)), 1e-3f) || !near(s[1], 0.0f, 1e-5f))
        return fail("yaw shift = -f tan(yaw)");
    if (!(s[0] < 0.0f))
        return fail("turn right: the picture moves left");
    sh4der_taa_shift(fx, fy, 0.0f, 0.01f, s);
    if (!near(s[1], (float)(f * tan(0.01)), 1e-3f) || !near(s[0], 0.0f, 1e-5f))
        return fail("pitch shift = +f tan(pitch)");
    /* the warp at the centre is the centre shift (both axes, the cos term included) */
    sh4der_taa_warp(fx, fy, CX, CY, a, 0.01f, CX, CY, p);
    sh4der_taa_shift(fx, fy, a, 0.01f, s);
    if (!near(CX - p[0], s[0], 1e-3f) || !near(CY - p[1], s[1], 1e-3f))
        return fail("warp at the centre = centre shift");
    /* the corners against a matrix product in double: pitch by R_X(-b), then yaw by R_Y(a) */
    for (i = 0; i < 4; i++)
        for (j = 0; j < 3; j++) {
            const float x = (i & 1) ? W : 0.0f, y = (i & 2) ? H : 0.0f;
            const float dyaw = (j == 0) ? 0.02f : (j == 1) ? -0.013f : 0.0f;
            const float dpitch = (j == 1) ? 0.008f : (j == 2) ? -0.02f : 0.0f;
            const double ca = cos(dyaw), sa = sin(dyaw), cb = cos(dpitch), sb = sin(dpitch);
            const double rx = (x - CX) / f, ry = -(y - CY) / f;
            /* R_X(-b) = [[1,0,0],[0,cb,sb],[0,-sb,cb]] applied to (rx, ry, 1) */
            const double x1 = rx, y1 = cb * ry + sb, z1 = -sb * ry + cb;
            /* R_Y(a) = [[ca,0,sa],[0,1,0],[-sa,0,ca]] */
            const double x2 = ca * x1 + sa * z1, y2 = y1, z2 = -sa * x1 + ca * z1;
            const double wx = CX + f * x2 / z2, wy = CY - f * y2 / z2;
            sh4der_taa_warp(fx, fy, CX, CY, dyaw, dpitch, x, y, p);
            if (!near(p[0], (float)wx, 2e-3f) || !near(p[1], (float)wy, 2e-3f))
                return fail("corner warp against the matrix product");
        }
    /* a wide yaw: the left edge's shift differs from the centre's (the warp is not a uniform shift) */
    sh4der_taa_warp(fx, fy, CX, CY, 0.2f, 0.0f, 0.0f, CY, p);
    sh4der_taa_shift(fx, fy, 0.2f, 0.0f, s);
    if (!(fabsf((0.0f - p[0]) - s[0]) > 1.0f))
        return fail("wide yaw: the edge moves more than the centre");
    return 0;
}

/* The history weight: its ends, monotonic in speed, and the midpoint at the mean of the squared speed limits. */
static int weight(void) {
    float prev = 2.0f, s2;
    int k;
    if (!near(sh4der_taa_weight(0.0f), SH4DER_TAA_WEIGHT_HI, 1e-6f) || !near(sh4der_taa_weight(1000.0f), SH4DER_TAA_WEIGHT_LO, 1e-6f))
        return fail("weight ends");
    for (k = 0; k <= 400; k++) {
        const float w = sh4der_taa_weight((float)k * 0.25f);
        if (w > prev + 1e-7f || w < SH4DER_TAA_WEIGHT_LO - 1e-7f || w > SH4DER_TAA_WEIGHT_HI + 1e-7f)
            return fail("weight monotonic in speed");
        prev = w;
    }
    s2 = 0.5f * (SH4DER_TAA_SPEED_LO * SH4DER_TAA_SPEED_LO + SH4DER_TAA_SPEED_HI * SH4DER_TAA_SPEED_HI);
    if (!near(sh4der_taa_weight(s2), 0.5f * (SH4DER_TAA_WEIGHT_HI + SH4DER_TAA_WEIGHT_LO), 1e-4f))
        return fail("weight midpoint");
    return 0;
}

/* The angle difference wraps across pi. */
static int angle_wrap(void) {
    if (!near(sh4der_taa_angle_delta(-3.1f, 3.1f), 0.0832f, 1e-4f) || !near(sh4der_taa_angle_delta(0.3f, 0.1f), 0.2f, 1e-5f))
        return fail("angle delta wraps");
    return 0;
}

/* One TAA state through a still camera, a slow turn, a fast turn, a cut and a reset. */
static int steps(void) {
    sh4der_taa_t t;
    sh4der_taa_frame_t o;
    const float fx = (float)focal35(), fy = fx;
    float yaw = 0.0f, pitch = 0.0f, first[2];
    int k;
    sh4der_taa_init(&t);
    sh4der_taa_step(&t, yaw, pitch, fx, fy, &o);
    if (!o.cut || o.weight != 0.0f || o.shift[0] != 0.0f)
        return fail("first frame: cut, no history");
    first[0] = o.jitter[0];
    first[1] = o.jitter[1];
    for (k = 1; k <= 16; k++) {   /* still camera: full weight, phases cycle */
        sh4der_taa_step(&t, yaw, pitch, fx, fy, &o);
        if (o.cut || !near(o.weight, SH4DER_TAA_WEIGHT_HI, 1e-6f) || o.shift[0] != 0.0f || o.shift[1] != 0.0f)
            return fail("still camera: full history");
        if (k % SH4DER_TAA_PHASES == 0 && (!near(o.jitter[0], first[0], 1e-6f) || !near(o.jitter[1], first[1], 1e-6f)))
            return fail("jitter cycle repeats");
    }
    yaw += 0.1f;   /* a slow turn: 0.1 rad a frame is 46 px at this focal, past SPEED_HI: no history, not a cut */
    sh4der_taa_step(&t, yaw, pitch, fx, fy, &o);
    if (o.cut || o.weight != SH4DER_TAA_WEIGHT_LO || !(o.shift[0] < -40.0f))
        return fail("fast turn: no history, not a cut");
    yaw += 0.5f;   /* 0.5 rad in a frame: a cut */
    sh4der_taa_step(&t, yaw, pitch, fx, fy, &o);
    if (!o.cut || o.weight != 0.0f || o.shift[0] != 0.0f)
        return fail("cut: no history");
    /* the weight the frame gets is the one the speed gives: a still frame after the cut is full again */
    sh4der_taa_step(&t, yaw, pitch, fx, fy, &o);
    if (o.cut || !near(o.weight, SH4DER_TAA_WEIGHT_HI, 1e-6f))
        return fail("after the cut: full history");
    sh4der_taa_reset(&t);
    sh4der_taa_step(&t, yaw, pitch, fx, fy, &o);
    if (!o.cut)
        return fail("reset: cut");
    return 0;
}

/* The keep and add layer colours at 0, 0.75 and 1, and alpha 0. */
static int colours(void) {
    if (sh4der_taa_keep_argb(0.0f) != 0x00ffffffu || sh4der_taa_keep_argb(0.75f) != 0x00404040u || sh4der_taa_keep_argb(1.0f) != 0u)
        return fail("keep colour");
    if (sh4der_taa_add_argb(0.0f) != 0u || sh4der_taa_add_argb(0.75f) != 0x00bfbfbfu || sh4der_taa_add_argb(1.0f) != 0x00ffffffu)
        return fail("add colour");
    if ((sh4der_taa_keep_argb(0.3f) | sh4der_taa_add_argb(0.3f)) & 0xff000000u)
        return fail("alpha 0");
    return 0;
}

static int copy_px(void) {   /* the feed copy's whole offsets: x even (the parity), y whole */
    if (sh4der_taa_even_px(0.9f) != 0 || sh4der_taa_even_px(1.0f) != 2 || sh4der_taa_even_px(3.1f) != 4 || sh4der_taa_even_px(-3.1f) != -4)
        return fail("even px");
    if (sh4der_taa_whole_px(2.5f) != 3 || sh4der_taa_whole_px(-2.6f) != -3 || sh4der_taa_whole_px(-2.4f) != -2)
        return fail("whole px");
    {
        sh4der_taa_t t;
        sh4der_taa_frame_t o;
        sh4der_taa_init(&t);
        sh4der_taa_step(&t, 0.0f, 0.0f, (float)focal35(), (float)focal35(), &o);
        sh4der_taa_step(&t, 0.02f, 0.0f, (float)focal35(), (float)focal35(), &o);
        if (o.copy_px[0] % 2 != 0 || o.copy_px[0] != -10 || o.copy_px[1] != 0)
            return fail("copy_px from the step");
    }
    return 0;
}

int main(void) {
    if (halton_values() || jitter_cycle() || jitter_clip() || reproject() || weight() || angle_wrap() || steps() || colours() || copy_px())
        return 1;
    printf("taa_test: ok\n");
    return 0;
}
