/* The autorun never snaps the car: the stage changes main.c makes (demo_auto_set and the switches, no demo_auto_place
 * after the first), run frame by frame at 30 Hz through a day / night / size / orbit / lod sequence. Each frame the car's
 * world step must be at most its speed x dt (+ 5 cm), its heading step under 0.05 rad, and the camera orbit's step under
 * 0.05 rad (its eased return, never a jump). Built with DEMO_HOST, any of the four builds (street, -DPL4NAR_MAP,
 * -DPL4NAR_MAP -DPL4NAR_HILLS, -DPL4NAR_WET). The street's wrap by its period is not a jump (the same picture).
 * Not the wet build: a fly-by stage (demo_auto_cine) too. Its first frame is a cut to the kerb camera on purpose; from
 * then on the drawn eye must move under 2 m a frame (60 m/s), through the hold and the ease back to the follow camera.
 * The heat (the owner's rule): drawn only while the eye stands still. Every frame is drawn; a frame with heat and an
 * eye that moved fails. The fly-by must show heat at least 5 s; then the car stopped with X on LIGHT: heat within 1 s,
 * and gone the first frame the car (and the camera) moves off. Not checked in the wet build (rain: no heat). */
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "../src/demo.h"
#include "../src/gfx.h"
#include "gfx_host.h"
#include "sh4der_pool.h"                 /* SH4DER_POOL_BEAM_ROWS, for the beam's line */

uint64_t plat_us(void) { return (uint64_t)clock() * 1000000ull / CLOCKS_PER_SEC; }
void  demo_host_car(float *x, float *z, float *yaw);
float demo_host_orbit(void);
void  demo_host_eye(float *x, float *z);
float demo_host_heat(void);

static float wrap_pi(float a) { while (a > 3.14159265f) a -= 6.2831853f; while (a < -3.14159265f) a += 6.2831853f; return a; }

