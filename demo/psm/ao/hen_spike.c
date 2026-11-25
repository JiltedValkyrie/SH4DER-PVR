/* SH4DER-PVR: hens-eyes, rays against a collision sweep for the same hens in the same room, Dreamcast test program

   demo/psm/ao/hen_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A live A / B for a real console. The living room (room_data.h, drawn with its baked vertex AO, CLAY) and the town's
   hens walking in it (hen_room.h), steered one of four ways that do the same job:

     EYES              hen_eyes.h: a fan of rays, cast on a tick in seconds (0.1 s), each hen at its own phase
     SWEEP GRID step   hen_sweep.h: a capsule sweep every step of 1/60 s, a 16 x 16 grid as its broad phase
     SWEEP GRID tick   the same sweep on the eyes' tick
     SWEEP NAIVE       the sweep every step against every triangle: no broad phase

   The hens, their starts and their walk are the same in every mode, and the walk goes in steps of 1/60 s whatever a
   frame takes, so a mode's run repeats. A change of mode, count, rays or tick starts the hens again.

   WHAT IS TIMED, with the SH-4's cycle counter (PRFC1, CPU cycles), a step of 1/60 s for all the hens:
     pre       the eyes or the sweep alone: the mean of the run and the dearest step
     post+sep  the push out of the furniture and the hens kept apart (the same code in every mode)
   Beside them the work counted (hen_stats_t, hen_sweep_stats_t: exact) for 100 steps, and what the modules' cost
   models make of the counts: ESTIMATE, there to be replaced by the measured line above it.

   It runs by itself first: 4 hens and 16 hens in each of the four modes, then 32 hens with EYES and with SWEEP GRID
   step, HENS_AUTO_S seconds each; then the result pages and a graph. START on the pages: the live view.

     X  the mode          Y  the hens: 4, 8, 16, 32          A  the rays drawn (hen_eyes_debug) on / off
     B  the tick: 50, 100, 200, 400 ms      up / down  the fan: 5, 9, 13 rays      stick  the camera      START  the pages

   Waits: "ready" and "render done" are polled (the render-done interrupt noted by this program's handler in front of
   KallistiOS's); no wait is longer than LIMIT_NS. Only PVR registers and video memory are written. The fault catcher
   (../psm_fault.h) is in. Every number comes from the run that prints it; only a real console's run is evidence.
   Build: python demo/psm/ao/build.py --spike hen --tag <new tag>. Not run on a console when this file was written. */

#include <kos.h>
#include <stdarg.h>

#define PSM_FAULT_IMPL
#include "../psm_fault.h"
#include "../psm_suite.h"
#include "ao4_scene.h"
#include "ao4_room.h"
#include "hero_hens.h"
#include "hen_eyes.h"
#include "hen_sweep.h"
static inline uint64_t cyc(void) { return perf_cntr_count(PRFC1); }
#define HENR_CYC() cyc()
#include "hen_room.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#ifndef HEN_TAG
#define HEN_TAG untagged
#endif
#define HENS_STR_(x) #x
#define HENS_STR(x) HENS_STR_(x)
#ifndef HENS_AUTO_S
#define HENS_AUTO_S 5u                      /* seconds a stage in the run that counts */
#endif
#define CYC_PER_MS 200006u                  /* 200.006 MHz, MEASURED (bench s2) */

/* ---- the result pages ----------------------------------------------------------- */

#define PG_PREFIX "hens"
#define PG_LINES_MAX 120u
#include "../psm_page.h"
#define say pg_say
static psm_fault_crumbs_t crumbs;

static void step(const char *name) {
    psm_fault_beat(&crumbs);
    psm_fault_stage(&crumbs, name);
    crumbs.frame++;
}

#define LIMIT_NS 200000000ull
/* KallistiOS's ns clock steps 2,502,400 ns ahead at each reload of TMU2 (MEASURED, image ray2 CLOCK.4): taken out.
   Used for the bounded waits and the stages' lengths only; every cost is in cycles. */
