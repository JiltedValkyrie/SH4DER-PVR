/* SH4ZAM-DCE tests: shz_dce_xmtrx.h as the SH-4 runs it

   xmtrx_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   xmtrx_check <xmtrx_kernels.elf>
   For each of the four routines:
   - the SH-4 loop against the plain C form, both on the SH-4, for every count from 0 up: every byte of both output arrays must
     be the same ("bit-identical: yes"), the bytes between records included
   - every result against the exact value (in double) within the bound of the transform, and of the reciprocal root for the
     projection
   - the bytes between records and after the last record are untouched, XMTRX is unchanged, a second run with other garbage in
     the scratch registers gives the same bytes
   - the smallest strides and larger ones, different for the two arrays; and in place (the output is the input array)
   - every array sits on a multiple of 4 that is not one of 8
*/

#include <sh4zam_dce/shz_dce_xmtrx.h>

#include "sh4_check.h"
#include "xmtrx_jobs.h"

#ifndef XMTRX_CHECK_NAME
#define XMTRX_CHECK_NAME "xmtrx_check"
#endif

#define BOUND 4.76837158203125e-07   /* 2^-21 */

static const char *const kind_name[XMTRX_KINDS] = { "vec4", "point3", "vec3", "project" };
static const uint32_t kind_in[XMTRX_KINDS] = { 4, 3, 3, 3 }, kind_out[XMTRX_KINDS] = { 4, 3, 3, 3 };   /* floats a record */

/* column major. For the projection: X and Y rows of a few hundred a unit, W = a mild mix of x, y, z plus 8 .. 40, so W stays
   between 2 and 60 for inputs in +-2. */
static void fill_matrix(float m[16], uint32_t kind) {
    int c;
    for (c = 0; c < 4; c++) {
        m[4 * c + 0] = c == 3 ? t_frand(-2000, 2000) : t_frand(-300, 300);
        m[4 * c + 1] = c == 3 ? t_frand(-2000, 2000) : t_frand(-300, 300);
        m[4 * c + 2] = kind == XMTRX_PROJECT ? t_frand(-1, 1) : t_frand(-300, 300);
        m[4 * c + 3] = c == 3 ? t_frand(8, 40) : t_frand(-1, 1);
    }
}

/* row r of the matrix times v, exactly, and the largest of its four products */
static void exact(const float m[16], const double v[4], double e[4], double pm[4]) {
    int r, c;
    for (r = 0; r < 4; r++) {
        e[r] = 0.0; pm[r] = 0.0;
        for (c = 0; c < 4; c++) { const double t = (double)m[4 * c + r] * v[c]; e[r] += t; if (fabs(t) > pm[r]) pm[r] = fabs(t); }
    }
}

