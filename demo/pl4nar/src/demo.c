/* The demo scene. World: right-handed, Y up, metres; the street runs along +Z and repeats every PERIOD.
 * Frame: (1) the world mirrored about the road plane into the reflection target (pl4nar), (2) the screen: opaque world +
 * car, then one translucent layer on the car's paint and glass with the target as its texture and Fresnel as vertex alpha.
 * SH4ZAM for all maths: XMTRX chains (screen x perspective x lookat x translate x rotate), FTRV per vertex, shz_sincosf,
 * shz_vec3_*, shz_lerpf / shz_saturatef / shz_fmaf. Hand arithmetic is scalar a * b + c only (named where it is). */
#include <stdio.h>
#include <string.h>
#include "demo.h"
#include "gfx.h"
#include "hud.h"
#include "controls.h"
#include "assets.h"
#include "assets_gen.h"
#include "sh4der_pl4nar.h"
#include "sh4der_spark.h"
#include "sh4der_pool.h"
#include "heat.h"                                 /* the heat shimmer over the road's far end (Senior: day, dry) */
#include "../../psm/psm_split.h"                   /* the divider's tile columns and stepping */
#ifdef PL4NAR_MAP
#include "../../psm/psm_effects.h"                 /* the effect labels' text */
#include "pl4nar_fx.h"
#endif
#ifdef PL4NAR_MAP                                  /* the new map (tools/map.py, src/map.c): a closed loop in place of the street */
#include "map.h"
#include "map_gen.h"
#include "map_ao_gen.h"                            /* tools/map_ao.c: a byte a model vertex, the road's patches */
#ifdef PL4NAR_HILLS
#include "hills.h"                                 /* the hills build: the height field, the meshes' heights */
#endif
#define MAP_DRAW     200.0f        /* drawn round the camera, m (the haze is full by then) */
#define MAP_MIRROR   80.0f         /* mirrored round the camera */
#define LOD_NEAR     50.0f         /* a building past this is its box (lod_init): the screen pass (demo_auto_lod) */
#define LOD_MIRROR   15.0f         /* and in the mirror pass */
#define TRAFFIC_DRAW 150.0f        /* traffic drawn round the camera */
#define CAR_NEAR     40.0f         /* a traffic car past this is its box */
#define CAR_MIRROR   30.0f         /* traffic mirrored round the camera (s7: 60) */
#define MAP_AO_DRAW  120.0f        /* the road's occlusion patches round the camera: past it the haze has them */
#define MAP_CAP_NIGHT 0.50f        /* the layer's alpha at night at most: the strips over the paint keep its colour */
#define TILE_N       8u            /* car shadow tiles: 4 x 2 in the right half of the reflection target */
#define TILE_M       10.0f         /* a tile's side on the ground, m */
#define TILE_NEAR    50.0f         /* traffic nearer than this to the car may have a tile */
#define TILE_Z       1.0e4f        /* the tiles' depth key in the mirror pass: in front of anything mirrored (z = 1 / w) */
#ifdef PL4NAR_HILLS
/* The hills build: the mirror at the top left, the tiles' region (256 x 256) to its right; one target, a power of two
   each way: 512 x 256 for a mirror of 128 x 64 or 256 x 128 (256 KB), 1024 x 256 for 512 x 256 (512 KB). The pass's
   tile cut keeps the top left (w + 256) x 256: 12 x 8 = 96 tiles at the default (tile_cut.h's most). */
#define RT_TILES_W   256u
#define RT_TILES_H   256u
#define RT_W(w)      ((w) + RT_TILES_W <= 512u ? 512u : 1024u)
#define RT_H(h)      ((h) > RT_TILES_H ? (h) : RT_TILES_H)
#define TILE_FADE_NEAR 40.0f       /* a tile's shadow is whole within this of the camera (the client's tuning) */
#define TILE_FADE_FAR  50.0f       /* and gone at this: a tile is given up only here, so none pops */
#define TILE_FADE_IN   0.5f        /* s: a tile given to a car inside the band grows in over this */
#define LAMP_FADE_W    10.0f       /* the lamps' pools and halos fade over the last this of their reach */
#define BLOB_REACH     80.0f       /* the blob discs: within this of the car (as before), fading over the last 10 m */
#else
#define RT_W(w)      ((w) * 2u)    /* the target: the mirror on the left, the tiles on the right */
#define RT_H(h)      (h)
#endif
#if MAP_MAX_NV > MDL_MAX_NV
#define SCRATCH_MAP_NV MAP_MAX_NV  /* the map's largest model is larger than the bake's */
#endif
#else
#define RT_W(w)      (w)
#define RT_H(h)      (h)
#endif

/* ------------------------------------------------------------------------------------------------ world numbers */
#define ROAD_HALF    8.0f          /* asphalt half width */
#define BLD_X        12.0f         /* building line (asphalt + 4 m of sidewalk): the road texture's half width */
#define BLOCK        96.0f
#define NBLOCK       4
#define PERIOD       (BLOCK * NBLOCK)
#define CROSS        12.0f         /* cross street width */
#define ROAD_TILE    24.0f         /* the road texture is 24 m x 24 m */
#define FACADE_TILE  12.8f         /* a facade texture is 4 bays x 4 floors of 3.2 m */
/* The drawn range round the camera along z; the two add to PERIOD. 48 / 336 is the chase camera's split. An orbiting
   camera looks back as far as ahead: -DDRAW_BACK=160.0f -DDRAW_FWD=224.0f moves 112 m behind. What that costs sideways
   (buildings seen across a cross street) is not measured: the autorun prints the triangle counts of both. */
#ifndef DRAW_BACK
#define DRAW_BACK    48.0f
#endif
#ifndef DRAW_FWD
#define DRAW_FWD     336.0f
#endif
_Static_assert(DRAW_BACK + DRAW_FWD == PERIOD, "DRAW_BACK + DRAW_FWD must be PERIOD: every z has one image in the drawn range");
#define LANE_MAX     5.2f          /* the car's |x| limit: the kerb lane stays free for parked cars */
#define MAX_BLD      112
#define MAX_PROP     64
#ifdef SCRATCH_MAP_NV
#define SCRATCH_N    SCRATCH_MAP_NV
#else
#define SCRATCH_N    MDL_MAX_NV    /* the largest model of the bake */
#endif
#define FOV_MAIN     0.96f         /* 55 degrees, vertical */
static float s_amb = 0.56f, s_dif = 0.50f;        /* the light of the hour (tod_eval) */
#define AMBIENT      s_amb
#define DIFFUSE      s_dif
#ifdef PL4NAR_MAP                                  /* the map ends at MAP_DRAW: the haze is full (HAZE_MAX) there */
#define HAZE_START   70.0f
#define HAZE_INV     (1.0f / 130.0f)
#else
#define HAZE_START   90.0f
#define HAZE_INV     (1.0f / 320.0f)
#endif
#define HAZE_MAX     0.88f
static uint32_t s_haze = 0xffc4ced6u;
#define COL_HAZE     s_haze
#define COL_LAMP     0x00ffc070u   /* street lamps: sodium */
#define COL_HEAD     0x00fff0c8u   /* headlights */
#define COL_TAIL     0x00d81818u
#define LAMP_NEAR    60.0f         /* street lamps light the road and glow within this of the car */
#ifdef PL4NAR_WET
#define LAMP_MIRROR  90.0f         /* the wet road: the road carries the lamps far up the street */
#else
#define LAMP_MIRROR  40.0f         /* and are drawn in the mirror pass within this */
#endif
#define LAMP_HEAD_Y  7.45f
#define LAMP_ARM     2.6f          /* the lamp head hangs this far out over the road */
#define COL_ASPHALT  0xff383836u
#define COL_ROOF     0xff4a4a50u
#define COL_GROUND_REFLECT 0xff2c2c2eu   /* what a ray into the road sees: dark asphalt */
#define COL_GLASS    0xff1c2430u

typedef struct bld { float x0, x1, z0, z1, h; uint8_t mat, side, pad[2]; } bld_t;
typedef struct prop { const mdl_t *body, *wheels; float x, z, yaw; } prop_t;

#ifndef PL4NAR_MAP
static bld_t   s_bld[MAX_BLD];
static unsigned s_nbld;
#endif
static prop_t  s_prop[MAX_PROP];
static unsigned s_nprop;
static float   s_dome[5][12][3];                 /* unit directions: 4 rings of 12 + the zenith in [4][0] */
static uint32_t s_dome_col[5] = { 0xff8c949cu, 0xffb8cadcu, 0xff86aedau, 0xff4a80c8u, 0xff2c58a8u };
static float s_sun[3] = { 0.4472f, 0.6200f, 0.6442f };           /* unit: ahead, right-hand side of the street, 38 degrees up by day */

/* The hour. One clock (0 .. 1, a 45 s loop) drives the sky, the haze, the light's colour and strength, the sun's
   height and the lamps. No layer: the tint goes into the vertex colours the frame computes anyway. */
typedef struct tod_key { uint32_t dome[5], haze, light, sunc; float amb, dif, elev, night; } tod_key_t;
static const tod_key_t s_tod_key[3] = {
    { { 0xff8c949cu, 0xffb8cadcu, 0xff86aedau, 0xff4a80c8u, 0xff2c58a8u }, 0xffc4ced6u, 0xffffffffu, 0xfffffcecu, 0.56f, 0.50f, 38.0f, 0.0f },   /* day */
    { { 0xff6a5450u, 0xffe89a5cu, 0xffb47c7cu, 0xff5c5c92u, 0xff2a3068u }, 0xffb48c70u, 0xffffc8a0u, 0xffffb060u, 0.46f, 0.44f, 7.0f, 0.55f },    /* dusk, dawn */
    { { 0xff0a0e16u, 0xff141c2eu, 0xff0e1628u, 0xff080e1eu, 0xff040814u }, 0xff0e141eu, 0xff6878a0u, 0xffd0d8ecu, 0.30f, 0.12f, 30.0f, 1.0f }     /* night (the disc is the moon) */
};
static const struct { float t; unsigned key; } s_tod_knot[7] = { { 0.0f, 0u }, { 0.35f, 0u }, { 0.45f, 1u }, { 0.55f, 2u }, { 0.85f, 2u }, { 0.93f, 1u }, { 1.0f, 0u } };
static const char *const s_tod_name[4] = { "AUTO", "DAY", "NIGHT", "PAUSE" };
#ifdef PL4NAR_WET
#ifdef PL4NAR_MAP
#error "the wet road is the street's (a straight flat road): not with PL4NAR_MAP"
#endif
static float    s_tod = 0.70f;                    /* the wet road: night, always (Y is the rain there) */
static unsigned s_tod_mode = 2u;
static unsigned s_rain = 2u;                       /* the rain key (Y in this build) */
static int      s_wet_streak = 1;                  /* the stretch on (the autorun's "no stretch" stage turns it off) */
static uint32_t s_wet_cpu_us;
static int      s_wet_pool_dim = 1;                /* the pools dimmed by the rain (the autorun's "pools undimmed" stage turns it off) */
void demo_auto_rain(unsigned rain, int streak, int pool_dim) { s_rain = rain % 3u; s_wet_streak = streak; s_wet_pool_dim = pool_dim; }
static float pool_dim(void) {
    static const float d[3] = { 1.0f, WET_POOL_DIM_DAMP * 0.01f, WET_POOL_DIM_WET * 0.01f };
    return s_wet_pool_dim ? d[s_rain] : 1.0f;
}
#else
static float pool_dim(void) { return 1.0f; }
static float    s_tod = 0.10f;                    /* the clock */
static unsigned s_tod_mode;                       /* 0 AUTO, 1 DAY, 2 NIGHT, 3 PAUSE */
#endif
/* The sun's sweep (B + D-pad right): FIXED is the old sun (azimuth 34.8 degrees, its two scales below); SWEEP takes the
   azimuth from one diagonal (45) to the other (225) and back, SUN_SWEEP_S a whole loop, at the hour's elevation; HOLD
   stops it where it is. Everything lit or shadowed reads s_sun each frame: the models, the boxes, the tiles, the disc. */
#define SUN_SWEEP_S 26.0f
#define SUN_AZ0 45.0f
#define SUN_AZ1 225.0f
static const char *const s_sun_name[3] = { "FIXED", "SWEEP", "HOLD" };
static unsigned s_sun_mode;                       /* 0 FIXED, 1 SWEEP, 2 HOLD */
static float    s_sun_ph;                         /* 0 .. 1: the sweep's loop */
static uint32_t s_light = 0xffffffffu, s_sunc = 0xfffffcecu, s_sun_sky = 0xff6e9cccu, s_road_col = 0xffffffffu;
static float    s_night, s_lamp;                  /* 0 day .. 1 night; the lamps' ramp */
static unsigned s_lamp_k;                         /* s_lamp as 0 .. 256 */
#ifdef PL4NAR_MAP                                  /* the look the owner chose (pictures_v2c): a short reach, more F0 and gain */
static float    s_lamp_gain = 3.0f;               /* reflection layer gain at night (the lamps on the paint) */
static float    s_day_gain = 0.9f, s_reach = 9.0f;
#else
static float    s_lamp_gain = 2.0f;               /* reflection layer gain at night (the lamps on the paint) */
static float    s_day_gain = 1.0f, s_reach = SH4DER_PL4NAR_REACH;
#endif
static sh4der_spark_view_t s_sv;                  /* this pass's camera for the halos */

static gfx_vert_t s_scr[SCRATCH_N];               /* model scratch */
/* sized by the bake (assets_gen.h): a model that outgrows an array is refused, never written past it */
static gfx_vert_t s_body[MDL_PLAYER_BODY_NV];     /* the player's body, opaque pass */
static gfx_vert_t s_refl[MDL_PLAYER_BODY_NV];     /* the same vertices for the reflection layer */
static float      s_wp[MDL_PLAYER_BODY_NV][3], s_wn[MDL_PLAYER_BODY_NV][3], s_wsn[MDL_PLAYER_BODY_NV][3];   /* world: position, flat normal (light), smoothed normal (reflection) */
static unsigned   s_refused;                      /* models refused for size: logged once each kind */

/* car + camera */
/* x, z, h: the car in the street's frame (z along it, x across, h from its direction). On the new map z is s along
   the loop and x the offset across it. wx, wz, wh: the car in the world; wcam: the camera's yaw before the orbit. */
static struct { float x, z, h, v, cam_h, orbit, clock, wx, wz, wh, wcam; } s_car;

/* The fly-by shot (B + X, an autorun stage): the camera fixed low at the kerb CINE_LEAD s of driving ahead of the car,
   looking down the road at its far end, where the heat sits; the car sweeps past close and drives away into the
   shimmer. No follow: the camera holds till CINE_HOLD s, then eases into the follow camera over CINE_BLEND s
   (smoothstep on both points; both look forward, so it is a dolly, no turn, no snap). The heat is on (CINE_HEAT)
   through it, by day and dry (the gate still holds). A look back at the car was tried on the host: the car left the
   frame at 3.6 s and the way back to the follow camera was a half turn. */
#define CINE_LEAD   1.0f                          /* s: the car reaches the camera */
#define CINE_HOLD   4.5f                          /* s: the camera starts after it (the car about 50 m on, in the heat) */
#define CINE_BLEND  2.5f                          /* a dolly forward, at most about 1.6 m a frame (snap_test) */
#define CINE_TOTAL  (CINE_HOLD + CINE_BLEND)
#define CINE_LAT    7.0f                          /* m from the centre line: past the lanes (5.2), inside the kerb (8) */
#define CINE_EYE_Y  0.7f                          /* m over the road: low */
#define CINE_LOOK   60.0f                         /* m on down the road: where the camera looks */
#define CINE_AT_LAT 0.35f                         /* x the car's lane: the slight angle toward it */
#define CINE_HEAT   2u                            /* STRONG */
static struct { int on; float t; shz_vec3_t eye, at; } s_cine;
#if defined(PL4NAR_HILLS) || defined(PL4NAR_DRIVE)
static int      s_reflect = 1, s_info = 0, s_auto;  /* the overlay off at boot: its text costs (START shows it) */
#else
static int      s_reflect = 1, s_info = 1, s_auto;
#endif
static int      s_low = 1;                         /* the low look: 64 x 64, 16-colour, point sampled textures (A switches) */
static unsigned s_size;                            /* index into s_sizes */
#ifdef PL4NAR_MAP
static float    s_f0 = 0.25f;                      /* paint F0: 0.05 is physical; flat-shaded art wants far more */
#else
static float    s_f0 = 0.14f;                      /* paint F0: 0.05 is physical, the default leans on the clear coat */
#endif
#ifdef PL4NAR_WET
static const uint16_t s_sizes[3][2] = { { 256, 128 }, { 128, 64 }, { 64, 32 } };   /* the wet road: 128 x 64 at the start */
#else
static const uint16_t s_sizes[3][2] = { { 256, 128 }, { 128, 64 }, { 512, 256 } };
#endif
static uint32_t s_refl_cpu_us, s_tris_main, s_tris_refl;

/* per pass */
static shz_vec4_t s_vp[4];                         /* rows of screen x projection x view (x mirror) of the pass being drawn */
#define VP_LOAD() shz_xmtrx_load_rows_4x4(&s_vp[0], &s_vp[1], &s_vp[2], &s_vp[3])
static float s_vw, s_vh, s_focal, s_camz, s_camx;
#ifdef PL4NAR_MAP
static uint32_t s_tint = 0xffffffffu;              /* draw_model: this instance's tint */
static const uint8_t *s_mao;                       /* draw_model: this model's baked occlusion, a byte a vertex (NULL none) */
static int s_ao = 1;                               /* the baked occlusion on (the autorun's "no AO" stage) */
void demo_auto_ao(int on) { s_ao = on; }
#endif
#ifdef PL4NAR_HILLS
static float s_my, s_mp, s_mr;                     /* draw_model / draw_box: the base's height, the pitch and roll (rise a metre) */
static float s_cy, s_cp, s_cr;                     /* the player's car: the same, smoothed */
static float s_aoy[MAP_AOG_ROWS][MAP_AOG_COLS];    /* the road's occlusion patches: the ground under each corner */
#define MODEL_Y s_my
#define CAR_Y   s_cy
#define HILLS_SNAP() (s_cp = -1e9f)                /* the next demo_place takes the tilt whole */
#else
#define HILLS_SNAP() ((void)0)
#define MODEL_Y 0.0f
#define CAR_Y   0.0f
#endif
static unsigned s_heat_lvl = 0u;                   /* the heat shimmer: 0 OFF, 1 LIGHT, 2 STRONG (X); OFF at the start everywhere
                                                      (the owner: it swims like a texture with no depth test) until it is clean */
/* The heat's gate (heat.h: 0 in rain, gone by night 0.25) x the key's level. */
static inline float heat_level(void) {
#ifdef PL4NAR_WET
    const unsigned rain = s_rain;
#else
    const unsigned rain = 0u;                     /* no rain in this build */
#endif
    return heat_gate(s_night, rain) * (float)(s_cine.on && s_heat_lvl < CINE_HEAT ? CINE_HEAT : s_heat_lvl) * 0.5f;   /* the fly-by: on */
}
static unsigned s_haze_max = (unsigned)(HAZE_MAX * 256.0f);   /* this pass: the reflection pass keeps more contrast (less haze) */
static sh4der_pl4nar_view_t s_pv;
static int s_pv_ok;                               /* this frame's reflection view was built: the layer may be drawn */
#ifdef PL4NAR_HILLS
static sh4der_pl4nar_plane_t s_road_plane = { { 0.0f, 1.0f, 0.0f }, 0.0f };   /* fitted under the car each frame (road_plane_fit) */
#else
static const sh4der_pl4nar_plane_t s_road_plane = { { 0.0f, 1.0f, 0.0f }, 0.0f };
#endif
#ifdef PL4NAR_MAP
/* Car shadow tiles: drawn in the mirror pass into the target's right half, read on the screen by a ground quad a car. */
static int      s_tiles = 1;                       /* the switch (the autorun's "no tiles" stage); hills: 1 ON, 2 TYPICAL, 0 OFF */
static unsigned s_tile_n;                          /* this frame's tiles; 0: blobs only */
static int      s_tile_car[TILE_N];                /* -1 the player, else the traffic index */
static float    s_tile_c[TILE_N][2];               /* the tile's centre on the ground (x, z) */
static float    s_tile_px;                         /* a tile's side in target pixels */
void demo_auto_tiles(int on) { s_tiles = on; }
#ifdef PL4NAR_HILLS
void demo_auto_sun(unsigned mode) { s_sun_mode = mode % 3u; s_sun_ph = 0.0f; }
#endif
#ifdef PL4NAR_HILLS
static float s_lamp_s[MAP_LAMPS];                  /* each lamp head's s on the loop */
#ifdef PL4NAR_BLOBS
static float s_blob_s;                             /* blob(): the car's s on the loop */
#endif
static float s_tb_y, s_tb_p, s_tb_r;               /* tile_box(): the car's base height, pitch and roll */
static float s_tile_s[TILE_N], s_tile_y[TILE_N];   /* each tile's car: s on the loop, its ground height */
/* The tiles as a list of slots, each its own place and size in the tiles' region (32 px units from the region's top
   left, the mirror's right edge) and a kind, so a 64 or 32 px tile for a small caster can join the list later. The
   default: four of 128 x 128, the player and the three nearest cars. The small set (8 of 32 px at 128 x 64, the c8
   layout: its units scale with the mirror's height / 64) is the cost stage's (demo_auto_tile_set, -DPL4NAR_TILES8). */
