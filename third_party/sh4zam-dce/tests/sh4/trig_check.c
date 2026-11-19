/* SH4ZAM-DCE tests: shz_dce_trig.h as the SH-4 runs it

   trig_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   trig_check <trig_kernels.elf>
   The ELF is trig_kernels.c built by sh-elf-gcc; tests/sh4run executes it instruction by instruction (FSRRA with the console's own
   bits when the bit-exact model is present).
   - shz_dce_asinf and shz_dce_acosf, as the switch builds them and with the barrier off and on, against the true angle (the host's
     asin / acos in double), inside the header's bound, over a sweep of [-1, 1] that is dense near 0 and near +-1
   - the special cases of all three: 0, the ends, arguments past +-1, the sign and the odd symmetry of the arcsine
   - in a strict build the public routine has the bits of the form its switch selects (with -ffast-math the compiler regroups the
     product where there is no barrier, differently from one call site to another: counted and printed, inside the bound)
   - SH4ZAM's routines on the same input: their worst error is printed beside the rework's (the "before" of the header's claim)
   - every call twice, the scratch registers seeded differently: the results must not change
*/

#include <sh4zam_dce/shz_dce_trig.h>

#include "trig_jobs.h"
#include "sh4_check.h"

#define BOUND 8e-6
#define PI_D 3.14159265358979323846
#define N 4096u

static double worst_as[3], worst_ac[3], worst_os, worst_oc;
static const char *const form[3] = { "public", "barrier off", "barrier on" };
static unsigned built, regrouped;

