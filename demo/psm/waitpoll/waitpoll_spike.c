/* SH4DER-PVR: the render-done wait, sleeping against polled. Dreamcast test
   demo/psm/waitpoll/waitpoll_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Every demo and library pass that waits for the PVR before it rewrites a texture or a render target is one of the
   shapes below. Each shape runs twice, 120 frames each: with KallistiOS's sleeping pvr_wait_ready + pvr_wait_render_done,
   with sh4der_pvrx_wait_idle (the pvrx hook on, every scene ended through sh4der_pvrx_scene_finish), and with the
   sleeping waits plus the hook's immediate wake (sh4der_pvrx_wait_hook(2): the render-done handler calls thd_schedule,
   Falco Girgis's idea). Same scene, same frames, one switch. T5 and T6 wait right after the pass is finished, while its
   render runs: the shape where the sleeping wait stalls (route A).

     T1 tex/1 light   rewrite a 128 x 128 texture every frame (the ocean, cloud and torch walk programs' shape)
     T2 tex/6 light   the same one frame in six (matchlight, the town: PVR-136 says polling gains nothing here)
     T3 tex/1 heavy   T1 with 4 full-screen translucent layers added (a scene the PVR takes longer over)
     T4 tex/6 heavy   T2 with the same heavy scene
     T5 rtt/1 light   a render target pass every frame through the library's pl4nar target, then the screen samples it
     T6 rtt/1 heavy   T5 with the heavy screen scene

   Output: one text page of results, then a line graph of the frame time of every counted frame for each test (sleeping
   and polled drawn together), then bar graphs of mean frame, worst frame and the wait's own time. Timings are frames
   20 .. 83 for the lines and 20 .. 119 for the means: frame start to start on the SH-4 clock, one console, one run.
   Nothing here reads the picture back: a polled wait that returned early would show as a torn or wrong texture, not as
   a number, so the picture is for the owner's eye. The timeout counter (tmo) is the only wrong-wait number.

   Start, on the last page, runs the whole test again (a fresh pvr_init, a second run to compare).
   Build: python demo/psm/waitpoll/build.py --tag <new tag> [--elf-only]. A plain ELF: it also runs from the suite's menu. */

#include <kos.h>
#include <stdarg.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "sh4der_suite.h"
#include "sh4der_pl4nar.h"
#include "sh4der_pvrx.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#ifndef WP_TAG
#define WP_TAG untagged
#endif
#ifndef WP_LIB
#define WP_LIB unknown
#endif
#ifndef WP_DIFF
#define WP_DIFF unknown
#endif
#define WP_STR_(x) #x
#define WP_STR(x) WP_STR_(x)

#define PG_PREFIX "wp"
#define PG_LINES_MAX 120u
#include "sh4der_page.h"

#define say pg_say
static sh4der_fault_crumbs_t crumbs;

#define TESTS    6u
#define MODES    2u                         /* 0 sleeping, 1 sleeping with the handler's thd_schedule (the polled wait was dropped) */
#define STAGES   (TESTS * MODES)
#define FRAMES   120u
#define SKIP     20u                        /* frames before the means start */
#define LINEPTS  64u                        /* PG_POINTS: frames SKIP .. SKIP + 63 are drawn */
#define TEX      128u
#define HEAVY_LAYERS 4u

static inline uint64_t ns(void) { const uint64_t n = timer_ns_gettime64(); return n - n / 1000000000ull * 2502400ull; }   /* KallistiOS's clock, its reload step taken out */

typedef struct {
    const char *name;
    unsigned period;                        /* the texture (or the target) is rewritten every period-th frame */
    int heavy;                              /* HEAVY_LAYERS full-screen translucent layers in the screen scene */
    int rtt;                                /* 1: the rewrite is a render target pass (the library's pl4nar target), 0: a texture load */
    int after;                              /* 1: the wait comes right after the pass is finished, while its render still runs (route A's shape) */
} test_t;

static const test_t test[TESTS] = {
    { "T1 tex/1 light", 1, 0, 0, 0 },
    { "T2 tex/6 light", 6, 0, 0, 0 },
    { "T3 tex/1 heavy", 1, 1, 0, 0 },
    { "T4 tex/6 heavy", 6, 1, 0, 0 },
    { "T5 pass+wait lt", 1, 0, 1, 1 },
    { "T6 pass+wait hv", 1, 1, 1, 1 },
};

