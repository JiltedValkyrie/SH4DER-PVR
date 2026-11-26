/* SH4DER-PVR: perspective shadow maps on the Dreamcast, two routes side by side

   demo/psm/psm_dc.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   One disc program, one scene, two ways to shadow it, switched live:

     ROUTE A  the PVR draws the map: the casters flat into a render target (pvr_scene_begin_rtt), the ground drawn a
              second time as a multiply layer that looks the target up per pixel (sh4der_psm_pvr.h). No depth compare,
              so a model does not shadow itself.
     ROUTE B  the SH-4 draws the map: a 16-bit depth map in RAM, compared per vertex, the answer in the vertex colour
              (sh4der_psm_raster.h). A model shadows itself; the edge is as fine as the ground's vertices.

   On each, the uniform map against the perspective shadow map at the same size. The scene, the pad's meaning, the
   map, the screen placement, the cells and the readout's text are demo/psm/psm_scene.h, which the host tests
   (test/psm_scene_test.c). This file is the KallistiOS part: PVR setup, the render target, the lists, the pad, the
   timers and the text textures.

   Pad, as the torch walk demo (demo/torchwalk_ab.c, single stick): the stick looks (INVERT Y on: up looks down), Y / A
   held walk forward / back, X / B strafe left / right. D-pad up steps the route (A, B, AB), down flips uniform / PSM,
   left / right the clock (the town) or the sun's azimuth (the field), the L / R triggers the sun's elevation. START opens
   the settings panel (three tabs: SHADOWS, SUN & TIME, VIEW; every row, the warp's near plane, the casters, the presets and
   FREEZE among them): in it nothing walks, L / R pick the tab, D-pad up / down the row, left / right and A change it, B or
   START close it. A value held repeats. The meaning is psm_input in psm_scene.h, the panel psm_panel_* there and
   psm_send_panel in psm_send.h; this file reads the pad.

   DRAW DISTANCE (a menu row): the ground cells and model triangles wholly past it in view depth are not sent (psm_place), and
   the PVR's table fog (pvr_fog_table_linear, the sky's colour) fades the last quarter of it, on the ground and the models only
   (not the route A layer, the text or the map pass). INFINITE sends everything through headers without fog, as before. The map's
   casters do not follow it.

   THE TOWN (the SCENE row; the image boots into it, 14:00, the first view): a market square of the Modular Village kit (CC0)
   with a church tower, people and hens as placeholder blocks, the sun on a clock (TIME, TIME SPEED; D-pad left / right a
   quarter of an hour). demo/psm/psm_town.h is its logic, demo/psm/tools/bake_town.py its mesh. The models go as strips; the
   casters are their own low meshes, culled an object at a time against the map's square. With the air on (state atmos: the
   town's default, the later LOOK row's slot) the sky is a textured band with a cap, hills and a treeline ring the town
   (demo/psm/psm_sky.h, the texture baked by demo/psm/tools/bake_sky.py from a CC0 Poly Haven sky), and the fog is quadratic
   from 0.43 to 1 x DRAW DISTANCE in the colour of the band's horizon. Off, the sky and fog come from psm_look_at as before.

   The cost label reads "--" until the console has measured a second. Nothing in it comes from a host.

   KallistiOS paths: pvr_init, pvr_mem_malloc / pvr_mem_free (the target, the textures), pvr_scene_begin_rtt (the
   map pass), pvr_prim + pvr_dr_target / pvr_dr_commit (store queue vertices), pvr_txr_load (text and inset uploads),
   pvr_get_stats after a wait (the PVR's render time, one sampled frame a second: see render_wait), timer_us_gettime64
   (the SH-4's), maple (the pad), minifont (the text). Build: make psm-dc (see the Makefile). Compiles to nothing off
   the Dreamcast. */

#ifdef _arch_dreamcast

#include <kos.h>
#include <malloc.h>
#include <dc/minifont.h>
#include "sh4der_guard.h"
#include "psm_scene.h"
#include "psm_mapsrc.h"
#include "psm_sky.h"
#include "psm_ground.h"
#include "psm_ao.h"
#define PSM_FAULT_IMPL
#include "psm_fault.h"
#ifdef PSM_VIEWCOST                 /* the view cost test (psm_viewcost.h): a script for the pad and a timer between the frame's steps */
#define PG_PREFIX "vc"
#include "psm_page.h"
#include "psm_viewcost.h"
#include "psm_suite.h"              /* on the suite disc: back to the menu from the pages (A + B + X + Y), or after them in a chained run */
static vc_run_t vc_run;
static vc_frame_t vc_f;
static uint64_t vc_w0[4];
static float vc_x0, vc_z0, vc_yaw0;
static volatile uint32_t vc_evt[VC_EVT_N];
static asic_evt_handler_entry_t vc_evt_prev[VC_EVT_N];
#define VC_T(k) (vc_f.m[k] = timer_us_gettime64())
#define VC_RDY1(us) (vc_f.rdy1_us = (us))
#else
#define VC_T(k) ((void)0)
#define VC_RDY1(us) ((void)0)
#include "psm_suite.h"              /* on the suite disc: back to the menu with A + B + X + Y held */
static inline void suite_pages_unused_(void) { (void)suite_tick; }   /* the live build has no result pages */
#endif

KOS_INIT_FLAGS(INIT_DEFAULT);

#define MAP_MAX PSM_MAP_STATIC     /* the static maps' side: route B's RAM, route A's target (PSM_MAP_MAX_A); 512 is allocated */
#define TEXT_W PSM_TEXT_W          /* the text texture: sixteen lines of the 8 x 16 font (ten used), 256 KB */
#define TEXT_H PSM_TEXT_H
#define INSET 128u                 /* the map inset, texels and pixels */
#define HUD_EVERY 20u              /* frames between looks at the text and the inset */
#define STICK_DEAD 12

static psm_scene_t scene;
static psm_state_t state;
static psm_frame_t frame;
static psm_cost_t cost;
static psm_walk_limits_t walk_lim;

static uint16_t depth_map[MAP_MAX * MAP_MAX] __attribute__((aligned(32)));    /* route B's map: 128 KB of RAM at 256 */
static uint16_t text_px[TEXT_W * TEXT_H] __attribute__((aligned(32)));
static uint16_t inset_px[INSET * INSET] __attribute__((aligned(32)));
static uint16_t checker_px[PSM_GROUND_N * PSM_GROUND_N] __attribute__((aligned(32)));   /* the ground: 8 KB */
static uint16_t depth_map2[MAP_MAX * MAP_MAX] __attribute__((aligned(32)));   /* route AB's second slot: the same again (32 KB at 128, 128 KB at 256) */
static psm_mapsrc_t mapsrc;               /* route AB (stub): the map source hook, the SH-4's raster standing in for the PVR, one frame late */
static psm_ab_stub_t ab_stub;
static psm_mapbuf_t mapbuf;               /* MAP SIZE 512 on routes B and AB: its buffers while it is chosen */
static int filter_a_hdr = -1;             /* the A FILTER the target headers were compiled with */
static char ln_now[PSM_HUD_LINES][PSM_HUD_COLS + 1u];     /* the lines this frame, and the lines on the texture */
static char ln_prev[PSM_HUD_LINES][PSM_HUD_COLS + 1u];

