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
   held walk forward / back, X / B strafe left / right. D-pad down opens the menu (a row a line: route, map, map size (512 on
   routes B and AB only, its RAM taken while chosen), A filter (the PVR's bilinear or point), B filter (2 x 2 or point),
   bias a and b, warp near, sun azimuth and elevation, inset, readout and the SH-4's split, presets, invert Y); in it the
   face buttons do not walk, D-pad up / down pick a row, left / right and A change it, B or START close it. Quick, menu
   closed: D-pad up the route, a tap of START uniform / PSM, D-pad left / right the sun's azimuth, L / R triggers its
   elevation, START + L / R the warp's near plane. A value held repeats. The meaning is psm_input in psm_scene.h; this file
   reads the pad.

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
static uint16_t checker_px[16 * 16] __attribute__((aligned(32)));
static uint16_t depth_map2[MAP_MAX * MAP_MAX] __attribute__((aligned(32)));   /* route AB's second slot: the same again (32 KB at 128, 128 KB at 256) */
static psm_mapsrc_t mapsrc;               /* route AB (stub): the map source hook, the SH-4's raster standing in for the PVR, one frame late */
static psm_ab_stub_t ab_stub;
static int route3;                        /* the route of three: psm_input knows two */
static psm_mapbuf_t mapbuf;               /* MAP SIZE 512 on routes B and AB: its buffers while it is chosen */
static int filter_a_hdr = -1;             /* the A FILTER the target headers were compiled with */
static char ln_now[PSM_HUD_LINES][PSM_HUD_COLS + 1u];     /* the lines this frame, and the lines on the texture */
static char ln_prev[PSM_HUD_LINES][PSM_HUD_COLS + 1u];

static pvr_ptr_t target_mem, text_mem, inset_mem, checker_mem, sky_mem;
static unsigned target_size;              /* 0: no target (never made, or no video memory) */
static pvr_poly_hdr_t hdr_sky, hdr_skycol, hdr_ground, hdr_model, hdr_ground_fog, hdr_model_fog, hdr_flat, hdr_layer, hdr_text, hdr_dim, hdr_inset_a, hdr_inset_b;
static int hdr_target_ok;                 /* hdr_layer and hdr_inset_a are compiled for the current target */
static unsigned ta_v;                     /* vertices sent this frame, counted where they are sent */
static unsigned map_tris, layer_v;        /* route A: triangles into the target, vertices of the ground layer */
static int rtt_refused_shown;
static float fog_dist;                    /* the distance the fog table was made for (0: none yet) */
static float sky_now[3] = { -1.0f, -1.0f, -1.0f }, fog_now[3];   /* the sky and fog colours set on the PVR */
static int fog_atmos = -1;                /* the fog table's shape on the PVR: 0 linear, 1 quadratic (-1: none yet) */
static psm_sky_out_t sky;                 /* this frame's sky band, cap and hills, in send order */
static unsigned sky_v;                    /* their vertices sent this frame */

/* ---- small helpers --------------------------------------------------------- */

static inline void vert(uint32_t flags, float x, float y, float z, float u, float v, uint32_t argb) {
    pvr_vertex_t *o = pvr_dr_target();

    o->flags = flags;
    o->x = x; o->y = y; o->z = z;
    o->u = u; o->v = v;
    o->argb = argb;
    o->oargb = 0u;
    pvr_dr_commit(o);
    ta_v++;
}

/* A header the list can take, or nothing: a header that was never compiled is "end of list" to the tile accelerator. */
static int header(const pvr_poly_hdr_t *h) {
    if (!sh4der_poly_hdr_ok((const uint32_t *)h)) return 0;
    pvr_prim((void *)h, sizeof *h);
    return 1;
}

static void quad(const pvr_poly_hdr_t *h, float x, float y, float w, float ht, float z, float u0, float v0, float u1, float v1, uint32_t argb) {
    if (!header(h)) return;
    vert(PVR_CMD_VERTEX, x, y, z, u0, v0, argb);
    vert(PVR_CMD_VERTEX, x + w, y, z, u1, v0, argb);
    vert(PVR_CMD_VERTEX, x, y + ht, z, u0, v1, argb);
    vert(PVR_CMD_VERTEX_EOL, x + w, y + ht, z, u1, v1, argb);
}

/* ---- load ------------------------------------------------------------------ */

static void make_headers(void) {
    pvr_poly_cxt_t c;

    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, 16, 16, checker_mem, PVR_FILTER_NEAREST);
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

static uint16_t rgb565(float r, float g, float b) {
    return (uint16_t)(((unsigned)(r * 31.0f + 0.5f) << 11) | ((unsigned)(g * 63.0f + 0.5f) << 5) | (unsigned)(b * 31.0f + 0.5f));
}

/* The ground's 16 x 16 texture, squares of 8 texels: the field's checker (1 m squares, u and v are metres / 2), or the town's
   cobble (two near stones, TOWN_MOCKUP.md; 1.5 m squares at a third of a repeat a metre). Call when the PVR is not reading it. */
static void ground_texture(void) {
    const uint16_t c1 = scene.ground_tex ? rgb565(0.75f, 0.665f, 0.54f) : 0xffffu, c2 = scene.ground_tex ? rgb565(0.725f, 0.64f, 0.52f) : 0xc618u;
    unsigned i;

    for (i = 0; i < 256u; i++) checker_px[i] = (((i >> 3) ^ (i >> 7)) & 1u) ? c1 : c2;
    pvr_txr_load(checker_px, checker_mem, sizeof checker_px);
}

/* The background and the fog colour, set when they change; the fog table's ramp when the draw distance or its shape changes.
   With the air on, both are the band's horizon fog (psm_sky_build ran this frame) and the ramp is quadratic from 0.43 D;
   off, the scene's table (psm_look_at) and the linear ramp from 0.75 D. The vertex fog of the band's foot takes the same colour. */
static void look_update(void) {
    float bg[3], fog[3];
    const int atmos = state.atmos && sky_mem;

    psm_look_at(&scene, state.clock, bg, fog);
    if (atmos) { memcpy(bg, sky.fog, sizeof bg); memcpy(fog, sky.fog, sizeof fog); }
    if (fabsf(bg[0] - sky_now[0]) + fabsf(bg[1] - sky_now[1]) + fabsf(bg[2] - sky_now[2]) > 1.0f / 512.0f) {
        pvr_set_bg_color(bg[0], bg[1], bg[2]);
        memcpy(sky_now, bg, sizeof sky_now);
    }
    if (fabsf(fog[0] - fog_now[0]) + fabsf(fog[1] - fog_now[1]) + fabsf(fog[2] - fog_now[2]) > 1.0f / 512.0f) {
        pvr_fog_table_color(1.0f, fog[0], fog[1], fog[2]);
        pvr_fog_vertex_color(fog[0], fog[1], fog[2]);
        memcpy(fog_now, fog, sizeof fog_now);
    }
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
}

/* sky vertices i0 .. i1 - 1 as psm_sky_build put them (offset colour included: the band's vertex fog) */
static void sky_send(unsigned i0, unsigned i1) {
    unsigned i;

    for (i = i0; i < i1; i++) {
        const psm_skyv_t *v = &sky.v[i];
        pvr_vertex_t *o = pvr_dr_target();

        o->flags = sky.eol[i] ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        o->x = v->x; o->y = v->y; o->z = v->z;
        o->u = v->u; o->v = v->v;
        o->argb = v->argb;
        o->oargb = v->oargb;
        pvr_dr_commit(o);
        ta_v++; sky_v++;
    }
}

/* The sky band, then the cap and the hills, into the opaque list before the ground. */
static void sky_draw(void) {
    sky_v = 0;
    if (!state.atmos || !sky_mem) return;
    if (sky.n_band && header(&hdr_sky)) sky_send(0u, sky.n_band);
    if (sky.n_cap + sky.n_hill && header(&hdr_skycol)) sky_send(sky.n_band, sky.n_band + sky.n_cap + sky.n_hill);
}

/* The SCENE row changed: the other scene, its walk, its ground; the frame's cached positions and placement are old. */
static void scene_switch(void) {
    pvr_wait_ready();
    pvr_wait_render_done();
    if (state.scene) psm_scene_init_town(&scene);
    else psm_scene_init(&scene);
    psm_scene_limits(&scene, &walk_lim);
    frame.gpos_ok = 0; frame.place_ok = 0;
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
    ground_texture();
    ab_stub.f = &frame; ab_stub.s = &scene;
    psm_mapsrc_init(&mapsrc, psm_ab_stub_fill, &ab_stub, 1u, depth_map, depth_map2);
    route3 = state.route;
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

    if (st) {
        const uint32_t b = st->buttons;

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
    {
        const int route_before = state.route;

        psm_input(&state, &walk_lim, &in, dt);
        state.route = psm_ab_route_step(&route3, route_before, state.route);   /* A, B, AB (stub) */
        if (state.route != route_before) psm_mapsrc_reset(&mapsrc);          /* a route change: route AB starts with no map */
    }
    return held != before;
}

/* ---- the frame's vertices ---------------------------------------------------- */

/* The ground as strips, a ring at a time, a strip broken where a cell cannot be sent (psm_cell_ok: the flags psm_place
   made). layer 0: the ground texture, lit. layer 1: route A's multiply layer, the map target's coordinates, white. Returns the
   vertices sent. One header for the run of strips, as sh4der_spark.c and sh4der_bump.h send theirs. */
static unsigned ground(int layer) {
    const unsigned before = ta_v;
    unsigned r, c;

    if (!header(layer ? &hdr_layer : frame.dist > 0.0f ? &hdr_ground_fog : &hdr_ground)) return 0;
    for (r = 0; r + 1u < frame.grows; r++) {
        c = 0;
        while (c + 1u < frame.gcols) {
            unsigned end, k;

            if (!psm_cell_ok(&frame, r, c, layer)) { c++; continue; }
            end = psm_run_end(&frame, r, c, layer);
            for (k = c; k <= end; k++) {                    /* spokes c .. end: the outer ring's vertex, then the inner's */
                const unsigned o = (r + 1u) * frame.gcols + k, i = r * frame.gcols + k;

                if (layer) {
                    vert(PVR_CMD_VERTEX, frame.gscr[o][0], frame.gscr[o][1], frame.gscr[o][2], frame.guv[o][0], frame.guv[o][1], 0xffffffffu);
                    vert(k == end ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX, frame.gscr[i][0], frame.gscr[i][1], frame.gscr[i][2], frame.guv[i][0], frame.guv[i][1], 0xffffffffu);
                } else {
                    vert(PVR_CMD_VERTEX, frame.gscr[o][0], frame.gscr[o][1], frame.gscr[o][2], frame.gpos[o][0] * scene.ground_uv, frame.gpos[o][2] * scene.ground_uv, frame.gcol[o]);
                    vert(k == end ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX, frame.gscr[i][0], frame.gscr[i][1], frame.gscr[i][2], frame.gpos[i][0] * scene.ground_uv, frame.gpos[i][2] * scene.ground_uv, frame.gcol[i]);
                }
            }
            c = end;
        }
    }
    return ta_v - before;
}

/* The models: the runs of strip triangles psm_place passed (in front, with area) as strips, and those across the near plane
   cut there. */
static void models(void) {
    unsigned i, k, end;

    if (!header(frame.dist > 0.0f ? &hdr_model_fog : &hdr_model)) return;
    for (i = 0; i < scene.mtn; i++) {
        const unsigned a = scene.mt[i][0], b = scene.mt[i][1];

        if (frame.mtri[i] == 2u) {
            psm_cv_t cv[4];
            const unsigned n = psm_clip_near(&frame, &scene, i, cv);

            for (k = 0; k < n; k++) vert(k + 1u == n ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX, cv[k].x, cv[k].y, cv[k].iw, 0.0f, 0.0f, cv[k].argb);
            continue;
        }
        if (!frame.mtri[i]) continue;
        end = psm_model_run_end(&frame, &scene, i);         /* i .. end: one strip, the first's corners 0 and 1, then each one's 2 */
        vert(PVR_CMD_VERTEX, frame.mscr[a][0], frame.mscr[a][1], frame.mscr[a][2], 0.0f, 0.0f, frame.mcol[a]);
        vert(PVR_CMD_VERTEX, frame.mscr[b][0], frame.mscr[b][1], frame.mscr[b][2], 0.0f, 0.0f, frame.mcol[b]);
        for (k = i; k <= end; k++) {
            const unsigned e = scene.mt[k][2];

            vert(k == end ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX, frame.mscr[e][0], frame.mscr[e][1], frame.mscr[e][2], 0.0f, 0.0f, frame.mcol[e]);
        }
        i = end;
    }
}

/* ---- route A's map pass -------------------------------------------------------- */

/* The casters into the target: white first (far), each caster triangle in the shadow's colour (nearer; the scene's ambient
   over its full light, so the layer's multiply leaves the ambient), then a white frame one texel wide (nearest), so a lookup
   clamped past the map's edge reads lit. A caster object off the map's square (frame.con) is not clipped or sent. Returns 1
   when the pass ran. */
static int map_pass(void) {
    static sh4der_psm_pvr_vtx_t out[SH4DER_PSM_PVR_MAX_TRIS][3];
    const float n = (float)target_size;
    const uint32_t grey = psm_pack_rgb(frame.shadow_rgb[0], frame.shadow_rgb[1], frame.shadow_rgb[2]), white = 0xffffffffu;
    unsigned i, j, k, t;

    map_tris = 0;
    if (!target_mem || !hdr_target_ok) return 0;
    psm_frame_a_casters(&frame, &scene);     /* the casters' map transform, before the scene opens (XMTRX, not the store queue) */
    if (pvr_scene_begin_rtt(target_mem, target_size, target_size, target_size) < 0) {
        if (!rtt_refused_shown) { rtt_refused_shown = 1; printf("psm: pvr_scene_begin_rtt refused a %u target\n", target_size); }
        return 0;
    }
    pvr_list_begin(PVR_LIST_OP_POLY);
    quad(&hdr_flat, 0.0f, 0.0f, n, n, 0.1f, 0.0f, 0.0f, 0.0f, 0.0f, white);
    if (frame.built && header(&hdr_flat)) {
        for (j = 0; j < scene.con; j++)
            for (i = scene.co[j].t0; i < scene.co[j].t0 + scene.co[j].tn; i++) {
                if (!psm_caster_on(&scene, i, state.movers_only)) continue;     /* CASTERS: movers only leaves the static ones out */
                frame.tris++;
                if (!frame.con[j]) continue;                                  /* the object is off the map's square */
                t = psm_pvr_tris_h(frame.ma[scene.ct[i][0]], frame.ma[scene.ct[i][1]], frame.ma[scene.ct[i][2]], target_size, out);
                for (k = 0; k < t; k++) {
                    const float area = (out[k][1].x - out[k][0].x) * (out[k][2].y - out[k][0].y) - (out[k][2].x - out[k][0].x) * (out[k][1].y - out[k][0].y);

                    if (!(area > 1.0e-3f || area < -1.0e-3f)) continue;       /* no area in the map: not sent */
                    vert(PVR_CMD_VERTEX, out[k][0].x, out[k][0].y, 1.0f, 0.0f, 0.0f, grey);
                    vert(PVR_CMD_VERTEX, out[k][1].x, out[k][1].y, 1.0f, 0.0f, 0.0f, grey);
                    vert(PVR_CMD_VERTEX_EOL, out[k][2].x, out[k][2].y, 1.0f, 0.0f, 0.0f, grey);
                    map_tris++;
                }
            }
    }
    frame.drawn = map_tris;
    quad(&hdr_flat, 0.0f, 0.0f, n, 1.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, white);
    quad(&hdr_flat, 0.0f, n - 1.0f, n, 1.0f, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, white);
    quad(&hdr_flat, 0.0f, 0.0f, 1.0f, n, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, white);
    quad(&hdr_flat, n - 1.0f, 0.0f, 1.0f, n, 2.0f, 0.0f, 0.0f, 0.0f, 0.0f, white);
    pvr_list_finish();
    pvr_scene_finish();
    return 1;
}

/* ---- text and inset ------------------------------------------------------------ */

/* The eight lines into ln_now (psm_hud). 1 when they differ from the lines on the texture: only then is a wait for the PVR
   and an upload worth it. */
static int text_pending(void) {
    if (state.route == PSM_ROUTE_AB) psm_ab_hud(&state, &frame, &cost, &mapsrc, ln_now);
    else psm_hud(&state, &frame, &cost, ln_now);
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

/* One line of the text texture at (x, y) on screen, on a dark strip. */
static void text_line(unsigned line, float x, float y) {
    const float v0 = (float)(line * 16u) * (1.0f / (float)TEXT_H), v1 = (float)(line * 16u + 16u) * (1.0f / (float)TEXT_H);

    quad(&hdr_dim, x - 4.0f, y - 1.0f, (float)TEXT_W + 8.0f, 18.0f, 900.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0x90000000u);
    quad(&hdr_text, x, y, (float)TEXT_W, 16.0f, 1000.0f, 0.0f, v0, 1.0f, v1, 0xffffffffu);
}

static void hud(int a_drawn) {
    const float base = 420.0f;               /* the cost label's line: the split goes under it, clear of a TV's overscan */
    unsigned i;

    text_line(0u, 16.0f, 24.0f);
    text_line(9u, 16.0f, 42.0f);                                                /* the controls: always on */
    if (state.menu) text_line(8u, 16.0f, 64.0f);                               /* the menu's row */
    text_line(1u, 16.0f, base - (state.readout ? 5.0f * 18.0f : 18.0f));        /* texel over pixel: always on */
    if (state.readout) {
        for (i = 2; i < 6u; i++) text_line(i, 16.0f, base - (float)(6u - i) * 18.0f);
        text_line(10u, 16.0f, base - 6.0f * 18.0f);                          /* the draw distance */
    }
    text_line(6u, 16.0f, base);
    if (state.split) text_line(7u, 16.0f, base + 18.0f);
    if (state.inset) {
        const float x = PSM_SCR_W - 16.0f - (float)INSET, y = 290.0f - (float)INSET;

        if (state.route != PSM_ROUTE_A) quad(&hdr_inset_b, x, y, (float)INSET, (float)INSET, 1000.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0xffffffffu);
        else if (a_drawn) quad(&hdr_inset_a, x, y, (float)INSET, (float)INSET, 1000.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0xffffffffu);
    }
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

/* ---- main ---------------------------------------------------------------------- */

int main(void) {
    pvr_init_params_t init = pvr_default_params;
    uint64_t previous, win_t0, win_us[4] = { 0, 0, 0, 0 }, win_rnd_ns = 0;
    unsigned win_frames = 0, hud_tick = HUD_EVERY;
    size_t vram0;

    /* PSM_VTX_BUDGET vertices of 32 B: the host test holds the worst frame (the ground strips and the layer at the widest
       window, the models, the text) under it. KallistiOS sizes two vertex buffers with this value (one a frame,
       pvr_buffers.c), 2 MB of video memory */
    init.vertex_buf_size = PSM_VTX_BUDGET * 32;
    init.opb_overflow_count = 4;
    init.autosort_disabled = 1;              /* the translucent list is drawn in the order it is sent: layer, then text */
    if (pvr_init(&init) < 0) { printf("psm: pvr_init FAIL\n"); return 1; }
    vram0 = pvr_mem_available();
    if (!load()) { printf("psm: texture allocation FAIL\n"); return 1; }
    /* the rings' vertices are the whole ground lattice; a frame sends the window the view covers (psm_window_half) */
    printf("psm: %u rings, %u ground vertices in the rings (a frame sends its view's window), %u model vertices, %u model triangles,"
           " %u caster triangles in %u objects\n", scene.rows, scene.rows * (PSM_COLS + 1u), scene.mvn, scene.mtn, scene.ctn, scene.con);
    printf("psm: VRAM free %lu KB (after load; %lu after pvr_init), vertex buffers 2 x %u KB, textures %u KB (text %u, inset %u, ground %u,"
           " sky %u%s); route A's target %u KB at %u from the free\n", (unsigned long)(pvr_mem_available() / 1024u), (unsigned long)(vram0 / 1024u),
           (unsigned)(PSM_VTX_BUDGET * 32u / 1024u),
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
        pad_edge = pad(dt);
        if (state.scene != scene.town) scene_switch();     /* the SCENE row */
        map_buffers();                                     /* MAP SIZE 512 on B / AB: its RAM while chosen */
        psm_town_update(&scene, &state, dt);               /* the town's clock, sun, people and hens (nothing in the field) */
        ta_v = 0; layer_v = 0;

        /* the SH-4's work, in the label's parts. B: build, clear, casters, receivers. A: build, clip (the map pass), uv.
           A wait for the PVR is the PVR's time: it is taken between two parts, in none of them. */
        t0 = timer_us_gettime64();
        psm_frame_setup(&frame, &scene, &state);
        psm_place_geo(&frame, &scene);       /* the screen placement first: route B compares only the model vertices it sends */
        if (state.atmos && sky_mem) psm_sky_build(&sky, &frame, &state);   /* the band, the cap, the hills and the fog colour */
        t1 = timer_us_gettime64();
        win_us[0] += t1 - t0;
        if (state.route == PSM_ROUTE_B) {
            b_ok = psm_frame_b_begin(&frame, &scene, psm_mapbuf_depth(&mapbuf, 0u, depth_map));
            t2 = timer_us_gettime64();
            if (b_ok) psm_frame_b_casters(&frame, &scene, state.movers_only);
            t3 = timer_us_gettime64();
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
            if (b_ok) psm_frame_ab_compare(&frame, &scene, &state, map);
            else psm_frame_unshadowed(&frame, &scene);
            win_us[2] += t3 - t1; win_us[3] += timer_us_gettime64() - t3;
        } else {
            target_fit(frame.size);          /* waits for the PVR, on a size change only */
            if (state.filter_a != filter_a_hdr) make_target_headers();   /* A FILTER changed: the two headers again */
            pvr_wait_ready();
            t2 = timer_us_gettime64();
            a_drawn = map_pass();
            if (sample && a_drawn) rnd_rtt = render_wait();      /* the map's own render, once it is done */
            t3 = timer_us_gettime64();
            psm_frame_unshadowed(&frame, &scene);
            psm_frame_a_ground(&frame);
            win_us[1] += t3 - t2; win_us[2] += timer_us_gettime64() - t3;
        }
        psm_colour(&frame, &scene);          /* the colours: this frame's, after the lit and the lookup */
        look_update();

        pvr_wait_ready();
        if (++hud_tick >= HUD_EVERY || pad_edge) {   /* the text and the inset: uploaded when they changed, after the last render */
            hud_tick = 0;
            want_text = text_pending();
            want_inset = state.inset && state.route != PSM_ROUTE_A && b_ok;
            if (want_text || want_inset) {
                pvr_wait_render_done();
                if (want_text) text_upload();
                if (want_inset) inset_update();
            }
        }
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        sky_draw();
        ground(0);
        models();
        pvr_list_finish();
        pvr_list_begin(PVR_LIST_TR_POLY);
        if (a_drawn) layer_v = ground(1);
        hud(a_drawn);
        pvr_list_finish();
        pvr_scene_finish();
        if (sample) rnd_scr = render_wait();
        win_frames++;
        win_rnd_ns += rnd_rtt + rnd_scr;

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
    }
    return 0;
}

#endif /* _arch_dreamcast */