typedef struct {
    int ran;
    unsigned frame_us[FRAMES];              /* frame start to start */
    unsigned wait_sum_us, wait_n;           /* the wait's own time, over the counted frames that waited */
    unsigned mean_us, max_us, slow;         /* over frames SKIP .. FRAMES - 1; slow: frames over 20,000 us */
    unsigned long timeouts;
} result_t;
static result_t res[STAGES];                /* stage = test * MODES + mode (0 sleeping, 1 woken at once) */

static uint16_t src_a[TEX * TEX] __attribute__((aligned(32)));
static uint16_t src_b[TEX * TEX] __attribute__((aligned(32)));
static pvr_ptr_t tex;
static sh4der_pl4nar_target_t rt;
static pvr_poly_hdr_t h_col, h_tex, h_rt, h_blend;
static pvr_vertex_t vbuf[4] __attribute__((aligned(32)));

static void vtx(pvr_vertex_t *v, float x, float y, float z, float u, float w, uint32_t argb, int eol) {
    v->flags = eol ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    v->x = x; v->y = y; v->z = z; v->u = u; v->v = w; v->argb = argb; v->oargb = 0u;
}

static void quad(float x0, float y0, float x1, float y1, float z, uint32_t argb) {
    vtx(&vbuf[0], x0, y0, z, 0.0f, 0.0f, argb, 0);
    vtx(&vbuf[1], x1, y0, z, 1.0f, 0.0f, argb, 0);
    vtx(&vbuf[2], x0, y1, z, 0.0f, 1.0f, argb, 0);
    vtx(&vbuf[3], x1, y1, z, 1.0f, 1.0f, argb, 1);
    pvr_prim(vbuf, sizeof vbuf);
}

static void headers(void) {
    pvr_poly_cxt_t c;

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&h_col, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)TEX, (int)TEX, tex, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&h_tex, &c);
    if (rt.tex) {
        pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)rt.w, (int)rt.h, (pvr_ptr_t)rt.tex, PVR_FILTER_NEAREST);
        c.gen.culling = PVR_CULLING_NONE;
        pvr_poly_compile(&h_rt, &c);
    }
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&h_blend, &c);
}

static void fill(void) {
    unsigned i;

    for (i = 0; i < TEX * TEX; i++) {
        const unsigned x = i % TEX, y = i / TEX;

        src_a[i] = (uint16_t)(((x * 31u / TEX) << 11) | ((y * 63u / TEX) << 5) | 8u);
        src_b[i] = (uint16_t)(((y * 31u / TEX) << 11) | ((x * 63u / TEX) << 5) | 20u);
    }
}

/* The shape of every client's wait: one call, sleeping or polled. */
static unsigned wait_now(unsigned mode) {
    const uint64_t t0 = ns();

    if (mode == 1u) sh4der_pvrx_wait_idle();    /* with the hook on: KallistiOS's sleeping waits, woken at once by the handler */
    else { pvr_wait_ready(); pvr_wait_render_done(); }
    return (unsigned)((ns() - t0) / 1000u);
}

static void target_pass(unsigned f) {
    if (!sh4der_pl4nar_target_begin(&rt)) return;
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&h_col, sizeof h_col);
    quad(0.0f, 0.0f, (float)rt.w, (float)rt.h, 1.0f, 0xff204060u);
    quad(8.0f + (float)(f % 64u), 16.0f, 56.0f + (float)(f % 64u), 64.0f, 2.0f, 0xffe0a030u);
    pvr_list_finish();
    sh4der_pl4nar_target_end();             /* counted for the polled wait when the hook is on */
}

static void screen_scene(const test_t *t, unsigned f) {
    unsigned i;

    pvr_wait_ready();
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    pvr_prim(&h_col, sizeof h_col);
    quad(0.0f, 0.0f, 640.0f, 480.0f, 1.0f, 0xff2a2a30u);
    pvr_prim(t->rtt ? &h_rt : &h_tex, sizeof h_tex);
    quad(64.0f, 64.0f + (float)(f % 32u), 320.0f, 320.0f + (float)(f % 32u), 3.0f, 0xffffffffu);
    pvr_list_finish();
    if (t->heavy) {
        pvr_list_begin(PVR_LIST_TR_POLY);
        pvr_prim(&h_blend, sizeof h_blend);
        for (i = 0; i < HEAVY_LAYERS; i++)
            quad(0.0f, 0.0f, 640.0f, 480.0f, 4.0f + (float)i, 0x18000000u | (0x2030a0u + i * 0x302010u));
        pvr_list_finish();
    }
    sh4der_pvrx_scene_finish();             /* every scene, screen and target, so the polled wait counts it */
}

