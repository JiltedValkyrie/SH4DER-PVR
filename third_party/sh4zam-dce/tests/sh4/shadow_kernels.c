/* SH4ZAM-DCE tests: shz_dce_shadow.h, the SH-4 side

   shadow_kernels.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc (-ml -m4-single; -O2 and -O3; the strict flags and the -ffast-math flags) and run by shadow_check.c through
   the interpreter (tests/sh4run). Each function takes one record of shadow_jobs.h. The job runs the routine's dispatch, its _sh4 form,
   its _c form, and BEFORE: the planar path of the per-point compare that the routine replaces (the code of the shadow-map library's
   planar batch, operation for operation: one FTRV for the point and one for each axis a point, copied out through memory, then the
   texel measure, the plane compare and the tap reads; the degenerate solve's fallback uses the library's plain 4 x 4 row form on the
   matrix). The four outputs must agree: the dispatch, _sh4 and _c are the same C here, and BEFORE uses the same FTRV on the same inputs.
   This file is also the compile check of the header: it instantiates every public routine, so every asm block is assembled.
*/

#include <sh4zam_dce/shz_dce_shadow.h>

#include "shadow_jobs.h"

#define P(type, addr) ((type)(uintptr_t)(addr))

/* ---- BEFORE: the planar path the routine replaces ---- */

static void before_fwd(const float p[3], float w, float out[4]) {   /* one FTRV, copied out through memory */
    const shz_vec4_t v = shz_xmtrx_transform_vec4(shz_vec4_init(p[0], p[1], p[2], w));

    out[0] = v.x; out[1] = v.y; out[2] = v.z; out[3] = v.w;
}

static void before_plain(const float m[16], const float p[3], float w, float out[4]) {   /* the library's plain row form, for the fallback */
    int c;

    for (c = 0; c < 4; c++) out[c] = p[0] * m[c] + p[1] * m[4 + c] + p[2] * m[8 + c] + w * m[12 + c];
}

static int before_tap_lit(const shz_dce_shadow_map_t *m, int x, int y, unsigned ref) {
    if (x < 0 || y < 0 || x >= (int)m->size || y >= (int)m->size) return 1;
    return m->depth[(size_t)y * m->size + (unsigned)x] <= ref;
}

/* the point-only compare, for a degenerate solve: the library's lit read, with the plain transform of the point */
static float before_lit(const shz_dce_shadow_map_t *m, const float M[16], const float x[3], float bias) {
    float h[4], iw, tx, ty, fx, fy, k;
    unsigned ref;
    int ix, iy;

    before_plain(M, x, 1.0f, h);
    if (!shz_dce_shadow_finite_(h[3]) || !(h[3] > m->w_eps || h[3] < -m->w_eps)) return 1.0f;
    iw = shz_divf(1.0f, h[3]);
    tx = (h[0] * iw + 1.0f) * 0.5f * (float)m->size;
    ty = (h[1] * iw + 1.0f) * 0.5f * (float)m->size;
    if (!(tx > -1.0f) || !(ty > -1.0f) || !(tx < (float)m->size + 1.0f) || !(ty < (float)m->size + 1.0f)) return 1.0f;
    k = shz_clampf(h[2] + bias * m->key_scale, 0.0f, 65535.0f);
    ref = (unsigned)(int)k;
    if (!m->pcf) return before_tap_lit(m, (int)(tx + 1.0f) - 1, (int)(ty + 1.0f) - 1, ref);
    tx += 1.5f; ty += 1.5f;
    ix = (int)tx; iy = (int)ty;
    fx = tx - (float)ix; fy = ty - (float)iy;
    ix -= 2; iy -= 2;
    return shz_lerpf(shz_lerpf((float)before_tap_lit(m, ix, iy, ref), (float)before_tap_lit(m, ix + 1, iy, ref), fx),
                     shz_lerpf((float)before_tap_lit(m, ix, iy + 1, ref), (float)before_tap_lit(m, ix + 1, iy + 1, ref), fx), fy);
}

static float before_texel(const shz_dce_shadow_map_t *m, const float h[4], float iw, const float g1[4], const float g2[4], float delta) {
    const float half = 0.5f * (float)m->size, u0 = h[0] * iw, v0 = h[1] * iw;
    const float a3 = h[3] + delta * g1[3], b3 = h[3] + delta * g2[3];
    float ia, ib, u1, v1, u2, v2, area;

    if (!shz_dce_shadow_finite_(h[3]) || !(h[3] > m->w_eps || h[3] < -m->w_eps)) return 1e30f;
    if (!shz_dce_shadow_finite_(a3) || !(a3 > m->w_eps || a3 < -m->w_eps)) return 1e30f;
    if (!shz_dce_shadow_finite_(b3) || !(b3 > m->w_eps || b3 < -m->w_eps)) return 1e30f;
    ia = shz_divf(1.0f, a3);
    ib = shz_divf(1.0f, b3);
    u1 = ((h[0] + delta * g1[0]) * ia - u0) * half; v1 = ((h[1] + delta * g1[1]) * ia - v0) * half;
    u2 = ((h[0] + delta * g2[0]) * ib - u0) * half; v2 = ((h[1] + delta * g2[1]) * ib - v0) * half;
    area = u1 * v2 - v1 * u2;
    if (area < 0.0f) area = -area;
    if (!(area > 1e-12f)) return 1e30f;
    return shz_sqrtf(shz_divf(delta * delta, area));
}

