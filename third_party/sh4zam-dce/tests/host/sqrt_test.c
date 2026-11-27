/* SH4ZAM-DCE tests: shz_dce_sqrt.h on the host (SH4ZAM's software back end)

   sqrt_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Pinned to SH4ZAM master ff39547 (2026-11-22). What is checked:
   - every wrapper against SH4ZAM's routine of the same name on the same input: the same bits (each from its own function that is
     never inlined), over zeros of both signs, denormals, squares that underflow and overflow, negatives and ordinary numbers
   - SH4ZAM's roots and lengths against the exact value within a loose bound; zeros give zero
   - SH4ZAM ff39547 claim: shz_hypotf does not overflow for inputs near the largest float (it scales above 2^62)
   The same on the SH-4 is tests/sh4/sqrt_check.c; tests/host/wrap_test.c checks every wrapper of the layer once more.
*/

#include <sh4zam_dce/shz_dce_sqrt.h>

#include "shz_dce_test.h"

#define REL 9.5367431640625e-07   /* 2^-20 */
#define NOINLINE __attribute__((noinline))

static NOINLINE float d_sqrtf(float x) { return shz_dce_sqrtf(x); }
static NOINLINE float s_sqrtf(float x) { return shz_sqrtf(x); }
static NOINLINE float d_inv_sqrtf(float x) { return shz_dce_inv_sqrtf(x); }
static NOINLINE float s_inv_sqrtf(float x) { return shz_inv_sqrtf(x); }
static NOINLINE float d_hypotf(float x, float y) { return shz_dce_hypotf(x, y); }
static NOINLINE float s_hypotf(float x, float y) { return shz_hypotf(x, y); }

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

#define SAME(a, b, what) T_CHECK(t_bits(a) == t_bits(b), "wrapper: %s: %08x, SH4ZAM's %08x", what, (unsigned)t_bits(a), (unsigned)t_bits(b))

/* volatile: the routines see run-time values (a constant operand takes SH4ZAM's own compile-time path) */
static volatile float vx, vy;

int main(int argc, char **argv) {
    (void)argc; (void)argv;
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
            SAME(d_sqrtf(x), s_sqrtf(x), "sqrtf");
            SAME(d_inv_sqrtf(x), s_inv_sqrtf(x), "inv_sqrtf");
            SAME(d_hypotf(x, y), s_hypotf(x, y), "hypotf");
            if (x == 0.0f) T_CHECK(s_sqrtf(x) == 0.0f && s_inv_sqrtf(x) == 0.0f, "a zero must give zero");
            else if (x > 0.0f) {
                T_CHECK(fabs(s_sqrtf(x) - sqrt((double)x)) <= sqrt((double)x) * REL, "sqrtf(%g) = %g", (double)x, (double)s_sqrtf(x));
                T_CHECK(fabs(s_inv_sqrtf(x) - 1.0 / sqrt((double)x)) <= REL / sqrt((double)x), "inv_sqrtf(%g) = %g", (double)x, (double)s_inv_sqrtf(x));
            }
            {
                const double h = sqrt((double)x * x + (double)y * y);
                if (h > 1e-15 && h < 3.4e38) T_CHECK(fabs(s_hypotf(x, y) - h) <= h * 2.0 * REL, "SH4ZAM ff39547 claim: hypotf(%a, %a) = %g, exact %g", (double)x, (double)y, (double)s_hypotf(x, y), h);
            }
            SAME(shz_dce_vec2_magnitude(u.xy), shz_vec2_magnitude(u.xy), "vec2 magnitude");
            SAME(shz_dce_vec3_magnitude(u.xyz), shz_vec3_magnitude(u.xyz), "vec3 magnitude");
            SAME(shz_dce_vec4_magnitude(u), shz_vec4_magnitude(u), "vec4 magnitude");
            SAME(shz_dce_vec2_magnitude_inv(u.xy), shz_vec2_magnitude_inv(u.xy), "vec2 magnitude_inv");
            SAME(shz_dce_vec3_magnitude_inv(u.xyz), shz_vec3_magnitude_inv(u.xyz), "vec3 magnitude_inv");
            SAME(shz_dce_vec4_magnitude_inv(u), shz_vec4_magnitude_inv(u), "vec4 magnitude_inv");
            SAME(shz_dce_vec2_distance(u.xy, v.xy), shz_vec2_distance(u.xy, v.xy), "vec2 distance");
            SAME(shz_dce_vec3_distance(u.xyz, v.xyz), shz_vec3_distance(u.xyz, v.xyz), "vec3 distance");
            SAME(shz_dce_vec4_distance(u, v), shz_vec4_distance(u, v), "vec4 distance");
            {
                const shz_vec2_t n2 = shz_dce_vec2_normalize_safe(u.xy), o2 = shz_vec2_normalize_safe(u.xy);
                const shz_vec3_t n3 = shz_dce_vec3_normalize_safe(u.xyz), o3 = shz_vec3_normalize_safe(u.xyz);
                const shz_vec4_t n4 = shz_dce_vec4_normalize_safe(u), o4 = shz_vec4_normalize_safe(u);
                const double m3 = (double)a[0] * a[0] + (double)a[1] * a[1] + (double)a[2] * a[2];
                T_CHECK(!memcmp(&n2, &o2, sizeof n2), "wrapper: vec2 normalize_safe");
                T_CHECK(!memcmp(&n3, &o3, sizeof n3), "wrapper: vec3 normalize_safe");
                T_CHECK(!memcmp(&n4, &o4, sizeof n4), "wrapper: vec4 normalize_safe");
                if (m3 > 1e-30 && m3 < 1e30) {
                    const double l2 = (double)o3.x * o3.x + (double)o3.y * o3.y + (double)o3.z * o3.z;
                    T_CHECK(fabs(l2 - 1.0) <= 8.0 * REL, "vec3 normalize_safe: squared length %.9g", l2);
                    T_CHECK(fabs(shz_vec3_magnitude(u.xyz) - sqrt(m3)) <= sqrt(m3) * 4.0 * REL, "vec3 magnitude against the exact length");
                } else if (m3 == 0.0) {
                    T_CHECK(o3.x == 0.0f && o3.y == 0.0f && o3.z == 0.0f && shz_vec3_magnitude(u.xyz) == 0.0f && shz_vec3_magnitude_inv(u.xyz) == 0.0f,
                            "the zero vector must give zeros");
                }
            }
        }
    }
    printf("sqrt: every wrapper == SH4ZAM's routine bit for bit, 400000 inputs (SH4ZAM ff39547)\n");
    return t_done("sqrt_test");
}
