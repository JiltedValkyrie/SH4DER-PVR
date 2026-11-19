/* SH4ZAM-DCE tests: shz_dce_planes.h on the host (SH4ZAM's software back end)

   planes_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked:
   - every distance against the exact inner product (in double), inside the bound the header states: (P + |R|) x 2^-21
   - an outcode bit is the sign of its distance; a zero of either sign leaves it clear (and a NaN, in a build that keeps NaNs)
   - the batch routines give the bits of the per-point and per-sphere routines, for every stride and count, odd counts included
   - against the EXACT side of each plane an outcode or a cull bit may differ only inside the bound: the header's "not bit-identical"
     is measured (the count is printed), and nothing outside the bound is ever misjudged
   - the six-plane sphere classes (behind, crossing, in front) and the cull batch
*/

#include <sh4zam_dce/shz_dce_planes.h>

#include "shz_dce_test.h"

#define BOUND 4.76837158203125e-07   /* 2^-21 */

static void random_plane(float pl[4], float reach) {
    float n[3], len;
    do {
        n[0] = t_frand(-1, 1); n[1] = t_frand(-1, 1); n[2] = t_frand(-1, 1);
        len = sqrtf(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
    } while (len < 0.1f);
    pl[0] = n[0] / len; pl[1] = n[1] / len; pl[2] = n[2] / len;
    pl[3] = t_frand(-reach, reach);
}

/* the exact distance, the largest of the four products (P) */
static double exact(const float pl[4], const float p[3], double *pmax) {
    const double t[4] = { (double)pl[0] * p[0], (double)pl[1] * p[1], (double)pl[2] * p[2], (double)pl[3] };
    int k;
    *pmax = 0.0;
    for (k = 0; k < 4; k++) if (fabs(t[k]) > *pmax) *pmax = fabs(t[k]);
    return t[0] + t[1] + t[2] + t[3];
}

int main(void) {
    static shz_dce_planes6_t six;        /* static: on a multiple of 8 (16 on the host) */
    static shz_dce_planes4_t four;
    static float pts[600 * 8];
    static uint8_t codes[600], ref[600];
    int trial, near_differs = 0, sphere_band = 0;

    /* -- four planes against a point -- */
    for (trial = 0; trial < 40000; trial++) {
        const float reach = trial % 3 == 0 ? 2.0f : trial % 3 == 1 ? 200.0f : 5000.0f;
        float pl[4][4], p[3];
        shz_vec4_t d;
        unsigned code, exact_code = 0u;
        int i, in_band = 0;
        for (i = 0; i < 4; i++) random_plane(pl[i], reach);
        p[0] = t_frand(-reach, reach); p[1] = t_frand(-reach, reach); p[2] = t_frand(-reach, reach);
        if (trial % 7 == 0) {   /* a point put on plane 1 as nearly as floats allow: the near-plane case */
            double pm;
            const double e = exact(pl[1], p, &pm);
            p[0] = (float)(p[0] - e * pl[1][0]); p[1] = (float)(p[1] - e * pl[1][1]); p[2] = (float)(p[2] - e * pl[1][2]);
        }
        shz_dce_planes4_set(&four, shz_vec4_init(pl[0][0], pl[0][1], pl[0][2], pl[0][3]), shz_vec4_init(pl[1][0], pl[1][1], pl[1][2], pl[1][3]),
                            shz_vec4_init(pl[2][0], pl[2][1], pl[2][2], pl[2][3]), shz_vec4_init(pl[3][0], pl[3][1], pl[3][2], pl[3][3]));
        shz_dce_planes4_load(&four);
        d = shz_dce_planes4_dist(shz_vec3_init(p[0], p[1], p[2]));
        code = shz_dce_planes4_outcode(shz_vec3_init(p[0], p[1], p[2]));
        for (i = 0; i < 4; i++) {
            double pm;
            const double e = exact(pl[i], p, &pm), bound = (pm + fabs(e)) * BOUND;
            const float got = i == 0 ? d.x : i == 1 ? d.y : i == 2 ? d.z : d.w;
            T_CHECK(fabs((double)got - e) <= bound, "plane %d distance %.9g, exact %.9g, bound %.3g", i, (double)got, e, bound);
            T_CHECK(((code >> i) & 1u) == (unsigned)(got < 0.0f), "outcode bit %d is not the sign of its distance %g", i, (double)got);
            if (e < 0.0) exact_code |= 1u << i;
            if (fabs(e) <= bound) in_band = 1;
        }
        if (code != exact_code) {
            near_differs++;
            T_CHECK(in_band, "outcode %x, exact %x, and no distance is inside the bound", code, exact_code);
        }
    }
    printf("planes4: %d of 40000 points got an outcode other than the exact one, all within the bound of a plane (\"not bit-identical\")\n", near_differs);

    /* -- zeros and NaN -- */
    T_CHECK(shz_dce_outcode4(shz_vec4_init(0.0f, -0.0f, -1.0f, 1.0f)) == 4u, "zeros of either sign must leave their bits clear");
    T_CHECK(shz_dce_outcode4(shz_vec4_init(-1e-30f, 1e-30f, -1e30f, -3.0f)) == 13u, "signs");
#if !defined(__FINITE_MATH_ONLY__) || !__FINITE_MATH_ONLY__
    T_CHECK(shz_dce_outcode4(shz_vec4_init(t_float(0x7fc00000u), t_float(0xffc00000u), -1.0f, 1.0f)) == 4u, "a NaN must leave its bit clear");
#endif

    /* -- the batch against the single routine: strides, counts (0, odd, even), ANY and ALL -- */
    {
        static const size_t strides[] = { 12, 16, 20, 32 };
        float pl[4][4];
        size_t si;
        int i;
        for (i = 0; i < 4; i++) random_plane(pl[i], 3.0f);
        shz_dce_planes4_set(&four, shz_vec4_init(pl[0][0], pl[0][1], pl[0][2], pl[0][3]), shz_vec4_init(pl[1][0], pl[1][1], pl[1][2], pl[1][3]),
                            shz_vec4_init(pl[2][0], pl[2][1], pl[2][2], pl[2][3]), shz_vec4_init(pl[3][0], pl[3][1], pl[3][2], pl[3][3]));
        shz_dce_planes4_load(&four);
        for (si = 0; si < sizeof strides / sizeof strides[0]; si++) {
            size_t count;
            for (count = 0; count <= 37; count++) {
                const size_t stride = strides[si];
                unsigned any = 0u, all = 0xfu, r;
                size_t k;
                for (k = 0; k < count * stride / 4; k++) pts[k] = t_frand(-6, 6);
                memset(codes, 0xee, sizeof codes);
                r = shz_dce_planes4_outcode_batch(codes, pts, stride, count);
                for (k = 0; k < count; k++) {
                    const float *p = (const float *)((const char *)pts + k * stride);
                    const unsigned c = shz_dce_planes4_outcode(shz_vec3_init(p[0], p[1], p[2]));
                    T_CHECK(codes[k] == c, "stride %u count %u: point %u code %x, single %x", (unsigned)stride, (unsigned)count, (unsigned)k, codes[k], c);
                    any |= c; all &= c;
                }
                T_CHECK(codes[count] == 0xee, "the batch wrote past its last code");
                T_CHECK(r == (count ? (any | (all << 8)) : 0u), "stride %u count %u: returned %x", (unsigned)stride, (unsigned)count, r);
                T_CHECK(SHZ_DCE_OUTCODE_ANY(r) == (count ? any : 0u) && SHZ_DCE_OUTCODE_ALL(r) == (count ? all : 0u), "ANY / ALL macros");
            }
        }
    }

    /* -- a sphere against six planes -- */
    for (trial = 0; trial < 30000; trial++) {
        const float reach = trial % 2 ? 50.0f : 4.0f;
        shz_vec4_t pv[6];
        float pl[6][4], c[3], radius, dist[6];
        unsigned r, out_exact = 0u, cross_exact = 0u, unsure = 0u;
        int i;
        for (i = 0; i < 6; i++) { random_plane(pl[i], reach); pv[i] = shz_vec4_init(pl[i][0], pl[i][1], pl[i][2], pl[i][3]); }
        c[0] = t_frand(-reach, reach); c[1] = t_frand(-reach, reach); c[2] = t_frand(-reach, reach);
        radius = t_frand(0.0f, reach * 0.5f);
        if (trial % 5 == 0) {   /* a sphere touching plane 2 as nearly as floats allow */
            double pm;
            radius = (float)fabs(exact(pl[2], c, &pm));
        }
        shz_dce_planes6_set(&six, pv);
        shz_dce_planes6_load(&six);
        shz_dce_planes6_dist(&six, shz_vec3_init(c[0], c[1], c[2]), dist);
        r = shz_dce_planes6_sphere(&six, shz_vec3_init(c[0], c[1], c[2]), radius);
        for (i = 0; i < 6; i++) {
            double pm;
            const double e = exact(pl[i], c, &pm), bound = (pm + fabs(e)) * BOUND;
            T_CHECK(fabs((double)dist[i] - e) <= bound, "plane %d distance %.9g, exact %.9g", i, (double)dist[i], e);
            if (e < -(double)radius) out_exact |= 1u << i;
            else if (e <= (double)radius) cross_exact |= 1u << i;
            if (fabs(e + radius) <= bound || fabs(e - radius) <= bound) unsure |= 1u << i;
        }
        T_CHECK(!(SHZ_DCE_SPHERE_OUT(r) & SHZ_DCE_SPHERE_CROSS(r)), "a plane cannot be both behind and crossed");
        T_CHECK(!((SHZ_DCE_SPHERE_OUT(r) ^ out_exact) & ~unsure), "sphere OUT %02x, exact %02x, outside the bound", SHZ_DCE_SPHERE_OUT(r), out_exact);
        T_CHECK(!((SHZ_DCE_SPHERE_CROSS(r) ^ cross_exact) & ~unsure), "sphere CROSS %02x, exact %02x, outside the bound", SHZ_DCE_SPHERE_CROSS(r), cross_exact);
        if (unsure) sphere_band++;
    }
    printf("planes6: %d of 30000 spheres were inside the bound of touching a plane\n", sphere_band);

    /* -- the cull batch against the single routine -- */
    {
        static const size_t strides[] = { 16, 20, 32 };
        shz_vec4_t pv[6];
        float pl[4];
        size_t si;
        int i;
        for (i = 0; i < 6; i++) { random_plane(pl, 3.0f); pv[i] = shz_vec4_init(pl[0], pl[1], pl[2], pl[3]); }
        shz_dce_planes6_set(&six, pv);
        shz_dce_planes6_load(&six);
        for (si = 0; si < sizeof strides / sizeof strides[0]; si++) {
            size_t count;
            for (count = 0; count <= 33; count++) {
                const size_t stride = strides[si];
                size_t k;
                for (k = 0; k < count * stride / 4; k++) pts[k] = t_frand(-8, 8);
                for (k = 0; k < count; k++) ((float *)((char *)pts + k * stride))[3] = t_frand(0.0f, 3.0f);
                memset(codes, 0xee, sizeof codes);
                shz_dce_planes6_cull_batch(codes, &six, pts, stride, count);
                for (k = 0; k < count; k++) {
                    const float *s = (const float *)((const char *)pts + k * stride);
                    ref[k] = (uint8_t)SHZ_DCE_SPHERE_OUT(shz_dce_planes6_sphere(&six, shz_vec3_init(s[0], s[1], s[2]), s[3]));
                    T_CHECK(codes[k] == ref[k], "cull stride %u count %u: sphere %u mask %02x, single %02x", (unsigned)stride, (unsigned)count, (unsigned)k, codes[k], ref[k]);
                }
                T_CHECK(codes[count] == 0xee, "the cull batch wrote past its last mask");
            }
        }
    }

    return t_done("planes_test");
}
