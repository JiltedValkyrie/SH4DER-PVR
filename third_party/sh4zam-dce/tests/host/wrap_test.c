/* SH4ZAM-DCE tests: the shz_dce_ names that are SH4ZAM's own routines, on the host

   wrap_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   SH4ZAM has a routine for each of these twenty-one jobs (master ff39547, 2026-11-22), so the shz_dce_ name calls SH4ZAM's routine
   underneath and nothing is replaced: the roots and lengths, the FIPR names, shz_dce_asinf / _acosf (SH4ZAM took them as cff97e8,
   #92), shz_dce_atan2f (the same commit) and shz_dce_opaquef (SH4ZAM's shz_opaquef, 3cdff11). What is checked:
   - each of the twenty-one gives, bit for bit, what SH4ZAM's routine gives: over random inputs and over zeros of both signs,
     denormals, squares that underflow, negatives, infinities and NaN; each from its own function that is never inlined
   Pinned to ff39547: a later SH4ZAM that changes a routine still passes here (the wrapper follows it); the routines' own claims are
   pinned in sqrt_test.c, trig_test.c and atan2_test.c.
*/

#include <sh4zam_dce/shz_dce_sqrt.h>
#include <sh4zam_dce/shz_dce_vector.h>
#include <sh4zam_dce/shz_dce_trig.h>
#include <sh4zam_dce/shz_dce_atan2.h>

#include "shz_dce_test.h"

#define NOINLINE __attribute__((noinline))

/* one function a routine and a name: the two are compiled apart, as two callers would have them */
#define F1(name) \
    static NOINLINE float d_##name(float x) { return shz_dce_##name(x); } \
    static NOINLINE float s_##name(float x) { return shz_##name(x); }
#define VF(n, name, dce) \
    static NOINLINE float d_vec##n##_##name(shz_vec##n##_t v) { return shz_dce_vec##n##_##dce(v); } \
    static NOINLINE float s_vec##n##_##name(shz_vec##n##_t v) { return shz_vec##n##_##name(v); }
#define VVF(n, name) \
    static NOINLINE float d_vec##n##_##name(shz_vec##n##_t u, shz_vec##n##_t v) { return shz_dce_vec##n##_##name(u, v); } \
    static NOINLINE float s_vec##n##_##name(shz_vec##n##_t u, shz_vec##n##_t v) { return shz_vec##n##_##name(u, v); }
#define VV(n, name) \
    static NOINLINE shz_vec##n##_t d_vec##n##_##name(shz_vec##n##_t v) { return shz_dce_vec##n##_##name(v); } \
    static NOINLINE shz_vec##n##_t s_vec##n##_##name(shz_vec##n##_t v) { return shz_vec##n##_##name(v); }

F1(sqrtf) F1(inv_sqrtf) F1(asinf) F1(acosf) F1(opaquef)
static NOINLINE float d_atan2f(float y, float x) { return shz_dce_atan2f(y, x); }
static NOINLINE float s_atan2f(float y, float x) { return shz_atan2f(y, x); }
static NOINLINE float d_hypotf(float x, float y) { return shz_dce_hypotf(x, y); }
static NOINLINE float s_hypotf(float x, float y) { return shz_hypotf(x, y); }
VF(2, magnitude, magnitude) VF(3, magnitude, magnitude) VF(4, magnitude, magnitude)
VF(2, magnitude_inv, magnitude_inv) VF(3, magnitude_inv, magnitude_inv) VF(4, magnitude_inv, magnitude_inv)
VVF(2, distance) VVF(3, distance) VVF(4, distance)
VV(2, normalize_safe) VV(3, normalize_safe) VV(4, normalize_safe)
VV(4, normalize)
static NOINLINE float d_vec3_magnitude_fipr(shz_vec3_t v) { return shz_dce_vec3_magnitude_fipr(v); }

static const uint32_t special[] = {
    0x00000000u, 0x80000000u, 0x00000001u, 0x80000001u, 0x007fffffu, 0x00800000u, 0x80800000u, 0x0da24260u /* 1e-30: its square underflows */,
    0x1e3ce508u /* 1e-20 */, 0x3f800000u, 0xbf800000u, 0x40490fdbu, 0x60ad78ecu /* 1e20: its square overflows */, 0x7f7fffffu, 0x7f800000u, 0xff800000u, 0x7fc00000u,
};
#define SPECIALS (sizeof special / sizeof special[0])

