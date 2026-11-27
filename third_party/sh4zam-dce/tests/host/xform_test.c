/* SH4ZAM-DCE tests: shz_dce_xform.h on the host (SH4ZAM's software back end)

   xform_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked:
   - the positions are, bit for bit, shz_xmtrx_transform_point3 of the input (the plain loop), and within the bound of the exact product
   - the two carried words arrive untouched (any bit pattern), bytes 20..31 of a record are not written, nothing past the last record
   - every clip code is the compares written out on the position's own bits, in the three modes; the bit values are the documented ones
   - the returned value is the OR of the codes; count 0 writes nothing
*/

#include <sh4zam_dce/shz_dce_xform.h>

#include "shz_dce_test.h"

#define BOUND 4.76837158203125e-07   /* 2^-21 */
#define N 67

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    static float in[N * 8], out[(N + 1) * 8 + 8];   /* out is deliberately used off a multiple of 32: no line allocation on the host anyway */
    static uint8_t codes[N + 4];
    static shz_mat4x4_t m;
    shz_dce_clip_t clip;
    int trial, mode;

    T_CHECK(SHZ_DCE_CLIP_NEAR == 1u && SHZ_DCE_CLIP_FAR == 2u && SHZ_DCE_CLIP_LEFT == 4u && SHZ_DCE_CLIP_RIGHT == 8u && SHZ_DCE_CLIP_BOTTOM == 16u &&
            SHZ_DCE_CLIP_TOP == 32u && SHZ_DCE_CLIP_GUARD == 64u, "clip bit values");
    clip.kx = 1.25f; clip.ky = 0.75f; clip.gkx = 2.5f; clip.gky = 1.5f; clip.near_z = 0.5f; clip.far_z = 100.0f;
    /* one point per bit, by hand */
    T_CHECK(shz_dce_clip_full(0.0f, 0.0f, 1.0f, &clip) == 0u, "a point on the axis is inside");
    T_CHECK(shz_dce_clip_full(0.0f, 0.0f, 0.25f, &clip) == SHZ_DCE_CLIP_NEAR, "near");
    T_CHECK(shz_dce_clip_full(0.0f, 0.0f, 200.0f, &clip) == SHZ_DCE_CLIP_FAR, "far");
    T_CHECK(shz_dce_clip_full(-2.0f, 0.0f, 1.0f, &clip) == SHZ_DCE_CLIP_LEFT, "left (inside the guard band)");
    T_CHECK(shz_dce_clip_full(2.0f, 0.0f, 1.0f, &clip) == SHZ_DCE_CLIP_RIGHT, "right");
    T_CHECK(shz_dce_clip_full(0.0f, -1.0f, 1.0f, &clip) == SHZ_DCE_CLIP_BOTTOM, "bottom");
    T_CHECK(shz_dce_clip_full(0.0f, 1.0f, 1.0f, &clip) == SHZ_DCE_CLIP_TOP, "top");
    T_CHECK(shz_dce_clip_full(3.0f, 0.0f, 1.0f, &clip) == (SHZ_DCE_CLIP_RIGHT | SHZ_DCE_CLIP_GUARD), "right and past the guard band");
    T_CHECK(shz_dce_clip_full(0.0f, -2.0f, 1.0f, &clip) == (SHZ_DCE_CLIP_BOTTOM | SHZ_DCE_CLIP_GUARD), "bottom and past the guard band");
    T_CHECK(shz_dce_clip_sides(3.0f, -2.0f, 1.0f, clip.kx, clip.ky) == (SHZ_DCE_CLIP_RIGHT | SHZ_DCE_CLIP_BOTTOM), "the side bits alone");

    for (trial = 0; trial < 400; trial++) {
        const size_t count = (size_t)(trial % (N + 1));
        size_t k;
        int c, r;
        for (c = 0; c < 4; c++)
            for (r = 0; r < 4; r++)
                m.elem[4 * c + r] = r == 3 ? (c == 3 ? 1.0f : 0.0f) : c == 3 ? t_frand(-20, 20) : t_frand(-1, 1);
        clip.kx = t_frand(0.5f, 1.5f); clip.ky = t_frand(0.5f, 1.5f); clip.gkx = 2.0f * clip.kx; clip.gky = 2.0f * clip.ky;
        clip.near_z = 0.0625f; clip.far_z = 30.0f;
        for (k = 0; k < count * 8; k++) in[k] = k % 8 < 3 ? t_frand(-25, 25) : t_float(t_rand());
        shz_xmtrx_load_4x4(&m);
        for (mode = 0; mode <= 2; mode++) {
            float *o = out + 1;   /* a multiple of 4, not of 8 */
            unsigned any = 0u, ret;
            memset(out, 0x5a, sizeof out); memset(codes, 0xee, sizeof codes);
            ret = shz_dce_xform_vertex32(o, codes, in, count, mode, &clip);
            for (k = 0; k < count; k++) {
                const float *s = in + 8 * k, *p = o + 8 * k;
                const shz_vec3_t want = shz_xmtrx_transform_point3(shz_vec3_init(s[0], s[1], s[2]));
                const unsigned code = mode == SHZ_DCE_XFORM_NONE ? 0u : mode == SHZ_DCE_XFORM_SIDES ? shz_dce_clip_sides(p[0], p[1], p[2], clip.kx, clip.ky)
                                                                                                      : shz_dce_clip_full(p[0], p[1], p[2], &clip);
                T_CHECK(!memcmp(p, &want, 12), "record %u: not the plain transform's bits", (unsigned)k);
                T_CHECK(!memcmp(p + 3, s + 3, 8), "record %u: the two words were not carried through", (unsigned)k);
                T_CHECK(t_bits(p[5]) == 0x5a5a5a5au && t_bits(p[7]) == 0x5a5a5a5au, "record %u: bytes 20..31 were written", (unsigned)k);
                T_CHECK(codes[k] == code, "record %u mode %d: code %02x, the compares give %02x", (unsigned)k, mode, codes[k], code);
                for (r = 0; r < 3; r++) {
                    double e = (double)m.elem[12 + r], pm = fabs((double)m.elem[12 + r]);
                    for (c = 0; c < 3; c++) { const double t = (double)m.elem[4 * c + r] * (double)s[c]; e += t; if (fabs(t) > pm) pm = fabs(t); }
                    T_CHECK(fabs((double)p[r] - e) <= (pm + fabs(e)) * BOUND, "record %u axis %d: %.9g, exact %.9g", (unsigned)k, r, (double)p[r], e);
                }
                any |= code;
            }
            T_CHECK(ret == any, "returned %x, the OR of the codes is %x", ret, any);
            T_CHECK(codes[count] == 0xee, "a clip code written past the end");
            T_CHECK(t_bits(o[8 * count]) == 0x5a5a5a5au, "a record written past the end");
        }
    }
    return t_done("xform_test");
}
