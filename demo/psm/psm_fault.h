/* SH4DER-PVR: the demos' fault catcher

   demo/psm/psm_fault.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   A demo that dies on a console with nothing on screen tells nobody anything. This file puts a report on the screen
   instead, and stops. Two kinds, and the first line says which:

     *** EXCEPTION ***  the SH-4 raised an exception nothing handled (an address error, an illegal instruction, ...).
                        The report has the exception's name and code, PC, PR, SR, SP and all the registers, the
                        address at fault, and the return addresses found on the stack.
     *** HANG ***       the program stopped counting frames for PSM_FAULT_HANG_VBLANKS vertical blanks (2 s at
                        60 Hz) with interrupts still running. The report has the PC that was running when the watchdog
                        looked, the main thread's own PC and what it is waiting for, and the PVR's and the ASIC's
                        status registers.
     also               *** ASSERT *** (a failed assert) and *** EXIT *** (the program returned or called exit).

   Under the report: the breadcrumbs, a small fixed struct the demo fills every frame (frame count, the stage of the
   frame it reached, the pad, the menu, the camera, the vertex totals).

   How it draws. Not with the PVR, not with printf, not with anything that takes a lock or allocates: the lines are
   built with the small formatter in this file and drawn with KallistiOS's minifont (a pure function over a table)
   straight into video memory at offset 0, and the display is pointed there. Interrupts are off from then on, so no
   new scene starts; a render that was already running may still land once, so the report is drawn again every half
   second for ever, timed by polling the PVR's sync status register. The PVR is not reset: nothing about the display
   is changed but its start address. No thread, no interrupt and no heap is needed once the report is taken.

   How it hooks in (KallistiOS's own mechanisms, nothing replaced):
     EXC_UNHANDLED_EXC   KallistiOS calls this for any exception or interrupt no handler took, before it panics.
                         Handlers KallistiOS or the program installed (TRAPA, the timers, the ASIC) are left alone.
     EXC_DOUBLE_FAULT    an exception inside an interrupt handler.
     vblank_handler_add  the watchdog: one compare and one add a vertical blank.
     assert_set_handler, atexit

   What it cannot catch:
     - an exception while the SH-4 has exceptions blocked (inside KallistiOS's own exception entry, or inside this
       catcher): the CPU resets. The console goes back to its boot screen with nothing shown.
     - arch_panic and abort (KallistiOS's own "kernel panic", an out-of-memory abort): KallistiOS resets the PVR and
       leaves the program; there is no hook. The message goes to the serial line only.
     - a hang with interrupts masked (a loop inside an interrupt handler, or after irq_disable): the watchdog is an
       interrupt and never runs.
     - a wild write that destroys KallistiOS's handler table, the catcher's own data or the code.
     - floating point: KallistiOS leaves the FPU's traps off, so a NaN or a divide by zero raises nothing. An integer
       divide by zero raises nothing on the SH-4 either.
     - a picture that is wrong but still being drawn. That is not a fault.

   Cost a frame: psm_fault_beat is one increment; the breadcrumb setters are a few stores each (a frame's worth is
   under a hundred instructions; psm_fault_stage copies up to 11 characters). The watchdog is about twenty
   instructions a vertical blank. No memory but the crumbs (PSM_FAULT_CRUMBS_BYTES) and about 3 KB of report buffers.

   Use, in ONE source file of the program (the others include it without the define):
       #define PSM_FAULT_IMPL
       #include "psm_fault.h"
       static psm_fault_crumbs_t crumbs;
       ... after pvr_init:   psm_fault_init(&crumbs);
       ... each frame:       psm_fault_beat(&crumbs); and the setters below as the frame goes
   -DPSM_FAULT_TEST adds psm_fault_test() and psm_fault_test_combo(), which fault or hang on purpose (test hooks, for
   seeing the screens on a console; leave the define out of a build that is not for that).

   The first part of the file is plain C and is tested on the host (test/psm_fault_test.c). The second part is
   KallistiOS and compiles only for the Dreamcast. The screen assumes the 640 x 480 RGB565 mode the demos run in.
   Not run on a console when this file was written.
*/

#ifndef PSM_FAULT_H
#define PSM_FAULT_H

#include <stdint.h>
#include <string.h>

#define PSM_FAULT_COLS 64u                 /* characters a line: the demos' text width */
#define PSM_FAULT_ROWS 26u                 /* lines on the screen (8 x 16 characters, 640 x 480, margins kept) */
#define PSM_FAULT_HANG_VBLANKS 120u        /* vertical blanks without a heartbeat before the watchdog reports */
#define PSM_FAULT_TRACE_MAX 12u            /* return addresses kept from the stack */
#define PSM_FAULT_STAGE_LEN 12u

