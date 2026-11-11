/* SH4DER-PVR

   test/ao_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the small-scale AO maths: the edge's concave amount, the crease
   factor, the vertex factor on a corner mesh, the screen cavity on synthetic luma
   grids, and the texel map. Prints the numbers and an ASCII patch of the ledge case.
*/

#include <stdio.h>
#include <string.h>
#include "../include/sh4der_ao.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0002f
#endif

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static int near(float a, float b, float tol) { return (a > b ? a - b : b - a) <= tol; }

/* --- crease ---------------------------------------------------------------- */

/* Checks the edge's concave amount (the sine of the rise angle) and the crease factor's clamp and midpoint. */
static int crease(void) {
    const float up[3] = { 0.0f, 1.0f, 0.0f };
    const float p[3] = { 0.0f, 0.0f, 0.0f };
    const float e[3] = { 0.0f, 0.0f, 1.0f };   /* the edge along z */
    const float wall[3] = { 0.0f, 3.0f, 4.0f };   /* a wall's far vertex: rises from the edge */
    const float flat[3] = { 4.0f, 0.0f, 0.0f };   /* a floor's far vertex: on the plane */
    const float down[3] = { 0.0f, -3.0f, 0.0f };  /* a convex edge: the other face drops away */
    float c;
    if (!near(sh4der_ao_edge_concave(up, p, e, wall), 1.0f, SH4DER_TEST_TOL)) return fail("right-angle wall is fully concave");
    if (sh4der_ao_edge_concave(up, p, e, flat) != 0.0f) return fail("flat is 0");
    if (sh4der_ao_edge_concave(up, p, e, down) != 0.0f) return fail("convex is 0");
    if (sh4der_ao_edge_concave(up, p, e, p) != 0.0f) return fail("vertex on the edge is 0");
    {   /* a 45 degree face: sin 45 */
        const float diag[3] = { 1.0f, 1.0f, 0.0f };   /* rises 45 degrees out of the floor, across x */
        c = sh4der_ao_edge_concave(up, p, e, diag);
        if (!near(c, 0.70710678f, 0.001f)) return fail("45 degree concave is sin 45");
    }
    if (!near(sh4der_ao_crease_factor(0.0f), 1.0f, SH4DER_TEST_TOL)) return fail("no edge: factor 1");
    if (!near(sh4der_ao_crease_factor(2.0f), 1.0f - SH4DER_AO_CREASE_K, SH4DER_TEST_TOL)) return fail("full sum: 1 - K");
    if (!near(sh4der_ao_crease_factor(9.0f), 1.0f - SH4DER_AO_CREASE_K, SH4DER_TEST_TOL)) return fail("factor clamps");
    if (!near(sh4der_ao_crease_factor(1.0f), 1.0f - 0.5f * SH4DER_AO_CREASE_K, SH4DER_TEST_TOL)) return fail("half sum: half");
    return 0;
}

/* --- corner mesh: floor y = 0 (normal +y), wall x = 0 (normal +x) rising to y = 3 ---- */

typedef struct { float p[3]; } vtx_t;

/* Checks the vertex factors on a two-face corner: only the two vertices of the shared edge darken. */
static int corner_mesh(void) {
    /* A (0,0,0)  B (4,0,0)  C (4,0,4)  D (0,0,4)  E (0,3,4)  F (0,3,0). Floor (A,B,C) (A,C,D); wall (A,D,E) (A,E,F). */
    const vtx_t v[6] = { { { 0, 0, 0 } }, { { 4, 0, 0 } }, { { 4, 0, 4 } }, { { 0, 0, 4 } }, { { 0, 3, 4 } }, { { 0, 3, 0 } } };
    const float floor_n[3] = { 0, 1, 0 };   /* floor normal (the wall's is +x) */
    /* The only interior edge is A-D (floor and wall meet). Its concave amount comes from the floor side (n1 = +y, face 2 = wall, opposite E). */
    const float e_ad[3] = { 0.0f, 0.0f, 1.0f };
    float sum[6] = { 0 };
    float factor[6];
    int i;
    const float c_ad = sh4der_ao_edge_concave(floor_n, v[0].p, e_ad, v[4].p);   /* floor n1, wall's E */
    sum[0] += c_ad;   /* A */
    sum[3] += c_ad;   /* D */
    for (i = 0; i < 6; i++) factor[i] = sh4der_ao_crease_factor(sum[i]);
    if (!near(factor[0], 1.0f - SH4DER_AO_CREASE_K * 0.5f, SH4DER_TEST_TOL)) return fail("corner vertex A factor");
    if (!near(factor[3], 1.0f - SH4DER_AO_CREASE_K * 0.5f, SH4DER_TEST_TOL)) return fail("corner vertex D factor");
    if (factor[1] != 1.0f || factor[2] != 1.0f) return fail("far floor corners untouched");
    if (factor[4] != 1.0f || factor[5] != 1.0f) return fail("wall top untouched by the floor's edge");
    printf("ao crease: corner A/D factor %.3f, far floor B/C %.3f, wall top E/F %.3f\n", factor[0], factor[1], factor[4]);
    return 0;
}

