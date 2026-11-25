/* SH4DER-PVR: camera depth map, SH-4 raster against a PVR-drawn map (host model)

   demo/psm/recipes/depthmap_model.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).

   The camera depth map several recipes need (RECIPES_NOTES.md R9; wiki depth-shading ideas D1, D2, D5, C1, C4) is
   OPEN: nobody has drawn one. Two ways to make it, 128 x 96, one 8-bit code a pixel (linear in 1/z between near
   and far, the quantity a screen-space interpolation keeps exact):

   CPU  the SH-4 transforms the scene, clips at near, culls backs, rasterises by edge functions with an 8-bit
        depth test into a 12 KB buffer (it fits the 16 KB operand cache), then sends it to video memory as a
        16-bit bump texture (128 x 128, 32 KB; the compare byte low).
   PVR  the SH-4 transforms and sends the scene to the TA once more; the PVR draws the codes through c4's ramp
        lookup into a target cut to its tiles. Nothing comes back to the SH-4.

   This program checks the CPU raster against a ray cast (double) on synthetic scenes of 98 to 12,002 triangles,
   counts the work each route does, and prices it from the ledger. Every price is an ESTIMATE built from the rows
   named next to it; MEASURED rows are one console, one run, 2026-11-25. Prints PASS and FAIL counts.

   Build (plain C):  gcc -O2 -std=c99 -DSH4DER_CORE_NO_SH4ZAM -I../../../include -I../../../sh4dow-dc/include depthmap_model.c -lm
   SH4ZAM path: as the Makefile's test-shz line.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "sh4der_core.h"

static unsigned passes, fails;
#define CHECK(c) do { if (c) passes++; else { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

#define MAXV 40000u
#define MAXT 14000u
#define NEAR_Z 0.5f
#define FAR_Z 64.0f

typedef struct { unsigned w, h; float f; } cam_t;         /* view space: x right, y up, z forward; f pixels */

static float vpos[MAXV][3];
static unsigned tri[MAXT][3], nv, nt;

/* ---- the scene: a ground and boxes, deterministic --------------------------------------------- */

static unsigned rng = 12345u;
static float frand(float a, float b) { rng = rng * 1664525u + 1013904223u; return a + (b - a) * (float)(rng >> 8) / 16777216.0f; }

static unsigned add_v(float x, float y, float z) { vpos[nv][0] = x; vpos[nv][1] = y; vpos[nv][2] = z; return nv++; }
static void add_t(unsigned a, unsigned b, unsigned c) { tri[nt][0] = a; tri[nt][1] = b; tri[nt][2] = c; nt++; }

/* A box, its faces wound counter-clockwise seen from outside (y up), its 8 corners shared. */
static void box(float x0, float y0, float z0, float x1, float y1, float z1) {
    static const unsigned q[6][4] = { { 0, 2, 3, 1 }, { 4, 5, 7, 6 }, { 0, 1, 5, 4 }, { 2, 6, 7, 3 }, { 0, 4, 6, 2 }, { 1, 3, 7, 5 } };
    unsigned v[8], i;
    for (i = 0; i < 8u; i++) v[i] = add_v(i & 1u ? x1 : x0, i & 2u ? y1 : y0, i & 4u ? z1 : z0);
    for (i = 0; i < 6u; i++) { add_t(v[q[i][0]], v[q[i][1]], v[q[i][2]]); add_t(v[q[i][0]], v[q[i][2]], v[q[i][3]]); }
}

/* The ground (y = -1.7 under the eye, from z -10 to 80) and n boxes in front of the eye. View space directly. */
static void scene(unsigned n) {
    unsigned g[4], i;
    nv = nt = 0; rng = 12345u + n;
    g[0] = add_v(-60.0f, -1.7f, -10.0f); g[1] = add_v(60.0f, -1.7f, -10.0f); g[2] = add_v(-60.0f, -1.7f, 80.0f); g[3] = add_v(60.0f, -1.7f, 80.0f);
    add_t(g[0], g[2], g[3]); add_t(g[0], g[3], g[1]);
    for (i = 0; i < n; i++) {
        const float z = frand(2.0f, 60.0f), x = frand(-0.6f, 0.6f) * z, s = frand(0.2f, 1.2f), h = frand(0.3f, 4.0f);
        box(x - s, -1.7f, z - s, x + s, -1.7f + h, z + s);
    }
}

