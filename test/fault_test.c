/* SH4DER-PVR: the demos' fault catcher, host test

   test/fault_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of the plain C part of include/sh4der_fault.h (the KallistiOS part compiles only for the Dreamcast and is
   tested there with the psm-fault image):
     - the breadcrumb setters: what each stores, the stage's length limit, a menu change is noticed once
     - the watchdog with a fake clock: nothing before the first heartbeat, nothing while it moves, a report after
       exactly the limit, and only one
     - the report's text for an exception, a hang, an assert and an exit: every line within 64 columns and within
       the screen's rows, the values where they should be, worst-case values included
     - the formatter's numbers: hex, signed, hundredths, and that a line never runs past its end
     - the event names, which events carry an address, which instructions are calls
     - the stack scan over a fake memory: return addresses kept, look-alikes dropped, a bad stack ends it
*/

#include <stdio.h>
#include <string.h>
#include "sh4der_fault.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

static char ln[SH4DER_FAULT_ROWS][SH4DER_FAULT_COLS + 1u];

static int has(unsigned n, const char *what) {
    unsigned i;

    for (i = 0; i < n; i++) if (strstr(ln[i], what)) return 1;
    return 0;
}

/* every line inside the columns and terminated, the count inside the rows, no line past the count used */
static void shape(unsigned n) {
    unsigned i;

    CHECK(n >= 3u && n <= SH4DER_FAULT_ROWS);
    for (i = 0; i < SH4DER_FAULT_ROWS; i++) {
        CHECK(strlen(ln[i]) <= SH4DER_FAULT_COLS);
        if (i >= n) CHECK(ln[i][0] == 0);
    }
}

static void crumbs(void) {
    sh4der_fault_crumbs_t c;

    memset(&c, 0, sizeof c);
    CHECK(SH4DER_FAULT_CRUMBS_BYTES <= 96u);                         /* small, and no pointer in it */
    sh4der_fault_beat(&c); sh4der_fault_beat(&c);
    CHECK(c.beat == 2u);
    sh4der_fault_stage(&c, "world");
    CHECK(!strcmp(c.stage, "world"));
    sh4der_fault_stage(&c, "a-very-long-stage-name");
    CHECK(strlen(c.stage) == SH4DER_FAULT_STAGE_LEN - 1u && !strncmp(c.stage, "a-very-long", 11));
    sh4der_fault_stage(&c, "");
    CHECK(c.stage[0] == 0);
    sh4der_fault_frame(&c, 1234u, 0x0408u, 2u, 1u);
    CHECK(c.frame == 1234u && c.pad == 0x0408u && c.scene == 2u && c.route == 1u);
    sh4der_fault_camera(&c, 1.5f, -0.25f, 100.0f, 359.5f, -12.25f);
    CHECK(c.cam_cm[0] == 150 && c.cam_cm[1] == -25 && c.cam_cm[2] == 10000 && c.yaw_cdeg == 35950 && c.pitch_cdeg == -1225);
    sh4der_fault_counts(&c, 1000u, 2760u, 40u, 1500u);
    CHECK(c.verts[0] == 1000u && c.verts[1] == 2760u && c.verts[2] == 40u && c.verts[3] == 3800u && c.tris == 1500u);

    /* the menu: opening it and moving the cursor is not a change; a new value on the same row is */
    sh4der_fault_menu(&c, 1, 3u, 10);
    CHECK(c.menu_open == 1u && c.menu_row == 3u && c.menu_value == 10 && c.change_frame == 0u);
    sh4der_fault_menu(&c, 1, 4u, 7);
    CHECK(c.change_frame == 0u);
    sh4der_fault_frame(&c, 1300u, 0u, 2u, 1u);
    sh4der_fault_menu(&c, 1, 4u, 8);
    CHECK(c.change_row == 4u && c.change_value == 8 && c.change_frame == 1300u);
    sh4der_fault_frame(&c, 1400u, 0u, 2u, 1u);
    sh4der_fault_menu(&c, 1, 4u, 8);                                 /* the same value again: the change stays where it was */
    CHECK(c.change_frame == 1300u);
    sh4der_fault_menu(&c, 0, 4u, 9);                                 /* the menu shut: not a change */
    CHECK(c.change_value == 8 && c.menu_open == 0u);
}

