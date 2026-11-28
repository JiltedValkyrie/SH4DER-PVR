/* SH4DER-PVR: route A, the box and the ground patch. Dreamcast test
   demo/psm/routea/routea_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Runs include/sh4der_psm_a.h's pass (src/sh4der_psm_a.c) on a console: a 1.2 m box slides over an 8 m ground patch, a sun
   at about 53 degrees, the pass draws the box flat dark into a render target, the layer multiplies it over the ground.
   Nothing in it belongs to any game. It compares four tile-cut modes at 128 and 256, plus row 0 vs row 2, a no-pass frame
   parity shift in held mode, and a PVR restart:
     1 uncut  baseline (no tile cut, sleeping waits, as in ra2)
     2 ra2    per-pass cut/restore with sleeping pvr_wait_render_done (the ra2 behaviour: 20 ms stall)
     3 poll   per-pass cut/restore with pvrx polled render-done waits (sh4der_pvrx_wait_hook(1) + sh4der_pvrx_scene_finish)
     4 hold   single-table cut held on the pass's own table (no render-done wait in begin or settle; release on order break)
   A table with its first row left out (a compacted 640 x 360 band) is NOT made here: rows 0 and 2 of the plain 20 x 15
   table are.

   Twelve stages, each 120 frames, the picture held on the screen (the map is drawn in the corner):
     0  128 row 0 uncut     1  128 row 2 uncut     2  128 row 2 ra2       3  128 row 2 poll      4  128 row 2 hold
     5  256 row 0 uncut     6  256 row 2 uncut     7  256 row 2 ra2       8  256 row 2 poll      9  256 row 2 hold
     10 128 row 2 h-skp (hold + no-pass frame at 30: release, screen only, parity shifts to other table)
     11 128 row 2 r-pol (release, unhook, pvr_shutdown, vid_set_mode, pvr_init, re-hook, poll)
   At frame 45 of every stage the target is read back by PVR-DMA (sh4der_pvrx_vram_download) and its texture part is
   summed (FNV-1a) and counted: dark texels against the same triangles rasterised on the SH-4 (estimate to a few edge
   texels), and border texels not white. Every stage of a size must match that size's uncut sum. Timings are means of
   frames 20 .. 119 (readback and skip transition frames left out): pass PVR time (us), pass total on SH-4 clock (us),
   frame time (us), worst frame (us), tables found, and frames over 20 ms (checks Q69).

   RAM, VRAM, AICA: see the first page (computed from the program's own sizes). No sound is played. The program writes
   PVR registers and video memory, the PVR-DMA registers of pvrx, and rewrites KallistiOS's tile tables for cut passes
   (restored when each pass, no-pass frame, stage, or run ends; a bad restore is counted and stops the run).
   Build: python demo/psm/routea/build.py --tag <new tag> [--elf-only]. A plain ELF: it also runs from the suite's menu. */

#include <kos.h>
#include <stdarg.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "sh4der_suite.h"
#include "sh4der_core.h"
#include "sh4der_psm_a.h"
#include "sh4der_pvrx.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#ifndef ROUTEA_TAG
#define ROUTEA_TAG untagged
#endif
#ifndef ROUTEA_LIB
#define ROUTEA_LIB unknown
#endif
#ifndef ROUTEA_DIFF
#define ROUTEA_DIFF unknown
#endif
#define RA_STR_(x) #x
#define RA_STR(x) RA_STR_(x)

#define PG_PREFIX "ra"
#define PG_LINES_MAX 200u
#include "sh4der_page.h"

#define say pg_say
static sh4der_fault_crumbs_t crumbs;

#define STAGES   12u
#define FRAMES   120u
#define SKIP     20u                        /* frames before the means start */
#define READ_AT  45u                        /* the frame whose target is read back (the box stands still there, the same in every stage) */
#define MAX_SIZE 256u
#define MAX_ROW  2u
#define PATCH_HALF 4.0f                     /* an 8 m square of ground */
#define BOX_H    0.6f

static inline uint64_t ns(void) { const uint64_t n = timer_ns_gettime64(); return n - n / 1000000000ull * 2502400ull; }   /* KallistiOS's clock, its reload step taken out */

typedef struct {
    unsigned size, row;
    int cut;                                /* SH4DER_PSM_A_CUT_NONE, _CUT_PASS, _CUT_HOLD */
    int poll;                               /* 0: sleeping pvr_wait_render_done, 1: pvrx polled wait */
    int restart;                            /* 1: release, unhook, pvr_shutdown, vid_set_mode, pvr_init before stage */
    unsigned skip_at;                       /* >0: frame with no pass (release + screen only, shifts table parity) */
    const char *label;
} stage_t;