static pvr_ptr_t target_mem, text_mem, inset_mem, checker_mem, sky_mem;
static unsigned target_size;              /* 0: no target (never made, or no video memory) */
static pvr_poly_hdr_t hdr_sky, hdr_skycol, hdr_ground, hdr_model, hdr_ground_fog, hdr_model_fog, hdr_flat, hdr_layer, hdr_text, hdr_dim, hdr_inset_a, hdr_inset_b;
static int hdr_target_ok;                 /* hdr_layer and hdr_inset_a are compiled for the current target */
static unsigned ta_v;                     /* vertices sent this frame (the map pass and the scene), counted where they are sent */
static unsigned map_tris, layer_v;        /* route A: triangles into the target, vertices of the ground layer */
static int rtt_refused_shown;
static float fog_dist;                    /* the distance the fog table was made for (0: none yet) */
static float sky_now[3] = { -1.0f, -1.0f, -1.0f }, fog_now[3];   /* the sky and fog colours set on the PVR */
static int fog_atmos = -1;                /* the fog table's shape on the PVR: 0 linear, 1 quadratic (-1: none yet) */
static psm_sky_out_t sky;                 /* this frame's sky band, cap and hills, in send order */

static unsigned frame_no;                 /* frames sent since boot: the heartbeat */
static psm_fault_crumbs_t fault_crumbs;   /* psm_fault.h: what the frame was doing, on the report if it faults or hangs */
static uint32_t vbuf_used, vbuf_max;      /* KallistiOS's vertex buffer bytes: the last frame registered, the largest */
static int look_pending;                  /* the fog colour or table changed: written once the PVR is idle (look_apply) */
static unsigned shed_now;                 /* SHED this frame (psm_send_plan): 0 everything sent */
static uint32_t est_now;                  /* this frame's parameter bytes, estimated (psm_send.h) */
#ifndef PSM_PLAN_LAZY
#define PSM_PLAN_LAZY 0                   /* the walled square's build: 1 */
#endif
#ifndef PSM_PLACE_DEFAULT
#define PSM_PLACE_DEFAULT 0u              /* the walled square's build: 7, all three (f7: -3.6 ms still, -11.8 ms turning, vc-c2) */
#endif
static int plan_lazy = PSM_PLAN_LAZY;     /* 1: no dry count: SHED from the frame before's own send (psm_send_plan_next). 0, the default: psm_send_plan */
static uint32_t est_last;                 /* plan_lazy: the last frame's own bytes */

/* ---- what is sent (psm_send.h walks it; the host test walks the same) ------- */

/* TA SEND (a menu row). Runs: each header and vertex is one 32-byte record in ta_run, its cache line taken with MOVCA (no
   read of RAM), and a full run (or the list's end, ta_flush) goes to the TA as one store-queue copy (pvr_prim: sq_fast_cpy
   to the TA input). The lines are then invalidated, so the written records are never written back to RAM. Off: one
   pvr_dr store-queue write a vertex, as before. The measured rates (demo/psm/bench/NOTES.md h1): 144 ns a vertex in one
   store-queue run, about 258 ns a vertex one at a time. */
#define TA_RUN 256u                        /* records a run: 8 KB, half the operand cache */
static uint32_t ta_run[TA_RUN][8] __attribute__((aligned(32)));
static unsigned ta_run_n;
static int ta_runs;                        /* state.send_runs for this frame's lists */

static void ta_flush(void) {
    if (!ta_run_n) return;
    pvr_prim(ta_run, ta_run_n * 32u);
    dcache_inval_range((uintptr_t)ta_run, ta_run_n * 32u);   /* sent: drop the dirty lines, no write-back */
    ta_run_n = 0;
}

static inline uint32_t *ta_record(void) {
    uint32_t *o;

    if (ta_run_n == TA_RUN) ta_flush();
    o = ta_run[ta_run_n++];
    shz_dcache_alloc_line(o);              /* MOVCA: the line is ours, nothing read from RAM */
    return o;
}

static void send_hdr_(int kind);
#define PSM_SINK_HDR(kind) send_hdr_(kind)
#define PSM_SINK_VTX(eol, X, Y, Z, U, V, ARGB, OARGB) do {         \
        pvr_vertex_t *o_ = ta_runs ? (pvr_vertex_t *)ta_record() : (pvr_vertex_t *)pvr_dr_target(); \
        o_->flags = (eol) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;    \
        o_->x = (X); o_->y = (Y); o_->z = (Z);                     \
        o_->u = (U); o_->v = (V);                                  \
        o_->argb = (ARGB); o_->oargb = (OARGB);                    \
        if (!ta_runs) pvr_dr_commit(o_);                           \
    } while (0)
#include "psm_send.h"

static const pvr_poly_hdr_t *const hdr_tab[PSM_HK_N] = {
    &hdr_sky, &hdr_skycol, &hdr_ground, &hdr_ground_fog, &hdr_layer, &hdr_model, &hdr_model_fog, &hdr_flat,
    &hdr_text, &hdr_dim, &hdr_inset_a, &hdr_inset_b
};
static psm_lazy_t lazy;                   /* plan_lazy: the level the last frame's send asked for */
static psm_sink_t sink;

static void send_hdr_(int kind) {
    if (ta_runs) {
        const uint32_t *h = (const uint32_t *)hdr_tab[kind];
        uint32_t *o = ta_record();

        o[0] = h[0]; o[1] = h[1]; o[2] = h[2]; o[3] = h[3]; o[4] = h[4]; o[5] = h[5]; o[6] = h[6]; o[7] = h[7];
    } else pvr_prim((void *)hdr_tab[kind], sizeof(pvr_poly_hdr_t));
}

/* The headers the lists can take this frame: a header that was never compiled is "end of list" to the tile accelerator, so
   its part is not sent (psm_sink_hdr). */
static void hdr_ok_fill(uint8_t ok[PSM_HK_N]) {
    unsigned k;

    for (k = 0; k < PSM_HK_N; k++) ok[k] = (uint8_t)sh4der_poly_hdr_ok((const uint32_t *)hdr_tab[k]);
    if (!sky_mem) ok[PSM_HK_SKY] = 0;
}

/* ---- load ------------------------------------------------------------------ */