typedef struct tile_slot { uint8_t x, y, w, h, kind; } tile_slot_t;
#define TILE_KIND_CAR 0u
static const tile_slot_t s_slots_big[4] = { { 0, 0, 4, 4, TILE_KIND_CAR }, { 4, 0, 4, 4, TILE_KIND_CAR }, { 0, 4, 4, 4, TILE_KIND_CAR }, { 4, 4, 4, 4, TILE_KIND_CAR } };
static const tile_slot_t s_slots_small[8] = { { 0, 0, 1, 1, TILE_KIND_CAR }, { 1, 0, 1, 1, TILE_KIND_CAR }, { 2, 0, 1, 1, TILE_KIND_CAR }, { 3, 0, 1, 1, TILE_KIND_CAR },
                                              { 0, 1, 1, 1, TILE_KIND_CAR }, { 1, 1, 1, 1, TILE_KIND_CAR }, { 2, 1, 1, 1, TILE_KIND_CAR }, { 3, 1, 1, 1, TILE_KIND_CAR } };
#ifdef PL4NAR_TILES8
static int s_tiles_small = 1;
#else
static int s_tiles_small;
#endif
static int   s_slot_car[TILE_N];                   /* a slot's car, kept from frame to frame: -1 the player (slot 0), -2 none */
static float s_slot_in[TILE_N];                    /* a slot's grow-in, 0 .. 1 */
static float s_tile_fade[TILE_N];                  /* this frame's strength a slot */
static float s_slot_clock;                         /* the last frame's clock, for the grow-in */
static unsigned s_tile_live;                       /* tiles with a car this frame */
static unsigned s_tile_band;                       /* of them, cars in the fade band (TILE_FADE_NEAR .. _FAR) */
static int   s_cut_on;                             /* the mirror pass through its own tiles (tile_cut.h): a stage */
static unsigned tile_slots(void) { return s_tiles_small ? 8u : 4u; }
/* Slot k in target pixels: its top left and side. */
static void tile_rect(unsigned k, float *x0, float *y0, float *T) {
    const tile_slot_t *t = s_tiles_small ? &s_slots_small[k] : &s_slots_big[k];
    const float u = 32.0f * (s_tiles_small ? (float)s_sizes[s_size][1] * (1.0f / 64.0f) : 1.0f);
    *x0 = (float)s_sizes[s_size][0] + u * (float)t->x; *y0 = u * (float)t->y; *T = u * (float)t->w;
}
void demo_auto_tile_set(int small) { unsigned k; s_tiles_small = small; for (k = 0; k < TILE_N; k++) s_slot_car[k] = -2; }
void demo_auto_cut(int on) { s_cut_on = on; }
void demo_auto_info(int on) { s_info = on; }
/* The cut for this frame's mirror pass, in 32 px tiles from the target's top left: the mirror and, when tiles were
   drawn last frame, the tiles' region to its right (4 x 128: 256 x 256; 8 small: 4 x 2 of their units). 0 x 0: off. */
static void tile_cut_size(unsigned *cw, unsigned *ch) {
    const unsigned tw = s_sizes[s_size][0], th = s_sizes[s_size][1];
    const unsigned u = s_tiles_small ? th / 2u : 0u, rw = s_tile_live ? (s_tiles_small ? 4u * u : RT_TILES_W) : 0u;
    const unsigned rh = s_tile_live ? (s_tiles_small ? 2u * u : RT_TILES_H) : 0u;
    *cw = s_cut_on ? (tw + rw + 31u) / 32u : 0u;
    *ch = s_cut_on ? ((th > rh ? th : rh) + 31u) / 32u : 0u;
}
#endif
#endif
void demo_auto_heat(unsigned level) { s_heat_lvl = level % 3u; }
static void cine_start(void);
void demo_auto_cine(void) { cine_start(); }
int demo_cine_on(void) { return s_cine.on; }
void demo_heat_copy(uint32_t *us, uint32_t *copies) {
#ifdef _arch_dreamcast
    *us = s_heat.copy_us; *copies = s_heat.copies;
#else
    *us = 0u; *copies = 0u;
#endif
}

int demo_reflect_on(void) { return s_reflect; }
int demo_info_on(void) { return s_info; }
#ifdef PL4NAR_DRIVE
/* DRIVE's laps (the map: a lap is the loop's start line crossed forward; the street has none). */
static unsigned s_laps;                            /* laps done since the start */
static float    s_lap_t0, s_lap_last, s_lap_best;  /* the clock at the last crossing; the last lap and the best, 0: none */
#endif

/* The split: the reflection layer drawn inside the left half's user clip only, the right half the flat paint. */
static int          s_split;
static unsigned     s_div = SPLIT_COLS / 2u;
static split_step_t s_split_step;
void demo_auto_split(int on) { s_split = on; s_div = split_middle(SPLIT_COLS); }
static int          s_fx_hidden;                  /* the labels: B + START; off while the autorun measures */
static int          s_hud_hidden;                 /* the whole HUD (B + Y): the picture alone; START shows it again */
static unsigned     s_blob_n;                     /* blob discs drawn this frame (the labels) */

static void demo_place(float dt);
#ifdef PL4NAR_MAP
static void lod_init(void);
#endif

/* The autorun's hold on the demo: what a stage sets, what it reads. */
int demo_auto_set(int reflect, unsigned size, unsigned tod_mode, int orbit) {
    int r = 0;
    s_reflect = reflect;
    s_tod_mode = tod_mode % 4u;
    s_auto = orbit;
    s_info = 0;                                    /* the overlay's text is not part of what is measured */
    s_fx_hidden = 1;                               /* nor the labels (psm_effects.h: OFF in measured stages) */
    s_hud_hidden = 0;                              /* each stage starts with the HUD the autorun shows */
    if (size % 3u != s_size) { s_size = size % 3u; r = gfx_reflect_resize(RT_W(s_sizes[s_size][0]), RT_H(s_sizes[s_size][1])); }
    return r;
}
float demo_speed(void) { return s_car.v; }
void demo_auto_place(float z, float x, float v) {
    s_car.z = z; s_car.x = x; s_car.h = 0.0f; s_car.cam_h = 0.0f; s_car.orbit = 0.0f; s_car.v = v;
    HILLS_SNAP();
    demo_place(0.0f);
}
void demo_counts(demo_counts_t *c) {
#ifdef PL4NAR_WET
    c->tris_main = s_tris_main; c->tris_refl = s_tris_refl; c->lookup_us = s_pv_ok ? s_refl_cpu_us + (s_rain && s_reflect ? s_wet_cpu_us : 0u) : 0u;
#else
    c->tris_main = s_tris_main; c->tris_refl = s_tris_refl; c->lookup_us = s_pv_ok ? s_refl_cpu_us : 0u;
#endif
    c->tw = s_sizes[s_size][0]; c->th = s_sizes[s_size][1]; c->target_bytes = gfx_reflect_bytes();
    c->night = s_night; c->refl_ok = s_pv_ok; c->draw_back = DRAW_BACK; c->draw_fwd = DRAW_FWD;
#ifdef PL4NAR_MAP
#ifdef PL4NAR_HILLS
    c->tiles = s_tile_live; c->tiles_band = s_tile_band;
#else
    c->tiles = s_tile_n; c->tiles_band = 0u;
#endif
    c->ao = s_ao;
#else
    c->ao = 0;
    c->tiles = 0u; c->tiles_band = 0u;
#endif
}

/* ------------------------------------------------------------------------------------------------ init */
#ifndef PL4NAR_MAP                                 /* the street's own layout */
static uint32_t s_rng = 0x1234abcdu;
static unsigned rnd(unsigned n) { s_rng = s_rng * 1664525u + 1013904223u; return (s_rng >> 10) % n; }

static void add_bld(float x0, float x1, float z0, float z1, float h, unsigned mat, unsigned side) {
    if (s_nbld < MAX_BLD) { bld_t *b = &s_bld[s_nbld++]; b->x0 = x0; b->x1 = x1; b->z0 = z0; b->z1 = z1; b->h = h; b->mat = (uint8_t)mat; b->side = (uint8_t)side; }
}
static void add_prop(const mdl_t *body, const mdl_t *wheels, float x, float z, float yaw) {
    if (s_nprop < MAX_PROP) { prop_t *p = &s_prop[s_nprop++]; p->body = body; p->wheels = wheels; p->x = x; p->z = z; p->yaw = yaw; }
}
#endif

#ifdef PL4NAR_HILLS
/* The mirror plane under the car: through the ground point under it, its normal from the eased pitch and roll (the
   tilt kept, the camera's up still the world's). n = F x R, F = forward + up x pitch, R = right + up x roll. */
static void road_plane_fit(void) {
    const shz_sincos_t hc = shz_sincosf(s_car.wh);
    const shz_vec3_t f = shz_vec3_init(hc.sin, s_cp, hc.cos), r = shz_vec3_init(hc.cos, s_cr, -hc.sin);
    const shz_vec3_t n = shz_vec3_normalize(shz_vec3_cross(f, r));
    s_road_plane.n[0] = n.x; s_road_plane.n[1] = n.y; s_road_plane.n[2] = n.z;
    s_road_plane.d = shz_fmaf(n.x, s_car.wx, shz_fmaf(n.y, s_cy, n.z * s_car.wz));
}
#endif

/* The car's world pose and the camera's yaw from the street-frame state. */
static void demo_place(float dt) {
#ifdef PL4NAR_MAP
    const map_pose_t p = map_at(s_car.z, s_car.x);
    s_car.wx = p.x; s_car.wz = p.z; s_car.wh = p.yaw + s_car.h; s_car.wcam = p.yaw + s_car.cam_h;
#ifdef PL4NAR_HILLS
    {   /* the ground's pose under the car: the height as it is (eased, a fast car would sink or float by v x slope x the
           time), the tilt eased over 0.25 s (the mirror plane follows it: no wobble from facet to facet) */
        const hills_tilt_t t = hills_car(s_car.z, s_car.x, s_car.wh);
        s_cy = t.y;
        if (s_cp < -1e8f) { s_cp = t.pitch; s_cr = t.roll; }   /* a new pose (demo_init, a jump): taken whole */
        else {
            const float kt = shz_saturatef(dt * (1.0f / 0.25f));
            s_cp = shz_lerpf(s_cp, t.pitch, kt); s_cr = shz_lerpf(s_cr, t.roll, kt);
        }
    }
#else
    (void)dt;
#endif
#else
    (void)dt;
    s_car.wx = s_car.x; s_car.wz = s_car.z; s_car.wh = s_car.h; s_car.wcam = s_car.cam_h;
#endif
}

void demo_init(void) {
    static const float ring_deg[4] = { -9.0f, 0.0f, 13.0f, 38.0f };
    unsigned side, b, r, k;
    for (r = 0; r < 4u; r++)
        for (k = 0; k < 12u; k++) {
            const shz_sincos_t e = shz_sincosf_deg(ring_deg[r]), a = shz_sincosf_deg(30.0f * (float)k);
            s_dome[r][k][0] = e.cos * a.sin; s_dome[r][k][1] = e.sin; s_dome[r][k][2] = e.cos * a.cos;    /* hand: scalar products */
        }
    s_dome[4][0][0] = 0.0f; s_dome[4][0][1] = 1.0f; s_dome[4][0][2] = 0.0f;
#ifdef PL4NAR_MAP
    (void)side; (void)b;
    map_traffic_reset();
    lod_init();
#else
    for (side = 0; side < 2u; side++) {
        const float sgn = side ? 1.0f : -1.0f;
        for (b = 0; b < NBLOCK; b++) {
            float z = (float)b * BLOCK + CROSS * 0.5f;
            const float end = (float)(b + 1u) * BLOCK - CROSS * 0.5f;
            while (z < end - 0.1f) {
                float w = 12.8f + 3.2f * (float)rnd(5u);
                const float depth = 14.0f + 2.0f * (float)rnd(5u), h = 3.2f * (float)(3u + rnd(11u));
                if (end - (z + w) < 12.8f) w = end - z;
                if (side) add_bld(BLD_X, BLD_X + depth, z, z + w, h, rnd(4u), side);
                else      add_bld(-BLD_X - depth, -BLD_X, z, z + w, h, rnd(4u), side);
                z += w;
            }
            /* a building across the far end of each cross street closes the view */
            add_bld(sgn * 96.0f - 10.0f, sgn * 96.0f + 10.0f, (float)(b + 1u) * BLOCK - 22.0f, (float)(b + 1u) * BLOCK + 22.0f,
                    3.2f * (float)(6u + rnd(6u)), rnd(4u), side);
            /* street furniture: lamps both sides, poles (wires) on the right-hand side, a signal at the cross street */
            for (k = 0; k < 3u; k++)
                add_prop(&mdl_lamp, NULL, sgn * (ROAD_HALF + 0.9f), (float)b * BLOCK + 16.0f + 32.0f * (float)k, side ? 1.5708f : -1.5708f);
            if (side == 0u)
                for (k = 0; k < 3u; k++) add_prop(&mdl_pole, NULL, -(BLD_X - 0.7f), (float)b * BLOCK + 32.0f * (float)k + 30.0f, 1.5708f);
            add_prop(&mdl_signal, NULL, sgn * (ROAD_HALF + 0.6f), (float)(b + 1u) * BLOCK - CROSS * 0.5f - 1.0f, side ? 3.1416f : 0.0f);
        }
    }
    /* parked cars in the kerb lanes (the left side of a right-handed +Z street is +X) */
    add_prop(&mdl_taxi_body, &mdl_taxi_wheels, 6.7f, 40.0f, 0.0f);
    add_prop(&mdl_van_body, &mdl_van_wheels, -6.7f, 70.0f, 3.1416f);
    add_prop(&mdl_police_body, &mdl_police_wheels, 6.7f, 150.0f, 0.0f);
    add_prop(&mdl_taxi_body, &mdl_taxi_wheels, -6.7f, 215.0f, 3.1416f);
    add_prop(&mdl_van_body, &mdl_van_wheels, 6.7f, 262.0f, 0.0f);
    add_prop(&mdl_police_body, &mdl_police_wheels, -6.7f, 330.0f, 3.1416f);
#endif
    s_car.x = -2.0f; s_car.z = 8.0f; s_car.h = 0.0f; s_car.v = 0.0f; s_car.cam_h = 0.0f; s_car.orbit = 0.0f; s_car.clock = 0.0f;   /* the start (DRIVE's restart too) */
#ifdef PL4NAR_WET
    s_size = 1u;                                   /* the wet road: 128 x 64 at the start; B+A cycles 64 x 32, 256 x 128 */
#endif
#ifdef PL4NAR_HILLS
    hills_init();
    {
        unsigned r, c;
        for (r = 0; r < MAP_AOG_ROWS; r++)
            for (c = 0; c < MAP_AOG_COLS; c++) s_aoy[r][c] = hills_y_xz(map_aog[r][c][0], map_aog[r][c][1]);
    }
    s_size = 1u;                                   /* 128 x 64 from c9 on (the owner's choice); B+A still cycles */
    {
        unsigned i;
        for (i = 0; i < MAP_LAMPS; i++) s_lamp_s[i] = hills_s_of(map_lamp[i][0], map_lamp[i][1]);
    }
#endif
    HILLS_SNAP();
    demo_place(0.0f);
    gfx_reflect_resize(RT_W(s_sizes[s_size][0]), RT_H(s_sizes[s_size][1]));
}

/* ------------------------------------------------------------------------------------------------ update */
void demo_update(const demo_input_t *in, float dt) {
    float rate;
    if (in->pressed & DEMO_BTN_REFLECT) s_reflect = !s_reflect;
    if (in->pressed & DEMO_BTN_HIDE) s_hud_hidden = !s_hud_hidden;
    else if ((in->pressed & DEMO_BTN_INFO) && s_hud_hidden) s_hud_hidden = 0;   /* START brings the HUD back as it was */
    else if (in->pressed & DEMO_BTN_INFO) s_info = !s_info;
    if (in->pressed & DEMO_BTN_MORE) s_f0 = shz_fminf(s_f0 + 0.02f, 0.60f);
    if (in->pressed & DEMO_BTN_LESS) s_f0 = shz_fmaxf(s_f0 - 0.02f, 0.02f);
    if (in->pressed & DEMO_BTN_LAMP_UP) s_lamp_gain = shz_fminf(s_lamp_gain + 0.25f, 4.0f);
    if (in->pressed & DEMO_BTN_LAMP_DN) s_lamp_gain = shz_fmaxf(s_lamp_gain - 0.25f, 0.5f);
    if (in->pressed & DEMO_BTN_LOOK) s_low = !s_low;
#ifndef PL4NAR_WET
    if (in->pressed & DEMO_BTN_TOD) s_tod_mode = (s_tod_mode + 1u) % 4u;
#endif
    if (in->pressed & DEMO_BTN_SUN) {
        s_sun_mode = (s_sun_mode + 1u) % 3u;
        if (s_sun_mode == 1u && s_tod_mode != 2u) s_tod_mode = 1u;   /* a sweep wants the sun up: to DAY (NIGHT stays as asked) */
    }
    if (s_sun_mode == 1u) { s_sun_ph += dt * (1.0f / SUN_SWEEP_S); if (s_sun_ph >= 1.0f) s_sun_ph -= 1.0f; }
    if (s_tod_mode == 0u) s_tod += dt * (1.0f / 45.0f);
    else if (s_tod_mode < 3u) {                    /* DAY, NIGHT: the clock runs the short way round to that hour, 0.55 of a turn in 1.6 s */
        float d = (s_tod_mode == 1u ? 0.15f : 0.70f) - s_tod;
        if (d > 0.5f) d -= 1.0f;
        if (d < -0.5f) d += 1.0f;
        s_tod += shz_clampf(d, -dt * 0.34f, dt * 0.34f);
    }
    if (s_tod >= 1.0f) s_tod -= 1.0f;
    if (s_tod < 0.0f) s_tod += 1.0f;
    if (in->pressed & DEMO_BTN_SPLIT) s_split = !s_split;
#ifdef PL4NAR_WET
    if (in->pressed & DEMO_BTN_RAIN) s_rain = (s_rain + 1u) % 3u;
#endif
    if (in->pressed & DEMO_BTN_FX) s_fx_hidden = !s_fx_hidden;
    if (in->pressed & DEMO_BTN_HEAT) s_heat_lvl = (s_heat_lvl + 1u) % 3u;
    if (in->pressed & DEMO_BTN_CINE) cine_start();
    if (s_cine.on) { s_cine.t += dt; if (s_cine.t >= CINE_TOTAL) s_cine.on = 0; }
#ifdef PL4NAR_MAP
#ifdef PL4NAR_HILLS
#ifdef PL4NAR_BLOBS
    if (in->pressed & DEMO_BTN_TILES) s_tiles = s_tiles == 1 ? 2 : s_tiles == 2 ? 0 : 1;   /* ON -> TYPICAL -> OFF */
#endif
#else
    if (in->pressed & DEMO_BTN_TILES) s_tiles = !s_tiles;
#endif
    if (in->pressed & DEMO_BTN_AO) s_ao = !s_ao;
#endif
    s_div = split_step(&s_split_step, s_div, SPLIT_COLS, in->split_l, in->split_r);
    if (in->pressed & DEMO_BTN_SIZE) {
        s_size = (s_size + 1u) % 3u;
        gfx_reflect_resize(RT_W(s_sizes[s_size][0]), RT_H(s_sizes[s_size][1]));
    }
    /* arcade car: each step is a x b + c, one FMAC (shz_fmaf); the rates are constants x dt */
    if (in->gas > 0.05f)        s_car.v = shz_fmaf(in->gas, 13.0f * dt, s_car.v);
    if (in->brake > 0.05f)      s_car.v = shz_fmaf(in->brake, -(s_car.v > 0.5f ? 26.0f : 7.0f) * dt, s_car.v);
    s_car.v = shz_fmaf(s_car.v, -0.22f * dt, s_car.v);
    s_car.v = shz_clampf(s_car.v, -8.0f, 46.0f);
    rate = 1.25f * shz_saturatef(shz_fabsf(s_car.v) * (1.0f / 5.0f)) * shz_invf(1.0f + shz_fabsf(s_car.v) * (1.0f / 28.0f));
    s_car.h = shz_fmaf(in->steer * rate, s_car.v < 0.0f ? dt : -dt, s_car.h);   /* right-handed: a right turn lowers the heading */
    s_car.h = shz_clampf(s_car.h, -0.85f, 0.85f);
    {
        const shz_sincos_t sc = shz_sincosf(s_car.h);
        s_car.x = shz_fmaf(sc.sin, s_car.v * dt, s_car.x);
        s_car.z = shz_fmaf(sc.cos, s_car.v * dt, s_car.z);
    }
    if (s_car.x > LANE_MAX)  { s_car.x = LANE_MAX;  if (s_car.h > 0.0f) s_car.h -= s_car.h * shz_saturatef(4.0f * dt); }
    if (s_car.x < -LANE_MAX) { s_car.x = -LANE_MAX; if (s_car.h < 0.0f) s_car.h -= s_car.h * shz_saturatef(4.0f * dt); }
#ifdef PL4NAR_MAP
    s_car.z = map_wrap(s_car.z);
    map_traffic_step(dt);
#else
    if (s_car.z >= PERIOD) s_car.z -= PERIOD;
    if (s_car.z < 0.0f)    s_car.z += PERIOD;
#endif
#if defined(PL4NAR_DRIVE) && defined(PL4NAR_MAP)
    {   /* a lap: the start line (s 0) crossed forward; backward over it takes the lap back, with no time */
        static float prev_z = 8.0f;
        if (prev_z > MAP_LOOP_LEN * 0.75f && s_car.z < MAP_LOOP_LEN * 0.25f) {
            const float t = s_car.clock - s_lap_t0;
            if (s_laps) { s_lap_last = t; if (s_lap_best <= 0.0f || t < s_lap_best) s_lap_best = t; }   /* the first crossing ends the run up from s 8 */
            s_laps++; s_lap_t0 = s_car.clock;
        } else if (prev_z < MAP_LOOP_LEN * 0.25f && s_car.z > MAP_LOOP_LEN * 0.75f && s_laps) s_laps--;
        prev_z = (in->pressed & DEMO_BTN_RESTART) ? 8.0f : s_car.z;
    }
#endif
#ifdef PL4NAR_DRIVE
    if (in->pressed & DEMO_BTN_RESTART) {          /* the car as demo_init puts it; the hour, the keys' settings and the traffic stay */
        s_car.x = -2.0f; s_car.z = 8.0f; s_car.h = 0.0f; s_car.v = 0.0f; s_car.cam_h = 0.0f; s_car.orbit = 0.0f; s_car.clock = 0.0f;
        s_laps = 0u; s_lap_t0 = s_lap_last = s_lap_best = 0.0f;
        s_cine.on = 0;
        HILLS_SNAP();
    }
#endif
    s_car.cam_h = shz_lerpf(s_car.cam_h, s_car.h, shz_saturatef(3.5f * dt));
    s_car.orbit += (in->orbit * 1.8f + (s_auto ? 0.35f : 0.0f)) * dt;
    if (s_car.orbit > 3.14159f)  s_car.orbit -= 6.28318f;
    if (s_car.orbit < -3.14159f) s_car.orbit += 6.28318f;
    if (!s_auto && shz_fabsf(in->orbit) < 0.05f && shz_fabsf(s_car.v) > 4.0f)  /* driving: the camera eases back behind the car */
        s_car.orbit -= shz_clampf(s_car.orbit * shz_saturatef(1.2f * dt), -0.9f * dt, 0.9f * dt);   /* at most 0.9 rad/s: no whip
                                                                                                  round from half a turn */
    s_car.clock += dt;
    demo_place(dt);
}

