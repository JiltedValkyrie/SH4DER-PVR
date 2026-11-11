/* SH4DER-PVR

   test/trail_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of motion blur and the feed route's maths: the weight, the copy
   offsets and their order, the cut and still camera, the TAA copy offset and
   the keep and add sum. Ends with an RGB565 drift report: the history blend
   iterated for 900 frames.
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_trail.h"

#define FOCAL_FX 457.1f   /* focal in pixels: 320 / tan 35 deg */

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int near(float a, float b, float tol) {
    return fabsf(a - b) <= tol;
}

/* The weight: its ends, rises with speed up to the cap, and the midpoint at the mean of the squared speed limits. */
static int weight_checks(void) {
    float prev = -1.0f, s2;
    int k;
    if (!near(sh4der_trail_weight(0.0f), SH4DER_TRAIL_WEIGHT_LO, 1e-6f) || !near(sh4der_trail_weight(1000.0f), SH4DER_TRAIL_WEIGHT_HI, 1e-6f))
        return fail("trail weight ends");
    for (k = 0; k <= 400; k++) {   /* rises with speed, never past the cap */
        const float w = sh4der_trail_weight((float)k * 0.25f);
        if (w < prev - 1e-7f || w < SH4DER_TRAIL_WEIGHT_LO - 1e-7f || w > SH4DER_TRAIL_WEIGHT_HI + 1e-7f)
            return fail("trail weight rises with speed, capped");
        prev = w;
    }
    s2 = 0.5f * (SH4DER_TRAIL_SPEED_LO * SH4DER_TRAIL_SPEED_LO + SH4DER_TRAIL_SPEED_HI * SH4DER_TRAIL_SPEED_HI);
    if (!near(sh4der_trail_weight(s2), 0.5f * (SH4DER_TRAIL_WEIGHT_LO + SH4DER_TRAIL_WEIGHT_HI), 1e-4f))
        return fail("trail weight midpoint");
    return 0;
}

/* Copy offsets: even_px gives the nearest even pixel count, whole_px the nearest whole pixel. */
static int copy_offsets(void) {
    if (sh4der_taa_even_px(0.9f) != 0 || sh4der_taa_even_px(1.0f) != 2 || sh4der_taa_even_px(2.9f) != 2 || sh4der_taa_even_px(3.1f) != 4 ||
        sh4der_taa_even_px(-3.1f) != -4 || sh4der_taa_even_px(-0.4f) != 0 || sh4der_taa_even_px(7.9f) != 8)
        return fail("even px");
    if (sh4der_taa_whole_px(2.4f) != 2 || sh4der_taa_whole_px(2.5f) != 3 || sh4der_taa_whole_px(-2.6f) != -3 || sh4der_taa_whole_px(-2.4f) != -2 || sh4der_taa_whole_px(0.0f) != 0)
        return fail("whole px");
    return 0;
}

/* A still camera gives no layers, and a 0.5 rad turn in one frame is a cut with no blur. */
static int still_and_cut(void) {
    sh4der_trail_t t;
    sh4der_trail_frame_t o;
    int k;
    sh4der_trail_reset(&t);
    sh4der_trail_step(&t, 0.0f, 0.0f, FOCAL_FX, FOCAL_FX, &o);
    if (!o.cut || o.weight != 0.0f || o.copies != 0)
        return fail("first frame: cut, no layers");
    for (k = 0; k < 8; k++) {   /* still: no blur, no layers */
        sh4der_trail_step(&t, 0.0f, 0.0f, FOCAL_FX, FOCAL_FX, &o);
        if (o.cut || o.weight != 0.0f || o.copies != 0 || o.copy_w != 0.0f)
            return fail("still camera: no blur");
    }
    sh4der_trail_step(&t, 0.5f, 0.0f, FOCAL_FX, FOCAL_FX, &o);   /* 0.5 rad in a frame: a cut */
    if (!o.cut || o.weight != 0.0f || o.copies != 0)
        return fail("cut: no blur");
    return 0;
}

/* A fast pan: full weight, copies run to the motion with one sign, clamped, and the weights sum to the weight. */
static int fast_pan(void) {
    sh4der_trail_t t;
    sh4der_trail_frame_t o;
    float yaw = 0.0f, sum = 0.0f;
    int k;
    sh4der_trail_reset(&t);
    sh4der_trail_step(&t, yaw, 0.0f, FOCAL_FX, FOCAL_FX, &o);
    yaw += 0.02f;   /* about 9 px a frame: full weight */
    sh4der_trail_step(&t, yaw, 0.0f, FOCAL_FX, FOCAL_FX, &o);
    if (o.cut || o.copies != SH4DER_TRAIL_COPIES || !near(o.weight, SH4DER_TRAIL_WEIGHT_HI, 1e-6f))
        return fail("fast pan: full blur, copies");
    for (k = 0; k < o.copies; k++) {
        sum += o.copy_w;
        if (o.dx[k] & 1)
            return fail("copy x offset even");
        if (o.dy[k] != 0)
            return fail("pure yaw: no y offset");
        if (o.dx[k] > 0 || (k > 0 && o.dx[k] > o.dx[k - 1]))
            return fail("copies run to the motion, one sign");
        if (o.dx[k] < -SH4DER_TRAIL_SHIFT_MAX)
            return fail("copy clamp");
    }
    if (o.dx[0] != 0 || o.dx[SH4DER_TRAIL_COPIES - 1] != -SH4DER_TRAIL_SHIFT_MAX)
        return fail("copies: none to the full (clamped) motion");
    if (!near(sum, o.weight, 1e-5f))
        return fail("copy weights sum to the weight");
    return 0;
}

