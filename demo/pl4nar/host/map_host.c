/* Host preview of the new map (PL4NAR_MAP, DEMO_HOST): the same demo code over the software back end.
 *   map_host <out dir>
 * Part 1, a lap: the car driven round the loop by its own steering (it holds lane -2), its offset printed; a car that
 * leaves the road fails. Part 2, shots: NN_on.ppm, NN_off.ppm, NN_target.ppm, NN_noao.ppm (on, the baked occlusion
 * off); shots.txt names them.
 * Cameras: 0 the demo's chase camera; 2 low behind (6.5 m back at 2 m, 12 m ahead at 2.6 m); 3 low in front, off to the
 * side, at the hood; 4 low behind along the roof. */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../src/demo.h"
#include "../src/gfx.h"
#include "../src/map.h"
#include "gfx_host.h"
#include "map_gen.h"

void demo_host_pose(float z, float x, float orbit, float tod, int reflect);
void demo_host_cam(float ex, float ey, float ez, float ax, float ay, float az);
void demo_host_car(float *x, float *z, float *yaw);
#ifdef PL4NAR_HILLS
float demo_host_car_y(void);
#endif
uint64_t plat_us(void) { return (uint64_t)clock() * 1000000ull / CLOCKS_PER_SEC; }

typedef struct shot { const char *name; float sl[2]; float orbit, tod; int cam; } shot_t;

static void set(const shot_t *s, int reflect) {
    float x, z, h, sh, ch;
    demo_host_pose(s->sl[0], s->sl[1], s->orbit, s->tod, reflect);
    demo_host_car(&x, &z, &h);
    sh = sinf(h); ch = cosf(h);
    if (s->cam == 2) demo_host_cam(x - sh * 6.5f, 2.0f, z - ch * 6.5f, x + sh * 12.0f, 2.6f, z + ch * 12.0f);
    if (s->cam == 3) demo_host_cam(x + sh * 5.5f + ch * 2.5f, 1.25f, z + ch * 5.5f - sh * 2.5f, x + sh * 0.8f, 0.85f, z + ch * 0.8f);
    if (s->cam == 4) demo_host_cam(x - sh * 4.5f, 1.7f, z - ch * 4.5f, x + sh * 3.0f, 1.0f, z + ch * 3.0f);
#ifdef PL4NAR_HILLS                                 /* the hills: the camera's heights over the car's ground */
    {
        float gy = demo_host_car_y();
        if (s->cam == 5) demo_host_cam(x - ch * 9.0f, gy + 0.9f, z + sh * 9.0f, x, gy + 0.6f, z);                       /* low, from the side: the wheels on the road */
        if (s->cam == 6) demo_host_cam(x - sh * 45.0f, gy + 16.0f, z - ch * 45.0f, x + sh * 40.0f, gy, z + ch * 40.0f);   /* high, wide: the road's rise and fall */
        if (s->cam == 7) demo_host_cam(x - sh * 9.0f, gy + 1.4f, z - ch * 9.0f, x + sh * 30.0f, gy + 0.8f, z + ch * 30.0f);   /* low behind: the road ahead */
    }
#endif
}