/* ---- breadcrumbs ---------------------------------------------------------------- */

/* What the program was doing, kept up to date as the frame goes. Plain values only: nothing in here is followed by
   the report, so nothing in here can fault it. */
typedef struct psm_fault_crumbs {
    uint32_t beat;                         /* the heartbeat: one more every frame (psm_fault_beat) */
    uint32_t frame;                        /* the frame counter */
    char stage[PSM_FAULT_STAGE_LEN];       /* where in the frame: "input", "map", "world", "text", "finish", ... */
    uint32_t pad;                          /* the last pad button mask */
    uint8_t scene, route;                  /* the scene and the shadow route on screen */
    uint8_t menu_open, menu_row;           /* the menu, and the row under the cursor */
    int32_t menu_value;                    /* that row's value */
    uint8_t change_row;                    /* the last row whose value was changed */
    uint8_t reserved[3];
    int32_t change_value;                  /* the value it was changed to */
    uint32_t change_frame;                 /* and the frame it happened on */
    int32_t cam_cm[3];                     /* the camera's position, in hundredths of a unit */
    int32_t yaw_cdeg, pitch_cdeg;          /* its heading, in hundredths of a degree */
    uint32_t verts[4];                     /* vertices sent this frame: ground, models, other, all */
    uint32_t tris;                         /* triangles sent this frame */
} psm_fault_crumbs_t;

#define PSM_FAULT_CRUMBS_BYTES ((unsigned)sizeof(psm_fault_crumbs_t))

static inline void psm_fault_beat(psm_fault_crumbs_t *c) { c->beat++; }

/* The stage of the frame the program is entering. At most 11 characters are kept. */
static inline void psm_fault_stage(psm_fault_crumbs_t *c, const char *name) {
    unsigned i;

    for (i = 0; i < PSM_FAULT_STAGE_LEN - 1u && name[i]; i++) c->stage[i] = name[i];
    c->stage[i] = 0;
}

static inline void psm_fault_frame(psm_fault_crumbs_t *c, uint32_t frame, uint32_t pad, unsigned scene, unsigned route) {
    c->frame = frame; c->pad = pad;
    c->scene = (uint8_t)scene; c->route = (uint8_t)route;
}

/* The menu as it is, and a change when the row's value is not the one last seen for that row. */
static inline void psm_fault_menu(psm_fault_crumbs_t *c, int open, unsigned row, int32_t value) {
    if (open && c->menu_open && c->menu_row == (uint8_t)row && c->menu_value != value) {
        c->change_row = (uint8_t)row; c->change_value = value; c->change_frame = c->frame;
    }
    c->menu_open = (uint8_t)(open != 0); c->menu_row = (uint8_t)row; c->menu_value = value;
}

/* The camera. The conversion to whole numbers is done here, in the frame, so the report needs no floating point. */
static inline void psm_fault_camera(psm_fault_crumbs_t *c, float x, float y, float z, float yaw_deg, float pitch_deg) {
    c->cam_cm[0] = (int32_t)(x * 100.0f); c->cam_cm[1] = (int32_t)(y * 100.0f); c->cam_cm[2] = (int32_t)(z * 100.0f);
    c->yaw_cdeg = (int32_t)(yaw_deg * 100.0f); c->pitch_cdeg = (int32_t)(pitch_deg * 100.0f);
}

static inline void psm_fault_counts(psm_fault_crumbs_t *c, uint32_t ground, uint32_t models, uint32_t other, uint32_t tris) {
    c->verts[0] = ground; c->verts[1] = models; c->verts[2] = other; c->verts[3] = ground + models + other;
    c->tris = tris;
}

/* ---- the watchdog's logic --------------------------------------------------------- */

typedef struct psm_fault_watch {
    uint32_t last;                         /* the heartbeat at the last look */
    uint32_t stale;                        /* looks since it last moved */
    int armed;                             /* a heartbeat has been seen: before the first one nothing is a hang */
    int fired;                             /* a hang was reported: it is reported once */
} psm_fault_watch_t;

/* One look at the heartbeat (one a vertical blank). 1 the one time the heartbeat has stood still for `limit` looks. */
static inline int psm_fault_watch_step(psm_fault_watch_t *w, uint32_t beat, uint32_t limit) {
    if (beat != w->last) { w->last = beat; w->stale = 0; w->armed = 1; return 0; }
    if (!w->armed || w->fired) return 0;
    if (++w->stale < limit) return 0;
    w->fired = 1;
    return 1;
}

/* ---- the report ------------------------------------------------------------------- */

enum { PSM_FAULT_EXCEPTION = 0, PSM_FAULT_DOUBLE = 1, PSM_FAULT_HANG = 2, PSM_FAULT_ASSERT = 3, PSM_FAULT_EXIT = 4 };

