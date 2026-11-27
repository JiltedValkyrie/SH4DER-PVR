/* SH4ZAM-DCE tests: shz_dce_shadow.h on the host (SH4ZAM's software back end)

   shadow_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked:
   - every output byte against an exact reference written here in double: the same algorithm, its own code, no float in it. A byte may
     differ only when one of the point's decisions (the singularity, a texel edge, the solve's determinant, the texel measure's clamp, a
     depth level, the byte's rounding) lies inside the margin float rounding can move; the count of such bytes is printed
   - outside the map, at the light's singularity, NaN and infinite points: lit (255)
   - a uniform map: every tap equal, so pcf gives that tap's value exactly (0 or 255)
   - the batch against the per-point call for every count from 0 to 37, strides 12, 16, 20 and 32, out strides 1 and 3; no byte the
     batch does not own is touched (before, between, or after its outputs), and nothing is written past the last byte of the batch
   - XMTRX holds the map's matrix after every call (the routine reads it, it does not change it)
   - the routine against the planar loop it replaces (the point and each axis one FTRV, copied out through memory; the same operations):
     zero bytes differ in strict builds; under -ffast-math the count is printed and not claimed (KERNEL_GUIDE.md section 8)
*/

#include <sh4zam_dce/shz_dce_shadow.h>

#include "shz_dce_test.h"

static uint16_t depth_buf[64 * 64];

static void random_map(shz_dce_shadow_map_t *m, float *key) {
    static const unsigned sizes[] = { 2, 3, 4, 8, 16, 33, 64 };
    const unsigned n = sizes[t_rand() % 7u];
    unsigned i;

    for (i = 0; i < n * n; i++) {
        const uint32_t r = t_rand();
        depth_buf[i] = (r & 3u) == 0u ? 0u : (uint16_t)(r >> 8);
    }
    *key = t_frand(200.0f, 6000.0f);
    m->depth = depth_buf;
    m->size = n;
    m->key_scale = *key;
    m->slope_max = 8.0f;
    m->bias_a = t_frand(-0.05f, 0.05f);
    m->bias_b = (t_rand() & 1u) ? 1.5f : 0.0f;
    m->texel_max = 5.0f;
    m->texel_step = 0.02f;
    m->w_eps = 1.0e-6f;
    m->pcf = (int)(t_rand() & 1u);
}

/* A random 4 x 4 map matrix in the layout the map code uses: columns 0 .. 2 are the X, Y, K contributions, column 3 the W row. */
static void random_matrix(float M[16], float key) {
    int i;

    for (i = 0; i < 16; i++) M[i] = t_frand(-1.0f, 1.0f);
    M[2] = key * t_frand(-0.3f, 0.3f); M[6] = key * t_frand(-0.3f, 0.3f); M[10] = key * t_frand(-0.3f, 0.3f);
    M[14] = 32768.0f + t_frand(-2000.0f, 2000.0f);
    M[15] = t_frand(0.5f, 2.0f);
}

