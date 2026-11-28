/* SH4DER-PVR: cloud fly-through demo (sh4der_cloudvolume.h)

   demo/cloudvolume.c
   Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.

   Two programs in one file, as demo/ocean.c.

   The Dreamcast program (make cloudvolume SH4ZAM=...): the clouds marched into the small buffer every frame, drawn as one filtered quad over a
   PVR sky (gradient mesh, a high streak layer, the sun), with the ocean demo's water under them. A readout and rows of settings, handled as the
   ocean demo's: Y readout, D-pad left / right a row, up / down a step, A a step that wraps, LT / RT height, stick look, X pause, Start reset.
   Rows: PRESET, RESOLUTION, STEPS, LIGHT SAMPLES, DETAIL, DENSITY, COVERAGE, SPREAD, SMOOTH, JITTER, FILTER, OUTPUT (PAL8 / RGB565), FRAME LOCK,
   HIGH LAYER, SUN GLOW, ROLL, SEA (OFF / A scaled ocean / B flat / C low cloud), REFLECTION, WATER TRIANGLES, SUN AZIMUTH, SUN ELEVATION, SPEED.
   NOT in it yet: the clouds mirrored in the water through sh4der_pl4nar (the water mirrors the sky and the sun only).

   The host preview (make cloudvolume-preview) runs the same march, then does on the host what the PVR does: looks the
   bytes up in the palette, filters the buffer up to 640 x 480 as one quad turned by the camera's roll, over a sky with
   a sun, a high streak layer and a sea far below (a stand-in for the ocean scene). Output: binary PPM.

     cloudvolume-preview out.ppm [key=value ...]
       t=12.5 (seconds)  res=128x96  steps=16  light=0  detail=1  density=100  coverage=60  spread=4  frames=1
       jitter=1  jturn=1  smooth=1  filter=1  high=1  ground=1  refl=1  roll=1  out=pal8 | 565 | ideal  speed=150  az=8  el=12
       alt=-1 (metres, -1 = the path's)  pitch=-99 (degrees, -99 = the path's)  preset=performant | quality | cinema
   frames=N marches N frames 1/24 s apart ending at t (so spread 2 and 4 show what they leave behind).
   It prints the march's counters and a cycle ESTIMATE (not a measurement).
*/

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../include/sh4der_cloudvolume.h"

#ifdef _arch_dreamcast
/* ------------------------------------------------------------------------------------------------------------------ */
/* The Dreamcast program. Rows as demo/ocean.c: Y shows / hides the readout (shown at start), D-pad left / right picks a
 * row, up / down steps it (stops at the ends), A steps up and wraps, LT / RT lower / raise the camera, the stick looks,
 * X pauses the time, Start resets the view. */
#include <kos.h>
#include <dc/minifont.h>
#include <dc/vblank.h>
#include "sh4der_water.h"
#include "sh4der_guard.h"
#include "sh4der_pvrx.h"
#include "sh4der_suite.h"
#include "sh4der_waitrun.h"
static sh4der_waitrun_t wr_run;

/* The sea under the clouds is the ocean demo's water (ocean_scene.h), with two things of its own, set before the include:
 * its sky colours (so the water mirrors THIS sky) and its wave table. */
static const sh4der_water_wave_t cv_waves_far[] = {   /* SEA A: seen from high up, the swell kept low (0.3 of the ocean demo's heights) */
    {0.28f,0.96f,0.039f,15.0f,4.84f,0.2f,0},
    {-0.60f,0.80f,0.0225f,9.0f,3.75f,1.7f,0},
    {0.80f,0.60f,0.0114f,5.4f,2.90f,4.0f,0},
    {-0.96f,0.28f,0.006f,3.2f,2.23f,2.2f,0},
    {0.28f,0.96f,0.0070f,0.78f,1.10f,0.8f,1},
    {-0.60f,0.80f,0.0038f,0.43f,0.82f,3.2f,1},
    {0.80f,0.60f,0.0017f,0.25f,0.62f,5.3f,1}
};
static const sh4der_water_wave_t cv_waves_near[] = {  /* SEA C: the ocean demo's own waves */
    {0.28f,0.96f,0.130f,15.0f,4.84f,0.2f,0},
    {-0.60f,0.80f,0.075f,9.0f,3.75f,1.7f,0},
    {0.80f,0.60f,0.038f,5.4f,2.90f,4.0f,0},
    {-0.96f,0.28f,0.020f,3.2f,2.23f,2.2f,0},
    {0.28f,0.96f,0.0070f,0.78f,1.10f,0.8f,1},
    {-0.60f,0.80f,0.0038f,0.43f,0.82f,3.2f,1},
    {0.80f,0.60f,0.0017f,0.25f,0.62f,5.3f,1}
};
static const sh4der_water_wave_t *cv_waves = cv_waves_far;
static void cv_sky_rgb(const float d[3], const float sun[3], float col[3]);
#define OCEAN_WAVE_SET cv_waves
#define OCEAN_ENV_RGB(d, sun, rgb) cv_sky_rgb(d, sun, rgb)
#include "ocean_scene.h"

KOS_INIT_FLAGS(INIT_DEFAULT);
_Static_assert(sizeof(ocean_vertex_t) == sizeof(pvr_vertex_t), "PVR ABI");

#define CV_FOCAL   OCEAN_FOCAL                  /* pixels: the ocean scene's, so the sea and the clouds share one view */
#define CV_TAN_X   (320.0f / CV_FOCAL)
#define CV_TAN_Y   (240.0f / CV_FOCAL)
#define CV_OVERSCAN 1.12f                       /* the buffer sees this much more than the screen, so the rolled quad still covers it */
#define CV_MAX_W   256
#define CV_MAX_H   192
#define CV_TEX     256                          /* side of the largest cloud texture */
#define CV_SKY_X   24
#define CV_SKY_Y   18
#define CV_HUD_H   128
#define CV_STREAK  128                          /* side of the high layer's texture */
#define CV_SEA_DROP 1400.0f                     /* SEA A / B: the water lies this far under the cloud base, metres (for the fog of SEA B) */
#define CV_LOW_BASE 150.0f                      /* SEA C: the cloud base over the water, metres */
#define CV_FRAME_US 41667u                      /* 24 frames a second */
#define CV_VBL_US   16683u                      /* one 59.94 Hz field */

enum { ROW_PRESET, ROW_RES, ROW_STEPS, ROW_LIGHT, ROW_DETAIL, ROW_DENSITY, ROW_COVERAGE, ROW_SPREAD, ROW_SMOOTH, ROW_JITTER, ROW_FILTER,
       ROW_OUTPUT, ROW_LOCK, ROW_HIGH, ROW_SUNGLOW, ROW_ROLL, ROW_SEA, ROW_REFLECT, ROW_MESH, ROW_SUN_AZ, ROW_SUN_EL, ROW_SPEED, ROWS };
static const char *names[ROWS] = { "PRESET", "RESOLUTION", "STEPS", "LIGHT SAMPLES", "DETAIL", "DENSITY", "COVERAGE", "SPREAD", "SMOOTH", "JITTER", "FILTER",
       "OUTPUT", "FRAME LOCK", "HIGH LAYER", "SUN GLOW", "ROLL", "SEA", "REFLECTION", "WATER TRIANGLES", "SUN AZIMUTH", "SUN ELEVATION", "SPEED" };
enum { SEA_OFF, SEA_A, SEA_B, SEA_C };
enum { OUTP_PAL8, OUTP_565 };
enum { LOCK_AUTO, LOCK_30, LOCK_20, LOCK_15, LOCK_FREE };
static const int counts[ROWS] = { 3, 6, 7, 5, 3, 8, 8, 3, 2, 3, 2, 2, 5, 2, 2, 2, 4, 2, OCEAN_MESH_STEPS, OCEAN_SUN_AZ_STEPS, OCEAN_SUN_EL_STEPS, 9 };
static const int res_w[6] = { 64, 96, 128, 160, 192, 256 }, res_h[6] = { 48, 72, 96, 120, 144, 192 }, res_tex[6] = { 64, 128, 128, 256, 256, 256 };
static const int step_list[7] = { 8, 12, 16, 24, 32, 48, 64 };
static const int spread_list[3] = { 1, 2, 4 };
typedef struct { const char *name; int res, steps, light, detail, spread; } cv_preset_t;
#define PRESETS 3
static const cv_preset_t presets[PRESETS] = {
    { "PERFORMANT", 1, 1, 0, 1, 2 },            /* 96 x 72, 12 steps, baked light, 1 octave, spread 4 */
    { "QUALITY",    2, 2, 0, 1, 1 },            /* 128 x 96, 16 steps, baked light, 1 octave, spread 2: the start state */
    { "CINEMA",     5, 5, 3, 2, 0 } };          /* 256 x 192, 48 steps, 3 light samples, 2 octaves, every pixel every frame */
/* the start state: QUALITY, jitter FIXED, lock AUTO, sky at its best (high layer, sun glow), roll on, SEA A with reflection, 3,000 water triangles,
 * the sun 9.8 degrees right and 12 degrees up, 150 m/s */
static int val[ROWS] = { 1, 2, 2, 0, 1, 3, 4, 1, 1, 1, 1, OUTP_PAL8, LOCK_AUTO, 1, 1, 1, SEA_A, 1, OCEAN_MESH_STEP_FOR(3000), OCEAN_SUN_AZ_ORIGINAL, 5, 3 };
static int hud = 1, paused, pick = ROW_PRESET, stats_reset, hud_dirty = 1;   /* hud_dirty: the readout's text changed, draw and upload it again */

