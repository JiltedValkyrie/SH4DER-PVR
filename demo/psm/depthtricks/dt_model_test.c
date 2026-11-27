/* SH4DER-PVR: depth tricks for the PVR chain, host models (not measurements)

   demo/psm/depthtricks/dt_model_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Two models for demo/psm/depthtricks/NOTES.md, both ESTIMATE: what each route would put in its pixels, counted
   against an exact answer. Nothing here is timed; a cycle figure printed here is the model's count of work times an
   assumed cost a unit, marked ESTIMATE.

     1 CAMERA DEPTH MAP at 128 x 96 of c4's scene (../compare/c4_model.h: the ground, a house, a wall with an eave, a
       hen, a post), seen by c4's camera. The truth is a ray a pixel centre. Two ways to make the map:
         PVR   the c4 MAP pass drawn from the camera: each pixel gets its own depth code (MEASURED in c4 test 0 from the
               light: within 1 code; 2,303 / 348 of 16,384 one code off, plain ramp / 8 sub-steps). Modelled as the
               truth rounded, with the measured error rates quoted, not simulated.
         CPU   a raster on the SH-4: the faces clipped at the near plane, cut into triangles, filled by edge functions
               at pixel centres, nearest kept. The code is either linear in 1 / z (affine on the screen: no divide a
               pixel) or linear in z (one reciprocal a pixel). Counted: holes and extra pixels against the rays, the
               code error, the depth error in metres, pixels written (overdraw), triangles.
     2 RAMP CEL SHADING (recipe R1): clamp(N.L, 0, 1) per vertex as the texture coordinate u, a point-sampled ramp of
       8 texels holding 4 tone bands. A unit sphere, orthographic view, cut into slices x stacks; the truth is the
       sphere's own normal at each pixel. Counted: pixels whose band differs from the truth's, for
         RAMP     u interpolated across the triangle, then the ramp (what the PVR does per pixel)
         RAMP10   the same with u held to 10 bits (the texture-coordinate path, PVR-023)
         GOURAUD  the band's tone worked out per vertex and the TONE interpolated (no ramp): pixels whose tone is
                  more than half a band step from the truth's
       at tessellations 8 x 6, 16 x 12 and 32 x 24.

   Build (from the repository's root):
     gcc -std=c99 -O2 -Wall -Wextra -DSH4DER_CORE_NO_SH4ZAM -Iinclude demo/psm/depthtricks/dt_model_test.c -lm -o build/dt_model_test
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../compare/c4_model.h"

static int fails;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); fails++; } } while (0)

/* ---- 1. the camera depth map ------------------------------------------------------------------------------- */

#define DW 128u
#define DH 96u
#define NEAR_W 0.25f                       /* the near plane the raster clips at, metres of view depth */

static float truth[DW * DH];               /* view depth of the ray's hit, 0 = sky */
static float ras[DW * DH];                 /* the raster's 1 / z, 0 = nothing */
static unsigned ras_writes, ras_tris;

/* A corner in clip space: X, Y (pixels x W), W (view depth). */
typedef struct { float x, y, w; } cv_t;

/* The polygon's part with W >= NEAR_W (one plane of Sutherland-Hodgman). */
static unsigned clip_near(const cv_t *in, unsigned n, cv_t *out) {
    unsigned i, k = 0;

    for (i = 0; i < n; i++) {
        const cv_t *a = &in[i], *b = &in[(i + 1u) % n];
        const int ia = a->w >= NEAR_W, ib = b->w >= NEAR_W;

        if (ia) out[k++] = *a;
        if (ia != ib) {
            const float t = (NEAR_W - a->w) / (b->w - a->w);

            out[k].x = a->x + t * (b->x - a->x); out[k].y = a->y + t * (b->y - a->y); out[k].w = NEAR_W; k++;
        }
    }
    return k;
}

/* Fill one triangle of screen corners (x, y) with 1 / w at each: pixel centres inside or on an edge; the nearest
   (largest 1 / w) kept. */