static void make_headers(void) {
    pvr_poly_cxt_t c;

    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, PSM_GROUND_N, PSM_GROUND_N, checker_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.shading = PVR_SHADE_GOURAUD;
    c.txr.env = PVR_TXRENV_MODULATE;
    pvr_poly_compile(&hdr_ground, &c);
    c.gen.fog_type = PVR_FOG_TABLE;                          /* DRAW DISTANCE: the same, faded by the fog table */
    pvr_poly_compile(&hdr_ground_fog, &c);

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.shading = PVR_SHADE_GOURAUD;
    pvr_poly_compile(&hdr_model, &c);
    c.gen.fog_type = PVR_FOG_TABLE;
    pvr_poly_compile(&hdr_model_fog, &c);

    if (sky_mem) {                                          /* the sky band: its texture times the vertex colour (the warm side), */
        pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_TWIDDLED, PSM_SKY_W, PSM_SKY_H, sky_mem, PVR_FILTER_BILINEAR);
        c.gen.culling = PVR_CULLING_NONE;                    /* its foot in vertex fog (the offset colour's alpha) */
        c.gen.shading = PVR_SHADE_GOURAUD;
        c.gen.fog_type = PVR_FOG_VERTEX;
        c.gen.specular = 1;
        c.txr.env = PVR_TXRENV_MODULATE;
        c.txr.uv_clamp = PVR_UVCLAMP_V;                     /* u goes round, v stops at the band's edges */
        pvr_poly_compile(&hdr_sky, &c);
    }
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                 /* the cap and the hills: vertex colour, no fog */
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.shading = PVR_SHADE_GOURAUD;
    pvr_poly_compile(&hdr_skycol, &c);

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                 /* the map pass: flat, no culling (a caster is seen from either side) */
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.shading = PVR_SHADE_FLAT;
    pvr_poly_compile(&hdr_flat, &c);

    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, TEXT_W, TEXT_H, text_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&hdr_text, &c);

    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                 /* the dark strip under the text */
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&hdr_dim, &c);

    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, INSET, INSET, inset_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ZERO;
    pvr_poly_compile(&hdr_inset_b, &c);
}

/* The target's two headers: the ground layer (target x what is on screen) and the inset (the target as it is). */
static void make_target_headers(void) {
    pvr_poly_cxt_t c;

    hdr_target_ok = 0;
    memset(&hdr_layer, 0, sizeof hdr_layer);
    memset(&hdr_inset_a, 0, sizeof hdr_inset_a);
    if (!target_mem) return;
    filter_a_hdr = state.filter_a;                          /* A FILTER: the PVR's bilinear or point on the lookup and the inset */
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)target_size, (int)target_size, target_mem,
                     state.filter_a ? PVR_FILTER_BILINEAR : PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.shading = PVR_SHADE_GOURAUD;
    c.txr.env = PVR_TXRENV_MODULATE;
    c.txr.uv_clamp = PVR_UVCLAMP_UV;                        /* outside the map is its white border: lit */
    c.blend.src = PVR_BLEND_DESTCOLOR; c.blend.dst = PVR_BLEND_ZERO;   /* the screen times the target */
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;               /* the layer has the ground's own depth */
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&hdr_layer, &c);
    c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ZERO;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    pvr_poly_compile(&hdr_inset_a, &c);
    hdr_target_ok = 1;
}

/* The render target at `size`: made once a size, after the PVR has finished with the old one. */
static void target_fit(unsigned size) {
    if (target_mem && target_size == size) return;
    pvr_wait_ready();
    pvr_wait_render_done();
    if (target_mem) pvr_mem_free(target_mem);
    target_mem = pvr_mem_malloc(size * size * 2u);
    target_size = target_mem ? size : 0u;
    if (!target_mem) printf("psm: no video memory for a %u x %u target\n", size, size);
    make_target_headers();
}

/* The ground's 64 x 64 texture: the field's checker (squares of 32 texels, 1 m squares, u and v are metres / 2), or the
   square's paving slabs (psm_ground.h; a repeat 3 m, a third of a repeat a metre). Call when the PVR is not reading it. */
static void ground_texture(void) {
    unsigned i;

    if (scene.ground_tex) psm_ground_slabs(checker_px, 158, 152, 142, 1u);
    else for (i = 0; i < PSM_GROUND_N * PSM_GROUND_N; i++) checker_px[i] = (((i >> 5) ^ (i >> 11)) & 1u) ? 0xffffu : 0xc618u;
    pvr_txr_load(checker_px, checker_mem, sizeof checker_px);
}

/* The background colour, set when it changes (KallistiOS keeps it and puts it in the background polygon when a render
   starts: safe at any time). The fog colour and the fog table's ramp are PVR registers the render reads, so a change is only
   noted here (look_pending) and written by look_apply once the PVR is idle. With the air on, both colours are the band's
   horizon fog (psm_sky_build ran this frame) and the ramp is quadratic from 0.43 D; off, the scene's table (psm_look_at)
   and the linear ramp from 0.75 D. The vertex fog of the band's foot takes the same colour. */
static float fog_want[3];

static void look_update(void) {
    float bg[3], fog[3];
    const int atmos = state.atmos && sky_mem;

    psm_look_at(&scene, state.clock, bg, fog);
    if (atmos) { memcpy(bg, sky.fog, sizeof bg); memcpy(fog, sky.fog, sizeof fog); }
    if (sh4der_abs_(bg[0] - sky_now[0]) + sh4der_abs_(bg[1] - sky_now[1]) + sh4der_abs_(bg[2] - sky_now[2]) > 1.0f / 512.0f) {
        pvr_set_bg_color(bg[0], bg[1], bg[2]);
        memcpy(sky_now, bg, sizeof sky_now);
    }
    memcpy(fog_want, fog, sizeof fog_want);
    if (sh4der_abs_(fog[0] - fog_now[0]) + sh4der_abs_(fog[1] - fog_now[1]) + sh4der_abs_(fog[2] - fog_now[2]) > 1.0f / 512.0f) look_pending = 1;
    if (frame.dist > 0.0f && (frame.dist != fog_dist || atmos != fog_atmos)) look_pending = 1;
}

/* The fog registers, written with no render running: call straight after pvr_wait_render_done. */
static void look_apply(void) {
    const int atmos = state.atmos && sky_mem;

    if (!look_pending) return;
    pvr_fog_table_color(1.0f, fog_want[0], fog_want[1], fog_want[2]);
    pvr_fog_vertex_color(fog_want[0], fog_want[1], fog_want[2]);
    memcpy(fog_now, fog_want, sizeof fog_now);
    if (frame.dist > 0.0f && (frame.dist != fog_dist || atmos != fog_atmos)) {   /* DRAW DISTANCE: the ramp, made again on a change */
        if (atmos) {
            float tbl[129];

            psm_fog_table(frame.dist, tbl);
            pvr_fog_far_depth(frame.dist);
            pvr_fog_table_custom(tbl);
        } else
            pvr_fog_table_linear(frame.dist * PSM_DIST_FADE, frame.dist);
        fog_dist = frame.dist; fog_atmos = atmos;
    }
    look_pending = 0;
}

