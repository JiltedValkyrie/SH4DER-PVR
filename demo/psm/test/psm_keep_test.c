/* Host test of PSM_KEEP_SUN (demo/psm/psm_scene.h, the turn plan's step 2): two frames go through the same town steps, one
   making everything every frame and one keeping what depends on the light alone. After every step the two have the same
   bytes where anything reads them: the models' sun share (mlit) and colours, the ground's sun share, colours and route A's
   map coordinates. The steps: the camera still, then turning, the sun stepped a clock minute at a time, the movers
   walking, route A, B and A again, AO on and off, and the light's colours changed. The keeps must also be hit: on a still
   step with the same sun the static N.L, the ground tables and the coordinates are not made again. Plain C. */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../psm_scene.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static psm_scene_t scene;
static psm_frame_t ref, kept, vis;   /* vis: PSM_PLACE_VIS (step 4), the seen marks kept on a still camera */
static uint16_t depth[1024u * 1024u];
static uint8_t ao_g[PSM_GV_MAX], ao_m[PSM_MV_MAX];

static void run(psm_frame_t *f, const psm_state_t *st, int ao) {
    psm_frame_setup(f, &scene, st);
    psm_place_geo(f, &scene);
    if (st->route == PSM_ROUTE_A) { psm_frame_unshadowed(f, &scene); psm_frame_a_casters(f, &scene); psm_frame_a_ground(f); }
    else if (psm_frame_b_begin(f, &scene, depth)) { psm_frame_b_casters(f, &scene, 0); psm_frame_b_receivers(f, &scene, st); }
    else psm_frame_unshadowed(f, &scene);
    f->mao = ao ? ao_m : NULL;
    f->gao_on = ao;
    if (ao) { memcpy(f->gao, ao_g, sizeof f->gao); f->gao_gen = f->gpos_gen; f->gao_n = f->gvn; }
    psm_colour(f, &scene);
}

static unsigned differ(const psm_state_t *st) {
    unsigned i, n = 0;

    if (ref.gvn != kept.gvn) return 1u;
    for (i = 0; i < scene.mvn; i++) n += ref.mlit[i] != kept.mlit[i] || ref.mcol[i] != kept.mcol[i];
    for (i = 0; i < ref.gvn; i++) {
        n += ref.glit[i] != kept.glit[i] || ref.gcol[i] != kept.gcol[i];
        if (st->route == PSM_ROUTE_A) n += ref.gok[i] != kept.gok[i] || memcmp(ref.guv[i], kept.guv[i], sizeof ref.guv[i]) != 0;
    }
    return n;
}

