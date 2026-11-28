/* SH4DER-PVR: a timed run that ends on a results page, for a program that otherwise runs for ever

   include/sh4der_waitrun.h
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   RECOMMENDED
   Cost:       one call a frame: a clock read and one store (4 bytes a frame, SH4DER_WAITRUN_MAX frames = 6 KB of RAM). The pages
               are drawn once, after the PVR is shut down.
   Start with: SH4DER_WAITRUN_SECONDS 20, SH4DER_WAITRUN_SKIP 20 frames of left out of the figures.
   Status:     PROTOTYPE. The statistics are plain C and host tested (test/waitrun_test.c); the results pages are NOT RUN on a
               console.
   Do:         call sh4der_waitrun_frame at the same place every frame (the top of the frame loop) and only when the program
               was started from the test suite's menu (suite_present()); a disc of the program's own keeps its live behaviour.
   Do not:     call it from a program that also draws its own result pages; the pages here shut the PVR down.
   Maturity:   ALPHA v0.1
   Needs:      <kos.h>, then sh4der_suite.h, sh4der_pvrx.h; sh4der_page.h is included here (PG_PREFIX "wr").

   Why: a live demo run from the suite disc has no result, so a console run of it says nothing a person can photograph.
   This gives every such program the same ending: it runs the program once, SH4DER_WAITRUN_SECONDS each, with
   the woken wait (the pvrx hook, PROTOTYPE), records every
   frame's time (start to start), shuts the PVR down and shows a text page (frames, mean, best, worst, frames over 20 ms of
   each mode) and two graphs (the mean and the worst frame of each of 64 slices of a run, one
   line a mode). A and B turn the pages, A held 2 s on the last page or A+B+X+Y goes back to the suite's menu.

     static sh4der_waitrun_t wr;
     ...
     if (suite_present()) {                              (once, before the loop: sh4der_waitrun_init(&wr, SH4DER_WAITRUN_SECONDS);)
         if (sh4der_waitrun_frame(&wr, timer_us_gettime64())) sh4der_waitrun_show(&wr, "OCEAN");
     }

   sh4der_waitrun_show does not return to the program: it ends in the suite's page loop. */

#ifndef SH4DER_WAITRUN_H
#define SH4DER_WAITRUN_H

#include <stdint.h>

#ifndef SH4DER_WAITRUN_SECONDS
#define SH4DER_WAITRUN_SECONDS 20u
#endif
#ifndef SH4DER_WAITRUN_SKIP
#define SH4DER_WAITRUN_SKIP 20u
#endif
#define SH4DER_WAITRUN_MAX 1500u                /* 25 s at 60 frames a second */
#define SH4DER_WAITRUN_MODES 1u                 /* one run in the woken wait: a live scene cannot be replayed from its start for a second mode, so the sleep-against-wake comparison lives in demo/psm/waitpoll (identical frames) */
#define SH4DER_WAITRUN_SLICES 64u               /* the graph's points: PG_POINTS in sh4der_page.h */

typedef struct sh4der_waitrun {
    uint32_t us[SH4DER_WAITRUN_MODES][SH4DER_WAITRUN_MAX];   /* frame start to start, microseconds; us[m][0] is 0 */
    unsigned n[SH4DER_WAITRUN_MODES];
    unsigned phase;                         /* the mode running now */
    unsigned seconds;                       /* a mode's time */
    uint64_t t0, last;
    int started;
} sh4der_waitrun_t;

typedef struct sh4der_waitrun_stats {
    unsigned counted;                       /* frames after the skip */
    unsigned mean_us, min_us, max_us;
    unsigned slow;                          /* frames over 20,000 us */
} sh4der_waitrun_stats_t;

static const char *const sh4der_waitrun_mode_name[SH4DER_WAITRUN_MODES] = { "wake" };

static inline void sh4der_waitrun_init(sh4der_waitrun_t *w, unsigned seconds) {
    unsigned m, i;

    for (m = 0; m < SH4DER_WAITRUN_MODES; m++) { for (i = 0; i < SH4DER_WAITRUN_MAX; i++) w->us[m][i] = 0u; w->n[m] = 0u; }
    w->phase = 0u;
    w->seconds = seconds ? seconds : SH4DER_WAITRUN_SECONDS;
    w->t0 = w->last = 0u;
    w->started = 0;
}

#ifdef _arch_dreamcast
#include "sh4der_pvrx.h"
#endif

/* Put the wait of mode m in force: finish what is in flight, then the hook off (sleeping wait) or on (woken wait). */
static inline void sh4der_waitrun_mode(unsigned m) {
#ifdef _arch_dreamcast
    (void)m;
    sh4der_pvrx_wait_idle();
    sh4der_pvrx_wait_hook(0);
    sh4der_pvrx_wait_hook(2);
#else
    (void)m;
#endif
}

