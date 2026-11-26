/* SH4DER-PVR: host test of include/sh4der_aomesh.h (baked occlusion over a triangle mesh, a cell grid and a ground grid)

   test/aomesh_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Checks: an open plane gives 255; a point 0.1 m from a wall is darker than a point 2.1 m and one 1.8 m from it (both 255:
   the ray misses, past the 1.6 m reach); a box's inside corner is darker than an edge point, and the edge darker than the
   centre (255); the tick count (every 256 vertices, every 8 ground rows); the ground grid with a rectangle, and none with
   NULL (255 outside the grid, always); an empty list, a zero normal (255, and no ray cast: the ray count is exact), a
   stride under 3 (in floats), a normal stride under 3, and each buffer one entry too small: refused, with every output (vertex bytes,
   grid, stats, scratch) left as it was.

   Runs on the plain C path (-DSH4DER_CORE_NO_SH4ZAM) and on the SH4ZAM software path (make test-shz).
   Build: cc -O2 -std=c99 -Wall -Wextra -DSH4DER_CORE_NO_SH4ZAM -Iinclude test/aomesh_test.c -lm */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/sh4der_aomesh.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

#define MAXV 700u
#define MAXT 64u

/* A mesh with its bake points: every vertex is baked (its normal in nrm); the triangles are the occluders. */
typedef struct mesh {
    float pos[MAXV][4];
    float nrm[MAXV][3];
    uint16_t idx[MAXT][3];
    unsigned nv, nt;
} mesh_t;

static unsigned add_v(mesh_t *m, float x, float y, float z, float nx, float ny, float nz) {
    const unsigned i = m->nv++;

    m->pos[i][0] = x; m->pos[i][1] = y; m->pos[i][2] = z; m->pos[i][3] = 0.0f;
    m->nrm[i][0] = nx; m->nrm[i][1] = ny; m->nrm[i][2] = nz;
    return i;
}
static void add_quad(mesh_t *m, unsigned a, unsigned b, unsigned c, unsigned d) {
    m->idx[m->nt][0] = (uint16_t)a; m->idx[m->nt][1] = (uint16_t)b; m->idx[m->nt][2] = (uint16_t)c; m->nt++;
    m->idx[m->nt][0] = (uint16_t)a; m->idx[m->nt][1] = (uint16_t)c; m->idx[m->nt][2] = (uint16_t)d; m->nt++;
}

/* A floor y = 0 of +-6 m (vertices 0..3) and a wall at x = 0, z -6 .. 6, y 0 .. 3 (vertices 4..7). */
static void make_wall(mesh_t *m) {
    unsigned a = add_v(m, -6, 0, -6, 0, 1, 0), b = add_v(m, 6, 0, -6, 0, 1, 0), c = add_v(m, 6, 0, 6, 0, 1, 0), d = add_v(m, -6, 0, 6, 0, 1, 0);
    unsigned e = add_v(m, 0, 0, -6, 1, 0, 0), f = add_v(m, 0, 0, 6, 1, 0, 0), g = add_v(m, 0, 3, 6, 1, 0, 0), h = add_v(m, 0, 3, -6, 1, 0, 0);

    add_quad(m, a, b, c, d);
    add_quad(m, e, f, g, h);
}

/* The bake through the header's API, with the caller's buffers. The grid's bytes are kept until the next call. */
static uint8_t *kept;
static int bake(const mesh_t *m, const sh4der_aomesh_rect_t *ground, uint8_t *vert, sh4der_aomesh_grid_t *grid,
                sh4der_aomesh_stats_t *st, void (*tick)(void)) {
    sh4der_aomesh_tris_t t;
    sh4der_aomesh_need_t need, cap;
    sh4der_aomesh_work_t w;
    int rc;

    t.pos = &m->pos[0][0]; t.stride = 4u; t.idx = m->idx; t.ntri = m->nt;
    if (!sh4der_aomesh_need(&t, ground, &need)) return 0;
    cap = need;
    w.box = (float (*)[6])malloc((size_t)need.box * sizeof w.box[0]);
    w.cell = (uint32_t *)malloc((size_t)need.cell * sizeof w.cell[0]);
    w.list = (uint16_t *)malloc((size_t)need.list * sizeof w.list[0]);
    w.grid = need.grid ? (uint8_t *)malloc(need.grid) : NULL;
    rc = sh4der_aomesh_bake(&t, &m->nrm[0][0], 3u, m->nv, ground, &cap, &w, vert, grid, st, tick);
    free(w.box); free(w.cell); free(w.list);
    free(kept); kept = NULL;
    if (rc && grid->g) kept = grid->g;
    else free(w.grid);
    return rc;
}

