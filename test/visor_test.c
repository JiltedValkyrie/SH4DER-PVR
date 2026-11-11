/* SH4DER-PVR

   test/visor_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the visor modes: the presets and the amount's grade, the
   night tint through the look's layers, the thermal ramp's affine fit (the
   error on grey and on coloured input, printed), the thermal LUT and grid,
   the grain / scanline tiles, the vignette alpha and the fade envelope.
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_visor.h"

#ifndef SH4DER_TEST_TOL
#define SH4DER_TEST_TOL 0.0002f
#endif

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

/* One channel of a grey or coloured input through the solved layers, as the blender does it. */
static float through(const sh4der_look_layer_t *l, int n, int c, float x) {
    int k;
    for (k = 0; k < n; k++)
        x = sh4der_look_apply_(&l[k], c, x);
    return x;
}

/* Presets: bad modes refused, amount 0 neutral, night is green, amber is warm. */
static int presets(void) {
    sh4der_visor_desc_t d;
    sh4der_look_grade_t g;
    sh4der_look_layer_t l[SH4DER_LOOK_LAYERS];
    int n;
    if (sh4der_visor_preset(0, &d) != -1 || sh4der_visor_preset(SH4DER_VISOR_MODES, &d) != -1) return fail("bad mode refused");
    if (sh4der_visor_preset(SH4DER_VISOR_OFF + 0, &d) != -1) return fail("off is not a preset");
    /* amount 0 is neutral: no layer at all */
    sh4der_visor_preset(SH4DER_VISOR_NIGHT, &d);
    sh4der_visor_grade(&d, 0.0f, &g);
    if (sh4der_look_solve(&g, l) != 0) return fail("amount 0 neutral");
    /* night: green is the strong channel. A grey 0.5 comes out green-led (G > R, G > B) */
    sh4der_visor_grade(&d, 1.0f, &g);
    n = sh4der_look_solve(&g, l);
    if (n < 1 || n > SH4DER_LOOK_LAYERS) return fail("night layer count");
    {
        const float r = through(l, n, 0, 0.5f), gg = through(l, n, 1, 0.5f), b = through(l, n, 2, 0.5f);
        printf("night grey 0.5 -> r %.4f g %.4f b %.4f (layers %d)\n", r, gg, b, n);
        if (!(gg > r * 2.0f && gg > b * 2.0f)) return fail("night is green");
    }
    /* amber: red over blue */
    sh4der_visor_preset(SH4DER_VISOR_AMBER, &d);
    sh4der_visor_grade(&d, 1.0f, &g);
    n = sh4der_look_solve(&g, l);
    if (!(through(l, n, 0, 0.5f) > through(l, n, 2, 0.5f) * 2.0f)) return fail("amber is warm");
    return 0;
}

/* The thermal ramp's affine fit: worst error on grey input (must stay under 0.25) and on pure red, printed. */
static int thermal_fit(void) {
    float gain[3], lift[3];
    int c, k;
    float worst_grey = 0.0f, worst_red = 0.0f;
    sh4der_visor_fit_ramp(gain, lift);
    printf("thermal affine fit: gain %.4f %.4f %.4f lift %.4f %.4f %.4f\n", gain[0], gain[1], gain[2], lift[0], lift[1], lift[2]);
    /* grey input: x = luma, so the line's error is the fit's own */
    for (k = 0; k <= 32; k++) {
        const float l = (float)k / 32.0f;
        for (c = 0; c < 3; c++) {
            const float e = fabsf(gain[c] * l + lift[c] - sh4der_visor_ramp_(c, l));
            if (e > worst_grey) worst_grey = e;
        }
    }
    /* pure red input (luma 0.299): the line sees red = 1 on the R channel, not the luma */
    for (c = 0; c < 3; c++) {
        const float e = fabsf(gain[c] * 1.0f + lift[c] - sh4der_visor_ramp_(c, 0.299f));
        if (e > worst_red) worst_red = e;
    }
    printf("thermal fit worst error: grey %.4f, pure red %.4f (colour input is not luma)\n", worst_grey, worst_red);
    if (!(worst_grey < 0.25f)) return fail("thermal fit grey");
    return 0;
}