static void watchdog(void) {
    sh4der_fault_watch_t w;
    uint32_t beat = 0;
    unsigned t, fired = 0, when = 0;

    memset(&w, 0, sizeof w);
    for (t = 0; t < 1000u; t++) CHECK(!sh4der_fault_watch_step(&w, beat, SH4DER_FAULT_HANG_VBLANKS));   /* loading: no heartbeat yet, no hang */
    for (t = 0; t < 1000u; t++) {                                 /* running: a frame every other vblank */
        if (!(t & 1u)) beat++;
        CHECK(!sh4der_fault_watch_step(&w, beat, SH4DER_FAULT_HANG_VBLANKS));
    }
    beat++;
    CHECK(!sh4der_fault_watch_step(&w, beat, SH4DER_FAULT_HANG_VBLANKS));
    for (t = 0; t < SH4DER_FAULT_HANG_VBLANKS - 1u; t++) CHECK(!sh4der_fault_watch_step(&w, beat, SH4DER_FAULT_HANG_VBLANKS));   /* a long frame, one look short of the limit */
    beat++;
    CHECK(!sh4der_fault_watch_step(&w, beat, SH4DER_FAULT_HANG_VBLANKS) && w.stale == 0u);
    for (t = 1; t <= 500u; t++) {                                 /* then it stops for good */
        if (sh4der_fault_watch_step(&w, beat, SH4DER_FAULT_HANG_VBLANKS)) { fired++; when = t; }
    }
    CHECK(fired == 1u && when == SH4DER_FAULT_HANG_VBLANKS && w.stale == SH4DER_FAULT_HANG_VBLANKS);
    beat++;                                                       /* a heartbeat after the report does not report again */
    for (t = 0; t < 500u; t++) CHECK(!sh4der_fault_watch_step(&w, beat, SH4DER_FAULT_HANG_VBLANKS) || !"second report");
    printf("  watchdog: reports once, %u vblanks after the last frame\n", when);
}

static void fill_regs(sh4der_fault_report_t *r) {
    unsigned i;

    r->pc = 0x8c012344u; r->pr = 0x8c0100f8u; r->sr = 0x400000f0u; r->gbr = 0x8c3f0000u; r->vbr = 0x8c00f400u;
    r->mach = 0x11111111u; r->macl = 0x22222222u; r->fpscr = 0x00140001u; r->fpul = 0x3f800000u;
    for (i = 0; i < 16u; i++) r->r[i] = 0xa0000000u + i;
    r->r[15] = 0x8cfffe00u;
}

