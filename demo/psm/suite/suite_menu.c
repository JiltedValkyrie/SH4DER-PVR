/* SH4DER-PVR: the test suite disc's menu, Dreamcast

   demo/psm/suite/suite_menu.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   One disc, every test: this program is what the disc boots, and what every test comes back to (../psm_suite.h has
   how, and what a start through arch_exec does and does not clear). It lists the tests of suite_list.h (written by
   build.py from its table, with what the build checked of each binary), loads the one picked from the disc and starts
   it in its own place.

     up / down   pick            A   start the test
     RUN ALL     the tests marked with a number, in that order, each turning its own result pages (20 s a page) and
                 coming back here; B during the 5 s between two tests stops the run
   A test goes back to this menu when A + B + X + Y are held on its result pages. A test marked NO RETURN was built
   without ../psm_suite.h: the console is switched off after it.

   The PVR is not started here: the SH-4 draws into the frame buffer with ../psm_page.h's text, inside the title-safe
   area. Nothing is written but the frame buffer and RAM. The fault catcher (../psm_fault.h) is in. The waits: 20 ms
   between two reads of the pad, and the disc read of a test (KallistiOS's own, a few seconds).
*/

#include <kos.h>
#include <stdarg.h>

#define PSM_FAULT_IMPL
#include "../psm_fault.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define PG_PREFIX "suite"
#define PG_LINES_MAX 8u
#include "../psm_page.h"

typedef struct suite_entry {
    const char *file, *name, *what, *read;
    unsigned order;                         /* place in RUN ALL, 1 ..; 0: not in it */
    unsigned returns;                       /* 1: built with psm_suite.h (the build found its mail) */
    unsigned bytes;
} suite_entry_t;

#include "suite_list.h"                     /* SUITE_TAG, suite_list[], SUITE_N: written by build.py */
#include "../psm_suite.h"

#define PG_YELLOW PG_RGB(255, 220, 0)
#define ROW_Y(r) (PG_Y0 + PG_CH * (r))

static psm_fault_crumbs_t crumbs;
static char note[PG_COLS + 1u];

static void line(unsigned row, uint16_t col, const char *fmt, ...) {
    char s[PG_COLS + 1u];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s, sizeof s, fmt, ap);
    va_end(ap);
    pg_text(PG_X0, ROW_Y(row), col, s);
}

/* The entry after place `order` in RUN ALL, or -1. */
static int next_in_order(unsigned order) {
    int best = -1;
    unsigned i;

    for (i = 0; i < SUITE_N; i++)
        if (suite_list[i].order > order && suite_list[i].returns && (best < 0 || suite_list[i].order < suite_list[best].order)) best = (int)i;
    return best;
}

/* The list. cursor: 0 is RUN ALL, 1 .. SUITE_N the tests. */
static void draw(unsigned cursor) {
    unsigned i, row = 0;

    vid_clear(0, 0, 0);
    line(row++, PG_WHITE, "PSM TEST SUITE %s   up/down pick, A start", SUITE_TAG);
    row++;
    line(row++, cursor == 0u ? PG_YELLOW : PG_WHITE, "%c RUN ALL: the numbered tests in order, 20 s a page", cursor == 0u ? '>' : ' ');
    for (i = 0; i < SUITE_N; i++) {
        const suite_entry_t *e = &suite_list[i];
        const int on = cursor == i + 1u;
        char ord[4] = " ";

        if (e->order && e->returns) snprintf(ord, sizeof ord, "%u", e->order % 10u);
        line(row++, on ? PG_YELLOW : PG_WHITE, "%c%s %-10.10s %.49s", on ? '>' : ' ', ord, e->name, e->what);
        if (on) {
            line(row++, PG_GREY, "     %.59s", e->read);
            if (!e->returns) line(row++, PG_YELLOW, "     NO RETURN: switch the console off after this test");
        }
    }
    line(PG_ROWS - 2u, PG_GREY, "In a test: hold A+B+X+Y on its result pages for this menu.");
    line(PG_ROWS - 1u, PG_GREY, "A fault page tries the menu after 60 s; if it stays, power off.");
    line(PG_ROWS, PG_GREY, "No reset button is needed. Every test starts from a full restart.");
    {
        char rev[32];

        suite_rev_text(rev, sizeof rev);    /* two register reads: which console this is, on every photo of the menu */
        pg_text(PG_X0 + (PG_COLS - 22u) * PG_CW, ROW_Y(0u), PG_GREY, rev);
    }
    if (note[0]) line(PG_ROWS + 1u, PG_YELLOW, "%s", note);
}

static unsigned pad(void) {
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;

    return st ? st->buttons : 0u;
}

/* The disc read of a test blocks this thread for longer than the fault catcher's watchdog allows (2 s with no
   heartbeat), so a second thread gives the heartbeat while the read runs: for 30 s at most, after which a read that
   has not come back is reported as the hang it is. */
