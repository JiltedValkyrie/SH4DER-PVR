/* SH4DER-PVR: the town's view cost test: a fixed script of views and where each frame's time goes (demo/psm, PSM_VIEWCOST images only)

   demo/psm/psm_viewcost.h
   Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE.

   A measurement, not a method: nothing in the town's frame changes. With PSM_VIEWCOST, psm_dc.c boots the town into this
   script instead of the pad and puts a timer between every step of its frame. The script: route A, then route B, each at
   three pitches (the sky, the boot view, down at the ground), the camera still and then turning 90 degrees, and the boot
   view once more with the map inset on. Every stage starts from the boot pose; the simulation steps VC_DT a frame whatever
   the frame took, so the camera, the sun and the walkers are the same in every run. The first VC_SKIP frames of a stage
   are not counted. Start ends the run and opens the pages with what was measured (no other button: a press of A by mistake
   must not cut a run short). While it runs, the frame line on the screen says the stage and its frame (vc_status_line).

   What is kept a stage, each as n / mean / min / max, in microseconds unless said:
     the frame (end of one frame's work to the end of the next), apart for the frames KallistiOS's render times are
     sampled on (psm_dc.c waits for the PVR twice on one frame a second: those frames are longer and are not mixed in);
     the frame's steps in order: top (pad, the town's clock and walkers), p0 .. p3 (psm_dc.c's own four parts, the sampled
     map render wait taken out of p1), colour, plan (the dry count of the send), the ready waits, the HUD's block, the
     opaque list's send, the translucent list's, the scene's finish, and what is left over;
     vertices sent: all, ground, route A's layer, models, the map pass; KallistiOS's vertex buffer reading;
     on the sampled frames: the two render waits, the PVR's render time of the map pass and of the screen, the ground
     window's vertices and the model vertices marked as sent (route B compares those).

   Plain C, no KallistiOS: demo/psm/test/psm_viewcost_test.c runs the script, the statistics and the page text on the host. */

#ifndef PSM_VIEWCOST_H
#define PSM_VIEWCOST_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifndef VC_PART
#define VC_PART       0                 /* 0 the whole script; 1 all but route A's two turns over the town; 2 those two alone */
#endif
#if VC_PART == 1
#define VC_TAG        "vc-b1"
#elif VC_PART == 2
#define VC_TAG        "vc-b2"
#elif VC_PART == 3
#define VC_TAG        "vc-c1"           /* the placement's options (psm_scene.h, PSM_PLACE_*) on route B */
#elif VC_PART == 7
#define VC_TAG        "vc-f"            /* the model colours from a table (c1), and only the sent vertices' (c3) */
#elif VC_PART == 6
#define VC_TAG        "vc-e"            /* the send's dry count left out (f8), beside the placement's options */
#elif VC_PART == 5
#define VC_TAG        "vc-d"            /* the stage that froze in vc-a, ten times over: build it with VC_VBUF 1,024 KB and with 1,536 KB */
#elif VC_PART == 4
#define VC_TAG        "vc-c2"           /* and on route A's turns: the sky first, the town last, the plain placement last of all */
#elif VC_PART == 8
#define VC_TAG        "vc-g"            /* the turn plan's step 0: p0 split, the placement forced on a still camera, f0 and f7 */
#else
#define VC_TAG        "vc-b"
#endif
#ifndef VC_VBUF
#define VC_VBUF       (1536u * 1024u)   /* this test's vertex buffer, bytes a frame: with the town's 1,024 KB route A's boot view fills it */
#endif
#define VC_EVT_N      6u                /* the TA's error events: ISP, strip, PARAM, OPB, input error, input overflow */
#define VC_DT         (1.0f / 30.0f)    /* the simulation's step a frame, seconds */
#define VC_TURN       1.5707963f        /* a turning stage's yaw, radians, over its counted and skipped frames */
#define VC_SKIP       20u               /* frames at a stage's start that are not counted (the route's first map, the caches) */
#define VC_STILL      100u              /* counted frames of a still stage */
#define VC_TURNING    200u              /* and of a turning one */
#define VC_PITCH_SKY  (-1.25f)          /* radians, positive down: the screen's lower edge is 41.6 degrees above the horizon */
#define VC_PITCH_BOOT 0.12217f          /* the town's boot pitch (PSM_PITCH) */
#define VC_PITCH_DOWN 0.30f             /* the walk's limit down (PSM_PITCH_DOWN) */