typedef struct psm_fault_report {
    int kind;
    uint32_t code;                         /* the SH-4's event code (EXPEVT / INTEVT) */
    uint32_t pc, pr, sr, gbr, vbr, mach, macl, fpscr, fpul, r[16];
    uint32_t tea;                          /* the address at fault, for the exceptions that have one */
    uint32_t trace[PSM_FAULT_TRACE_MAX];   /* return addresses found on the stack, nearest first */
    unsigned ntrace;
    /* a hang's extras */
    uint32_t stale;                        /* vertical blanks without a heartbeat */
    uint32_t main_pc, main_pr;             /* the main thread's saved PC and PR (it may not be the thread interrupted) */
    int main_state;
    char main_wait[24];                    /* what KallistiOS says it is waiting for */
    uint32_t ta_busy;                      /* pvr_check_ready: the tile accelerator still holds a scene */
    uint32_t istnrm, istext, isterr;       /* the ASIC's interrupt status */
    uint32_t pvr_opb_pos, pvr_vtx_pos, pvr_render_addr, pvr_sync, pvr_isp_feed;
    uint32_t vtx_used, vtx_used_max, pvr_frames, pvr_vbl;
    int have_stats;
    /* an assert's or an exit's text */
    char text[2][PSM_FAULT_COLS + 1u];
} psm_fault_report_t;

/* A line being written: never more than PSM_FAULT_COLS characters, always terminated. */
typedef struct psm_fault_line { char *s; unsigned n; } psm_fault_line_t;

static inline void psm_fault_line_begin(psm_fault_line_t *l, char *buf) { l->s = buf; l->n = 0; buf[0] = 0; }
static inline void psm_fault_put_c(psm_fault_line_t *l, char c) {
    if (l->n < PSM_FAULT_COLS) { l->s[l->n++] = c; l->s[l->n] = 0; }
}
static inline void psm_fault_put_s(psm_fault_line_t *l, const char *s) { while (*s) psm_fault_put_c(l, *s++); }
static inline void psm_fault_put_hex(psm_fault_line_t *l, uint32_t v, unsigned digits) {
    while (digits--) psm_fault_put_c(l, "0123456789abcdef"[(v >> (4u * digits)) & 15u]);
}
static inline void psm_fault_put_u(psm_fault_line_t *l, uint32_t v) {
    char t[10];
    unsigned n = 0;

    do { t[n++] = (char)('0' + v % 10u); v /= 10u; } while (v);
    while (n) psm_fault_put_c(l, t[--n]);
}
static inline void psm_fault_put_i(psm_fault_line_t *l, int32_t v) {
    if (v < 0) { psm_fault_put_c(l, '-'); psm_fault_put_u(l, 0u - (uint32_t)v); }
    else psm_fault_put_u(l, (uint32_t)v);
}
/* a value kept in hundredths, as 12.34 */
static inline void psm_fault_put_centi(psm_fault_line_t *l, int32_t v) {
    const uint32_t a = v < 0 ? 0u - (uint32_t)v : (uint32_t)v;

    if (v < 0) psm_fault_put_c(l, '-');
    psm_fault_put_u(l, a / 100u);
    psm_fault_put_c(l, '.');
    psm_fault_put_c(l, (char)('0' + a % 100u / 10u));
    psm_fault_put_c(l, (char)('0' + a % 10u));
}
/* " NAME xxxxxxxx" */
static inline void psm_fault_put_reg(psm_fault_line_t *l, const char *name, uint32_t v) {
    psm_fault_put_s(l, name); psm_fault_put_c(l, ' '); psm_fault_put_hex(l, v, 8u); psm_fault_put_s(l, "  ");
}

/* The SH-4's name for an event code. The codes shared by an instruction fetch and a data read are named for both. */
static inline const char *psm_fault_code_name(uint32_t code) {
    switch (code) {
    case 0x040: return "TLB MISS (READ OR FETCH)";
    case 0x060: return "TLB MISS (WRITE)";
    case 0x080: return "INITIAL PAGE WRITE";
    case 0x0a0: return "TLB PROTECTION (READ OR FETCH)";
    case 0x0c0: return "TLB PROTECTION (WRITE)";
    case 0x0e0: return "ADDRESS ERROR (READ OR FETCH)";
    case 0x100: return "ADDRESS ERROR (WRITE)";
    case 0x120: return "FPU EXCEPTION";
    case 0x140: return "TLB MULTIPLE HIT";
    case 0x160: return "TRAPA";
    case 0x180: return "ILLEGAL INSTRUCTION";
    case 0x1a0: return "ILLEGAL INSTRUCTION IN A DELAY SLOT";
    case 0x1c0: return "NMI";
    case 0x1e0: return "USER BREAK";
    case 0x800: return "FPU DISABLED";
    case 0x820: return "FPU DISABLED IN A DELAY SLOT";
    default: return code >= 0x200 ? "INTERRUPT NOBODY HANDLES" : "UNKNOWN EVENT";
    }
}

