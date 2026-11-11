/* SH4DER-PVR

   test/frame_test.c
   Copyright (c) 2026 Jilted Valkyrie

   Host check of the frame read's portable parts and of the glow's read helpers: the stamp
   there and back, the history ring, the darker-of-two pixel, the gain taken off, the
   looked-at term, the glow taken off a read, the weight table from a threshold.
*/

#include <math.h>
#include <stdio.h>
#include "../include/sh4der_frame.h"
#include "../include/sh4der_glow.h"

static int fail(const char *msg) {
    fprintf(stderr, "FAIL %s\n", msg);
    return 1;
}

static uint16_t pack(unsigned r8, unsigned g8, unsigned b8) { return (uint16_t)(((r8 >> 3) << 11) | ((g8 >> 2) << 5) | (b8 >> 3)); }

static int stamp(void) {
    sh4der_frame_hist_t h;
    unsigned id, seen = 0;
    sh4der_frame_hist_reset(&h);
    if (h.gain[3] != 1.0f || h.glow[7] != 0.0f)
        return fail("history start");
    for (id = 0; id < SH4DER_FRAME_HISTORY; id++) {
        const uint32_t c = sh4der_frame_stamp_argb(id);
        if (sh4der_frame_stamp_id(pack((c >> 16) & 255u, (c >> 8) & 255u, c & 255u)) != id)   /* through RGB565 and back */
            return fail("stamp there and back");
    }
    for (id = 0; id < 20; id++) {   /* the ring: 1, 2 .. 7, 0, 1 .. and each keeps its own numbers */
        const unsigned s = sh4der_frame_next(&h, 1.0f + 0.01f * (float)id, 0.01f * (float)id, 0.1f * (float)id);
        if (s != ((id + 1u) & 7u) || h.gain[s] != 1.0f + 0.01f * (float)id || h.glow[s] != 0.1f * (float)id)
            return fail("history ring");
        seen |= 1u << s;
    }
    return seen == 255u ? 0 : fail("every stamp used");
}

static int pixels(void) {
    uint16_t px[3] = { pack(128, 128, 128), pack(255, 255, 255), pack(16, 200, 40) };
    if (sh4der_frame_min565(pack(255, 0, 80), pack(40, 200, 255)) != pack(40, 0, 80))
        return fail("darker of two");
    sh4der_frame_degain(px, 3, 1.0f);
    if (px[0] != pack(128, 128, 128))
        return fail("gain 1 changes nothing");
    sh4der_frame_degain(px, 3, 0.5f);   /* the frame was halved: the scene is twice the read, clamped at white */
    if (px[0] != pack(255, 255, 255) || px[1] != pack(255, 255, 255))
        return fail("gain taken off, clamped");
    px[0] = pack(200, 200, 200);
    sh4der_frame_degain(px, 1, 2.0f);
    if (((px[0] >> 5) & 63u) != ((pack(200, 200, 200) >> 5) & 63u) / 2u)
        return fail("gain 2 halves");
    if (sh4der_frame_hot(0.5f) != 0.0f || sh4der_frame_hot(SH4DER_FRAME_HOT_FROM) != 0.0f || sh4der_frame_hot(1.0f) != 1.0f || !(sh4der_frame_hot(0.95f) > 0.2f && sh4der_frame_hot(0.95f) < 0.7f))
        return fail("looked-at term");
    return 0;
}

static int glow_helpers(void) {
    uint8_t lut[256];
    uint16_t grid[4] = { pack(200, 200, 200), pack(40, 40, 40), pack(200, 200, 200), pack(0, 0, 0) };
    const uint16_t map[4] = { pack(100, 100, 100), pack(255, 255, 255), 0, 0 }, small[1] = { pack(40, 40, 40) };
    int i;
    sh4der_glow_curve(lut, SH4DER_GLOW_THRESHOLD, 1);   /* the shipped curve */
    {
        uint8_t ref[256];
        sh4der_glow_lut(ref);
        for (i = 0; i < 256; i++)
            if (lut[i] != ref[i])
                return fail("curve at the shipped numbers is the shipped table");
    }
    sh4der_glow_curve(lut, 0.5f, 0);   /* straight from 0.5 */
    if (lut[127] != 0 || lut[255] != 255 || lut[191] < 120 || lut[191] > 135)
        return fail("straight curve");
    for (i = 1; i < 256; i++)
        if (lut[i] < lut[i - 1])
            return fail("curve never falls");
    /* the read carried the glow: map x 0.5 + shrink x 0.25 comes off, never under 0, nothing where the maps are empty */
    sh4der_glow_unfeed(grid, 4, 1, map, 1, small, 1, 0.5f, 0.25f);
    if (((grid[0] >> 5) & 63u) > 37u || ((grid[0] >> 5) & 63u) < 33u)   /* 200 - 50 - 10 = 140 -> 35 of 63 */
        return fail("glow taken off");
    if (grid[1] != 0)
        return fail("never under black");
    if (((grid[2] >> 5) & 63u) > 49u || ((grid[2] >> 5) & 63u) < 45u)   /* only the shrink's 10 */
        return fail("the shrink alone");
    return 0;
}

int main(void) {
    if (stamp() || pixels() || glow_helpers())
        return 1;
    printf("frame_test: ok\n");
    return 0;
}
