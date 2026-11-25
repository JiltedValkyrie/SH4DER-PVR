/* SH4ZAM-DCE tests: shz_dce_pvr.h as the SH-4 runs it

   pvr_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   pvr_check <pvr_kernels.elf>
   - the SH-4 loop against the plain C form, both on the SH-4, for every count from 0 up: all 32 bytes of every record must be the
     same bytes ("bit-identical: yes")
   - the screen point against the exact quotient (in double) within the bound of the transform and the reciprocal root; the five
     words carried through are the source's bits
   - every byte of a record is written (none is what the line allocation left), the record after the last is untouched, XMTRX is
     unchanged, a second run with other garbage in the scratch registers gives the same bytes
   - the input sits on a multiple of 4 that is not one of 8; the output on a multiple of 32, and once on a multiple of 4 only
*/

#include <sh4zam_dce/shz_dce_pvr.h>

#include "sh4_check.h"
#include "pvr_jobs.h"

#define BOUND 4.76837158203125e-07   /* 2^-21 */

/* a view: X and Y rows of a few hundred a unit, W = a mild mix of x, y, z plus 8 .. 40, so W stays between 2 and 60 for inputs in +-2 */
static void fill_matrix(float m[16]) {   /* column major */
    int c;
    for (c = 0; c < 4; c++) {
        m[4 * c + 0] = c == 3 ? t_frand(-2000, 2000) : t_frand(-300, 300);
        m[4 * c + 1] = c == 3 ? t_frand(-2000, 2000) : t_frand(-300, 300);
        m[4 * c + 2] = t_frand(-1, 1);
        m[4 * c + 3] = c == 3 ? t_frand(8, 40) : t_frand(-1, 1);
    }
}

static void run(uint32_t count, int out_align32) {
    const uint32_t aj = ram(sizeof(job_pvr_t), 4, 0), am = ram(64, 8, 0), ain = ram(count * 32 + 8, 8, 4);
    const uint32_t ao1 = ram(count * 32 + 64, 32, out_align32 ? 0 : 4), ao2 = ram(count * 32 + 64, 32, out_align32 ? 0 : 4);
    job_pvr_t *j = (job_pvr_t *)host(aj, sizeof *j);
    float *m = (float *)host(am, 64), *in = (float *)host(ain, count * 32 + 8), x[16];
    uint8_t *o1 = (uint8_t *)host(ao1, count * 32 + 64), *o2 = (uint8_t *)host(ao2, count * 32 + 64);
    uint8_t *keep = (uint8_t *)malloc(count * 32 + 1);
    uint32_t k;

    fill_matrix(m);
    for (k = 0; k < count * 8; k++) {
        const uint32_t bits = t_rand();
        if (k % 8 >= 1 && k % 8 <= 3) in[k] = t_frand(-2, 2);
        else memcpy(&in[k], &bits, 4);   /* the words carried through: any bit pattern */
    }
    memset(o1, 0x5a, count * 32 + 64); memset(o2, 0x5a, count * 32 + 64);
    j->matrix = am; j->in = ain; j->count = count; j->out_asm = ao1; j->out_c = ao2;
    call("_t_pvr", aj);
    for (k = 0; k < count; k++) {
        const float *p = (const float *)(o1 + 32 * k), *s = in + 8 * k;
        double e[4], pm[4];
        int r, c;
        T_CHECK(!memcmp(o1 + 32 * k, o2 + 32 * k, 32), "count %u: record %u differs from the C form", (unsigned)count, (unsigned)k);
        T_CHECK(!memcmp(p, s, 4) && !memcmp(p + 4, s + 4, 16), "count %u: record %u: the five words were not carried through", (unsigned)count, (unsigned)k);
        for (r = 0; r < 4; r++) {   /* row r of the column-major matrix times (x, y, z, 1) */
            e[r] = (double)m[12 + r]; pm[r] = fabs((double)m[12 + r]);
            for (c = 0; c < 3; c++) { const double t = (double)m[4 * c + r] * (double)s[1 + c]; e[r] += t; if (fabs(t) > pm[r]) pm[r] = fabs(t); }
        }
        {
            const double relw = (pm[3] + fabs(e[3])) / fabs(e[3]) * BOUND + 4.0 * BOUND;   /* W's own error, and the reciprocal root's */
            T_CHECK(e[3] > 1.0, "count %u: record %u: the test's own W is %.9g", (unsigned)count, (unsigned)k, e[3]);
            for (r = 0; r < 2; r++) {
                const double want = e[r] / e[3], tol = 2.0 * ((pm[r] + fabs(e[r])) / e[3] * BOUND + fabs(want) * relw);
                T_CHECK(fabs((double)p[1 + r] - want) <= tol, "count %u: record %u axis %d: %.9g, exact %.9g (tolerance %.3g)", (unsigned)count, (unsigned)k, r, (double)p[1 + r], want, tol);
            }
            T_CHECK(fabs((double)p[3] - 1.0 / e[3]) <= 2.0 * relw / e[3], "count %u: record %u: 1 / W %.9g, exact %.9g", (unsigned)count, (unsigned)k, (double)p[3], 1.0 / e[3]);
        }
    }
    for (k = 0; k < 32; k++) T_CHECK(o1[count * 32 + k] == 0x5a, "count %u: the record after the last was touched", (unsigned)count);
    sh4_xmtrx_get(S, x);
    T_CHECK(!memcmp(x, m, 64), "count %u: XMTRX changed", (unsigned)count);
    memcpy(keep, o1, count * 32);
    memset(o1, 0x5a, count * 32);
    call("_t_pvr", aj);   /* again, other garbage in the scratch registers */
    T_CHECK(!memcmp(keep, o1, count * 32), "count %u: a second run gave other bytes", (unsigned)count);
    free(keep);
    sh4_alloc_reset(S);
}