static inline uint64_t ns(void) { const uint64_t n = timer_ns_gettime64(); return n - n / 1000000000ull * 2502400ull; }

/* ---- memory, headers ----------------------------------------------------------------- */

#define RUN 256u
#define TEXT_W 512u
#define TEXT_H 64u
#define TEXT_LINES 4u

static pvr_ptr_t text_mem;
static pvr_poly_hdr_t h_obj, h_text, h_dim, h_ray;
static pvr_vertex_t vrun[RUN] __attribute__((aligned(32)));
static unsigned run_n, sent;
static uint16_t text_px[TEXT_W * TEXT_H] __attribute__((aligned(32)));
static char text_ln[TEXT_LINES][65];
static int text_dirty;

static ao4_scene_t room;
static uint32_t room_col[AO4_V_MAX];
static float scr[AO4_V_MAX][3];
static henr_t hr;
static int world_ok;

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
    sent++;
}

static void rect(float x0, float y0, float x1, float y1, float z, float u0, float v0, float u1, float v1, uint32_t argb) {
    put(0, x0, y1, z, u0, v1, argb);
    put(0, x0, y0, z, u0, v0, argb);
    put(0, x1, y1, z, u1, v1, argb);
    put(1, x1, y0, z, u1, v0, argb);
}

static void headers(void) {
    pvr_poly_cxt_t c;

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                      /* the room and the hens */
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&h_obj, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                      /* the rays: added, over everything */
    c.gen.culling = PVR_CULLING_NONE;
    c.blend.src = PVR_BLEND_SRCALPHA; c.blend.dst = PVR_BLEND_ONE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_ray, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, (int)TEXT_W, (int)TEXT_H, text_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_text, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                      /* the strips under the text */
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&h_dim, &c);
}

/* ---- the text: four lines of 64 characters in a texture ------------------------------------- */

static void text_set(unsigned line, const char *fmt, ...) {
    char s[65];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s, sizeof s, fmt, ap);
    va_end(ap);
    if (strcmp(s, text_ln[line])) { strcpy(text_ln[line], s); text_dirty = 1; }
}

/* Draw the lines into RAM (white, alpha = the glyph's cover) and load the texture. Call with no render running. */
static void text_upload(void) {
    unsigned l, i, x, y;

    if (!text_dirty) return;
    memset(text_px, 0, sizeof text_px);
    for (l = 0; l < TEXT_LINES; l++)
        for (i = 0; i < 64u && text_ln[l][i]; i++) {
            const unsigned c = (unsigned char)text_ln[l][i];
            const uint8_t (*gl)[PG_CW] = pg_glyph[c >= 32u && c < 127u ? c - 32u : 0u];

            for (y = 0; y < PG_CH; y++)
                for (x = 0; x < PG_CW; x++) text_px[(l * 16u + y) * TEXT_W + i * 8u + x] = (uint16_t)(((gl[y][x] >> 4) << 12) | 0x0fffu);
        }
    pvr_txr_load(text_px, text_mem, sizeof text_px);
    text_dirty = 0;
}

static void text_put(unsigned line, float x, float y) {
    const float v0 = (float)(line * 16u) * (1.0f / (float)TEXT_H), v1 = (float)(line * 16u + 16u) * (1.0f / (float)TEXT_H);

    flush(); pvr_prim(&h_dim, sizeof h_dim);
    rect(x - 2.0f, y - 1.0f, x + 514.0f, y + 17.0f, 10.0f, 0, 0, 0, 0, 0xa0000000u);
    flush(); pvr_prim(&h_text, sizeof h_text);
    rect(x, y, x + 512.0f, y + 16.0f, 11.0f, 0.0f, v0, 1.0f, v1, 0xffffffffu);
}

/* ---- scenes on the PVR --------------------------------------------------------------- */

