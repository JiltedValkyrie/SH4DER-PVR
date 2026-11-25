/* Dreamcast entry: pad -> demo input, frame timing, the numbers the overlay shows.
 * KOS path: maple controller status (maple_enum_type + maple_dev_status), timer_us_gettime64, pvr_get_stats,
 * pvr_mem_available, mallinfo + sbrk for free main memory. No per-frame printf (serial is 115200 baud). */
#include <kos.h>
#include <malloc.h>
#include <unistd.h>
#include <arch/arch.h>
#include <arch/timer.h>
#include <dc/maple.h>
#include <dc/maple/controller.h>
#include <dc/pvr.h>
#include "demo.h"
#include "controls.h"
#include "gfx.h"
#include "gfx_pvr.h"
#include "version.h"
#ifdef PL4NAR_AUTORUN
#define PSM_FAULT_IMPL
#include "psm_fault.h"
#include "psm_suite.h"
#define PG_PREFIX "pl4m"
#define PG_LINES_MAX 160u
#ifdef PL4NAR_MAP
#define PG_TOP 2u                                    /* the effect labels' three lines under the title (pl4nar_fx.h) */
#endif
#include "psm_page.h"
#ifdef PL4NAR_MAP
#include "psm_effects.h"
#include "pl4nar_fx.h"
#endif
#endif

KOS_INIT_FLAGS(INIT_DEFAULT);

uint64_t plat_us(void) { return timer_us_gettime64(); }

static uint32_t ram_free_kb(void) {
    const struct mallinfo mi = mallinfo();
    const uintptr_t heap_end = (uintptr_t)sbrk(0), heap_top = (uintptr_t)_arch_mem_top - THD_KERNEL_STACK_SIZE;
    return (uint32_t)(((uint32_t)mi.fordblks + (heap_top > heap_end ? heap_top - heap_end : 0u)) / 1024u);
}

/* The pad, the live view's keys (controls.h, the one table); the autorun reads the same keys (its driver keeps the
   gas, the brake and the wheel). */
static void read_pad(demo_input_t *in, uint32_t *held) {
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
    uint32_t pad = 0u;
    memset(in, 0, sizeof *in);
    if (!st) { *held = 0u; return; }
    in->steer = (float)st->joyx * (1.0f / 127.0f);
    if (in->steer > -0.12f && in->steer < 0.12f) in->steer = 0.0f;
    if (st->buttons & CONT_DPAD_LEFT)  in->steer = -1.0f;
    if (st->buttons & CONT_DPAD_RIGHT) in->steer = 1.0f;
    in->gas = (float)st->rtrig * (1.0f / 255.0f);            /* right trigger: engine */
    in->brake = (float)st->ltrig * (1.0f / 255.0f);          /* left trigger: brake, then reverse */
    if (st->buttons & CONT_B) {                              /* B held: the stick turns the camera round the car, the triggers move the divider */
        in->orbit = (float)st->joyx * (1.0f / 127.0f);       /* the stick only: B + D-pad left is the AO key */
        if (in->orbit > -0.12f && in->orbit < 0.12f) in->orbit = 0.0f;
        in->steer = 0.0f;
        in->split_l = st->ltrig > 128; in->split_r = st->rtrig > 128; in->gas = in->brake = 0.0f;
    }
    if (st->buttons & CONT_A) pad |= CTL_A;
    if (st->buttons & CONT_B) pad |= CTL_B;
    if (st->buttons & CONT_X) pad |= CTL_X;
    if (st->buttons & CONT_Y) pad |= CTL_Y;
    if (st->buttons & CONT_START) pad |= CTL_START;
    if (st->buttons & CONT_DPAD_UP) pad |= CTL_UP;
    if (st->buttons & CONT_DPAD_DOWN) pad |= CTL_DOWN;
    if (st->buttons & CONT_DPAD_LEFT) pad |= CTL_LEFT;
    if (st->buttons & CONT_DPAD_RIGHT) pad |= CTL_RIGHT;
    {
        const uint32_t now = ctl_map(pad, ctl_when(demo_info_on()));
        in->pressed = now & ~*held;
        *held = now;
    }
}