/* The lap: steer toward lane -2 from the car's own offset (a driver, not the demo's input), 1 s steps printed. */
static int lap(void) {
    demo_input_t in;
    map_pose_t p;
    float lat = -2.0f, s = 8.0f, x, z, h, worst = 0.0f;
    int f, bad = 0;
    memset(&in, 0, sizeof in);
    demo_host_pose(8.0f, -2.0f, 0.0f, 0.15f, 1);
    for (f = 0; f < 60 * 100; f++) {
        demo_host_car(&x, &z, &h);
        /* the car's offset: from the nearest place on the centre line, found by walking s forward */
        {
            float best = 1e9f, bs = s, k;
            for (k = -6.0f; k <= 30.0f; k += 0.5f) {
                const map_pose_t q = map_at(s + k, 0.0f);
                const float d = (x - q.x) * (x - q.x) + (z - q.z) * (z - q.z);
                if (d < best) { best = d; bs = s + k; }
            }
            s = map_wrap(bs);
            p = map_at(s, 0.0f);
            lat = (x - p.x) * cosf(p.yaw) - (z - p.z) * sinf(p.yaw);
        }
        in.gas = 0.7f;
        in.steer = fmaxf(-1.0f, fminf(1.0f, (lat + 2.0f) * 0.6f));
        demo_update(&in, 1.0f / 60.0f);
        if (fabsf(lat) > worst) worst = fabsf(lat);
        if (fabsf(lat) > 8.0f) bad = 1;
        if (f % 600 == 0) printf("  lap t %3d s: s %7.1f lat %+5.2f world %7.1f %7.1f\n", f / 60, s, lat, x, z);
    }
    printf("LAP %s: worst offset %.2f m (road band %.0f)\n", bad ? "FAIL" : "OK", worst, MAP_BAND);
    return bad;
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    static const shot_t shots[] = {
        { "a under the overpass, night, low camera in front, at the hood", { MAP_SHOT_OVERPASS_SL }, 0.0f, 0.70f, 3 },
        { "a under the overpass, night, low camera behind, along the roof", { MAP_SHOT_OVERPASS_SL }, 0.0f, 0.70f, 4 },
        { "a under the overpass, night, the demo's chase camera", { MAP_SHOT_OVERPASS_SL }, 0.0f, 0.70f, 0 },
        { "a under the overpass, day, low camera in front, at the hood", { MAP_SHOT_OVERPASS_SL }, 0.0f, 0.15f, 3 },
        { "b in the covered passage, night, low camera in front, at the hood", { MAP_SHOT_PASSAGE_SL }, 0.0f, 0.70f, 3 },
        { "b in the covered passage, night, low camera behind, along the roof", { MAP_SHOT_PASSAGE_SL }, 0.0f, 0.70f, 4 },
        { "b in the covered passage, night, the demo's chase camera", { MAP_SHOT_PASSAGE_SL }, 0.0f, 0.70f, 0 },
        { "c in the open, day, the demo's chase camera", { MAP_SHOT_OPEN_SL }, 0.0f, 0.15f, 0 },
        { "c in the open, day, low camera in front, at the hood", { MAP_SHOT_OPEN_SL }, 0.0f, 0.15f, 3 },
        { "c in the open, dusk, the chase camera turned to the side", { MAP_SHOT_OPEN_SL }, 1.6f, 0.45f, 0 },
        { "d a corner, day, the chase camera (the loop's curve)", { 200.0f, -2.0f }, 0.0f, 0.15f, 0 },
        { "a under the overpass, day, the demo's chase camera", { MAP_SHOT_OVERPASS_SL }, 0.0f, 0.15f, 0 },
        { "b in the covered passage, day, the demo's chase camera", { MAP_SHOT_PASSAGE_SL }, 0.0f, 0.15f, 0 },
        { "a before the overpass, day, the chase camera 20 m back", { 97.0f, 2.0f }, 0.0f, 0.15f, 0 },
        { "c in the open, day, low camera in front, the split at the middle (layer on the left)", { MAP_SHOT_OPEN_SL }, 0.0f, 0.15f, 3 },
#ifdef PL4NAR_HILLS
        { "e hills: south side, day, low from the side (the wheels on the road)", { 900.0f, -2.0f }, 0.0f, 0.15f, 5 },
        { "e hills: south side, day, high and wide (the road's rise and fall)", { 900.0f, -2.0f }, 0.0f, 0.15f, 6 },
        { "e hills: east side, day, high and wide, toward the overpass (the flat ground there)", { 40.0f, -2.0f }, 0.0f, 0.15f, 6 },
        { "e hills: north side, day, low behind (the road ahead)", { 420.0f, -2.0f }, 0.0f, 0.15f, 7 },
        { "e hills: west side, dusk, low from the side", { 800.0f, -2.0f }, 0.0f, 0.45f, 5 },
        { "e hills: a corner, day, high and wide", { 330.0f, -2.0f }, 0.0f, 0.15f, 6 },
        { "f the steepest stretch, day, the chase camera", { 925.7f, -2.0f }, 0.0f, 0.15f, 0 },
        { "f the steepest stretch, day, low from the side", { 925.7f, -2.0f }, 0.0f, 0.15f, 5 },
        { "f the steepest stretch, day, the chase camera turned to the side", { 925.7f, -2.0f }, 1.3f, 0.15f, 0 },
        { "g across the crest, day, low behind", { 30.0f, -2.0f }, 0.0f, 0.15f, 7 },
#endif
    };
    demo_stats_t st = { 0, 0, 0, 0, 0, 0 };
    char p[512];
    FILE *f;
    unsigned k;
    int bad;
    gfx_init();
    demo_init();
    bad = lap();
    snprintf(p, sizeof p, "%s/shots.txt", dir);
    f = fopen(p, "w");
    for (k = 0; k < sizeof shots / sizeof shots[0]; k++) {
        const shot_t *s = &shots[k];
        set(s, 1);
        demo_auto_split(strstr(s->name, "split") != NULL);
        demo_draw(&st);
        snprintf(p, sizeof p, "%s/%02u_on.ppm", dir, k); gfx_host_save(p, 0);
        snprintf(p, sizeof p, "%s/%02u_target.ppm", dir, k); gfx_host_save(p, 1);
        set(s, 0);
        demo_draw(&st);
        snprintf(p, sizeof p, "%s/%02u_off.ppm", dir, k); gfx_host_save(p, 0);
        set(s, 1);
        demo_auto_ao(0);
        demo_draw(&st);
        demo_auto_ao(1);
        snprintf(p, sizeof p, "%s/%02u_noao.ppm", dir, k); gfx_host_save(p, 0);
        demo_auto_split(0);
#ifdef PL4NAR_HILLS
        set(s, 1);                                  /* the comparison: TYPICAL (a flat quad at the car's ground height) */
        demo_auto_tiles(2);
        demo_draw(&st);
        demo_auto_tiles(1);
        snprintf(p, sizeof p, "%s/%02u_typ.ppm", dir, k); gfx_host_save(p, 0);
#endif
        if (f) {                                    /* the screen scene's triangles (reflection on), AO on and off */
            demo_counts_t c;
            unsigned on;
            demo_draw(&st); demo_counts(&c); on = c.tris_main;
            demo_auto_ao(0); demo_draw(&st); demo_counts(&c); demo_auto_ao(1);
            fprintf(f, "%02u %s (screen tris, reflection on: AO on %u, off %u)\n", k, s->name, on, c.tris_main);
        }
    }
    if (f) fclose(f);
    printf("MAP_HOST_OK %u shots, lap %s\n", k, bad ? "FAIL" : "OK");
    return bad;
}
