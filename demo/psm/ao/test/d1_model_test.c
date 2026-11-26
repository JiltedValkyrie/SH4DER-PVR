/* SH4DER-PVR: host test of the depth fade spike's arithmetic (demo/psm/ao/d1_model.h). A model, not a measurement.

   demo/psm/ao/test/d1_model_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Compile: cc -std=c99 -Wall -Wextra -Werror -DSH4DER_CORE_NO_SH4ZAM -Iinclude
            demo/psm/ao/test/d1_model_test.c -lm */

#include <math.h>
#include <stdio.h>

#include "../d1_model.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

int main(void) {
    unsigned d, last;
    float b;

    /* the code: 256 a turn, comes round */
    CHECK(d1_code(0.0f) == 0u && d1_code(D1_TURN * 0.5f) == 128u && d1_code(D1_TURN * 3.25f) == 64u);
    CHECK(d1_code(D1_TURN * 7.0f + 0.0001f) == 0u && d1_code(D1_TURN - 0.0001f) == 255u);
    CHECK(d1_diff(3u, 250u) == 9 && d1_diff(250u, 3u) == -9 && d1_diff(128u, 0u) == -128 && d1_diff(5u, 5u) == 0);

    /* the level: c4's cosine with K1 128 / K3 127; with 0 / 255 the lower half is 0 */
    CHECK(d1_level(128u, 127u, 0u) == 255u && d1_level(128u, 127u, 64u) == 128u && d1_level(128u, 127u, 128u) == 1u && d1_level(128u, 127u, 192u) == 128u);
    CHECK(d1_level(0u, 255u, 0u) == 255u && d1_level(0u, 255u, 64u) == 0u && d1_level(0u, 255u, 128u) == 0u && d1_level(0u, 255u, 191u) == 0u && d1_level(0u, 255u, 224u) == 180u);

    /* the fade: 0 at the halo, rising over D1_FADE, full at it; in front nothing */
    for (b = 3.7f; b < 9.0f; b += 1.13f) {
        CHECK(d1_fade(b, 0.0f, 1) <= 4u);
        CHECK(d1_fade(b, -0.05f, 1) == 0u && d1_fade(b, -2.0f, 0) == 0u);
        for (d = 0, last = 0; d <= 60u; d++) {          /* never falls on the way up (a code's step either way allowed) */
            const unsigned a = d1_fade(b, D1_FADE * (float)d / 64.0f, 1);

            CHECK(a + 8u >= last);
            if (a > last) last = a;
        }
        CHECK(d1_fade(b, D1_FADE * 0.98f, 1) >= 250u);
        CHECK(d1_fade(b, D1_FADE * 0.5f, 1) > 150u && d1_fade(b, D1_FADE * 0.5f, 1) < 210u);       /* sin 45 degrees: 180 */
        CHECK(d1_fade(b, D1_FADE * 1.5f, 1) == 255u && d1_fade(b, 7.77f, 1) == 255u);
        /* the design claim: without the second quad the level comes down again and is 0 over the turn's other half */
        CHECK(d1_fade(b, D1_FADE * 1.5f, 0) < 210u);
        CHECK(d1_fade(b, D1_FADE * 2.5f, 0) == 0u && d1_fade(b, D1_FADE * 3.5f, 0) == 0u);
        CHECK(d1_fade(b, D1_FADE * 4.5f, 0) > 150u);    /* and comes round a turn further */
    }

    /* a tilted quad: 1 / depth is what runs evenly across the screen */
    CHECK(fabsf(d1_tilt_depth(160.0f, 320.0f, 3.0f, 6.0f, 160.0f) - 3.0f) < 1e-4f && fabsf(d1_tilt_depth(160.0f, 320.0f, 3.0f, 6.0f, 320.0f) - 6.0f) < 1e-4f);
    CHECK(fabsf(d1_tilt_depth(160.0f, 320.0f, 3.0f, 6.0f, 240.0f) - 4.0f) < 1e-4f);    /* the middle of the screen span is depth 4, not 4.5 */

    if (fails) printf("d1_model_test: %d FAILED\n", fails); else printf("d1_model_test: ok\n");
    return fails ? 1 : 0;
}