static unsigned ticks;
static void count_tick(void) { ticks++; }

int main(void) {
    static mesh_t m, bx, t2, e;
    static uint8_t vert[MAXV], vert_again[MAXV];
    sh4der_aomesh_grid_t grid;
    sh4der_aomesh_stats_t st;
    const sh4der_aomesh_rect_t rect = { -3.0f, -3.0f, 3.0f, 3.0f }, big = { -6.0f, -6.0f, 6.0f, 6.0f };
    unsigned i;

    /* the constants: a ray touches 4 cells at most; 12 rays */
    CHECK(SH4DER_AOMESH_CELL >= SH4DER_AOMESH_REACH && SH4DER_AOMESH_RAYS == 12 && SH4DER_AOMESH_GMAX == 65536);

    /* an open plane: every vertex 255, and every vertex casts its rays */
    {
        unsigned a;

        a = add_v(&e, -6, 0, -6, 0, 1, 0); add_v(&e, 6, 0, -6, 0, 1, 0); add_v(&e, 6, 0, 6, 0, 1, 0); add_v(&e, -6, 0, 6, 0, 1, 0);
        add_v(&e, 0, 0, 0, 0, 1, 0);
        add_quad(&e, a, a + 1u, a + 2u, a + 3u);
        memset(vert, 7, sizeof vert);
        CHECK(bake(&e, NULL, vert, &grid, &st, NULL) == 1);
        for (i = 0; i < e.nv; i++) CHECK(vert[i] == 255u);
        CHECK(st.rays == e.nv * SH4DER_AOMESH_RAYS);
    }

    /* a wall: the point 0.1 m from it is darker; the points 2.1 m and 1.8 m from it (past the reach) are open */
    make_wall(&m);
    add_v(&m, 0.1f, 0.0f, 0.0f, 0, 1, 0);       /* 8: near */
    add_v(&m, -2.1f, 0.0f, 0.0f, 0, 1, 0);      /* 9: far */
    add_v(&m, -1.8f, 0.0f, 0.0f, 0, 1, 0);      /* 10: past the reach too */
    memset(vert, 7, sizeof vert);
    CHECK(bake(&m, NULL, vert, &grid, &st, NULL) == 1);
    CHECK(vert[8] < 250u);
    CHECK(vert[9] == 255u && vert[10] == 255u);
    CHECK(vert[8] < vert[9]);
    CHECK(grid.g == NULL && grid.gw == 0u && grid.gh == 0u);
    CHECK(sh4der_aomesh_ground(&grid, 0.0f, 0.0f) == 255u && sh4der_aomesh_ground(&grid, 1e9f, 0.0f) == 255u);
    CHECK(sh4der_aomesh_ground(&grid, (float)NAN, 0.0f) == 255u);
    CHECK(st.rays == m.nv * SH4DER_AOMESH_RAYS && st.tests > 0u && st.recips > 0u && st.recips <= st.tests);
    memcpy(vert_again, vert, m.nv);

    /* the edge tests on the numerators: a ray straight through the edge two triangles share is taken (by one or both), on
       either winding (det of both signs); a ray just outside both is refused, and no reciprocal is taken for it */
    {
        const float o[3] = { 0.3f, 0.0f, 0.3f }, up[3] = { 0.0f, 1.0f, 0.0f }, off[3] = { 1.5f, 0.0f, 0.3f };
        const float p0[3] = { -1, 1, -1 }, p1[3] = { 1, 1, -1 }, p2[3] = { 1, 1, 1 }, p3[3] = { -1, 1, 1 };
        uint32_t nr = 0u;
        float t0 = sh4der_aomesh_tri_(o, up, p0, p1, p2, 2.0f, &nr), t1 = sh4der_aomesh_tri_(o, up, p0, p2, p3, 2.0f, &nr);
        float t2b = sh4der_aomesh_tri_(o, up, p0, p2, p1, 2.0f, &nr), t3 = sh4der_aomesh_tri_(o, up, p0, p3, p2, 2.0f, &nr);

        CHECK((t0 > 0.999f && t0 < 1.001f) || (t1 > 0.999f && t1 < 1.001f));
        CHECK((t2b > 0.999f && t2b < 1.001f) || (t3 > 0.999f && t3 < 1.001f));
        nr = 0u;
        CHECK(sh4der_aomesh_tri_(off, up, p0, p1, p2, 2.0f, &nr) < 0.0f && sh4der_aomesh_tri_(off, up, p0, p2, p1, 2.0f, &nr) < 0.0f && nr == 0u);
        CHECK(sh4der_aomesh_tri_(o, up, p0, p1, p2, 0.5f, &nr) < 0.0f && nr == 1u);   /* past the reach: one reciprocal, no hit */
    }
    CHECK(bake(&m, NULL, vert, &grid, &st, NULL) == 1 && memcmp(vert, vert_again, m.nv) == 0);

    /* a box with no top, 6 m by 6 m by 3 m: the inside corner darkest, the edge darker than the centre (255) */
    {
        unsigned f0 = add_v(&bx, -3, 0, -3, 0, 1, 0), f1 = add_v(&bx, 3, 0, -3, 0, 1, 0), f2 = add_v(&bx, 3, 0, 3, 0, 1, 0), f3 = add_v(&bx, -3, 0, 3, 0, 1, 0);
        unsigned w0 = add_v(&bx, -3, 0, -3, 1, 0, 0), w1 = add_v(&bx, -3, 0, 3, 1, 0, 0), w2 = add_v(&bx, -3, 3, 3, 1, 0, 0), w3 = add_v(&bx, -3, 3, -3, 1, 0, 0);
        unsigned z0 = add_v(&bx, -3, 0, -3, 0, 0, 1), z1 = add_v(&bx, 3, 0, -3, 0, 0, 1), z2 = add_v(&bx, 3, 3, -3, 0, 0, 1), z3 = add_v(&bx, -3, 3, -3, 0, 0, 1);

        add_quad(&bx, f0, f1, f2, f3);
        add_quad(&bx, w0, w1, w2, w3);
        add_quad(&bx, z0, z1, z2, z3);
        add_v(&bx, -2.9f, 0.0f, -2.9f, 0, 1, 0);   /* 12: corner, 0.1 m from two walls */
        add_v(&bx, 0.0f, 0.0f, -2.9f, 0, 1, 0);    /* 13: edge, 0.1 m from one wall */
        add_v(&bx, 0.0f, 0.0f, 0.0f, 0, 1, 0);     /* 14: centre, 3 m from every wall */
        memset(vert, 7, sizeof vert);
        CHECK(bake(&bx, NULL, vert, &grid, &st, NULL) == 1);
        CHECK(vert[12] < vert[13] && vert[13] < vert[14] && vert[14] == 255u);
    }

    /* the tick: every 256 vertices from 0, and every 8 rows of the ground */
    {

        add_v(&t2, -6, 0, -6, 0, 1, 0); add_v(&t2, 6, 0, -6, 0, 1, 0); add_v(&t2, 6, 0, 6, 0, 1, 0); add_v(&t2, -6, 0, 6, 0, 1, 0);
        add_quad(&t2, 0, 1, 2, 3);
        while (t2.nv < 600u) add_v(&t2, (float)(t2.nv % 40u) * 0.1f - 2.0f, 0.0f, 0.5f, 0, 1, 0);
        ticks = 0u;
        CHECK(bake(&t2, NULL, vert, &grid, &st, count_tick) == 1);
        CHECK(ticks == t2.nv / 256u);
        ticks = 0u;
        CHECK(bake(&t2, &rect, vert, &grid, &st, count_tick) == 1);
        CHECK(grid.gh > 8u && ticks == t2.nv / 256u + grid.gh / 8u);
    }

    /* the ground grid: with a rectangle its bytes and the lookup; the cell by the wall is darker than 255 */
    {
        unsigned dark = 0, cells;

        CHECK(bake(&m, &rect, vert, &grid, &st, NULL) == 1);
        CHECK(grid.g != NULL && grid.gw > 8u && grid.gh > 8u);
        cells = grid.gw * grid.gh;
        for (i = 0; i < cells; i++) dark += grid.g[i] < 200u;
        CHECK(dark > 0u);
        CHECK(st.rays == (m.nv + cells) * SH4DER_AOMESH_RAYS);
        CHECK(sh4der_aomesh_ground(&grid, grid.x0 - 1.0f, grid.z0 + 1.0f) == 255u);
        CHECK(sh4der_aomesh_ground(&grid, grid.x0 + grid.gw * SH4DER_AOMESH_GCELL + 1.0f, 0.0f) == 255u);
        CHECK(sh4der_aomesh_ground(&grid, grid.x0 + 0.25f, grid.z0 + 0.25f) == grid.g[0]);
        CHECK(sh4der_aomesh_ground(&grid, -0.25f, 0.0f) < 255u);
        CHECK(bake(&m, &big, vert, &grid, &st, NULL) == 1 && grid.gw > grid.gh / 2u);
    }

    /* the empty list: refused, nothing written */
    {
        sh4der_aomesh_tris_t tt;
        sh4der_aomesh_need_t need;

        add_v(&e, 0, 0, 0, 0, 1, 0);
        tt.pos = &e.pos[0][0]; tt.stride = 4u; tt.idx = e.idx; tt.ntri = 0u;
        CHECK(sh4der_aomesh_need(&tt, NULL, &need) == 0 && need.box == 0u && need.cell == 0u && need.list == 0u && need.grid == 0u);
        memset(vert, 9, sizeof vert);
        memset(&st, 0x5a, sizeof st);
        e.nt = 0u;                               /* no triangles: the helper takes the count from the mesh */
        CHECK(bake(&e, NULL, vert, &grid, &st, NULL) == 0);
        CHECK(vert[0] == 9u && st.rays == 0x5a5a5a5au && st.tests == 0x5a5a5a5au && st.recips == 0x5a5a5a5au);
    }

    /* a zero normal: 255, and it casts no rays: the ray count is the other vertices' and the grid's cells' */
    {
        m.nrm[8][0] = 0.0f; m.nrm[8][1] = 0.0f; m.nrm[8][2] = 0.0f;
        memset(vert, 7, sizeof vert);
        CHECK(bake(&m, &rect, vert, &grid, &st, NULL) == 1);
        CHECK(vert[8] == 255u);
        CHECK(st.rays == (m.nv - 1u + grid.gw * grid.gh) * SH4DER_AOMESH_RAYS);
        m.nrm[8][1] = 1.0f;
    }

    /* a stride under 3 floats: refused (0 and 2); a stride of 3 (x y z packed) and of 4 are fine; a normal stride under 3
       is refused by the bake, with the vertex bytes untouched */
    {
        sh4der_aomesh_tris_t tt;
        sh4der_aomesh_need_t need;
        sh4der_aomesh_grid_t g3;
        sh4der_aomesh_stats_t s3;
        sh4der_aomesh_work_t w3;
        float packed[MAXV][3];
        unsigned j;

        tt.pos = &m.pos[0][0]; tt.stride = 0u; tt.idx = m.idx; tt.ntri = m.nt;
        CHECK(sh4der_aomesh_need(&tt, NULL, &need) == 0);
        tt.stride = 2u;
        CHECK(sh4der_aomesh_need(&tt, NULL, &need) == 0);
        for (j = 0; j < m.nv; j++) { packed[j][0] = m.pos[j][0]; packed[j][1] = m.pos[j][1]; packed[j][2] = m.pos[j][2]; }
        tt.pos = &packed[0][0];
        tt.stride = 3u;
        CHECK(sh4der_aomesh_need(&tt, NULL, &need) == 1);
        w3.box = (float (*)[6])malloc((size_t)need.box * sizeof w3.box[0]);
        w3.cell = (uint32_t *)malloc((size_t)need.cell * sizeof w3.cell[0]);
        w3.list = (uint16_t *)malloc((size_t)need.list * sizeof w3.list[0]);
        w3.grid = (uint8_t *)malloc(need.grid ? need.grid : 1u);
        memset(vert, 7, sizeof vert);
        CHECK(sh4der_aomesh_bake(&tt, &m.nrm[0][0], 3u, m.nv, NULL, &need, &w3, vert, &g3, &s3, NULL) == 1);
        CHECK(sh4der_aomesh_bake(&tt, &m.nrm[0][0], 2u, m.nv, NULL, &need, &w3, vert_again, &g3, &s3, NULL) == 0);
        CHECK(sh4der_aomesh_bake(&tt, &m.nrm[0][0], 0u, m.nv, NULL, &need, &w3, vert_again, &g3, &s3, NULL) == 0);
        free(w3.box); free(w3.cell); free(w3.list); free(w3.grid);
    }

    /* each buffer one entry too small: refused; the vertex bytes, the grid, the stats and every scratch byte as they were */
    {
        sh4der_aomesh_tris_t tt;
        sh4der_aomesh_need_t need, cap;
        sh4der_aomesh_work_t w;
        sh4der_aomesh_grid_t g2, g_ref;
        sh4der_aomesh_stats_t s2, s_ref;
        int k, rc;
        unsigned j;

        tt.pos = &m.pos[0][0]; tt.stride = 4u; tt.idx = m.idx; tt.ntri = m.nt;
        CHECK(sh4der_aomesh_need(&tt, &rect, &need) == 1 && need.box && need.cell && need.list && need.grid);
        for (k = 0; k < 4; k++) {
            cap = need;
            if (k == 0) cap.box--;
            if (k == 1) cap.cell--;
            if (k == 2) cap.list--;
            if (k == 3) cap.grid--;
            w.box = (float (*)[6])malloc((size_t)need.box * sizeof w.box[0]);
            w.cell = (uint32_t *)malloc((size_t)need.cell * sizeof w.cell[0]);
            w.list = (uint16_t *)malloc((size_t)need.list * sizeof w.list[0]);
            w.grid = (uint8_t *)malloc(need.grid);
            memset(w.box, 0xee, need.box * sizeof w.box[0]);
            memset(w.cell, 0xee, need.cell * sizeof w.cell[0]);
            memset(w.list, 0xee, need.list * sizeof w.list[0]);
            memset(w.grid, 0xee, need.grid);
            memset(vert, 0x33, m.nv);
            memset(&g2, 0x44, sizeof g2);
            memset(&g_ref, 0x44, sizeof g_ref);
            memset(&s2, 0x55, sizeof s2);
            memset(&s_ref, 0x55, sizeof s_ref);
            rc = sh4der_aomesh_bake(&tt, &m.nrm[0][0], 3u, m.nv, &rect, &cap, &w, vert, &g2, &s2, NULL);
            CHECK(rc == 0);
            for (j = 0; j < m.nv; j++) CHECK(vert[j] == 0x33u);
            for (j = 0; j < need.box * sizeof w.box[0]; j++) CHECK(((uint8_t *)w.box)[j] == 0xeeu);
            for (j = 0; j < need.cell * sizeof w.cell[0]; j++) CHECK(((uint8_t *)w.cell)[j] == 0xeeu);
            for (j = 0; j < need.list * sizeof w.list[0]; j++) CHECK(((uint8_t *)w.list)[j] == 0xeeu);
            for (j = 0; j < need.grid; j++) CHECK(w.grid[j] == 0xeeu);
            CHECK(memcmp(&g2, &g_ref, sizeof g2) == 0);
            CHECK(memcmp(&s2, &s_ref, sizeof s2) == 0);
            free(w.box); free(w.cell); free(w.list); free(w.grid);
        }
        /* the same sizes, not cut: accepted, and the grid is the buffer the caller gave */
        w.box = (float (*)[6])malloc((size_t)need.box * sizeof w.box[0]);
        w.cell = (uint32_t *)malloc((size_t)need.cell * sizeof w.cell[0]);
        w.list = (uint16_t *)malloc((size_t)need.list * sizeof w.list[0]);
        w.grid = (uint8_t *)malloc(need.grid);
        CHECK(sh4der_aomesh_bake(&tt, &m.nrm[0][0], 3u, m.nv, &rect, &need, &w, vert, &g2, &s2, NULL) == 1);
        CHECK(g2.g == w.grid && g2.gw * g2.gh == need.grid);
        free(w.box); free(w.cell); free(w.list); free(w.grid);
    }

    free(kept);
    printf(fails ? "aomesh_test: %d FAILED\n" : "aomesh_test: ok\n", fails);
    return fails != 0;
}
