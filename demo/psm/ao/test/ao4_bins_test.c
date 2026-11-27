/* SH4DER-PVR: host ESTIMATE of the tile bins ao4's passes fill (the object lists a tile, and their overflow blocks)

   demo/psm/ao/test/ao4_bins_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   ao4-d's console run counted 4,948 "tile bins out of memory" events (wiki PVR-143) and did not say in which pass.
   This puts every pass's polygons into 32-pixel tiles the way the program sends them and counts, a tile and a list,
   the entries its object list would hold and the overflow blocks that needs, against the room KallistiOS was given
   (ao4's pvr_init: 16-word bins for the opaque and the translucent list, opb_overflow_count 4).

   THE MODEL, all of it an ESTIMATE of what the TA does (read from documents, not measured here):
     - a strip is cut into pieces of up to six triangles; a piece is one entry in every tile that the bounding box of
       one of its triangles touches;
     - a tile's first block holds BIN - 1 entries and a link; so does every overflow block;
     - overflow blocks come from one pool a scene: overflow_count x tiles x (the two lists' bins) words;
     - a polygon with the user clip on is binned only in the clip's tiles.
   It answers "which pass needs more blocks than the pool has", not "how many events": the console's count a split
   comes from the program itself.

   Compile with -DSH4DER_CORE_NO_SH4ZAM, the library's include folders and the room's folder (build.py --room-only). */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../ao4_scene.h"
#include "../ao4_draw.h"
#include "../ao4_room.h"
#include "../ao4_hens.h"

#define TW 20
#define TH 15
#define BIN 16u
#define OVERFLOW 4u
#define POOL (OVERFLOW * (unsigned)(TW * TH) * 2u)      /* blocks of BIN words: both lists' bins, overflow_count times */

static unsigned bins[2][TW][TH];            /* entries: [list: 0 opaque, 1 translucent][x][y] */
static uint8_t piece_mark[TW][TH];
static int clip_x0, clip_x1, lim_w, lim_h;  /* the user clip's tile columns; the pass's size in pixels */
static unsigned cur_list;

static void piece_begin(void) { memset(piece_mark, 0, sizeof piece_mark); }
static void piece_tri(const float *a, const float *b, const float *c) {
    float x0 = a[0], x1 = a[0], y0 = a[1], y1 = a[1];
    int tx0, tx1, ty0, ty1, x, y;

    if (b[0] < x0) x0 = b[0];
    if (c[0] < x0) x0 = c[0];
    if (b[0] > x1) x1 = b[0];
    if (c[0] > x1) x1 = c[0];
    if (b[1] < y0) y0 = b[1];
    if (c[1] < y0) y0 = c[1];
    if (b[1] > y1) y1 = b[1];
    if (c[1] > y1) y1 = c[1];
    if (x1 < 0.0f || y1 < 0.0f || x0 >= (float)lim_w || y0 >= (float)lim_h) return;
    tx0 = x0 < 0.0f ? 0 : (int)x0 / 32; ty0 = y0 < 0.0f ? 0 : (int)y0 / 32;
    tx1 = x1 >= (float)lim_w ? (lim_w - 1) / 32 : (int)x1 / 32; ty1 = y1 >= (float)lim_h ? (lim_h - 1) / 32 : (int)y1 / 32;
    if (tx0 < clip_x0) tx0 = clip_x0;
    if (tx1 > clip_x1) tx1 = clip_x1;
    for (x = tx0; x <= tx1 && x < TW; x++) for (y = ty0; y <= ty1 && y < TH; y++) piece_mark[x][y] = 1;
}
static void piece_end(void) {
    int x, y;

    for (x = 0; x < TW; x++) for (y = 0; y < TH; y++) bins[cur_list][x][y] += piece_mark[x][y];
}

/* a strip of n screen points (x, y in p[i][0], p[i][1]): pieces of six triangles */
static void strip(const float (*p)[3], unsigned n) {
    unsigned t;

    for (t = 0; t + 2u < n; t++) {
        if (t % 6u == 0u) piece_begin();
        piece_tri(p[t], p[t + 1u], p[t + 2u]);
        if (t % 6u == 5u || t + 3u == n) piece_end();
    }
}