static void tri(const float p[3][2], const float iw[3]) {
    const float area = (p[1][0] - p[0][0]) * (p[2][1] - p[0][1]) - (p[2][0] - p[0][0]) * (p[1][1] - p[0][1]);
    float x0 = p[0][0], x1 = p[0][0], y0 = p[0][1], y1 = p[0][1];
    int xa, xb, ya, yb, x, y;
    unsigned k;

    if (fabsf(area) < 1e-9f) return;
    for (k = 1; k < 3u; k++) { if (p[k][0] < x0) x0 = p[k][0]; if (p[k][0] > x1) x1 = p[k][0]; if (p[k][1] < y0) y0 = p[k][1]; if (p[k][1] > y1) y1 = p[k][1]; }
    xa = (int)floorf(x0); xb = (int)ceilf(x1); ya = (int)floorf(y0); yb = (int)ceilf(y1);
    xa = xa < 0 ? 0 : xa; ya = ya < 0 ? 0 : ya;
    xb = xb > (int)DW - 1 ? (int)DW - 1 : xb; yb = yb > (int)DH - 1 ? (int)DH - 1 : yb;
    ras_tris++;
    for (y = ya; y <= yb; y++) {
        for (x = xa; x <= xb; x++) {
            const float px = (float)x + 0.5f, py = (float)y + 0.5f;
            float b[3];

            b[0] = ((p[1][0] - px) * (p[2][1] - py) - (p[2][0] - px) * (p[1][1] - py)) / area;
            b[1] = ((p[2][0] - px) * (p[0][1] - py) - (p[0][0] - px) * (p[2][1] - py)) / area;
            b[2] = 1.0f - b[0] - b[1];
            if (b[0] < -1e-6f || b[1] < -1e-6f || b[2] < -1e-6f) continue;
            {
                const float v = b[0] * iw[0] + b[1] * iw[1] + b[2] * iw[2];   /* 1 / w is affine on the screen */

                ras_writes++;
                if (v > ras[y * DW + x]) ras[y * DW + x] = v;
            }
        }
    }
}

