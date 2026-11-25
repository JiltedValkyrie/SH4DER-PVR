/* SH4DER-PVR: the room's AO, baked on the host: a byte a vertex and a lightmap a flat surface (demo/psm/ao)

   demo/psm/ao/tools/room_bake.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   A host program. It includes the room_mesh.h that room_import.py wrote and writes room_ao.h beside it:

     room_ao_v[ROOM_V]     the AO at each vertex on screen
     room_lm_<s>[w * h]    the AO at each texel of surface s, and room_lm[s] pointing at them

   A point casts ROOM_AO_RAYS cosine-weighted rays over its hemisphere (the fixed Hammersley set, the reach and the
   weight 1 - t / reach of ../../tools/ao_bake.c, the town's bake) against the room's occluder triangles, either side
   of a triangle. 255 open sky .. 0 closed. A ray starts ROOM_AO_PUSH off its surface.

   A POINT INSIDE A SOLID. The furniture sinks a centimetre into the floor and the trim into the walls, so some points
   of a flat surface start inside a closed object: every ray meets a back face at once and the point bakes black. Such
   a point is never seen, but its black would bleed into the texels and vertices beside it. So a surface's point whose
   rays meet the BACK of a closed object first (more than ROOM_AO_INSIDE of them) is not baked: it takes the mean of
   the baked points next to it, pass after pass, until none is left.
   An object's own vertex inside a solid (a picture's corner under its frame) is baked a little way into each of its
   triangles instead, where that is outside.

   Build and run:  gcc -O2 -std=c99 -Wall -Wextra -I<dir with room_mesh.h> demo/psm/ao/tools/room_bake.c -o room_bake -lm
                   room_bake <dir>
   --check prints the numbers and writes nothing. */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define ROOM_NO_AO
#include "../room_data.h"

#define ROOM_AO_RAYS 64
#define ROOM_AO_R    2.5f
#define ROOM_AO_PUSH 0.02f
#define ROOM_AO_INSIDE 0.25f

static float dirs[ROOM_AO_RAYS][3];
static unsigned near_t[ROOM_OT];
static float tc[ROOM_OT][3], tr[ROOM_OT];      /* each occluder's middle and reach, to leave the far ones out */

static void cross3(const float *a, const float *b, float *o) { o[0] = a[1] * b[2] - a[2] * b[1]; o[1] = a[2] * b[0] - a[0] * b[2]; o[2] = a[0] * b[1] - a[1] * b[0]; }
static float dot3(const float *a, const float *b) { return a[0] * b[0] + a[1] * b[1] + a[2] * b[2]; }
static void norm3(float *a) { const float l = sqrtf(dot3(a, a)); if (l > 1e-12f) { a[0] /= l; a[1] /= l; a[2] /= l; } }

/* the distance along o + t d to triangle k, or a large number; *back: the ray met the triangle's back */
static float ray_tri(const float o[3], const float d[3], unsigned k, int *back) {
    const float *a = room_ot[k][0], *b = room_ot[k][1], *c = room_ot[k][2];
    const float e1[3] = { b[0] - a[0], b[1] - a[1], b[2] - a[2] }, e2[3] = { c[0] - a[0], c[1] - a[1], c[2] - a[2] };
    const float s[3] = { o[0] - a[0], o[1] - a[1], o[2] - a[2] };
    float h[3], q[3], det, u, v, t;

    cross3(d, e2, h);
    det = dot3(e1, h);
    if (det > -1e-9f && det < 1e-9f) return 1e30f;
    *back = det < 0.0f;
    det = 1.0f / det;
    u = dot3(s, h) * det;
    if (u < 0.0f || u > 1.0f) return 1e30f;
    cross3(s, e1, q);
    v = dot3(d, q) * det;
    if (v < 0.0f || u + v > 1.0f) return 1e30f;
    t = dot3(e2, q) * det;
    return t > 1e-4f ? t : 1e30f;
}

/* the AO at p; *inside: p is inside a closed object */
static float ao_at(const float p[3], const float n[3], int *inside) {
    const float ax[3] = { fabsf(n[1]) > 0.9f ? 1.0f : 0.0f, fabsf(n[1]) > 0.9f ? 0.0f : 1.0f, 0.0f };
    const float o[3] = { p[0] + n[0] * ROOM_AO_PUSH, p[1] + n[1] * ROOM_AO_PUSH, p[2] + n[2] * ROOM_AO_PUSH };
    float t[3], b[3], occl = 0.0f;
    unsigned k, i, nn = 0, in = 0;

    for (i = 0; i < ROOM_OT; i++) {
        const float d[3] = { tc[i][0] - o[0], tc[i][1] - o[1], tc[i][2] - o[2] };

        if (dot3(d, d) < (ROOM_AO_R + tr[i]) * (ROOM_AO_R + tr[i])) near_t[nn++] = i;
    }
    cross3(n, ax, t); norm3(t);
    cross3(n, t, b);
    for (k = 0; k < ROOM_AO_RAYS; k++) {
        const float d[3] = { t[0] * dirs[k][0] + n[0] * dirs[k][1] + b[0] * dirs[k][2], t[1] * dirs[k][0] + n[1] * dirs[k][1] + b[1] * dirs[k][2],
                             t[2] * dirs[k][0] + n[2] * dirs[k][1] + b[2] * dirs[k][2] };
        float best = ROOM_AO_R;
        int best_back = 0, back = 0;

        for (i = 0; i < nn; i++) {
            const float hit = ray_tri(o, d, near_t[i], &back);

            if (hit < best) { best = hit; best_back = back && room_ot_closed[near_t[i]]; }
        }
        if (best < ROOM_AO_R) occl += 1.0f - best / ROOM_AO_R;
        in += (unsigned)best_back;
    }
    *inside = (float)in > ROOM_AO_INSIDE * (float)ROOM_AO_RAYS;
    return 1.0f - occl / (float)ROOM_AO_RAYS;
}