/* the per-frame times, in the frame's order, then the counts */
enum {
    VC_FRAME, VC_TOP, VC_P0, VC_P1, VC_P2, VC_P3, VC_COLOUR, VC_PLAN, VC_RDY1, VC_RDY2, VC_HUD, VC_OP, VC_TR, VC_FIN, VC_OTHER,
    VC_V_ALL, VC_V_GROUND, VC_V_LAYER, VC_V_MODEL, VC_V_MAP, VC_KB,
    VC_S_FRAME, VC_S_WAIT, VC_S_RTT, VC_S_SCR, VC_S_GVN, VC_S_MVIS, VC_S_BLK,
    VC_P0_SET, VC_P0_PLACE, VC_P0_SKY, VC_REDO,     /* p0's three steps (setup, placement, sky), and 1 when the frame placed everything again */
    VC_T_GROUND, VC_T_LAYER, VC_T_MODEL,            /* strip triangles sent (run - 2 a strip): ground, route A's layer, models */
    VC_T_CAST, VC_T_DRAWN,                          /* casters: route A triangles into the target (both); B given to the SH-4's map, drawn into it */
    VC_METRICS
};

/* the timer marks psm_dc.c takes (VC_T): after the top, round the map's sampled render wait, after the parts, the colours,
   the plan, the ready wait, the HUD's block, the opaque list, the translucent list, the finish, round the screen's wait */
enum { VC_M_TOP, VC_M_RTT0, VC_M_RTT1, VC_M_PARTS, VC_M_COLOUR, VC_M_PLAN, VC_M_RDY, VC_M_HUD, VC_M_OP, VC_M_TR, VC_M_FIN, VC_M_SCR0, VC_M_SCR1, VC_MARKS };

typedef struct vc_stage {
    const char *name;
    int route;              /* 0 route A, 1 route B (PSM_ROUTE_A, PSM_ROUTE_B) */
    float pitch;
    int turn, inset;
    unsigned opt;           /* the placement's options this stage (PSM_PLACE_AREA 1, _BLOCKS 2, _SIDE 4), 8: no dry send count, 16 and 32: PSM_COLOUR_TABLE and _SEEN,
                               64: the placement forced every frame (a still camera paid as a turn), 128: PSM_PLACE_GROUND */
} vc_stage_t;

#ifndef VC_SHIP_OPT
#define VC_SHIP_OPT   0u                /* bits added to every stage's opt: 56 = 8 (no dry count) | 48 (PSM_COLOUR_TABLE | SEEN), the shipping set */
#endif
#define VC_FORCE      64u               /* vc_stage_t.opt: place_ok cleared before every frame's placement */
#define VC_GROUND     128u              /* and the ground window's view cull (PSM_PLACE_GROUND) */
#define VC_KEEP       256u              /* and the frame's keeps on a still light (PSM_KEEP_SUN, the sun stepped a clock minute) */
#define VC_BOUND      512u              /* and the world cut to the walls (PSM_WORLD_BOUND, the closed square) with its fog */
#define VC_VIS        1024u             /* and the models' seen marks kept on a still camera (PSM_PLACE_VIS, step 4) */
#define VC_CAST       2048u             /* and route B's static casters' map kept between sun steps (PSM_KEEP_CAST, with VC_BOUND) */

