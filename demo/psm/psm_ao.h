/* SH4DER-PVR: baked ambient occlusion for a psm scene (the walled square): an adapter over include/sh4der_aomesh.h

   demo/psm/psm_ao.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   The bake moved to the library on 2026-11-26 (include/sh4der_aomesh.h: the method, the ray set, the cell grid, the
   ground grid, the tuning macros SH4DER_AOMESH_*, its RECOMMENDED block and its status). What stays here is the scene:
   the buffers (malloc'd here, sized by sh4der_aomesh_need), the mapping of psm_scene_t's arrays (the static model's
   vertices and triangles, the normals, the walk area as the ground rectangle), and the frame's hand-over to psm_colour.
   The movers (people, hens) are open: 255.

   RECOMMENDED: see sh4der_aomesh.h. Here: psm_ao_bake at load (a tick for a watchdog), psm_ao_frame a frame.
   Status:     ADVISED for the demo; the bake's time and the look on a console are OPEN (NOT MEASURED).
   Maturity:   PROTOTYPE v0.1

   The occlusion multiplies the AMBIENT light only: colour = albedo x (ambient x ao + sun x direct). psm_colour
   (psm_scene.h) does that when the frame has the bytes (psm_frame_t.mao, gao_on); without them it is as it always was.

   Host tested: demo/psm/test/psm_ao_test.c (its bytes are pinned: the checksum on the plain C path). */

#ifndef PSM_AO_H
#define PSM_AO_H

#include <stdlib.h>
#include <string.h>
#include "psm_scene.h"
#include "sh4der_aomesh.h"

typedef struct psm_ao {
    int ok;                              /* the bake ran; 0: the bytes are all open */
    uint8_t mao[PSM_MV_MAX];             /* a model vertex: 255 open .. 0 closed */
    uint8_t *g;                          /* the ground's grid, gw x gh, row by row along x; NULL: none (open) */
    unsigned gw, gh;
    float gx0, gz0;
    uint32_t rays, tests;                /* rays cast, ray-triangle tests made (sh4der_aomesh_stats_t) */
} psm_ao_t;

static inline void psm_ao_free(psm_ao_t *a) { free(a->g); a->g = NULL; a->gw = a->gh = 0u; a->ok = 0; memset(a->mao, 255, sizeof a->mao); }

/* The bake of the static part of the scene: the model vertices (the first dyn0 of them) and triangles, normals as the
   model has them, the walk area as the ground rectangle. The positions are read in place: psm_scene_t's mv is float[3]
   a vertex, a stride of 3 floats (the library's stride is in floats, at least 3). The bake's buffers are malloc'd and
   given back here: the grid is kept (a->g). `tick` (NULL: none) is the library's heartbeat. 1: it ran; 0: no memory,
   nothing static, or the mesh is unusable, and the bytes are all open. A second call bakes again (another scene). */
static inline int psm_ao_bake(psm_ao_t *a, const psm_scene_t *s, void (*tick)(void)) {
    const unsigned vn = s->mdyn0 < s->mvn ? s->mdyn0 : s->mvn, tn = s->tdyn0 < s->mtn ? s->tdyn0 : s->mtn;
    sh4der_aomesh_tris_t m;
    sh4der_aomesh_rect_t r;
    sh4der_aomesh_need_t need, cap;
    sh4der_aomesh_work_t w;
    sh4der_aomesh_grid_t g;
    sh4der_aomesh_stats_t st = { 0u, 0u };
    int ok;

    psm_ao_free(a);
    a->rays = a->tests = 0u;
    if (!vn || !tn) return 0;
    m.pos = &s->mv[0][0]; m.stride = 3u; m.idx = s->mt; m.ntri = tn;
    r.x0 = s->x0; r.z0 = s->z0; r.x1 = s->x1; r.z1 = s->z1;
    if (!sh4der_aomesh_need(&m, &r, &need)) return 0;
    cap = need;
    w.box = (float (*)[6])malloc((size_t)need.box * sizeof w.box[0]);
    w.cell = (uint32_t *)malloc((size_t)need.cell * sizeof w.cell[0]);
    w.list = (uint16_t *)malloc((size_t)need.list * sizeof w.list[0]);
    w.grid = need.grid ? (uint8_t *)malloc((size_t)need.grid) : NULL;
    if (!w.box || !w.cell || !w.list || (need.grid && !w.grid)) {
        free(w.box); free(w.cell); free(w.list); free(w.grid);
        return 0;
    }
    ok = sh4der_aomesh_bake(&m, &s->mn[0][0], 3u, vn, &r, &cap, &w, a->mao, &g, &st, tick);
    if (ok) { a->g = g.g; a->gw = g.gw; a->gh = g.gh; a->gx0 = g.x0; a->gz0 = g.z0; }
    else free(w.grid);
    a->rays = st.rays; a->tests = st.tests;
    a->ok = ok;
    free(w.box); free(w.cell); free(w.list);
    return ok;
}

/* The ground's byte at (x, z): the cell's, 255 outside the grid. */
static inline uint8_t psm_ao_ground(const psm_ao_t *a, float x, float z) {
    const sh4der_aomesh_grid_t g = { a->gx0, a->gz0, a->gw, a->gh, a->g };

    return sh4der_aomesh_ground(&g, x, z);
}

/* The frame's AO for psm_colour: the model's bytes, and each ground vertex's byte, read again only when the ground's
   positions changed (gpos_gen). a NULL (or not baked): off, the colours as they always were. Call before psm_colour. */
static inline void psm_ao_frame(psm_frame_t *f, const psm_ao_t *a) {
    unsigned i;

    if (!a || !a->ok) { f->mao = NULL; f->gao_on = 0; return; }
    f->mao = a->mao;
    if (!f->gpos_ok || !a->g) { f->gao_on = 0; return; }
    if (!f->gao_on || f->gao_gen != f->gpos_gen || f->gao_n != f->gvn) {
        for (i = 0; i < f->gvn; i++) f->gao[i] = psm_ao_ground(a, f->gpos[i][0], f->gpos[i][2]);
        f->gao_gen = f->gpos_gen; f->gao_n = f->gvn;
    }
    f->gao_on = 1;
}

#endif /* PSM_AO_H */