static void step(const char *name) {
    sh4der_fault_beat(&crumbs);
    sh4der_fault_stage(&crumbs, name);
    crumbs.frame++;
}

static void run_stage(unsigned k) {
    const test_t *t = &test[k / MODES];
    const unsigned mode = k % MODES;
    result_t *r = &res[k];
    unsigned f;
    uint64_t prev = 0, sum = 0;
    unsigned long tmo0;

    step(t->name);
    sh4der_pvrx_wait_hook(0);               /* drain and unhook between stages, then hook for the woken stage */
    wait_now(0);
    sh4der_pvrx_wait_hook(mode == 0u ? 0 : 2);
    wait_now(mode);                         /* a clean start: nothing of the last stage in flight */
    tmo0 = sh4der_pvrx_wait_timeouts();
    memset(r, 0, sizeof *r);
    for (f = 0; f < FRAMES; f++) {
        const uint64_t now = ns();

        if (f) r->frame_us[f] = (unsigned)((now - prev) / 1000u);
        prev = now;
        if (f % t->period == 0u) {
            unsigned w;

            if (t->rtt && t->after) {
                target_pass(f);             /* the pass's render runs now: the wait starts inside it */
                w = wait_now(mode);
            } else {
                w = wait_now(mode);
                if (t->rtt) target_pass(f);
                else pvr_txr_load(((f / t->period) & 1u) ? src_b : src_a, tex, TEX * TEX * 2u);
            }
            if (f >= SKIP) { r->wait_sum_us += w; r->wait_n++; }
        }
        screen_scene(t, f);
        step(t->name);
    }
    for (f = SKIP; f < FRAMES; f++) {
        sum += r->frame_us[f];
        if (r->frame_us[f] > r->max_us) r->max_us = r->frame_us[f];
        if (r->frame_us[f] > 20000u) r->slow++;
    }
    r->mean_us = (unsigned)(sum / (FRAMES - SKIP));
    r->timeouts = sh4der_pvrx_wait_timeouts() - tmo0;
    r->ran = 1;
}

static unsigned wait_mean(const result_t *r) { return r->wait_n ? r->wait_sum_us / r->wait_n : 0u; }

static const char *const mode_name[MODES] = { "sleep", "wake" };

static void bars(const char *title, const char *unit, unsigned (*pick)(const result_t *)) {
    unsigned k, m;
    char cn[12];
    pg_plot_t *p = pg_plot(title, "test", unit, 1);

    for (m = 0; m < MODES; m++) {
        pg_series(p, mode_name[m]);
        for (k = 0; k < TESTS; k++) pg_point(p, (float)k, (float)pick(&res[k * MODES + m]) * 0.001f);
    }
    for (k = 0; k < TESTS; k++) { snprintf(cn, sizeof cn, "T%u", k + 1u); pg_cat(p, cn); }
}

static unsigned pick_mean(const result_t *r) { return r->mean_us; }
static unsigned pick_max(const result_t *r) { return r->max_us; }
static unsigned pick_wait(const result_t *r) { return wait_mean(r); }

