/* SH4DER-PVR: the suite disc's menu, Dreamcast

   demo/psm/suite/suite_menu.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   One disc, every program: this is what the disc boots, and what every program comes back to
   (include/sh4der_suite.h has how, and what a start through arch_exec does and does not clear). It shows the entries
   of suite_list.h with their cards of suite_cards.h (both written by build.py from its table, with what the build
   checked of each binary), loads the one picked from the disc and starts it in its own place.

   What is on the screen, what a button does and every pixel of it is suite_view.h (pure C, host tested; its head has
   the screen), over the library's menu logic (include/sh4der_menu.h):
     the list   two tabs, BY DEMO and BY EFFECT (D-Pad left / right); up / down pick a demo; the stick scrolls the
                picked demo's card; A or START starts it; B on BY EFFECT goes back to BY DEMO; Y the credits
     CREDITS    the notices of ../psm_credits.h as pages with a CRC line      A / B pages, Y or START back
   A program goes back to this menu when A + B + X + Y are held on its result pages, or when A is held for 2 s on its
   last result page (a program built with suite_page; one built before it has the four buttons only). The menu then
   opens where it was left: the same tab, row and scroll (the mail's spare word), or on the program's row of BY DEMO
   when the program did not hand the place back. One whose binary has no mail cannot come back: the console is
   switched off after it.
   RUN ALL is not in this menu (the chain's code stays in sh4der_suite.h; a program that comes back from a chained
   run is treated as any other).

   The PVR is not started here: the SH-4 draws into a RAM copy of the frame (suite_view.h: plain rectangles and the
   page kit's glyphs, no skin and no texture) and copies the lines that changed to the frame buffer by store queue;
   the CREDITS pages are the page kit's own. Video memory: the frame buffer only. RAM: the 600 KB frame copy. Drawn
   after a key press or a stick step only: no cost a frame; each redraw's time goes to the serial line (NOT MEASURED
   on a console for this menu). Nothing is written but the frame buffer and RAM. The fault catcher
   (include/sh4der_fault.h) is in. The waits: 20 ms between two reads of the pad, and the disc read of a program
   (KallistiOS's own, a few seconds).
*/

#include <kos.h>
#include <stdarg.h>

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define PG_PREFIX "suite"
#define PG_LINES_MAX 400u                   /* the credits' lines, with the room their page breaks take */
#include "../../../include/sh4der_page.h"

#include "suite_view.h"                     /* the menu: suite_ui.h's drawing, suite_nav.h's suite_entry_t */
#include "suite_list.h"                     /* SUITE_TAG, SUITE_TITLE, suite_list[], SUITE_N: written by build.py */
#include "suite_cards.h"                    /* suite_card[], suite_fx[], the groups, NOT IN THE MENU: written by build.py */
#include "../../../include/sh4der_suite.h"
#include "../psm_credits.h"

#define ROW_Y(r) (PG_Y0 + PG_CH * (r))

typedef char suite_ui_grid_[PG_CW == UI_CW && PG_CH == UI_CH && PG_X0 == UI_X0 && PG_Y0 == UI_Y0 ? 1 : -1];   /* the page kit's grid is suite_ui.h's */
static const ui_font_t font = { &pg_glyph[0][0][0], PG_CW, PG_CH };
static uint16_t frame[640u * 480u] __attribute__((aligned(32)));   /* the screen in RAM: drawn here, copied by store queue */
static int frame_ok;                        /* 0: the frame buffer is not this copy (start, credits, a load): copy all */

static const view_data_t data = { SUITE_TITLE, SUITE_TAG, suite_list, suite_card, SUITE_N, suite_group_name, SUITE_GROUPS_N, suite_fx, SUITE_FX_N,
                                  suite_nim_subtitle, suite_nim_intro, suite_nim_item, SUITE_NIM_N };
static view_t view;
static sh4der_fault_crumbs_t crumbs;

/* Lines y0 .. y1 of the RAM copy to the frame buffer: 1280 bytes a line, a multiple of 32, by store queue. */
static void show(int y0, int y1) {
    sq_cpy(vram_s + y0 * 640, frame + y0 * 640, (size_t)(y1 - y0 + 1) * 1280u);
}