static sh4der_cloudvolume_maps_t maps __attribute__((aligned(32)));
static sh4der_cloudvolume_t clouds;
static sh4der_cloudvolume_frame_t frame;
static sh4der_cloudvolume_view_t view;
static uint8_t shade_m[CV_MAX_W * CV_MAX_H], alpha_m[CV_MAX_W * CV_MAX_H], shade_s[CV_MAX_W * CV_MAX_H], alpha_s[CV_MAX_W * CV_MAX_H];
static uint8_t tex8[CV_TEX * CV_TEX] __attribute__((aligned(32)));
static uint16_t tex16[CV_TEX * CV_TEX] __attribute__((aligned(32)));
static uint16_t streak_pixels[CV_STREAK * CV_STREAK] __attribute__((aligned(32)));
static uint16_t hud_pixels[512 * CV_HUD_H] __attribute__((aligned(32)));
static uint32_t twid[CV_TEX];                   /* a coordinate's bits spread to the even places: KOS texture order is twid[y] | twid[x] << 1 */
static uint8_t cloud_rgb[256][3];               /* the cloud ramp as bytes, for the RGB565 output */
static pvr_ptr_t tex8_vram, tex16_vram, streak_vram, env_vram, hud_vram;
static pvr_poly_hdr_t sky_header, water_header, hud_header, streak_header, sun_header, cloud_header;
static ocean_vertex_t sky_mesh[(CV_SKY_X + 1) * (CV_SKY_Y + 1)] __attribute__((aligned(32)));
static float sky_dir[(CV_SKY_X + 1) * (CV_SKY_Y + 1)][3];
static ocean_settings_t sea = { 0, 0, 0, 1, 1, 0, 1, 0, 0 };   /* swell and ripple on, transmission off, reflection on, SH4DOW off, bloom off */
static float fly_time, fly_z, look_yaw, look_pitch, lift, low_eye = 4.0f, sun_through = 1.0f;
static unsigned bake_row = SH4DER_CLOUDVOLUME_MAP, env_row = OCEAN_ENV, env_upload;   /* rows still to rebuild after a sun or coverage change */
static float env_sun[3];
static volatile uint32_t vbl_count;
static uint32_t vbl_last;
static int auto_n = 2, auto_calm;
static uint64_t t0_us;
static unsigned long st_frame_us, st_worst_us, st_cpu_us, st_march_us, st_render_us, st_over;
static unsigned long st_rays, st_samples, st_hits;

static void cv_vblank(uint32_t code, void *data) { (void)code; (void)data; vbl_count++; }

static void cv_stage(unsigned n, uint8_t r, uint8_t g, uint8_t b, const char *what) {
    vid_clear(r, g, b);
    printf("cloudvolume: stage %u %s at %lu ms\n", n, what, (unsigned long)((timer_us_gettime64() - t0_us) / 1000u));
    thd_sleep(250);
}

/* The sky by direction (unit, y up), no sun: deep blue overhead through a clear blue to a pale horizon; under the horizon a blue-grey haze. */
static void cv_sky_base(const float d[3], float col[3]) {
    static const float zen[3] = { 0.13f, 0.27f, 0.52f }, mid[3] = { 0.33f, 0.52f, 0.72f }, hor[3] = { 0.66f, 0.80f, 0.86f }, haze[3] = { 0.47f, 0.57f, 0.66f };
    const float e = d[1];
    unsigned k;

    if (e >= 0.0f) {
        float a = sh4der_sat_(e * 3.2f);
        const float b = sh4der_sat_(sh4der_fma_(e, 1.6f, -0.5f));

        a = a * (2.0f - a);
        for (k = 0; k < 3u; k++) col[k] = sh4der_lerp_(sh4der_lerp_(hor[k], mid[k], a), zen[k], b);
    } else {
        const float a = sh4der_sat_(e * -5.0f);

        for (k = 0; k < 3u; k++) col[k] = sh4der_lerp_(hor[k], haze[k], a);
    }
}

/* The same sky with the sun's disc and halo in it: what the water mirrors (the ocean scene's environment texture). */
static void cv_sky_rgb(const float d[3], const float sun[3], float col[3]) {
    const float sx = d[0] - sun[0], sy = d[1] - sun[1], sz = d[2] - sun[2];
    const float d2 = sh4der_len3_(sx, sy, sz);                  /* chord distance squared, as the ocean scene does */
    const float halo = sh4der_water_exp(d2 * -60.0f) * 0.5f;
    const float disc = 1.0f - sh4dow_smooth01(sh4der_fma_(d2, 1.0f / 0.00018f, -0.00022f / 0.00018f));

    cv_sky_base(d, col);
    col[0] = sh4der_lerp_(sh4der_fma_(0.86f, halo, col[0]), 1.0f, disc);
    col[1] = sh4der_lerp_(sh4der_fma_(0.95f, halo, col[1]), 1.0f, disc);
    col[2] = sh4der_lerp_(sh4der_fma_(1.00f, halo, col[2]), 1.0f, disc);
}

static uint32_t cv_argb(float a, float r, float g, float b) {
    return ((uint32_t)sh4der_fma_(sh4der_sat_(a), 255.0f, 0.5f) << 24) | ((uint32_t)sh4der_fma_(sh4der_sat_(r), 255.0f, 0.5f) << 16) |
           ((uint32_t)sh4der_fma_(sh4der_sat_(g), 255.0f, 0.5f) << 8) | (uint32_t)sh4der_fma_(sh4der_sat_(b), 255.0f, 0.5f);
}

/* a screen point from a point of the view before the camera rolled (sx right, sy up, in tangents) */
static void cv_screen(float sx, float sy, float cr, float sr, float *px, float *py) {
    *px = sh4der_fma_(sh4der_fma_(sx, cr, sy * sr), CV_FOCAL, 320.0f);
    *py = sh4der_fma_(sh4der_fma_(sx, -sr, sy * cr), -CV_FOCAL, 240.0f);
}

static int buf_w(void) { return res_w[val[ROW_RES]]; }
static int buf_h(void) { return res_h[val[ROW_RES]]; }
static int buf_tex(void) { return res_tex[val[ROW_RES]]; }
static int preset_now(void) {
    int i;

    for (i = 0; i < PRESETS; i++)
        if (val[ROW_RES] == presets[i].res && val[ROW_STEPS] == presets[i].steps && val[ROW_LIGHT] == presets[i].light &&
            val[ROW_DETAIL] == presets[i].detail && val[ROW_SPREAD] == presets[i].spread) return i;
    return PRESETS;
}

/* The settings into the cloud block (every frame: cheap). */
static void cv_apply(void) {
    clouds.steps = (uint32_t)step_list[val[ROW_STEPS]];
    clouds.light_samples = (uint32_t)val[ROW_LIGHT];
    clouds.detail = (uint32_t)val[ROW_DETAIL];
    clouds.density = SH4DER_CLOUDVOLUME_DENSITY * 0.25f * (float)(val[ROW_DENSITY] + 1);    /* 25 .. 200 % */
    clouds.coverage = 0.20f + 0.10f * (float)val[ROW_COVERAGE];                             /* 20 .. 90 % */
    clouds.jitter = val[ROW_JITTER] ? 1u : 0u;
    clouds.base_y = val[ROW_SEA] == SEA_C ? CV_LOW_BASE : 0.0f;
    sea.reflection = val[ROW_REFLECT];
}

static void cv_sun_change(void) {
    ocean_sun_vector((unsigned)val[ROW_SUN_AZ], (unsigned)val[ROW_SUN_EL], env_sun);
    clouds.sun[0] = env_sun[0]; clouds.sun[1] = env_sun[1]; clouds.sun[2] = env_sun[2];
    ocean_sun[0] = env_sun[0]; ocean_sun[1] = env_sun[1]; ocean_sun[2] = env_sun[2];
    bake_row = 0;                                               /* the clouds' shadows and the water's sky follow over the next frames */
    env_row = 0;
}

static void cv_sea_change(void) {
    cv_waves = val[ROW_SEA] == SEA_C ? cv_waves_near : cv_waves_far;
    ocean_eye_set(val[ROW_SEA] == SEA_C ? low_eye : OCEAN_EYE_MAX);
}

static void cv_value(int row, char *out, size_t n) {
    static const char *sea_names[4] = { "OFF", "A SCALED OCEAN", "B FLAT", "C LOW CLOUD" };
    static const char *lock_names[5] = { "AUTO", "30", "20", "15", "FREE" };
    static const char *jit_names[3] = { "OFF", "FIXED", "TURNING" };

    switch (row) {
    case ROW_PRESET: { const int p = preset_now(); snprintf(out, n, "%s", p < PRESETS ? presets[p].name : "CUSTOM"); break; }
    case ROW_RES: snprintf(out, n, "%dx%d", buf_w(), buf_h()); break;
    case ROW_STEPS: snprintf(out, n, "%d", step_list[val[row]]); break;
    case ROW_LIGHT: if (val[row]) snprintf(out, n, "%d", val[row]); else snprintf(out, n, "BAKED"); break;
    case ROW_DETAIL: snprintf(out, n, "%d", val[row]); break;
    case ROW_DENSITY: snprintf(out, n, "%d%%", 25 * (val[row] + 1)); break;
    case ROW_COVERAGE: snprintf(out, n, "%d%%", 20 + 10 * val[row]); break;
    case ROW_SPREAD: snprintf(out, n, "%d", spread_list[val[row]]); break;
    case ROW_JITTER: snprintf(out, n, "%s", jit_names[val[row]]); break;
    case ROW_OUTPUT: snprintf(out, n, "%s", val[row] == OUTP_PAL8 ? "PAL8" : "RGB565"); break;
    case ROW_LOCK: snprintf(out, n, "%s", lock_names[val[row]]); break;
    case ROW_SEA: snprintf(out, n, "%s", sea_names[val[row]]); break;
    case ROW_MESH: snprintf(out, n, "%u tris", 2u * ocean_mesh_w * ocean_mesh_h); break;
    case ROW_SUN_AZ: snprintf(out, n, "%d deg", (int)(ocean_sun_az_deg[val[row]] + (ocean_sun_az_deg[val[row]] < 0 ? -0.5f : 0.5f))); break;
    case ROW_SUN_EL: snprintf(out, n, "%d deg", (int)(ocean_sun_el_deg[val[row]] + 0.5f)); break;
    case ROW_SPEED: snprintf(out, n, "%d m/s", 50 * val[row]); break;
    default: snprintf(out, n, "%s", val[row] ? "ON" : "OFF");
    }
}

/* One change to the picked row: dir +1 / -1 (D-pad up / down, stops at the ends), or a = 1 (A: one up, wrapping). */
static void cv_step(int dir, int a) {
    int v = val[pick];
    const int n = counts[pick], before = v;

    if (pick == ROW_PRESET) {
        int p = preset_now();

        p = p >= PRESETS ? 1 : a ? (p + 1) % PRESETS : p + dir < 0 ? 0 : p + dir >= PRESETS ? PRESETS - 1 : p + dir;
        val[ROW_RES] = presets[p].res; val[ROW_STEPS] = presets[p].steps; val[ROW_LIGHT] = presets[p].light;
        val[ROW_DETAIL] = presets[p].detail; val[ROW_SPREAD] = presets[p].spread;
        stats_reset = 1;
        return;
    }
    if (a) v = pick == ROW_MESH ? (v + 10) % n : (v + 1) % n;
    else { v += dir; v = v < 0 ? 0 : v >= n ? n - 1 : v; }
    if (v == before) return;
    val[pick] = v;
    stats_reset = 1;
    if (pick == ROW_SUN_AZ || pick == ROW_SUN_EL) cv_sun_change();
    if (pick == ROW_COVERAGE) bake_row = 0;
    if (pick == ROW_MESH) ocean_mesh_set((unsigned)v);
    if (pick == ROW_SEA) cv_sea_change();
    printf("cloudvolume: %s = %d\n", names[pick], v);            /* one serial line a change, none a frame */
}

