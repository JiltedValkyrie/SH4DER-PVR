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
   Status:     ALPHA v0.4 (wiki/status/maturity.md, "suite menu"). A start from the menu repeated Shadow chain with
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
    uint32_t chain, index, status, spare;
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

#ifdef _arch_dreamcast

static void *suite_menu_image;              /* the menu's binary, in RAM since suite_init */
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
    suite_exec(suite_menu_image, suite_menu_size);
}

#ifdef SH4DER_FAULT_IMPL
static void suite_fault_leave_(void) {
    suite_menu_mail->chain = 0u;            /* a fault ends a chained run: the owner looks before going on */
    suite_menu_mail->index = suite_mail.index;
    suite_menu_mail->status = SUITE_FAULT;
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
static int suite_tick(unsigned buttons, unsigned page, unsigned pages) {
    const uint64_t now = timer_ms_gettime64();

    if (!suite_present()) return 0;
    if ((buttons & SUITE_KEYS_ALL) == SUITE_KEYS_ALL) suite_exit(SUITE_KEYS);
    if (page != suite_last_page) { suite_last_page = page; suite_turned_ms = now; }
    if (!suite_chained() || now - suite_turned_ms < SUITE_PAGE_MS) return 0;
    if (page + 1u >= pages) suite_exit(SUITE_OK);
    return 1;
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
