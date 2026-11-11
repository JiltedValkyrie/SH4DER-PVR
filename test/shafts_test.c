/* SH4DER-PVR

   test/shafts_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the shadow shafts' maths: the turn toward the light, depth
   and its cap, what casts, the near-light weight and the copy's scale. Also
   the shade map built from a read (a dark cell near the light shades; a
   bright or far one does not).
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_shafts.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0001f
#endif

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int near(float a, float b, float tol) { return fabsf(a - b) <= tol; }

/* The terms: turn toward the light, depth and its cap, cast, near-light weight and copy scale. */
static int terms(void) {
    if (sh4der_shafts_toward(0.4f) != 0.0f || sh4der_shafts_toward(0.5f) != 0.0f || !near(sh4der_shafts_toward(0.95f), 1.0f, 1e-5f) || sh4der_shafts_toward(1.0f) != 1.0f)
        return fail("toward the light");
    if (!(sh4der_shafts_toward(0.7f) > 0.2f && sh4der_shafts_toward(0.7f) < 0.8f))
        return fail("toward, between");
    if (!near(sh4der_shafts_depth(1.0f, 1.0f), SH4DER_SHAFTS_DEPTH, 1e-6f) || !near(sh4der_shafts_depth(10.0f, 1.0f), SH4DER_SHAFTS_DEPTH_MAX, 1e-6f))
        return fail("depth and its cap");
    if (sh4der_shafts_depth(1.0f, 0.0f) != 0.0f || sh4der_shafts_depth(-1.0f, 1.0f) != 0.0f)
        return fail("no turn or no strength: no depth");
    if (sh4der_shafts_cast(0.5f) != 0.0f || sh4der_shafts_cast(0.30f) != 0.0f || !near(sh4der_shafts_cast(0.10f), 1.0f, 1e-5f) || sh4der_shafts_cast(0.0f) != 1.0f)
        return fail("what casts");
    if (!near(sh4der_shafts_cast(0.20f), 0.5f, 1e-4f))
        return fail("cast, half way");
    if (!near(sh4der_shafts_near(0.5f, 0.5f, 0.5f, 0.5f), 1.0f, SH4DER_TEST_TOL) || !near(sh4der_shafts_near(0.5f, 0.6f, 0.5f, 0.5f), 1.0f, SH4DER_TEST_TOL))
        return fail("near the light: full");
    if (!near(sh4der_shafts_near(0.5f, 1.5f, 0.5f, 0.5f), 0.0f, SH4DER_TEST_TOL))
        return fail("far from the light: none");
    if (!near(sh4der_shafts_scale(1.0f), 1.0f - SH4DER_SHAFTS_PUSH, 1e-6f) || sh4der_shafts_scale(0.0f) != 1.0f || !(sh4der_shafts_scale(100.0f) > 0.5f))
        return fail("the copy's scale");
    return 0;
}

/* The shade map: a texel eases half way per step, then dark cells near the light shade and bright or far ones do not. */
static int map_from_read(void) {
    static uint16_t grid[SH4DER_GLOW_GRID_X * SH4DER_GLOW_GRID_Y], map[SH4DER_GLOW_TEX_W * SH4DER_GLOW_TEX_H];
    unsigned i, g;
    /* a texel eases half way: white (63) toward keep 0 */
    g = ((unsigned)sh4der_shafts_texel(0xffffu, 0.0f) >> 5) & 63u;
    if (g != 32u)
        return fail("half way a step");
    g = ((unsigned)sh4der_shafts_texel(0xffffu, 1.0f) >> 5) & 63u;
    if (g != 63u)
        return fail("white stays white");
    for (i = 0; i < SH4DER_GLOW_GRID_X * SH4DER_GLOW_GRID_Y; i++) grid[i] = 0xffffu;   /* a bright frame */
    grid[30u * SH4DER_GLOW_GRID_X + 8u] = 0x0000u;    /* one black cell at the light (the screen's left, mid height) */
    grid[30u * SH4DER_GLOW_GRID_X + 78u] = 0x0000u;   /* one black cell far from it: the other side, 1.17 screen heights away */
    sh4der_shafts_clear(map, SH4DER_GLOW_TEX_W, SH4DER_GLOW_TEX_H);
    for (i = 0; i < 12u; i++)   /* a dozen frames: the ease has landed */
        sh4der_shafts_map(map, SH4DER_GLOW_TEX_W, grid, 0.1f, 0.5f, sh4der_shafts_depth(1.0f, 1.0f));
    g = ((unsigned)map[30u * SH4DER_GLOW_TEX_W + 8u] >> 5) & 63u;
    if (g < 33u || g > 37u)   /* 1 - 0.45 = 0.55 of 63 = 34.65 */
        return fail("the dark cell at the light shades to 1 - DEPTH");
    if (map[30u * SH4DER_GLOW_TEX_W + 78u] != 0xffffu)
        return fail("a dark cell far from the light does not shade");
    if (map[10u * SH4DER_GLOW_TEX_W + 10u] != 0xffffu)
        return fail("a bright cell does not shade");
    if (map[5u * SH4DER_GLOW_TEX_W + 100u] != 0xffffu)
        return fail("texels past the grid are left alone");
    sh4der_shafts_map(NULL, SH4DER_GLOW_TEX_W, grid, 0.5f, 0.5f, 0.45f);   /* no map, a short stride: nothing */
    sh4der_shafts_map(map, 8u, grid, 0.5f, 0.5f, 0.45f);
    if (map[30u * SH4DER_GLOW_TEX_W + 78u] != 0xffffu)
        return fail("a short stride writes nothing");
    return 0;
}

int main(void) {
    if (terms() || map_from_read())
        return 1;
    printf("shafts_test: ok\n");
    return 0;
}