static uint64_t last_render_ns;
static asic_evt_handler_entry_t rd_prev;
static volatile uint32_t rd_seen;
static int rd_hooked;

static void rd_evt(uint32_t code, void *data) {
    (void)data;
    if (rd_prev.hdl) rd_prev.hdl(code, rd_prev.data);
    rd_seen = 1u;
}

static int wait_ready(void) {
    const uint64_t t0 = ns();

    while (pvr_check_ready() < 0) if (ns() - t0 > LIMIT_NS) return 0;
    return 1;
}

static int scene_close(void) {
    pvr_stats_t st;
    uint64_t t0;
    unsigned t;

    flush();
    rd_seen = 0u;
    pvr_scene_finish();
    if (!wait_ready()) return 0;
    t0 = ns();
    if (rd_hooked) {
        while (!rd_seen) if (ns() - t0 > LIMIT_NS) return 0;
    } else {
        for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) psm_fault_beat(&crumbs);
        if (t == 20u) return 0;
    }
    if (pvr_get_stats(&st) < 0) return 0;
    last_render_ns = st.rnd_last_time;
    return 1;
}

/* ---- the view ---------------------------------------------------------------------------- */

typedef struct view {
    unsigned mode, ni, ri, ti;
    int debug;
    float turn, lift, t;
} view_t;

static const uint8_t n_of[4] = { 4, 8, 16, 32 }, rays_of[3] = { 5, 9, 13 };
static const float tick_of[4] = { 0.05f, 0.1f, 0.2f, 0.4f };
static const char *const mode_name[HENR_MODES] = { "EYES", "SWEEP GRID step", "SWEEP GRID tick", "SWEEP NAIVE" };
static const char *const mode_short[HENR_MODES] = { "E", "Gs", "Gt", "N" };

static void apply(const view_t *v) {        /* the hens again from their start, this way */
    hr.cfg.rays = rays_of[v->ri]; hr.cfg.tick = tick_of[v->ti];
    henr_reset(&hr, n_of[v->ni], v->mode);
}

/* The hens: the town's template where each stands, lit as the room's clay, a little warm so they are seen. */
static void hens_draw(const ao4_cam_t *cam) {
    const float amb = ao4_amb[AO4_CLAY], sunc = ao4_sunc[AO4_CLAY];
    unsigned k, j, i, at;

    for (k = 0; k < hr.n; k++) {
        const henr_hen_t *m = &hr.h[k];
        float sb, cb, st, ct;

        sh4der_sincos_(m->bearing, &sb, &cb);
        sh4der_sincos_(m->tilt, &st, &ct);
        for (j = 0, at = 0; j < PSM_TOWN_HEN_S; j++)
            for (i = 0; i < psm_town_hen_sl[j]; i++, at++) {
                const unsigned x = psm_town_hen_sv[at];
                const float nl[3] = { (float)psm_town_hen_n[x][0] * (1.0f / 127.0f), (float)psm_town_hen_n[x][1] * (1.0f / 127.0f), (float)psm_town_hen_n[x][2] * (1.0f / 127.0f) };
                float p[3], n[3], sp[3], ndl, l;

                henr_place(m, sb, cb, st, ct, psm_town_hen_v[x], p);
                henr_turn(sb, cb, st, ct, nl, n);
                ndl = sh4der_dot3_(n, room.sun);
                l = AO4_CLAY_GREY * (amb + sunc * (ndl > 0.0f ? ndl : 0.0f)) * 255.0f;
                ao4_cam_pt(cam, p, sp);
                put(i + 1u == psm_town_hen_sl[j], sp[0], sp[1], sp[2], 0.0f, 0.0f,
                    0xff000000u | ((uint32_t)(sh4der_sat_(l * (1.0f / 255.0f)) * 255.0f) << 16) | ((uint32_t)(sh4der_sat_(l * (0.86f / 255.0f)) * 255.0f) << 8) |
                    (uint32_t)(sh4der_sat_(l * (0.62f / 255.0f)) * 255.0f));
            }
    }
}

