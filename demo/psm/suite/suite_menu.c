/* SH4DER-PVR: the suite disc's menu, Dreamcast

   demo/psm/suite/suite_menu.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   One disc, every program: this is what the disc boots, and what every program comes back to (../psm_suite.h has
   how, and what a start through arch_exec does and does not clear). It shows the entries of suite_list.h (written by
   build.py from its table, with what the build checked of each binary), loads the one picked from the disc and
   starts it in its own place.

   Which screen, which row and what a button does is suite_nav.h (pure C, host tested; its head has the screens):
     TOP       TECH DEMOS / DEVELOPER TOOLS in large letters      up / down pick, A open, Y credits
     LIST      a group's entries under its own RUN ALL            up / down pick, A start, B or START back, Y credits
     CREDITS   the notices of ../psm_credits.h as pages with a CRC line   A / B pages, Y or START back
   RUN ALL: a group's numbered entries in order, each turning its own result pages (20 s a page) and coming back
   here; B during the 5 s between two stops the run. DEVELOPER TOOLS run once through; TECH DEMOS go round.
   A program goes back to this menu when A + B + X + Y are held on its result pages. One marked NO RETURN was built
   without ../psm_suite.h: the console is switched off after it.
   Every menu screen ends with the copyright foot (../psm_credits.h).

   The PVR is not started here: the SH-4 draws into the frame buffer with ../psm_page.h's text, inside the title-safe
   area. Nothing is written but the frame buffer and RAM. The fault catcher (../psm_fault.h) is in. The waits: 20 ms
   between two reads of the pad, and the disc read of a program (KallistiOS's own, a few seconds).
*/

#include <kos.h>
#include <stdarg.h>

#define PSM_FAULT_IMPL
#include "../psm_fault.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define PG_PREFIX "suite"
#define PG_LINES_MAX 400u                   /* the credits' lines, with the room their page breaks take */
#include "../psm_page.h"

#include "suite_nav.h"                      /* suite_entry_t, the screens */
#include "suite_list.h"                     /* SUITE_TAG, SUITE_TITLE, suite_list[], SUITE_N: written by build.py */
#include "../psm_suite.h"
#include "../psm_credits.h"

#define PG_YELLOW PG_RGB(255, 220, 0)
#define ROW_Y(r) (PG_Y0 + PG_CH * (r))

static psm_fault_crumbs_t crumbs;
static char note[PG_COLS + 1u];
static nav_t nav;

static void line(unsigned row, uint16_t col, const char *fmt, ...) {
    char s[PG_COLS + 1u];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s, sizeof s, fmt, ap);
    va_end(ap);
    pg_text(PG_X0, ROW_Y(row), col, s);
}

/* A string twice the size (16 x 32 a character, 32 to a line): each pixel of the small glyph as four. */
static void big_text(unsigned x, unsigned y, uint16_t col, const char *s) {
    const unsigned r = col >> 11, g = (col >> 5) & 63u, b = col & 31u;
    unsigned i, j;

    for (; *s && x + 2u * PG_CW <= 640u - PG_X0; s++, x += 2u * PG_CW) {
        const unsigned c = (unsigned char)*s;
        const uint8_t (*gl)[PG_CW] = pg_glyph[c >= 32u && c < 127u ? c - 32u : 0u];

        if (y + 2u * PG_CH > 480u) return;
        for (j = 0; j < 2u * PG_CH; j++)
            for (i = 0; i < 2u * PG_CW; i++) {
                const unsigned a = gl[j / 2u][i / 2u];

                if (a) vram_s[(y + j) * 640u + x + i] = (uint16_t)(((r * a / 255u) << 11) | ((g * a / 255u) << 5) | (b * a / 255u));
            }
    }
}

/* The TOP or the LIST screen: suite_nav.h says what is on it. */
static void draw(void) {
    static const uint16_t colour[3] = { PG_WHITE, PG_GREY, PG_YELLOW };
    static nav_grid_t g;
    char rev[32];
    unsigned row;

    nav_grid(&g, &nav, suite_list, SUITE_N, SUITE_TITLE " " SUITE_TAG, note, psm_credits_foot);
    vid_clear(0, 0, 0);
    for (row = 0; row < NAV_ROWS; row++) {
        if (!g.text[row][0]) continue;
        if (g.big[row]) big_text(PG_X0, ROW_Y(row), colour[g.colour[row]], g.text[row]);
        else pg_text(PG_X0, ROW_Y(row), colour[g.colour[row]], g.text[row]);
    }
    suite_rev_text(rev, sizeof rev);        /* two register reads: which console this is, on every photo of the menu */
    pg_text(PG_X0 + (PG_COLS - 22u) * PG_CW, ROW_Y(0u), PG_GREY, rev);
}

