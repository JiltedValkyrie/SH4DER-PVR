/* SH4DER-PVR

   test/torch_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the flashlight hotspot: cone radii, boost and carve profiles,
   the near add, the fog bend, the darkness gate (hysteresis, ramp times), the
   power ramp at 30 Hz, and both meshes (strip layout, eol, edge colours,
   corner coverage).
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_torch.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0002f
#endif

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

/* Cone radii: tan of the cone angle from cos squared, the 320 px edge, and a squeezed core. */
static int radii(void) {
    float rx, ry;
    if (fabsf(sh4der_torch_tan_of_cos2(0.75f) - 1.0f / sqrtf(3.0f)) > 1e-4f)
        return fail("tan of 30 degrees");
    if (fabsf(sh4der_torch_tan_of_cos2(0.941474f) - tanf(14.0f * 3.14159265f / 180.0f)) > 2e-4f)
        return fail("tan of 14 degrees");
    if (sh4der_torch_tan_of_cos2(1.0f) != 0.0f)
        return fail("tan of 0");
    /* the reference: focal 320 / tan 30 degrees (a 30 degree half field), the edge at 320 px */
    sh4der_torch_radii(320.0f / tanf(0.5235988f), 320.0f / tanf(0.5235988f), 1.0f / sqrtf(3.0f), &rx, &ry);
    if (fabsf(rx - 320.0f) > 0.1f || fabsf(ry - 320.0f) > 0.1f)
        return fail("edge radius 320 px");
    sh4der_torch_radii(240.0f, 320.0f, 0.2493f, &rx, &ry);   /* a squeeze: x 0.75 of y, the core at 14 degrees */
    if (fabsf(rx - 240.0f * 0.2493f) > 0.05f || fabsf(ry - 320.0f * 0.2493f) > 0.05f)
        return fail("core radius and squeeze");
    return 0;
}

/* The boost and carve profiles over the radius, and no carve in daylight. */
static int profiles(void) {
    float u, prev = 2.0f;
    if (fabsf(sh4der_torch_boost_at(0.0f, 1.0f) - 1.0f) > 1e-6f)
        return fail("boost centre is 1 (BOOST + HOT at full)");
    if (sh4der_torch_boost_at(1.0f, 1.0f) != 0.0f || sh4der_torch_boost_at(1.2f, 1.0f) != 0.0f)
        return fail("boost is 0 from the edge");
    if (fabsf(sh4der_torch_boost_at(0.5f, 0.5f) - 0.5f * SH4DER_TORCH_BOOST * 0.5625f) > SH4DER_TEST_TOL)
        return fail("boost at half power, u 0.5");
    for (u = 0.0f; u <= 1.0f; u += 0.05f) {
        const float b = sh4der_torch_boost_at(u, 1.0f);
        if (b > prev + 1e-6f)
            return fail("boost falls with radius");
        prev = b;
    }
    if (sh4der_torch_carve_at(0.5f, 1.0f) != 1.0f || sh4der_torch_carve_at(0.95f, 1.0f) < 0.67f || sh4der_torch_carve_at(0.95f, 1.0f) > 0.68f)
        return fail("carve: inside 1, feather at 0.95 half way");
    if (fabsf(sh4der_torch_carve_at(1.0f, 1.0f) - SH4DER_TORCH_MUL_OUT) > 1e-6f || fabsf(sh4der_torch_carve_at(1.6f, 1.0f) - SH4DER_TORCH_MUL_OUT) > 1e-6f)
        return fail("carve outside is MUL_OUT at full darkness");
    if (sh4der_torch_carve_at(1.6f, 0.0f) != 1.0f)
        return fail("daylight: no carve");
    return 0;
}

