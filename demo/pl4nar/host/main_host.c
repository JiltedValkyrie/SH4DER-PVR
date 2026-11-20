/* Host preview: the same demo code over the software back end. Drives a scripted lap and writes PPM frames.
 *   preview <out dir>
 * Frames: NN_on.ppm (reflection on), NN_off.ppm (off), NN_target.ppm (the reflection target of the "on" frame). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../src/demo.h"
#include "../src/gfx.h"
#include "gfx_host.h"

uint64_t plat_us(void) { return (uint64_t)clock() * 1000000ull / CLOCKS_PER_SEC; }

static void shot(const char *dir, int n, const demo_stats_t *st) {
    char p[512];
    demo_input_t in;
    memset(&in, 0, sizeof in);
    demo_draw(st);
    snprintf(p, sizeof p, "%s/%02d_on.ppm", dir, n); gfx_host_save(p, 0);
    snprintf(p, sizeof p, "%s/%02d_target.ppm", dir, n); gfx_host_save(p, 1);
    in.pressed = DEMO_BTN_REFLECT; demo_update(&in, 0.0f);
    demo_draw(st);
    snprintf(p, sizeof p, "%s/%02d_off.ppm", dir, n); gfx_host_save(p, 0);
    demo_update(&in, 0.0f);
}

int main(int argc, char **argv) {
    const char *dir = argc > 1 ? argv[1] : ".";
    demo_stats_t st = { 2100u, 5200u, 900u, 600u, 11800u, 4200u };
    demo_input_t in;
    int f, n = 0;
    gfx_init();
    demo_init();
    memset(&in, 0, sizeof in);
    for (f = 0; f < 60 * 14; f++) {
        in.gas = 1.0f; in.brake = 0.0f; in.pressed = 0u;
        in.steer = (f > 200 && f < 250) ? 0.8f : (f > 330 && f < 400) ? -0.8f : (f > 520 && f < 560) ? 0.7f : 0.0f;
        in.orbit = (f > 600 && f < 700) ? 1.0f : 0.0f;
        demo_update(&in, 1.0f / 60.0f);
        if (f == 30 || f == 150 || f == 240 || f == 380 || f == 470 || f == 690 || f == 800) shot(dir, n++, &st);
    }
    /* night: Y twice (AUTO -> DAY -> NIGHT), let the clock get there, then two more shots */
    in.gas = 0.6f; in.steer = 0.0f; in.orbit = 0.0f;
    in.pressed = DEMO_BTN_TOD; demo_update(&in, 0.0f); demo_update(&in, 0.0f);
    for (f = 0; f < 60 * 5; f++) {
        in.pressed = 0u;
        in.orbit = (f > 200) ? 1.0f : 0.0f;
        demo_update(&in, 1.0f / 60.0f);
        if (f == 150 || f == 270) shot(dir, n++, &st);
    }
    /* day again, then the look switched to full size for one pair */
    in.pressed = DEMO_BTN_TOD; demo_update(&in, 0.0f); demo_update(&in, 0.0f); demo_update(&in, 0.0f);
    in.orbit = 0.0f;
    for (f = 0; f < 60 * 4; f++) { in.pressed = 0u; demo_update(&in, 1.0f / 60.0f); }
    shot(dir, n++, &st);
    in.pressed = DEMO_BTN_LOOK; demo_update(&in, 0.0f);
    shot(dir, n++, &st);
    printf("frames %d\n", n);
    return 0;
}