/* The last tick's rays of every hen: each segment a quad 1.5 pixels wide on the screen. A segment under a pixel long
   is left out, so no quad without area is sent. */
static void rays_draw(const ao4_cam_t *cam) {
    float seg[HEN_DEBUG_SEGS][2][3];
    uint32_t argb[HEN_DEBUG_SEGS];
    unsigned k, i;

    for (k = 0; k < hr.n; k++) {
        const unsigned n = hen_eyes_debug(&hr.world, &hr.eyes[k], &hr.cfg, seg, argb);

        for (i = 0; i < n; i++) {
            float a[3], b[3], dx, dy, d2, inv;

            if (!ao4_cam_pt(cam, seg[i][0], a) || !ao4_cam_pt(cam, seg[i][1], b)) continue;
            dx = b[0] - a[0]; dy = b[1] - a[1]; d2 = dx * dx + dy * dy;
            if (!(d2 > 1.0f)) continue;
            inv = 0.75f * sh4der_recip_pos_(sh4der_sqrt_(d2));
            dx *= inv; dy *= inv;
            put(0, a[0] - dy, a[1] + dx, a[2], 0.0f, 0.0f, argb[i]);
            put(0, a[0] + dy, a[1] - dx, a[2], 0.0f, 0.0f, argb[i]);
            put(0, b[0] - dy, b[1] + dx, b[2], 0.0f, 0.0f, argb[i]);
            put(1, b[0] + dy, b[1] - dx, b[2], 0.0f, 0.0f, argb[i]);
        }
    }
}

/* A whole frame. 0 when the render did not come back. */
static int frame(const view_t *v) {
    ao4_cam_t cam;
    unsigned st, i, k = 0;

    ao4_cam_at(&room, &cam, v->t, v->turn, v->lift);
    for (i = 0; i < room.vn; i++) ao4_cam_pt(&cam, room.p[i], scr[i]);
    if (!wait_ready()) return 0;
    pvr_scene_begin();
    sent = 0;
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&h_obj, sizeof h_obj);
    for (st = 0; st < room.strips; st++)
        for (i = 0; i < room.slen[st]; i++, k++) {
            const unsigned x = room.idx[k];

            put(i + 1u == room.slen[st], scr[x][0], scr[x][1], scr[x][2], 0.0f, 0.0f, room_col[x]);
        }
    hens_draw(&cam);
    flush();
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    if (v->debug) { pvr_prim(&h_ray, sizeof h_ray); rays_draw(&cam); }
    text_put(0u, 64.0f, 40.0f);
    text_put(1u, 64.0f, 376.0f);
    text_put(2u, 64.0f, 396.0f);
    text_put(3u, 64.0f, 416.0f);
    flush();
    pvr_list_finish();
    return scene_close();
}

/* A run's numbers: the hens' counters (hen_room.h, since their start) and the frames'. */
typedef struct tally {
    unsigned n, mode, frames;
    unsigned long steps;
    unsigned long long cyc_pre, cyc_post, cyc_frame;
    unsigned pre_worst, post_worst;
    uint64_t pvr_ns;
    hen_stats_t st;
    hen_sweep_stats_t sst;
} tally_t;

static void tally_take(tally_t *t, unsigned frames, unsigned long long cyc_frame, uint64_t pvr_ns) {
    t->n = hr.n; t->mode = hr.mode; t->frames = frames; t->steps = hr.steps;
    t->cyc_pre = hr.cyc_pre; t->cyc_post = hr.cyc_post; t->pre_worst = hr.cyc_pre_worst; t->post_worst = hr.cyc_post_worst;
    t->cyc_frame = cyc_frame; t->pvr_ns = pvr_ns; t->st = hr.st; t->sst = hr.sst;
}

static unsigned per100(unsigned long count, unsigned long steps) { return (unsigned)(count * 100ul / (steps ? steps : 1ul)); }