static const stage_t stage[STAGES] = {
    { 128, 0, SH4DER_PSM_A_CUT_NONE, 0, 0,  0, "uncut" },
    { 128, 2, SH4DER_PSM_A_CUT_NONE, 0, 0,  0, "uncut" },
    { 128, 2, SH4DER_PSM_A_CUT_PASS, 0, 0,  0, "ra2  " },
    { 128, 2, SH4DER_PSM_A_CUT_PASS, 1, 0,  0, "poll " },
    { 128, 2, SH4DER_PSM_A_CUT_HOLD, 1, 0,  0, "hold " },
    { 256, 0, SH4DER_PSM_A_CUT_NONE, 0, 0,  0, "uncut" },
    { 256, 2, SH4DER_PSM_A_CUT_NONE, 0, 0,  0, "uncut" },
    { 256, 2, SH4DER_PSM_A_CUT_PASS, 0, 0,  0, "ra2  " },
    { 256, 2, SH4DER_PSM_A_CUT_PASS, 1, 0,  0, "poll " },
    { 256, 2, SH4DER_PSM_A_CUT_HOLD, 1, 0,  0, "hold " },
    { 128, 2, SH4DER_PSM_A_CUT_HOLD, 1, 0, 30, "h-skp" },
    { 128, 2, SH4DER_PSM_A_CUT_PASS, 1, 1,  0, "r-pol" },
};

typedef struct {
    int ran, hooked;
    uint32_t crc;                           /* FNV-1a of the texture part of the readback */
    unsigned dark, expect, border_bad;      /* dark texels read, the same triangles counted on the SH-4, border texels not white */
    int dma;                                /* the readback's return: 0 good */
    unsigned pvr_ns, pass_ns, frame_ns, frame_max_us;   /* means over the counted frames */
    unsigned pvr_pre_ns, pvr_post_ns;       /* skip_at stage: pass PVR mean before and after skip_at */
    unsigned pvr_read_us;                   /* held mode: pass PVR time from waited READ_AT frame (cross-check) */
    unsigned slow_frames;                   /* counted frames > 20,000 us (PVR-118 / Q69 check) */
    unsigned cut_passes, init_uncut, passes, found, refused, broken, bad, vram_free_kb;
    unsigned long timeouts;
    int held_before, held_after;            /* table index held cut at frame 25 and frame 45 (-1 if none) */
    unsigned sent_tris;
} result_t;
static result_t res[STAGES];

static uint16_t rb[MAX_SIZE * (MAX_ROW * 32u + MAX_SIZE)] __attribute__((aligned(32)));   /* the readback: the whole render */
static uint8_t cover[MAX_SIZE * MAX_SIZE];                                           /* the SH-4's own count of the box's texels */
static sh4der_psm_a_t a;
static sh4der_psm_a_target_t tgt;
static pvr_poly_hdr_t h_col, h_map;
static pvr_vertex_t vbuf[16] __attribute__((aligned(32)));

/* ---- the scene ------------------------------------------------------------------------------------------------- */

static const uint16_t box_idx[12][3] = {
    { 0, 1, 2 }, { 1, 3, 2 }, { 4, 6, 5 }, { 5, 6, 7 }, { 0, 4, 1 }, { 1, 4, 5 },
    { 2, 3, 6 }, { 3, 7, 6 }, { 0, 2, 4 }, { 2, 6, 4 }, { 1, 5, 3 }, { 3, 5, 7 },
};
static float box_pos[8][3];
static const float light[3] = { 0.4472136f, 0.7155418f, 0.5403023f };    /* unit, toward the sun: about 53 degrees up */

static float box_x(unsigned f) {            /* a triangle wave over 120 frames, +-2 m */
    const float t = (float)(f % 120u) * (1.0f / 30.0f) - 2.0f;

    return 2.0f * ((t < 0.0f ? -t : t) - 1.0f);
}

static void box_make(float cx) {
    unsigned i;

    for (i = 0; i < 8u; i++) {
        box_pos[i][0] = cx + ((i & 1u) ? BOX_H : -BOX_H);
        box_pos[i][1] = (i & 2u) ? 2.0f * BOX_H : 0.0f;
        box_pos[i][2] = (i & 4u) ? BOX_H : -BOX_H;
    }
}

