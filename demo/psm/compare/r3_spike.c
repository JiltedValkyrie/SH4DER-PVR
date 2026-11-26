/* SH4DER-PVR: the R3 page (the punch-through product), the Dreamcast test program

   demo/psm/compare/r3_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A test program for a real console, written to ../recipes/R3_PAGE_SPEC.md. It settles one open point (PVR-064): how
   the PVR rounds "texture alpha x vertex alpha" before the punch-through test against the reference. c4's test 6
   showed the tested alpha is that product; four rounding rules are left (../recipes/recipes.h: VTX256, BOTH256, ROUND,
   FLOOR), and three renders tell them apart.

   Every line is one quad of 256 x 128 pixels in the punch-through list, through c4's texture (ARGB4444, 256 x 8, not
   twiddled, alpha nibble = column / 16, white), KallistiOS's own header for the list (never blend ONE / ZERO, which
   writes the failing pixels too: PVR-061), one vertex alpha and one reference a line. The corner of the frame is read
   back through the uncached frame buffer and counted (c4's punch_pass).

     page 1   the deciding lines A, B, C, D, F, then C and D again: n (nibbles drawn), lo (the first drawn nibble),
              px (pixels drawn), s (1: a clean step). A and B repeat c4's two counts (16,384 and 2,048): anything
              else and the run is void. The last line is the rule the lines name, worked out by this program.
     page 2+  the fade curve at reference 16: vertex alpha 0, 8 .. 248, 255, the nibbles drawn at each, and which of
              the four rules fit all 33 rows.
     last     the picture: the quad at vertex alpha 16, 24, 40, 64, 120, 240 (reference 16), 256 x 64 each, as read
              back from the frame buffer, with its alpha beside it.

   Only PVR registers and video memory are written. The fault catcher (include/sh4der_fault.h) is in; every wait is bounded.
   Every number comes from the run that prints it; only a real console's run is evidence. The numbers the host model
   expects are printed beside each line, marked "model": they are ESTIMATE.
   Build: python demo/psm/compare/build.py --spike r3 --tag <new tag> --sh4zam <checkout>. Not run on a console when
   this file was written.
*/

#include <kos.h>
#include <stdarg.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"
#include "../psm_suite.h"
#include "r3_model.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#ifndef R3_TAG
#define R3_TAG untagged                     /* the build gives the image's tag, a bare word */
#endif
#define R3_STR_(x) #x
#define R3_STR(x) R3_STR_(x)

/* ---- the result pages ----------------------------------------------------------- */

#define PG_PREFIX "r3"
#define PG_LINES_MAX 200u
#include "../../../include/sh4der_page.h"
#define say pg_say
#define page_break pg_break
static sh4der_fault_crumbs_t crumbs;

static void step(const char *name) {
    sh4der_fault_beat(&crumbs);
    sh4der_fault_stage(&crumbs, name);
    crumbs.frame++;
}

/* ---- the pass (c4's punch_pass) ----------------------------------------------------- */

static pvr_ptr_t a4_mem;
static pvr_poly_hdr_t h_pt;
static pvr_vertex_t quad[4] __attribute__((aligned(32)));
static uint16_t tex16[256u * 8u] __attribute__((aligned(32)));
static uint16_t scr_px[R3_W * R3_H];

#define PIC_ROWS 6u
#define PIC_H 64u
static const uint8_t pic_alpha[PIC_ROWS] = { 16, 24, 40, 64, 120, 240 };
static uint16_t pic_px[PIC_ROWS][R3_W * PIC_H];

/* A rectangle as four vertices of a strip: x 0 .. 256, y 0 .. h, the texture's whole width at the middle of its
   height, white with vertex alpha a. */
static void rect(float h, unsigned a) {
    unsigned i;

    for (i = 0; i < 4u; i++) {
        quad[i].flags = i == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        quad[i].x = (i & 2u) ? (float)R3_W : 0.0f; quad[i].y = (i & 1u) ? 0.0f : h; quad[i].z = 1.0f;
        quad[i].u = (i & 2u) ? 1.0f : 0.0f; quad[i].v = 0.5f;
        quad[i].argb = (a << 24) | 0x00ffffffu; quad[i].oargb = 0u;
    }
}

/* Wait until the PVR takes a new scene: KallistiOS's wait, 100 ms a try, 20 tries. */
static int wait_ready(void) {
    unsigned t;

    for (t = 0; t < 20u && pvr_wait_ready() < 0; t++) sh4der_fault_beat(&crumbs);
    return t < 20u;
}

/* One punch-through pass onto the screen with reference ref. scr_px gets the 256 x 128 corner. 0 when the frame did
   not come back. */