static void run(uint32_t kind, uint32_t count, uint32_t in_stride, uint32_t out_stride, int in_place) {
    const uint32_t ni = kind_in[kind], no = kind_out[kind];
    const uint32_t in_bytes = count * in_stride + 32, out_bytes = count * out_stride + 32;
    const uint32_t aj = ram(sizeof(job_xmtrx_t), 4, 0), am = ram(64, 8, 0);
    const uint32_t ai1 = ram(in_bytes, 8, 4), ai2 = ram(in_bytes, 8, 4);
    const uint32_t ao1 = in_place ? ai1 : ram(out_bytes, 8, 4), ao2 = in_place ? ai2 : ram(out_bytes, 8, 4);
    job_xmtrx_t *j = (job_xmtrx_t *)host(aj, sizeof *j);
    float *m = (float *)host(am, 64), x[16];
    uint8_t *i1 = (uint8_t *)host(ai1, in_bytes), *i2 = (uint8_t *)host(ai2, in_bytes);
    uint8_t *o1 = (uint8_t *)host(ao1, out_bytes), *o2 = (uint8_t *)host(ao2, out_bytes);
    uint8_t *src = (uint8_t *)malloc(in_bytes), *keep = (uint8_t *)malloc(out_bytes);
    uint32_t k, b;

    fill_matrix(m, kind);
    for (b = 0; b < in_bytes; b++) src[b] = (uint8_t)(t_rand() >> 24);          /* the bytes between records: any */
    for (k = 0; k < count; k++) {
        float v[4];
        for (b = 0; b < ni; b++) v[b] = t_frand(-2, 2);
        memcpy(src + k * in_stride, v, 4 * ni);
    }
    if (!in_place) { memset(o1, 0x5a, out_bytes); memset(o2, 0x5a, out_bytes); }
    memcpy(i1, src, in_bytes); memcpy(i2, src, in_bytes);
    j->matrix = am; j->kind = kind; j->count = count; j->in_a = ai1; j->in_c = ai2; j->in_stride = in_stride;
    j->out_a = ao1; j->out_c = ao2; j->out_stride = out_stride;
    call("_t_xmtrx", aj);

    T_CHECK(!memcmp(o1, o2, out_bytes), "%s count %u strides %u %u%s: differs from the C form", kind_name[kind], (unsigned)count, (unsigned)in_stride,
            (unsigned)out_stride, in_place ? " in place" : "");
    for (k = 0; k < count; k++) {
        float s[4] = { 0.0f, 0.0f, 0.0f, 0.0f }, p[4];
        double v[4], e[4], pm[4];
        uint32_t r;

        memcpy(s, src + k * in_stride, 4 * ni);
        memcpy(p, o1 + k * out_stride, 4 * no);
        v[0] = (double)s[0]; v[1] = (double)s[1]; v[2] = (double)s[2];
        v[3] = kind == XMTRX_VEC4 ? (double)s[3] : kind == XMTRX_VEC3 ? 0.0 : 1.0;
        exact(m, v, e, pm);
        if (kind == XMTRX_PROJECT) {
            const double relw = (pm[3] + fabs(e[3])) / fabs(e[3]) * BOUND + 4.0 * BOUND;   /* W's own error, and the reciprocal root's */
            T_CHECK(e[3] > 1.0, "project count %u: record %u: the test's own W is %.9g", (unsigned)count, (unsigned)k, e[3]);
            for (r = 0; r < 2; r++) {
                const double want = e[r] / e[3], tol = 2.0 * ((pm[r] + fabs(e[r])) / e[3] * BOUND + fabs(want) * relw);
                T_CHECK(fabs((double)p[r] - want) <= tol, "project count %u: record %u axis %u: %.9g, exact %.9g (tolerance %.3g)", (unsigned)count, (unsigned)k,
                        (unsigned)r, (double)p[r], want, tol);
            }
            T_CHECK(fabs((double)p[2] - 1.0 / e[3]) <= 2.0 * relw / e[3], "project count %u: record %u: 1 / W %.9g, exact %.9g", (unsigned)count, (unsigned)k,
                    (double)p[2], 1.0 / e[3]);
        } else {
            for (r = 0; r < no; r++)
                T_CHECK(fabs((double)p[r] - e[r]) <= (pm[r] + fabs(e[r])) * BOUND, "%s count %u: record %u row %u: %.9g, exact %.9g", kind_name[kind], (unsigned)count,
                        (unsigned)k, (unsigned)r, (double)p[r], e[r]);
        }
        for (b = 4 * no; b < out_stride; b++)                                    /* between this record and the next */
            T_CHECK(o1[k * out_stride + b] == (in_place ? src[k * in_stride + b] : 0x5a), "%s count %u: record %u: byte %u after it was written",
                    kind_name[kind], (unsigned)count, (unsigned)k, (unsigned)b);
    }
    for (b = 0; b < 32; b++)
        T_CHECK(o1[count * out_stride + b] == (in_place ? src[count * in_stride + b] : 0x5a), "%s count %u: written after the last record", kind_name[kind],
                (unsigned)count);
    if (!in_place) T_CHECK(!memcmp(i1, src, in_bytes), "%s count %u: the input was changed", kind_name[kind], (unsigned)count);
    sh4_xmtrx_get(S, x);
    T_CHECK(!memcmp(x, m, 64), "%s count %u: XMTRX changed", kind_name[kind], (unsigned)count);

    memcpy(keep, o1, out_bytes);
    memcpy(i1, src, in_bytes); memcpy(i2, src, in_bytes);
    if (!in_place) memset(o1, 0x5a, out_bytes);
    call("_t_xmtrx", aj);   /* again, other garbage in the scratch registers */
    T_CHECK(!memcmp(keep, o1, out_bytes), "%s count %u: a second run gave other bytes", kind_name[kind], (unsigned)count);
    free(src); free(keep);
    sh4_alloc_reset(S);
}