static void quad(float x0, float y0, float x1, float y1) {
    const float p[4][3] = { { x0, y1, 0 }, { x0, y0, 0 }, { x1, y1, 0 }, { x1, y0, 0 } };

    strip(p, 4u);
}

static void emit(void *user, const float a[2], const float b[2], const float c[2], float z, uint32_t argb, int blend) {
    (void)user; (void)z; (void)argb; (void)blend;
    piece_begin(); piece_tri(a, b, c); piece_end();
}

typedef struct need { unsigned most[2], blocks, tiles_over; } need_t;

static void pass_begin(int w, int h) { memset(bins, 0, sizeof bins); lim_w = w; lim_h = h; clip_x0 = 0; clip_x1 = TW - 1; }
static void pass_end(need_t *n) {
    unsigned l;
    int x, y;

    memset(n, 0, sizeof *n);
    for (l = 0; l < 2u; l++)
        for (x = 0; x < TW; x++)
            for (y = 0; y < TH; y++) {
                const unsigned e = bins[l][x][y];

                if (e > n->most[l]) n->most[l] = e;
                if (e > BIN - 1u) { n->blocks += (e - (BIN - 1u) + (BIN - 2u)) / (BIN - 1u); n->tiles_over++; }
            }
}

static ao4_scene_t scene;
static ao4_hens_t hens;
static float scr[AO4_V_MAX][3];

/* one half of the screen, as ao4_spike.c's half() sends it */
static void half(unsigned method, const ao4_cam_t *cam, int side) {
    float p[512][3];
    unsigned st = 0, i, k = 0, n;

    clip_x0 = side ? 10 : 0; clip_x1 = side ? 19 : 9;
    cur_list = 0;
    if (method >= AO4_LMAP) {
        static const uint8_t order[4] = { 0, 1, 3, 2 };

        for (; st < scene.surf_strips; st++) k += scene.slen[st];
        for (i = 0; i < ROOM_SURFS; i++) {
            float w[3], uv[2];
            unsigned c;

            for (c = 0; c < 4u; c++) { ao4_room_quad(i, order[c], w, uv); ao4_cam_pt(cam, w, p[c]); }
            strip((const float (*)[3])p, 4u);
        }
    }
    for (; st < scene.strips; st++) {
        n = scene.slen[st];
        for (i = 0; i < n && i < 512u; i++, k++) { p[i][0] = scr[scene.idx[k]][0]; p[i][1] = scr[scene.idx[k]][1]; }
        strip((const float (*)[3])p, n);
    }
    if (method == AO4_LMAP_M) {             /* the hens' quads, translucent */
        static const uint8_t order[4] = { 0, 1, 3, 2 };

        cur_list = 1;
        for (i = 0; i < AO4_HENS; i++) {
            float w[3], uv[2];
            unsigned c;

            for (c = 0; c < 4u; c++) { ao4_hen_quad(&hens, i, order[c], w, uv); ao4_cam_pt(cam, w, p[c]); }
            strip((const float (*)[3])p, 4u);
        }
    }
}

static void report(const char *what, const need_t *n) {
    printf("    %-34s most entries a tile: opaque %4u, translucent %4u; tiles over a block %3u; overflow blocks %5u of %u%s\n", what, n->most[0], n->most[1],
           n->tiles_over, n->blocks, POOL, n->blocks > POOL ? "   OVER THE POOL" : "");
}