static const vc_stage_t vc_script[] = {
#if VC_PART == 8
    /* st still, fp still with the placement made again every frame, tn turning; f0 the plain placement, f7 all three options.
       fp against st is what the placement costs; tn against fp is what the turn adds on top (the ground positions, the caches).
       Route B first, and route A's two turns over the town last, as vc-b */
    { "B sky st f0",  1, VC_PITCH_SKY,  0, 0, 0u },  { "B sky fp f0",  1, VC_PITCH_SKY,  0, 0, VC_FORCE },  { "B sky tn f0",  1, VC_PITCH_SKY,  1, 0, 0u },
    { "B sky st f7",  1, VC_PITCH_SKY,  0, 0, 7u },  { "B sky fp f7",  1, VC_PITCH_SKY,  0, 0, VC_FORCE | 7u }, { "B sky tn f7",  1, VC_PITCH_SKY,  1, 0, 7u },
    { "B boot st f0", 1, VC_PITCH_BOOT, 0, 0, 0u },  { "B boot fp f0", 1, VC_PITCH_BOOT, 0, 0, VC_FORCE },  { "B boot tn f0", 1, VC_PITCH_BOOT, 1, 0, 0u },
    { "B boot st f7", 1, VC_PITCH_BOOT, 0, 0, 7u },  { "B boot fp f7", 1, VC_PITCH_BOOT, 0, 0, VC_FORCE | 7u }, { "B boot tn f7", 1, VC_PITCH_BOOT, 1, 0, 7u },
    { "B down st f0", 1, VC_PITCH_DOWN, 0, 0, 0u },  { "B down fp f0", 1, VC_PITCH_DOWN, 0, 0, VC_FORCE },  { "B down tn f0", 1, VC_PITCH_DOWN, 1, 0, 0u },
    { "B down st f7", 1, VC_PITCH_DOWN, 0, 0, 7u },  { "B down fp f7", 1, VC_PITCH_DOWN, 0, 0, VC_FORCE | 7u }, { "B down tn f7", 1, VC_PITCH_DOWN, 1, 0, 7u },
    { "A sky st f0",  0, VC_PITCH_SKY,  0, 0, 0u },  { "A sky fp f0",  0, VC_PITCH_SKY,  0, 0, VC_FORCE },  { "A sky tn f0",  0, VC_PITCH_SKY,  1, 0, 0u },
    { "A sky st f7",  0, VC_PITCH_SKY,  0, 0, 7u },  { "A sky fp f7",  0, VC_PITCH_SKY,  0, 0, VC_FORCE | 7u }, { "A sky tn f7",  0, VC_PITCH_SKY,  1, 0, 7u },
    { "A boot st f0", 0, VC_PITCH_BOOT, 0, 0, 0u },  { "A boot fp f0", 0, VC_PITCH_BOOT, 0, 0, VC_FORCE },
    { "A boot st f7", 0, VC_PITCH_BOOT, 0, 0, 7u },  { "A boot fp f7", 0, VC_PITCH_BOOT, 0, 0, VC_FORCE | 7u },
    { "A down st f0", 0, VC_PITCH_DOWN, 0, 0, 0u },  { "A down fp f0", 0, VC_PITCH_DOWN, 0, 0, VC_FORCE },
    { "A down st f7", 0, VC_PITCH_DOWN, 0, 0, 7u },  { "A down fp f7", 0, VC_PITCH_DOWN, 0, 0, VC_FORCE | 7u },
    { "A boot tn f0", 0, VC_PITCH_BOOT, 1, 0, 0u },  { "A boot tn f7", 0, VC_PITCH_BOOT, 1, 0, 7u },
    { "A down tn f0", 0, VC_PITCH_DOWN, 1, 0, 0u },  { "A down tn f7", 0, VC_PITCH_DOWN, 1, 0, 7u },
#elif VC_PART == 3
    /* f0 the plain placement, f1 area from the stored 1 / w, f3 and by block, f7 and the side cull */
    { "B sky st f0",  1, VC_PITCH_SKY,  0, 0, 0u },
    { "B sky tn f0",  1, VC_PITCH_SKY,  1, 0, 0u },
    { "B sky tn f1",  1, VC_PITCH_SKY,  1, 0, 1u },
    { "B sky tn f3",  1, VC_PITCH_SKY,  1, 0, 3u },
    { "B sky tn f7",  1, VC_PITCH_SKY,  1, 0, 7u },
    { "B sky st f7",  1, VC_PITCH_SKY,  0, 0, 7u },
    { "B boot st f0", 1, VC_PITCH_BOOT, 0, 0, 0u },
    { "B boot tn f0", 1, VC_PITCH_BOOT, 1, 0, 0u },
    { "B boot tn f1", 1, VC_PITCH_BOOT, 1, 0, 1u },
    { "B boot tn f3", 1, VC_PITCH_BOOT, 1, 0, 3u },
    { "B boot tn f7", 1, VC_PITCH_BOOT, 1, 0, 7u },
    { "B boot st f7", 1, VC_PITCH_BOOT, 0, 0, 7u },
#elif VC_PART == 7
    { "B boot st c0", 1, VC_PITCH_BOOT, 0, 0, 0u },  { "B boot st c1", 1, VC_PITCH_BOOT, 0, 0, 16u }, { "B boot st c3", 1, VC_PITCH_BOOT, 0, 0, 48u },
    { "A sky st c0",  0, VC_PITCH_SKY,  0, 0, 0u },  { "A sky st c1",  0, VC_PITCH_SKY,  0, 0, 16u }, { "A sky st c3",  0, VC_PITCH_SKY,  0, 0, 48u },
    { "A boot st c0", 0, VC_PITCH_BOOT, 0, 0, 0u },  { "A boot st c1", 0, VC_PITCH_BOOT, 0, 0, 16u }, { "A boot st c3", 0, VC_PITCH_BOOT, 0, 0, 48u },
    { "A boot tn c0", 0, VC_PITCH_BOOT, 1, 0, 0u },  { "A boot tn c3", 0, VC_PITCH_BOOT, 1, 0, 48u },
    { "A boot st f7", 0, VC_PITCH_BOOT, 0, 0, 7u },  { "A boot c3f7",  0, VC_PITCH_BOOT, 0, 0, 55u },   /* with the placement's options: fewer vertices are sent */
#elif VC_PART == 6
    { "B boot st f0", 1, VC_PITCH_BOOT, 0, 0, 0u },  { "B boot st f8", 1, VC_PITCH_BOOT, 0, 0, 8u },
    { "B boot tn f7", 1, VC_PITCH_BOOT, 1, 0, 7u },  { "B boot tn 15", 1, VC_PITCH_BOOT, 1, 0, 15u },
    { "A sky st f0",  0, VC_PITCH_SKY,  0, 0, 0u },  { "A sky st f8",  0, VC_PITCH_SKY,  0, 0, 8u },
    { "A boot st f0", 0, VC_PITCH_BOOT, 0, 0, 0u },  { "A boot st f8", 0, VC_PITCH_BOOT, 0, 0, 8u },
    { "A boot tn f7", 0, VC_PITCH_BOOT, 1, 0, 7u },  { "A boot tn 15", 0, VC_PITCH_BOOT, 1, 0, 15u },
#elif VC_PART == 5
    { "A boot tn 1",  0, VC_PITCH_BOOT, 1, 0, 0u }, { "A boot tn 2",  0, VC_PITCH_BOOT, 1, 0, 0u }, { "A boot tn 3",  0, VC_PITCH_BOOT, 1, 0, 0u },
    { "A boot tn 4",  0, VC_PITCH_BOOT, 1, 0, 0u }, { "A boot tn 5",  0, VC_PITCH_BOOT, 1, 0, 0u }, { "A boot tn 6",  0, VC_PITCH_BOOT, 1, 0, 0u },
    { "A boot tn 7",  0, VC_PITCH_BOOT, 1, 0, 0u }, { "A boot tn 8",  0, VC_PITCH_BOOT, 1, 0, 0u }, { "A boot tn 9",  0, VC_PITCH_BOOT, 1, 0, 0u },
    { "A boot tn 10", 0, VC_PITCH_BOOT, 1, 0, 0u },
#elif VC_PART == 4
    { "A sky tn f0",  0, VC_PITCH_SKY,  1, 0, 0u },
    { "A sky tn f1",  0, VC_PITCH_SKY,  1, 0, 1u },
    { "A sky tn f3",  0, VC_PITCH_SKY,  1, 0, 3u },
    { "A sky tn f7",  0, VC_PITCH_SKY,  1, 0, 7u },
    { "A boot st f7", 0, VC_PITCH_BOOT, 0, 0, 7u },
    { "A boot st f0", 0, VC_PITCH_BOOT, 0, 0, 0u },
    { "A boot tn f7", 0, VC_PITCH_BOOT, 1, 0, 7u },
    { "A boot tn f3", 0, VC_PITCH_BOOT, 1, 0, 3u },
    { "A boot tn f1", 0, VC_PITCH_BOOT, 1, 0, 1u },
    { "A boot tn f0", 0, VC_PITCH_BOOT, 1, 0, 0u },
#else
    /* route B first, and route A's two turns over the town last: vc-a froze in "A boot turn" (the run's video, 2026-11-25) */
#if VC_PART != 2
    { "B sky still",  1, VC_PITCH_SKY,  0, 0, 0u },
    { "B boot still", 1, VC_PITCH_BOOT, 0, 0, 0u },
    { "B down still", 1, VC_PITCH_DOWN, 0, 0, 0u },
    { "B boot inset", 1, VC_PITCH_BOOT, 0, 1, 0u },
    { "B sky turn",   1, VC_PITCH_SKY,  1, 0, 0u },
    { "B boot turn",  1, VC_PITCH_BOOT, 1, 0, 0u },
    { "B down turn",  1, VC_PITCH_DOWN, 1, 0, 0u },
    { "A sky still",  0, VC_PITCH_SKY,  0, 0, 0u },
    { "A boot still", 0, VC_PITCH_BOOT, 0, 0, 0u },
    { "A down still", 0, VC_PITCH_DOWN, 0, 0, 0u },
    { "A boot inset", 0, VC_PITCH_BOOT, 0, 1, 0u },
    { "A sky turn",   0, VC_PITCH_SKY,  1, 0, 0u },
#endif
#if VC_PART != 1
    { "A boot turn",  0, VC_PITCH_BOOT, 1, 0, 0u },
    { "A down turn",  0, VC_PITCH_DOWN, 1, 0, 0u },
#endif
#endif
};
#define VC_STAGES ((unsigned)(sizeof vc_script / sizeof vc_script[0]))

