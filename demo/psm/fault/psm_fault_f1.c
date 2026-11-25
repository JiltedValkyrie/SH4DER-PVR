/* SH4DER-PVR: the fault catcher's own test program

   demo/psm/fault/psm_fault_f1.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   A small program for a real console that does nothing but run frames and, on a button, die in a chosen way, so
   that the screens of ../psm_fault.h can be seen before the catcher goes into a demo:

     A      a 32-bit write to an odd address      -> *** EXCEPTION ***  ADDRESS ERROR (WRITE) at 8c000001
     B      the SH-4's undefined instruction      -> *** EXCEPTION ***  ILLEGAL INSTRUCTION
     X      the main loop spins, interrupts on    -> *** HANG *** after two seconds, PC inside the spin
     Y      the main thread sleeps for ever       -> *** HANG *** after two seconds, MAIN state 3
     START  the program returns from main         -> *** EXIT ***

   It draws with the PVR as a demo does (a moving bar and a texture of instructions), fills the breadcrumbs every
   frame, and calls psm_fault_init after pvr_init: the same use a demo makes of it. Every button is a test hook.
   Build: python demo/psm/fault/build.py. Not run on a console when this file was written.
*/

#include <kos.h>

#define PSM_FAULT_IMPL
#define PSM_FAULT_TEST
#include "../psm_fault.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define TEXT_W 512u
#define TEXT_H 128u

static psm_fault_crumbs_t crumbs;
static uint16_t text_px[TEXT_W * TEXT_H] __attribute__((aligned(32)));
static pvr_ptr_t text_mem;
static pvr_poly_hdr_t hdr_text, hdr_col;

static void quad(pvr_poly_hdr_t *h, float x0, float y0, float x1, float y1, float u1, float v1, uint32_t argb) {
    pvr_vertex_t v;
    unsigned i;

    pvr_prim(h, sizeof *h);
    memset(&v, 0, sizeof v);
    v.argb = argb;
    v.z = 1.0f;
    for (i = 0; i < 4u; i++) {
        v.flags = i == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = (i & 2u) ? x1 : x0; v.y = (i & 1u) ? y0 : y1;
        v.u = (i & 2u) ? u1 : 0.0f; v.v = (i & 1u) ? 0.0f : v1;
        pvr_prim(&v, sizeof v);
    }
}

static int load(void) {
    static const char *const lines[7] = {
        "FAULT CATCHER TEST f1   every button stops the program",
        "A      address error    (EXCEPTION screen)",
        "B      illegal instruction (EXCEPTION screen)",
        "X      spin for ever    (HANG screen after 2 s)",
        "Y      sleep for ever   (HANG screen after 2 s)",
        "START  return from main (EXIT screen)",
        "the bar moves while frames are running",
    };
    pvr_poly_cxt_t c;
    unsigned i;

    text_mem = pvr_mem_malloc(sizeof text_px);
    if (!text_mem) return 0;
    minifont_set_color(255, 255, 255);
    for (i = 0; i < 7u; i++) minifont_draw_str(text_px + (4u + 16u * i) * TEXT_W + 8u, TEXT_W, lines[i]);
    pvr_txr_load(text_px, text_mem, sizeof text_px);
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)TEXT_W, (int)TEXT_H, text_mem, PVR_FILTER_NEAREST);
    pvr_poly_compile(&hdr_text, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);
    pvr_poly_compile(&hdr_col, &c);
    return 1;
}

int main(void) {
    uint32_t frame = 0;

    pvr_init_defaults();
    if (psm_fault_init(&crumbs) < 0) printf("psm_fault_f1: a hook was refused\n");
    if (!load()) { printf("psm_fault_f1: no video memory\n"); return 1; }
    pvr_set_bg_color(0.0f, 0.0f, 0.25f);

    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const uint32_t pad = st ? st->buttons : 0u;
        const float bar = (float)(frame % 480u);

        psm_fault_beat(&crumbs);
        psm_fault_frame(&crumbs, frame, pad, 0u, 0u);
        psm_fault_camera(&crumbs, bar, 1.7f, -0.5f * bar, (float)(frame % 360u), -5.0f);
        psm_fault_menu(&crumbs, 0, 0u, 0);
        psm_fault_counts(&crumbs, 0u, 0u, 8u, 4u);

        psm_fault_stage(&crumbs, "input");
        if (frame > 30u) {                                       /* TEST HOOKS: each of these ends the program on purpose */
            if (pad & CONT_A) psm_fault_test(PSM_FAULT_TEST_ADDRESS);
            if (pad & CONT_B) psm_fault_test(PSM_FAULT_TEST_ILLEGAL);
            if (pad & CONT_X) psm_fault_test(PSM_FAULT_TEST_SPIN);
            if (pad & CONT_Y) psm_fault_test(PSM_FAULT_TEST_BLOCK);
            if (pad & CONT_START) return 0;
        }

        psm_fault_stage(&crumbs, "draw");
        pvr_wait_ready();
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        quad(&hdr_text, 64.0f, 64.0f, 64.0f + (float)TEXT_W, 64.0f + (float)TEXT_H, 1.0f, 1.0f, 0xffffffffu);
        quad(&hdr_col, 64.0f + bar, 240.0f, 96.0f + bar, 272.0f, 0.0f, 0.0f, 0xffffc000u);
        pvr_list_finish();
        psm_fault_stage(&crumbs, "finish");
        pvr_scene_finish();
        frame++;
    }
    return 0;
}