static void run(int kind) {
    const uint32_t aj = ram(sizeof(job_trig_t), 4, 0), ai = ram(N * 4, 8, 4), a3 = ram(N * 4 + 4, 8, 4), a4 = ram(N * 4 + 4, 8, 4);
    uint32_t a_s[3], a_c[3];
    job_trig_t *j = (job_trig_t *)host(aj, sizeof *j);
    float *in = (float *)host(ai, N * 4), *os = (float *)host(a3, N * 4 + 4), *oc = (float *)host(a4, N * 4 + 4), *as[3], *ac[3];
    static float keep[6][N];
    uint32_t k;
    int v;
    for (v = 0; v < 3; v++) {
        a_s[v] = ram(N * 4 + 4, 8, 4); a_c[v] = ram(N * 4 + 4, 8, 4);
        as[v] = (float *)host(a_s[v], N * 4 + 4); ac[v] = (float *)host(a_c[v], N * 4 + 4);
        as[v][N] = ac[v][N] = t_float(0xeeeeeeeeu);
    }
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
    j->in = ai; j->count = N; j->asin_shz = a3; j->acos_shz = a4;
    j->asin_dce = a_s[0]; j->acos_dce = a_c[0]; j->asin_plain = a_s[1]; j->acos_plain = a_c[1]; j->asin_opaque = a_s[2]; j->acos_opaque = a_c[2];
    call("_t_trig", aj);
    built = j->built;
    for (v = 0; v < 3; v++) {
        T_CHECK(t_bits(as[v][N]) == 0xeeeeeeeeu && t_bits(ac[v][N]) == 0xeeeeeeeeu, "%s: a write past the last result", form[v]);
        memcpy(keep[2 * v], as[v], N * 4); memcpy(keep[2 * v + 1], ac[v], N * 4);
    }
    call("_t_trig", aj);   /* again, other garbage in the scratch registers */
    for (v = 0; v < 3; v++)
        T_CHECK(!memcmp(keep[2 * v], as[v], N * 4) && !memcmp(keep[2 * v + 1], ac[v], N * 4), "%s: a second run gave other results (a register read before it was written)", form[v]);
    for (v = 0; v < 3; v++) {
        const float *s = as[v], *c = ac[v];
        for (k = 0; k < N; k++) {
            const double x = in[k], cl = x > 1.0 ? 1.0 : x < -1.0 ? -1.0 : x, es = fabs(s[k] - asin(cl)), ec = fabs(c[k] - acos(cl));
            T_CHECK(es <= BOUND, "%s: asinf(%.9g) = %.9g, true %.9g, off by %.3g", form[v], x, (double)s[k], asin(cl), es);
            T_CHECK(ec <= BOUND, "%s: acosf(%.9g) = %.9g, true %.9g, off by %.3g", form[v], x, (double)c[k], acos(cl), ec);
            T_CHECK(s[k] == 0.0f || (s[k] < 0.0f) == (in[k] < 0.0f), "%s: asinf(%.9g) = %.9g has the wrong sign", form[v], x, (double)s[k]);
            T_CHECK(c[k] >= 0.0f && c[k] <= (float)PI_D, "%s: acosf(%.9g) = %.9g leaves [0, pi]", form[v], x, (double)c[k]);
            if (es > worst_as[v]) worst_as[v] = es;
            if (ec > worst_ac[v]) worst_ac[v] = ec;
            if (fabs(x) > 1.0) {
                T_CHECK(t_bits(s[k]) == t_bits(x > 0.0 ? (float)(PI_D / 2) : -(float)(PI_D / 2)), "%s: asinf past the end: %.9g", form[v], (double)s[k]);
                T_CHECK(t_bits(c[k]) == t_bits(x > 0.0 ? 0.0f : (float)PI_D), "%s: acosf past the end: %.9g", form[v], (double)c[k]);
            }
        }
        if (kind == 0) {
            T_CHECK(t_bits(s[1]) == 0u && (t_bits(s[2]) << 1) == 0u, "%s: asinf(0) must be 0: %08x %08x", form[v], (unsigned)t_bits(s[1]), (unsigned)t_bits(s[2]));
            T_CHECK(t_bits(c[N - 1u]) == 0u && t_bits(c[0]) == t_bits((float)PI_D), "%s: acosf(1) = 0, acosf(-1) = pi", form[v]);
            T_CHECK(t_bits(s[N - 1u]) == t_bits((float)(PI_D / 2)) && t_bits(s[0]) == t_bits(-(float)(PI_D / 2)), "%s: asinf(+-1) = +-pi/2", form[v]);
            for (k = 0; k < N / 2u; k++)   /* the even sweep is symmetric: asin is odd bit for bit */
                T_CHECK(t_bits(s[k]) == (t_bits(s[N - 1u - k]) ^ 0x80000000u) || (s[k] == 0.0f && s[N - 1u - k] == 0.0f) || in[k] != -in[N - 1u - k],
                        "%s: asinf is not odd at %.9g", form[v], (double)in[k]);
        }
    }
    {   /* the public routine against the form its switch selects */
        const int sel = (built & TRIG_BUILT_OPAQUE) ? 2 : 1;
        for (k = 0; k < N; k++) {
            const int same = t_bits(as[0][k]) == t_bits(as[sel][k]) && t_bits(ac[0][k]) == t_bits(ac[sel][k]);
            if (!same && (built & TRIG_BUILT_FAST) && sel == 1) regrouped++;
            else T_CHECK(same, "the public routine and the form its switch selects differ at %.9g", (double)in[k]);
        }
    }
    for (k = 0; k < N; k++) {   /* SH4ZAM's routines, where they are defined */
        const double x = in[k];
        if (fabs(x) <= 1.0) {
            const double eos = fabs(os[k] - asin(x)), eoc = fabs(oc[k] - acos(x));
            if (eos == eos && eos > worst_os) worst_os = eos;
            if (eoc == eoc && eoc > worst_oc) worst_oc = eoc;
        }
    }
    sh4_alloc_reset(S);
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    int kind, rep, v;
    if (rc) return rc;
    for (kind = 0; kind < 5; kind++)
        for (rep = 0; rep < (kind == 0 ? 1 : 6); rep++) run(kind);
    printf("worst error against the true angle, rad: asinf %.3g (SH4ZAM %.3g), acosf %.3g (SH4ZAM %.3g); bound %.3g\n", worst_as[0], worst_os, worst_ac[0], worst_oc, BOUND);
    printf("barrier off: asinf %.3g, acosf %.3g; barrier on: asinf %.3g, acosf %.3g; the switch is %s; %u public results differ from the barrier-off form's in the last bits "
           "(another grouping at another call site)\n", worst_as[1], worst_ac[1], worst_as[2], worst_ac[2], (built & TRIG_BUILT_OPAQUE) ? "on" : "off", regrouped);
    T_CHECK((built & TRIG_BUILT_FAST) || regrouped == 0u, "a strict build must give the same bits at every call site");
    for (v = 0; v < 3; v++)
        T_CHECK(worst_os > 100.0 * worst_as[v] && worst_oc > 100.0 * worst_ac[v], "the header says the reworks are far closer than SH4ZAM's routines (%s)", form[v]);
    return t_done("trig_check");
}
