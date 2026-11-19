/* SH4ZAM-DCE tests: shz_dce_sqrt.h on the host (SH4ZAM's software back end)

   sqrt_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked:
   - every rework against SH4ZAM's routine of the same name on the same input: the same bits. One allowance, counted and printed:
     a zero result may differ in its sign in a build with -ffast-math (there the compiler may hand back the operand that compared
     equal to zero; the SH-4 build pins the zero, tests/sh4/sqrt_check.c proves +0 there)
   - zeros of both signs give zero; ordinary inputs give the exact root / length / unit vector within a loose bound
   The SH-4 timing claim of the header (the test in FSRRA's shadow) is tests/sh4/sqrt_bench.c's.
*/

#include <sh4zam_dce/shz_dce_sqrt.h>

#include "shz_dce_test.h"

#define REL 9.5367431640625e-07   /* 2^-20 */

static int zero_signs;

static int same(float a, float b) {
    if (t_bits(a) == t_bits(b)) return 1;
    if ((t_bits(a) << 1) == 0u && (t_bits(b) << 1) == 0u) { zero_signs++; return 1; }
    return 0;
}

static const uint32_t special[] = {
    0x00000000u, 0x80000000u, 0x00800000u, 0x0da24260u, 0x1e3ce508u, 0x3f800000u, 0x40800000u, 0x3e800000u, 0x40000000u, 0x3f000000u,
    0x7149f2cau, 0x7f7fffffu, 0x3f7fffffu, 0x3f800001u,
};

static float pick(void) {
    switch (t_rand() % 6u) {
    case 0: return t_float(special[t_rand() % (sizeof special / sizeof *special)]);
    case 1: return t_float((t_rand() % 0x7e000000u) + 0x00800000u);   /* any positive normal */
    case 2: return t_frand(-4.0f, 4.0f);
    case 3: return t_frand(0.0f, 1.0f) * 1e-18f;
    case 4: return t_frand(0.0f, 4.0f);
    default: return t_frand(-2000.0f, 2000.0f);
    }
}

/* volatile: the routines see run-time values (a constant operand takes SH4ZAM's own compile-time path) */
static volatile float vx, vy;

int main(void) {
    int trial;
    for (trial = 0; trial < 400000; trial++) {
        float a[4], b[4];
        int k;
        for (k = 0; k < 4; k++) { a[k] = pick(); b[k] = trial % 5 == 0 ? a[k] : pick(); }
        if (trial % 9 == 0) { a[0] = a[1] = a[2] = a[3] = trial % 2 ? -0.0f : 0.0f; }
        {
            const shz_vec4_t u = shz_vec4_init(a[0], a[1], a[2], a[3]), v = shz_vec4_init(b[0], b[1], b[2], b[3]);
            float x, y;
            vx = a[0]; vy = b[0]; x = vx; y = vy;
            if (x >= 0.0f) {
                const float r = shz_dce_sqrtf(x), i = shz_dce_inv_sqrtf(x);
                T_CHECK(same(r, shz_sqrtf(x)), "sqrtf(%a): %a, SH4ZAM %a", (double)x, (double)r, (double)shz_sqrtf(x));
                T_CHECK(same(i, shz_inv_sqrtf(x)), "inv_sqrtf(%a): %a, SH4ZAM %a", (double)x, (double)i, (double)shz_inv_sqrtf(x));
                if (x == 0.0f) T_CHECK(r == 0.0f && i == 0.0f, "a zero must give zero: %a %a", (double)r, (double)i);
                else {
                    T_CHECK(fabs(r - sqrt((double)x)) <= sqrt((double)x) * REL, "sqrtf(%g) = %g", (double)x, (double)r);
                    T_CHECK(fabs(i - 1.0 / sqrt((double)x)) <= REL / sqrt((double)x), "inv_sqrtf(%g) = %g", (double)x, (double)i);
                }
            }
            T_CHECK(same(shz_dce_hypotf(x, y), shz_hypotf(x, y)), "hypotf(%a, %a)", (double)x, (double)y);
            T_CHECK(same(shz_dce_vec2_magnitude(u.xy), shz_vec2_magnitude(u.xy)), "vec2 magnitude");
            T_CHECK(same(shz_dce_vec3_magnitude(u.xyz), shz_vec3_magnitude(u.xyz)), "vec3 magnitude");
            T_CHECK(same(shz_dce_vec4_magnitude(u), shz_vec4_magnitude(u)), "vec4 magnitude");
            T_CHECK(same(shz_dce_vec2_magnitude_inv(u.xy), shz_vec2_magnitude_inv(u.xy)), "vec2 magnitude_inv");
            T_CHECK(same(shz_dce_vec3_magnitude_inv(u.xyz), shz_vec3_magnitude_inv(u.xyz)), "vec3 magnitude_inv");
            T_CHECK(same(shz_dce_vec4_magnitude_inv(u), shz_vec4_magnitude_inv(u)), "vec4 magnitude_inv");
            T_CHECK(same(shz_dce_vec2_distance(u.xy, v.xy), shz_vec2_distance(u.xy, v.xy)), "vec2 distance");
            T_CHECK(same(shz_dce_vec3_distance(u.xyz, v.xyz), shz_vec3_distance(u.xyz, v.xyz)), "vec3 distance");
            T_CHECK(same(shz_dce_vec4_distance(u, v), shz_vec4_distance(u, v)), "vec4 distance");
            {
                const shz_vec2_t n2 = shz_dce_vec2_normalize_safe(u.xy), o2 = shz_vec2_normalize_safe(u.xy);
                const shz_vec3_t n3 = shz_dce_vec3_normalize_safe(u.xyz), o3 = shz_vec3_normalize_safe(u.xyz);
                const shz_vec4_t n4 = shz_dce_vec4_normalize_safe(u), o4 = shz_vec4_normalize_safe(u);
                const double m3 = (double)a[0] * a[0] + (double)a[1] * a[1] + (double)a[2] * a[2];
                T_CHECK(same(n2.x, o2.x) && same(n2.y, o2.y), "vec2 normalize_safe");
                T_CHECK(same(n3.x, o3.x) && same(n3.y, o3.y) && same(n3.z, o3.z), "vec3 normalize_safe");
                T_CHECK(same(n4.x, o4.x) && same(n4.y, o4.y) && same(n4.z, o4.z) && same(n4.w, o4.w), "vec4 normalize_safe");
                if (m3 > 1e-30 && m3 < 1e30) {
                    const double l2 = (double)n3.x * n3.x + (double)n3.y * n3.y + (double)n3.z * n3.z;
                    T_CHECK(fabs(l2 - 1.0) <= 8.0 * REL, "vec3 normalize_safe: squared length %.9g", l2);
                    T_CHECK(fabs(shz_dce_vec3_magnitude(u.xyz) - sqrt(m3)) <= sqrt(m3) * 4.0 * REL, "vec3 magnitude against the exact length");
                } else if (m3 == 0.0) {
                    T_CHECK(n3.x == 0.0f && n3.y == 0.0f && n3.z == 0.0f && shz_dce_vec3_magnitude(u.xyz) == 0.0f && shz_dce_vec3_magnitude_inv(u.xyz) == 0.0f,
                            "the zero vector must give zeros");
                }
            }
        }
    }
    printf("sqrt: every rework == SH4ZAM's routine; %d zero results differed in sign only (0 in a strict build)\n", zero_signs);
    return t_done("sqrt_test");
}
