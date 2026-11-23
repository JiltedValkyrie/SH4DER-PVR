/* SH4DER-PVR demo: "The Little Match Girl" (a wordless 60 second short film), the Dreamcast program.
 * Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
 *
 * This is the SCAFFOLD: it boots and plays the ten shots in order with placeholder art, so the look can be seen and
 * then improved. All art is built from numbers (matchlight_scene.h); nothing is loaded from a disc.
 *
 * What it draws, per frame:
 *   opaque list      the ground grid (shaped snow: banks, drifts and piles from ml_snow_height, vertex coloured, lit by an
 *                    ambient colour, a moon shade by slope and a warm term that falls off with distance from the flame),
 *                    the trail patch (a fine strip along the girl's walk whose vertices follow the snow dent grid, so
 *                    her footprints are real pits), house fronts, pale windows,
 *                    the girl (four dark quads), the small tree (shot M6). Every polygon uses fog table 1.
 *   punch-through    falling snow (the library's sh4der_motes).
 *   translucent list the warm pool (sh4der_pool fans, additive), the flame halo and window glows
 *                    (sh4der_spark quads), the three hooks (film_fog_draw, film_stars_draw, film_vision_draw's
 *                    placeholder quad), the full-screen fade / flash quad, the debug caption and the optional readout.
 *
 * Pad: A pause / resume the clock; D-pad right / left next / previous shot; Y show / hide the readout; X show / hide the
 * debug caption (the shot's name and a few words on what it shows, bottom left).
 *
 * KallistiOS paths used: pvr_init, pvr_fog_table_color + pvr_fog_table_linear (fog table 1), pvr_set_bg_color,
 * pvr_scene_begin / pvr_list_begin / pvr_prim (every header and vertex: this scene sends about 3,000 vertices, so the
 * direct-render path buys little), pvr_txr_load_ex (twiddled textures), pvr_txr_load (readout), vid_clear (boot stages),
 * timer_us_gettime64 (the clock), maple (the pad).
 * Every polygon header is checked with sh4der_poly_hdr_ok and every triangle with sh4der_tri_area_ok before it is sent.
 */
#ifndef _arch_dreamcast
#error "matchlight.c is the Dreamcast program; the scene maths is host tested by test/matchlight_scene_test.c"
#endif

#include <stdio.h>
#include <string.h>
#include <kos.h>
#include <dc/minifont.h>
#include "matchlight_scene.h"
#include "sh4der_guard.h"
#include "sh4der_pool.h"
#include "sh4der_spark.h"
#include "sh4der_motes.h"
#include "sh4der_deform.h"
#include "sh4der_mist.h"
#include "sh4der_rise.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

/* ------------------------------------------------------------------------------------------------------------- */
/* Tuning numbers of the program (the scene's own numbers live in matchlight_scene.h). */

#define STAGE_HOLD_MS     300           /* each boot colour is held this long so a camera can see it, milliseconds */
#define STUCK_WAITS       20u           /* KallistiOS waits (100 ms each) in a row before the PVR is called stuck */
#define MAX_DT_S          0.1f          /* longest frame step the clock accepts, seconds (a stall does not jump the film) */
#define TA_VERTEX_KB      512u          /* TA vertex buffer per frame, kilobytes (the KallistiOS default, set explicitly) */
#define OPB_OVERFLOW      6             /* extra object-pointer-buffer blocks the TA may spill into (an overflow hangs a real console) */
#define BIN_OPAQUE        PVR_BINSIZE_32   /* opaque bin: the dense ground grid puts many triangles in a tile */
#define BIN_TRANSLUCENT   PVR_BINSIZE_16   /* translucent bin */
#define BIN_PUNCH         PVR_BINSIZE_8    /* punch-through bin: the snow dots are small and sparse */
#define STATS_WINDOW_US   2000000u      /* the readout's frame statistics cover this long, microseconds */
#define HUD_REFRESH_S     0.25f         /* the readout image is rebuilt this often, seconds */
#define HUD_W             512           /* readout image width, texels (a power of two) */
#define HUD_H             64            /* readout image height, texels */
#define HUD_LINE_PX       20            /* distance between readout text lines, pixels */
#define HUD_X             64.0f         /* readout left edge on screen, pixels */
#define HUD_Y             400.0f        /* readout top edge on screen, pixels */
#define HUD_TENTHS_MAX    9999u         /* largest readout number in tenths (999.9) */
#define HUD_LINE_BUF      96            /* text buffer of one readout line, characters (longer than any line can be) */
#define HUD_CHARS         42            /* longest readout line that fits at the widest glyph the font may have (12 px) */
#define Z_OVERLAY         900.0f        /* depth key of the full-screen fade quad (larger is nearer: in front of everything) */
#define Z_HUD             1000.0f       /* depth key of the readout (in front of the fade) */
#define GROUND_Y          0.0f          /* world height of the undisturbed snow surface, metres */
#define TRAIL_MID_Z       19.5f         /* the snow dent grid is centred here along the street, metres: the middle of the girl's walk */
#define CAP_W             256           /* caption image width, texels (a power of two) */
#define CAP_H             16            /* caption image height, texels: one text line */
#define CAP_X             40.0f         /* caption left edge on screen, pixels */
#define CAP_Y             440.0f        /* caption top edge on screen, pixels (bottom left corner) */
#define CAP_Y_OVER_HUD    380.0f        /* the same while the readout is shown: just above it */
#define POOL_Y            0.04f         /* the pool fans lie this high over the snow surface, metres */
#define POOL_ALPHA_INNER  0.85f         /* centre alpha of the inner pool fan at full flame */
#define POOL_OUTER_SCALE  1.8f          /* the outer, fainter fan is this many times wider than the inner one */
#define POOL_ALPHA_OUTER  0.22f         /* centre alpha of the outer pool fan at full flame */
#define HALO_BASE         0.25f         /* flame halo half size at zero pool radius, metres */
#define HALO_PER_RADIUS   0.20f         /* flame halo half size added per metre of pool radius */
#define HALO_BRIGHT       0.9f          /* flame halo brightness share of the pool colour */
#define CORE_HALF         0.06f         /* hot flame core half size, metres */
#define FLICKER_RATE      41.0f         /* flame flicker, radians a second */
#define FLICKER_DEPTH     0.06f         /* flame size swing, as a share of its size */
#define GLOW_HALF         0.8f          /* window glow half size, metres */
#define GLOW_OFFSET       0.3f          /* window glow sits this far out from its wall, toward the street, metres */
#define GLOW_BRIGHT       0.6f          /* window glow brightness before the fog share, 0..1 */
#define VISION_HALF       0.38f         /* placeholder vision quad half size, metres */
#define VISION_BRIGHT     0.8f          /* placeholder vision quad brightness at full flame */
#define VISION_DX         0.5f          /* vision offset from the flame, across, metres */
#define VISION_DY         0.23f         /* vision offset from the flame, up, metres */
#define VISION_DZ         0.65f         /* vision offset from the flame, along (behind the girl, away from the camera), metres */
#define GIRL_WARM_GAIN    0.35f         /* how much of the pool colour lights the girl's lower edge */
#define TREE_WARM_GAIN    0.30f         /* the same for the tree */
#define TREE_TRUNK_HALF   0.10f         /* trunk half width, metres */
#define TREE_TRUNK_TOP    0.60f         /* trunk height, metres */
#define SIDE_WALL_SHADE   0.7f         /* a house's near side wall is this share as bright as its front (it is in shade) */
#define WINDOW_BRIGHT     0.9f          /* a lit window's share of the pale window colour */
#define TRUNK_SHADE       0.6f          /* the trunk is this share of the tree colour (darker than the branches) */
#define TIER_LAYER        0.02f         /* each tree tier sits this much nearer the camera than the one below, metres, so tiers do not share a plane */
#define TREE_HEIGHT       3.0f          /* tree height, metres (for the warm gradient) */
#define SNOW_EMITTERS     3u            /* snow emitters along the street */
#define SNOW_EMITTER_SEED_STEP 1000u    /* seed gap between emitters, so their fields differ */
#define MIST_SIDE         64            /* mist noise texture side, texels: 8 KB of video memory at 16 bits */
#define MIST_NEAR_Z       19.0f         /* the fog bank's near face, metres along the street (behind the girl and the tree) */
#define MIST_DEPTH        14.0f         /* the bank runs this far back from its near face, metres */
#define MIST_HALF_WIDTH   11.0f         /* half the bank's width, metres: the street from wall to wall and a little more */
#define MIST_TOP_Y        4.5f          /* top of the bank, metres */
#define MIST_FEATHER      2.0f          /* the bank's soft top, metres (the film brief: "top edge soft over 2 m") */
#define MIST_SLICES       10u           /* sheets in the bank: 40 triangles */
#define MIST_OPACITY_NIGHT 0.75f        /* share of light the whole bank stops at night */
#define MIST_OPACITY_DAWN 0.35f         /* and at the end of the dawn shot: the fog thins */
#define MIST_SEED         7u            /* any number: picks the bank's movement and the noise pattern */
#define STAR_FIELD        10u           /* fixed stars in the sky above the fog */
#define STAR_HALF         0.22f         /* a fixed star's half size, metres (about 3 pixels at their distance) */
#define STAR_BRIGHT       0.55f         /* a fixed star's brightness, 0..1 */
#define STAR_TWINKLE      0.25f         /* share of that brightness the twinkle removes */
#define STAR_TWINKLE_RATE 2.1f          /* twinkle speed, radians a second */
#define CANDLE_LIFT_S     4.0f          /* seconds into the tree shot at which the first candle lifts off */
#define CANDLE_FOREVER_S  1000.0f       /* a rise time far past the last arrival: every candle is a star at rest */
#define CANDLE_DEPTH      0.06f         /* the candles sit this far in front of the tree's plane, metres */
#define SHOT_TREE         5             /* index of the shot with the tree (M6) */
#define SHOT_FALL         6             /* index of the falling-star shot (M7) */
#define SHOT_BUNDLE       8             /* index of the whole-bundle shot (M9) */
#define SHOT_DAWN         9             /* index of the dawn shot (M10) */
#define FALL_START_S      1.4f          /* seconds into its shot at which the falling star appears */
#define FALL_TIME_S       0.6f          /* seconds it takes to cross (the film brief) */
#define FALL_TAIL         4u            /* quads in its tail, behind the head */
#define FALL_TAIL_GAP     0.06f         /* each tail quad trails the one before by this share of the path */
#define FALL_HALF         0.45f         /* the head's half size, metres */
#define BUNDLE_LIGHTS     10u           /* lights that rise from the last pool */
#define BUNDLE_RING       1.1f          /* they start on a ring of this radius round the flame, metres */
#define BUNDLE_START_S    0.6f          /* seconds into the shot at which the first lifts off */
#define SNOW_DEFORM_CELLS (SH4DER_DEFORM_GRID_W * SH4DER_DEFORM_GRID_H)   /* snow dent grid: one byte a cell, 25,600 bytes (20 x 20 m at 0.125 m) */

