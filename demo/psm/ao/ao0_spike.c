/* SH4DER-PVR: ao0, the hardware questions of AO method 2 (sky directions into the shadow target), Dreamcast test program

   demo/psm/ao/ao0_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A test program for a real console. The method is ../ao_layer.h and ../AO_METHOD.md: the casters are drawn into the
   shadow map's render target k more times, each slid along another direction of the sky onto the ground, as a faint
   black. It runs by itself (no pad needed) and ends on two result pages.

     TEST 1  THE UNION. The small scene (a house, a lean-to half inside it, a post, a mover: 48 triangles), no sun, into
             a 256 x 256 RGB565 target, read back and compared texel by texel with the model (ao0_model.h), for k = 1,
             2, 4, 8 and three ways of drawing:
               U   depth "greater" with depth write in the translucent list, sent in order: one darkening a direction
               P   plain blending: one darkening a triangle
               C   plain blending, one side of the triangles only (half the vertices)
             c: distinct values / the model's levels; b: texels whose value's rank is not their level's rank (of n
             compared; edge texels are left out); f: 1 = the target's rows come back upside down.
     TEST 2  THE COST. The load scene (97 boxes, 1,164 caster triangles; the town has about 1,162), the sun's shadow
             and the white border as route A draws them, then k directions. 8 frames a row after one that is not
             counted: v vertices of the directions, e the TA bytes estimated (60 a triangle, 76 a rectangle), k
             KallistiOS's vertex buffer figure, ao and sun the SH-4's time to transform and send them, pvr the PVR's
             render time (KallistiOS's figure) mean and largest. UC: the union and one side.

   START on the pages: a live view (the target laid on the ground under a fixed camera). X k, Y the way of drawing,
   B the sun; the squares top left are k, the square beside them the way (green U, red P, yellow C, cyan UC).

   Waits: the PVR's "ready" and "render done" are polled on the timer (the render-done interrupt is noted by this
   program's handler in front of KallistiOS's, which still runs); no wait is longer than LIMIT_NS. Only PVR registers
   and video memory are written. The fault catcher (include/sh4der_fault.h) is in.
   Every number comes from the run that prints it; only a real console's run is evidence.
   Build: python demo/psm/ao/build.py --tag <new tag>. Not run on a console when this file was written. */

#include <kos.h>
#include <stdarg.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "../psm_suite.h"
#include "sh4der_core.h"
#include "ao0_model.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#ifndef AO0_TAG
#define AO0_TAG untagged                    /* the build gives the image's tag, a bare word */
#endif
#define AO0_STR_(x) #x
#define AO0_STR(x) AO0_STR_(x)

/* ---- the result pages ----------------------------------------------------------- */

#define PG_PREFIX "ao0"
#define PG_LINES_MAX 120u
#include "../../../include/sh4der_page.h"
#define say pg_say
static sh4der_fault_crumbs_t crumbs;

static void step(const char *name) {
    sh4der_fault_beat(&crumbs);
    sh4der_fault_stage(&crumbs, name);
    crumbs.frame++;
}

/* ---- clocks ------------------------------------------------------------------------ */

#define LIMIT_NS 200000000ull               /* no wait in this file is longer */

/* KallistiOS's ns clock steps 2,502,400 ns ahead at each reload of TMU2 (MEASURED 2026-11-25, one console, image
   ray2 CLOCK.4); this takes the step out, as c4g does. */
static inline uint64_t ns(void) { const uint64_t n = timer_ns_gettime64(); return n - n / 1000000000ull * 2502400ull; }
static unsigned us_of(uint64_t t) { return (unsigned)((t + 500u) / 1000u); }

/* ---- memory, headers ----------------------------------------------------------------- */

#define MAP_ROWS (AO0_SIZE + 8u)            /* rows allocated for the target; AO0_SIZE + 2 are drawn, as c4's map */
#define RUN 256u                            /* vertices a store-queue run: 8 KB */

static pvr_ptr_t map_mem;
static pvr_poly_hdr_t h_flat, h_ao[2], h_ground;
static pvr_vertex_t vrun[RUN] __attribute__((aligned(32)));
static unsigned run_n;
static uint16_t rb[2][AO0_SIZE * AO0_SIZE];
static uint8_t m_bits[AO0_SIZE * AO0_SIZE], m_count[AO0_SIZE * AO0_SIZE], m_edge[AO0_SIZE * AO0_SIZE];
static ao0_scene_t scene;
static float sxy[AO0_V_MAX][2];
static uint8_t sok[AO0_V_MAX];