static int punch_pass(unsigned ref) {
    const volatile uint16_t *fb;
    unsigned x, y, t;

    sh4der_fault_beat(&crumbs);
    PVR_SET(PVR_PT_ALPHA_REF, ref);
    if (!wait_ready()) return 0;
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_PT_POLY);
    pvr_prim(&h_pt, sizeof h_pt);
    pvr_sq_load(NULL, quad, sizeof quad, PVR_DMA_TA);
    pvr_list_finish();
    PVR_SET(PVR_PT_ALPHA_REF, ref);
    pvr_scene_finish();
    if (!wait_ready()) return 0;
    for (t = 0; t < 20u && pvr_wait_render_done() < 0; t++) sh4der_fault_beat(&crumbs);
    if (t == 20u) return 0;
    fb = (const volatile uint16_t *)(uintptr_t)(0xa5000000u | (PVR_GET(PVR_RENDER_ADDR) & 0x00ffffffu));
    for (y = 0; y < R3_H; y++) for (x = 0; x < R3_W; x++) scr_px[y * R3_W + x] = fb[y * 640u + x];
    return 1;
}

/* ---- the run ---------------------------------------------------------------------- */

static int start(void) {
    pvr_init_params_t init = pvr_default_params;
    pvr_poly_cxt_t c;
    unsigned tu, tv;

    init.opb_sizes[PVR_LIST_OP_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_TR_POLY] = PVR_BINSIZE_16;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_PT_POLY] = PVR_BINSIZE_16;
    init.vertex_buf_size = 256 * 1024;
    if (pvr_init(&init) < 0) { say("pvr_init FAIL"); return 0; }
    pvr_set_bg_color(0.0f, 0.0f, 1.0f);
    a4_mem = pvr_mem_malloc(sizeof tex16);
    if (!a4_mem) { say("video memory allocation FAIL"); pvr_shutdown(); return 0; }
    for (tv = 0; tv < 8u; tv++) for (tu = 0; tu < 256u; tu++) tex16[tv * 256u + tu] = (uint16_t)(((tu >> 4) << 12) | 0x0fffu);   /* alpha column / 16, white */
    pvr_txr_load(tex16, a4_mem, sizeof tex16);
    pvr_poly_cxt_txr(&c, PVR_LIST_PT_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, 256, 8, a4_mem, PVR_FILTER_NEAREST);
    c.gen.culling = PVR_CULLING_NONE;                            /* otherwise KallistiOS's own settings for the list */
    pvr_poly_compile(&h_pt, &c);
    vid_set_dithering(false);
    return 1;
}

