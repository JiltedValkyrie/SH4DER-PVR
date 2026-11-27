/* Host test of the colours' options (demo/psm/psm_scene.h, PSM_COLOUR_*): on the town, routes A and B, the table gives
   every model vertex the colour the per-vertex expression gives; with PSM_COLOUR_SEEN every vertex that is sent has it;
   the table is made once and again only when the light or the palette changes; the ground's colours do not change.
   Plain C. */

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "../psm_scene.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static psm_scene_t scene;
static psm_frame_t frame;
static uint32_t ref_m[PSM_MV_MAX], ref_g[PSM_GV_MAX];
static uint16_t depth[1024u * 1024u];

static unsigned differ(int seen_only) {
    unsigned i, n = 0;

    for (i = 0; i < scene.mvn; i++) if ((!seen_only || frame.mvis[i]) && frame.mcol[i] != ref_m[i]) n++;
    for (i = 0; i < frame.gvn; i++) if (frame.gcol[i] != ref_g[i]) n++;
    return n;
}

static void lit(psm_state_t *st) {
    psm_frame_setup(&frame, &scene, st);
    psm_place_geo(&frame, &scene);
    if (st->route == PSM_ROUTE_A) { psm_frame_unshadowed(&frame, &scene); psm_frame_a_casters(&frame, &scene); psm_frame_a_ground(&frame); }
    else if (psm_frame_b_begin(&frame, &scene, depth)) { psm_frame_b_casters(&frame, &scene, 0); psm_frame_b_receivers(&frame, &scene, st); }
    else { fails++; puts("FAIL: route B map"); }
}

static void plain(void) {
    frame.colour_opt = 0u;
    memset(frame.mcol, 0, sizeof frame.mcol);
    psm_colour(&frame, &scene);
    memcpy(ref_m, frame.mcol, sizeof ref_m); memcpy(ref_g, frame.gcol, sizeof ref_g);
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    psm_state_t st;
    unsigned route, i, sent, made;

    psm_scene_init_town(&scene);
    for (route = 0; route < 2u; route++) {
        psm_state_town(&st);
        st.route = route ? PSM_ROUTE_B : PSM_ROUTE_A;
        lit(&st);
        plain();
        for (sent = 0, i = 0; i < scene.mvn; i++) sent += frame.mvis[i];
        CHECK(sent > 0u && sent < scene.mvn);

        frame.colour_opt = PSM_COLOUR_TABLE;
        memset(frame.mcol, 0, sizeof frame.mcol);
        psm_colour(&frame, &scene);
        CHECK(differ(0) == 0u && frame.mlut_ok && frame.mlut_n == scene.paln);

        frame.mlut[0][0] ^= 1u;                              /* a mark: a second frame with the same light does not make the table again */
        psm_colour(&frame, &scene);
        made = (frame.mlut[0][0] ^ 1u) != psm_pack_rgb(scene.pal[0][0] * scene.amb[0], scene.pal[0][1] * scene.amb[1], scene.pal[0][2] * scene.amb[2]);
        CHECK(!made);
        frame.mlut[0][0] ^= 1u;

        frame.colour_opt = PSM_COLOUR_TABLE | PSM_COLOUR_SEEN;
        memset(frame.mcol, 0, sizeof frame.mcol);
        psm_colour(&frame, &scene);
        CHECK(differ(1) == 0u);
        for (made = 0, i = 0; i < scene.mvn; i++) if (!frame.mvis[i] && frame.mcol[i]) made++;
        CHECK(made == 0u);                                   /* and nothing was worked out for the others */

        frame.colour_opt = PSM_COLOUR_SEEN;                  /* the per-vertex way, the sent ones only */
        memset(frame.mcol, 0, sizeof frame.mcol);
        psm_colour(&frame, &scene);
        CHECK(differ(1) == 0u);

        printf("route %c: %u model vertices, %u sent (%u %%), palette %u: the table's colours are the per-vertex ones\n", route ? 'B' : 'A', scene.mvn, sent,
               sent * 100u / scene.mvn, scene.paln);

        /* the light changes: the table follows */
        scene.amb[0] *= 0.5f; scene.sunc[2] *= 0.7f;
        plain();
        frame.colour_opt = PSM_COLOUR_TABLE;
        psm_colour(&frame, &scene);
        CHECK(differ(0) == 0u);
        scene.pal[scene.paln - 1u][1] *= 0.5f; scene.pal[0][0] *= 0.9f;      /* and the palette */
        plain();
        frame.colour_opt = PSM_COLOUR_TABLE;
        psm_colour(&frame, &scene);
        CHECK(differ(0) == 0u);
        psm_scene_init_town(&scene);
    }
    {                                                        /* a vertex whose palette entry is past the palette takes entry 0, as before */
        psm_state_town(&st);
        lit(&st);
        scene.mkind[0] = (uint8_t)(scene.paln + 3u);
        plain();
        frame.colour_opt = PSM_COLOUR_TABLE;
        psm_colour(&frame, &scene);
        CHECK(differ(0) == 0u);
    }
    printf(fails ? "psm_colour_test: %d FAILED\n" : "psm_colour_test: ok\n", fails);
    return fails != 0;
}