static void cv_input(float dt) {
    static uint32_t last;
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
    if (st && suite_present() && (st->buttons & SUITE_KEYS_ALL) == SUITE_KEYS_ALL) { sh4der_pvrx_wait_hook(0); suite_exit(SUITE_KEYS); }   /* the test suite disc: A+B+X+Y back to the menu */
    uint32_t down;
    float trig;

    if (!st) return;
    down = st->buttons & ~last; last = st->buttons;
    if (down) hud_dirty = 1;
    if (down & CONT_DPAD_LEFT) pick = (pick + ROWS - 1) % ROWS;
    if (down & CONT_DPAD_RIGHT) pick = (pick + 1) % ROWS;
    if (down & CONT_A) cv_step(0, 1);
    if (down & CONT_DPAD_UP) cv_step(1, 0);
    if (down & CONT_DPAD_DOWN) cv_step(-1, 0);
    if (down & CONT_X) paused ^= 1;
    if (down & CONT_Y) hud ^= 1;
    if (down & CONT_START) { look_yaw = 0.0f; look_pitch = 0.0f; lift = 0.0f; }
    trig = ((float)st->rtrig - (float)st->ltrig) * (1.0f / 255.0f);     /* RT raises, LT lowers */
    if (sh4der_abs_(trig) > 0.03f) {
        if (val[ROW_SEA] == SEA_C) { low_eye = sh4der_clamp_(sh4der_fma_(trig, dt * 2.0f, low_eye), OCEAN_EYE_MIN, OCEAN_EYE_MAX); ocean_eye_set(low_eye); }
        else lift = sh4der_clamp_(sh4der_fma_(trig, dt * 300.0f, lift), -600.0f, 900.0f);
    }
    if (sh4der_abs_((float)st->joyx) > 12.0f) look_yaw = sh4der_clamp_(sh4der_fma_((float)st->joyx, dt * (1.0f / 160.0f), look_yaw), -0.9f, 0.9f);
    if (sh4der_abs_((float)st->joyy) > 12.0f) look_pitch = sh4der_clamp_(sh4der_fma_((float)st->joyy, dt * (-1.0f / 320.0f), look_pitch), -0.25f, 0.25f);
}

/* The camera this frame. With the ocean scene's water on screen the pitch stays inside the range that scene was built for. */
static void cv_camera(void) {
    sh4der_cloudvolume_path(&clouds, fly_time, 0.0f, val[ROW_ROLL], &view);
    view.eye[2] = fly_z;
    view.yaw += look_yaw;
    view.pitch += look_pitch;
    if (val[ROW_SEA] == SEA_C) {
        view.eye[1] = low_eye;                                  /* under the deck, just over the water */
        view.pitch += 0.07f;                                    /* level, not nose down: the clouds are overhead */
    } else {
        view.eye[1] += lift;
    }
    if (val[ROW_SEA] == SEA_A || val[ROW_SEA] == SEA_C) view.pitch = sh4der_clamp_(view.pitch, -0.20f, 0.08f);
    else view.pitch = sh4der_clamp_(view.pitch, -0.6f, 0.6f);
    view.tan_x = CV_TAN_X * CV_OVERSCAN;
    view.tan_y = CV_TAN_Y * CV_OVERSCAN;
}

/* The buffer into texture bytes, in the PVR's twiddled order. PAL8: palette indices. RGB565: the cloud over a sky row colour. */
static void cv_pack(void) {
    const int w = buf_w(), h = buf_h();
    const uint8_t *sh = val[ROW_SMOOTH] ? shade_s : shade_m, *al = val[ROW_SMOOTH] ? alpha_s : alpha_m;
    int x, y;

    if (val[ROW_OUTPUT] == OUTP_PAL8) {
        for (y = 0; y < h; y++) {
            const uint32_t ty = twid[y];

            for (x = 0; x < w; x++) tex8[ty | (twid[x] << 1)] = sh4der_cloudvolume_index(sh[y * w + x], al[y * w + x], (unsigned)x, (unsigned)y);
        }
    } else {
        for (y = 0; y < h; y++) {
            const uint32_t ty = twid[y];
            const float sy = (1.0f - ((float)y + 0.5f) * sh4der_div_(2.0f, (float)h)) * view.tan_y;
            float d[3], sky[3];
            int32_t sr, sg, sb;

            d[0] = frame.fwd[0] + frame.up[0] * sy; d[1] = frame.fwd[1] + frame.up[1] * sy; d[2] = frame.fwd[2] + frame.up[2] * sy;
            sh4der_norm3_(d);
            cv_sky_base(d, sky);                                /* one sky colour a row: the buffer has no roll */
            sr = (int32_t)(sky[0] * 255.0f); sg = (int32_t)(sky[1] * 255.0f); sb = (int32_t)(sky[2] * 255.0f);
            for (x = 0; x < w; x++) {
                const int32_t a = al[y * w + x], bay = (int32_t)sh4der_cloudvolume_bayer_((unsigned)x, (unsigned)y);
                const uint8_t *c = cloud_rgb[sh[y * w + x]];
                /* hand: integer blend, then 5 / 6 / 5 bits rounded through the ordered pattern */
                int32_t r = sr + (((c[0] - sr) * a) >> 8), g = sg + (((c[1] - sg) * a) >> 8), b = sb + (((c[2] - sb) * a) >> 8);

                r = (r * 31 + bay * 16) / 255; g = (g * 63 + bay * 16) / 255; b = (b * 31 + bay * 16) / 255;
                r = r > 31 ? 31 : r; g = g > 63 ? 63 : g; b = b > 31 ? 31 : b;
                tex16[ty | (twid[x] << 1)] = (uint16_t)((r << 11) | (g << 5) | b);
            }
        }
    }
}

/* The sky mesh: a screen grid, each vertex coloured by its ray. SEA B paints the part under the horizon as still water. */
static void cv_sky_build(float cr, float sr) {
    static const float deep[3] = { 0.03f, 0.11f, 0.17f };
    const float height = view.eye[1] - clouds.base_y + CV_SEA_DROP;     /* eye over the water, SEA B */
    int x, y;

    for (y = 0; y <= CV_SKY_Y; y++) {
        for (x = 0; x <= CV_SKY_X; x++) {
            ocean_vertex_t *v = &sky_mesh[y * (CV_SKY_X + 1) + x];
            float *d = sky_dir[y * (CV_SKY_X + 1) + x];
            const float px = (float)x * (640.0f / CV_SKY_X), py = (float)y * (480.0f / CV_SKY_Y);
            const float nx = sh4der_fma_(px, 1.0f / CV_FOCAL, -320.0f / CV_FOCAL), ny = sh4der_fma_(py, -1.0f / CV_FOCAL, 240.0f / CV_FOCAL);
            const float sx = sh4der_fma_(nx, cr, -ny * sr), sy = sh4der_fma_(nx, sr, ny * cr);   /* the same point before the camera rolled */
            float col[3];
            unsigned k;

            for (k = 0; k < 3u; k++) d[k] = frame.fwd[k] + frame.right[k] * sx + frame.up[k] * sy;
            sh4der_norm3_(d);
            cv_sky_base(d, col);
            if (val[ROW_SEA] == SEA_B && d[1] < -0.004f) {
                float r[3] = { d[0], -d[1], d[2] }, sky[3];
                float fr = 1.0f + d[1];
                const float fog = 1.0f - sh4der_water_exp(sh4der_div_(height, d[1]) * (1.0f / 16000.0f));   /* d[1] < 0: the exponent is negative */

                fr = 0.04f + 0.96f * fr * fr * fr * fr * fr;    /* Fresnel: weak looking down, a mirror at the horizon */
                if (val[ROW_REFLECT]) cv_sky_rgb(r, frame.sun, sky); else cv_sky_base(r, sky);
                for (k = 0; k < 3u; k++) col[k] = sh4der_lerp_(sh4der_lerp_(deep[k], sky[k], fr), col[k], fog);
            }
            v->flags = 0; v->x = px; v->y = py; v->z = OCEAN_SKY_DEPTH; v->u = 0.0f; v->v = 0.0f;
            v->argb = cv_argb(1.0f, col[0], col[1], col[2]); v->oargb = 0;
        }
    }
}

static int cv_hdr_ok(const pvr_poly_hdr_t *hdr) {
    uint32_t words[8];

    memcpy(words, hdr, sizeof(words));
    return sh4der_poly_hdr_ok(words);
}

/* a grid of ocean_vertex_t as strips, KOS direct render (as the ocean demo) */
static void cv_submit(const ocean_vertex_t *grid, unsigned w, unsigned h, const pvr_poly_hdr_t *hdr) {
    unsigned x, y, k;

    if (!cv_hdr_ok(hdr)) return;
    pvr_prim(hdr, sizeof(*hdr));
    for (y = 0; y < h; ++y) for (x = 0; x <= w; ++x) for (k = 0; k < 2; ++k) {
        const ocean_vertex_t *v = grid + (y + k) * (w + 1) + x;
        pvr_vertex_t *out = pvr_dr_target();

        out->flags = (x == w && k) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        out->x = v->x; out->y = v->y; out->z = v->z; out->u = v->u; out->v = v->v;
        out->argb = v->argb; out->oargb = v->oargb;
        pvr_dr_commit(out);
    }
}

static void cv_vertex(float x, float y, float z, float u, float v, uint32_t argb, int last) {
    pvr_vertex_t *out = pvr_dr_target();

    out->flags = last ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
    out->x = x; out->y = y; out->z = z; out->u = u; out->v = v; out->argb = argb; out->oargb = 0;
    pvr_dr_commit(out);
}

/* The high layer: a flat sheet of streaks far above, drawn on the sky mesh's cells that look up. Depth 1 / distance makes the PVR's
 * texture mapping follow the sheet's perspective. */