/* ---- the SH-4 side: clip, cull, rasterise (sh4der_core.h helpers: SH4ZAM or plain C) ------------- */

typedef struct {
    unsigned tris_in, culled, clipped, tris_drawn, verts;
    unsigned long long bbox_px, covered_px, written_px;
} work_t;

static uint8_t buf[256u * 192u];

static uint8_t code_of(float q) {                /* q = 1/z; nearest = 255 */
    const float t = (q - 1.0f / FAR_Z) * (255.0f / (1.0f / NEAR_Z - 1.0f / FAR_Z));
    return (uint8_t)(t <= 0.0f ? 0u : t >= 255.0f ? 255u : (unsigned)(t + 0.5f));
}

/* One screen triangle (x, y, q = 1/z). Pixel centres at +0.5; a pixel is drawn when its centre is inside all three
   edges, with a top-left rule for centres on an edge (the rule the PVR is reported to use, PVR-082). */
static void raster(const cam_t *c, const float s[3][3], work_t *w) {
    float area = (s[1][0] - s[0][0]) * (s[2][1] - s[0][1]) - (s[2][0] - s[0][0]) * (s[1][1] - s[0][1]);
    float minx, maxx, miny, maxy, inv, qa, qb, qc;
    int x0, x1, y0, y1, x, y, k;
    if (area <= 0.0f) { w->culled++; return; }          /* back face, or no area */
    w->tris_drawn++;
    inv = sh4der_recip_pos_(area);
    minx = s[0][0]; maxx = minx; miny = s[0][1]; maxy = miny;
    for (k = 1; k < 3; k++) {
        if (s[k][0] < minx) minx = s[k][0];
        if (s[k][0] > maxx) maxx = s[k][0];
        if (s[k][1] < miny) miny = s[k][1];
        if (s[k][1] > maxy) maxy = s[k][1];
    }
    x0 = minx < 0.0f ? 0 : (int)minx; x1 = maxx >= (float)c->w ? (int)c->w - 1 : (int)maxx;
    y0 = miny < 0.0f ? 0 : (int)miny; y1 = maxy >= (float)c->h ? (int)c->h - 1 : (int)maxy;
    /* q as a plane over the screen: q = qa x + qb y + qc */
    qa = ((s[1][2] - s[0][2]) * (s[2][1] - s[0][1]) - (s[2][2] - s[0][2]) * (s[1][1] - s[0][1])) * inv;
    qb = ((s[2][2] - s[0][2]) * (s[1][0] - s[0][0]) - (s[1][2] - s[0][2]) * (s[2][0] - s[0][0])) * inv;
    qc = s[0][2] - qa * s[0][0] - qb * s[0][1];
    for (y = y0; y <= y1; y++) {
        const float py = (float)y + 0.5f;
        for (x = x0; x <= x1; x++) {
            const float px = (float)x + 0.5f;
            int in = 1;
            w->bbox_px++;
            for (k = 0; k < 3 && in; k++) {
                const float *a = s[k], *b = s[(k + 1) % 3];
                const float e = (b[0] - a[0]) * (py - a[1]) - (b[1] - a[1]) * (px - a[0]);
                const int top_left = (a[1] == b[1] && b[0] < a[0]) || (b[1] < a[1]);
                if (e < 0.0f || (e == 0.0f && !top_left)) in = 0;
            }
            if (!in) continue;
            w->covered_px++;
            {
                const uint8_t code = code_of(qa * px + qb * py + qc);
                uint8_t *p = &buf[(unsigned)y * c->w + (unsigned)x];
                if (code > *p) { *p = code; w->written_px++; }
            }
        }
    }
}