/* The near add (distance form and near_at), the fog table layout, and the fog bend. */
static int near_and_fog(void) {
    const float r2 = SH4DER_TORCH_NEAR_WU * SH4DER_TORCH_NEAR_WU;
    unsigned i;
    if (fabsf(sh4der_torch_near(0.0f, 1.0f) - SH4DER_TORCH_NEAR_K) > 1e-6f || sh4der_torch_near(r2, 1.0f) != 0.0f || sh4der_torch_near(r2 * 2.0f, 1.0f) != 0.0f)
        return fail("near: distance only, 0 past the reach");
    if (fabsf(sh4der_torch_near(r2 * 0.25f, 1.0f) - SH4DER_TORCH_NEAR_K * 0.75f) > 1e-5f || sh4der_torch_near(0.0f, 0.0f) != 0.0f)
        return fail("near: linear, gated");
    {   /* near = near_at with the library's own strength and reach; near_at takes its own, linear, 0 past its reach */
        const float inv = 1.0f / r2;
        unsigned j;
        for (j = 0; j <= 20; j++) {
            const float d2 = r2 * (float)j * 0.05f;
            if (fabsf(sh4der_torch_near(d2, 0.7f) - sh4der_torch_near_at(d2, 0.7f, SH4DER_TORCH_NEAR_K, inv)) > 1e-6f)
                return fail("near: equals near_at with the shipped strength and reach");
        }
        if (fabsf(sh4der_torch_near_at(r2 * 0.5f, 0.5f, 0.2f, inv) - 0.2f * 0.5f * 0.5f) > 1e-6f)
            return fail("near_at: near_k x k x (1 - d2 / r2)");
        if (sh4der_torch_near_at(r2 * 0.5f, 1.0f, 0.2f, inv) <= 0.0f || sh4der_torch_near_at(r2, 1.0f, 0.2f, inv) != 0.0f)
            return fail("near_at: 0 at and past its reach");
        if (sh4der_torch_near_at(0.0f, 0.0f, 0.2f, inv) != 0.0f || sh4der_torch_near_at(0.0f, -1.0f, 0.2f, inv) != 0.0f)
            return fail("near_at: 0 when k <= 0");
        if (fabsf(sh4der_torch_near_at(4.0f, 1.0f, 1.0f, 0.25f) - 0.0f) > 1e-6f)
            return fail("near_at: 0 at the reach, d2 x inv_r2 = 1");
    }
    /* the fog table entries: entry 0 is the far depth, entry 128 the far depth / 256 (the atmofog layout) */
    if (fabsf(sh4der_torch_entry_depth(0, 100.0f) - 100.0f) > 1e-4f || fabsf(sh4der_torch_entry_depth(128, 100.0f) - 100.0f / 256.0f) > 1e-4f)
        return fail("entry depth layout");
    if (fabsf(sh4der_torch_fog_bend(0.2f, 1.0f, 0.5f) - 0.2f) > 1e-6f)
        return fail("fog: nearer than FOG_NEAR untouched");
    {   /* black fog: the bent frame equals the frame times the table's (1 - T) times (1 - R) */
        const float T = 0.2f, k = 0.5f, z = 6.0f;
        const float got = sh4der_torch_fog_bend(T, z, k);
        if (fabsf(got - (1.0f - (1.0f - T) * (1.0f - k))) > 1e-5f)
            return fail("fog bend: black composition");
    }
    for (i = 0; i < 129; i++) {   /* the bend only darkens: out <= in, at every entry */
        const float z = sh4der_torch_entry_depth(i, 40.0f), T = (float)i / 128.0f;
        if (sh4der_torch_fog_bend(T, z, 1.0f) < T - 1e-6f)
            return fail("fog bend only brightens the table");
    }
    return 0;
}

/* The darkness gate: thresholds, hysteresis with no pump inside the band, and ramps up and down. */
static int gate(void) {
    sh4der_torch_gate_t g;
    int i;
    sh4der_torch_gate_reset(&g);
    for (i = 0; i < 60; i++) sh4der_torch_gate_step(&g, 0.15f, 1.0f / 30.0f);   /* in between, from off: stays off */
    if (g.on || g.d != 0.0f)
        return fail("gate: between the thresholds from off holds off");
    sh4der_torch_gate_step(&g, 0.11f, 1.0f / 30.0f);
    if (!g.on)
        return fail("gate: on below ON_LO");
    for (i = 0; i < 36; i++) sh4der_torch_gate_step(&g, 0.15f, 1.0f / 30.0f);   /* 1.2 s: 0.944 of the way (UP 0.4 s, 3 time constants) */
    if (!g.on || g.d < 0.93f)
        return fail("gate: between the thresholds from on holds on, ramps up");
    sh4der_torch_gate_step(&g, 0.25f, 1.0f / 30.0f);
    if (g.on)
        return fail("gate: off above OFF_HI");
    for (i = 0; i < 30; i++) sh4der_torch_gate_step(&g, 0.25f, 1.0f / 30.0f);
    if (!(g.d < 0.5f) || g.d < 0.0f)
        return fail("gate: eases down (DOWN 1 s)");
    /* no pump: a wobble across the band from off never switches on; from on it never switches off */
    sh4der_torch_gate_reset(&g);
    for (i = 0; i < 200; i++) sh4der_torch_gate_step(&g, (i & 1) ? 0.21f : 0.13f, 1.0f / 30.0f);
    if (g.on || g.d != 0.0f)
        return fail("gate: a wobble in the band pumps");
    sh4der_torch_gate_step(&g, 0.1f, 1.0f / 30.0f);
    for (i = 0; i < 200; i++) sh4der_torch_gate_step(&g, (i & 1) ? 0.21f : 0.13f, 1.0f / 30.0f);
    if (!g.on)
        return fail("gate: a wobble in the band pumps from on");
    return 0;
}