/* The thermal LUT ends (cold blue, hot white) and the grid lookup for white and black input. */
static int thermal_map(void) {
    static uint16_t lut[SH4DER_VISOR_LUT_BINS];
    static uint16_t src[SH4DER_FOCUS_SRC_W * SH4DER_FOCUS_SRC_H];
    static uint16_t dst[SH4DER_FOCUS_TEX_W * SH4DER_FOCUS_TEX_H];
    const uint16_t white = 0xffffu, black = 0x0000u;
    unsigned i;
    sh4der_visor_lut(lut);
    /* cold end is blue, hot end is white-hot */
    if (!((lut[0] & 31) > (lut[0] >> 11))) return fail("lut cold is blue");
    if (!((lut[SH4DER_VISOR_LUT_BINS - 1] >> 11) >= 30 && ((lut[SH4DER_VISOR_LUT_BINS - 1] >> 5) & 63) >= 55)) return fail("lut hot is white");
    for (i = 0; i < SH4DER_FOCUS_SRC_W * SH4DER_FOCUS_SRC_H; i++) src[i] = white;
    sh4der_visor_thermal_grid(lut, src, dst);
    if (dst[0] != lut[sh4der_look_luma565(white)]) return fail("thermal grid through lut");
    for (i = 0; i < SH4DER_FOCUS_SRC_W * SH4DER_FOCUS_SRC_H; i++) src[i] = black;
    sh4der_visor_thermal_grid(lut, src, dst);
    if (dst[0] != lut[0]) return fail("thermal grid black");
    return 0;
}

/* The grain tile (amp 0 all white, amp 0.3 keeps the top 5 bits in 21..31), the tile byte sizes, and the scan rows. */
static int tiles(void) {
    static uint16_t g[SH4DER_VISOR_GRAIN_TEX * SH4DER_VISOR_GRAIN_TEX];
    static uint16_t s[SH4DER_VISOR_SCAN_TEX * SH4DER_VISOR_SCAN_TEX];
    unsigned i;
    sh4der_visor_noise_grid(g, 0.0f);
    for (i = 0; i < SH4DER_VISOR_GRAIN_TEX * SH4DER_VISOR_GRAIN_TEX; i++)
        if (g[i] != 0xffffu) return fail("grain amp 0 is white");
    sh4der_visor_noise_grid(g, 0.3f);
    for (i = 0; i < SH4DER_VISOR_GRAIN_TEX * SH4DER_VISOR_GRAIN_TEX; i++) {
        const unsigned r = g[i] >> 11;
        if (r < 21u || r > 31u) return fail("grain range 0.7..1");
    }
    if (sizeof g != SH4DER_VISOR_GRAIN_BYTES || sizeof s != SH4DER_VISOR_SCAN_BYTES) return fail("tile bytes");
    sh4der_visor_scan_grid(s, 0.0f);
    if (s[0] != 0xffffu || s[SH4DER_VISOR_SCAN_TEX * 7] != 0xffffu) return fail("scan depth 0 white");
    sh4der_visor_scan_grid(s, 0.5f);
    if (!((s[0] >> 11) < 31u && (s[SH4DER_VISOR_SCAN_TEX * 2] >> 11) == 31u)) return fail("scan rows 0,1 dark only");
    return 0;
}

/* The vignette alpha at full, zero and half amount, and the fade in and out envelopes. */
static int vignette_and_fade(void) {
    sh4der_visor_fade_t f = { 0.0f };
    float a;
    if (sh4der_visor_vignette_argb(0xff00ff00u, 1.0f, 1.0f) != 0xff000000u) return fail("vignette full");
    if (sh4der_visor_vignette_argb(0xff00ff00u, 1.0f, 0.0f) != 0u) return fail("vignette amount 0");
    if (sh4der_visor_vignette_argb(0xff00ff00u, 0.5f, 1.0f) != 0x80000000u) return fail("vignette half");
    /* in: 0.25 s to on; eight 1/8 s steps: 0, 0.5, 1 (smoothstep 0.5 at the middle) */
    a = sh4der_visor_fade(&f, 1, 0.125f);
    if (fabsf(a - 0.5f) > SH4DER_TEST_TOL) return fail("fade in half at middle");
    a = sh4der_visor_fade(&f, 1, 0.125f);
    if (fabsf(a - 1.0f) > SH4DER_TEST_TOL) return fail("fade in full at 0.25 s");
    a = sh4der_visor_fade(&f, 1, 0.5f);
    if (a != 1.0f) return fail("fade held at on");
    /* out: 0.40 s; a 0.2 s step is half */
    a = sh4der_visor_fade(&f, 0, 0.2f);
    if (fabsf(a - 0.5f) > SH4DER_TEST_TOL) return fail("fade out half at 0.2 s");
    a = sh4der_visor_fade(&f, 0, 1.0f);
    if (a != 0.0f) return fail("fade out held at off");
    return 0;
}

static int run_all(void) {
    if (presets()) return 1;
    if (thermal_fit()) return 1;
    if (thermal_map()) return 1;
    if (tiles()) return 1;
    if (vignette_and_fade()) return 1;
    return 0;
}

int main(void) {
    if (run_all()) return 1;
    printf("visor_test: ok\n");
    return 0;
}