#ifdef PL4NAR_AUTORUN
/* pl4m, the measuring autorun: the demo as it is, driven by itself through fixed stages, then result pages (psm_page.h,
   CRC line a page, back to the suite menu when there is one). Each stage settles AUTO_SETTLE_S (a size change, the
   hour's 1.6 s ease) and is then counted for AUTO_COUNT_S. One frame in AUTO_SAMPLE waits for both scenes and reads
   each one's TA bytes and PVR render; frame times are taken over the other frames only, so the waits are not in them.
   The car drives itself at about AUTO_SPEED m/s in its lane, and each stage starts it again from the same place
   (AUTO_Z, AUTO_X), so every stage sees the same stretch: on the new map (PL4NAR_MAP) the east side from s = 60 m,
   under the overpass (s 113 .. 127) and the first gantry (s 200). The frame before a sampled one waits for its own
   render to end (gfx_pvr_sample 2), so the sampled frame's render-done is its own; neither is in the frame times.
   Every wait for the PVR is bounded (200 ms, gfx_pvr_timeouts). */
#ifndef AUTO_SETTLE_S
#define AUTO_SETTLE_S 3u
#endif
#ifndef AUTO_COUNT_S
#define AUTO_COUNT_S 8u
#endif
#define AUTO_SAMPLE 8u
#define AUTO_SPEED 14.0f
#ifdef PL4NAR_HILLS
#define AUTO_Z 880.0f                              /* the hills: the south side's steepest stretch (s 926), then over the crest past the seam (s 45) */
#else
#define AUTO_Z 60.0f
#endif
#define AUTO_X -2.0f
#ifndef PL4M_TAG
#define PL4M_TAG untagged
#endif
#define STR_(x) #x
#define STR(x) STR_(x)

