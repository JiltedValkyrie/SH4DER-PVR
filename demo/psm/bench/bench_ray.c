/* SH4DER-PVR: the ray bench. Copyright (c) 2026 Jilted Valkyrie. See ../../../LICENSE.

   THESIS_SH4_SHADOWS.md, predictions P1, P2 and P9 (section 6), timed on a console: the height-grid walk toward the sun
   (5.3), a slice of such rays (5.9), and the movers' proxies (5.6). The kernels and their C references are in
   ray_kernels.h, which the host test ray_test.c checked first (the same bytes from the kernel and the reference, the
   walk's cell count against 5.3's n, a hit where the geometry says, the shadows where the runs say).

     group 1 (at boot)
       clock     the two clocks: the KallistiOS ns timer and performance counter 1 counting CPU cycles (s2's pair)
       P1.1-4    rays of known n: on a 4 KB grid of zeros (every ray lit, n set by its height or the edge) at 14:00 and
                 16:00; on the town grid (lit rays only: n 24 and more there) at 14:00 and 16:00. Cycles a ray at each
                 n, and a straight-line fit C_setup + n C_step against 5.3's ESTIMATE (C_step 12 to 16, C_setup about 30)
       P2.1-2    a slice of 1,024 receivers (a STAND-IN for the town's vertices, ray_kernels.h says how) at 14:00 and
                 16:00: batched with the grid cold, batched with the grid warm, 28,699 PVR vertices projected alone,
                 and the same slice interleaved with the projection (8 rays, then the vertices' share)
       P9        the proxy pass at 16:00: 8 people (capsules) and 4 hens (spheres) at their load places, each against
                 the 0.5 m ground points in its shadow run (7.41 m for a person), against the slice
       h3        the transform: mat_transform, SH4ZAM-DCE's projecting batch built with and without its prefetch, and the
                 plain SH4ZAM loop; 256 vectors in the cache, 16,384 not
     group 2 (START)
       P2.3-4    the slice with the grid in OC-RAM (the operand cache's half as RAM), batched and interleaved, at 14:00
                 and 16:00. OC-RAM is switched on and LEFT ON until the power goes off (as bench h1 did).

   Placing (F24): one pool with offsets, no two arrays an exact multiple of 16 KB apart (the grid, the points, the bytes,
   the vertex streams); the offsets are under "memory".
   Every result line starts with "RESULT" and its page's test number; a page's first line says what is measured and holds
   no result. Every number comes from the run that prints it: one run, one console; only that is evidence. Lines marked
   DERIVED are arithmetic on the RESULT lines above them.

   Only RAM and SH-4 registers are written (the OC-RAM switch is in the cache's own register: a power cycle clears it).
   No flash, no GD-ROM, no sound chip, no PVR drawing. Every loop has a fixed count.
   Build: python demo/psm/bench/build.py --src bench_ray.c --tag <new tag>. Image ray1 ran on a console on 2026-11-25
   (NOTES.md). ray2, changes: CLOCK reads both clocks together (ray1's CLOCK.2 held a print) and
   looks for the ns clock's step once a second; every P1 row prints the ns less the cycles, the read gaps, whether the ns
   clock's second changed, and the interrupts of a second run; P1.2 and P1.4 run three times; page RT times a shadow ray
   against a triangle (the kernels and their references are in ray_kernels.h, host tested).
*/

#include <kos.h>
#include <dc/cache.h>
#include <stdarg.h>

#include "sh4zam/shz_sh4zam.h"
#include "sh4zam_dce/shz_dce.h"
#include "ray_kernels.h"

#define SH4DER_FAULT_IMPL
#include "sh4der_fault.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

_Static_assert(sizeof(double) == 8, "the fit needs a real double (-m4-single, not -m4-single-only)");

void h3_project_np(void *out, const void *in, unsigned n);   /* bench_ray_np.c */

/* ---- the result pages ----------------------------------------------------------- */

#define COLS 50u
#define ROWS 17u
#define LINES_MAX 1400u
static char lines[LINES_MAX][COLS + 1u];
static unsigned nlines;
static sh4der_fault_crumbs_t crumbs;

/* A line for the pages and the serial line. A line over the page's width goes on, indented, on the next. */
static void say(const char *fmt, ...) {
    char big[240];
    const char *s = big;
    size_t left;
    va_list ap;

    va_start(ap, fmt);
    vsnprintf(big, sizeof big, fmt, ap);
    va_end(ap);
    printf("ray: %s\n", big);
    for (left = strlen(big); nlines < LINES_MAX; ) {
        const int cont = s != big;
        const size_t room = cont ? COLS - 3u : COLS, n = left < room ? left : room;

        snprintf(lines[nlines++], COLS + 1u, "%s%.*s", cont ? "   " : "", (int)n, s);
        s += n; left -= n;
        if (!left) break;
    }
}