/* 1 when the event leaves the address at fault in TEA. */
static inline int psm_fault_code_has_tea(uint32_t code) { return code >= 0x040 && code <= 0x100; }

/* 1 when the 16-bit instruction is a call (JSR @Rn, BSR, BSRF Rn): the word two instructions before a return address. */
static inline int psm_fault_is_call(unsigned insn) {
    return (insn & 0xf0ffu) == 0x400bu || (insn & 0xf000u) == 0xb000u || (insn & 0xf0ffu) == 0x0003u;
}

/* Return addresses on a stack: every word from sp upward (`words` of them at most) that points into the program's
   code, is even, and comes two instructions after a call. `read32` and `read16` fetch memory and answer 0 for an
   address that must not be touched; `in_text` says whether an address is code. A word that passes is very likely a
   real return address, but old ones left on the stack pass too: read the list as "these calls, perhaps not all of
   them still open". Returns how many were kept (PSM_FAULT_TRACE_MAX at most). */
static inline unsigned psm_fault_scan_stack(uint32_t sp, unsigned words, int (*read32)(uint32_t addr, uint32_t *out, void *user),
                                            int (*read16)(uint32_t addr, unsigned *out, void *user), int (*in_text)(uint32_t addr, void *user),
                                            void *user, uint32_t *out) {
    unsigned i, n = 0, insn;
    uint32_t w;

    sp &= ~3u;
    for (i = 0; i < words && n < PSM_FAULT_TRACE_MAX; i++) {
        if (!read32(sp + 4u * i, &w, user)) break;
        if ((w & 1u) || w < 4u || !in_text(w, user) || !in_text(w - 4u, user)) continue;
        if (!read16(w - 4u, &insn, user) || !psm_fault_is_call(insn)) continue;
        out[n++] = w;
    }
    return n;
}

/* The report and the breadcrumbs as lines of text, PSM_FAULT_COLS wide at most. Returns the number of lines
   (PSM_FAULT_ROWS at most). `c` may be NULL. No library call but memset: safe wherever the report is taken. */