typedef struct stage { const char *name; int reflect; unsigned size, tod; int orbit, blobs, noao, split; float lod; unsigned sun, xs; } stage_t;   /* xs (the hills): XS_* */
#define XS_SMALL 1u                                /* the eight small tiles (the c8 set), not four of 128 */
#define XS_CUT   2u                                /* the mirror pass through its own tiles (tile_cut.h) */
#define XS_INFO  4u                                /* the overlay on */
#define XS_STOP  8u                                /* the car stands: traffic crosses the tiles' fade band */
#define AUTO_V(k) ((stages[k].xs & XS_STOP) ? 0.0f : AUTO_SPEED)   /* blobs: no shadow tiles (the hills: 2 the flat quad); noao: no baked occlusion; split: the layer on the left half; lod: buildings boxed past this (m), 0 the default; sun: 1 the sweep (the hills) */
#if defined(PL4NAR_HILLS)
static const stage_t stages[] = {                  /* size 0 256x128, 1 128x64, 2 512x256; tod 1 day, 2 night; blobs 0 mesh, 1 off, 2 flat quad */
    { "128x64 day mesh",  1, 1, 1, 0, 0, 0, 0, 0.0f, 0, 0 }, { "128x64 day flat",  1, 1, 1, 0, 2, 0, 0, 0.0f, 0, 0 }, { "128x64 noshad", 1, 1, 1, 0, 1, 0, 0, 0.0f, 0, 0 },
    { "128x64 swp mesh", 1, 1, 1, 0, 0, 0, 0, 0.0f, 1, 0 }, { "128x64 swp flat", 1, 1, 1, 0, 2, 0, 0, 0.0f, 1, 0 }, { "128x64 no AO", 1, 1, 1, 0, 0, 1, 0, 0.0f, 0, 0 },
    { "off day",          0, 1, 1, 0, 0, 0, 0, 0.0f, 0, 0 }, { "256x128 day mesh", 1, 0, 1, 0, 0, 0, 0, 0.0f, 0, 0 }, { "512x256 day mesh", 1, 2, 1, 0, 0, 0, 0, 0.0f, 0, 0 },
    { "128x64 night",     1, 1, 2, 0, 0, 0, 0, 0.0f, 0, 0 }, { "off night",        0, 1, 2, 0, 0, 0, 0, 0.0f, 0, 0 }, { "128x64 orbit", 1, 1, 1, 1, 0, 0, 0, 0.0f, 0, 0 },
    { "t8 small uncut",   1, 1, 1, 0, 0, 0, 0, 0.0f, 0, XS_SMALL }, { "t8 small cut", 1, 1, 1, 0, 0, 0, 0, 0.0f, 0, XS_SMALL | XS_CUT },
    { "t4 128 cut",       1, 1, 1, 0, 0, 0, 0, 0.0f, 0, XS_CUT },   { "t4 overlay on", 1, 1, 1, 0, 0, 0, 0, 0.0f, 0, XS_INFO },
    { "t4 fade band",     1, 1, 1, 0, 0, 0, 0, 0.0f, 0, XS_STOP }
};
#elif defined(PL4NAR_MAP)
static const stage_t stages[] = {                  /* size 0 256x128, 1 128x64, 2 512x256; tod 1 day, 2 night */
    { "on 256x128 day",   1, 0, 1, 0, 0, 0, 0, 0.0f }, { "on 256 day blobs", 1, 0, 1, 0, 1, 0, 0, 0.0f }, { "on 256 day no AO", 1, 0, 1, 0, 0, 1, 0, 0.0f },
    { "on 256 day split", 1, 0, 1, 0, 0, 0, 1, 0.0f }, { "off day",          0, 0, 1, 0, 0, 0, 0, 0.0f }, { "on 256x128 night", 1, 0, 2, 0, 0, 0, 0, 0.0f },
    { "off night",        0, 0, 2, 0, 0, 0, 0, 0.0f }, { "on 128x64 night",  1, 1, 2, 0, 0, 0, 0, 0.0f }, { "on 512x256 night", 1, 2, 2, 0, 0, 0, 0, 0.0f },
    { "on 256 day orbit", 1, 0, 1, 1, 0, 0, 0, 0.0f }, { "on 256 nite orbit", 1, 0, 2, 1, 0, 0, 0, 0.0f },
    { "on 256 day lod30", 1, 0, 1, 0, 0, 0, 0, 30.0f }, { "off day lod30",    0, 0, 1, 0, 0, 0, 0, 30.0f }
};
#else
static const stage_t stages[] = {                  /* size 0 256x128, 1 128x64, 2 512x256; tod 1 day, 2 night */
    { "on 256x128 day",   1, 0, 1, 0, 0, 0, 0, 0.0f }, { "on 128x64 day",    1, 1, 1, 0, 0, 0, 0, 0.0f }, { "on 512x256 day",   1, 2, 1, 0, 0, 0, 0, 0.0f },
    { "off day",          0, 0, 1, 0, 0, 0, 0, 0.0f }, { "on 256x128 night", 1, 0, 2, 0, 0, 0, 0, 0.0f }, { "on 128x64 night",  1, 1, 2, 0, 0, 0, 0, 0.0f },
    { "on 512x256 night", 1, 2, 2, 0, 0, 0, 0, 0.0f }, { "off night",        0, 0, 2, 0, 0, 0, 0, 0.0f }, { "on 256 day orbit", 1, 0, 1, 1, 0, 0, 0, 0.0f },
    { "on 256 nite orbit", 1, 0, 2, 1, 0, 0, 0, 0.0f }
};
#endif
#define STAGES (sizeof stages / sizeof stages[0])

typedef struct acc {
    uint32_t n, dt_sum, dt_max, cpu_n, cpu_sum, lookup_sum, tri_main_sum, tri_refl_sum;   /* the unsampled frames */
    uint32_t s, ta_scr_sum, ta_scr_max, ta_mir_sum, ta_mir_max, rnd_scr_sum, rnd_mir_sum;   /* the sampled frames */
    uint32_t vram_kb, ram_kb, target_bytes, resize_fail, bad_binds, tiles_max, touched;
    uint32_t band_frames, cut_passes, cut_refused;  /* the hills: frames with a car in the fade band; cut passes, refused */
} acc_t;
static acc_t acc[STAGES];
static unsigned auto_done;
static psm_fault_crumbs_t crumbs;