typedef struct vc_stat {
    unsigned n;
    double sum;
    float lo, hi;
} vc_stat_t;

typedef struct vc_run {
    unsigned stage, frame;          /* the stage, the frame in it (skipped ones included) */
    int done, stopped;              /* the script ended; a button ended it */
    unsigned stop_stage, stop_frame;
    vc_stat_t st[VC_STAGES][VC_METRICS];
    uint32_t evt0[VC_EVT_N];        /* the TA's event counts when the stage's counted frames began */
    uint32_t evt[VC_STAGES][VC_EVT_N];   /* and how many came in them */
} vc_run_t;

/* this frame's view: what psm_dc.c puts in its state before the frame is made */
typedef struct vc_view {
    int route, inset;
    float pitch, yaw;               /* yaw: added to the boot heading */
    unsigned opt;                   /* the placement's options */
} vc_view_t;

/* one frame's numbers from psm_dc.c: its marks and what it counted */
typedef struct vc_frame {
    uint64_t last_end, end;         /* the end of the frame before, and of this one */
    uint64_t rdy1_us;               /* route A: the target's fit and the ready wait before the map pass */
    uint64_t part[4];               /* this frame's share of its four parts, us */
    uint64_t m[VC_MARKS];
    int sampled, rtt_waited;
    uint64_t rnd_rtt_ns, rnd_scr_ns;
    unsigned v_all, v_ground, v_layer, v_model, v_map, vbuf_bytes, gvn, mvis, blk;   /* blk: model blocks the placement left out */
    uint32_t evt[VC_EVT_N];         /* the TA's event counts so far */
    uint64_t p0_set, p0_place, p0_sky;   /* p0's three steps, us: psm_frame_setup, psm_place_geo, psm_sky_build */
    int redo;                       /* 1: psm_place_geo placed everything again this frame (psm_frame_t.place_count moved) */
    unsigned t_ground, t_layer, t_model;   /* strip triangles sent by header (psm_sink_t.kt) */
    unsigned t_cast, t_drawn;       /* the casters' triangles: route A into the target (both the same); B given to the map, drawn */
} vc_frame_t;