/* The baked occlusion (psm_ao.h): once a scene, at load, on the SH-4; timed for the AO row. The beat keeps the fault
   catcher's watchdog quiet over the seconds it takes. */
static psm_ao_t ao;
static void ao_tick(void) { psm_fault_beat(&fault_crumbs); }
static void ao_bake(void) {
    const uint64_t t0 = timer_us_gettime64();
    const int ok = psm_ao_bake(&ao, &scene, ao_tick);

    state.ao_ms = (unsigned)((timer_us_gettime64() - t0) / 1000u);
    state.ao_none = !ok;
    printf("psm: AO bake %s, %u ms, %u rays, %u ray-triangle tests, ground grid %u x %u\n", ok ? "ran" : "DID NOT RUN", state.ao_ms, (unsigned)ao.rays, (unsigned)ao.tests, ao.gw, ao.gh);
}

/* The SCENE row changed: the other scene, its walk, its ground; the frame's cached positions and placement are old. */
static void scene_switch(void) {
    pvr_wait_ready();
    pvr_wait_render_done();
    if (state.scene) psm_scene_init_town(&scene);
    else psm_scene_init(&scene);
    psm_scene_limits(&scene, &walk_lim);
    frame.gpos_ok = 0; frame.place_ok = 0;
    ao_bake();
    ground_texture();
    psm_mapsrc_reset(&mapsrc);
    fog_dist = 0.0f;
}

static int load(void) {
    text_mem = pvr_mem_malloc(sizeof text_px);
    inset_mem = pvr_mem_malloc(sizeof inset_px);
    checker_mem = pvr_mem_malloc(sizeof checker_px);
    if (!text_mem || !inset_mem || !checker_mem) return 0;
    sky_mem = pvr_mem_malloc(PSM_SKY_W * PSM_SKY_H * 2u);  /* 256 KB; none: the town draws without the air */
    if (sky_mem) pvr_txr_load_ex(psm_sky_px, sky_mem, PSM_SKY_W, PSM_SKY_H, PVR_TXRLOAD_16BPP);   /* twiddled on the way */
    else printf("psm: no video memory for the sky band (%u KB)\n", PSM_SKY_W * PSM_SKY_H * 2u / 1024u);
    memset(inset_px, 0, sizeof inset_px);
    pvr_txr_load(inset_px, inset_mem, sizeof inset_px);
    memset(text_px, 0, sizeof text_px);
    pvr_txr_load(text_px, text_mem, sizeof text_px);
    make_headers();
    psm_scene_init_town(&scene);                            /* the image boots into the town, 14:00, the first view */
    psm_scene_limits(&scene, &walk_lim);
    psm_state_town(&state);
    ao_bake();
    frame.place_opt = PSM_PLACE_DEFAULT;                    /* the placement's options (psm_scene.h, PSM_PLACE_*): the square's build 7 */
    ground_texture();
    ab_stub.f = &frame; ab_stub.s = &scene;
    psm_mapsrc_init(&mapsrc, psm_ab_stub_fill, &ab_stub, 1u, depth_map, depth_map2);
    memset(&cost, 0, sizeof cost);
    return 1;
}

/* 32-byte aligned RAM for a 512 map (psm_mapbuf_fit) */
static void *map_alloc(size_t bytes) { return memalign(32, bytes); }

/* MAP SIZE 512 on route B or AB: its buffers taken or given back after the pad; route AB's slots pointed at them (its maps
   start again). A refused allocation steps the size back and the readout says so (psm_mapbuf_fit). */
static void map_buffers(void) {
    if (!psm_mapbuf_fit(&mapbuf, &state, map_alloc, free)) return;
    if (mapbuf.n == 0u && state.size_refused_kb) printf("psm: no RAM for a %u x %u map (%u KB), size stepped back\n", PSM_MAP_MAX_B, PSM_MAP_MAX_B, state.size_refused_kb);
    psm_mapsrc_init(&mapsrc, psm_ab_stub_fill, &ab_stub, 1u, psm_mapbuf_depth(&mapbuf, 0u, depth_map), psm_mapbuf_depth(&mapbuf, 1u, depth_map2));
}

/* ---- the pad ---------------------------------------------------------------- */

/* Returns 1 when a button went down or up this frame (the text is looked at then, so the menu answers at once). */
static int pad(float dt) {
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
    psm_pad_t in = { 0u, 0.0f, 0.0f, 0.0f, 0.0f };
    unsigned held = 0;
    const unsigned before = state.last;
    int moved = 0;

    if (st) {
        const uint32_t b = st->buttons;

        if (suite_present() && (b & SUITE_KEYS_ALL) == SUITE_KEYS_ALL) suite_exit(SUITE_KEYS);   /* the suite disc: back to the menu */
        if (b & CONT_A) held |= PSM_BTN_A;
        if (b & CONT_X) held |= PSM_BTN_X;
        if (b & CONT_Y) held |= PSM_BTN_Y;
        if (b & CONT_B) held |= PSM_BTN_B;
        if (b & CONT_START) held |= PSM_BTN_START;
        if (b & CONT_DPAD_UP) held |= PSM_BTN_UP;
        if (b & CONT_DPAD_DOWN) held |= PSM_BTN_DOWN;
        if (b & CONT_DPAD_LEFT) held |= PSM_BTN_LEFT;
        if (b & CONT_DPAD_RIGHT) held |= PSM_BTN_RIGHT;
        if (st->ltrig > 160) held |= PSM_BTN_L;
        if (st->rtrig > 160) held |= PSM_BTN_R;
        if (st->joyx > STICK_DEAD || st->joyx < -STICK_DEAD) in.sx = (float)st->joyx * (1.0f / 128.0f);
        if (st->joyy > STICK_DEAD || st->joyy < -STICK_DEAD) in.sy = (float)st->joyy * (1.0f / 128.0f);
        in.lt = (float)st->ltrig * (1.0f / 255.0f);
        in.rt = (float)st->rtrig * (1.0f / 255.0f);
    }
    in.held = held;
    psm_fault_frame(&fault_crumbs, frame_no, held, (unsigned)state.scene, (unsigned)state.route);
    psm_fault_stage(&fault_crumbs, "input");
    {
        const int route_before = state.route, grp_before = state.grp, row_before = state.row, menu_before = state.menu;
        const int32_t value_before = psm_row_int(&state, state.row);

        psm_input(&state, &walk_lim, &in, dt);                                   /* the route goes A, B, AB itself */
        if (state.route != route_before) psm_mapsrc_reset(&mapsrc);          /* a route change: route AB starts with no map */
        moved = state.menu != menu_before || state.grp != grp_before || state.row != row_before
                      || psm_row_int(&state, state.row) != value_before;      /* a held repeat changes it with no new button */
    }
    psm_fault_menu(&fault_crumbs, state.menu, (unsigned)state.row, psm_row_int(&state, state.row));
    psm_fault_camera(&fault_crumbs, state.x, PSM_EYE_H, state.z, state.yaw * 57.29578f, -state.pitch * 57.29578f);
    return held != before || moved;
}

