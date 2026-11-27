/* SH4DER-PVR: host test of demo/psm/ao/ao0_model.h (the ao0 image's scenes and its model of the target)

   demo/psm/ao/test/ao0_model_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   The scenes' counts, and the reading (ao0_read) against two made-up targets: one drawn as the union would leave it
   (every texel must read right, either way up), one drawn as plain blending would (it must read wrong). It tests the
   program's reading of a target; it says nothing of what the PVR draws.

   Build: gcc -O2 -std=c99 -Wall -Wextra -Werror -DSH4DER_CORE_NO_SH4ZAM demo/psm/ao/test/ao0_model_test.c */

#include <stdio.h>
#include "../ao0_model.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static ao0_scene_t scene;
static uint8_t bits[AO0_SIZE * AO0_SIZE], count[AO0_SIZE * AO0_SIZE], edge[AO0_SIZE * AO0_SIZE];
static uint16_t px[AO0_SIZE * AO0_SIZE];

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    unsigned k, i, x, y;

    ao0_small(&scene, 5.0f, 0.0f);
    CHECK(scene.vn == 32u && scene.tn == 48u, "SMALL: %u vertices, %u triangles", scene.vn, scene.tn);
    ao0_load(&scene);
    CHECK(scene.vn == AO0_LOAD_BOXES * 8u && scene.tn == 1164u, "LOAD: %u vertices, %u triangles", scene.vn, scene.tn);
    for (i = 0; i < scene.vn; i++)
        CHECK(scene.v[i][0] > -AO0_SPAN + 1.0f && scene.v[i][0] < AO0_SPAN - 1.0f && scene.v[i][2] > -AO0_SPAN + 1.0f && scene.v[i][2] < AO0_SPAN - 1.0f && scene.v[i][1] <= 4.0f,
              "LOAD vertex %u is off the ground's middle", i);
    for (k = 1; k <= 8u; k *= 2u) {                                /* LOAD: what is sent, for the cost rows' estimate */
        const unsigned drawn = ao0_cover(&scene, k, 0, bits, count);

        CHECK(drawn > k * 1164u * 5u / 6u - k * 200u && drawn <= k * 1164u, "LOAD k %u: %u triangles drawn", k, drawn);
        printf("  LOAD k %u: %u triangles, %u vertices, %u KB of TA at 60 bytes a triangle\n", k, drawn, drawn * 3u, drawn * 60u / 1024u);
    }
    ao0_small(&scene, 5.0f, 0.0f);
    for (k = 1; k <= 8u; k *= 2u) {
        ao0_read_t r;
        unsigned edges, overlap = 0, max = 0;

        {   /* one side alone covers what both sides cover, with half the triangles, whichever side it is */
            static uint8_t b1[AO0_SIZE * AO0_SIZE], c1[AO0_SIZE * AO0_SIZE];
            const unsigned both = ao0_cover(&scene, k, 0, bits, count);
            unsigned front = ao0_cover(&scene, k, 1, b1, c1);
            unsigned differ = 0;

            edges = ao0_edges(bits, edge);
            for (i = 0; i < AO0_SIZE * AO0_SIZE; i++) differ += !edge[i] && b1[i] != bits[i];
            CHECK(differ == 0u && front * 2u == both, "k %u side 1: %u texels differ, %u of %u triangles", k, differ, front, both);
            front = ao0_cover(&scene, k, -1, b1, c1);
            for (differ = 0, i = 0; i < AO0_SIZE * AO0_SIZE; i++) differ += !edge[i] && b1[i] != bits[i];
            CHECK(differ == 0u && front * 2u == both, "k %u side -1: %u texels differ, %u of %u triangles", k, differ, front, both);
            ao0_cover(&scene, k, 0, bits, count);
        }
        for (i = 0; i < AO0_SIZE * AO0_SIZE; i++) {
            const unsigned n = ao0_popcount(bits[i]);

            CHECK(count[i] >= n, "count under bits at %u", i);
            if (count[i] > n) overlap++;
            if (n > max) max = n;
            CHECK(bits[i] >> k == 0u, "a bit past direction k");
        }
        CHECK(edges < AO0_SIZE * AO0_SIZE / 4u, "k %u: %u edge texels", k, edges);
        CHECK(overlap > 100u, "k %u: only %u texels under two triangles of one direction: the union test needs them", k, overlap);
        for (y = 0; y < AO0_SIZE; y++) for (x = 0; x < AO0_SIZE; x++) px[y * AO0_SIZE + x] = ao0_guess565(k, ao0_popcount(bits[y * AO0_SIZE + x]));
        ao0_read(px, bits, edge, 0, &r);
        CHECK(r.bad == 0u && r.colours == r.levels && r.levels >= 2u && r.tested + edges == AO0_SIZE * AO0_SIZE, "k %u union: bad %u colours %u levels %u", k, r.bad, r.colours, r.levels);
        CHECK(r.value[0] == 0xffffu, "the lightest is white");
        for (i = 1; i < r.colours; i++) CHECK(ao0_luma565(r.value[i]) < ao0_luma565(r.value[i - 1u]), "k %u: values in order", k);
        printf("  SMALL k %u: %u levels (most blocked %u), %u edge texels, %u texels under overlapping triangles; 565 guess:", k, r.levels, max, edges, overlap);
        for (i = 0; i < r.colours; i++) printf(" %04x", r.value[i]);
        printf("\n");
        for (y = 0; y < AO0_SIZE; y++) for (x = 0; x < AO0_SIZE; x++) px[(AO0_SIZE - 1u - y) * AO0_SIZE + x] = ao0_guess565(k, ao0_popcount(bits[y * AO0_SIZE + x]));
        ao0_read(px, bits, edge, 1, &r);
        CHECK(r.bad == 0u, "k %u union, the other way up: bad %u", k, r.bad);
        ao0_read(px, bits, edge, 0, &r);
        CHECK(r.bad > 500u, "k %u: a flipped target read unflipped must be wrong (%u)", k, r.bad);
        for (y = 0; y < AO0_SIZE; y++) for (x = 0; x < AO0_SIZE; x++) px[y * AO0_SIZE + x] = ao0_guess565(k, count[y * AO0_SIZE + x]);   /* plain blending */
        ao0_read(px, bits, edge, 0, &r);
        CHECK(r.bad > 100u && r.colours > r.levels, "k %u plain: bad %u colours %u levels %u", k, r.bad, r.colours, r.levels);
    }
    for (k = 1; k <= 8u; k *= 2u)                                  /* the levels stay apart in 565 */
        for (i = 1; i <= k; i++) CHECK(ao0_guess565(k, i) != ao0_guess565(k, i - 1u), "k %u: levels %u and %u are one 565 value", k, i - 1u, i);
    if (fails) { printf("ao0_model_test: %d FAILED\n", fails); return 1; }
    printf("ao0_model_test: all passed\n");
    return 0;
}