static inline unsigned vc_stage_frames(unsigned s) { return VC_SKIP + (vc_script[s < VC_STAGES ? s : 0u].turn ? VC_TURNING : VC_STILL); }

static inline void vc_stat_add(vc_stat_t *s, float v) {
    if (!s->n || v < s->lo) s->lo = v;
    if (!s->n || v > s->hi) s->hi = v;
    s->sum += (double)v;
    s->n++;
}

static inline float vc_mean(const vc_stat_t *s) { return s->n ? (float)(s->sum / (double)s->n) : 0.0f; }

static inline void vc_start(vc_run_t *r) { memset(r, 0, sizeof *r); }

/* The view of the frame about to be made. */
static inline void vc_apply(const vc_run_t *r, vc_view_t *v) {
    const unsigned s = r->stage < VC_STAGES ? r->stage : VC_STAGES - 1u;
    const vc_stage_t *g = &vc_script[s];
    const unsigned n = vc_stage_frames(s);

    v->route = g->route; v->inset = g->inset; v->pitch = g->pitch; v->opt = g->opt | VC_SHIP_OPT;
    v->yaw = g->turn ? VC_TURN * (float)r->frame / (float)(n - 1u) : 0.0f;
}

/* a - b in microseconds as a float; 0 when the marks are out of order */
static inline float vc_us_(uint64_t a, uint64_t b) { return a > b ? (float)(a - b) : 0.0f; }

