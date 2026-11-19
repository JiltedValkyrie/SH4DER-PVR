/* SH4ZAM-DCE tests: shz_dce_project.h on the host (SH4ZAM's software back end)

   project_test.c
   Based on SH4ZAM by Falco Girgis (https://github.com/gyrovorbis/sh4zam).
   Copyright (c) 2025, 2026 Falco Girgis and SH4ZAM Contributors.
   MIT License: see ../../LICENSE.

   What is checked:
   - the bit values of the clip code, the layout of the view (the order the SH-4 loop reads it in), one point per bit by hand, the
     near bit alone behind the near plane, a point exactly on a plane inside
   - every processed record against one vertex written out with the same SH4ZAM calls and one fused multiply-add a coordinate: the
     position, the two carried words, the screen point (same bits), the third word, the code; and the screen point against the
     quotient in double within the reciprocal root's bound
   - a vertex marked 0x80 keeps its record, its code and its word when `lazy` is set, and is processed when it is not; the returned
     count; nothing past the last record, code and word; count 0; strides 24, 28 and 40; no clip test: every code 0, every vertex
     its screen point
*/

#include <sh4zam_dce/shz_dce_project.h>

#include "shz_dce_test.h"

#define BOUND 4.76837158203125e-07   /* 2^-21 */
#define N 67
#define MAX_STRIDE 40
#define IN_BYTES (N * MAX_STRIDE + 8)

static unsigned code_of(shz_vec3_t p, const shz_dce_view4_t *v, int clip) {
    unsigned c = 0u;
    if (!clip) return 0u;
    if (p.z < v->near_z) return SHZ_DCE_PROJECT_NEAR;
    if (p.x < v->x_lo * p.z) c |= SHZ_DCE_PROJECT_X_LO;
    if (p.x > v->x_hi * p.z) c |= SHZ_DCE_PROJECT_X_HI;
    if (p.y < v->y_lo * p.z) c |= SHZ_DCE_PROJECT_Y_LO;
    if (p.y > v->y_hi * p.z) c |= SHZ_DCE_PROJECT_Y_HI;
    return c;
}

/* the screen point of one vertex, the three steps of the file comment written out: the products rounded on their own (volatile:
   an optimiser allowed to regroup must not), then one fused multiply-add each. The fused step is the library's, which
   vertex_test.c checks against an exact integer reference; the C library's fmaf is no reference (the one of the host compiler
   used here, x86_64-w64-mingw32, returns a neighbour of the correctly rounded result in a quarter of all cases). */
static void screen_of(shz_vec3_t p, const shz_dce_view4_t *v, float s[3]) {
    const float inv = shz_invf_fsrra(p.z);
    volatile float fx = v->focal * p.x, fy = v->focal * p.y;
    s[0] = shz_dce_vertex_fmaf(fx, inv, v->centre_x);
    s[1] = shz_dce_vertex_fmaf(-fy, inv, v->centre_y);
    s[2] = shz_dce_vertex_fmaf(v->depth_scale, inv, v->depth_base);
}

static unsigned one(float x, float y, float z, const shz_dce_view4_t *v, float rec[8]) {
    float in[6] = { x, y, z, 0.0f, 0.0f, 0.0f };
    uint8_t code = 0xee;
    uint32_t word;
    const size_t done = shz_dce_project_vertices(rec, &code, &word, in, 24, 1, v, 1, 0);
    T_CHECK(done == 1u, "one vertex: returned %u", (unsigned)done);
    return code;
}

