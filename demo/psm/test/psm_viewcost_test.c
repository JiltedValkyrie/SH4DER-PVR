/* Host test of demo/psm/psm_viewcost.h: the script's views and frame counts, the statistics (the sampled frames apart, the
   map's sampled wait out of route A's part, the left-over), a stopped run, the page text's width, the run's line on the
   screen and (VC_PART 8) the graphs' groups and labels. Plain C. */
#include <string.h>
#include <math.h>
#include <stdarg.h>
#include <stdio.h>

#include "psm_viewcost.h"

static int failed;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failed++; } } while (0)

static unsigned lines, widest, breaks;
static int show;

static void say(const char *fmt, ...) {
    char b[320];
    va_list ap;
    unsigned n;

    va_start(ap, fmt);
    n = (unsigned)vsnprintf(b, sizeof b, fmt, ap);
    va_end(ap);
    lines++;
    if (n > widest) widest = n;
    if (show) printf("%s\n", b);
}

static void brk(void) { breaks++; }

/* a frame of 50 ms whose steps are 1, 2, .. ms; sampled ones wait 5 + 7 ms more */
static void fake(vc_frame_t *f, uint64_t *clock, int route_a, int sampled) {
    static const unsigned step_ms[] = { 1, 2, 3, 4, 5, 6, 7, 8 };   /* colour, plan, rdy2, hud, op, tr, fin, and the top */
    uint64_t t = *clock;
    unsigned i;

    memset(f->m, 0, sizeof f->m);
    f->last_end = t;
    t += step_ms[7] * 1000u; f->m[VC_M_TOP] = t;
    f->part[0] = 2000u; f->part[1] = 6000u + (route_a && sampled ? 5000u : 0u); f->part[2] = 4000u; f->part[3] = route_a ? 0u : 9000u;
    f->rdy1_us = route_a ? 500u : 0u;
    f->rtt_waited = route_a && sampled;
    if (f->rtt_waited) { f->m[VC_M_RTT0] = t + 100u; f->m[VC_M_RTT1] = t + 5100u; }
    t += f->part[0] + f->part[1] + f->part[2] + f->part[3] + f->rdy1_us;
    f->m[VC_M_PARTS] = t;
    for (i = 0; i < 7u; i++) { t += step_ms[i] * 1000u; f->m[VC_M_COLOUR + i] = t; }
    f->m[VC_M_SCR0] = t;
    if (sampled) t += 7000u;
    f->m[VC_M_SCR1] = t;
    t += 250u;                                  /* what no mark covers */
    f->end = t;
    f->sampled = sampled;
    f->rnd_rtt_ns = 3900000u; f->rnd_scr_ns = 12000000u;
    f->v_all = 30000u; f->v_ground = 8000u; f->v_layer = route_a ? 8000u : 0u; f->v_model = 10000u; f->v_map = 3000u;
    f->vbuf_bytes = 700u * 1024u; f->gvn = 5141u; f->mvis = 8815u;
    f->evt[2] += route_a ? 1u : 0u; f->evt[3] += 1u;                /* a PARAM event a route A frame, an OPB event a frame */
    f->p0_set = 300u; f->p0_place = 1500u; f->p0_sky = 200u;        /* p0's three steps (they sum to part[0]) */
    f->redo = sampled;
    f->t_ground = 7000u; f->t_layer = route_a ? 7000u : 0u; f->t_model = 6000u;
    f->t_cast = route_a ? 1100u : 1200u; f->t_drawn = route_a ? 1100u : 900u;
    *clock = t;
}