/* The fog colour of fog table 1 (night fog, 0..1): a cold blue-grey bank lighter than the sky so the street dissolves into it. */
static const float fog_rgb[3] = { 0.20f, 0.22f, 0.32f };
/* Wall base colour as 0..1, from the scene's 0..255 numbers. */
static const float wall_rgb[3] = { (float)ML_WALL_R / 255.0f, (float)ML_WALL_G / 255.0f, (float)ML_WALL_B / 255.0f };
/* Window colour as 0..1, from the scene's 0..255 numbers. */
static const float window_rgb[3] = { (float)ML_WINDOW_R / 255.0f, (float)ML_WINDOW_G / 255.0f, (float)ML_WINDOW_B / 255.0f };
/* Night ambient, the same numbers as the scene: dawn walls are lit by the ratio of the current ambient to this. */
static const float amb_night[3] = { ML_AMB_NIGHT_R, ML_AMB_NIGHT_G, ML_AMB_NIGHT_B };
/* The girl's own colour: a very dark blue (0..1). */
static const float girl_rgb[3] = { 12.0f / 255.0f, 14.0f / 255.0f, 24.0f / 255.0f };
/* The tree's own colour: dark green (0..1), after the brief's (120, 180, 120) at night. */
static const float tree_rgb[3] = { 36.0f / 255.0f, 56.0f / 255.0f, 44.0f / 255.0f };
/* The window glow colour: pale cold (0..255). */
static const uint8_t glow_rgb[3] = { 110u, 130u, 175u };
/* The hot flame core colour: near white-gold (0..255). */
static const uint8_t core_rgb[3] = { 255u, 235u, 190u };
/* The fog bank's colour at night and at the end of the dawn shot, 0..255: a little lighter than the table fog so the bank
 * reads as a band. */
static const uint8_t mist_night_rgb[3] = { 78u, 86u, 118u };
static const uint8_t mist_dawn_rgb[3] = { 150u, 165u, 190u };
/* The fixed stars: world positions well behind and above the fog bank (x across, y up, z along), metres. */
static const float star_field[STAR_FIELD][3] = {
    { -11.0f, 10.5f, 44.0f }, { -7.5f, 13.0f, 44.0f }, { -4.0f, 9.0f, 44.0f }, { -1.5f, 12.0f, 44.0f }, { 1.0f, 14.5f, 44.0f },
    { 3.5f, 10.0f, 44.0f }, { 6.0f, 13.5f, 44.0f }, { 8.5f, 9.5f, 44.0f }, { 11.0f, 12.5f, 44.0f }, { 13.5f, 15.0f, 44.0f },
};
/* The falling star's path across the sky: from the upper left to the lower right, metres. */
static const float fall_from[3] = { -8.0f, 15.0f, 40.0f };
static const float fall_to[3] = { 7.0f, 9.0f, 40.0f };

/* ------------------------------------------------------------------------------------------------------------- */
/* State. */

/* Keeps a function out of main: the SH compiler of this toolchain hit an internal error (register reload) when everything
 * was inlined into the main loop. */
#define NOINLINE __attribute__((noinline))

/* A vertex after the view transform: the view-space position (for the area guard), the screen position and depth key,
 * and its colour. v[2] below the near plane marks a point that must not be sent. */
typedef struct vert {
    float v[3];
    float x, y, iz;
    uint32_t argb;
} vert_t;

static ml_camera_t cam;                                   /* the camera of the frame being drawn */
static sh4der_spark_view_t spark_view;                    /* the same camera for the spark quads (world space) */
static sh4der_spark_view_t snow_view;                     /* the camera with y and z swapped, for the snow's z-up field */
static sh4der_spark_screen_t spark_screen;                /* screen centre and focal length for the spark and snow draws */
static sh4der_spark_pool_t glow_pool, vision_pool;        /* 24 additive quads each: flame and windows; the vision */
static sh4der_spark_pool_t star_pool, extra_pool;         /* 24 each: fixed stars and candles; the falling star or the last rise */
static sh4der_rise_params_t candle_rise, bundle_rise;     /* how the tree's candles, and the lights of the last pool, lift off */
static uint8_t mist_noise[MIST_SIDE * MIST_SIDE];         /* the mist's cloud pattern, one byte a texel, 4 KB (built once) */
static uint16_t mist_px[MIST_SIDE * MIST_SIDE] __attribute__((aligned(32)));   /* the same as an ARGB4444 texture, 8 KB */
static pvr_ptr_t mist_tex;                                /* the mist texture in video memory */
static pvr_poly_hdr_t hdr_mist;                           /* the fog bank's sheets: textured, alpha blended */
static sh4der_motes_params_t snow_params;                 /* the snow's look */
static sh4der_motes_emitter_t snow_emitters[SNOW_EMITTERS];   /* where the snow starts, in the swapped (x, z, y) space */
static sh4der_motes_draw_t snow_draws[SH4DER_MOTES_DRAW_MAX] __attribute__((aligned(32)));   /* collected snow sprites */
static uint8_t deform_cells[SNOW_DEFORM_CELLS] __attribute__((aligned(32)));   /* the snow dent grid, 25,600 bytes */
static sh4der_deform_t deform;                            /* its window and settings */
static float vision_level;                                /* flame strength this frame, 0..1 (set before the hooks are called) */
static float vision_flame[3];                             /* flame position this frame (set before the hooks are called) */
static unsigned steps_done;                               /* footprints stamped so far (see footprints_update) */
static vert_t ground[ML_GROUND_VERTS];                    /* the ground vertices of the frame, 1,066 x 28 bytes */
static float ground_h[ML_GROUND_VERTS];                   /* snow height at each ground vertex, metres (built once) */
static float ground_shade[ML_GROUND_VERTS];               /* moon shade at each ground vertex, share of level snow (built once) */
static vert_t trail[ML_TRAIL_VERTS];                      /* the trail patch vertices of the frame, 801 x 28 bytes */
static float trail_h[ML_TRAIL_VERTS];                     /* height of each trail vertex from the dent grid, metres */
static float trail_depth[ML_TRAIL_VERTS];                 /* how deep it is pressed, 0..1 (darkens it) */
static float trail_shade[ML_TRAIL_VERTS];                 /* moon shade from the dent's slope, share of level snow */
static int trail_dirty;                                   /* 1: the dent grid changed and the three arrays above must be rebuilt */

static uint16_t atlas_px[ML_ATLAS_SIDE * ML_ATLAS_SIDE] __attribute__((aligned(32)));   /* sprite atlas, RGB565, 8 KB */
static uint16_t mask_px[SH4DER_MOTES_MASK_SIDE * SH4DER_MOTES_MASK_SIDE] __attribute__((aligned(32)));   /* snow dot mask, 512 B */
static uint16_t hud_px[HUD_W * HUD_H] __attribute__((aligned(32)));   /* readout image, ARGB1555, 64 KB */
static uint16_t cap_px[CAP_W * CAP_H] __attribute__((aligned(32)));   /* caption image, ARGB1555, 8 KB */
static pvr_ptr_t atlas_tex, mask_tex, hud_tex, cap_tex;   /* the four textures in video memory */
static pvr_poly_hdr_t hdr_set, hdr_pool, hdr_fade, hdr_hud, hdr_cap;   /* opaque set; pool fans; fade; readout; caption */

static uint64_t t_boot;                                   /* timer reading at boot, microseconds */
static unsigned long ta_kb, opb_kb;                       /* most TA vertex and object-pointer-buffer kilobytes seen in a frame */
static unsigned frame_tris;                               /* triangles this program counted itself sending this frame */
static float clock_s;                                     /* the film clock, seconds, 0 .. 60 (wrapped) */
static float snow_time;                                   /* snow time, seconds: runs with the clock, never wraps back */
static int paused, hud_on, cur_shot;                      /* pause flag, readout flag, shot of the last frame */
static int cap_on = 1;                                    /* caption flag: shown from the start (a debug aid) */
static int cap_shot = -1;                                 /* the shot whose caption is in the caption image; -1: none yet */
static float frame_avg_us, frame_worst_us;                /* readout statistics over the last window, microseconds */

/* ------------------------------------------------------------------------------------------------------------- */
/* Boot helpers. */

/* One visible boot stage: clears the displayed frame to one colour (KallistiOS vid_clear, no PVR render needed), prints one
 * serial line, then holds so a camera can see it. White later means the PVR never finished a frame. */
static void boot_stage(unsigned n, uint8_t r, uint8_t g, uint8_t b, const char *what) {
    vid_clear(r, g, b);
    printf("matchlight: stage %u %s at %lu ms\n", n, what, (unsigned long)((timer_us_gettime64() - t_boot) / 1000u));
    if (STAGE_HOLD_MS) {
        thd_sleep(STAGE_HOLD_MS);
    }
}

/* Packs a 0..1 colour into 0xFFRRGGBB (hand: the byte conversion, rounding to nearest). */
static uint32_t pack_rgb(float r, float g, float b) {
    return 0xFF000000u | ((uint32_t)sh4der_fma_(sh4der_sat_(r), 255.0f, 0.5f) << 16) |
           ((uint32_t)sh4der_fma_(sh4der_sat_(g), 255.0f, 0.5f) << 8) | (uint32_t)sh4der_fma_(sh4der_sat_(b), 255.0f, 0.5f);
}

/* Packs 0..255 colour bytes scaled by a 0..1 share into 0xFFRRGGBB, for the additive sprites. */
static uint32_t pack_bytes(const uint8_t rgb[3], float scale) {
    return pack_rgb((float)rgb[0] * (1.0f / 255.0f) * scale, (float)rgb[1] * (1.0f / 255.0f) * scale, (float)rgb[2] * (1.0f / 255.0f) * scale);
}

/* TA usage of the frame being registered: register reads only, taken while a list is still open (before KallistiOS
 * re-arms the TA for the next frame). Keeps the largest value seen. */
static void ta_meter(void) {
    const uint32_t vb = (PVR_GET(PVR_TA_VERTBUF_POS) - PVR_GET(PVR_TA_VERTBUF_START)) & 0x00ffffffu;
    const uint32_t ob = ((PVR_GET(PVR_TA_OPB_POS) << 2) - PVR_GET(PVR_TA_OPB_INIT)) & 0x00ffffffu;

    if (vb / 1024u > ta_kb) {
        ta_kb = vb / 1024u;
    }
    if (ob / 1024u > opb_kb) {
        opb_kb = ob / 1024u;
    }
}

