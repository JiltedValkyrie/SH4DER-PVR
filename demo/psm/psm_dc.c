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
   held walk forward / back, X / B strafe left / right. D-pad down opens the menu (a row a line: route, map, map size, PCF,
   bias a and b, warp near, sun azimuth and elevation, inset, readout and the SH-4's split, presets, invert Y); in it the
   face buttons do not walk, D-pad up / down pick a row, left / right and A change it, B or START close it. Quick, menu
   closed: D-pad up the route, a tap of START uniform / PSM, D-pad left / right the sun's azimuth, L / R triggers its
   elevation, START + L / R the warp's near plane. A value held repeats. The meaning is psm_input in psm_scene.h; this file
   reads the pad.

   DRAW DISTANCE (a menu row): the ground cells and model triangles wholly past it in view depth are not sent (psm_place), and
   the PVR's table fog (pvr_fog_table_linear, the sky's colour) fades the last quarter of it, on the ground and the models only
   (not the route A layer, the text or the map pass). INFINITE sends everything through headers without fog, as before. The map's
   casters do not follow it.

   The cost label reads "--" until the console has measured a second. Nothing in it comes from a host.

   KallistiOS paths: pvr_init, pvr_mem_malloc / pvr_mem_free (the target, the textures), pvr_scene_begin_rtt (the
   map pass), pvr_prim + pvr_dr_target / pvr_dr_commit (store queue vertices), pvr_txr_load (text and inset uploads),
   pvr_get_stats after a wait (the PVR's render time, one sampled frame a second: see render_wait), timer_us_gettime64
   (the SH-4's), maple (the pad), minifont (the text). Build: make psm-dc (see the Makefile). Compiles to nothing off
   the Dreamcast. */

#ifdef _arch_dreamcast

#include <kos.h>
#include <dc/minifont.h>
#include "sh4der_guard.h"
#include "psm_scene.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define MAP_MAX 256u               /* the largest map side: route B's RAM, route A's target */
#define TEXT_W PSM_TEXT_W          /* the text texture: sixteen lines of the 8 x 16 font (ten used), 256 KB */
#define TEXT_H PSM_TEXT_H
#define INSET 128u                 /* the map inset, texels and pixels */
#define HUD_EVERY 20u              /* frames between looks at the text and the inset */
#define STICK_DEAD 12

static const float SKY[3] = { 0.55f, 0.68f, 0.85f };

static psm_scene_t scene;
static psm_state_t state;
static psm_frame_t frame;
static psm_cost_t cost;
static psm_walk_limits_t walk_lim;

static uint16_t depth_map[MAP_MAX * MAP_MAX] __attribute__((aligned(32)));    /* route B's map: 128 KB of RAM at 256 */
static uint16_t text_px[TEXT_W * TEXT_H] __attribute__((aligned(32)));
static uint16_t inset_px[INSET * INSET] __attribute__((aligned(32)));
static uint16_t checker_px[16 * 16] __attribute__((aligned(32)));
static char ln_now[PSM_HUD_LINES][PSM_HUD_COLS + 1u];     /* the lines this frame, and the lines on the texture */
static char ln_prev[PSM_HUD_LINES][PSM_HUD_COLS + 1u];

static pvr_ptr_t target_mem, text_mem, inset_mem, checker_mem;
static unsigned target_size;              /* 0: no target (never made, or no video memory) */
static pvr_poly_hdr_t hdr_ground, hdr_model, hdr_ground_fog, hdr_model_fog, hdr_flat, hdr_layer, hdr_text, hdr_dim, hdr_inset_a, hdr_inset_b;
static int hdr_target_ok;                 /* hdr_layer and hdr_inset_a are compiled for the current target */
static unsigned ta_v;                     /* vertices sent this frame, counted where they are sent */
static unsigned map_tris, layer_v;        /* route A: triangles into the target, vertices of the ground layer */
static int rtt_refused_shown;
static float fog_dist;                    /* the distance the fog table was made for (0: none yet) */

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
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)target_size, (int)target_size, target_mem, PVR_FILTER_BILINEAR);
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