/* --- screen cavity ------------------------------------------------------------ */

#define W SH4DER_AO_SCR_GRID_X
#define H SH4DER_AO_SCR_GRID_Y

static uint8_t luma_buf[W * H];
static float mul_buf[W * H];

/* Prints one summary line for mul_buf: the texels below 0.9, and the darkest value. */
static void stats(const char *name) {
    unsigned n = 0, dark = 0, i;
    float lo = 1.0f;
    for (i = 0; i < (unsigned)(W * H); i++) {
        if (mul_buf[i] < lo) lo = mul_buf[i];
        if (mul_buf[i] < 0.9f) dark++;
        n++;
    }
    printf("ao screen %-16s texels darkened (below 0.9) %5u of %u (%.1f %%), darkest %.3f\n", name, dark, n, 100.0f * dark / n, lo);
}

/* Checks the cavity on five synthetic scenes (dark ledge, bright wall, flat step, noise, flat grey) and the texel map. */
static int screen(void) {
    unsigned x, y, i;
    uint32_t s = 12345u;
    /* Scene 1: a dark ledge over a bright floor. Rows 0..29 luma 40, rows 30.. luma 200. */
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) luma_buf[y * W + x] = y < 30 ? 40 : 200;
    sh4der_ao_grid(luma_buf, W, H, mul_buf);
    stats("ledge");
    if (!(mul_buf[30 * W + 10] < 0.8f)) return fail("ledge: the bright texel under the dark edge is darkened");
    if (mul_buf[31 * W + 10] != 1.0f) return fail("ledge: the bright floor one row further is untouched");
    if (mul_buf[29 * W + 10] != 1.0f) return fail("ledge: the dark side is untouched");
    printf("  ledge: texel row 30 factor %.3f (the bright side of the edge), row 31 %.3f\n", mul_buf[30 * W + 10], mul_buf[31 * W + 10]);
    printf("  ledge patch (rows 27..33, cols 8..13, multiply as '#' <0.7, '+' <0.9, '.' 1.0):\n");
    for (y = 27; y < 34; y++) {
        printf("    ");
        for (x = 8; x < 14; x++) { const float m = mul_buf[y * W + x]; putchar(m < 0.7f ? '#' : m < 0.9f ? '+' : '.'); }
        putchar('\n');
    }
    /* Scene 2: a bright wall over a dark floor, the same edge the other way round: nothing is darkened (the cue is silent here). */
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) luma_buf[y * W + x] = y < 30 ? 200 : 40;
    sh4der_ao_grid(luma_buf, W, H, mul_buf);
    stats("wall over floor");
    if (mul_buf[30 * W + 10] != 1.0f) return fail("bright wall over dark floor: no darkening");
    /* Scene 3: a corner with a real crease but a flat luma step (wall 150, floor 120): the cue sees a step, not a crease. */
    for (y = 0; y < H; y++) for (x = 0; x < W; x++) luma_buf[y * W + x] = y < 30 ? 150 : 120;
    sh4der_ao_grid(luma_buf, W, H, mul_buf);
    stats("corner step");
    /* Scene 4: noise, luma 100 +- 40 on a plain grey texture: what a textured wall does to the cue. */
    for (i = 0; i < (unsigned)(W * H); i++) {
        s = s * 1664525u + 1013904223u;
        luma_buf[i] = (uint8_t)(60u + ((s >> 16) % 81u));
    }
    sh4der_ao_grid(luma_buf, W, H, mul_buf);
    stats("noise 60..140");
    /* Scene 5: a flat wall, nothing: the factor must be 1 everywhere (no noise at rest). */
    for (i = 0; i < (unsigned)(W * H); i++) luma_buf[i] = 128;
    sh4der_ao_grid(luma_buf, W, H, mul_buf);
    stats("flat grey");
    for (i = 0; i < (unsigned)(W * H); i++) if (mul_buf[i] != 1.0f) return fail("flat grey: no darkening");
    /* The texel map */
    if (sh4der_ao_texel565(0.0f) != 0x0000u) return fail("texel 0");
    if (sh4der_ao_texel565(1.0f) != 0xffffu) return fail("texel 1");
    if (sh4der_ao_texel565(2.0f) != 0xffffu) return fail("texel clamps high");
    return 0;
}

int main(void) {
    if (crease()) return 1;
    if (corner_mesh()) return 1;
    if (screen()) return 1;
    printf("ao_test: ok\n");
    return 0;
}