/* A w x h grid of bytes with a flag a point: each flagged point takes the mean of its unflagged neighbours (8 round
   it), pass after pass. Returns how many were flagged. */
static unsigned fill(unsigned char *v, unsigned char *bad, unsigned w, unsigned h, unsigned stride) {
    unsigned n = 0, left, x, y;
    unsigned char *next = malloc((size_t)w * h);

    if (!next) return 0;
    for (y = 0; y < h; y++) for (x = 0; x < w; x++) n += bad[y * w + x];
    for (left = n; left; ) {
        unsigned done = 0;

        memcpy(next, bad, (size_t)w * h);
        for (y = 0; y < h; y++)
            for (x = 0; x < w; x++) {
                unsigned sum = 0, k = 0;
                int dx, dy;

                if (!bad[y * w + x]) continue;
                for (dy = -1; dy <= 1; dy++)
                    for (dx = -1; dx <= 1; dx++) {
                        const int xx = (int)x + dx, yy = (int)y + dy;

                        if (xx < 0 || yy < 0 || xx >= (int)w || yy >= (int)h || bad[(unsigned)yy * w + (unsigned)xx]) continue;
                        sum += v[((unsigned)yy * w + (unsigned)xx) * stride]; k++;
                    }
                if (k) { v[(y * w + x) * stride] = (unsigned char)((sum + k / 2u) / k); next[y * w + x] = 0; done++; }
            }
        memcpy(bad, next, (size_t)w * h);
        if (!done) break;
        left -= done;
    }
    free(next);
    return n;
}

static unsigned char to_byte(float a) { return (unsigned char)((a < 0.0f ? 0.0f : a > 1.0f ? 1.0f : a) * 255.0f + 0.5f); }

static void put_bytes(FILE *f, const unsigned char *b, unsigned n) {
    unsigned i;

    for (i = 0; i < n; i++) fprintf(f, "%s%u,", i % 32u == 0u ? "\n    " : " ", b[i]);
}

