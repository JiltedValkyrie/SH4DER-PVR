/* SH4ZAM-DCE tests: shz_dce_xmtrx.h on the host (SH4ZAM's software back end)

   xmtrx_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked, for each of the four routines:
   - every result is, bit for bit, what SH4ZAM's own single call gives for that vector (shz_xmtrx_transform_vec4, _point3,
     _vec3; for the projection shz_xmtrx_transform_vec4, shz_invf_fsrra and one product each), and within the bound of the
     exact value
   - the dispatch and the C form give the same bytes
   - the bytes between records and after the last record are untouched, the input is not changed, count 0 writes nothing
   - strides: the smallest and larger ones, different for the two arrays; in place (the output is the input array)
*/

#include <sh4zam_dce/shz_dce_xmtrx.h>

#include "shz_dce_test.h"

#define BOUND 4.76837158203125e-07   /* 2^-21 */
#define N 67
#define STRIDE_MAX 40

enum { VEC4, POINT3, VEC3, PROJECT, KINDS };
static const char *const kind_name[KINDS] = { "vec4", "point3", "vec3", "project" };
static const size_t kind_floats[KINDS] = { 4, 3, 3, 3 };

static void batch(int kind, int c_form, void *out, size_t os, const void *in, size_t is, size_t n) {
    switch (kind) {
    case VEC4:   if (c_form) shz_dce_xmtrx_transform_vec4_batch_c(out, os, in, is, n); else shz_dce_xmtrx_transform_vec4_batch(out, os, in, is, n); break;
    case POINT3: if (c_form) shz_dce_xmtrx_transform_point3_batch_c(out, os, in, is, n); else shz_dce_xmtrx_transform_point3_batch(out, os, in, is, n); break;
    case VEC3:   if (c_form) shz_dce_xmtrx_transform_vec3_batch_c(out, os, in, is, n); else shz_dce_xmtrx_transform_vec3_batch(out, os, in, is, n); break;
    default:     if (c_form) shz_dce_xmtrx_project_point3_batch_c(out, os, in, is, n); else shz_dce_xmtrx_project_point3_batch(out, os, in, is, n); break;
    }
}

/* what SH4ZAM's single call gives for one vector */
static void single(int kind, const float *s, float *d) {
    if (kind == VEC4) {
        const shz_vec4_t r = shz_xmtrx_transform_vec4(shz_vec4_init(s[0], s[1], s[2], s[3]));
        d[0] = r.x; d[1] = r.y; d[2] = r.z; d[3] = r.w;
    } else if (kind == POINT3) {
        const shz_vec3_t r = shz_xmtrx_transform_point3(shz_vec3_init(s[0], s[1], s[2]));
        d[0] = r.x; d[1] = r.y; d[2] = r.z;
    } else if (kind == VEC3) {
        const shz_vec3_t r = shz_xmtrx_transform_vec3(shz_vec3_init(s[0], s[1], s[2]));
        d[0] = r.x; d[1] = r.y; d[2] = r.z;
    } else {
        const shz_vec4_t r = shz_xmtrx_transform_vec4(shz_vec4_init(s[0], s[1], s[2], 1.0f));
        const float inv = shz_invf_fsrra(r.w);
        d[0] = r.x * inv; d[1] = r.y * inv; d[2] = inv;
    }
}