static void cv_streaks(void) {
    const float texel = clouds.tile * (1.0f / (float)SH4DER_CLOUDVOLUME_MAP);
    const float plane = clouds.base_y + clouds.thickness * 2.6f - view.eye[1];      /* metres above the eye */
    const float su = sh4der_div_(0.10f, texel * (float)CV_STREAK), sv = sh4der_div_(0.42f, texel * (float)CV_STREAK);
    const float ou = sh4der_frac_(view.eye[0] * su + frame.cover_off[0] * (0.5f / CV_STREAK)), ov = sh4der_frac_(view.eye[2] * sv + frame.cover_off[1] * (0.5f / CV_STREAK));
    const float lit = 0.80f + 0.20f * sh4der_sat_(sh4der_dot3_(frame.fwd, frame.sun));
    int x, y, k;

    if (!(plane > 10.0f) || !cv_hdr_ok(&streak_header)) return;
    pvr_prim(&streak_header, sizeof(streak_header));
    for (y = 0; y < CV_SKY_Y; y++) {
        for (x = 0; x < CV_SKY_X; x++) {
            const int i00 = y * (CV_SKY_X + 1) + x, idx[4] = { i00, i00 + 1, i00 + CV_SKY_X + 1, i00 + CV_SKY_X + 2 };

            if (sky_dir[idx[0]][1] < 0.03f || sky_dir[idx[1]][1] < 0.03f || sky_dir[idx[2]][1] < 0.03f || sky_dir[idx[3]][1] < 0.03f) continue;
            for (k = 0; k < 4; k++) {                           /* one quad a cell: a four-vertex strip */
                const float *d = sky_dir[idx[k]];
                const float t = sh4der_div_(plane, d[1]);       /* metres along the ray to the sheet */
                const float a = 0.62f * sh4der_water_exp(t * (-1.0f / 60000.0f)) * sh4der_sat_(sh4der_fma_(d[1], 12.0f, -0.36f));

                cv_vertex(sky_mesh[idx[k]].x, sky_mesh[idx[k]].y, sh4der_div_(100.0f, t), sh4der_fma_(d[0] * t, su, ou), sh4der_fma_(d[2] * t, sv, ov),
                          cv_argb(a, 0.72f * lit + 0.10f, 0.76f * lit + 0.10f, 0.86f * lit + 0.10f), k == 3);
            }
        }
    }
}

/* The clouds: one quad, the buffer's picture, turned by the camera's roll. */
static void cv_cloud_quad(float cr, float sr) {
    const float tx = view.tan_x, ty = view.tan_y;
    const float umax = (float)buf_w() / (float)buf_tex(), vmax = (float)buf_h() / (float)buf_tex();
    const float cx[4] = { -tx, tx, -tx, tx }, cy[4] = { ty, ty, -ty, -ty };
    int k;

    if (!cv_hdr_ok(&cloud_header)) return;
    pvr_prim(&cloud_header, sizeof(cloud_header));
    for (k = 0; k < 4; k++) {
        float px, py;

        cv_screen(cx[k], cy[k], cr, sr, &px, &py);
        cv_vertex(px, py, 2.0f, (k & 1) ? umax : 0.0f, (k & 2) ? vmax : 0.0f, 0xffffffffu, k == 3);
    }
}

/* The sun: a small white disc and two soft rings, added on top, dimmed by the cloud the march found in front of it. */
static void cv_sun(float cr, float sr) {
    static const float radius[4] = { 5.0f, 9.0f, 34.0f, 120.0f }, level[4] = { 1.0f, 0.55f, 0.16f, 0.0f };
    const float z = sh4der_dot3_(frame.sun, frame.fwd);
    float sx, sy, px, py, want = 0.0f;
    int k, s;

    if (z > 0.05f) {
        sx = sh4der_div_(sh4der_dot3_(frame.sun, frame.right), z);
        sy = sh4der_div_(sh4der_dot3_(frame.sun, frame.up), z);
        {   /* the cloud's alpha where the sun is in the buffer */
            const int bx = (int)(sh4der_fma_(sh4der_div_(sx, view.tan_x), 0.5f, 0.5f) * (float)buf_w()), by = (int)(sh4der_fma_(sh4der_div_(sy, view.tan_y), -0.5f, 0.5f) * (float)buf_h());

            want = 1.0f;
            if (bx >= 0 && by >= 0 && bx < buf_w() && by < buf_h()) want = 1.0f - (float)(val[ROW_SMOOTH] ? alpha_s : alpha_m)[by * buf_w() + bx] * (1.0f / 255.0f);
        }
    } else {
        sx = sy = 0.0f;
    }
    sun_through = sh4der_lerp_(sun_through, want, 0.25f);       /* eased, so one buffer pixel does not blink the sun */
    if (!(z > 0.05f) || !(sun_through > 0.01f) || !val[ROW_SUNGLOW] || !cv_hdr_ok(&sun_header)) return;
    cv_screen(sx, sy, cr, sr, &px, &py);
    if (px < -200.0f || px > 840.0f || py < -200.0f || py > 680.0f) return;
    pvr_prim(&sun_header, sizeof(sun_header));
    for (k = 0; k < 3; k++) {                                   /* three bands, each a strip round the centre; the disc is the first band's inside */
        const float l0 = level[k] * sun_through, l1 = level[k + 1] * sun_through;
        const uint32_t c0 = cv_argb(1.0f, l0 * 0.92f, l0 * 0.97f, l0), c1 = cv_argb(1.0f, l1 * 0.92f, l1 * 0.97f, l1);

        for (s = 0; s <= 16; s++) {
            float sn, cs;

            sh4der_sincos_((float)(s & 15) * (6.2831853f / 16.0f), &sn, &cs);
            cv_vertex(sh4der_fma_(cs, radius[k], px), sh4der_fma_(sn, radius[k], py), 3.0f, 0.0f, 0.0f, c0, 0);
            cv_vertex(sh4der_fma_(cs, radius[k + 1], px), sh4der_fma_(sn, radius[k + 1], py), 3.0f, 0.0f, 0.0f, c1, s == 16);
        }
    }
    {   /* the disc itself: a fan as a strip (centre, rim, rim, centre ...) */
        const uint32_t c = cv_argb(1.0f, sun_through, sun_through, sun_through);

        for (s = 0; s <= 16; s++) {
            float sn, cs;

            sh4der_sincos_((float)(s & 15) * (6.2831853f / 16.0f), &sn, &cs);
            cv_vertex(px, py, 3.0f, 0.0f, 0.0f, c, 0);
            cv_vertex(sh4der_fma_(cs, radius[0], px), sh4der_fma_(sn, radius[0], py), 3.0f, 0.0f, 0.0f, c, s == 16);
        }
    }
}

static void cv_hud(void) {
    char text[100], value[32];
    unsigned i;
    const int lock = val[ROW_LOCK] == LOCK_AUTO ? auto_n : val[ROW_LOCK] == LOCK_FREE ? 0 : val[ROW_LOCK] + 1;

    memset(hud_pixels, 0, sizeof(hud_pixels)); minifont_set_color(255, 255, 255);
    cv_value(pick, value, sizeof(value));
    snprintf(text, sizeof(text), "CLOUDS  < %s %s >  %s", names[pick], value, paused ? "PAUSED" : "");
    minifont_draw_str(hud_pixels, 512, text);
    snprintf(text, sizeof(text), "frame %lu.%lu ms  worst %lu.%lu  >41.7ms %lu  lock %d vbl", st_frame_us / 1000, (st_frame_us / 100) % 10,
             st_worst_us / 1000, (st_worst_us / 100) % 10, st_over, lock);
    minifont_draw_str(hud_pixels + 512 * 18, 512, text);
    snprintf(text, sizeof(text), "MARCH %lu.%lu ms  CPU %lu.%lu  RENDER %lu.%lu  VRAM %lu KB", st_march_us / 1000, (st_march_us / 100) % 10,
             st_cpu_us / 1000, (st_cpu_us / 100) % 10, st_render_us / 1000, (st_render_us / 100) % 10, (unsigned long)pvr_mem_available() / 1024);
    minifont_draw_str(hud_pixels + 512 * 36, 512, text);
    snprintf(text, sizeof(text), "rays %lu  samples %lu  hits %lu  %dx%d %s", st_rays, st_samples, st_hits, buf_w(), buf_h(), val[ROW_OUTPUT] == OUTP_PAL8 ? "PAL8" : "565");
    minifont_draw_str(hud_pixels + 512 * 54, 512, text);
    snprintf(text, sizeof(text), "ALT %d m  SUN %d/%d  %s%s", (int)view.eye[1], (int)ocean_sun_az_deg[val[ROW_SUN_AZ]], (int)ocean_sun_el_deg[val[ROW_SUN_EL]],
             bake_row < SH4DER_CLOUDVOLUME_MAP ? "BAKE " : "", env_row < OCEAN_ENV ? "SKY REBUILD" : "");
    minifont_draw_str(hud_pixels + 512 * 72, 512, text);
    for (i = 0; i < 512 * CV_HUD_H; ++i) hud_pixels[i] = hud_pixels[i] ? 0xffff : 0;
}

static void cv_hud_draw(void) {
    unsigned i;

    if (!cv_hdr_ok(&hud_header)) return;
    pvr_prim(&hud_header, sizeof(hud_header));
    for (i = 0; i < 4; ++i) cv_vertex(20.0f + (i & 1 ? 512.0f : 0.0f), 18.0f + (i & 2 ? (float)CV_HUD_H : 0.0f), 1000.0f, i & 1 ? 1.0f : 0.0f, i & 2 ? 1.0f : 0.0f, 0xffd0d0d0u, i == 3);
}

/* The headers that follow a setting: the cloud quad's texture size, format and filter. */
static void cv_cloud_header(void) {
    pvr_poly_cxt_t c;
    const int pal = val[ROW_OUTPUT] == OUTP_PAL8;

    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, pal ? (PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(0) | PVR_TXRFMT_TWIDDLED) : (PVR_TXRFMT_RGB565 | PVR_TXRFMT_TWIDDLED),
                     buf_tex(), buf_tex(), pal ? tex8_vram : tex16_vram, val[ROW_FILTER] ? PVR_FILTER_BILINEAR : PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE; c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_SRCALPHA; c.blend.dst = PVR_BLEND_INVSRCALPHA;
    c.txr.env = PVR_TXRENV_REPLACE; c.txr.alpha = PVR_TXRALPHA_ENABLE; c.txr.uv_clamp = PVR_UVCLAMP_UV;
    pvr_poly_compile(&cloud_header, &c);
}

