/* The new map's baked ambient occlusion, a host tool run at build time (never on the console).
 *   map_ao <out folder>     writes <out>/map_ao_gen.c and map_ao_gen.h
 * Built against the baked assets and the map (GEN: assets_gen.c, map_gen.c) and src/map.c, with the occlusion rules of
 * ../../../tools/ao_bake_core.h (64 cosine rays, a hit at t takes 1 - t / R of a ray).
 *   1. A byte a vertex of every map model: the model's own triangles and the ground under it (a 60 m square at y = 0),
 *      reach AOB_R (2.5 m). Instances share their model's bytes: the map's neighbours are not seen.
 *   2. The road: a 2 m grid across the loop's band (lat -12 .. 12 m, the sidewalks too) every 2 m along it, against every instance, reach
 *      10 m (the scene is baked at a scale of AOB_R / 10, so the core is used as it is). Only the rows near a place that
 *      darkens are kept: the demo draws them as multiplied patches over the road.
 * Plain C (host). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "assets.h"
#include "map.h"
#include "map_gen.h"
#include "../../../tools/ao_bake_core.h"

#define ROAD_R     10.0f                    /* the road's reach, m */
#define ROAD_STEP  2.0f
#define ROAD_COLS  13u                      /* lat -12 .. 12: the asphalt and both sidewalks */
#define ROAD_LAT0  (-12.0f)
#define ROAD_KEEP  250u                     /* a row is kept when a byte is under this (or a neighbour row's is); the demo
                                               skips a quad whose four corners are all at or over it (MAP_AOG_OPEN) */
#define ROAD_INSIDE 64u                     /* under it, a road point is taken to be inside a solid (open ground under a deck bakes about 130 .. 200) */
#define GROUND_H   30.0f

static float (*s_ot)[3][3];
static unsigned s_n, s_cap;

static void add_tri(const float a[3], const float b[3], const float c[3], float k) {
    unsigned i;
    if (s_n == s_cap) { s_cap = s_cap ? s_cap * 2u : 4096u; s_ot = realloc(s_ot, sizeof *s_ot * s_cap); if (!s_ot) exit(2); }
    for (i = 0; i < 3u; i++) { s_ot[s_n][0][i] = a[i] * k; s_ot[s_n][1][i] = b[i] * k; s_ot[s_n][2][i] = c[i] * k; }
    s_n++;
}

/* model point to world: draw_model's translate x rotate_y (local +z to (sin yaw, cos yaw), +x to (cos yaw, -sin yaw)) */
static void place(const float p[3], float x, float z, float c, float s, float o[3]) {
    o[0] = x + p[0] * c + p[2] * s; o[1] = p[1]; o[2] = z - p[0] * s + p[2] * c;
}