/* the view: a camera behind and above the patch's near edge */
static float vr[3], vu[3], vf[3];
static const float eye[3] = { 0.0f, 5.5f, -8.5f };

static void view_make(void) {
    const float at[3] = { 0.0f, 0.3f, 0.0f }, up[3] = { 0.0f, 1.0f, 0.0f };

    vf[0] = at[0] - eye[0]; vf[1] = at[1] - eye[1]; vf[2] = at[2] - eye[2];
    sh4der_norm3_(vf);
    vr[0] = up[1] * vf[2] - up[2] * vf[1]; vr[1] = up[2] * vf[0] - up[0] * vf[2]; vr[2] = up[0] * vf[1] - up[1] * vf[0];
    sh4der_norm3_(vr);
    vu[0] = vf[1] * vr[2] - vf[2] * vr[1]; vu[1] = vf[2] * vr[0] - vf[0] * vr[2]; vu[2] = vf[0] * vr[1] - vf[1] * vr[0];
}

static int project(const float p[3], float *sx, float *sy, float *sz) {
    const float d[3] = { p[0] - eye[0], p[1] - eye[1], p[2] - eye[2] };
    const float zc = sh4der_dot3_(d, vf), xc = sh4der_dot3_(d, vr), yc = sh4der_dot3_(d, vu);
    float inv;

    if (zc < 0.5f) return 0;
    inv = 1.0f / zc;
    *sx = 320.0f + 560.0f * xc * inv;
    *sy = 240.0f - 560.0f * yc * inv;
    *sz = inv;
    return 1;
}

static void vtx(pvr_vertex_t *v, float x, float y, float z, float u, float w, uint32_t argb, int eol) {
    v->flags = eol ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    v->x = x; v->y = y; v->z = z; v->u = u; v->v = w; v->argb = argb; v->oargb = 0u;
}

static void ground_quad(float y, uint32_t argb) {
    const float c[4][3] = { { -PATCH_HALF, y, -PATCH_HALF }, { PATCH_HALF, y, -PATCH_HALF }, { -PATCH_HALF, y, PATCH_HALF }, { PATCH_HALF, y, PATCH_HALF } };
    unsigned i;

    for (i = 0; i < 4u; i++) {
        float sx, sy, sz;

        if (!project(c[i], &sx, &sy, &sz)) return;
        vtx(&vbuf[i], sx, sy, sz, 0.0f, 0.0f, argb, i == 3u);
    }
    pvr_prim(vbuf, 4u * sizeof vbuf[0]);
}

static void box_draw(void) {
    unsigned i, k, n = 0;
    static const uint32_t shade[6] = { 0xffd84a3a, 0xffd84a3a, 0xff9a2e22, 0xff9a2e22, 0xffe8806a, 0xffe8806a };

    for (i = 0; i < 12u; i++) {
        for (k = 0; k < 3u; k++) {
            float sx, sy, sz;

            if (!project(box_pos[box_idx[i][k]], &sx, &sy, &sz)) { n = 0; break; }
            vtx(&vbuf[n++], sx, sy, sz, 0.0f, 0.0f, shade[i / 2u], k == 2u);
        }
        if (n) pvr_prim(vbuf, n * sizeof vbuf[0]);
        n = 0;
    }
}

static void corner_view(void) {             /* the map itself, 128 pixels, in the top left of the screen */
    vtx(&vbuf[0], 8.0f, 8.0f, 5.0f, 0.0f, 0.0f, 0xffffffffu, 0);
    vtx(&vbuf[1], 136.0f, 8.0f, 5.0f, 1.0f, 0.0f, 0xffffffffu, 0);
    vtx(&vbuf[2], 8.0f, 136.0f, 5.0f, 0.0f, 1.0f, 0xffffffffu, 0);
    vtx(&vbuf[3], 136.0f, 136.0f, 5.0f, 1.0f, 1.0f, 0xffffffffu, 1);
    pvr_prim(vbuf, 4u * sizeof vbuf[0]);
}

static void screen_headers(void) {
    pvr_poly_cxt_t c;

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&h_col, &c);
}

static void map_header(void) {              /* the map as a picture: the target's texture, point sampled, opaque */
    pvr_poly_cxt_t c;

    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)tgt.cfg.size, (int)tgt.cfg.size, (pvr_ptr_t)tgt.tex, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&h_map, &c);
}