int main(void) {
    static uint32_t in_store[IN_BYTES / 4];   /* the records, on a multiple of 4 */
    uint8_t *const in = (uint8_t *)in_store;
    static float out[(N + 1) * 8 + 8];
    static uint8_t codes[N + 4], codes0[N + 4];
    static uint32_t words[N + 2];
    static shz_mat4x4_t m;
    static const uint32_t strides[3] = { 24, 28, 40 };
    shz_dce_view4_t v;
    float rec[8];
    int trial;

    T_CHECK(SHZ_DCE_PROJECT_NEAR == 1u && SHZ_DCE_PROJECT_X_LO == 2u && SHZ_DCE_PROJECT_X_HI == 4u && SHZ_DCE_PROJECT_Y_LO == 8u && SHZ_DCE_PROJECT_Y_HI == 16u &&
            SHZ_DCE_PROJECT_SKIP == 0x80u, "clip bit values");
    T_CHECK(sizeof(shz_dce_view4_t) == 40u && offsetof(shz_dce_view4_t, y_hi) == 0u && offsetof(shz_dce_view4_t, y_lo) == 4u && offsetof(shz_dce_view4_t, x_hi) == 8u &&
            offsetof(shz_dce_view4_t, x_lo) == 12u && offsetof(shz_dce_view4_t, depth_base) == 16u && offsetof(shz_dce_view4_t, centre_y) == 20u &&
            offsetof(shz_dce_view4_t, centre_x) == 24u && offsetof(shz_dce_view4_t, near_z) == 28u && offsetof(shz_dce_view4_t, focal) == 32u &&
            offsetof(shz_dce_view4_t, depth_scale) == 36u, "the view's layout");

    /* one point per bit, by hand, through an identity matrix */
    memset(&m, 0, sizeof m); m.elem[0] = m.elem[5] = m.elem[10] = m.elem[15] = 1.0f;
    shz_xmtrx_load_4x4(&m);
    v.x_lo = -1.0f; v.x_hi = 2.0f; v.y_lo = -0.5f; v.y_hi = 0.75f; v.near_z = 0.5f; v.focal = 100.0f; v.centre_x = 320.0f; v.centre_y = 240.0f;
    v.depth_base = 0.125f; v.depth_scale = 1.0f;
    T_CHECK(one(0.0f, 0.0f, 1.0f, &v, rec) == 0u, "a point on the axis is inside");
    T_CHECK(fabsf(rec[5] - 320.0f) < 1e-3f && fabsf(rec[6] - 240.0f) < 1e-3f && fabsf(rec[7] - 1.125f) < 1e-5f, "the axis lands on the centre: %g %g %g", (double)rec[5],
            (double)rec[6], (double)rec[7]);
    T_CHECK(one(1.0f, 0.5f, 2.0f, &v, rec) == 0u, "inside");
    T_CHECK(fabsf(rec[5] - 370.0f) < 1e-3f && fabsf(rec[6] - 215.0f) < 1e-3f && fabsf(rec[7] - 0.625f) < 1e-5f, "x to the right, y up the screen (smaller sy): %g %g %g",
            (double)rec[5], (double)rec[6], (double)rec[7]);
    T_CHECK(one(0.0f, 0.0f, 0.25f, &v, rec) == SHZ_DCE_PROJECT_NEAR, "near");
    T_CHECK(one(-30.0f, 50.0f, 0.25f, &v, rec) == SHZ_DCE_PROJECT_NEAR, "near alone, however far outside");
    T_CHECK(one(0.0f, 0.0f, -1.0f, &v, rec) == SHZ_DCE_PROJECT_NEAR, "behind the eye is near");
    T_CHECK(one(-1.5f, 0.0f, 1.0f, &v, rec) == SHZ_DCE_PROJECT_X_LO, "x low");
    T_CHECK(one(2.5f, 0.0f, 1.0f, &v, rec) == SHZ_DCE_PROJECT_X_HI, "x high");
    T_CHECK(one(0.0f, -1.0f, 1.0f, &v, rec) == SHZ_DCE_PROJECT_Y_LO, "y low");
    T_CHECK(one(0.0f, 1.0f, 1.0f, &v, rec) == SHZ_DCE_PROJECT_Y_HI, "y high");
    T_CHECK(one(3.0f, 1.0f, 1.0f, &v, rec) == (SHZ_DCE_PROJECT_X_HI | SHZ_DCE_PROJECT_Y_HI), "a corner");
    T_CHECK(one(-4.0f, -2.0f, 2.0f, &v, rec) == (SHZ_DCE_PROJECT_X_LO | SHZ_DCE_PROJECT_Y_LO), "the other corner");
    T_CHECK(one(-1.0f, 0.75f, 1.0f, &v, rec) == 0u && one(2.0f, -0.5f, 1.0f, &v, rec) == 0u && one(0.0f, 0.0f, 0.5f, &v, rec) == 0u, "a point exactly on a plane is inside");

    for (trial = 0; trial < 600; trial++) {
        const size_t count = (size_t)(trial % (N + 1));
        const uint32_t stride = strides[trial % 3];
        const int clip = (trial / 3) % 4 != 0, lazy = (trial / 7) % 2;
        float *o = out + ((trial / 5) % 2);   /* now on a multiple of 32 (as the array happens to lie), now not: no line allocation on the host anyway */
        size_t k, done, want = 0;
        int c, r;

        for (c = 0; c < 4; c++)
            for (r = 0; r < 4; r++)
                m.elem[4 * c + r] = r == 3 ? (c == 3 ? 1.0f : 0.0f) : c == 3 ? t_frand(-20, 20) : t_frand(-1, 1);
        v.x_lo = t_frand(-1.5f, -0.5f); v.x_hi = t_frand(0.5f, 1.5f); v.y_lo = t_frand(-1.5f, -0.5f); v.y_hi = t_frand(0.5f, 1.5f);
        v.near_z = 0.0625f; v.focal = t_frand(200, 400); v.centre_x = t_frand(100, 500); v.centre_y = t_frand(100, 400);
        v.depth_base = t_frand(-1, 1); v.depth_scale = t_frand(0.5f, 2);
        for (k = 0; k < IN_BYTES; k++) in[k] = (uint8_t)(t_rand() >> 13);
        for (k = 0; k < count; k++) {
            const float pos[3] = { t_frand(-25, 25), t_frand(-25, 25), t_frand(-25, 25) };
            memcpy(in + k * stride, pos, 12);
            codes0[k] = (t_rand() >> 9) % 8u < 3u ? SHZ_DCE_PROJECT_SKIP : (uint8_t)((t_rand() >> 9) & 0x7fu);
            want += !(lazy && codes0[k] == SHZ_DCE_PROJECT_SKIP);
        }
        shz_xmtrx_load_4x4(&m);
        memset(out, 0x5a, sizeof out); memset(codes, 0xee, sizeof codes); memcpy(codes, codes0, count); memset(words, 0xee, sizeof words);
        done = shz_dce_project_vertices(o, codes, words, in, stride, count, &v, clip, lazy);
        T_CHECK(done == want, "trial %d: returned %u, processed %u", trial, (unsigned)done, (unsigned)want);
        T_CHECK(codes[count] == 0xee && words[count] == 0xeeeeeeeeu && t_bits(o[8 * count]) == 0x5a5a5a5au, "trial %d: written past the end", trial);
        for (k = 0; k < count; k++) {
            const float *p = o + 8 * k;
            float s[3], sp[3];
            shz_vec3_t wantp;
            unsigned code;

            if (lazy && codes0[k] == SHZ_DCE_PROJECT_SKIP) {
                int i, clean = 1;
                for (i = 0; i < 8; i++) clean &= t_bits(p[i]) == 0x5a5a5a5au;
                T_CHECK(clean && codes[k] == SHZ_DCE_PROJECT_SKIP && words[k] == 0xeeeeeeeeu, "trial %d: skipped vertex %u was touched", trial, (unsigned)k);
                continue;
            }
            memcpy(s, in + k * stride, 12);
            wantp = shz_xmtrx_transform_point3(shz_vec3_init(s[0], s[1], s[2]));
            code = code_of(wantp, &v, clip);
            T_CHECK(!memcmp(p, &wantp, 12), "trial %d: record %u: not the plain transform's bits", trial, (unsigned)k);
            T_CHECK(!memcmp(p + 3, in + k * stride + 12, 8), "trial %d: record %u: the two words were not carried through", trial, (unsigned)k);
            T_CHECK(!memcmp(words + k, in + k * stride + 20, 4), "trial %d: record %u: the third word did not arrive", trial, (unsigned)k);
            T_CHECK(codes[k] == code, "trial %d: record %u: code %02x, the compares give %02x", trial, (unsigned)k, codes[k], code);
            for (r = 0; r < 3; r++) {
                double e = (double)m.elem[12 + r], pm = fabs((double)m.elem[12 + r]);
                for (c = 0; c < 3; c++) { const double t = (double)m.elem[4 * c + r] * (double)s[c]; e += t; if (fabs(t) > pm) pm = fabs(t); }
                T_CHECK(fabs((double)p[r] - e) <= (pm + fabs(e)) * BOUND, "trial %d: record %u axis %d: %.9g, exact %.9g", trial, (unsigned)k, r, (double)p[r], e);
            }
            if (code & SHZ_DCE_PROJECT_NEAR) {
                T_CHECK(t_bits(p[5]) == 0x5a5a5a5au && t_bits(p[6]) == 0x5a5a5a5au && t_bits(p[7]) == 0x5a5a5a5au, "trial %d: record %u: a screen point behind the near plane",
                        trial, (unsigned)k);
                continue;
            }
            screen_of(wantp, &v, sp);
            T_CHECK(!memcmp(p + 5, sp, 12), "trial %d: record %u: screen point %.9g %.9g %.9g, written out %.9g %.9g %.9g", trial, (unsigned)k, (double)p[5], (double)p[6],
                    (double)p[7], (double)sp[0], (double)sp[1], (double)sp[2]);
            if (fabs((double)wantp.z) > 1e-3) {
                const double q = 1.0 / fabs((double)wantp.z);
                const double tx = (double)v.focal * (double)wantp.x * q, ty = (double)v.focal * (double)wantp.y * q, tz = (double)v.depth_scale * q;
                /* the software back end's reciprocal root is the host's own (an approximation when the host build is -ffast-math): twice the SH-4's bound */
                T_CHECK(fabs((double)p[5] - ((double)v.centre_x + tx)) <= (fabs(tx) + fabs((double)v.centre_x)) * 2.0 * BOUND, "trial %d: record %u: sx %.9g, the quotient gives %.9g",
                        trial, (unsigned)k, (double)p[5], (double)v.centre_x + tx);
                T_CHECK(fabs((double)p[6] - ((double)v.centre_y - ty)) <= (fabs(ty) + fabs((double)v.centre_y)) * 2.0 * BOUND, "trial %d: record %u: sy %.9g, the quotient gives %.9g",
                        trial, (unsigned)k, (double)p[6], (double)v.centre_y - ty);
                T_CHECK(fabs((double)p[7] - ((double)v.depth_base + tz)) <= (fabs(tz) + fabs((double)v.depth_base)) * 2.0 * BOUND, "trial %d: record %u: sz %.9g, the quotient gives %.9g",
                        trial, (unsigned)k, (double)p[7], (double)v.depth_base + tz);
            }
        }
    }
    return t_done("project_test");
}