static void flush(void) {
    if (!run_n) return;
    pvr_prim(vrun, run_n * sizeof vrun[0]);
    run_n = 0;
}

static inline void put(int eol, float x, float y, float z, float u, float v, uint32_t argb) {
    pvr_vertex_t *o;

    if (run_n == RUN) flush();
    o = &vrun[run_n++];
    o->flags = eol ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    o->x = x; o->y = y; o->z = z; o->u = u; o->v = v; o->argb = argb; o->oargb = 0u;
}

static void rect(float x0, float y0, float x1, float y1, float z, uint32_t argb) {
    put(0, x0, y1, z, 0.0f, 0.0f, argb);
    put(0, x0, y0, z, 0.0f, 0.0f, argb);
    put(0, x1, y1, z, 0.0f, 0.0f, argb);
    put(1, x1, y0, z, 0.0f, 0.0f, argb);
}

static void headers(void) {
    pvr_poly_cxt_t c;

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&h_flat, &c);
    sh4der_aosky_header(&h_ao[0], 0);
    sh4der_aosky_header(&h_ao[1], 1);
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)AO0_SIZE, (int)AO0_SIZE, map_mem, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE; c.txr.env = PVR_TXRENV_MODULATE;
    pvr_poly_compile(&h_ground, &c);
}

/* ---- scenes on the PVR --------------------------------------------------------------- */

static uint64_t last_render_ns;
static uint32_t last_vbuf;
static asic_evt_handler_entry_t rd_prev;
static volatile uint32_t rd_seen;
static int rd_hooked;
static volatile uint32_t ta_evt[3];         /* the TA's out-of-memory events seen: parameters, bins, ISP */
static int ta_hooked[3];
static asic_evt_handler_entry_t ta_prev[3];
static const uint16_t ta_code[3] = { ASIC_EVT_PVR_PARAM_OUTOFMEM, ASIC_EVT_PVR_OPB_OUTOFMEM, ASIC_EVT_PVR_ISP_OUTOFMEM };

static void rd_evt(uint32_t code, void *data) {
    (void)data;
    if (rd_prev.hdl) rd_prev.hdl(code, rd_prev.data);
    rd_seen = 1u;
}

static void ta_evt_fn(uint32_t code, void *data) {
    const unsigned i = (unsigned)(uintptr_t)data;

    if (ta_prev[i].hdl) ta_prev[i].hdl(code, ta_prev[i].data);
    ta_evt[i]++;
}

/* Wait until the PVR takes a new scene: a poll on the timer. */
static int wait_ready(void) {
    const uint64_t t0 = ns();

    while (pvr_check_ready() < 0) if (ns() - t0 > LIMIT_NS) return 0;
    return 1;
}

static int scene_open(int to_map) {
    if (!wait_ready()) return 0;
    if (to_map) return pvr_scene_begin_rtt(map_mem, AO0_SIZE, AO0_SIZE + 2u, AO0_SIZE) >= 0;
    pvr_scene_begin();
    return 1;
}

static int scene_close(void) {
    pvr_stats_t st;
    uint64_t t0;
    unsigned t;

    rd_seen = 0u;                           /* the render starts inside pvr_scene_finish's wait at the earliest */
    pvr_scene_finish();
    if (!wait_ready()) return 0;
    t0 = ns();
    if (rd_hooked) {
        while (!rd_seen) if (ns() - t0 > LIMIT_NS) return 0;
    } else {                                /* our handler was refused: KallistiOS's sleeping wait, and the page says so */
        for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) sh4der_fault_beat(&crumbs);
        if (t == 20u) return 0;
    }
    if (pvr_get_stats(&st) < 0) return 0;
    last_render_ns = st.rnd_last_time;
    last_vbuf = (uint32_t)st.vtx_buffer_used;
    return 1;
}

/* (p, 1) x m for many points: the matrix's columns as XMTRX's rows, one FTRV a point (psm_scene.h's psm_xf). */
static void xf_load(const float m[16]) {
    const shz_vec4_t r0 = shz_vec4_init(m[0], m[4], m[8], m[12]);
    const shz_vec4_t r1 = shz_vec4_init(m[1], m[5], m[9], m[13]);
    const shz_vec4_t r2 = shz_vec4_init(m[2], m[6], m[10], m[14]);
    const shz_vec4_t r3 = shz_vec4_init(m[3], m[7], m[11], m[15]);

    shz_xmtrx_load_rows_4x4(&r0, &r1, &r2, &r3);
}