static void line(unsigned row, uint16_t col, const char *fmt, ...) {
    char s[PG_COLS + 1u];
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(s, sizeof s, fmt, ap);
    va_end(ap);
    ui_text(frame, &font, 1u, PG_X0, ROW_Y(row), col, s, PG_COLS);
}

/* The copyright line of the foot: ../psm_credits.h's first, without the library's name the title already shows. */
static const char *foot_text(void) {
    static const char lib[] = "SH4DER-PVR ";
    const char *s = psm_credits_foot[0];

    return strncmp(s, lib, sizeof lib - 1u) ? s : s + sizeof lib - 1u;
}

/* The menu's screen. text_only: only the card's text moved (a stick step), so only its lines are drawn and copied;
   else all of it. The time it took goes to the serial line. */
static void draw(int text_only) {
    const uint64_t t0 = timer_us_gettime64();
    char rev[32];
    int y0 = 0, y1 = 479;

    if (text_only && frame_ok) view_text_lines(&y0, &y1);
    suite_rev_text(rev, sizeof rev);        /* two register reads: which console this is, on every photo of the menu */
    ui_clip(y0, y1);
    view_draw(frame, &font, &view, rev, foot_text());
    ui_clip(0, 479);
    show(y0, y1);
    frame_ok = 1;
    printf("suite: redraw lines %d .. %d, %u us\n", y0, y1, (unsigned)(timer_us_gettime64() - t0));
}

/* A screen of lines (a load): look B's bands, the lines, then all of it to the screen. */
static void plain_begin(void) {
    ui_clip(0, 479);
    ui_rect_(frame, 0, 0, 639, 479, VIEW_C_BG);
    ui_rect_(frame, 0, 0, 639, VIEW_HEAD_Y1, VIEW_C_BAND);
    ui_rect_(frame, 0, VIEW_HEAD_Y1 + 1, 639, VIEW_HEAD_Y1 + 2, VIEW_C_RULE);
    frame_ok = 0;
}
static void plain_show(void) { show(0, 479); }

/* The credits into the page kit's lines, once: its pages and its CRC line then come with pg_draw. */
static void credits_fill(void) {
    unsigned i;

    for (i = 0; i < PSM_CREDITS_N && pg_nlines < PG_LINES_MAX; i++) {
        if (psm_credits_text[i][0] == '\f') { pg_break(); continue; }
        snprintf(pg_lines[pg_nlines], sizeof pg_lines[0], "%s", psm_credits_text[i]);
        pg_nlines++;
    }
}

/* The pad: its buttons and the stick's up / down axis (down is positive). */
static unsigned pad(int *joyy) {
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;

    *joyy = st ? st->joyy : 0;
    return st ? st->buttons : 0u;
}

/* The disc read of a program blocks this thread for longer than the fault catcher's watchdog allows (2 s with no
   heartbeat), so a second thread gives the heartbeat while the read runs: for 30 s at most, after which a read that
   has not come back is reported as the hang it is. */
static volatile int loading;

static void *load_beat(void *arg) {
    unsigned n;

    (void)arg;
    for (n = 0; loading && n < 300u; n++) { sh4der_fault_beat(&crumbs); thd_sleep(100); }
    return NULL;
}