static float before_plane(const shz_dce_shadow_map_t *m, const float M[16], const float x[3], const float h[4], float iw,
                          const float g1[4], const float g2[4], float bias) {
    const float half = 0.5f * (float)m->size, cap = m->slope_max * m->key_scale;
    float s[3][3], det, kx, ky, tx, ty, fx, fy, k0, tap[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
    int i, ix, iy, taps;

    s[0][0] = (h[0] * iw + 1.0f) * half; s[0][1] = (h[1] * iw + 1.0f) * half; s[0][2] = h[2];
    for (i = 1; i < 3; i++) {
        const float *g = i == 1 ? g1 : g2;

        s[i][0] = (g[0] - h[0] * iw * g[3]) * iw * half;
        s[i][1] = (g[1] - h[1] * iw * g[3]) * iw * half;
        s[i][2] = g[2];
    }
    det = s[1][0] * s[2][1] - s[1][1] * s[2][0];
    if (!shz_dce_shadow_finite_(det) || !(det > 1e-12f || det < -1e-12f)) return before_lit(m, M, x, bias);
    det = shz_divf(1.0f, det);
    kx = (s[1][2] * s[2][1] - s[2][2] * s[1][1]) * det;
    ky = (s[2][2] * s[1][0] - s[1][2] * s[2][0]) * det;
    tx = s[0][0]; ty = s[0][1];
    if (!(tx > -1.0f) || !(ty > -1.0f) || !(tx < (float)m->size + 1.0f) || !(ty < (float)m->size + 1.0f)) return 1.0f;
    k0 = s[0][2] + bias * m->key_scale;
    if (m->pcf) {
        tx += 1.5f; ty += 1.5f;
        ix = (int)tx; iy = (int)ty;
        fx = tx - (float)ix; fy = ty - (float)iy;
        ix -= 2; iy -= 2;
        tx -= 1.5f; ty -= 1.5f;
        taps = 4;
    } else {
        ix = (int)(tx + 1.0f) - 1; iy = (int)(ty + 1.0f) - 1;
        fx = fy = 0.0f;
        taps = 1;
    }
    for (i = 0; i < taps; i++) {
        const int cx = ix + (i & 1), cy = iy + (i >> 1);
        const float slope = shz_clampf(kx * ((float)cx + 0.5f - tx) + ky * ((float)cy + 0.5f - ty), -cap, cap);

        tap[i] = (float)before_tap_lit(m, cx, cy, (unsigned)(int)shz_clampf(k0 + slope, 0.0f, 65535.0f));
    }
    if (!m->pcf) return tap[0];
    return shz_lerpf(shz_lerpf(tap[0], tap[1], fx), shz_lerpf(tap[2], tap[3], fx), fy);
}

static void before_batch(uint8_t *out, size_t out_stride, const float *pos, size_t stride, size_t n, const float M[16], const float t1[3],
                         const float t2[3], const shz_dce_shadow_map_t *m) {
    size_t i;

    shz_xmtrx_load_unaligned_4x4(M);
    for (i = 0; i < n; i++) {
        const float *x = (const float *)((const char *)pos + i * stride);
        float h[4], g1[4], g2[4], iw, v;

        before_fwd(x, 1.0f, h);
        before_fwd(t1, 0.0f, g1);
        before_fwd(t2, 0.0f, g2);
        if (!shz_dce_shadow_finite_(h[3]) || !(h[3] > m->w_eps || h[3] < -m->w_eps)) {
            v = 1.0f;
        } else {
            float bias = m->bias_a;

            iw = shz_divf(1.0f, h[3]);
            if (m->bias_b != 0.0f) {
                float texel = before_texel(m, h, iw, g1, g2, m->texel_step);

                if (!(texel < m->texel_max)) texel = m->texel_max;
                bias = m->bias_a + m->bias_b * texel;
            }
            v = before_plane(m, M, x, h, iw, g1, g2, bias);
        }
        out[i * out_stride] = (uint8_t)(int)(v * 255.0f + 0.5f);
    }
}

/* ---- the job ---- */

void t_shadow(job_shadow_t *j) {
    const shz_dce_shadow_map_t *map = P(const shz_dce_shadow_map_t *, j->map);
    const float *M = P(const float *, j->matrix), *t1 = P(const float *, j->t1), *t2 = P(const float *, j->t2);
    const float *pts = P(const float *, j->points);
    float g1[4], g2[4];
    shz_vec4_t a, b;

    shz_xmtrx_load_unaligned_4x4(M);
    a = shz_xmtrx_transform_vec4(shz_vec4_init(t1[0], t1[1], t1[2], 0.0f));   /* the caller's axes, once per batch */
    b = shz_xmtrx_transform_vec4(shz_vec4_init(t2[0], t2[1], t2[2], 0.0f));
    g1[0] = a.x; g1[1] = a.y; g1[2] = a.z; g1[3] = a.w;
    g2[0] = b.x; g2[1] = b.y; g2[2] = b.z; g2[3] = b.w;
    shz_dce_shadow_compare_batch(P(uint8_t *, j->out_asm), j->out_stride, pts, j->stride, j->count, map, g1, g2);
    shz_dce_shadow_compare_batch_sh4(P(uint8_t *, j->out_sh4), j->out_stride, pts, j->stride, j->count, map, g1, g2);
    shz_dce_shadow_compare_batch_c(P(uint8_t *, j->out_c), j->out_stride, pts, j->stride, j->count, map, g1, g2);
    before_batch(P(uint8_t *, j->out_before), j->out_stride, pts, j->stride, j->count, M, t1, t2, map);
}