/* An identity matrix with W's row (0, 0, 1, 0): the projection of (x, y, z) is (x / z, y / z, 1 / z) and the transform passes
   x, y, z through unchanged, so each result is FSRRA's reciprocal times one float. */
static void known(void) {
    static const float pts[][3] = { { 1.0f, 2.0f, 4.0f }, { -3.0f, 0.5f, 2.0f }, { 640.0f, -480.0f, 1.0f }, { 0.0f, 0.0f, 16.0f }, { 5.0f, 7.0f, 3.0f } };
    const uint32_t n = 5, aj = ram(sizeof(job_xmtrx_t), 4, 0), am = ram(64, 8, 0), ain = ram(n * 12, 8, 4), ain2 = ram(n * 12, 8, 4);
    const uint32_t ao1 = ram(n * 12, 8, 4), ao2 = ram(n * 12, 8, 4);
    job_xmtrx_t *j = (job_xmtrx_t *)host(aj, sizeof *j);
    float *m = (float *)host(am, 64), *o = (float *)host(ao1, n * 12);
    uint32_t k;

    memset(m, 0, 64); m[0] = m[5] = m[10] = 1.0f; m[11] = 1.0f;   /* W = z */
    memcpy(host(ain, n * 12), pts, n * 12); memcpy(host(ain2, n * 12), pts, n * 12);
    j->matrix = am; j->kind = XMTRX_PROJECT; j->count = n; j->in_a = ain; j->in_c = ain2; j->in_stride = 12; j->out_a = ao1; j->out_c = ao2; j->out_stride = 12;
    call("_t_xmtrx", aj);
    for (k = 0; k < n; k++) {
        const double z = (double)pts[k][2];
        T_CHECK(fabs((double)o[3 * k + 0] - (double)pts[k][0] / z) <= fabs((double)pts[k][0] / z) * 4.0 * BOUND, "known %u: sx %.9g", (unsigned)k, (double)o[3 * k + 0]);
        T_CHECK(fabs((double)o[3 * k + 1] - (double)pts[k][1] / z) <= fabs((double)pts[k][1] / z) * 4.0 * BOUND, "known %u: sy %.9g", (unsigned)k, (double)o[3 * k + 1]);
        T_CHECK(fabs((double)o[3 * k + 2] - 1.0 / z) <= 4.0 * BOUND / z, "known %u: 1 / W %.9g", (unsigned)k, (double)o[3 * k + 2]);
    }
    sh4_alloc_reset(S);
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    uint32_t kind, count;
    if (rc) return rc;
    for (kind = 0; kind < XMTRX_KINDS; kind++) {
        const uint32_t least = 4 * kind_in[kind];
        for (count = 0; count <= 41; count++) {
            run(kind, count, least, least, 0);
            run(kind, count, least + 4, least + 16, 0);
            run(kind, count, least, least, 1);
        }
        for (count = 0; count <= 9; count++) { run(kind, count, 32, least, 0); run(kind, count, least + 8, least + 8, 1); }
        run(kind, 257, least, least, 0);
        run(kind, 257, 16, 16, 1);
    }
    known();
    printf("xmtrx: SH-4 loops == C forms on the SH-4 (every byte of the output arrays), 4 routines, counts 0..41 and 257, in place and apart\n");
    return t_done(XMTRX_CHECK_NAME);
}
