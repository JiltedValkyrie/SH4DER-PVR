/* SH4DER-PVR: the test suite disc: a test's way back to the menu, and the menu's way into a test

   include/sh4der_suite.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   One disc holds a menu program and every test program as a file (the menu's source is demo/psm/suite/suite_menu.c
   in this repository, and it stays a demo). The menu loads a test's binary and starts it in its own place
   (KallistiOS's arch_exec); the test, when it is done, loads the menu's binary and starts that the same way. So the
   console is never switched off between tests.

   A program built with this file is still a whole program: on a disc of its own there is no menu file, suite_init()
   finds none, and every call here does nothing. The plain part (suite_mail_find, the mark, suite_keys_text,
   suite_rev_text, suite_present = 0 and suite_chained = 0 on a host build) compiles and runs on the host; the
   start, the return and the page timer are Dreamcast only.

   RECOMMENDED (no console run of this file's own start, return or chain: nothing here is measured by that; the arch_exec
   facts below are read from KallistiOS's source, NOT MEASURED. The console facts are the suite NOTES and the ledger rows
   named in wiki/effects/suite.md)
   Cost:       at suite_init, one read of the menu's binary into RAM, kept until the program leaves (its size is the
               menu's file size: NOT MEASURED here); a chained page turn is one timer read. VRAM 0. No frame cost.
   Start with: suite_init() first in main, after <kos.h> (and after sh4der_fault.h when the test has it); suite_tick in
               the result-page loop; one suite list for the disc (demo/psm/suite/build.py --list)
   Status:     suite_page, the hold of A and the place in the spare word are v0.5: host tested (test/suite_test.c),
               NOT RUN on a console. The rest:
               ALPHA v0.4 (wiki/status/maturity.md, "suite menu"). A start from the menu repeated Shadow chain with
               measured edges (PVR-116, one test, not the same binary as its own disc). Every started test's result pages
               carry "ABXY menu" (MEASURED, demo/psm/suite/NOTES.md, the first run of s1). OPEN: the four-button return,
               the 5 s countdown, a chained test going back by itself, and the fault page's return (no photo; NOTES)
   Do:         keep one suite_mail_t in the program (the mark is 16 bytes, the words 16 more); call suite_tick once per
               pass of the result-page loop
   Do not:     expect RAM outside the new program's image and .bss to be what the last program left (the heap holds what
               was there); change the mark text (the menu and every test match on it); leave SH4DER_FAULT_TEST in a build
               for players (its fault page can return to the menu and may hang)
   Maturity:   ALPHA v0.4

   WHAT A START THROUGH arch_exec IS (KallistiOS source, kernel/arch/dreamcast/kernel/exec.c and init.c; read, not
   measured). The leaving program runs KallistiOS's whole shutdown (sound, maple, CD, G2 DMA, video, the vertical blank
   list, every ASIC event mask to 0, the timers, the PVR: reset, its events unhooked, its DMA shut, video memory
   cleared; then the file systems and the threads), puts back the SR, VBR and FPSCR it was started with, turns the
   cache off and invalidates it (CCR = 0x808, so OC-RAM mode and the index bits go too), copies the new binary to
   0x8c010000 and jumps there. The new program then starts as from a boot: its own cache setting, its .bss cleared,
   every KallistiOS subsystem started again, its own KOS_INIT_FLAGS and its own PVR parameters (vertex buffer size).
   NOT cleared by that: main RAM outside the new program's image and .bss (the heap holds what was there), the sound
   RAM, PVR registers that neither vid_init nor pvr_init writes (the fog table, the palette), the drive's state, the
   real-time clock, what a VMU shows. A test that reads any of those before it writes them reads the last program's.

   THE MAIL. The two programs pass a few words by writing them into the other's image before it is started: each
   holds one suite_mail_t in its data, found by its 16-byte mark. Nothing is kept anywhere else, so nothing depends on
   RAM that the start leaves alone.
     chain   1: "run all": the test turns its own pages (SUITE_PAGE_MS each) and goes back to the menu after the
             last; the menu then starts the next one.
     index   the menu's row of the test (the menu puts its cursor back there).
     status  how the test ended: SUITE_OK, SUITE_KEYS (the four buttons), SUITE_FAULT (from the fault page).

   IN A TEST. After <kos.h>, and after sh4der_fault.h when the test has it:
       suite_init();                              once, early in main: loads the menu's binary while all is well
       ... in the loop that shows the result pages, every pass:
       if (suite_tick(buttons, page, pages)) { page++; redraw = 1; }
   suite_tick does not come back when A + B + X + Y are all held, or when a chained run has shown the last page for
   its time: it goes to the menu. suite_chained() is 1 in a chained run: skip what waits for a button.

   THE PAGE LOOP IN ONE CALL (v0.5, NOT RUN on a console). suite_page does the page turning as well, so a test's loop
   has one line for it and every test turns its pages the same way:
       if (suite_page(buttons, &page, pages)) redraw = 1;
     A        the next page (the first after the last). On the LAST page of a test started from the menu: a tap is
              still the next page, and A held for SH4DER_MENU_HOLD_MS (2 s) goes back to the menu, on the row the test
              was started from, with the menu's tab and scroll as they were. A line under the page says so, and a bar
              fills while A is held; letting go before the time stays on the page.
     B        the page before.
     A+B+X+Y  the menu, from any page, as suite_tick.
   On a disc of the test's own there is no menu: A and B turn the pages and nothing else happens. The decisions are
   suite_pager_step, plain C, host tested (test/suite_test.c); suite_page is the Dreamcast part round it (the clock,
   the line and the bar in the frame buffer, the start of the menu). The line and the bar are drawn at y = 434 .. 453,
   under the page kit's last row (432): a full last page keeps all of its lines.
   THE PLACE. The menu puts its place (tab, row, scroll: one word, sh4der_menu_pack) in the mail's spare word when it
   starts a test; suite_exit hands it back. A test built before v0.5 does not hand it back: the menu then opens on the
   test's row by its index, as before.
   With sh4der_fault.h (SH4DER_FAULT_IMPL) in the same file, suite_init also has the fault page go back to the menu after
   SUITE_FAULT_SHOWS redraws (60 s). That is a try from a broken program: it may hang, and then the console is
   switched off as before.
*/