static void cv_setup(void) {
    pvr_poly_cxt_t c;
    uint32_t pal[256];
    unsigned i, x, y;

    for (i = 0; i < CV_TEX; i++) {                              /* hand: bit spread for the twiddled order */
        uint32_t r = 0, b;

        for (b = 0; b < 8u; b++) r |= ((i >> b) & 1u) << (2u * b);
        twid[i] = r;
    }
    sh4der_cloudvolume_palette(pal);
    pvr_set_pal_format(PVR_PAL_ARGB8888);
    for (i = 0; i < 256u; i++) pvr_set_pal_entry(i, pal[i]);
    for (i = 0; i < 256u; i++) {
        float rgb[3];

        sh4der_cloudvolume_rgb(i, rgb);
        cloud_rgb[i][0] = (uint8_t)(rgb[0] * 255.0f); cloud_rgb[i][1] = (uint8_t)(rgb[1] * 255.0f); cloud_rgb[i][2] = (uint8_t)(rgb[2] * 255.0f);
    }
    /* the high layer's texture: white, its alpha two scales of the cover map (streaks come from the stretch it is drawn with) */
    for (y = 0; y < CV_STREAK; y++) {
        for (x = 0; x < CV_STREAK; x++) {
            const unsigned a0 = maps.cover[y * SH4DER_CLOUDVOLUME_MAP + x] & 255u;
            const unsigned a1 = maps.cover[((y * 3u) & (SH4DER_CLOUDVOLUME_MAP - 1u)) * SH4DER_CLOUDVOLUME_MAP + ((x * 3u + 9u) & (SH4DER_CLOUDVOLUME_MAP - 1u))] & 255u;
            const float n = ((float)a0 * 0.65f + (float)a1 * 0.35f) * (1.0f / 255.0f);
            const unsigned a = (unsigned)(sh4der_sat_((n - 0.40f) * 3.0f) * 15.0f + 0.5f);

            streak_pixels[twid[y] | (twid[x] << 1)] = (uint16_t)((a << 12) | 0x0fffu);
        }
    }
    tex8_vram = pvr_mem_malloc(sizeof(tex8)); tex16_vram = pvr_mem_malloc(sizeof(tex16)); streak_vram = pvr_mem_malloc(sizeof(streak_pixels));
    env_vram = pvr_mem_malloc(sizeof(ocean_environment)); hud_vram = pvr_mem_malloc(sizeof(hud_pixels));
    if (!tex8_vram || !tex16_vram || !streak_vram || !env_vram || !hud_vram) { printf("cloudvolume: texture allocation FAIL\n"); arch_exit(); }
    memset(tex8, 0, sizeof(tex8)); memset(tex16, 0, sizeof(tex16));
    pvr_txr_load(tex8, tex8_vram, sizeof(tex8));
    pvr_txr_load(tex16, tex16_vram, sizeof(tex16));
    pvr_txr_load(streak_pixels, streak_vram, sizeof(streak_pixels));
    pvr_txr_load(ocean_environment, env_vram, sizeof(ocean_environment));

    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);                     /* the sky: vertex colours */
    c.gen.culling = PVR_CULLING_NONE; c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    pvr_poly_compile(&sky_header, &c);

    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, OCEAN_ENV, OCEAN_ENV, env_vram, PVR_FILTER_BILINEAR);   /* the water, as the ocean demo */
    c.gen.culling = PVR_CULLING_NONE; c.gen.fog_type = PVR_FOG_TABLE;
    c.txr.env = PVR_TXRENV_MODULATE; c.txr.uv_clamp = PVR_UVCLAMP_UV;
    c.gen.specular = 1;
    pvr_poly_compile(&water_header, &c);

    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_TWIDDLED, CV_STREAK, CV_STREAK, streak_vram, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE; c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_SRCALPHA; c.blend.dst = PVR_BLEND_INVSRCALPHA;
    c.txr.env = PVR_TXRENV_MODULATEALPHA; c.txr.alpha = PVR_TXRALPHA_ENABLE;
    pvr_poly_compile(&streak_header, &c);

    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);                     /* the sun: added */
    c.gen.culling = PVR_CULLING_NONE; c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_ONE; c.blend.dst = PVR_BLEND_ONE;
    pvr_poly_compile(&sun_header, &c);

    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, 512, CV_HUD_H, hud_vram, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE; c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS; c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_SRCALPHA; c.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&hud_header, &c);
    cv_cloud_header();
}

int main(void) {
    suite_init();   /* on the test suite disc: the menu's binary into RAM, before anything else */
    if (suite_present()) sh4der_waitrun_init(&wr_run, SH4DER_WAITRUN_SECONDS);   /* from the suite's menu: a timed run that ends on a results page */
    pvr_init_params_t init = pvr_default_params;
    pvr_stats_t stats;
    uint64_t previous, report, cpu_total = 0, frame_total = 0, render_total = 0, march_total = 0, worst = 0;
    unsigned frames = 0, over = 0, stuck = 0, turn = 0;
    unsigned long total_frames = 0;
    int stuck_shown = 0;

    t0_us = timer_us_gettime64();
    (void)ocean_cpu_us; (void)ocean_frame_us; (void)ocean_worst_us; (void)ocean_over;   /* the ocean demo's readout numbers: not used here */
    /* Stage colours (the displayed frame, no PVR render): 1 red main, 2 orange PVR init, 3 yellow clouds made, 4 green sea sky built,
     * 5 cyan textures and headers, 6 blue first march, 7 magenta first submit. The picture replaces it when the PVR renders. White later = stuck. */
    cv_stage(1, 160, 0, 0, "main");
    init.vertex_buf_size = 1024 * 1024; init.opb_overflow_count = 4; init.autosort_disabled = 1;
    if (pvr_init(&init) < 0) { printf("cloudvolume: pvr_init FAIL\n"); return 1; }

#ifndef WAIT_SLEEP
    sh4der_pvrx_wait_hook(1);   /* polled render-done wait (PVR-117): every scene below ends through sh4der_pvrx_scene_finish; -DWAIT_SLEEP keeps KallistiOS's sleeping wait for an A/B */
#endif
    pvr_set_bg_color(0.47f, 0.57f, 0.66f);
    vblank_handler_add(cv_vblank, NULL);
    cv_stage(2, 224, 96, 0, "pvr_init");
    sh4der_cloudvolume_init(&clouds);
    cv_apply();
    ocean_sun_vector((unsigned)val[ROW_SUN_AZ], (unsigned)val[ROW_SUN_EL], env_sun);
    clouds.sun[0] = env_sun[0]; clouds.sun[1] = env_sun[1]; clouds.sun[2] = env_sun[2];
    ocean_sun[0] = env_sun[0]; ocean_sun[1] = env_sun[1]; ocean_sun[2] = env_sun[2];
    sh4der_cloudvolume_generate(&maps, 20261124u);
    sh4der_cloudvolume_bake(&maps, &clouds, 0, SH4DER_CLOUDVOLUME_MAP);
    cv_stage(3, 200, 200, 0, "clouds made");
    { float fog[129];
      ocean_fog_samples(fog);
      pvr_fog_table_color(1.0f, 0.66f, 0.80f, 0.86f);           /* the sky's horizon colour */
      pvr_fog_far_depth(OCEAN_FOG_END); pvr_fog_table_custom(fog); }
    ocean_scene_init();                                         /* the water's sky texture (this sky, this sun) and its tables */
    ocean_mesh_set((unsigned)val[ROW_MESH]);
    cv_sea_change();
    cv_stage(4, 0, 160, 0, "sea sky built");
    cv_setup();
    cv_stage(5, 0, 160, 160, "textures + headers");
    printf("cloudvolume: maps %lu KB, buffers %lu KB, VRAM free %lu KB, water %ux%u\n", (unsigned long)sizeof(maps) / 1024,
           (unsigned long)(sizeof(shade_m) * 4 + sizeof(tex8) + sizeof(tex16)) / 1024, (unsigned long)pvr_mem_available() / 1024, ocean_mesh_w, ocean_mesh_h);
    cv_camera();
    if (sh4der_cloudvolume_prepare(&clouds, &view, 0.0f, 0u, &frame)) {
        const uint64_t t = timer_us_gettime64();

        sh4der_cloudvolume_march(&maps, &frame, (unsigned)buf_w(), (unsigned)buf_h(), 1u, 0u, shade_m, alpha_m);
        printf("cloudvolume: first full march %lu us (%lu rays, %lu samples, %lu hits)\n", (unsigned long)(timer_us_gettime64() - t),
               (unsigned long)frame.rays, (unsigned long)(frame.samples + frame.skips), (unsigned long)frame.hits);
    }
    cv_stage(6, 0, 0, 200, "first march");
    cv_stage(7, 200, 0, 200, "first submit");
    previous = report = timer_us_gettime64();
    vbl_last = vbl_count;
    for (;;) {
        if (suite_present() && sh4der_waitrun_frame(&wr_run, timer_us_gettime64())) sh4der_waitrun_show(&wr_run, "CLOUDS");
        const uint64_t now = timer_us_gettime64(), elapsed = now - previous;
        uint64_t cpu_start, march_us = 0, work;
        const int lock = val[ROW_LOCK] == LOCK_AUTO ? auto_n : val[ROW_LOCK] == LOCK_FREE ? 0 : val[ROW_LOCK] + 1;   /* fields a frame; 0 = as fast as it goes */
        /* a locked frame moves the camera by the lock's own time, so the flight is even; a free one by the clock */
        const float dt = lock ? (float)lock * (1.0f / 59.94f) : sh4der_clamp_((float)elapsed * 1.0e-6f, 0.0f, 0.1f);
        float cr, sr;
        int ready, sea_mesh;

        previous = now;
        cpu_start = now;
        cv_input(dt);
        cv_apply();
        if (stats_reset) { stats_reset = 0; report = now; cpu_total = frame_total = render_total = march_total = 0; frames = 0; worst = 0; over = 0; }
        if (!paused) { fly_time += dt; fly_z += dt * 50.0f * (float)val[ROW_SPEED]; }
        cv_camera();
        sea_mesh = val[ROW_SEA] == SEA_A || val[ROW_SEA] == SEA_C;
        sh4der_sincos_(view.roll, &sr, &cr);

        /* a few rows a frame after a sun or coverage change, so no frame stalls */
        if (bake_row < SH4DER_CLOUDVOLUME_MAP) { sh4der_cloudvolume_bake(&maps, &clouds, bake_row, bake_row + 8u); bake_row += 8u; }
        if (env_row < OCEAN_ENV) { ocean_env_rows(env_sun, env_row, env_row + 8u); env_row += 8u; if (env_row >= OCEAN_ENV) env_upload = 1; }

        /* the clouds: march, smooth, pack */
        if (sh4der_cloudvolume_prepare(&clouds, &view, fly_time, val[ROW_JITTER] == 2 ? turn : 0u, &frame)) {
            const uint64_t t = timer_us_gettime64();

            sh4der_cloudvolume_march(&maps, &frame, (unsigned)buf_w(), (unsigned)buf_h(), (unsigned)spread_list[val[ROW_SPREAD]], turn, shade_m, alpha_m);
            if (val[ROW_SMOOTH]) sh4der_cloudvolume_smooth(shade_m, alpha_m, (unsigned)buf_w(), (unsigned)buf_h(), shade_s, alpha_s);
            cv_pack();
            march_us = timer_us_gettime64() - t;
            st_rays = frame.rays; st_samples = frame.samples + frame.skips; st_hits = frame.hits;
        }
        turn++;
        cv_sky_build(cr, sr);
        if (sea_mesh) {                                         /* the ocean scene's water for this camera, then turned by the roll */
            unsigned i;
            const unsigned n = (ocean_mesh_w + 1u) * (ocean_mesh_h + 1u);

            sea.time = fly_time; sea.yaw = view.yaw; sea.pitch = view.pitch;
            ocean_scene_update(&sea);
            for (i = 0; i < n; i++) {
                const float sx = (ocean_water[i].x - 320.0f) * (1.0f / CV_FOCAL), sy = (240.0f - ocean_water[i].y) * (1.0f / CV_FOCAL);

                cv_screen(sx, sy, cr, sr, &ocean_water[i].x, &ocean_water[i].y);
            }
        }
        if (hud && hud_dirty) cv_hud();
        cv_cloud_header();
        work = timer_us_gettime64() - cpu_start;

        /* AUTO lock: the fewest fields a frame the work fits in, slower at once when it does not fit, faster only after two calm seconds */
        if (work + 3000u > (uint64_t)auto_n * CV_VBL_US) { if (auto_n < 8) auto_n++; auto_calm = 0; }
        else if (auto_n > 2 && work + 5000u < (uint64_t)(auto_n - 1) * CV_VBL_US) { if (++auto_calm > 120 / auto_n) { auto_n--; auto_calm = 0; } }
        else auto_calm = 0;

        ready = sh4der_pvrx_wait_idle();      /* textures change only after the PVR has used them: every frame, so the polled wait (PVR-117) */
        if (pvr_get_stats(&stats) == 0) render_total += stats.rnd_last_time / 1000u;
        if (val[ROW_OUTPUT] == OUTP_PAL8) pvr_txr_load(tex8, tex8_vram, (size_t)(buf_tex() * buf_tex()));
        else pvr_txr_load(tex16, tex16_vram, (size_t)(buf_tex() * buf_tex() * 2));
        if (hud && hud_dirty) { pvr_txr_load(hud_pixels, hud_vram, sizeof(hud_pixels)); hud_dirty = 0; }
        if (env_upload) { pvr_txr_load(ocean_environment, env_vram, sizeof(ocean_environment)); env_upload = 0; }
        if (ready < 0) {
            if (++stuck >= 20 && !stuck_shown) { stuck_shown = 1; vid_clear(255, 255, 255); printf("cloudvolume: PVR stuck after %lu frames\n", total_frames); }
        } else {
            if (total_frames == 1) printf("cloudvolume: frame 1 rendered by %lu ms\n", (unsigned long)((timer_us_gettime64() - t0_us) / 1000u));
            stuck = 0;
        }
        if (lock) {                                             /* hold every frame for the same number of fields */
            unsigned spin = 0;

            while ((uint32_t)(vbl_count - vbl_last) < (uint32_t)lock && spin++ < 200000u) thd_pass();
        }
        vbl_last = vbl_count;

        cpu_start = timer_us_gettime64();
        pvr_scene_begin(); pvr_list_begin(PVR_LIST_OP_POLY);
        cv_submit(sky_mesh, CV_SKY_X, CV_SKY_Y, &sky_header);
        if (sea_mesh) cv_submit(ocean_water, ocean_mesh_w, ocean_mesh_h, &water_header);
        pvr_list_finish(); pvr_list_begin(PVR_LIST_TR_POLY);
        if (val[ROW_HIGH] && val[ROW_OUTPUT] == OUTP_PAL8) cv_streaks();
        cv_cloud_quad(cr, sr);
        cv_sun(cr, sr);
        if (hud) cv_hud_draw();
        pvr_list_finish(); sh4der_pvrx_scene_finish();
        ++total_frames;

        cpu_total += work + timer_us_gettime64() - cpu_start;
        march_total += march_us;
        if (frames > 0) { frame_total += elapsed; if (elapsed > worst) worst = elapsed; over += elapsed > CV_FRAME_US; }
        ++frames;
        if (now - report >= 2000000 && frames > 1) {            /* two-second windows, as the ocean demo */
            st_cpu_us = (unsigned long)(cpu_total / frames); st_frame_us = (unsigned long)(frame_total / (frames - 1));
            st_worst_us = (unsigned long)worst; st_over = over; st_render_us = (unsigned long)(render_total / frames); st_march_us = (unsigned long)(march_total / frames);
            report = now; cpu_total = frame_total = render_total = march_total = 0; frames = 0; worst = 0; over = 0;
            hud_dirty = 1;
        }
    }
}
#else
/* ------------------------------------------------------------------------------------------------------------------ */
/* The host preview. */