/* The scene's triangles slid along `slope` into the target at depth z. The header is the caller's. Returns vertices. */
static unsigned send_dir(const float slope[2], float z, uint32_t argb, int cull) {
    const float half = 0.5f * (float)AO0_SIZE;
    float m[16];
    unsigned i, n = 0;

    sh4der_aosky_slide_map(ao0_map, slope, 0.0f, m);
    xf_load(m);
    for (i = 0; i < scene.vn; i++) {
        const shz_vec4_t v = shz_xmtrx_transform_vec4(shz_vec4_init(scene.v[i][0], scene.v[i][1], scene.v[i][2], 1.0f));
        const float h[4] = { v.x, v.y, v.z, v.w };

        sok[i] = (uint8_t)sh4der_aosky_px(h, 1.0f, half, sxy[i]);       /* a uniform map: w is 1 */
    }
    for (i = 0; i < scene.tn; i++) {
        const unsigned a = scene.t[i][0], b = scene.t[i][1], c = scene.t[i][2];

        if (!(sok[a] & sok[b] & sok[c])) continue;
        if (!sh4der_aosky_tri_ok(sxy[a], sxy[b], sxy[c], (float)AO0_SIZE, cull)) continue;
        put(0, sxy[a][0], sxy[a][1], z, 0.0f, 0.0f, argb);
        put(0, sxy[b][0], sxy[b][1], z, 0.0f, 0.0f, argb);
        put(1, sxy[c][0], sxy[c][1], z, 0.0f, 0.0f, argb);
        n += 3u;
    }
    flush();
    return n;
}

static unsigned ao_v, sun_v;
static uint64_t ao_ns, sun_ns;

/* The map pass as route A draws it (white, the sun's casters, a white border), then k directions. way: bit 0 the
   union, bit 1 one side. 0 when the pass did not come back. */
static int map_pass(unsigned k, unsigned way, int sun) {
    const float n = (float)AO0_SIZE;
    const int cull = (way & 2u) ? 1 : 0;
    uint64_t t0;
    unsigned d;

    ao_v = sun_v = 0; ao_ns = sun_ns = 0;
    if (!scene_open(1)) return 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&h_flat, sizeof h_flat);
    rect(0.0f, 0.0f, n, n + 2.0f, 0.1f, 0xffffffffu);
    flush();
    if (sun) {
        t0 = ns();
        sun_v = send_dir(ao0_sun_slope, 1.0f, AO0_SUN_GREY, 0);
        sun_ns = ns() - t0;
    }
    rect(0.0f, 0.0f, n, 1.0f, 2.0f, 0xffffffffu);
    rect(0.0f, n - 1.0f, n, n + 2.0f, 2.0f, 0xffffffffu);
    rect(0.0f, 0.0f, 1.0f, n, 2.0f, 0xffffffffu);
    rect(n - 1.0f, 0.0f, n, n, 2.0f, 0xffffffffu);
    flush();
    pvr_list_finish();
    if (k) {
        const uint32_t argb = sh4der_aosky_argb(k, 1.0f);

        pvr_list_begin(PVR_LIST_TR_POLY);
        t0 = ns();
        pvr_prim(&h_ao[way & 1u], sizeof h_ao[0]);
        for (d = 0; d < k; d++) {
            float slope[2];

            sh4der_aosky_slope(k, d, slope);
            ao_v += send_dir(slope, sh4der_aosky_depth(d), argb, cull);
        }
        ao_ns = ns() - t0;
        pvr_list_finish();
    }
    return scene_close();
}

/* The target into rb[which]: words read straight from video memory (not timed). */
static void read_back(unsigned which) {
    const volatile uint16_t *src = (const volatile uint16_t *)map_mem;
    unsigned i;

    for (i = 0; i < AO0_SIZE * AO0_SIZE; i++) {
        rb[which][i] = src[i];
        if ((i & 8191u) == 0u) sh4der_fault_beat(&crumbs);
    }
}

/* ---- start, stop ------------------------------------------------------------------- */