/* One frame's numbers, after it was sent. Moves the script on; 1 when it is done. route as vc_view_t's. */
static inline int vc_record(vc_run_t *r, const vc_frame_t *f) {
    unsigned e;

    if (r->done) return 1;
    if (r->frame + 1u == VC_SKIP) memcpy(r->evt0, f->evt, sizeof r->evt0);          /* the events of the frames not counted are left out */
    if (r->frame >= VC_SKIP) for (e = 0; e < VC_EVT_N; e++) r->evt[r->stage][e] = f->evt[e] - r->evt0[e];
    if (r->frame >= VC_SKIP && f->last_end) {
        vc_stat_t *st = r->st[r->stage];
        const int a = vc_script[r->stage].route == 0;
        const float frame = vc_us_(f->end, f->last_end);
        const float rtt = f->rtt_waited ? vc_us_(f->m[VC_M_RTT1], f->m[VC_M_RTT0]) : 0.0f;
        const float scr = f->sampled ? vc_us_(f->m[VC_M_SCR1], f->m[VC_M_SCR0]) : 0.0f;
        float p[4], t[VC_OTHER], sum = 0.0f;
        unsigned i;

        for (i = 0; i < 4u; i++) p[i] = (float)f->part[i];
        if (a) p[1] = p[1] > rtt ? p[1] - rtt : 0.0f;                     /* the sampled wait is inside route A's map part */
        t[VC_TOP] = vc_us_(f->m[VC_M_TOP], f->last_end);
        t[VC_P0] = p[0]; t[VC_P1] = p[1]; t[VC_P2] = p[2]; t[VC_P3] = p[3];
        t[VC_COLOUR] = vc_us_(f->m[VC_M_COLOUR], f->m[VC_M_PARTS]);
        t[VC_PLAN] = vc_us_(f->m[VC_M_PLAN], f->m[VC_M_COLOUR]);
        t[VC_RDY1] = a ? (float)f->rdy1_us : 0.0f;
        t[VC_RDY2] = vc_us_(f->m[VC_M_RDY], f->m[VC_M_PLAN]);
        t[VC_HUD] = vc_us_(f->m[VC_M_HUD], f->m[VC_M_RDY]);
        t[VC_OP] = vc_us_(f->m[VC_M_OP], f->m[VC_M_HUD]);
        t[VC_TR] = vc_us_(f->m[VC_M_TR], f->m[VC_M_OP]);
        t[VC_FIN] = vc_us_(f->m[VC_M_FIN], f->m[VC_M_TR]);
        for (i = VC_TOP; i < VC_OTHER; i++) sum += t[i];
        sum += rtt + scr;
        for (i = VC_TOP; i < VC_OTHER; i++) vc_stat_add(&st[i], t[i]);
        vc_stat_add(&st[VC_OTHER], frame > sum ? frame - sum : 0.0f);
        vc_stat_add(&st[VC_V_ALL], (float)f->v_all); vc_stat_add(&st[VC_V_GROUND], (float)f->v_ground);
        vc_stat_add(&st[VC_V_LAYER], (float)f->v_layer); vc_stat_add(&st[VC_V_MODEL], (float)f->v_model);
        vc_stat_add(&st[VC_V_MAP], (float)f->v_map); vc_stat_add(&st[VC_KB], (float)f->vbuf_bytes * (1.0f / 1024.0f));
        vc_stat_add(&st[VC_P0_SET], (float)f->p0_set); vc_stat_add(&st[VC_P0_PLACE], (float)f->p0_place);
        vc_stat_add(&st[VC_P0_SKY], (float)f->p0_sky); vc_stat_add(&st[VC_REDO], f->redo ? 1.0f : 0.0f);
        vc_stat_add(&st[VC_T_GROUND], (float)f->t_ground); vc_stat_add(&st[VC_T_LAYER], (float)f->t_layer); vc_stat_add(&st[VC_T_MODEL], (float)f->t_model);
        vc_stat_add(&st[VC_T_CAST], (float)f->t_cast); vc_stat_add(&st[VC_T_DRAWN], (float)f->t_drawn);
        if (f->sampled) {
            vc_stat_add(&st[VC_S_FRAME], frame);
            vc_stat_add(&st[VC_S_WAIT], rtt + scr);
            if (f->rtt_waited) vc_stat_add(&st[VC_S_RTT], (float)f->rnd_rtt_ns * 1.0e-3f);
            vc_stat_add(&st[VC_S_SCR], (float)f->rnd_scr_ns * 1.0e-3f);
            vc_stat_add(&st[VC_S_GVN], (float)f->gvn); vc_stat_add(&st[VC_S_MVIS], (float)f->mvis); vc_stat_add(&st[VC_S_BLK], (float)f->blk);
        } else
            vc_stat_add(&st[VC_FRAME], frame);
    }
    if (++r->frame < vc_stage_frames(r->stage)) return 0;
    r->frame = 0;
    if (++r->stage >= VC_STAGES) { r->stage = VC_STAGES - 1u; r->done = 1; }
    return r->done;
}

/* Start ended the run: the pages show what was measured. */
static inline void vc_stop(vc_run_t *r) {
    if (r->done) return;
    r->stopped = 1; r->stop_stage = r->stage; r->stop_frame = r->frame; r->done = 1;
}

/* The run's line on the screen while it runs, so a still stage is seen to be alive: the stage, its frame, the way to stop
   it. 58 characters at most (the host test checks every stage). */
static inline void vc_status_line(const vc_run_t *r, char *out, size_t size) {
    const unsigned s = r->stage < VC_STAGES ? r->stage : VC_STAGES - 1u;

    snprintf(out, size, "STAGE %2u OF %u  %-12s FRAME %3u OF %3u  START STOPS", s + 1u, VC_STAGES, vc_script[s].name, r->frame + 1u,
             vc_stage_frames(s));
}

/* A stage's name without its route and view ("B sky st f0" -> "st f0"): a bar graph's group label. */
static inline const char *vc_short_name(unsigned s) {
    const char *n = vc_script[s < VC_STAGES ? s : 0u].name + 2, *sp = strchr(n, ' ');

    return sp ? sp + 1 : n;
}

/* 1 when stage s is on route `route` (1 B, 0 A) at the view named `view` ("sky", "boot", "down": the name's second word) */
static inline int vc_stage_at(unsigned s, int route, const char *view) {
    const size_t n = strlen(view);

    return s < VC_STAGES && vc_script[s].route == route && !strncmp(vc_script[s].name + 2, view, n) && vc_script[s].name[2 + n] == ' ';
}

#define VC_MS_(st, k) ((double)vc_mean(&(st)[k]) * 1.0e-3)

#ifndef PSM_PAD_MCLIP
#define PSM_PAD_MCLIP 8192u             /* psm_scene.h's default: the bytes between mclip and mscr this build was made with */
#endif
#ifndef PSM_PAD_MN
#define PSM_PAD_MN 0u                   /* and between the scene's mv and mn (the turn plan's 2b) */
#endif
#ifndef PSM_PAD_LUT
#define PSM_PAD_LUT 0u                  /* and between the frame's mlut_a and mlut_s (2d) */
#endif
#ifndef PSM_SINK_INLINE
#define PSM_SINK_INLINE 0               /* psm_send.h's: the sink's hot path inline (step 2 b) */
#endif
#ifdef PSM_TOWN_CLOSED
#define VC_CLOSED_ 1                    /* the closed square's mesh (bake_square.py --closed): page 1 says so */
#else
#define VC_CLOSED_ 0
#endif

