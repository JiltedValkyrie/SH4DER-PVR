/* SH4ZAM-DCE tests: shz_dce_shadow.h, the loops its cycle ESTIMATES are made from

   shadow_bench.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   Built by sh-elf-gcc with the fast-math flags and run by tests/sh4run/sh4bench: every bench_ function is
   `void f(void *out, const void *in0, const void *in1, size_t count [, float t])` on buffers of random words. Here in0 is the points
   (x y z at 12 bytes a point) and in1 the depth map's texels (the first 32 x 32 uint16_t are used; the random words are a valid map).
   Each routine is paired with the planar path it replaces (bench_shadow_planar_before*): the same matrix in XMTRX, the same axes, the
   same answers. The figures are the interpreter's pipeline model (an ESTIMATE, cache hits assumed), not a console's.
   Both sides load the map's matrix inside the timed call (one load a call, the same on both sides).
*/

#include <sh4zam_dce/shz_dce_shadow.h>

#define R SHZ_RESTRICT
#define BENCH_SIZE 32u

static const float BM[16] = {
    0.61f, -0.22f, 0.10f, 0.02f,  0.18f, 0.74f, -0.31f, 0.01f,  -0.05f, 0.27f, 0.58f, -0.02f,  0.12f, -0.40f, 31000.0f, 1.20f
};
static const float BT1[3] = { 0.8f, 0.0f, 0.6f }, BT2[3] = { 0.0f, 0.0f, 1.0f };

/* ---- the planar path the routine replaces: per point, one FTRV for the point and one for each axis, copied out through memory ---- */

static void before_fwd(const float p[3], float w, float out[4]) {
    const shz_vec4_t v = shz_xmtrx_transform_vec4(shz_vec4_init(p[0], p[1], p[2], w));

    out[0] = v.x; out[1] = v.y; out[2] = v.z; out[3] = v.w;
}

static void before_plain(const float m[16], const float p[3], float w, float out[4]) {
    int c;

    for (c = 0; c < 4; c++) out[c] = p[0] * m[c] + p[1] * m[4 + c] + p[2] * m[8 + c] + w * m[12 + c];
}

static int before_tap_lit(const shz_dce_shadow_map_t *m, int x, int y, unsigned ref) {
    if (x < 0 || y < 0 || x >= (int)m->size || y >= (int)m->size) return 1;
    return m->depth[(size_t)y * m->size + (unsigned)x] <= ref;
}

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
    if (!m->pcf) return (float)before_tap_lit(m, (int)(tx + 1.0f) - 1, (int)(ty + 1.0f) - 1, ref);
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

static void before_batch(uint8_t *out, const float *pos, size_t n, const shz_dce_shadow_map_t *m) {
    size_t i;

    shz_xmtrx_load_unaligned_4x4(BM);
    for (i = 0; i < n; i++) {
        const float *x = (const float *)((const char *)pos + i * 12u);
        float h[4], g1[4], g2[4], v;

        before_fwd(x, 1.0f, h);
        before_fwd(BT1, 0.0f, g1);
        before_fwd(BT2, 0.0f, g2);
        if (!shz_dce_shadow_finite_(h[3]) || !(h[3] > m->w_eps || h[3] < -m->w_eps)) {
            v = 1.0f;
        } else {
            float bias = m->bias_a;
            const float iw = shz_divf(1.0f, h[3]);

            if (m->bias_b != 0.0f) {
                float texel = before_texel(m, h, iw, g1, g2, m->texel_step);

                if (!(texel < m->texel_max)) texel = m->texel_max;
                bias = m->bias_a + m->bias_b * texel;
            }
            v = before_plane(m, BM, x, h, iw, g1, g2, bias);
        }
        out[i] = (uint8_t)(int)(v * 255.0f + 0.5f);
    }
}

/* ---- the routine: the map from in1, the axes transformed once per call (the caller's, as on the planar path's per-point FTRVs) ---- */

static shz_dce_shadow_map_t bench_map(const void *depth, int pcf, float bias_b) {
    shz_dce_shadow_map_t m;

    m.depth = (const uint16_t *)depth; m.size = BENCH_SIZE; m.key_scale = 3000.0f; m.slope_max = 8.0f;
    m.bias_a = 0.0f; m.bias_b = bias_b; m.texel_max = 5.0f; m.texel_step = 0.02f; m.w_eps = 1.0e-6f; m.pcf = pcf;
    return m;
}

static void bench_axes(float g1[4], float g2[4]) {
    const shz_vec4_t a = shz_xmtrx_transform_vec4(shz_vec4_init(BT1[0], BT1[1], BT1[2], 0.0f));
    const shz_vec4_t b = shz_xmtrx_transform_vec4(shz_vec4_init(BT2[0], BT2[1], BT2[2], 0.0f));

    g1[0] = a.x; g1[1] = a.y; g1[2] = a.z; g1[3] = a.w;
    g2[0] = b.x; g2[1] = b.y; g2[2] = b.z; g2[3] = b.w;
}

/* ---- the pairs: the kernel (dispatch, plain form) and the planar path it replaces, the full compare and the plain one ---- */

void bench_shadow_compare(uint8_t *R out, const float *R points, const void *depth, size_t n) {
    const shz_dce_shadow_map_t m = bench_map(depth, 1, 1.5f);
    float g1[4], g2[4];

    shz_xmtrx_load_unaligned_4x4(BM);
    bench_axes(g1, g2);
    shz_dce_shadow_compare_batch(out, 1, points, 12, n, &m, g1, g2);
}

void bench_shadow_compare_c(uint8_t *R out, const float *R points, const void *depth, size_t n) {
    const shz_dce_shadow_map_t m = bench_map(depth, 1, 1.5f);
    float g1[4], g2[4];

    shz_xmtrx_load_unaligned_4x4(BM);
    bench_axes(g1, g2);
    shz_dce_shadow_compare_batch_c(out, 1, points, 12, n, &m, g1, g2);
}

void bench_shadow_planar_before(uint8_t *R out, const float *R points, const void *depth, size_t n) {
    const shz_dce_shadow_map_t m = bench_map(depth, 1, 1.5f);

    before_batch(out, points, n, &m);
}

void bench_shadow_compare_plain(uint8_t *R out, const float *R points, const void *depth, size_t n) {
    const shz_dce_shadow_map_t m = bench_map(depth, 0, 0.0f);
    float g1[4], g2[4];

    shz_xmtrx_load_unaligned_4x4(BM);
    bench_axes(g1, g2);
    shz_dce_shadow_compare_batch(out, 1, points, 12, n, &m, g1, g2);
}

void bench_shadow_planar_before_plain(uint8_t *R out, const float *R points, const void *depth, size_t n) {
    const shz_dce_shadow_map_t m = bench_map(depth, 0, 0.0f);

    before_batch(out, points, n, &m);
}