/* A new test starts on a new page: one test a page, for the photographs. */
static void page_break(void) {
    while (nlines % ROWS && nlines < LINES_MAX) lines[nlines++][0] = '\0';
}

/* The first line of a test's page: what is measured. It carries no result. */
static void heading(const char *id, const char *what) {
    page_break();
    say("TEST %s (what is measured, not a result):", id);
    say("  %s", what);
}

static void page_draw(unsigned page, unsigned pages, const char *next) {
    char head[96];
    unsigned i;

    vid_clear(0, 0, 0);
    snprintf(head, sizeof head, "RAY BENCH p%u/%u  A/B page  %s", page + 1u, pages, next);
    bfont_draw_str(vram_s + 16u * 640u + 20u, 640, true, head);
    for (i = 0; i < ROWS && page * ROWS + i < nlines; i++)
        bfont_draw_str(vram_s + (16u + 24u * (i + 2u)) * 640u + 20u, 640, true, lines[page * ROWS + i]);
}

static void step(const char *name) {
    sh4der_fault_beat(&crumbs);
    sh4der_fault_stage(&crumbs, name);
    crumbs.frame++;
}

/* ---- clocks and memory ---------------------------------------------------------- */

#define MB (1024u * 1024u)
#define KB 1024u
#define OC 16384u                          /* the operand cache: two addresses this far apart share an entry */
#define REG32(a) (*(volatile uint32_t *)(a))

/* One pool, so the distances are known; no two starts an exact multiple of 16 KB apart. */
static uint8_t pool[256u * KB + 2u * MB + 2u * OC] __attribute__((aligned(32)));
#define GRID (pool)                                              /* 4 KB: the grid the walk reads */
#define RAYS ((float (*)[4])(void *)(pool + 5u * KB))             /* up to 4,096 points of 16 B */
#define LIT (pool + 72u * KB + 512u)                              /* 4,096 bytes from the kernel */
#define LITREF (pool + 80u * KB + 1536u)                          /* 4,096 bytes from the reference */
#define NSTEP (pool + 88u * KB + 2560u)                           /* 65,536 bytes: each candidate's n (0: not used) */
#define P9PTS ((float (*)[4])(void *)(pool + 160u * KB + 3584u))  /* up to 4,096 receivers of 16 B */
#define VIN (pool + 256u * KB)                                    /* 1 MB: P1's candidates, then the vertices in */
#define VOUT (pool + 256u * KB + MB + OC / 2u)                    /* the vertices out */
#define NCAND 65536u                                              /* P1's candidates: 1 MB at VIN */
#define CAND ((float (*)[4])(void *)VIN)
#define PK 1024u                                                  /* the slice */
#define NV 28699u                                                 /* the a3 frame's TA vertices (MEASURED, 5.2) */
#define OCRAM ((uint8_t *)0x7c001000u)                            /* where bench h1 found it (F23) */
#define CCR REG32(0xff00001c)

static inline uint64_t ns(void) { return timer_ns_gettime64(); }
static inline uint64_t cyc(void) { return perf_cntr_count(PRFC1); }
static unsigned x100(uint64_t num, uint64_t den) { return den ? (unsigned)(num * 100u / den) : 0u; }
#define F2(v) (v) / 100u, (v) % 100u       /* a value x 100 for "%u.%02u" */

static const float sun_az[2] = { TOWN_RAY_SUN14_AZ, TOWN_RAY_SUN16_AZ }, sun_el[2] = { TOWN_RAY_SUN14_EL, TOWN_RAY_SUN16_EL };
static const char *const sun_name[2] = { "14:00", "16:00" };

/* Cycles as ns at F = 200.006 MHz (MEASURED s2). */
static int64_t cyc_ns(uint64_t c) { return (int64_t)(c * 1000000u / 200006u); }

/* ray2. ray1 read CLOCK.2's cycles inside the say() after CLOCK.1's say() had printed (its 1,027,528 cycles in 1 ms
   held that print); here both clocks are read together, before anything is printed. CLOCK.4 looks for the step the ns
   clock is expected to take once a second: KallistiOS reloads TMU2 each 12,468,720 counts (its clock constant
   199,499,520 Hz / 16), counts each as 80 ns (997,497,600 ns) and then adds a whole second. */