#define OUT_W 640
#define OUT_H 480
#define TAN_X 0.70f                 /* half the screen's horizontal field of view, as a tangent (70 degrees across) */
#define TAN_Y (TAN_X * 0.75f)
#define OVERSCAN 1.12f              /* the buffer sees this much more than the screen, so a rolled quad still covers it */
#define MAX_PIX (256 * 192)

/* ESTIMATE, SH-4 cycles: a density read with its cover map blend; a noise volume blend; a ray's setup; a pixel's pack and store */
#define CYC_SAMPLE 42.0             /* float position -> fixed, 4 aligned 16-bit reads, the blend, the profile test */
#define CYC_SKIP   16.0             /* float position -> fixed, one byte read, the test */
#define CYC_HIT    38.0             /* light table, opacity table, haze, shade, composite */
#define CYC_NOISE  52.0             /* 8 byte reads and the blend */
#define CYC_RAY    110.0            /* direction, length (FSRRA), slab cut (one divide), phase */
#define CYC_PIXEL  14.0             /* pack to an index, store */
#define CYC_SMOOTH 30.0

static sh4der_cloudvolume_maps_t maps;
static uint8_t shade_m[MAX_PIX], alpha_m[MAX_PIX];   /* as marched (kept between frames for the spread) */
static uint8_t shade[MAX_PIX], alpha[MAX_PIX];       /* after the smooth */
static uint8_t out_rgb[OUT_W * OUT_H * 3];
#define MIR_W 64
#define MIR_H 48
#define MIR_STEPS 8
#define SEA_BELOW 1400.0f           /* the water lies this far under the slab's base, metres */
static uint8_t mir_shade_m[MIR_W * MIR_H], mir_alpha_m[MIR_W * MIR_H], mir_shade[MIR_W * MIR_H], mir_alpha[MIR_W * MIR_H];
static int mir_on;
static float mir_tan_x, mir_tan_y;
static float tex[MAX_PIX][4];       /* the texture as the PVR would hold it: r, g, b, a */

typedef struct { int w, h, spread, frames, filter, high, ground, roll, out, smooth, refl, jturn; float t, speed, alt, pitch, az, el; } opts_t;
enum { OUT_PAL8, OUT_565, OUT_IDEAL };

static float satf(float x) { return x < 0 ? 0 : x > 1 ? 1 : x; }
static float mixf(float a, float b, float t) { return a + (b - a) * t; }

static void sun_add(const sh4der_cloudvolume_frame_t *f, const float d[3], float through, float rgb[3]);

static float cover_at(float px, float pz) {     /* the cover map's height 0 .. 1, blended */
    int32_t h, s;
    sh4der_cloudvolume_cover_(&maps, sh4der_cloudvolume_fix_(px), sh4der_cloudvolume_fix_(pz), &h, &s);
    return (float)h * (1.0f / 255.0f);
}

