/* SH4ZAM-DCE tests: shz_dce_override.h, the SH-4 compile check of the old switch

   override_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc in the four flag sets (the compile step). SHZ_DCE_OVERRIDE_APPROX 0, the switch this header had before
   it had one a group, must still turn the arcsine, arccosine and arctangent off; with nothing else set no name is renamed, and
   code written with SH4ZAM's names compiles to SH4ZAM's routines. Every switch on is overrideall_kernels.c.
*/

#define SHZ_DCE_OVERRIDE_APPROX 0
#include <sh4zam_dce/shz_dce_override.h>

#if defined(shz_asinf) || defined(shz_acosf) || defined(shz_atan2f) || defined(shz_vec4_normalize)
#error "SHZ_DCE_OVERRIDE_APPROX 0 must leave the arcsine, the arccosine, the arctangent and the FIPR forms off"
#endif
#if defined(shz_sqrtf) || defined(shz_inv_sqrtf) || defined(shz_hypotf) || defined(shz_vec3_magnitude) || defined(shz_vec3_distance) || defined(shz_vec3_normalize_safe)
#error "the root family is off unless its switches are set"
#endif

float t_override(const shz_vec4_t *u, const shz_vec4_t *v, shz_vec4_t *out) {
    *out = shz_vec4_normalize_safe(*u);
    return shz_sqrtf(u->x) + shz_inv_sqrtf(u->y) + shz_vec3_magnitude(u->xyz) + shz_vec3_distance(u->xyz, v->xyz) + shz_asinf(u->z) + shz_acosf(u->w) + shz_atan2f(u->y, u->x);
}