/* The model's cycles a step for a tally: ESTIMATE (the modules' own cost tables). */
static unsigned model_of(const tally_t *t) {
    hen_cost_t c;
    hen_sweep_cost_t sc;
    float cycles;

    hen_cost_default(&c); hen_sweep_cost_default(&sc);
    cycles = t->mode == HENR_EYES ? hen_stats_cycles(&t->st, &c) : hen_sweep_cycles(&t->sst, &sc);
    return (unsigned)(cycles / (float)(t->steps ? t->steps : 1ul));
}

static void counts_text(const tally_t *t, char *out, size_t size) {
    if (t->mode == HENR_EYES)
        snprintf(out, size, "/100 steps: tick %u ray %u box %u tri %u pair %u", per100(t->st.ticks, t->steps),
                 per100(t->st.ray_fan + t->st.ray_down + t->st.ray_sight, t->steps), per100(t->st.box, t->steps), per100(t->st.tri, t->steps), per100(t->st.pair, t->steps));
    else
        snprintf(out, size, "/100 steps: cast %u tri %u rej %u cell %u pair %u", per100(t->sst.casts, t->steps), per100(t->sst.tri, t->steps),
                 per100(t->sst.reject, t->steps), per100(t->sst.cell, t->steps), per100(t->sst.pair, t->steps));
}

static void labels(const view_t *v, const tally_t *t) {
    const unsigned long s = t->steps ? t->steps : 1ul;
    const unsigned pre = (unsigned)(t->cyc_pre / s), post = (unsigned)(t->cyc_post / s);
    char line[65];

    text_set(0u, "%s  %u hens  %u rays  tick %u ms%s", mode_name[v->mode], (unsigned)n_of[v->ni], (unsigned)rays_of[v->ri], (unsigned)(tick_of[v->ti] * 1000.0f + 0.5f),
             world_ok ? "" : "  NO WORLD");
    text_set(1u, "pre %u cyc/step (worst %u) post+sep %u = %u us", pre, t->pre_worst, post, (unsigned)(((unsigned long long)(pre + post) * 1000ull) / CYC_PER_MS));
    counts_text(t, line, sizeof line);
    text_set(2u, "%s", line);
    text_set(3u, "X mode Y hens A rays drawn B tick up/down fan START pages");
}

/* ---- start, stop ------------------------------------------------------------------- */

static int start(void) {
    pvr_init_params_t init = pvr_default_params;

    init.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_0;
    init.vertex_buf_size = 768 * 1024;
    init.autosort_disabled = 1;
    init.opb_overflow_count = 4;
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return 0; }
    pvr_set_bg_color(AO4_BACKDROP_R, AO4_BACKDROP_G, AO4_BACKDROP_B);
    text_mem = pvr_mem_malloc(sizeof text_px);
    if (!text_mem) { say("video memory allocation FAIL"); pvr_shutdown(); return 0; }
    headers();
    memset(text_ln, 0, sizeof text_ln);
    text_dirty = 1;
    text_upload();
    vid_set_dithering(false);
    rd_prev = asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_evt, NULL);
    rd_hooked = rd_prev.hdl != NULL;
    if (!rd_hooked) asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, NULL, NULL);
    return 1;
}

static void stop(const char *stage) {
    step(stage);
    if (rd_hooked) { asic_evt_set_handler(ASIC_EVT_PVR_RENDERDONE_TSP, rd_prev.hdl, rd_prev.data); rd_hooked = 0; }
    vid_set_dithering(true);
    pvr_shutdown();
}

/* ---- the loop: the run that counts, and the live view ------------------------------------------ */