#ifndef SH4DER_SUITE_H
#define SH4DER_SUITE_H

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "sh4der_menu.h"                    /* the hold of A (tap or held), the place in the spare word */

#ifndef CONT_A  /* KallistiOS's controller masks (kernel/arch/dreamcast/include/dc/maple/controller.h). A host build has none. */
#define CONT_A 0x0004
#define CONT_B 0x0002
#define CONT_X 0x0400
#define CONT_Y 0x0200
#endif

#define SUITE_MENU_FILE "/cd/MENU.BIN"
#define SUITE_PAGE_MS 20000u                /* a chained run: this long a result page */
#define SUITE_FAULT_SHOWS 120u              /* the fault page: redraws (one each half second) before it leaves */
#define SUITE_KEYS_ALL (CONT_A | CONT_B | CONT_X | CONT_Y)
#define SUITE_IMAGE_MAX (12u << 20)         /* a binary larger than this is not one of ours */

enum { SUITE_OK = 0, SUITE_KEYS = 1, SUITE_FAULT = 2 };
#define SUITE_NO_INDEX 0xffffffffu

typedef struct suite_mail {
    char mark[16];
    uint32_t chain, index, status, spare;   /* spare: the menu's place (sh4der_menu_pack), handed back as it came */
} suite_mail_t;

#define SUITE_MARK { 'P', 'S', 'M', '-', 'S', 'U', 'I', 'T', 'E', '-', 'M', 'A', 'I', 'L', '-', '1' }

/* This program's own mail: written into its image by whoever started it. Never const, never optimised away. */
static volatile suite_mail_t suite_mail __attribute__((used, aligned(4))) = { SUITE_MARK, 0u, SUITE_NO_INDEX, 0u, 0u };

/* The mail inside an image: the place of the one mark, or NULL when there is none or more than one. */
static suite_mail_t *suite_mail_find(void *image, size_t size) {
    uint8_t *p = image;
    suite_mail_t *found = NULL;
    char mark[16];
    size_t i;

    for (i = 0; i < 16u; i++) mark[i] = suite_mail.mark[i];
    for (i = 0; i + sizeof(suite_mail_t) <= size; i += 4u)
        if (p[i] == (uint8_t)mark[0] && !memcmp(p + i, mark, 16u)) {
            if (found) return NULL;
            found = (suite_mail_t *)(void *)(p + i);
        }
    return found;
}