static void report(void) {
    char rev[32];
    unsigned k;
    pg_plot_t *g;
    pvr_stats_t ps;
    demo_counts_t c;
    suite_rev_text(rev, sizeof rev);
    demo_counts(&c);
#ifdef PL4NAR_MAP
#ifdef PL4NAR_HILLS
    pg_say("pl4m %s: the new map with hills, each stage's cost.", STR(PL4M_TAG));
#else
    pg_say("pl4m %s: the new map (loop, overpass, strips, traffic), each stage's cost.", STR(PL4M_TAG));
#endif
#else
    pg_say("pl4m %s: the street demo as it is, each stage's cost.", STR(PL4M_TAG));
#endif
    pg_say("%s. settle %u s, count %u s a stage, 1 frame in %u sampled", rev, AUTO_SETTLE_S, AUTO_COUNT_S, AUTO_SAMPLE);
    pg_say("draw back %u / fwd %u m, RTT %s, car %u m/s, this run's numbers", (unsigned)c.draw_back, (unsigned)c.draw_fwd,
#ifdef SH4DER_PL4NAR_HAVE_RTT
           "on",
#else
           "off",
#endif
           (unsigned)AUTO_SPEED);
    if (auto_done < STAGES) pg_say("THE RUN DID NOT FINISH: the lines below are of what ran");
    pg_say("ms. fr frame (unsampled frames), SH4 CPU less PVR waits,");
    pg_say("look lookup, PVRs/PVRm render screen/mirror, TA KB a scene");
    pg_say("#  stage             fr   SH4  look  PVRs  PVRm TAs TAm");
    for (k = 0; k < STAGES; k++) {
        const acc_t *a = &acc[k];
        const float n = a->n ? (float)a->n : 1.0f, sn = a->s ? (float)a->s : 1.0f;
        pg_say("%-2u %-16s %5.2f %5.2f %5.2f %5.2f %5.2f %3u %3u", k + 1u, stages[k].name, (double)((float)a->dt_sum / n * 1e-3f),
               (double)((float)a->cpu_sum / (a->cpu_n ? (float)a->cpu_n : 1.0f) * 1e-3f), (double)((float)a->lookup_sum / n * 1e-3f), (double)((float)a->rnd_scr_sum / sn * 1e-3f),
               (double)((float)a->rnd_mir_sum / sn * 1e-3f), (unsigned)((float)a->ta_scr_sum / sn / 1024.0f + 0.5f), (unsigned)((float)a->ta_mir_sum / sn / 1024.0f + 0.5f));
    }
    pg_say("#  fr max TA peak s/m KB  tri s/m      VRAM  RAM tgt KB n/s");
    for (k = 0; k < STAGES; k++) {
        const acc_t *a = &acc[k];
        const float n = a->n ? (float)a->n : 1.0f;
        pg_say("%-2u %6.2f %4u/%-4u %5u/%-5u %4u %5u %3u %u/%u%s%s%s", k + 1u, (double)((float)a->dt_max * 1e-3f),
               (unsigned)((a->ta_scr_max + 512u) / 1024u), (unsigned)((a->ta_mir_max + 512u) / 1024u),
               (unsigned)((float)a->tri_main_sum / n + 0.5f), (unsigned)((float)a->tri_refl_sum / n + 0.5f), (unsigned)a->vram_kb, (unsigned)a->ram_kb,
               (unsigned)(a->target_bytes / 1024u), (unsigned)a->n, (unsigned)a->s, a->resize_fail ? " NOVRAM" : "", a->bad_binds ? " BADBIND" : "", a->touched ? " TOUCHED" : "");
    }
    if (pvr_get_stats(&ps) == 0) pg_say("largest scene of the run: %u KB of the 1024 KB vertex buffer", (unsigned)((ps.vtx_buffer_used_max + 512u) / 1024u));
    pg_say("headers refused (wrong list or never compiled): %u", (unsigned)gfx_pvr_bad_binds());
    pg_say("PVR waits given up (200 ms each): %u", (unsigned)gfx_pvr_timeouts());
    pg_say("each stage from s %u m, lane %d m, %u m/s", (unsigned)AUTO_Z, (int)AUTO_X, (unsigned)AUTO_SPEED);
#ifdef PL4NAR_MAP
    {   /* the car shadow tiles: by day with the reflection on; "blobs" stages, night and off keep the blob discs */
        char t[80];
        size_t o = (size_t)snprintf(t, sizeof t, "shadow tiles a stage, most seen:");
        for (k = 0; k < STAGES && o < sizeof t - 4u; k++) o += (size_t)snprintf(t + o, sizeof t - o, " %u", (unsigned)acc[k].tiles_max);
        pg_say("%s", t);
    }
#endif
    pg_say("TA bytes include headers; PVR = rnd_last_time of the scene");
    pg_say("keys act as live but steer, gas, brake (the driver's); B+stick,");
    pg_say("B+LT/RT change no work; any other key: TOUCHED. No key ends it");
#ifdef PL4NAR_MAP
#ifdef PL4NAR_HILLS
    pg_say("effect labels OFF in every stage (fill); hills on, no split");
    pg_say("shadow: mesh = laid on the ground's triangles, flat = one quad");
    pg_say("at the car's ground height (B+Y), noshad = blobs; swp: sun sweep");
    pg_say("stage 12: the camera orbits, across the crest at s 45");
    pg_say("13-17 tiles: t4 = 4 of 128 x 128 (1, the default), t8 = 8 small");
    pg_say("cut: the mirror pass through its own tiles (t4 12 x 8, t8 8 x 2)");
    pg_say("16: the overlay on (one text pass); 17: the car stands, traffic");
    pg_say("crosses the fade band: whole to %u m, gone at %u m", 40u, 50u);
    pg_say("17 us a tile is one point (PVR-156), not a rule");
    {
        gfx_cut_stats_t cs;
        gfx_tile_cut_stats(&cs);
        pg_say("tile cut: %u passes, %u refused, %u FAILED (%u words), tables %u", (unsigned)cs.passes, (unsigned)cs.refused, (unsigned)cs.failed, (unsigned)cs.bad_words, cs.tables);
        if (cs.failed) pg_say("TILE CUT FAILED: the PVR was restarted, the cut was off after");
        pg_say("#  stage            cut n / refused   fade band frames");
        for (k = 12; k < STAGES; k++) pg_say("%-2u %-16s %5u / %-5u   %u", k + 1u, stages[k].name, (unsigned)acc[k].cut_passes, (unsigned)acc[k].cut_refused, (unsigned)acc[k].band_frames);
    }
#else
    pg_say("effect labels OFF in every stage (fill); stage 4: the split at the middle");
    pg_say("buildings boxed past %u m (traffic %u), stages 12-13 past 30 m", 50u, 40u);
#endif
#endif
    g = pg_plot("pl4m: frame and PVR a stage, ms", "stage", "ms", 1);
    for (k = 0; k < STAGES; k++) { char c[4]; snprintf(c, sizeof c, "%u", k + 1u); pg_cat(g, c); }
    pg_series(g, "frame"); for (k = 0; k < STAGES; k++) pg_point(g, (float)k, acc[k].n ? (float)acc[k].dt_sum / (float)acc[k].n * 1e-3f : 0.0f);
    pg_series(g, "PVR scr"); for (k = 0; k < STAGES; k++) pg_point(g, (float)k, acc[k].s ? (float)acc[k].rnd_scr_sum / (float)acc[k].s * 1e-3f : 0.0f);
    pg_series(g, "PVR mir"); for (k = 0; k < STAGES; k++) pg_point(g, (float)k, acc[k].s ? (float)acc[k].rnd_mir_sum / (float)acc[k].s * 1e-3f : 0.0f);
}
#endif