static int start(void) {
    pvr_init_params_t init = pvr_default_params;
    unsigned i;

    init.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_0;
    init.vertex_buf_size = 1024 * 1024;                          /* the town's */
    init.autosort_disabled = 1;                                  /* translucent polygons in the order they are sent */
    init.opb_overflow_count = 4;                                 /* the town's */
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return 0; }
    pvr_set_bg_color(0.0f, 0.0f, 0.3f);
    map_mem = pvr_mem_malloc(AO0_SIZE * MAP_ROWS * 2u);
    if (!map_mem) { say("video memory allocation FAIL"); pvr_shutdown(); return 0; }
    headers();
    vid_set_dithering(false);
    rd_prev = asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_evt, NULL);   /* ours in front of KallistiOS's */
    rd_hooked = rd_prev.hdl != NULL;
    if (!rd_hooked) asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, NULL, NULL);
    for (i = 0; i < 3u; i++) {              /* the TA's out-of-memory events: counted; KallistiOS's handler, if any, still runs */
        ta_prev[i] = asic_evt_set_handler(ta_code[i], ta_evt_fn, (void *)(uintptr_t)i);
        if (!ta_prev[i].hdl) asic_evt_enable(ta_code[i], ASIC_IRQ_DEFAULT);
        ta_hooked[i] = 1;
    }
    return 1;
}

static void stop(const char *stage) {
    unsigned i;

    step(stage);
    if (rd_hooked) { asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_prev.hdl, rd_prev.data); rd_hooked = 0; }   /* KallistiOS's own back */
    for (i = 0; i < 3u; i++) {
        if (!ta_hooked[i]) continue;
        if (ta_prev[i].hdl) asic_evt_set_handler(ta_code[i], ta_prev[i].hdl, ta_prev[i].data);
        else { asic_evt_disable(ta_code[i], ASIC_IRQ_DEFAULT); asic_evt_remove_handler(ta_code[i]); }
        ta_hooked[i] = 0;
    }
    vid_set_dithering(true);
    pvr_shutdown();
}

/* ---- test 1: the union ------------------------------------------------------------- */

static const char *const way_name[4] = { "P ", "U ", "C ", "UC" };

/* The target just drawn against the model, either way up: the better reading, *flip the way it was. */
static void reading(unsigned which, ao0_read_t *r, int *flip) {
    ao0_read_t r1;

    read_back(which);
    ao0_read(rb[which], m_bits, m_edge, 0, r);
    *flip = 0;
    if (r->bad * 20u <= r->tested) return;
    sh4der_fault_beat(&crumbs);
    ao0_read(rb[which], m_bits, m_edge, 1, &r1);
    if (r1.bad < r->bad) { *r = r1; *flip = 1; }
}

static int union_ok = -1;                   /* 1 works, 0 fails, -1 not known */

static void t_union(void) {
    unsigned k, i, same_all = 1, u_bad_all = 0, c_bad_all = 0;
    char line[PG_COLS + 1u];

    say("1 UNION 48 triangles, no sun. c values/levels b bad of n f flip");
    ao0_small(&scene, 5.0f, 0.0f);
    for (k = 1; k <= 8u; k *= 2u) {
        ao0_read_t ru, rp, rc;
        int fu, fp, fc;
        unsigned differ = 0, n = 0;

        step("1 model");
        ao0_cover(&scene, k, 0, m_bits, m_count);
        ao0_edges(m_bits, m_edge);
        step("1 union");
        if (!map_pass(k, 1u, 0)) { say("1 k%u U NO RENDER END", k); return; }
        reading(0u, &ru, &fu);
        step("1 plain");
        if (!map_pass(k, 0u, 0)) { say("1 k%u P NO RENDER END", k); return; }
        reading(1u, &rp, &fp);
        for (i = 0; i < AO0_SIZE * AO0_SIZE; i++) differ += rb[0][i] != rb[1][i];
        step("1 side");
        if (!map_pass(k, 2u, 0)) { say("1 k%u C NO RENDER END", k); return; }
        reading(1u, &rc, &fc);
        say("1 k%u U c%u/%u b%u n%u f%d|P c%u b%u|C c%u b%u", k, ru.colours, ru.levels, ru.bad, ru.tested, fu, rp.colours, rp.bad, rc.colours, rc.bad);
        n = (unsigned)snprintf(line, sizeof line, "1 k%u U565", k);
        for (i = 0; i < ru.colours && i < 10u && n + 5u < sizeof line; i++) n += (unsigned)snprintf(line + n, sizeof line - n, " %04x", ru.value[i]);
        say("%s", line);
        n = (unsigned)snprintf(line, sizeof line, "1 k%u C565", k);
        for (i = 0; i < rc.colours && i < 10u && n + 5u < sizeof line; i++) n += (unsigned)snprintf(line + n, sizeof line - n, " %04x", rc.value[i]);
        say("%s", line);
        if (differ) same_all = 0;
        if (ru.bad * 500u > ru.tested || ru.colours != ru.levels) u_bad_all++;
        if (rc.bad * 500u > rc.tested) c_bad_all++;
        (void)fp;
    }
    union_ok = u_bad_all == 0u;
    say("1 DERIVED UNION %s%s", union_ok ? "WORKS: one darkening a direction" : "FAILS", !union_ok && same_all ? ": U draws what P draws" : "");
    say("1 DERIVED ONE SIDE %s", c_bad_all == 0u ? "alone reads as the union at every k" : "differs where two casters overlap (b above)");
}

