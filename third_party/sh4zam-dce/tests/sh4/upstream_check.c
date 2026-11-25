/* SH4ZAM-DCE tests: doc/upstream/shz_asin_acos.h as the SH-4 runs it

   upstream_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   upstream_check <upstream_kernels.elf>
   The ELF is upstream_kernels.c built by sh-elf-gcc; tests/sh4run executes it instruction by instruction (FSRRA with the console's
   own bits when the bit-exact model is present).
   - shz_asinf_quartic and shz_acosf_quartic against the true angle (the host's asin / acos in double), inside the file's bound, over
     a sweep of [-1, 1] that is dense near 0 and near +-1
   - the special cases: a zero of either sign, the ends, arguments past +-1, a NaN, the sign and the odd symmetry of the arcsine
   - SH4ZAM's routines on the same input: their worst error is printed beside it
   - every call twice, the scratch registers seeded differently: the results must not change
*/

#include <sh4zam/shz_sh4zam.h>

#include "upstream_jobs.h"
#include "sh4_check.h"

#define BOUND 8e-6
#define PI_D 3.14159265358979323846
#define N 4096u

static double worst_as, worst_ac, worst_os, worst_oc;
static unsigned differ, fast_build;   /* arguments where SH4ZAM's routines and the file's differ in some bit; how the kernels were built */