static void reports(void) {
    sh4der_fault_report_t r;
    sh4der_fault_crumbs_t c;
    unsigned n, i;

    memset(&c, 0, sizeof c);
    sh4der_fault_frame(&c, 4242u, 0x00000408u, 1u, 2u);
    sh4der_fault_stage(&c, "world");
    c.beat = 4242u;
    sh4der_fault_menu(&c, 1, 5u, 3); sh4der_fault_menu(&c, 1, 5u, 4);
    sh4der_fault_camera(&c, 12.5f, 1.7f, -30.25f, 271.5f, -5.0f);
    sh4der_fault_counts(&c, 5000u, 2760u, 640u, 9000u);

    /* an exception with an address and a trace */
    memset(&r, 0, sizeof r);
    r.kind = SH4DER_FAULT_EXCEPTION; r.code = 0x100u; r.tea = 0x8c000001u;
    fill_regs(&r);
    r.ntrace = 8u;
    for (i = 0; i < 8u; i++) r.trace[i] = 0x8c010000u + 0x100u * i;
    n = sh4der_fault_format(&r, &c, ln);
    shape(n);
    for (i = 0; i < n; i++) printf("  |%s\n", ln[i]);
    CHECK(!strncmp(ln[0], "*** EXCEPTION ***", 17));
    CHECK(!strcmp(ln[1], "ADDRESS ERROR (WRITE)  code 100  at address 8c000001"));
    CHECK(!strncmp(ln[2], "PC 8c012344  PR 8c0100f8  SR 400000f0  SP 8cfffe00", 50));
    CHECK(!strncmp(ln[3], "R0  a0000000  R1  a0000001  R2  a0000002  R3  a0000003", 54));
    CHECK(!strncmp(ln[6], "R12 a000000c  R13 a000000d  R14 a000000e  R15 8cfffe00", 54));
    CHECK(has(n, "GBR 8c3f0000  VBR 8c00f400  FPSCR 00140001  FPUL 3f800000"));
    CHECK(has(n, "MACH 11111111  MACL 22222222"));
    CHECK(has(n, "CALLS 8c010000 8c010100 8c010200 8c010300 8c010400 8c010500") && has(n, "      8c010600 8c010700"));
    CHECK(has(n, "FRAME 4242  BEAT 4242  STAGE world"));
    CHECK(has(n, "SCENE 1  ROUTE 2  PAD 00000408  MENU open row 5 = 4"));
    CHECK(has(n, "LAST CHANGE row 5 to 4 on frame 4242"));
    CHECK(has(n, "CAM 12.50 1.70 -30.25  YAW 271.50  PITCH -5.00"));
    CHECK(has(n, "VERTS ground 5000 models 2760 other 640 all 8400 TRIS 9000"));
    CHECK(!strcmp(ln[n - 1u], "Photograph this screen. Power off to restart."));
    CHECK(!has(n, "*** HANG") && !has(n, "MAIN "));                  /* an exception's screen does not look like a hang's */

    /* an illegal instruction has no address; no trace says so; no crumbs says so */
    r.code = 0x180u; r.ntrace = 0u;
    n = sh4der_fault_format(&r, NULL, ln);
    shape(n);
    CHECK(!strcmp(ln[1], "ILLEGAL INSTRUCTION  code 180") && has(n, "CALLS none found on the stack") && has(n, "no breadcrumbs were given"));
    r.kind = SH4DER_FAULT_DOUBLE;
    n = sh4der_fault_format(&r, &c, ln);
    shape(n);
    CHECK(!strncmp(ln[0], "*** EXCEPTION in an interrupt handler ***", 41));

    /* a hang: the first line says HANG, and it carries the PVR's and the main thread's lines */
    memset(&r, 0, sizeof r);
    r.kind = SH4DER_FAULT_HANG; r.stale = SH4DER_FAULT_HANG_VBLANKS;
    fill_regs(&r);
    r.ntrace = SH4DER_FAULT_TRACE_MAX;
    for (i = 0; i < SH4DER_FAULT_TRACE_MAX; i++) r.trace[i] = 0x8c020000u + 4u * i;
    r.main_pc = 0x8c0456a2u; r.main_pr = 0x8c045600u; r.main_state = 3;
    strcpy(r.main_wait, "PVR wait ready");
    r.ta_busy = 1u; r.istnrm = 0x00000048u; r.istext = 0u; r.isterr = 0x00000004u;
    r.pvr_opb_pos = 0x00012345u; r.pvr_vtx_pos = 0x00067890u; r.pvr_render_addr = 0x00600000u; r.pvr_sync = 0x000021f0u; r.pvr_isp_feed = 0u;
    r.have_stats = 1; r.vtx_used = 123456u; r.vtx_used_max = 234567u; r.pvr_frames = 4242u; r.pvr_vbl = 9000u;
    n = sh4der_fault_format(&r, &c, ln);
    shape(n);
    for (i = 0; i < n; i++) printf("  |%s\n", ln[i]);
    CHECK(!strncmp(ln[0], "*** HANG ***", 12) && !has(n, "*** EXCEPTION"));
    CHECK(!strcmp(ln[1], "no frame for 120 vblanks. PC below = the code that was running"));
    CHECK(has(n, "MAIN PC 8c0456a2  PR 8c045600  state 3 (1 run 2 ready 3 wait)"));
    CHECK(has(n, "MAIN waits for: PVR wait ready"));
    CHECK(has(n, "ASIC NRM 00000048  EXT 00000000  ERR 00000004"));
    CHECK(has(n, "PVR TA busy 1  OPB 00012345  VTX 00067890"));
    CHECK(has(n, "PVR RADDR 00600000  SYNC 000021f0  ISP 00000000"));
    CHECK(has(n, "PVR vtx buf 123456 max 234567  frames 4242 vbl 9000"));
    CHECK(has(n, "STAGE world"));
    printf("  hang report: %u lines of %u\n", n, SH4DER_FAULT_ROWS);

    /* the worst case still fits the rows and the columns: every number at its widest */
    memset(&r, 0xff, sizeof r);
    r.kind = SH4DER_FAULT_HANG; r.ntrace = SH4DER_FAULT_TRACE_MAX; r.have_stats = 1; r.main_state = -2147483647 - 1;
    memset(r.main_wait, 'W', sizeof r.main_wait); r.main_wait[sizeof r.main_wait - 1u] = 0;
    memset(&c, 0xff, sizeof c);
    c.cam_cm[0] = c.cam_cm[1] = c.cam_cm[2] = c.yaw_cdeg = c.pitch_cdeg = c.menu_value = c.change_value = -2147483647 - 1;
    memset(c.stage, 'S', sizeof c.stage);                         /* not terminated: the report must not run on */
    n = sh4der_fault_format(&r, &c, ln);
    shape(n);
    CHECK(n <= SH4DER_FAULT_ROWS && has(n, "STAGE SSSSSSSSSSS") && !has(n, "SSSSSSSSSSSS"));
    CHECK(has(n, "CAM -21474836.48"));

    /* an assert and an exit: two lines of text, no registers */
    memset(&r, 0, sizeof r);
    r.kind = SH4DER_FAULT_ASSERT;
    strcpy(r.text[0], "tris <= PSM_TRIS_MAX"); strcpy(r.text[1], "psm_dc.c line 321 in draw_world");
    n = sh4der_fault_format(&r, &c, ln);
    shape(n);
    CHECK(!strncmp(ln[0], "*** ASSERT ***", 14) && !strcmp(ln[1], "tris <= PSM_TRIS_MAX") && !strcmp(ln[2], "psm_dc.c line 321 in draw_world") && !has(n, "PC "));
    r.kind = SH4DER_FAULT_EXIT;
    n = sh4der_fault_format(&r, &c, ln);
    shape(n);
    CHECK(!strncmp(ln[0], "*** EXIT ***", 12));
    r.kind = 99;                                                  /* a kind out of range is shown as an exception, not read out of the table */
    n = sh4der_fault_format(&r, &c, ln);
    shape(n);
}