static int load(void) {
    unsigned i;

    text_mem = pvr_mem_malloc(sizeof text_px);
    inset_mem = pvr_mem_malloc(sizeof inset_px);
    checker_mem = pvr_mem_malloc(sizeof checker_px);
    if (!text_mem || !inset_mem || !checker_mem) return 0;
    for (i = 0; i < 256u; i++)                              /* 1 m squares: u and v are metres / 2 */
        checker_px[i] = (((i >> 3) ^ (i >> 7)) & 1u) ? 0xffffu : 0xc618u;
    pvr_txr_load(checker_px, checker_mem, sizeof checker_px);
    memset(inset_px, 0, sizeof inset_px);
    pvr_txr_load(inset_px, inset_mem, sizeof inset_px);
    memset(text_px, 0, sizeof text_px);
    pvr_txr_load(text_px, text_mem, sizeof text_px);
    make_headers();
    psm_scene_init(&scene);
    psm_scene_limits(&scene, &walk_lim);
    psm_state_default(&state);
    memset(&cost, 0, sizeof cost);
    return 1;
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
    psm_input(&state, &walk_lim, &in, dt);
    return held != before;
}

/* ---- the frame's vertices ---------------------------------------------------- */

/* The ground as strips, a ring at a time, a strip broken where a cell cannot be sent (psm_cell_ok: the flags psm_place
   made). layer 0: the checker, lit. layer 1: route A's multiply layer, the map target's coordinates, white. Returns the
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
                    vert(PVR_CMD_VERTEX, frame.gscr[o][0], frame.gscr[o][1], frame.gscr[o][2], frame.gpos[o][0] * 0.5f, frame.gpos[o][2] * 0.5f, frame.gcol[o]);
                    vert(k == end ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX, frame.gscr[i][0], frame.gscr[i][1], frame.gscr[i][2], frame.gpos[i][0] * 0.5f, frame.gpos[i][2] * 0.5f, frame.gcol[i]);
                }
            }
            c = end;
        }
    }
    return ta_v - before;
}

/* The models: the triangles psm_place passed (in front, with area), and those across the near plane cut there. */
static void models(void) {
    unsigned i;

    if (!header(frame.dist > 0.0f ? &hdr_model_fog : &hdr_model)) return;
    for (i = 0; i < scene.mtn; i++) {
        const unsigned a = scene.mt[i][0], b = scene.mt[i][1], c = scene.mt[i][2];

        if (frame.mtri[i] == 2u) {
            psm_cv_t cv[4];
            const unsigned n = psm_clip_near(&frame, &scene, i, cv);
            unsigned k;

            for (k = 0; k < n; k++) vert(k + 1u == n ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX, cv[k].x, cv[k].y, cv[k].iw, 0.0f, 0.0f, cv[k].argb);
            continue;
        }
        if (!frame.mtri[i]) continue;
        vert(PVR_CMD_VERTEX, frame.mscr[a][0], frame.mscr[a][1], frame.mscr[a][2], 0.0f, 0.0f, frame.mcol[a]);
        vert(PVR_CMD_VERTEX, frame.mscr[b][0], frame.mscr[b][1], frame.mscr[b][2], 0.0f, 0.0f, frame.mcol[b]);
        vert(PVR_CMD_VERTEX_EOL, frame.mscr[c][0], frame.mscr[c][1], frame.mscr[c][2], 0.0f, 0.0f, frame.mcol[c]);
    }
}

/* ---- route A's map pass -------------------------------------------------------- */

/* The casters into the target: white first (far), each caster triangle in the shadow's grey (nearer), then a white
   frame one texel wide (nearest), so a lookup clamped past the map's edge reads lit. Returns 1 when the pass ran. */
