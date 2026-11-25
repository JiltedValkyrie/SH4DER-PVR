/* SH4ZAM-DCE tests: shz_dce_shadow.h as the SH-4 runs it

   shadow_check.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   shadow_check <shadow_kernels.elf>
   The ELF is shadow_kernels.c built by sh-elf-gcc; tests/sh4run executes it instruction by instruction. For each batch:
   - the dispatch, the _sh4 form, the _c form and the planar path the routine replaces (BEFORE), all on the SH-4: bit for bit
     (they share the same FTRV on the same inputs)
   - the SH-4 bytes against an exact reference in double, written here: a byte may differ only where a decision lies inside the
     margin the FTRV bound (and float rounding) can move
   - the bytes a batch does not own (the stride gaps, the bytes after the batch) are untouched; XMTRX holds the map's matrix afterwards
   - every batch run twice, so the scratch registers see other garbage the second time: the bytes must not change
   Data is placed on the weakest alignment the routine's comment allows (points on a multiple of 4 that is not one of 8; the depth map on a
   multiple of 2 that is not one of 4; the matrix and the axes on a multiple of 4 that is not one of 8).
*/

#include "shadow_jobs.h"
#include "sh4_check.h"

typedef struct {   /* the SH-4's map record, as the host writes it: 40 bytes, 4-byte aligned */
    uint32_t depth, size;
    float key_scale, slope_max, bias_a, bias_b, texel_max, texel_step, w_eps;
    int32_t pcf;
} map_rec_t;

/* the host's copy of the same map, for the reference */
typedef struct {
    const uint16_t *depth;
    unsigned size;
    double key, slope, a, b, tmax, step, eps;
    int pcf;
} ref_map_t;

/* The exact answer for one point, in double: the algorithm of the header's comment, written here. *near: a decision lies inside the margin. */
static double reference(const ref_map_t *m, const float M[16], const float p[3], const float t1[3], const float t2[3], int *near) {
    const double half = 0.5 * (double)m->size, mk = m->key, cap = m->slope * mk;
    double h[4], g1[4], g2[4], s[3][3], W, iw, tx, ty, k0, bias, texel, det, kx = 0.0, ky = 0.0, fx = 0.0, fy = 0.0, tap[4] = { 1.0, 1.0, 1.0, 1.0 }, v;
    int j, i, taps, ix = 0, iy = 0;

    *near = 0;
    for (j = 0; j < 4; j++) {
        h[j] = (double)p[0] * M[j] + (double)p[1] * M[4 + j] + (double)p[2] * M[8 + j] + (double)M[12 + j];
        g1[j] = (double)t1[0] * M[j] + (double)t1[1] * M[4 + j] + (double)t1[2] * M[8 + j];
        g2[j] = (double)t2[0] * M[j] + (double)t2[1] * M[4 + j] + (double)t2[2] * M[8 + j];
    }
    W = h[3];
    if (fabs(W) < 1.0e-2) *near = 1;
    if (!(fabs(W) > m->eps)) return 1.0;            /* the light's own plane: lit */
    iw = 1.0 / W;
    bias = m->a;
    if (m->b != 0.0) {                              /* the texel size: a forward difference of step along each axis */
        const double delta = m->step, a3 = W + delta * g1[3], b3 = W + delta * g2[3];
        const double u0 = h[0] * iw, v0 = h[1] * iw;
        const double u1 = ((h[0] + delta * g1[0]) / a3 - u0) * half, w1 = ((h[1] + delta * g1[1]) / a3 - v0) * half;
        const double u2 = ((h[0] + delta * g2[0]) / b3 - u0) * half, w2 = ((h[1] + delta * g2[1]) / b3 - v0) * half;
        const double area = fabs(u1 * w2 - w1 * u2);
        if (fabs(a3) < 1.0e-2 || fabs(b3) < 1.0e-2 || area < 1.0e-9) *near = 1;
        texel = area > 1.0e-12 ? sqrt(delta * delta / area) : 1.0e30;
        if (!(texel < m->tmax)) texel = m->tmax;
        if (fabs(texel - m->tmax) < 1.0e-4 * m->tmax) *near = 1;
        bias = m->a + m->b * texel;
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
    if (!(fabs(det) > 1.0e-12)) {                   /* degenerate solve: the point-only compare */
        if (!(tx > -1.0) || !(ty > -1.0) || !(tx < (double)m->size + 1.0) || !(ty < (double)m->size + 1.0)) return 1.0;
    } else {
        if (fabs(det) < 1.0e-6 * (fabs(s[1][0]) + fabs(s[1][1])) * (fabs(s[2][0]) + fabs(s[2][1])) + 1.0e-12) *near = 1;
        kx = (s[1][2] * s[2][1] - s[2][2] * s[1][1]) / det;
        ky = (s[2][2] * s[1][0] - s[1][2] * s[2][0]) / det;
        if (!(tx > -1.0) || !(ty > -1.0) || !(tx < (double)m->size + 1.0) || !(ty < (double)m->size + 1.0)) return 1.0;
    }
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
            const double margin = 1.0e-4 * (1.0 + fabs(k0) + (double)m->size * (fabs(kx) + fabs(ky)) + fabs(bias) * mk);
            if (fabs(kf - stored) <= margin) *near = 1;
            tap[i] = stored <= kf ? 1.0 : 0.0;
        }
    }
    if (!m->pcf) return tap[0];
    v = (tap[0] + (tap[1] - tap[0]) * fx) * (1.0 - fy) + (tap[2] + (tap[3] - tap[2]) * fx) * fy;
    {
        const double r = v * 255.0 + 0.5;
        if (fabs(r - floor(r)) < 1e-3 || fabs(r - floor(r) - 1.0) < 1e-3) *near = 1;
    }
    return v;
}

