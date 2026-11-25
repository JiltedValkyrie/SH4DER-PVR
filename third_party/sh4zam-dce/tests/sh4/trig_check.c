/* SH4ZAM-DCE tests: shz_dce_trig.h as the SH-4 runs it

   trig_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   trig_check <trig_kernels.elf>
   The ELF is trig_kernels.c built by sh-elf-gcc; tests/sh4run executes it instruction by instruction (FSRRA with the console's own
   bits when the bit-exact model is present).
   Pinned to SH4ZAM master ff39547 (2026-11-22): every "SH4ZAM ff39547 claim" below is a promise of that commit, so a later SH4ZAM
   change that breaks one fails here by name. What is checked:
   - the wrappers: shz_dce_asinf / shz_dce_acosf give shz_asinf / shz_acosf bit for bit, every element
   - SH4ZAM's routines against the true angle (the host's asin / acos in double), inside SHZ_ASINF_ERROR_MAX (8e-6 rad), over a sweep
     of [-1, 1] that is dense near 0 and near +-1, and past +-1
   - their special cases: 0, the ends, arguments past +-1, the sign and the odd symmetry of the arcsine
   - every call twice, the scratch registers seeded differently: the results must not change
*/

#include <sh4zam_dce/shz_dce_trig.h>

#include "trig_jobs.h"
#include "sh4_check.h"

#define PI_D 3.14159265358979323846
#define N 4096u

static double worst_as, worst_ac;
static unsigned built, elements;