static void screen_scene(const sh4der_psm_a_target_t *t) {
    const pvr_poly_hdr_t *layer = sh4der_psm_a_layer_header(t);
    static const float corner[4][3] = { { -PATCH_HALF, 0.03f, -PATCH_HALF }, { PATCH_HALF, 0.03f, -PATCH_HALF }, { -PATCH_HALF, 0.03f, PATCH_HALF }, { PATCH_HALF, 0.03f, PATCH_HALF } };
    unsigned i;

    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&h_col, sizeof h_col);
    ground_quad(0.0f, 0xff8a8a80u);
    box_draw();
    pvr_prim(&h_map, sizeof h_map);
    corner_view();
    pvr_list_finish();
    if (layer && a.built) {
        pvr_list_begin(PVR_LIST_TR_POLY);
        pvr_prim((void *)(uintptr_t)layer, sizeof *layer);
        for (i = 0; i < 4u; i++) {
            float sx, sy, sz;

            if (!project(corner[i], &sx, &sy, &sz) || !sh4der_psm_a_layer_vertex(&a, corner[i], sx, sy, sz, i == 3u, &vbuf[i])) break;
        }
        if (i == 4u) pvr_prim(vbuf, 4u * sizeof vbuf[0]);
        pvr_list_finish();
    }
#ifdef ROUTEA_PLAIN_FINISH
    pvr_scene_finish();                     /* build with -DROUTEA_PLAIN_FINISH and the link wrap: a plain finish must still be counted (the pass keeps sh4der_pvrx_scene_finish: both paths, no double count) */
#else
    sh4der_pvrx_scene_finish();             /* counts every scene when sh4der_pvrx_wait_hook(1) is on; plain pvr_scene_finish when off */
#endif
}

/* ---- the check: the same triangles counted by the SH-4 ---------------------------------------------------------- */

static unsigned expect_count(const sh4der_psm_a_t *am) {
    sh4der_psm_a_t tmp = *am;
    sh4der_psm_a_tri_t tri[SH4DER_PSM_PVR_MAX_TRIS];
    unsigned i, m, n, count = 0;
    const unsigned size = am->size;

    memset(cover, 0, size * size);
    tmp.y0 = 0.0f;                          /* the map's own pixels */
    for (i = 0; i < 12u; i++) {
        n = sh4der_psm_a_tri(&tmp, box_pos[box_idx[i][0]], box_pos[box_idx[i][1]], box_pos[box_idx[i][2]], tri);
        for (m = 0; m < n; m++) {
            const sh4der_psm_pvr_vtx_t *v = tri[m].v;
            float x0 = v[0].x, x1 = v[1].x, x2 = v[2].x, y0 = v[0].y, y1 = v[1].y, y2 = v[2].y;
            float lo_x = x0 < x1 ? (x0 < x2 ? x0 : x2) : (x1 < x2 ? x1 : x2), hi_x = x0 > x1 ? (x0 > x2 ? x0 : x2) : (x1 > x2 ? x1 : x2);
            float lo_y = y0 < y1 ? (y0 < y2 ? y0 : y2) : (y1 < y2 ? y1 : y2), hi_y = y0 > y1 ? (y0 > y2 ? y0 : y2) : (y1 > y2 ? y1 : y2);
            int px, py, ax = (int)(lo_x < 0.0f ? 0.0f : lo_x), bx = (int)(hi_x + 1.0f), ay = (int)(lo_y < 0.0f ? 0.0f : lo_y), by = (int)(hi_y + 1.0f);

            if (bx > (int)size) bx = (int)size;
            if (by > (int)size) by = (int)size;
            for (py = ay; py < by; py++)
                for (px = ax; px < bx; px++) {
                    const float cx = (float)px + 0.5f, cy = (float)py + 0.5f;
                    const float e0 = (x1 - x0) * (cy - y0) - (y1 - y0) * (cx - x0);
                    const float e1 = (x2 - x1) * (cy - y1) - (y2 - y1) * (cx - x1);
                    const float e2 = (x0 - x2) * (cy - y2) - (y0 - y2) * (cx - x2);

                    if ((e0 >= 0.0f && e1 >= 0.0f && e2 >= 0.0f) || (e0 <= 0.0f && e1 <= 0.0f && e2 <= 0.0f)) cover[(unsigned)py * size + (unsigned)px] = 1u;
                }
        }
    }
    for (i = 0; i < size * size; i++) count += cover[i];
    return count;
}

/* ---- one stage -------------------------------------------------------------------------------------------------- */