/* ---- route A's map pass -------------------------------------------------------- */

/* The casters into the target (psm_send_map: white, the casters in the shadow's colour, a white frame one texel wide), its own
   scene. Returns 1 when the pass ran. */
static int map_pass(void) {
    static sh4der_psm_pvr_vtx_t out[SH4DER_PSM_PVR_MAX_TRIS][3];
    uint8_t ok[PSM_HK_N];

    map_tris = 0;
    if (!target_mem || !hdr_target_ok) return 0;
    psm_frame_a_casters(&frame, &scene);     /* the casters' map transform, before the scene opens (XMTRX, not the store queue) */
    if (pvr_scene_begin_rtt(target_mem, target_size, target_size, target_size) < 0) {
        if (!rtt_refused_shown) { rtt_refused_shown = 1; printf("psm: pvr_scene_begin_rtt refused a %u target\n", target_size); }
        return 0;
    }
    hdr_ok_fill(ok);
    psm_sink_reset(&sink, 1, 0, 0u, NULL);
    memcpy(sink.hdr_ok, ok, sizeof ok);
    ta_runs = state.send_runs;
    pvr_list_begin(PVR_LIST_OP_POLY);
    map_tris = psm_send_map(&sink, &frame, &scene, &state, target_size, out);
    ta_flush();
    pvr_list_finish();
    pvr_scene_finish();
    ta_v += sink.verts;
    return 1;
}

/* ---- text and inset ------------------------------------------------------------ */

/* The lines into ln_now (psm_hud, the panel when open). 1 when they differ from the lines on the texture: only then is a wait for the PVR
   and an upload worth it. */
static int text_pending(void) {
    if (state.route == PSM_ROUTE_AB) psm_ab_hud(&state, &frame, &cost, &mapsrc, ln_now);
    else psm_hud(&state, &frame, &cost, ln_now);
    if (state.menu) {                                       /* the settings panel's lines over the readout's */
        psm_panel_t p;

        psm_panel_layout(&state, &p);
        psm_panel_lines(&state, &p, ln_now);
    }
    {
        const psm_diag_t d = { frame_no, est_now, vbuf_used, vbuf_max, PSM_TA_VBUF, shed_now, (unsigned)plan_lazy };

        psm_diag_line(&d, ln_now);
    }
    return memcmp(ln_prev, ln_now, sizeof ln_prev) != 0;
}

/* The lines that changed into the text texture, a line's 16 rows (16 KB) at a time. Call after the PVR's last render is done. */
static void text_upload(void) {
    unsigned i, k;

    for (i = 0; i < PSM_HUD_LINES; i++) {
        uint16_t *row = text_px + i * 16u * TEXT_W;

        if (!memcmp(ln_prev[i], ln_now[i], sizeof ln_now[i])) continue;
        memset(row, 0, 16u * TEXT_W * sizeof *row);
        minifont_draw_str(row, TEXT_W, ln_now[i]);
        for (k = 0; k < 16u * TEXT_W; k++) if (row[k]) row[k] = 0xffffu;   /* ARGB1555: opaque white */
        pvr_txr_load(row, (uint8_t *)text_mem + i * 16u * TEXT_W * sizeof *row, 16u * TEXT_W * sizeof *row);
    }
    memcpy(ln_prev, ln_now, sizeof ln_prev);
}

/* Route B's inset (and route AB's): the depth map the compare read, as a grey ramp, 128 x 128. Call after the PVR's last
   render is done. */
static void inset_update(void) {
    const unsigned n = frame.raster.size;      /* the map compared against: at route AB the one of a frame ago */
    unsigned cx[INSET], ry[INSET], x, y;

    for (x = 0; x < INSET; x++) cx[x] = x * n / INSET;          /* the map's column of each inset column */
    for (y = 0; y < INSET; y++) ry[y] = (y * n / INSET) * n;    /* and its row, as an offset into the depth map */
    for (y = 0; y < INSET; y++)
        for (x = 0; x < INSET; x++) {
            const unsigned d = frame.raster.depth[ry[y] + cx[x]];
            const unsigned g = d ? 48u + (d >> 9) + (d >> 10) : 0u;           /* empty is black, a caster 48 .. 143 .. */
            const unsigned c = g > 255u ? 255u : g;

            inset_px[y * INSET + x] = (uint16_t)(((c >> 3) << 11) | ((c >> 2) << 5) | (c >> 3));
        }
    pvr_txr_load(inset_px, inset_mem, sizeof inset_px);
}

/* ---- the PVR's time ------------------------------------------------------------ */

/* The PVR's render time of the scene just finished. pvr_get_stats reports the last render to finish, and a render is
   queued behind the one before it, so a read straight after pvr_scene_finish is some earlier render. The sampled frame
   waits for the TA (its render has started) and then for the render, and reads its own. */
static uint64_t render_wait(void) {
    pvr_stats_t stats;

    pvr_wait_ready();
    pvr_wait_render_done();
    if (pvr_get_stats(&stats) < 0) return 0;
    return stats.rnd_last_time;
}

#ifdef PSM_VIEWCOST
/* ---- the view cost test (psm_viewcost.h): the pad only ends it; then the result pages ------------------------------- */

/* The TA's error events counted (KallistiOS hooks none in a normal build): the handler counts, then calls what was there.
   The form the town's autorun images ran with (patches/auto.diff). */
static void vc_evt_hnd(uint32_t code, void *data) {
    const unsigned i = (unsigned)(code - ASIC_EVT_PVR_ISP_OUTOFMEM);

    (void)data;
    if (i < VC_EVT_N) {
        vc_evt[i]++;
        if (vc_evt_prev[i].hdl) vc_evt_prev[i].hdl(code, vc_evt_prev[i].data);
    }
}

static void vc_hook(int on) {
    unsigned i;

    for (i = 0; i < VC_EVT_N; i++) {
        const uint16_t code = (uint16_t)(ASIC_EVT_PVR_ISP_OUTOFMEM + i);

        if (on) { vc_evt_prev[i] = asic_evt_set_handler(code, vc_evt_hnd, NULL); asic_evt_enable(code, ASIC_IRQ_DEFAULT); }
        else asic_evt_set_handler(code, vc_evt_prev[i].hdl, vc_evt_prev[i].data);
    }
}

