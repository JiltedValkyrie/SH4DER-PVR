/* SH4ZAM-DCE tests: shz_dce_atan2.h as the SH-4 runs it

   atan2_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   atan2_check <atan2_kernels.elf>
   The ELF is atan2_kernels.c built by sh-elf-gcc; tests/sh4run executes it instruction by instruction (FSRRA with the console's own
   bits when the bit-exact model is present).
   Pinned to SH4ZAM master ff39547 (2026-11-22). Every check below is a claim of that commit; a later SH4ZAM change that breaks one
   fails here by name ("SH4ZAM ff39547 claim: ...") rather than as a mystery. What is checked:
   - the wrapper: shz_dce_atan2f gives SH4ZAM's shz_atan2f bit for bit, every element, each from its own function
   - SH4ZAM's routine against the true angle (the host's atan2 in double): inside SHZ_ATAN2F_ERROR_MAX (1.4e-5 rad with the default
     5 terms) over directions spread evenly round the circle, random points, points next to both axes, lengths 2e-18 to 5e17
   - its special cases: atan2 of two zeros of any sign is a zero, the result has the sign of y (a zero y counts as positive), the
     four axes within 6e-7, the result inside [-pi, pi]
   - the default number of terms is 5
   - every call twice, the scratch registers seeded differently: the results must not change
*/

#include <sh4zam_dce/shz_dce_atan2.h>

#include "atan2_jobs.h"
#include "sh4_check.h"

#define PI_D 3.14159265358979323846
#define N 4096u

static double worst;
static unsigned built_terms, built_fast, elements;

static double angle_err(float got, double want) {
    double e = fabs((double)got - want);
    if (e > PI_D) e = fabs(e - 2.0 * PI_D);   /* +pi and -pi are the same direction */
    return e;
}