static inline unsigned psm_fault_format(const psm_fault_report_t *r, const psm_fault_crumbs_t *c, char ln[PSM_FAULT_ROWS][PSM_FAULT_COLS + 1u]) {
    static const char *const title[5] = { "*** EXCEPTION ***", "*** EXCEPTION in an interrupt handler ***", "*** HANG ***", "*** ASSERT ***", "*** EXIT ***" };
    psm_fault_line_t l;
    unsigned n = 0, i;

    memset(ln, 0, PSM_FAULT_ROWS * (PSM_FAULT_COLS + 1u));
    psm_fault_line_begin(&l, ln[n++]);
    psm_fault_put_s(&l, title[r->kind >= 0 && r->kind <= 4 ? r->kind : 0]);
    psm_fault_put_s(&l, "  demo stopped");

    if (r->kind == PSM_FAULT_EXCEPTION || r->kind == PSM_FAULT_DOUBLE) {
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_s(&l, psm_fault_code_name(r->code));
        psm_fault_put_s(&l, "  code "); psm_fault_put_hex(&l, r->code, 3u);
        if (psm_fault_code_has_tea(r->code)) { psm_fault_put_s(&l, "  at address "); psm_fault_put_hex(&l, r->tea, 8u); }
    } else if (r->kind == PSM_FAULT_HANG) {
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_s(&l, "no frame for "); psm_fault_put_u(&l, r->stale);
        psm_fault_put_s(&l, " vblanks. PC below = the code that was running");
    } else {
        psm_fault_line_begin(&l, ln[n++]); psm_fault_put_s(&l, r->text[0]);
        psm_fault_line_begin(&l, ln[n++]); psm_fault_put_s(&l, r->text[1]);
    }

    if (r->kind != PSM_FAULT_ASSERT && r->kind != PSM_FAULT_EXIT) {
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_reg(&l, "PC", r->pc); psm_fault_put_reg(&l, "PR", r->pr); psm_fault_put_reg(&l, "SR", r->sr); psm_fault_put_reg(&l, "SP", r->r[15]);
        for (i = 0; i < 16u; i++) {
            if (!(i & 3u)) psm_fault_line_begin(&l, ln[n++]);
            psm_fault_put_c(&l, 'R'); psm_fault_put_u(&l, i); if (i < 10u) psm_fault_put_c(&l, ' ');
            psm_fault_put_c(&l, ' '); psm_fault_put_hex(&l, r->r[i], 8u); psm_fault_put_s(&l, "  ");
        }
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_reg(&l, "GBR", r->gbr); psm_fault_put_reg(&l, "VBR", r->vbr); psm_fault_put_reg(&l, "FPSCR", r->fpscr); psm_fault_put_reg(&l, "FPUL", r->fpul);
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_reg(&l, "MACH", r->mach); psm_fault_put_reg(&l, "MACL", r->macl);
        for (i = 0; i < r->ntrace && i < PSM_FAULT_TRACE_MAX; i++) {
            if (!(i % 6u)) { psm_fault_line_begin(&l, ln[n++]); psm_fault_put_s(&l, i ? "      " : "CALLS "); }
            psm_fault_put_hex(&l, r->trace[i], 8u); psm_fault_put_c(&l, ' ');
        }
        if (!r->ntrace) { psm_fault_line_begin(&l, ln[n++]); psm_fault_put_s(&l, "CALLS none found on the stack"); }
    }

    if (r->kind == PSM_FAULT_HANG) {
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_s(&l, "MAIN "); psm_fault_put_reg(&l, "PC", r->main_pc); psm_fault_put_reg(&l, "PR", r->main_pr);
        psm_fault_put_s(&l, "state "); psm_fault_put_i(&l, r->main_state); psm_fault_put_s(&l, " (1 run 2 ready 3 wait)");
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_s(&l, "MAIN waits for: "); psm_fault_put_s(&l, r->main_wait[0] ? r->main_wait : "(no message)");
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_s(&l, "ASIC "); psm_fault_put_reg(&l, "NRM", r->istnrm); psm_fault_put_reg(&l, "EXT", r->istext); psm_fault_put_reg(&l, "ERR", r->isterr);
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_s(&l, "PVR TA busy "); psm_fault_put_u(&l, r->ta_busy); psm_fault_put_s(&l, "  ");
        psm_fault_put_reg(&l, "OPB", r->pvr_opb_pos); psm_fault_put_reg(&l, "VTX", r->pvr_vtx_pos);
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_s(&l, "PVR "); psm_fault_put_reg(&l, "RADDR", r->pvr_render_addr); psm_fault_put_reg(&l, "SYNC", r->pvr_sync); psm_fault_put_reg(&l, "ISP", r->pvr_isp_feed);
        psm_fault_line_begin(&l, ln[n++]);
        if (r->have_stats) {
            psm_fault_put_s(&l, "PVR vtx buf "); psm_fault_put_u(&l, r->vtx_used); psm_fault_put_s(&l, " max "); psm_fault_put_u(&l, r->vtx_used_max);
            psm_fault_put_s(&l, "  frames "); psm_fault_put_u(&l, r->pvr_frames); psm_fault_put_s(&l, " vbl "); psm_fault_put_u(&l, r->pvr_vbl);
        } else {
            psm_fault_put_s(&l, "PVR stats: none");
        }
    }

    if (c) {
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_s(&l, "FRAME "); psm_fault_put_u(&l, c->frame); psm_fault_put_s(&l, "  BEAT "); psm_fault_put_u(&l, c->beat);
        psm_fault_put_s(&l, "  STAGE "); for (i = 0; i < PSM_FAULT_STAGE_LEN - 1u && c->stage[i]; i++) psm_fault_put_c(&l, c->stage[i]);
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_s(&l, "SCENE "); psm_fault_put_u(&l, c->scene); psm_fault_put_s(&l, "  ROUTE "); psm_fault_put_u(&l, c->route);
        psm_fault_put_s(&l, "  PAD "); psm_fault_put_hex(&l, c->pad, 8u);
        psm_fault_put_s(&l, "  MENU "); psm_fault_put_s(&l, c->menu_open ? "open" : "shut");
        psm_fault_put_s(&l, " row "); psm_fault_put_u(&l, c->menu_row); psm_fault_put_s(&l, " = "); psm_fault_put_i(&l, c->menu_value);
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_s(&l, "LAST CHANGE row "); psm_fault_put_u(&l, c->change_row); psm_fault_put_s(&l, " to "); psm_fault_put_i(&l, c->change_value);
        psm_fault_put_s(&l, " on frame "); psm_fault_put_u(&l, c->change_frame);
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_s(&l, "CAM "); psm_fault_put_centi(&l, c->cam_cm[0]); psm_fault_put_c(&l, ' '); psm_fault_put_centi(&l, c->cam_cm[1]);
        psm_fault_put_c(&l, ' '); psm_fault_put_centi(&l, c->cam_cm[2]);
        psm_fault_put_s(&l, "  YAW "); psm_fault_put_centi(&l, c->yaw_cdeg); psm_fault_put_s(&l, "  PITCH "); psm_fault_put_centi(&l, c->pitch_cdeg);
        psm_fault_line_begin(&l, ln[n++]);
        psm_fault_put_s(&l, "VERTS ground "); psm_fault_put_u(&l, c->verts[0]); psm_fault_put_s(&l, " models "); psm_fault_put_u(&l, c->verts[1]);
        psm_fault_put_s(&l, " other "); psm_fault_put_u(&l, c->verts[2]); psm_fault_put_s(&l, " all "); psm_fault_put_u(&l, c->verts[3]);
        psm_fault_put_s(&l, " TRIS "); psm_fault_put_u(&l, c->tris);
    } else {
        psm_fault_line_begin(&l, ln[n++]); psm_fault_put_s(&l, "no breadcrumbs were given");
    }
    psm_fault_line_begin(&l, ln[n++]);
    psm_fault_put_s(&l, "Photograph this screen. Power off to restart.");
    return n;
}

