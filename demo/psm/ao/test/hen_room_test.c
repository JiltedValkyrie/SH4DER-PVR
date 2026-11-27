/* SH4DER-PVR: host test of demo/psm/ao/hen_room.h (the hens-eyes entry's hens)

   demo/psm/ao/test/hen_room_test.c
   Copyright (c) 2026 Jilted Valkyrie. See ../../../../LICENSE.

   Compile with -I the folder of room_mesh.h and room_ao.h (build.py --room-only). For each of the four ways of
   steering and 4, 16 and 32 hens it walks a minute and checks: the same run twice gives the same places; every hen
   stays on the floor and out of the furniture's footprints; the hens move; the work is counted. It prints the counts
   a step and what hen_eyes.h's and hen_sweep.h's cost models make of them: ESTIMATES, not measurements. */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "../room_data.h"
#include "../hen_eyes.h"
#include "../hen_sweep.h"
#include "../hen_room.h"

static int fails;
#define CHECK(c, ...) do { if (!(c)) { fails++; printf("FAIL %s:%d: ", __FILE__, __LINE__); printf(__VA_ARGS__); printf("\n"); } } while (0)

static henr_t room, again;
static const char *const mode_name[HENR_MODES] = { "eyes", "sweep grid, every step", "sweep grid, on the tick", "sweep, no broad phase" };

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    static const unsigned counts[3] = { 4u, 16u, 32u };
    hen_cost_t cost;
    hen_sweep_cost_t scost;
    unsigned mode, c, i, k, s;

    hen_cost_default(&cost); hen_sweep_cost_default(&scost);
    CHECK(henr_init(&room), "the room gave no world or the grid does not fit");
    CHECK(henr_init(&again), "the second world");
    printf("  world: %u objects, %u triangles, grid %u x %u with %u items; floor %.2f .. %.2f, %.2f .. %.2f\n", room.objs, room.world.ntri, HENR_GRID, HENR_GRID,
           room.grid_items, (double)room.world.floor_rect[0], (double)room.world.floor_rect[1], (double)room.world.floor_rect[2], (double)room.world.floor_rect[3]);
    for (c = 0; c < 3u; c++)
        for (mode = 0; mode < HENR_MODES; mode++) {
            float moved = 0.0f, x0[HENR_MAX], z0[HENR_MAX], model;
            unsigned off = 0, in = 0, same = 1;

            henr_reset(&room, counts[c], mode); henr_reset(&again, counts[c], mode);
            for (i = 0; i < room.n; i++) { x0[i] = room.h[i].x; z0[i] = room.h[i].z; }
            for (s = 0; s < 3600u; s++) {
                henr_step(&room); henr_step(&again);
                for (i = 0; i < room.n; i++) {
                    const float x = room.h[i].x, z = room.h[i].z;

                    same &= x == again.h[i].x && z == again.h[i].z && room.h[i].bearing == again.h[i].bearing;
                    off += !(x >= room.world.floor_rect[0] && x <= room.world.floor_rect[1] && z >= room.world.floor_rect[2] && z <= room.world.floor_rect[3]);
                    for (k = 0; k < room.world.nobj; k++) {
                        const float *f = room.world.obj[k].foot;

                        if (room.world.obj[k].has_foot && x > f[0] + 0.02f && x < f[1] - 0.02f && z > f[2] + 0.02f && z < f[3] - 0.02f) in++;
                    }
                }
            }
            for (i = 0; i < room.n; i++) moved += sqrtf((room.h[i].x - x0[i]) * (room.h[i].x - x0[i]) + (room.h[i].z - z0[i]) * (room.h[i].z - z0[i]));
            model = mode == HENR_EYES ? hen_stats_cycles(&room.st, &cost) : hen_sweep_cycles(&room.sst, &scost);
            if (mode == HENR_EYES)
                printf("  %2u hens, %-24s a step: ticks %.2f rays %.1f box %.1f tri %.1f pair %.1f; model %.0f cycles (ESTIMATE)\n", room.n, mode_name[mode],
                       (double)room.st.ticks / 3600.0, (double)(room.st.ray_fan + room.st.ray_down + room.st.ray_sight) / 3600.0, (double)room.st.box / 3600.0,
                       (double)room.st.tri / 3600.0, (double)room.st.pair / 3600.0, (double)model / 3600.0);
            else
                printf("  %2u hens, %-24s a step: casts %.2f tri %.1f reject %.1f cell %.1f box %.1f pair %.1f; model %.0f cycles (ESTIMATE)\n", room.n, mode_name[mode],
                       (double)room.sst.casts / 3600.0, (double)room.sst.tri / 3600.0, (double)room.sst.reject / 3600.0, (double)room.sst.cell / 3600.0,
                       (double)room.sst.box / 3600.0, (double)room.sst.pair / 3600.0, (double)model / 3600.0);
            CHECK(same, "%u hens, %s: two runs differ", room.n, mode_name[mode]);
            CHECK(off == 0u, "%u hens, %s: off the floor %u times", room.n, mode_name[mode], off);
            CHECK(in == 0u, "%u hens, %s: inside a footprint %u times", room.n, mode_name[mode], in);
            CHECK(moved > 0.05f * (float)room.n, "%u hens, %s: they did not move (%.2f m in all)", room.n, mode_name[mode], (double)moved);
            CHECK(room.steps == 3600u && room.st.push > 0u && room.st.sep == 3600ul * (room.n * (room.n - 1u) / 2u), "%u hens, %s: the counts", room.n, mode_name[mode]);
            CHECK(mode == HENR_EYES ? room.st.ticks > 0u && room.st.tri > 0u : room.sst.casts > 0u, "%u hens, %s: no work counted", room.n, mode_name[mode]);
        }
    {                                       /* the first four are ao4's hens: the same start */
        henr_reset(&room, 4u, HENR_EYES);
        CHECK(fabsf(room.h[0].x - 0.2f) < 1e-5f && fabsf(room.h[0].z - 1.15f) < 1e-5f && fabsf(room.h[2].x - (-1.4f)) < 1e-5f && fabsf(room.h[2].z - (-0.05f)) < 1e-5f, "the start");
        CHECK(fabsf(room.h[0].b[0] - (-0.65f)) < 1e-5f && fabsf(room.h[0].b[1] - 2.05f) < 1e-5f && fabsf(room.h[0].b[2] - 0.85f) < 1e-5f && fabsf(room.h[0].b[3] - 2.05f) < 1e-5f, "the pen");
    }
    if (fails) { printf("hen_room_test: %d FAILED\n", fails); return 1; }
    printf("hen_room_test: all passed\n");
    return 0;
}