static void formatter(void) {
    char buf[SH4DER_FAULT_COLS + 1u + 8u];
    sh4der_fault_line_t l;
    unsigned i;

    memset(buf, '#', sizeof buf);
    sh4der_fault_line_begin(&l, buf);
    sh4der_fault_put_hex(&l, 0x00abc123u, 8u); sh4der_fault_put_c(&l, ' ');
    sh4der_fault_put_hex(&l, 0x1e0u, 3u); sh4der_fault_put_c(&l, ' ');
    sh4der_fault_put_u(&l, 0u); sh4der_fault_put_c(&l, ' ');
    sh4der_fault_put_u(&l, 4294967295u); sh4der_fault_put_c(&l, ' ');
    sh4der_fault_put_i(&l, -2147483647 - 1); sh4der_fault_put_c(&l, ' ');
    sh4der_fault_put_centi(&l, 5); sh4der_fault_put_c(&l, ' ');
    sh4der_fault_put_centi(&l, -5); sh4der_fault_put_c(&l, ' ');
    sh4der_fault_put_centi(&l, 12345);
    CHECK(!strcmp(buf, "00abc123 1e0 0 4294967295 -2147483648 0.05 -0.05 123.45"));
    for (i = 0; i < 200u; i++) sh4der_fault_put_s(&l, "xyz");        /* far too much: it stops at the last column */
    CHECK(strlen(buf) == SH4DER_FAULT_COLS && l.n == SH4DER_FAULT_COLS && buf[SH4DER_FAULT_COLS] == 0 && buf[SH4DER_FAULT_COLS + 1u] == '#');
}

static void names(void) {
    CHECK(!strcmp(sh4der_fault_code_name(0x0e0u), "ADDRESS ERROR (READ OR FETCH)") && !strcmp(sh4der_fault_code_name(0x100u), "ADDRESS ERROR (WRITE)"));
    CHECK(!strcmp(sh4der_fault_code_name(0x180u), "ILLEGAL INSTRUCTION") && !strcmp(sh4der_fault_code_name(0x1a0u), "ILLEGAL INSTRUCTION IN A DELAY SLOT"));
    CHECK(!strcmp(sh4der_fault_code_name(0x800u), "FPU DISABLED") && !strcmp(sh4der_fault_code_name(0x120u), "FPU EXCEPTION"));
    CHECK(!strcmp(sh4der_fault_code_name(0x420u), "INTERRUPT NOBODY HANDLES") && !strcmp(sh4der_fault_code_name(0x020u), "UNKNOWN EVENT"));
    CHECK(sh4der_fault_code_has_tea(0x0e0u) && sh4der_fault_code_has_tea(0x100u) && sh4der_fault_code_has_tea(0x040u));
    CHECK(!sh4der_fault_code_has_tea(0x180u) && !sh4der_fault_code_has_tea(0x120u) && !sh4der_fault_code_has_tea(0x400u));
    CHECK(sh4der_fault_is_call(0x410bu) && sh4der_fault_is_call(0x4f0bu));         /* jsr @r1, jsr @r15 */
    CHECK(sh4der_fault_is_call(0xb123u) && sh4der_fault_is_call(0x0303u));         /* bsr, bsrf r3 */
    CHECK(!sh4der_fault_is_call(0x000bu) && !sh4der_fault_is_call(0x402bu) && !sh4der_fault_is_call(0xa123u) && !sh4der_fault_is_call(0x0009u));   /* rts, jmp, bra, nop */
}