int main(int argc, char **argv) {
    static const struct { unsigned size, tod; int orbit; float lod; } seq[] = {   /* main.c's kinds of change, 7 s each; the second: the fly-by */
        { 1, 1, 0, 0.0f }, { 0, 1, 0, 0.0f }, { 0, 1, 0, 0.0f }, { 2, 1, 0, 0.0f }, { 0, 2, 0, 0.0f }, { 1, 2, 0, 0.0f },
        { 0, 1, 1, 0.0f }, { 0, 1, 0, 30.0f }, { 0, 2, 1, 0.0f }, { 1, 1, 0, 0.0f }
    };
    const float dt = 1.0f / 30.0f;
    demo_stats_t st = { 2100u, 5200u, 900u, 600u, 11800u, 4200u };
    demo_input_t in;
    float px, pz, ph, po, ex = 0.0f, ez = 0.0f, worst = 0.0f, eye_worst = 0.0f, heat_s = 0.0f;
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
#ifndef PL4NAR_WET
            if (k == 1u) demo_auto_cine();
#endif
        }
        for (f = 0; f < 7u * 30u; f++, frames++) {
            float x, z, h, o, d, lim;
            const float v = demo_speed();
            in.gas = v < 14.0f ? 1.0f : 0.0f;
            demo_update(&in, dt);
            demo_draw(&st);                          /* every frame: the eye and the heat are checked */
            {
                float nx, nz, de;
                const int cut = k == 1u && f == 0u;   /* the fly-by's cut to the kerb camera, on purpose */
                demo_host_eye(&nx, &nz);
                de = sqrtf((nx - ex) * (nx - ex) + (nz - ez) * (nz - ez));
#ifndef PL4NAR_MAP
                if (de > 100.0f) de = 0.0f;           /* the street's wrap: the same picture */
#endif
                if (frames && !cut && k >= 1u && k <= 2u && de > eye_worst) eye_worst = de;
                if (frames && !cut && de > 2.0f) { if (bad++ < 8u) printf("stage %u frame %u: the eye jumped %.2f m\n", k + 1u, f, (double)de); }
                if (demo_host_heat() > 0.0f && de > 1e-4f) { if (bad++ < 8u) printf("stage %u frame %u: heat drawn while the eye moved %.3f m\n", k + 1u, f, (double)de); }
                if (k >= 1u && k <= 2u && demo_host_heat() > 0.0f) heat_s += dt;
                ex = nx; ez = nz;
            }
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
    printf("fly-by: the eye's largest step %.2f m a frame, heat shown %.1f s\n", (double)eye_worst, (double)heat_s);
#ifndef PL4NAR_WET
    if (heat_s < 5.0f) { printf("the fly-by showed heat %.1f s, under 5\n", (double)heat_s); bad++; }
    {   /* the follow camera: the car stopped, X on LIGHT; heat comes once still, goes at the first move */
        float still_s = -1.0f;
        demo_auto_heat(1u);
        demo_auto_place(300.0f, -2.0f, 0.0f);
        in.gas = 0.0f;
        for (f = 0; f < 60u; f++) { demo_update(&in, dt); demo_draw(&st); if (still_s < 0.0f && demo_host_heat() > 0.0f) still_s = (float)f * dt; }
        printf("stopped: heat after %.2f s\n", (double)still_s);
        if (still_s < 0.0f || still_s > 1.0f) { printf("no heat within 1 s of a still camera\n"); bad++; }
        in.gas = 1.0f;
        demo_host_eye(&ex, &ez);
        for (f = 0; f < 30u; f++) {
            float nx, nz;
            demo_update(&in, dt); demo_draw(&st); demo_host_eye(&nx, &nz);
            if ((nx != ex || nz != ez) && demo_host_heat() > 0.0f) { printf("driving off: heat on frame %u with the eye moving\n", f); bad++; break; }
            ex = nx; ez = nz;
        }
    }
#endif
#ifndef PL4NAR_WET
    {   /* the autorun's two headlight stages (main.c, the street: "headlight fade", "headlight plain"): night, the far
           shot (demo_auto_cine_far) with the beam on, then the same shot with it off, the car driving on between
           them (no demo_auto_place after the first, as main.c). Through each shot's hold the eye must not move at
           all, and the car must never step more than its speed allows. The triangles sent are counted. Not a
           picture test. */
        unsigned long sent[2] = { 0ul, 0ul };
        unsigned most[2] = { 0u, 0u }, pass, moved = 0u;
        float cx, cz, ch;
        (void)demo_auto_set(1, 0, 2, 0);
        demo_auto_place(argc > 1 ? (float)atof(argv[1]) : 60.0f, -2.0f, 14.0f);
        demo_host_car(&cx, &cz, &ch);
        for (pass = 0; pass < 2u; pass++) {
            float hx = 0.0f, hz = 0.0f;
            (void)demo_auto_set(1, 0, 2, 0);
            demo_auto_cine_far();
            demo_auto_beam(pass == 0u);
            for (f = 0; f < 7u * 30u; f++) {
                float nx, nz, x, z, h, d, lim;
                const float v = demo_speed();
                unsigned n;
                in.gas = v < 14.0f ? 1.0f : 0.0f;
                demo_update(&in, dt); demo_draw(&st);
                n = gfx_tris_sent();
                sent[pass] += n;
                if (n > most[pass]) most[pass] = n;
                demo_host_eye(&nx, &nz);
                if (f >= 1u && f < 175u && (nx != hx || nz != hz)) { if (moved++ < 4u) printf("headlight stage %u frame %u: the still camera moved\n", pass + 1u, f); }
                hx = nx; hz = nz;
                demo_host_car(&x, &z, &h);
#ifndef PL4NAR_MAP
                {
                    demo_counts_t c;
                    float per;
                    demo_counts(&c);
                    per = (float)(c.draw_back + c.draw_fwd);
                    if (z - cz > 0.5f * per) cz += per;
                    if (cz - z > 0.5f * per) cz -= per;
                }
#endif
                d = sqrtf((x - cx) * (x - cx) + (z - cz) * (z - cz));
                lim = (v > demo_speed() ? v : demo_speed()) * dt + 0.05f;
                if (d > lim) { if (bad++ < 8u) printf("headlight stage %u frame %u: the car moved %.2f m (speed allows %.2f)\n", pass + 1u, f, (double)d, (double)lim); }
                cx = x; cz = z;
            }
        }
        demo_auto_beam(0);
        bad += moved;
        printf("headlight stages (beam rows %u), night, the far shot, 210 frames each: triangles sent with the fade %lu (most a frame %u), without %lu (most %u)\n",
               (unsigned)SH4DER_POOL_BEAM_ROWS, sent[0], most[0], sent[1], most[1]);
        if (!sent[0] || !sent[1]) { printf("a headlight stage drew nothing\n"); bad++; }
    }
#endif
    printf(bad ? "SNAP_TEST FAIL (%u)\n" : "SNAP_TEST_OK %u frames, %u stages, no jump\n", bad ? bad : frames, (unsigned)(sizeof seq / sizeof seq[0]));
    return bad != 0u;
}