/* The turn plan's step 0 (VC_PART 8): two pages, a line a stage. The frame (sampled frames left out), p0's three steps, the
   shadow's parts (p1 + p2 + p3: the map, the casters, the compare or the uv), the rest (colour, the plan, the two sends) and
   the share of frames that placed everything again. Then, from a page of their own (brk), what each stage sent: vertices and
   triangles of the models, the ground and route A's layer, and the casters' triangles. */
static inline void vc_report_turn(const vc_run_t *r, void (*say)(const char *fmt, ...), void (*brk)(void), unsigned map_size, int warp, unsigned vbuf_kb) {
    unsigned s;

    say("TURN COST " VC_TAG "%s: pads clip %u mn %u lut %u B%s%s", VC_SHIP_OPT ? " +opt" : "", (unsigned)PSM_PAD_MCLIP, (unsigned)PSM_PAD_MN,
        (unsigned)PSM_PAD_LUT, PSM_SINK_INLINE ? " sink-in" : "", VC_CLOSED_ ? " closed" : "");
    if (VC_SHIP_OPT) say("+opt %u: 56 ship, 128 gnd, 256 keep, 512 bnd, 1k vis, 2k cast",(unsigned)VC_SHIP_OPT);
    say("map %u %s, 1/30 s, fr %u+%u st %u+%u tn, buf %u KB", map_size, warp ? "PSM" : "UNIFORM", VC_SKIP, VC_STILL, VC_SKIP, VC_TURNING, vbuf_kb);
    say("st still; fp still, everything placed again each frame;");
    say(" tn turning 90 deg. f0 plain placement, f7 all three options.");
    say("ms, means of the counted frames, one console, one run. After the");
    say(" name: the frame; p0 = se setup + pl placement + sk sky; sh =");
    say(r->stopped ? " p1+p2+p3; rs col+plan+sends; pr %%re-placed. STOP st %u fr %u" : " p1+p2+p3 (shadow); rs col+plan+sends; pr %% re-placed. All ran.",
        r->stop_stage + 1u, r->stop_frame);
    for (s = 0; s < VC_STAGES; s++) {
        const vc_stat_t *t = r->st[s];

        say("%-12s%6.1f se%5.2f pl%5.1f sk%5.2f sh%5.1f rs%5.1f pr%3.0f", vc_script[s].name, VC_MS_(t, VC_FRAME),
            VC_MS_(t, VC_P0_SET), VC_MS_(t, VC_P0_PLACE), VC_MS_(t, VC_P0_SKY), VC_MS_(t, VC_P1) + VC_MS_(t, VC_P2) + VC_MS_(t, VC_P3),
            VC_MS_(t, VC_COLOUR) + VC_MS_(t, VC_PLAN) + VC_MS_(t, VC_OP) + VC_MS_(t, VC_TR), (double)vc_mean(&t[VC_REDO]) * 100.0);
    }
    if (brk) brk();
    say("SENT A FRAME, means: vertices/triangles to the TA (strip");
    say(" triangles: run - 2 a strip). m models, g ground, l route A's");
    say(" ground layer (vertices). c casters: A map pass vertices/");
    say(" triangles into the target; B triangles given/drawn (SH-4).");
    for (s = 0; s < VC_STAGES; s++) {
        const vc_stat_t *t = r->st[s];
        const int a = vc_script[s].route == 0;

        say("%-12s m%6.0f/%5.0f g%6.0f/%5.0f l%6.0f c%6.0f/%5.0f", vc_script[s].name,
            (double)vc_mean(&t[VC_V_MODEL]), (double)vc_mean(&t[VC_T_MODEL]), (double)vc_mean(&t[VC_V_GROUND]), (double)vc_mean(&t[VC_T_GROUND]),
            (double)vc_mean(&t[VC_V_LAYER]), (double)vc_mean(&t[a ? VC_V_MAP : VC_T_CAST]), (double)vc_mean(&t[a ? VC_T_CAST : VC_T_DRAWN]));
    }
}

/* The result pages' text through `say` (psm_page.h's pg_say), `brk` a page break (pg_break; may be NULL). Numbers only:
   milliseconds are means, the frame also min .. max. map_size, warp, movers_only, tspeed, readout: the state the run had. */
