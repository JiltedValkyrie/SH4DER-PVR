/* The demo: a street, a car, a chase camera, and the pl4nar reflection on the car body. Platform-free (gfx.h below it). */
#ifndef DEMO_H
#define DEMO_H
#include <stdint.h>

enum {
    DEMO_BTN_REFLECT = 1u << 0,    /* reflection on / off (X; the keys of every action: controls.h) */
    DEMO_BTN_SIZE    = 1u << 1,    /* next reflection target size */
    DEMO_BTN_MORE    = 1u << 2,    /* F0 up */
    DEMO_BTN_LESS    = 1u << 3,    /* F0 down */
    DEMO_BTN_INFO    = 1u << 4,    /* debug overlay + target thumbnail on / off */
    DEMO_BTN_AUTO    = 1u << 5,    /* slow camera orbit on / off: no key (the autorun's orbit stages set it, demo_auto_set) */
    DEMO_BTN_TOD     = 1u << 6,    /* time of day: AUTO -> DAY -> NIGHT -> PAUSE */
    DEMO_BTN_LAMP_UP = 1u << 7,    /* night gain of the reflection up */
    DEMO_BTN_LAMP_DN = 1u << 8,    /* and down */
    DEMO_BTN_LOOK    = 1u << 9,    /* textures: 64 x 64, 16 colours, point sampled <-> full size, filtered */
    DEMO_BTN_SPLIT   = 1u << 10,   /* the split: the reflection layer on the left of a divider only (B + X) */
    DEMO_BTN_FX      = 1u << 11,   /* the effect labels shown / hidden (B + START) */
    DEMO_BTN_TILES   = 1u << 12,   /* the new map: car shadow tiles on / off, blobs only (B + Y) */
    DEMO_BTN_AO      = 1u << 13,   /* the new map: baked occlusion on / off (B + D-pad left) */
    DEMO_BTN_SUN     = 1u << 14    /* the sun: FIXED -> SWEEP (azimuth, one diagonal to the other) -> HOLD (B + D-pad right) */
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
void  demo_auto_cut(int on);                       /* the mirror pass through its own tiles (tile_cut.h) */
void  demo_auto_info(int on);                      /* the overlay (after demo_auto_set, which turns it off) */
#endif

/* Platform: microseconds since start. */
uint64_t plat_us(void);
#endif
