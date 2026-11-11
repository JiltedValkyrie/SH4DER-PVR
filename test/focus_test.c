/* SH4DER-PVR

   test/focus_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the fake depth of field: the sharp mask (disc, band, rect),
   the mesh (count, strip ends, alpha = blur), the /9 of the box blur
   (exact on every sum), the blur itself (flat stays flat, an impulse spreads
   to 1/9 per tap, edge clamp, padding).
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_focus.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0002f
#endif

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int mask_shapes(void) {
    sh4der_focus_mask_t m;
    sh4der_focus_mask_default(&m, SH4DER_FOCUS_DISC);
    if (fabsf(sh4der_focus_sharp(&m, m.cx, m.cy) - 1.0f) > SH4DER_TEST_TOL) return fail("disc centre sharp");
    if (sh4der_focus_sharp(&m, m.cx + SH4DER_FOCUS_DISC_R + SH4DER_FOCUS_FEATHER + 1.0f, m.cy) != 0.0f) return fail("disc far blurred");
    if (!(sh4der_focus_sharp(&m, m.cx + SH4DER_FOCUS_DISC_R + 0.5f * SH4DER_FOCUS_FEATHER, m.cy) > 0.0f &&
          sh4der_focus_sharp(&m, m.cx + SH4DER_FOCUS_DISC_R + 0.5f * SH4DER_FOCUS_FEATHER, m.cy) < 1.0f)) return fail("disc falloff in between");

    sh4der_focus_mask_default(&m, SH4DER_FOCUS_BAND);
    if (fabsf(sh4der_focus_sharp(&m, 10.0f, m.cy) - 1.0f) > SH4DER_TEST_TOL) return fail("band sharp across");
    if (sh4der_focus_sharp(&m, 10.0f, m.cy + SH4DER_FOCUS_BAND_H + SH4DER_FOCUS_FEATHER + 1.0f) != 0.0f) return fail("band top blurred");
    if (sh4der_focus_sharp(&m, 10.0f, m.cy - SH4DER_FOCUS_BAND_H - SH4DER_FOCUS_FEATHER - 1.0f) != 0.0f) return fail("band bottom blurred");

    sh4der_focus_mask_default(&m, SH4DER_FOCUS_RECT);
    if (fabsf(sh4der_focus_sharp(&m, m.cx + 5.0f, m.cy + 5.0f) - 1.0f) > SH4DER_TEST_TOL) return fail("rect inside");
    if (sh4der_focus_sharp(&m, m.cx + SH4DER_FOCUS_RECT_W + SH4DER_FOCUS_FEATHER + 1.0f, m.cy) != 0.0f) return fail("rect side blurred");
    if (sh4der_focus_sharp(&m, m.cx, m.cy + SH4DER_FOCUS_RECT_H + SH4DER_FOCUS_FEATHER + 1.0f) != 0.0f) return fail("rect bottom blurred");
    return 0;
}

static int mesh(void) {
    static sh4der_focus_vtx_t v[SH4DER_FOCUS_VERTS];
    sh4der_focus_mask_t m;
    unsigned n, i, eols = 0;
    sh4der_focus_mask_default(&m, SH4DER_FOCUS_DISC);
    n = sh4der_focus_mesh(&m, v);
    if (n != SH4DER_FOCUS_VERTS || SH4DER_FOCUS_VERTS != 408 || SH4DER_FOCUS_TRIS != 384) return fail("mesh count");
    for (i = 0; i < n; i++) {
        const uint32_t a = v[i].argb >> 24;
        if (v[i].eol) eols++;
        if (v[i].x < 0.0f || v[i].x > SH4DER_FOCUS_SCREEN_W || v[i].y < 0.0f || v[i].y > SH4DER_FOCUS_SCREEN_H) return fail("mesh inside screen");
        if (v[i].u < 0.0f || v[i].u > sh4der_focus_uv_max_u() + 1e-6f || v[i].v < 0.0f || v[i].v > sh4der_focus_uv_max_v() + 1e-6f) return fail("mesh uv");
        if (a > 255u || (v[i].argb & 0xffffffu) != 0xffffffu) return fail("mesh colour");
    }
    if (eols != SH4DER_FOCUS_GRID_Y) return fail("one eol per strip");
    /* the vertex nearest the centre is sharp (alpha 0), a corner is full blur (alpha 255) */
    if ((v[0].argb >> 24) != 255u) return fail("corner blurred");
    for (i = 0; i < n; i++) {
        if (fabsf(v[i].x - m.cx) < 1.0f && fabsf(v[i].y - m.cy) < 1.0f && (v[i].argb >> 24) != 0u) return fail("centre sharp");
    }
    return 0;
}