static int is_car(unsigned m) {
    unsigned k;
    for (k = 0; k < MAP_CARS; k++) if (map_car[k].body == m || map_car[k].wheels == m) return 1;
    return 0;
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    static uint8_t mao[MAP_MODELS][MAP_MAX_NV];
    unsigned m, i, rows, kept = 0, sum = 0, nv = 0, lo = 255;
    uint8_t *road, *keep;
    char path[512];
    FILE *fc, *fh;
    aob_t b;

    /* 1. the models */
    for (m = 0; m < MAP_MODELS; m++) {
        const mdl_t *md = map_models[m];
        const float g[4][3] = { { -GROUND_H, 0, -GROUND_H }, { GROUND_H, 0, -GROUND_H }, { GROUND_H, 0, GROUND_H }, { -GROUND_H, 0, GROUND_H } };
        s_n = 0;
        for (i = 0; i + 2u < md->ni; i += 3u) add_tri(md->v[md->idx[i]].p, md->v[md->idx[i + 1u]].p, md->v[md->idx[i + 2u]].p, 1.0f);
        if (!is_car(m)) { add_tri(g[0], g[2], g[1], 1.0f); add_tri(g[0], g[3], g[2], 1.0f); }   /* cars move: no ground (their tile or blob is the contact) */
        if (!aob_init(&b, (const float (*)[3][3])s_ot, NULL, s_n)) return 2;
        for (i = 0; i < md->nv; i++) {
            int in;
            mao[m][i] = aob_byte(aob_at(&b, md->v[i].p, md->v[i].n, &in));
            sum += mao[m][i]; nv++; if (mao[m][i] < lo) lo = mao[m][i];
        }
        aob_free(&b);
    }
    printf("models: %u, %u vertices, mean byte %u, lowest %u\n", (unsigned)MAP_MODELS, nv, nv ? sum / nv : 0u, lo);

    /* 2. the road */
    {
        const float k = AOB_R / ROAD_R, up[3] = { 0.0f, 1.0f, 0.0f };
        s_n = 0;
        for (i = 0; i < MAP_INST; i++) {
            const map_inst_t *p = &map_inst[i];
            const mdl_t *md = map_models[p->model];
            const float c = cosf(p->yaw), s = sinf(p->yaw);
            for (m = 0; m + 2u < md->ni; m += 3u) {
                float a[3], bb[3], cc[3];
                place(md->v[md->idx[m]].p, p->x, p->z, c, s, a);
                place(md->v[md->idx[m + 1u]].p, p->x, p->z, c, s, bb);
                place(md->v[md->idx[m + 2u]].p, p->x, p->z, c, s, cc);
                add_tri(a, bb, cc, k);
            }
        }
        if (!aob_init(&b, (const float (*)[3][3])s_ot, NULL, s_n)) return 2;
        rows = (unsigned)(MAP_LOOP_LEN / ROAD_STEP);
        road = malloc((size_t)rows * ROAD_COLS); keep = calloc(rows, 1);
        if (!road || !keep) return 2;
        for (i = 0; i < rows; i++)
            for (m = 0; m < ROAD_COLS; m++) {
                const map_pose_t q = map_at((float)i * ROAD_STEP, ROAD_LAT0 + ROAD_STEP * (float)m);
                const float pt[3] = { q.x * k, 0.0f, q.z * k };
                int in;
                road[i * ROAD_COLS + m] = aob_byte(aob_at(&b, pt, up, &in));
            }
        aob_free(&b);
        {   /* a point in a pillar or a wall is not seen: no closed flags here, so a byte under ROAD_INSIDE says it. Such a
               point takes its neighbours' mean (the core's fill), or its dark would bleed over the road between them */
            uint8_t *bad = malloc((size_t)rows * ROAD_COLS);
            if (!bad) return 2;
            for (i = 0; i < rows * ROAD_COLS; i++) bad[i] = road[i] < ROAD_INSIDE;
            printf("road: %u points inside a solid, filled\n", aob_fill(road, bad, ROAD_COLS, rows, 1u));
            free(bad);
        }
        for (i = 0; i < rows; i++)
            for (m = 0; m < ROAD_COLS; m++)
                if (road[i * ROAD_COLS + m] < ROAD_KEEP) { keep[i] = 1; keep[(i + rows - 1u) % rows] = 1; keep[(i + 1u) % rows] = 1; }
    }

    /* out */
    snprintf(path, sizeof path, "%s/map_ao_gen.h", dir);
    fh = fopen(path, "wb");
    snprintf(path, sizeof path, "%s/map_ao_gen.c", dir);
    fc = fopen(path, "wb");
    if (!fh || !fc) return 3;
    for (i = 0; i < rows; i++) kept += keep[i];
    fprintf(fh, "/* tools/map_ao.c: the new map's baked occlusion. Generated: do not edit. */\n#ifndef MAP_AO_GEN_H\n#define MAP_AO_GEN_H\n#include <stdint.h>\n#include \"map_gen.h\"\n");
    fprintf(fh, "extern const uint8_t *const map_model_ao[MAP_MODELS];   /* a byte a vertex (255 open) */\n");
    fprintf(fh, "#define MAP_AOG_COLS %uu\n#define MAP_AOG_ROWS %uu\n#define MAP_AOG_OPEN %.4ff\n", ROAD_COLS, kept, ROAD_KEEP / 255.0f);
    fprintf(fh, "extern const float map_aog[MAP_AOG_ROWS][MAP_AOG_COLS][3];   /* the road's kept rows: x, z, byte / 255 */\n");
    fprintf(fh, "extern const uint8_t map_aog_join[MAP_AOG_ROWS];             /* 1: this row joins the one before (a patch goes on) */\n#endif\n");
    fprintf(fc, "/* tools/map_ao.c: the new map's baked occlusion. Generated: do not edit. */\n#include \"map_ao_gen.h\"\n");
    for (m = 0; m < MAP_MODELS; m++) {
        fprintf(fc, "static const uint8_t ao%u[%u] = {", m, (unsigned)map_models[m]->nv);
        for (i = 0; i < map_models[m]->nv; i++) fprintf(fc, "%s%s%u", i ? "," : "", i % 32u ? "" : "\n ", mao[m][i]);
        fprintf(fc, " };\n");
    }
    fprintf(fc, "const uint8_t *const map_model_ao[MAP_MODELS] = {");
    for (m = 0; m < MAP_MODELS; m++) fprintf(fc, "%s ao%u", m ? "," : "", m);
    fprintf(fc, " };\nconst float map_aog[MAP_AOG_ROWS][MAP_AOG_COLS][3] = {\n");
    {
        unsigned prev = 0xffffffffu, n = 0;
        uint8_t *join = calloc(kept ? kept : 1u, 1);
        for (i = 0; i < rows; i++) {
            if (!keep[i]) continue;
            join[n++] = prev != 0xffffffffu && prev + 1u == i;
            prev = i;
            fprintf(fc, " {");
            for (m = 0; m < ROAD_COLS; m++) {
                const map_pose_t q = map_at((float)i * ROAD_STEP, ROAD_LAT0 + ROAD_STEP * (float)m);
                fprintf(fc, "%s{%.3ff,%.3ff,%.4ff}", m ? "," : "", q.x, q.z, road[i * ROAD_COLS + m] / 255.0f);
            }
            fprintf(fc, "},\n");
        }
        fprintf(fc, "};\nconst uint8_t map_aog_join[MAP_AOG_ROWS] = {");
        for (i = 0; i < kept; i++) fprintf(fc, "%s%s%u", i ? "," : "", i % 64u ? "" : "\n ", join[i]);
        fprintf(fc, " };\n");
        lo = 255;
        for (i = 0; i < rows * ROAD_COLS; i++) if (road[i] < lo) lo = road[i];
        printf("road: %u rows of %u, %u kept (%u quads), lowest byte %u\n", rows, ROAD_COLS, kept, kept ? (kept - 1u) * (ROAD_COLS - 1u) : 0u, lo);
        free(join);
    }
    fclose(fc); fclose(fh);
    printf("MAP_AO_OK\n");
    return 0;
}