static void report(unsigned done) {
    unsigned k, m, f;
    pg_plot_t *p;
    char name[28];

    pg_nlines = 0; pg_nplots = 0;
    say("RENDER-DONE WAIT: sleeping against woken at once");
    say("build %s  library HEAD %s  working tree diff %s", WP_STR(WP_TAG), WP_STR(WP_LIB), WP_STR(WP_DIFF));
    say("stages run %u of %u. One console, one run: MEASURED only where", done, STAGES);
    say("a line says a number. Nothing reads the picture back.");
    say("sleep: pvr_wait_ready + pvr_wait_render_done (KallistiOS).");
    say("wake:  the same sleeping waits, the pvrx hook: the render-done handler");
    say("       also calls thd_schedule(true) (no spin). PROTOTYPE.");
    say("T5, T6: the wait comes right after the pass is finished, while");
    say("its render runs. T1 to T4 wait at the top of the frame.");
    say("");
    say("MEAN frame, us        sleep     wake");
    for (k = 0; k < TESTS; k++)
        if (res[k * MODES].ran && res[k * MODES + 1u].ran)
            say("%-15s %8u %8u", test[k].name, res[k * MODES].mean_us, res[k * MODES + 1u].mean_us);
    say("WORST frame, us");
    for (k = 0; k < TESTS; k++)
        if (res[k * MODES].ran && res[k * MODES + 1u].ran)
            say("%-15s %8u %8u", test[k].name, res[k * MODES].max_us, res[k * MODES + 1u].max_us);
    say("TIME INSIDE THE WAIT, us");
    for (k = 0; k < TESTS; k++)
        if (res[k * MODES].ran && res[k * MODES + 1u].ran)
            say("%-15s %8u %8u", test[k].name, wait_mean(&res[k * MODES]), wait_mean(&res[k * MODES + 1u]));
    say("FRAMES OVER 20 ms (of %u)", FRAMES - SKIP);
    for (k = 0; k < TESTS; k++)
        if (res[k * MODES].ran && res[k * MODES + 1u].ran)
            say("%-15s %8u %8u", test[k].name, res[k * MODES].slow, res[k * MODES + 1u].slow);
    say("");
    say("mean: frame start to start, frames %u..%u (60 Hz = 16,667 us).", SKIP, FRAMES - 1u);
    say("The graphs: frames %u..%u of each test, two lines, then three", SKIP, SKIP + LINEPTS - 1u);
    say("bar graphs over all six tests. NOT SHOWN: a heavy game scene,");
    say("the picture (a torn texture would show to the eye only).");

    for (k = 0; k < TESTS; k++) {
        snprintf(name, sizeof name, "%s frame ms", test[k].name);
        p = pg_plot(name, "frame", "ms", 0);
        for (m = 0; m < MODES; m++) {
            pg_series(p, mode_name[m]);
            for (f = SKIP; f < SKIP + LINEPTS; f++) pg_point(p, (float)f, (float)res[k * MODES + m].frame_us[f] * 0.001f);
        }
    }
    bars("Mean frame by test", "ms", pick_mean);
    bars("Worst frame by test", "ms", pick_max);
    bars("Time inside the wait", "ms", pick_wait);
}

static int fault_done;

static unsigned run_all(void) {
    unsigned k, done = 0;

    memset(res, 0, sizeof res);
    if (pvr_init_defaults() < 0) return 0;
    pvr_set_bg_color(0.1f, 0.1f, 0.12f);
    vid_set_dithering(false);
    if (!fault_done) { fault_done = 1; if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused"); }   /* once: each init adds a vblank handler and an atexit */
    tex = pvr_mem_malloc(TEX * TEX * 2u);
    sh4der_pl4nar_target_init(&rt, 128u, 128u);
    fill();
    headers();
    for (k = 0; k < STAGES; k++) {
        if (!tex) break;
        run_stage(k);
        done++;
    }
    step("wp end");
    sh4der_pvrx_wait_hook(1);
    wait_now(1);
    sh4der_pl4nar_target_free(&rt);
    if (tex) pvr_mem_free(tex);
    tex = NULL;
    sh4der_pvrx_wait_hook(0);               /* Call order: pvrx step 7, before pvr_shutdown */
    vid_set_dithering(true);
    pvr_shutdown();
    return done;
}

int main(void) {
    unsigned page = 0, pages, prev_buttons = 0u;
    int redraw = 1;

    suite_init();                           /* on the suite's disc: the menu's binary into RAM, before anything else */
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    report(run_all());
    vid_set_mode(DM_640x480, PM_RGB565);
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u;

        sh4der_fault_beat(&crumbs);
        pages = pg_pages();
        if ((now & CONT_START) && !(prev_buttons & CONT_START) && page % pages == pages - 1u) {   /* Start on the last page: run it all again */
            report(run_all());
            vid_set_mode(DM_640x480, PM_RGB565);
            page = 0; redraw = 1;
        } else if (suite_page(now, &page, pages)) redraw = 1;
        prev_buttons = now;
        if (redraw) { pg_draw(page % pages, "WAIT POLL", suite_keys_text("A/B pages, Start on last: rerun")); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