#define AUTO_STAGES 10u
static const uint8_t auto_stage[AUTO_STAGES][2] = { { 0, HENR_EYES }, { 0, HENR_SWEEP_FRAME }, { 0, HENR_SWEEP_TICK }, { 0, HENR_SWEEP_NAIVE },     /* hens (n_of's index), mode */
                                                    { 2, HENR_EYES }, { 2, HENR_SWEEP_FRAME }, { 2, HENR_SWEEP_TICK }, { 2, HENR_SWEEP_NAIVE },
                                                    { 3, HENR_EYES }, { 3, HENR_SWEEP_FRAME } };
static tally_t auto_tally[AUTO_STAGES];
static unsigned auto_done;

/* Frames until the run is over (auto_run 1) or START is pressed (0). */
static void loop(int auto_run) {
    view_t v;
    tally_t t;
    unsigned held = ~0u, frames = 0, run_frames = 0, stage = 0;
    unsigned long long run_cyc = 0;
    uint64_t t_prev = ns(), t_stage = ns(), run_pvr = 0;

    memset(&v, 0, sizeof v);
    v.ri = 1u; v.ti = 1u; v.debug = 1;
    if (auto_run) { v.ni = auto_stage[0][0]; v.mode = auto_stage[0][1]; auto_done = 0; }
    apply(&v);
    memset(&t, 0, sizeof t);
    tally_take(&t, 0u, 0ull, 0ull);
    labels(&v, &t);
    text_upload();
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;
        const uint64_t t_now = ns();
        const unsigned long long c0 = cyc();
        const float dt = (float)(t_now - t_prev) * 1.0e-9f;
        int changed = 0;

        step("hens frame ");
        t_prev = t_now; held = now;
        v.t += dt < 0.1f ? dt : 0.1f;
        if (st) {
            if (st->joyx > 24 || st->joyx < -24) v.turn += (float)st->joyx * (1.0f / 128.0f) * dt * 1.2f;
            if (st->joyy > 24 || st->joyy < -24) v.lift -= (float)st->joyy * (1.0f / 128.0f) * dt * 4.0f;
            if (v.lift < -4.5f) v.lift = -4.5f;
            if (v.lift > 7.0f) v.lift = 7.0f;
            if (v.turn > 0.6f) v.turn = 0.6f;
            if (v.turn < -0.6f) v.turn = -0.6f;
        }
        if (auto_run) {
            if (t_now - t_stage > (uint64_t)HENS_AUTO_S * 1000000000ull) {
                tally_take(&auto_tally[stage], run_frames, run_cyc, run_pvr);
                t_stage = t_now; stage++; auto_done = stage; changed = 1;
                if (stage >= AUTO_STAGES) break;
                v.ni = auto_stage[stage][0]; v.mode = auto_stage[stage][1];
            }
        } else {
            if ((hit & CONT_START) && frames > 10u) break;
            if (hit & CONT_X) { v.mode = (v.mode + 1u) % HENR_MODES; changed = 1; }
            if (hit & CONT_Y) { v.ni = (v.ni + 1u) % 4u; changed = 1; }
            if (hit & CONT_A) { v.debug = !v.debug; }
            if (hit & CONT_B) { v.ti = (v.ti + 1u) % 4u; changed = 1; }
            if (hit & CONT_DPAD_UP) { v.ri = (v.ri + 1u) % 3u; changed = 1; }
            if (hit & CONT_DPAD_DOWN) { v.ri = (v.ri + 2u) % 3u; changed = 1; }
        }
        if (changed) { apply(&v); run_frames = 0; run_cyc = 0; run_pvr = 0; }
        else if (world_ok) henr_advance(&hr, dt < 0.1f ? dt : 0.1f);
        if (!frame(&v)) { say("%s: NO RENDER END at frame %u (%s, %u hens)", auto_run ? "auto" : "live", frames, mode_name[v.mode], (unsigned)n_of[v.ni]); break; }
        frames++; run_frames++;
        run_cyc += cyc() - c0; run_pvr += last_render_ns;
        if (changed || (run_frames % 30u) == 0u) {      /* the text: when the view changes, and the run's means every 30 frames */
            tally_take(&t, run_frames, run_cyc, run_pvr);
            labels(&v, &t);
            text_upload();                  /* the render is done (scene_close): nothing reads the texture now */
        }
    }
}