static volatile int loading;

static void *load_beat(void *arg) {
    unsigned n;

    (void)arg;
    for (n = 0; loading && n < 300u; n++) { psm_fault_beat(&crumbs); thd_sleep(100); }
    return NULL;
}

/* Load a test and start it. Comes back only when the file cannot be loaded. */
static void launch(unsigned i, unsigned chain) {
    const suite_entry_t *e = &suite_list[i];
    char path[40];
    suite_mail_t *mail;
    size_t size = 0;
    void *image;

    psm_fault_stage(&crumbs, "load");
    vid_clear(0, 0, 0);
    line(0, PG_WHITE, "PSM TEST SUITE %s", SUITE_TAG);
    line(2, PG_WHITE, "loading %s (%u KB) ...", e->name, (e->bytes + 1023u) / 1024u);
    line(3, PG_GREY, "%.63s", e->what);
    line(4, PG_GREY, "%.63s", e->read);
    snprintf(path, sizeof path, "/cd/%s", e->file);
    psm_fault_beat(&crumbs);
    loading = 1;
    thd_create(1, load_beat, NULL);
    image = suite_load(path, &size);
    loading = 0;
    psm_fault_beat(&crumbs);
    if (!image) { snprintf(note, sizeof note, "%s: could not be read from the disc", e->file); return; }
    mail = suite_mail_find(image, size);
    if (mail) { mail->chain = chain; mail->index = i; mail->status = 0u; }
    else if (e->returns) { free(image); snprintf(note, sizeof note, "%s: not the binary the build checked", e->file); return; }
    psm_fault_stage(&crumbs, "exec");
    suite_exec(image, size);
}

/* Between two tests of RUN ALL: 5 s in which B stops the run. 1 to go on. */
static int countdown(unsigned i) {
    unsigned t;

    for (t = 0; t < 250u; t++) {
        if (t % 50u == 0u) {
            vid_clear(0, 0, 0);
            line(0, PG_WHITE, "PSM TEST SUITE %s   RUN ALL", SUITE_TAG);
            line(2, PG_WHITE, "next: %u %s in %u s.   B stops the run.", suite_list[i].order, suite_list[i].name, 5u - t / 50u);
            line(3, PG_GREY, "%.63s", suite_list[i].what);
            line(4, PG_GREY, "%.63s", suite_list[i].read);
            if (note[0]) line(6, PG_YELLOW, "%s", note);
        }
        psm_fault_beat(&crumbs);
        if (pad() & CONT_B) return 0;
        thd_sleep(20);
    }
    return 1;
}

int main(void) {
    static const char *const ended[3] = { "ended", "left by A+B+X+Y", "FAULT PAGE" };
    const unsigned from = suite_mail.index, status = suite_mail.status, chain = suite_mail.chain;
    unsigned cursor = 0, held = ~0u;
    int redraw = 1;

    (void)pg_say; (void)pg_plot; (void)pg_series; (void)pg_point; (void)pg_draw; (void)suite_init; (void)suite_tick; (void)suite_keys_text; (void)suite_exit;
    vid_set_mode(DM_640x480, PM_RGB565);
    if (psm_fault_init(&crumbs) < 0) snprintf(note, sizeof note, "fault catcher: a hook was refused");
    pg_init();
    if (from < SUITE_N) {                   /* a test came back */
        cursor = from + 1u;
        snprintf(note, sizeof note, "last: %s %s", suite_list[from].name, ended[status < 3u ? status : 0u]);
        if (chain && status == SUITE_OK) {
            const int next = next_in_order(suite_list[from].order);

            if (next < 0) snprintf(note, sizeof note, "RUN ALL is done (last: %s)", suite_list[from].name);
            else if (countdown((unsigned)next)) launch((unsigned)next, 1u);
            else snprintf(note, sizeof note, "RUN ALL stopped after %s", suite_list[from].name);
        }
    }
    for (;;) {
        const unsigned now = pad(), hit = now & ~held;

        psm_fault_beat(&crumbs);
        crumbs.frame++;
        held = now;
        if (hit & CONT_DPAD_UP) { cursor = (cursor + SUITE_N) % (SUITE_N + 1u); redraw = 1; }
        else if (hit & CONT_DPAD_DOWN) { cursor = (cursor + 1u) % (SUITE_N + 1u); redraw = 1; }
        else if (hit & CONT_A) {
            if (cursor == 0u) {
                const int first = next_in_order(0u);

                if (first >= 0) launch((unsigned)first, 1u); else snprintf(note, sizeof note, "RUN ALL: no test on this disc can come back");
            } else launch(cursor - 1u, 0u);
            held = ~0u; redraw = 1;         /* only when the load failed */
        }
        if (redraw) { psm_fault_stage(&crumbs, "menu"); draw(cursor); redraw = 0; }
        thd_sleep(20);
    }
    return 0;
}
