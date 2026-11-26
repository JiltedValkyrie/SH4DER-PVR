/* SH4DER-PVR: the town's data for the ray bench (THESIS_SH4_SHADOWS.md P1, P2, P9), made from the town file
   demo/psm/tools/town_ray_data.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A host tool. It reads the town as the demo loads it (psm_scene_init_town, plain C path) and writes one header of
   plain data for a bench: the static casters' height grid (thesis 5.3: 64 x 64 cells of 1.5 m, 1 byte a cell), its
   4 x 4 block maxima, the overhang cells and the caster triangles over them, and the twelve movers (8 people, 4 hens)
   with their sizes, start places, bounds and shadow runs at 14:00 and 16:00. It changes nothing in the town.

   build (from the repo root, after the town's mesh header is made by make):
     cc -O2 -std=gnu11 -DSH4DER_CORE_NO_SH4ZAM -DPSM_HAVE_TOWN -Idemo/psm -Iinclude \
        -I<BUILD>/psm_town -I<BUILD>/psm_sky demo/psm/tools/town_ray_data.c src/sh4der_core.c -lm -o town_ray_data
     ./town_ray_data > demo/psm/town_ray_data.h

   World coordinates (the demo's): x east, y up, z = minus the story's north. Metres. */
#include <stdio.h>
#include <string.h>
#include "psm_scene.h"

#define G_N 64u                    /* cells a side (thesis 5.3) */
#define G_C 1.5f                   /* metres a cell */
#define G_X0 (-58.5f)              /* the grid's corner: centred on the static town's box, x -38.3 .. 18.0, z -18.0 .. 35.0 */
#define G_Z0 (-39.0f)
#define G_B 4u                     /* block side for the block maxima, cells */
#define SAMPLE 0.2f                /* metres between samples on a caster triangle */
#define LOW 1.0f                   /* a cell whose lowest caster surface is above this, over open ground, is an overhang */

static psm_scene_t S;
static float hmax[G_N][G_N], hmin[G_N][G_N];
static uint16_t ntri[G_N][G_N];
static uint8_t touch[PSM_CT_MAX];

static int cell_of(float x, float z, unsigned *i, unsigned *j) {
    const float fx = (x - G_X0) / G_C, fz = (z - G_Z0) / G_C;
    if (fx < 0.0f || fz < 0.0f || fx >= (float)G_N || fz >= (float)G_N) return 0;
    *i = (unsigned)fx; *j = (unsigned)fz;
    return 1;
}

/* every cell a caster triangle passes over (sampled at SAMPLE metres), with the height it has there */
static void raster(unsigned t) {
    const float *a = S.cv[S.ct[t][0]], *b = S.cv[S.ct[t][1]], *c = S.cv[S.ct[t][2]];
    float l1 = 0, l2 = 0;
    unsigned n, u, v, i, j, k;
    static uint32_t seen[G_N][G_N];
    static uint32_t stamp;
    for (k = 0; k < 3; k++) { l1 += (b[k] - a[k]) * (b[k] - a[k]); l2 += (c[k] - a[k]) * (c[k] - a[k]); }
    n = 1u + (unsigned)(sqrtf(l1 > l2 ? l1 : l2) / SAMPLE);
    stamp++;
    for (u = 0; u <= n; u++)
        for (v = 0; u + v <= n; v++) {
            const float s = (float)u / n, r = (float)v / n;
            const float p[3] = { a[0] + s * (b[0] - a[0]) + r * (c[0] - a[0]), a[1] + s * (b[1] - a[1]) + r * (c[1] - a[1]),
                                 a[2] + s * (b[2] - a[2]) + r * (c[2] - a[2]) };
            if (!cell_of(p[0], p[2], &i, &j)) continue;
            if (p[1] > hmax[i][j]) hmax[i][j] = p[1];
            if (p[1] < hmin[i][j]) hmin[i][j] = p[1];
            if (seen[i][j] != stamp) { seen[i][j] = stamp; ntri[i][j]++; }
        }
}