/* A road point: s along the street or the loop, lat across (the car's x), up over the ground there. */
static shz_vec3_t cine_pt(float s, float lat, float up) {
#ifdef PL4NAR_MAP
    const map_pose_t p = map_at(map_wrap(s), lat);
#ifdef PL4NAR_HILLS
    return shz_vec3_init(p.x, hills_y_xz(p.x, p.z) + up, p.z);
#else
    return shz_vec3_init(p.x, up, p.z);
#endif
#else
    return shz_vec3_init(lat, up, s);
#endif
}
static void cine_start(void) {
    const float s = s_car.z + shz_fmaxf(s_car.v, 8.0f) * CINE_LEAD, side = s_car.x < 0.0f ? -1.0f : 1.0f;
    s_cine.eye = cine_pt(s, side * CINE_LAT, CINE_EYE_Y);
    s_cine.at = cine_pt(s + CINE_LOOK, s_car.x * CINE_AT_LAT, 1.0f);
    s_cine.t = 0.0f; s_cine.on = 1;
}

/* ------------------------------------------------------------------------------------------------ helpers */
static inline void haze_tex(gfx_vert_t *v) {       /* textured: base x (1 - f), offset = haze x f */
    const unsigned t = (unsigned)(shz_saturatef((v->w - HAZE_START) * HAZE_INV) * (float)s_haze_max);
    v->oargb = gfx_scale(COL_HAZE, t);
    v->argb = gfx_scale(v->argb, 256u - t);
}
static inline void haze_flat(gfx_vert_t *v) {      /* untextured: the PVR has no offset colour there */
    const unsigned t = (unsigned)(shz_saturatef((v->w - HAZE_START) * HAZE_INV) * (float)s_haze_max);
    v->argb = gfx_mix(v->argb, COL_HAZE, t);
    v->oargb = 0u;
}
static inline uint32_t grey(float k) { const unsigned g = (unsigned)(shz_saturatef(k) * 255.0f); return gfx_rgb(255u, g, g, g); }
static inline uint32_t lit(uint32_t c) { return gfx_mod(c, s_light); }       /* the hour's light colour on a surface */

/* The sky's colour at an elevation (degrees), between the dome's rings. */
static uint32_t dome_at(float deg) {
    static const float ring[5] = { -9.0f, 0.0f, 13.0f, 38.0f, 90.0f };
    unsigned r = 0u;
    while (r < 3u && deg >= ring[r + 1u]) r++;
    return gfx_mix(s_dome_col[r], s_dome_col[r + 1u], (unsigned)(shz_saturatef(shz_divf(deg - ring[r], ring[r + 1u] - ring[r])) * 256.0f));
}

static void tod_eval(void) {
    unsigned i = 0u, k, m;
    const tod_key_t *a, *b;
    float f, elev;
    shz_sincos_t sc;
    while (i < 5u && s_tod >= s_tod_knot[i + 1u].t) i++;
    a = &s_tod_key[s_tod_knot[i].key]; b = &s_tod_key[s_tod_knot[i + 1u].key];
    f = shz_saturatef(shz_divf(s_tod - s_tod_knot[i].t, s_tod_knot[i + 1u].t - s_tod_knot[i].t));
    m = (unsigned)(f * 256.0f);
    for (k = 0; k < 5u; k++) s_dome_col[k] = gfx_mix(a->dome[k], b->dome[k], m);
    s_haze = gfx_mix(a->haze, b->haze, m);
    s_light = gfx_mix(a->light, b->light, m);
    s_sunc = gfx_mix(a->sunc, b->sunc, m);
    s_amb = shz_lerpf(a->amb, b->amb, f);
    s_dif = shz_lerpf(a->dif, b->dif, f);
    s_night = shz_lerpf(a->night, b->night, f);
    elev = shz_lerpf(a->elev, b->elev, f);
    sc = shz_sincosf_deg(elev);
    if (s_sun_mode == 0u) { s_sun[0] = 0.5702f * sc.cos; s_sun[1] = sc.sin; s_sun[2] = 0.8214f * sc.cos; }   /* hand: two scales */
    else {                                         /* SWEEP / HOLD: there and back, a triangle wave, eased at the ends */
        const float tri = s_sun_ph < 0.5f ? 2.0f * s_sun_ph : 2.0f - 2.0f * s_sun_ph;
        const float e = tri * tri * (3.0f - 2.0f * tri);
        const shz_sincos_t az = shz_sincosf_deg(shz_lerpf(SUN_AZ0, SUN_AZ1, e));
        s_sun[0] = az.sin * sc.cos; s_sun[1] = sc.sin; s_sun[2] = az.cos * sc.cos;
    }
    s_sun_sky = dome_at(elev);
    s_road_col = lit(grey(shz_fmaf(s_dif, s_sun[1], s_amb) * (1.0f / 0.87f)));               /* the flat ground: 1 at noon */
    s_lamp = shz_saturatef((s_night - 0.3f) * 2.5f);
    s_lamp_k = (unsigned)(s_lamp * 256.0f);
}

/* z of something in the repeating street nearest to the camera's z */
static inline float wrap_z(float z) {
#ifdef PL4NAR_MAP
    return z;                                      /* the loop is closed: nothing repeats */
#else
    float d = z - s_camz;
    while (d > DRAW_FWD) d -= PERIOD;              /* DRAW_BACK + DRAW_FWD = PERIOD: every z has one image in the drawn range */
    while (d <= -DRAW_BACK) d += PERIOD;
    return s_camz + d;
#endif
}

/* 1 = a sphere (centre c, radius r) may be on the target. Leaves XMTRX = s_vp. */
static int sphere_seen(float x, float y, float z, float r) {
    gfx_vert_t c;
    c.x = c.y = 0.0f;
    VP_LOAD();
    gfx_xform(&c, shz_vec3_init(x, y, z));
#ifdef PL4NAR_MAP
    if (c.w < -r || c.w > MAP_DRAW + CAM_DIST + r) return 0;
#else
    if (c.w < -r || c.w > DRAW_FWD + CAM_DIST + r) return 0;
#endif
    if (c.w > r + GFX_NEAR) {
        const float rp = r * s_focal * shz_invf(c.w);         /* hand: scalar products */
        if (c.x + rp < 0.0f || c.x - rp > s_vw || c.y + rp < 0.0f || c.y - rp > s_vh) return 0;
    }
    return 1;
}

static void quad(const gfx_vert_t *q) { gfx_tri(&q[0], &q[1], &q[2]); gfx_tri(&q[0], &q[2], &q[3]); }

/* ------------------------------------------------------------------------------------------------ sky */
static void draw_sky(shz_vec3_t eye) {
    static gfx_vert_t v[4][12], top, sun[21];
    unsigned r, k;
    const float R = 420.0f;
    VP_LOAD();
    for (r = 0; r < 4u; r++)
        for (k = 0; k < 12u; k++) {
            gfx_xform(&v[r][k], shz_vec3_init(shz_fmaf(s_dome[r][k][0], R, eye.x), shz_fmaf(s_dome[r][k][1], R, eye.y), shz_fmaf(s_dome[r][k][2], R, eye.z)));
            v[r][k].argb = s_dome_col[r]; v[r][k].oargb = 0u; v[r][k].u = v[r][k].v = 0.0f;
        }
    gfx_xform(&top, shz_vec3_init(eye.x, eye.y + R, eye.z));
    top.argb = s_dome_col[4]; top.oargb = 0u; top.u = top.v = 0.0f;
    gfx_bind(GFX_MAT_FLAT);
    gfx_cull(GFX_CULL_NONE);
    for (k = 0; k < 12u; k++) {
        const unsigned n = (k + 1u) % 12u;
        for (r = 0; r < 3u; r++) { gfx_tri(&v[r][k], &v[r][n], &v[r + 1u][n]); gfx_tri(&v[r][k], &v[r + 1u][n], &v[r + 1u][k]); }
        gfx_tri(&v[3][k], &v[3][n], &top);
    }
    {   /* the sun: a white core and a halo that ends in the sky's colour (opaque gouraud, no alpha) */
        const shz_vec3_t d = shz_vec3_init(s_sun[0], s_sun[1], s_sun[2]);
        const shz_vec3_t a = shz_vec3_normalize(shz_vec3_cross(d, shz_vec3_init(0.0f, 1.0f, 0.0f)));
        const shz_vec3_t b = shz_vec3_cross(a, d);
        const shz_vec3_t c = shz_vec3_add(eye, shz_vec3_scale(d, R * 0.96f));
        gfx_xform(&sun[0], c);
        sun[0].argb = s_sunc; sun[0].oargb = 0u; sun[0].u = sun[0].v = 0.0f;
        for (k = 0; k < 10u; k++) {
            const shz_sincos_t sc = shz_sincosf_deg(36.0f * (float)k);
            const shz_vec3_t dir = shz_vec3_add(shz_vec3_scale(a, sc.cos), shz_vec3_scale(b, sc.sin));
            gfx_xform(&sun[1u + k], shz_vec3_add(c, shz_vec3_scale(dir, 13.0f)));
            gfx_xform(&sun[11u + k], shz_vec3_add(c, shz_vec3_scale(dir, 44.0f)));
            sun[1u + k].argb = gfx_mix(s_sunc, s_sun_sky, 48u); sun[11u + k].argb = s_sun_sky;
            sun[1u + k].oargb = sun[11u + k].oargb = 0u;
            sun[1u + k].u = sun[1u + k].v = sun[11u + k].u = sun[11u + k].v = 0.0f;
        }
        for (k = 0; k < 10u; k++) {
            const unsigned n = (k + 1u) % 10u;
            gfx_tri(&sun[0], &sun[1u + k], &sun[1u + n]);
            gfx_tri(&sun[1u + k], &sun[11u + k], &sun[11u + n]);
            gfx_tri(&sun[1u + k], &sun[11u + n], &sun[1u + n]);
        }
    }
}

/* ------------------------------------------------------------------------------------------------ road + buildings */
#ifdef PL4NAR_MAP
/* The ground: 20 m cells (the park in the middle green), then the loop's road band, a quad a path step. Both share
   their corners: a corner is transformed once (a row of the grid at a time; the road's last pair is the next quad's
   first), not once a quad. */