/* What is behind the clouds along a unit direction: sky, the high streak layer, the ground. *veil = how much the high layer hides the sun. */
static void backdrop(const sh4der_cloudvolume_t *c, const sh4der_cloudvolume_frame_t *f, const opts_t *o, const float eye[3], const float d[3], float rgb[3], float *veil) {
    static const float zen[3] = { 0.13f, 0.27f, 0.52f }, mid[3] = { 0.33f, 0.52f, 0.72f }, hor[3] = { 0.66f, 0.80f, 0.86f }, haze[3] = { 0.47f, 0.57f, 0.66f };
    const float texel = c->tile / (float)SH4DER_CLOUDVOLUME_MAP;
    const float e = d[1];
    int k;

    *veil = 0.0f;
    if (e >= 0.0f) {
        const float a = satf(e * 3.2f), b = satf((e - 0.3125f) * 1.6f);
        for (k = 0; k < 3; k++) rgb[k] = mixf(mixf(hor[k], mid[k], powf(a, 0.7f)), zen[k], b);
        if (o->high && e > 0.004f) {
            const float t = (c->base_y + c->thickness * 2.6f - eye[1]) / e;        /* metres to the sheet */
            if (t > 0.0f) {
                const float px = (eye[0] + d[0] * t) / texel, pz = (eye[2] + d[2] * t) / texel;
                float n = cover_at(px * 0.10f + f->cover_off[0] * 0.5f + 40.0f, pz * 0.42f + f->cover_off[1] * 0.5f) * 0.65f +
                          cover_at(px * 0.31f + 9.0f, pz * 1.3f + f->cover_off[1]) * 0.35f;
                float a2 = satf((n - 0.40f) * 3.0f) * 0.62f * expf(-t / 60000.0f);
                const float lit = 0.78f + 0.22f * satf(d[0] * f->sun[0] + d[1] * f->sun[1] + d[2] * f->sun[2]);
                static const float sheet[3] = { 0.62f, 0.66f, 0.76f };
                for (k = 0; k < 3; k++) rgb[k] = mixf(rgb[k], sheet[k] * lit + 0.10f, a2);
                *veil = a2;
            }
        }
    } else {
        const float a = satf(-e * 5.0f);
        for (k = 0; k < 3; k++) rgb[k] = mixf(hor[k], haze[k], a);
        if (o->ground && e < -0.004f) {                          /* the sea (the ocean demo's water stands here on the console) */
            const float t = (c->base_y - SEA_BELOW - eye[1]) / e;
            const float px = (eye[0] + d[0] * t) / texel, pz = (eye[2] + d[2] * t) / texel;
            /* ripples: a small tilt of the mirror from two reads of the cover map */
            const float n0 = cover_at(px * 2.3f + f->noise_off[1] * 3.0f, pz * 2.3f), n1 = cover_at(px * 2.3f + 0.35f + f->noise_off[1] * 3.0f, pz * 2.3f), n2 = cover_at(px * 2.3f + f->noise_off[1] * 3.0f, pz * 2.3f + 0.35f);
            float r[3] = { d[0] + (n1 - n0) * 0.9f, -d[1], d[2] + (n2 - n0) * 0.9f }, sky[3], v2, fres, l, fog;
            static const float deep[3] = { 0.03f, 0.11f, 0.17f };
            l = 1.0f / sqrtf(r[0] * r[0] + r[1] * r[1] + r[2] * r[2]);
            r[0] *= l; r[1] *= l; r[2] *= l;
            if (r[1] < 0.02f) r[1] = 0.02f;
            {   opts_t up = *o; up.ground = 0; backdrop(c, f, &up, eye, r, sky, &v2); }
            sun_add(f, r, 0.8f * (1.0f - v2 * 0.5f), sky);        /* the sun's glitter */
            if (mir_on) {                                         /* the clouds, mirrored: what the reflection pass would hold */
                const float m[3] = { r[0], -r[1], r[2] };
                const float z = m[0] * f->fwd[0] + m[1] * f->fwd[1] + m[2] * f->fwd[2];
                if (z > 0.05f) {
                    const float sx = (m[0] * f->right[0] + m[1] * f->right[1] + m[2] * f->right[2]) / z / mir_tan_x;
                    const float sy = (m[0] * f->up[0] + m[1] * f->up[1] + m[2] * f->up[2]) / z / mir_tan_y;
                    const float bu = (sx * 0.5f + 0.5f) * MIR_W - 0.5f, bv = (0.5f + sy * 0.5f) * MIR_H - 0.5f;   /* the mirror view is upside down */
                    int x0 = (int)floorf(bu), y0 = (int)floorf(bv), x1, y1;
                    const float fx = bu - (float)x0, fy = bv - (float)y0;
                    float a, sh, cl[3];
                    x1 = x0 + 1; y1 = y0 + 1;
                    x0 = x0 < 0 ? 0 : x0 >= MIR_W ? MIR_W - 1 : x0; x1 = x1 < 0 ? 0 : x1 >= MIR_W ? MIR_W - 1 : x1;
                    y0 = y0 < 0 ? 0 : y0 >= MIR_H ? MIR_H - 1 : y0; y1 = y1 < 0 ? 0 : y1 >= MIR_H ? MIR_H - 1 : y1;
                    a = mixf(mixf(mir_alpha[y0 * MIR_W + x0], mir_alpha[y0 * MIR_W + x1], fx), mixf(mir_alpha[y1 * MIR_W + x0], mir_alpha[y1 * MIR_W + x1], fx), fy) * (1.0f / 255.0f);
                    sh = mixf(mixf(mir_shade[y0 * MIR_W + x0], mir_shade[y0 * MIR_W + x1], fx), mixf(mir_shade[y1 * MIR_W + x0], mir_shade[y1 * MIR_W + x1], fx), fy);
                    sh4der_cloudvolume_rgb((unsigned)sh, cl);
                    for (k = 0; k < 3; k++) sky[k] = mixf(sky[k], cl[k], a);
                }
            }
            fres = 1.0f + d[1]; fres = fres * fres; fres = 0.04f + 0.96f * fres * fres * (1.0f + d[1]);   /* Fresnel: weak looking down, a mirror at the horizon */
            fog = 1.0f - expf(-t / 16000.0f);
            for (k = 0; k < 3; k++) rgb[k] = mixf(mixf(deep[k], sky[k], fres), rgb[k], fog);
        }
    }
}

static void sun_add(const sh4der_cloudvolume_frame_t *f, const float d[3], float through, float rgb[3]) {
    const float cs = d[0] * f->sun[0] + d[1] * f->sun[1] + d[2] * f->sun[2];
    const float ang = acosf(cs > 1.0f ? 1.0f : cs < -1.0f ? -1.0f : cs);
    const float disc = satf((0.013f - ang) * 300.0f);
    const float halo = 0.40f * expf(-ang * 26.0f) + 0.16f * expf(-ang * 5.0f);
    rgb[0] = satf(rgb[0] + through * (disc + halo * 0.86f));
    rgb[1] = satf(rgb[1] + through * (disc + halo * 0.95f));
    rgb[2] = satf(rgb[2] + through * (disc + halo * 1.00f));
}

static void ray_dir(const sh4der_cloudvolume_frame_t *f, float sx, float sy, float d[3]) {
    float l; int k;
    for (k = 0; k < 3; k++) d[k] = f->fwd[k] + f->right[k] * sx + f->up[k] * sy;
    l = 1.0f / sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    for (k = 0; k < 3; k++) d[k] *= l;
}