int main(int argc, char **argv) {
    static unsigned char av[ROOM_V], abad[ROOM_V];
    int inside;
    unsigned char *lm[ROOM_SURFS];
    const int check = argc > 1 && !strcmp(argv[1], "--check");
    unsigned i, k, s;
    double sum = 0.0;
    char path[1024];
    FILE *f;

    if (argc < 2) { fprintf(stderr, "usage: room_bake <dir of room_mesh.h> | --check\n"); return 2; }
    for (k = 0; k < ROOM_AO_RAYS; k++) {                 /* cosine-weighted, Hammersley: u1 even steps, u2 the base-2 radical inverse */
        const float u1 = ((float)k + 0.5f) / (float)ROOM_AO_RAYS;
        float u2 = 0.0f, fr = 0.5f, rr, ph;
        unsigned b = k;

        while (b) { if (b & 1u) u2 += fr; fr *= 0.5f; b >>= 1; }
        rr = sqrtf(u1); ph = 6.2831853f * u2;
        dirs[k][0] = rr * cosf(ph); dirs[k][1] = sqrtf(1.0f - u1); dirs[k][2] = rr * sinf(ph);
    }
    for (i = 0; i < ROOM_OT; i++) {
        float r2 = 0.0f;

        for (k = 0; k < 3u; k++) tc[i][k] = (room_ot[i][0][k] + room_ot[i][1][k] + room_ot[i][2][k]) * (1.0f / 3.0f);
        for (k = 0; k < 3u; k++) {
            const float d[3] = { room_ot[i][k][0] - tc[i][0], room_ot[i][k][1] - tc[i][1], room_ot[i][k][2] - tc[i][2] };

            if (dot3(d, d) > r2) r2 = dot3(d, d);
        }
        tr[i] = sqrtf(r2);
    }
    for (i = 0; i < ROOM_V; i++) { av[i] = to_byte(ao_at(room_v[i], room_n[i], &inside)); abad[i] = (unsigned char)inside; }
    for (k = 0; k < ROOM_OBJS; k++) {                    /* an object's vertex inside another of its shells (a panel's corner under its frame): */
        unsigned moved = 0;                              /* the mean of the points 0.3 of the way to the middles of its triangles that are outside */

        if (room_obj[k].surf >= 0) continue;
        for (i = room_obj[k].v0; i < (unsigned)room_obj[k].v0 + room_obj[k].vn; i++) {
            float acc = 0.0f;
            unsigned t, n = 0;

            if (!abad[i]) continue;
            for (t = room_obj[k].t0; t < (unsigned)room_obj[k].t0 + room_obj[k].tn; t++) {
                const uint16_t *tri = room_t[t];
                float q[3], a;
                unsigned c;

                if (tri[0] != i && tri[1] != i && tri[2] != i) continue;
                for (c = 0; c < 3u; c++) q[c] = room_v[i][c] + 0.3f * ((room_v[tri[0]][c] + room_v[tri[1]][c] + room_v[tri[2]][c]) * (1.0f / 3.0f) - room_v[i][c]);
                a = ao_at(q, room_n[i], &inside);
                if (!inside) { acc += a; n++; }
            }
            if (n) { av[i] = to_byte(acc / (float)n); abad[i] = 0; moved++; }
        }
        if (moved) printf("  %s: %u vertices inside a solid, baked a little way into their triangles\n", room_obj[k].name, moved);
    }
    for (s = 0; s < ROOM_SURFS; s++) {                   /* a surface's grid of vertices */
        const unsigned n = fill(&av[room_surf[s].v0], &abad[room_surf[s].v0], room_surf[s].nu + 1u, room_surf[s].nv + 1u, 1u);

        printf("  surface %u (%s): %u of %u vertices inside a solid, filled from beside\n", s, room_obj[room_surf[s].obj].name, n,
               (room_surf[s].nu + 1u) * (room_surf[s].nv + 1u));
    }
    for (i = 0; i < ROOM_V; i++) sum += av[i];
    printf("room_bake: %u vertices, mean AO byte %.0f; %u rays a point, reach %.1f m, %u occluder triangles\n", (unsigned)ROOM_V, sum / ROOM_V, ROOM_AO_RAYS,
           (double)ROOM_AO_R, (unsigned)ROOM_OT);
    for (s = 0; s < ROOM_SURFS; s++) {
        const room_surf_t *sf = &room_surf[s];
        unsigned x, y, lo = 255, hi = 0, nbad;
        unsigned char *bad = malloc((size_t)sf->w * sf->h);

        lm[s] = malloc((size_t)sf->w * sf->h);
        if (!lm[s] || !bad) return 1;
        sum = 0.0;
        for (y = 0; y < sf->h; y++)
            for (x = 0; x < sf->w; x++) {
                const float a = ((float)x + 0.5f) / (float)sf->w, b = ((float)y + 0.5f) / (float)sf->h;
                const float p[3] = { sf->o[0] + sf->du[0] * a + sf->dv[0] * b, sf->o[1] + sf->du[1] * a + sf->dv[1] * b, sf->o[2] + sf->du[2] * a + sf->dv[2] * b };
                lm[s][y * sf->w + x] = to_byte(ao_at(p, sf->n, &inside));
                bad[y * sf->w + x] = (unsigned char)inside;
            }
        nbad = fill(lm[s], bad, sf->w, sf->h, 1u);
        free(bad);
        for (i = 0; i < (unsigned)sf->w * sf->h; i++) {
            const unsigned char v = lm[s][i];

            sum += v;
            if (v < lo) lo = v;
            if (v > hi) hi = v;
        }
        printf("  surface %u (%s): lightmap %u x %u, AO byte min %u mean %.0f max %u; %u texels inside a solid, filled\n", s, room_obj[sf->obj].name, sf->w, sf->h, lo,
               sum / (sf->w * sf->h), hi, nbad);
    }
    if (check) return 0;
    snprintf(path, sizeof path, "%s/room_ao.h", argv[1]);
    f = fopen(path, "w");
    if (!f) { fprintf(stderr, "cannot write %s\n", path); return 1; }
    fprintf(f, "/* SH4DER-PVR: the room's baked AO. WRITTEN BY demo/psm/ao/tools/room_bake.c; do not edit, do not commit.\n"
               "   %u rays a point, reach %.1f m. Included by demo/psm/ao/room_data.h. */\n", ROOM_AO_RAYS, (double)ROOM_AO_R);
    fprintf(f, "static const uint8_t room_ao_v[ROOM_V] = {");
    put_bytes(f, av, ROOM_V);
    fprintf(f, "\n};\n");
    for (s = 0; s < ROOM_SURFS; s++) {
        fprintf(f, "static const uint8_t room_lm_%u[%u] = {", s, (unsigned)room_surf[s].w * room_surf[s].h);
        put_bytes(f, lm[s], (unsigned)room_surf[s].w * room_surf[s].h);
        fprintf(f, "\n};\n");
    }
    fprintf(f, "static const uint8_t *const room_lm[ROOM_SURFS] = {");
    for (s = 0; s < ROOM_SURFS; s++) fprintf(f, " room_lm_%u,", s);
    fprintf(f, " };\n");
    fclose(f);
    printf("  wrote %s\n", path);
    return 0;
}