/* The credits into the page kit's lines, once: its pages and its CRC line then come with pg_draw. */
static void credits_fill(void) {
    unsigned i;

    for (i = 0; i < PSM_CREDITS_N && pg_nlines < PG_LINES_MAX; i++) {
        if (psm_credits_text[i][0] == '\f') { pg_break(); continue; }
        snprintf(pg_lines[pg_nlines], sizeof pg_lines[0], "%s", psm_credits_text[i]);
        pg_nlines++;
    }
}

static unsigned pad(void) {
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;

    return st ? st->buttons : 0u;
}

/* The disc read of a program blocks this thread for longer than the fault catcher's watchdog allows (2 s with no
   heartbeat), so a second thread gives the heartbeat while the read runs: for 30 s at most, after which a read that
   has not come back is reported as the hang it is. */
static volatile int loading;

static void *load_beat(void *arg) {
    unsigned n;

    (void)arg;
    for (n = 0; loading && n < 300u; n++) { psm_fault_beat(&crumbs); thd_sleep(100); }
    return NULL;
}

/* Load a program and start it. Comes back only when the file cannot be loaded. */
static void launch(unsigned i, unsigned chain) {
    const suite_entry_t *e = &suite_list[i];
    char path[40];
    suite_mail_t *mail;
    size_t size = 0;
    void *image;

    psm_fault_stage(&crumbs, "load");
    vid_clear(0, 0, 0);
    line(0, PG_WHITE, "%s %s", SUITE_TITLE, SUITE_TAG);
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

/* Between two programs of RUN ALL: 5 s in which B stops the run. 1 to go on. */
static int countdown(unsigned i) {
    unsigned t;

    for (t = 0; t < 250u; t++) {
        if (t % 50u == 0u) {
            vid_clear(0, 0, 0);
            line(0, PG_WHITE, "%s %s   RUN ALL, %s", SUITE_TITLE, SUITE_TAG, nav_group_name[suite_list[i].group]);
            line(2, PG_WHITE, "next: %u %s in %u s.   B stops the run.", suite_list[i].order, suite_list[i].name, 5u - t / 50u);
            line(3, PG_GREY, "%.63s", suite_list[i].what);
            line(4, PG_GREY, "%.63s", suite_list[i].read);
            if (note[0]) line(6, PG_YELLOW, "%s", note);
            line(NAV_FOOT_ROW, PG_GREY, "%s", psm_credits_foot[0]);
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
    unsigned held = ~0u;
    int redraw = 1;

    (void)pg_say; (void)pg_plot; (void)pg_series; (void)pg_point; (void)suite_init; (void)suite_tick; (void)suite_keys_text; (void)suite_exit;
    vid_set_mode(DM_640x480, PM_RGB565);
    if (psm_fault_init(&crumbs) < 0) snprintf(note, sizeof note, "fault catcher: a hook was refused");
    pg_init();
    credits_fill();
    nav_init(&nav, suite_list, SUITE_N, from);
    if (from < SUITE_N) {                   /* a program came back */
        snprintf(note, sizeof note, "last: %s %s", suite_list[from].name, ended[status < 3u ? status : 0u]);
        if (chain && status == SUITE_OK) {
            const int next = nav_next_chain(suite_list, SUITE_N, from);

            if (next < 0) snprintf(note, sizeof note, "RUN ALL is done (last: %s)", suite_list[from].name);
            else if (countdown((unsigned)next)) launch((unsigned)next, 1u);
            else snprintf(note, sizeof note, "RUN ALL stopped after %s", suite_list[from].name);
        }
    }
    for (;;) {
        const unsigned now = pad(), down = now & ~held;
        const unsigned hit = (down & CONT_DPAD_UP ? NAV_UP : 0u) | (down & CONT_DPAD_DOWN ? NAV_DOWN : 0u) | (down & CONT_A ? NAV_A : 0u) |
                             (down & CONT_B ? NAV_B : 0u) | (down & CONT_Y ? NAV_Y : 0u) | (down & CONT_START ? NAV_START : 0u);
        int index, act;

        psm_fault_beat(&crumbs);
        crumbs.frame++;
        held = now;
        act = hit ? nav_key(&nav, suite_list, SUITE_N, hit, pg_pages(), &index) : NAV_NONE;
        if (act == NAV_LAUNCH) { launch((unsigned)index, 0u); held = ~0u; }            /* comes back only when the load failed */
        else if (act == NAV_RUN_ALL) {
            if (index >= 0) launch((unsigned)index, 1u); else snprintf(note, sizeof note, "RUN ALL: nothing here can come back to the menu");
            held = ~0u;
        }
        if (act != NAV_NONE) redraw = 1;
        if (redraw) {
            psm_fault_stage(&crumbs, "menu");
            if (nav.screen == NAV_CREDITS) {
                char keys[64];

                nav_legend(keys, sizeof keys, NAV_CREDITS, nav.has_top);
                pg_draw(nav.page % pg_pages(), "CREDITS", keys);
            } else draw();
            redraw = 0;
        }
        thd_sleep(20);
    }
    return 0;
}
