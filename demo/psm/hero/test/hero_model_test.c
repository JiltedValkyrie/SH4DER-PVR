/* SH4DER-PVR: hero1's model, clips and file, host test.  Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   cc -O2 -std=gnu11 -Wall -Wextra -DSH4DER_CORE_NO_SH4ZAM -Iinclude -Ish4dow-dc/include -Idemo/psm/hero \
      demo/psm/hero/test/hero_model_test.c -lm -o hero_model_test && ./hero_model_test [out.bin]

   Checks the built hero (counts in budget, indices, weights, unit normals, the clips loop), that the bind pose skins to
   the rest positions, the blend's two-matrix path against a direct sum, and that hero_file.h writes and reads the same
   hero. With an argument it leaves the file there for tools/hero_check.py. */

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include "hero_model.h"
#include "hero_file.h"
#include "hero_view.h"
#ifdef HERO_TEST_ROOM
#include "../ao/room_data.h"
#endif

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static hero_t h, g;
static float pose[HERO_BONES][12], pal[HERO_BONES][16], scr[HERO_V_MAX][3], ref[HERO_V_MAX][3];

int main(int argc, char **argv) {
    unsigned i, s, k, c, b, blends = 0, sum = 0;
    float id[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 }, worst = 0.0f;
    const char *path = argc > 1 ? argv[1] : "hero_model_test.bin";
    FILE *f;

    CHECK(hero_build(&h), "a table is too small");
    printf("hero: %u vertices, %u triangles (%u in the visor), %u strips, %u TA vertices a pass, %u bones\n", h.vn, h.tris,
           0u, h.strips, h.idx_n, HERO_BONES);
    CHECK(h.vn >= 900u && h.vn <= 1600u, "vertices %u out of the budget 900 .. 1600", h.vn);
    CHECK(h.tris >= 1100u && h.tris <= 2200u, "triangles %u out of the budget 1100 .. 2200", h.tris);
    for (s = 0; s < h.strips; s++) sum += h.slen[s];
    CHECK(sum == h.idx_n, "strip lengths %u, indices %u", sum, h.idx_n);
    CHECK(h.visor_strip0 < h.strips, "no visor strips");
    for (i = 0; i < h.idx_n; i++) CHECK(h.idx[i] < h.vn, "index %u = %u", i, h.idx[i]);
    for (i = 0; i < h.vn; i++) {
        const float l = h.n[i][0] * h.n[i][0] + h.n[i][1] * h.n[i][1] + h.n[i][2] * h.n[i][2];

        CHECK(fabsf(l - 1.0f) < 1e-3f, "normal %u length^2 %f", i, l);
        CHECK(h.b0[i] < HERO_BONES && h.b1[i] < HERO_BONES, "bones of %u", i);
        if (h.b1[i] != h.b0[i]) { blends++; CHECK(h.w0[i] > 0u && h.w0[i] < 255u, "blend weight %u", h.w0[i]); }
        if (i) CHECK(h.b0[i] > h.b0[i - 1u] || (h.b0[i] == h.b0[i - 1u] && h.b1[i] >= h.b1[i - 1u]), "not sorted at %u", i);
    }
    CHECK(blends > 0u, "no blended vertices");
    for (c = 0; c < HERO_CLIPS; c++) {                /* the loop closes: the step last -> first is like the others */
        const unsigned n = h.frames[c];
        float step = 0.0f, wrap = 0.0f;

        for (b = 0; b < HERO_BONES; b++)
            for (k = 0; k < 12u; k++) {
                const float d1 = fabsf(h.clip[c][1][b][k] - h.clip[c][0][b][k]), d2 = fabsf(h.clip[c][0][b][k] - h.clip[c][n - 1u][b][k]);

                if (d1 > step) step = d1;
                if (d2 > wrap) wrap = d2;
            }
        printf("clip %u: %u frames, largest step %.4f, wrap step %.4f\n", c, n, step, wrap);
        CHECK(wrap < step * 1.5f + 1e-4f, "clip %u jumps at the loop", c);
    }

    /* the bind pose: identity skins put every vertex at its rest place */
    for (b = 0; b < HERO_BONES; b++) { memset(pose[b], 0, sizeof pose[b]); pose[b][0] = pose[b][5] = pose[b][10] = 1.0f; }
    id[14] = 0.0f;
    {
        float vp[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 1, 0, 0, 0, 1 };   /* X = x, Y = y, Z = 0, W = z + 1 */

        hero_palette(vp, (const float (*)[12])pose, pal);
        hero_skin(&h, (const float (*)[16])pal, 1, 1e-3f, scr);
        for (i = 0; i < h.vn; i++) {
            const float w = h.p[i][2] + 1.0f, ex = h.p[i][0] / w, ey = h.p[i][1] / w;

            CHECK(fabsf(scr[i][0] - ex) < 1e-4f && fabsf(scr[i][1] - ey) < 1e-4f && fabsf(scr[i][2] - 1.0f / w) < 1e-4f, "bind vertex %u", i);
        }
    }
    (void)id;

    /* a running frame: hero_skin's grouped two-matrix path against a direct blend a vertex */
    {
        float vp[16] = { 300, 0, 0, 0, 0, -300, 0, 0, 320, 240, 0, 1, 0, 0, 1, 4 };   /* z pushed back 4 m */

        hero_pose(&h, HERO_RUN_AIM, 0.37f, pose);
        hero_palette(vp, (const float (*)[12])pose, pal);
        hero_skin(&h, (const float (*)[16])pal, 1, 1e-3f, scr);
        for (i = 0; i < h.vn; i++) {
            float a[4], d[4], w = (float)h.w0[i] / 255.0f, o[4];

            hero_xf_(pal[h.b0[i]], h.p[i], a);
            hero_xf_(pal[h.b1[i]], h.p[i], d);
            for (k = 0; k < 4u; k++) o[k] = h.b1[i] == h.b0[i] ? a[k] : a[k] * w + d[k] * (1.0f - w);
            ref[i][0] = o[0] / o[3]; ref[i][1] = o[1] / o[3]; ref[i][2] = 1.0f / o[3];
            for (k = 0; k < 3u; k++) { const float e = fabsf(ref[i][k] - scr[i][k]); if (e > worst) worst = e; }
        }
        printf("skin against a direct blend: worst %.6f px\n", worst);
        CHECK(worst < 1e-3f, "skin differs by %f", worst);
    }

    /* the file: write, read back, the same */
    f = fopen(path, "wb");
    CHECK(f != NULL, "cannot write %s", path);
    if (f) { CHECK(hero_file_write(f, &h) == 0, "write"); fclose(f); }
    f = fopen(path, "rb");
    CHECK(f != NULL, "cannot read %s", path);
    if (f) {
        const int r = hero_file_read(f, &g);

        fclose(f);
        CHECK(r == 0, "read: %s", hero_file_error(r));
        CHECK(g.vn == h.vn && g.idx_n == h.idx_n && g.strips == h.strips && g.visor_strip0 == h.visor_strip0 && g.tris == h.tris, "counts");
        CHECK(!memcmp(g.p, h.p, h.vn * sizeof h.p[0]) && !memcmp(g.idx, h.idx, h.idx_n * sizeof h.idx[0]), "mesh");
        CHECK(!memcmp(g.clip[1], h.clip[1], h.frames[1] * sizeof h.clip[1][0]), "clip");
        CHECK(!memcmp(g.tex[0], h.tex[0], 128u * 128u * 2u), "texture");
        CHECK(g.from_file == 1, "from_file");
    }
#ifdef HERO_TEST_ROOM
    {   /* the room's cull: from the camera's start, floor and back walls face it (hero_area2 < 0), and most are kept */
        hero_cam_t cam;
        const float tgt[3] = { 0.0f, 1.0f, 0.0f };
        unsigned t, kept = 0, floor_kept = 0, floor_n = 0, o;

        hero_cam_make(&cam, 0.6f, 0.28f, 3.5f, tgt);
        for (o = 0; o < ROOM_OBJS; o++) {
            const int fl = room_obj[o].surf >= 0 && room_surf[room_obj[o].surf].kind == ROOM_FLOOR;

            for (t = room_obj[o].t0; t < (unsigned)room_obj[o].t0 + room_obj[o].tn; t++) {
                float a[3], b[3], d[3];

                if (!hero_view_pt(cam.vp, room_v[room_t[t][0]], a) || !hero_view_pt(cam.vp, room_v[room_t[t][1]], b) || !hero_view_pt(cam.vp, room_v[room_t[t][2]], d)) continue;
                if (hero_area2(a, b, d) < 0.0f) { kept++; floor_kept += fl; }
                floor_n += fl;
            }
        }
        printf("room cull from the start camera: %u of %u triangles kept, floor %u of %u\n", kept, ROOM_T, floor_kept, floor_n);
        CHECK(floor_n > 0u && floor_kept * 10u > floor_n * 9u, "the floor is culled: the area sign is the wrong way");
    }
#endif
    printf("hero_model_test: %s (%d failed)\n", fails ? "FAILED" : "all passed", fails);
    return fails ? 1 : 0;
}