static void run(int kind) {
    const uint32_t aj = ram(sizeof(job_trig_t), 4, 0), ai = ram(N * 4, 8, 4);
    const uint32_t a1 = ram(N * 4 + 4, 8, 4), a2 = ram(N * 4 + 4, 8, 4), a3 = ram(N * 4 + 4, 8, 4), a4 = ram(N * 4 + 4, 8, 4);
    job_trig_t *j = (job_trig_t *)host(aj, sizeof *j);
    float *in = (float *)host(ai, N * 4);
    float *ds = (float *)host(a1, N * 4 + 4), *dc = (float *)host(a2, N * 4 + 4), *s = (float *)host(a3, N * 4 + 4), *c = (float *)host(a4, N * 4 + 4);
    static float keep[4][N];
    uint32_t k;
    ds[N] = dc[N] = s[N] = c[N] = t_float(0xeeeeeeeeu);
    for (k = 0; k < N; k++) {
        switch (kind) {
        case 0: in[k] = -1.0f + 2.0f * (float)k / (float)(N - 1u); break;                               /* even steps over [-1, 1] */
        case 1: in[k] = t_frand(-1.0f, 1.0f); break;
        case 2: in[k] = (k & 1u ? -1.0f : 1.0f) * (1.0f - t_float(0x33800000u + (t_rand() % 0x0c000000u))); break;   /* 1 - (2^-24 .. 1): dense near +-1 */
        case 3: in[k] = (k & 1u ? -1.0f : 1.0f) * t_float(0x1e000000u + (t_rand() % 0x21800000u)); break;           /* 1e-20 .. 1: dense near 0 */
        default: in[k] = (k & 1u ? -1.0f : 1.0f) * (1.0f + t_frand(0.0f, 1.0f) * (k % 3u ? 1e-6f : 3.0f)); break;   /* at and past +-1 */
        }
    }
    if (kind == 0) { in[0] = -1.0f; in[N - 1u] = 1.0f; in[1] = 0.0f; in[2] = -0.0f; in[3] = t_float(0x3f7fffffu); in[4] = -in[3]; in[5] = 1e-30f; in[6] = -1e-30f; }
    j->in = ai; j->count = N; j->asin_dce = a1; j->acos_dce = a2; j->asin_shz = a3; j->acos_shz = a4;
    call("_t_trig", aj);
    built = j->built;
    T_CHECK(t_bits(ds[N]) == 0xeeeeeeeeu && t_bits(dc[N]) == 0xeeeeeeeeu && t_bits(s[N]) == 0xeeeeeeeeu && t_bits(c[N]) == 0xeeeeeeeeu, "a write past the last result");
    memcpy(keep[0], ds, N * 4); memcpy(keep[1], dc, N * 4); memcpy(keep[2], s, N * 4); memcpy(keep[3], c, N * 4);
    call("_t_trig", aj);   /* again, other garbage in the scratch registers */
    T_CHECK(!memcmp(keep[0], ds, N * 4) && !memcmp(keep[1], dc, N * 4) && !memcmp(keep[2], s, N * 4) && !memcmp(keep[3], c, N * 4),
            "a second run gave other results (a register read before it was written)");
    for (k = 0; k < N; k++) {
        const double x = in[k], cl = x > 1.0 ? 1.0 : x < -1.0 ? -1.0 : x, es = fabs(s[k] - asin(cl)), ec = fabs(c[k] - acos(cl));
        elements++;
        T_CHECK(t_bits(ds[k]) == t_bits(s[k]) && t_bits(dc[k]) == t_bits(c[k]), "wrapper: at %.9g asin %08x / %08x, acos %08x / %08x (dce / SH4ZAM)", x,
                (unsigned)t_bits(ds[k]), (unsigned)t_bits(s[k]), (unsigned)t_bits(dc[k]), (unsigned)t_bits(c[k]));
        T_CHECK(es <= SHZ_ASINF_ERROR_MAX, "SH4ZAM ff39547 claim (SHZ_ASINF_ERROR_MAX): asinf(%.9g) = %.9g, true %.9g, off by %.3g", x, (double)s[k], asin(cl), es);
        T_CHECK(ec <= SHZ_ASINF_ERROR_MAX, "SH4ZAM ff39547 claim (SHZ_ASINF_ERROR_MAX): acosf(%.9g) = %.9g, true %.9g, off by %.3g", x, (double)c[k], acos(cl), ec);
        T_CHECK(s[k] == 0.0f || (s[k] < 0.0f) == (in[k] < 0.0f), "SH4ZAM ff39547 claim: asinf(%.9g) = %.9g has the sign of x", x, (double)s[k]);
        T_CHECK(c[k] >= 0.0f && c[k] <= (float)PI_D, "SH4ZAM ff39547 claim: acosf(%.9g) = %.9g inside [0, pi]", x, (double)c[k]);
        if (es > worst_as) worst_as = es;
        if (ec > worst_ac) worst_ac = ec;
        if (fabs(x) > 1.0) {
            T_CHECK(t_bits(s[k]) == t_bits(x > 0.0 ? (float)(PI_D / 2) : -(float)(PI_D / 2)), "SH4ZAM ff39547 claim: asinf past the end: %.9g", (double)s[k]);
            T_CHECK(t_bits(c[k]) == t_bits(x > 0.0 ? 0.0f : (float)PI_D), "SH4ZAM ff39547 claim: acosf past the end: %.9g", (double)c[k]);
        }
    }
    if (kind == 0) {
        T_CHECK(t_bits(s[1]) == 0u && (t_bits(s[2]) << 1) == 0u, "SH4ZAM ff39547 claim: asinf(0) is 0: %08x %08x", (unsigned)t_bits(s[1]), (unsigned)t_bits(s[2]));
        T_CHECK(t_bits(c[N - 1u]) == 0u && t_bits(c[0]) == t_bits((float)PI_D), "SH4ZAM ff39547 claim: acosf(1) = 0, acosf(-1) = pi");
        T_CHECK(t_bits(s[N - 1u]) == t_bits((float)(PI_D / 2)) && t_bits(s[0]) == t_bits(-(float)(PI_D / 2)), "SH4ZAM ff39547 claim: asinf(+-1) = +-pi/2");
        for (k = 0; k < N / 2u; k++)   /* the even sweep is symmetric: asin is odd bit for bit */
            T_CHECK(t_bits(s[k]) == (t_bits(s[N - 1u - k]) ^ 0x80000000u) || (s[k] == 0.0f && s[N - 1u - k] == 0.0f) || in[k] != -in[N - 1u - k],
                    "SH4ZAM ff39547 claim: asinf is odd at %.9g", (double)in[k]);
    }
    sh4_alloc_reset(S);
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    int kind, rep;
    if (rc) return rc;
    for (kind = 0; kind < 5; kind++)
        for (rep = 0; rep < (kind == 0 ? 1 : 6); rep++) run(kind);
    printf("wrapper: shz_dce_asinf / shz_dce_acosf == SH4ZAM's bit for bit, %u elements (%s build); SH4ZAM ff39547's routines: worst error asinf %.3g, acosf %.3g rad, bound %.3g\n",
           elements, (built & TRIG_BUILT_FAST) ? "-ffast-math" : "strict", worst_as, worst_ac, (double)SHZ_ASINF_ERROR_MAX);
    return t_done("trig_check");
}
