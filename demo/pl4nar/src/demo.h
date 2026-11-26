/* The demo: a street, a car, a chase camera, and the pl4nar reflection on the car body. Platform-free (gfx.h below it).
 *
 * RECOMMENDED (no console run: nothing below is measured; wiki/limits/header-recommended.md)
 * Start with: the heat gate HEAT_MOVE = 0.02 m/s, HEAT_TURN = 0.005 rad/s, HEAT_STILL = 0.4 s; the fly-by
 *             CINE_LEAD = 1.0 s, CINE_HOLD = 6.0 s, CINE_BLEND = 2.5 s, CINE_FADE = 0.3 s (src/demo.c, README
 *             "The still-camera gate and the fly-by")
 * Cost:       the gate: two vec3 distances and a normalize a frame (ESTIMATE: a few us); the fly-by: none of its
 *             own beyond the heat it shows. The heat's own cost: src/heat.h (ESTIMATE)
 * Status:     ADVISED (NOT RUN); host tested (host/snap_test.c: no heat with a moving eye, 5.2 s in the fly-by)
 * Do:         call demo_draw every frame (the gate reads the frame's eye); keep the heat off while the camera moves
 * Do not:     move the camera in a fly-by while its heat is up; snap the car between autorun stages (demo_auto_place
 *             once, at the start)
 * Maturity:   demo code (not a library part) */
#ifndef DEMO_H
#define DEMO_H
#include <stdint.h>

/* The chase camera (README "Camera and thumbnail"). DRIVE and the wet road: a racing game's, low behind the car,
   looking down by CAM_PITCH (radians), so the horizon sits near 41 % from the top at the 55 degree field of view
   (the horizon's place depends on the pitch alone) and the car's centre near 61 %. The wet road's autorun keeps the
   1.5 m camera of its earlier runs; the measuring autoruns of the map and the hills keep the old high one, so their
   numbers compare with earlier runs. */
#if defined(PL4NAR_DRIVE) || defined(PL4NAR_WET)
#ifndef CAM_HEIGHT
#if defined(PL4NAR_AUTORUN)
#define CAM_HEIGHT   1.5f          /* metres over the car's ground */
#else
#define CAM_HEIGHT   1.9f          /* metres over the car's ground (DRIVE and the live view) */
#endif
#endif
#ifndef CAM_DIST
#define CAM_DIST     6.0f          /* metres behind the car's centre */
#endif
#ifndef CAM_PITCH
#if defined(PL4NAR_AUTORUN)
#define CAM_PITCH    0.10f         /* down, about 6 degrees */
#else
#define CAM_PITCH    0.09f         /* down, about 5 degrees */
#endif
#endif
#else
#define CAM_HEIGHT   3.3f          /* the old camera: looks at a point 1.2 m ahead of the car, 1 m up */
#define CAM_DIST     7.0f
#endif
/* The reflection target in the tech overlay only (START; hidden in DRIVE until then), as two small insets down the
   right edge, neither over the other: the mirror on top, the car shadow tiles under it (the map builds). Pixels. */
#ifndef THUMB_MIRROR_W
#define THUMB_MIRROR_W 128.0f      /* the mirror's width; its height from the mirror's own shape */
#endif
#ifndef THUMB_TILES_W
#define THUMB_TILES_W  96.0f       /* the tiles' region's width; its height from the region's shape */
#endif
#ifndef THUMB_RIGHT
#define THUMB_RIGHT    620.0f      /* both insets' right edge */
#endif
#ifndef THUMB_TOP
#ifdef PL4NAR_MAP
#define THUMB_TOP      20.0f       /* the mirror's top: the map builds have nothing at the top right */
#else
#define THUMB_TOP      124.0f      /* the mirror's top: under the street's map */
#endif
#endif
#ifndef THUMB_GAP
#define THUMB_GAP      24.0f       /* under an inset: its label, then the next */
#endif

enum {
    DEMO_BTN_REFLECT = 1u << 0,    /* reflection on / off (X; the keys of every action: controls.h) */
    DEMO_BTN_SIZE    = 1u << 1,    /* next reflection target size */
    DEMO_BTN_MORE    = 1u << 2,    /* F0 up */
    DEMO_BTN_LESS    = 1u << 3,    /* F0 down */
    DEMO_BTN_INFO    = 1u << 4,    /* debug overlay + target thumbnail on / off */
    DEMO_BTN_RESTART = 1u << 5,    /* DRIVE: the car back at the start, the clock and the laps from 0 (A, overlay closed) */
    DEMO_BTN_TOD     = 1u << 6,    /* time of day: AUTO -> DAY -> NIGHT -> PAUSE */
    DEMO_BTN_LAMP_UP = 1u << 7,    /* night gain of the reflection up */
    DEMO_BTN_LAMP_DN = 1u << 8,    /* and down */
    DEMO_BTN_LOOK    = 1u << 9,    /* textures: 64 x 64, 16 colours, point sampled <-> full size, filtered */
    DEMO_BTN_SPLIT   = 1u << 10,   /* the split: the reflection layer on the left of a divider only (B + X) */
    DEMO_BTN_FX      = 1u << 11,   /* the effect labels shown / hidden (B + START) */
    DEMO_BTN_TILES   = 1u << 12,   /* the new map: car shadow tiles on / off, blobs only (B + Y) */
    DEMO_BTN_AO      = 1u << 13,   /* the new map: baked occlusion on / off (B + D-pad left) */
    DEMO_BTN_SUN     = 1u << 14,   /* the sun: FIXED -> SWEEP (azimuth, one diagonal to the other) -> HOLD (B + D-pad right) */
    DEMO_BTN_RAIN    = 1u << 15,   /* the wet road build: the rain DRY -> DAMP -> WET (Y there) */
    DEMO_BTN_HEAT    = 1u << 16,   /* the heat shimmer: OFF -> LIGHT -> STRONG (X; by day, dry: heat.h) */
    DEMO_BTN_HIDE    = 1u << 17,   /* the whole HUD hidden / shown (B + Y); START shows it again */
    DEMO_BTN_CINE    = 1u << 18    /* the fly-by shot: a fixed camera low at the kerb ahead, the car sweeps past (B + X) */
};

