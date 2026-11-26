/* Host test of the baked occlusion (demo/psm/psm_ao.h over include/sh4der_aomesh.h), its colours (psm_colour in psm_scene.h), the AO row and the
   one-press hide (psm_input, psm_send_hud). Plain C, on the town or the square's mesh:

   cc -std=c11 -O2 -Wall -Wextra -DSH4DER_CORE_NO_SH4ZAM -DPSM_HAVE_TOWN -I<mesh dir> -I<sky dir> -Iinclude -Ish4dow-dc/include \
      demo/psm/test/psm_ao_test.c -lm -o psm_ao_test */

#include <math.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "../psm_scene.h"
#include "../psm_sky.h"
#define PSM_SINK_HDR(kind) ((void)(kind))
#define PSM_SINK_VTX(eol, X, Y, Z, U, V, ARGB, OARGB) ((void)0)
#include "../psm_send.h"
#include "../psm_ao.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; printf("FAIL line %d: %s\n", __LINE__, #c); } } while (0)

static psm_scene_t scene;
static psm_frame_t frame;
static psm_ao_t ao, ao2;
static uint32_t ref_m[PSM_MV_MAX], ref_g[PSM_GV_MAX], tab_m[PSM_MV_MAX];
static unsigned ticks;
static void tick(void) { ticks++; }

/* The bytes' checksum (FNV-1a 64). The plain C path pins it: the bake's bytes do not move on a refactor. The SH4ZAM path
   prints it and, with argv[1], writes the raw bytes (vertices, then ground); with argv[2] (a file of those bytes from the
   plain path) it prints the largest difference. */
/* The pin is for the town's mesh from the Fantasy-kit zip (demo/psm/assets/_dl/modular_village_collection.zip), the mesh
   with PIN_MESH_VERT static vertices and PIN_MESH_TRI static triangles. Any other mesh (no zip: the stand-in; SQUARE=square:
   the walled square) prints "pin NOT RUN" and passes: the pin is not its bytes. */
#define PIN_VERT_FNV   0x6b3016ac8abc6a74ull   /* retaken 2026-11-26 for the edge tests on the numerators (was 0x7f1f04588085b50f: 3 bytes moved) */
#define PIN_GROUND_FNV 0x9eacf14eb738d88full
#define PIN_MESH_VERT  8697u
#define PIN_MESH_TRI   5320u
static uint64_t fnv_(uint64_t h, const uint8_t *p, size_t n) { size_t i; for (i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; } return h; }

static void lit(psm_state_t *st) {
    psm_frame_setup(&frame, &scene, st);
    psm_place_geo(&frame, &scene);
    psm_frame_unshadowed(&frame, &scene); psm_frame_a_casters(&frame, &scene); psm_frame_a_ground(&frame);
}

static unsigned chan_far(uint32_t a, uint32_t b, unsigned tol) {
    unsigned k, n = 0;

    for (k = 0; k < 24u; k += 8u) { const int d = (int)((a >> k) & 255u) - (int)((b >> k) & 255u); if (d > (int)tol || d < -(int)tol) n++; }
    return n + ((a >> 24) != (b >> 24));
}

