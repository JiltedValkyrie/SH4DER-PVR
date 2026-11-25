/* SH4DER-PVR: a host prototype of the town's static shadow bake (TOWN_NOTES.md, "Static on static: the bake")
   demo/psm/tools/town_bake.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Build (the town mesh header from tools/bake_town.py in <BUILD>/psm_town, the sky in <BUILD>/psm_sky):
     cc -O2 -std=gnu11 -DSH4DER_CORE_NO_SH4ZAM -DPSM_HAVE_TOWN -Idemo/psm -Iinclude -Ish4dow-dc/include
        -I<BUILD>/psm_town -I<BUILD>/psm_sky demo/psm/tools/town_bake.c src/sh4der_core.c -lm -o town_bake
   Run: town_bake [map side, 256] [out dir, .] [normal offset m, 0.05] [sun offset m, 0] [1: the screen mesh occludes]
   It writes town_lightmap_<hour>_<side>.pgm and never overwrites one.

   Static-on-static shadow bake for the town, one fixed sun.
   Model vertices: N.L times a hard visibility ray toward the sun against the static caster triangles (movers' excluded).
   Ground: a world lightmap over the ray grid's 96 x 96 m square, one ray a texel centre at y = 0.02.
   Acceleration: casters binned on a 2D grid in light space (the plane across the sun), so a ray tests only its bin.
   Prints counts and ray-triangle tests (for the SH-4 load-time ESTIMATE) and writes the lightmap as PGM. Plain C path. */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "psm_scene.h"

static psm_scene_t S;
static float lx[3], ly[3], sd[3];
#define LB 96u                         /* light-space bins a side */
static float bu0, bv0, bsz;
static uint16_t *bin[LB][LB];
static unsigned binn[LB][LB];
static unsigned long long tests;
static const float *TV[8192][3];        /* the occluder triangles: the caster proxies, or the screen mesh (argv 5 = 1) */
static unsigned TN;

static float dot(const float *a, const float *b) { return a[0]*b[0] + a[1]*b[1] + a[2]*b[2]; }
static void cross(const float *a, const float *b, float *o) { o[0]=a[1]*b[2]-a[2]*b[1]; o[1]=a[2]*b[0]-a[0]*b[2]; o[2]=a[0]*b[1]-a[1]*b[0]; }
static void norm(float *v) { float l = sqrtf(dot(v, v)); if (l > 0) { v[0]/=l; v[1]/=l; v[2]/=l; } }

static int ray_tri(const float o[3], const float d[3], const float *a, const float *b, const float *c) {
    float e1[3] = { b[0]-a[0], b[1]-a[1], b[2]-a[2] }, e2[3] = { c[0]-a[0], c[1]-a[1], c[2]-a[2] }, h[3], s[3], q[3], det, u, v;
    cross(d, e2, h); det = dot(e1, h);
    if (fabsf(det) < 1e-9f) return 0;
    det = 1.0f / det;
    s[0] = o[0]-a[0]; s[1] = o[1]-a[1]; s[2] = o[2]-a[2];
    u = dot(s, h) * det; if (u < 0 || u > 1) return 0;
    cross(s, e1, q);
    v = dot(d, q) * det; if (v < 0 || u + v > 1) return 0;
    return dot(e2, q) * det > 1e-3f;
}

static void setup(float az, float el) {
    static const float up[3] = { 0, 1, 0 };
    float lo[2] = { 1e30f, 1e30f }, hi[2] = { -1e30f, -1e30f };
    unsigned t, k, i, j, pass;
    sd[0] = sinf(az * 0.01745329f) * cosf(el * 0.01745329f); sd[1] = sinf(el * 0.01745329f); sd[2] = -cosf(az * 0.01745329f) * cosf(el * 0.01745329f);
    cross(up, sd, lx); norm(lx); cross(sd, lx, ly);
    /* the bin square covers the casters and the whole ground square's corners */
    for (k = 0; k < 4; k++) {
        const float p[3] = { (k & 1) ? 37.5f : -58.5f, 0, (k & 2) ? 57.0f : -39.0f };
        const float u = dot(p, lx), v = dot(p, ly);
        lo[0] = fminf(lo[0], u); hi[0] = fmaxf(hi[0], u); lo[1] = fminf(lo[1], v); hi[1] = fmaxf(hi[1], v);
    }
    for (k = 0; k < S.cvn; k++) {
        const float u = dot(S.cv[k], lx), v = dot(S.cv[k], ly);
        lo[0] = fminf(lo[0], u); hi[0] = fmaxf(hi[0], u); lo[1] = fminf(lo[1], v); hi[1] = fmaxf(hi[1], v);
    }
    bu0 = lo[0]; bv0 = lo[1];
    bsz = ((hi[0] - lo[0]) > (hi[1] - lo[1]) ? (hi[0] - lo[0]) : (hi[1] - lo[1])) / LB + 1e-4f;
    for (i = 0; i < LB; i++) for (j = 0; j < LB; j++) { free(bin[i][j]); bin[i][j] = NULL; binn[i][j] = 0; }
    for (pass = 0; pass < 2; pass++) {
        if (pass) for (i = 0; i < LB; i++) for (j = 0; j < LB; j++) { bin[i][j] = malloc((binn[i][j] + 1) * sizeof(uint16_t)); binn[i][j] = 0; }
        for (t = 0; t < TN; t++) {
            float u0 = 1e30f, u1 = -1e30f, v0 = 1e30f, v1 = -1e30f;
            unsigned a0, a1, c0, c1;
            for (k = 0; k < 3; k++) {
                const float *p = TV[t][k], u = dot(p, lx), v = dot(p, ly);
                u0 = fminf(u0, u); u1 = fmaxf(u1, u); v0 = fminf(v0, v); v1 = fmaxf(v1, v);
            }
            a0 = (unsigned)((u0 - bu0) / bsz); a1 = (unsigned)((u1 - bu0) / bsz);
            c0 = (unsigned)((v0 - bv0) / bsz); c1 = (unsigned)((v1 - bv0) / bsz);
            a1 = a1 < LB ? a1 : LB - 1; c1 = c1 < LB ? c1 : LB - 1;
            for (i = a0; i <= a1; i++) for (j = c0; j <= c1; j++) { if (pass) bin[i][j][binn[i][j]] = (uint16_t)t; binn[i][j]++; }
        }
    }
}

