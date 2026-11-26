/* The autorun never snaps the car: the stage changes main.c makes (demo_auto_set and the switches, no demo_auto_place
 * after the first), run frame by frame at 30 Hz through a day / night / size / orbit / lod sequence. Each frame the car's
 * world step must be at most its speed x dt (+ 5 cm), its heading step under 0.05 rad, and the camera orbit's step under
 * 0.05 rad (its eased return, never a jump). Built with DEMO_HOST, any of the four builds (street, -DPL4NAR_MAP,
 * -DPL4NAR_MAP -DPL4NAR_HILLS, -DPL4NAR_WET). The street's wrap by its period is not a jump (the same picture). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../src/demo.h"
#include "../src/gfx.h"
#include "gfx_host.h"

uint64_t plat_us(void) { return (uint64_t)clock() * 1000000ull / CLOCKS_PER_SEC; }
void  demo_host_car(float *x, float *z, float *yaw);
float demo_host_orbit(void);

static float wrap_pi(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }

int main(int argc, char **argv) {
    static const struct { unsigned size, tod; int orbit; float lod; } seq[] = {   /* main.c's kinds of change, 7 s each */
        { 1, 1, 0, 0.0f }, { 0, 1, 0, 0.0f }, { 2, 1, 0, 0.0f }, { 0, 2, 0, 0.0f }, { 1, 2, 0, 0.0f },
        { 0, 1, 1, 0.0f }, { 0, 1, 0, 30.0f }, { 0, 2, 1, 0.0f }, { 1, 1, 0, 0.0f }
    };
    const float dt = 1.0f / 30.0f;
    demo_stats_t st = { 2100u, 5200u, 900u, 600u, 11800u, 4200u };
    demo_input_t in;
    float px, pz, ph, po, worst = 0.0f;
    unsigned k, f, bad = 0u, frames = 0u;
    gfx_init(); demo_init();
    memset(&in, 0, sizeof in);
    (void)demo_auto_set(1, seq[0].size, seq[0].tod, seq[0].orbit);
    demo_auto_place(argc > 1 ? (float)atof(argv[1]) : 60.0f, -2.0f, 14.0f);   /* once, as main.c does; the hills autorun: 880 */
    demo_host_car(&px, &pz, &ph); po = demo_host_orbit();
    for (k = 0; k < sizeof seq / sizeof seq[0]; k++) {
        if (k) {                                     /* a stage change: what main.c calls, and nothing that places the car */
            (void)demo_auto_set(1, seq[k].size, seq[k].tod, seq[k].orbit);
#ifdef PL4NAR_MAP
            demo_auto_lod(seq[k].lod);
#endif
        }
        for (f = 0; f < 7u * 30u; f++, frames++) {
            float x, z, h, o, d, lim;
            const float v = demo_speed();
            in.gas = v < 14.0f ? 1.0f : 0.0f;
            demo_update(&in, dt);
            if ((f & 15u) == 0u) demo_draw(&st);
            demo_host_car(&x, &z, &h); o = demo_host_orbit();
#ifndef PL4NAR_MAP                                  /* the street repeats every PERIOD (draw back + forward): its wrap
                                                      is the same picture, drawn round the car, not a jump */
            {
                demo_counts_t c;
                float per;
                demo_counts(&c);
                per = (float)(c.draw_back + c.draw_fwd);
                if (z - pz > 0.5f * per) pz += per;
                if (pz - z > 0.5f * per) pz -= per;
            }
#endif
            d = sqrtf((x - px) * (x - px) + (z - pz) * (z - pz));
            lim = (v > demo_speed() ? v : demo_speed()) * dt + 0.05f;
            if (d - lim > worst) worst = d - lim;
            if (d > lim) { if (bad++ < 8u) printf("stage %u frame %u: the car moved %.2f m (speed allows %.2f)\n", k + 1u, f, (double)d, (double)lim); }
            if (fabsf(wrap_pi(h - ph)) > 0.05f) { if (bad++ < 8u) printf("stage %u frame %u: heading jumped %.3f rad\n", k + 1u, f, (double)wrap_pi(h - ph)); }
            if (fabsf(wrap_pi(o - po)) > 0.05f) { if (bad++ < 8u) printf("stage %u frame %u: camera orbit jumped %.3f rad\n", k + 1u, f, (double)wrap_pi(o - po)); }
            px = x; pz = z; ph = h; po = o;
        }
    }
    printf(bad ? "SNAP_TEST FAIL (%u)\n" : "SNAP_TEST_OK %u frames, %u stages, no jump\n", bad ? bad : frames, (unsigned)(sizeof seq / sizeof seq[0]));
    return bad != 0u;
}