int main(int argc, char **argv) {
    demo_stats_t st;
    demo_input_t in;
    uint32_t held = 0u, frame = 0u;
    uint64_t t_prev;
#ifdef PL4NAR_AUTORUN
    unsigned stage = 0u, page = 0u, pages;
    uint64_t t_stage;
    int sampled_prev = 0, redraw = 1;
    suite_init();
#endif
    (void)argc; (void)argv;
    printf("pl4nar reflection demo v%s b%d\n", DEMO_VERSION, DEMO_BUILD);
    gfx_init();
    demo_init();
    memset(&st, 0, sizeof st);
    printf("video memory free after load: %lu KB, main memory free: %lu KB\n", (unsigned long)(pvr_mem_available() / 1024u), (unsigned long)ram_free_kb());
#ifdef PL4NAR_AUTORUN
    if (psm_fault_init(&crumbs) < 0) pg_say("fault catcher: a hook was refused");
    acc[0].resize_fail = demo_auto_set(stages[0].reflect, stages[0].size, stages[0].tod, stages[0].orbit) != 0;
#ifdef PL4NAR_MAP
#ifdef PL4NAR_HILLS
    demo_auto_tiles(stages[0].blobs == 2 ? 2 : !stages[0].blobs);
    demo_auto_sun(stages[0].sun);
    demo_auto_tile_set(!!(stages[0].xs & XS_SMALL)); demo_auto_cut(!!(stages[0].xs & XS_CUT)); demo_auto_info(!!(stages[0].xs & XS_INFO));
#else
    demo_auto_tiles(!stages[0].blobs);
#endif
    demo_auto_ao(!stages[0].noao);
    demo_auto_split(stages[0].split);
    demo_auto_lod(stages[0].lod);
#endif
#ifdef PL4NAR_HILLS
    demo_auto_place(AUTO_Z, AUTO_X, AUTO_V(0));
#else
    demo_auto_place(AUTO_Z, AUTO_X, AUTO_SPEED);
#endif
    psm_fault_stage(&crumbs, stages[0].name);
    t_stage = timer_us_gettime64();
#endif
    t_prev = timer_us_gettime64();
    for (;;) {
        const uint64_t t0 = timer_us_gettime64();
        float dt = (float)(uint32_t)(t0 - t_prev) * (1.0f / 1000000.0f);
        uint64_t t1, waited;
        pvr_stats_t ps;
#ifdef PL4NAR_AUTORUN
        const uint32_t dt_us = (uint32_t)(t0 - t_prev);
        const int counting = t0 - t_stage >= (uint64_t)AUTO_SETTLE_S * 1000000u, sampled = (frame % AUTO_SAMPLE) == 0u;
        const int drain = (frame % AUTO_SAMPLE) == AUTO_SAMPLE - 1u;
        acc_t *a = &acc[stage];
        if (counting && frame && !sampled_prev) {      /* the interval t_prev .. t0 held the last frame: unsampled, so no waits in it */
            a->n++; a->dt_sum += dt_us; if (dt_us > a->dt_max) a->dt_max = dt_us;
        }
#endif
        t_prev = t0;
        if (dt > 0.05f) dt = 0.05f;
#ifdef PL4NAR_AUTORUN
        read_pad(&in, &held);                        /* the live view's keys; the camera and the divider change no work */
        if (in.pressed) a->touched = 1u;             /* any other key does: the stage is marked TOUCHED */
        in.steer = 0.0f; in.brake = 0.0f;
#ifdef PL4NAR_HILLS
        in.gas = demo_speed() < AUTO_V(stage) ? 1.0f : 0.0f;
        if (stages[stage].xs & XS_STOP) in.brake = 1.0f;
#else
        in.gas = demo_speed() < AUTO_SPEED ? 1.0f : 0.0f;
#endif
        gfx_pvr_sample(sampled ? 1 : drain ? 2 : 0);
        psm_fault_beat(&crumbs);
        crumbs.frame++;
#else
        read_pad(&in, &held);
        gfx_pvr_sample((frame & 31u) == 0u ? 1 : (frame & 31u) == 31u ? 2 : 0);   /* one frame in 32: the reflection pass's PVR time */
#endif
        demo_update(&in, dt);
        (void)gfx_pvr_wait_us();
        demo_draw(&st);
        t1 = timer_us_gettime64();
        waited = gfx_pvr_wait_us();
        st.cpu_us = (uint32_t)(t1 - t0 > waited ? t1 - t0 - waited : 0u);
        if (pvr_get_stats(&ps) == 0) {
            st.pvr_us = (uint32_t)(ps.rnd_last_time / 1000u);
            st.fps_x10 = (uint32_t)(ps.frame_rate * 10.0f);
        }
        st.rtt_us = demo_reflect_on() ? gfx_pvr_rtt_us() : 0u;
        if ((frame++ & 15u) == 0u) {                 /* the two memory numbers: 4 times a second is plenty */
            st.ram_free_kb = ram_free_kb();
            st.vram_free_kb = (uint32_t)(pvr_mem_available() / 1024u);
        }
#ifdef PL4NAR_AUTORUN
        if (counting) {
            demo_counts_t c;
            demo_counts(&c);
            if (c.tiles > a->tiles_max) a->tiles_max = c.tiles;
            a->band_frames += c.tiles_band != 0u;
            if (!sampled && !drain) { a->cpu_n++; a->cpu_sum += st.cpu_us; a->lookup_sum += c.lookup_us; a->tri_main_sum += c.tris_main; a->tri_refl_sum += c.tris_refl; }
            else if (sampled) {                       /* the sampled frame alone: the drain frame's passes are not its own */
                const gfx_pvr_pass_t *ps0 = gfx_pvr_pass(0u), *ps1 = gfx_pvr_pass(1u);
                a->s++;
                a->ta_scr_sum += ps0->ta_bytes; if (ps0->ta_bytes > a->ta_scr_max) a->ta_scr_max = ps0->ta_bytes;
                a->rnd_scr_sum += ps0->rnd_us;
                if (c.refl_ok) {
                    a->ta_mir_sum += ps1->ta_bytes; if (ps1->ta_bytes > a->ta_mir_max) a->ta_mir_max = ps1->ta_bytes;
                    a->rnd_mir_sum += ps1->rnd_us;
                }
            }
            a->target_bytes = c.refl_ok ? c.target_bytes : 0u;
        }
        sampled_prev = sampled || drain;
        if (t0 - t_stage >= (uint64_t)(AUTO_SETTLE_S + AUTO_COUNT_S) * 1000000u) {
            a->vram_kb = (uint32_t)(pvr_mem_available() / 1024u);
            a->ram_kb = ram_free_kb();
            a->bad_binds = gfx_pvr_bad_binds();
#ifdef PL4NAR_HILLS
            {
                static gfx_cut_stats_t prev;
                gfx_cut_stats_t cs;
                gfx_tile_cut_stats(&cs);
                a->cut_passes = cs.passes - prev.passes; a->cut_refused = cs.refused - prev.refused;
                prev = cs;
            }
#endif
            auto_done = ++stage;
            if (stage >= STAGES) break;
            acc[stage].resize_fail = demo_auto_set(stages[stage].reflect, stages[stage].size, stages[stage].tod, stages[stage].orbit) != 0;
#ifdef PL4NAR_MAP
#ifdef PL4NAR_HILLS
            demo_auto_tiles(stages[stage].blobs == 2 ? 2 : !stages[stage].blobs);
            demo_auto_sun(stages[stage].sun);
            demo_auto_tile_set(!!(stages[stage].xs & XS_SMALL)); demo_auto_cut(!!(stages[stage].xs & XS_CUT)); demo_auto_info(!!(stages[stage].xs & XS_INFO));
#else
            demo_auto_tiles(!stages[stage].blobs);
#endif
            demo_auto_ao(!stages[stage].noao);
            demo_auto_split(stages[stage].split);
            demo_auto_lod(stages[stage].lod);
#endif
#ifdef PL4NAR_HILLS
            demo_auto_place(AUTO_Z, AUTO_X, AUTO_V(stage));
#else
            demo_auto_place(AUTO_Z, AUTO_X, AUTO_SPEED);
#endif
            psm_fault_stage(&crumbs, stages[stage].name);
            t_stage = t_prev = timer_us_gettime64();
            sampled_prev = 1;                        /* the resize's wait is not a frame */
        }
#endif
    }
#ifdef PL4NAR_AUTORUN
    psm_fault_stage(&crumbs, "pages");
    gfx_shutdown();
    vid_set_mode(DM_640x480, PM_RGB565);
    pg_init();
    report();
    held = ~0u;
    for (;;) {
        maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *cs = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
        const unsigned now = cs ? cs->buttons : 0u, hit = now & ~held;
        psm_fault_beat(&crumbs);
        held = now;
        pages = pg_pages();
        if (hit & CONT_A) { page = (page + 1u) % pages; redraw = 1; }
        else if (hit & CONT_B) { page = (page + pages - 1u) % pages; redraw = 1; }
        else if (suite_tick(now, page, pages)) { page++; redraw = 1; }
        if (redraw) {
            pg_draw(page % pages, "PL4M " STR(PL4M_TAG), suite_keys_text("A/B pages"));
#ifdef PL4NAR_MAP
            (void)fx_page(pl4nar_fx_page, PL4NAR_FX_PAGE_N);
#endif
            redraw = 0;
        }
        thd_sleep(20);
    }
#endif
    return 0;
}