static void run(int kind, size_t count, size_t is, size_t os, int in_place) {
    static float src_f[(N + 1) * STRIDE_MAX / 4 + 8], in_f[(N + 1) * STRIDE_MAX / 4 + 8], in2_f[(N + 1) * STRIDE_MAX / 4 + 8];
    static float out_f[(N + 1) * STRIDE_MAX / 4 + 8], out2_f[(N + 1) * STRIDE_MAX / 4 + 8];
    static shz_mat4x4_t m;
    const size_t nf = kind_floats[kind], in_bytes = count * is + 32, out_bytes = count * os + 32;
    uint8_t *src = (uint8_t *)src_f + 4, *in = (uint8_t *)in_f + 4, *in2 = (uint8_t *)in2_f + 4;   /* a multiple of 4, not of 8 */
    uint8_t *out = in_place ? in : (uint8_t *)out_f + 4, *out2 = in_place ? in2 : (uint8_t *)out2_f + 4;
    size_t k, b;
    int c;

    for (c = 0; c < 4; c++) {
        m.elem[4 * c + 0] = c == 3 ? t_frand(-2000, 2000) : t_frand(-300, 300);
        m.elem[4 * c + 1] = c == 3 ? t_frand(-2000, 2000) : t_frand(-300, 300);
        m.elem[4 * c + 2] = kind == PROJECT ? t_frand(-1, 1) : t_frand(-300, 300);
        m.elem[4 * c + 3] = c == 3 ? t_frand(8, 40) : t_frand(-1, 1);
    }
    for (b = 0; b < in_bytes; b++) src[b] = (uint8_t)(t_rand() >> 24);
    for (k = 0; k < count; k++) {
        float v[4];
        for (b = 0; b < nf; b++) v[b] = t_frand(-2, 2);
        memcpy(src + k * is, v, 4 * nf);
    }
    memcpy(in, src, in_bytes); memcpy(in2, src, in_bytes);
    if (!in_place) { memset(out, 0x5a, out_bytes); memset(out2, 0x5a, out_bytes); }
    shz_xmtrx_load_4x4(&m);
    batch(kind, 0, out, os, in, is, count);
    batch(kind, 1, out2, os, in2, is, count);

    T_CHECK(!memcmp(out, out2, out_bytes), "%s count %u strides %u %u%s: the dispatch and the C form differ", kind_name[kind], (unsigned)count, (unsigned)is,
            (unsigned)os, in_place ? " in place" : "");
    for (k = 0; k < count; k++) {
        float s[4] = { 0.0f, 0.0f, 0.0f, 0.0f }, p[4], want[4];
        double v[4], e[4], pm[4];
        size_t r;

        memcpy(s, src + k * is, 4 * nf);
        memcpy(p, out + k * os, 4 * nf);
        single(kind, s, want);
        for (r = 0; r < nf; r++)
            T_CHECK(t_bits(p[r]) == t_bits(want[r]), "%s count %u: record %u number %u: not the bits of SH4ZAM's single call", kind_name[kind], (unsigned)count,
                    (unsigned)k, (unsigned)r);
        v[0] = (double)s[0]; v[1] = (double)s[1]; v[2] = (double)s[2];
        v[3] = kind == VEC4 ? (double)s[3] : kind == VEC3 ? 0.0 : 1.0;
        for (r = 0; r < 4; r++) {
            e[r] = 0.0; pm[r] = 0.0;
            for (c = 0; c < 4; c++) { const double t = (double)m.elem[4 * c + r] * v[c]; e[r] += t; if (fabs(t) > pm[r]) pm[r] = fabs(t); }
        }
        if (kind == PROJECT) {
            const double relw = (pm[3] + fabs(e[3])) / fabs(e[3]) * BOUND + 4.0 * BOUND;
            T_CHECK(e[3] > 1.0, "project: record %u: the test's own W is %.9g", (unsigned)k, e[3]);
            for (r = 0; r < 2; r++) {
                const double q = e[r] / e[3], tol = 2.0 * ((pm[r] + fabs(e[r])) / e[3] * BOUND + fabs(q) * relw);
                T_CHECK(fabs((double)p[r] - q) <= tol, "project: record %u axis %u: %.9g, exact %.9g", (unsigned)k, (unsigned)r, (double)p[r], q);
            }
            T_CHECK(fabs((double)p[2] - 1.0 / e[3]) <= 2.0 * relw / e[3], "project: record %u: 1 / W %.9g, exact %.9g", (unsigned)k, (double)p[2], 1.0 / e[3]);
        } else {
            for (r = 0; r < nf; r++)
                T_CHECK(fabs((double)p[r] - e[r]) <= (pm[r] + fabs(e[r])) * BOUND, "%s: record %u row %u: %.9g, exact %.9g", kind_name[kind], (unsigned)k, (unsigned)r,
                        (double)p[r], e[r]);
        }
        for (b = 4 * nf; b < os; b++)
            T_CHECK(out[k * os + b] == (in_place ? src[k * is + b] : 0x5a), "%s count %u: record %u: byte %u after it was written", kind_name[kind], (unsigned)count,
                    (unsigned)k, (unsigned)b);
    }
    for (b = 0; b < 32; b++)
        T_CHECK(out[count * os + b] == (in_place ? src[count * is + b] : 0x5a), "%s count %u: written after the last record", kind_name[kind], (unsigned)count);
    if (!in_place) T_CHECK(!memcmp(in, src, in_bytes), "%s count %u: the input was changed", kind_name[kind], (unsigned)count);
}

int main(void) {
    int kind, trial;

    for (kind = 0; kind < KINDS; kind++) {
        const size_t least = 4 * kind_floats[kind];
        for (trial = 0; trial < 300; trial++) {
            const size_t count = (size_t)(trial % (N + 1));
            run(kind, count, least, least, 0);
            run(kind, count, least + 4, least + 16, 0);
            run(kind, count, 32, least, 0);
            run(kind, count, least, least, 1);
            run(kind, count, least + 8, least + 8, 1);
        }
    }
    return t_done("xmtrx_test");
}
