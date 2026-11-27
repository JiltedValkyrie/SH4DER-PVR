/* SH4DER-PVR: host test of the mip depth test's arithmetic (demo/psm/ao/mip_model.h). A model, not a measurement.

   demo/psm/ao/test/mip_model_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Compile: cc -std=c99 -Wall -Wextra -Werror -DSH4DER_CORE_NO_SH4ZAM -Iinclude
            demo/psm/ao/test/mip_model_test.c -lm */

#include <math.h>
#include <stdio.h>

#include "../mip_model.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

static uint16_t chain[MIP_BYTES / 2u + 16u];

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    unsigned k, rule;
    uint32_t i, end = 6u;
    float z;

    /* the chain: the levels follow one another from byte 6 with one gap (bytes 6, 8: the 1 x 1 level is 2 bytes) and end at MIP_BYTES */
    for (k = MIP_LEVELS; k-- > 0u;) {
        CHECK(mip_offset(k) >= end);
        CHECK(mip_offset(k) - end <= 6u);
        end = mip_offset(k) + mip_side(k) * mip_side(k) * 2u;
    }
    CHECK(end == MIP_BYTES);
    CHECK(mip_side(0) == 256u && mip_side(8) == 1u && mip_offset(8) == 6u && mip_offset(0) == 0xaab0u);

    /* a level's texel reads back as its code, and the codes are apart */
    for (k = 0; k < MIP_LEVELS; k++) {
        CHECK(mip_code(mip_texel(k)) == MIP_STEP * (k + 1u));
        CHECK((mip_texel(k) >> 11) == 3u * (k + 1u) && (mip_texel(k) & 31u) == 3u * (k + 1u));
    }
    CHECK(MIP_STEP * MIP_LEVELS <= 63u && 3u * MIP_LEVELS <= 31u);

    /* every word of the chain is some level's, and each level is whole */
    mip_fill(chain);
    for (i = 0; i < MIP_BYTES / 2u; i++) CHECK(mip_code(chain[i]) % MIP_STEP == 0u && mip_code(chain[i]) != 0u);
    for (k = 0; k < MIP_LEVELS; k++) {
        CHECK(chain[mip_offset(k) / 2u] == mip_texel(k));
        CHECK(chain[mip_offset(k) / 2u + mip_side(k) * mip_side(k) - 1u] == mip_texel(k));
    }

    /* the floor: 1 m is the bottom row, the row gives the depth back, flat and tilted */
    CHECK(fabsf(mip_floor_y(1.0f, 0.0f) - 240.0f) < 1e-3f && fabsf(mip_floor_y(60.0f, 0.0f) - 122.0f) < 1e-3f);
    CHECK(fabsf(mip_floor_y(1.0f, 0.15f) - 240.0f) < 1e-3f);
    for (z = 1.0f; z < 60.0f; z *= 1.37f) {
        CHECK(fabsf(mip_floor_z(mip_floor_y(z, 0.0f), 0.0f) - z) < z * 1e-3f);
        CHECK(fabsf(mip_floor_z(mip_floor_y(z, 0.15f), 0.15f) - z) < z * 1e-3f);
    }

    /* the ratios: across is the depth itself; down the floor against a difference of two rows' texture coordinates */
    CHECK(fabsf(mip_rx(1.0f) - 1.0f) < 1e-5f && fabsf(mip_rx(24.0f) - 24.0f) < 1e-4f);
    for (z = 1.5f; z < 40.0f; z *= 1.7f) {
        const float y = mip_floor_y(z, 0.0f), za = mip_floor_z(y - 0.5f, 0.0f), zb = mip_floor_z(y + 0.5f, 0.0f);
        const float texels = (za - zb) * (float)MIP_SIZE / MIP_S;

        CHECK(fabsf(texels - mip_ry(z, 0.0f)) < mip_ry(z, 0.0f) * 0.05f);
    }

    /* log2, and the guesses' order: MIN <= AREA <= MAX, all rising with depth, MAX at the last level far away */
    CHECK(mip_ilog2(0.3f) == 0u && mip_ilog2(1.99f) == 0u && mip_ilog2(2.0f) == 1u && mip_ilog2(255.0f) == 7u && mip_ilog2(256.0f) == 8u && mip_ilog2(1e9f) == 8u);
    for (z = 1.0f; z < 60.0f; z *= 1.21f) {
        CHECK(mip_guess(MIP_MIN, z, 0.0f) <= mip_guess(MIP_AREA, z, 0.0f) && mip_guess(MIP_AREA, z, 0.0f) <= mip_guess(MIP_MAX, z, 0.0f));
        for (rule = 0; rule < MIP_RULES; rule++) CHECK(mip_guess(rule, z, 0.0f) <= mip_guess(rule, z * 1.21f, 0.0f));
    }
    CHECK(mip_guess(MIP_MAX, 40.0f, 0.0f) == 8u && mip_guess(MIP_MIN, 3.0f, 0.0f) == 1u && mip_guess(MIP_MIN, 24.0f, 0.0f) == 4u);

    if (fails) printf("mip_model_test: %d FAILED\n", fails); else printf("mip_model_test: ok\n");
    return fails ? 1 : 0;
}