/* 1 when the point is inside a closed caster shell: crossings of a ray toward +x with every static caster triangle */
static int inside(const float p[3]) {
    unsigned t, n = 0;
    for (t = 0; t < S.ctn; t++) {
        const float *a, *b, *c;
        float e1[3], e2[3], h[3], s[3], q[3], det, f, u, v, tt;
        if (S.ctmov[t]) continue;
        a = S.cv[S.ct[t][0]]; b = S.cv[S.ct[t][1]]; c = S.cv[S.ct[t][2]];
        e1[0] = b[0]-a[0]; e1[1] = b[1]-a[1]; e1[2] = b[2]-a[2]; e2[0] = c[0]-a[0]; e2[1] = c[1]-a[1]; e2[2] = c[2]-a[2];
        h[0] = 0; h[1] = -e2[2]; h[2] = e2[1];                       /* d = (1, 0, 0): h = d x e2 */
        det = e1[0]*h[0] + e1[1]*h[1] + e1[2]*h[2];
        if (fabsf(det) < 1e-9f) continue;
        f = 1.0f / det;
        s[0] = p[0]-a[0]; s[1] = p[1]-a[1]; s[2] = p[2]-a[2];
        u = f * (s[0]*h[0] + s[1]*h[1] + s[2]*h[2]);
        if (u < 0 || u > 1) continue;
        q[0] = s[1]*e1[2]-s[2]*e1[1]; q[1] = s[2]*e1[0]-s[0]*e1[2]; q[2] = s[0]*e1[1]-s[1]*e1[0];
        v = f * q[0];
        if (v < 0 || u + v > 1) continue;
        tt = f * (e2[0]*q[0] + e2[1]*q[1] + e2[2]*q[2]);
        if (tt > 1e-4f) n++;
    }
    return n & 1u;
}