static int pvr_start(int hook) {
    if (pvr_init_defaults() < 0) return 0;
    pvr_set_bg_color(0.1f, 0.1f, 0.12f);
    vid_set_dithering(false);               /* a value must reach the target as it is (wiki PVR-023) */
    if (hook) sh4der_pvrx_wait_hook(1);
    if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused");
    screen_headers();
    return 1;
}

/* The polled wait, then KallistiOS's wait as a backstop: it returns at once when no render is busy (DERIVED from reading
   pvr_scene.c, NOT RUN), so it costs nothing normally and covers a timeout or a scene not finished through pvrx. */
static void ra_wait_done(void) {
    (void)sh4der_pvrx_wait_render_done_poll(SH4DER_PVRX_WAIT_US);
    pvr_wait_render_done();
}

static void step(const char *name) {
    sh4der_fault_beat(&crumbs);
    sh4der_fault_stage(&crumbs, name);
    crumbs.frame++;
}

static void readback(unsigned k) {
    result_t *r = &res[k];
    const unsigned size = tgt.cfg.size, row = tgt.cfg.row;
    const uint16_t *px = rb + row * 32u * size;
    uint32_t h = 2166136261u;
    unsigned i, x;

    r->dma = sh4der_pvrx_vram_download(rb, (pvr_ptr_t)tgt.base, tgt.bytes, SH4DER_PVRX_AREA_64, SH4DER_PVRX_END_POLL, SH4DER_PVRX_DMA_US);
    if (r->dma) return;
    for (i = 0; i < size * size; i++) {
        h = (h ^ (px[i] & 0xffu)) * 16777619u;
        h = (h ^ (px[i] >> 8)) * 16777619u;
        r->dark += (px[i] >> 11) < 16u;
    }
    r->crc = h;
    for (x = 0; x < size; x++)              /* the border strips: one texel, white */
        r->border_bad += (px[x] != 0xffffu) + (px[(size - 1u) * size + x] != 0xffffu) + (px[x * size] != 0xffffu) + (px[x * size + size - 1u] != 0xffffu);
    r->expect = expect_count(&a);
}