static void depth_map(void) {
    c4_cam_t cam;
    c4_face_t f[C4_FACES];
    float m[16], near = 1e30f, far = 0.0f;
    unsigned x, y, i, k, hits = 0, holes = 0, extra = 0, off_inv = 0, off_lin = 0, worst_inv = 0, worst_lin = 0;
    double err_inv = 0.0, err_lin = 0.0, mx_inv = 0.0, mx_lin = 0.0;

    c4_camera(&cam, DW, DH);
    c4_cam_matrix(&cam, m);
    for (y = 0; y < DH; y++) {
        for (x = 0; x < DW; x++) {
            const float ifo = 1.0f / cam.focal, sx = ((float)x + 0.5f - cam.cx) * ifo, sy = -((float)y + 0.5f - cam.cy) * ifo;
            const float d[3] = { cam.fwd[0] + sx * cam.right[0] + sy * cam.up[0], cam.fwd[1] + sx * cam.right[1] + sy * cam.up[1], cam.fwd[2] + sx * cam.right[2] + sy * cam.up[2] };
            float t, n[3];
            unsigned face;

            truth[y * DW + x] = c4_ray(cam.eye, d, 0.0f, &t, n, &face) ? t : 0.0f;   /* d's forward part is 1: t is the view depth */
            if (truth[y * DW + x] > 0.0f) { hits++; if (t < near) near = t; if (t > far) far = t; }
        }
    }
    c4_faces(f, 0.0f);
    memset(ras, 0, sizeof ras);
    for (i = 0; i < C4_FACES; i++) {
        cv_t c[4], o[8];
        unsigned n;

        for (k = 0; k < 4u; k++) {
            const float *p = f[i].p[k];

            c[k].x = m[0] * p[0] + m[4] * p[1] + m[8] * p[2] + m[12];
            c[k].y = m[1] * p[0] + m[5] * p[1] + m[9] * p[2] + m[13];
            c[k].w = m[3] * p[0] + m[7] * p[1] + m[11] * p[2] + m[15];
        }
        n = clip_near(c, 4u, o);
        for (k = 1; k + 1u < n; k++) {
            const cv_t *v[3] = { &o[0], &o[k], &o[k + 1u] };
            float p[3][2], iw[3];
            unsigned j;

            for (j = 0; j < 3u; j++) { iw[j] = 1.0f / v[j]->w; p[j][0] = v[j]->x * iw[j]; p[j][1] = v[j]->y * iw[j]; }
            tri((const float (*)[2])p, iw);
        }
    }
    for (i = 0; i < DW * DH; i++) {
        const int t = truth[i] > 0.0f, r = ras[i] > 0.0f;

        if (t && !r) holes++;
        if (r && !t) extra++;
        if (t && r) {
            /* codes 0 .. 255 over [near, far]: linear in 1 / z (no divide a pixel), and linear in z (one a pixel) */
            const float zi = 1.0f / ras[i];
            const float ci = (1.0f / near - ras[i]) / (1.0f / near - 1.0f / far) * 255.0f, ti = (1.0f / near - 1.0f / truth[i]) / (1.0f / near - 1.0f / far) * 255.0f;
            const float cl = (zi - near) / (far - near) * 255.0f, tl = (truth[i] - near) / (far - near) * 255.0f;
            const int qi = (int)floorf(ci + 0.5f), qti = (int)floorf(ti + 0.5f), ql = (int)floorf(cl + 0.5f), qtl = (int)floorf(tl + 0.5f);
            /* the metres a code stands for there: decode the raster's code and compare with the truth */
            const float zq_i = 1.0f / (1.0f / near - (float)qi / 255.0f * (1.0f / near - 1.0f / far)), zq_l = near + (float)ql / 255.0f * (far - near);
            const double ei = fabs((double)zq_i - truth[i]), el = fabs((double)zq_l - truth[i]);
            const unsigned di = (unsigned)abs(qi - qti), dl = (unsigned)abs(ql - qtl);

            off_inv += di != 0u; off_lin += dl != 0u;
            if (di > worst_inv) worst_inv = di;
            if (dl > worst_lin) worst_lin = dl;
            err_inv += ei; err_lin += el;
            if (ei > mx_inv) mx_inv = ei;
            if (el > mx_lin) mx_lin = el;
        }
    }
    printf("1 CAMERA DEPTH MAP %u x %u, c4's scene and camera: %u pixels hit, view depth %.2f .. %.2f m\n", DW, DH, hits, (double)near, (double)far);
    printf("  CPU raster: %u triangles, %u pixel writes (%.2f a pixel hit); against the rays %u holes, %u extra\n", ras_tris, ras_writes, (double)ras_writes / hits, holes, extra);
    printf("  code linear in 1/z (affine, no divide): %u pixels off the true pixel's code, worst %u; depth error mean %.3f max %.3f m\n", off_inv, worst_inv, err_inv / hits, mx_inv);
    printf("  code linear in z (a reciprocal a pixel): %u pixels off, worst %u; depth error mean %.3f max %.3f m\n", off_lin, worst_lin, err_lin / hits, mx_lin);
    printf("  a code is %.1f cm at the near end and %.1f cm at the far end in 1/z; %.1f cm everywhere in z\n",
           100.0 * (1.0 / (1.0 / near - 1.0 / 255.0 * (1.0 / near - 1.0 / far)) - near), 100.0 * (far - 1.0 / (1.0 / far + 1.0 / 255.0 * (1.0 / near - 1.0 / far))), 100.0 * (far - near) / 255.0);
    CHECK(hits > DW * DH / 3u);                                   /* the rest is sky */
    CHECK(holes <= hits / 200u && extra <= hits / 200u);          /* only pixel centres on an edge may differ */
    CHECK(worst_inv <= 1u && worst_lin <= 1u);
}

/* ---- 2. ramp cel shading ------------------------------------------------------------------------------------- */

#define CW 128u
#define BANDS 4u
#define RAMP 8u

static const float LDIR[3] = { 0.48f, 0.64f, 0.60f };             /* unit: 0.48^2 + 0.64^2 + 0.6^2 = 1 */

static unsigned band_of(float ndl) {                               /* the truth's band and the ramp's: 8 texels, 2 a band */
    unsigned t;

    ndl = ndl < 0.0f ? 0.0f : ndl > 1.0f ? 1.0f : ndl;
    t = (unsigned)(ndl * (float)RAMP);
    if (t >= RAMP) t = RAMP - 1u;
    return t / (RAMP / BANDS);
}

static float tone_of(unsigned band) { return ((float)band + 0.5f) / (float)BANDS; }