/* ---- the page loop's decisions (plain C) --------------------------------------------------------------------- */

enum { SUITE_PG_NONE = 0,                   /* nothing to do */
       SUITE_PG_TURN,                       /* *page changed (or a hold ended on a tap): draw the page again */
       SUITE_PG_HOLD,                       /* A is held on the last page: *permille is how far, draw the bar */
       SUITE_PG_LEAVE_KEYS,                 /* go to the menu: A + B + X + Y */
       SUITE_PG_LEAVE_HOLD,                 /* go to the menu: A was held for its time on the last page */
       SUITE_PG_LEAVE_CHAIN };              /* go to the menu: a chained run showed its last page for its time */

typedef struct suite_pager {
    unsigned started, held, last_page;
    uint32_t turned_ms;
    sh4der_menu_hold_t hold;
} suite_pager_t;

static inline void suite_pager_turned_(suite_pager_t *p, unsigned page, uint32_t now_ms, unsigned buttons) {
    p->last_page = page;
    p->turned_ms = now_ms;
    sh4der_menu_hold_reset(&p->hold, (buttons & CONT_A) != 0u);     /* A still down does nothing until it is let go */
}

/* One pass of a result-page loop. buttons: the pad's now (KallistiOS's masks). now_ms: any clock in ms. menu: 1 when
   there is a menu to go back to; chained: 1 in a chained run. *page is changed for SUITE_PG_TURN. A button that is
   down on the first pass does nothing until it is let go (the test's own last button is not a page turn). */
static inline int suite_pager_step(suite_pager_t *p, unsigned buttons, uint32_t now_ms, unsigned *page, unsigned pages, int menu, int chained,
                                   unsigned *permille) {
    const unsigned n = pages ? pages : 1u;
    unsigned hit, last;

    if (!p->started) { p->started = 1u; p->held = buttons; suite_pager_turned_(p, *page, now_ms, buttons); }
    hit = buttons & ~p->held;
    p->held = buttons;
    if (menu && (buttons & SUITE_KEYS_ALL) == SUITE_KEYS_ALL) return SUITE_PG_LEAVE_KEYS;
    if (*page != p->last_page) suite_pager_turned_(p, *page, now_ms, buttons);     /* the test turned the page itself */
    last = *page + 1u >= n;
    if (hit & CONT_B) {
        *page = (*page + n - 1u) % n;
        suite_pager_turned_(p, *page, now_ms, buttons);
        return SUITE_PG_TURN;
    }
    if (menu && !chained && last) {         /* the last page: a tap turns, a hold leaves */
        switch (sh4der_menu_hold_step(&p->hold, (buttons & CONT_A) != 0u, now_ms, SH4DER_MENU_HOLD_MS, permille)) {
        case SH4DER_MENU_HOLD_TAP:     *page = (*page + 1u) % n; suite_pager_turned_(p, *page, now_ms, buttons); return SUITE_PG_TURN;
        case SH4DER_MENU_HOLD_HOLDING: return SUITE_PG_HOLD;
        case SH4DER_MENU_HOLD_FIRED:   return SUITE_PG_LEAVE_HOLD;
        default: break;
        }
    } else if (hit & CONT_A) {
        *page = (*page + 1u) % n;
        suite_pager_turned_(p, *page, now_ms, buttons);
        return SUITE_PG_TURN;
    }
    if (menu && chained && now_ms - p->turned_ms >= SUITE_PAGE_MS) {
        if (last) return SUITE_PG_LEAVE_CHAIN;
        *page += 1u;
        suite_pager_turned_(p, *page, now_ms, buttons);
        return SUITE_PG_TURN;
    }
    return SUITE_PG_NONE;
}

/* The bar's text for a hold `permille` far: "Hold A ... 1.2 s of 2 s" (tenths of a second, no float). */
static inline void suite_hold_text(char *out, size_t size, unsigned permille) {
    const unsigned tenths = SH4DER_MENU_HOLD_MS * (permille > 1000u ? 1000u : permille) / 100000u;

    snprintf(out, size, "Hold A ... %u.%u s of %u s", tenths / 10u, tenths % 10u, SH4DER_MENU_HOLD_MS / 1000u);
}

