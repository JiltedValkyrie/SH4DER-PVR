/* SH4DER-PVR: the perspective shadow map demo's map source hook test

   test/psm_mapsrc_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Host check of demo/psm/psm_mapsrc.h, the seam for a future route AB (a map filled by somebody else, read a frame
   late): at delay 0 the hook is route B byte for byte (the depth map, the ground's bytes, the models' bytes); at
   delay 1 the map read on frame N is frame N - 1's map exactly (its depth, its matrix, its scale, its size), the
   first frame has no map, and the compare gives what a direct compare against that older map gives; a fill that
   takes several frames is never read half done, no fill starts over it, and the complete map is not touched; a
   change of map size is read one frame late like the rest; a failed fill keeps the last complete map; the route of
   three; the readout's title; bad input.
*/

#include <math.h>
#include <stdio.h>
#include <string.h>
#include "../demo/psm/psm_mapsrc.h"

static int fails;
#define CHECK(c) do { if (!(c)) { fails++; fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

#define MAP_MAX 256u
#define FRAMES 9u

static psm_scene_t scene;
static psm_frame_t frame, direct;
static uint16_t buf0[MAP_MAX * MAP_MAX], buf1[MAP_MAX * MAP_MAX], dbuf[MAP_MAX * MAP_MAX];
static uint16_t keep_depth[FRAMES][MAP_MAX * MAP_MAX];    /* route B's map of each frame, for the frame after it */
static sh4der_psm_raster_t keep_raster[FRAMES];
static uint8_t want_g[PSM_GV_MAX], want_m[PSM_MV_MAX];

/* a camera that turns and a sun that moves, a different map every frame; frame 5 on a smaller map */
static void state_at(psm_state_t *st, unsigned n) {
    psm_state_default(st);
    st->route = PSM_ROUTE_B; st->warp = (int)(n & 1u);
    st->yaw = 0.35f * (float)n; st->az = 120.0f + 15.0f * (float)n; st->el = 35.0f - 2.0f * (float)n;
    st->size_i = n >= 5u ? 1u : 2u;
    st->bias_b = (n & 2u) ? 1.5f : 0.0f;
}

/* route B, as the program runs it, into `direct` and dbuf */
static int route_b(const psm_state_t *st) {
    psm_frame_setup(&direct, &scene, st);
    if (!psm_frame_b_begin(&direct, &scene, dbuf)) { psm_frame_unshadowed(&direct, &scene); return 0; }
    psm_frame_b_casters(&direct, &scene, st->movers_only);
    psm_frame_b_receivers(&direct, &scene, st);
    return 1;
}

static int same_raster(const sh4der_psm_raster_t *a, const sh4der_psm_raster_t *b) {
    return a->size == b->size && a->key_scale == b->key_scale && !memcmp(a->m, b->m, sizeof a->m)
        && !memcmp(a->depth, b->depth, (size_t)a->size * a->size * sizeof *a->depth);
}

/* delay 0: the hook with the SH-4 raster as its fill is route B */
static void delay0(void) {
    psm_mapsrc_t src;
    psm_ab_stub_t stub = { &frame, &scene, 0 };
    unsigned n;

    CHECK(psm_mapsrc_init(&src, psm_ab_stub_fill, &stub, 0u, buf0, NULL));
    for (n = 0; n < FRAMES; n++) {
        const sh4der_psm_raster_t *map;
        psm_state_t st;

        state_at(&st, n);
        CHECK(route_b(&st));
        psm_frame_setup(&frame, &scene, &st);
        map = psm_frame_ab_fill(&frame, &scene, &src);
        CHECK(map != NULL);
        if (!map) continue;
        psm_frame_ab_compare(&frame, &scene, &st, map);
        CHECK(map->depth == buf0 && same_raster(map, &direct.raster));
        CHECK(!memcmp(frame.glit, direct.glit, direct.gvn) && !memcmp(frame.mlit, direct.mlit, scene.mvn));
        CHECK(frame.tris == direct.tris && frame.drawn == direct.drawn);
    }
    CHECK(src.fills == FRAMES && src.swaps == 0u && src.skipped == 0u);
}