static inline void vc_report(const vc_run_t *r, void (*say)(const char *fmt, ...), void (*brk)(void),
                             unsigned map_size, int warp, int movers_only, unsigned tspeed, int readout, unsigned vbuf_kb) {
    unsigned s;

#if VC_PART == 8
    (void)movers_only; (void)tspeed; (void)readout; (void)s;
    vc_report_turn(r, say, brk, map_size, warp, vbuf_kb);
    return;
#endif
    say("TOWN VIEW COST " VC_TAG ": where the frame goes, by view");
    say("pitch sky -72, boot 7, down 17 deg (+ down); a turn is 90 deg.");
    say(" Step 1/30 s; %u + %u (still) or %u (turn) fr a stage, first", VC_SKIP, VC_STILL, VC_TURNING);
    say(" %u not counted. State: map %u %s, casters %s, time x%u,", VC_SKIP, map_size, warp ? "PSM" : "UNIFORM", movers_only ? "MOVERS" : "ALL", tspeed);
    say(" readout %d, vertex buffer %u KB x2. One console, one run.", readout, vbuf_kb);
    say("ms, means. frame: frames with no sampled render wait");
    say(" (min..max). In frame order: top = pad, town; p0 build;");
    say(" A: p1 map pass, p2 uv. B: p1 clear, p2 casters, p3 compare;");
    say(" col colours; plan the send's dry count; rdy = ready waits");
    say(" (A before the map pass, both before the HUD); hud its text");
    say(" and inset block; op, tr the two lists' sends; fin the");
    say(" scene's finish; other = frame less all of these.");
    say("smp: frames the render times are sampled on (two waits):");
    say(" their count, frame and waits; PVR map, scr = render times.");
    say("v all, g ground, l layer, m models, map = vertices sent;");
    say(" KB = KallistiOS's buffer reading; gvn = ground window");
    say(" vertices, mvis = model vertices marked sent, blk = model");
    say(" blocks left out (sampled). f (or 15) in a name = options:");
    say(" 1 area 2 blocks 4 side 8 no dry count; c: 1 table 2 sent only");
    say("TA events in the counted frames: PARAM = vertex buffer full,");
    say(" OPB = tile bins; other = ISP, strip, input error, overflow.");
    if (r->stopped) say("RUN STOPPED BY START at stage %u frame %u: n shows what ran", r->stop_stage + 1u, r->stop_frame);
    else say("the run was not stopped: every stage ran whole");
    if (brk) brk();
    for (s = 0; s < VC_STAGES; s++) {
        const vc_stat_t *t = r->st[s];

        say("%2u %-12s n %3u frame %6.2f (%6.2f..%6.2f)", s + 1u, vc_script[s].name, t[VC_FRAME].n, VC_MS_(t, VC_FRAME),
            (double)t[VC_FRAME].lo * 1.0e-3, (double)t[VC_FRAME].hi * 1.0e-3);
        say("   top %5.2f p0 %5.2f p1 %5.2f p2 %5.2f p3 %5.2f col %5.2f", VC_MS_(t, VC_TOP), VC_MS_(t, VC_P0), VC_MS_(t, VC_P1),
            VC_MS_(t, VC_P2), VC_MS_(t, VC_P3), VC_MS_(t, VC_COLOUR));
        say("   plan %5.2f rdy %5.2f %5.2f hud %5.2f op %5.2f tr %5.2f", VC_MS_(t, VC_PLAN), VC_MS_(t, VC_RDY1), VC_MS_(t, VC_RDY2),
            VC_MS_(t, VC_HUD), VC_MS_(t, VC_OP), VC_MS_(t, VC_TR));
        say("   fin %5.2f other %5.2f; smp %2u frame %6.2f wait %5.2f", VC_MS_(t, VC_FIN), VC_MS_(t, VC_OTHER), t[VC_S_FRAME].n,
            VC_MS_(t, VC_S_FRAME), VC_MS_(t, VC_S_WAIT));
        say("   PVR map %5.2f scr %5.2f v %5.0f g %5.0f l %5.0f m %5.0f", VC_MS_(t, VC_S_RTT), VC_MS_(t, VC_S_SCR),
            (double)vc_mean(&t[VC_V_ALL]), (double)vc_mean(&t[VC_V_GROUND]), (double)vc_mean(&t[VC_V_LAYER]), (double)vc_mean(&t[VC_V_MODEL]));
        say("   map v %5.0f KB %4.0f gvn %5.0f mvis %5.0f blk %3.0f", (double)vc_mean(&t[VC_V_MAP]), (double)vc_mean(&t[VC_KB]),
            (double)vc_mean(&t[VC_S_GVN]), (double)vc_mean(&t[VC_S_MVIS]), (double)vc_mean(&t[VC_S_BLK]));
        say("   TA events PARAM %lu OPB %lu other %lu", (unsigned long)r->evt[s][2], (unsigned long)r->evt[s][3],
            (unsigned long)(r->evt[s][0] + r->evt[s][1] + r->evt[s][4] + r->evt[s][5]));
        if (brk && s % 3u == 2u) brk();
    }
}

#endif /* PSM_VIEWCOST_H */