#define SUITE_HINT_TEXT "Last page. Hold A for 2 s: back to the menu, on this demo."
#define SUITE_BAR_X0 64                     /* the line and the bar: the title-safe columns, under the page's last row */
#define SUITE_BAR_X1 575
#define SUITE_BAR_Y0 434
#define SUITE_BAR_Y1 453

/* The bar's filled part ends at this x (SUITE_BAR_X0: nothing filled; SUITE_BAR_X1 - 1: all of it). */
static inline int suite_bar_fill_x(unsigned permille) {
    const int inner = SUITE_BAR_X1 - SUITE_BAR_X0 - 1;

    return SUITE_BAR_X0 + (int)((unsigned)inner * (permille > 1000u ? 1000u : permille) / 1000u);
}

static inline void suite_bar_rect_(uint16_t *fb, int x0, int y0, int x1, int y1, uint16_t c) {
    int x, y;

    for (y = y0; y <= y1; y++) for (x = x0; x <= x1; x++) fb[y * 640 + x] = c;
}

/* The place of the line and the bar in a 640 x 480 RGB565 frame, without their text (the caller draws it at
   SUITE_BAR_X0 + 8, SUITE_BAR_Y0 + 2): black for the line; for a hold a grey border, the filled part in blue, the
   rest dark. */
static inline void suite_bar_paint(uint16_t *fb, int holding, unsigned permille) {
    suite_bar_rect_(fb, SUITE_BAR_X0, SUITE_BAR_Y0, SUITE_BAR_X1, SUITE_BAR_Y1, 0x0000u);
    if (!holding) return;
    suite_bar_rect_(fb, SUITE_BAR_X0, SUITE_BAR_Y0, SUITE_BAR_X1, SUITE_BAR_Y1, 0x94f5u);                   /* the border, grey */
    suite_bar_rect_(fb, SUITE_BAR_X0 + 1, SUITE_BAR_Y0 + 1, SUITE_BAR_X1 - 1, SUITE_BAR_Y1 - 1, 0x2988u);   /* not yet filled */
    if (suite_bar_fill_x(permille) > SUITE_BAR_X0)                                                          /* filled, blue */
        suite_bar_rect_(fb, SUITE_BAR_X0 + 1, SUITE_BAR_Y0 + 1, suite_bar_fill_x(permille), SUITE_BAR_Y1 - 1, 0x2b38u);
}

#ifdef _arch_dreamcast

static void *suite_menu_image;            /* the menu's binary, in RAM since suite_init */
static size_t suite_menu_size;
static suite_mail_t *suite_menu_mail;
static uint64_t suite_turned_ms;
static unsigned suite_last_page = ~0u;

/* A binary from the disc into RAM, its size rounded up to 4. NULL when it is not there or is not one of ours. */
static void *suite_load(const char *path, size_t *size) {
    const file_t f = fs_open(path, O_RDONLY);
    uint8_t *p;
    ssize_t total, got = 0, n;

    if (f < 0) return NULL;
    total = fs_total(f);
    if (total < 1024 || (size_t)total > SUITE_IMAGE_MAX) { fs_close(f); return NULL; }
    p = memalign(32, ((size_t)total + 35u) & ~(size_t)31u);
    if (!p) { fs_close(f); return NULL; }
    memset(p + ((size_t)total & ~(size_t)3u), 0, 4u);
    while (got < total && (n = fs_read(f, p + got, (size_t)(total - got))) > 0) got += n;
    fs_close(f);
    if (got != total) { free(p); return NULL; }
    *size = ((size_t)total + 3u) & ~(size_t)3u;
    return p;
}

static inline int suite_present(void) { return suite_menu_image != NULL; }
static inline int suite_chained(void) { return suite_present() && suite_mail.chain != 0u; }

/* Start an image in this program's place. The PVR is shut down here with interrupts still on; KallistiOS's own
   shutdown does the rest. Does not come back. */
static void __attribute__((noreturn)) suite_exec(void *image, size_t size) {
    pvr_shutdown();
    arch_exec(image, (uint32_t)size);
    for (;;) {}
}