/* ---- the Dreamcast part (KallistiOS), in the one file that defines PSM_FAULT_IMPL ------ */

#ifdef _arch_dreamcast

#include <assert.h>
#include <stdlib.h>
#include <kos.h>
#include <dc/minifont.h>
#include <dc/vblank.h>

/* Hook everything up. `crumbs` may be NULL (the report then has none, and there is no hang watchdog: it needs the
   heartbeat). Call after pvr_init. 0, or -1 when a hook was refused. */
int psm_fault_init(psm_fault_crumbs_t *crumbs);

#ifdef PSM_FAULT_TEST
/* TEST HOOK: fault or hang on purpose, to see the screens on a console. */
enum { PSM_FAULT_TEST_ADDRESS = 0, PSM_FAULT_TEST_ILLEGAL = 1, PSM_FAULT_TEST_SPIN = 2, PSM_FAULT_TEST_BLOCK = 3, PSM_FAULT_TEST_ASSERT = 4 };
void psm_fault_test(int what);

/* TEST HOOK: the pad combination for a demo. Both triggers and START held, then one more button:
   X an address error, Y an illegal instruction, B a spinning hang, A a blocked hang. Call it with the pad's state
   each frame; it does not come back when the combination is down. */
static inline void psm_fault_test_combo(uint32_t buttons, int ltrig, int rtrig) {
    if (ltrig < 200 || rtrig < 200 || !(buttons & CONT_START)) return;
    if (buttons & CONT_X) psm_fault_test(PSM_FAULT_TEST_ADDRESS);
    if (buttons & CONT_Y) psm_fault_test(PSM_FAULT_TEST_ILLEGAL);
    if (buttons & CONT_B) psm_fault_test(PSM_FAULT_TEST_SPIN);
    if (buttons & CONT_A) psm_fault_test(PSM_FAULT_TEST_BLOCK);
}
#endif

#ifdef PSM_FAULT_IMPL

#define PSM_FAULT_REG32(a) (*(volatile uint32_t *)(a))
#define PSM_FAULT_TEA      PSM_FAULT_REG32(0xff00000c)   /* the SH-4's TLB exception address */
#define PSM_FAULT_ISTNRM   PSM_FAULT_REG32(0xa05f6900)
#define PSM_FAULT_ISTEXT   PSM_FAULT_REG32(0xa05f6904)
#define PSM_FAULT_ISTERR   PSM_FAULT_REG32(0xa05f6908)
#define PSM_FAULT_FB       ((uint16_t *)0xa5000000)      /* video memory, 32-bit area, offset 0: where the report is drawn */
#define PSM_FAULT_VSYNC    (1u << 13)                    /* PVR_SYNC_STATUS: in vertical blank */

static psm_fault_crumbs_t *psm_fault_crumbs_;
static psm_fault_watch_t psm_fault_watch_;
static kthread_t *psm_fault_main_;
static int psm_fault_interlace_;
static volatile int psm_fault_shown_;
static psm_fault_report_t psm_fault_rep_;
static char psm_fault_ln_[PSM_FAULT_ROWS][PSM_FAULT_COLS + 1u];

static int psm_fault_read32_(uint32_t addr, uint32_t *out, void *user) {
    (void)user;
    if ((addr & 3u) || !arch_valid_address(addr) || !arch_valid_address(addr + 3u)) return 0;
    *out = *(const uint32_t *)(uintptr_t)addr;
    return 1;
}
static int psm_fault_read16_(uint32_t addr, unsigned *out, void *user) {
    (void)user;
    if ((addr & 1u) || !arch_valid_text_address(addr)) return 0;
    *out = *(const uint16_t *)(uintptr_t)addr;
    return 1;
}
static int psm_fault_in_text_(uint32_t addr, void *user) { (void)user; return arch_valid_text_address(addr); }