int main(int argc, char **argv) {
    psm_state_t st;
    unsigned i, n, open = 0, dark = 0, lo_y = 0, hi_y = 0;
    unsigned long lo_sum = 0, hi_sum = 0;
    clock_t t0;

    /* the packed add and subtract */
    CHECK(psm_rgb_add_(0xff102030u, 0xff010203u) == 0xff112233u);
    CHECK(psm_rgb_add_(0xfff0f0f0u, 0xff202020u) == 0xffffffffu && psm_rgb_add_(0xff00ff00u, 0xff000100u) == 0xff00ff00u);
    CHECK(psm_rgb_add_(0xffff0000u, 0xff010001u) == 0xffff0001u && psm_rgb_add_(0xff0000ffu, 0xff000001u) == 0xff0000ffu);

    /* the bake */
    psm_scene_init_town(&scene);
    t0 = clock();
    CHECK(psm_ao_bake(&ao, &scene, tick) && ao.ok && ticks > 0u);
    printf("bake: %u static vertices, %u static triangles, %u rays, %u ray-triangle tests (%.1f a ray), %u reciprocals, ground grid %u x %u, %.0f ms on this host\n",
           scene.mdyn0, scene.tdyn0, (unsigned)ao.rays, (unsigned)ao.tests, (double)ao.tests / (double)(ao.rays ? ao.rays : 1u), (unsigned)ao.recips, ao.gw, ao.gh,
           (double)(clock() - t0) * 1000.0 / CLOCKS_PER_SEC);
    CHECK(ao.rays == (scene.mdyn0 + ao.gw * ao.gh) * SH4DER_AOMESH_RAYS);
    for (i = 0; i < scene.mdyn0; i++) {
        open += ao.mao[i] >= 250u; dark += ao.mao[i] < 128u;
        if (scene.mn[i][1] < 0.3f && scene.mn[i][1] > -0.3f) {           /* a wall's vertex: its foot is darker than its head */
            if (scene.mv[i][1] < 0.3f) { lo_sum += ao.mao[i]; lo_y++; }
            else if (scene.mv[i][1] > 2.5f) { hi_sum += ao.mao[i]; hi_y++; }
        }
    }
    for (i = scene.mdyn0; i < scene.mvn; i++) CHECK(ao.mao[i] == 255u);  /* the movers are open */
    CHECK(lo_y > 50u && hi_y > 50u && lo_sum / lo_y + 20u < hi_sum / hi_y);
    CHECK(open > 0u && dark > 0u);
    printf("  model bytes: %u open (250+), %u under 128; walls: mean %lu at the foot, %lu above 2.5 m\n", open, dark, lo_sum / lo_y, hi_sum / hi_y);
    {   /* the ground: darker somewhere than it is in the open, open outside the grid; the same bake twice is the same bytes */
        unsigned gmin = 255u, gopen = 0u;

        CHECK(ao.g != NULL && ao.gw > 8u && ao.gh > 8u);
        for (i = 0; i < ao.gw * ao.gh; i++) { if (ao.g[i] < gmin) gmin = ao.g[i]; gopen += ao.g[i] == 255u; }
        CHECK(gmin < 200u && gopen > ao.gw * ao.gh / 10u);
        CHECK(psm_ao_ground(&ao, ao.gx0 - 1.0f, ao.gz0 + 1.0f) == 255u && psm_ao_ground(&ao, 1e9f, 0.0f) == 255u && psm_ao_ground(&ao, (float)NAN, 0.0f) == 255u);
        CHECK(psm_ao_ground(&ao, ao.gx0 + 0.25f, ao.gz0 + 0.25f) == ao.g[0]);
        printf("  ground bytes: least %u, %u of %u cells open\n", gmin, gopen, ao.gw * ao.gh);
        CHECK(psm_ao_bake(&ao2, &scene, NULL) && memcmp(ao.mao, ao2.mao, sizeof ao.mao) == 0 && ao2.gw == ao.gw && memcmp(ao.g, ao2.g, ao.gw * ao.gh) == 0);
        psm_ao_free(&ao2);
        CHECK(ao2.g == NULL && !ao2.ok && ao2.mao[0] == 255u);
    }

    {   /* the bytes' checksum, the pin (plain C), the raw dump and the comparison (argv, for the SH4ZAM path) */
        const uint64_t hv = fnv_(1469598103934665603ull, ao.mao, scene.mdyn0);
        const uint64_t hg = fnv_(1469598103934665603ull, ao.g, ao.gw * ao.gh);

        printf("bytes: %u vertices fnv1a64 %016llx; ground %u x %u fnv1a64 %016llx\n", scene.mdyn0, (unsigned long long)hv,
               ao.gw, ao.gh, (unsigned long long)hg);
#ifdef SH4DER_CORE_NO_SH4ZAM
        if (scene.mdyn0 == PIN_MESH_VERT && scene.tdyn0 == PIN_MESH_TRI) {
            CHECK(hv == PIN_VERT_FNV && hg == PIN_GROUND_FNV);
            printf("pin checked: the town's kit mesh (%u vertices, %u triangles)\n", scene.mdyn0, scene.tdyn0);
        } else {
            printf("pin NOT RUN: not the town's kit mesh (%u vertices)\n", scene.mdyn0);
        }
#endif
        if (argc > 1) {                                                  /* dump: vertices, then the ground */
            FILE *f = fopen(argv[1], "wb");

            CHECK(f != NULL);
            if (f) { fwrite(ao.mao, 1, scene.mdyn0, f); fwrite(ao.g, 1, ao.gw * ao.gh, f); fclose(f); }
        }
        if (argc > 2) {                                                  /* compare with a plain dump */
            const unsigned n = scene.mdyn0 + ao.gw * ao.gh;
            uint8_t *ref = (uint8_t *)malloc(n);
            FILE *f = fopen(argv[2], "rb");
            unsigned k, diff = 0, big = 0;

            CHECK(ref != NULL && f != NULL);
            if (ref && f && fread(ref, 1, n, f) == n) {
                for (k = 0; k < n; k++) {
                    const int d = (int)(k < scene.mdyn0 ? ao.mao[k] : ao.g[k - scene.mdyn0]) - (int)ref[k];
                    const unsigned a = d < 0 ? (unsigned)-d : (unsigned)d;

                    diff += a != 0u;
                    if (a > big) big = a;
                }
                printf("vs plain dump: %u bytes, %u differ, largest difference %u\n", n, diff, big);
            }
            if (f) fclose(f);
            free(ref);
        }
    }

    /* the colours: off is what it always was; all open is the same colours; baked is darker or equal, never brighter; the
       table's form agrees with the per-vertex one */
    psm_state_town(&st);
    CHECK(st.ao == 1 && st.hide == 0);                                   /* AO is on by default */
    lit(&st);
    psm_ao_frame(&frame, NULL);
    CHECK(frame.mao == NULL && frame.gao_on == 0);
    psm_colour(&frame, &scene);
    memcpy(ref_m, frame.mcol, sizeof ref_m); memcpy(ref_g, frame.gcol, sizeof ref_g);
    {
        static psm_ao_t all;

        psm_ao_free(&all); all.ok = 1;                                   /* every byte 255, no ground grid */
        psm_ao_frame(&frame, &all);
        CHECK(frame.mao == all.mao && frame.gao_on == 0);
        psm_colour(&frame, &scene);
        for (n = 0, i = 0; i < scene.mvn; i++) n += chan_far(frame.mcol[i], ref_m[i], 1u);
        CHECK(n == 0u);
        CHECK(memcmp(frame.gcol, ref_g, frame.gvn * sizeof ref_g[0]) == 0);
    }
    psm_ao_frame(&frame, &ao);
    CHECK(frame.mao == ao.mao && frame.gao_on == 1 && frame.gao_n == frame.gvn);
    for (i = 0; i < frame.gvn; i++) CHECK(frame.gao[i] == psm_ao_ground(&ao, frame.gpos[i][0], frame.gpos[i][2]));
    frame.gao[0] ^= 1u;                                                  /* a mark: the same ground positions are not read again */
    psm_ao_frame(&frame, &ao);
    CHECK((frame.gao[0] ^ 1u) == psm_ao_ground(&ao, frame.gpos[0][0], frame.gpos[0][2]));
    frame.gao[0] ^= 1u;
    psm_colour(&frame, &scene);
    {
        unsigned darker = 0, gdarker = 0, k;

        for (i = 0; i < scene.mvn; i++) {
            for (k = 0; k < 24u; k += 8u) CHECK(((frame.mcol[i] >> k) & 255u) <= ((ref_m[i] >> k) & 255u) + 1u);
            darker += (frame.mcol[i] & 0xffu) + 8u < (ref_m[i] & 0xffu) || ((frame.mcol[i] >> 8) & 0xffu) + 8u < ((ref_m[i] >> 8) & 0xffu);
            CHECK((frame.mcol[i] >> 24) == 255u);
        }
        for (i = 0; i < frame.gvn; i++) {
            for (k = 0; k < 24u; k += 8u) CHECK(((frame.gcol[i] >> k) & 255u) <= ((ref_g[i] >> k) & 255u) + 1u);
            gdarker += ((frame.gcol[i] >> 8) & 0xffu) + 8u < ((ref_g[i] >> 8) & 0xffu);
            if (frame.gao[i] == 255u) CHECK(chan_far(frame.gcol[i], ref_g[i], 1u) == 0u);
        }
        CHECK(darker > 100u);
        printf("  this view: %u of %u model vertices and %u of %u ground vertices visibly darker\n", darker, scene.mvn, gdarker, frame.gvn);
        memcpy(tab_m, frame.mcol, sizeof tab_m);
        frame.colour_opt = PSM_COLOUR_TABLE;
        psm_colour(&frame, &scene);
        for (n = 0, i = 0; i < scene.mvn; i++) n += chan_far(frame.mcol[i], tab_m[i], 2u);
        CHECK(n == 0u);                                                  /* the table's form: within 2 of 255 a channel */
        psm_ao_frame(&frame, NULL);
        psm_colour(&frame, &scene);
        CHECK(memcmp(frame.mcol, ref_m, scene.mvn * sizeof ref_m[0]) == 0);   /* and off again, with the table: the plain table */
        frame.colour_opt = 0u;
    }

    /* the AO row */
    {
        char buf[64];

        CHECK(psm_row_def[PSM_ROW_AO].group == PSM_GRP_VIEW && strcmp(psm_row_name[PSM_ROW_AO], "AO") == 0);
        CHECK(strncmp(psm_row_value(&st, PSM_ROW_AO, buf, sizeof buf), "ON", 2) == 0 && psm_row_int(&st, PSM_ROW_AO) == 1);
        st.ao_ms = 2340u;
        CHECK(strstr(psm_row_value(&st, PSM_ROW_AO, buf, sizeof buf), "2.3 s") != NULL);
        st.ao_none = 1;
        CHECK(strstr(psm_row_value(&st, PSM_ROW_AO, buf, sizeof buf), "not baked") != NULL);
        psm_row_change(&st, PSM_ROW_AO, 1);
        CHECK(st.ao == 0 && strcmp(psm_row_value(&st, PSM_ROW_AO, buf, sizeof buf), "off") == 0 && psm_row_int(&st, PSM_ROW_AO) == 0);
        psm_row_change(&st, PSM_ROW_AO, -1);
        CHECK(st.ao == 1);
    }

    /* the hide: Y + A together, panel closed, on and off; a step forward or back alone does nothing to it; START shows
       all again; hidden, the readout sends nothing */
    {
        psm_walk_limits_t lim;
        psm_pad_t pad = { 0u, 0.0f, 0.0f, 0.0f, 0.0f };
        static psm_sink_t sink;
        char ln[PSM_HUD_LINES][PSM_HUD_COLS + 1u];
        float x0, z0;

        psm_scene_limits(&scene, &lim);
        psm_state_town(&st);
        pad.held = PSM_BTN_Y; psm_input(&st, &lim, &pad, 1.0f / 60.0f); CHECK(!st.hide);
        pad.held = 0u; psm_input(&st, &lim, &pad, 1.0f / 60.0f);
        x0 = st.x; z0 = st.z;
        pad.held = PSM_BTN_Y | PSM_BTN_A; psm_input(&st, &lim, &pad, 1.0f / 60.0f); CHECK(st.hide == 1);
        psm_input(&st, &lim, &pad, 1.0f / 60.0f); CHECK(st.hide == 1);                  /* held: once */
        CHECK(st.x == x0 && st.z == z0);                                               /* forward and back: no step */
        pad.held = PSM_BTN_Y; psm_input(&st, &lim, &pad, 1.0f / 60.0f); CHECK(st.hide == 1);
        pad.held = PSM_BTN_Y | PSM_BTN_A; psm_input(&st, &lim, &pad, 1.0f / 60.0f); CHECK(st.hide == 0);   /* the same press brings it back */
        pad.held = 0u; psm_input(&st, &lim, &pad, 1.0f / 60.0f);
        pad.held = PSM_BTN_Y | PSM_BTN_A; psm_input(&st, &lim, &pad, 1.0f / 60.0f); CHECK(st.hide == 1);
        pad.held = 0u; psm_input(&st, &lim, &pad, 1.0f / 60.0f);
        psm_sink_reset(&sink, 0, 0, 0u, NULL);
        memset(sink.hdr_ok, 1, sizeof sink.hdr_ok);
        psm_send_hud(&sink, &st, 0, 3u);
        CHECK(sink.verts == 0u && sink.bytes == 0u);                                   /* hidden: nothing sent, the heartbeat too */
        st.hide = 0;
        psm_send_hud(&sink, &st, 0, 3u);
        CHECK(sink.verts > 0u);
        st.hide = 1;
        pad.held = PSM_BTN_START; psm_input(&st, &lim, &pad, 1.0f / 60.0f);
        CHECK(st.menu == 1 && st.hide == 0);                                           /* the panel opens and all is shown */
        pad.held = 0u; psm_input(&st, &lim, &pad, 1.0f / 60.0f);
        pad.held = PSM_BTN_Y | PSM_BTN_A; psm_input(&st, &lim, &pad, 1.0f / 60.0f);
        CHECK(st.hide == 0);                                                           /* in the panel the keys are the panel's */
        psm_state_town(&st);
        lit(&st);
        psm_hud(&st, &frame, NULL, ln);
        CHECK(strstr(ln[9], "Y+A HUD") != NULL && strlen(ln[9]) <= PSM_HUD_COLS);      /* the first screen's key line says so */
    }

    psm_ao_free(&ao);
    printf(fails ? "psm_ao_test: %d FAILED\n" : "psm_ao_test: ok\n", fails);
    return fails != 0;
}