/* Back to the menu. Comes back only on a disc that has no menu. */
static void suite_exit(unsigned status) {
    if (!suite_present()) return;
    suite_menu_mail->chain = suite_mail.chain;
    suite_menu_mail->index = suite_mail.index;
    suite_menu_mail->status = status;
    suite_menu_mail->spare = suite_mail.spare;          /* the menu's place, as the menu gave it */
    suite_exec(suite_menu_image, suite_menu_size);
}

#ifdef SH4DER_FAULT_IMPL
static void suite_fault_leave_(void) {
    suite_menu_mail->chain = 0u;            /* a fault ends a chained run: the owner looks before going on */
    suite_menu_mail->index = suite_mail.index;
    suite_menu_mail->status = SUITE_FAULT;
    suite_menu_mail->spare = suite_mail.spare;
    arch_exec(suite_menu_image, (uint32_t)suite_menu_size);
}
#endif

/* Once, early: the menu's binary into RAM. 1 on a suite disc, 0 on a disc of the test's own. */
static int suite_init(void) {
    suite_menu_image = suite_load(SUITE_MENU_FILE, &suite_menu_size);
    if (!suite_menu_image) return 0;
    suite_menu_mail = suite_mail_find(suite_menu_image, suite_menu_size);
    if (!suite_menu_mail) { free(suite_menu_image); suite_menu_image = NULL; return 0; }
#ifdef SH4DER_FAULT_IMPL
    sh4der_fault_leave(suite_fault_leave_, SUITE_FAULT_SHOWS);
#endif
    return 1;
}

/* Every pass of a result-page loop. Goes to the menu when the four buttons are held, or when a chained run has shown
   the last page for its time. Returns 1 when a chained run's page time is up and there is a next page. */
static int __attribute__((unused)) suite_tick(unsigned buttons, unsigned page, unsigned pages) {
    const uint64_t now = timer_ms_gettime64();

    if (!suite_present()) return 0;
    if ((buttons & SUITE_KEYS_ALL) == SUITE_KEYS_ALL) suite_exit(SUITE_KEYS);
    if (page != suite_last_page) { suite_last_page = page; suite_turned_ms = now; }
    if (!suite_chained() || now - suite_turned_ms < SUITE_PAGE_MS) return 0;
    if (page + 1u >= pages) suite_exit(SUITE_OK);
    return 1;
}

/* The line and the bar under the last page, into the frame buffer (640 x 480 RGB565, as the page kit draws). The
   text is the BIOS font reduced 3 to 2 (8 x 16, the page kit's size and weights), mixed over what is under it in
   shifts; this file draws it itself so it does not matter whether sh4der_page.h is included, or where. */
static void suite_bar_text_(const char *s) {
    static uint16_t src[24u * 12u];
    unsigned x = (unsigned)SUITE_BAR_X0 + 8u, i, j;

    for (; *s && x + 8u <= (unsigned)SUITE_BAR_X1; s++, x += 8u) {
        const unsigned c = (unsigned char)*s;

        if (c <= 32u || c > 126u) continue;
        memset(src, 0, sizeof src);
        bfont_draw(src, 12u, true, c);
        for (j = 0; j < 16u; j++)
            for (i = 0; i < 8u; i++) {
                const unsigned sx = (i / 2u) * 3u + (i & 1u), sy = (j / 2u) * 3u + (j & 1u);
                const unsigned wx0 = (i & 1u) ? 1u : 2u, wy0 = (j & 1u) ? 1u : 2u;
                uint16_t *p = &vram_s[((unsigned)SUITE_BAR_Y0 + 2u + j) * 640u + x + i];
                unsigned v = 0;

                v += wy0 * wx0 * (src[sy * 12u + sx] != 0u);
                v += wy0 * (3u - wx0) * (src[sy * 12u + sx + 1u] != 0u);
                v += (3u - wy0) * wx0 * (src[(sy + 1u) * 12u + sx] != 0u);
                v += (3u - wy0) * (3u - wx0) * (src[(sy + 1u) * 12u + sx + 1u] != 0u);
                v *= 57u;                   /* 9 is full cover; twice as bright, as the page kit's glyphs */
                if (!v) continue;
                if (v >= 255u) { *p = 0xffffu; continue; }
                {
                    const int k = (int)(v + (v >> 7)), br = *p >> 11, bg = (*p >> 5) & 63, bb = *p & 31;

                    *p = (uint16_t)(((br + (((31 - br) * k) >> 8)) << 11) | ((bg + (((63 - bg) * k) >> 8)) << 5) | (bb + (((31 - bb) * k) >> 8)));
                }
            }
    }
}