static void t_clock(void) {
    uint64_t a = ns(), b, least = ~0ull, c0, c1, t0, t, pc, pn, e0, e1, n0;
    int64_t top = 0;
    unsigned i, jumps = 0, call;

    heading("CLOCK", "the two clocks: the ns timer's least step; cycles in 1 ms of it (both read together); one ns call; steps of the ns clock against the cycles over 2.2 s");
    for (i = 0; i < 20000u; i++) { b = ns(); if (b != a && b - a < least) least = b - a; a = b; }
    perf_cntr_start(PRFC1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
    c0 = cyc(); t0 = ns();
    while ((t = ns() - t0) < 1000000u) { }
    c1 = cyc() - c0;
    e0 = cyc(); for (i = 0; i < 4096u; i++) (void)ns(); e1 = cyc() - e0;
    call = x100(e1, 4096u);
    step("clock steps");
    pc = e0 = cyc(); pn = n0 = ns();
    for (i = 0; i < 8u; i++) {                       /* 8 x 55,000,000 cycles: 2.2 s, a heartbeat each 0.275 s */
        const uint64_t until = pc + 55000000u;

        while (pc < until) {
            const uint64_t c = cyc(), n = ns();
            const int64_t d = (int64_t)(n - pn) - cyc_ns(c - pc);

            if (d > top) top = d;
            jumps += d > 1000000;
            pc = c; pn = n;
        }
        sh4der_fault_beat(&crumbs);
    }
    say("RESULT CLOCK.1 ns timer least step: %u ns", (unsigned)least);
    say("RESULT CLOCK.2 cycles in %u ns: %u", (unsigned)t, (unsigned)c1);
    say("RESULT CLOCK.3 one ns call: %u.%02u cycles", F2(call));
    say("RESULT CLOCK.4 in %u us of cycles the ns clock moved %u us; it stepped over 1 ms ahead %u times, largest step %d ns",
        (unsigned)(cyc_ns(pc - e0) / 1000), (unsigned)((pn - n0) / 1000u), jumps, (int)top);
    say("  (CLOCK.4 predicted from KallistiOS's timer source: +2,502,400 ns once a second; ESTIMATE until this line)");
}

/* ---- P1: rays of known n ----------------------------------------------------------------------- */

static const unsigned want[] = { 1, 2, 4, 8, 12, 16, 24, 32, 48, 64, 96 };
static const uint8_t zeros[RAY_N][RAY_N];

/* grid: 0 the grid of zeros, 1 the town's. */
static void t_p1(const char *id, unsigned town, unsigned sun) {
    const uint8_t (*g)[RAY_N] = town ? town_ray_h : zeros;
    double sn = 0.0, sc = 0.0, snn = 0.0, snc = 0.0;
    unsigned b, c, r, fits = 0, bad = 0;
    char what[160];
    ray_sun_t s;

    snprintf(what, sizeof what, "rays of known n on the %s grid (4 KB, in the cache), sun at %s; cycles a ray at each n, then a straight-line fit",
             town ? "town" : "zero", sun_name[sun]);
    heading(id, what);
    memcpy(GRID, g, 4096u);
    ray_sun_init(&s, sun_az[sun], sun_el[sun]);
    if (town) ray_points(CAND, NCAND, 7u); else ray_points_any(CAND, NCAND, 11u);
    step("p1 count");
    for (c = 0; c < NCAND; c++) {
        unsigned n;
        const unsigned lit = ray_walk_ref(g, &s, CAND[c][0], CAND[c][1], CAND[c][2], &n);

        NSTEP[c] = (uint8_t)(lit ? n : 0u);
    }
    say("  (second line of a row: the ns less the cycles as ns, for the row's 16 passes; the two gaps between an ns read and the cycle read beside it; 1 if the ns clock's second changed in the row; interrupts counted in a second run of the row)");
    for (b = 0; b < sizeof want / sizeof want[0]; b++) {
        unsigned cnt = 0, cy, nsr, irq;
        uint64_t a0, a1, b0, b1, c0, c1, dc, dt;

        step("p1 bucket");
        for (c = 0; c < NCAND && cnt < 256u; c++)
            if (NSTEP[c] == want[b]) { memcpy(RAYS[cnt], CAND[c], 16u); cnt++; }
        if (cnt < 32u) { say("RESULT %s n %u: %u rays among %u, not timed", id, want[b], cnt, NCAND); continue; }
        ray_slice(GRID, &s, (const float (*)[4])RAYS, LIT, cnt);
        a0 = ns(); c0 = cyc(); a1 = ns();
        for (r = 0; r < 16u; r++) ray_slice(GRID, &s, (const float (*)[4])RAYS, LIT, cnt);
        b0 = ns(); c1 = cyc(); b1 = ns();
        dc = c1 - c0; dt = b0 - a1;
        ray_slice_ref(g, &s, (const float (*)[4])RAYS, LITREF, NULL, cnt);
        bad += memcmp(LIT, LITREF, cnt) != 0;
        /* the same passes again with counter 1 counting interrupts, then the counter back on cycles */
        perf_cntr_start(PRFC1, PMCR_INTERRUPT_COUNTER_MODE, PMCR_COUNT_CPU_CYCLES);
        for (r = 0; r < 16u; r++) ray_slice(GRID, &s, (const float (*)[4])RAYS, LIT, cnt);
        irq = (unsigned)cyc();
        perf_cntr_start(PRFC1, PMCR_ELAPSED_TIME_MODE, PMCR_COUNT_CPU_CYCLES);
        cy = x100(dc, 16u * cnt); nsr = x100(dt, 16u * cnt);
        say("RESULT %s n %2u (%3u rays): %u.%02u cycles a ray, %u.%02u ns", id, want[b], cnt, F2(cy), F2(nsr));
        say("  ns-cyc %d ns, gaps %u %u, sec %u, irq %u", (int)((int64_t)dt - cyc_ns(dc)), (unsigned)(a1 - a0), (unsigned)(b1 - b0),
            (unsigned)(a0 / 1000000000u != b1 / 1000000000u), irq);
        if (cnt >= 64u) { const double n = want[b], y = cy / 100.0; sn += n; sc += y; snn += n * n; snc += n * y; fits++; }
    }
    say("RESULT %s kernel against reference: %s", id, bad ? "BYTES DIFFER" : "the same bytes");
    if (fits >= 2u) {
        const double den = fits * snn - sn * sn, slope = (fits * snc - sn * sc) / den, icpt = (sc - slope * sn) / fits;
        const int s100 = (int)(slope * 100.0 + (slope < 0 ? -0.5 : 0.5)), i100 = (int)(icpt * 100.0 + (icpt < 0 ? -0.5 : 0.5));

        say("RESULT %s DERIVED fit over the %u rows with 64 rays or more:", id, fits);
        say("  C_step %d.%02u cycles, C_setup %d.%02u cycles", s100 / 100, (unsigned)(s100 < 0 ? -s100 : s100) % 100u,
            i100 / 100, (unsigned)(i100 < 0 ? -i100 : i100) % 100u);
        say("  (C_setup holds the loop's load and store too)");
        say("  thesis 5.3 ESTIMATE: C_step 12 to 16, C_setup ~30");
    }
}

/* ---- P2: the slice ---------------------------------------------------------------------------- */

static matrix_t tmat __attribute__((aligned(32))) = { { 0.8f, 0.1f, 0.0f, 0.0f }, { -0.1f, 0.8f, 0.2f, 0.0f }, { 0.0f, -0.2f, 0.9f, 0.01f }, { 3.0f, 4.0f, 5.0f, 1.0f } };
#define TMAT_SHZ ((const shz_mat4x4_t *)(const void *)&tmat)

static unsigned slice_warm16[2];                    /* P2's warm batched slice at each sun, cycles x 100 a ray (for P9) */
static uint64_t slice_warm16_total[2];

static void vertices_fill(void) {
    pvr_vertex_t *v = (pvr_vertex_t *)(void *)VIN;
    unsigned i;

    for (i = 0; i < NV; i++) {
        v[i].flags = PVR_CMD_VERTEX; v[i].x = (float)(i & 63u); v[i].y = (float)(i & 31u); v[i].z = 1.0f + (float)(i & 15u);
        v[i].u = 0.25f; v[i].v = 0.75f; v[i].argb = 0xff808080u + i; v[i].oargb = i;
    }
}

static void purge_all(const uint8_t *grid) {
    dcache_purge_range((uintptr_t)grid, 4096u);
    dcache_purge_range((uintptr_t)RAYS, PK * 16u);
    dcache_purge_range((uintptr_t)LIT, PK);
    dcache_purge_range((uintptr_t)VIN, NV * 32u);
    dcache_purge_range((uintptr_t)VOUT, NV * 32u);
}

static uint64_t run_interleaved(const uint8_t *grid, const ray_sun_t *s) {
    uint64_t c0 = cyc();
    unsigned r;

    for (r = 0; r < PK; r += 8u) {
        const unsigned v0 = r * NV / PK, v1 = (r + 8u) * NV / PK;

        ray_slice(grid, s, (const float (*)[4])(RAYS + r), LIT + r, 8u);
        shz_dce_pvr_project(VOUT + v0 * 32u, VIN + v0 * 32u, v1 - v0);
    }
    return cyc() - c0;
}

static uint64_t least(uint64_t a, uint64_t b) { return a < b ? a : b; }

/* ocram: 0 the grid in the pool; 1 the grid in OC-RAM (group 2). */
static void t_p2(const char *id, unsigned sun, unsigned ocram) {
    const uint8_t *grid = ocram ? OCRAM : GRID;
    uint64_t cold = ~0ull, warm = ~0ull, alone = ~0ull, inter = ~0ull, c0, dc;
    unsigned r, bad = 0;
    int64_t extra;
    char what[200];
    ray_sun_t s;

    snprintf(what, sizeof what, "a slice of %u stand-in receivers on the town grid%s, sun at %s: batched cold, batched warm, %u PVR vertices alone, the two interleaved",
             PK, ocram ? " in OC-RAM" : "", sun_name[sun], NV);
    heading(id, what);
    memcpy((void *)grid, town_ray_h, 4096u);
    ray_sun_init(&s, sun_az[sun], sun_el[sun]);
    ray_points(RAYS, PK, 7u);
    ray_slice_ref(town_ray_h, &s, (const float (*)[4])RAYS, LITREF, NULL, PK);
    shz_xmtrx_load_4x4(TMAT_SHZ);
    step("p2");
    for (r = 0; r < 5u; r++) {
        purge_all(GRID);                            /* OC-RAM is not in the cache: only the RAM side is purged */
        c0 = cyc(); ray_slice(grid, &s, (const float (*)[4])RAYS, LIT, PK); cold = least(cold, cyc() - c0);
        bad += memcmp(LIT, LITREF, PK) != 0;
    }
    for (r = 0; r < 5u; r++) {
        c0 = cyc(); ray_slice(grid, &s, (const float (*)[4])RAYS, LIT, PK); warm = least(warm, cyc() - c0);
    }
    for (r = 0; r < 3u; r++) {
        purge_all(GRID);
        c0 = cyc(); shz_dce_pvr_project(VOUT, VIN, NV); alone = least(alone, cyc() - c0);
    }
    for (r = 0; r < 3u; r++) {
        purge_all(GRID);
        memset(LIT, 0, PK);
        dc = run_interleaved(grid, &s); inter = least(inter, dc);
        bad += memcmp(LIT, LITREF, PK) != 0;
    }
    say("RESULT %s.1 batched, grid cold: %u.%02u cycles a ray, slice %u us", id, F2(x100(cold, PK)), (unsigned)(cold / 200u));
    say("RESULT %s.2 batched, grid warm: %u.%02u cycles a ray, slice %u us", id, F2(x100(warm, PK)), (unsigned)(warm / 200u));
    say("RESULT %s.3 vertices alone: %u.%02u cycles a vertex, %u us", id, F2(x100(alone, NV)), (unsigned)(alone / 200u));
    say("RESULT %s.4 interleaved: %u us", id, (unsigned)(inter / 200u));
    extra = (int64_t)inter - (int64_t)alone - (int64_t)warm;
    say("RESULT %s.5 DERIVED .4 - .3 - .2: %d us, %d cycles a ray", id, (int)(extra / 200), (int)(extra / (int64_t)PK));
    say("RESULT %s.6 kernel against reference: %s", id, bad ? "BYTES DIFFER" : "the same bytes");
    say("  (least of 5 runs, 3 for .3 and .4; us = cycles / 200 at F = 200.006 MHz, MEASURED s2)");
    say("  (.1 cold: grid, points, bytes and vertices purged first; P2 predicts .1 - .2 <= 18 us)");
    if (!ocram) { slice_warm16[sun] = x100(warm, PK); slice_warm16_total[sun] = warm; }
}

/* ---- P9: the proxy pass at 16:00 ----------------------------------------------------------------- */

static void t_p9(void) {
    static ray_proxy_t prox[TOWN_RAY_MOVERS];
    static unsigned first[TOWN_RAY_MOVERS + 1u];
    uint64_t acc = 0, accref = 0, c0;
    unsigned m, n = 0, r, bad = 0, dark = 0;
    ray_sun_t s;

    heading("P9", "the proxy pass at 16:00: 8 people (capsules) and 4 hens (spheres) at their load places, each against the 0.5 m ground points in its shadow run");
    ray_proxies(prox);
    ray_sun_init(&s, TOWN_RAY_SUN16_AZ, TOWN_RAY_SUN16_EL);
    for (m = 0; m < TOWN_RAY_MOVERS; m++) { first[m] = n; n += ray_run_points(&prox[m], &s, P9PTS + n, 4096u - n); }
    first[TOWN_RAY_MOVERS] = n;
    step("p9");
    for (r = 0; r < 65u; r++) {
        memset(LIT, 255, n);
        c0 = cyc(); ray_proxy_pass(prox, TOWN_RAY_MOVERS, first, &s, (const float (*)[4])P9PTS, LIT);
        if (r) acc += cyc() - c0;                    /* the first run warms the cache */
        memset(LITREF, 255, n);
        c0 = cyc(); ray_proxy_pass_ref(prox, TOWN_RAY_MOVERS, first, &s, (const float (*)[4])P9PTS, LITREF);
        if (r) accref += cyc() - c0;
        bad += memcmp(LIT, LITREF, n) != 0;
    }
    for (r = 0; r < n; r++) dark += LIT[r] == 0u;
    say("RESULT P9.1 receivers: %u (people %u, hens %u); %u shadowed", n, first[8], n - first[8], dark);
    say("RESULT P9.2 pass with early outs: %u.%02u cycles a pair, %u us a pass", F2(x100(acc, 64u * n)), (unsigned)(acc / 64u / 200u));
    say("RESULT P9.3 reference (no early outs): %u.%02u cycles a pair", F2(x100(accref, 64u * n)));
    say("RESULT P9.4 kernel against reference: %s", bad ? "BYTES DIFFER" : "the same bytes");
    say("  thesis 5.6 ESTIMATE: sphere 15-20 early out, 40-50 full; capsule about twice");
    if (slice_warm16_total[1]) {
        const uint64_t pass = acc / 64u, kb = 2u * 200006u * 100u / (slice_warm16[1] ? slice_warm16[1] : 1u);

        say("RESULT P9.5 DERIVED: the pass is %u.%02u %% of P2's slice of %u rays at 16:00 (P2.2)", F2(x100(pass * 100u, slice_warm16_total[1])), PK);
        say("RESULT P9.6 DERIVED: k for a 2 ms slice at P2.2's cost a ray: %u rays; the pass is %u.%02u %% of 2 ms",
            (unsigned)kb, F2(x100(pass * 100u, 2u * 200006u)));
    }
    say("  (P9 is falsified if the pass costs more than k x C_ray)");
}

/* ---- h3: the transform ---------------------------------------------------------------------------- */

/* The plain loop the batch claims to match: shz_xmtrx_transform_vec4, shz_invf_fsrra, one product each. */
static void __attribute__((noinline)) h3_c_loop(const vector_t *in, vector_t *out, unsigned n) {
    unsigned i;

    for (i = 0; i < n; i++) {
        const shz_vec4_t r = shz_xmtrx_transform_vec4(shz_vec4_init(in[i].x, in[i].y, in[i].z, 1.0f));
        const float iw = shz_invf_fsrra(r.w);

        out[i].x = r.x * iw; out[i].y = r.y * iw; out[i].z = iw;
    }
}

static void __attribute__((noinline)) h3_project(void *out, const void *in, unsigned n) { shz_dce_xmtrx_project_point3_batch(out, 16u, in, 16u, n); }
static void __attribute__((noinline)) h3_kos(void *out, const void *in, unsigned n) { mat_transform((const vector_t *)in, (vector_t *)out, (int)n, sizeof(vector_t)); }
static void h3_c(void *out, const void *in, unsigned n) { h3_c_loop((const vector_t *)in, (vector_t *)out, n); }

static uint32_t ulps(float a, float b) {
    int32_t ia, ib;
    int64_t d;

    memcpy(&ia, &a, 4u); memcpy(&ib, &b, 4u);
    if (ia < 0) ia = INT32_MIN - ia;
    if (ib < 0) ib = INT32_MIN - ib;
    d = (int64_t)ia - (int64_t)ib;
    return (uint32_t)(d < 0 ? -d : d);
}

static void t_h3(void) {
    static const char *const name[4] = { "mat_transform", "batch, prefetch", "batch, no prefetch", "SH4ZAM C loop" };
    static void (*const fn[4])(void *, const void *, unsigned) = { h3_kos, h3_project, h3_project_np, h3_c };
    vector_t *in = (vector_t *)(void *)VIN, *out = (vector_t *)(void *)(VIN + OC / 2u), *cmp = (vector_t *)(void *)(VIN + 3u * OC / 4u);
    unsigned hot[4], cold[4], k, i, r, top = 0, same12, same13;
    uint64_t c0;

    heading("h3", "the transform with 1 / w: four forms, cycles a vertex, 256 vectors in the cache and 16,384 not");
    for (i = 0; i < 16384u; i++) { in[i].x = (float)(i & 63u); in[i].y = (float)(i & 31u); in[i].z = 1.0f + (float)(i & 15u); in[i].w = 1.0f; }
    for (k = 0; k < 4u; k++) {
        step("h3");
        if (k == 0u) mat_load(&tmat); else shz_xmtrx_load_4x4(TMAT_SHZ);
        fn[k](out, in, 256u);
        c0 = cyc(); for (r = 0; r < 256u; r++) fn[k](out, in, 256u); hot[k] = x100(cyc() - c0, 65536u);
        dcache_purge_range((uintptr_t)VIN, 256u * KB); dcache_purge_range((uintptr_t)VOUT, 256u * KB);
        c0 = cyc(); fn[k](VOUT, in, 16384u); cold[k] = x100(cyc() - c0, 16384u);
    }
    for (k = 0; k < 4u; k++) say("RESULT h3.%u %s: %u.%02u in cache, %u.%02u not", k + 1u, name[k], F2(hot[k]), F2(cold[k]));
    shz_xmtrx_load_4x4(TMAT_SHZ);
    h3_c(cmp, in, 256u); h3_project(out, in, 256u); same12 = !memcmp(out, cmp, 256u * 16u);
    h3_project_np(out, in, 256u); same13 = !memcmp(out, cmp, 256u * 16u);
    mat_load(&tmat); h3_kos(out, in, 256u);
    for (i = 0; i < 256u; i++) {
        const uint32_t u[3] = { ulps(out[i].x, cmp[i].x), ulps(out[i].y, cmp[i].y), ulps(out[i].z, cmp[i].z) };

        for (k = 0; k < 3u; k++) if (u[k] > top) top = u[k];
    }
    say("RESULT h3.5 batch against C loop: %s; no-prefetch build: %s", same12 ? "the same bytes" : "BYTES DIFFER", same13 ? "the same bytes" : "BYTES DIFFER");
    say("RESULT h3.6 mat_transform against C loop (x, y, 1/w): max %u ulp", top);
    say("  (cycles a vertex; out of cache: input and output purged first)");
}

/* ---- RT (ray2): a shadow ray against a triangle ---------------------------------------------------- */

#define RT_N 96u                                                  /* 4,608 bytes of triangles, 6,144 prepared */
#define RT_P 256u
#define RT_TRI ((ray_tri_t *)(void *)(VIN + 9u * KB))             /* cache offsets 9 .. 13.5 KB; the points are at 5 .. 9 */
#define RT_SUN ((ray_tri_sun_t *)(void *)(VIN + OC + 9u * KB))    /* 9 .. 15 KB; the bytes out at 1.5 .. 2.5 KB */

static void t_rt(void) {
    static const char *const name[4] = { "any direction, early outs", "any direction, reference", "prepared for the sun, early outs", "prepared for the sun, reference" };
    uint8_t *const o[4] = { LITREF, LITREF + 512u, LITREF, LITREF + 512u };
    unsigned k, r, i, tests[4] = { 0, 0, 0, 0 }, cy[4], dark = 0, same_any = 0;
    uint64_t c0 = 0;
    ray_sun_t s;
    float d[3];

    heading("RT", "a shadow ray against a triangle, to the first hit: 256 points x 96 triangles, all in the cache, sun at 14:00; cycles a test, four forms");
    ray_sun_init(&s, TOWN_RAY_SUN14_AZ, TOWN_RAY_SUN14_EL);
    d[0] = s.sx; d[1] = s.sy; d[2] = s.sz;
    ray_tri_scene(RT_TRI, RT_N, RAYS, RT_P, d, 1.5f, 5u);
    for (i = 0; i < RT_N; i++) ray_tri_sun_init(&RT_SUN[i], &RT_TRI[i], d);
    for (k = 0; k < 4u; k++) {
        step("rt");
        for (r = 0; r < 17u; r++) {                  /* the first pass warms the cache and is not timed */
            if (r == 1u) c0 = cyc();
            tests[k] = k == 0u ? ray_tri_pass(RT_TRI, RT_N, (const float (*)[4])RAYS, RT_P, d, o[k])
                     : k == 1u ? ray_tri_pass_ref(RT_TRI, RT_N, (const float (*)[4])RAYS, RT_P, d, o[k])
                     : k == 2u ? ray_tri_sun_pass(RT_SUN, RT_N, (const float (*)[4])RAYS, RT_P, o[k])
                     : ray_tri_sun_pass_ref(RT_SUN, RT_N, (const float (*)[4])RAYS, RT_P, o[k]);
        }
        cy[k] = x100(cyc() - c0, 16u * (uint64_t)(tests[k] ? tests[k] : 1u));
        if (k == 1u) same_any = !memcmp(o[0], o[1], RT_P);   /* forms 3 and 4 write the same two arrays next */
    }
    for (r = 0; r < RT_P; r++) dark += o[2][r] == 0u;
    for (k = 0; k < 4u; k++) say("RESULT RT.%u %s: %u.%02u cycles a test (%u tests a pass)", k + 1u, name[k], F2(cy[k]), tests[k]);
    say("RESULT RT.5 any direction, kernel against reference: %s", same_any ? "the same bytes" : "BYTES DIFFER");
    say("RESULT RT.6 prepared, kernel against reference: %s; %u of %u rays shadowed", memcmp(o[2], o[3], RT_P) ? "BYTES DIFFER" : "the same bytes", dark, RT_P);
    say("  (a test includes the loop over triangles; the town notes' TOWN-063 assumes 50 cycles a test: ESTIMATE)");
    say("  (host test: 472,197 tests for 4,096 points x 128 triangles; this page's scene is smaller)");
}

/* ---- the groups --------------------------------------------------------------------------------- */

static void group1(void) {
    unsigned sun;

    say("RAY BENCH ray2: THESIS_SH4_SHADOWS.md P1, P2, P9; h3; RT. One run, this console.");
    say("Each test has its own page. A/B turn the pages.");
    t_clock();
    t_p1("P1.1", 0u, 0u);
    t_p1("P1.2a", 0u, 1u);
    t_p1("P1.2b", 0u, 1u);
    t_p1("P1.2c", 0u, 1u);
    t_p1("P1.3", 1u, 0u);
    t_p1("P1.4a", 1u, 1u);
    t_p1("P1.4b", 1u, 1u);
    t_p1("P1.4c", 1u, 1u);
    vertices_fill();
    for (sun = 0; sun < 2u; sun++) t_p2(sun ? "P2.B" : "P2.A", sun, 0u);
    t_p9();
    t_h3();
    t_rt();
    page_break();
    say("GROUP 1 END. START runs group 2: OC-RAM, left on.");
    step("group 1 end");
}

static void group2(void) {
    heading("OCRAM", "OC-RAM switched on (stays on until the power goes off); the cache for RAM is then 8 KB");
    say("CCR before %08x", (unsigned)CCR);
    dcache_toggle_ocram(true);
    say("CCR now %08x", (unsigned)CCR);
    vertices_fill();
    t_p2("P2.C", 0u, 1u);
    t_p2("P2.D", 1u, 1u);
    page_break();
    say("GROUP 2 END.");
    step("group 2 end");
}

int main(void) {
    static const char *const next[3] = { "START: OC-RAM", "(end)", "(end)" };
    unsigned page = 0, pages, held = ~0u, done = 1;

    pvr_init_defaults();
    if (sh4der_fault_init(&crumbs) < 0) say("fault catcher: a hook was refused");
    group1();
    pvr_shutdown();
    vid_set_mode(DM_640x480, PM_RGB565);
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = st ? st->buttons : 0u, hit = now & ~held;

        sh4der_fault_beat(&crumbs);
        pages = (nlines + ROWS - 1u) / ROWS;
        if (!pages) pages = 1u;
        if (held == ~0u) page_draw(page, pages, next[done - 1u]);
        else if (hit & CONT_A) { page = (page + 1u) % pages; page_draw(page, pages, next[done - 1u]); }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; page_draw(page, pages, next[done - 1u]); }
        else if ((hit & CONT_START) && done < 2u) {
            const unsigned first = nlines;

            group2();
            done++;
            pages = (nlines + ROWS - 1u) / ROWS;
            page = first / ROWS;
            page_draw(page, pages, next[done - 1u]);
        }
        held = now;
        thd_sleep(20);
    }
    return 0;
}