/* Load a program and start it, the menu's place in its mail. Comes back only when the file cannot be loaded. */
static void launch(unsigned i) {
    const suite_entry_t *e = &suite_list[i];
    char path[40];
    suite_mail_t *mail;
    size_t size = 0;
    void *image;

    sh4der_fault_stage(&crumbs, "load");
    plain_begin();
    ui_text(frame, &font, 2u, VIEW_TITLE_X, VIEW_TITLE_Y, VIEW_C_TEXT, SUITE_TITLE, 20u);
    line(2, VIEW_C_TEXT, "loading %s (%u KB) ...", e->title ? e->title : e->name, (e->bytes + 1023u) / 1024u);
    line(4, VIEW_C_GREY, "To come back: hold A + B + X + Y on its result pages,");
    line(5, VIEW_C_GREY, "or hold A for 2 s on its last result page.");
    if (!e->returns) line(7, VIEW_C_YELLOW, "NO RETURN: switch the console off after this program.");
    plain_show();
    snprintf(path, sizeof path, "/cd/%s", e->file);
    sh4der_fault_beat(&crumbs);
    loading = 1;
    thd_create(1, load_beat, NULL);
    image = suite_load(path, &size);
    loading = 0;
    sh4der_fault_beat(&crumbs);
    if (!image) { snprintf(view.note, sizeof view.note, "%s: could not be read from the disc", e->file); return; }
    mail = suite_mail_find(image, size);
    if (mail) { mail->chain = 0u; mail->index = i; mail->status = 0u; mail->spare = view_place(&view); }
    else if (e->returns) { free(image); snprintf(view.note, sizeof view.note, "%s: not the binary the build checked", e->file); return; }
    sh4der_fault_stage(&crumbs, "exec");
    suite_exec(image, size);
}

static void cont_reset_btn_callback_(uint8_t addr, uint32_t btns) {
    (void)addr;
    (void)btns;
    arch_exit();
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    cont_btn_callback(0, CONT_RESET_BUTTONS, cont_reset_btn_callback_);
    static const char *const ended[3] = { "ended", "left by A+B+X+Y", "FAULT PAGE" };
    const unsigned from = suite_mail.index, status = suite_mail.status;
    const uint32_t place = suite_mail.spare;
    unsigned held = ~0u;
    int redraw = 1, text_only = 0, hook_refused;

    (void)pg_say; (void)pg_text; (void)pg_plot; (void)pg_series; (void)pg_point; (void)suite_init; (void)suite_tick; (void)suite_keys_text; (void)suite_exit;
    (void)suite_page;
    vid_set_mode(DM_640x480, PM_RGB565);
    hook_refused = sh4der_fault_init(&crumbs) < 0;
    pg_init();
    credits_fill();
    view_init(&view, &data, from, place);   /* back where it was left, or on the row of the program that came back */
    if (hook_refused) snprintf(view.note, sizeof view.note, "fault catcher: a hook was refused");
    else if (from < SUITE_N && status != SUITE_OK) snprintf(view.note, sizeof view.note, "last: %.24s %s", suite_list[from].name, ended[status < 3u ? status : 0u]);
    for (;;) {
        int joyy, index, act;
        const unsigned now = pad(&joyy), down = now & ~held;
        const unsigned hit = (down & CONT_A ? NAV_A : 0u) | (down & CONT_B ? NAV_B : 0u) | (down & CONT_Y ? NAV_Y : 0u) | (down & CONT_START ? NAV_START : 0u) |
                             (down & CONT_DPAD_LEFT ? NAV_LEFT : 0u) | (down & CONT_DPAD_RIGHT ? NAV_RIGHT : 0u);
        const int dpad = now & CONT_DPAD_UP ? -1 : now & CONT_DPAD_DOWN ? 1 : 0;

        sh4der_fault_beat(&crumbs);
        crumbs.frame++;
        held = now;
        act = view_key(&view, hit, dpad, joyy, (uint32_t)timer_ms_gettime64(), pg_pages(), &index);
        if (act == VIEW_LAUNCH) {           /* comes back only when the load failed */
            launch((unsigned)index);
            held = ~0u;
            redraw = 1;
        } else if (act == VIEW_REDRAW) { redraw = 1; view.note[0] = 0; }
        else if (act == VIEW_REDRAW_TEXT && !redraw) { redraw = 1; text_only = 1; }
        if (redraw) {
            sh4der_fault_stage(&crumbs, "menu");
            if (view.screen == VIEW_CREDITS) {
                pg_draw(view.page % pg_pages(), "CREDITS", "A next  B last  Y/START back");
                frame_ok = 0;               /* the page kit drew on the frame buffer itself */
            } else draw(text_only);
            redraw = 0;
            text_only = 0;
        }
        thd_sleep(20);
    }
    return 0;
}