/* Fills the sprite atlas from ml_atlas_value and the snow dot mask from the library, and loads both to video memory
 * (twiddled by KallistiOS). Returns 1 on success, 0 when video memory could not be allocated. */
NOINLINE static int textures_build(void) {
    unsigned x, y;

    atlas_tex = pvr_mem_malloc(sizeof(atlas_px));
    mask_tex = pvr_mem_malloc(sizeof(mask_px));
    hud_tex = pvr_mem_malloc(sizeof(hud_px));
    cap_tex = pvr_mem_malloc(sizeof(cap_px));
    if (!atlas_tex || !mask_tex || !hud_tex || !cap_tex) {
        return 0;
    }
    for (y = 0; y < ML_ATLAS_SIDE; y++) {
        for (x = 0; x < ML_ATLAS_SIDE; x++) {
            const float v = ml_atlas_value(x, y);
            const uint32_t g5 = (uint32_t)sh4der_fma_(v, 31.0f, 0.5f);   /* hand: the grey to a 5 and a 6 bit field */
            const uint32_t g6 = (uint32_t)sh4der_fma_(v, 63.0f, 0.5f);

            atlas_px[y * ML_ATLAS_SIDE + x] = (uint16_t)((g5 << 11) | (g6 << 5) | g5);
        }
    }
    pvr_txr_load_ex(atlas_px, atlas_tex, ML_ATLAS_SIDE, ML_ATLAS_SIDE, PVR_TXRLOAD_16BPP);
    sh4der_motes_mask_make(mask_px);
    pvr_txr_load_ex(mask_px, mask_tex, SH4DER_MOTES_MASK_SIDE, SH4DER_MOTES_MASK_SIDE, PVR_TXRLOAD_16BPP);
    return 1;
}

/* Compiles the five polygon headers of this program. The library compiles its own (spark, snow). */
NOINLINE static void headers_build(void) {
    pvr_poly_cxt_t c;

    /* The set: opaque, untextured, Gouraud, fog table 1, depth tested and written, no culling. */
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_TABLE;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    c.depth.write = PVR_DEPTHWRITE_ENABLE;
    pvr_poly_compile(&hdr_set, &c);
    /* The warm pool: translucent, source alpha added to what is behind (additive light), tested but not written. */
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.gen.alpha = PVR_ALPHA_ENABLE;
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = PVR_BLEND_ONE;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&hdr_pool, &c);
    /* The full-screen fade and flash: ordinary alpha blend, always passes the depth test, never written. */
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    pvr_poly_compile(&hdr_fade, &c);
    /* The readout: the text image as a translucent textured quad. */
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, HUD_W, HUD_H, hud_tex, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&hdr_hud, &c);
    /* The caption: the same kind of quad with its own, smaller text image. */
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, CAP_W, CAP_H, cap_tex, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&hdr_cap, &c);
}

/* Sets up the snow: the look of the specks (a slow fall with a little drift, small hard dots), and three emitters along the
 * street. The motes library works in a z-up space, so the emitter anchors and the view are stored with y and z swapped
 * (world (x, y, z) is motes (x, z, y)): "height above the anchor" in the params is height above the snow here. */
