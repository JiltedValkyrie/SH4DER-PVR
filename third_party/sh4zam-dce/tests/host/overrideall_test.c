/* SH4ZAM-DCE tests: shz_dce_override.h on the host, every switch on and one taken back by name

   overrideall_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked:
   - SHZ_DCE_OVERRIDE_ALL 1 renames every routine that has a rework; a switch set to 0 by name (here SHZ_DCE_USE_DISTANCE)
     stays off; SHZ_DCE_OPAQUE 1 sets every barrier switch
   - calls written with SH4ZAM's names and SH4ZAM's argument types compile and give the rework's value: the signatures drop in
*/

#define SHZ_DCE_OVERRIDE_ALL 1
#define SHZ_DCE_USE_DISTANCE 0
#define SHZ_DCE_OPAQUE 1
#include <sh4zam_dce/shz_dce_override.h>

#include "shz_dce_test.h"

#define NAME_(x) #x
#define NAME(x) NAME_(x)
#define IS(shz, dce) T_CHECK(!strcmp(NAME(shz), dce), "%s is %s, not %s", #shz, NAME(shz), dce)

static volatile float vf[8];

int main(void) {
    int k;

    IS(shz_sqrtf, "shz_dce_sqrtf");
    IS(shz_inv_sqrtf, "shz_dce_inv_sqrtf");
    IS(shz_hypotf, "shz_dce_hypotf");
    IS(shz_vec2_magnitude, "shz_dce_vec2_magnitude");
    IS(shz_vec3_magnitude, "shz_dce_vec3_magnitude_fipr");   /* the FIPR group is on too, and takes this name */
    IS(shz_vec4_magnitude, "shz_dce_vec4_magnitude");
    IS(shz_vec2_magnitude_inv, "shz_dce_vec2_magnitude_inv");
    IS(shz_vec3_magnitude_inv, "shz_dce_vec3_magnitude_inv");
    IS(shz_vec4_magnitude_inv, "shz_dce_vec4_magnitude_inv");
    IS(shz_vec2_normalize_safe, "shz_dce_vec2_normalize_safe");
    IS(shz_vec3_normalize_safe, "shz_dce_vec3_normalize_safe");
    IS(shz_vec4_normalize_safe, "shz_dce_vec4_normalize_safe");
    IS(shz_asinf, "shz_dce_asinf");
    IS(shz_acosf, "shz_dce_acosf");
    IS(shz_vec4_normalize, "shz_dce_vec4_normalize");
    IS(shz_atan2f, "shz_dce_atan2f");
    /* taken back by name */
    IS(shz_vec2_distance, "shz_vec2_distance");
    IS(shz_vec3_distance, "shz_vec3_distance");
    IS(shz_vec4_distance, "shz_vec4_distance");
    /* routines with no rework keep their names */
    IS(shz_vec3_normalize, "shz_vec3_normalize");

    T_CHECK(SHZ_DCE_OPAQUE_SQRT == 1 && SHZ_DCE_OPAQUE_INV_SQRT == 1 && SHZ_DCE_OPAQUE_MAGNITUDE == 1 && SHZ_DCE_OPAQUE_DISTANCE == 1 && SHZ_DCE_OPAQUE_NORMALIZE2 == 1 &&
            SHZ_DCE_OPAQUE_NORMALIZE3 == 1 && SHZ_DCE_OPAQUE_NORMALIZE4 == 1 && SHZ_DCE_OPAQUE_ASIN_ACOS == 1, "SHZ_DCE_OPAQUE 1 sets every barrier switch");

    for (k = 0; k < 20000; k++) {   /* calls as a caller writes them */
        int i;
        for (i = 0; i < 8; i++) vf[i] = t_frand(-3.0f, 3.0f);
        {
            const shz_vec4_t u = shz_vec4_init(vf[0], vf[1], vf[2], vf[3]), v = shz_vec4_init(vf[4], vf[5], vf[6], vf[7]);
            const float x = shz_fabsf(vf[0]), c = vf[1] * (1.0f / 3.0f);
            const shz_vec2_t n2 = shz_vec2_normalize_safe(u.xy);
            const shz_vec3_t n3 = shz_vec3_normalize_safe(u.xyz);
            const shz_vec4_t n4 = shz_vec4_normalize_safe(u), m4 = shz_vec4_normalize(u);
            T_CHECK(shz_sqrtf(x) == shz_dce_sqrtf(x) && shz_inv_sqrtf(x) == shz_dce_inv_sqrtf(x) && shz_hypotf(x, c) == shz_dce_hypotf(x, c), "scalar roots");
            T_CHECK(shz_vec2_magnitude(u.xy) == shz_dce_vec2_magnitude(u.xy) && shz_vec3_magnitude(u.xyz) == shz_dce_vec3_magnitude_fipr(u.xyz) &&
                    shz_vec4_magnitude(u) == shz_dce_vec4_magnitude(u), "magnitudes");
            T_CHECK(shz_vec2_magnitude_inv(u.xy) == shz_dce_vec2_magnitude_inv(u.xy) && shz_vec3_magnitude_inv(u.xyz) == shz_dce_vec3_magnitude_inv(u.xyz) &&
                    shz_vec4_magnitude_inv(u) == shz_dce_vec4_magnitude_inv(u), "inverse magnitudes");
            T_CHECK(fabs(shz_vec3_distance(u.xyz, v.xyz) - shz_dce_vec3_distance(u.xyz, v.xyz)) <= 1e-5, "distances (SH4ZAM's own here) agree with the reworks");
            T_CHECK(fabs((double)n2.x * n2.x + (double)n2.y * n2.y - 1.0) < 1e-5 && fabs((double)n3.x * n3.x + (double)n3.y * n3.y + (double)n3.z * n3.z - 1.0) < 1e-5 &&
                    fabs((double)n4.x * n4.x + (double)n4.y * n4.y + (double)n4.z * n4.z + (double)n4.w * n4.w - 1.0) < 1e-5, "safe normalisations");
            T_CHECK(fabs((double)m4.x * m4.x + (double)m4.y * m4.y + (double)m4.z * m4.z + (double)m4.w * m4.w - 1.0) < 1e-5, "vec4 normalize");
            T_CHECK(fabs(shz_asinf(c) - asin((double)c)) <= 8e-6 && fabs(shz_acosf(c) - acos((double)c)) <= 8e-6, "asinf / acosf are the reworks (their bound holds)");
            T_CHECK(fabs(shz_atan2f(vf[2], vf[3]) - atan2((double)vf[2], (double)vf[3])) <= 2.5e-6, "atan2f is the rework (its bound holds)");
        }
    }
    return t_done("overrideall_test");
}