/* Wait for `n` vertical blanks by polling the PVR's status. Bounded: a dead video clock does not hold it for ever. */
static void psm_fault_vblanks_(unsigned n) {
    unsigned spin;

    while (n--) {
        for (spin = 0; spin < 4000000u && (PVR_GET(PVR_SYNC_STATUS) & PSM_FAULT_VSYNC); spin++) {}
        for (spin = 0; spin < 4000000u && !(PVR_GET(PVR_SYNC_STATUS) & PSM_FAULT_VSYNC); spin++) {}
    }
}

/* Put the report on the screen and never come back. */
static void __attribute__((noreturn)) psm_fault_show_(const psm_fault_report_t *r) {
    unsigned n, i, k;

    irq_disable();
    n = psm_fault_format(r, psm_fault_crumbs_, psm_fault_ln_);
    for (;;) {
        PVR_SET(PVR_FB_ADDR, 0);                                 /* show video memory from offset 0 */
        if (psm_fault_interlace_) PVR_SET(PVR_FB_IL_ADDR, 640 * 2);
        for (k = 0; k < 640u * 480u; k++) PSM_FAULT_FB[k] = 0;
        for (i = 0; i < n; i++) {
            minifont_set_color(255, 255, i ? 255 : 0);           /* the title in yellow */
            minifont_draw_str(PSM_FAULT_FB + (32u + 16u * i) * 640u + 64u, 640, psm_fault_ln_[i]);
        }
        psm_fault_vblanks_(30u);                                 /* and again, in case a render was still landing */
    }
}

static void psm_fault_regs_(psm_fault_report_t *r, const irq_context_t *cx) {
    unsigned i;

    r->pc = cx->pc; r->pr = cx->pr; r->sr = cx->sr; r->gbr = cx->gbr; r->vbr = cx->vbr;
    r->mach = cx->mach; r->macl = cx->macl; r->fpscr = cx->fpscr; r->fpul = cx->fpul;
    for (i = 0; i < 16u; i++) r->r[i] = cx->r[i];
    r->ntrace = psm_fault_scan_stack(cx->r[15], 256u, psm_fault_read32_, psm_fault_read16_, psm_fault_in_text_, NULL, r->trace);
}

/* An exception or an interrupt nothing handled, or one inside an interrupt handler (data != NULL). */
static void psm_fault_exception_(irq_t code, irq_context_t *cx, void *data) {
    psm_fault_report_t *r = &psm_fault_rep_;

    if (psm_fault_shown_) for (;;) {}                            /* a fault while showing a fault: stay where we are */
    psm_fault_shown_ = 1;
    memset(r, 0, sizeof *r);
    r->kind = data ? PSM_FAULT_DOUBLE : PSM_FAULT_EXCEPTION;
    r->code = (uint32_t)code;
    r->tea = PSM_FAULT_TEA;
    psm_fault_regs_(r, cx);
    psm_fault_show_(r);
}

/* The watchdog: one look a vertical blank. */
static void psm_fault_vblank_(uint32_t code, void *data) {
    psm_fault_report_t *r = &psm_fault_rep_;
    const kthread_t *t = psm_fault_main_;
    pvr_stats_t st;
    unsigned i;

    (void)code; (void)data;
    if (!psm_fault_crumbs_ || !psm_fault_watch_step(&psm_fault_watch_, psm_fault_crumbs_->beat, PSM_FAULT_HANG_VBLANKS)) return;
    if (psm_fault_shown_) return;
    psm_fault_shown_ = 1;
    memset(r, 0, sizeof *r);
    r->kind = PSM_FAULT_HANG;
    r->stale = psm_fault_watch_.stale;
    r->istnrm = PSM_FAULT_ISTNRM; r->istext = PSM_FAULT_ISTEXT; r->isterr = PSM_FAULT_ISTERR;
    r->pvr_opb_pos = PVR_GET(PVR_TA_OPB_POS); r->pvr_vtx_pos = PVR_GET(PVR_TA_VERTBUF_POS);
    r->pvr_render_addr = PVR_GET(PVR_RENDER_ADDR); r->pvr_sync = PVR_GET(PVR_SYNC_STATUS); r->pvr_isp_feed = PVR_GET(PVR_ISP_START);
    r->ta_busy = pvr_check_ready() < 0;
    if (pvr_get_stats(&st) == 0) {
        r->have_stats = 1;
        r->vtx_used = (uint32_t)st.vtx_buffer_used; r->vtx_used_max = (uint32_t)st.vtx_buffer_used_max;
        r->pvr_frames = (uint32_t)st.frame_count; r->pvr_vbl = (uint32_t)st.vbl_count;
    }
    psm_fault_regs_(r, irq_get_context());                       /* the code this interrupt cut into */
    if (t) {
        r->main_pc = t->context.pc; r->main_pr = t->context.pr; r->main_state = (int)t->state;
        if (t->wait_msg && arch_valid_address((uintptr_t)t->wait_msg))
            for (i = 0; i < sizeof r->main_wait - 1u && arch_valid_address((uintptr_t)t->wait_msg + i) && t->wait_msg[i]; i++) r->main_wait[i] = t->wait_msg[i];
    }
    psm_fault_show_(r);
}