#define GROUND_NX 18u
#define GROUND_NZ 26u
static void draw_road(void) {
    static gfx_vert_t row[2][GROUND_NX + 1u];
    gfx_vert_t q[4], prev[2];
    unsigned gx, gz, k;
    int have = 0;
    gfx_bind(GFX_MAT_FLAT);
    VP_LOAD();
    for (gz = 0; gz <= GROUND_NZ; gz++) {
        gfx_vert_t *cur = row[gz & 1u], *last = row[(gz & 1u) ^ 1u];
        const float z1 = -260.0f + 20.0f * (float)gz, dz = z1 - 10.0f - s_camz;
        int any = 0;
        for (gx = 0; gx < GROUND_NX; gx++) {                                /* a row is needed when one of its cells is */
            const float dx = -170.0f + 20.0f * (float)gx - s_camx;
            if (dx * dx + dz * dz <= MAP_DRAW * MAP_DRAW || (dz + 20.0f) * (dz + 20.0f) + dx * dx <= MAP_DRAW * MAP_DRAW) { any = 1; break; }   /* hand: scalar products */
        }
        if (!any) { have = 0; continue; }
#ifdef PL4NAR_HILLS
        for (gx = 0; gx <= GROUND_NX; gx++) gfx_xform(&cur[gx], shz_vec3_init(-180.0f + 20.0f * (float)gx, hills_gy[gz][gx], z1));
#else
        for (gx = 0; gx <= GROUND_NX; gx++) gfx_xform(&cur[gx], shz_vec3_init(-180.0f + 20.0f * (float)gx, -0.03f, z1));
#endif
        if (have)
            for (gx = 0; gx < GROUND_NX; gx++) {
                const float x0 = -180.0f + 20.0f * (float)gx, x1 = x0 + 20.0f, z0 = z1 - 20.0f;
                const float cx = x0 + 10.0f - s_camx;
                const int park = x0 >= -40.0f && x1 <= 40.0f && z0 >= -40.0f && z1 <= 40.0f;
                if (cx * cx + dz * dz > MAP_DRAW * MAP_DRAW) continue;            /* hand: scalar products */
                q[0] = cur[gx]; q[1] = cur[gx + 1u]; q[2] = last[gx + 1u]; q[3] = last[gx];
                for (k = 0; k < 4u; k++) { q[k].argb = gfx_mod(park ? 0xff4a6e3cu : 0xff6a6862u, s_road_col); q[k].u = q[k].v = 0.0f; haze_flat(&q[k]); }
                quad(q);
            }
        have = 1;
    }
    gfx_bind(s_low ? GFX_MAT_P_ROAD : GFX_MAT_ROAD);
    have = 0;
    for (k = 0; k + 3u < MAP_ROAD_V; k += 2u) {
        const float *a = map_road[k], *b = map_road[k + 1u], *c = map_road[k + 2u], *d = map_road[k + 3u];
        const float cx = a[0] - s_camx, cz = a[1] - s_camz;
        unsigned i;
        if (cx * cx + cz * cz > MAP_DRAW * MAP_DRAW) { have = 0; continue; }
#ifdef PL4NAR_HILLS
        const float *ry = &hills_ry[k];               /* the heights of a, b, c, d */
#else
        static const float ry[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
#endif
        if (have) { q[3] = prev[0]; q[2] = prev[1]; }
        else {
            gfx_xform(&q[2], shz_vec3_init(b[0], ry[1], b[1]));
            gfx_xform(&q[3], shz_vec3_init(a[0], ry[0], a[1]));
        }
        gfx_xform(&q[0], shz_vec3_init(c[0], ry[2], c[1]));
        gfx_xform(&q[1], shz_vec3_init(d[0], ry[3], d[1]));
        prev[0] = q[0]; prev[1] = q[1]; have = 1;                             /* untouched: the next quad's a, b */
        q[0].u = c[2]; q[0].v = c[3]; q[1].u = d[2]; q[1].v = d[3]; q[2].u = b[2]; q[2].v = b[3]; q[3].u = a[2]; q[3].v = a[3];
        for (i = 0; i < 4u; i++) { q[i].argb = s_road_col; haze_tex(&q[i]); }
        quad(q);
    }
}
#else
#ifdef PL4NAR_WET
/* The wet road (a one-off, the street at night in rain): a grid in the plane round the car, ahead, behind and to the
   building lines, each vertex looked up in the mirror pass's target (sh4der_pl4nar_flat) and drawn in the translucent
   list as ONE added layer (GFX_MAT_REFLECT_ADD: texel x vertex alpha) over the whole dry road, which the rain darkens.
   The grid is a soft pool centred on the car: full within WET_FULL m along the road and to the kerbs, then a smoothstep
   to 0 at WET_AHEAD m ahead, WET_BEHIND m behind and the building lines, so there is no water line anywhere, and the orbit
   camera looking back sees it behind the car too. A vertex
   behind the camera has no lookup: its share is 0 (sh4der_pl4nar_flat). Rain: 0 dry (no grid), 1 damp, 2 wet. The
   streak: v pulled toward the horizon's row, so a lamp's image runs toward the viewer. The speckle: a texel of jitter
   a vertex, new 15 times a second (rain on the film). */
#define WET_COLS     13u           /* x -12 .. 12 m, 2 m apart: the building lines (alpha 0), the kerbs, the lanes */
#define WET_BACK     14u           /* rows behind the car, even: WET_BEHIND / 14 = 2.9 m */
#define WET_FWD      20u           /* rows ahead, even: WET_AHEAD / 20 = 3.5 m */
#define WET_ROWS     (WET_BACK + 1u + WET_FWD)
#define WET_BEHIND   40.0f         /* m behind the car where the pool reaches 0 */
#define WET_AHEAD    70.0f         /* m ahead */
#define WET_FULL     15.0f         /* m each way along the road at full strength */
#define WET_KERB     8.0f          /* |x|: full across to the kerbs, 0 at the building lines (12) */
#define WET_LIFT     0.01f         /* m over the road, against depth fighting (the pools are at 0.02 .. 0.03) */
#define GRIT_LIFT    0.015f        /* the grit layer over the reflection */
static const float s_wet_gain[3]    = { 0.0f, 1.0f, 2.2f };     /* the reflection's share x the Fresnel (WET_ROAD, f0 raised) */
static const float s_wet_stretch[3] = { 1.0f, 1.5f, 2.6f };     /* v toward the horizon: 1 none */
static const float s_wet_dark[3]    = { 1.0f, 0.75f, 0.5f };    /* the road's own tone kept: water darkens it */
#define GRIT_DRY     1.00f         /* the road layer's share (tex_grit's alpha x this): dry, on the plain road */
#define GRIT_DAMP    0.80f         /* damp: the reflection comes through the whole layer a little more */
#define GRIT_WET     0.55f         /* wet: more again, the road still read through it (owner's figures, by eye on the host) */
static const float s_grit_alpha[3]  = { GRIT_DRY, GRIT_DAMP, GRIT_WET };
static uint32_t wet_hash(uint32_t a, uint32_t b) {
    uint32_t h = a * 0x9e3779b1u ^ (b + 0x7f4a7c15u) * 0x85ebca77u;
    h ^= h >> 15; h *= 0x2c1b3c6du; h ^= h >> 12;
    return h;
}

/* The grid round the car, in the translucent list (VP_LOAD after it). */
static void wet_grid(void) {
    static float pos[WET_ROWS][WET_COLS][3];
    static gfx_vert_t gv[WET_ROWS][WET_COLS];
    static float fade[WET_ROWS][WET_COLS];
    sh4der_pl4nar_material_t mat = sh4der_pl4nar_preset(SH4DER_PL4NAR_WET_ROAD);
    const float us = gfx_reflect_u_scale(), iw = shz_invf((float)s_sizes[s_size][0]), ih = shz_invf((float)s_sizes[s_size][1]);
    const float k = s_wet_streak ? shz_invf(s_wet_stretch[s_rain]) : 1.0f, gain = s_wet_gain[s_rain];
    const uint32_t tick = (uint32_t)(s_car.clock * 15.0f);
    const uint64_t t0 = plat_us();
    const int refl = s_rain && s_pv_ok && s_reflect;  /* dry, or no target: the grit alone, no lookup */
    float vh = 0.0f;
    unsigned r, c;
    mat.f0 = 0.15f;                                   /* the preset's 0.02 is water head on; wet asphalt at night wants more (tuned by eye on the host) */
    for (r = 0; r < WET_ROWS; r++) {                  /* row WET_BACK is under the car; even rows each way */
        const float dz = r < WET_BACK ? -WET_BEHIND * (float)(WET_BACK - r) * (1.0f / (float)WET_BACK)
                                      : WET_AHEAD * (float)(r - WET_BACK) * (1.0f / (float)WET_FWD);
        const float tz = shz_saturatef((shz_fabsf(dz) - WET_FULL) * shz_invf((dz < 0.0f ? WET_BEHIND : WET_AHEAD) - WET_FULL));
        for (c = 0; c < WET_COLS; c++) {
            const float x = -12.0f + 2.0f * (float)c;
            const float tx = shz_saturatef((shz_fabsf(x) - WET_KERB) * (1.0f / (12.0f - WET_KERB)));
            const float d = shz_fminf(shz_sqrtf(shz_fmaf(tz, tz, tx * tx)), 1.0f);
            pos[r][c][0] = x; pos[r][c][1] = 0.0f; pos[r][c][2] = s_car.z + dz;
            fade[r][c] = 1.0f - d * d * (3.0f - 2.0f * d);   /* 1 - smoothstep: the pool's soft edge */
        }
    }
    if (refl) sh4der_pl4nar_begin(&s_pv);            /* XMTRX = the reflection view until VP_LOAD */
    if (refl) {   /* the horizon's row in the target: a point far down the road */
        const float far[3] = { s_car.wx, 0.0f, s_camz + 5000.0f };
        sh4der_pl4nar_sample_t smp;
        (void)sh4der_pl4nar_flat(&s_pv, far, &mat, &smp);
        vh = smp.v;
    }
    for (r = 0; r < WET_ROWS; r++) {
        for (c = 0; c < WET_COLS; c++) {
            sh4der_pl4nar_sample_t smp;
            gfx_vert_t *o = &gv[r][c];
            const uint32_t h = wet_hash(c * 64u + (uint32_t)(int32_t)(pos[r][c][2] * 0.5f), tick)   /* rows behind z 0 are negative */;
            const float edge = fade[r][c];
            if (!refl) continue;
            (void)sh4der_pl4nar_flat(&s_pv, pos[r][c], &mat, &smp);
            o->u = shz_saturatef(shz_fmaf((float)(h & 255u) - 127.5f, 0.5f * (1.0f / 128.0f) * iw, smp.u)) * us;   /* hand: a jitter of half a texel */
            o->v = shz_saturatef(shz_fmaf(smp.v - vh, k, vh) + ((float)((h >> 8) & 255u) - 127.5f) * (0.3f / 128.0f) * ih);
            o->argb = gfx_rgb((unsigned)(shz_saturatef(smp.fresnel * gain * edge) * 255.0f), 255u, 255u, 255u);
#ifdef WET_SHOW                                       /* host check only: the target at full strength */
            o->argb = gfx_rgb((unsigned)(edge * 255.0f), 255u, 255u, 255u);
#endif
        }
    }
    VP_LOAD();
    if (refl) {
        for (r = 0; r < WET_ROWS; r++)
            for (c = 0; c < WET_COLS; c++) {
                gfx_vert_t *o = &gv[r][c];
                const float u = o->u, v = o->v;
                const uint32_t argb = o->argb;
                gfx_xform(o, shz_vec3_init(pos[r][c][0], WET_LIFT, pos[r][c][2]));
                o->u = u; o->v = v; o->oargb = 0u;
                {   /* the haze: the layer's share fades as the texel's does in haze_tex */
                    const unsigned t = (unsigned)(shz_saturatef((o->w - HAZE_START) * HAZE_INV) * (float)s_haze_max);
                    o->argb = (uint32_t)((((argb >> 24) * (256u - t)) >> 8) << 24) | (argb & 0x00ffffffu);
                }
            }
        gfx_bind(GFX_MAT_REFLECT_ADD);
        for (r = 0; r + 1u < WET_ROWS; r++)
            for (c = 0; c + 1u < WET_COLS; c++) {
                const gfx_vert_t q[4] = { gv[r + 1u][c], gv[r + 1u][c + 1u], gv[r][c + 1u], gv[r][c] };
                quad(q);
            }
    }
    {   /* the grit over it: tex_grit as tex_road lies (u = 1 at x -12, 0 at x 12), 12 m a repeat along; alpha = the rain's
           share x the edge x the haze, colour = the road's light, so the layer has the road's tone and no edge */
        const float ga = s_grit_alpha[s_rain] * 255.0f;
        const uint32_t road_col = gfx_scale(s_road_col, (unsigned)(s_wet_dark[s_rain] * 256.0f)) & 0x00ffffffu;
        for (r = 0; r < WET_ROWS; r++)
            for (c = 0; c < WET_COLS; c++) {
                gfx_vert_t *o = &gv[r][c];
                unsigned t;
                gfx_xform(o, shz_vec3_init(pos[r][c][0], GRIT_LIFT, pos[r][c][2]));
                t = (unsigned)(shz_saturatef((o->w - HAZE_START) * HAZE_INV) * (float)s_haze_max);
                o->u = (12.0f - pos[r][c][0]) * (1.0f / 24.0f);
                o->v = pos[r][c][2] * (1.0f / 12.0f);
                o->argb = ((uint32_t)(((unsigned)(ga * fade[r][c]) * (256u - t)) >> 8) << 24) | road_col;
#ifdef GRIT_SHOW                                      /* host check only: the layer at full strength, white light */
                o->argb = 0xffffffffu;
#endif
            }
        gfx_bind(GFX_MAT_GRIT);
        for (r = 0; r + 1u < WET_ROWS; r++)
            for (c = 0; c + 1u < WET_COLS; c++) {
                const gfx_vert_t q[4] = { gv[r + 1u][c], gv[r + 1u][c + 1u], gv[r][c + 1u], gv[r][c] };
                quad(q);
            }
    }
    s_wet_cpu_us = (uint32_t)(plat_us() - t0);
}
#endif

static void draw_road(void) {
    gfx_vert_t q[4];
    const float z_first = ROAD_TILE * shz_floorf((s_camz - DRAW_BACK) * (1.0f / ROAD_TILE));
#ifdef PL4NAR_WET
    const uint32_t road_col = gfx_scale(s_road_col, (unsigned)(s_wet_dark[s_rain] * 256.0f));   /* the rain darkens all of it */
#else
    const uint32_t road_col = s_road_col;
#endif
    unsigned i, b;
    VP_LOAD();
    gfx_bind(s_low ? GFX_MAT_P_ROAD : GFX_MAT_ROAD);
    for (i = 0; i < (unsigned)((DRAW_BACK + DRAW_FWD) / ROAD_TILE) + 1u; i++) {
        const float z0 = z_first + ROAD_TILE * (float)i, z1 = z0 + ROAD_TILE;
        gfx_xform(&q[0], shz_vec3_init(-BLD_X, 0.0f, z1)); q[0].u = 1.0f; q[0].v = (float)(i + 1u);
        gfx_xform(&q[1], shz_vec3_init(BLD_X, 0.0f, z1));  q[1].u = 0.0f; q[1].v = (float)(i + 1u);
        gfx_xform(&q[2], shz_vec3_init(BLD_X, 0.0f, z0));  q[2].u = 0.0f; q[2].v = (float)i;
        gfx_xform(&q[3], shz_vec3_init(-BLD_X, 0.0f, z0)); q[3].u = 1.0f; q[3].v = (float)i;
        for (b = 0; b < 4u; b++) { q[b].argb = road_col; haze_tex(&q[b]); }
        quad(q);
    }
    gfx_bind(GFX_MAT_FLAT);                           /* cross streets: plain asphalt out to the closing buildings */
    for (b = 0; b <= NBLOCK; b++) {
        const float zc = wrap_z((float)b * BLOCK);
        int s;
        if (zc < s_camz - DRAW_BACK - CROSS || zc > s_camz + DRAW_FWD) continue;
        for (s = -1; s <= 1; s += 2) {
            const float xa = s < 0 ? -86.0f : BLD_X, xb = s < 0 ? -BLD_X : 86.0f;
            gfx_xform(&q[0], shz_vec3_init(xa, 0.0f, zc + CROSS * 0.5f));
            gfx_xform(&q[1], shz_vec3_init(xb, 0.0f, zc + CROSS * 0.5f));
            gfx_xform(&q[2], shz_vec3_init(xb, 0.0f, zc - CROSS * 0.5f));
            gfx_xform(&q[3], shz_vec3_init(xa, 0.0f, zc - CROSS * 0.5f));
            for (i = 0; i < 4u; i++) { q[i].argb = gfx_mod(COL_ASPHALT, s_road_col); q[i].u = q[i].v = 0.0f; haze_flat(&q[i]); }
            quad(q);
        }
    }
}

#endif

static void wall(float ax, float az, float bx, float bz, float h, float len, float shade) {
    gfx_vert_t q[4];
    unsigned i;
    gfx_xform(&q[0], shz_vec3_init(ax, 0.0f, az)); q[0].u = 0.0f; q[0].v = h * (1.0f / FACADE_TILE);
    gfx_xform(&q[1], shz_vec3_init(bx, 0.0f, bz)); q[1].u = len * (1.0f / FACADE_TILE); q[1].v = q[0].v;
    gfx_xform(&q[2], shz_vec3_init(bx, h, bz));    q[2].u = q[1].u; q[2].v = 0.0f;
    gfx_xform(&q[3], shz_vec3_init(ax, h, az));    q[3].u = 0.0f; q[3].v = 0.0f;
    for (i = 0; i < 4u; i++) { q[i].argb = lit(grey(shade)); haze_tex(&q[i]); }
    quad(q);
}

static void draw_buildings(int roofs) {
#ifdef PL4NAR_MAP
    (void)roofs; (void)wall;                       /* the map's buildings are instances (draw_props) */
}
#else
    static uint8_t seen[MAX_BLD];
    static float zoff[MAX_BLD];
    unsigned i, m;
    for (i = 0; i < s_nbld; i++) {
        const bld_t *b = &s_bld[i];
        const float zc = wrap_z((b->z0 + b->z1) * 0.5f), hw = (b->z1 - b->z0) * 0.5f, hx = (b->x1 - b->x0) * 0.5f;
        zoff[i] = zc - (b->z0 + b->z1) * 0.5f;
        seen[i] = (uint8_t)(zc + hw > s_camz - DRAW_BACK && zc - hw < s_camz + DRAW_FWD &&
                            sphere_seen((b->x0 + b->x1) * 0.5f, b->h * 0.5f, zc, shz_sqrtf(hw * hw + hx * hx + b->h * b->h * 0.25f)));
    }
    VP_LOAD();
    for (m = 0; m < 4u; m++) {
        gfx_bind((s_low ? GFX_MAT_P_FACADE0 : GFX_MAT_FACADE0) + (int)m);
        for (i = 0; i < s_nbld; i++) {
            const bld_t *b = &s_bld[i];
            const float z0 = b->z0 + zoff[i], z1 = b->z1 + zoff[i];
            if (!seen[i] || b->mat != m) continue;
            /* shades: AMBIENT + DIFFUSE x max(0, n . sun), per face normal */
            if (b->side) wall(b->x0, z0, b->x0, z1, b->h, z1 - z0, AMBIENT);                                     /* street face, -X: in shade */
            else         wall(b->x1, z1, b->x1, z0, b->h, z1 - z0, AMBIENT + DIFFUSE * s_sun[0]);                /* street face, +X */
            wall(b->x0, z1, b->x1, z1, b->h, b->x1 - b->x0, AMBIENT + DIFFUSE * s_sun[2]);                       /* +Z */
            wall(b->x1, z0, b->x0, z0, b->h, b->x1 - b->x0, AMBIENT);                                            /* -Z */
        }
    }
    if (!roofs) return;
    gfx_bind(GFX_MAT_FLAT);
    for (i = 0; i < s_nbld; i++) {
        const bld_t *b = &s_bld[i];
        gfx_vert_t q[4];
        unsigned k;
        if (!seen[i]) continue;
        gfx_xform(&q[0], shz_vec3_init(b->x0, b->h, b->z1 + zoff[i]));
        gfx_xform(&q[1], shz_vec3_init(b->x1, b->h, b->z1 + zoff[i]));
        gfx_xform(&q[2], shz_vec3_init(b->x1, b->h, b->z0 + zoff[i]));
        gfx_xform(&q[3], shz_vec3_init(b->x0, b->h, b->z0 + zoff[i]));
        for (k = 0; k < 4u; k++) { q[k].argb = gfx_mod(COL_ROOF, s_road_col); q[k].u = q[k].v = 0.0f; haze_flat(&q[k]); }
        quad(q);
    }
}
#endif

/* ------------------------------------------------------------------------------------------------ models */
/* Opaque model, palette colour x (AMBIENT + DIFFUSE x n . sun). XMTRX is s_vp x translate x rotate on return. */
static void draw_model(const mdl_t *m, float x, float z, float yaw) {
    const shz_sincos_t sc = shz_sincosf(yaw);
    /* the sun in model space (rotation by -yaw about Y): two FMACs (shz_fmaf), once a model */
    const float lm[3] = { shz_fmaf(s_sun[0], sc.cos, -s_sun[2] * sc.sin), s_sun[1], shz_fmaf(s_sun[0], sc.sin, s_sun[2] * sc.cos) };
    unsigned i;
    if (m->nv > SCRATCH_N) {
        if (!(s_refused & 1u)) { s_refused |= 1u; printf("demo: model of %u vertices refused (scratch holds %u)\n", (unsigned)m->nv, (unsigned)SCRATCH_N); }
        return;
    }
    VP_LOAD();
    shz_xmtrx_translate(x, MODEL_Y, z);
    shz_xmtrx_rotate_y(yaw);
#ifdef PL4NAR_HILLS
    if (s_mp != 0.0f || s_mr != 0.0f) { shz_xmtrx_rotate_x(-s_mp); shz_xmtrx_rotate_z(s_mr); }   /* the slope: small angles, rise = angle */
#endif
    for (i = 0; i < m->nv; i++) {
        const mdl_vert_t *s = &m->v[i];
        gfx_vert_t *o = &s_scr[i];
#ifdef PL4NAR_MAP                                  /* the ambient x the vertex's baked byte: light = ambient x ao + sun */
        const float amb = s_mao ? AMBIENT * (float)s_mao[i] * (1.0f / 255.0f) : AMBIENT;     /* hand: two scalar products */
        const float k = shz_fmaf(DIFFUSE, shz_fmaxf(0.0f, sh4der_dot3_(s->n, lm)), amb);
#else
        const float k = shz_fmaf(DIFFUSE, shz_fmaxf(0.0f, sh4der_dot3_(s->n, lm)), AMBIENT);
#endif
        gfx_xform(o, shz_vec3_init(s->p[0], s->p[1], s->p[2]));
#ifdef PL4NAR_MAP
        o->argb = lit(gfx_scale(gfx_mod(0xff000000u | s->rgb, s_tint), (unsigned)(shz_fminf(k, 1.0f) * 256.0f)));
        if (s->cls == CLS_LAMP) o->argb = 0xff000000u | s->rgb;      /* a light (the lit strips, the bars): its own colour */
#else
        o->argb = lit(gfx_scale(0xff000000u | s->rgb, (unsigned)(shz_fminf(k, 1.0f) * 256.0f)));
#endif
        o->u = o->v = 0.0f;
        haze_flat(o);
    }
    for (i = 0; i + 2u < m->ni; i += 3u) gfx_tri(&s_scr[m->idx[i]], &s_scr[m->idx[i + 1u]], &s_scr[m->idx[i + 2u]]);
}

#ifdef PL4NAR_MAP
/* Level of detail. A building (a model of radius 8 m or more with no lit parts) past LOD_NEAR (LOD_MIRROR in the mirror)
   is drawn as its box: the model's bounds, five faces (no floor), its mean colour x tint, lit per face, its mean baked
   occlusion. 8 corners transformed, 10 triangles, against 150 .. 955 vertices for the model. A traffic car past
   CAR_NEAR is its body's box too (490 .. 931 vertices), with no wheels. Structures (the lit strips) and lamps keep their
   model.
   The band: a model with lit strips facing down (the overpass's bars, the passage's) has, in the mirror pass only, one
   quad 3 cm under them over all of them, their colour x the share of the band they cover. Bars mirrored one by one onto
   the paint, looked up per vertex, gave a black and white moire (s7, the night orbit under the overpass). */
typedef struct band { float x0, x1, z0, z1, y; uint32_t rgb; } band_t;   /* y 0: none */
static uint32_t s_box_rgb[MAP_MODELS];             /* the mean colour of the model's unlit vertices */
static uint8_t  s_box_bld[MAP_MODELS];             /* 1: a building, boxed past LOD_NEAR */
static float    s_box_ao[MAP_MODELS];
static float    s_box_r[MAP_MODELS];               /* the footprint's radius (x, z): the distance test (a tall tower's sphere is mostly height) */
static band_t   s_band[MAP_MODELS];
static float    s_lod_near = LOD_NEAR;
void demo_auto_lod(float m) { s_lod_near = m > 0.0f ? m : LOD_NEAR; }

static void lod_init(void) {
    unsigned mi, i;
    for (mi = 0; mi < MAP_MODELS; mi++) {
        const mdl_t *m = map_models[mi];
        float sum[3] = { 0.0f, 0.0f, 0.0f }, ao = 0.0f, area = 0.0f, n = 0.0f, lo[3] = { 1e9f, 1e9f, 1e9f }, hi[3] = { -1e9f, -1e9f, -1e9f };
        unsigned lamps = 0u, down = 0u, k;
        for (i = 0; i < m->nv; i++) {
            const mdl_vert_t *v = &m->v[i];
            if (v->cls == CLS_LAMP) {
                lamps++;
                if (v->n[1] < -0.9f) {
                    down++;
                    for (k = 0; k < 3u; k++) { lo[k] = shz_fminf(lo[k], v->p[k]); hi[k] = shz_fmaxf(hi[k], v->p[k]); }
                    sum[0] += (float)((v->rgb >> 16) & 255u); sum[1] += (float)((v->rgb >> 8) & 255u); sum[2] += (float)(v->rgb & 255u);
                }
                continue;
            }
            ao += map_model_ao[mi][i] * (1.0f / 255.0f); n += 1.0f;
        }
        s_box_ao[mi] = n > 0.0f ? ao * shz_invf(n) : 1.0f;
        if (down >= 8u) {                          /* two bars or more: their area over the band's (hand: scalar products) */
            for (i = 0; i + 2u < m->ni; i += 3u) {
                const mdl_vert_t *a = &m->v[m->idx[i]], *b = &m->v[m->idx[i + 1u]], *c = &m->v[m->idx[i + 2u]];
                if (a->cls != CLS_LAMP || a->n[1] > -0.9f || b->cls != CLS_LAMP || c->cls != CLS_LAMP) continue;
                area += 0.5f * shz_fabsf((b->p[0] - a->p[0]) * (c->p[2] - a->p[2]) - (c->p[0] - a->p[0]) * (b->p[2] - a->p[2]));
            }
            {
                const float duty = shz_saturatef(area * shz_invf((hi[0] - lo[0]) * (hi[2] - lo[2]))), inv = shz_invf((float)down);
                if (duty < 0.9f) {
                    band_t *bd = &s_band[mi];
                    bd->x0 = lo[0]; bd->x1 = hi[0]; bd->z0 = lo[2]; bd->z1 = hi[2]; bd->y = lo[1] - 0.03f;
                    bd->rgb = gfx_rgb(255u, (unsigned)(sum[0] * inv * duty), (unsigned)(sum[1] * inv * duty), (unsigned)(sum[2] * inv * duty));
                }
            }
        }
        s_box_bld[mi] = m->radius >= 8.0f && !lamps;
        sum[0] = sum[1] = sum[2] = 0.0f;
        for (i = 0; i < m->nv; i++) {
            const uint32_t c = m->v[i].rgb;
            if (m->v[i].cls == CLS_LAMP) continue;
            sum[0] += (float)((c >> 16) & 255u); sum[1] += (float)((c >> 8) & 255u); sum[2] += (float)(c & 255u);
        }
        s_box_r[mi] = shz_sqrtf(shz_fmaxf(m->lo[0] * m->lo[0], m->hi[0] * m->hi[0]) + shz_fmaxf(m->lo[2] * m->lo[2], m->hi[2] * m->hi[2]));   /* hand: scalar products */
        {
            const float inv = shz_invf((float)(m->nv - lamps > 0u ? m->nv - lamps : 1u));
            s_box_rgb[mi] = gfx_rgb(255u, (unsigned)(sum[0] * inv), (unsigned)(sum[1] * inv), (unsigned)(sum[2] * inv));
        }
    }
}