static void run(void) {
    static const char *const id[R3_LINES] = { "A ", "B ", "C ", "D ", "F ", "C2", "D2" };
    r3_line_t l[R3_LINES];
    unsigned k, i, r, fits[4] = { 1, 1, 1, 1 }, rule, clean = 1;
    static uint8_t fade_n[R3_FADE];
    char lo[4], cell[3][32];

    say("TEST R3 PT PRODUCT  %s: numbers below are measured by this run", R3_STR(R3_TAG));
    if (!start()) return;
    if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused");
    say("(a: vertex alpha, r: reference, n: nibbles drawn, lo: first drawn");
    say(" nibble, px: pixels of 32768, s: 1 a clean step;");
    say(" model: n by VTX256 BOTH256 ROUND FLOOR, ESTIMATE)");
    for (k = 0; k < R3_LINES; k++) {
        step("r3 line");
        rect((float)R3_H, r3_alpha[k]);
        if (!punch_pass(r3_ref[k])) { say("%s NO RENDER END", id[k]); goto out; }
        r3_derive(scr_px, &l[k]);
        if (l[k].lo < 16u) snprintf(lo, sizeof lo, "%02u", l[k].lo); else strcpy(lo, "--");
        say("RESULT %s a%03u r%03u n%02u lo%s px%05u s%u   (model n %u %u %u %u)", id[k], r3_alpha[k], r3_ref[k], l[k].n, lo, l[k].px, l[k].step,
            r3_model_n(r3_alpha[k], r3_ref[k], RCP_PT_VTX256), r3_model_n(r3_alpha[k], r3_ref[k], RCP_PT_BOTH256),
            r3_model_n(r3_alpha[k], r3_ref[k], RCP_PT_ROUND), r3_model_n(r3_alpha[k], r3_ref[k], RCP_PT_FLOOR));
    }
    rule = r3_rule(l[R3_C].n, l[R3_D].n, l[R3_F].n);
    if (!r3_valid(l)) say("RUN VOID: A, B, a step or a repeat is off");
    say("DERIVED %s%s", rule == RCP_PT_N ? "" : "RULE ", r3_rule_name(rule));

    say("TEST E FADE reference 16: a, n, s (and px if odd); model n by VTX256 BOTH256 ROUND FLOOR");
    for (i = 0; i < R3_FADE; i++) {
        const unsigned a = r3_fade_alpha(i);
        r3_line_t e;

        step("r3 fade");
        rect((float)R3_H, a);
        if (!punch_pass(16u)) { say("E a%03u NO RENDER END", a); goto out; }
        r3_derive(scr_px, &e);
        clean &= e.step;
        fade_n[i] = (uint8_t)e.n;
        if (e.step && e.px == 2048u * e.n) snprintf(cell[i % 3u], sizeof cell[0], "a%03u n%02u s1", a, e.n);
        else snprintf(cell[i % 3u], sizeof cell[0], "a%03u n%02u s%u px%05u", a, e.n, e.step, e.px);
        if (i % 3u == 2u || i + 1u == R3_FADE) {                  /* three rows of the curve a line */
            say("RESULT E %s%s%s%s%s", cell[0], i % 3u >= 1u ? "   " : "", i % 3u >= 1u ? cell[1] : "", i % 3u == 2u ? "   " : "", i % 3u == 2u ? cell[2] : "");
        }
        for (r = 0; r < 4u; r++) if (r3_model_n(a, 16u, RCP_PT_VTX256 + r) != e.n) fits[r] = 0u;
    }
    say("DERIVED E rules that fit all %u rows (1 = fits): VTX256 %u BOTH256 %u ROUND %u FLOOR %u%s", R3_FADE, fits[0], fits[1], fits[2], fits[3], clean ? "" : "; A ROW WAS NOT A STEP");
    {
        pg_plot_t *g = pg_plot("E FADE ref 16: nibbles drawn against vertex alpha", "vertex alpha", "nibbles drawn of 16", 0);

        pg_series(g, "MEASURED this run"); for (i = 0; i < R3_FADE; i++) pg_point(g, (float)r3_fade_alpha(i), (float)fade_n[i]);
        for (r = 0; r < 3u; r++) {
            pg_series(g, r == 0u ? "model VTX256" : r == 1u ? "model BOTH256" : "model ROUND (and FLOOR)");
            for (i = 0; i < R3_FADE; i++) pg_point(g, (float)r3_fade_alpha(i), (float)r3_model_n(r3_fade_alpha(i), 16u, RCP_PT_VTX256 + r));
        }
    }

    for (i = 0; i < PIC_ROWS; i++) {       /* the picture: 256 x 64 a row, kept for the last page */
        step("r3 picture");
        rect((float)PIC_H, pic_alpha[i]);
        if (!punch_pass(16u)) { say("picture a%03u NO RENDER END", pic_alpha[i]); goto out; }
        memcpy(pic_px[i], scr_px, sizeof pic_px[i]);
    }
out:
    step("done");
    vid_set_dithering(true);
    pvr_shutdown();
}

/* ---- the pages ---------------------------------------------------------------------- */

/* The pages of psm_page.h (text, then the fade graph), and one more: the picture. */
static void page_draw(unsigned page, unsigned pages) {
    unsigned i, x, y;

    if (page + 1u < pages) { pg_draw(page, "R3", suite_keys_text("(last page: the picture)")); return; }
    vid_clear(0, 0, 0);
    {
        char head[PG_COLS + 1u];

        snprintf(head, sizeof head, "R3 p%u/%u PICTURE A/B  reference 16; blue: not drawn", page + 1u, pages);
        pg_text(PG_X0, PG_Y0, PG_WHITE, head);
    }
    /* the six rows as they were read back (the top 56 of each row's 64 lines, to stay in the title-safe area), the
       quad's left edge at x 128 */
    for (i = 0; i < PIC_ROWS; i++) {
        char label[16];
        const unsigned top = 72u + i * 60u;

        snprintf(label, sizeof label, "a %3u", pic_alpha[i]);
        pg_text(PG_X0, top + 20u, PG_WHITE, label);
        for (y = 0; y < 56u; y++) for (x = 0; x < R3_W; x++) vram_s[(top + y) * 640u + 128u + x] = pic_px[i][y * R3_W + x];
    }
}

int main(void) {
    unsigned page = 0, pages, held = ~0u;
    int redraw = 1;

    suite_init();                          /* the suite disc: the menu's binary into RAM, before anything else */
    run();
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    pg_extra_pages = 1u;                   /* the picture */
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;

        sh4der_fault_beat(&crumbs);
        held = now;
        pages = pg_pages() + 1u;                                   /* the text pages, the graph, and the picture */
        if (hit & CONT_A) { page = (page + 1u) % pages; redraw = 1; }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; redraw = 1; }
        else if (suite_tick(now, page, pages)) { page++; redraw = 1; }
        if (redraw) { page_draw(page, pages); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