static void psm_fault_text_(psm_fault_report_t *r, unsigned row, const char *a, const char *b, int32_t num, int with_num) {
    psm_fault_line_t l;

    psm_fault_line_begin(&l, r->text[row]);
    if (a) psm_fault_put_s(&l, a);
    if (with_num) psm_fault_put_i(&l, num);
    if (b) psm_fault_put_s(&l, b);
}

static void psm_fault_assert_(const char *file, int line, const char *expr, const char *msg, const char *func) {
    psm_fault_report_t *r = &psm_fault_rep_;

    (void)msg;
    if (psm_fault_shown_) for (;;) {}
    psm_fault_shown_ = 1;
    memset(r, 0, sizeof *r);
    r->kind = PSM_FAULT_ASSERT;
    psm_fault_text_(r, 0u, expr ? expr : "(no expression)", NULL, 0, 0);
    psm_fault_text_(r, 1u, file ? file : "?", NULL, 0, 0);
    {
        psm_fault_line_t l = { r->text[1], (unsigned)strlen(r->text[1]) };

        psm_fault_put_s(&l, " line "); psm_fault_put_i(&l, line);
        if (func) { psm_fault_put_s(&l, " in "); psm_fault_put_s(&l, func); }
    }
    psm_fault_show_(r);
}

static void psm_fault_exit_(void) {
    psm_fault_report_t *r = &psm_fault_rep_;

    if (psm_fault_shown_) return;
    psm_fault_shown_ = 1;
    memset(r, 0, sizeof *r);
    r->kind = PSM_FAULT_EXIT;
    psm_fault_text_(r, 0u, "the program returned from main or called exit", NULL, 0, 0);
    psm_fault_text_(r, 1u, "(not a crash of the CPU: look for an early return)", NULL, 0, 0);
    psm_fault_show_(r);
}

int psm_fault_init(psm_fault_crumbs_t *crumbs) {
    int bad = 0;

    psm_fault_crumbs_ = crumbs;
    memset(&psm_fault_watch_, 0, sizeof psm_fault_watch_);
    if (crumbs) psm_fault_watch_.last = crumbs->beat;
    psm_fault_main_ = thd_get_current();
    psm_fault_interlace_ = (vid_mode->flags & VID_INTERLACE) != 0;
    psm_fault_shown_ = 0;
    bad |= irq_set_handler(EXC_UNHANDLED_EXC, psm_fault_exception_, NULL) < 0;
    bad |= irq_set_handler(EXC_DOUBLE_FAULT, psm_fault_exception_, (void *)1) < 0;
    bad |= vblank_handler_add(psm_fault_vblank_, NULL) < 0;
    assert_set_handler(psm_fault_assert_);
    bad |= atexit(psm_fault_exit_) != 0;
    return bad ? -1 : 0;
}

#ifdef PSM_FAULT_TEST
/* TEST HOOK. Each of these ends the program on purpose. */
void psm_fault_test(int what) {
    switch (what) {
    case PSM_FAULT_TEST_ADDRESS:                                 /* a 32-bit write to an odd address: ADDRESS ERROR (WRITE) at 8c000001 */
        *(volatile uint32_t *)0x8c000001 = 0x12345678u;
        break;
    case PSM_FAULT_TEST_ILLEGAL:                                 /* the SH-4's undefined opcode: ILLEGAL INSTRUCTION */
        __asm__ volatile (".word 0xfffd");
        break;
    case PSM_FAULT_TEST_SPIN:                                    /* the main thread loops with interrupts on: HANG, PC in this loop */
        for (;;) { __asm__ volatile ("nop"); }
    case PSM_FAULT_TEST_BLOCK:                                   /* the main thread sleeps for ever: HANG, "MAIN waits for" filled in */
        for (;;) thd_sleep(1000000);
    case PSM_FAULT_TEST_ASSERT:
        assert(what == -1);
        break;
    default:
        break;
    }
}
#endif /* PSM_FAULT_TEST */

#endif /* PSM_FAULT_IMPL */
#endif /* _arch_dreamcast */

#endif /* PSM_FAULT_H */