/* delay 1: frame N reads frame N - 1's map, whole */
static void delay1(void) {
    psm_mapsrc_t src;
    psm_ab_stub_t stub = { &frame, &scene, 0 };
    unsigned n, i, lagged = 0;

    for (n = 0; n < FRAMES; n++) {                               /* route B's map of every frame, kept */
        psm_state_t st;

        state_at(&st, n);
        CHECK(route_b(&st));
        memcpy(keep_depth[n], dbuf, sizeof dbuf);
        keep_raster[n] = direct.raster;
        keep_raster[n].depth = keep_depth[n];
    }
    CHECK(psm_mapsrc_init(&src, psm_ab_stub_fill, &stub, 1u, buf0, buf1));
    for (n = 0; n < FRAMES; n++) {
        const sh4der_psm_raster_t *map;
        psm_state_t st;

        state_at(&st, n);
        psm_frame_setup(&frame, &scene, &st);
        map = psm_frame_ab_fill(&frame, &scene, &src);
        if (n == 0u) {                                           /* nothing complete yet: unshadowed */
            CHECK(map == NULL);
            psm_frame_unshadowed(&frame, &scene);
            for (i = 0; i < frame.gvn; i++) if (frame.glit[i] != 255u) { CHECK(frame.glit[i] == 255u); break; }
            continue;
        }
        CHECK(map != NULL);
        if (!map) continue;
        CHECK(same_raster(map, &keep_raster[n - 1u]));           /* the map of the frame before, exactly */
        CHECK(map->depth != src.slot[src.write].depth);          /* and not the buffer this frame's fill went into */
        CHECK(same_raster(&src.slot[src.write], &keep_raster[n]));   /* which holds this frame's map, for the next */
        psm_frame_ab_compare(&frame, &scene, &st, map);
        /* what a direct compare of this frame's receivers against that older map gives */
        psm_frame_setup(&direct, &scene, &st);
        direct.raster = keep_raster[n - 1u];
        psm_frame_b_receivers(&direct, &scene, &st);
        CHECK(!memcmp(frame.glit, direct.glit, direct.gvn) && !memcmp(frame.mlit, direct.mlit, scene.mvn));
        memcpy(want_g, direct.glit, direct.gvn); memcpy(want_m, direct.mlit, scene.mvn);
        CHECK(route_b(&st));                                     /* and it is not this frame's own answer: the lag is real */
        lagged += memcmp(want_g, direct.glit, direct.gvn) != 0;
    }
    CHECK(lagged > 0u);
    CHECK(src.fills == FRAMES && src.swaps == FRAMES - 1u && src.skipped == 0u);
    printf("  delay 1: %u frames, %u swaps, the map read is the frame before's on every one; %u of %u frames differ from the same-frame answer\n",
           FRAMES, src.swaps, lagged, FRAMES - 1u);
}

/* a fill that lands later: it scribbles on half its buffer, says "pending", and finishes when told */
typedef struct { unsigned calls; sh4der_psm_raster_t *last; int fail; } slow_t;

static int slow_fill(sh4der_psm_raster_t *dst, void *user) {
    slow_t *u = (slow_t *)user;
    unsigned i;

    u->calls++;
    u->last = dst;
    if (u->fail) return PSM_MAPSRC_FAIL;
    for (i = 0; i < dst->size * dst->size / 2u; i++) dst->depth[i] = (uint16_t)(0x4000u + u->calls);   /* half done */
    return PSM_MAPSRC_PENDING;
}

static void slow_finish(slow_t *u) {
    unsigned i;

    for (i = u->last->size * u->last->size / 2u; i < u->last->size * u->last->size; i++) u->last->depth[i] = (uint16_t)(0x4000u + u->calls);
}

static int all_are(const sh4der_psm_raster_t *r, unsigned v) {
    unsigned i;

    for (i = 0; i < r->size * r->size; i++) if (r->depth[i] != v) return 0;
    return 1;
}

static void slow_source(void) {
    psm_mapsrc_t src;
    slow_t slow = { 0u, NULL, 0 };
    const sh4der_psm_raster_t *map;
    psm_state_t st;
    unsigned n;

    state_at(&st, 1u);
    psm_frame_setup(&frame, &scene, &st);
    CHECK(frame.built);
    CHECK(psm_mapsrc_init(&src, slow_fill, &slow, 1u, buf0, buf1));
    for (n = 0; n < 4u; n++) {                                   /* the first fill is on its way for four frames */
        map = psm_mapsrc_frame(&src, &frame.map, 64u, 0.0f, 10.0f);
        CHECK(map == NULL);                                      /* a half filled buffer is never the map */
    }
    CHECK(slow.calls == 1u && src.fills == 1u && src.skipped == 3u);   /* and no second fill was started over it */
    slow_finish(&slow);
    psm_mapsrc_ready(&src);
    map = psm_mapsrc_frame(&src, &frame.map, 64u, 0.0f, 10.0f);  /* landed: read from now on; fill 2 starts in the other slot */
    CHECK(map != NULL && map == slow.last - (slow.last == &src.slot[1] ? 1 : -1));
    CHECK(map && all_are(map, 0x4001u) && slow.calls == 2u && slow.last != map && slow.last->depth != map->depth);
    for (n = 0; n < 3u; n++) {                                   /* fill 2 on its way: the complete map stays, untouched */
        const sh4der_psm_raster_t *again = psm_mapsrc_frame(&src, &frame.map, 64u, 0.0f, 10.0f);

        CHECK(again == map && all_are(again, 0x4001u) && slow.calls == 2u);
    }
    slow_finish(&slow);
    psm_mapsrc_ready(&src);
    map = psm_mapsrc_frame(&src, &frame.map, 64u, 0.0f, 10.0f);
    CHECK(map && all_are(map, 0x4002u) && slow.calls == 3u);
    psm_mapsrc_ready(&src);                                      /* told ready while only half is written: the source's own promise, not checked here */
    slow.fail = 1;                                               /* a fill that fails: the last complete map stays */
    map = psm_mapsrc_frame(&src, &frame.map, 64u, 0.0f, 10.0f);
    CHECK(map != NULL && slow.calls == 4u);
    {
        const sh4der_psm_raster_t *again = psm_mapsrc_frame(&src, &frame.map, 64u, 0.0f, 10.0f);

        CHECK(again == map && !src.pending && slow.calls == 5u);
    }
    psm_mapsrc_reset(&src);
    slow.fail = 0;
    CHECK(psm_mapsrc_frame(&src, &frame.map, 64u, 0.0f, 10.0f) == NULL && !src.have);   /* a reset: no map again */
}

