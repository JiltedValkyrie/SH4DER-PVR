/* SH4ZAM-DCE tests: shz_dce_override.h on the host, the defaults and one switch set by name

   override_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked:
   - a plain include renames shz_asinf, shz_acosf and shz_atan2f and nothing else (the defaults); one switch set by name
     (here SHZ_DCE_USE_INV_SQRT) renames its own group and no other
   - the renamed calls, written with SH4ZAM's names and argument types, compile and give the rework's value
   - the barrier switches of shz_dce_cdefs.h have their documented defaults
   Every switch on at once, and one taken back by name, is overrideall_test.c; the old switch SHZ_DCE_OVERRIDE_APPROX is
   tests/sh4/override_kernels.c.
*/

#define SHZ_DCE_USE_INV_SQRT 1
#include <sh4zam_dce/shz_dce_override.h>

#include "shz_dce_test.h"

#define NAME_(x) #x
#define NAME(x) NAME_(x)
#define IS(shz, dce) T_CHECK(!strcmp(NAME(shz), dce), "%s is %s, not %s", #shz, NAME(shz), dce)

static volatile float vf[2];

int main(void) {
    int k;

    /* on by default */
    IS(shz_asinf, "shz_dce_asinf");
    IS(shz_acosf, "shz_dce_acosf");
    IS(shz_atan2f, "shz_dce_atan2f");
    /* set by name in this file */
    IS(shz_inv_sqrtf, "shz_dce_inv_sqrtf");
    /* off by default: SH4ZAM's own routines */
    IS(shz_sqrtf, "shz_sqrtf");
    IS(shz_hypotf, "shz_hypotf");
    IS(shz_vec2_magnitude, "shz_vec2_magnitude");
    IS(shz_vec3_magnitude, "shz_vec3_magnitude");
    IS(shz_vec4_magnitude, "shz_vec4_magnitude");
    IS(shz_vec2_magnitude_inv, "shz_vec2_magnitude_inv");
    IS(shz_vec3_magnitude_inv, "shz_vec3_magnitude_inv");
    IS(shz_vec4_magnitude_inv, "shz_vec4_magnitude_inv");
    IS(shz_vec2_distance, "shz_vec2_distance");
    IS(shz_vec3_distance, "shz_vec3_distance");
    IS(shz_vec4_distance, "shz_vec4_distance");
    IS(shz_vec2_normalize_safe, "shz_vec2_normalize_safe");
    IS(shz_vec3_normalize_safe, "shz_vec3_normalize_safe");
    IS(shz_vec4_normalize_safe, "shz_vec4_normalize_safe");
    IS(shz_vec4_normalize, "shz_vec4_normalize");
    /* routines with no rework keep their names */
    IS(shz_vec3_normalize, "shz_vec3_normalize");
    IS(shz_invf, "shz_invf");
    IS(shz_inv_sqrtf_fsrra, "shz_inv_sqrtf_fsrra");

    T_CHECK(SHZ_DCE_USE_ASIN_ACOS == 1 && SHZ_DCE_USE_INV_SQRT == 1 && SHZ_DCE_USE_SQRT == 0 && SHZ_DCE_USE_MAGNITUDE == 0 && SHZ_DCE_USE_DISTANCE == 0 &&
            SHZ_DCE_USE_NORMALIZE == 0 && SHZ_DCE_USE_FIPR == 0 && SHZ_DCE_USE_ATAN2 == 1 && SHZ_DCE_OVERRIDE_ALL == 0, "the override switches' defaults");
    T_CHECK(SHZ_DCE_OPAQUE_SQRT == 0 && SHZ_DCE_OPAQUE_INV_SQRT == 1 && SHZ_DCE_OPAQUE_MAGNITUDE == 0 && SHZ_DCE_OPAQUE_DISTANCE == 0 && SHZ_DCE_OPAQUE_NORMALIZE2 == 1 &&
            SHZ_DCE_OPAQUE_NORMALIZE3 == 0 && SHZ_DCE_OPAQUE_NORMALIZE4 == 0 && SHZ_DCE_OPAQUE_ASIN_ACOS == 0, "the barrier switches' defaults");

    for (k = 0; k < 20000; k++) {   /* calls as a caller writes them */
        vf[0] = t_frand(0.0f, 9.0f); vf[1] = t_frand(-1.0f, 1.0f);
        {
            const float x = vf[0], c = vf[1];
            T_CHECK(shz_inv_sqrtf(x) == shz_dce_inv_sqrtf(x), "inv_sqrtf is the rework");
            T_CHECK(fabs(shz_asinf(c) - asin((double)c)) <= 8e-6 && fabs(shz_acosf(c) - acos((double)c)) <= 8e-6, "asinf / acosf are the reworks (their bound holds)");
            T_CHECK(fabs(shz_sqrtf(x) - sqrt((double)x)) <= 2e-6 * sqrt((double)x) + 1e-30, "sqrtf is still a square root");
            T_CHECK(fabs(shz_atan2f(c, x + 0.5f) - atan2((double)c, (double)(x + 0.5f))) <= 1.4e-5, "atan2f is the rework (its bound holds)");
        }
    }
    return t_done("override_test");
}