static void run(const char *name, unsigned ml, unsigned mr, unsigned k) {
    need_t worst, n;
    float t;
    unsigned i;

    memset(&worst, 0, sizeof worst);
    for (t = 0.0f; t < 18.0f; t += 1.5f) {                 /* the swing's whole way, or most of an orbit */
        ao4_cam_t cam;

        ao4_cam_at(&scene, &cam, t, 0.0f, 0.0f);
        for (i = 0; i < scene.vn; i++) ao4_cam_pt(&cam, scene.p[i], scr[i]);
        pass_begin(640, 480);
        half(ml, &cam, 0); half(mr, &cam, 1);
        clip_x0 = 0; clip_x1 = TW - 1; cur_list = 1;
        quad(319.0f, 0.0f, 321.0f, 480.0f);
        for (i = 0; i < 2u; i++) { quad(66.0f, 51.0f, 326.0f, 69.0f); quad(326.0f, 51.0f, 586.0f, 69.0f); quad(66.0f, 371.0f, 326.0f, 389.0f); quad(326.0f, 371.0f, 586.0f, 389.0f);
                                   quad(62.0f, 391.0f, 578.0f, 409.0f); quad(62.0f, 411.0f, 578.0f, 429.0f); }
        pass_end(&n);
        if (n.blocks >= worst.blocks) worst = n;
    }
    report(name, &worst);
    (void)k;
}

static void targets(unsigned k, int with_hens_m) {
    need_t n;
    const float s = (float)AO4_SIZE;

    pass_begin((int)AO4_SIZE, (int)AO4_SIZE + 2);
    cur_list = 0; quad(0.0f, 0.0f, s, s + 2.0f);
    ao4_draw_sun(&scene, AO4_CLAY, emit, NULL);
    quad(0.0f, 0.0f, s, 1.0f); quad(0.0f, s - 1.0f, s, s + 2.0f); quad(0.0f, 0.0f, 1.0f, s); quad(s - 1.0f, 0.0f, s, s);
    pass_end(&n); report("target S (the sun alone)", &n);
    pass_begin((int)AO4_SIZE, (int)AO4_SIZE + 2);
    cur_list = 0; quad(0.0f, 0.0f, s, s + 2.0f);
    ao4_draw_sun(&scene, AO4_CLAY, emit, NULL);
    quad(0.0f, 0.0f, s, 1.0f); quad(0.0f, s - 1.0f, s, s + 2.0f); quad(0.0f, 0.0f, 1.0f, s); quad(s - 1.0f, 0.0f, s, s);
    cur_list = 1; ao4_draw_sky(&scene, k, 0, ao4_reach[AO4_REACH_DEFAULT], emit, NULL);
    pass_end(&n);
    { char w[48]; snprintf(w, sizeof w, "target A (sun + K %u directions)", k); report(w, &n); }
    if (with_hens_m) {
        pass_begin((int)AO4_HEN_ATLAS, (int)AO4_HEN_ATLAS + 2);
        cur_list = 0; quad(0.0f, 0.0f, (float)AO4_HEN_ATLAS, (float)AO4_HEN_ATLAS + 2.0f);
        cur_list = 1; ao4_hens_tiles(&scene, &hens, k, 0, ao4_reach[AO4_REACH_DEFAULT], emit, NULL);
        pass_end(&n); report("target M (a tile a hen)", &n);
    }
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    unsigned which;

    printf("ao4 tile bins, ESTIMATE (a model of the TA's binning; not a measurement). Bins of %u words, pool %u overflow blocks a scene.\n", BIN, POOL);
    for (which = 0; which < AO4_SCENES; which++) {
        ao4_build(&scene, which);
        printf("  scene %c: %u triangles a half, %u strips\n", 'A' + which, scene.tris, scene.strips);
        run("screen, OFF | 1", AO4_OFF, AO4_BAKED, 4u);
        run("screen, 1 | 2", AO4_BAKED, AO4_MAP, 4u);
        targets(4u, 0); targets(8u, 0);
    }
    ao4_build_room(&scene);
    ao4_hens_add(&scene, &hens);
    printf("  scene C with the hens: %u triangles a half, %u strips\n", scene.tris, scene.strips);
    run("screen, OFF | 1", AO4_OFF, AO4_BAKED, 4u);
    run("screen, 1 | 3", AO4_BAKED, AO4_LMAP, 4u);
    run("screen, 3 | 2", AO4_LMAP, AO4_MAP, 4u);
    run("screen, 3+M | 3+M", AO4_LMAP_M, AO4_LMAP_M, 4u);
    targets(4u, 1); targets(8u, 0);
    return 0;
}