/* The power ramp: full in 0.2 s and gone in 0.8 s at 30 Hz, and no change when paused. */
static int power(void) {
    float p = 0.0f;
    int i;
    for (i = 0; i < 6; i++) p = sh4der_torch_power_step(p, 1, 1.0f / 30.0f);   /* 6 ticks at 30 Hz: flashlight +1/6 each: full */
    if (p < 0.999f)
        return fail("power rises in 0.2 s");
    for (i = 0; i < 24; i++) p = sh4der_torch_power_step(p, 0, 1.0f / 30.0f);   /* 24 ticks: flashlight -1/24 each: none */
    if (p > 0.001f)
        return fail("power falls in 0.8 s");
    if (sh4der_torch_power_step(0.5f, 1, 0.0f) != 0.5f)
        return fail("power holds at dt 0 (paused)");
    return 0;
}

static uint8_t red(uint32_t c) { return (uint8_t)(c >> 16); }
static uint8_t green(uint32_t c) { return (uint8_t)(c >> 8); }
static int near8(uint32_t got, float want) { return fabsf((float)red(got) - want * 255.0f) <= 1.5f; }

/* The boost and carve meshes: strip sizes, eol per strip, edge colours and positions, corner coverage, daylight. */
static int meshes(void) {
    static sh4der_torch_vert_t bv[SH4DER_TORCH_VERTS], cv[SH4DER_TORCH_VERTS];
    const unsigned band_n = 2 * (SH4DER_TORCH_SPOKES + 1);
    unsigned nb, nc, i, k;
    const float cx = 320.0f, cy = 240.0f, rx = 320.0f, ry = 320.0f;
    nb = sh4der_torch_boost_mesh(bv, cx, cy, rx, ry, 1.0f);
    nc = sh4der_torch_carve_mesh(cv, cx, cy, rx, ry, 1.0f);
    if (nb != SH4DER_TORCH_RINGS * band_n || nc != SH4DER_TORCH_BANDS * band_n || nb + nc != SH4DER_TORCH_VERTS)
        return fail("mesh sizes: 5 boost + 4 carve strips of 34");
    for (i = 0; i < nb; i++) {   /* eol exactly on each band's last vertex */
        const int want = (i + 1) % band_n == 0;
        if (bv[i].eol != want)
            return fail("boost eol per strip");
    }
    for (i = 0; i < nc; i++) {
        const int want = (i + 1) % band_n == 0;
        if (cv[i].eol != want)
            return fail("carve eol per strip");
    }
    if (!near8(bv[0].argb, 1.0f) || red(bv[band_n * SH4DER_TORCH_RINGS - 1].argb) != 0)
        return fail("boost: full at the centre, 0 at the edge");
    if (green(bv[0].argb) != (uint8_t)(255.0f * SH4DER_TORCH_TINT_G + 0.5f))
        return fail("boost tint: green");
    if (!near8(cv[0].argb, 1.0f))
        return fail("carve: 1 at the feather's start");
    if (!near8(cv[band_n * SH4DER_TORCH_BANDS - 1].argb, SH4DER_TORCH_MUL_OUT))
        return fail("carve: MUL_OUT at UMAX");
    /* the edge ring sits on the ellipse: outer vertex of spoke 0 in the last boost band at (cx + rx, cy) */
    k = (SH4DER_TORCH_RINGS - 1) * band_n + 1;
    if (fabsf(bv[k].x - (cx + rx)) > 1e-3f || fabsf(bv[k].y - cy) > 1e-3f)
        return fail("boost edge on the radius");
    /* coverage: the carve reaches the screen corners (1.25 of the edge at 640 x 480 with rx = ry = 320) */
    {
        const float corner = sqrtf(1.0f + 0.5625f);
        if (!(corner < SH4DER_TORCH_UMAX))
            return fail("carve reaches the corners");
    }
    /* daylight: the carve mesh is all 1 (no change), the boost mesh stays */
    nc = sh4der_torch_carve_mesh(cv, cx, cy, rx, ry, 0.0f);
    for (i = 0; i < nc; i++)
        if (red(cv[i].argb) != 255)
            return fail("daylight: carve mesh is 1");
    return 0;
}

/* The carve by darkness: full up to DARK_LO, none from DARK_HI, never rising with more light, half way near the middle. */
static int dark(void) {
    float last = 1.0f, l;
    if (sh4der_torch_dark(0.0f) != 1.0f || sh4der_torch_dark(SH4DER_TORCH_DARK_LO) != 1.0f || sh4der_torch_dark(SH4DER_TORCH_DARK_HI) != 0.0f || sh4der_torch_dark(1.0f) != 0.0f)
        return fail("dark: the ends");
    for (l = 0.0f; l <= 1.0f; l += 0.01f) {
        const float d = sh4der_torch_dark(l);
        if (d > last + 1e-6f || d < 0.0f || d > 1.0f)
            return fail("dark: never rises with the light");
        last = d;
    }
    if (fabsf(sh4der_torch_dark(0.5f * (SH4DER_TORCH_DARK_LO + SH4DER_TORCH_DARK_HI)) - 0.5f) > 0.01f)
        return fail("dark: half at the middle");
    return 0;
}

int main(void) {
    if (radii() || profiles() || near_and_fog() || gate() || power() || meshes() || dark())
        return 1;
    printf("torch_test: ok\n");
    return 0;
}