/* 1 when a button or a trigger went down (the stick does not count; what is held at the start does not either) */
static int vc_pad_hit(void) {
    static unsigned held = ~0u;
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
    const unsigned now = st ? (st->buttons | (st->ltrig > 160 ? 0x10000u : 0u) | (st->rtrig > 160 ? 0x20000u : 0u)) : 0u;
    const unsigned hit = now & ~held;

    held = now;
    return hit != 0u;
}

/* The pages: the numbers, then a graph a route. A / B turn the page; with no controller the pages turn every 20 s. */
static void vc_pages(void) {
    unsigned page = 0, held = ~0u, pages, r, g, k;
    uint64_t turned;
    char nm[12];

    pvr_wait_ready();
    pvr_wait_render_done();
    vc_hook(0);
    pvr_shutdown();
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    vc_report(&vc_run, pg_say, pg_break, psm_map_size(&state), state.warp, state.movers_only, (unsigned)psm_time_speed[state.tspeed], state.readout,
              VC_VBUF / 1024u);
    for (r = 0; r < 2u; r++) {
        pg_plot_t *p;

        for (g = 0, k = 0; g < VC_STAGES; g++) k += vc_script[g].route == (int)r;
        if (!k) continue;                    /* no stage of this route in this part of the script */
        p = pg_plot(r ? "route B: mean ms a stage" : "route A: mean ms a stage", "stage", "ms", 1);
        for (g = 0; g < VC_STAGES; g++)
            if (vc_script[g].route == (int)r) { snprintf(nm, sizeof nm, "%.11s", vc_script[g].name + 2); pg_cat(p, nm); }
        pg_series(p, "frame");
        for (g = 0, k = 0; g < VC_STAGES; g++) if (vc_script[g].route == (int)r) pg_point(p, (float)k++, vc_mean(&vc_run.st[g][VC_FRAME]) * 1.0e-3f);
        pg_series(p, "p0..p3");
        for (g = 0, k = 0; g < VC_STAGES; g++)
            if (vc_script[g].route == (int)r)
                pg_point(p, (float)k++, (vc_mean(&vc_run.st[g][VC_P0]) + vc_mean(&vc_run.st[g][VC_P1]) + vc_mean(&vc_run.st[g][VC_P2]) + vc_mean(&vc_run.st[g][VC_P3])) * 1.0e-3f);
        pg_series(p, "col+plan+sends");
        for (g = 0, k = 0; g < VC_STAGES; g++)
            if (vc_script[g].route == (int)r)
                pg_point(p, (float)k++, (vc_mean(&vc_run.st[g][VC_COLOUR]) + vc_mean(&vc_run.st[g][VC_PLAN]) + vc_mean(&vc_run.st[g][VC_OP]) + vc_mean(&vc_run.st[g][VC_TR])) * 1.0e-3f);
        pg_series(p, "rdy+fin");
        for (g = 0, k = 0; g < VC_STAGES; g++)
            if (vc_script[g].route == (int)r)
                pg_point(p, (float)k++, (vc_mean(&vc_run.st[g][VC_RDY1]) + vc_mean(&vc_run.st[g][VC_RDY2]) + vc_mean(&vc_run.st[g][VC_FIN])) * 1.0e-3f);
    }
    turned = timer_ms_gettime64();
    pg_draw(page, "TOWN VIEW COST", suite_keys_text("A B turn"));
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;
        int redraw = 0;

        psm_fault_beat(&fault_crumbs);
        held = now;
        pages = pg_pages();
        if (hit & CONT_A) { page = (page + 1u) % pages; redraw = 1; }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; redraw = 1; }
        else if (suite_tick(now, page, pages)) { page++; redraw = 1; }      /* does not come back when it goes to the menu */
        else if (!suite_present() && !st && timer_ms_gettime64() - turned >= 20000u) { page = (page + 1u) % pages; redraw = 1; }
        if (redraw) { pg_draw(page, "TOWN VIEW COST", suite_keys_text("A B turn")); turned = timer_ms_gettime64(); }
        thd_sleep(20);
    }
}
#endif /* PSM_VIEWCOST */

/* ---- main ---------------------------------------------------------------------- */