int main(void) {
    psm_state_t st;
    unsigned step, i, hits_n = 0, hits_g = 0, hits_t = 0, sun_moves = 0, bad = 0, hits_v = 0, bad_v = 0;
    float yaw0, sun_was[3] = { 0.0f, 0.0f, 0.0f };

    psm_scene_init_town(&scene);
    for (i = 0; i < PSM_GV_MAX; i++) ao_g[i] = (uint8_t)(97u + (i * 37u) % 159u);
    for (i = 0; i < PSM_MV_MAX; i++) ao_m[i] = (uint8_t)(80u + (i * 53u) % 176u);
    psm_state_town(&st);
    st.sun_step = PSM_SUN_STEP_H;
    st.tspeed = 3u;                                                    /* a faster clock: the sun steps within the run */
    yaw0 = st.yaw;
    ref.keep_opt = vis.keep_opt = 0u; kept.keep_opt = PSM_KEEP_SUN;
    ref.place_opt = kept.place_opt = 7u; vis.place_opt = 7u | PSM_PLACE_VIS;
    ref.colour_opt = kept.colour_opt = vis.colour_opt = PSM_COLOUR_TABLE | PSM_COLOUR_SEEN;
    for (step = 0; step < 240u; step++) {
        const int ao = (step / 40u) % 2u == 1u;
        unsigned gen_before;
        int unsh_before, guv_before;

        st.route = step < 80u ? PSM_ROUTE_A : step < 160u ? PSM_ROUTE_B : PSM_ROUTE_A;
        st.yaw = yaw0 + ((step % 60u) >= 30u ? 0.02f * (float)(step % 30u) : 0.0f);   /* 30 still steps, then 30 turning */
        if (step == 200u) { scene.amb[1] *= 0.8f; scene.sunc[0] *= 0.9f; }        /* the light's colours change: the tables follow */
        psm_town_update(&scene, &st, 1.0f / 30.0f);
        unsh_before = kept.unsh_ok; guv_before = kept.guv_ok; gen_before = kept.gpos_gen;
        run(&ref, &st, ao);
        run(&kept, &st, ao);
        {
            const unsigned before = vis.place_count;

            run(&vis, &st, ao);
            hits_v += vis.place_count == before;              /* a frame with no whole placement: the marks were kept */
            for (i = 0; i < scene.mvn; i++)
                if (ref.mvis[i] != vis.mvis[i] || ref.mlit[i] != vis.mlit[i] || ref.mcol[i] != vis.mcol[i]) { bad_v++; break; }
            for (i = 0; i < ref.gvn; i++)
                if (ref.gcol[i] != vis.gcol[i]) { bad_v++; break; }
        }
        if (memcmp(sun_was, kept.sun, sizeof sun_was)) { sun_moves++; memcpy(sun_was, kept.sun, sizeof sun_was); }
        else if (st.route == PSM_ROUTE_A && step % 80u != 0u) {        /* the same sun: what was kept is used */
            hits_n += unsh_before && kept.unsh_ok;
            hits_g += kept.glut_ok;
            hits_t += guv_before && kept.guv_ok && gen_before == kept.gpos_gen;
        }
        i = differ(&st);
        if (i) { bad++; if (bad < 5u) printf("step %u route %c ao %d: %u bytes differ\n", step, st.route == PSM_ROUTE_A ? 'A' : 'B', ao, i); }
    }
    CHECK(bad == 0u);
    CHECK(sun_moves >= 3u && sun_moves < 120u);                        /* the sun stepped, and stood still between steps */
    CHECK(hits_n > 50u && hits_g > 50u && hits_t > 10u);
    CHECK(bad_v == 0u && hits_v > 50u);                                /* PSM_PLACE_VIS: the same marks and colours, kept on still steps */
    printf("psm_keep_test: PSM_PLACE_VIS: %u steps differ, %u steps kept the static marks\n", bad_v, hits_v);
    printf("psm_keep_test: 240 steps, the sun moved %u times; kept and used: static N.L %u, ground tables %u, map coordinates %u steps\n",
           sun_moves, hits_n, hits_g, hits_t);
    {                                                                  /* the sun step: the clock snapped down to a clock minute */
        psm_state_t a, b;

        psm_state_town(&a); psm_state_town(&b);
        a.sun_step = b.sun_step = PSM_SUN_STEP_H;
        a.clock = 14.0f + 0.2f / 60.0f; b.clock = 14.0f + 0.7f / 60.0f;
        psm_town_update(&scene, &a, 0.0f); psm_town_update(&scene, &b, 0.0f);
        CHECK(a.az == b.az && a.el == b.el);
        b.clock = 14.0f + 1.2f / 60.0f;
        psm_town_update(&scene, &b, 0.0f);
        CHECK(a.az != b.az || a.el != b.el);
        a.sun_step = 0.0f; a.clock = 14.0f + 0.2f / 60.0f;
        psm_town_update(&scene, &a, 0.0f);
        b.sun_step = 0.0f; b.clock = 14.0f + 0.7f / 60.0f;
        psm_town_update(&scene, &b, 0.0f);
        CHECK(a.az != b.az || a.el != b.el);                            /* no step: the sun follows the clock, as before */
    }
    printf(fails ? "psm_keep_test: %d FAILED\n" : "psm_keep_test: ok\n", fails);
    return fails != 0;
}
