/* Host test of psm_send_plan_next (demo/psm/psm_send.h): the SHED level from the frame before's own send, with no dry
   count. Against psm_send_plan on the town: a still view under the share is sent the same (level 0 both ways, the same
   bytes); a small buffer makes the lazy level climb a frame at a time to the plan's level and stay there with no flip;
   a large one lets it fall back to 0. Plain C. */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../psm_scene.h"
#include "../psm_sky.h"
#define PSM_SINK_HDR(kind) ((void)(kind))
#define PSM_SINK_VTX(eol, X, Y, Z, U, V, ARGB, OARGB) ((void)0)
#include "../psm_send.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static psm_scene_t scene;
static psm_frame_t frame;
static psm_sky_out_t sky;
static psm_state_t st;
static psm_sink_t k;

static uint32_t send_at(unsigned shed) {                     /* what the real send counts at a level */
    psm_sink_reset(&k, 1, 0, shed, NULL);
    psm_send_op(&k, &frame, &scene, &sky);
    psm_send_tr(&k, &frame, &scene, &st, 1, 0u);
    return k.bytes;
}

int main(void) {
    const uint32_t v = 1024u * 1024u, cap = (uint32_t)((float)v * PSM_TA_SHARE), back = (uint32_t)((float)v * PSM_SHED_BACK);
    psm_lazy_t z;
    uint8_t ok[PSM_HK_N];
    unsigned plan, i, flips, pc;
    uint32_t bytes0, vbuf;

    /* the rule */
    memset(&z, 0, sizeof z);
    CHECK(psm_send_plan_next(&z, cap - PSM_TA_BKG, v) == 0u && !z.rose);
    CHECK(psm_send_plan_next(&z, cap - PSM_TA_BKG + 1u, v) == 1u && z.rose);
    CHECK(psm_send_plan_next(&z, cap - PSM_TA_BKG + 1u - 5000u, v) == 1u && z.gap[0] == 5000u && !z.rose);     /* fits now; level 0 would not */
    CHECK(psm_send_plan_next(&z, back - PSM_TA_BKG - 5000u + 1u, v) == 1u);                                 /* one byte over the mark with the gap */
    CHECK(psm_send_plan_next(&z, back - PSM_TA_BKG - 5000u, v) == 0u);
    memset(&z, 0, sizeof z);
    for (i = 0; i < PSM_SHED_MAX + 3u; i++) psm_send_plan_next(&z, v, v);                                   /* nothing fits: the top level, and no further */
    CHECK(z.shed == PSM_SHED_MAX);

    /* the town's boot view on route A */
    psm_scene_init_town(&scene);
    psm_state_town(&st);
    st.route = PSM_ROUTE_A;
    psm_frame_setup(&frame, &scene, &st);
    psm_place_geo(&frame, &scene);
    psm_sky_build(&sky, &frame, &st);
    psm_frame_unshadowed(&frame, &scene);
    psm_frame_a_casters(&frame, &scene);
    psm_frame_a_ground(&frame);
    psm_colour(&frame, &scene);
    memset(ok, 1, sizeof ok);

    plan = psm_send_plan(&k, &frame, &scene, &st, &sky, 1, ok, PSM_TA_VBUF);
    bytes0 = k.bytes;
    memset(&z, 0, sizeof z);
    CHECK(plan == 0u && send_at(0u) == bytes0 && psm_send_plan_next(&z, bytes0, PSM_TA_VBUF) == 0u);
    printf("boot view, route A: %u bytes at SHED 0 (%u KB; the %u KB buffer's share is %u KB)\n", (unsigned)bytes0, (unsigned)(bytes0 / 1024u),
           (unsigned)(PSM_TA_VBUF / 1024u), (unsigned)((float)PSM_TA_VBUF * PSM_TA_SHARE / 1024.0f));
    for (i = 0; i <= PSM_SHED_MAX; i++) printf("  level %u: %u bytes\n", i, (unsigned)send_at(i));

    /* buffers the frame does not fit: the lazy level climbs to the plan's and holds, with no flip */
    for (pc = 95u; pc >= 35u; pc -= 10u) {
        unsigned prev;

        vbuf = (uint32_t)((float)bytes0 * (float)pc * 0.01f);
        plan = psm_send_plan(&k, &frame, &scene, &st, &sky, 1, ok, vbuf);
        memset(&z, 0, sizeof z);
        for (flips = 0, i = 0; i < 16u; i++) {
            prev = z.shed;
            psm_send_plan_next(&z, send_at(z.shed), vbuf);
            if (i >= PSM_SHED_MAX && z.shed != prev) flips++;
        }
        CHECK(flips == 0u && z.shed == plan);
        printf("a buffer of %u %% of that: the plan's level %u, the lazy level %u after %u frames at most, %u flips after\n", pc, plan, z.shed, PSM_SHED_MAX, flips);

        for (i = 0; i < 8u; i++) psm_send_plan_next(&z, send_at(z.shed), PSM_TA_VBUF);      /* the buffer large again: back to 0 */
        CHECK(z.shed == 0u);
    }

    printf(fails ? "psm_plan_test: %d FAILED\n" : "psm_plan_test: ok\n", fails);
    return fails != 0;
}