/* ---- test 2: the cost -------------------------------------------------------------- */

#define FRAMES 8u

static void t_cost(void) {
    static const uint8_t cfg[][2] = { { 0, 1 }, { 1, 1 }, { 2, 1 }, { 4, 1 }, { 8, 1 }, { 8, 0 }, { 8, 2 }, { 8, 3 }, { 4, 3 } };
    unsigned c, f, ao0us = 0, ao8us = 0, pvr0 = 0, pvr8 = 0, ok0 = 0, ok8 = 0;

    say("2 COST 1164 triangles + sun. %u frames a row. us; K = KB", FRAMES);
    ao0_load(&scene);
    for (c = 0; c < sizeof cfg / sizeof cfg[0]; c++) {
        const unsigned k = cfg[c][0], way = cfg[c][1];
        uint64_t ao = 0, sun = 0, pvr = 0, pvr_max = 0;
        unsigned est;

        step("2 cost");
        for (f = 0; f <= FRAMES; f++) {
            sh4der_fault_beat(&crumbs);
            if (!map_pass(k, way, 1)) { say("2 k%u %s NO RENDER END (frame %u)", k, way_name[way], f); return; }
            if (!f) continue;               /* the first is not counted */
            ao += ao_ns; sun += sun_ns; pvr += last_render_ns;
            if (last_render_ns > pvr_max) pvr_max = last_render_ns;
        }
        est = (ao_v / 3u + sun_v / 3u) * 60u + 5u * 76u;
        say("2 k%u %s v%u e%uK k%uK ao%u sun%u pvr%u mx%u", k, way_name[way], ao_v, (est + 512u) / 1024u, (unsigned)((last_vbuf + 512u) / 1024u),
            us_of(ao / FRAMES), us_of(sun / FRAMES), us_of(pvr / FRAMES), us_of(pvr_max));
        if (k == 0u) { ao0us = us_of(ao / FRAMES); pvr0 = us_of(pvr / FRAMES); ok0 = 1; }
        if (k == 8u && way == 1u) { ao8us = us_of(ao / FRAMES); pvr8 = us_of(pvr / FRAMES); ok8 = 1; }
    }
    if (ok0 && ok8) say("2 DERIVED a direction (k8 U - k0)/8: SH-4 %u us, PVR %d us", (ao8us - ao0us) / 8u, ((int)pvr8 - (int)pvr0) / 8);
    say("2 TA out-of-memory events: param %u bins %u isp %u", (unsigned)ta_evt[0], (unsigned)ta_evt[1], (unsigned)ta_evt[2]);
}

/* ---- the run ---------------------------------------------------------------------- */

static int fault_on;