int main(int argc, char **argv) {
    sh4der_cloudvolume_t c;
    sh4der_cloudvolume_view_t v;
    sh4der_cloudvolume_frame_t f;
    opts_t o = { 128, 96, 4, 1, 1, 1, 1, 1, OUT_PAL8, 1, 1, 1, 12.0f, 150.0f, -1.0f, -99.0f, 8.0f, 12.0f };
    uint32_t pal[256];
    double cyc = 0, rays = 0, samples = 0, noise = 0, marched = 0, hits = 0, skips = 0, mir_cyc = 0;
    float dens = 100.0f, sun_through = 1.0f;
    int i, x, y, fr;
    FILE *fp;

    if (argc < 2) { fprintf(stderr, "usage: %s out.ppm [key=value ...]\n", argv[0]); return 2; }
    sh4der_cloudvolume_init(&c);
    for (i = 2; i < argc; i++) {
        const char *a = argv[i], *q = strchr(a, '='); float n;
        if (!q) continue;
        n = (float)atof(q + 1);
        if (!strncmp(a, "preset=", 7)) {
            if (q[1] == 'p') { o.w = 96; o.h = 72; c.steps = 12; c.light_samples = 0; c.detail = 1; o.spread = 4; }
            else if (q[1] == 'c') { o.w = 256; o.h = 192; c.steps = 48; c.light_samples = 3; c.detail = 2; o.spread = 1; }
            else { o.w = 128; o.h = 96; c.steps = 16; c.light_samples = 0; c.detail = 1; o.spread = 4; }
        }
        else if (!strncmp(a, "t=", 2)) o.t = n;
        else if (!strncmp(a, "res=", 4)) sscanf(q + 1, "%dx%d", &o.w, &o.h);
        else if (!strncmp(a, "steps=", 6)) c.steps = (uint32_t)n;
        else if (!strncmp(a, "light=", 6)) c.light_samples = (uint32_t)n;
        else if (!strncmp(a, "detail=", 7)) c.detail = (uint32_t)n;
        else if (!strncmp(a, "density=", 8)) dens = n;
        else if (!strncmp(a, "coverage=", 9)) c.coverage = n * 0.01f;
        else if (!strncmp(a, "spread=", 7)) o.spread = (int)n;
        else if (!strncmp(a, "frames=", 7)) o.frames = (int)n;
        else if (!strncmp(a, "jitter=", 7)) c.jitter = (uint32_t)n;
        else if (!strncmp(a, "filter=", 7)) o.filter = (int)n;
        else if (!strncmp(a, "smooth=", 7)) o.smooth = (int)n;
        else if (!strncmp(a, "refl=", 5)) o.refl = (int)n;
        else if (!strncmp(a, "jturn=", 6)) o.jturn = (int)n;
        else if (!strncmp(a, "high=", 5)) o.high = (int)n;
        else if (!strncmp(a, "ground=", 7)) o.ground = (int)n;
        else if (!strncmp(a, "roll=", 5)) o.roll = (int)n;
        else if (!strncmp(a, "speed=", 6)) o.speed = n;
        else if (!strncmp(a, "alt=", 4)) o.alt = n;
        else if (!strncmp(a, "pitch=", 6)) o.pitch = n;
        else if (!strncmp(a, "az=", 3)) o.az = n;
        else if (!strncmp(a, "el=", 3)) o.el = n;
        else if (!strncmp(a, "out=", 4)) o.out = q[1] == '5' ? OUT_565 : q[1] == 'i' ? OUT_IDEAL : OUT_PAL8;
    }
    c.density = SH4DER_CLOUDVOLUME_DENSITY * dens * 0.01f;
    { const float az = o.az * 0.0174533f, el = o.el * 0.0174533f;
      c.sun[0] = cosf(el) * sinf(az); c.sun[1] = sinf(el); c.sun[2] = cosf(el) * cosf(az); }
    if (o.w * o.h > MAX_PIX || o.w < 8 || o.h < 8 || !sh4der_cloudvolume_valid(&c)) { fprintf(stderr, "bad settings\n"); return 2; }
    if (o.frames < 1) o.frames = 1;

    sh4der_cloudvolume_generate(&maps, 20261124u);
    sh4der_cloudvolume_bake(&maps, &c, 0, SH4DER_CLOUDVOLUME_MAP);
    sh4der_cloudvolume_palette(pal);
    memset(alpha_m, 0, sizeof(alpha_m));
    memset(shade_m, 128, sizeof(shade_m));

    for (fr = 0; fr < o.frames; fr++) {
        const float t = o.t - (float)(o.frames - 1 - fr) * (1.0f / 24.0f);
        const unsigned turn = (unsigned)(t * 24.0f + 0.5f);
        unsigned n;

        sh4der_cloudvolume_path(&c, t, o.speed, o.roll, &v);
        if (o.alt >= 0.0f) v.eye[1] = o.alt;
        if (o.pitch > -90.0f) v.pitch = o.pitch * 0.0174533f;
        v.tan_x = TAN_X * OVERSCAN;
        v.tan_y = TAN_Y * OVERSCAN;
        if (!sh4der_cloudvolume_prepare(&c, &v, t, o.jturn ? turn : 0u, &f)) { fprintf(stderr, "prepare refused\n"); return 2; }
        n = sh4der_cloudvolume_march(&maps, &f, (unsigned)o.w, (unsigned)o.h, (unsigned)(o.frames == 1 ? 1 : o.spread), turn, shade_m, alpha_m);
        mir_on = o.refl && o.ground;
        if (mir_on) {                                            /* the same clouds from the eye's mirror image under the water */
            sh4der_cloudvolume_t c2 = c;
            sh4der_cloudvolume_view_t v2 = v;
            sh4der_cloudvolume_frame_t f2;
            const float sea = c.base_y - SEA_BELOW;

            c2.steps = MIR_STEPS; c2.light_samples = 0; c2.detail = c.detail ? 1u : 0u;
            v2.eye[1] = sea + sea - v.eye[1];
            v2.pitch = -v.pitch;
            v2.tan_x = mir_tan_x = TAN_X * 1.6f;                  /* wide: ripples and the roll look outside the screen's view */
            v2.tan_y = mir_tan_y = TAN_Y * 1.6f;
            if (sh4der_cloudvolume_prepare(&c2, &v2, t, turn, &f2)) {
                const unsigned n2 = sh4der_cloudvolume_march(&maps, &f2, MIR_W, MIR_H, (unsigned)(o.frames == 1 ? 1 : 4), turn, mir_shade_m, mir_alpha_m);
                const double div2 = o.frames == 1 ? 4.0 : 1.0;
                (void)n2;
                sh4der_cloudvolume_smooth(mir_shade_m, mir_alpha_m, MIR_W, MIR_H, mir_shade, mir_alpha);
                mir_cyc = (f2.samples * CYC_SAMPLE + f2.skips * CYC_SKIP + f2.hits * CYC_HIT + f2.noise_reads * CYC_NOISE + f2.rays * CYC_RAY) / div2 + (double)(MIR_W * MIR_H) * (CYC_PIXEL + CYC_SMOOTH);
            }
        }
        if (fr == o.frames - 1) {                                /* the last frame's cost; a single frame is marched whole, so its cost is divided */
            const double div = o.frames == 1 ? (double)o.spread : 1.0;
            rays = f.rays / div; samples = f.samples / div; noise = f.noise_reads / div; marched = n / div; hits = f.hits / div; skips = f.skips / div;
            cyc = samples * CYC_SAMPLE + skips * CYC_SKIP + hits * CYC_HIT + noise * CYC_NOISE + rays * CYC_RAY + (double)(o.w * o.h) * (CYC_PIXEL + (o.smooth ? CYC_SMOOTH : 0.0));
        }
    }

    if (o.smooth) sh4der_cloudvolume_smooth(shade_m, alpha_m, (unsigned)o.w, (unsigned)o.h, shade, alpha);
    else { memcpy(shade, shade_m, sizeof(shade)); memcpy(alpha, alpha_m, sizeof(alpha)); }
    /* the buffer as the texture the PVR would hold */
    {   /* the cloud alpha at the sun: one value for the 565 path */
        float ds[3] = { f.sun[0], f.sun[1], f.sun[2] };
        const float z = ds[0] * f.fwd[0] + ds[1] * f.fwd[1] + ds[2] * f.fwd[2];
        if (z > 0.05f) {
            const float sx = (ds[0] * f.right[0] + ds[1] * f.right[1] + ds[2] * f.right[2]) / z / v.tan_x;
            const float sy = (ds[0] * f.up[0] + ds[1] * f.up[1] + ds[2] * f.up[2]) / z / v.tan_y;
            const int bx = (int)((sx * 0.5f + 0.5f) * (float)o.w), by = (int)((0.5f - sy * 0.5f) * (float)o.h);
            if (bx >= 0 && by >= 0 && bx < o.w && by < o.h) sun_through = 1.0f - (float)alpha[by * o.w + bx] * (1.0f / 255.0f);
        }
    }
    for (y = 0; y < o.h; y++) {
        for (x = 0; x < o.w; x++) {
            const int p = y * o.w + x;
            float *q = tex[p], rgb[3];
            if (o.out == OUT_IDEAL) {
                sh4der_cloudvolume_rgb(shade[p], rgb);
                q[0] = rgb[0]; q[1] = rgb[1]; q[2] = rgb[2]; q[3] = (float)alpha[p] * (1.0f / 255.0f);
            } else if (o.out == OUT_PAL8) {
                const uint32_t e = pal[sh4der_cloudvolume_index(shade[p], alpha[p], (unsigned)x, (unsigned)y)];
                q[0] = (float)((e >> 16) & 255u) * (1.0f / 255.0f); q[1] = (float)((e >> 8) & 255u) * (1.0f / 255.0f);
                q[2] = (float)(e & 255u) * (1.0f / 255.0f); q[3] = (float)(e >> 24) * (1.0f / 255.0f);
            } else {                                             /* 565: the sky composed on the CPU at the buffer's size, then 5 / 6 / 5 bits with the ordered pattern */
                const float sx = (((float)x + 0.5f) * 2.0f / (float)o.w - 1.0f) * v.tan_x, sy = (1.0f - ((float)y + 0.5f) * 2.0f / (float)o.h) * v.tan_y;
                const float a = (float)alpha[p] * (1.0f / 255.0f), b = ((float)sh4der_cloudvolume_bayer_((unsigned)x, (unsigned)y) + 0.5f) * (1.0f / 16.0f) - 0.5f;
                float d[3], sky[3], veil;
                ray_dir(&f, sx, sy, d);
                backdrop(&c, &f, &o, v.eye, d, sky, &veil);
                sh4der_cloudvolume_rgb(shade[p], rgb);
                q[0] = floorf(satf(mixf(sky[0], rgb[0], a)) * 31.0f + 0.5f + b) * (1.0f / 31.0f);
                q[1] = floorf(satf(mixf(sky[1], rgb[1], a)) * 63.0f + 0.5f + b) * (1.0f / 63.0f);
                q[2] = floorf(satf(mixf(sky[2], rgb[2], a)) * 31.0f + 0.5f + b) * (1.0f / 31.0f);
                q[0] = satf(q[0]); q[1] = satf(q[1]); q[2] = satf(q[2]);
                q[3] = 1.0f;
            }
        }
    }

    /* the screen: the quad turned by the roll, filtered, over the backdrop, the sun on top */
    {
        const float cr = cosf(v.roll), sr = sinf(v.roll);
        for (y = 0; y < OUT_H; y++) {
            for (x = 0; x < OUT_W; x++) {
                const float nx = (((float)x + 0.5f) * (2.0f / OUT_W) - 1.0f) * TAN_X, ny = (1.0f - ((float)y + 0.5f) * (2.0f / OUT_H)) * TAN_Y;
                const float sx = nx * cr - ny * sr, sy = nx * sr + ny * cr;       /* the same point before the camera rolled */
                const float bu = (sx / v.tan_x * 0.5f + 0.5f) * (float)o.w - 0.5f, bv = (0.5f - sy / v.tan_y * 0.5f) * (float)o.h - 0.5f;
                float d[3], rgb[3], cl[4], veil;
                int k;

                if (o.filter) {
                    int x0 = (int)floorf(bu), y0 = (int)floorf(bv), x1, y1;
                    const float fx = bu - (float)x0, fy = bv - (float)y0;
                    x1 = x0 + 1; y1 = y0 + 1;
                    x0 = x0 < 0 ? 0 : x0 >= o.w ? o.w - 1 : x0; x1 = x1 < 0 ? 0 : x1 >= o.w ? o.w - 1 : x1;
                    y0 = y0 < 0 ? 0 : y0 >= o.h ? o.h - 1 : y0; y1 = y1 < 0 ? 0 : y1 >= o.h ? o.h - 1 : y1;
                    for (k = 0; k < 4; k++)
                        cl[k] = mixf(mixf(tex[y0 * o.w + x0][k], tex[y0 * o.w + x1][k], fx), mixf(tex[y1 * o.w + x0][k], tex[y1 * o.w + x1][k], fx), fy);
                } else {
                    int x0 = (int)floorf(bu + 0.5f), y0 = (int)floorf(bv + 0.5f);
                    x0 = x0 < 0 ? 0 : x0 >= o.w ? o.w - 1 : x0; y0 = y0 < 0 ? 0 : y0 >= o.h ? o.h - 1 : y0;
                    for (k = 0; k < 4; k++) cl[k] = tex[y0 * o.w + x0][k];
                }
                ray_dir(&f, sx, sy, d);
                if (o.out == OUT_565) {
                    float dummy[3]; backdrop(&c, &f, &o, v.eye, d, dummy, &veil);
                    for (k = 0; k < 3; k++) rgb[k] = cl[k];
                    sun_add(&f, d, sun_through * (1.0f - veil * 0.5f), rgb);
                } else {
                    backdrop(&c, &f, &o, v.eye, d, rgb, &veil);
                    sun_add(&f, d, 1.0f - veil * 0.5f, rgb);
                    for (k = 0; k < 3; k++) rgb[k] = mixf(rgb[k], cl[k], cl[3]);
                }
                for (k = 0; k < 3; k++) out_rgb[(y * OUT_W + x) * 3 + k] = (uint8_t)(satf(rgb[k]) * 255.0f + 0.5f);
            }
        }
    }
    fp = fopen(argv[1], "wb");
    if (!fp) { perror(argv[1]); return 1; }
    fprintf(fp, "P6\n%d %d\n255\n", OUT_W, OUT_H);
    fwrite(out_rgb, 1, sizeof(out_rgb), fp);
    fclose(fp);
    printf("cloudvolume: t %.2f  %dx%d steps %u light %u detail %u spread %d | marched %.0f px  rays %.0f  samples %.0f + %.0f skipped (%.0f hit)  noise reads %.0f | ESTIMATE %.2f M cycles = %.1f ms at 200 MHz | mirror %dx%d %d steps spread 4: %.1f ms | eye y %.0f m\n",
           o.t, o.w, o.h, (unsigned)c.steps, (unsigned)c.light_samples, (unsigned)c.detail, o.spread, marched, rays, samples, skips, hits, noise, cyc * 1e-6, cyc / 200000.0, MIR_W, MIR_H, MIR_STEPS, mir_cyc / 200000.0, v.eye[1]);
    return 0;
}
#endif /* _arch_dreamcast */