static float pick(int trial, int lane) {
    if (trial < 4000) return t_float(special[(t_rand() >> 8) % SPECIALS]);
    if ((trial + lane) % 7 == 0) return t_float(special[(t_rand() >> 8) % SPECIALS]);
    return t_frand(-50.0f, 50.0f) * (trial % 3 == 0 ? 1e-18f : 1.0f);
}

#define SAME(a, b, what) T_CHECK(t_bits(a) == t_bits(b), "%s: %08x, SH4ZAM's %08x (trial %d)", what, (unsigned)t_bits(a), (unsigned)t_bits(b), trial)

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    int trial;

    for (trial = 0; trial < 60000; trial++) {
        float f[8];
        int i;

        for (i = 0; i < 8; i++) f[i] = pick(trial, i);
        {
            const shz_vec4_t u = shz_vec4_init(f[0], f[1], f[2], f[3]), v = shz_vec4_init(f[4], f[5], f[6], f[7]);
            const shz_vec2_t n2 = d_vec2_normalize_safe(u.xy), m2 = s_vec2_normalize_safe(u.xy);
            const shz_vec3_t n3 = d_vec3_normalize_safe(u.xyz), m3 = s_vec3_normalize_safe(u.xyz);
            const shz_vec4_t n4 = d_vec4_normalize_safe(u), m4 = s_vec4_normalize_safe(u), p4 = d_vec4_normalize(u), q4 = s_vec4_normalize(u);

            SAME(d_sqrtf(f[0]), s_sqrtf(f[0]), "sqrtf");
            SAME(d_inv_sqrtf(f[0]), s_inv_sqrtf(f[0]), "inv_sqrtf");
            SAME(d_hypotf(f[0], f[1]), s_hypotf(f[0], f[1]), "hypotf");
            SAME(d_asinf(f[0]), s_asinf(f[0]), "asinf");
            SAME(d_acosf(f[0]), s_acosf(f[0]), "acosf");
            SAME(d_asinf(f[0] * 1e-2f), s_asinf(f[0] * 1e-2f), "asinf in range");
            SAME(d_acosf(f[1] * 1e-2f), s_acosf(f[1] * 1e-2f), "acosf in range");
            SAME(d_atan2f(f[0], f[1]), s_atan2f(f[0], f[1]), "atan2f");
            SAME(d_opaquef(f[2]), s_opaquef(f[2]), "opaquef");
            SAME(d_vec2_magnitude(u.xy), s_vec2_magnitude(u.xy), "vec2_magnitude");
            SAME(d_vec3_magnitude(u.xyz), s_vec3_magnitude(u.xyz), "vec3_magnitude");
            SAME(d_vec4_magnitude(u), s_vec4_magnitude(u), "vec4_magnitude");
            SAME(d_vec2_magnitude_inv(u.xy), s_vec2_magnitude_inv(u.xy), "vec2_magnitude_inv");
            SAME(d_vec3_magnitude_inv(u.xyz), s_vec3_magnitude_inv(u.xyz), "vec3_magnitude_inv");
            SAME(d_vec4_magnitude_inv(u), s_vec4_magnitude_inv(u), "vec4_magnitude_inv");
            SAME(d_vec2_distance(u.xy, v.xy), s_vec2_distance(u.xy, v.xy), "vec2_distance");
            SAME(d_vec3_distance(u.xyz, v.xyz), s_vec3_distance(u.xyz, v.xyz), "vec3_distance");
            SAME(d_vec4_distance(u, v), s_vec4_distance(u, v), "vec4_distance");
            T_CHECK(!memcmp(&n2, &m2, sizeof n2), "vec2_normalize_safe: not SH4ZAM's bits (trial %d)", trial);
            T_CHECK(!memcmp(&n3, &m3, sizeof n3), "vec3_normalize_safe: not SH4ZAM's bits (trial %d)", trial);
            T_CHECK(!memcmp(&n4, &m4, sizeof n4), "vec4_normalize_safe: not SH4ZAM's bits (trial %d)", trial);
            T_CHECK(!memcmp(&p4, &q4, sizeof p4), "vec4_normalize: not SH4ZAM's bits (trial %d)", trial);
            SAME(d_vec3_magnitude_fipr(u.xyz), s_vec3_magnitude(u.xyz), "vec3_magnitude_fipr");
        }
    }
    printf("wrap: 21 shz_dce_ names == SH4ZAM's routine of the same job, bit for bit, 60000 inputs each\n");
    return t_done("wrap_test");
}
