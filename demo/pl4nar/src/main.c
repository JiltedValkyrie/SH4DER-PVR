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

/* The pad, the live view's keys; the autorun reads the same keys (its driver keeps the gas, the brake and the wheel). */
static void read_pad(demo_input_t *in, uint32_t *held) {
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
    uint32_t now = 0u;
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
    if (st->buttons & CONT_X)         now |= (st->buttons & CONT_B) ? DEMO_BTN_SPLIT : DEMO_BTN_REFLECT;
    if (st->buttons & CONT_Y)         now |= (st->buttons & CONT_B) ? DEMO_BTN_TILES : DEMO_BTN_TOD;
    if (st->buttons & CONT_A)         now |= (st->buttons & CONT_B) ? DEMO_BTN_SIZE : DEMO_BTN_LOOK;
    if (st->buttons & CONT_DPAD_UP)   now |= (st->buttons & CONT_B) ? DEMO_BTN_LAMP_UP : DEMO_BTN_MORE;
    if (st->buttons & CONT_DPAD_DOWN) now |= (st->buttons & CONT_B) ? DEMO_BTN_LAMP_DN : DEMO_BTN_LESS;
    if ((st->buttons & CONT_DPAD_LEFT) && (st->buttons & CONT_B)) now |= DEMO_BTN_AO;
    if (st->buttons & CONT_START)     now |= (st->buttons & CONT_B) ? DEMO_BTN_FX : DEMO_BTN_INFO;
    in->pressed = now & ~*held;
    *held = now;
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
#define AUTO_Z 60.0f
#define AUTO_X -2.0f
#ifndef PL4M_TAG
#define PL4M_TAG untagged
#endif
#define STR_(x) #x
#define STR(x) STR_(x)

typedef struct stage { const char *name; int reflect; unsigned size, tod; int orbit, blobs, noao, split; float lod; } stage_t;   /* blobs: no shadow tiles; noao: no baked occlusion; split: the layer on the left half; lod: buildings boxed past this (m), 0 the default */
#ifdef PL4NAR_MAP
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
    pg_say("pl4m %s: the new map (loop, overpass, strips, traffic), each stage's cost.", STR(PL4M_TAG));
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
    pg_say("keys as live; B+stick, B+LT/RT free; any other key: TOUCHED");
#ifdef PL4NAR_MAP
    pg_say("effect labels OFF in every stage (fill); stage 4: the split at the middle");
    pg_say("buildings boxed past %u m (traffic %u), stages 12-13 past 30 m", 50u, 40u);
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
    demo_auto_tiles(!stages[0].blobs);
    demo_auto_ao(!stages[0].noao);
    demo_auto_split(stages[0].split);
    demo_auto_lod(stages[0].lod);
#endif
    demo_auto_place(AUTO_Z, AUTO_X, AUTO_SPEED);
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
        in.gas = demo_speed() < AUTO_SPEED ? 1.0f : 0.0f;
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
            auto_done = ++stage;
            if (stage >= STAGES) break;
            acc[stage].resize_fail = demo_auto_set(stages[stage].reflect, stages[stage].size, stages[stage].tod, stages[stage].orbit) != 0;
#ifdef PL4NAR_MAP
            demo_auto_tiles(!stages[stage].blobs);
            demo_auto_ao(!stages[stage].noao);
            demo_auto_split(stages[stage].split);
            demo_auto_lod(stages[stage].lod);
#endif
            demo_auto_place(AUTO_Z, AUTO_X, AUTO_SPEED);
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