/* An identity matrix with W's row (0, 0, 1, 0): the screen point is (x / z, y / z, 1 / z) and the transform passes x, y, z through
   unchanged, so each result is FSRRA's reciprocal times one float: within 2^-21 of the quotient. Powers of two in z: exact. */
static void known(void) {
    static const float pts[][3] = { { 1.0f, 2.0f, 4.0f }, { -3.0f, 0.5f, 2.0f }, { 640.0f, -480.0f, 1.0f }, { 0.0f, 0.0f, 16.0f }, { 5.0f, 7.0f, 3.0f } };
    const uint32_t n = 5, aj = ram(sizeof(job_pvr_t), 4, 0), am = ram(64, 8, 0), ain = ram(n * 32, 8, 4), ao1 = ram(n * 32, 32, 0), ao2 = ram(n * 32, 32, 0);
    job_pvr_t *j = (job_pvr_t *)host(aj, sizeof *j);
    float *m = (float *)host(am, 64), *in = (float *)host(ain, n * 32), *o = (float *)host(ao1, n * 32);
    uint32_t k;

    memset(m, 0, 64); m[0] = m[5] = m[10] = 1.0f; m[11] = 1.0f;   /* W = z */
    memset(in, 0, n * 32);
    for (k = 0; k < n; k++) memcpy(in + 8 * k + 1, pts[k], 12);
    j->matrix = am; j->in = ain; j->count = n; j->out_asm = ao1; j->out_c = ao2;
    call("_t_pvr", aj);
    for (k = 0; k < n; k++) {
        const double z = (double)pts[k][2];
        T_CHECK(fabs((double)o[8 * k + 1] - (double)pts[k][0] / z) <= fabs((double)pts[k][0] / z) * 4.0 * BOUND, "known %u: sx %.9g", (unsigned)k, (double)o[8 * k + 1]);
        T_CHECK(fabs((double)o[8 * k + 2] - (double)pts[k][1] / z) <= fabs((double)pts[k][1] / z) * 4.0 * BOUND, "known %u: sy %.9g", (unsigned)k, (double)o[8 * k + 2]);
        T_CHECK(fabs((double)o[8 * k + 3] - 1.0 / z) <= 4.0 * BOUND / z, "known %u: sz %.9g", (unsigned)k, (double)o[8 * k + 3]);
    }
    sh4_alloc_reset(S);
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    uint32_t count;
    if (rc) return rc;
    for (count = 0; count <= 41; count++) run(count, 1);
    run(257, 1);
    for (count = 0; count <= 9; count++) run(count, 0);
    known();
    printf("pvr: SH-4 loop == C form on the SH-4 (all 32 bytes of every record), counts 0..41 and 257\n");
    return t_done("pvr_check");
}