/* Once a frame, at the same place. 1 when both modes have run. A mode ends when its time is up or its table is full. */
static inline int sh4der_waitrun_frame(sh4der_waitrun_t *w, uint64_t now_us) {
    const unsigned m = w->phase;

    if (!w->started) { w->started = 1; sh4der_waitrun_mode(0u); w->t0 = w->last = now_us; w->n[0] = 0u; }
    if (w->n[m] < SH4DER_WAITRUN_MAX) {
        w->us[m][w->n[m]] = w->n[m] ? (uint32_t)(now_us - w->last) : 0u;
        w->n[m]++;
    }
    w->last = now_us;
    if (w->n[m] >= SH4DER_WAITRUN_MAX || now_us - w->t0 >= (uint64_t)w->seconds * 1000000ull) {
        if (m + 1u >= SH4DER_WAITRUN_MODES) return 1;
        w->phase = m + 1u;
        sh4der_waitrun_mode(w->phase);
        w->n[w->phase] = 0u;
        w->t0 = w->last = now_us;             /* the switch's own time is in neither mode's frames */
    }
    return 0;
}

static inline void sh4der_waitrun_stats(const sh4der_waitrun_t *w, unsigned m, sh4der_waitrun_stats_t *s) {
    unsigned i;
    uint64_t sum = 0u;

    s->counted = s->mean_us = s->min_us = s->max_us = s->slow = 0u;
    for (i = SH4DER_WAITRUN_SKIP; i < w->n[m]; i++) {
        const unsigned v = w->us[m][i];

        if (!s->counted || v < s->min_us) s->min_us = v;
        if (v > s->max_us) s->max_us = v;
        if (v > 20000u) s->slow++;
        sum += v;
        s->counted++;
    }
    if (s->counted) s->mean_us = (unsigned)(sum / s->counted);
}

/* Slice k of SH4DER_WAITRUN_SLICES of mode m: the mean and the worst frame in it, in microseconds. 0 for an empty slice. */
static inline void sh4der_waitrun_slice(const sh4der_waitrun_t *w, unsigned m, unsigned k, unsigned *mean_us, unsigned *max_us) {
    const unsigned lo = (unsigned)((uint64_t)w->n[m] * k / SH4DER_WAITRUN_SLICES), hi = (unsigned)((uint64_t)w->n[m] * (k + 1u) / SH4DER_WAITRUN_SLICES);
    unsigned i, mx = 0u;
    uint64_t sum = 0u;

    for (i = lo; i < hi; i++) { sum += w->us[m][i]; if (w->us[m][i] > mx) mx = w->us[m][i]; }
    *mean_us = hi > lo ? (unsigned)(sum / (hi - lo)) : 0u;
    *max_us = mx;
}

#ifdef _arch_dreamcast

#ifndef PG_PREFIX
#define PG_PREFIX "wr"
#endif
#include "sh4der_page.h"
#include "sh4der_suite.h"

/* Shut the PVR down and show the run: text page, then two graphs. Never returns (the suite's page loop). */
static void sh4der_waitrun_show(const sh4der_waitrun_t *w, const char *name) {
    sh4der_waitrun_stats_t s[SH4DER_WAITRUN_MODES];
    unsigned k, m, page = 0, pages;
    int redraw = 1;
    pg_plot_t *p;
    char title[40];

    for (m = 0; m < SH4DER_WAITRUN_MODES; m++) sh4der_waitrun_stats(w, m, &s[m]);
    sh4der_pvrx_wait_idle();
    sh4der_pvrx_wait_hook(0);
    pvr_shutdown();
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    pg_nlines = 0; pg_nplots = 0;
    pg_say("%s: a timed run of the program as it plays, %u s", name, w->seconds);
    pg_say("wait: KallistiOS's sleeping waits with the pvrx hook (woken at once, PROTOTYPE).");
    pg_say("One console, one run: MEASURED only where a line says a number.");
    pg_say("");
    pg_say("frames        %9u", w->n[0]);
    pg_say("mean us       %9u", s[0].mean_us);
    pg_say("best us       %9u", s[0].min_us);
    pg_say("worst us      %9u", s[0].max_us);
    pg_say("over 20 ms    %9u", s[0].slow);
    pg_say("(60 Hz = 16,667 us; the first %u frames of each mode are left out)", (unsigned)SH4DER_WAITRUN_SKIP);
    pg_say("");
    pg_say("Frame time is frame start to start on the SH-4 clock. Next: the mean and the");
    pg_say("worst frame of each run, 64 slices, one line. A and B turn the page.");
    for (m = 0; m < 2u; m++) {
        snprintf(title, sizeof title, "%s %s frame ms", name, m ? "worst" : "mean");
        p = pg_plot(title, "slice", "ms", 0);
        for (k = 0; k < SH4DER_WAITRUN_MODES; k++) {
            unsigned j;

            pg_series(p, sh4der_waitrun_mode_name[k]);
            for (j = 0; j < SH4DER_WAITRUN_SLICES; j++) { unsigned a, x; sh4der_waitrun_slice(w, k, j, &a, &x); pg_point(p, (float)j, (float)(m ? x : a) * 0.001f); }
        }
    }
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;

        pages = pg_pages();
        if (suite_page(st ? st->buttons : 0u, &page, pages)) redraw = 1;
        if (redraw) { pg_draw(page % pages, name, suite_keys_text("A/B pages")); redraw = 0; }
        thd_sleep(20);
    }
}

#endif /* _arch_dreamcast */

#endif /* SH4DER_WAITRUN_H */