int main(void) {
    static vc_run_t r;
    vc_frame_t f;
    vc_view_t v;
    uint64_t clock = 1000u;
    unsigned s, k, total = 0;

#if VC_PART == 0
    CHECK(VC_STAGES == 14u);
#elif VC_PART == 8
    CHECK(VC_STAGES == 36u);
    for (s = 0; s < VC_STAGES; s++) {                                       /* step 0's script: every still view has its forced twin, both routes, f0 and f7 */
        const vc_stage_t *g = &vc_script[s];

        CHECK((g->opt & ~(VC_FORCE | 7u)) == 0u && ((g->opt & 7u) == 0u || (g->opt & 7u) == 7u) && !(g->turn && (g->opt & VC_FORCE)) && !g->inset);
        CHECK(s < 18u ? g->route == 1 : g->route == 0);                    /* route B first */
        if (g->opt & VC_FORCE) {
            unsigned k2, still = 0, turn = 0;

            for (k2 = 0; k2 < VC_STAGES; k2++) {
                const vc_stage_t *h = &vc_script[k2];

                if (h->route != g->route || h->pitch != g->pitch || (h->opt & 7u) != (g->opt & 7u) || (h->opt & VC_FORCE)) continue;
                if (h->turn) turn++; else still++;
            }
            CHECK(still == 1u && turn == 1u);
        }
    }
    {                                                                       /* the graphs: a route and view a graph, six stages, labels that fit a group */
        static const char *const view[3] = { "sky", "boot", "down" };
        unsigned rt, vw, in;

        for (rt = 0; rt < 2u; rt++)
            for (vw = 0; vw < 3u; vw++) {
                for (s = 0, in = 0; s < VC_STAGES; s++) {
                    if (!vc_stage_at(s, (int)rt, view[vw])) continue;
                    in++;
                    CHECK(strlen(vc_short_name(s)) == 5u);                  /* "st f0": 60 pixels of a 75 pixel group at 12 a character */
                    for (k = 0; k < s; k++) CHECK(!vc_stage_at(k, (int)rt, view[vw]) || strcmp(vc_short_name(k), vc_short_name(s)));
                }
                CHECK(in == 6u);
            }
    }
#endif
    {                                                                       /* the line on the screen while it runs: every stage at its last frame fits */
        char line[65];                                                     /* psm_dc.c's HUD line: 64 characters */

        vc_start(&r);
        for (s = 0; s < VC_STAGES; s++) {
            r.stage = s; r.frame = vc_stage_frames(s) - 1u;
            vc_status_line(&r, line, sizeof line);
            CHECK(strlen(line) <= 58u && strstr(line, vc_script[s].name) && strstr(line, "START"));
        }
    }
    vc_start(&r);
    memset(&f, 0, sizeof f);
    for (s = 0; s < VC_STAGES; s++) {
        const unsigned n = vc_stage_frames(s);

        CHECK(n == VC_SKIP + (vc_script[s].turn ? VC_TURNING : VC_STILL));
        for (k = 0; k < n; k++) {
            CHECK(!r.done && r.stage == s && r.frame == k);
            vc_apply(&r, &v);
            CHECK(v.route == vc_script[s].route && v.inset == vc_script[s].inset && v.pitch == vc_script[s].pitch && v.opt == (vc_script[s].opt | VC_SHIP_OPT));
            if (!vc_script[s].turn) CHECK(v.yaw == 0.0f);
            else if (k == 0u) CHECK(v.yaw == 0.0f);
            else if (k == n - 1u) CHECK(fabsf(v.yaw - VC_TURN) < 1e-5f);
            fake(&f, &clock, vc_script[s].route == 0, k % 15u == 3u);
            vc_record(&r, &f);
            total++;
        }
    }
    CHECK(r.done && !r.stopped);
    (void)total;
#if VC_PART == 0
    CHECK(total == 7u * 2u * VC_SKIP + 8u * VC_STILL + 6u * VC_TURNING);
#endif
    for (s = 0; s < VC_STAGES; s++) {
        const vc_stat_t *t = r.st[s];
        const int a = vc_script[s].route == 0;
        const unsigned counted = vc_stage_frames(s) - VC_SKIP;
        const float base = 8000.0f + 2000.0f + 6000.0f + 4000.0f + (a ? 500.0f : 9000.0f) + 28000.0f + 250.0f;

        CHECK(t[VC_FRAME].n + t[VC_S_FRAME].n == counted && t[VC_S_FRAME].n >= counted / 15u);
        CHECK(fabsf(vc_mean(&t[VC_FRAME]) - base) < 1.0f && t[VC_FRAME].lo == t[VC_FRAME].hi);
        CHECK(fabsf(vc_mean(&t[VC_S_FRAME]) - (base + (a ? 12000.0f : 7000.0f))) < 1.0f);
        CHECK(fabsf(vc_mean(&t[VC_S_WAIT]) - (a ? 12000.0f : 7000.0f)) < 1.0f);
        CHECK(fabsf(vc_mean(&t[VC_P1]) - 6000.0f) < 1.0f);                    /* the sampled wait is out of route A's map part */
        CHECK(fabsf(vc_mean(&t[VC_TOP]) - 8000.0f) < 1.0f && fabsf(vc_mean(&t[VC_COLOUR]) - 1000.0f) < 1.0f && fabsf(vc_mean(&t[VC_PLAN]) - 2000.0f) < 1.0f);
        CHECK(fabsf(vc_mean(&t[VC_RDY2]) - 3000.0f) < 1.0f && fabsf(vc_mean(&t[VC_HUD]) - 4000.0f) < 1.0f && fabsf(vc_mean(&t[VC_OP]) - 5000.0f) < 1.0f);
        CHECK(fabsf(vc_mean(&t[VC_TR]) - 6000.0f) < 1.0f && fabsf(vc_mean(&t[VC_FIN]) - 7000.0f) < 1.0f);
        CHECK(fabsf(vc_mean(&t[VC_RDY1]) - (a ? 500.0f : 0.0f)) < 1.0f && fabsf(vc_mean(&t[VC_P3]) - (a ? 0.0f : 9000.0f)) < 1.0f);
        CHECK(fabsf(vc_mean(&t[VC_OTHER]) - 250.0f) < 2.0f);
        CHECK(t[VC_S_RTT].n == (a ? t[VC_S_FRAME].n : 0u) && fabsf(vc_mean(&t[VC_S_SCR]) - 12000.0f) < 1.0f);
        CHECK(vc_mean(&t[VC_V_ALL]) == 30000.0f && vc_mean(&t[VC_KB]) == 700.0f && vc_mean(&t[VC_S_MVIS]) == 8815.0f);
        CHECK(r.evt[s][3] == counted && r.evt[s][2] == (a ? counted : 0u) && r.evt[s][0] == 0u);   /* the counted frames' events only */
        CHECK(fabsf(vc_mean(&t[VC_P0_SET]) - 300.0f) < 1.0f && fabsf(vc_mean(&t[VC_P0_PLACE]) - 1500.0f) < 1.0f && fabsf(vc_mean(&t[VC_P0_SKY]) - 200.0f) < 1.0f);
        CHECK(t[VC_REDO].n == counted && vc_mean(&t[VC_REDO]) * (float)counted == (float)t[VC_S_FRAME].n);   /* 1 on the sampled frames, 0 on the rest */
        CHECK(vc_mean(&t[VC_T_GROUND]) == 7000.0f && vc_mean(&t[VC_T_LAYER]) == (a ? 7000.0f : 0.0f) && vc_mean(&t[VC_T_MODEL]) == 6000.0f);
        CHECK(vc_mean(&t[VC_T_CAST]) == (a ? 1100.0f : 1200.0f) && vc_mean(&t[VC_T_DRAWN]) == (a ? 1100.0f : 900.0f));
#if VC_PART == 0
        CHECK(s < 7u ? !a : a);                                              /* route B first */
#endif
    }
    show = 1;
    vc_report(&r, say, brk, 256u, 0, 0, 1u, 1, VC_VBUF / 1024u);
    show = 0;
    CHECK(widest <= 64u);                       /* psm_page.h's PG_COLS: no line is wrapped */
#if VC_PART == 8
    CHECK(lines == 7u + (VC_SHIP_OPT ? 1u : 0u) + VC_STAGES + 4u + VC_STAGES && breaks == 1u);         /* the times (two pages, a line a stage), then the counts from a page of their own */
    CHECK(8u + VC_STAGES <= 2u * 22u && 4u + VC_STAGES <= 2u * 22u);
#else
    CHECK(lines == 22u + 7u * VC_STAGES && breaks == 1u + VC_STAGES / 3u);
#endif
    printf("pages: %u lines, widest %u, %u breaks\n", lines, widest, breaks);

    /* a run a button ends: what ran is kept, the rest has no frames, and the page says where */
    vc_start(&r);
    clock = 1000u;
    for (k = 0; k < VC_SKIP + 30u; k++) { fake(&f, &clock, vc_script[0].route == 0, 0); vc_record(&r, &f); }
    vc_stop(&r);
    CHECK(r.done && r.stopped && r.stop_stage == 0u && r.stop_frame == VC_SKIP + 30u && r.st[0][VC_FRAME].n == 30u && r.st[1][VC_FRAME].n == 0u);
    CHECK(vc_record(&r, &f) == 1 && r.st[0][VC_FRAME].n == 30u);
    lines = widest = breaks = 0;
    vc_report(&r, say, NULL, 512u, 1, 1, 60u, 2, 1024u);
#if VC_PART == 8
    CHECK(widest <= 64u && lines == 7u + (VC_SHIP_OPT ? 1u : 0u) + VC_STAGES + 4u + VC_STAGES);
#else
    CHECK(widest <= 64u && lines == 22u + 7u * VC_STAGES);
#endif

    printf(failed ? "psm_viewcost_test: %d FAILED\n" : "psm_viewcost_test: ok\n", failed);
    return failed != 0;
}