static void random_axis(float t[3]) {
    float len;
    do {
        t[0] = t_frand(-1.0f, 1.0f); t[1] = t_frand(-1.0f, 1.0f); t[2] = t_frand(-1.0f, 1.0f);
        len = sqrtf(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
    } while (len < 0.1f);
    t[0] /= len; t[1] /= len; t[2] /= len;
}

/* The axes as the caller passes them: FTRV of (t, 0) with the map's matrix in XMTRX, once per batch. */
static void transform_axes(const float t1[3], const float t2[3], float g1[4], float g2[4]) {
    const shz_vec4_t a = shz_xmtrx_transform_vec4(shz_vec4_init(t1[0], t1[1], t1[2], 0.0f));
    const shz_vec4_t b = shz_xmtrx_transform_vec4(shz_vec4_init(t2[0], t2[1], t2[2], 0.0f));

    g1[0] = a.x; g1[1] = a.y; g1[2] = a.z; g1[3] = a.w;
    g2[0] = b.x; g2[1] = b.y; g2[2] = b.z; g2[3] = b.w;
}

/* The exact answer for one point, in double, from the algorithm's own steps. *near is set when a decision lies inside the margin:
   the reference cannot tell whether the header's float answer falls the other way there. */
static double reference(const shz_dce_shadow_map_t *m, const float M[16], const float p[3], const float t1[3], const float t2[3], int *near) {
    const double half = 0.5 * (double)m->size, mk = (double)m->key_scale, cap = (double)m->slope_max * mk;
    double h[4], g1[4], g2[4], s[3][3], W, iw, tx, ty, k0, bias, texel, det, kx, ky, fx = 0.0, fy = 0.0, tap[4] = { 1.0, 1.0, 1.0, 1.0 }, v;
    int j, i, taps, ix = 0, iy = 0;

    *near = 0;
    for (j = 0; j < 4; j++) {
        h[j] = (double)p[0] * M[j] + (double)p[1] * M[4 + j] + (double)p[2] * M[8 + j] + (double)M[12 + j];
        g1[j] = (double)t1[0] * M[j] + (double)t1[1] * M[4 + j] + (double)t1[2] * M[8 + j];
        g2[j] = (double)t2[0] * M[j] + (double)t2[1] * M[4 + j] + (double)t2[2] * M[8 + j];
    }
    W = h[3];
    if (fabs(W) < 1.0e-2) *near = 1;               /* the reciprocal of a small W magnifies every rounding */
    if (!(fabs(W) > (double)m->w_eps)) return 1.0;  /* the light's own plane: lit */
    iw = 1.0 / W;

    bias = (double)m->bias_a;
    if (m->bias_b != 0.0f) {                       /* the texel size: a forward difference of step texel_step along each axis */
        const double delta = (double)m->texel_step, a3 = W + delta * g1[3], b3 = W + delta * g2[3];
        const double u0 = h[0] * iw, v0 = h[1] * iw;
        const double u1 = ((h[0] + delta * g1[0]) / a3 - u0) * half, w1 = ((h[1] + delta * g1[1]) / a3 - v0) * half;
        const double u2 = ((h[0] + delta * g2[0]) / b3 - u0) * half, w2 = ((h[1] + delta * g2[1]) / b3 - v0) * half;
        const double area = fabs(u1 * w2 - w1 * u2);

        if (fabs(a3) < 1.0e-2 || fabs(b3) < 1.0e-2) *near = 1;
        if (area < 1.0e-9) *near = 1;
        texel = area > 1.0e-12 ? sqrt(delta * delta / area) : 1.0e30;
        if (!(texel < (double)m->texel_max)) texel = (double)m->texel_max;
        if (fabs(texel - (double)m->texel_max) < 1.0e-4 * (double)m->texel_max) *near = 1;
        bias = (double)m->bias_a + (double)m->bias_b * texel;
    }

    s[0][0] = (h[0] * iw + 1.0) * half; s[0][1] = (h[1] * iw + 1.0) * half; s[0][2] = h[2];
    for (i = 1; i <= 2; i++) {
        const double *g = i == 1 ? g1 : g2;
        s[i][0] = (g[0] - h[0] * iw * g[3]) * iw * half;
        s[i][1] = (g[1] - h[1] * iw * g[3]) * iw * half;
        s[i][2] = g[2];
    }
    tx = s[0][0]; ty = s[0][1];
    k0 = s[0][2] + bias * mk;

    det = s[1][0] * s[2][1] - s[1][1] * s[2][0];
    if (!(fabs(det) > 1.0e-12)) {                   /* degenerate solve: the point-only compare, no slope */
        if (!(tx > -1.0) || !(ty > -1.0) || !(tx < (double)m->size + 1.0) || !(ty < (double)m->size + 1.0)) return 1.0;
        kx = ky = 0.0;
    } else {
        if (fabs(det) < 1.0e-6 * (fabs(s[1][0]) + fabs(s[1][1])) * (fabs(s[2][0]) + fabs(s[2][1])) + 1.0e-12) *near = 1;
        kx = (s[1][2] * s[2][1] - s[2][2] * s[1][1]) / det;
        ky = (s[2][2] * s[1][0] - s[1][2] * s[2][0]) / det;
        if (!(tx > -1.0) || !(ty > -1.0) || !(tx < (double)m->size + 1.0) || !(ty < (double)m->size + 1.0)) {
            if (fabs(tx + 1.0) < 1e-3 || fabs(ty + 1.0) < 1e-3 || fabs(tx - (double)m->size - 1.0) < 1e-3 || fabs(ty - (double)m->size - 1.0) < 1e-3)
                *near = 1;
            return 1.0;
        }
    }
    /* a tap's floor flips when 2 tx (or 2 ty) is a whole number: the texel edges sit on half-integers of tx */
    if (fabs(2.0 * tx - floor(2.0 * tx + 0.5)) < 2e-3 || fabs(2.0 * ty - floor(2.0 * ty + 0.5)) < 2e-3) *near = 1;

    if (m->pcf) {
        const double tx1 = tx + 1.5, ty1 = ty + 1.5;
        ix = (int)floor(tx1) - 2; iy = (int)floor(ty1) - 2;
        fx = tx1 - floor(tx1); fy = ty1 - floor(ty1);
        taps = 4;
    } else {
        ix = (int)floor(tx); iy = (int)floor(ty);
        taps = 1;
    }
    for (i = 0; i < taps; i++) {
        const int cx = ix + (i & 1), cy = iy + (i >> 1);
        double slope = kx * ((double)cx + 0.5 - tx) + ky * ((double)cy + 0.5 - ty), kf;

        if (slope < -cap) slope = -cap;
        if (slope > cap) slope = cap;
        kf = k0 + slope;
        if (kf < 0.0) kf = 0.0;
        if (kf > 65535.0) kf = 65535.0;
        if (cx < 0 || cy < 0 || cx >= (int)m->size || cy >= (int)m->size) {
            tap[i] = 1.0;
        } else {
            const double stored = (double)m->depth[(size_t)cy * m->size + (size_t)cx];
            /* stored <= floor(kf) is stored <= kf (stored is an integer); the margin is float error in kf, scaled by the terms it sums */
            const double margin = 1.0e-4 * (1.0 + fabs(k0) + (double)m->size * (fabs(kx) + fabs(ky)) + fabs(bias) * mk);
            if (fabs(kf - stored) <= margin) *near = 1;
            tap[i] = stored <= kf ? 1.0 : 0.0;
        }
    }
    if (!m->pcf) return tap[0];
    v = (tap[0] + (tap[1] - tap[0]) * fx) * (1.0 - fy) + (tap[2] + (tap[3] - tap[2]) * fx) * fy;
    {
        const double r = v * 255.0 + 0.5;
        if (fabs(r - floor(r) ) < 1e-3 || fabs(r - floor(r) - 1.0) < 1e-3) *near = 1;   /* the byte's rounding */
    }
    return v;
}

static uint8_t expected_byte(double v) { return (uint8_t)(int)floor(v * 255.0 + 0.5); }

/* The planar loop the routine replaces, operation for operation (the shadow-map library's planar batch: the point and each axis
   one FTRV, copied out through memory, then the texel measure, the plane compare and the tap reads). Here only to compare bytes
   with the routine on the software back end. */
/* ---- BEFORE: the planar path the routine replaces ---- */

static void planar_fwd(const float p[3], float w, float out[4]) {   /* one FTRV, copied out through memory */
    const shz_vec4_t v = shz_xmtrx_transform_vec4(shz_vec4_init(p[0], p[1], p[2], w));

    out[0] = v.x; out[1] = v.y; out[2] = v.z; out[3] = v.w;
}

static void planar_plain(const float m[16], const float p[3], float w, float out[4]) {   /* the library's plain row form, for the fallback */
    int c;

    for (c = 0; c < 4; c++) out[c] = p[0] * m[c] + p[1] * m[4 + c] + p[2] * m[8 + c] + w * m[12 + c];
}

static int planar_tap_lit(const shz_dce_shadow_map_t *m, int x, int y, unsigned ref) {
    if (x < 0 || y < 0 || x >= (int)m->size || y >= (int)m->size) return 1;
    return m->depth[(size_t)y * m->size + (unsigned)x] <= ref;
}

/* the point-only compare, for a degenerate solve: the library's lit read, with the plain transform of the point */
static float planar_lit(const shz_dce_shadow_map_t *m, const float M[16], const float x[3], float bias) {
    float h[4], iw, tx, ty, fx, fy, k;
    unsigned ref;
    int ix, iy;

    planar_plain(M, x, 1.0f, h);
    if (!shz_dce_shadow_finite_(h[3]) || !(h[3] > m->w_eps || h[3] < -m->w_eps)) return 1.0f;
    iw = shz_divf(1.0f, h[3]);
    tx = (h[0] * iw + 1.0f) * 0.5f * (float)m->size;
    ty = (h[1] * iw + 1.0f) * 0.5f * (float)m->size;
    if (!(tx > -1.0f) || !(ty > -1.0f) || !(tx < (float)m->size + 1.0f) || !(ty < (float)m->size + 1.0f)) return 1.0f;
    k = shz_clampf(h[2] + bias * m->key_scale, 0.0f, 65535.0f);
    ref = (unsigned)(int)k;
    if (!m->pcf) return planar_tap_lit(m, (int)(tx + 1.0f) - 1, (int)(ty + 1.0f) - 1, ref);
    tx += 1.5f; ty += 1.5f;
    ix = (int)tx; iy = (int)ty;
    fx = tx - (float)ix; fy = ty - (float)iy;
    ix -= 2; iy -= 2;
    return shz_lerpf(shz_lerpf((float)planar_tap_lit(m, ix, iy, ref), (float)planar_tap_lit(m, ix + 1, iy, ref), fx),
                     shz_lerpf((float)planar_tap_lit(m, ix, iy + 1, ref), (float)planar_tap_lit(m, ix + 1, iy + 1, ref), fx), fy);
}

static float planar_texel(const shz_dce_shadow_map_t *m, const float h[4], float iw, const float g1[4], const float g2[4], float delta) {
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

static float planar_plane(const shz_dce_shadow_map_t *m, const float M[16], const float x[3], const float h[4], float iw,
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
    if (!shz_dce_shadow_finite_(det) || !(det > 1e-12f || det < -1e-12f)) return planar_lit(m, M, x, bias);
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

        tap[i] = (float)planar_tap_lit(m, cx, cy, (unsigned)(int)shz_clampf(k0 + slope, 0.0f, 65535.0f));
    }
    if (!m->pcf) return tap[0];
    return shz_lerpf(shz_lerpf(tap[0], tap[1], fx), shz_lerpf(tap[2], tap[3], fx), fy);
}

static void planar_batch(uint8_t *out, size_t out_stride, const float *pos, size_t stride, size_t n, const float M[16], const float t1[3],
                         const float t2[3], const shz_dce_shadow_map_t *m) {
    size_t i;

    shz_xmtrx_load_unaligned_4x4(M);
    for (i = 0; i < n; i++) {
        const float *x = (const float *)((const char *)pos + i * stride);
        float h[4], g1[4], g2[4], iw, v;

        planar_fwd(x, 1.0f, h);
        planar_fwd(t1, 0.0f, g1);
        planar_fwd(t2, 0.0f, g2);
        if (!shz_dce_shadow_finite_(h[3]) || !(h[3] > m->w_eps || h[3] < -m->w_eps)) {
            v = 1.0f;
        } else {
            float bias = m->bias_a;

            iw = shz_divf(1.0f, h[3]);
            if (m->bias_b != 0.0f) {
                float texel = planar_texel(m, h, iw, g1, g2, m->texel_step);

                if (!(texel < m->texel_max)) texel = m->texel_max;
                bias = m->bias_a + m->bias_b * texel;
            }
            v = planar_plane(m, M, x, h, iw, g1, g2, bias);
        }
        out[i * out_stride] = (uint8_t)(int)(v * 255.0f + 0.5f);
    }
}


int main(int argc, char **argv) {
    (void)argc; (void)argv;
    static float pts[64 * 8];
    static float M[16], back[16], g1[4], g2[4], t1[3], t2[3];
    static uint8_t out_batch[64 * 4 + 8], out_one[64 * 4 + 8];
    shz_dce_shadow_map_t map;
    float key;
    int trial, mismatches = 0, band = 0, lit_n = 0, shadow_n = 0, degenerate_n = 0, planar_diff = 0;

    /* -- the reference against the routine: random maps, matrices, axes and points -- */
    for (trial = 0; trial < 1500; trial++) {
        int k;
        random_map(&map, &key);
        random_matrix(M, key);
        random_axis(t1);
        random_axis(t2);
        if (trial % 9 == 0) { t2[0] = t1[0]; t2[1] = t1[1]; t2[2] = t1[2]; }        /* the same axis twice: a zero determinant, exactly */
        if (trial % 9 == 4) { t1[0] = t1[1] = t1[2] = t2[0] = t2[1] = t2[2] = 0.0f; } /* zero-length axes: the same fallback */
        shz_xmtrx_load_unaligned_4x4(M);
        transform_axes(t1, t2, g1, g2);
        for (k = 0; k < 64; k++) {
            uint8_t b_k, p_k;
            int near = 0;
            double v;

            pts[0] = t_frand(-2.0f, 2.0f); pts[1] = t_frand(-2.0f, 2.0f); pts[2] = t_frand(-2.0f, 2.0f);
            shz_dce_shadow_compare_batch(&b_k, 1, pts, 12, 1, &map, g1, g2);
            planar_batch(&p_k, 1, pts, 12, 1, M, t1, t2, &map);
            if (b_k != p_k) planar_diff++;
            v = reference(&map, M, pts, t1, t2, &near);
            if (v == 1.0) lit_n++; else shadow_n++;
            if (trial % 9 == 0 || trial % 9 == 4) degenerate_n++;
            if (b_k != expected_byte(v)) {
                mismatches++;
                if (near) band++;
                else T_CHECK(0, "point (%g %g %g): byte %u, exact %u, no decision near its margin (pcf %d, bias_b %g, size %u)",
                             (double)pts[0], (double)pts[1], (double)pts[2], (unsigned)b_k, (unsigned)expected_byte(v), map.pcf,
                             (double)map.bias_b, (unsigned)map.size);
            }
        }
        shz_xmtrx_store_unaligned_4x4(back);
        T_CHECK(!memcmp(back, M, sizeof M), "XMTRX changed by the routine (trial %d)", trial);
    }
    printf("shadow: %d points, %d lit, %d shadowed; %d bytes differ from the exact answer, all inside a decision's margin; %d points on degenerate axes\n",
           lit_n + shadow_n, lit_n, shadow_n, band, degenerate_n);
    T_CHECK(mismatches == band, "a byte outside every margin");
    printf("shadow: %d bytes differ from the planar loop the routine replaces (software back end)\n", planar_diff);
#ifdef __FAST_MATH__
    /* -ffast-math enables -funsafe-math-optimizations, and the compiler then folds SH4ZAM's software reciprocal (1 / sqrt(x x)) differently
       in the two inline contexts: the identity is not claimed here (KERNEL_GUIDE.md section 8). It holds with -fno-unsafe-math-optimizations. */
    (void)planar_diff;
#else
    T_CHECK(planar_diff == 0, "the routine differs from the planar loop on %d points", planar_diff);
#endif
    T_CHECK(shadow_n > 1000 && lit_n > 1000, "the random points cover both answers");

    /* -- a map that is the identity in X and Y, K = 0, W = 1: outside the map lit, inside shadowed by a full map -- */
    {
        static uint16_t full[16];
        static const float I[16] = { 1, 0, 0, 0,  0, 1, 0, 0,  0, 0, 0, 0,  0, 0, 0, 1 };   /* (x, y, z, 1) -> (x, y, 0, 1) */
        static const float ax[3] = { 1, 0, 0 }, az[3] = { 0, 1, 0 };
        static const float ay[3] = { 0, 0, 1 };
        shz_dce_shadow_map_t m2;
        float gx[4], gz[4], gy[4], Z[16];
        unsigned i;

        for (i = 0; i < 16; i++) full[i] = 65535u;
        m2.depth = full; m2.size = 4; m2.key_scale = 1.0f; m2.slope_max = 8.0f; m2.bias_a = 0.0f; m2.bias_b = 0.0f;
        m2.texel_max = 5.0f; m2.texel_step = 0.02f; m2.w_eps = 1.0e-6f; m2.pcf = 0;

        shz_xmtrx_load_unaligned_4x4(I);
        transform_axes(ax, az, gx, gz);
        (void)ay; (void)gy;
        {   /* the points: tx = (x + 1) x 2 for these (texel 0 .. 3 inside the map); -0.5 and 0.0 and 0.9 inside, 1.1 beyond the edge */
            static float q[4][3] = { { -0.5f, 0.0f, 0.0f }, { 0.0f, 0.0f, 0.0f }, { 0.9f, 0.0f, 0.0f }, { 1.1f, 0.0f, 0.0f } };
            uint8_t res[4];
            shz_dce_shadow_compare_batch(res, 1, &q[0][0], 12, 4, &m2, gx, gz);
            T_CHECK(res[0] == 0 && res[1] == 0 && res[2] == 0 && res[3] == 255, "inside shadowed, beyond the edge lit: got %u %u %u %u",
                    res[0], res[1], res[2], res[3]);
        }
        {   /* a singular map row: W is 0 for every point: all lit */
            static float q[2][3] = { { 0.1f, 0.2f, 0.3f }, { -0.4f, 0.5f, 0.6f } };
            uint8_t res[2];
            memcpy(Z, I, sizeof Z);
            Z[3] = Z[7] = Z[11] = Z[15] = 0.0f;
            shz_xmtrx_load_unaligned_4x4(Z);
            shz_dce_shadow_compare_batch(res, 1, &q[0][0], 12, 2, &m2, gx, gz);
            T_CHECK(res[0] == 255 && res[1] == 255, "singular W lit: got %u %u", res[0], res[1]);
            shz_xmtrx_load_unaligned_4x4(I);
        }
        {   /* NaN and infinite points: lit */
            static float q[3][3];
            uint8_t res[3];
            q[0][0] = t_float(0x7fc00000u); q[0][1] = 0.0f; q[0][2] = 0.0f;
            q[1][0] = 0.0f; q[1][1] = t_float(0x7f800000u); q[1][2] = 0.0f;
            q[2][0] = 0.0f; q[2][1] = 0.0f; q[2][2] = t_float(0xff800000u);
            shz_dce_shadow_compare_batch(res, 1, &q[0][0], 12, 3, &m2, gx, gz);
            T_CHECK(res[0] == 255 && res[1] == 255 && res[2] == 255, "NaN and infinite points lit: got %u %u %u", res[0], res[1], res[2]);
        }
        {   /* uniform maps: every tap equal, so the four-tap blend gives that tap exactly, with pcf on and off */
            static uint16_t empty[64 * 64];
            static float q[2][3] = { { -0.3f, 0.2f, 0.0f }, { 0.4f, -0.6f, 0.0f } };
            uint8_t res[2];
            m2.pcf = 1;
            m2.depth = empty; m2.size = 4;   /* every stored depth 0: each tap is lit */
            shz_dce_shadow_compare_batch(res, 1, &q[0][0], 12, 2, &m2, gx, gz);
            T_CHECK(res[0] == 255 && res[1] == 255, "uniform empty map, pcf: got %u %u", res[0], res[1]);
            m2.depth = full;                 /* every stored depth 65535, reference 0: each tap is shadowed */
            shz_dce_shadow_compare_batch(res, 1, &q[0][0], 12, 2, &m2, gx, gz);
            T_CHECK(res[0] == 0 && res[1] == 0, "uniform full map, pcf: got %u %u", res[0], res[1]);
            m2.pcf = 0;
            shz_dce_shadow_compare_batch(res, 1, &q[0][0], 12, 2, &m2, gx, gz);
            T_CHECK(res[0] == 0 && res[1] == 0, "uniform full map, one tap: got %u %u", res[0], res[1]);
        }
    }

    /* -- the batch against the per-point call: every count, stride and out stride; the bytes it does not own are untouched -- */
    {
        static const unsigned strides[] = { 12, 16, 20, 32 }, ostrides[] = { 1, 3 };
        static float Mb[16], gb1[4], gb2[4], tb1[3], tb2[3];
        shz_dce_shadow_map_t mb;
        float kb;
        size_t si, oi, count, k, j;
        int pass;

        random_map(&mb, &kb);
        mb.pcf = 1; mb.bias_b = 1.5f;
        random_matrix(Mb, kb);
        random_axis(tb1); random_axis(tb2);
        shz_xmtrx_load_unaligned_4x4(Mb);
        transform_axes(tb1, tb2, gb1, gb2);
        for (pass = 0; pass < 2; pass++)
            for (si = 0; si < sizeof strides / sizeof strides[0]; si++)
                for (oi = 0; oi < sizeof ostrides / sizeof ostrides[0]; oi++)
                    for (count = 0; count <= 37; count++) {
                        const size_t stride = strides[si], os = ostrides[oi];
                        const size_t first = 4;

                        memset(out_batch, 0x5c, sizeof out_batch);
                        memset(out_one, 0x5c, sizeof out_one);
                        for (k = 0; k < count * stride / 4 + 4; k++) pts[k] = t_frand(-2.5f, 2.5f);
                        if (pass) for (k = 0; k < count; k++) ((float *)((char *)pts + k * stride))[1] = -0.01f * (float)k;
                        shz_dce_shadow_compare_batch(out_batch + first, os, pts, stride, count, &mb, gb1, gb2);
                        for (k = 0; k < count; k++)
                            shz_dce_shadow_compare_batch(out_one + first + k * os, 1, (const float *)((const char *)pts + k * stride), stride, 1,
                                                         &mb, gb1, gb2);
                        for (k = 0; k < count; k++)
                            T_CHECK(out_batch[first + k * os] == out_one[first + k * os], "stride %u out %u count %u: point %u batch %u single %u",
                                    (unsigned)stride, (unsigned)os, (unsigned)count, (unsigned)k, out_batch[first + k * os], out_one[first + k * os]);
                        for (j = 0; j < sizeof out_batch; j++) {   /* every byte the batch does not own still holds the fill */
                            int owned = 0;
                            for (k = 0; k < count; k++) if (j == first + k * os) owned = 1;
                            if (!owned) T_CHECK(out_batch[j] == 0x5c, "stride %u out %u count %u: byte %u written", (unsigned)stride, (unsigned)os,
                                                (unsigned)count, (unsigned)j);
                        }
                        shz_xmtrx_store_unaligned_4x4(back);
                        T_CHECK(!memcmp(back, Mb, sizeof Mb), "XMTRX changed by the batch (count %u)", (unsigned)count);
                    }
        /* count 0 touches nothing */
        memset(out_batch, 0x77, sizeof out_batch);
        shz_dce_shadow_compare_batch(out_batch, 1, pts, 12, 0, &mb, gb1, gb2);
        T_CHECK(out_batch[0] == 0x77 && out_batch[sizeof out_batch - 1] == 0x77, "count 0 wrote something");
    }

    return t_done("shadow_test");
}