/* Project a view-space point: screen x right, y down. */
static void project(const cam_t *c, const float v[3], float out[3]) {
    const float q = sh4der_recip_pos_(v[2]);
    out[0] = 0.5f * (float)c->w + c->f * v[0] * q;
    out[1] = 0.5f * (float)c->h - c->f * v[1] * q;
    out[2] = q;
}

/* The whole map: clear, then every triangle clipped at the near plane (a triangle becomes 0, 1 or 2). */
static void cpu_map(const cam_t *c, work_t *w) {
    unsigned t, k;
    memset(w, 0, sizeof *w);
    memset(buf, 0, (size_t)c->w * c->h);
    w->verts = nv;
    for (t = 0; t < nt; t++) {
        float poly[4][3], s[4][3];
        unsigned n = 0;
        w->tris_in++;
        for (k = 0; k < 3u; k++) {                /* Sutherland-Hodgman against z = NEAR_Z */
            const float *a = vpos[tri[t][k]], *b = vpos[tri[t][(k + 1u) % 3u]];
            const int ain = a[2] >= NEAR_Z, bin = b[2] >= NEAR_Z;
            if (ain) { memcpy(poly[n], a, sizeof poly[n]); n++; }
            if (ain != bin) {
                const float u = (NEAR_Z - a[2]) / (b[2] - a[2]);
                poly[n][0] = a[0] + u * (b[0] - a[0]); poly[n][1] = a[1] + u * (b[1] - a[1]); poly[n][2] = NEAR_Z; n++;
            }
        }
        if (n < 3u) { w->clipped++; continue; }
        if (n == 4u) w->clipped++;
        for (k = 0; k < n; k++) project(c, poly[k], s[k]);
        {
            const float t0[3][3] = { { s[0][0], s[0][1], s[0][2] }, { s[1][0], s[1][1], s[1][2] }, { s[2][0], s[2][1], s[2][2] } };
            raster(c, t0, w);
        }
        if (n == 4u) {
            const float t1[3][3] = { { s[0][0], s[0][1], s[0][2] }, { s[2][0], s[2][1], s[2][2] }, { s[3][0], s[3][1], s[3][2] } };
            raster(c, t1, w);
        }
    }
}

/* ---- the reference: a ray through each pixel centre, nearest hit (double) ---------------------- */

static unsigned ref_code(const cam_t *c, unsigned x, unsigned y) {
    const double dx = ((double)x + 0.5 - 0.5 * c->w) / c->f, dy = -((double)y + 0.5 - 0.5 * c->h) / c->f;
    double best = 1e30;
    unsigned t;
    for (t = 0; t < nt; t++) {                    /* Moller-Trumbore with the ray (dx, dy, 1) t from the eye */
        const float *A = vpos[tri[t][0]], *B = vpos[tri[t][1]], *C = vpos[tri[t][2]];
        const double e1[3] = { B[0] - A[0], B[1] - A[1], B[2] - A[2] }, e2[3] = { C[0] - A[0], C[1] - A[1], C[2] - A[2] };
        const double d[3] = { dx, dy, 1.0 };
        const double p[3] = { d[1] * e2[2] - d[2] * e2[1], d[2] * e2[0] - d[0] * e2[2], d[0] * e2[1] - d[1] * e2[0] };
        const double det = e1[0] * p[0] + e1[1] * p[1] + e1[2] * p[2];
        double s[3], q[3], u, v, z;
        if (fabs(det) < 1e-12) continue;
        s[0] = -A[0]; s[1] = -A[1]; s[2] = -A[2];
        u = (s[0] * p[0] + s[1] * p[1] + s[2] * p[2]) / det;
        if (u < 0.0 || u > 1.0) continue;
        q[0] = s[1] * e1[2] - s[2] * e1[1]; q[1] = s[2] * e1[0] - s[0] * e1[2]; q[2] = s[0] * e1[1] - s[1] * e1[0];
        v = (d[0] * q[0] + d[1] * q[1] + d[2] * q[2]) / det;
        if (v < 0.0 || u + v > 1.0) continue;
        z = (e2[0] * q[0] + e2[1] * q[1] + e2[2] * q[2]) / det;   /* the ray's t is its view z, since d.z = 1 */
        if (z >= NEAR_Z && z < best) best = z;
    }
    return best > 1e29 ? 0u : code_of((float)(1.0 / best));
}