int main(void) {
    pvr_init_params_t init = pvr_default_params;
    uint64_t previous, win_t0, win_us[4] = { 0, 0, 0, 0 }, win_rnd_ns = 0;
    unsigned win_frames = 0, hud_tick = HUD_EVERY;
    size_t vram0;

    /* PSM_TA_VBUF bytes of the TA's parameters a frame: psm_send_plan counts each frame against it before sending and sheds
       far parts past PSM_TA_SHARE (psm_send.h); the host test holds the worst view under it. KallistiOS makes two of them
       (one a frame, pvr_buffers.c) and puts the background polygon after the last vertex. The tile bins: KallistiOS's
       default 16-word blocks for the two lists, PSM_OPB_EXTRA times that again as the overflow pool */
    init.vertex_buf_size = PSM_TA_VBUF;
    suite_init();                            /* the suite disc: the menu's binary into RAM, before anything else */
#ifdef PSM_VIEWCOST
    init.vertex_buf_size = VC_VBUF;          /* this test only: room for route A's boot view, so no frame is measured short of its last vertices */
#endif
    init.opb_overflow_count = PSM_OPB_EXTRA;
    init.autosort_disabled = 1;              /* the translucent list is drawn in the order it is sent: layer, then text */
    if (pvr_init(&init) < 0) { printf("psm: pvr_init FAIL\n"); return 1; }
    psm_fault_init(&fault_crumbs);           /* an exception, a hang or an exit puts a report on the screen (psm_fault.h) */
    vram0 = pvr_mem_available();
    if (!load()) { printf("psm: texture allocation FAIL\n"); return 1; }
#ifdef PSM_VIEWCOST
    vc_start(&vc_run);
    vc_hook(1);
    vc_x0 = state.x; vc_z0 = state.z; vc_yaw0 = state.yaw;      /* every stage starts from the boot pose */
#endif
    /* the rings' vertices are the whole ground lattice; a frame sends the window the view covers (psm_window_half) */
    printf("psm: %u rings, %u ground vertices in the rings (a frame sends its view's window), %u model vertices, %u model triangles,"
           " %u caster triangles in %u objects\n", scene.rows, scene.rows * (PSM_COLS + 1u), scene.mvn, scene.mtn, scene.ctn, scene.con);
    printf("psm: VRAM free %lu KB (after load; %lu after pvr_init), vertex buffers 2 x %u KB, textures %u KB (text %u, inset %u, ground %u,"
           " sky %u%s); route A's target %u KB at %u from the free\n", (unsigned long)(pvr_mem_available() / 1024u), (unsigned long)(vram0 / 1024u),
           (unsigned)(PSM_TA_VBUF / 1024u),
           (unsigned)((sizeof text_px + sizeof inset_px + sizeof checker_px + (sky_mem ? PSM_SKY_W * PSM_SKY_H * 2u : 0u) + 1023u) / 1024u),
           (unsigned)(sizeof text_px / 1024u), (unsigned)(sizeof inset_px / 1024u), (unsigned)((sizeof checker_px + 1023u) / 1024u),
           sky_mem ? PSM_SKY_W * PSM_SKY_H * 2u / 1024u : 0u, PSM_SKY_HAVE ? "" : " stand-in", PSM_MAP_MAX_A * PSM_MAP_MAX_A * 2u / 1024u, PSM_MAP_MAX_A);
    printf("psm: RAM maps %u KB static (route B %u, route AB's second slot %u); MAP SIZE %u takes %u KB a slot from the heap while chosen\n",
           (unsigned)((sizeof depth_map + sizeof depth_map2) / 1024u), (unsigned)(sizeof depth_map / 1024u), (unsigned)(sizeof depth_map2 / 1024u),
           PSM_MAP_MAX_B, PSM_MAP_MAX_B * PSM_MAP_MAX_B * 2u / 1024u);
    previous = win_t0 = timer_us_gettime64();
    for (;;) {
        const uint64_t now = timer_us_gettime64();
        float dt = (float)(now - previous) * 1.0e-6f;
        uint64_t t0, t1, t2, t3, rnd_rtt = 0, rnd_scr = 0;
        int a_drawn = 0, b_ok = 0, want_text = 0, want_inset = 0, pad_edge;
        const int sample = win_frames == 0u;     /* the window's first frame: its render times are read, after a wait */

        previous = now;
        if (dt > 0.1f) dt = 0.1f;
        frame_no++;
        psm_fault_beat(&fault_crumbs);
#ifdef PSM_VIEWCOST
        {                                    /* the script, not the pad: a button only ends the run */
            const int route_before = state.route;
            vc_view_t v;

            if (vc_pad_hit()) { vc_stop(&vc_run); break; }
            vc_apply(&vc_run, &v);
            state.route = v.route; state.inset = v.inset; state.pitch = v.pitch; state.yaw = vc_yaw0 + v.yaw;
            frame.place_opt = v.opt & 7u;    /* the placement's options this stage (psm_scene.h): the placement is made again on a change */
            plan_lazy = (v.opt & 8u) != 0u;  /* and the send's dry count left out */
            frame.colour_opt = (v.opt >> 4) & 3u;   /* and the colours' options (PSM_COLOUR_*) */
            state.x = vc_x0; state.z = vc_z0; state.menu = 0;
            if (state.route != route_before) psm_mapsrc_reset(&mapsrc);
            dt = VC_DT;
            pad_edge = 0;
            (void)pad;
            psm_fault_frame(&fault_crumbs, frame_no, 0u, (unsigned)state.scene, (unsigned)state.route);
            memcpy(vc_w0, win_us, sizeof vc_w0);
            vc_f.rdy1_us = 0; vc_f.rtt_waited = 0;
        }
#else
        pad_edge = pad(dt);
#endif
        if (state.scene != scene.town) scene_switch();     /* the SCENE row */
        map_buffers();                                     /* MAP SIZE 512 on B / AB: its RAM while chosen */
        psm_town_update(&scene, &state, dt);               /* the town's clock, sun, people and hens (nothing in the field) */
        ta_v = 0; layer_v = 0;
        VC_T(VC_M_TOP);

        /* the SH-4's work, in the label's parts. B: build, clear, casters, receivers. A: build, clip (the map pass), uv.
           A wait for the PVR is the PVR's time: it is taken between two parts, in none of them. */
        t0 = timer_us_gettime64();
        psm_fault_stage(&fault_crumbs, "setup");
        psm_frame_setup(&frame, &scene, &state);
        psm_place_geo(&frame, &scene);       /* the screen placement first: route B compares only the model vertices it sends */
        if (state.atmos && sky_mem) psm_sky_build(&sky, &frame, &state);   /* the band, the cap, the hills and the fog colour */
        t1 = timer_us_gettime64();
        win_us[0] += t1 - t0;
        psm_fault_stage(&fault_crumbs, "map");
        if (state.route == PSM_ROUTE_B) {
            b_ok = psm_frame_b_begin(&frame, &scene, psm_mapbuf_depth(&mapbuf, 0u, depth_map));
            t2 = timer_us_gettime64();
            if (b_ok) psm_frame_b_casters(&frame, &scene, state.movers_only);
            t3 = timer_us_gettime64();
            psm_fault_stage(&fault_crumbs, "compare");
            if (b_ok) psm_frame_b_receivers(&frame, &scene, &state);
            else psm_frame_unshadowed(&frame, &scene);
            win_us[1] += t2 - t1; win_us[2] += t3 - t2; win_us[3] += timer_us_gettime64() - t3;
        } else if (state.route == PSM_ROUTE_AB) {
            /* the hook: this frame's fill goes into the slot that is not read, the compare reads the last complete map.
               The fill here is the SH-4's raster (the stand-in), so its time is the casters' part, clear included */
            const sh4der_psm_raster_t *map;

            ab_stub.movers_only = state.movers_only;
            map = psm_frame_ab_fill(&frame, &scene, &mapsrc);
            t3 = timer_us_gettime64();
            b_ok = map != NULL;
            psm_fault_stage(&fault_crumbs, "compare");
            if (b_ok) psm_frame_ab_compare(&frame, &scene, &state, map);
            else psm_frame_unshadowed(&frame, &scene);
            win_us[2] += t3 - t1; win_us[3] += timer_us_gettime64() - t3;
        } else {
            target_fit(frame.size);          /* waits for the PVR, on a size change only */
            if (state.filter_a != filter_a_hdr) make_target_headers();   /* A FILTER changed: the two headers again */
            pvr_wait_ready();
            t2 = timer_us_gettime64();
            VC_RDY1(t2 - t1);
            a_drawn = map_pass();
            if (sample && a_drawn) {         /* the map's own render, once it is done */
                VC_T(VC_M_RTT0);
                rnd_rtt = render_wait();
                VC_T(VC_M_RTT1);
#ifdef PSM_VIEWCOST
                vc_f.rtt_waited = 1;
#endif
            }
            t3 = timer_us_gettime64();
            psm_frame_unshadowed(&frame, &scene);
            psm_frame_a_ground(&frame);
            win_us[1] += t3 - t2; win_us[2] += timer_us_gettime64() - t3;
        }
        VC_T(VC_M_PARTS);
        psm_ao_frame(&frame, state.ao ? &ao : NULL);   /* AO: the bytes for the colours, or none (the row) */
        psm_colour(&frame, &scene);          /* the colours: this frame's, after the lit and the lookup */
        VC_T(VC_M_COLOUR);
        look_update();
        {                                    /* SHED: the frame counted before it is sent; far parts dropped past 90 % */
            uint8_t ok[PSM_HK_N];

            hdr_ok_fill(ok);
            if (plan_lazy) {                 /* the level the last frame's own send asked for; its bytes on the readout */
                shed_now = lazy.shed; est_now = est_last;
                psm_sink_reset(&sink, 0, 0, shed_now, NULL);
                memcpy(sink.hdr_ok, ok, sizeof ok);
            } else {
                shed_now = psm_send_plan(&sink, &frame, &scene, &state, state.atmos && sky_mem ? &sky : NULL, a_drawn, ok, PSM_TA_VBUF);
                est_now = sink.bytes + PSM_TA_BKG;
                memset(&lazy, 0, sizeof lazy); lazy.shed = shed_now; est_last = est_now;
            }
        }

        VC_T(VC_M_PLAN);
        psm_fault_stage(&fault_crumbs, "text");
        pvr_wait_ready();
        VC_T(VC_M_RDY);
        if (++hud_tick >= HUD_EVERY || pad_edge) {   /* the text, the inset and the fog registers: written when they changed, after the last render */
            hud_tick = 0;
            want_text = text_pending();
            want_inset = state.inset && state.route != PSM_ROUTE_A && b_ok;
            if (want_text || want_inset || look_pending) {
                pvr_wait_render_done();
                look_apply();
                if (want_text) text_upload();
                if (want_inset) inset_update();
            }
        }
        VC_T(VC_M_HUD);
        psm_fault_stage(&fault_crumbs, "world");
        {
            const unsigned shed = shed_now;
            uint8_t ok[PSM_HK_N];

            memcpy(ok, sink.hdr_ok, sizeof ok);
            psm_sink_reset(&sink, 1, 0, shed, NULL);
            memcpy(sink.hdr_ok, ok, sizeof ok);
        }
        ta_runs = state.send_runs;
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        psm_send_op(&sink, &frame, &scene, state.atmos && sky_mem ? &sky : NULL);
        ta_flush();
        pvr_list_finish();
        VC_T(VC_M_OP);
        pvr_list_begin(PVR_LIST_TR_POLY);
        layer_v = psm_send_tr(&sink, &frame, &scene, &state, a_drawn, frame_no);
        ta_flush();
        pvr_list_finish();
        VC_T(VC_M_TR);
        psm_fault_stage(&fault_crumbs, "finish");
        pvr_scene_finish();
        VC_T(VC_M_FIN);
        if (plan_lazy) { est_last = sink.bytes + PSM_TA_BKG; psm_send_plan_next(&lazy, sink.bytes, PSM_TA_VBUF); }
        ta_v += sink.verts;
        psm_fault_counts(&fault_crumbs, sink.kv[PSM_HK_GROUND] + sink.kv[PSM_HK_GROUND_FOG] + sink.kv[PSM_HK_LAYER],
                         sink.kv[PSM_HK_MODEL] + sink.kv[PSM_HK_MODEL_FOG], sink.verts - sink.kv[PSM_HK_GROUND] - sink.kv[PSM_HK_GROUND_FOG]
                         - sink.kv[PSM_HK_LAYER] - sink.kv[PSM_HK_MODEL] - sink.kv[PSM_HK_MODEL_FOG], sink.verts - 2u * sink.strips);
        {
            pvr_stats_t vs;                  /* KallistiOS's own reading of the buffer: the last frame registered, the largest */

            if (pvr_get_stats(&vs) == 0) { vbuf_used = (uint32_t)vs.vtx_buffer_used; vbuf_max = (uint32_t)vs.vtx_buffer_used_max; }
        }
        VC_T(VC_M_SCR0);
        if (sample) rnd_scr = render_wait();
        VC_T(VC_M_SCR1);
        win_frames++;
        win_rnd_ns += rnd_rtt + rnd_scr;
#ifdef PSM_VIEWCOST
        {                                    /* this frame's numbers into the script (the counts of marked vertices on sampled frames only) */
            static uint64_t last_end;
            unsigned i;

            for (i = 0; i < 4u; i++) vc_f.part[i] = win_us[i] - vc_w0[i];
            vc_f.sampled = sample;
            vc_f.rnd_rtt_ns = rnd_rtt; vc_f.rnd_scr_ns = rnd_scr;
            vc_f.v_all = ta_v; vc_f.v_map = ta_v - sink.verts;
            vc_f.v_ground = sink.kv[PSM_HK_GROUND] + sink.kv[PSM_HK_GROUND_FOG];
            vc_f.v_layer = sink.kv[PSM_HK_LAYER];
            vc_f.v_model = sink.kv[PSM_HK_MODEL] + sink.kv[PSM_HK_MODEL_FOG];
            vc_f.vbuf_bytes = vbuf_used;
            vc_f.gvn = frame.gvn; vc_f.mvis = 0u; vc_f.blk = frame.mb_culled;
            if (sample) for (i = 0; i < scene.mvn; i++) vc_f.mvis += frame.mvis[i];
            for (i = 0; i < VC_EVT_N; i++) vc_f.evt[i] = vc_evt[i];
            vc_f.last_end = last_end;
            vc_f.end = last_end = timer_us_gettime64();
            vc_record(&vc_run, &vc_f);
        }
#endif

        if (timer_us_gettime64() - win_t0 >= 1000000u) {      /* one second: the label's numbers */
            const float fr = (float)win_frames, secs = (float)(timer_us_gettime64() - win_t0) * 1.0e-6f;
            unsigned i;

            for (i = 0; i < 4u; i++) cost.part_ms[i] = (float)win_us[i] * 1.0e-3f / fr;
            cost.sh4_ms = cost.part_ms[0] + cost.part_ms[1] + cost.part_ms[2] + cost.part_ms[3];
            cost.pvr_ms = (float)win_rnd_ns * 1.0e-6f;   /* the window's one sampled frame: its render time(s), in ms */
            cost.fps = fr / secs;
            cost.ta_v = ta_v;                /* this frame's: the map pass, the scene, the layer and the text */
            cost.vram_kb = ((state.route == PSM_ROUTE_A ? target_size * target_size * 2u : 0u)
                          + (state.inset && state.route != PSM_ROUTE_A ? (unsigned)sizeof inset_px : 0u) + 1023u) / 1024u;
            cost.map_tris = map_tris; cost.layer_v = layer_v;
            cost.have = 1;
            win_us[0] = win_us[1] = win_us[2] = win_us[3] = 0; win_rnd_ns = 0; win_frames = 0;
            win_t0 = timer_us_gettime64();
        }
#ifdef PSM_VIEWCOST
        if (vc_run.done) break;
#endif
    }
#ifdef PSM_VIEWCOST
    vc_pages();
#endif
    return 0;
}

#endif /* _arch_dreamcast */