/* A slow pan: partial weight, and copy offsets 0, 0, -2. */
static int slow_pan(void) {
    sh4der_trail_t t;
    sh4der_trail_frame_t o;
    sh4der_trail_reset(&t);
    sh4der_trail_step(&t, 0.0f, 0.0f, FOCAL_FX, FOCAL_FX, &o);
    sh4der_trail_step(&t, 0.004f, 0.0f, FOCAL_FX, FOCAL_FX, &o);   /* about 1.8 px: above SPEED_LO, below SPEED_HI */
    if (o.cut || !(o.weight > 0.0f) || !(o.weight < SH4DER_TRAIL_WEIGHT_HI))
        return fail("slow pan: partial weight");
    if (o.dx[0] != 0 || o.dx[1] != 0 || o.dx[2] != -2)
        return fail("slow pan: copy offsets 0, 0, -2");
    return 0;
}

/* The TAA frame's copy offset after a 0.02 rad turn: even and whole, -10 px. */
static int taa_copy_px(void) {
    sh4der_taa_t t;
    sh4der_taa_frame_t o;
    sh4der_taa_init(&t);
    sh4der_taa_step(&t, 0.0f, 0.0f, FOCAL_FX, FOCAL_FX, &o);
    sh4der_taa_step(&t, 0.02f, 0.0f, FOCAL_FX, FOCAL_FX, &o);   /* shift about -9.1 px */
    if (o.cut || o.copy_px[0] != -10 || o.copy_px[1] != 0)
        return fail("taa copy offset even and whole");
    return 0;
}

static int keep_add_sum(void) {   /* the two layers' colours sum to 255 (one level): the still frame does not brighten or dim */
    int i;
    for (i = 0; i <= 100; i++) {
        const float w = (float)i * 0.01f;
        const int sum = (int)(sh4der_taa_keep_argb(w) & 0xffu) + (int)(sh4der_taa_add_argb(w) & 0xffu);
        if (sum < 254 || sum > 256)
            return fail("keep + add sum to one");
    }
    return 0;
}

/* The history blend on RGB565, one channel, the still colour: out = Q(keep x c + add x E(Q(out))), 8-bit products rounded.
   expand: 0 = bit replication (5 -> 8: x << 3 | x >> 2), 1 = shift only. quant: 0 = truncation to 565, 1 = round to nearest. */
/* Expand a 5 or 6 bit level to 8 bits: replicate the top bits, or shift only. */
static int expand8(int q, int bits, int variant) {
    if (variant == 0)
        return bits == 5 ? (q << 3) | (q >> 2) : (q << 2) | (q >> 4);
    return bits == 5 ? q << 3 : q << 2;
}

/* Round an 8-bit value back to a level: truncate, or round to nearest. */
static int quant_back(int v8, int bits, int mode) {
    const int maxq = (1 << bits) - 1;
    return mode == 0 ? v8 >> (8 - bits) : (v8 * maxq + 127) / 255;
}

/* An 8-bit product: a times b over 255, rounded. */
static int mul255(int a, int b) {
    return (a * b + 127) / 255;
}

/* Iterate the blend for 900 frames from every level, and print the largest drift and how many levels move. */
static void drift_report(float w, int bits, int variant, int mode) {
    const int keep = (int)(sh4der_taa_keep_argb(w) & 0xffu), add = (int)(sh4der_taa_add_argb(w) & 0xffu);
    const int levels = 1 << bits, frames = 900;   /* 30 s at 30 fps */
    int q0, maxd = 0, drifted = 0, up = 0, down = 0;
    for (q0 = 0; q0 < levels; q0++) {
        const int c8 = expand8(q0, bits, variant);
        int q = q0, f;
        for (f = 0; f < frames; f++) {
            const int out8 = mul255(c8, keep) + mul255(expand8(q, bits, variant), add);
            q = quant_back(out8 > 255 ? 255 : out8, bits, mode);
        }
        {
            const int d = q - q0, ad = d < 0 ? -d : d;
            if (ad > maxd) maxd = ad;
            if (ad >= 1) drifted++;
            if (d > 0) up++;
            if (d < 0) down++;
        }
    }
    printf("drift565 w %.2f %s expand %s quant %s: max %d level(s) after 900 frames, %d of %d levels move (%d up, %d down)\n",
           (double)w, bits == 5 ? "R5/B5" : "G6", variant == 0 ? "replicate" : "shift", mode == 0 ? "truncate" : "round", maxd, drifted, levels, up, down);
}

/* Print the drift report for both weights, both channel widths, and both expand and quant modes. */
static void drift_all(void) {
    const float ws[2] = { 0.75f, 0.5f };
    int i, variant, mode;
    for (i = 0; i < 2; i++)
        for (variant = 0; variant < 2; variant++)
            for (mode = 0; mode < 2; mode++) {
                drift_report(ws[i], 5, variant, mode);
                drift_report(ws[i], 6, variant, mode);
            }
}

int main(void) {
    if (weight_checks() || copy_offsets() || still_and_cut() || fast_pan() || slow_pan() || taa_copy_px() || keep_add_sum())
        return 1;
    drift_all();
    printf("trail_test: ok\n");
    return 0;
}
