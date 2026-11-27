/* SH4ZAM-DCE tests: shz_dce_pvr.h on the host (SH4ZAM's software back end)

   pvr_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked:
   - the screen point is, bit for bit, the plain loop's (shz_xmtrx_transform_vec4, shz_invf_fsrra, one product each), and within the
     bound of the exact quotient
   - the five carried words arrive untouched (any bit pattern), nothing is written past the last record, count 0 writes nothing
   - the dispatch and the C form give the same bytes
*/

#include <sh4zam_dce/shz_dce_pvr.h>

#include "shz_dce_test.h"

#define BOUND 4.76837158203125e-07   /* 2^-21 */
#define N 67

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    static float in[N * 8], out[(N + 1) * 8 + 8], out2[(N + 1) * 8 + 8];   /* out is deliberately used off a multiple of 32 */
    static shz_mat4x4_t m;
    int trial;

    for (trial = 0; trial < 600; trial++) {
        const size_t count = (size_t)(trial % (N + 1));
        float *o = out + 1, *o2 = out2 + 1;   /* a multiple of 4, not of 8 */
        size_t k;
        int c;

        for (c = 0; c < 4; c++) {
            m.elem[4 * c + 0] = c == 3 ? t_frand(-2000, 2000) : t_frand(-300, 300);
            m.elem[4 * c + 1] = c == 3 ? t_frand(-2000, 2000) : t_frand(-300, 300);
            m.elem[4 * c + 2] = t_frand(-1, 1);
            m.elem[4 * c + 3] = c == 3 ? t_frand(8, 40) : t_frand(-1, 1);
        }
        for (k = 0; k < count * 8; k++) in[k] = (k % 8 >= 1 && k % 8 <= 3) ? t_frand(-2, 2) : t_float(t_rand());
        shz_xmtrx_load_4x4(&m);
        memset(out, 0x5a, sizeof out); memset(out2, 0x5a, sizeof out2);
        shz_dce_pvr_project(o, in, count);
        shz_dce_pvr_project_c(o2, in, count);
        T_CHECK(!memcmp(out, out2, sizeof out), "count %u: the dispatch and the C form differ", (unsigned)count);
        for (k = 0; k < count; k++) {
            const float *s = in + 8 * k, *p = o + 8 * k;
            const shz_vec4_t want = shz_xmtrx_transform_vec4(shz_vec4_init(s[1], s[2], s[3], 1.0f));
            const float inv = shz_invf_fsrra(want.w), sx = want.x * inv, sy = want.y * inv;
            double e[4], pm[4], relw;
            int r;

            T_CHECK(t_bits(p[1]) == t_bits(sx) && t_bits(p[2]) == t_bits(sy) && t_bits(p[3]) == t_bits(inv), "record %u: not the plain loop's bits", (unsigned)k);
            T_CHECK(!memcmp(p, s, 4) && !memcmp(p + 4, s + 4, 16), "record %u: the five words were not carried through", (unsigned)k);
            for (r = 0; r < 4; r++) {
                e[r] = (double)m.elem[12 + r]; pm[r] = fabs((double)m.elem[12 + r]);
                for (c = 0; c < 3; c++) { const double t = (double)m.elem[4 * c + r] * (double)s[1 + c]; e[r] += t; if (fabs(t) > pm[r]) pm[r] = fabs(t); }
            }
            relw = (pm[3] + fabs(e[3])) / fabs(e[3]) * BOUND + 4.0 * BOUND;
            T_CHECK(e[3] > 1.0, "record %u: the test's own W is %.9g", (unsigned)k, e[3]);
            for (r = 0; r < 2; r++) {
                const double q = e[r] / e[3], tol = 2.0 * ((pm[r] + fabs(e[r])) / e[3] * BOUND + fabs(q) * relw);
                T_CHECK(fabs((double)p[1 + r] - q) <= tol, "record %u axis %d: %.9g, exact %.9g", (unsigned)k, r, (double)p[1 + r], q);
            }
            T_CHECK(fabs((double)p[3] - 1.0 / e[3]) <= 2.0 * relw / e[3], "record %u: 1 / W %.9g, exact %.9g", (unsigned)k, (double)p[3], 1.0 / e[3]);
        }
        T_CHECK(t_bits(o[8 * count]) == 0x5a5a5a5au && t_bits(out[0]) == 0x5a5a5a5au, "count %u: written outside the records", (unsigned)count);
    }
    return t_done("pvr_test");
}
