/* SH4DER-PVR: result pages as QR codes, the reading test (image psm-qr-q1), Dreamcast

   demo/psm/qr/qr_spike.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   Does a phone read a QR code off a console's picture on a television, and at which module size? This program shows
   known payloads as codes at 10, 8, 6 and 4 pixels a module (qr_page.h has the set), and the same payloads as text
   pages, which are the fallback. Nothing is measured by the console: the result is what the phone reads.

   The codes are made by Project Nayuki's QR Code generator library (MIT License), used as a library and not in this
   repository: see qr_page.h and ../CREDITS.md.

   Pages: the text pages of ../psm_page.h first (what this is, every payload as text, each page with its CRC), then one
   page a set of codes. A / B turn the page. Y changes the level of the light panel under the codes (232, 255, 176 of
   255): try another when a code does not read.

   The PVR is not started: everything is drawn by the SH-4 into the frame buffer. No wait in this file is longer than
   the 20 ms between two reads of the pad. The fault catcher (../psm_fault.h) is in. Nothing is written but the frame
   buffer.
*/

#include <kos.h>
#include <stdarg.h>

#define PSM_FAULT_IMPL
#include "../psm_fault.h"
#include "../psm_suite.h"
#include "qrcodegen.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#ifndef QR_TAG
#define QR_TAG untagged                     /* the build gives the image's tag, a bare word */
#endif
#define QR_STR_(x) #x
#define QR_STR(x) QR_STR_(x)

#define PG_PREFIX "qr"
#define PG_LINES_MAX 200u
#include "../psm_page.h"
#include "qr_page.h"
#define say pg_say

static psm_fault_crumbs_t crumbs;

static void text_pages(void) {
    char payload[QR_PAYLOAD_MAX + 1u];
    uint8_t code[QR_BUF];
    unsigned s, k, bad = 0;

    say("TEST QR READING  %s: no number here is measured by the console", QR_STR(QR_TAG));
    say("The pages after the text pages show QR codes. Point a phone's camera at each and note which codes it reads; then photograph each page.");
    say("Every code is level M, byte mode, full: version 3 holds 42 bytes, version 5 holds 84. A code's text is");
    say("  q1 <id> <crc>|<data>   id: version-pixels-letter");
    say("crc: CRC-32 of data. tools/qr_check.py checks what the phone gives.");
    say("Y changes the panel under the codes: 232, 255, 176 of 255.");
    say("Sets: V3 at 10, 8, 6, 4 px (1, 1, 2, 6 codes); V5 at 8, 6, 4 px (1, 1, 4 codes).");
    say("Encoder: Project Nayuki's QR Code generator library, MIT License.");
    say("The payloads as text (the fallback; a version 5 payload is two lines):");
    for (s = 0; s < QR_SETS; s++) {
        for (k = 0; k < qr_sets[s].count; k++) {
            psm_fault_beat(&crumbs);
            qr_fill(&qr_sets[s], k, payload);
            bad += !qr_encode(&qr_sets[s], payload, code);
            say("%.42s", payload);
            if (qr_capacity(qr_sets[s].ver) > 42u) say("%s", payload + 42);
        }
    }
    say("RESULT QR.1 codes the encoder refused: %u of 16", bad);
}

int main(void) {
    static const uint8_t level[3] = { 232, 255, 176 };
    unsigned page = 0, pages, held = ~0u, lv = 0;
    int redraw = 1;

    (void)pg_plot; (void)pg_series; (void)pg_point;        /* the page kit's graphs: not used here */
    suite_init();                          /* the suite disc: the menu's binary into RAM, before anything else */
    vid_set_mode(DM_640x480, PM_RGB565);
    if (psm_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused");
    pg_init();
    text_pages();
    pg_extra_pages = QR_SETS;
    pages = pg_pages() + QR_SETS;
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;

        psm_fault_beat(&crumbs);
        crumbs.frame++;
        held = now;
        if (hit & CONT_A) { page = (page + 1u) % pages; redraw = 1; }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; redraw = 1; }
        else if (hit & CONT_Y) { lv = (lv + 1u) % 3u; redraw = 1; }
        else if (suite_tick(now, page, pages)) { page++; redraw = 1; }
        if (redraw) {
            psm_fault_stage(&crumbs, "qr draw");
            if (page < pg_pages()) pg_draw(page, "QR q1", suite_keys_text("(then the codes)"));
            else if (!qr_draw(&qr_sets[page - pg_pages()], page, pages, level[lv])) pg_text(PG_X0, 480u - PG_Y0 - PG_CH, PG_WHITE, "ENCODER REFUSED A CODE");
            psm_fault_stage(&crumbs, "qr idle");
            redraw = 0;
        }
        thd_sleep(20);
    }
    return 0;
}