static int division(void) {
    unsigned t;
    for (t = 0; t <= 567u; t++)
        if (sh4der_focus_div9_(t) != (t + 4u) / 9u) return fail("div9 exact");
    return 0;
}

static int blur(void) {
    static uint16_t src[SH4DER_FOCUS_SRC_W * SH4DER_FOCUS_SRC_H];
    static uint16_t dst[SH4DER_FOCUS_TEX_W * SH4DER_FOCUS_TEX_H];
    const uint16_t grey = (uint16_t)((16u << 11) | (32u << 5) | 16u);
    unsigned i;
    for (i = 0; i < SH4DER_FOCUS_SRC_W * SH4DER_FOCUS_SRC_H; i++) src[i] = grey;
    sh4der_focus_box565(src, dst);
    for (i = 0; i < SH4DER_FOCUS_TEX_W * SH4DER_FOCUS_TEX_H; i++)
        if (dst[i] != grey && (i % SH4DER_FOCUS_TEX_W) < SH4DER_FOCUS_SRC_W && (i / SH4DER_FOCUS_TEX_W) < SH4DER_FOCUS_SRC_H)
            return fail("flat stays flat");
    /* an impulse of full white at (40, 30): a 3x3 of 1/9 each, the rest of the grid unchanged */
    for (i = 0; i < SH4DER_FOCUS_SRC_W * SH4DER_FOCUS_SRC_H; i++) src[i] = 0;
    src[30 * SH4DER_FOCUS_SRC_W + 40] = 0xffffu;
    sh4der_focus_box565(src, dst);
    {
        const unsigned r9 = dst[30 * SH4DER_FOCUS_TEX_W + 40] >> 11;   /* 31 / 9 = 3 */
        const unsigned g9 = (dst[30 * SH4DER_FOCUS_TEX_W + 40] >> 5) & 63u;   /* 63 / 9 = 7 */
        if (r9 != 3u || g9 != 7u) return fail("impulse 1/9");
        if ((dst[29 * SH4DER_FOCUS_TEX_W + 39] >> 11) != 3u) return fail("impulse spreads to the diagonal");
        if ((dst[28 * SH4DER_FOCUS_TEX_W + 40] >> 11) != 0u) return fail("impulse stays inside 3x3");
    }
    /* edge clamp: a white corner at (0, 0). The clamped left tap repeats it: row 0 sums 31 + 31 + 0 = 62 and row 1 is 0, so the
       output is (62 + 62 + 0 + 4) / 9 = 14 in red */
    for (i = 0; i < SH4DER_FOCUS_SRC_W * SH4DER_FOCUS_SRC_H; i++) src[i] = 0;
    src[0] = 0xffffu;
    sh4der_focus_box565(src, dst);
    if ((dst[0] >> 11) != 14u)
        return fail("edge clamp");
    /* padding: bilinear reads of the edge column and row are the edge */
    if (dst[SH4DER_FOCUS_SRC_W] != dst[SH4DER_FOCUS_SRC_W - 1]) return fail("pad column");
    if (dst[SH4DER_FOCUS_SRC_H * SH4DER_FOCUS_TEX_W] != dst[(SH4DER_FOCUS_SRC_H - 1) * SH4DER_FOCUS_TEX_W]) return fail("pad row");
    if (SH4DER_FOCUS_TEX_BYTES != 16384) return fail("texture bytes");
    return 0;
}

int main(void) {
    if (mask_shapes()) return 1;
    if (mesh()) return 1;
    if (division()) return 1;
    if (blur()) return 1;
    printf("focus_test: ok\n");
    return 0;
}