/* ---- the run ---------------------------------------------------------------------- */

static int fault_on;

static void report(void) {
    pg_plot_t *g;
    unsigned i;
    char rev[32], line[65];

    suite_rev_text(rev, sizeof rev);
    say("hens %s: rays against a sweep, the same hens and room.", HENS_STR(HEN_TAG));
    say("%s. Cycles a step of 1/60 s, all hens (PRFC1).", rev);
    say("pre: the eyes or the sweep. post+sep: the push, hens apart.");
    say("model: the modules' cost tables on the counts, ESTIMATE.");
    say("world %u objects %u triangles; grid 16x16, %u items", hr.objs, hr.world.ntri, hr.grid_items);
    say("9 rays + 2 down, tick 100 ms; sweeps look as far as the eyes");
    if (!world_ok) say("NO WORLD from the room: nothing was steered");
    if (auto_done < AUTO_STAGES) say("THE RUN DID NOT FINISH: %u of %u stages", auto_done, AUTO_STAGES);
    for (i = 0; i < auto_done; i++) {
        const tally_t *t = &auto_tally[i];
        const unsigned long s = t->steps ? t->steps : 1ul;
        const unsigned f = t->frames ? t->frames : 1u;
        const unsigned frame_us = (unsigned)(t->cyc_frame / f * 1000ull / CYC_PER_MS);

        say("%2u %s: pre %u worst %u; post+sep %u", t->n, mode_name[t->mode], (unsigned)(t->cyc_pre / s), t->pre_worst, (unsigned)(t->cyc_post / s));
        counts_text(t, line, sizeof line);
        say("  %s", line);
        say("  model %u; %lu steps %u frames %u.%u ms, PVR %u us", model_of(t), t->steps, t->frames, frame_us / 1000u, frame_us % 1000u / 100u,
            (unsigned)(t->pvr_ns / f / 1000u));
    }
    g = pg_plot("hens: steering, cycles a step of 1/60 s", "hens, mode: E eyes, G grid, N naive", "cycles", 1);
    for (i = 0; i < auto_done; i++) {
        char cat[12];

        snprintf(cat, sizeof cat, "%u %s", auto_tally[i].n & 63u, mode_short[auto_tally[i].mode]);
        pg_cat(g, cat);
    }
    pg_series(g, "pre, mean");
    for (i = 0; i < auto_done; i++) pg_point(g, (float)i, (float)(auto_tally[i].cyc_pre / (auto_tally[i].steps ? auto_tally[i].steps : 1ul)));
}

int main(void) {
    unsigned page = 0, pages, held = ~0u;
    int redraw = 1;

    suite_init();                           /* the suite disc: the menu's binary into RAM, before anything else */
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    perf_cntr_start(PRFC1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
    ao4_build_room(&room);
    ao4_light(&room, AO4_BAKED, AO4_CLAY, 0, room_col);
    world_ok = henr_init(&hr);
    if (start()) {
        if (!fault_on) { fault_on = 1; if (psm_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused"); }
        loop(1);
        stop("auto end");
    }
    report();
    vid_set_mode(DM_640x480, PM_RGB565);
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;

        psm_fault_beat(&crumbs);
        held = now;
        pages = pg_pages();
        if (hit & CONT_A) { page = (page + 1u) % pages; redraw = 1; }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; redraw = 1; }
        else if (hit & CONT_START) {
            if (start()) { loop(0); stop("live end"); }
            vid_set_mode(DM_640x480, PM_RGB565);
            held = ~0u;
            redraw = 1;
        }
        else if (suite_tick(now, page, pages)) { page++; redraw = 1; }
        if (redraw) { pg_draw(page % pages, "HENS EYES", suite_keys_text("START live view")); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