/* ---- the prices ---------------------------------------------------------------------------------- */

#define CYC_PER_US 200.186                       /* SH4-001: 1 ms = 200,176 to 200,186 cycles (MEASURED) */

typedef struct { double cpu_lo, cpu_hi, pvr_sh4_lo, pvr_sh4_hi, pvr_gpu; } price_t;

static price_t price(const cam_t *c, const work_t *w) {
    price_t p;
    const double lines = (double)c->w * c->h / 32.0;
    /* Both routes transform every vertex: mat_transform 13.16 cycles a vertex in the cache, 43.77 out of it
       (SH4-020, SH4-022, MEASURED, h2). */
    const double xf_lo = 13.16 * w->verts, xf_hi = 43.77 * w->verts;
    /* CPU route.
       clear: MOVCA, 16.24 cycles a line (SH4-004, MEASURED).
       setup a drawn triangle: ESTIMATE 80 cycles (3 edges and the q plane: about 30 multiply-adds, one FSRRA, the box;
         an instruction count, not measured). Clipping is in the same count.
       per pixel, low: ESTIMATE 3 cycles a box pixel tested, 3 more a covered one (incremental edges, dual issue).
       per pixel, high: the MODEL of this repo's depth raster kernel, 12 cycles a rejected texel and 43 a covered one
         (THESIS F14: sh4zam-dce KERNEL_GUIDE, tri_row; a pipeline model, cache hits assumed).
       the buffer is 12 KB at 128 x 96, under the 16 KB operand cache (FACT, Sega SysArc Table 2-4): hits.
       send: 128 x 128 x 2 = 32 KB by store queue, 146 MB/s at 32 KB (BUS-010, MEASURED): 224 us; the widening of each
         byte to a 16-bit texel rides in the same loop (ESTIMATE: no extra cost counted). */
    const double clear = 16.24 * lines;
    const double setup = 80.0 * w->tris_drawn;
    const double px_lo = 3.0 * (double)w->bbox_px + 3.0 * (double)w->covered_px;
    const double px_hi = 12.0 * (double)(w->bbox_px - w->covered_px) + 43.0 * (double)w->covered_px;
    const double send_us = 32768.0 / 146.2;     /* bytes / (MB/s) = us */
    unsigned long long ta_verts;
    p.cpu_lo = (xf_lo + clear + setup + px_lo) / CYC_PER_US + send_us;
    p.cpu_hi = (xf_hi + clear + setup + px_hi) / CYC_PER_US + send_us;
    /* PVR route.
       SH-4: the transform, then the TA: 142 ns a vertex in one store-queue run (BUS-003), 260 ns one at a time
         (BUS-001), MEASURED. Vertices sent: 3 a triangle drawn (no strips; ESTIMATE, an upper bound for boxes).
       PVR: the map pass cut to its tiles took 375 us for c4's 128 x 128 map of 68 vertices (PVR-092, MEASURED);
         4,080 opaque triangles spread over a screen took 588 us (PVR-009, MEASURED), 0.144 us a triangle, added
         here per triangle (ESTIMATE: the map is smaller than a screen, so this is high). Not counted: the waits a
         separate render adds to the frame (PVR-094, OPEN; c4's frames moved in 10 ms steps). */
    ta_verts = 3ull * w->tris_drawn;
    p.pvr_sh4_lo = xf_lo / CYC_PER_US + 0.142 * (double)ta_verts;
    p.pvr_sh4_hi = xf_hi / CYC_PER_US + 0.260 * (double)ta_verts;
    p.pvr_gpu = 375.0 + 0.144 * w->tris_drawn;
    return p;
}

