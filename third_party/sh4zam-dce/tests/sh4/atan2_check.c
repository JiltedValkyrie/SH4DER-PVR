/* SH4ZAM-DCE tests: shz_dce_atan2.h as the SH-4 runs it

   atan2_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   atan2_check <atan2_kernels.elf>
   The ELF is atan2_kernels.c built by sh-elf-gcc; tests/sh4run executes it instruction by instruction (FSRRA with the console's own
   bits when the bit-exact model is present).
   - the rework with 2, 5, 6 and 7 terms against the true angle (the host's atan2 in double), each inside the header's bound, over
     directions spread evenly round the circle, random points, points next to both axes and lengths from 2e-18 to 5e17
   - the special cases of every form: (+-0, +-0), the sign of y (a zero y of either sign counts as positive), the four axes
     (within 6e-7 for every number of terms: the polynomials have no error of their own there)
   - the public routine is the form SHZ_DCE_ATAN2_TERMS names (the same bits in a strict build; with -ffast-math the compiler may
     regroup the polynomial at another call site: counted and printed, inside the bound)
   - SH4ZAM's routine on the same input: its worst error is printed beside them (the "before" of the header's claim)
   - every call twice, the scratch registers seeded differently: the results must not change
*/

#include <sh4zam_dce/shz_dce_atan2.h>

#include "atan2_jobs.h"
#include "sh4_check.h"

#define PI_D 3.14159265358979323846
#define N 4096u

static const double bound[4] = { 6.2e-3, 1.4e-5, 2.5e-6, 1.0e-6 };
static const int terms_of[4] = { 2, 5, 6, 7 };
static double worst[4], worst_shz;
static unsigned regrouped, built_terms, built_fast;

static double angle_err(float got, double want) {
    double e = fabs((double)got - want);
    if (e > PI_D) e = fabs(e - 2.0 * PI_D);   /* +pi and -pi are the same direction */
    return e;
}

static void run(int kind) {
    const uint32_t aj = ram(sizeof(job_atan2_t), 4, 0), ay = ram(N * 4, 8, 4), ax = ram(N * 4, 8, 4), ad = ram(N * 4 + 4, 8, 4), as = ram(N * 4 + 4, 8, 4);
    uint32_t an[4];
    job_atan2_t *j = (job_atan2_t *)host(aj, sizeof *j);
    float *y = (float *)host(ay, N * 4), *x = (float *)host(ax, N * 4), *d = (float *)host(ad, N * 4 + 4), *s = (float *)host(as, N * 4 + 4), *o[4];
    static float keep[5][N];
    uint32_t k;
    int v;
    for (v = 0; v < 4; v++) { an[v] = ram(N * 4 + 4, 8, 4); o[v] = (float *)host(an[v], N * 4 + 4); o[v][N] = t_float(0xeeeeeeeeu); j->out_n[v] = an[v]; }
    d[N] = t_float(0xeeeeeeeeu);
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
    for (v = 0; v < 4; v++) { T_CHECK(t_bits(o[v][N]) == 0xeeeeeeeeu, "%d terms: a write past the last result", terms_of[v]); memcpy(keep[v], o[v], N * 4); }
    T_CHECK(t_bits(d[N]) == 0xeeeeeeeeu, "the public routine: a write past the last result");
    memcpy(keep[4], d, N * 4);
    call("_t_atan2", aj);   /* again, other garbage in the scratch registers */
    for (v = 0; v < 4; v++) T_CHECK(!memcmp(keep[v], o[v], N * 4), "%d terms: a second run gave other results (a register read before it was written)", terms_of[v]);
    T_CHECK(!memcmp(keep[4], d, N * 4), "the public routine: a second run gave other results");
    for (k = 0; k < N; k++) {
        const double want = atan2((double)y[k], (double)x[k]);
        if (x[k] == 0.0f && y[k] == 0.0f) {
            for (v = 0; v < 4; v++) T_CHECK((t_bits(o[v][k]) << 1) == 0u, "%d terms: atan2 of two zeros is %08x", terms_of[v], (unsigned)t_bits(o[v][k]));
            continue;
        }
        for (v = 0; v < 4; v++) {
            const double e = angle_err(o[v][k], want);
            T_CHECK(e <= bound[v], "%d terms: atan2(%.9g, %.9g) = %.9g, true %.9g, off by %.3g", terms_of[v], (double)y[k], (double)x[k], (double)o[v][k], want, e);
            T_CHECK((t_bits(o[v][k]) >> 31) == (uint32_t)(y[k] < 0.0f), "%d terms: atan2(%.9g, %.9g) = %.9g has not the sign of y (a zero y counts as positive)", terms_of[v], (double)y[k], (double)x[k], (double)o[v][k]);
            T_CHECK(fabs((double)o[v][k]) <= PI_D + 4e-7, "%d terms: atan2(%.9g, %.9g) = %.9g leaves [-pi, pi]", terms_of[v], (double)y[k], (double)x[k], (double)o[v][k]);
            if (x[k] == 0.0f || y[k] == 0.0f) T_CHECK(e <= 6e-7, "%d terms: on an axis, atan2(%.9g, %.9g) = %.9g is off by %.3g", terms_of[v], (double)y[k], (double)x[k], (double)o[v][k], e);
            if (e > worst[v]) worst[v] = e;
        }
        { const double e = angle_err(s[k], want); if (e == e && e > worst_shz) worst_shz = e; }
    }
    {   /* the public routine against the form its switch names */
        int sel = 0;
        for (v = 0; v < 4; v++) if ((unsigned)terms_of[v] == built_terms) sel = v;
        for (k = 0; k < N; k++) {
            if (t_bits(d[k]) == t_bits(o[sel][k])) continue;
            if (built_fast) regrouped++;
            else T_CHECK(0, "the public routine and the %u-term form differ at (%.9g, %.9g)", built_terms, (double)y[k], (double)x[k]);
        }
    }
    sh4_alloc_reset(S);
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    int kind, rep, v;
    if (rc) return rc;
    for (kind = 0; kind < 5; kind++)
        for (rep = 0; rep < (kind == 0 ? 2 : 5); rep++) run(kind);
    printf("worst error against the true angle, rad: 2 terms %.3g, 5 terms %.3g, 6 terms %.3g, 7 terms %.3g; SH4ZAM %.3g\n", worst[0], worst[1], worst[2], worst[3], worst_shz);
    printf("the public routine is the %u-term form; %u of its results differ from that form's in the last bits (another grouping at another call site)\n", built_terms, regrouped);
    for (v = 0; v < 4; v++) T_CHECK(worst[v] < worst_shz, "the %d-term form is no closer than SH4ZAM's routine", terms_of[v]);
    T_CHECK(worst_shz > 1000.0 * worst[2], "the header says the default is far closer than SH4ZAM's routine");
    return t_done("atan2_check");
}