NOINLINE static void snow_setup(void) {
    unsigned i;

    sh4der_motes_params_default(&snow_params);
    snow_params.rate = 40.0f;                    /* specks born a second per emitter; with a 6 s longest life: 240 slots, under the 256 limit */
    snow_params.life_lo = 4.0f;
    snow_params.life_hi = 6.0f;
    snow_params.box_xy = 18.0f;                  /* each emitter fills an 18 m square, centred on its anchor */
    snow_params.box_z_lo = 6.0f;                 /* born 6 to 8 m above the snow */
    snow_params.box_z_hi = 8.0f;
    snow_params.vel_lo[0] = -0.3f; snow_params.vel_hi[0] = 0.4f;     /* a light drift across the street */
    snow_params.vel_lo[1] = -0.2f; snow_params.vel_hi[1] = 0.2f;     /* and along it */
    snow_params.vel_lo[2] = -1.4f; snow_params.vel_hi[2] = -0.9f;    /* falling at about 1 m a second */
    for (i = 0; i < 3; i++) {
        snow_params.acc_lo[i] = 0.0f;
        snow_params.acc_hi[i] = 0.0f;
    }
    snow_params.size_lo = 0.03f;                 /* flakes 3 to 6 cm across */
    snow_params.size_hi = 0.06f;
    snow_params.alpha_in = 0.15f;
    snow_params.clip = 0.333f;
    snow_params.col_hold = 0.70f;
    snow_params.col_end = 1.0f;
    snow_params.cull_k = 0.08f;
    snow_params.lod_cut = 60.0f;                 /* an emitter further than this from the eye is skipped */
    snow_params.min_px = 2.0f;
    snow_params.max_px = 7.0f;
    snow_params.big_px = 6.0f;
    snow_params.big_k = 0.8f;
    snow_params.flat_px = 0.0f;                  /* every flake uses the punch-through mask: no untextured flat draw */
    snow_params.max_draw = ML_SNOW_QUADS;
    snow_params.color_argb = 0xFFE6EEFFu;        /* (230, 238, 255): pale cold white */
    snow_params.fade_argb = 0xFF28304Au;         /* (40, 48, 74): a dying flake sinks into the night */
    for (i = 0; i < SNOW_EMITTERS; i++) {
        snow_emitters[i].anchor[0] = 0.0f;                                   /* motes x = world x */
        snow_emitters[i].anchor[1] = 10.0f + 10.0f * (float)i;               /* motes y = world z: 10, 20, 30 m along the street */
        snow_emitters[i].anchor[2] = GROUND_Y;                               /* motes z = world y: the snow surface */
        snow_emitters[i].seed = SNOW_EMITTER_SEED_STEP * i;
    }
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Submitting geometry. */

/* Transforms one world point to a vertex: view position, screen position, depth key and colour. A point nearer than the
 * near plane gets iz 0 and is refused later by vert_ok. */
static void vert_set(vert_t *o, const float w[3], uint32_t argb) {
    ml_screen_t s = { 0.0f, 0.0f, 0.0f };

    ml_to_view(&cam, w, o->v);
    o->argb = argb;
    if (ml_view_to_screen(&cam, o->v, &s)) {
        o->x = s.x;
        o->y = s.y;
        o->iz = s.iz;
    } else {
        o->x = 0.0f;
        o->y = 0.0f;
        o->iz = 0.0f;
    }
}

/* 1 when the vertex is at or past the near plane and may be sent. */
static int vert_ok(const vert_t *p) {
    return p->v[2] >= ML_NEAR_Z;
}

/* Sends one vertex command to the TA (a stack vertex copied by pvr_prim; the flags say whether it ends the strip). */
static void emit_vertex(const vert_t *p, uint32_t flags) {
    pvr_vertex_t v;

    v.flags = flags;
    v.x = p->x;
    v.y = p->y;
    v.z = p->iz;
    v.u = 0.0f;
    v.v = 0.0f;
    v.argb = p->argb;
    v.oargb = 0;
    pvr_prim(&v, sizeof(v));
}

/* The same with a texture coordinate, for the fog bank's sheets. */
static void emit_vertex_uv(const vert_t *p, float u, float v_tex, uint32_t flags) {
    pvr_vertex_t v;

    v.flags = flags;
    v.x = p->x;
    v.y = p->y;
    v.z = p->iz;
    v.u = u;
    v.v = v_tex;
    v.argb = p->argb;
    v.oargb = 0;
    pvr_prim(&v, sizeof(v));
}

/* Sends a polygon header once after checking it was compiled (a zeroed header is "end of list" to the TA and hangs a real
 * console). Returns 1 when it was sent; when 0, the caller must send no vertices that belong to it. */
static int header_send(const pvr_poly_hdr_t *h) {
    uint32_t words[8];

    memcpy(words, h, sizeof(words));                  /* word access through a copy: no cast of a wider pointer */
    if (!sh4der_poly_hdr_ok(words)) {
        return 0;
    }
    pvr_prim((void *)h, sizeof(*h));
    return 1;
}

/* 1 when all the given vertices are off the same side of the screen (a triangle that cannot show; skipping it saves TA space). */
static int all_offscreen(const vert_t *const *p, unsigned n) {
    unsigned i;
    int left = 1, right = 1, top = 1, bottom = 1;

    for (i = 0; i < n; i++) {
        left = left && p[i]->x < 0.0f;
        right = right && p[i]->x >= (float)ML_SCREEN_W;
        top = top && p[i]->y < 0.0f;
        bottom = bottom && p[i]->y >= (float)ML_SCREEN_H;
    }
    return left || right || top || bottom;
}

/* Sends one triangle (a three vertex strip). Guards: all vertices past the near plane, not off screen, with screen area.
 * Returns 1 when sent. The header must already be sent. */
static unsigned send_tri(const vert_t *a, const vert_t *b, const vert_t *c) {
    const vert_t *p[3];

    p[0] = a; p[1] = b; p[2] = c;
    if (!vert_ok(a) || !vert_ok(b) || !vert_ok(c) || all_offscreen(p, 3)) {
        return 0;
    }
    if (!sh4der_tri_area_ok(a->v, b->v, c->v, ML_NEAR_Z, SH4DER_GUARD_MIN_AREA2)) {
        return 0;
    }
    emit_vertex(a, PVR_CMD_VERTEX);
    emit_vertex(b, PVR_CMD_VERTEX);
    emit_vertex(c, PVR_CMD_VERTEX_EOL);
    frame_tris++;
    return 1;
}

/* Sends one quad as a four vertex strip (corners in strip order 0 1 2 3: triangles 0 1 2 and 1 2 3). Both triangles must pass
 * the guards or nothing is sent. Returns 1 when sent. The header must already be sent. */
static unsigned send_quad(const vert_t *a, const vert_t *b, const vert_t *c, const vert_t *d) {
    const vert_t *p[4];

    p[0] = a; p[1] = b; p[2] = c; p[3] = d;
    if (!vert_ok(a) || !vert_ok(b) || !vert_ok(c) || !vert_ok(d) || all_offscreen(p, 4)) {
        return 0;
    }
    if (!sh4der_tri_area_ok(a->v, b->v, c->v, ML_NEAR_Z, SH4DER_GUARD_MIN_AREA2) ||
        !sh4der_tri_area_ok(b->v, c->v, d->v, ML_NEAR_Z, SH4DER_GUARD_MIN_AREA2)) {
        return 0;
    }
    emit_vertex(a, PVR_CMD_VERTEX);
    emit_vertex(b, PVR_CMD_VERTEX);
    emit_vertex(c, PVR_CMD_VERTEX);
    emit_vertex(d, PVR_CMD_VERTEX_EOL);
    frame_tris += 2u;
    return 1;
}

/* Sends a world quad (corners in strip order), one colour per corner. Returns 1 when sent. The header must already be sent. */
static unsigned world_quad(const ml_quad_t *q, const uint32_t argb[4]) {
    vert_t v[4];
    unsigned k;

    for (k = 0; k < 4u; k++) {
        vert_set(&v[k], q->p[k], argb[k]);
    }
    return send_quad(&v[0], &v[1], &v[2], &v[3]);
}

/* Sends a full-screen-space rectangle (pixel coordinates) as one strip, the corners in strip order, in one colour, at depth key z.
 * With uv non-zero the corners carry the texture corners 0..1 (the readout). No guard needed: the rectangle is a fixed shape. */
static void screen_rect(float x0, float y0, float x1, float y1, float z, uint32_t argb, int uv) {
    unsigned k;

    for (k = 0; k < 4u; k++) {
        pvr_vertex_t v;

        v.flags = k == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = (k & 2u) ? x1 : x0;                    /* strip order: left top, left bottom, right top, right bottom */
        v.y = (k & 1u) ? y1 : y0;
        v.z = z;
        v.u = uv ? ((k & 2u) ? 1.0f : 0.0f) : 0.0f;
        v.v = uv ? ((k & 1u) ? 1.0f : 0.0f) : 0.0f;
        v.argb = argb;
        v.oargb = 0;
        pvr_prim(&v, sizeof(v));
    }
    frame_tris += 2u;
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Footprints (the snow dent grid) and the snow surface. */

/* Empties the snow dent grid: every cell back at the undisturbed surface, the window centred on the girl's walk (it never
 * moves: 20 x 20 m at 0.125 m a cell covers the whole trail patch), no footprints stamped. */
NOINLINE static void snow_reset(void) {
    (void)sh4der_deform_init(&deform, deform_cells, SH4DER_DEFORM_GRID_W, SH4DER_DEFORM_GRID_H, ML_TRAIL_CELL, GROUND_Y - ML_SNOW_DEPTH, GROUND_Y);
    (void)sh4der_deform_recentre(&deform, 0.0f, TRAIL_MID_Z);
    steps_done = 0;
    trail_dirty = 1;
}

/* Brings the dent grid up to the footprints the film has made by now (ml_steps_due): presses each new bare foot into the
 * grid as two overlapping round stamps, heel and toe. When the film goes backward (a loop, a jump to an earlier shot) the
 * grid is emptied and the footprints replayed, so jumping to a shot shows the prints that shot should have. */
NOINLINE static void footprints_update(int shot, float progress) {
    const unsigned due = ml_steps_due(shot, progress);

    if (due < steps_done) {
        snow_reset();
    }
    while (steps_done < due) {
        float x, z;

        ml_foot(steps_done, &x, &z);
        (void)sh4der_deform_stamp(&deform, x, z + ML_PRINT_HALF_GAP, GROUND_Y - ML_PRINT_DEPTH, ML_PRINT_RADIUS, 1.0f);   /* heel: a full press at once */
        (void)sh4der_deform_stamp(&deform, x, z - ML_PRINT_HALF_GAP, GROUND_Y - ML_PRINT_DEPTH, ML_PRINT_RADIUS, 1.0f);   /* toe, toward the cameras */
        steps_done++;
        trail_dirty = 1;
    }
}

/* Reads the trail patch's vertices out of the dent grid: height, pressed depth and moon shade (from the dent's slope) for
 * each. Only called when a footprint was added or the grid emptied, not every frame. The patch's outer ring of vertices is
 * not read from the grid: it is tucked just under the ground grid round it (see trail_draw). */
NOINLINE static void trail_rebuild(void) {
    unsigned i, j;

    for (j = 0; j <= ML_TRAIL_ROWS; j++) {
        for (i = 0; i <= ML_TRAIL_COLS; i++) {
            const unsigned n = j * (ML_TRAIL_COLS + 1u) + i;
            const float x = sh4der_fma_((float)i, ML_TRAIL_CELL, ML_TRAIL_X0);
            const float z = sh4der_fma_((float)j, ML_TRAIL_CELL, ML_TRAIL_Z0);
            float sx, sz;

            if (i == 0u || i == ML_TRAIL_COLS || j == 0u || j == ML_TRAIL_ROWS) {
                trail_h[n] = GROUND_Y - ML_TRAIL_TUCK_Y;
                trail_depth[n] = 0.0f;
                trail_shade[n] = 1.0f;
                continue;
            }
            sh4der_deform_slope(&deform, x, z, &sx, &sz);
            trail_h[n] = sh4der_deform_height(&deform, x, z);
            trail_depth[n] = sh4der_deform_depth(&deform, x, z);
            trail_shade[n] = ml_snow_shade(sx, sz);
        }
    }
    trail_dirty = 0;
}

/* Builds, once, the snow height and the moon shade of every ground grid vertex from ml_snow_height (the banks against the
 * walls, the rolling drifts, the piles). The shape never changes, so the frame only transforms and colours. */
NOINLINE static void ground_setup(void) {
    unsigned i, j;

    for (j = 0; j <= ML_GROUND_ROWS; j++) {
        for (i = 0; i <= ML_GROUND_COLS; i++) {
            const unsigned n = j * (ML_GROUND_COLS + 1u) + i;
            const float x = sh4der_fma_((float)i, ML_GROUND_DX, ML_GROUND_X0);
            const float z = sh4der_fma_((float)j, ML_GROUND_DZ, ML_GROUND_Z0);
            float sx, sz;

            ml_snow_slope(x, z, &sx, &sz);
            ground_h[n] = GROUND_Y + ml_snow_height(x, z);
            ground_shade[n] = ml_snow_shade(sx, sz);
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------- */
/* The set, on the opaque list. */

/* The colour of one snow vertex: ml_snow_colour (ambient, the flame's warm term, a dent's darkening) times the moon shade. */
static uint32_t snow_argb(const float amb[3], float level, float d2, float radius, float depth, float shade) {
    float rgb[3];

    ml_snow_colour(amb, level, d2, radius, depth, rgb);
    return pack_rgb(rgb[0] * shade, rgb[1] * shade, rgb[2] * shade);
}

/* Lights and draws the ground grid: the shaped snow. Every vertex takes its height and moon shade from the tables built by
 * ground_setup; the ambient and the warm term of the flame go into the vertex colour. The quads the trail patch replaces
 * are left out. `flame` is the flame's world position (only its x and z are used), `level` its 0..1 strength, `radius`
 * the pool radius. */
static void ground_draw(const float amb[3], const float flame[3], float level, float radius) {
    unsigned i, j;

    for (j = 0; j <= ML_GROUND_ROWS; j++) {
        for (i = 0; i <= ML_GROUND_COLS; i++) {
            const unsigned n = j * (ML_GROUND_COLS + 1u) + i;
            float w[3];

            w[0] = sh4der_fma_((float)i, ML_GROUND_DX, ML_GROUND_X0);
            w[1] = ground_h[n];
            w[2] = sh4der_fma_((float)j, ML_GROUND_DZ, ML_GROUND_Z0);
            vert_set(&ground[n], w, snow_argb(amb, level, sh4der_len2_(w[0] - flame[0], w[2] - flame[2]), radius, 0.0f, ground_shade[n]));
        }
    }
    for (j = 0; j < ML_GROUND_ROWS; j++) {
        for (i = 0; i < ML_GROUND_COLS; i++) {
            const vert_t *a = &ground[j * (ML_GROUND_COLS + 1u) + i];          /* (i, j) */
            const vert_t *b = &ground[(j + 1u) * (ML_GROUND_COLS + 1u) + i];   /* (i, j + 1) */
            const vert_t *c = a + 1;                                            /* (i + 1, j) */
            const vert_t *d = b + 1;                                            /* (i + 1, j + 1) */

            if (ml_ground_quad_in_trail(i, j)) {
                continue;                                                       /* the trail patch lies here */
            }
            (void)send_quad(a, b, c, d);
        }
    }
}

/* Lights and draws the trail patch: the fine strip of snow along the girl's walk, its vertices pressed down by the dent
 * grid so each footprint is a real pit with a dark far wall. The patch's outer ring reaches ML_TRAIL_TUCK under the ground
 * grid, ML_TRAIL_TUCK_Y below it, so the join has no crack. `dawn` (0..1) fills the pits again over the last shot ("the
 * prints gone under new snow"); the other arguments are those of ground_draw. */
static void trail_draw(const float amb[3], const float flame[3], float level, float radius, float dawn) {
    const float keep = 1.0f - dawn;                       /* share of each dent that is still there */
    unsigned i, j;

    if (trail_dirty) {
        trail_rebuild();
    }
    for (j = 0; j <= ML_TRAIL_ROWS; j++) {
        for (i = 0; i <= ML_TRAIL_COLS; i++) {
            const unsigned n = j * (ML_TRAIL_COLS + 1u) + i;
            const float out_x = i == 0u ? -ML_TRAIL_TUCK : (i == ML_TRAIL_COLS ? ML_TRAIL_TUCK : 0.0f);   /* the outer ring's reach */
            const float out_z = j == 0u ? -ML_TRAIL_TUCK : (j == ML_TRAIL_ROWS ? ML_TRAIL_TUCK : 0.0f);
            const int ring = i == 0u || i == ML_TRAIL_COLS || j == 0u || j == ML_TRAIL_ROWS;
            float w[3];

            w[0] = sh4der_fma_((float)i, ML_TRAIL_CELL, ML_TRAIL_X0) + out_x;
            w[1] = ring ? trail_h[n] : trail_h[n] * keep;                       /* the tuck stays; a dent fills */
            w[2] = sh4der_fma_((float)j, ML_TRAIL_CELL, ML_TRAIL_Z0) + out_z;
            vert_set(&trail[n], w, snow_argb(amb, level, sh4der_len2_(w[0] - flame[0], w[2] - flame[2]), radius, trail_depth[n] * keep,
                                             sh4der_lerp_(1.0f, trail_shade[n], keep)));
        }
    }
    for (j = 0; j < ML_TRAIL_ROWS; j++) {
        for (i = 0; i < ML_TRAIL_COLS; i++) {
            const vert_t *a = &trail[j * (ML_TRAIL_COLS + 1u) + i];            /* (i, j) */
            const vert_t *b = &trail[(j + 1u) * (ML_TRAIL_COLS + 1u) + i];     /* (i, j + 1) */

            (void)send_quad(a, b, a + 1, b + 1);
        }
    }
}

/* Draws the ten house fronts and their lit windows. A house is a front wall (quad), a gable (triangle) and a near side wall
 * (quad), in a dark blue-grey that varies by house; at dawn the walls take the ambient's brightening. */
static void houses_draw(const float amb[3]) {
    unsigned h, j;
    float gain[3];
    int k;

    for (k = 0; k < 3; k++) {
        gain[k] = sh4der_div_(amb[k], amb_night[k]);   /* 1 at night, brighter at dawn */
    }
    for (h = 0; h < ML_HOUSES; h++) {
        const ml_house_t *s = &ml_houses[h];
        const float shade = (float)s->shade * (1.0f / 255.0f);
        const float away = s->face_x < 0.0f ? -s->depth : s->depth;   /* the box runs away from the street */
        const float z1 = s->z0 + s->width;
        const float zm = s->z0 + 0.5f * s->width;
        const uint32_t front = pack_rgb(wall_rgb[0] * shade * gain[0], wall_rgb[1] * shade * gain[1], wall_rgb[2] * shade * gain[2]);
        const uint32_t side = pack_rgb(wall_rgb[0] * shade * gain[0] * SIDE_WALL_SHADE, wall_rgb[1] * shade * gain[1] * SIDE_WALL_SHADE,
                                       wall_rgb[2] * shade * gain[2] * SIDE_WALL_SHADE);   /* the side wall is in shade */
        float p[5][3];                                   /* front wall near bottom / near top / far bottom / far top, gable peak */
        float sp[4][3];                                  /* near side wall: wall corner bottom / top, far bottom / top */
        vert_t v[5], sv[4];

        p[0][0] = s->face_x; p[0][1] = 0.0f;                 p[0][2] = s->z0;
        p[1][0] = s->face_x; p[1][1] = s->height;            p[1][2] = s->z0;
        p[2][0] = s->face_x; p[2][1] = 0.0f;                 p[2][2] = z1;
        p[3][0] = s->face_x; p[3][1] = s->height;            p[3][2] = z1;
        p[4][0] = s->face_x; p[4][1] = s->height + s->gable; p[4][2] = zm;
        sp[0][0] = s->face_x;        sp[0][1] = 0.0f;        sp[0][2] = s->z0;
        sp[1][0] = s->face_x;        sp[1][1] = s->height;   sp[1][2] = s->z0;
        sp[2][0] = s->face_x + away; sp[2][1] = 0.0f;        sp[2][2] = s->z0;
        sp[3][0] = s->face_x + away; sp[3][1] = s->height;   sp[3][2] = s->z0;
        for (k = 0; k < 5; k++) {
            vert_set(&v[k], p[k], front);
        }
        for (k = 0; k < 4; k++) {
            vert_set(&sv[k], sp[k], side);
        }
        (void)send_quad(&v[0], &v[1], &v[2], &v[3]);          /* front wall */
        (void)send_tri(&v[1], &v[4], &v[3]);                  /* gable */
        (void)send_quad(&sv[0], &sv[1], &sv[2], &sv[3]);      /* near side wall */
        for (j = 0; j < (unsigned)s->win_cols * s->win_rows; j++) {
            ml_quad_t q;
            int lit = 0;

            if (ml_house_window(s, j, &q, &lit) && lit) {
                const uint32_t c = pack_rgb(window_rgb[0] * WINDOW_BRIGHT, window_rgb[1] * WINDOW_BRIGHT, window_rgb[2] * WINDOW_BRIGHT);
                const uint32_t cs[4] = { c, c, c, c };

                (void)world_quad(&q, cs);
            }
        }
    }
}

/* Draws the girl: four dark quads facing the camera, the lower edge faintly warm while the match burns (she is lit from
 * below by it, and never from the front). `pos` is her feet, `level` the flame strength. */
static void girl_draw(const float pos[3], float level) {
    ml_quad_t q[ML_GIRL_QUADS];
    unsigned i, k;

    ml_girl_build(&cam, pos, q);
    for (i = 0; i < ML_GIRL_QUADS; i++) {
        uint32_t argb[4];

        for (k = 0; k < 4u; k++) {
            const float low = sh4der_sat_(1.0f - sh4der_div_(q[i].p[k][1] - pos[1], ML_GIRL_HEIGHT));   /* 1 at the feet, 0 at the crown */
            const float warm = level * GIRL_WARM_GAIN * low;

            argb[k] = pack_rgb(girl_rgb[0] + ML_WARM_R * warm, girl_rgb[1] + ML_WARM_G * warm, girl_rgb[2] + ML_WARM_B * warm);
        }
        (void)world_quad(&q[i], argb);
    }
}

/* Draws the small tree of the M6 vision: a trunk and three triangular tiers, camera-facing, dark green (it keeps its night
 * colour: M6 is a night shot), warmer low down while the match burns. */
static void tree_draw(float level) {
    const float base[3] = { ML_TREE_X, GROUND_Y + ml_snow_height(ML_TREE_X, ML_TREE_Z), ML_TREE_Z };   /* its foot on the snow */
    float side[3], toward[3];
    ml_quad_t trunk;
    uint32_t argb[4];
    unsigned t, k;

    ml_flat_basis(&cam, side, toward);
    ml_flat_quad(base, side, toward, 0.0f, 0.0f, TREE_TRUNK_HALF, TREE_TRUNK_TOP, TREE_TRUNK_HALF, 0.0f, &trunk);
    for (k = 0; k < 4u; k++) {
        const float low = sh4der_sat_(1.0f - sh4der_div_(trunk.p[k][1] - base[1], TREE_HEIGHT));
        const float warm = level * TREE_WARM_GAIN * low;

        argb[k] = pack_rgb(tree_rgb[0] * TRUNK_SHADE + ML_WARM_R * warm, tree_rgb[1] * TRUNK_SHADE + ML_WARM_G * warm, tree_rgb[2] * TRUNK_SHADE + ML_WARM_B * warm);
    }
    (void)world_quad(&trunk, argb);
    for (t = 0; t < 3u; t++) {
        const float hw = ml_tree_tier[t][0];
        const float y0 = ml_tree_tier[t][1];
        const float y1 = ml_tree_tier[t][2];
        float p[3][3];
        vert_t v[3];

        for (k = 0; k < 3u; k++) {
            const float across = k == 0u ? -hw : (k == 1u ? 0.0f : hw);   /* bottom left, top middle, bottom right */
            const float up = k == 1u ? y1 : y0;
            const float low = sh4der_sat_(1.0f - sh4der_div_(up, TREE_HEIGHT));
            const float warm = level * TREE_WARM_GAIN * low;
            int j;

            for (j = 0; j < 3; j++) {
                p[k][j] = sh4der_fma_(side[j], across, sh4der_fma_(toward[j], TIER_LAYER * (float)(t + 1u), base[j]));
            }
            p[k][1] += up;
            vert_set(&v[k], p[k], pack_rgb(tree_rgb[0] + ML_WARM_R * warm, tree_rgb[1] + ML_WARM_G * warm, tree_rgb[2] + ML_WARM_B * warm));
        }
        (void)send_tri(&v[0], &v[1], &v[2]);
    }
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Translucent list pieces. */

/* Draws one warm pool fan on the snow: sh4der_pool_disc builds the fan (centre alpha, rim alpha 0), every fan vertex is
 * then lifted to POOL_Y over the snow surface at its place (the snow is not flat), and each triangle
 * (0, k + 1, (k + 1) % n + 1) is sent with the guards. The pool header must already be sent. */
static void pool_fan_draw(const float centre[3], float radius, uint32_t rgb, float alpha) {
    const float u[3] = { radius, 0.0f, 0.0f };          /* the two axes in the ground plane: equal lengths make a circle */
    const float v[3] = { 0.0f, 0.0f, radius };
    sh4der_pool_vtx_t fan[SH4DER_POOL_MAX_N + 2];
    vert_t pv[SH4DER_POOL_MAX_N + 2];
    unsigned n, k;

    n = sh4der_pool_disc(centre, u, v, ML_POOL_SEGMENTS, rgb, alpha, fan);
    if (!n) {
        return;
    }
    for (k = 0; k < n; k++) {
        const float w[3] = { fan[k].x, GROUND_Y + ml_snow_height(fan[k].x, fan[k].z) + POOL_Y, fan[k].z };

        vert_set(&pv[k], w, fan[k].argb);
    }
    for (k = 0; k < ML_POOL_SEGMENTS; k++) {
        (void)send_tri(&pv[0], &pv[k + 1u], &pv[(k + 1u) % ML_POOL_SEGMENTS + 1u]);
    }
}

/* Fills the sprite pool of the frame (flame halo and core, one glow per house window) and draws it. Both go through the
 * library's sh4der_spark_draw (its own header, additive). The pool is rebuilt every frame with a 1 s life at age 0, so a quad
 * is always at full brightness and nothing needs the pool's update step. `flame` is the flame position, `level` its strength,
 * `film_clock` the film clock (flicker phase). Returns the quads sent. */
static unsigned sprites_draw(int shot, const float flame[3], float level, float film_clock) {
    unsigned h, j, sent;
    const ml_shot_t *s = &ml_shots[shot];

    sh4der_spark_reset(&glow_pool);
    /* One glow on the first lit window of each house, faded with distance like the fog fades the wall. */
    for (h = 0; h < ML_HOUSES; h++) {
        for (j = 0; j < (unsigned)ml_houses[h].win_cols * ml_houses[h].win_rows; j++) {
            ml_quad_t q;
            int lit = 0;

            if (ml_house_window(&ml_houses[h], j, &q, &lit) && lit) {
                float centre[3], view[3];
                const float toward = ml_houses[h].face_x < 0.0f ? GLOW_OFFSET : -GLOW_OFFSET;

                centre[0] = q.p[0][0] + toward;
                centre[1] = 0.5f * (q.p[0][1] + q.p[1][1]);
                centre[2] = 0.5f * (q.p[0][2] + q.p[2][2]);
                ml_to_view(&cam, centre, view);
                (void)sh4der_spark_billboard(&glow_pool, centre, GLOW_HALF, 0.0f, pack_bytes(glow_rgb, GLOW_BRIGHT * ml_fog_keep(view[2])),
                                             1.0f, ML_ATLAS_WINDOW, 0u);
                break;                                   /* one glow a house */
            }
        }
    }
    if (level > 0.0f) {
        float sn, cs, flick;

        sh4der_sincos_(film_clock * FLICKER_RATE, &sn, &cs);
        flick = sh4der_fma_(sn, FLICKER_DEPTH, 1.0f);
        (void)sh4der_spark_billboard(&glow_pool, flame, (HALO_BASE + HALO_PER_RADIUS * s->pool_radius) * flick, 0.0f,
                                     pack_bytes(s->pool_rgb, level * HALO_BRIGHT), 1.0f, ML_ATLAS_GLOW, SH4DER_SPARK_KEEP);
        (void)sh4der_spark_billboard(&glow_pool, flame, CORE_HALF * flick, 0.0f, pack_bytes(core_rgb, level), 1.0f, ML_ATLAS_GLOW,
                                     SH4DER_SPARK_KEEP);
    }
    sent = sh4der_spark_draw(&glow_pool, &spark_view, &spark_screen);
    frame_tris += 2u * sent;
    return sent;
}

/* ------------------------------------------------------------------------------------------------------------- */
/* The three hooks. Each is called once a frame inside the TRANSLUCENT list, after the pool and the sprites and before the
 * fade, with the shot index (0 .. 9), the 0..1 progress inside the shot and the film clock in seconds. The camera of the frame
 * is the file-level `cam` (ml_camera_t) and the sprite view and screen are `spark_view` and `spark_screen`; a hook may draw
 * with its own sh4der_spark pool and sh4der_spark_draw (which sends its own header), or with pvr_prim after sending a header
 * it compiled itself (check it with sh4der_poly_hdr_ok, guard every triangle with sh4der_tri_area_ok). */

/* Builds what the fog and the stars need once at start: the mist's noise texture and header, and the two rise set-ups.
 * Returns 1, or 0 when video memory could not be allocated. Cost: 12 KB of RAM (the noise and its 16-bit copy), 8 KB of
 * video memory. */
NOINLINE static int fog_and_stars_setup(void) {
    pvr_poly_cxt_t c;
    unsigned i;

    mist_tex = pvr_mem_malloc(sizeof(mist_px));
    if (!mist_tex || !sh4der_mist_noise(mist_noise, MIST_SIDE, MIST_SIDE, MIST_SEED)) {
        return 0;
    }
    for (i = 0; i < MIST_SIDE * MIST_SIDE; i++) {
        mist_px[i] = (uint16_t)(((uint32_t)(mist_noise[i] >> 4) << 12) | 0x0FFFu);   /* ARGB4444: the noise as alpha over white */
    }
    pvr_txr_load_ex(mist_px, mist_tex, MIST_SIDE, MIST_SIDE, PVR_TXRLOAD_16BPP);
    /* The sheets: translucent, the noise's alpha times the vertex alpha, ordinary blend, depth tested (the girl and the
     * houses hide the sheets behind them) but not written, no culling, no table fog on top of the fog itself. */
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_TWIDDLED, MIST_SIDE, MIST_SIDE, mist_tex, PVR_FILTER_BILINEAR);
    c.gen.alpha = PVR_ALPHA_ENABLE;
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.txr.env = PVR_TXRENV_MODULATEALPHA;
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&hdr_mist, &c);

    /* The tree's candles: they leave about one every third of a second, take 3.5 s to arrive, and come to rest in a band
     * of sky 9 to 13 m up, spread up to 6 m from the tree. Sizes are the quad's side in metres. */
    sh4der_rise_init(&candle_rise);
    candle_rise.stagger = 0.3f;
    candle_rise.jitter = 0.9f;
    candle_rise.rise_time = 3.5f;
    candle_rise.sky_y = 9.0f;
    candle_rise.sky_band = 4.0f;
    candle_rise.sky_spread = 6.0f;
    candle_rise.sway_amp = 0.35f;
    candle_rise.sway_freq = 0.4f;
    candle_rise.size0 = 0.30f;
    candle_rise.size1 = 0.36f;
    candle_rise.seed = 3u;
    /* The lights of the last pool: quicker, higher, wider, and all gone up within the 4 s shot. */
    bundle_rise = candle_rise;
    bundle_rise.stagger = 0.12f;
    bundle_rise.jitter = 0.4f;
    bundle_rise.rise_time = 2.4f;
    bundle_rise.sky_y = 10.0f;
    bundle_rise.sky_spread = 8.0f;
    bundle_rise.size0 = 0.34f;
    bundle_rise.seed = 11u;
    sh4der_spark_reset(&star_pool);
    sh4der_spark_reset(&extra_pool);
    return 1;
}

/* HOOK: the fog bank, from the library's mist effect: MIST_SLICES upright sheets across the street behind the girl, with a
 * soft top, each drifting its own way. Sent far sheet first (the translucent list is drawn in the order it is submitted).
 * At dawn (the last shot) the bank thins and pales. Every sheet is a six vertex strip: all six points must be in front of
 * the near plane and all four triangles must have screen area, or the sheet is left out whole. */
static void film_fog_draw(int shot, float progress, float time) {
    const float dawn = ml_dawn_blend(shot, progress);
    sh4der_mist_t m;
    uint32_t rgb;
    unsigned i;
    int sent_header = 0;

    sh4der_mist_init(&m);
    m.x = 0.0f;
    m.z = MIST_NEAR_Z;
    m.depth = MIST_DEPTH;
    m.half_width = MIST_HALF_WIDTH;
    m.base_y = GROUND_Y;
    m.top_y = MIST_TOP_Y;
    m.feather = MIST_FEATHER;
    m.slices = MIST_SLICES;
    m.seed = MIST_SEED;
    m.opacity = sh4der_lerp_(MIST_OPACITY_NIGHT, MIST_OPACITY_DAWN, dawn);
    rgb = pack_rgb(sh4der_lerp_((float)mist_night_rgb[0], (float)mist_dawn_rgb[0], dawn) * (1.0f / 255.0f),
                   sh4der_lerp_((float)mist_night_rgb[1], (float)mist_dawn_rgb[1], dawn) * (1.0f / 255.0f),
                   sh4der_lerp_((float)mist_night_rgb[2], (float)mist_dawn_rgb[2], dawn) * (1.0f / 255.0f)) & 0x00ffffffu;
    for (i = MIST_SLICES; i-- > 0u;) {
        sh4der_mist_vtx_t mv[SH4DER_MIST_SLICE_VERTS];
        vert_t v[SH4DER_MIST_SLICE_VERTS];
        unsigned k;
        int ok = 1;

        if (!sh4der_mist_slice(&m, i, time, cam.eye, mv)) {
            continue;
        }
        for (k = 0; k < SH4DER_MIST_SLICE_VERTS; k++) {
            const float w[3] = { mv[k].x, mv[k].y, mv[k].z };

            vert_set(&v[k], w, ((uint32_t)sh4der_fma_(sh4der_sat_(mv[k].alpha), 255.0f, 0.5f) << 24) | rgb);   /* hand: alpha byte */
            ok = ok && vert_ok(&v[k]);
        }
        for (k = 0; ok && k + 2u < SH4DER_MIST_SLICE_VERTS; k++) {
            ok = sh4der_tri_area_ok(v[k].v, v[k + 1u].v, v[k + 2u].v, ML_NEAR_Z, SH4DER_GUARD_MIN_AREA2);
        }
        if (!ok) {
            continue;
        }
        if (!sent_header) {
            if (!header_send(&hdr_mist)) {
                return;
            }
            sent_header = 1;
        }
        for (k = 0; k < SH4DER_MIST_SLICE_VERTS; k++) {
            emit_vertex_uv(&v[k], mv[k].u, mv[k].v, k + 1u == SH4DER_MIST_SLICE_VERTS ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX);
        }
        frame_tris += 4u;
    }
}

/* Adds one light of a rise effect to a sprite pool: a soft glow while it sits or rises, a four-point star once it rests.
 * `fade` 0..1 scales its brightness (the dawn takes the stars away). */
static void rise_sprite(sh4der_spark_pool_t *pool, const sh4der_rise_params_t *rp, uint32_t i, const float source[3], float t, float fade) {
    sh4der_rise_light_t l;
    float at[3];

    if (!sh4der_rise_light(rp, i, source, t, &l)) {
        return;
    }
    at[0] = l.x;
    at[1] = l.y;
    at[2] = l.z;
    (void)sh4der_spark_billboard(pool, at, 0.5f * l.size, 0.0f, pack_rgb(l.r * l.alpha * fade, l.g * l.alpha * fade, l.b * l.alpha * fade),
                                 1.0f, l.phase == SH4DER_RISE_STAR ? ML_ATLAS_STAR : ML_ATLAS_GLOW, 0u);
}

/* HOOK: the sky's lights, all additive sprites through the library's spark pools (rebuilt every frame, as the flame is).
 * - Every night shot: STAR_FIELD fixed stars above the fog, twinkling; the dawn fades them out.
 * - The tree shot: the twelve candles sit on the tree, then lift off one after another and come to rest as stars
 *   (the library's rise effect); in the shots after it they are stars at rest, until the dawn.
 * - The falling-star shot: a head and a short tail cross the sky in FALL_TIME_S.
 * - The whole-bundle shot: BUNDLE_LIGHTS lights lift off a ring round the flame.
 * At most 22 quads in one pool and 10 in the other: 64 triangles. */
static void film_stars_draw(int shot, float progress, float time) {
    const float night = 1.0f - ml_dawn_blend(shot, progress);          /* 1 at night, falling to 0 over the dawn shot */
    const float seconds = progress * (ml_shots[shot].end_s - ml_shots[shot].start_s);   /* seconds into the shot */
    unsigned i, sent;

    sh4der_spark_reset(&star_pool);
    sh4der_spark_reset(&extra_pool);
    if (night > 0.0f) {
        for (i = 0; i < STAR_FIELD; i++) {
            float sn, cs, b;

            sh4der_sincos_(sh4der_fma_(time, STAR_TWINKLE_RATE, (float)i * 1.7f), &sn, &cs);   /* each star its own phase */
            b = STAR_BRIGHT * night * (1.0f - STAR_TWINKLE * (0.5f + 0.5f * sn));
            (void)cs;
            (void)sh4der_spark_billboard(&star_pool, star_field[i], STAR_HALF, 0.0f, pack_rgb(0.86f * b, 0.90f * b, b), 1.0f, ML_ATLAS_STAR, 0u);
        }
    }
    if (shot >= SHOT_TREE && night > 0.0f) {
        /* in the tree shot the candles follow the shot's own clock; afterwards they are long since stars */
        const float t = shot == SHOT_TREE ? seconds - CANDLE_LIFT_S : CANDLE_FOREVER_S;

        for (i = 0; i < ML_TREE_CANDLES; i++) {
            const float source[3] = { ML_TREE_X + ml_tree_candles[i][0], GROUND_Y + ml_snow_height(ML_TREE_X, ML_TREE_Z) + ml_tree_candles[i][1],
                                        ML_TREE_Z - CANDLE_DEPTH };

            rise_sprite(&star_pool, &candle_rise, i, source, t, night);
        }
    }
    if (shot == SHOT_FALL && seconds >= FALL_START_S && seconds <= FALL_START_S + FALL_TIME_S) {
        const float head = sh4der_div_(seconds - FALL_START_S, FALL_TIME_S);   /* 0..1 along the path */

        for (i = 0; i <= FALL_TAIL; i++) {
            const float along = head - FALL_TAIL_GAP * (float)i;              /* the tail trails the head */
            const float bright = 1.0f - sh4der_div_((float)i, (float)(FALL_TAIL + 1u));
            float at[3];

            if (along < 0.0f) {
                continue;
            }
            at[0] = sh4der_lerp_(fall_from[0], fall_to[0], along);
            at[1] = sh4der_lerp_(fall_from[1], fall_to[1], along);
            at[2] = sh4der_lerp_(fall_from[2], fall_to[2], along);
            (void)sh4der_spark_billboard(&extra_pool, at, FALL_HALF * bright, 0.0f, pack_rgb(bright, bright, bright), 1.0f, ML_ATLAS_GLOW, 0u);
        }
    }
    if (shot == SHOT_BUNDLE) {
        for (i = 0; i < BUNDLE_LIGHTS; i++) {
            float sn, cs, source[3];

            sh4der_sincos_((float)i * (6.2831853f / (float)BUNDLE_LIGHTS), &sn, &cs);   /* evenly round the ring */
            source[0] = sh4der_fma_(cs, BUNDLE_RING, vision_flame[0]);
            source[1] = GROUND_Y + 0.3f;
            source[2] = sh4der_fma_(sn, BUNDLE_RING, vision_flame[2]);
            rise_sprite(&extra_pool, &bundle_rise, i, source, seconds - BUNDLE_START_S, vision_level > 0.0f ? 1.0f : 0.0f);
        }
    }
    sent = sh4der_spark_draw(&star_pool, &spark_view, &spark_screen);
    sent += sh4der_spark_draw(&extra_pool, &spark_view, &spark_screen);
    frame_tris += 2u * sent;
}

/* HOOK, PLACEHOLDER: the visions. For now a soft additive quad in the pool in the vision's colour (stove in M3, goose table in
 * M5, the tree's warm glow in M6, the grandmother in M8 and M9), as bright as the flame. It goes here: the flipbook of the stove,
 * the table, the tree and the grandmother (a textured quad whose texture changes), faded in over 0.2 s and out over 0.4 s with
 * the flame. `flame` and `level` are taken from the frame's own state (see vision_state below). */
static void film_vision_draw(int shot, float progress, float time) {
    float at[3];
    unsigned sent;

    (void)progress;
    (void)time;
    if (!(vision_level > 0.0f) || !ml_shot_lit(shot)) {
        return;
    }
    at[0] = vision_flame[0] + VISION_DX;
    at[1] = vision_flame[1] + VISION_DY;
    at[2] = vision_flame[2] + VISION_DZ;
    sh4der_spark_reset(&vision_pool);
    (void)sh4der_spark_billboard(&vision_pool, at, VISION_HALF, 0.0f, pack_bytes(ml_shots[shot].vision_rgb, VISION_BRIGHT * vision_level), 1.0f,
                                 ML_ATLAS_GLOW, 0u);
    sent = sh4der_spark_draw(&vision_pool, &spark_view, &spark_screen);
    frame_tris += 2u * sent;
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Readout. */

/* A readout number in tenths: the value x 10 (the caller multiplies) rounded to nearest and capped at HUD_TENTHS_MAX so it prints in at most
 * five characters ("999.9"). Negative or non-finite input gives 0. */
static unsigned hud_tenths(float v) {
    if (!sh4der_finite_(v) || v < 0.0f) {
        return 0u;
    }
    return v > (float)HUD_TENTHS_MAX ? HUD_TENTHS_MAX : (unsigned)(v + 0.5f);
}

/* Redraws the three text lines of the readout into hud_px and uploads them. Integer maths only (no floats are printed).
 * Call only after the PVR has finished reading the texture (the caller waits for the render). */
NOINLINE static void hud_build(int shot, float clock) {
    char line[HUD_LINE_BUF];                           /* roomy, so no number can be cut by snprintf; shortened to HUD_CHARS below */
    const unsigned t10 = (unsigned)sh4der_fma_(clock, 10.0f, 0.5f);                /* tenths of a second, rounded */
    const unsigned f10 = hud_tenths(frame_avg_us > 0.0f ? sh4der_div_(10000000.0f, frame_avg_us) : 0.0f);   /* frames a second, in tenths */
    const unsigned ms10 = hud_tenths(frame_avg_us * 0.01f);              /* frame time, tenths of a millisecond */
    const unsigned wms10 = hud_tenths(frame_worst_us * 0.01f);
    unsigned i;

    memset(hud_px, 0, sizeof(hud_px));
    minifont_set_color(255, 255, 255);
    snprintf(line, sizeof(line), "MATCHLIGHT %s  %u.%u s of 60  %s", ml_shots[shot].name, t10 / 10u, t10 % 10u, paused ? "PAUSED" : "");
    line[HUD_CHARS] = '\0';                           /* a line never runs past the image */
    minifont_draw_str(hud_px, HUD_W, line);
    snprintf(line, sizeof(line), "%u.%u fps  %u.%u ms  worst %u.%u ms", f10 / 10u, f10 % 10u, ms10 / 10u, ms10 % 10u, wms10 / 10u, wms10 % 10u);
    line[HUD_CHARS] = '\0';
    minifont_draw_str(hud_px + HUD_W * HUD_LINE_PX, HUD_W, line);
    snprintf(line, sizeof(line), "TA %lu/%u KB  tris %u  VRAM free %lu KB", ta_kb, TA_VERTEX_KB, frame_tris, (unsigned long)pvr_mem_available() / 1024ul);
    line[HUD_CHARS] = '\0';
    minifont_draw_str(hud_px + HUD_W * 2 * HUD_LINE_PX, HUD_W, line);
    for (i = 0; i < HUD_W * HUD_H; i++) {
        hud_px[i] = hud_px[i] ? 0xffffu : 0u;         /* a drawn pixel: opaque white; the rest: clear */
    }
    pvr_txr_load(hud_px, hud_tex, sizeof(hud_px));
}

/* Redraws the debug caption image for a shot ("M3 first match: stove") and uploads it. Call only after the PVR has finished
 * reading the texture (the caller waits for the render). */
NOINLINE static void caption_build(int shot) {
    char line[HUD_LINE_BUF];
    unsigned i;

    memset(cap_px, 0, sizeof(cap_px));
    minifont_set_color(255, 255, 255);
    snprintf(line, sizeof(line), "%s %s", ml_shots[shot].name, ml_shots[shot].caption);
    line[ML_CAPTION_CHARS] = '\0';                    /* a line never runs past the image */
    minifont_draw_str(cap_px, CAP_W, line);
    for (i = 0; i < CAP_W * CAP_H; i++) {
        cap_px[i] = cap_px[i] ? 0xffffu : 0u;         /* a drawn pixel: opaque white; the rest: clear */
    }
    pvr_txr_load(cap_px, cap_tex, sizeof(cap_px));
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Pad and clock. */

/* Moves the film clock to the start of a shot (wrapping the shot number round the ten shots). */
static void jump_to_shot(int shot) {
    const int s = (shot % ML_SHOTS + ML_SHOTS) % ML_SHOTS;

    clock_s = ml_shots[s].start_s;
}

/* Reads the first controller: A pauses / resumes, D-pad right / left jump to the next / previous shot, Y shows / hides the
 * readout, X shows / hides the debug caption. Edge triggered (a held button acts once). */
NOINLINE static void pad_read(void) {
    static uint32_t last;
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
    uint32_t down;

    if (!st) {
        return;
    }
    down = st->buttons & ~last;
    last = st->buttons;
    if (down & CONT_A) {
        paused ^= 1;
    }
    if (down & CONT_DPAD_RIGHT) {
        jump_to_shot(cur_shot + 1);
    }
    if (down & CONT_DPAD_LEFT) {
        jump_to_shot(cur_shot - 1);
    }
    if (down & CONT_Y) {
        hud_on ^= 1;
    }
    if (down & CONT_X) {
        cap_on ^= 1;
    }
}

/* ------------------------------------------------------------------------------------------------------------- */
/* One frame. */

/* Sets the camera of the frame and everything derived from it: the sprite views (the world one, and the one with y and z
 * swapped for the snow) and the screen record. */
NOINLINE static void camera_set(const ml_where_t *w) {
    int k;

    if (!ml_camera_at(w->shot, w->progress, &cam)) {
        return;                                        /* cannot happen for a valid shot; keep last frame's camera */
    }
    for (k = 0; k < 3; k++) {
        spark_view.right[k] = cam.right[k];
        spark_view.up[k] = cam.up[k];
        spark_view.fwd[k] = cam.fwd[k];
        spark_view.eye[k] = cam.eye[k];
    }
    spark_view.near_z = ML_NEAR_Z;
    spark_view.focal_px = cam.focal_px;
    spark_view.min_px = SH4DER_SPARK_MIN_PX;
    spark_view.inset = SH4DER_SPARK_INSET;
    spark_view.cols = 2;
    spark_view.rows = 2;
    snow_view = spark_view;
    snow_view.right[1] = cam.right[2]; snow_view.right[2] = cam.right[1];       /* swap y and z in every axis and the eye */
    snow_view.up[1] = cam.up[2]; snow_view.up[2] = cam.up[1];
    snow_view.fwd[1] = cam.fwd[2]; snow_view.fwd[2] = cam.fwd[1];
    snow_view.eye[1] = cam.eye[2]; snow_view.eye[2] = cam.eye[1];
    spark_screen = sh4der_spark_screen_make(0.5f * (float)ML_SCREEN_W, 0.5f * (float)ML_SCREEN_H, cam.focal_px);
}

/* Draws the whole picture of one frame: opaque set, snow, then the translucent list. The scene is already begun. */
NOINLINE static void frame_draw(const ml_where_t *w) {
    float amb[3], flame[3], girl[3], sky[3], overlay_a;
    uint8_t overlay_rgb[3];
    const ml_shot_t *s = &ml_shots[w->shot];
    const float level = ml_match_level(w->shot, w->seconds);
    const float dawn = ml_dawn_blend(w->shot, w->progress);
    unsigned n_snow;

    frame_tris = 0;
    ml_sky(w->shot, w->progress, sky);
    ml_ambient(w->shot, w->progress, amb);
    ml_girl_pos(w->shot, w->progress, girl);
    ml_flame_pos(girl, flame);
    vision_level = level;
    vision_flame[0] = flame[0]; vision_flame[1] = flame[1]; vision_flame[2] = flame[2];
    pvr_set_bg_color(sky[0], sky[1], sky[2]);

    /* Opaque list: the set. */
    pvr_list_begin(PVR_LIST_OP_POLY);
    if (header_send(&hdr_set)) {
        ground_draw(amb, flame, level, s->pool_radius);
        trail_draw(amb, flame, level, s->pool_radius, dawn);
        houses_draw(amb);
        girl_draw(girl, level);
        if (s->has_tree) {
            tree_draw(level);
        }
    }
    pvr_list_finish();

    /* Punch-through list: falling snow, from the library. The snow's own space has y and z swapped. */
    pvr_list_begin(PVR_LIST_PT_POLY);
    n_snow = sh4der_motes_collect(&snow_params, snow_emitters, SNOW_EMITTERS, snow_time, &snow_view, &spark_screen, snow_draws);
    frame_tris += 2u * sh4der_motes_draw_masked(snow_draws, n_snow, 1, &snow_view, &spark_screen);
    pvr_list_finish();

    /* Translucent list, submitted in this order (autosort is off): pool, sprites, hooks, fade, caption, readout. */
    pvr_list_begin(PVR_LIST_TR_POLY);
    if (level > 0.0f && header_send(&hdr_pool)) {
        const float centre[3] = { flame[0], GROUND_Y + POOL_Y, flame[2] };
        const float radius = s->pool_radius;

        pool_fan_draw(centre, radius * POOL_OUTER_SCALE, pack_bytes(s->pool_rgb, 1.0f) & 0x00ffffffu, POOL_ALPHA_OUTER * level);
        pool_fan_draw(centre, radius, pack_bytes(s->pool_rgb, 1.0f) & 0x00ffffffu, POOL_ALPHA_INNER * level);
    }
    (void)sprites_draw(w->shot, flame, level, w->clock);
    film_fog_draw(w->shot, w->progress, w->clock);
    film_stars_draw(w->shot, w->progress, w->clock);
    film_vision_draw(w->shot, w->progress, w->clock);
    ml_overlay(w->shot, w->seconds, &overlay_a, overlay_rgb);
    if (overlay_a > 0.0f && header_send(&hdr_fade)) {
        screen_rect(0.0f, 0.0f, (float)ML_SCREEN_W, (float)ML_SCREEN_H, Z_OVERLAY,
                    ((uint32_t)sh4der_fma_(sh4der_sat_(overlay_a), 255.0f, 0.5f) << 24) | ((uint32_t)overlay_rgb[0] << 16) |
                    ((uint32_t)overlay_rgb[1] << 8) | (uint32_t)overlay_rgb[2], 0);
    }
    if (cap_on && cap_shot == w->shot && header_send(&hdr_cap)) {
        const float y = hud_on ? CAP_Y_OVER_HUD : CAP_Y;

        screen_rect(CAP_X, y, CAP_X + (float)CAP_W, y + (float)CAP_H, Z_HUD, 0xffe0e0e0u, 1);
    }
    if (hud_on && header_send(&hdr_hud)) {
        screen_rect(HUD_X, HUD_Y, HUD_X + (float)HUD_W, HUD_Y + (float)HUD_H, Z_HUD, 0xffb0b0b0u, 1);
    }
    ta_meter();                                          /* the translucent list is still open: the TA is not yet re-armed */
    pvr_list_finish();
}

int main(void) {
    pvr_init_params_t init = pvr_default_params;
    uint64_t previous, window_start, window_total = 0, window_worst = 0;
    unsigned window_frames = 0, stuck = 0;
    unsigned long total_frames = 0;
    int stuck_shown = 0, ta_reported = 0, rendered = 0;
    float hud_age = HUD_REFRESH_S;

    t_boot = timer_us_gettime64();
    /* Boot colours (displayed frame, no PVR render): 1 red main, 2 orange PVR init, 3 yellow fog table, 4 green textures,
     * 5 cyan headers and set-up, 6 blue first scene update, 7 magenta first frame submit. White later = the PVR never finished a frame. */
    boot_stage(1, 160, 0, 0, "main");
    init.opb_sizes[PVR_LIST_OP_POLY] = BIN_OPAQUE;
    init.opb_sizes[PVR_LIST_OP_MOD] = PVR_BINSIZE_0;     /* modifier volumes off */
    init.opb_sizes[PVR_LIST_TR_POLY] = BIN_TRANSLUCENT;
    init.opb_sizes[PVR_LIST_TR_MOD] = PVR_BINSIZE_0;
    init.opb_sizes[PVR_LIST_PT_POLY] = BIN_PUNCH;
    init.vertex_buf_size = (int)(TA_VERTEX_KB * 1024u);
    init.opb_overflow_count = OPB_OVERFLOW;
    init.autosort_disabled = 1;                          /* the translucent list is submitted in a fixed order */
    if (pvr_init(&init) < 0) {
        printf("matchlight: pvr_init FAIL\n");
        return 1;
    }
    boot_stage(2, 224, 96, 0, "pvr_init");
    pvr_set_bg_color(ML_SKY_NIGHT_R, ML_SKY_NIGHT_G, ML_SKY_NIGHT_B);
    pvr_fog_table_color(1.0f, fog_rgb[0], fog_rgb[1], fog_rgb[2]);   /* fog table 1, set once: night fog */
    pvr_fog_table_linear(ML_FOG_START_Z, ML_FOG_END_Z);
    boot_stage(3, 200, 200, 0, "fog table");
    if (!textures_build()) {
        printf("matchlight: texture allocation FAIL\n");
        return 1;
    }
    boot_stage(4, 0, 160, 0, "textures built");
    headers_build();
    if (!sh4der_spark_init(atlas_tex, ML_ATLAS_SIDE, ML_ATLAS_SIDE, PVR_TXRFMT_RGB565 | PVR_TXRFMT_TWIDDLED) ||
        !sh4der_motes_init_masked(mask_tex, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_TWIDDLED)) {
        printf("matchlight: effect header FAIL\n");
        return 1;
    }
    snow_setup();
    if (!fog_and_stars_setup()) {
        printf("matchlight: mist texture FAIL\n");
        return 1;
    }
    sh4der_spark_reset(&glow_pool);
    sh4der_spark_reset(&vision_pool);
    snow_reset();
    ground_setup();
    boot_stage(5, 0, 160, 160, "headers and set-up");
    printf("matchlight: static RAM about %lu KB, video memory textures %lu KB, VRAM free %lu KB, TA vertex buffer %u KB x2, "
           "worst shot %u triangles (counted)\n",
           (unsigned long)(sizeof(deform_cells) + sizeof(ground) + sizeof(ground_h) + sizeof(ground_shade) + sizeof(trail) + sizeof(trail_h) + sizeof(trail_depth) +
                           sizeof(trail_shade) + sizeof(atlas_px) + sizeof(hud_px) + sizeof(cap_px) + sizeof(snow_draws) + sizeof(mist_noise) + sizeof(mist_px)) / 1024ul,
           (unsigned long)(sizeof(atlas_px) + sizeof(mask_px) + sizeof(hud_px) + sizeof(cap_px) + sizeof(mist_px)) / 1024ul, (unsigned long)pvr_mem_available() / 1024ul,
           TA_VERTEX_KB, ml_tris_counted(5));
    boot_stage(6, 0, 0, 200, "first update");
    boot_stage(7, 200, 0, 200, "first submit");

    previous = window_start = timer_us_gettime64();
    for (;;) {
        const uint64_t now = timer_us_gettime64();
        const uint64_t elapsed = now - previous;
        float dt;
        ml_where_t w;
        int ready;

        previous = now;
        dt = shz_fminf((float)elapsed * 1.0e-6f, MAX_DT_S);   /* hand: microseconds to seconds, one product */
        pad_read();
        if (!paused) {
            clock_s = ml_wrap_clock(clock_s + dt);
            snow_time += dt;
        }
        ml_locate(clock_s, &w);
        cur_shot = w.shot;
        camera_set(&w);
        footprints_update(w.shot, w.progress);

        ready = pvr_wait_ready();
        /* KallistiOS times each wait out at 100 ms. Twenty in a row means the PVR never finished a frame: paint white once, log once. */
        if (ready < 0) {
            if (++stuck >= STUCK_WAITS && !stuck_shown) {
                stuck_shown = 1;
                vid_clear(255, 255, 255);
                printf("matchlight: PVR stuck after %lu frames: TA %lu KB, OPB overflow %lu KB\n", total_frames, ta_kb, opb_kb);
            }
        } else {
            if (!rendered && total_frames >= 1) {         /* once: the first frame went through the PVR */
                rendered = 1;
                printf("matchlight: frame 1 rendered by %lu ms\n", (unsigned long)((timer_us_gettime64() - t_boot) / 1000u));
            }
            stuck = 0;
        }
        hud_age += dt;
        if (hud_on && hud_age >= HUD_REFRESH_S) {
            hud_age = 0.0f;
            (void)pvr_wait_render_done();                 /* the readout texture is rewritten: the PVR must be done reading it */
            hud_build(w.shot, w.clock);
        }
        if (cap_on && cap_shot != w.shot) {
            (void)pvr_wait_render_done();                 /* the caption texture is rewritten: the PVR must be done reading it */
            caption_build(w.shot);
            cap_shot = w.shot;
        }
        pvr_scene_begin();
        frame_draw(&w);
        pvr_scene_finish();
        total_frames++;
        /* Two serial lines about the TA: after the first frame and after about 10 s. */
        if ((total_frames == 1 && ta_reported == 0) || (total_frames >= 300 && ta_reported == 1)) {
            ta_reported++;
            printf("matchlight: frame %lu at %lu ms: TA vertex %lu of %u KB, OPB overflow %lu KB, waits timed out %u, triangles sent %u\n", total_frames,
                   (unsigned long)((timer_us_gettime64() - t_boot) / 1000u), ta_kb, TA_VERTEX_KB, opb_kb, stuck, frame_tris);
        }
        if (window_frames > 0) {
            window_total += elapsed;
            if (elapsed > window_worst) {
                window_worst = elapsed;
            }
        }
        window_frames++;
        if (now - window_start >= STATS_WINDOW_US && window_frames > 1) {
            frame_avg_us = (float)window_total / (float)(window_frames - 1u);
            frame_worst_us = (float)window_worst;
            window_start = now;
            window_total = 0;
            window_worst = 0;
            window_frames = 0;
        }
    }
}