static int run_stage(unsigned k) {
    const stage_t *s = &stage[k];
    result_t *r = &res[k];
    sh4der_psm_a_cfg_t cfg;
    uint64_t f_prev = 0, pvr_sum = 0, pvr_pre_sum = 0, pvr_post_sum = 0, pass_sum = 0, frame_sum = 0;
    unsigned f, pvr_n = 0, pvr_pre_n = 0, pvr_post_n = 0, pass_n = 0, frame_n = 0;
    unsigned long tmo0;
    int prev_held = 0;
    char name[14];
    static const float shadow[3] = { 0.35f, 0.35f, 0.38f };

    r->held_before = r->held_after = -1;
    snprintf(name, sizeof name, "ra stage %u", k);
    step(name);
    if (s->restart) {
        const unsigned bad = sh4der_psm_a_release(&tgt);

        if (bad) { r->bad += bad; say("stage %u: release before restart failed (%u words): stopping", k, bad); return 0; }
        pvr_wait_ready();
        ra_wait_done();
        sh4der_pvrx_wait_hook(0);           /* Call order: pvrx step 7, before pvr_shutdown */
        sh4der_psm_a_target_forget(&tgt);   /* the memory dies with the old PVR */
        pvr_shutdown();
        vid_set_mode(DM_640x480, PM_RGB565);
        if (!pvr_start(s->poll)) return 0;
    } else {
        if (tgt.ready) {
            const unsigned bad = sh4der_psm_a_release(&tgt);

            if (bad) { r->bad += bad; say("stage %u: release from previous stage failed (%u words): stopping", k, bad); return 0; }
            sh4der_psm_a_target_free(&tgt);
        } else {
            pvr_wait_ready();
            ra_wait_done();
        }
        sh4der_pvrx_wait_hook(s->poll);     /* no render in flight here: safe to switch hook on or off */
    }
    r->hooked = sh4der_pvrx_wait_hook(s->poll);
    tmo0 = sh4der_pvrx_wait_timeouts();

    sh4der_psm_a_cfg_default(&cfg);
    cfg.size = s->size; cfg.row = s->row; cfg.cut = s->cut;
    if (!sh4der_psm_a_target_init(&tgt, &cfg)) { say("stage %u: no video memory for the target", k); return 0; }
    sh4der_psm_a_init(&a, s->size, s->row);
    map_header();
    r->ran = 1;
    r->vram_free_kb = (unsigned)(pvr_mem_available() / 1024u);

    for (f = 0; f < FRAMES; f++) {
        const uint64_t t0 = ns();
        const float centre[3] = { 0.0f, 0.0f, 0.0f };
        const int skip_near = s->skip_at && f >= s->skip_at && f <= s->skip_at + 2u;
        sh4der_psm_a_caster_t cast;
        uint64_t tp0 = 0;
        int opened;

        sh4der_fault_beat(&crumbs);
        if (f_prev && f >= SKIP && f != READ_AT + 1u && !(s->skip_at && (f == s->skip_at + 1u || f == s->skip_at + 2u))) {
            const unsigned d = (unsigned)((t0 - f_prev) / 1000u);

            frame_sum += d; frame_n++;
            if (d > r->frame_max_us) r->frame_max_us = d;
            if (d > 20000u) r->slow_frames++;
        }
        f_prev = t0;
        box_make(box_x(f));

        if (s->skip_at && f == s->skip_at) {    /* a frame without a pass: release held table before screen, shifting parity */
            const unsigned bad = sh4der_psm_a_release(&tgt);

            if (bad) { r->bad += bad; say("stage %u frame %u: release failed (%u words): stopping", k, f, bad); return 0; }
            sh4der_psm_a_begin_frame(&a, shadow);
            prev_held = 0;
            screen_scene(&tgt);
            continue;
        }

        if (s->cut && tgt.cut.n < tgt.cut.need) {      /* the tile tables are found after a render, until both are known */
            pvr_wait_ready();
            ra_wait_done();
            sh4der_psm_a_find(&tgt);
        }
        sh4der_psm_a_begin_frame(&a, shadow);
        sh4der_psm_a_fit(&a, light, centre, PATCH_HALF, -0.05f, 0.05f);
        cast.pos = (const float (*)[3])box_pos; cast.idx = box_idx; cast.nt = 12u; cast.strength = 1.0f; cast.max_tris = 0u;
        const int tables_ready = tgt.cut.n >= tgt.cut.need;
        tp0 = ns();
        opened = sh4der_psm_a_begin(&tgt, &a);
        if (opened) {
            pvr_stats_t st;
            const int cut_now = tgt.cut_on;
            const int held_now = s->cut == SH4DER_PSM_A_CUT_HOLD && cut_now;
            unsigned bad;
            uint64_t tp1;

            /* Held mode: right after begin's pvr_wait_ready, the previous screen just started rendering,
               so rnd_last_time holds the previous frame's pass render time. */
            if (held_now && prev_held && f >= SKIP && f != READ_AT && f != READ_AT + 1u && !skip_near && pvr_get_stats(&st) >= 0) {
                pvr_sum += st.rnd_last_time;
                pvr_n++;
                if (s->skip_at) {
                    if (f < s->skip_at) { pvr_pre_sum += st.rnd_last_time; pvr_pre_n++; }
                    else { pvr_post_sum += st.rnd_last_time; pvr_post_n++; }
                }
            }
            sh4der_psm_a_caster(&tgt, &a, &cast);
            sh4der_psm_a_end(&tgt, &a);
            bad = sh4der_psm_a_settle(&tgt);
            tp1 = ns();
            if (bad) { r->bad += bad; say("stage %u frame %u: the tile table did not come back (%u words): stopping", k, f, bad); return 0; }
            r->passes++;
            if (s->cut && !tables_ready) r->init_uncut++;
            r->cut_passes += cut_now != 0;
            if (f == 25u) r->held_before = tgt.cut_held;
            if (f == READ_AT) r->held_after = tgt.cut_held;
            if (f >= SKIP && f != READ_AT && f != READ_AT + 1u && !skip_near) {
                pass_sum += tp1 - tp0;
                pass_n++;
                if (!held_now && pvr_get_stats(&st) >= 0) { pvr_sum += st.rnd_last_time; pvr_n++; }
            }
            if (f == READ_AT) {
                if (held_now) {                 /* wait for pass READ_AT to finish before DMA readback */
                    pvr_wait_ready();
                    ra_wait_done();
                    if (pvr_get_stats(&st) >= 0) r->pvr_read_us = (unsigned)(st.rnd_last_time / 1000u);
                }
                readback(k);
            }
            prev_held = held_now && f != READ_AT;
        } else {
            prev_held = 0;
        }
        screen_scene(&tgt);
        if (f == READ_AT) r->found = tgt.cut.n;
    }
    r->refused = tgt.cut_refused;
    r->broken = tgt.cut_broken;
    r->sent_tris = a.sent_tris;
    r->timeouts = sh4der_pvrx_wait_timeouts() - tmo0;
    if (pvr_n) r->pvr_ns = (unsigned)(pvr_sum / pvr_n);
    if (pvr_pre_n) r->pvr_pre_ns = (unsigned)(pvr_pre_sum / pvr_pre_n);
    if (pvr_post_n) r->pvr_post_ns = (unsigned)(pvr_post_sum / pvr_post_n);
    if (pass_n) r->pass_ns = (unsigned)(pass_sum / pass_n);
    if (frame_n) r->frame_ns = (unsigned)(frame_sum / frame_n * 1000u);   /* kept in ns: the means above are us * 1000 */
    return 1;
}