int main(void) {
    float H = 0, az14, el14, az16, el16, tan14, tan16;
    unsigned i, j, t, m, nstatic = 0, over = 0, occupied = 0, listed = 0, tlisted = 0;
    static uint8_t over_flag[G_N][G_N];

    psm_scene_init(&S);
    psm_scene_init_town(&S);
    for (i = 0; i < G_N; i++) for (j = 0; j < G_N; j++) { hmax[i][j] = 0.0f; hmin[i][j] = 1e30f; }
    for (t = 0; t < S.ctn; t++) if (!S.ctmov[t]) { raster(t); nstatic++; }
    for (i = 0; i < G_N; i++) for (j = 0; j < G_N; j++) if (hmax[i][j] > H) H = hmax[i][j];
    for (i = 0; i < G_N; i++)
        for (j = 0; j < G_N; j++) {
            float p[3];
            if (hmax[i][j] <= 0.05f) continue;
            occupied++;
            p[0] = G_X0 + (i + 0.5f) * G_C; p[1] = 0.5f; p[2] = G_Z0 + (j + 0.5f) * G_C;
            if (hmin[i][j] > LOW && !inside(p)) { over_flag[i][j] = 1; over++; listed += ntri[i][j]; }
        }
    /* the distinct caster triangles over an overhang cell */
    for (t = 0; t < S.ctn; t++) {
        const float *a, *b, *c;
        unsigned n, u, v, k;
        float l1 = 0, l2 = 0;
        if (S.ctmov[t]) continue;
        a = S.cv[S.ct[t][0]]; b = S.cv[S.ct[t][1]]; c = S.cv[S.ct[t][2]];
        for (k = 0; k < 3; k++) { l1 += (b[k] - a[k]) * (b[k] - a[k]); l2 += (c[k] - a[k]) * (c[k] - a[k]); }
        n = 1u + (unsigned)(sqrtf(l1 > l2 ? l1 : l2) / SAMPLE);
        for (u = 0; u <= n && !touch[t]; u++)
            for (v = 0; u + v <= n; v++) {
                const float s = (float)u / n, r = (float)v / n;
                if (cell_of(a[0] + s * (b[0] - a[0]) + r * (c[0] - a[0]), a[2] + s * (b[2] - a[2]) + r * (c[2] - a[2]), &i, &j) && over_flag[i][j]) {
                    touch[t] = 1; break;
                }
            }
        tlisted += touch[t];
    }
    psm_sun_at(14.0f, &az14, &el14);
    psm_sun_at(16.0f, &az16, &el16);
    tan14 = tanf(el14 * 0.01745329f); tan16 = tanf(el16 * 0.01745329f);

    printf("/* SH4DER-PVR: the town's data for the ray bench. GENERATED by demo/psm/tools/town_ray_data.c from the town file\n"
           "   (psm_town.h and the baked mesh); do not edit. Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.\n\n"
           "   Tags: FACT = read or computed exactly from the town file; ESTIMATE = this tool's heuristic or a model.\n"
           "   World coordinates (the demo's): x east, y up, z = minus the story's north. Metres. */\n\n");
    printf("#ifndef TOWN_RAY_DATA_H\n#define TOWN_RAY_DATA_H\n\n#include <stdint.h>\n\n");
    printf("/* The grid. FACT (the layout is THESIS 5.3's, placed here on the town's box): %u x %u cells of %.1f m, corner (x %.1f, z %.1f),\n"
           "   covering x %.1f .. %.1f, z %.1f .. %.1f; the static town's casters lie in x -38.3 .. 18.0, z -18.0 .. 35.0 (FACT).\n"
           "   1 byte a cell: the tallest static caster surface over the cell, in steps of TOWN_RAY_H / 255 (%.1f cm). %u bytes. */\n",
           G_N, G_N, G_C, G_X0, G_Z0, G_X0, G_X0 + G_N * G_C, G_Z0, G_Z0 + G_N * G_C, 100.0f * H / 255.0f, G_N * G_N);
    printf("#define TOWN_RAY_N %uu\n#define TOWN_RAY_CELL %.1ff\n#define TOWN_RAY_X0 (%.1ff)\n#define TOWN_RAY_Z0 (%.1ff)\n", G_N, G_C, G_X0, G_Z0);
    printf("#define TOWN_RAY_H %.3ff               /* FACT: the tallest static caster point (the tower's roof), metres */\n", H);
    printf("#define TOWN_RAY_STATIC_CASTER_TRIS %uu   /* FACT: static caster triangles rasterised (of %u, movers' excluded) */\n", nstatic, S.ctn);
    printf("#define TOWN_RAY_OCCUPIED %uu          /* FACT (sampled every %.1f m): cells with a caster over them */\n", occupied, SAMPLE);
    printf("/* row j (z), column i (x): h[j][i] */\nstatic const uint8_t town_ray_h[%u][%u] = {\n", G_N, G_N);
    for (j = 0; j < G_N; j++) {
        printf("    {");
        for (i = 0; i < G_N; i++) printf("%3u%s", (unsigned)(hmax[i][j] <= 0.0f ? 0.0f : (hmax[i][j] * 255.0f / H + 0.999f > 255.0f ? 255.0f : hmax[i][j] * 255.0f / H + 0.999f)), i + 1 < G_N ? "," : "");
        printf(" },\n");
    }
    printf("};\n\n");
    printf("/* Block maxima, %u x %u cells a block (the thesis's two-level walk): %u x %u bytes. FACT (from the grid). */\nstatic const uint8_t town_ray_hb[%u][%u] = {\n",
           G_B, G_B, G_N / G_B, G_N / G_B, G_N / G_B, G_N / G_B);
    for (j = 0; j < G_N / G_B; j++) {
        printf("    {");
        for (i = 0; i < G_N / G_B; i++) {
            unsigned a, b, mx = 0;
            for (a = 0; a < G_B; a++) for (b = 0; b < G_B; b++) {
                const float h = hmax[i * G_B + a][j * G_B + b];
                const unsigned q = h <= 0.0f ? 0u : (unsigned)(h * 255.0f / H + 0.999f > 255.0f ? 255.0f : h * 255.0f / H + 0.999f);
                if (q > mx) mx = q;
            }
            printf("%3u%s", mx, i + 1 < G_N / G_B ? "," : "");
        }
        printf(" },\n");
    }
    printf("};\n\n");
    printf("/* Overhang cells. ESTIMATE (this tool's rule): the lowest caster surface over the cell is above %.1f m and the cell's\n"
           "   centre at 0.5 m is not inside a closed caster shell (eaves, the arch's passage, canopies). %u cells of %u occupied.\n"
           "   Over them: %u (cell, triangle) list entries, %u distinct static caster triangles (36 B each: %u B as one list). */\n",
           LOW, over, occupied, listed, tlisted, 36u * tlisted);
    printf("#define TOWN_RAY_OVERHANG_CELLS %uu\n#define TOWN_RAY_OVERHANG_ENTRIES %uu\n#define TOWN_RAY_OVERHANG_TRIS %uu\n", over, listed, tlisted);
    printf("static const uint8_t town_ray_overhang[%u][%u / 8u] = {   /* bit i & 7 of byte i >> 3 in row j */\n", G_N, G_N);
    for (j = 0; j < G_N; j++) {
        printf("    {");
        for (i = 0; i < G_N / 8; i++) {
            unsigned b, v = 0;
            for (b = 0; b < 8; b++) v |= (unsigned)over_flag[i * 8 + b][j] << b;
            printf("0x%02x%s", v, i + 1 < G_N / 8 ? "," : "");
        }
        printf(" },\n");
    }
    printf("};\n\n");

    printf("/* The sun. FACT (psm_sun_at): 14:00 az %.2f el %.2f (tan %.4f); 16:00 az %.2f el %.2f (tan %.4f).\n"
           "   The direction toward the sun, world: (sin az cos el, sin el, -cos az cos el) (psm_scene.h's f->sun). */\n",
           az14, el14, tan14, az16, el16, tan16);
    printf("#define TOWN_RAY_SUN14_AZ %.4ff\n#define TOWN_RAY_SUN14_EL %.4ff\n#define TOWN_RAY_SUN16_AZ %.4ff\n#define TOWN_RAY_SUN16_EL %.4ff\n\n",
           az14, el14, az16, el16);

    printf("/* The movers: 8 people and 4 hens. FACT: kind, size (the caster template's box, local to the feet), the place at\n"
           "   load (cast time 0: psm_cast_at(i, 23 i) for a person; a hen's home), a person's route box (its waypoints) or a\n"
           "   hen's pen. Shadow run = height / tan el: how far past its feet its shadow reaches on flat ground (DERIVED).\n"
           "   The movers walk on the cast's own clock (seconds), not the sun's: where they are AT 16:00 is not defined; the\n"
           "   bench should take the run lengths at 16:00 with the places below, or any place in the route box. */\n");
    printf("typedef struct { uint8_t kind; float lo[3], hi[3]; float x, z; float box[4]; float run14, run16; } town_ray_mover_t;\n");
    printf("#define TOWN_RAY_MOVERS %uu\nstatic const town_ray_mover_t town_ray_movers[%u] = {\n", S.movern, S.movern);
    for (m = 0; m < S.movern; m++) {
        const psm_mover_t *mv = &S.mover[m];
        float lo[3] = { 1e30f, 1e30f, 1e30f }, hi[3] = { -1e30f, -1e30f, -1e30f }, bx[4];
        unsigned k, a;
        for (k = 0; k < mv->cvn; k++) {
            const float *p = S.dcloc[mv->cv0 - S.cdyn0 + k];
            for (a = 0; a < 3; a++) { if (p[a] < lo[a]) lo[a] = p[a]; if (p[a] > hi[a]) hi[a] = p[a]; }
        }
        if (mv->kind == 0u) {
            const unsigned f = psm_cast_route[m][0], n = psm_cast_route[m][1];
            bx[0] = bx[2] = 1e30f; bx[1] = bx[3] = -1e30f;
            for (k = 0; k < n; k++) {
                const float x = psm_cast_wp[f + k].x, z = -psm_cast_wp[f + k].z;
                if (x < bx[0]) bx[0] = x;
                if (x > bx[1]) bx[1] = x;
                if (z < bx[2]) bx[2] = z;
                if (z > bx[3]) bx[3] = z;
            }
        } else { bx[0] = mv->b[0]; bx[1] = mv->b[1]; bx[2] = -mv->b[3]; bx[3] = -mv->b[2]; }
        printf("    { %u, { %.2ff, %.2ff, %.2ff }, { %.2ff, %.2ff, %.2ff }, %.2ff, %.2ff, { %.1ff, %.1ff, %.1ff, %.1ff }, %.2ff, %.2ff },   /* %s */\n",
               mv->kind, lo[0], lo[1], lo[2], hi[0], hi[1], hi[2], mv->x, -mv->z, bx[0], bx[1], bx[2], bx[3],
               hi[1] / tan14, hi[1] / tan16, mv->kind ? "hen" : psm_cast_name[m]);
    }
    printf("};\n\n#endif /* TOWN_RAY_DATA_H */\n");
    fprintf(stderr, "grid %ux%u at %.1f m, H %.2f m (%.1f cm a step), occupied %u, overhang %u cells, %u entries, %u tris; static caster tris %u; movers %u\n",
            G_N, G_N, G_C, H, 100.0f * H / 255.0f, occupied, over, listed, tlisted, nstatic, S.movern);
    return 0;
}