typedef struct demo_input {
    float    steer;                /* -1 left .. 1 right */
    float    gas, brake;           /* 0 .. 1 */
    float    orbit;                /* -1 .. 1: camera yaw rate round the car */
    int      split_l, split_r;     /* held: the divider steps left / right (B + LT / RT) */
    uint32_t pressed;              /* DEMO_BTN_*: went down this frame */
} demo_input_t;

typedef struct demo_stats {        /* filled by the platform, shown by the overlay */
    uint32_t cpu_us;               /* update + submit, waits for the PVR taken out */
    uint32_t pvr_us;               /* last render the PVR reported */
    uint32_t rtt_us;               /* the reflection pass on the PVR, sampled (0 = not measured) */
    uint32_t fps_x10;
    uint32_t ram_free_kb, vram_free_kb;
} demo_stats_t;

void demo_init(void);
void demo_update(const demo_input_t *in, float dt);
void demo_draw(const demo_stats_t *st);
int  demo_reflect_on(void);
int  demo_info_on(void);           /* the overlay is open (DRIVE: the tech keys are live, controls.h ctl_when) */
#ifdef PL4NAR_WET
/* A lit pool on a wet road looks shinier (the mirror's streak), not brighter: its alpha scaled down by the rain, in percent. By eye, not measured. */
#ifndef WET_POOL_DIM_DAMP
#define WET_POOL_DIM_DAMP 80
#endif
#ifndef WET_POOL_DIM_WET
#define WET_POOL_DIM_WET  60
#endif
void demo_auto_rain(unsigned rain, int streak, int pool_dim);    /* the wet road: rain 0 dry, 1 damp, 2 wet; streak 0: the stretch off; pool_dim 0: pools at full alpha */
#endif

/* The autorun (main.c, PL4NAR_AUTORUN): set a stage, read what the last frame drew. */
typedef struct demo_counts {
    uint32_t tris_main, tris_refl;     /* triangles sent: screen scene, mirror scene */
    uint32_t lookup_us;                /* the reflection lookup over the body (0 with no layer) */
    uint32_t tw, th, target_bytes;     /* the reflection target */
    float    night;                    /* 0 day .. 1 night */
    int      refl_ok;                  /* the layer was drawn */
    float    draw_back, draw_fwd;      /* the drawn range behind and ahead of the camera, m */
    int      ao;                       /* the baked occlusion was on */
    uint32_t tiles;                    /* car shadow tiles drawn (the new map, by day with the reflection on); 0 elsewhere */
    uint32_t tiles_band;               /* the hills: of them, cars in the fade band (40 .. 50 m) */
} demo_counts_t;
int   demo_auto_set(int reflect, unsigned size, unsigned tod_mode, int orbit);   /* size 0 256x128, 1 128x64, 2 512x256; tod 1 DAY, 2 NIGHT; -1: no video memory */
float demo_speed(void);
void  demo_auto_place(float z, float x, float v);   /* the car at z along the street (s on the loop), x across, speed v */
void  demo_counts(demo_counts_t *c);
void  demo_auto_tiles(int on);
void  demo_auto_ao(int on);
void  demo_auto_lod(float m);                      /* the new map: a building past m (its footprint's edge) is its box; 0: LOD_NEAR */
void  demo_auto_split(int on);                     /* the split at its middle (the reflection layer on the left half only), or off */
#ifdef PL4NAR_HILLS
void  demo_auto_sun(unsigned mode);                /* the hills build: the sun 0 FIXED, 1 SWEEP (from its start), 2 HOLD */
void  demo_auto_tile_set(int small);               /* the shadow tiles: 0 four of 128 x 128, 1 eight small (the c8 set) */
void  demo_auto_cut(int on);                       /* the mirror pass through its own tiles (sh4der_tilecut.h) */
void  demo_auto_info(int on);                      /* the overlay (after demo_auto_set, which turns it off) */
#endif
void  demo_auto_heat(unsigned level);              /* the heat shimmer: 0 OFF, 1 LIGHT, 2 STRONG (heat.h; by day, dry) */
void  demo_auto_hex(int on, int gate_free);        /* the heat as hexagons (heat_hex.h) in place of the layer, their band in the
                                                      target (a resize); gate_free: drawn under a moving camera too. Not wet */
unsigned demo_hex_band_tris(void);                 /* triangles sent a second time into the band, the last frame */
void  demo_auto_beam(int on);                      /* the headlights' fade by distance (sh4der_pool_beam, the lamps' reach): 0 the
                                                      two wedges (the start, unless -DPL4NAR_HEAD_BEAM), 1 the beam. Shows only
                                                      where the camera is far from the car (the fly-by). NOT RUN on a console */
void  demo_auto_cine(void);                        /* the fly-by shot from here (CINE_TOTAL s, then the follow camera again) */
int   demo_cine_on(void);                          /* the fly-by shot is running */
void  demo_heat_copy(uint32_t *us, uint32_t *copies);   /* the heat's last copy time and copies made (0, 0 off the Dreamcast) */

/* Platform: microseconds since start. */
uint64_t plat_us(void);
#endif