/* The box of model mi (lod_init). XMTRX = s_vp x translate x rotate on return. */
static void draw_box(unsigned mi, float x, float z, float yaw) {
    static const uint8_t face[5][4] = { { 1, 3, 7, 5 }, { 0, 4, 6, 2 }, { 2, 6, 7, 3 }, { 4, 5, 7, 6 }, { 0, 2, 3, 1 } };   /* corner bits: x 1, y 2, z 4; counter-clockwise outside */
    static const float fn[5][3] = { { 1, 0, 0 }, { -1, 0, 0 }, { 0, 1, 0 }, { 0, 0, 1 }, { 0, 0, -1 } };
    const mdl_t *m = map_models[mi];
    const shz_sincos_t sc = shz_sincosf(yaw);
    const float lm[3] = { shz_fmaf(s_sun[0], sc.cos, -s_sun[2] * sc.sin), s_sun[1], shz_fmaf(s_sun[0], sc.sin, s_sun[2] * sc.cos) };
    const float amb = s_ao ? AMBIENT * s_box_ao[mi] : AMBIENT;
    const uint32_t base = gfx_mod(s_box_rgb[mi], s_tint);
    gfx_vert_t c[8], q[4];
    unsigned f, k;
    VP_LOAD();
    shz_xmtrx_translate(x, MODEL_Y, z);
    shz_xmtrx_rotate_y(yaw);
#ifdef PL4NAR_HILLS
    if (s_mp != 0.0f || s_mr != 0.0f) { shz_xmtrx_rotate_x(-s_mp); shz_xmtrx_rotate_z(s_mr); }
#endif
    for (k = 0; k < 8u; k++) gfx_xform(&c[k], shz_vec3_init(k & 1u ? m->hi[0] : m->lo[0], k & 2u ? m->hi[1] : m->lo[1], k & 4u ? m->hi[2] : m->lo[2]));
    for (f = 0; f < 5u; f++) {
        const float l = shz_fmaf(DIFFUSE, shz_fmaxf(0.0f, sh4der_dot3_(fn[f], lm)), amb);
        const uint32_t col = lit(gfx_scale(base, (unsigned)(shz_fminf(l, 1.0f) * 256.0f)));
        for (k = 0; k < 4u; k++) { q[k] = c[face[f][k]]; q[k].argb = col; q[k].u = q[k].v = 0.0f; haze_flat(&q[k]); }
        quad(q);
    }
}

/* The band under model mi's strips (mirror pass). XMTRX: the instance's (draw_model's), unchanged. */
static void draw_band(unsigned mi) {
    const band_t *b = &s_band[mi];
    gfx_vert_t q[4];
    unsigned k;
    gfx_xform(&q[0], shz_vec3_init(b->x0, b->y, b->z0)); gfx_xform(&q[1], shz_vec3_init(b->x1, b->y, b->z0));
    gfx_xform(&q[2], shz_vec3_init(b->x1, b->y, b->z1)); gfx_xform(&q[3], shz_vec3_init(b->x0, b->y, b->z1));
    for (k = 0; k < 4u; k++) { q[k].argb = b->rgb; q[k].oargb = 0u; q[k].u = q[k].v = 0.0f; }
    quad(q);
}

/* The map's instances (buildings, structures, lamps, furniture) and the traffic, by distance from the camera. */
static void draw_props(int reflect_pass) {
    const float lim = reflect_pass ? MAP_MIRROR : MAP_DRAW, lod = reflect_pass ? LOD_MIRROR : s_lod_near;
    unsigned i;
    gfx_bind(GFX_MAT_FLAT);
    for (i = 0; i < MAP_INST; i++) {
        const map_inst_t *p = &map_inst[i];
        const mdl_t *m = map_models[p->model];
        const float dx = p->x - s_camx, dz = p->z - s_camz, d2 = dx * dx + dz * dz, r = lim + m->radius;   /* hand: scalar products */
        if (d2 > r * r) continue;
        if (m->radius < 4.0f && d2 > (reflect_pass ? 60.0f * 60.0f : 100.0f * 100.0f)) continue;   /* small far things: not worth it */
#ifdef PL4NAR_HILLS
        s_my = hills_iy[i];
#endif
        if (!sphere_seen(p->x, MODEL_Y + m->radius * 0.5f, p->z, m->radius)) continue;
#ifdef PL4NAR_HILLS
        if (reflect_pass) {                           /* wholly under the mirror plane: the PVR has no clip plane, skip it */
            const float c[3] = { p->x, MODEL_Y + m->radius * 0.5f, p->z };
            if (sh4der_pl4nar_sphere_side(&s_road_plane, c, m->radius) < 0) continue;
        }
#endif
        s_tint = 0xff000000u | p->tint;
        if (s_box_bld[p->model] && d2 > (lod + s_box_r[p->model]) * (lod + s_box_r[p->model])) draw_box(p->model, p->x, p->z, p->yaw);
        else {
            s_mao = s_ao ? map_model_ao[p->model] : NULL;
            draw_model(m, p->x, p->z, p->yaw);
            if (reflect_pass && s_band[p->model].y != 0.0f) draw_band(p->model);
        }
        s_tint = 0xffffffffu;
    }
    for (i = 0; i < map_traffic_n(); i++) {
        const map_pose_t q = map_traffic_pose(i);
        const mdl_t *body = map_models[map_car[i].body];
        const float dx = q.x - s_camx, dz = q.z - s_camz, d2 = dx * dx + dz * dz;
        if (d2 > (reflect_pass ? CAR_MIRROR * CAR_MIRROR : TRAFFIC_DRAW * TRAFFIC_DRAW)) continue;
#ifdef PL4NAR_HILLS
        {   /* on the road's triangles, tilted with them (not eased: the traffic's own lane is smooth enough) */
            const hills_tilt_t t = hills_car(map_traffic_s(i), map_lane[map_car[i].lane][0], q.yaw);
            s_my = t.y; s_mp = t.pitch; s_mr = t.roll;
        }
#endif
        if (!sphere_seen(q.x, MODEL_Y + body->radius * 0.5f, q.z, body->radius)) continue;
#ifdef PL4NAR_HILLS
        if (reflect_pass) {
            const float c[3] = { q.x, MODEL_Y + body->radius * 0.5f, q.z };
            if (sh4der_pl4nar_sphere_side(&s_road_plane, c, body->radius) < 0) continue;
        }
#endif
        if (!reflect_pass && d2 > CAR_NEAR * CAR_NEAR) { draw_box(map_car[i].body, q.x, q.z, q.yaw); continue; }
        s_mao = s_ao ? map_model_ao[map_car[i].body] : NULL;
        draw_model(body, q.x, q.z, q.yaw);
        s_mao = NULL;
        if (!reflect_pass) draw_model(map_models[map_car[i].wheels], q.x, q.z, q.yaw);
    }
    s_mao = NULL;
#ifdef PL4NAR_HILLS
    s_my = s_mp = s_mr = 0.0f;
#endif
}

/* The road's baked occlusion: the kept rows of tools/map_ao.c's 2 m grid, multiplied over the road (GFX_MAT_MUL) at
   1 cm. A patch is a run of joined rows; a row is drawn within MAP_AO_DRAW, a quad when a corner darkens. XMTRX = s_vp. */
static void draw_road_ao(void) {
    static gfx_vert_t row[2][MAP_AOG_COLS];
    unsigned r, c, cur = 0;
    int prev = 0;                                      /* the row before is near and in row[cur ^ 1] */
    if (!s_ao) return;
    gfx_bind(GFX_MAT_MUL);
    for (r = 0; r < MAP_AOG_ROWS; r++) {
        const float dx = map_aog[r][0][0] - s_camx, dz = map_aog[r][0][1] - s_camz;
        if (dx * dx + dz * dz > MAP_AO_DRAW * MAP_AO_DRAW) { prev = 0; continue; }     /* hand: scalar products */
        cur ^= 1u;
        for (c = 0; c < MAP_AOG_COLS; c++) {
            const unsigned g = (unsigned)(map_aog[r][c][2] * 255.0f);
#ifdef PL4NAR_HILLS
            gfx_xform(&row[cur][c], shz_vec3_init(map_aog[r][c][0], s_aoy[r][c] + 0.01f, map_aog[r][c][1]));
#else
            gfx_xform(&row[cur][c], shz_vec3_init(map_aog[r][c][0], 0.01f, map_aog[r][c][1]));
#endif
            row[cur][c].argb = gfx_rgb(255u, g, g, g); row[cur][c].oargb = 0u; row[cur][c].u = row[cur][c].v = 0.0f;
        }
        if (!prev || !map_aog_join[r]) { prev = 1; continue; }
        for (c = 0; c + 1u < MAP_AOG_COLS; c++) {
            if (map_aog[r - 1u][c][2] >= MAP_AOG_OPEN && map_aog[r - 1u][c + 1u][2] >= MAP_AOG_OPEN &&
                map_aog[r][c][2] >= MAP_AOG_OPEN && map_aog[r][c + 1u][2] >= MAP_AOG_OPEN) continue;   /* nothing to darken */
            gfx_tri(&row[cur ^ 1u][c], &row[cur ^ 1u][c + 1u], &row[cur][c + 1u]);
            gfx_tri(&row[cur ^ 1u][c], &row[cur][c + 1u], &row[cur][c]);
        }
    }
}
#else
static void draw_props(int reflect_pass) {
    unsigned i;
    gfx_bind(GFX_MAT_FLAT);
    for (i = 0; i < s_nprop; i++) {
        const prop_t *p = &s_prop[i];
        const float z = wrap_z(p->z);
        if (z < s_camz - DRAW_BACK || z > s_camz + (p->wheels ? 220.0f : 170.0f)) continue;
        if (reflect_pass && !p->wheels && z > s_camz + 90.0f) continue;       /* small far furniture is not worth mirroring */
        if (!sphere_seen(p->x, p->body->radius * 0.5f, z, p->body->radius)) continue;
        draw_model(p->body, p->x, z, p->yaw);
        if (p->wheels && !reflect_pass && z < s_camz + 110.0f) draw_model(p->wheels, p->x, z, p->yaw);
    }
    if (reflect_pass) return;
    {   /* wires between the poles: two thin ribbons, both faces */
        gfx_vert_t q[4];
        const float z_first = 32.0f * shz_floorf((s_camz - DRAW_BACK) * (1.0f / 32.0f)) - 2.0f;
        unsigned w, k;
        VP_LOAD();
        gfx_cull(GFX_CULL_NONE);
        for (i = 0; i < 7u; i++)
            for (w = 0; w < 2u; w++) {
                const float z0 = z_first + 32.0f * (float)i, x = -(BLD_X - 0.7f) + (w ? 1.1f : -1.1f), y = 6.35f;
                gfx_xform(&q[0], shz_vec3_init(x, y, z0));
                gfx_xform(&q[1], shz_vec3_init(x, y, z0 + 32.0f));
                gfx_xform(&q[2], shz_vec3_init(x, y + 0.07f, z0 + 32.0f));
                gfx_xform(&q[3], shz_vec3_init(x, y + 0.07f, z0));
                for (k = 0; k < 4u; k++) { q[k].argb = 0xff202024u; q[k].u = q[k].v = 0.0f; haze_flat(&q[k]); }
                quad(q);
            }
        gfx_cull(GFX_CULL_BACK);
    }
}
#endif

/* ------------------------------------------------------------------------------------------------ the player's car */
/* Opaque body; fills s_refl (the reflection layer's vertices) when the reflection is on. */
static void draw_player(void) {
    const mdl_t *m = &mdl_player_body;
    unsigned i;
    uint64_t t0;
#ifdef PL4NAR_HILLS
    s_my = s_cy; s_mp = s_cp; s_mr = s_cr;
#endif
    draw_model(&mdl_player_wheels, s_car.wx, s_car.wz, s_car.wh);
#ifdef PL4NAR_HILLS
    s_my = s_mp = s_mr = 0.0f;
#endif
    if (m->nv > MDL_PLAYER_BODY_NV) {
        if (!(s_refused & 2u)) { s_refused |= 2u; printf("demo: player body of %u vertices refused (arrays hold %u)\n", (unsigned)m->nv, (unsigned)MDL_PLAYER_BODY_NV); }
        return;
    }
    /* world positions + normals (model matrix in XMTRX) */
    shz_xmtrx_init_translation(s_car.wx, CAR_Y, s_car.wz);
    shz_xmtrx_rotate_y(s_car.wh);
#ifdef PL4NAR_HILLS
    shz_xmtrx_rotate_x(-s_cp); shz_xmtrx_rotate_z(s_cr);
#endif
    for (i = 0; i < m->nv; i++) {
        const mdl_vert_t *s = &m->v[i];
        const shz_vec3_t p = shz_xmtrx_transform_point3(shz_vec3_init(s->p[0], s->p[1], s->p[2]));
        /* normals: directions through the same matrix (shz_xmtrx_transform_vec3: the rotation, no translation) */
        const float *m_sn = mdl_player_body_sn[i];
        const shz_vec3_t n = shz_xmtrx_transform_vec3(shz_vec3_init(s->n[0], s->n[1], s->n[2]));
        const shz_vec3_t sn = shz_xmtrx_transform_vec3(shz_vec3_init(m_sn[0], m_sn[1], m_sn[2]));
        s_wp[i][0] = p.x; s_wp[i][1] = p.y; s_wp[i][2] = p.z;
        s_wn[i][0] = n.x; s_wn[i][1] = n.y; s_wn[i][2] = n.z;
        s_wsn[i][0] = sn.x; s_wsn[i][1] = sn.y; s_wsn[i][2] = sn.z;
    }
    VP_LOAD();
    for (i = 0; i < m->nv; i++) {
        const mdl_vert_t *s = &m->v[i];
        gfx_vert_t *o = &s_body[i];
        const float k = shz_fmaf(DIFFUSE, shz_fmaxf(0.0f, sh4der_dot3_(s_wn[i], s_sun)), AMBIENT);
        gfx_xform(o, shz_vec3_init(s_wp[i][0], s_wp[i][1], s_wp[i][2]));
        o->argb = lit(gfx_scale(s->cls == CLS_GLASS ? COL_GLASS : (0xff000000u | s->rgb), (unsigned)(shz_fminf(k, 1.0f) * 256.0f)));
        if (s->cls == CLS_LAMP) o->argb = gfx_mix(o->argb, 0xff000000u | s->rgb, s_lamp_k);   /* lamps lit: their own colour, no shading */
        o->oargb = 0u; o->u = o->v = 0.0f;
    }
    gfx_bind(GFX_MAT_FLAT);
    for (i = 0; i + 2u < m->ni; i += 3u) gfx_tri(&s_body[m->idx[i]], &s_body[m->idx[i + 1u]], &s_body[m->idx[i + 2u]]);
    if (!s_pv_ok) return;
    /* pl4nar: one lookup + Fresnel per paint / glass vertex. Two materials from the library's presets, tuned here. */
    t0 = plat_us();
    {
        sh4der_pl4nar_material_t paint = sh4der_pl4nar_preset(SH4DER_PL4NAR_PAINT), glass = sh4der_pl4nar_preset(SH4DER_PL4NAR_GLASS);
        const float us = gfx_reflect_u_scale();
#ifdef PL4NAR_MAP
        const float umax = 1.0f - shz_invf((float)s_sizes[s_size][0]);   /* a mirror texel short of the tiles */
        const shz_sincos_t fsc = shz_sincosf(s_car.wh);                   /* the car's forward (sin, cos) */
        const float fs = fsc.sin, fc = fsc.cos;
#endif
        paint.f0 = s_f0; glass.f0 = s_f0 + 0.10f;
        paint.reach = glass.reach = s_reach;
        paint.gain = glass.gain = shz_lerpf(s_day_gain, s_lamp_gain, s_night);   /* at night the lamps want more of the layer */
        sh4der_pl4nar_begin(&s_pv);                           /* XMTRX = the reflection view until the loop ends */
        for (i = 0; i < m->nv; i++) {
            const mdl_vert_t *s = &m->v[i];
            const sh4der_pl4nar_material_t *mat = s->cls == CLS_GLASS ? &glass : &paint;
            gfx_vert_t *o = &s_refl[i];
            sh4der_pl4nar_sample_t smp;
            if (s->cls != CLS_PAINT && s->cls != CLS_GLASS) continue;
            sh4der_pl4nar_shade(&s_pv, s_wp[i], s_wsn[i], mat, &smp);
            *o = s_body[i];                                   /* the opaque vertex's own x, y, z: the layer has its depth */
#ifdef PL4NAR_MAP
#ifdef PL4NAR_HILLS                               /* the mirror is the target's top left corner */
            o->u = shz_fminf(smp.u, umax) * us * ((float)s_sizes[s_size][0] * shz_invf((float)RT_W(s_sizes[s_size][0])));
            o->v = shz_fminf(smp.v, 1.0f - shz_invf((float)s_sizes[s_size][1])) * ((float)s_sizes[s_size][1] * shz_invf((float)RT_H(s_sizes[s_size][1])));
#else
            o->u = shz_fminf(smp.u, umax) * us * 0.5f; o->v = smp.v;   /* the mirror is the target's left half */
#endif
#else
            o->u = smp.u * us; o->v = smp.v;                  /* hand: one scale (1 on a KallistiOS with a sized target) */
#endif
            o->argb = sh4der_pl4nar_argb(&smp, mat);
#ifdef PL4NAR_MAP
            if (shz_fmaf(s_wsn[i][0], fs, s_wsn[i][2] * fc) < -0.5f) {     /* a face turned back (the rear deck, the bumper):
                                                                          the target magnifies a few texels over it */
                const unsigned a = ((o->argb >> 24) * 102u) >> 8;          /* x 0.4 */
                o->argb = (o->argb & 0x00ffffffu) | (a << 24);
            }
            {   /* at night the layer's alpha is capped: a strip on the deck keeps the paint's red under it */
                const unsigned cap = (unsigned)(shz_lerpf(1.0f, MAP_CAP_NIGHT, s_night) * 255.0f);
                if ((o->argb >> 24) > cap) o->argb = (o->argb & 0x00ffffffu) | (cap << 24);
            }
#endif
        }
    }
    s_refl_cpu_us = (uint32_t)(plat_us() - t0);
}

/* The layer: paint first (added: a clear coat keeps the paint's colour), then glass (blended: glass is its reflection). */
static void draw_player_reflection(void) {
    const mdl_t *m = &mdl_player_body;
    unsigned i, pass;
    if (s_split) {                                    /* the layer in the left half only; none when that half is hidden */
        split_half_t l, r;
        split_halves(s_div, SPLIT_COLS, SPLIT_ROWS, &l, &r);
        if (l.hidden) return;
        {
            const unsigned t[4] = { l.tx0, l.ty0, l.tx1, l.ty1 };
            gfx_clip(t);
        }
    }
    gfx_cull(GFX_CULL_BACK);
    for (pass = 0; pass < 2u; pass++) {
        const uint32_t want = pass ? CLS_GLASS : CLS_PAINT;     /* the bake gives every triangle one class (tools/bake.py checks) */
        gfx_bind(pass ? GFX_MAT_REFLECT : GFX_MAT_REFLECT_ADD);
        for (i = 0; i + 2u < m->ni; i += 3u)
            if (m->v[m->idx[i]].cls == want && m->v[m->idx[i + 1u]].cls == want && m->v[m->idx[i + 2u]].cls == want)   /* all three were written */
                gfx_tri(&s_refl[m->idx[i]], &s_refl[m->idx[i + 1u]], &s_refl[m->idx[i + 2u]]);
    }
    gfx_clip(NULL);
}

/* ------------------------------------------------------------------------------------------------ lights + shadows */
/* A sh4der_pool fan through the pass's matrix. closed: a disc (the rim wraps); else a wedge. */
static void fan(const sh4der_pool_vtx_t *p, unsigned nv, int closed) {
    static gfx_vert_t v[SH4DER_POOL_MAX_N + 2];
    const unsigned n = closed ? nv - 1u : nv - 2u;
    unsigned i;
    if (nv < 3u || nv > SH4DER_POOL_MAX_N + 2u) return;       /* 0 = the pool call refused; never past the array */
    for (i = 0; i < nv; i++) {
        gfx_xform(&v[i], shz_vec3_init(p[i].x, p[i].y, p[i].z));
        v[i].argb = p[i].argb; v[i].oargb = 0u; v[i].u = v[i].v = 0.0f;
    }
    for (i = 0; i < n; i++) gfx_tri(&v[0], &v[i + 1u], &v[closed ? (i + 1u) % n + 1u : i + 2u]);
}

#ifdef PL4NAR_HILLS
/* Draping: a region triangle (a pool's fan triangle, a tile's half square) laid on the drawn ground. Each ground
   triangle near it is cut to the region in a 2D space that is affine on a plane (x, z for a pool; for a tile, the
   point slid along the sun to y = 0), so what is drawn lies in the ground's own planes: nothing clips, nothing floats.
   Sutherland-Hodgman, three edges; at most 6 points a piece. */
typedef struct dv { float p[3], c[2]; } dv_t;
static float s_drape[HILLS_NEAR_MAX][3][3];
static unsigned s_drape_n;

static void drape_2d(const float p[3], float c[2], int slide) {
    if (!slide) { c[0] = p[0]; c[1] = p[2]; return; }
    {
        const float t = p[1] * shz_invf(s_sun[1]);                   /* along -sun to y = 0 */
        c[0] = shz_fmaf(-s_sun[0], t, p[0]); c[1] = shz_fmaf(-s_sun[2], t, p[2]);
    }
}