static void random_axis(float t[3]) {
    float len;
    do {
        t[0] = t_frand(-1, 1); t[1] = t_frand(-1, 1); t[2] = t_frand(-1, 1);
        len = sqrtf(t[0] * t[0] + t[1] * t[1] + t[2] * t[2]);
    } while (len < 0.1f);
    t[0] /= len; t[1] /= len; t[2] /= len;
}

static void test_shadow(void) {
    static const uint32_t strides[] = { 12, 16, 32 }, sizes[] = { 2, 3, 8, 16, 31 };
    int trial, band = 0, lit = 0, shadow = 0, identical = 0, identical_n = 0, planar_band = 0;

    for (trial = 0; trial < 150; trial++) {
        const uint32_t stride = strides[trial % 3], size = sizes[t_rand() % 5u], count = t_rand() % 25u, os = (trial & 1) ? 3u : 1u;
        const uint32_t obytes = count * os + 8u;
        const uint32_t aj = ram(sizeof(job_shadow_t), 4, 0), amap = ram(sizeof(map_rec_t), 4, 0), adep = ram(size * size * 2u, 4, 2);
        const uint32_t am = ram(64, 8, 4), at1 = ram(12, 8, 4), at2 = ram(12, 8, 4), apts = ram(count * stride + 4u, 8, 4);
        const uint32_t oa[4] = { ram(obytes, 1, 1), ram(obytes, 1, 1), ram(obytes, 1, 1), ram(obytes, 1, 1) };
        job_shadow_t *j = (job_shadow_t *)host(aj, sizeof *j);
        map_rec_t *rec = (map_rec_t *)host(amap, sizeof(map_rec_t));
        uint16_t *dep = (uint16_t *)host(adep, size * size * 2u);
        float *M = (float *)host(am, 64), *t1 = (float *)host(at1, 12), *t2 = (float *)host(at2, 12);
        float *pts = (float *)host(apts, count * stride + 4u), mv[16], ax[3], bx[3], key = t_frand(200.0f, 6000.0f), got[16];
        uint8_t *outs[4], snap[4][96];
        ref_map_t rm;
        uint32_t k, c;

        for (k = 0; k < size * size; k++) { const uint32_t v = t_rand(); dep[k] = (v & 3u) == 0u ? 0u : (uint16_t)(v >> 8); }
        rec->depth = adep; rec->size = size; rec->key_scale = key; rec->slope_max = 8.0f;
        rec->bias_a = t_frand(-0.05f, 0.05f); rec->bias_b = (t_rand() & 1u) ? 1.5f : 0.0f;
        rec->texel_max = 5.0f; rec->texel_step = 0.02f; rec->w_eps = 1.0e-6f; rec->pcf = (int32_t)(t_rand() & 1u);
        for (k = 0; k < 16; k++) M[k] = t_frand(-1.0f, 1.0f);
        M[2] = key * t_frand(-0.3f, 0.3f); M[6] = key * t_frand(-0.3f, 0.3f); M[10] = key * t_frand(-0.3f, 0.3f);
        M[14] = 32768.0f + t_frand(-2000.0f, 2000.0f); M[15] = t_frand(0.5f, 2.0f);
        for (k = 0; k < 16; k++) mv[k] = M[k];
        random_axis(ax); random_axis(bx);
        if (trial % 5 == 0) { bx[0] = ax[0]; bx[1] = ax[1]; bx[2] = ax[2]; }   /* the same axis twice: a zero determinant, the fallback */
        for (k = 0; k < 3; k++) { t1[k] = ax[k]; t2[k] = bx[k]; }
        for (k = 0; k < count * stride / 4u + 1u; k++) pts[k] = t_frand(-2.5f, 2.5f);
        for (c = 0; c < 4; c++) for (k = 0; k < obytes; k++) ((uint8_t *)host(oa[c], obytes))[k] = 0x5c;

        j->map = amap; j->matrix = am; j->t1 = at1; j->t2 = at2; j->points = apts; j->stride = stride; j->count = count; j->out_stride = os;
        j->out_asm = oa[0]; j->out_sh4 = oa[1]; j->out_c = oa[2]; j->out_before = oa[3];
        call("_t_shadow", aj);
        for (c = 0; c < 4; c++) outs[c] = (uint8_t *)host(oa[c], obytes);

        for (c = 0; c < 4; c++) {   /* the bytes the batch does not own are untouched */
            for (k = 0; k < obytes; k++) {
                const int owned = k < count * os && k % os == 0u;
                if (!owned) T_CHECK(outs[c][k] == 0x5c, "trial %d output %u: byte %u written outside the batch", trial, (unsigned)c, (unsigned)k);
            }
        }
        T_CHECK(!memcmp(outs[0], outs[1], obytes) && !memcmp(outs[0], outs[2], obytes),
                "trial %d: the dispatch, _sh4 and _c forms differ on the SH-4 (count %u, stride %u)", trial, (unsigned)count, (unsigned)stride);
        sh4_xmtrx_get(S, got);
        T_CHECK(!memcmp(got, mv, sizeof mv), "trial %d: XMTRX does not hold the map's matrix after the batch", trial);

        /* the exact side: the routine's byte against the reference in double; it may differ only where a decision is inside its margin.
           The planar path against the routine: the same bytes, except (a) for identical axes, where the planar path's determinant is a
           rounding residual on the SH-4 (the routine takes the fallback there) and is not checked, and (b) where a margin is near. */
        identical = ax[0] == bx[0] && ax[1] == bx[1] && ax[2] == bx[2];
        rm.depth = dep; rm.size = size; rm.key = (double)key; rm.slope = 8.0; rm.a = (double)rec->bias_a; rm.b = (double)rec->bias_b;
        rm.tmax = 5.0; rm.step = 0.02; rm.eps = 1.0e-6; rm.pcf = rec->pcf;
        for (k = 0; k < count; k++) {
            const float *q = (const float *)((const char *)pts + k * stride);
            int near = 0;
            const double v = reference(&rm, mv, q, ax, bx, &near);
            const uint8_t eb = (uint8_t)(int)floor(v * 255.0 + 0.5);
            const uint8_t got_k = outs[0][k * os], planar_k = outs[3][k * os];

            if (v == 1.0) lit++; else shadow++;
            if (got_k != eb) {
                if (near) band++;
                else T_CHECK(0, "trial %d point %u: byte %u, exact %u, no decision near its margin", trial, (unsigned)k, (unsigned)got_k, (unsigned)eb);
            }
            if (got_k != planar_k) {
                if (identical) identical_n++;
                else if (near) planar_band++;
                else T_CHECK(0, "trial %d point %u: the routine %u, the planar path %u, neither near a margin", trial, (unsigned)k,
                             (unsigned)got_k, (unsigned)planar_k);
            }
        }

        /* a second run, other garbage in the scratch registers: the same bytes */
        for (c = 0; c < 4; c++) memcpy(snap[c], outs[c], obytes);
        call("_t_shadow", aj);
        for (c = 0; c < 4; c++) {
            outs[c] = (uint8_t *)host(oa[c], obytes);
            T_CHECK(!memcmp(snap[c], outs[c], obytes), "trial %d: a second run changed output %u", trial, (unsigned)c);
        }
        sh4_alloc_reset(S);
    }
    printf("shadow: %d SH-4 points, %d lit and %d shadowed; %d bytes differed from the exact answer, all inside a decision's margin; "
           "the planar path differed from the routine on %d identical-axes points (its residual) and %d points inside a margin\n",
           lit + shadow, lit, shadow, band, identical_n, planar_band);
    T_CHECK(lit > 200 && shadow > 200, "the SH-4 points cover both answers");
}

int main(int argc, char **argv) {
    const int rc = check_open(argc, argv);
    if (rc) return rc;
    test_shadow();
    return t_done("shadow_check");
}