static void cel(unsigned slices, unsigned stacks) {
    unsigned s, t, x, y, pixels = 0, d_ramp = 0, d_r10 = 0, d_gour = 0, k;
    static float vu[64 + 1][64 + 1], vp[64 + 1][64 + 1][3], vt[64 + 1][64 + 1];

    for (t = 0; t <= stacks; t++) {
        for (s = 0; s <= slices; s++) {
            const float th = 3.14159265f * (float)t / (float)stacks, ph = 6.2831853f * (float)s / (float)slices;
            const float n[3] = { sinf(th) * cosf(ph), cosf(th), sinf(th) * sinf(ph) };
            const float ndl = n[0] * LDIR[0] + n[1] * LDIR[1] + n[2] * LDIR[2];

            memcpy(vp[t][s], n, sizeof n);
            vu[t][s] = ndl < 0.0f ? 0.0f : ndl;                   /* what the SH-4 writes as u */
            vt[t][s] = tone_of(band_of(ndl));                     /* the per-vertex tone, for Gouraud */
        }
    }
    for (y = 0; y < CW; y++) {
        for (x = 0; x < CW; x++) {
            const float px = ((float)x + 0.5f) / (float)CW * 2.0f - 1.0f, py = 1.0f - ((float)y + 0.5f) / (float)CW * 2.0f;
            const float r2 = px * px + py * py;
            float nz, u = -1.0f, g = 0.0f;

            if (r2 >= 1.0f) continue;
            nz = sqrtf(1.0f - r2);                                /* the sphere's own normal, toward the viewer (+z) */
            /* the mesh triangle under the pixel: the visible half, z > 0; find it by the projected quad (t, s) */
            for (t = 0; t < stacks && u < 0.0f; t++) {
                for (s = 0; s < slices && u < 0.0f; s++) {
                    const unsigned q[4][2] = { { t, s }, { t, s + 1u }, { t + 1u, s + 1u }, { t + 1u, s } };

                    for (k = 0; k < 2u && u < 0.0f; k++) {
                        const unsigned *a = q[0], *b = q[k + 1u], *c = q[k + 2u];
                        const float ax = vp[a[0]][a[1]][0], ay = vp[a[0]][a[1]][1], bx = vp[b[0]][b[1]][0], by = vp[b[0]][b[1]][1], cx = vp[c[0]][c[1]][0], cy = vp[c[0]][c[1]][1];
                        const float area = (bx - ax) * (cy - ay) - (cx - ax) * (by - ay);
                        float w0, w1, w2;

                        if (vp[a[0]][a[1]][2] + vp[b[0]][b[1]][2] + vp[c[0]][c[1]][2] <= 0.0f || fabsf(area) < 1e-12f) continue;
                        w0 = ((bx - px) * (cy - py) - (cx - px) * (by - py)) / area;
                        w1 = ((cx - px) * (ay - py) - (ax - px) * (cy - py)) / area;
                        w2 = 1.0f - w0 - w1;
                        if (w0 < -1e-6f || w1 < -1e-6f || w2 < -1e-6f) continue;
                        u = w0 * vu[a[0]][a[1]] + w1 * vu[b[0]][b[1]] + w2 * vu[c[0]][c[1]];
                        g = w0 * vt[a[0]][a[1]] + w1 * vt[b[0]][b[1]] + w2 * vt[c[0]][c[1]];
                    }
                }
            }
            if (u < 0.0f) continue;                               /* a pixel past the mesh's silhouette */
            {
                const float ndl = px * LDIR[0] + py * LDIR[1] + nz * LDIR[2];
                const unsigned bt = band_of(ndl), br = band_of(u), b10 = band_of(floorf(u * 1024.0f) / 1024.0f);

                pixels++;
                d_ramp += br != bt;
                d_r10 += b10 != bt;
                d_gour += fabsf(g - tone_of(bt)) > 0.5f / (float)BANDS;
            }
        }
    }
    printf("  sphere %2u x %2u: %u pixels; band not the truth's: ramp %u (%.1f %%), ramp with u to 10 bits %u, Gouraud tone off by over half a band %u (%.1f %%)\n",
           slices, stacks, pixels, d_ramp, 100.0 * d_ramp / pixels, d_r10, d_gour, 100.0 * d_gour / pixels);
    CHECK(d_ramp < d_gour);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    depth_map();
    printf("2 RAMP CEL SHADING, 4 bands from an 8-texel point-sampled ramp, light (%.2f, %.2f, %.2f)\n", (double)LDIR[0], (double)LDIR[1], (double)LDIR[2]);
    cel(8u, 6u);
    cel(16u, 12u);
    cel(32u, 24u);
    printf(fails ? "dt_model_test: %d FAILED\n" : "dt_model_test: ok\n", fails);
    return fails != 0;
}