static unsigned clip_edge(const dv_t *in, unsigned n, const float a[2], const float b[2], dv_t *out) {
    const float ex = b[0] - a[0], ez = b[1] - a[1];
    unsigned i, m = 0, k;
    for (i = 0; i < n; i++) {
        const dv_t *P = &in[i], *Q = &in[i + 1u == n ? 0u : i + 1u];
        const float dp = shz_fmaf(ex, P->c[1] - a[1], -ez * (P->c[0] - a[0])), dq = shz_fmaf(ex, Q->c[1] - a[1], -ez * (Q->c[0] - a[0]));
        if (dp >= 0.0f) out[m++] = *P;
        if ((dp >= 0.0f) != (dq >= 0.0f)) {
            const float t = dp * shz_invf(dp - dq);
            for (k = 0; k < 3u; k++) out[m].p[k] = shz_lerpf(P->p[k], Q->p[k], t);
            out[m].c[0] = shz_lerpf(P->c[0], Q->c[0], t); out[m].c[1] = shz_lerpf(P->c[1], Q->c[1], t);
            m++;
        }
    }
    return m;
}

/* Region r (three 2D points) over s_drape's triangles. col: the region's corner colours (a pool), interpolated; NULL:
   the tile's texture coordinates from the slid point (tile k). lift: up from the ground, against depth fighting. */
static void drape(const float r[3][2], const uint32_t *col, int slide, unsigned tile, float lift) {
    const float area = shz_fmaf(r[1][0] - r[0][0], r[2][1] - r[0][1], -(r[2][0] - r[0][0]) * (r[1][1] - r[0][1]));
    const unsigned o1 = area > 0.0f ? 1u : 2u, o2 = area > 0.0f ? 2u : 1u;   /* inside on the left of each edge */
    float ia, iw = 0.0f, ih = 0.0f, ks = 0.0f, ox = 0.0f, oz = 0.0f;
    unsigned t, i, n;
    if (shz_fabsf(area) < 1e-6f) return;
    ia = shz_invf(area);
    if (!col) {
        const float W = (float)RT_W(s_sizes[s_size][0]), H = (float)RT_H(s_sizes[s_size][1]);
        float x0, y0, T;
        tile_rect(tile, &x0, &y0, &T);
        iw = shz_invf(W) * gfx_reflect_u_scale(); ih = shz_invf(H); ks = T * (1.0f / TILE_M);
        ox = x0 + 0.5f * T - s_tile_c[tile][0] * ks;
        oz = y0 + 0.5f * T - s_tile_c[tile][1] * ks;
    }
    for (t = 0; t < s_drape_n; t++) {
        dv_t a[8], b[8];
        gfx_vert_t v[8];
        for (i = 0; i < 3u; i++) { a[i].p[0] = s_drape[t][i][0]; a[i].p[1] = s_drape[t][i][1]; a[i].p[2] = s_drape[t][i][2]; drape_2d(a[i].p, a[i].c, slide); }
        n = clip_edge(a, 3u, r[0], r[o1], b);
        if (n >= 3u) n = clip_edge(b, n, r[o1], r[o2], a);
        if (n >= 3u) n = clip_edge(a, n, r[o2], r[0], b);
        if (n < 3u) continue;
        for (i = 0; i < n; i++) {
            gfx_xform(&v[i], shz_vec3_init(b[i].p[0], b[i].p[1] + lift, b[i].p[2]));
            v[i].oargb = 0u;
            if (col) {                                /* the corner colours at this point: barycentric in the region */
                const float w1 = shz_fmaf(b[i].c[0] - r[0][0], r[2][1] - r[0][1], -(r[2][0] - r[0][0]) * (b[i].c[1] - r[0][1])) * ia;
                const float w2 = shz_fmaf(r[1][0] - r[0][0], b[i].c[1] - r[0][1], -(b[i].c[0] - r[0][0]) * (r[1][1] - r[0][1])) * ia;
                uint32_t o = 0u;
                unsigned sh;
                for (sh = 0; sh < 32u; sh += 8u) {
                    const float c0 = (float)((col[0] >> sh) & 255u), c1 = (float)((col[1] >> sh) & 255u), c2 = (float)((col[2] >> sh) & 255u);
                    const float c = shz_fmaf(w1, c1 - c0, shz_fmaf(w2, c2 - c0, c0));
                    o |= (uint32_t)(shz_clampf(c, 0.0f, 255.0f) + 0.5f) << sh;
                }
                v[i].argb = o; v[i].u = v[i].v = 0.0f;
            } else {
                v[i].argb = 0xffffffffu;
                v[i].u = shz_fmaf(b[i].c[0], ks, ox) * iw; v[i].v = shz_fmaf(b[i].c[1], ks, oz) * ih;
            }
        }
        for (i = 1; i + 1u < n; i++) gfx_tri(&v[0], &v[i], &v[i + 1u]);
    }
}

/* A pool fan (sh4der_pool_disc / _wedge) on the drawn ground near (s, x, z) within r. */
static void fan_drape(const sh4der_pool_vtx_t *p, unsigned nv, int closed, float s, float x, float z, float r, float lift) {
    unsigned n, i;
    if (nv < 3u || nv > SH4DER_POOL_MAX_N + 2u) return;
    n = closed ? nv - 1u : nv - 2u;
    s_drape_n = hills_tris_near(s, x, z, r, s_drape, HILLS_NEAR_MAX);
    for (i = 0; i < n; i++) {
        const unsigned b = i + 1u, c = closed ? (i + 1u) % n + 1u : i + 2u;
        const float rg[3][2] = { { p[0].x, p[0].z }, { p[b].x, p[b].z }, { p[c].x, p[c].z } };
        const uint32_t col[3] = { p[0].argb, p[b].argb, p[c].argb };
        drape(rg, col, 0, 0u, lift);
    }
}
#endif

/* A halo: SH4DER-PVR's camera-facing quad (sh4der_spark_quad) at a world point, projected by hand (the quad comes
   back in view space). rgb x gain is the brightness; the bound material decides how it is drawn. */
static void halo(float x, float y, float z, float half, uint32_t rgb, float gain) {
    sh4der_spark_t s;
    sh4der_spark_vtx_t q[4];
    gfx_vert_t v[4];
    unsigned k;
    memset(&s, 0, sizeof s);
    s.p[0] = x; s.p[1] = y; s.p[2] = z;
    s.q[0] = half;
    s.argb = gfx_scale(0xff000000u | rgb, (unsigned)(shz_saturatef(gain) * 256.0f));
    s.life_ms = 1000u; s.shape = SH4DER_SPARK_BILLBOARD;
    if (!sh4der_spark_quad(&s, &s_sv, q)) return;
    for (k = 0; k < 4u; k++) {
        const float iz = shz_invf(q[k].z);
        v[k].x = shz_fmaf(q[k].x * iz, s_focal, s_vw * 0.5f);          /* hand: the projection of a view-space point */
        v[k].y = shz_fmaf(-q[k].y * iz, s_focal, s_vh * 0.5f);
        v[k].z = iz; v[k].w = q[k].z; v[k].cx = v[k].x * q[k].z; v[k].cy = v[k].y * q[k].z;      /* hand: two plain products, no SH4ZAM op */
        v[k].u = q[k].u; v[k].v = q[k].v; v[k].argb = q[k].argb; v[k].oargb = 0u; v[k].clip = 0u;
    }
    gfx_tri(&v[0], &v[1], &v[2]);
    gfx_tri(&v[1], &v[3], &v[2]);
}

static void spark_view(shz_vec3_t eye, shz_vec3_t at) {
    const shz_vec3_t f = shz_vec3_normalize(shz_vec3_sub(at, eye));
    const shz_vec3_t r = shz_vec3_normalize(shz_vec3_cross(f, shz_vec3_init(0.0f, 1.0f, 0.0f)));
    const shz_vec3_t u = shz_vec3_cross(r, f);
    s_sv.right[0] = r.x; s_sv.right[1] = r.y; s_sv.right[2] = r.z;
    s_sv.up[0] = u.x; s_sv.up[1] = u.y; s_sv.up[2] = u.z;
    s_sv.fwd[0] = f.x; s_sv.fwd[1] = f.y; s_sv.fwd[2] = f.z;
    s_sv.eye[0] = eye.x; s_sv.eye[1] = eye.y; s_sv.eye[2] = eye.z;
    s_sv.near_z = GFX_NEAR; s_sv.focal_px = s_focal; s_sv.min_px = SH4DER_SPARK_MIN_PX; s_sv.inset = 0.5f / (float)TEX_GLOW_SIZE;
    s_sv.cols = 1u; s_sv.rows = 1u;
}

/* The street lamps near the car. what 0: the pools on the road (GFX_MAT_ADD bound, XMTRX = s_vp). 1: the halos
   (GFX_MAT_GLOW). 2: the mirror pass: each head's glow at its mirrored place (GFX_MAT_GLOW, the target's translucent list). */
static void draw_lamps(int what) {
    const float range = what == 2 ? LAMP_MIRROR : LAMP_NEAR;
    static const float ux[3] = { 7.5f, 0.0f, 0.0f }, uz[3] = { 0.0f, 0.0f, 7.5f };
    sh4der_pool_vtx_t p[SH4DER_POOL_MAX_N + 2];
    unsigned i;
    if (s_lamp <= 0.02f) return;
#ifdef PL4NAR_MAP
    /* the lamp heads, then the lit strips under the structures (a cooler white, a smaller pool: there are many) */
    for (i = 0; i < MAP_LAMPS + MAP_STRIPS; i++) {
        const int strip = i >= MAP_LAMPS;
        const float x = strip ? map_strip[i - MAP_LAMPS][0] : map_lamp[i][0], z = strip ? map_strip[i - MAP_LAMPS][2] : map_lamp[i][1];
#ifdef PL4NAR_HILLS
        const float gy = strip ? 0.0f : hills_ly[i];   /* the strips hang under the structures: flat ground */
#else
        const float gy = 0.0f;
#endif
        const float y = strip ? map_strip[i - MAP_LAMPS][1] : gy + LAMP_HEAD_Y;
        const uint32_t col = strip ? 0x00d8e8ffu : COL_LAMP;
        const float dx = x - s_car.wx, dz = z - s_car.wz;
        if (dx * dx + dz * dz > range * range) continue;                     /* hand: scalar products */
#ifdef PL4NAR_HILLS
        const float lk = s_lamp * shz_saturatef((range - shz_sqrtf(dx * dx + dz * dz)) * (1.0f / LAMP_FADE_W));   /* no pop at the reach */
#else
        const float lk = s_lamp;
#endif
        if (what == 0) {
            const float c[3] = { x, gy + 0.03f, z };
#ifdef PL4NAR_HILLS
            if (!strip) { fan_drape(p, sh4der_pool_disc(c, ux, uz, 10u, col, 0.60f * lk, p), 1, s_lamp_s[i], x, z, 7.6f, 0.03f); continue; }
#endif
            fan(p, sh4der_pool_disc(c, ux, uz, 10u, col, (strip ? 0.15f : 0.60f) * s_lamp, p), 1);
        } else if (what == 1) halo(x, y, z, strip ? 2.0f : 2.2f, col, lk);              /* D1 (the depth fade) goes here */
#ifdef PL4NAR_HILLS
        else {                                        /* the head mirrored about the fitted plane; none under it */
            const float h[3] = { x, y, z };
            float m[3];
            if (sh4der_pl4nar_sphere_side(&s_road_plane, h, 0.0f) < 0) continue;
            sh4der_pl4nar_mirror_point(&s_road_plane, h, m);
            halo(m[0], m[1], m[2], strip ? 1.6f : 1.5f, col, lk);
        }
#else
        else halo(x, -y, z, strip ? 1.6f : 1.5f, col, s_lamp);
#endif
    }
    return;
#endif
    for (i = 0; i < s_nprop; i++) {
        const prop_t *pr = &s_prop[i];
        const float z = wrap_z(pr->z), hx = pr->x - LAMP_ARM * shz_sinf(pr->yaw);
        if (pr->body != &mdl_lamp || shz_fabsf(z - s_car.wz) > range) continue;
        if (what == 0) {
            const float c[3] = { hx, 0.03f, z };
            fan(p, sh4der_pool_disc(c, ux, uz, 10u, COL_LAMP, 0.60f * s_lamp * pool_dim(), p), 1);
        } else if (what == 1) halo(hx, LAMP_HEAD_Y, z, 2.2f, COL_LAMP, s_lamp);
#ifdef PL4NAR_WET
        else halo(hx, -LAMP_HEAD_Y, z, 3.0f, COL_LAMP, s_lamp);    /* the wet road: a wider glow, the streak's source */
#else
        else halo(hx, -LAMP_HEAD_Y, z, 1.5f, COL_LAMP, s_lamp);
#endif
    }
}

/* A point on the player's car (forward, right, up in metres, the car's own frame) in the world: the body's own matrix,
   translate(wx, CAR_Y, wz) x rotate_y(wh) x rotate_x(-pitch) x rotate_z(roll) (draw_player), so a lamp stays on the
   car on a slope and on a camber. */
static void car_point(float fwd, float side, float up, float out[3]) {
    const shz_sincos_t hc = shz_sincosf(s_car.wh);
#ifdef PL4NAR_HILLS
    const shz_sincos_t pc = shz_sincosf(s_cp), rc = shz_sincosf(s_cr);
    const float x = shz_fmaf(side, rc.cos, -up * rc.sin), y = shz_fmaf(side, rc.sin, up * rc.cos);
    const float y2 = shz_fmaf(y, pc.cos, fwd * pc.sin), z2 = shz_fmaf(fwd, pc.cos, -y * pc.sin);
#else
    const float x = side, y2 = up, z2 = fwd;
#endif
    out[0] = shz_fmaf(x, hc.cos, shz_fmaf(z2, hc.sin, s_car.wx));
    out[1] = CAR_Y + y2;
    out[2] = shz_fmaf(-x, hc.sin, shz_fmaf(z2, hc.cos, s_car.wz));
}

#ifdef PL4NAR_WET
/* The wet road's extra lights (GFX_MAT_GLOW bound): a red sign and a blue sign on the facades, once a PERIOD; mirror 1:
   in the mirror pass, the signs and the car's own four lamps at their mirrored height, so the road carries them. */
static void wet_lights(int mirror) {
    static const struct { float x, y, z; uint32_t rgb; } sign[2] = { { -BLD_X + 0.3f, 5.5f, 60.0f, 0x00ff3020u }, { BLD_X - 0.3f, 4.5f, 150.0f, 0x002a60ffu } };
    unsigned i;
    for (i = 0; i < 2u; i++) {
        const float z = wrap_z(sign[i].z);
        if (z < s_camz - 10.0f || z > s_camz + LAMP_MIRROR) continue;
        halo(sign[i].x, mirror ? -sign[i].y : sign[i].y, z - 0.8f, mirror ? 2.2f : 1.3f, sign[i].rgb, 1.0f);   /* two glows: a bar */
        halo(sign[i].x, mirror ? -sign[i].y : sign[i].y, z + 0.8f, mirror ? 2.2f : 1.3f, sign[i].rgb, 1.0f);
    }
    if (mirror) {                                  /* the street is flat: the mirror is y -> -y */
        int s;
        for (s = -1; s <= 1; s += 2) {
            float h[3], t[3];
            car_point(2.40f, 0.62f * (float)s, 0.60f, h); car_point(-2.42f, 0.62f * (float)s, 0.60f, t);
            halo(h[0], -h[1], h[2], 0.45f, COL_HEAD, s_lamp);
            halo(t[0], -t[1], t[2], 0.35f, COL_TAIL, s_lamp);
        }
    }
}

/* Rain over the picture: thin falling streaks, screen space (GFX_MAT_HUD, translucent), stateless (slot, time). */
static void wet_rain(void) {
    static const unsigned count[3] = { 0u, 40u, 110u };
    const float t = s_car.clock;
    unsigned i;
    gfx_bind(GFX_MAT_HUD);
    for (i = 0; i < count[s_rain]; i++) {
        const uint32_t h = wet_hash(i, 0x51u);
        const float len = 14.0f + (float)(h & 15u), speed = 900.0f + (float)((h >> 4) & 255u);
        const float span = 480.0f + len, f = shz_fmaf(t, speed, (float)((h >> 20) & 511u));
        const float x = (float)((h >> 12) % 640u), y = f - span * shz_floorf(f * shz_invf(span)) - len;   /* hand: f mod span */
        gfx_quad2d(x, y, x + 1.0f, y + len, 9.5f, 0.0f, 0.0f, 0.0f, 0.0f, 0x40c8d4e4u);
    }
}
#endif

/* One inset of the target at the right edge (THUMB_RIGHT), y down to y + h: target u0 .. u1 across, 0 .. v1 down, shown
   with v turned over (the mirrored world right side up), a dark border and a label under it. Returns h + THUMB_GAP. */
static float thumb(float w, float h, float y, float u1, float v1, float u0, const char *label) {
    const float x = THUMB_RIGHT - w;
    gfx_bind(GFX_MAT_HUD);
    gfx_quad2d(x - 2.0f, y - 2.0f, x + w + 2.0f, y + h + 2.0f, 8.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0xc0000000u);
    gfx_bind(GFX_MAT_REFLECT);
    gfx_quad2d(x, y, x + w, y + h, 8.5f, u0, v1, u1, 0.0f, 0xffffffffu);
    hud_text(THUMB_RIGHT - 8.0f * (float)strlen(label), y + h + 4.0f, 1.0f, 0xffc0c0c0u, label);
    return h + THUMB_GAP;
}

/* The car's own lights: two wedges on the road ahead (GFX_MAT_ADD), then four halos (GFX_MAT_GLOW). */
static void draw_car_lights(int halos) {
    const shz_sincos_t hc = shz_sincosf(s_car.wh);
    const float fw[3] = { hc.sin, 0.0f, hc.cos }, rt[3] = { hc.cos, 0.0f, -hc.sin };
    sh4der_pool_vtx_t p[SH4DER_POOL_MAX_N + 2];
    int s;
    if (s_lamp <= 0.02f) return;
    for (s = -1; s <= 1; s += 2) {
        const float side = 0.62f * (float)s;
        if (!halos) {
            float a[3];
            car_point(2.3f, side, 0.0f, a); a[1] = CAR_Y + 0.035f;   /* the apex under the lamp; the hills drape the fan on the road */
#ifdef PL4NAR_HILLS
            {   /* on the road's triangles ahead (a beam 30 m long would leave a plane on the first crest) */
                const float mx = shz_fmaf(fw[0], 17.0f, s_car.wx), mz = shz_fmaf(fw[2], 17.0f, s_car.wz), ms = s_car.z + 17.0f;
                fan_drape(p, sh4der_pool_wedge(a, fw, rt, 30.0f, 0.11f, 2u, COL_HEAD, 0.45f * s_lamp, p), 0, ms, mx, mz, 17.0f, 0.035f);
                fan_drape(p, sh4der_pool_wedge(a, fw, rt, 20.0f, 0.30f, 4u, COL_HEAD, 0.22f * s_lamp, p), 0, ms, mx, mz, 17.0f, 0.035f);
            }
#else
            fan(p, sh4der_pool_wedge(a, fw, rt, 30.0f, 0.11f, 2u, COL_HEAD, 0.45f * s_lamp * pool_dim(), p), 0);      /* the beam */
            fan(p, sh4der_pool_wedge(a, fw, rt, 20.0f, 0.30f, 4u, COL_HEAD, 0.22f * s_lamp * pool_dim(), p), 0);      /* its spill: a soft edge */
#endif
        } else {                                   /* on the body: its height, pitch and roll (the bug was a flat frame at y 0.60) */
            float h[3], t[3];
            car_point(2.40f, side, 0.60f, h); car_point(-2.42f, side, 0.60f, t);
            halo(h[0], h[1], h[2], 0.55f, COL_HEAD, s_lamp);
            halo(t[0], t[1], t[2], 0.40f, COL_TAIL, s_lamp);
        }
    }
}

#ifdef PL4NAR_BLOBS
/* Blob shadows: a dark ellipse under each car (GFX_MAT_HUD: vertex colour, blended). Only with -DPL4NAR_BLOBS. */
static void blob(float x, float z, float yaw, float alpha) {
    s_blob_n++;
    const shz_sincos_t hc = shz_sincosf(yaw);
    const float c[3] = { x, 0.02f, z }, u[3] = { hc.sin * 2.9f, 0.0f, hc.cos * 2.9f }, v[3] = { hc.cos * 1.35f, 0.0f, -hc.sin * 1.35f };
    sh4der_pool_vtx_t p[SH4DER_POOL_MAX_N + 2];
#ifdef PL4NAR_HILLS
    fan_drape(p, sh4der_pool_disc(c, u, v, 8u, 0x00000000u, alpha, p), 1, s_blob_s, x, z, 3.2f, 0.02f);
#else
    fan(p, sh4der_pool_disc(c, u, v, 8u, 0x00000000u, alpha, p), 1);
#endif
}
#endif