/* 1 when p sees the sun past the static casters */
static int sees(const float p[3]) {
    const float u = dot(p, lx), v = dot(p, ly);
    unsigned i, j, k;
    if (u < bu0 || v < bv0) return 1;
    i = (unsigned)((u - bu0) / bsz); j = (unsigned)((v - bv0) / bsz);
    if (i >= LB || j >= LB) return 1;
    for (k = 0; k < binn[i][j]; k++) {
        const unsigned t = bin[i][j][k];
        tests++;
        if (ray_tri(p, sd, TV[t][0], TV[t][1], TV[t][2])) return 0;
    }
    return 1;
}

int main(int argc, char **argv) {
    const float hours[2] = { 14.0f, 16.0f };
    const unsigned side = argc > 1 ? (unsigned)atoi(argv[1]) : 256u;
    const char *dir = argc > 2 ? argv[2] : ".";
    const float off = argc > 3 ? (float)atof(argv[3]) : 0.05f, soff = argc > 4 ? (float)atof(argv[4]) : 0.0f;
    unsigned h, i, j, maxbin = 0;
    psm_scene_init(&S);
    psm_scene_init_town(&S);
    if (argc > 5 && atoi(argv[5]) == 1) { for (i = 0; i < S.tdyn0; i++) { TV[TN][0] = S.mv[S.mt[i][0]]; TV[TN][1] = S.mv[S.mt[i][1]]; TV[TN][2] = S.mv[S.mt[i][2]]; TN++; } }
    else for (i = 0; i < S.ctn; i++) if (!S.ctmov[i]) { TV[TN][0] = S.cv[S.ct[i][0]]; TV[TN][1] = S.cv[S.ct[i][1]]; TV[TN][2] = S.cv[S.ct[i][2]]; TN++; }
    printf("occluders: %u triangles (%s)\n", TN, argc > 5 && atoi(argv[5]) == 1 ? "screen mesh" : "caster proxies");
    printf("static model vertices %u, caster triangles %u\n", S.mdyn0, S.ctn);
    for (h = 0; h < 2; h++) {
        float az, el;
        unsigned away = 0, shadowed = 0, lit = 0, gsh = 0;
        unsigned long long mt, gt;
        static uint8_t lm[512 * 512];
        char path[512];
        FILE *f;
        psm_sun_at(hours[h], &az, &el);
        setup(az, el);
        maxbin = 0;
        for (i = 0; i < LB; i++) for (j = 0; j < LB; j++) if (binn[i][j] > maxbin) maxbin = binn[i][j];
        tests = 0;
        for (i = 0; i < S.mdyn0; i++) {
            const float *n = S.mn[i], ndl = dot(n, sd);
            float p[3];
            if (!(ndl > 0.0f)) { away++; continue; }
            p[0] = S.mv[i][0] + n[0] * off + sd[0] * soff; p[1] = S.mv[i][1] + n[1] * off + sd[1] * soff; p[2] = S.mv[i][2] + n[2] * off + sd[2] * soff;
            if (sees(p)) lit++; else shadowed++;
        }
        mt = tests; tests = 0;
        for (j = 0; j < side; j++)
            for (i = 0; i < side; i++) {
                const float p[3] = { -58.5f + (i + 0.5f) * 96.0f / side, 0.02f, -39.0f + (j + 0.5f) * 96.0f / side };
                const int s = sees(p);
                lm[j * side + i] = s ? 255u : 0u;
                gsh += !s;
            }
        gt = tests;
        printf("\n%.0f:00 (az %.2f el %.2f), %u light bins of %.2f m, fullest %u triangles\n", hours[h], az, el, LB, bsz, maxbin);
        printf("  models: %u face away, %u lit, %u shadowed by static casters (%.1f %% of those facing); %llu ray-tri tests (%.1f a ray)\n",
               away, lit, shadowed, 100.0 * shadowed / (lit + shadowed), mt, (double)mt / (lit + shadowed));
        printf("  ground %u x %u (%.3f m a texel): %u shadowed (%.1f %%); %llu ray-tri tests (%.1f a ray)\n",
               side, side, 96.0f / side, gsh, 100.0 * gsh / (side * side), gt, (double)gt / (side * side));
        snprintf(path, sizeof path, "%s/town_lightmap_%02.0f_%u.pgm", dir, hours[h], side);
        if ((f = fopen(path, "rb"))) { fclose(f); printf("  %s exists, not written\n", path); continue; }
        f = fopen(path, "wb");
        if (!f) continue;
        fprintf(f, "P5\n%u %u\n255\n", side, side);
        for (j = 0; j < side; j++) fwrite(&lm[j * side], 1, side, f);   /* north (world -z) at the top */
        fclose(f);
        printf("  wrote %s\n", path);
    }
    return 0;
}
