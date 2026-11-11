/* SH4DER-PVR

   test/ca_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the chromatic aberration: the two frame texture sizes and their
   video memory, the shortfall in KB, no shift in the centre, the split in the corner
   (green half of red, blue unmoved), samples kept inside the picture, and texel-centre
   texture coordinates.
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_ca.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0001f
#endif

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int near(float a, float b, float tol) { return fabsf(a - b) <= tol; }

/* Checks the two frame sizes and texture sizes, the video memory in bytes, and the shortfall in KB. */
static int sizes(void) {
    sh4der_ca_size_t full, half;
    sh4der_ca_size(0, &full);
    sh4der_ca_size(1, &half);
    if (full.w != 640u || full.h != 480u || full.tex_w != 1024u || full.tex_h != 512u || full.scale != 1.0f)
        return fail("full size");
    if (half.w != 320u || half.h != 240u || half.tex_w != 512u || half.tex_h != 256u || half.scale != 0.5f)
        return fail("half size");
    if (sh4der_ca_bytes(&full) != 640u * 1024u || sh4der_ca_bytes(&half) != 160u * 1024u)
        return fail("640 KB and 160 KB");
    if (sh4der_ca_short_kb(655360u, 655360u) != 0u || sh4der_ca_short_kb(655360u, 700000u) != 0u)
        return fail("it fits: nothing short");
    if (sh4der_ca_short_kb(655360u, 655359u) != 1u || sh4der_ca_short_kb(655360u, 0u) != 640u || sh4der_ca_short_kb(1310720u, 102400u) != 1180u)
        return fail("the shortfall in KB, rounded up");
    return 0;
}

/* Checks the split at several strengths: no shift at the centre, the offset in the corner, green half of red,
   blue fixed, and no push without a strength or a screen. */
static int split(void) {
    const float w = 640.0f, h = 480.0f, inv = 4.0f / (w * w + h * h);
    float strength;
    for (strength = 0.25f; strength <= 4.0f; strength += 0.75f) {
        float pu[3], pv[3], cx[3], cy[3];
        int c;
        for (c = 0; c < 3; c++) {
            const float k = sh4der_ca_push(c, strength, w, h);
            /* the centre: nothing moves */
            sh4der_ca_sample(k, sh4der_ca_r2(0.5f, 0.5f, w, h, inv), 0.5f, 0.5f, &pu[c], &pv[c]);
            if (!near(pu[c], 0.5f, 1e-6f) || !near(pv[c], 0.5f, 1e-6f))
                return fail("no shift in the centre");
            /* the corner: r2 is 1 */
            if (!near(sh4der_ca_r2(1.0f, 1.0f, w, h, inv), 1.0f, SH4DER_TEST_TOL))
                return fail("r2 is 1 in the corner");
            sh4der_ca_sample(k, 1.0f, 1.0f, 1.0f, &pu[c], &pv[c]);
            cx[c] = pu[c] * w;
            cy[c] = pv[c] * h;
            if (pu[c] > 1.0f || pv[c] > 1.0f || pu[c] < 0.5f || pv[c] < 0.5f)
                return fail("the sample stays inside the picture");
        }
        {   /* red against blue in the corner: SPLIT_PX x strength pixels along the diagonal; green half way */
            const float rb = sqrtf((cx[2] - cx[0]) * (cx[2] - cx[0]) + (cy[2] - cy[0]) * (cy[2] - cy[0]));
            const float gb = sqrtf((cx[2] - cx[1]) * (cx[2] - cx[1]) + (cy[2] - cy[1]) * (cy[2] - cy[1]));
            if (!near(rb, SH4DER_CA_SPLIT_PX * strength, 0.01f))
                return fail("the shipped split in the corner");
            if (!near(gb, 0.5f * rb, 0.01f))
                return fail("green is half of red");
            if (!near(cx[2], w, 1e-3f) || !near(cy[2], h, 1e-3f))
                return fail("blue is not moved");
        }
    }
    if (sh4der_ca_push(SH4DER_CA_RED, 0.0f, w, h) != 0.0f || sh4der_ca_push(SH4DER_CA_RED, -1.0f, w, h) != 0.0f || sh4der_ca_push(SH4DER_CA_RED, 1.0f, 0.0f, 0.0f) != 0.0f)
        return fail("no strength or no screen: no push");
    return 0;
}

/* Checks the texel-centre coordinates at both ends, clamping past the picture, and the one-channel ARGB values. */
static int coords(void) {
    sh4der_ca_size_t s;
    float u, v;
    int half;
    for (half = 0; half < 2; half++) {
        sh4der_ca_size(half, &s);
        sh4der_ca_uv(&s, 0.0f, 0.0f, &u, &v);
        if (!near(u * (float)s.tex_w, 0.5f, 1e-3f) || !near(v * (float)s.tex_h, 0.5f, 1e-3f))
            return fail("the first texel's centre");
        sh4der_ca_uv(&s, 1.0f, 1.0f, &u, &v);
        if (!near(u * (float)s.tex_w, (float)s.w - 0.5f, 1e-2f) || !near(v * (float)s.tex_h, (float)s.h - 0.5f, 1e-2f))
            return fail("the last texel's centre");
        sh4der_ca_uv(&s, 7.0f, -3.0f, &u, &v);   /* past the picture: clamped to it */
        if (u * (float)s.tex_w > (float)s.w || v < 0.0f)
            return fail("clamped to the picture");
    }
    if (sh4der_ca_argb(SH4DER_CA_RED) != 0xffff0000u || sh4der_ca_argb(SH4DER_CA_GREEN) != 0xff00ff00u || sh4der_ca_argb(SH4DER_CA_BLUE) != 0xff0000ffu)
        return fail("one channel each");
    return 0;
}

int main(void) {
    if (sizes() || split() || coords())
        return 1;
    printf("ca_test: ok\n");
    return 0;
}