static void suite_bar_draw_(int holding, unsigned permille) {
    char s[48];

    suite_bar_paint(vram_s, holding, permille);
    if (!holding) { suite_bar_text_(SUITE_HINT_TEXT); return; }
    suite_hold_text(s, sizeof s, permille);
    suite_bar_text_(s);
}

static suite_pager_t suite_pager_;
static unsigned suite_line_off_;

/* After the page loop was left for a while (a live view started from a page): the buttons that are down now are not
   presses, as on the loop's first pass. */
static void __attribute__((unused)) suite_page_forget(void) { suite_pager_.started = 0u; }

/* on = 0: no steady line under the last page (a last page that is a picture or a code a camera reads: the line would
   be drawn into it). The bar still shows while A is held. Default: on. */
static void __attribute__((unused)) suite_page_line(int on) { suite_line_off_ = !on; }

/* The page loop in one call (the head of this file). Returns 1 when the page is to be drawn again. Does not come
   back when it goes to the menu. The line under the last page is drawn on the pass after a redraw and then every
   25 passes, so a page the test drew again for its own reasons gets it back. */
static int __attribute__((unused)) suite_page(unsigned buttons, unsigned *page, unsigned pages) {
    static unsigned hint_in, was_holding;
    const int menu = suite_present(), chained = suite_chained();
    unsigned permille = 0;
    const int ev = suite_pager_step(&suite_pager_, buttons, (uint32_t)timer_ms_gettime64(), page, pages, menu, chained, &permille);

    switch (ev) {
    case SUITE_PG_LEAVE_KEYS:  suite_exit(SUITE_KEYS); break;
    case SUITE_PG_LEAVE_HOLD:
    case SUITE_PG_LEAVE_CHAIN: suite_exit(SUITE_OK); break;
    default: break;
    }
    if (ev == SUITE_PG_TURN) { hint_in = 1u; was_holding = 0u; return 1; }
    if (!menu || chained || *page + 1u < (pages ? pages : 1u)) return 0;
    if (ev == SUITE_PG_HOLD) { suite_bar_draw_(1, permille); was_holding = 1u; return 0; }
    if (was_holding) { was_holding = 0u; return 1; }            /* the hold was dropped without a tap: the page again, without the bar */
    if (suite_line_off_) return 0;
    if (hint_in) hint_in--;
    if (!hint_in) { suite_bar_draw_(0, 0u); hint_in = 25u; }
    return 0;
}

#else

static inline int suite_present(void) { return 0; }   /* a host build: no menu, no disc */
static inline int suite_chained(void) { return 0; }   /* a chained run needs a menu: 1 only with one */

#endif /* _arch_dreamcast */

/* What a result page's header says about the way out, for pg_draw's keys. */
static inline const char *suite_keys_text(const char *alone) { return suite_present() ? (suite_chained() ? "ABXY menu (auto)" : "ABXY menu") : alone; }

/* The console's chip revisions as text, "HOLLY REV xxxx SB xx": the PVR core's REVISION register (0x005F8004) and the
   system bus's SB_SBREV (0x005F689C), read through the uncached mirror. Two reads, nothing written; the PVR need not
   be started. For the menu's foot and a test's first line, so a photo says which console it was. A host build has no
   registers: it writes "HOLLY REV ---- SB --". */
static inline void suite_rev_text(char *out, size_t size) {
#ifdef _arch_dreamcast
    const uint32_t holly = *(volatile uint32_t *)0xa05f8004u, sb = *(volatile uint32_t *)0xa05f689cu;

    snprintf(out, size, "HOLLY REV %04X SB %02X", (unsigned)(holly & 0xffffu), (unsigned)(sb & 0xffu));
#else
    snprintf(out, size, "HOLLY REV ---- SB --");
#endif
}

#endif /* SH4DER_SUITE_H */