static void run(int kind) {
    const uint32_t aj = ram(sizeof(job_atan2_t), 4, 0), ay = ram(N * 4, 8, 4), ax = ram(N * 4, 8, 4), ad = ram(N * 4 + 4, 8, 4), as = ram(N * 4 + 4, 8, 4);
    job_atan2_t *j = (job_atan2_t *)host(aj, sizeof *j);
    float *y = (float *)host(ay, N * 4), *x = (float *)host(ax, N * 4), *d = (float *)host(ad, N * 4 + 4), *s = (float *)host(as, N * 4 + 4);
    static float keep[2][N];
    uint32_t k;
    d[N] = s[N] = t_float(0xeeeeeeeeu);
    for (k = 0; k < N; k++) {
        const double a = (k + 0.37 * (double)(t_rand() >> 8) / 16777216.0) * (2.0 * PI_D / N);
        switch (kind) {
        case 0: { const double m = 0.5 + 99.0 * (double)(t_rand() >> 8) / 16777216.0; x[k] = (float)(m * cos(a)); y[k] = (float)(m * sin(a)); break; }   /* round the circle */
        case 1: x[k] = t_frand(-1.0f, 1.0f); y[k] = t_frand(-1.0f, 1.0f); break;
        case 2: x[k] = t_frand(-1.0f, 1.0f) * 1e-4f; y[k] = t_frand(-50.0f, 50.0f); break;                                 /* next to the y axis */
        case 3: x[k] = t_frand(-50.0f, 50.0f); y[k] = t_frand(-1.0f, 1.0f) * 1e-4f; break;                                 /* next to the x axis */
        default: { const float m = t_float(0x03000000u * (1u + k % 20u) + 0x1f000000u); x[k] = (float)cos(a) * m; y[k] = (float)sin(a) * m; break; }   /* 1e-20 .. 1e16 */
        }
    }
    if (kind == 4) for (k = 0; k < N; k++) { const float m = (k & 1u) ? 2e-18f : 5e17f; x[k] = (float)cos(k * 0.0031) * m; y[k] = (float)sin(k * 0.0031) * m; }
    if (kind == 0) {   /* the special cases: zeros of every sign, the axes, the sign of y */
        static const float sy[12] = { 0.0f, -0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 0.0f, -0.0f, 3.0f, -3.0f, 1e-30f, -1e-30f };
        static const float sx[12] = { 0.0f, 0.0f, -0.0f, -0.0f, 2.0f, 2.0f, -2.0f, -2.0f, 0.0f, 0.0f, 1.0f, 1.0f };
        for (k = 0; k < 12; k++) { y[k] = sy[k]; x[k] = sx[k]; }
    }
    j->y = ay; j->x = ax; j->count = N; j->out_dce = ad; j->out_shz = as;
    call("_t_atan2", aj);
    built_terms = j->terms; built_fast = j->fast;
    T_CHECK(t_bits(d[N]) == 0xeeeeeeeeu && t_bits(s[N]) == 0xeeeeeeeeu, "a write past the last result");
    memcpy(keep[0], d, N * 4); memcpy(keep[1], s, N * 4);
    call("_t_atan2", aj);   /* again, other garbage in the scratch registers */
    T_CHECK(!memcmp(keep[0], d, N * 4) && !memcmp(keep[1], s, N * 4), "a second run gave other results (a register read before it was written)");
    for (k = 0; k < N; k++) {
        const double want = atan2((double)y[k], (double)x[k]);
        elements++;
        T_CHECK(t_bits(d[k]) == t_bits(s[k]), "wrapper: shz_dce_atan2f(%.9g, %.9g) = %08x, shz_atan2f %08x", (double)y[k], (double)x[k], (unsigned)t_bits(d[k]), (unsigned)t_bits(s[k]));
        if (x[k] == 0.0f && y[k] == 0.0f) {
            T_CHECK((t_bits(s[k]) << 1) == 0u, "SH4ZAM ff39547 claim: atan2 of two zeros is a zero, got %08x", (unsigned)t_bits(s[k]));
            continue;
        }
        {
            const double e = angle_err(s[k], want);
            T_CHECK(e <= SHZ_ATAN2F_ERROR_MAX, "SH4ZAM ff39547 claim (SHZ_ATAN2F_ERROR_MAX): atan2(%.9g, %.9g) = %.9g, true %.9g, off by %.3g", (double)y[k], (double)x[k], (double)s[k], want, e);
            T_CHECK((t_bits(s[k]) >> 31) == (uint32_t)(y[k] < 0.0f), "SH4ZAM ff39547 claim: atan2(%.9g, %.9g) = %.9g has the sign of y (a zero y counts as positive)", (double)y[k], (double)x[k], (double)s[k]);
            T_CHECK(fabs((double)s[k]) <= PI_D + 4e-7, "SH4ZAM ff39547 claim: atan2(%.9g, %.9g) = %.9g inside [-pi, pi]", (double)y[k], (double)x[k], (double)s[k]);
            if (x[k] == 0.0f || y[k] == 0.0f) T_CHECK(e <= 6e-7, "SH4ZAM ff39547 claim: on an axis, atan2(%.9g, %.9g) = %.9g is off by %.3g", (double)y[k], (double)x[k], (double)s[k], e);
            if (e > worst) worst = e;
        }
    }
    sh4_alloc_reset(S);
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    int kind, rep;
    if (rc) return rc;
    for (kind = 0; kind < 5; kind++)
        for (rep = 0; rep < (kind == 0 ? 2 : 5); rep++) run(kind);
    printf("wrapper: shz_dce_atan2f == shz_atan2f bit for bit, %u elements (%s build); SH4ZAM ff39547's routine, %u terms: worst error against the true angle %.3g rad, its bound %.3g\n",
           elements, built_fast ? "-ffast-math" : "strict", built_terms, worst, (double)SHZ_ATAN2F_ERROR_MAX);
    T_CHECK(built_terms == 5u, "SH4ZAM ff39547 claim: the default number of terms is 5, built with %u", built_terms);
    return t_done("atan2_check");
}
