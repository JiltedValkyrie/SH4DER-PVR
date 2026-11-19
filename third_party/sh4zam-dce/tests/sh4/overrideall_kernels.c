/* SH4ZAM-DCE tests: shz_dce_override.h, the SH-4 compile check with every switch on

   overrideall_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc in the four flag sets (the compile step): code written with SH4ZAM's names, as a caller has it, after the
   override include with SHZ_DCE_OVERRIDE_ALL 1. What the reworks compute is tests/sh4/sqrt_check.c's, trig_check.c's,
   atan2_check.c's and vector_check.c's.
*/

#define SHZ_DCE_OVERRIDE_ALL 1
#include <sh4zam_dce/shz_dce_override.h>

#if !defined(shz_asinf) || !defined(shz_sqrtf) || !defined(shz_inv_sqrtf) || !defined(shz_vec2_magnitude) || !defined(shz_vec4_distance) || \
    !defined(shz_vec3_normalize_safe) || !defined(shz_vec4_normalize) || !defined(shz_atan2f)
#error "SHZ_DCE_OVERRIDE_ALL 1 must turn every group on"
#endif

float t_overrideall(const shz_vec4_t *u, const shz_vec4_t *v, shz_vec4_t *out) {
    const shz_vec2_t n2 = shz_vec2_normalize_safe(u->xy);
    const shz_vec3_t n3 = shz_vec3_normalize_safe(u->xyz);
    const shz_vec4_t m4 = shz_vec4_normalize(*u);
    *out = shz_vec4_normalize_safe(*u);
    return shz_sqrtf(u->x) + shz_inv_sqrtf(u->y) + shz_hypotf(u->x, u->y) +
           shz_vec2_magnitude(u->xy) + shz_vec3_magnitude(u->xyz) + shz_vec4_magnitude(*u) +
           shz_vec2_magnitude_inv(u->xy) + shz_vec3_magnitude_inv(u->xyz) + shz_vec4_magnitude_inv(*u) +
           shz_vec2_distance(u->xy, v->xy) + shz_vec3_distance(u->xyz, v->xyz) + shz_vec4_distance(*u, *v) +
           n2.x + n3.y + m4.w + shz_asinf(u->z) + shz_acosf(u->w) + shz_atan2f(u->y, u->x);
}