static void run(int kind) {
    const uint32_t aj = ram(sizeof(job_upstream_t), 4, 0), ai = ram(N * 4, 8, 4);
    const uint32_t a1 = ram(N * 4 + 4, 8, 4), a2 = ram(N * 4 + 4, 8, 4), a3 = ram(N * 4 + 4, 8, 4), a4 = ram(N * 4 + 4, 8, 4);
    job_upstream_t *j = (job_upstream_t *)host(aj, sizeof *j);
    float *in = (float *)host(ai, N * 4), *as = (float *)host(a1, N * 4 + 4), *ac = (float *)host(a2, N * 4 + 4);
    float *os = (float *)host(a3, N * 4 + 4), *oc = (float *)host(a4, N * 4 + 4);
    static float keep[2 * N];
    uint32_t k;
    for (k = 0; k < N; k++) {
        switch (kind) {
        case 0: in[k] = -1.0f + 2.0f * (float)k / (float)(N - 1u); break;                               /* even steps over [-1, 1] */
        case 1: in[k] = t_frand(-1.0f, 1.0f); break;
        case 2: in[k] = (k & 1u ? -1.0f : 1.0f) * (1.0f - t_float(0x33800000u + (t_rand() % 0x0c000000u))); break;   /* 1 - (2^-24 .. 1): dense near +-1 */
        case 3: in[k] = (k & 1u ? -1.0f : 1.0f) * t_float(0x1e000000u + (t_rand() % 0x21800000u)); break;           /* 1e-20 .. 1: dense near 0 */
        default: in[k] = (k & 1u ? -1.0f : 1.0f) * (1.0f + t_frand(0.0f, 1.0f) * (k % 3u ? 1e-6f : 3.0f)); break;   /* at and past +-1 */
        }
    }
    if (kind == 0) {
        in[0] = -1.0f; in[N - 1u] = 1.0f; in[1] = 0.0f; in[2] = -0.0f; in[3] = t_float(0x3f7fffffu); in[4] = -in[3]; in[5] = 1e-30f; in[6] = -1e-30f;
        in[7] = t_float(0x7fc00000u);   /* a NaN */
    }
    j->in = ai; j->count = N; j->asin_new = a1; j->acos_new = a2; j->asin_shz = a3; j->acos_shz = a4;
    as[N] = ac[N] = t_float(0xeeeeeeeeu);
    call("_t_upstream", aj);
    T_CHECK(t_bits(as[N]) == 0xeeeeeeeeu && t_bits(ac[N]) == 0xeeeeeeeeu, "a write past the last result");
    memcpy(keep, as, N * 4); memcpy(keep + N, ac, N * 4);
    call("_t_upstream", aj);   /* again, other garbage in the scratch registers */
    T_CHECK(!memcmp(keep, as, N * 4) && !memcmp(keep + N, ac, N * 4), "a second run gave other results (a register read before it was written)");
    for (k = 0; k < N; k++) {
        const double x = in[k], c = x > 1.0 ? 1.0 : x < -1.0 ? -1.0 : x, es = fabs(as[k] - asin(c)), ec = fabs(ac[k] - acos(c));
        if (in[k] != in[k]) {   /* a NaN gives a NaN, in a build that has not told the compiler there are none */
            if (!j->fast) T_CHECK(as[k] != as[k] && ac[k] != ac[k], "a NaN gave %08x %08x", (unsigned)t_bits(as[k]), (unsigned)t_bits(ac[k]));
            continue;
        }
        fast_build = j->fast;
        if (t_bits(as[k]) != t_bits(os[k]) || t_bits(ac[k]) != t_bits(oc[k])) differ++;
        T_CHECK(es <= BOUND, "asinf(%.9g) = %.9g, true %.9g, off by %.3g", x, (double)as[k], asin(c), es);
        T_CHECK(ec <= BOUND, "acosf(%.9g) = %.9g, true %.9g, off by %.3g", x, (double)ac[k], acos(c), ec);
        T_CHECK(as[k] == 0.0f || (as[k] < 0.0f) == (in[k] < 0.0f), "asinf(%.9g) = %.9g has the wrong sign", x, (double)as[k]);
        T_CHECK(ac[k] >= 0.0f && ac[k] <= (float)PI_D, "acosf(%.9g) = %.9g leaves [0, pi]", x, (double)ac[k]);
        if (es > worst_as) worst_as = es;
        if (ec > worst_ac) worst_ac = ec;
        if (fabs(x) <= 1.0) {   /* SH4ZAM's routines, where they are defined */
            const double eos = fabs(os[k] - asin(x)), eoc = fabs(oc[k] - acos(x));
            if (eos == eos && eos > worst_os) worst_os = eos;
            if (eoc == eoc && eoc > worst_oc) worst_oc = eoc;
        } else {
            T_CHECK(t_bits(as[k]) == t_bits(x > 0.0 ? (float)(PI_D / 2) : -(float)(PI_D / 2)), "asinf past the end: %.9g", (double)as[k]);
            T_CHECK(t_bits(ac[k]) == t_bits(x > 0.0 ? 0.0f : (float)PI_D), "acosf past the end: %.9g", (double)ac[k]);
        }
    }
    if (kind == 0) {
        T_CHECK(t_bits(as[1]) == 0u && t_bits(as[2]) == 0u, "asinf of a zero of either sign must be +0: %08x %08x", (unsigned)t_bits(as[1]), (unsigned)t_bits(as[2]));
        T_CHECK(t_bits(ac[N - 1u]) == 0u && t_bits(ac[0]) == t_bits((float)PI_D), "acosf(1) = 0, acosf(-1) = pi");
        T_CHECK(t_bits(as[N - 1u]) == t_bits((float)(PI_D / 2)) && t_bits(as[0]) == t_bits(-(float)(PI_D / 2)), "asinf(+-1) = +-pi/2");
        for (k = 8; k < N / 2u; k++)   /* the even sweep is symmetric: asin is odd bit for bit */
            T_CHECK(t_bits(as[k]) == (t_bits(as[N - 1u - k]) ^ 0x80000000u) || (as[k] == 0.0f && as[N - 1u - k] == 0.0f) || in[k] != -in[N - 1u - k], "asinf is not odd at %.9g", (double)in[k]);
    }
    sh4_alloc_reset(S);
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    int kind, rep;
    if (rc) return rc;
    for (kind = 0; kind < 5; kind++)
        for (rep = 0; rep < (kind == 0 ? 1 : 6); rep++) run(kind);
    printf("worst error against the true angle, rad: asinf %.3g (SH4ZAM %.3g), acosf %.3g (SH4ZAM %.3g); bound %.3g\n", worst_as, worst_os, worst_ac, worst_oc, BOUND);
    printf("SH4ZAM ff39547's shz_asinf / shz_acosf against the file's routines on the SH-4: %u arguments differ in some bit (%s build)\n", differ, fast_build ? "-ffast-math" : "strict");
    T_CHECK(fast_build || differ == 0u, "SH4ZAM ff39547 claim: shz_asinf / shz_acosf are the file's (cff97e8, #92), bit for bit in a strict build");
    T_CHECK(worst_os == worst_as && worst_oc == worst_ac, "SH4ZAM ff39547 claim: shz_asinf / shz_acosf are the file's (cff97e8, #92): the same worst errors on the SH-4");
    return t_done("upstream_check");
}