/* ---- the pages -------------------------------------------------------------------------------------------------- */

static void report(unsigned done) {
    unsigned k;
    const unsigned ram = (unsigned)(sizeof rb + sizeof cover + sizeof a + sizeof tgt + sizeof vbuf + sizeof res);
    static const unsigned s128[7] = { 0, 1, 2, 3, 4, 10, 11 };
    static const unsigned s256[5] = { 5, 6, 7, 8, 9 };
    int ok128 = 1, any128 = 0, ok256 = 1, any256 = 0;

    say("ROUTE A: box and ground patch (tile-cut wait comparison)");
    say("build %s  library HEAD %s  working tree diff %s", RA_STR(ROUTEA_TAG), RA_STR(ROUTEA_LIB), RA_STR(ROUTEA_DIFF));
    say("stages run %u of %u. One console, one run: MEASURED only where", done, STAGES);
    say("a line says a number; the rest reads OK / DIFF against itself.");
#ifdef ROUTEA_PLAIN_FINISH
    say("SCREEN SCENE ENDS WITH PLAIN pvr_scene_finish, link wrap on: wrapped %d (1 = counted by the wrap)", sh4der_pvrx_wrapped());
    say("A wrong count shows as tmo above 0 (double) or a DIFF sum / short frame (missed).");
#endif
    say("MODES:");
    say(" uncut  no cut, sleeping waits (baseline)");
    say(" ra2    cut both tables a pass, sleeping waits (ra2)");
    say(" poll   cut both tables a pass, pvrx polled render-done");
    say(" hold   cut pass table only, left cut (no render-done wait)");
    say(" h-skp  hold + frame 30 no pass (release, shifts parity)");
    say(" r-pol  poll after release, unhook, pvr_shutdown, pvr_init");
    say("MEMORY of the test (computed from its sizes):");
    say(" RAM  %u KB statics (readback %u KB, rest %u KB)", (ram + 1023u) / 1024u, (unsigned)sizeof rb / 1024u, (ram - (unsigned)sizeof rb + 1023u) / 1024u);
    if (res[0].ran) say(" VRAM 128 r0: 32 KB, %u KB free; 128 r2: 48 KB, %u KB free", res[0].vram_free_kb, res[1].ran ? res[1].vram_free_kb : 0u);
    if (res[5].ran) say(" VRAM 256 r0: 128 KB, %u KB free; 256 r2: 160 KB, %u KB free", res[5].vram_free_kb, res[6].ran ? res[6].vram_free_kb : 0u);
    say(" AICA 0 KB: the test plays no sound");

    pg_break();
    say("Q1/Q2 PICTURE: FNV-1a of map texels at frame %u", READ_AT);
    say("#  sz  r mode  sum      dark  exp bdr dm pass cut");
    for (k = 0; k < STAGES; k++)
        if (res[k].ran)
            say("%2u %3u %u %s %08x %4u %4u %3u %2d %4u %3u", k, stage[k].size, stage[k].row, stage[k].label, (unsigned)res[k].crc, res[k].dark, res[k].expect, res[k].border_bad, res[k].dma, res[k].passes, res[k].cut_passes);
    for (k = 0; k < 7u; k++) {
        const unsigned id = s128[k];

        if (res[id].ran && !res[id].dma) { if (any128 && res[id].crc != res[0].crc) ok128 = 0; any128 = 1; }
    }
    for (k = 0; k < 5u; k++) {
        const unsigned id = s256[k];

        if (res[id].ran && !res[id].dma) { if (any256 && res[id].crc != res[5].crc) ok256 = 0; any256 = 1; }
    }
    say("128 (all 7 stages): %s", any128 ? (ok128 ? "OK, one picture" : "DIFF, sums differ") : "not run");
    say("256 (all 5 stages): %s", any256 ? (ok256 ? "OK, one picture" : "DIFF, sums differ") : "not run");
    if (res[10].ran) {
        const int ok10 = res[10].held_before >= 0 && res[10].held_after >= 0 && res[10].held_before != res[10].held_after &&
                         !res[10].dma && res[10].crc == res[1].crc &&
                         res[10].broken == 0u && res[10].bad == 0u && res[10].timeouts == 0ul && res[10].refused == 0u &&
                         res[10].init_uncut <= 2u && res[10].cut_passes + res[10].init_uncut == res[10].passes;

        say("h-skp: tbl %d->%d cut %u/%u brk %u bad %u tmo %lu %s",
            res[10].held_before, res[10].held_after, res[10].cut_passes, res[10].passes,
            res[10].broken, res[10].bad, res[10].timeouts, ok10 ? "OK" : "CHECK");
        say("h-skp PVR mean: pre-30 %u us, post-30 %u us",
            res[10].pvr_pre_ns / 1000u, res[10].pvr_post_ns / 1000u);
    }
    if (res[11].ran && res[1].ran)
        say("r-pol after PVR restart vs stage 1: %s", !res[11].dma && res[11].crc == res[1].crc ? "OK, same sum" : "DIFF");
    say("dark: R<16/31; exp: SH-4 count (est); bdr: non-white edge.");

    pg_break();
    say("Q3 COST (means frames %u..%u, MEASURED once, us)", SKIP, FRAMES - 1u);
    say("#  sz  r mode   PVR  pass tot  frame frm max fn >20");
    for (k = 0; k < STAGES; k++)
        if (res[k].ran)
            say("%2u %3u %u %s %4u %9u %6u %7u %2u %3u", k, stage[k].size, stage[k].row, stage[k].label, res[k].pvr_ns / 1000u, res[k].pass_ns / 1000u, res[k].frame_ns / 1000u, res[k].frame_max_us, res[k].found, res[k].slow_frames);
    say("PVR: pass render time (pvr_get_stats; in hold, read right");
    say("after begin while prev screen renders). pass tot: begin to");
    say("settle on SH-4 clock. frame: frame start to start (60 Hz =");
    say("16,667). fn: tile tables found (2 = both). >20: counted");
    say("frames over 20 ms (checks Q69 1-in-5 polled spike).");

    pg_break();
    say("CUT, POLL AND HELD DIAGNOSTICS");
    say("#  mode  hook cut/pass ref brk bad tmo pvr_rd");
    for (k = 0; k < STAGES; k++)
        if (res[k].ran && (stage[k].cut || stage[k].poll))
            say("%2u %s %4d %4u/%3u %3u %3u %3u %3lu %6u", k, stage[k].label, res[k].hooked, res[k].cut_passes, res[k].passes, res[k].refused, res[k].broken, res[k].bad, res[k].timeouts, res[k].pvr_read_us);
    say("hook: sh4der_pvrx_wait_hook state. ref: cuts refused.");
    say("brk: held order breaks recovered. bad: restore words bad.");
    say("tmo: pvrx wait timeouts. pvr_rd: held READ_AT waited PVR us.");

    pg_break();
    say("WHAT THIS DOES NOT SHOW");
    say("- A table with row 0 left out (compacted 640x360 band): not");
    say("  made here; rows 0 and 2 of the plain 20x15 table only.");
    say("- A heavy game scene: at 60 Hz, a light scene rests on the");
    say("  16.67 ms vsync floor once the 20 ms tick stall is gone.");
    say("- Self-shadow: none, by design. One light, one box.");
    say("- Any figure from another console, or the owner's eye.");
}

int main(void) {
    unsigned k, done = 0, page = 0, pages;
    int redraw = 1;

    suite_init();                           /* on the suite's disc: the menu's binary into RAM, before anything else */
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    view_make();
    if (pvr_start(0)) {
        for (k = 0; k < STAGES; k++) {
            if (!run_stage(k)) break;
            done++;
        }
        step("ra end");
        sh4der_psm_a_target_free(&tgt);
        pvr_wait_ready();
        ra_wait_done();
        sh4der_pvrx_wait_hook(0);           /* Call order: pvrx step 7, before pvr_shutdown */
        vid_set_dithering(true);
        pvr_shutdown();
    }
    report(done);
    vid_set_mode(DM_640x480, PM_RGB565);
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u;

        sh4der_fault_beat(&crumbs);
        pages = pg_pages();
        if (suite_page(now, &page, pages)) redraw = 1;
        if (redraw) { pg_draw(page % pages, "ROUTE A", suite_keys_text("A/B pages")); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