static void small_things(void) {
    char ln[PSM_HUD_LINES][PSM_HUD_COLS + 1u];
    psm_mapsrc_t src;
    psm_ab_stub_t stub = { &frame, &scene, 0 };
    psm_state_t st;
    psm_cost_t cost;
    int route3 = PSM_ROUTE_A;

    CHECK(!psm_mapsrc_init(NULL, psm_ab_stub_fill, &stub, 0u, buf0, NULL));
    CHECK(!psm_mapsrc_init(&src, NULL, &stub, 0u, buf0, NULL));
    CHECK(!psm_mapsrc_init(&src, psm_ab_stub_fill, &stub, 0u, NULL, NULL));
    CHECK(!psm_mapsrc_init(&src, psm_ab_stub_fill, &stub, 2u, buf0, buf1));
    CHECK(!psm_mapsrc_init(&src, psm_ab_stub_fill, &stub, 1u, buf0, NULL));     /* delay 1 needs the second buffer */
    CHECK(!psm_mapsrc_init(&src, psm_ab_stub_fill, &stub, 1u, buf0, buf0));     /* and a different one */
    CHECK(psm_mapsrc_frame(&src, &frame.map, 64u, 0.0f, 1.0f) == NULL);         /* a source that was refused gives no map */
    CHECK(psm_mapsrc_init(&src, psm_ab_stub_fill, &stub, 1u, buf0, buf1));
    state_at(&st, 0u);
    psm_frame_setup(&frame, &scene, &st);
    CHECK(psm_mapsrc_frame(&src, &frame.map, 64u, 1.0f, 1.0f) == NULL && src.fills == 0u);   /* a bad depth range: no fill */

    /* psm_input flips the route between 0 and 1 and turns a 2 into 0; three routes come out in turn */
    CHECK(psm_ab_route_step(&route3, PSM_ROUTE_A, PSM_ROUTE_A) == PSM_ROUTE_A);
    CHECK(psm_ab_route_step(&route3, PSM_ROUTE_A, !PSM_ROUTE_A) == PSM_ROUTE_B);
    CHECK(psm_ab_route_step(&route3, PSM_ROUTE_B, !PSM_ROUTE_B) == PSM_ROUTE_AB);
    CHECK(psm_ab_route_step(&route3, PSM_ROUTE_AB, PSM_ROUTE_AB) == PSM_ROUTE_AB);
    CHECK(psm_ab_route_step(&route3, PSM_ROUTE_AB, !PSM_ROUTE_AB) == PSM_ROUTE_A);

    memset(&cost, 0, sizeof cost);
    st.route = PSM_ROUTE_AB;
    psm_ab_hud(&st, &frame, &cost, &src, ln);
    printf("  | %s\n", ln[0]);
    CHECK(!strncmp(ln[0], "AB STUB: SH-4 fill, 1 frame late", 32) && strstr(ln[0], "MAP ") && strlen(ln[0]) <= PSM_HUD_COLS);
    CHECK(!strstr(ln[0], "PVR"));                                 /* it does not say it is the PVR's route */
    CHECK(strstr(ln[6], "--"));                                   /* and nothing measured is shown before the console measures */
    CHECK(strstr(ln[2], "2 x ") && strstr(ln[2], "KB RAM"));         /* two maps in RAM, the filled one and the read one */
    st.menu = 1; st.row = PSM_ROW_ROUTE;
    psm_ab_hud(&st, &frame, &cost, &src, ln);
    printf("  | %s\n  | %s\n", ln[2], ln[8]);
    CHECK(strstr(ln[8], "ROUTE: AB") == ln[8] && strlen(ln[8]) <= PSM_HUD_COLS);   /* the menu row says AB, not B */
    st.menu = 0;
    src.delay = 0u;
    psm_ab_hud(&st, &frame, &cost, &src, ln);
    CHECK(strstr(ln[0], "same frame"));
}

int main(void) {
    psm_scene_init(&scene);
    delay0();
    delay1();
    slow_source();
    small_things();
    printf("  cost: the second slot's buffer, 32 KB at 128 x 128 (128 KB at 256 x 256), and %u B of state\n", (unsigned)sizeof(psm_mapsrc_t));
    if (fails) { fprintf(stderr, "psm_mapsrc_test: %d failure(s)\n", fails); return 1; }
    printf("psm_mapsrc_test: ok\n");
    return 0;
}