static void run(void) {
    say("ao0 %s AO, sky directions into the shadow target. This run's", AO0_STR(AO0_TAG));
    say(" numbers. U union P plain C one side UC both. 256x256 565");
    {
        char rev[32];

        suite_rev_text(rev, sizeof rev);    /* which console: two register reads, nothing written */
        say("%s", rev);
    }
    if (!start()) return;
    if (!fault_on) { fault_on = 1; if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused"); }
    if (!rd_hooked) say("render-done handler refused: KallistiOS's sleeping wait used");
    t_union();
    t_cost();
    stop("done");
}

/* ---- the live view ---------------------------------------------------------------- */

static float cam_e[3] = { 0.0f, 13.0f, -17.0f }, cam_f[3], cam_r[3], cam_u[3];
#define CAM_FOCAL 520.0f

static void cam_make(void) {
    const float up[3] = { 0.0f, 1.0f, 0.0f };

    cam_f[0] = -cam_e[0]; cam_f[1] = -cam_e[1]; cam_f[2] = -cam_e[2];
    sh4der_norm3_(cam_f);
    sh4der_cross_(up, cam_f, cam_r);
    sh4der_norm3_(cam_r);
    sh4der_cross_(cam_f, cam_r, cam_u);
}

/* A world point on the screen: x, y, 1 / depth. Everything shown is in front of the camera. */
static void cam_pt(const float p[3], float s[3]) {
    const float d[3] = { p[0] - cam_e[0], p[1] - cam_e[1], p[2] - cam_e[2] };
    const float depth = sh4der_dot3_(d, cam_f), iw = sh4der_recip_pos_(depth > 0.1f ? depth : 0.1f);

    s[0] = 320.0f + CAM_FOCAL * sh4der_dot3_(d, cam_r) * iw;
    s[1] = 240.0f - CAM_FOCAL * sh4der_dot3_(d, cam_u) * iw;
    s[2] = iw;
}

static int screen_pass(unsigned k, unsigned way, int flip) {
    static const uint32_t way_col[4] = { 0xffff3030u, 0xff30ff30u, 0xffffe030u, 0xff30e0ffu };
    static const uint32_t face_col[6] = { 0xff5a5048u, 0xffe6d2b4u, 0xff9a8a72u, 0xffb4a288u, 0xff867862u, 0xffc8b496u };
    float s[3];
    unsigned i, j;

    if (!scene_open(0)) return 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&h_ground, sizeof h_ground);
    for (i = 0; i < 4u; i++) {              /* the ground: one quad, the target over it */
        const float p[3] = { (i & 2u) ? AO0_SPAN : -AO0_SPAN, 0.0f, (i & 1u) ? -AO0_SPAN : AO0_SPAN };
        const float v = (i & 1u) ? 0.0f : 1.0f;

        cam_pt(p, s);
        put(i == 3u, s[0], s[1], s[2], (i & 2u) ? 1.0f : 0.0f, flip ? 1.0f - v : v, 0xffa8b890u);
    }
    flush();
    pvr_prim(&h_flat, sizeof h_flat);
    for (i = 0; i < scene.tn; i++)
        for (j = 0; j < 3u; j++) {
            cam_pt(scene.v[scene.t[i][j]], s);
            put(j == 2u, s[0], s[1], s[2], 0.0f, 0.0f, face_col[(i % 12u) / 2u]);
        }
    for (i = 0; i < k; i++) rect(64.0f + 14.0f * (float)i, 52.0f, 74.0f + 14.0f * (float)i, 62.0f, 20.0f, 0xffffffffu);
    rect(64.0f + 14.0f * 9.0f, 52.0f, 74.0f + 14.0f * 9.0f, 62.0f, 20.0f, way_col[way & 3u]);
    flush();
    pvr_list_finish();
    return scene_close();
}

static void live(void) {
    static const uint8_t ks[5] = { 0, 1, 2, 4, 8 };
    unsigned held = ~0u, ki = 3u, way = union_ok == 0 ? 2u : 1u, frames = 0;
    int sun = 1;
    float ang = 0.0f;

    if (!start()) return;
    cam_make();
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;
        float sn, cs;

        step("live");
        held = now;
        if ((hit & CONT_START) && frames > 10u) break;
        if (hit & CONT_X) ki = (ki + 1u) % 5u;
        if (hit & CONT_Y) way = (way + 1u) & 3u;
        if (hit & CONT_B) sun = !sun;
        ang += 0.012f;
        if (ang > 6.2831853f) ang -= 6.2831853f;
        sh4der_sincos_(ang, &sn, &cs);
        ao0_small(&scene, 6.5f * cs, 6.5f * sn);
        if (!map_pass(ks[ki], way, sun)) { say("live: map NO RENDER END"); break; }
        if (!screen_pass(ks[ki], way, 0)) { say("live: screen NO RENDER END"); break; }
        frames++;
    }
    stop("live end");
}

int main(void) {
    unsigned page = 0, pages, held = ~0u;
    int redraw = 1;

    (void)pg_plot; (void)pg_series; (void)pg_point;               /* no graph pages here */
    suite_init();                           /* the suite disc: the menu's binary into RAM, before anything else */
    run();
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;

        sh4der_fault_beat(&crumbs);
        held = now;
        pages = pg_pages();
        if (hit & CONT_A) { page = (page + 1u) % pages; redraw = 1; }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; redraw = 1; }
        else if (hit & CONT_START) {
            live();
            vid_set_mode(DM_640x480, PM_RGB565);
            held = ~0u;                     /* what is still held from the live view is not a press here */
            redraw = 1;
        }
        else if (suite_tick(now, page, pages)) { page++; redraw = 1; }   /* the suite disc: a chained run turns the page; ABXY leaves */
        if (redraw) { pg_draw(page % pages, "AO ao0", suite_keys_text("START live view")); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