/* a fake machine: code at 0x8c010000 .. 0x8c011000 (16-bit words), a stack of 64 words at 0x8c0ff000 */
#define TEXT_LO 0x8c010000u
#define TEXT_HI 0x8c011000u
#define STACK_LO 0x8c0ff000u
static uint16_t text[(TEXT_HI - TEXT_LO) / 2u];
static uint32_t stack[64];

static int fake32(uint32_t a, uint32_t *out, void *user) {
    (void)user;
    if ((a & 3u) || a < STACK_LO || a >= STACK_LO + sizeof stack) return 0;
    *out = stack[(a - STACK_LO) / 4u];
    return 1;
}
static int fake16(uint32_t a, unsigned *out, void *user) {
    (void)user;
    if ((a & 1u) || a < TEXT_LO || a >= TEXT_HI) return 0;
    *out = text[(a - TEXT_LO) / 2u];
    return 1;
}
static int fake_text(uint32_t a, void *user) { (void)user; return a >= TEXT_LO && a < TEXT_HI; }

static void scan(void) {
    uint32_t out[SH4DER_FAULT_TRACE_MAX];
    unsigned n, i;

    memset(text, 0x09, sizeof text);                              /* nops (0x0909 is not a call) */
    memset(stack, 0, sizeof stack);
    text[(0x100u) / 2u] = 0x410bu;                                /* jsr @r1 at +0x100: returns to +0x104 */
    text[(0x200u) / 2u] = 0xb055u;                                /* bsr at +0x200: returns to +0x204 */
    text[(0x300u) / 2u] = 0x0303u;                                /* bsrf at +0x300: returns to +0x304 */
    stack[2] = TEXT_LO + 0x104u;                                  /* a real return address */
    stack[3] = 0x12345678u;                                       /* data */
    stack[5] = TEXT_LO + 0x400u;                                  /* points into code, but no call before it */
    stack[6] = TEXT_LO + 0x205u;                                  /* odd */
    stack[7] = TEXT_LO + 0x204u;
    stack[9] = STACK_LO + 16u;                                    /* a pointer into the stack */
    stack[10] = TEXT_LO + 0x304u;
    stack[11] = TEXT_LO;                                          /* the first word of code: nothing before it */
    n = sh4der_fault_scan_stack(STACK_LO, 64u, fake32, fake16, fake_text, NULL, out);
    CHECK(n == 3u && out[0] == TEXT_LO + 0x104u && out[1] == TEXT_LO + 0x204u && out[2] == TEXT_LO + 0x304u);
    CHECK(sh4der_fault_scan_stack(STACK_LO + 2u, 64u, fake32, fake16, fake_text, NULL, out) == 3u);          /* an unaligned SP is rounded down */
    CHECK(sh4der_fault_scan_stack(STACK_LO + 4u * 8u, 64u, fake32, fake16, fake_text, NULL, out) == 1u);     /* from higher up: only what is above; the read stops at the stack's end */
    CHECK(sh4der_fault_scan_stack(0x00000004u, 64u, fake32, fake16, fake_text, NULL, out) == 0u);            /* a wild SP: nothing read */
    CHECK(sh4der_fault_scan_stack(0xfffffffcu, 64u, fake32, fake16, fake_text, NULL, out) == 0u);
    for (i = 0; i < 64u; i++) stack[i] = TEXT_LO + 0x104u;        /* more than fit: kept to the limit */
    CHECK(sh4der_fault_scan_stack(STACK_LO, 64u, fake32, fake16, fake_text, NULL, out) == SH4DER_FAULT_TRACE_MAX);
}

int main(void) {
    crumbs();
    watchdog();
    reports();
    formatter();
    names();
    scan();
    if (fails) { fprintf(stderr, "fault_test: %d failure(s)\n", fails); return 1; }
    printf("fault_test: ok\n");
    return 0;
}