static int map_pass(void) {
    static sh4der_psm_pvr_vtx_t out[SH4DER_PSM_PVR_MAX_TRIS][3];
    const float n = (float)target_size, g = frame.shadow_grey;
    const uint32_t grey = psm_pack_rgb(g, g, g), white = 0xffffffffu;
    unsigned i, k, t;

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
        for (i = 0; i < scene.mtn; i++) {
            if (!psm_caster_on(&scene, i, state.movers_only)) continue;     /* CASTERS: movers only leaves the static ones out */
            frame.tris++;
            t = psm_pvr_tris_h(frame.ma[scene.mt[i][0]], frame.ma[scene.mt[i][1]], frame.ma[scene.mt[i][2]], target_size, out);
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
    psm_hud(&state, &frame, &cost, ln_now);
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

/* Route B's inset: the depth map as a grey ramp, 128 x 128. Call after the PVR's last render is done. */
static void inset_update(void) {
    const unsigned n = frame.size;
    unsigned cx[INSET], ry[INSET], x, y;

    for (x = 0; x < INSET; x++) cx[x] = x * n / INSET;          /* the map's column of each inset column */
    for (y = 0; y < INSET; y++) ry[y] = (y * n / INSET) * n;    /* and its row, as an offset into the depth map */
    for (y = 0; y < INSET; y++)
        for (x = 0; x < INSET; x++) {
            const unsigned d = depth_map[ry[y] + cx[x]];
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

        if (state.route == PSM_ROUTE_B) quad(&hdr_inset_b, x, y, (float)INSET, (float)INSET, 1000.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0xffffffffu);
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

    /* PSM_VTX_BUDGET vertices of 32 B: the host test holds the worst frame (the ground strips and the layer at the widest
       window, the models, the text) under it. KallistiOS sizes two vertex buffers with this value (one a frame,
       pvr_buffers.c), 2 MB of video memory */
    init.vertex_buf_size = PSM_VTX_BUDGET * 32;
    init.opb_overflow_count = 4;
    init.autosort_disabled = 1;              /* the translucent list is drawn in the order it is sent: layer, then text */
    if (pvr_init(&init) < 0) { printf("psm: pvr_init FAIL\n"); return 1; }
    pvr_set_bg_color(SKY[0], SKY[1], SKY[2]);
    if (!load()) { printf("psm: texture allocation FAIL\n"); return 1; }
    printf("psm: %u rings, %u ground vertices a frame, %u model vertices, %u model triangles, VRAM free %lu KB\n", scene.rows,
           scene.rows * (PSM_COLS + 1u), scene.mvn, scene.mtn, (unsigned long)(pvr_mem_available() / 1024u));
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
        ta_v = 0; layer_v = 0;

        /* the SH-4's work, in the label's parts. B: build, clear, casters, receivers. A: build, clip (the map pass), uv.
           A wait for the PVR is the PVR's time: it is taken between two parts, in none of them. */
        t0 = timer_us_gettime64();
        psm_frame_setup(&frame, &scene, &state);
        t1 = timer_us_gettime64();
        win_us[0] += t1 - t0;
        if (state.route == PSM_ROUTE_B) {
            b_ok = psm_frame_b_begin(&frame, &scene, depth_map);
            t2 = timer_us_gettime64();
            if (b_ok) psm_frame_b_casters(&frame, &scene, state.movers_only);
            t3 = timer_us_gettime64();
            if (b_ok) psm_frame_b_receivers(&frame, &scene, &state);
            else psm_frame_unshadowed(&frame, &scene);
            win_us[1] += t2 - t1; win_us[2] += t3 - t2; win_us[3] += timer_us_gettime64() - t3;
        } else {
            target_fit(frame.size);          /* waits for the PVR, on a size change only */
            pvr_wait_ready();
            t2 = timer_us_gettime64();
            a_drawn = map_pass();
            if (sample && a_drawn) rnd_rtt = render_wait();      /* the map's own render, once it is done */
            t3 = timer_us_gettime64();
            psm_frame_unshadowed(&frame, &scene);
            psm_frame_a_ground(&frame);
            win_us[1] += t3 - t2; win_us[2] += timer_us_gettime64() - t3;
        }
        psm_place(&frame, &scene);           /* the screen placement and the colours: this frame's, after the lit and the lookup */
        if (frame.dist > 0.0f && frame.dist != fog_dist) {   /* DRAW DISTANCE: the fog table, made again only when it changes */
            pvr_fog_table_color(1.0f, SKY[0], SKY[1], SKY[2]);
            pvr_fog_table_linear(frame.dist * PSM_DIST_FADE, frame.dist);
            fog_dist = frame.dist;
        }

        pvr_wait_ready();
        if (++hud_tick >= HUD_EVERY || pad_edge) {   /* the text and the inset: uploaded when they changed, after the last render */
            hud_tick = 0;
            want_text = text_pending();
            want_inset = state.inset && state.route == PSM_ROUTE_B && b_ok;
            if (want_text || want_inset) {
                pvr_wait_render_done();
                if (want_text) text_upload();
                if (want_inset) inset_update();
            }
        }
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
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
                          + (state.inset && state.route == PSM_ROUTE_B ? (unsigned)sizeof inset_px : 0u) + 1023u) / 1024u;
            cost.map_tris = map_tris; cost.layer_v = layer_v;
            cost.have = 1;
            win_us[0] = win_us[1] = win_us[2] = win_us[3] = 0; win_rnd_ns = 0; win_frames = 0;
            win_t0 = timer_us_gettime64();
        }
    }
    return 0;
}

#endif /* _arch_dreamcast */