#ifdef PL4NAR_MAP
/* The convex hull of n (<= 16) points, counter-clockwise, into h; its count. Monotone chain. */
static unsigned hull2(float (*p)[2], unsigned n, float (*h)[2]) {
    unsigned i, j, k = 0, lo;
    for (i = 1; i < n; i++)                                          /* insertion sort by x, then y */
        for (j = i; j > 0 && (p[j][0] < p[j - 1][0] || (p[j][0] == p[j - 1][0] && p[j][1] < p[j - 1][1])); j--) {
            const float tx = p[j][0], ty = p[j][1];
            p[j][0] = p[j - 1][0]; p[j][1] = p[j - 1][1]; p[j - 1][0] = tx; p[j - 1][1] = ty;
        }
    for (i = 0; i < n; i++) {                                        /* lower */
        while (k >= 2u && shz_vec2_cross(shz_vec2_init(h[k - 1][0] - h[k - 2][0], h[k - 1][1] - h[k - 2][1]),
                                         shz_vec2_init(p[i][0] - h[k - 2][0], p[i][1] - h[k - 2][1])) <= 0.0f) k--;
        h[k][0] = p[i][0]; h[k][1] = p[i][1]; k++;
    }
    lo = k + 1u;
    for (i = n - 1u; i-- > 0u;) {                                    /* upper */
        while (k >= lo && shz_vec2_cross(shz_vec2_init(h[k - 1][0] - h[k - 2][0], h[k - 1][1] - h[k - 2][1]),
                                         shz_vec2_init(p[i][0] - h[k - 2][0], p[i][1] - h[k - 2][1])) <= 0.0f) k--;
        h[k][0] = p[i][0]; h[k][1] = p[i][1]; k++;
    }
    return k > 1u ? k - 1u : k;                                      /* the last point is the first */
}

/* One box of a car (model space lo .. hi), slid along the sun onto the ground, as a hull in tile k's pixels. */
static void tile_box(unsigned k, float x, float z, float yaw, const float lo[3], const float hi[3], uint32_t argb) {
    const shz_sincos_t sc = shz_sincosf(yaw);
#ifdef PL4NAR_HILLS
    float x0, y0, T;
    const float iy = shz_invf(s_sun[1]);
    tile_rect(k, &x0, &y0, &T);
#else
    const float iy = shz_invf(s_sun[1]), T = s_tile_px, x0 = s_tile_px * (float)(4u + (k & 3u)), y0 = s_tile_px * (float)(k >> 2);
#endif
    const float ks = T * (1.0f / TILE_M), ox = x0 + 0.5f * T - s_tile_c[k][0] * ks, oz = y0 + 0.5f * T - s_tile_c[k][1] * ks;
    float p[8][2], h[9][2];
    gfx_vert_t v[9];
    unsigned i, n;
    for (i = 0; i < 8u; i++) {
        const float lx = (i & 1u) ? hi[0] : lo[0], ly = (i & 2u) ? hi[1] : lo[1], lz = (i & 4u) ? hi[2] : lo[2];
        const float wx = shz_fmaf(lx, sc.cos, shz_fmaf(lz, sc.sin, x)), wz = shz_fmaf(-lx, sc.sin, shz_fmaf(lz, sc.cos, z));
#ifdef PL4NAR_HILLS
        const float t = shz_fmaf(s_tb_p, lz, shz_fmaf(s_tb_r, lx, s_tb_y + ly)) * iy;   /* the corner's world height (base, tilt), along -sun to y = 0 */
#else
        const float t = ly * iy;                                     /* along -sun to y = 0 */
#endif
        p[i][0] = shz_clampf(shz_fmaf(shz_fmaf(-s_sun[0], t, wx), ks, ox), x0 + 2.0f, x0 + T - 2.0f);
        p[i][1] = shz_clampf(shz_fmaf(shz_fmaf(-s_sun[2], t, wz), ks, oz), y0 + 2.0f, y0 + T - 2.0f);
    }
    n = hull2(p, 8u, h);
    for (i = 0; i < n; i++) gfx_vert2d(&v[i], h[i][0], h[i][1], TILE_Z * 1.1f, 0.0f, 0.0f, argb);
    for (i = 1; i + 1u < n; i++) gfx_tri(&v[0], &v[i], &v[i + 1u]);
}

/* A car's casters: the body to 55 % of its height, the cabin above it, narrower and shorter. */
static void tile_car(unsigned k, float x, float z, float yaw, const mdl_t *m, uint32_t argb) {
    const float L = m->hi[2] - m->lo[2], W = m->hi[0] - m->lo[0], H = m->hi[1] - m->lo[1];
    const float blo[3] = { m->lo[0], m->lo[1], m->lo[2] }, bhi[3] = { m->hi[0], shz_fmaf(H, 0.55f, m->lo[1]), m->hi[2] };
    const float clo[3] = { shz_fmaf(W, 0.08f, m->lo[0]), m->lo[1], shz_fmaf(L, 0.22f, m->lo[2]) };
    const float chi[3] = { shz_fmaf(W, -0.08f, m->hi[0]), m->hi[1], shz_fmaf(L, -0.30f, m->hi[2]) };
#ifdef PL4NAR_HILLS
    float x0, y0, T;
    tile_rect(k, &x0, &y0, &T);
#else
    const float T = s_tile_px, x0 = T * (float)(4u + (k & 3u)), y0 = T * (float)(k >> 2);
#endif
    gfx_quad2d(x0, y0, x0 + T, y0 + T, TILE_Z, 0.0f, 0.0f, 0.0f, 0.0f, 0xffffffffu);
    tile_box(k, x, z, yaw, blo, bhi, argb);
    tile_box(k, x, z, yaw, clo, chi, argb);
}

/* The road's baked occlusion near (x, z): the nearest point of a kept row within 2.5 m, else 1 (open). A row is found
   by its first column (105 rows), then its nearest column. A car under a deck has no sun on it: its tile fades by this. */
static float road_ao_at(float x, float z) {
    float best = 3.0f * 3.0f * 9.0f * 9.0f, v = 1.0f;
    unsigned r, c, br = MAP_AOG_ROWS;
    for (r = 0; r < MAP_AOG_ROWS; r++) {               /* the row whose middle is nearest (column 0 is 9 m from the middle) */
        const float *m = map_aog[r][MAP_AOG_COLS / 2u];
        const float dx = m[0] - x, dz = m[1] - z, d = dx * dx + dz * dz;    /* hand: scalar products */
        if (d < best) { best = d; br = r; }
    }
    if (br == MAP_AOG_ROWS) return 1.0f;
    best = 2.5f * 2.5f;
    for (c = 0; c < MAP_AOG_COLS; c++) {
        const float *m = map_aog[br][c];
        const float dx = m[0] - x, dz = m[1] - z, d = dx * dx + dz * dz;
        if (d < best) { best = d; v = m[2]; }
    }
    return v;
}

/* The mirror pass's last opaque work: the player and the nearest traffic, a tile each. Day only (the sun up). */
static void draw_tiles(float tw, float th) {
    const float sy = s_sun[1];
    const unsigned g = (unsigned)(shz_lerpf(0.50f, 1.0f, shz_saturatef(s_night * 2.0f)) * 255.0f);
    float d2[MAP_CARS];
    unsigned i, k;
    s_tile_n = 0u;
#ifdef PL4NAR_HILLS
    s_tile_live = 0u; s_tile_band = 0u;
    (void)d2; (void)i;
    if (!s_tiles || s_night >= 0.5f || sy < 0.2f) { for (k = 0; k < TILE_N; k++) s_slot_car[k] = -2; return; }
    gfx_viewport((float)RT_W((unsigned)tw), (float)RT_H((unsigned)th));
    gfx_bind(GFX_MAT_FLAT);
    gfx_cull(GFX_CULL_NONE);
    {   /* the slots: the player in slot 0; a car keeps its slot until its shadow has faded to nothing (TILE_FADE_FAR from
           the camera), and a free slot goes to the nearest car without one inside that reach. So no shadow pops: one
           leaving fades out first, one arriving from beyond the reach comes in at 0, and one given a slot nearer grows in
           over TILE_FADE_IN. Distances from the camera. */
        const unsigned ns = tile_slots();
        const float dt = shz_clampf(s_car.clock - s_slot_clock, 0.0f, 0.1f);
        float cd2[MAP_CARS];
        int held[MAP_CARS];
        unsigned j;
        s_slot_clock = s_car.clock;
        for (j = 0; j < map_traffic_n(); j++) {
            const map_pose_t q = map_traffic_pose(j);
            const float dx = q.x - s_camx, dz = q.z - s_camz;
            cd2[j] = dx * dx + dz * dz;                              /* hand: scalar products */
            held[j] = 0;
        }
        s_slot_car[0] = -1;
        for (k = 1; k < TILE_N; k++) {
            if (k >= ns || s_slot_car[k] >= (int)map_traffic_n()) s_slot_car[k] = -2;
            if (s_slot_car[k] >= 0) {
                if (cd2[s_slot_car[k]] >= TILE_FADE_FAR * TILE_FADE_FAR) s_slot_car[k] = -2;   /* faded out: given up */
                else held[s_slot_car[k]] = 1;
            }
        }
        for (k = 1; k < ns; k++) {
            if (s_slot_car[k] >= 0) continue;
            {
                float best = TILE_FADE_FAR * TILE_FADE_FAR;
                int car = -2;
                for (j = 0; j < map_traffic_n(); j++) if (!held[j] && cd2[j] < best) { best = cd2[j]; car = (int)j; }
                if (car < 0) break;
                s_slot_car[k] = car; held[car] = 1; s_slot_in[k] = 0.0f;
            }
        }
        for (k = 0; k < ns; k++) {
            float x, z, yaw;
            const mdl_t *m;
            const int car = s_slot_car[k];
            s_tile_car[k] = car;
            s_tile_n = k + 1u;
            if (car < -1) { s_tile_fade[k] = 0.0f; continue; }
            if (car < 0) { x = s_car.wx; z = s_car.wz; yaw = s_car.wh; m = &mdl_player_body; s_tile_fade[k] = 1.0f; }
            else {
                const map_pose_t q = map_traffic_pose((unsigned)car);
                const float d = shz_sqrtf(cd2[car]);
                x = q.x; z = q.z; yaw = q.yaw; m = map_models[map_car[car].body];
                if (d > TILE_FADE_NEAR) s_tile_band++;
                s_slot_in[k] = shz_fminf(s_slot_in[k] + dt * (1.0f / TILE_FADE_IN), 1.0f);
                s_tile_fade[k] = shz_saturatef((TILE_FADE_FAR - d) * (1.0f / (TILE_FADE_FAR - TILE_FADE_NEAR))) * s_slot_in[k];
            }
            if (car < 0) { s_tb_y = s_cy; s_tb_p = s_cp; s_tb_r = s_cr; s_tile_s[k] = s_car.z; }
            else {
                const hills_tilt_t t = hills_car(map_traffic_s((unsigned)car), map_lane[map_car[car].lane][0], yaw);
                s_tb_y = t.y; s_tb_p = t.pitch; s_tb_r = t.roll; s_tile_s[k] = map_traffic_s((unsigned)car);
            }
            s_tile_y[k] = s_tb_y;
            {   /* the square in the slid space (y = 0): the car's base slid along the sun, then half the shadow's reach on */
                const float r = shz_fminf(0.5f * m->hi[1] * shz_invf(sy), 2.0f) + s_tb_y * shz_invf(sy);
                s_tile_c[k][0] = shz_fmaf(-s_sun[0], r, x); s_tile_c[k][1] = shz_fmaf(-s_sun[2], r, z);
            }
            {   /* under a deck the sun is gone (to white by 0.80 of the baked occlusion); the fade takes it to white too */
                const float f = shz_saturatef((0.95f - road_ao_at(x, z)) * (1.0f / 0.15f));
                const float gk = shz_lerpf(shz_lerpf((float)g, 255.0f, f), 255.0f, 1.0f - s_tile_fade[k]);
                const unsigned gi = (unsigned)gk;
                tile_car(k, x, z, yaw, m, gfx_rgb(255u, gi, gi, gi));
            }
            s_tile_live++;
        }
    }
    gfx_viewport(tw, th);
    return;
#endif
    if (!s_tiles || s_night >= 0.5f || sy < 0.2f) return;
    s_tile_px = th * 0.5f;
    gfx_viewport(tw * 2.0f, th);
    gfx_bind(GFX_MAT_FLAT);
    gfx_cull(GFX_CULL_NONE);
    for (i = 0; i < map_traffic_n(); i++) {
        const map_pose_t q = map_traffic_pose(i);
        const float dx = q.x - s_car.wx, dz = q.z - s_car.wz;
        d2[i] = dx * dx + dz * dz;                                   /* hand: scalar products */
    }
    for (k = 0; k < TILE_N; k++) {
        float x, z, yaw;
        const mdl_t *m;
        int car = -1;
        if (k > 0u) {                                                /* the nearest traffic not yet tiled */
            float best = TILE_NEAR * TILE_NEAR;
            for (i = 0; i < map_traffic_n(); i++) if (d2[i] < best) { best = d2[i]; car = (int)i; }
            if (car < 0) break;
            d2[car] = 1e30f;
        }
        if (car < 0) { x = s_car.wx; z = s_car.wz; yaw = s_car.wh; m = &mdl_player_body; }
        else { const map_pose_t q = map_traffic_pose((unsigned)car); x = q.x; z = q.z; yaw = q.yaw; m = map_models[map_car[car].body]; }
#ifdef PL4NAR_HILLS
        if (car < 0) { s_tb_y = s_cy; s_tb_p = s_cp; s_tb_r = s_cr; s_tile_s[k] = s_car.z; }
        else {
            const hills_tilt_t t = hills_car(map_traffic_s((unsigned)car), map_lane[map_car[car].lane][0], yaw);
            s_tb_y = t.y; s_tb_p = t.pitch; s_tb_r = t.roll; s_tile_s[k] = map_traffic_s((unsigned)car);
        }
        s_tile_y[k] = s_tb_y;
        {   /* the square in the slid space (y = 0): the car's base slid along the sun, then half the shadow's reach on */
            const float r = shz_fminf(0.5f * m->hi[1] * shz_invf(sy), 2.0f) + s_tb_y * shz_invf(sy);
            s_tile_c[k][0] = shz_fmaf(-s_sun[0], r, x); s_tile_c[k][1] = shz_fmaf(-s_sun[2], r, z);
        }
#else
        {   /* the tile's ground square: on the car, moved half the shadow's reach away from the sun (at most 2 m) */
            const float r = shz_fminf(0.5f * m->hi[1] * shz_invf(sy), 2.0f);
            s_tile_c[k][0] = shz_fmaf(-s_sun[0], r, x); s_tile_c[k][1] = shz_fmaf(-s_sun[2], r, z);
        }
#endif
        s_tile_car[k] = car;
        {   /* under a deck (baked road occlusion under 0.95) the sun is gone: the casters fade to white by 0.80 */
            const float f = shz_saturatef((0.95f - road_ao_at(x, z)) * (1.0f / 0.15f));
            const unsigned gk = (unsigned)shz_lerpf((float)g, 255.0f, f);
            tile_car(k, x, z, yaw, m, gfx_rgb(255u, gk, gk, gk));
        }
        s_tile_n = k + 1u;
    }
    gfx_viewport(tw, th);
}

#ifdef PL4NAR_BLOBS
static int tiled(int car) {
    unsigned k;
    for (k = 0; k < s_tile_n; k++) if (s_tile_car[k] == car) return 1;
    return 0;
}
#endif

/* The screen's side: a ground quad a tile, its texels multiplied into the frame (GFX_MAT_SHADOW). XMTRX = s_vp. */
static void draw_tile_quads(void) {
    const float W = (float)RT_W(s_sizes[s_size][0]), H = (float)RT_H(s_sizes[s_size][1]), us = gfx_reflect_u_scale();
    const float iw = shz_invf(W) * us, ih = shz_invf(H), h = TILE_M * 0.5f;
    gfx_vert_t q[4];
    unsigned k, c;
    if (!s_tile_n) return;
    gfx_bind(GFX_MAT_SHADOW);
#ifdef PL4NAR_HILLS
    for (k = 0; k < s_tile_n; k++) {
        float x0, y0, T, hs;
        const float cx = s_tile_c[k][0], cz = s_tile_c[k][1];
        if (s_tile_car[k] < -1) continue;             /* an empty slot */
        tile_rect(k, &x0, &y0, &T);
        hs = h - 0.5f * TILE_M * shz_invf(T);         /* half a texel in: the tile's own white border */
#ifdef PL4NAR_BLOBS
        if (s_tiles == 1)
#endif
        {                                             /* ON: the square laid on the ground's triangles, through the slid point */
            const float r0[3][2] = { { cx - hs, cz - hs }, { cx + hs, cz - hs }, { cx + hs, cz + hs } };
            const float r1[3][2] = { { cx - hs, cz - hs }, { cx + hs, cz + hs }, { cx - hs, cz + hs } };
            const float gy = s_tile_y[k] * shz_invf(s_sun[1]);
            s_drape_n = hills_tris_near(s_tile_s[k], shz_fmaf(s_sun[0], gy, cx), shz_fmaf(s_sun[2], gy, cz), h * 1.42f + 1.0f, s_drape, HILLS_NEAR_MAX);
            drape(r0, NULL, 1, k, 0.02f);
            drape(r1, NULL, 1, k, 0.02f);
            continue;
        }
#ifdef PL4NAR_BLOBS
        {   /* TYPICAL: one flat quad at the car's ground height (what a flat-ground shadow does on a slope) */
            const float y = s_tile_y[k] + 0.02f, ty = y * shz_invf(s_sun[1]);
            for (c = 0; c < 4u; c++) {
                const int ix = c == 1u || c == 2u, iz = c >= 2u;
                gfx_xform(&q[c], shz_vec3_init(shz_fmaf(s_sun[0], ty, cx + (ix ? h : -h)), y, shz_fmaf(s_sun[2], ty, cz + (iz ? h : -h))));
                q[c].u = (x0 + (ix ? T - 0.5f : 0.5f)) * iw; q[c].v = (y0 + (iz ? T - 0.5f : 0.5f)) * ih;
                q[c].argb = 0xffffffffu; q[c].oargb = 0u;
            }
            quad(q);
        }
#endif
    }
    return;
#endif
    for (k = 0; k < s_tile_n; k++) {
        const float T = s_tile_px, x0 = T * (float)(4u + (k & 3u)), y0 = T * (float)(k >> 2);
        for (c = 0; c < 4u; c++) {
            const int ix = c == 1u || c == 2u, iz = c >= 2u;
            gfx_xform(&q[c], shz_vec3_init(s_tile_c[k][0] + (ix ? h : -h), 0.02f, s_tile_c[k][1] + (iz ? h : -h)));
            q[c].u = (x0 + (ix ? T - 0.5f : 0.5f)) * iw; q[c].v = (y0 + (iz ? T - 0.5f : 0.5f)) * ih;
            q[c].argb = 0xffffffffu; q[c].oargb = 0u;
        }
        quad(q);
    }
}
#endif

static void draw_shadows(void) {
#ifndef PL4NAR_BLOBS                                /* the mesh shadow alone (the owner's word): no blob disc, no flat quad;
                                                       night and the street have no car shadow. -DPL4NAR_BLOBS keeps the discs */
    s_blob_n = 0u;
#ifdef PL4NAR_MAP
    draw_tile_quads();
    draw_road_ao();
#endif
#else
    const float alpha = shz_lerpf(0.50f, 0.25f, s_night);
    unsigned i;
    s_blob_n = 0u;
#ifdef PL4NAR_MAP
#ifdef PL4NAR_HILLS
    s_blob_s = s_car.z;
#endif
    if (!tiled(-1)) blob(s_car.wx, s_car.wz, s_car.wh, alpha);
#if defined(PL4NAR_HILLS) && !defined(PL4NAR_BLOBS)
    if (s_tile_n) { draw_tile_quads(); draw_road_ao(); return; }   /* tiles by day: the cars without one get no blob (the owner's
                                                                     rule; -DPL4NAR_BLOBS keeps them) */
#endif
    for (i = 0; i < map_traffic_n(); i++) {
        const map_pose_t q = map_traffic_pose(i);
        const float dx = q.x - s_car.wx, dz = q.z - s_car.wz;
#ifdef PL4NAR_HILLS
        s_blob_s = map_traffic_s(i);
#endif
#ifdef PL4NAR_HILLS
        {   /* faded over the last 10 m of the reach: no pop at the edge */
            const float d2b = dx * dx + dz * dz;                     /* hand: scalar products */
            if (d2b < BLOB_REACH * BLOB_REACH && !tiled((int)i))
                blob(q.x, q.z, q.yaw, alpha * shz_saturatef((BLOB_REACH - shz_sqrtf(d2b)) * 0.1f));
        }
#else
        if (dx * dx + dz * dz < 80.0f * 80.0f && !tiled((int)i)) blob(q.x, q.z, q.yaw, alpha);    /* hand: scalar products */
#endif
    }
    draw_tile_quads();
    draw_road_ao();
    return;
#else
    blob(s_car.wx, s_car.wz, s_car.wh, alpha);
#endif
    for (i = 0; i < s_nprop; i++) {
        const prop_t *pr = &s_prop[i];
        const float z = wrap_z(pr->z);
        if (pr->wheels && shz_fabsf(z - s_car.wz) < 80.0f) blob(pr->x, z, pr->yaw, alpha);
    }
#endif
}