/* ---- the runs ------------------------------------------------------------------------------------ */

static void check_scene(unsigned boxes) {
    const cam_t c = { 128u, 96u, 64.0f / 0.57735f };   /* 60 degrees across */
    work_t w;
    price_t p;
    unsigned x, y, worse = 0, edge = 0, total = 0;
    scene(boxes);
    cpu_map(&c, &w);
    for (y = 0; y < c.h; y++) {
        for (x = 0; x < c.w; x++) {
            const int r = (int)ref_code(&c, x, y), m = buf[y * c.w + x], d = r > m ? r - m : m - r;
            total++;
            if (d > 1) {
                /* a pixel whose 8 neighbours do not all agree with the reference is on an edge: allowed to differ */
                int nb_diff = 0, ox, oy;
                for (oy = -1; oy <= 1; oy++) for (ox = -1; ox <= 1; ox++) {
                    const int xx = (int)x + ox, yy = (int)y + oy;
                    if (xx >= 0 && yy >= 0 && xx < (int)c.w && yy < (int)c.h && (ox || oy)) {
                        const int rr = (int)ref_code(&c, (unsigned)xx, (unsigned)yy);
                        if (rr != r) nb_diff = 1;
                    }
                }
                if (nb_diff) edge++; else worse++;
            }
        }
    }
    p = price(&c, &w);
    printf("%6u tris (%5u drawn, %5u culled, %3u clipped), %5u verts: box px %7llu, covered %6llu (%.2f x the map), written %6llu;"
           " off by > 1 code: %u inside, %u on edges (%.2f %%)\n",
           w.tris_in, w.tris_drawn, w.culled, w.clipped, w.verts, w.bbox_px, w.covered_px, (double)w.covered_px / total, w.written_px,
           worse, edge, 100.0 * edge / total);
    printf("        CPU route %7.0f .. %7.0f us SH-4 | PVR route SH-4 %6.0f .. %6.0f us + PVR %6.0f us (+ its waits, OPEN)\n",
           p.cpu_lo, p.cpu_hi, p.pvr_sh4_lo, p.pvr_sh4_hi, p.pvr_gpu);
    CHECK(worse == 0u);                            /* away from edges the raster is the ray cast to within one code */
    CHECK(edge * 50u <= total);                    /* edges: under 2 % of the map */
}

static void sizes(void) {
    static const unsigned ws[3] = { 64u, 128u, 256u }, hs[3] = { 48u, 96u, 192u };
    unsigned k;
    scene(100u);
    for (k = 0; k < 3u; k++) {
        const cam_t c = { ws[k], hs[k], 0.5f * (float)ws[k] / 0.57735f };
        work_t w;
        price_t p;
        cpu_map(&c, &w);
        p = price(&c, &w);
        printf("size %3u x %3u, 1,202 tris: covered %7llu px; CPU route %6.0f .. %6.0f us (send counted at 32 KB for every size;"
               " buffer %u KB%s)\n", ws[k], hs[k], w.covered_px, p.cpu_lo, p.cpu_hi, ws[k] * hs[k] / 1024u,
               ws[k] * hs[k] > 16384u ? ", over the 16 KB cache: the hit prices are low" : "");
    }
}

int main(void) {
    static const unsigned boxes[5] = { 8u, 25u, 100u, 250u, 1000u };
    unsigned k;
    printf("camera depth map 128 x 96, 8-bit, near %.1f far %.0f; all prices ESTIMATE (see price())\n", (double)NEAR_Z, (double)FAR_Z);
    for (k = 0; k < 5u; k++) check_scene(boxes[k]);
    sizes();
    printf("depthmap_model: PASS %u FAIL %u\n", passes, fails);
    return fails ? 1 : 0;
}