/* ------------------------------------------------------------------------------------------------ frame */
static void overlay(const demo_stats_t *st) {
    char t[96];
    const unsigned cs = (unsigned)(s_car.clock * 100.0f);
    const float kmh = shz_fabsf(s_car.v) * 3.6f;
    gfx_cull(GFX_CULL_NONE);
    /* game HUD: timer, two bars, speed, map */
    snprintf(t, sizeof t, "%u:%02u.%02u", cs / 6000u, (cs / 100u) % 60u, cs % 100u);
    hud_text(24.0f, 20.0f, 2.0f, 0xffffffffu, t);
    /* the game HUD (no BODY / HEAT bars any more: they showed nothing real) */
#if defined(PL4NAR_DRIVE) && defined(PL4NAR_MAP)
    {   /* DRIVE: the lap, the last and the best under the clock */
        const unsigned l = (unsigned)(s_lap_last * 100.0f), b = (unsigned)(s_lap_best * 100.0f);
        size_t o = (size_t)snprintf(t, sizeof t, "LAP %u", s_laps ? s_laps : 1u);
        if (s_lap_last > 0.0f) o += (size_t)snprintf(t + o, sizeof t - o, "  LAST %u:%02u.%02u", l / 6000u, (l / 100u) % 60u, l % 100u);
        if (s_lap_best > 0.0f) (void)snprintf(t + o, sizeof t - o, "  BEST %u:%02u.%02u", b / 6000u, (b / 100u) % 60u, b % 100u);
        hud_text(24.0f, 56.0f, 1.0f, 0xffffffffu, t);
    }
#endif
#ifdef PL4NAR_DRIVE
    if (!s_info) {                                     /* one line of keys, from controls.h: the drive set */
        unsigned from = 0u;
        if (ctl_hud_line(t, 76u, &from, CTL_W_DRIVE)) hud_text(24.0f, 456.0f, 1.0f, 0xffc0c0c0u, t);
    }
#endif
    snprintf(t, sizeof t, "%3u km/h", (unsigned)kmh);
    hud_text(500.0f, 430.0f, 2.0f, 0xffffffffu, t);
#ifndef PL4NAR_MAP
    hud_map(530.0f, 20.0f, 88.0f, s_car.x * (1.0f / BLD_X), s_car.z * (1.0f / PERIOD), s_car.h);
#endif
    if (s_split && s_reflect && !split_at_end(s_div, SPLIT_COLS)) {   /* the divider, 2 px, and each half's name */
        gfx_bind(GFX_MAT_HUD);
        gfx_quad2d(split_x(s_div) - 1.0f, 0.0f, split_x(s_div) + 1.0f, GFX_SCREEN_H, 9.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0xff101010u);
    }
    if (s_split && s_reflect) {
        static const char *const name[2] = { "REFLECTION ON", "REFLECTION OFF" };
        split_half_t h[2];
        unsigned k;
        split_halves(s_div, SPLIT_COLS, SPLIT_ROWS, &h[0], &h[1]);
        for (k = 0; k < 2u; k++) {
            const int x = split_label_x(&h[k], (unsigned)strlen(name[k]) * 8u + 4u);
            if (x >= 0) hud_text((float)x + 2.0f, 120.0f, 1.0f, k ? 0xffff9080u : 0xff80ff90u, name[k]);
        }
    }
#ifdef PL4NAR_MAP
    if (!s_fx_hidden) {                               /* the effect labels: two lines over the speed, Realtime in yellow */
        fx_entry_t e[PL4NAR_FX_MAX];
        fx_text_t ft;
        const unsigned n = pl4nar_fx_live(s_pv_ok, s_tile_n > 0u ? s_tiles : 0, s_blob_n > 0u, s_ao, s_lamp > 0.5f, e);
        unsigned l, c, c0;
        if (n) {
            (void)fx_format_merged(&ft, e, n);
            for (l = 0; l < ft.lines && l < FX_LINES; l++) {
                const float y = 404.0f - 18.0f * (float)(ft.lines - 1u - l);
                for (c0 = 0, c = 1; ; c++) {              /* runs of one colour */
                    if (ft.line[l][c] && fx_marked(&ft, l, c) == fx_marked(&ft, l, c0)) continue;
                    {
                        char run[FX_COLS + 1u];
                        memcpy(run, &ft.line[l][c0], c - c0); run[c - c0] = '\0';
                        hud_text(64.0f + 8.0f * (float)c0, y, 1.0f, fx_marked(&ft, l, c0) ? 0xffffdc00u : 0xffc8c8c8u, run);
                    }
                    if (!ft.line[l][c]) break;
                    c0 = c;
                }
            }
        }
    }
#endif
#ifdef PL4NAR_WET
    if (!s_fx_hidden) {                               /* the effect's label over the speed */
        static const char *const rain[3] = { "DRY", "DAMP", "WET" };
        hud_text(64.0f, 386.0f, 1.0f, 0xffc8c8c8u, "Wet Road Reflection: SH-4 lookup, PVR added layer + grit");
        snprintf(t, sizeof t, "RAIN %s  streak %s  pools %s  target %ux%u", rain[s_rain], s_wet_streak ? "on" : "off", s_wet_pool_dim ? "dimmed" : "full", (unsigned)s_sizes[s_size][0], (unsigned)s_sizes[s_size][1]);
        hud_text(64.0f, 404.0f, 1.0f, 0xffffdc00u, t);
    }
#endif
#ifdef _arch_dreamcast
    if (!s_fx_hidden && s_heat_lvl && heat_level() > 0.0f) {   /* the heat's label, over the effect labels */
        if (s_heat.short_kb) snprintf(t, sizeof t, "Heat Haze %s (no video memory)", s_heat_lvl == 1u ? "LIGHT" : "STRONG");
        else snprintf(t, sizeof t, "Heat Haze %s copy %u.%02u ms (%lu)", s_heat_lvl == 1u ? "LIGHT" : "STRONG",   /* the read's time: the one cost not measured */
                      (unsigned)(s_heat.copy_us / 1000u), (unsigned)(s_heat.copy_us % 1000u / 10u), (unsigned long)s_heat.copies);
        hud_text(64.0f, 368.0f, 1.0f, 0xffffdc00u, t);
    }
#endif
#ifdef PL4NAR_HILLS
    {
        gfx_cut_stats_t cs;
        gfx_tile_cut_stats(&cs);
        if (cs.failed) hud_text(24.0f, 150.0f, 1.0f, 0xffff6060u, "TILE CUT FAILED: PVR restarted, cut off");   /* left, clear of the insets */
    }
#endif
    if (!s_info) return;
    {
        const unsigned tw = s_sizes[s_size][0], th = s_sizes[s_size][1];
        unsigned keys = 0u, from = 0u;
        float y;
        while (ctl_hud_line(t, 76u, &from, CTL_W_TECH)) keys++;
        y = 380.0f - 16.0f * (float)(5u + keys);    /* the block ends above the effect labels (y 386) and the speed (430) */
        snprintf(t, sizeof t, "FPS %u.%u  CPU %u.%02u ms  PVR %u.%02u ms", (unsigned)(st->fps_x10 / 10u), (unsigned)(st->fps_x10 % 10u),
                 (unsigned)(st->cpu_us / 1000u), (unsigned)((st->cpu_us % 1000u) / 10u), (unsigned)(st->pvr_us / 1000u), (unsigned)((st->pvr_us % 1000u) / 10u));
        hud_text(24.0f, y, 1.0f, 0xffffffffu, t); y += 16.0f;
        snprintf(t, sizeof t, "RAM free %u KB  VRAM free %u KB", (unsigned)st->ram_free_kb, (unsigned)st->vram_free_kb);
        hud_text(24.0f, y, 1.0f, 0xffffffffu, t); y += 16.0f;
        if (s_reflect) {
            snprintf(t, sizeof t, "PLANAR REFLECTION ON  %ux%u = %u KB  F0 %u.%02u", tw, th, gfx_reflect_bytes() / 1024u, (unsigned)s_f0, (unsigned)(s_f0 * 100.0f + 0.5f) % 100u);
            hud_text(24.0f, y, 1.0f, 0xff80ff90u, t); y += 16.0f;
            snprintf(t, sizeof t, "lookup %u.%02u ms  PVR mirror %u.%02u ms  tris %u + %u mirror", (unsigned)(s_refl_cpu_us / 1000u), (unsigned)((s_refl_cpu_us % 1000u) / 10u),
                     (unsigned)(st->rtt_us / 1000u), (unsigned)((st->rtt_us % 1000u) / 10u), (unsigned)s_tris_main, (unsigned)s_tris_refl);
            hud_text(24.0f, y, 1.0f, 0xff80ff90u, t); y += 16.0f;
        } else {
            hud_text(24.0f, y, 1.0f, 0xffff9080u, "PLANAR REFLECTION OFF (flat)"); y += 16.0f;
            snprintf(t, sizeof t, "tris %u", (unsigned)s_tris_main);
            hud_text(24.0f, y, 1.0f, 0xffff9080u, t); y += 16.0f;
        }
        snprintf(t, sizeof t, "%s  TIME %s  SUN %s  clock %u.%02u  lamps %u%%  night gain %u.%02u", s_low ? "LOOK 64x64 16col" : "LOOK full", s_tod_name[s_tod_mode], s_sun_name[s_sun_mode],
                 (unsigned)s_tod, (unsigned)(s_tod * 100.0f) % 100u, (unsigned)(s_lamp * 100.0f), (unsigned)s_lamp_gain, (unsigned)(s_lamp_gain * 100.0f + 0.5f) % 100u);
        hud_text(24.0f, y, 1.0f, 0xffffe0a0u, t); y += 16.0f;
        for (from = 0u; ctl_hud_line(t, 76u, &from, CTL_W_TECH); y += 16.0f) hud_text(24.0f, y, 1.0f, 0xffc0c0c0u, t);   /* the keys, from controls.h: what read_pad does */
        if (s_reflect) {                                  /* the target, in two insets down the right edge (demo.h THUMB_*) */
            const unsigned tw = s_sizes[s_size][0], th = s_sizes[s_size][1];
            const float iw = shz_invf((float)RT_W(tw)), ih = shz_invf((float)RT_H(th));
            float yt = THUMB_TOP + thumb(THUMB_MIRROR_W, THUMB_MIRROR_W * (float)th * shz_invf((float)tw), THUMB_TOP, (float)tw * iw, (float)th * ih, 0.0f, "REFLECTION TARGET");
#ifdef PL4NAR_MAP                                  /* the shadow tiles' region, to the mirror's right in the target */
#ifdef PL4NAR_HILLS
            yt += thumb(THUMB_TILES_W, THUMB_TILES_W * (float)RT_TILES_H * shz_invf((float)RT_TILES_W), yt, (float)(tw + RT_TILES_W) * iw, (float)RT_TILES_H * ih, (float)tw * iw, "SHADOW TILES");
#else
            yt += thumb(THUMB_TILES_W, THUMB_TILES_W * (float)th * shz_invf((float)tw), yt, 1.0f, 1.0f, 0.5f, "SHADOW TILES");
#endif
#endif
            (void)yt;
        }
    }
}

#ifdef DEMO_HOST
/* Host preview only: put the car (z along the street or the loop, x across), the hour (the clock paused), the camera. */
static int s_host_cam;
static shz_vec3_t s_host_eye, s_host_at;
void demo_host_pose(float z, float x, float orbit, float tod, int reflect) {
    s_car.z = z; s_car.x = x; s_car.h = 0.0f; s_car.cam_h = 0.0f; s_car.orbit = orbit; s_car.v = 0.0f;
    s_tod = tod; s_tod_mode = 3u; s_reflect = reflect; s_info = 0; s_host_cam = 0;
    HILLS_SNAP();
    demo_place(0.0f);
}
void demo_host_cam(float ex, float ey, float ez, float ax, float ay, float az) { s_host_cam = 1; s_host_eye = shz_vec3_init(ex, ey, ez); s_host_at = shz_vec3_init(ax, ay, az); }
void demo_host_car(float *x, float *z, float *yaw) { *x = s_car.wx; *z = s_car.wz; *yaw = s_car.wh; }
float demo_host_orbit(void) { return s_car.orbit; }
void demo_host_eye(float *x, float *z) { *x = s_camx; *z = s_camz; }   /* the last drawn frame's eye */
#ifdef PL4NAR_HILLS
float demo_host_car_y(void) { return s_cy; }
/* The lamps' check: car_point against the body's own matrix (draw_player's XMTRX chain), the largest gap in metres over
   the four lamps and a roof point. 0 means the lights sit on the body as it is drawn. */
float demo_host_lamp_err(void) {
    static const float pt[5][3] = { { 2.40f, 0.62f, 0.60f }, { 2.40f, -0.62f, 0.60f }, { -2.42f, 0.62f, 0.60f }, { -2.42f, -0.62f, 0.60f }, { 0.0f, 0.0f, 1.3f } };
    float worst = 0.0f;
    unsigned i;
    shz_xmtrx_init_translation(s_car.wx, CAR_Y, s_car.wz);
    shz_xmtrx_rotate_y(s_car.wh);
    shz_xmtrx_rotate_x(-s_cp); shz_xmtrx_rotate_z(s_cr);
    for (i = 0; i < 5u; i++) {
        float c[3];
        const shz_vec3_t m = shz_xmtrx_transform_point3(shz_vec3_init(pt[i][1], pt[i][2], pt[i][0]));
        car_point(pt[i][0], pt[i][1], pt[i][2], c);
        worst = shz_fmaxf(worst, shz_fmaxf(shz_fabsf(m.x - c[0]), shz_fmaxf(shz_fabsf(m.y - c[1]), shz_fabsf(m.z - c[2]))));
    }
    return worst;
}
#endif
#endif

void demo_draw(const demo_stats_t *st) {
    const shz_sincos_t hc = shz_sincosf(s_car.wh), cc = shz_sincosf(s_car.wcam + s_car.orbit);
    const shz_vec3_t eye_c = shz_vec3_init(s_car.wx - cc.sin * CAM_DIST, CAM_HEIGHT + CAR_Y, s_car.wz - cc.cos * CAM_DIST);
#ifdef CAM_PITCH
    /* low: along the camera's own yaw (through the car, the orbit kept), CAM_PITCH down, a point 10 m on */
    const shz_sincos_t pc = shz_sincosf(CAM_PITCH);
    const shz_vec3_t at_c = shz_vec3_init(shz_fmaf(cc.sin, 10.0f * pc.cos, eye_c.x), shz_fmaf(-10.0f, pc.sin, eye_c.y), shz_fmaf(cc.cos, 10.0f * pc.cos, eye_c.z));
    (void)hc;
#else
    const shz_vec3_t at_c = shz_vec3_init(s_car.wx + hc.sin * 1.2f, 1.0f + CAR_Y, s_car.wz + hc.cos * 1.2f);
#endif
    const float ck = s_cine.on ? shz_saturatef((s_cine.t - CINE_HOLD) * (1.0f / CINE_BLEND)) : 1.0f, cs = ck * ck * (3.0f - 2.0f * ck);
    const shz_vec3_t eye_f = s_cine.on ? shz_vec3_lerp(s_cine.eye, eye_c, cs) : eye_c, at_f = s_cine.on ? shz_vec3_lerp(s_cine.at, at_c, cs) : at_c;
#ifdef DEMO_HOST
    const shz_vec3_t eye = s_host_cam ? s_host_eye : eye_f, at = s_host_cam ? s_host_at : at_f;
#else
    const shz_vec3_t eye = eye_f, at = at_f;
#endif
    const float eye3[3] = { eye.x, eye.y, eye.z }, at3[3] = { at.x, at.y, at.z }, up3[3] = { 0.0f, 1.0f, 0.0f };
    s_camz = eye.z; s_camx = eye.x;
    s_tris_refl = 0u;
    tod_eval();
#ifdef _arch_dreamcast
#ifdef PL4NAR_HILLS
    {   /* a failed tile cut restarts the PVR (gfx_pvr.c): the heat texture went with the old video memory */
        static uint32_t failed_seen;
        gfx_cut_stats_t cs;
        gfx_tile_cut_stats(&cs);
        if (cs.failed != failed_seen) { failed_seen = cs.failed; heat_reset(); }
    }
#endif
    heat_copy(heat_level());                      /* no scene open yet: the store queue through pvr_txr_load */
#endif
    gfx_clear_colour(s_haze);
    s_pv_ok = 0;
#ifdef PL4NAR_MAP
    s_tile_n = 0u;
#endif
    if (s_reflect) {
        const float tw = (float)s_sizes[s_size][0], th = (float)s_sizes[s_size][1], fov = FOV_MAIN * SH4DER_PL4NAR_FOV_SCALE;
#ifdef PL4NAR_HILLS
        road_plane_fit();
#endif
        s_pv_ok = sh4der_pl4nar_view_build(&s_pv, &s_road_plane, eye3, at3, up3, fov, GFX_NEAR, tw, th, s_reach * 0.25f);   /* the fade: a quarter of the reach */
    }
    if (s_pv_ok) {
        const float tw = (float)s_sizes[s_size][0], th = (float)s_sizes[s_size][1], fov = FOV_MAIN * SH4DER_PL4NAR_FOV_SCALE;
        memcpy(s_vp, s_pv.row, sizeof s_vp);
        s_vw = tw; s_vh = th;
        s_focal = th * 0.5f * shz_divf(shz_cosf(fov * 0.5f), shz_sinf(fov * 0.5f));
        s_haze_max = (unsigned)(HAZE_MAX * 0.45f * 256.0f);
#ifdef PL4NAR_HILLS
        {
            unsigned cw, ch;
            tile_cut_size(&cw, &ch);
            gfx_tile_cut(cw, ch);
        }
#endif
        if (gfx_reflect_begin() == 0) {
            gfx_viewport(tw, th);
            gfx_bind(GFX_MAT_FLAT);
            gfx_cull(GFX_CULL_NONE);
            gfx_quad2d(0.0f, 0.0f, tw, th, 0.00002f, 0.0f, 0.0f, 0.0f, 0.0f, gfx_mod(COL_GROUND_REFLECT, s_road_col));   /* behind everything: a ray into the road */
            draw_sky(eye);
            gfx_cull(GFX_CULL_MIRROR);
            draw_buildings(0);
            draw_props(1);
#ifdef PL4NAR_MAP
            draw_tiles(tw, th);
#endif
            if (s_lamp > 0.02f) {                             /* the lamp heads, lit: what sweeps over the paint at night */
                spark_view(eye, at);
                gfx_list_translucent();
                gfx_bind(GFX_MAT_GLOW);
                gfx_cull(GFX_CULL_NONE);
                draw_lamps(2);
#ifdef PL4NAR_WET
                wet_lights(1);
#endif
            }
            s_tris_refl = gfx_tris_sent();
            gfx_reflect_end();
        }
    }
    s_haze_max = (unsigned)(HAZE_MAX * 256.0f);
    sh4der_pl4nar_viewproj_rows(s_vp, NULL, eye3, at3, up3, FOV_MAIN, GFX_NEAR, GFX_SCREEN_W, GFX_SCREEN_H);
    s_vw = GFX_SCREEN_W; s_vh = GFX_SCREEN_H;
    s_focal = GFX_SCREEN_H * 0.5f * shz_divf(shz_cosf(FOV_MAIN * 0.5f), shz_sinf(FOV_MAIN * 0.5f));
    gfx_frame_begin();
    gfx_viewport(GFX_SCREEN_W, GFX_SCREEN_H);
    draw_sky(eye);
    gfx_cull(GFX_CULL_BACK);
    draw_road();
    draw_buildings(1);
    draw_props(0);
    draw_player();
    gfx_list_translucent();
    spark_view(eye, at);
    gfx_cull(GFX_CULL_NONE);
#ifdef PL4NAR_WET
    wet_grid();                                       /* the reflection (in rain) and the grit, under the shadows and the pools */
#endif
    VP_LOAD();
    gfx_bind(GFX_MAT_HUD);
    draw_shadows();
    gfx_bind(GFX_MAT_ADD);
    draw_lamps(0);
    draw_car_lights(0);
    if (s_pv_ok) draw_player_reflection();
    gfx_cull(GFX_CULL_NONE);
    gfx_bind(GFX_MAT_GLOW);
    draw_lamps(1);
    draw_car_lights(1);
#ifdef PL4NAR_WET
    wet_lights(0);
    wet_rain();
#endif
#ifdef _arch_dreamcast
    {   /* the heat shimmer, the last world thing; its own header and strips (not counted in s_tris_main) */
        float hp[3];
        gfx_vert_t hv;
        heat_view(eye3, at3);                     /* this frame's camera: the layer follows a turn (heat v2) */
        if (heat_anchor(eye3, at3, CAR_Y, hp)) {
#ifdef PL4NAR_HILLS
            if (!heat_anchor(eye3, at3, hills_h(hp[0], hp[2]), hp)) hp[1] = -1e9f;   /* the road's own height 70 m on (HEAT_DIST) */
#endif
            VP_LOAD();
            gfx_xform(&hv, shz_vec3_init(hp[0], hp[1], hp[2]));
            if (hp[1] > -1e8f && !hv.clip) heat_draw(hv.x, hv.y, hv.z, heat_level(), s_car.clock);
        }
    }
#endif
    s_tris_main = gfx_tris_sent();
    if (!s_hud_hidden) overlay(st);
    gfx_frame_end();
}
