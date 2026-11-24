/* SH4DER-PVR demo: "The Little Match Girl" (a 120 second short film for a narrator), the Dreamcast program.
 * Copyright (c) 2026 Jilted Valkyrie. See ../../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
 *
 * It plays a title card, the twelve shots in order, then the end credits, and loops. Every shape of the film is built
 * from numbers (matchlight_scene.h); from the disc it reads only sound, the subtitle and credit type with their timing,
 * and the title card. A file that is missing is not played or shown: the film still runs.
 *
 * What it draws, per frame:
 *   opaque list      the ground grid (shaped snow: banks, drifts and piles from ml_snow_height, vertex coloured, lit by an
 *                    ambient colour, a moon shade by slope and a warm term that falls off with distance from the flame),
 *                    the trail patch (a fine strip along the girl's walk whose vertices follow the snow dent grid, so
 *                    her footprints are real pits), house fronts, pale windows,
 *                    the girl (four dark quads), the small tree (shot M6). Every polygon uses fog table 1.
 *   punch-through    falling snow (the library's sh4der_motes).
 *   translucent list the warm pool (sh4der_pool fans, additive), the flame glow and window glows
 *                    (sh4der_spark quads), the fog bank, the sky's lights and the visions (film_fog_draw,
 *                    film_stars_draw, film_vision_draw), the full-screen fade / flash quad, the debug caption and the optional readout.
 *
 * Subtitles: the narrator's sentences on the lower letterbox bar, typed letter by letter in step with the voice. The
 * type (font.bin: a glyph picture and table) and the timing (subs.bin: per sentence, each letter's place and moment)
 * are read from the disc at start; tools/narration.py makes both. Without the two files there are no subtitles.
 *
 * Pad: A pause / resume the clock; D-pad right / left next / previous shot; START show / hide the readout; B old-film look; Y lens pass; X show / hide the
 * debug caption (the shot's name and a few words on what it shows, on the upper letterbox bar; hidden at start).
 *
 * Sound: the effects of matchlight_scene.h (ml_sound_names) are loaded from the disc folder sfx/ with snd_sfx_load and
 * played with snd_sfx_play / snd_sfx_play_ex. The narrator's voice (voice.wav) and the music (music.wav) are 16-bit mono
 * PCM files streamed from the disc with snd_stream, polled once a frame and moved to the film clock's place whenever the
 * clock jumps (pause, shot jump, the film looping). A file that is missing is silent. tools/audio.py makes the files.
 *
 * KallistiOS paths used: snd_stream_init_ex (which starts the AICA driver), snd_sfx_load, snd_sfx_play, snd_sfx_play_ex,
 * snd_sfx_stop, snd_stream_alloc / start / stop / poll / volume, fs_open / fs_read / fs_seek (sound); pvr_init, pvr_fog_table_color + pvr_fog_table_linear (fog table 1), pvr_set_bg_color,
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
#include <stdlib.h>
#include <malloc.h>
#include <kos.h>
#include <dc/minifont.h>
#include <dc/sound/stream.h>
#include <dc/sound/sfxmgr.h>
#include <dc/sound/sound.h>
#include <dc/sound/aica_comm.h>
#include "matchlight_scene.h"
#include "sh4der_guard.h"
#include "sh4der_pool.h"
#include "sh4der_spark.h"
#include "sh4der_motes.h"
#include "sh4der_deform.h"
#include "sh4der_mist.h"
#include "sh4der_rise.h"
#include "sh4der_lens.h"
#include "sh4der_film.h"
#include "sh4der_ca.h"
#include "sh4der_taa.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

/* ------------------------------------------------------------------------------------------------------------- */
/* Tuning numbers of the program (the scene's own numbers live in matchlight_scene.h). */

#define STAGE_HOLD_MS     300           /* each boot colour is held this long so a camera can see it, milliseconds */
#define PT_ALPHA_REF      128           /* punch-through alpha reference, 0..255: a texel under it is not drawn. KallistiOS
                                         * never writes this register; unset, hardware may draw the snow flakes as squares */
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
#define CAP_Y             24.0f         /* caption top edge on screen, pixels (on the upper letterbox bar; the lower one has the subtitles) */
#define SUB_FONT_W        256           /* subtitle glyph picture width, texels (font.bin must say the same) */
#define SUB_FONT_H        128           /* and its height */
#define SUB_FONT_BYTES    (SUB_FONT_W * SUB_FONT_H * 2)   /* the picture as ARGB4444: 64 KB of video memory */
#define SUB_GLYPHS_MAX    96u           /* most glyphs font.bin may list (printable ASCII is 94) */
#define SUB_SENTENCES_MAX 48u           /* most sentences subs.bin may hold (the film has 30) */
#define SUBS_MAX_BYTES    12288u        /* largest subs.bin the program takes, bytes (the film's is under 7 KB) */
#define SUB_HEAD_BYTES    8u            /* subs.bin: "SUB1", u16 sentences, u16 0 */
#define SUB_SENT_BYTES    12u           /* per sentence: u32 start ms, u32 end ms, u16 glyphs, u16 0 */
#define SUB_GLYPH_BYTES   8u            /* per glyph: u16 x, u16 y, u16 delay ms, u8 glyph number, u8 0 */
#define SUB_ARGB          0xffe8e8e8u   /* subtitle colour: a soft white */
#define FILM_VIG_SPOKES   24u           /* spokes of the vignette mesh round the picture */
#define FILM_VIG_RINGS    6u            /* rings across its falloff */
#define FILM_VIG_VERTS    ((FILM_VIG_RINGS + 1u) * 2u * (FILM_VIG_SPOKES + 1u))   /* 350 vertices: 7 strips of 50 */
#define FILM_VIG_STRENGTH 0.30f         /* the corners fall to 1 - this of the picture's brightness (the film is a night film: mild) */
#define FILM_KEEP_HALF_X  40.0f         /* no scratch within this many pixels of the picture's middle column */
#define FILM_KEEP_Y0      200.0f        /* in the low footprint shot the picture rows FILM_KEEP_Y0 .. FILM_KEEP_Y1 (the trail) */
#define FILM_KEEP_Y1      340.0f        /* take at most FILM_KEEP_SPECKS dust specks a frame */
#define FILM_KEEP_SPECKS  2u
#define FILM_TINT_R       1.00f         /* the warm film base as multipliers a channel */
#define FILM_TINT_G       0.96f
#define FILM_TINT_B       0.88f
#define FILM_TINT         0.15f         /* how much of that tint is mixed in, 0 .. 1 */
#define FILM_TITLE_SCRATCHES 2u         /* scratch slots used over the title card */
#define FILM_TITLE_SPECKS 6u            /* most specks a frame over the title card */
#define FILM_SEED         1845u         /* any number: picks where and when the wear falls */
#define LENS_CA_STRENGTH  0.5f          /* chromatic aberration: red and blue 1.5 pixels apart in the screen's corner (the library's 1.0 is 3 pixels) */
#define LENS_TAA_WEIGHT   0.45f         /* anti-aliasing: the share of the frame before in each frame; above 0.5 the falling snow smears */
#define LENS_VRAM_RESERVE_KB 448u       /* video memory that must stay free after the frame textures are taken, KB */
#define Z_LENS            1.0f          /* depth key of the frame texture on the screen (its depth test always passes) */
#define TITLE_TILES       2u            /* texture tiles of the title card: a 640-texel width is not a size the PVR takes */
#define TITLE_TILE_A_W    512u          /* width of the first tile, texels: the card's columns 0 .. 511 */
#define TITLE_TILE_B_W    128u          /* width of the second: columns 512 .. 639 */
#define TITLE_TILE_H      512u          /* height of both, texels (the card uses rows 0 .. 479): 640 KB of video memory in all */
#define TITLE_BANDS_MAX   64u           /* most alpha bands title.bin may list (tools/title.py keeps to it) */
#define TITLE_HEAD_BYTES  12u           /* title.bin: "TTL1", u16 width, u16 height, u16 bands, u16 0 */
#define TITLE_BAND_BYTES  8u            /* per band: u16 first row, u16 row after the last, u8 alpha, u8 0, u16 0 */
#define CREDIT_FONT_W     256           /* the credits' glyph picture width, texels (credits.bin must say the same) */
#define CREDIT_FONT_H     256           /* and its height */
#define CREDIT_FONT_BYTES (CREDIT_FONT_W * CREDIT_FONT_H * 2)   /* the picture as ARGB4444: 128 KB of video memory */
#define CREDIT_GLYPHS_MAX 255u          /* most glyphs credits.bin may list (a letter names its glyph in one byte) */
#define CREDIT_LETTERS_MAX 128u         /* most letters of one card (tools/credits.py keeps to it) */
#define CREDIT_HEAD_BYTES 16u           /* credits.bin: "CRD1", u16 cards, u16 glyphs, u16 width, u16 height, u32 bytes of card records */
#define CREDIT_CARD_BYTES 4u            /* per card: u16 letters, u16 screen row of its accent rule */
#define CREDIT_LETTER_BYTES 6u          /* per letter: u16 x, u16 y, u8 glyph number, u8 ink (255 = full) */
#define CREDIT_DATA_BYTES 8192u         /* room for the card records (the eight cards take about 3 KB) */
#define POOL_Y            0.04f         /* the pool fans lie this high over the snow surface, metres */
#define POOL_ALPHA_INNER  0.85f         /* centre alpha of the inner pool fan at full flame */
#define POOL_OUTER_SCALE  1.8f          /* the outer, fainter fan is this many times wider than the inner one */
#define POOL_ALPHA_OUTER  0.22f         /* centre alpha of the outer pool fan at full flame */
#define FLAME_GLOW_BASE   0.25f         /* flame glow half size at zero pool radius, metres */
#define FLAME_GLOW_PER_RADIUS 0.20f         /* flame glow half size added per metre of pool radius */
#define FLAME_GLOW_BRIGHT 0.9f          /* flame glow brightness share of the pool colour */
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
#define SPIRIT_ALPHA_TOP   0.60f        /* alpha of a spirit's upper edge at full strength (added light) */
#define SPIRIT_ALPHA_HEM   0.15f        /* and of its lower edge: it thins out toward the hem */
#define GIRL_WARM_GAIN    0.35f         /* how much of the pool colour lights the girl's lower edge */
#define TREE_WARM_GAIN    0.30f         /* the same for the tree */
#define TREE_TRUNK_HALF   0.15f         /* trunk half width, metres */
#define TREE_TRUNK_TOP    0.90f         /* trunk height, metres */
#define SIDE_WALL_SHADE   0.7f         /* a house's near side wall is this share as bright as its front (it is in shade) */
#define WINDOW_BRIGHT     0.9f          /* a lit window's share of the pale window colour */
#define TRUNK_SHADE       0.6f          /* the trunk is this share of the tree colour (darker than the branches) */
#define TIER_LAYER        0.02f         /* each tree tier sits this much nearer the camera than the one below, metres, so tiers do not share a plane */
#define TREE_HEIGHT       ML_TREE_HEIGHT        /* tree height, metres (for the warm gradient) */
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
#define CANDLE_POOLS      ((ML_TREE_CANDLES + (unsigned)SH4DER_SPARK_MAX - 1u) / (unsigned)SH4DER_SPARK_MAX)   /* pools the candles need */
#define ORNAMENT_GLOW     1.6f          /* an ornament's glow quad is this many times its radius in half size (the glow is soft-edged) */
#define ORNAMENT_DIM      0.45f         /* an ornament's brightness with no match burning; 1 at full flame */
#define VISION_PART_ALPHA 0.85f         /* how strongly a vision's cut-out parts show at full flame (added light) */
#define GOOSE_FRONT       0.05f         /* the goose is this far nearer the lens than the table, metres, so it never shares its plane */
#define CANDLE_DEPTH      0.06f         /* the candles sit this far in front of the tree's plane, metres */
#define SHOT_TREE         ML_SHOT_M6    /* index of the shot with the tree */
#define SHOT_FALL         ML_SHOT_M7    /* index of the falling-star shot */
#define SHOT_BUNDLE       ML_SHOT_M9    /* index of the whole-bundle shot */
#define FALL_START_S      0.6f          /* seconds into its shot at which the falling star appears: as the shot has faded up */
#define FALL_TIME_S       0.6f          /* seconds it takes to cross (the film brief) */
#define FALL_HALF         0.45f         /* the head's half size, metres */
#define BUNDLE_LIGHTS     10u           /* lights that rise from the last pool */
#define BUNDLE_RING       1.1f          /* they start on a ring of this radius round the flame, metres */
#define SFX_PATH_BUF      32            /* buffer for a sound's disc path, characters ("/cd/sfx/" + 8 + ".wav" is 20) */
#define STREAMS           3u            /* disc streams: the narrator's voice, the music, the long sound effects */
#define STREAM_VOICE      0u            /* index of the voice stream */
#define STREAM_MUSIC      1u            /* index of the music stream */
#define STREAM_FX         2u            /* index of the long sound effects' stream (fx.wav: the sounds too long for an AICA
                                         * sound effect, laid at their film times by tools/audio.py) */
#define FX_VOLUME         255           /* stream volume of that stream: its levels are set in the file */
#define STREAM_BYTES      (32 * 1024)   /* AICA buffer of one stream, bytes: 0.74 s of 22,050 Hz 16-bit mono */
#define WAV_HEADER_BYTES  44u           /* the plain WAV header tools/audio.py writes: the samples start right after it */
#define VOICE_VOLUME      255           /* stream volume of the voice, 0..255 */
#define MUSIC_VOLUME      255           /* stream volume of the music: the same as the voice; the file itself is laid under the voice */
#define SFX_NO_CHANNEL    (-1)          /* no AICA channel: the loop is not playing */
#define SNOW_DEFORM_CELLS (SH4DER_DEFORM_GRID_W * SH4DER_DEFORM_GRID_H)   /* snow dent grid: one byte a cell, 25,600 bytes (20 x 20 m at 0.125 m) */

/* The fog colour of fog table 1 (night fog, 0..1): a cold blue-grey bank lighter than the sky so the street dissolves into it. */
static const float fog_rgb[3] = { ML_FOG_R, ML_FOG_G, ML_FOG_B };
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
static const uint8_t glow_rgb[3] = { 200u, 140u, 80u };
/* Colour of the whole-frame light at M9's peak: the strike flash's gold-white. */
static const uint8_t lift_peak_rgb[3] = { ML_FLASH_R, ML_FLASH_G, ML_FLASH_B };
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
static sh4der_spark_pool_t candle_pool[CANDLE_POOLS];     /* 24 each: the tree's 48 candles, on the tree and then as stars */
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
static unsigned steps_done;                               /* footprints begun so far (see footprints_update) */
static unsigned steps_full;                               /* of those, the first this many are at full depth and need no more pressing */
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
static unsigned long ta_kb;                               /* most TA vertex buffer kilobytes a scene has used (0 on an emulator) */
static unsigned frame_tris;                               /* triangles this program counted itself sending this frame */
static float clock_s;                                     /* the clock, seconds, 0 .. ML_LOOP_SECONDS (wrapped): the film, then the credits */
static float snow_time;                                   /* snow time, seconds: runs with the clock, never wraps back */
static int paused, hud_on, cur_shot;                      /* pause flag, readout flag, shot of the last frame */
static int cap_on;                                        /* caption flag: hidden at start (a debug aid; X shows it) */
static uint8_t sub_glyph[SUB_GLYPHS_MAX][4];              /* font.bin's table: where each glyph lies in the picture (x, y, w, h) */
static unsigned sub_glyphs;                               /* glyphs in that table */
static uint8_t subs_data[SUBS_MAX_BYTES];                 /* subs.bin as read from the disc, 12 KB */
static uint16_t sub_at[SUB_SENTENCES_MAX];                /* byte offset of each sentence's record in subs_data */
static unsigned sub_sentences;                            /* sentences in subs_data; 0: no subtitles */
/* One alpha band of the title card: the rows y0 .. y1 - 1 are drawn with this alpha (0..255). */
typedef struct title_band {
    uint16_t y0, y1;
    uint8_t alpha;
} title_band_t;
static sh4der_film_t film;                                /* the wear's settings (92 B) */
static sh4der_lens_vtx_t film_vig[FILM_VIG_VERTS];        /* the vignette mesh for the picture band, built once (4.2 KB) */
static unsigned film_vig_verts;                           /* vertices in it */
static pvr_ptr_t film_grain_tex;                          /* the grain tile in video memory (8 KB) */
static pvr_poly_hdr_t hdr_film_mul, hdr_film_grain;       /* multiply, untextured; multiply by the grain tile */
static int film_ready, film_on = 1;                       /* the look can be drawn; the look is switched on (B button) */
static float weave_x, weave_y;                            /* this frame's gate weave: the 3D picture's shift, pixels */
static sh4der_ca_gpu_t lens_gpu;                          /* the lens pass's frame textures (the library holds them) */
static sh4der_taa_t lens_taa;                             /* the anti-aliasing's jitter table and frame count */
static unsigned lens_n;                                   /* frame textures held: 2 aberration and anti-aliasing, 1 aberration, 0 no pass */
static int lens_on = 1;                                   /* the pass is switched on (Y button) */
static int lens_cur;                                      /* the frame texture this frame is drawn into, 0 or 1 */
static int lens_have;                                     /* 1: the other texture holds the frame before */
static int lens_shot = -1;                                /* the shot of the frame before; -1: none */
static float lens_weight;                                 /* this frame's share of the frame before, 0 .. LENS_TAA_WEIGHT */
static title_band_t title_band[TITLE_BANDS_MAX];         /* title.bin's bands, top to bottom */
static unsigned title_bands;                              /* bands ready to draw; 0: no title card */
static pvr_ptr_t title_tex[TITLE_TILES];                  /* the card's two tiles in video memory */
static pvr_poly_hdr_t hdr_title[TITLE_TILES];             /* one header a tile: textured, blended by the vertex alpha */
static uint8_t credit_glyph[CREDIT_GLYPHS_MAX][4];        /* credits.bin's table: where each glyph lies in the picture (x, y, w, h) */
static uint8_t credit_data[CREDIT_DATA_BYTES];            /* its card records as read from the disc */
static uint16_t credit_at[ML_CREDIT_CARDS];               /* byte offset of each card's record in credit_data */
static unsigned credit_glyphs, credit_cards;              /* glyphs in the table; cards ready to draw (0: no credits) */
static pvr_ptr_t credit_tex;                              /* the credits' glyph picture in video memory */
static pvr_poly_hdr_t hdr_credit;                         /* the credit letters: textured, alpha blended, in front of everything */
static pvr_ptr_t sub_tex;                                 /* the glyph picture in video memory */
static pvr_poly_hdr_t hdr_sub;                            /* the subtitle letters: textured, alpha blended, in front of everything */
/* One disc stream: an open WAV file and the AICA stream that plays it. */
typedef struct film_stream {
    file_t fd;                          /* the open file, or FILEHND_INVALID when it is not on the disc */
    snd_stream_hnd_t hnd;               /* the KallistiOS stream, or SND_STREAM_INVALID */
    uint32_t rate;                      /* sample rate from the file's header, Hz */
    uint32_t data_bytes;                /* bytes of samples in the file */
    int volume;                         /* stream volume, 0..255 */
    int playing;                        /* 1 while the stream is started */
} film_stream_t;

static film_stream_t streams[STREAMS];                    /* the voice, the music, the long sound effects */
static uint8_t stream_buf[STREAMS][STREAM_BYTES] __attribute__((aligned(32)));   /* what a stream hands the AICA, 32 KB each */
static int audio_resync = 1;                              /* 1: the film clock jumped (or has just started): move the streams to it */
static sfxhnd_t sfx[ML_SND_COUNT];                        /* the loaded sounds; SFXHND_INVALID for a file that was not on the disc */
static int sfx_ready;                                     /* 1 once snd_init succeeded: sound calls are allowed */
static int wind_chn = SFX_NO_CHANNEL;                     /* AICA channel of the wind loop, or SFX_NO_CHANNEL */
static int crackle_chn = SFX_NO_CHANNEL;                  /* AICA channel of the flame's crackle loop, or SFX_NO_CHANNEL */
static int sound_shot = -1;                               /* shot of the last frame the sound saw; -1: none yet */
static float sound_s;                                     /* seconds into that shot on that frame */
static unsigned sound_steps;                              /* footprints the sound has seen pressed */
static int fog_shot = -1;                                  /* the shot whose fog range is in the PVR's fog table; -1: none yet */
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

/* TA vertex buffer use: the largest of any scene so far, from KallistiOS's statistics (pvr_get_stats: it measures each
 * scene against the right one of its two buffers). An emulator that does not move the TA's write pointer reads 0 here;
 * a console reads the real number. Nothing is said about the object pointer buffer: no trusted reading of it exists. */
static void ta_meter(void) {
    pvr_stats_t st;

    if (pvr_get_stats(&st) == 0 && st.vtx_buffer_used_max / 1024u > ta_kb) {
        ta_kb = (unsigned long)(st.vtx_buffer_used_max / 1024u);
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
        o->x = s.x + weave_x;                             /* the gate weave moves the whole 3D picture */
        o->y = s.y + weave_y;
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
    steps_full = 0;
    trail_dirty = 1;
}

/* Brings the dent grid up to the footprints the film has made by now (ml_steps_due): presses each bare foot into the
 * grid as two overlapping round stamps, heel and toe. A foot sinks in rather than appearing: every frame the prints that
 * are not yet complete are pressed again, to the depth ml_step_press gives for this moment (a stamp only ever lowers the
 * snow, so pressing a little deeper each frame is a smooth sink, and the result depends on the film clock alone). When
 * the film goes backward (a loop, a jump to an earlier shot) the grid is emptied and the footprints replayed, so jumping
 * to a shot shows the prints that shot should have. */
NOINLINE static void footprints_update(int shot, float progress) {
    const unsigned due = ml_steps_due(shot, progress);
    unsigned n;

    if (due < steps_done) {
        snow_reset();
    }
    for (n = steps_full; n < due; n++) {
        const float press = ml_step_press(shot, progress, n);          /* 0 .. 1 of the full depth, by the film clock */
        float x, z, depth, radius, half_gap, turn, bottom;

        ml_foot(n, &x, &z);
        ml_foot_shape(n, &depth, &radius, &half_gap, &turn);            /* this print's own depth, size, length and turn */
        bottom = GROUND_Y - depth * press;                             /* the sole's height at this moment, metres */
        (void)sh4der_deform_stamp(&deform, x, z + half_gap, bottom, radius, 1.0f);          /* heel */
        (void)sh4der_deform_stamp(&deform, x + turn, z - half_gap, bottom, radius, 1.0f);   /* toe, toward the cameras */
        trail_dirty = 1;
        if (press >= 1.0f && n == steps_full) {
            steps_full++;                                              /* complete: never pressed again */
        }
    }
    steps_done = due;
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

/* Draws the girl: dark quads facing the camera (three standing, four seated, two lying) and a round head, the lower edge
 * faintly warm while the match burns (she is lit from below by it, and never from the front). `pos` is her feet, `level`
 * the flame strength. */
static void girl_draw(int shot, const float pos[3], float level) {
    ml_quad_t q[ML_GIRL_MAX_QUADS];
    const int pose = ml_girl_pose(shot);
    const unsigned n = pose == ML_POSE_SEATED ? ML_GIRL_SEATED_QUADS : (pose == ML_POSE_LYING ? ML_GIRL_LYING_QUADS : ML_GIRL_QUADS);
    unsigned i, k;

    if (pose == ML_POSE_SEATED) {
        ml_girl_build_seated(&cam, pos, q);
    } else if (pose == ML_POSE_LYING) {
        ml_girl_build_lying(&cam, pos, q);
    } else {
        ml_girl_build(&cam, pos, q);
    }
    for (i = 0; i < n; i++) {
        uint32_t argb[4];

        for (k = 0; k < 4u; k++) {
            const float low = sh4der_sat_(1.0f - sh4der_div_(q[i].p[k][1] - pos[1], ML_GIRL_HEIGHT));   /* 1 at the feet, 0 at the crown */
            const float warm = level * GIRL_WARM_GAIN * low;

            argb[k] = pack_rgb(girl_rgb[0] + ML_WARM_R * warm, girl_rgb[1] + ML_WARM_G * warm, girl_rgb[2] + ML_WARM_B * warm);
        }
        (void)world_quad(&q[i], argb);
    }
    {
        float hp[ML_GIRL_HEAD_SIDES + 1u][3];
        vert_t hv[ML_GIRL_HEAD_SIDES + 1u];

        ml_girl_head(&cam, pos, pose, hp);
        for (i = 0; i <= ML_GIRL_HEAD_SIDES; i++) {
            vert_set(&hv[i], hp[i], pack_rgb(girl_rgb[0], girl_rgb[1], girl_rgb[2]));   /* the head is out of the flame's reach: plain dark */
        }
        for (i = 0; i < ML_GIRL_HEAD_SIDES; i++) {
            (void)send_tri(&hv[0], &hv[i + 1u], &hv[(i + 1u) % ML_GIRL_HEAD_SIDES + 1u]);
        }
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

/* Draws a prop's parts as solid dark cut-outs (the opaque list, the set's header already sent): `base` is its foot,
 * `layer0` the first part's lift toward the lens in ML_GIRL_LAYER steps. */
static void parts_draw_solid(const float base[3], const ml_part_t *parts, unsigned n, unsigned layer0) {
    unsigned i;

    for (i = 0; i < n; i++) {
        const uint32_t c = pack_bytes(parts[i].rgb, 1.0f);
        const uint32_t argb[4] = { c, c, c, c };
        ml_quad_t q;

        ml_part_build(&cam, base, &parts[i], layer0 + i, &q);
        (void)world_quad(&q, argb);
    }
}

/* Draws a prop's parts as added light (the translucent list, the pool's header already sent: source alpha, added):
 * `alpha` 0..1 is how strongly it shows. */
static void parts_draw_light(const float base[3], const ml_part_t *parts, unsigned n, float alpha) {
    const uint32_t a = (uint32_t)sh4der_fma_(sh4der_sat_(alpha), 255.0f, 0.5f) << 24;   /* hand: alpha byte */
    unsigned i;

    for (i = 0; i < n; i++) {
        const uint32_t c = a | ((uint32_t)parts[i].rgb[0] << 16) | ((uint32_t)parts[i].rgb[1] << 8) | (uint32_t)parts[i].rgb[2];
        const uint32_t argb[4] = { c, c, c, c };
        ml_quad_t q;

        ml_part_build(&cam, base, &parts[i], i + 1u, &q);
        (void)world_quad(&q, argb);
    }
}

/* Draws the solid props of the tale: the slipper she lost, lying by her trail while she stands (M1 .. M4), and the burned
 * matches by her hand at dawn (M10). `girl` is where she is. */
static void props_draw(int shot, const float girl[3]) {
    if (shot <= ML_SHOT_M4) {
        const float base[3] = { ML_SLIPPER_X, GROUND_Y + ml_snow_height(ML_SLIPPER_X, ML_SLIPPER_Z), ML_SLIPPER_Z };

        parts_draw_solid(base, ml_slipper_parts, ML_SLIPPER_PARTS, 0u);
    }
    if (shot == ML_SHOT_M10) {
        const float base[3] = { girl[0], GROUND_Y + ml_snow_height(girl[0], girl[2]), girl[2] };

        parts_draw_solid(base, ml_stub_parts, ML_STUB_PARTS, ML_STUB_LAYER);
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

/* Draws the two spirits of the lift (M9): the grandmother leading the girl's spirit by the hand, rising from where the girl
 * sits. Added light (the pool's header: source alpha, added), pale gold-white, stronger at each shape's upper edge than at
 * its hem. `girl` is where she sits, `seconds` the time into the shot. Sends nothing outside the lift. */
static void spirits_draw(int shot, float seconds, const float girl[3]) {
    const float level = ml_spirit_level(shot, seconds);
    ml_quad_t q[ML_SPIRIT_QUADS];
    unsigned i;

    if (!(level > 0.0f) || !header_send(&hdr_pool)) {
        return;
    }
    ml_spirit_build(&cam, girl, ml_spirit_height(seconds), q);
    for (i = 0; i < ML_SPIRIT_QUADS; i++) {
        const uint32_t rgb = ((uint32_t)ML_SPIRIT_R << 16) | ((uint32_t)ML_SPIRIT_G << 8) | (uint32_t)ML_SPIRIT_B;
        const uint32_t top = ((uint32_t)sh4der_fma_(level * SPIRIT_ALPHA_TOP, 255.0f, 0.5f) << 24) | rgb;   /* hand: alpha byte */
        const uint32_t hem = ((uint32_t)sh4der_fma_(level * SPIRIT_ALPHA_HEM, 255.0f, 0.5f) << 24) | rgb;
        const uint32_t argb[4] = { hem, top, hem, top };       /* corners 0 and 2 are the lower edge */

        (void)world_quad(&q[i], argb);
    }
}

/* Fills the sprite pool of the frame (flame glow and core, one glow per house window) and draws it. Both go through the
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
                (void)sh4der_spark_billboard(&glow_pool, centre, GLOW_HALF, 0.0f, pack_bytes(glow_rgb, GLOW_BRIGHT * ml_fog_keep(shot, view[2])),
                                             1.0f, ML_ATLAS_WINDOW, 0u);
                break;                                   /* one glow a house */
            }
        }
    }
    if (level > 0.0f) {
        float sn, cs, flick;

        sh4der_sincos_(film_clock * FLICKER_RATE, &sn, &cs);
        flick = sh4der_fma_(sn, FLICKER_DEPTH, 1.0f);
        (void)sh4der_spark_billboard(&glow_pool, flame, (FLAME_GLOW_BASE + FLAME_GLOW_PER_RADIUS * s->pool_radius) * flick, 0.0f,
                                     pack_bytes(s->pool_rgb, level * FLAME_GLOW_BRIGHT), 1.0f, ML_ATLAS_GLOW, SH4DER_SPARK_KEEP);
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

    /* The tree's candles: 48 of them leave within about 3.4 s of one another, take 3 s to arrive, and come to rest in a band
     * of sky 9 to 13 m up, spread up to 6 m from the tree. Sizes are the quad's side in metres. */
    sh4der_rise_init(&candle_rise);
    candle_rise.stagger = 0.05f;
    candle_rise.jitter = 1.0f;
    candle_rise.rise_time = 3.0f;
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
    /* 1 at night, falling to 0 over the dawn shot; 0 in a closed-fog shot (the stars are far past the end of that fog) */
    const float night = ml_fog_closed(shot) ? 0.0f : 1.0f - ml_dawn_blend(shot, progress);
    const float seconds = progress * (ml_shots[shot].end_s - ml_shots[shot].start_s);   /* seconds into the shot */
    unsigned i, sent;

    sh4der_spark_reset(&star_pool);
    sh4der_spark_reset(&extra_pool);
    for (i = 0; i < CANDLE_POOLS; i++) {
        sh4der_spark_reset(&candle_pool[i]);
    }
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
            float across, up;

            ml_tree_candle(i, &across, &up);
            {
                const float source[3] = { ML_TREE_X + across, GROUND_Y + ml_snow_height(ML_TREE_X, ML_TREE_Z) + up, ML_TREE_Z - CANDLE_DEPTH };

                rise_sprite(&candle_pool[(i / (unsigned)SH4DER_SPARK_MAX) % CANDLE_POOLS], &candle_rise, i, source, t, night);
            }
        }
    }
    if (shot == SHOT_FALL && seconds >= FALL_START_S && seconds <= FALL_START_S + FALL_TIME_S) {
        const float head = sh4der_div_(seconds - FALL_START_S, FALL_TIME_S);   /* 0..1 along the path */

        for (i = 0; i <= ML_FALL_TAIL; i++) {
            const float along = head - ML_FALL_TAIL_GAP * (float)i;              /* the tail trails the head */
            const float bright = 1.0f - sh4der_div_((float)i, (float)(ML_FALL_TAIL + 1u));   /* 1 at the head, falling along the streak */
            float at[3];

            if (along < 0.0f) {
                continue;
            }
            at[0] = sh4der_lerp_(ml_fall_from[0], ml_fall_to[0], along);
            at[1] = sh4der_lerp_(ml_fall_from[1], ml_fall_to[1], along);
            at[2] = sh4der_lerp_(ml_fall_from[2], ml_fall_to[2], along);
            /* the streak narrows to half the head's size and dims with the square of its place, so it reads as one line of fire */
            (void)sh4der_spark_billboard(&extra_pool, at, FALL_HALF * sh4der_fma_(0.5f, bright, 0.5f), 0.0f,
                                         pack_rgb(bright * bright, bright * bright, bright * bright), 1.0f, ML_ATLAS_GLOW, 0u);
        }
    }
    if (shot == SHOT_BUNDLE) {
        for (i = 0; i < BUNDLE_LIGHTS; i++) {
            float sn, cs, source[3];

            sh4der_sincos_((float)i * (6.2831853f / (float)BUNDLE_LIGHTS), &sn, &cs);   /* evenly round the ring */
            source[0] = sh4der_fma_(cs, BUNDLE_RING, vision_flame[0]);
            source[1] = GROUND_Y + 0.3f;
            source[2] = sh4der_fma_(sn, BUNDLE_RING, vision_flame[2]);
            rise_sprite(&extra_pool, &bundle_rise, i, source, seconds - ML_LIFT_START_S, vision_level > 0.0f ? 1.0f : 0.0f);
        }
    }
    sent = sh4der_spark_draw(&star_pool, &spark_view, &spark_screen);
    sent += sh4der_spark_draw(&extra_pool, &spark_view, &spark_screen);
    for (i = 0; i < CANDLE_POOLS; i++) {
        sent += sh4der_spark_draw(&candle_pool[i], &spark_view, &spark_screen);
    }
    frame_tris += 2u * sent;
}

/* HOOK: the visions. Each is made of the match's light and follows the flame's level: it is there while the match burns
 * and gone 0.4 s after it is out ("the stove vanished"; "nothing but the thick, damp wall").
 * - A soft glow in the vision's colour, in the pool.
 * - M3 the stove with its brass, M5 the table with the goose that hops off its dish and waddles toward the lens,
 *   M8 the grandmother: flat cut-outs of added light (ml_vision_parts), drawn with the pool's header.
 * - M6: the tree's three ornaments, round glows on the tree, brighter while the match burns.
 * `flame` and `level` are taken from the frame's own state (vision_flame, vision_level). */
static void film_vision_draw(int shot, float progress, float time) {
    const ml_shot_t *s = &ml_shots[shot];
    const float seconds = progress * (s->end_s - s->start_s);   /* seconds into the shot */
    const ml_part_t *parts;
    unsigned n = 0u, i, sent;

    (void)time;
    sh4der_spark_reset(&vision_pool);
    if (vision_level > 0.0f && ml_shot_lit(shot)) {
        const float at[3] = { vision_flame[0] + VISION_DX, vision_flame[1] + VISION_DY, vision_flame[2] + VISION_DZ };

        (void)sh4der_spark_billboard(&vision_pool, at, VISION_HALF, 0.0f, pack_bytes(s->vision_rgb, VISION_BRIGHT * vision_level), 1.0f,
                                     ML_ATLAS_GLOW, 0u);
    }
    if (s->has_tree) {
        for (i = 0; i < ML_TREE_ORNAMENTS; i++) {
            const float at[3] = { ML_TREE_X + ml_tree_ornaments[i][0], GROUND_Y + ml_snow_height(ML_TREE_X, ML_TREE_Z) + ml_tree_ornaments[i][1],
                                  ML_TREE_Z - CANDLE_DEPTH };

            (void)sh4der_spark_billboard(&vision_pool, at, ML_ORNAMENT_RADIUS * ORNAMENT_GLOW, 0.0f,
                                         pack_bytes(ml_tree_ornament_rgb[i], sh4der_lerp_(ORNAMENT_DIM, 1.0f, vision_level)), 1.0f, ML_ATLAS_GLOW, 0u);
        }
    }
    sent = sh4der_spark_draw(&vision_pool, &spark_view, &spark_screen);
    frame_tris += 2u * sent;
    parts = ml_vision_parts(shot, &n);
    if (parts && vision_level > 0.0f && header_send(&hdr_pool)) {
        float base[3] = { ML_VISION_X, GROUND_Y + ml_snow_height(ML_VISION_X, ML_VISION_Z), ML_VISION_Z };

        parts_draw_light(base, parts, n, vision_level * VISION_PART_ALPHA);
        if (shot == ML_SHOT_M5) {                        /* the goose: on its dish, then down and toward the lens */
            float side[3], toward[3], nearer, up;
            int k;

            ml_flat_basis(&cam, side, toward);
            ml_goose_place(seconds, &nearer, &up);
            for (k = 0; k < 3; k++) {
                base[k] = sh4der_fma_(toward[k], nearer + GOOSE_FRONT, base[k]);
            }
            base[1] += up;
            parts_draw_light(base, ml_goose_parts, ML_GOOSE_PARTS, vision_level * VISION_PART_ALPHA);
        }
    }
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
    snprintf(line, sizeof(line), "MATCHLIGHT %s  %u.%u s of %u  %s", ml_shots[shot].name, t10 / 10u, t10 % 10u, (unsigned)ML_FILM_SECONDS, paused ? "PAUSED" : "");
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
/* Sound. */

/* A little-endian 16-bit and 32-bit value out of a byte buffer (byte reads: the buffer need not be aligned). */
static uint32_t le16(const uint8_t *b) {
    return (uint32_t)b[0] | ((uint32_t)b[1] << 8);
}

static uint32_t le32(const uint8_t *b) {
    return le16(b) | (le16(b + 2) << 16);
}

/* A stream's data callback: KallistiOS asks for `requested` BYTES (its header says samples; the installed code passes
 * bytes) and gets them from the file. Past the end of the file, or on a short read, the rest is silence. */
static void *stream_fill(snd_stream_hnd_t hnd, int requested, int *received) {
    unsigned i;

    for (i = 0; i < STREAMS; i++) {
        if (streams[i].hnd == hnd && requested > 0 && requested <= STREAM_BYTES) {
            ssize_t got = fs_read(streams[i].fd, stream_buf[i], (size_t)requested);

            if (got < 0) {
                got = 0;
            }
            memset(stream_buf[i] + got, 0, (size_t)requested - (size_t)got);
            *received = requested;
            return stream_buf[i];
        }
    }
    *received = 0;
    return NULL;
}

/* Opens one stream's file and checks it is what tools/audio.py writes: a 44 byte WAV header, PCM, one channel, 16 bits.
 * Returns 1 when the stream is ready to start, 0 when the file is missing or of another kind (the stream stays silent). */
NOINLINE static int stream_open(film_stream_t *st, const char *path, int volume) {
    uint8_t head[WAV_HEADER_BYTES];

    st->hnd = SND_STREAM_INVALID;
    st->playing = 0;
    st->volume = volume;
    st->fd = fs_open(path, O_RDONLY);
    if (st->fd == FILEHND_INVALID) {
        return 0;
    }
    if (fs_read(st->fd, head, sizeof(head)) != (ssize_t)sizeof(head) || memcmp(head, "RIFF", 4) != 0 || memcmp(head + 8, "WAVEfmt ", 8) != 0 ||
        le16(head + 20) != 1u || le16(head + 22) != 1u || le16(head + 34) != 16u || memcmp(head + 36, "data", 4) != 0) {
        fs_close(st->fd);
        st->fd = FILEHND_INVALID;
        return 0;
    }
    st->rate = le32(head + 24);
    st->data_bytes = le32(head + 40);
    st->hnd = snd_stream_alloc(stream_fill, STREAM_BYTES);
    if (st->hnd == SND_STREAM_INVALID) {
        fs_close(st->fd);
        st->fd = FILEHND_INVALID;
        return 0;
    }
    return 1;
}

/* Moves every stream to film time `clock` (seconds) and starts it there, or only stops it when `run` is 0 (the film is
 * paused). Called when the clock jumps: at the start, on a pause or resume, on a shot jump, when the film loops. */
NOINLINE static void streams_sync(float clock, int run) {
    unsigned i;

    for (i = 0; i < STREAMS; i++) {
        film_stream_t *st = &streams[i];
        uint32_t at;

        if (st->hnd == SND_STREAM_INVALID) {
            continue;
        }
        if (st->playing) {
            snd_stream_stop(st->hnd);
            st->playing = 0;
        }
        if (!run) {
            continue;
        }
        at = 2u * (uint32_t)sh4der_fma_(clock, (float)st->rate, 0.5f);   /* hand: the sample index as a byte offset (2 bytes a sample) */
        if (at > st->data_bytes) {
            at = st->data_bytes;                       /* past the end of the file: silence */
        }
        if (fs_seek(st->fd, (off_t)(WAV_HEADER_BYTES + at), SEEK_SET) < 0) {
            continue;
        }
        snd_stream_start(st->hnd, st->rate, 0);        /* 16-bit PCM, mono */
        snd_stream_volume(st->hnd, st->volume);
        st->playing = 1;
    }
}

/* Lets every playing stream refill its AICA buffer. Once a frame. */
static void streams_poll(void) {
    unsigned i;

    for (i = 0; i < STREAMS; i++) {
        if (streams[i].playing) {
            (void)snd_stream_poll(streams[i].hnd);
        }
    }
}

/* Starts the AICA driver and the stream layer (snd_stream_init_ex starts both, and must come before any sound is loaded:
 * it resets the AICA), opens the two streams, and loads every sound of ml_sound_names from the disc
 * ("/cd/sfx/<name>.wav"). A missing file leaves its handle invalid and the sound silent. Returns the number of sound
 * effects loaded (0 when the driver did not start); *n_streams gets the number of streams found. */
NOINLINE static unsigned sound_setup(unsigned *n_streams) {
    char path[SFX_PATH_BUF];
    unsigned i, loaded = 0;

    *n_streams = 0;
    for (i = 0; i < STREAMS; i++) {
        streams[i].fd = FILEHND_INVALID;
        streams[i].hnd = SND_STREAM_INVALID;
    }
    if (snd_stream_init_ex(2, STREAM_BYTES) < 0) {
        return 0;
    }
    sfx_ready = 1;
    *n_streams += (unsigned)stream_open(&streams[STREAM_VOICE], "/cd/voice.wav", VOICE_VOLUME);
    *n_streams += (unsigned)stream_open(&streams[STREAM_MUSIC], "/cd/music.wav", MUSIC_VOLUME);
    *n_streams += (unsigned)stream_open(&streams[STREAM_FX], "/cd/fx.wav", FX_VOLUME);
    for (i = 0; i < ML_SND_COUNT; i++) {
        snprintf(path, sizeof(path), "/cd/sfx/%s.wav", ml_sound_names[i]);
        sfx[i] = snd_sfx_load(path);
        if (sfx[i] != SFXHND_INVALID) {
            loaded++;
        }
    }
    return loaded;
}

/* Plays sound `id` once at volume `vol` (0..255) and pan `pan` (0 left .. 255 right). Silent when it was not loaded. */
static void sound_play(unsigned id, int vol, int pan) {
    if (sfx_ready && id < ML_SND_COUNT && sfx[id] != SFXHND_INVALID) {
        (void)snd_sfx_play(sfx[id], vol, pan);
    }
}

/* Starts sound `id` as an endless loop and returns its AICA channel, or SFX_NO_CHANNEL when it was not loaded or no
 * channel was free. The whole sample loops (loop end 0 = its last sample). */
static int sound_loop_start(unsigned id, int vol) {
    sfx_play_data_t d;
    int chn;

    if (!sfx_ready || id >= ML_SND_COUNT || sfx[id] == SFXHND_INVALID) {
        return SFX_NO_CHANNEL;
    }
    memset(&d, 0, sizeof(d));
    d.chn = -1;                                        /* the next free channel */
    d.idx = sfx[id];
    d.vol = vol;
    d.pan = ML_PAN_CENTRE;
    d.loop = 1;
    d.freq = -1;                                       /* the sample's own rate */
    chn = snd_sfx_play_ex(&d);
    return chn < 0 ? SFX_NO_CHANNEL : chn;
}

/* Stops a loop started by sound_loop_start and marks it as not playing. */
/* Sets the volume (0..255) of a playing loop's AICA channel without restarting it: the channel-update command that
 * KallistiOS's own snd_stream_volume sends (the sound effect manager has no call for it). */
static void sound_loop_volume(int chn, int vol) {
    AICA_CMDSTR_CHANNEL(tmp, cmd, chan);

    if (!sfx_ready || chn == SFX_NO_CHANNEL) {
        return;
    }
    cmd->cmd = AICA_CMD_CHAN;
    cmd->timestamp = 0;
    cmd->size = AICA_CMDSTR_CHANNEL_SIZE;
    cmd->cmd_id = (uint32_t)chn;
    chan->cmd = AICA_CH_CMD_UPDATE | AICA_CH_UPDATE_SET_VOL;
    chan->vol = (uint32_t)vol;
    snd_sh4_to_aica(tmp, cmd->size);
}

static void sound_loop_stop(int *chn) {
    if (sfx_ready && *chn != SFX_NO_CHANNEL) {
        snd_sfx_stop(*chn);
    }
    *chn = SFX_NO_CHANNEL;
}

/* The sound of one frame, from the film's state:
 * - the wind loop runs from the first frame on;
 * - the crackle loop runs while a match burns (`level` > 0);
 * - every cue of ml_cues whose time the shot's clock passed since the last frame is played;
 * - one footfall for a footprint pressed since the last frame (a replay of many prints after a jump is silent).
 * While the film is paused the clock stands still, so no cue falls due.
 * Last, the two disc streams: moved to the film clock when it jumped (audio_resync), then polled. */
NOINLINE static void sound_update(const ml_where_t *w, float level) {
    float prev = sound_s;
    unsigned i;

    if (!sfx_ready) {
        return;
    }
    if (w->credits_s >= 0.0f) {                          /* the credits have the song alone: the wind has faded out by now */
        if (wind_chn != SFX_NO_CHANNEL) {
            sound_loop_stop(&wind_chn);
        }
    } else {
        static int wind_vol;                             /* the volume the wind's channel has now */
        const int want = ml_wind_volume(w->shot, w->seconds);

        if (wind_chn == SFX_NO_CHANNEL) {
            wind_chn = sound_loop_start(ML_SND_WIND, want);
            wind_vol = want;
        } else if (want != wind_vol) {                   /* the fade at the film's end: one command per volume step */
            sound_loop_volume(wind_chn, want);
            wind_vol = want;
        }
    }
    if (level > 0.0f && crackle_chn == SFX_NO_CHANNEL) {
        crackle_chn = sound_loop_start(ML_SND_CRACKLE, ML_VOL_CRACKLE);
    } else if (!(level > 0.0f) && crackle_chn != SFX_NO_CHANNEL) {
        sound_loop_stop(&crackle_chn);
    }
    if (w->shot != sound_shot || w->seconds < prev) {
        prev = -1.0f;                                  /* a new shot, or a jump backward: cues at 0 s are due */
    }
    for (i = 0; i < ML_CUES; i++) {
        if (ml_cue_due(&ml_cues[i], w->shot, prev, w->seconds)) {
            sound_play(ml_cues[i].sound, ml_cues[i].vol, ml_cues[i].pan);
        }
    }
    if (steps_done == sound_steps + 1u) {
        sound_play(ML_SND_STEP0 + ml_step_sound(steps_done - 1u), ML_VOL_STEP, ML_PAN_CENTRE);   /* the print sinks for as long as this plays */
    }
    sound_steps = steps_done;
    sound_shot = w->shot;
    sound_s = w->seconds;
    if (audio_resync) {
        streams_sync(w->clock, !paused);
        audio_resync = 0;
    }
    streams_poll();
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Subtitles. */

/* Reads the subtitle type and timing from the disc and checks both, so the frame loop can trust every offset and index:
 * font.bin ("SFN1", the picture's size, the glyph table, the ARGB4444 picture) and subs.bin ("SUB1", the sentences).
 * Returns 1 when subtitles can be drawn, 0 when a file is missing or malformed (the film then has none). The picture is
 * staged in 64 KB of heap for the upload and freed again. */
NOINLINE static int subs_load(void) {
    uint8_t head[12];
    pvr_poly_cxt_t c;
    file_t fd;
    void *stage;
    ssize_t got;
    unsigned i, n, at;

    sub_sentences = 0;
    fd = fs_open("/cd/font.bin", O_RDONLY);
    if (fd == FILEHND_INVALID) {
        return 0;
    }
    if (fs_read(fd, head, sizeof(head)) != (ssize_t)sizeof(head) || memcmp(head, "SFN1", 4) != 0 || le16(head + 4) != (uint32_t)SUB_FONT_W ||
        le16(head + 6) != (uint32_t)SUB_FONT_H || le16(head + 8) < 1u || le16(head + 8) > SUB_GLYPHS_MAX) {
        fs_close(fd);
        return 0;
    }
    sub_glyphs = le16(head + 8);
    stage = memalign(32, SUB_FONT_BYTES);
    sub_tex = pvr_mem_malloc(SUB_FONT_BYTES);
    if (!stage || !sub_tex || fs_read(fd, sub_glyph, sub_glyphs * 4u) != (ssize_t)(sub_glyphs * 4u) ||
        fs_read(fd, stage, SUB_FONT_BYTES) != (ssize_t)SUB_FONT_BYTES) {
        free(stage);
        fs_close(fd);
        return 0;
    }
    fs_close(fd);
    pvr_txr_load(stage, sub_tex, SUB_FONT_BYTES);       /* rows as they are: the texture is not twiddled */
    free(stage);
    for (i = 0; i < sub_glyphs; i++) {                  /* every glyph inside the picture (x and w are bytes: x + w can reach 510) */
        if ((unsigned)sub_glyph[i][0] + sub_glyph[i][2] > (unsigned)SUB_FONT_W || (unsigned)sub_glyph[i][1] + sub_glyph[i][3] > (unsigned)SUB_FONT_H) {
            return 0;
        }
    }

    fd = fs_open("/cd/subs.bin", O_RDONLY);
    if (fd == FILEHND_INVALID) {
        return 0;
    }
    got = fs_read(fd, subs_data, sizeof(subs_data));
    fs_close(fd);
    if (got < (ssize_t)SUB_HEAD_BYTES || got >= (ssize_t)sizeof(subs_data) || memcmp(subs_data, "SUB1", 4) != 0) {
        return 0;                                       /* missing, or as large as the buffer (it may be cut short) */
    }
    n = le16(subs_data + 4);
    if (n < 1u || n > SUB_SENTENCES_MAX) {
        return 0;
    }
    at = SUB_HEAD_BYTES;
    for (i = 0; i < n; i++) {                           /* walk the records: each must lie inside the file and name real glyphs */
        unsigned g, count;

        if (at + SUB_SENT_BYTES > (unsigned)got) {
            return 0;
        }
        count = le16(subs_data + at + 8);
        if (count > ML_SUB_MAX_GLYPHS || at + SUB_SENT_BYTES + count * SUB_GLYPH_BYTES > (unsigned)got) {
            return 0;
        }
        for (g = 0; g < count; g++) {
            if (subs_data[at + SUB_SENT_BYTES + g * SUB_GLYPH_BYTES + 6u] >= sub_glyphs) {
                return 0;
            }
        }
        sub_at[i] = (uint16_t)at;
        at += SUB_SENT_BYTES + count * SUB_GLYPH_BYTES;
    }
    /* The letters: the glyph picture's alpha over white, ordinary blend, always in front, no fog, unfiltered (the letters
     * are drawn one texel to one pixel). */
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, SUB_FONT_W, SUB_FONT_H, sub_tex, PVR_FILTER_NONE);
    c.gen.alpha = PVR_ALPHA_ENABLE;
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.txr.env = PVR_TXRENV_MODULATEALPHA;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&hdr_sub, &c);
    sub_sentences = n;
    return 1;
}

/* Sets the subtitles up (subs_load). When that fails part way, the glyph picture's video memory is given back and the
 * film has no subtitles. Returns 1 when subtitles can be drawn. */
NOINLINE static int subs_setup(void) {
    if (subs_load()) {
        return 1;
    }
    if (sub_tex) {
        pvr_mem_free(sub_tex);
        sub_tex = NULL;
    }
    sub_sentences = 0;
    return 0;
}

/* Draws the subtitle of film time `clock` (seconds): the sentence whose time span holds the clock, and of it every
 * letter whose moment has come (the typewriter). Each letter is one screen rectangle with the glyph's texels, one texel
 * to one pixel. Nothing is sent between sentences. No guard is needed past the header's: a letter is a fixed, upright
 * rectangle with a width and height checked to be at least one pixel. */
static void subs_draw(float clock) {
    const uint32_t ms = (uint32_t)sh4der_fma_(clock, 1000.0f, 0.5f);   /* hand: seconds to whole milliseconds */
    unsigned i, g;

    for (i = 0; i < sub_sentences; i++) {
        const uint8_t *rec = subs_data + sub_at[i];
        const uint32_t t0 = le32(rec);
        const unsigned count = le16(rec + 8);

        if (ms < t0 || ms >= le32(rec + 4)) {
            continue;
        }
        if (!header_send(&hdr_sub)) {
            return;
        }
        for (g = 0; g < count; g++) {
            const uint8_t *q = rec + SUB_SENT_BYTES + g * SUB_GLYPH_BYTES;
            const uint8_t *gl = sub_glyph[q[6]];
            const float x0 = (float)le16(q), y0 = (float)le16(q + 2);
            unsigned k;

            if (t0 + le16(q + 4) > ms || gl[2] < 1u || gl[3] < 1u) {
                continue;                                    /* not typed yet, or an empty glyph */
            }
            for (k = 0; k < 4u; k++) {                       /* strip order: left top, left bottom, right top, right bottom */
                pvr_vertex_t v;

                v.flags = k == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                v.x = x0 + ((k & 2u) ? (float)gl[2] : 0.0f);
                v.y = y0 + ((k & 1u) ? (float)gl[3] : 0.0f);
                v.z = Z_HUD;
                v.u = (float)(gl[0] + ((k & 2u) ? gl[2] : 0u)) * (1.0f / (float)SUB_FONT_W);
                v.v = (float)(gl[1] + ((k & 1u) ? gl[3] : 0u)) * (1.0f / (float)SUB_FONT_H);
                v.argb = SUB_ARGB;
                v.oargb = 0;
                pvr_prim(&v, sizeof(v));
            }
            frame_tris += 2u;
        }
        return;                                              /* one sentence at a time */
    }
}

/* ------------------------------------------------------------------------------------------------------------- */
/* The old-film look: the picture as worn film in a projector. Laid over the 16:9 picture only, never over the letterbox
 * bars, the subtitles or the credits; the B button turns it off and on. Parts, in the order they are drawn:
 * - vignette (sh4der_lens): darker corners, a multiply mesh built once;
 * - grain (sh4der_lens): a 64 x 64 noise tile that jumps every frame, a multiply;
 * - exposure (sh4der_film): one multiply rectangle whose grey flickers a few per cent, with a faint warm tint;
 * - scratches and dust specks (sh4der_film): thin lines and small squares, blended white or black;
 * - gate weave (sh4der_film): not drawn here: camera_set moves the projection centre by up to a pixel.
 * The title card gets a lighter wear (half the grain, two scratches, six specks, no flicker, no tint, no weave). */

/* Builds the look's fixed parts once: the wear's settings, the vignette mesh for the 640 x 360 picture, the grain tile
 * (8 KB of video memory) and the two headers. Returns 1, or 0 when video memory could not be allocated (no film look). */
NOINLINE static int film_setup(void) {
    static uint16_t tile[SH4DER_LENS_GRAIN_TILE * SH4DER_LENS_GRAIN_TILE] __attribute__((aligned(32)));   /* staging for the upload */
    sh4der_lens_vig_t vig;
    pvr_poly_cxt_t c;
    unsigned i;

    film_ready = 0;
    sh4der_film_init(&film, (float)ML_SCREEN_W, (float)ML_BAND_H);
    film.keep_x0 = 0.5f * (float)ML_SCREEN_W - FILM_KEEP_HALF_X;   /* no scratch through the middle of the picture, where she is */
    film.keep_x1 = 0.5f * (float)ML_SCREEN_W + FILM_KEEP_HALF_X;
    film.seed = FILM_SEED;
    sh4der_lens_vig_default(&vig, (float)ML_SCREEN_W, (float)ML_BAND_H);
    vig.strength = FILM_VIG_STRENGTH;
    vig.spokes = FILM_VIG_SPOKES;
    vig.rings = FILM_VIG_RINGS;
    film_vig_verts = sh4der_lens_vig_mesh(&vig, film_vig, FILM_VIG_VERTS);
    for (i = 0; i < SH4DER_LENS_GRAIN_TILE * SH4DER_LENS_GRAIN_TILE; i++) {
        tile[i] = sh4der_lens_grain_argb4444(sh4der_lens_grain_level(i, SH4DER_LENS_GRAIN_SEED));
    }
    film_grain_tex = pvr_mem_malloc(sizeof(tile));
    if (!film_grain_tex || film_vig_verts == 0u) {
        return 0;
    }
    pvr_txr_load(tile, film_grain_tex, sizeof(tile));    /* rows as they are: the texture is not twiddled */
    /* Multiply, untextured: frame x vertex colour (source factor = the frame's colour, destination factor = 0). */
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.gen.alpha = PVR_ALPHA_ENABLE;
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.blend.src = PVR_BLEND_DESTCOLOR;
    c.blend.dst = PVR_BLEND_ZERO;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&hdr_film_mul, &c);
    /* Multiply by the grain tile: frame x (tile x vertex colour + offset colour), which is frame x ((1 - a) + a x noise)
     * with the vertex colour a and the offset colour 1 - a. Repeating, unfiltered: one texel to one pixel. */
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, SH4DER_LENS_GRAIN_TILE, SH4DER_LENS_GRAIN_TILE, film_grain_tex,
                     PVR_FILTER_NONE);
    c.gen.alpha = PVR_ALPHA_ENABLE;
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.gen.specular = PVR_SPECULAR_ENABLE;                /* the offset colour is added to the textured colour */
    c.txr.env = PVR_TXRENV_MODULATE;
    c.txr.uv_clamp = PVR_UVCLAMP_NONE;
    c.blend.src = PVR_BLEND_DESTCOLOR;
    c.blend.dst = PVR_BLEND_ZERO;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&hdr_film_grain, &c);
    film_ready = 1;
    return 1;
}

/* A grey of 0 .. 1 as an opaque colour 0xFFvvvvvv. */
static uint32_t film_grey(float v) {
    const uint32_t b = (uint32_t)sh4der_fma_(sh4der_sat_(v), 255.0f, 0.5f);   /* hand: the colour byte */

    return 0xff000000u | (b << 16) | (b << 8) | b;
}

/* Draws the old-film look over the picture band at clock time `t` (the translucent list is open). `title` 1: the lighter
 * wear of the title card. `keep_rows` 1: the lower middle of the picture (the trail of footprints in the low shot) takes
 * at most two specks. No guard is needed past the headers': every shape is a fixed mesh or an upright rectangle of at
 * least one pixel. */
static void film_draw(float t, int title, int keep_rows) {
    static const float warm[3] = { FILM_TINT_R, FILM_TINT_G, FILM_TINT_B };
    const float top = 0.5f * (float)(ML_SCREEN_H - ML_BAND_H);             /* first row of the picture */
    const float bottom = top + (float)ML_BAND_H;
    const float amp = title ? 0.5f * SH4DER_LENS_GRAIN_AMP : SH4DER_LENS_GRAIN_AMP;
    const uint32_t tick = (uint32_t)sh4der_trunc_(sh4der_sat_(t * (1.0f / SH4DER_FILM_TIME_MAX_S)) * SH4DER_FILM_TIME_MAX_S * film.tick_hz);   /* hand: the frame count for the grain's jump */
    sh4der_film_speck_t specks[SH4DER_FILM_SPECKS_MAX];
    float uv[4];
    unsigned i, n, k;

    if (!film_ready || !film_on) {
        return;
    }
    if (header_send(&hdr_film_mul)) {
        const unsigned per = 2u * (FILM_VIG_SPOKES + 1u);                  /* vertices of one band of the vignette: one strip */

        for (i = 0; i + per <= film_vig_verts; i += per) {
            for (k = 0; k < per; k++) {
                pvr_vertex_t v;

                v.flags = k == per - 1u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                v.x = film_vig[i + k].x;
                v.y = film_vig[i + k].y + top;
                v.z = Z_OVERLAY;
                v.u = 0.0f;
                v.v = 0.0f;
                v.argb = film_vig[i + k].argb;
                v.oargb = 0;
                pvr_prim(&v, sizeof(v));
            }
            frame_tris += per - 2u;
        }
        if (!title) {                                    /* the flicker and the warm film base */
            screen_rect(0.0f, top, (float)ML_SCREEN_W, bottom, Z_OVERLAY, sh4der_film_exposure_argb(&film, t, warm, FILM_TINT), 0);
        }
    }
    if (header_send(&hdr_film_grain)) {
        sh4der_lens_grain_uv(tick, (float)ML_SCREEN_W, (float)ML_BAND_H, uv);
        for (k = 0; k < 4u; k++) {                       /* strip order: left top, left bottom, right top, right bottom */
            pvr_vertex_t v;

            v.flags = k == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
            v.x = (k & 2u) ? (float)ML_SCREEN_W : 0.0f;
            v.y = (k & 1u) ? bottom : top;
            v.z = Z_OVERLAY;
            v.u = (k & 2u) ? uv[2] : uv[0];
            v.v = (k & 1u) ? uv[3] : uv[1];
            v.argb = film_grey(amp);
            v.oargb = film_grey(1.0f - amp);
            pvr_prim(&v, sizeof(v));
        }
        frame_tris += 2u;
    }
    if (!header_send(&hdr_fade)) {
        return;
    }
    for (i = 0; i < (title ? FILM_TITLE_SCRATCHES : SH4DER_FILM_SCRATCH_SLOTS); i++) {
        sh4der_film_scratch_t s;

        if (sh4der_film_scratch(&film, i, t, &s)) {
            screen_rect(s.x, top, s.x + 1.0f, bottom, Z_OVERLAY,
                        ((uint32_t)sh4der_fma_(s.alpha, 255.0f, 0.5f) << 24) | (s.light ? 0x00ffffffu : 0u), 0);
        }
    }
    film.keep_y0 = keep_rows ? FILM_KEEP_Y0 : 0.0f;      /* the band of rows that stays nearly clean, picture rows */
    film.keep_y1 = keep_rows ? FILM_KEEP_Y1 : 0.0f;
    film.keep_specks = FILM_KEEP_SPECKS;
    n = sh4der_film_specks(&film, t, specks, title ? FILM_TITLE_SPECKS : SH4DER_FILM_SPECKS_MAX);
    for (i = 0; i < n; i++) {
        screen_rect(specks[i].x, top + specks[i].y, specks[i].x + specks[i].size, top + specks[i].y + specks[i].size, Z_OVERLAY,
                    ((uint32_t)sh4der_fma_(specks[i].alpha, 255.0f, 0.5f) << 24) | (specks[i].light ? 0x00ffffffu : 0u), 0);
    }
}

/* ------------------------------------------------------------------------------------------------------------- */
/* The lens pass: the film's picture is drawn into a frame texture of the library (sh4der_ca: 640 x 480, 640 KB of video
 * memory each) and put on the screen as three colour channels, each at its own scale about the centre (chromatic
 * aberration). With a second frame texture the picture is also anti-aliased over time (sh4der_taa): every frame is drawn
 * a fraction of a pixel aside and the frame before is blended over it inside the texture, so edges average out. The
 * title card, the film wear, the letterbox bars, the subtitles and the readout are drawn after the pass, on the screen,
 * at full sharpness; the end credits do not go through it. The Y button switches the pass. */

/* Takes the frame textures: two (aberration and anti-aliasing), or one (aberration only) when two would leave less than
 * LENS_VRAM_RESERVE_KB of video memory free, or none when one would. Call after every other texture is allocated.
 * Returns how many are held; prints one line with the video memory free before and after. */
NOINLINE static unsigned lens_setup(void) {
    const unsigned long before_kb = (unsigned long)pvr_mem_available() / 1024ul;
    unsigned n;

    memset(&lens_gpu, 0, sizeof(lens_gpu));
    sh4der_taa_init(&lens_taa);
    for (n = SH4DER_CA_TEXTURES; n > 0u; n--) {
        if (sh4der_ca_setup(&lens_gpu, 0, n) && (unsigned long)pvr_mem_available() / 1024ul >= LENS_VRAM_RESERVE_KB) {
            break;
        }
    }
    if (n == 0u) {
        (void)sh4der_ca_setup(&lens_gpu, 0, 0u);         /* gives back whatever the last try held */
    }
    lens_n = n;
    printf("matchlight: lens pass: %u frame texture(s) of %u KB (%s), VRAM free %lu KB before, %lu KB after (reserve %u KB)\n", n,
           sh4der_ca_bytes(&lens_gpu.size) / 1024u, n == 2u ? "aberration and anti-aliasing" : n == 1u ? "aberration only" : "off: no room",
           before_kb, (unsigned long)pvr_mem_available() / 1024ul, LENS_VRAM_RESERVE_KB);
    return n;
}

/* 1 when this frame's picture goes through the lens pass: textures are held, the pass is switched on, and the frame is
 * not an end credits card. */
static int lens_active(const ml_where_t *w) {
    return lens_n > 0u && lens_on && w->credits_s < 0.0f;
}

/* The anti-aliasing's numbers for this frame (camera_set calls it): the sub-pixel offset of the picture, added to
 * shift[0..1] (pixels), and lens_weight, the share of the frame before in this one. A new shot, the first frame after the
 * pass was off, and a frame outside the pass are cuts: nothing of the frame before is kept. The camera's own slow moves
 * are not followed (the library's turn measure is handed no angles): at LENS_TAA_WEIGHT the frame before fades to a tenth
 * in three frames. */
static void lens_step(const ml_where_t *w, float shift[2]) {
    sh4der_taa_frame_t f;

    lens_weight = 0.0f;
    if (!lens_active(w) || lens_n < 2u) {
        lens_have = 0;
        lens_shot = -1;
        return;
    }
    if (w->shot != lens_shot) {
        sh4der_taa_reset(&lens_taa);                     /* a cut: the next step keeps no history */
        lens_have = 0;
    }
    lens_shot = w->shot;
    sh4der_taa_step(&lens_taa, 0.0f, 0.0f, cam.focal_px, cam.focal_px, &f);
    shift[0] += f.jitter[0];
    shift[1] += f.jitter[1];
    lens_weight = f.cut || !lens_have ? 0.0f : LENS_TAA_WEIGHT;
}

/* Ends the lens pass's first scene and begins its second: the frame before is blended into the frame texture (when there
 * is one to blend), the texture's scene is finished, and the screen's scene is begun with the texture's three colour
 * channels (green opaque, red and blue added). Leaves the screen's translucent list open for the layers that follow. */
static void lens_resolve(void) {
    unsigned tries = 0;

    if (lens_weight > 0.0f) {
        sh4der_ca_quad(&lens_gpu, (unsigned)lens_cur ^ 1u, lens_weight, 0.0f, 0.0f, Z_OVERLAY, 1.0f);
        frame_tris += 2u;
    }
    ta_meter();                                          /* this is the heavy scene (the whole picture): count it while its list is open */
    pvr_list_finish();
    pvr_scene_finish();
    while (pvr_wait_ready() < 0 && ++tries < STUCK_WAITS) {   /* the texture's render must be done before it is drawn (KallistiOS times a wait out at 100 ms) */
    }
    pvr_scene_begin();
    pvr_list_begin(PVR_LIST_OP_POLY);
    sh4der_ca_channel(&lens_gpu, (unsigned)lens_cur, SH4DER_CA_GREEN, LENS_CA_STRENGTH, (float)ML_SCREEN_W, (float)ML_SCREEN_H, Z_LENS);
    pvr_list_finish();
    pvr_list_begin(PVR_LIST_TR_POLY);
    sh4der_ca_channel(&lens_gpu, (unsigned)lens_cur, SH4DER_CA_RED, LENS_CA_STRENGTH, (float)ML_SCREEN_W, (float)ML_SCREEN_H, Z_LENS);
    sh4der_ca_channel(&lens_gpu, (unsigned)lens_cur, SH4DER_CA_BLUE, LENS_CA_STRENGTH, (float)ML_SCREEN_W, (float)ML_SCREEN_H, Z_LENS);
    frame_tris += 3u * 2u * SH4DER_CA_MESH_X * SH4DER_CA_MESH_Y;
    if (lens_n == 2u) {
        lens_cur ^= 1;                                   /* this frame's texture is the next frame's history */
        lens_have = 1;
    }
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Title card. */

/* Reads the title card from the disc and checks it: title.bin ("TTL1": the alpha bands, then two ARGB1555 texture tiles,
 * 512 x 512 and 128 x 512; tools/title.py writes it). Returns 1 when the card can be drawn, 0 when the file is missing
 * or malformed. Each tile is staged in heap (512 KB at the most, freed again) for its upload. */
NOINLINE static int title_load(void) {
    static const unsigned tile_w[TITLE_TILES] = { TITLE_TILE_A_W, TITLE_TILE_B_W };
    uint8_t head[TITLE_HEAD_BYTES];
    uint8_t rec[TITLE_BAND_BYTES];
    pvr_poly_cxt_t c;
    file_t fd;
    void *stage;
    unsigned i, row = 0;

    title_bands = 0;
    fd = fs_open("/cd/title.bin", O_RDONLY);
    if (fd == FILEHND_INVALID) {
        return 0;
    }
    if (fs_read(fd, head, sizeof(head)) != (ssize_t)sizeof(head) || memcmp(head, "TTL1", 4) != 0 || le16(head + 4) != (uint32_t)ML_SCREEN_W ||
        le16(head + 6) != (uint32_t)ML_SCREEN_H || le16(head + 8) < 1u || le16(head + 8) > TITLE_BANDS_MAX) {
        fs_close(fd);
        return 0;
    }
    for (i = 0; i < le16(head + 8); i++) {               /* the bands must tile the rows 0 .. 479 in order, each at least one row */
        if (fs_read(fd, rec, sizeof(rec)) != (ssize_t)sizeof(rec) || le16(rec) != row || le16(rec + 2) <= row || le16(rec + 2) > (uint32_t)ML_SCREEN_H) {
            fs_close(fd);
            return 0;
        }
        row = le16(rec + 2);
        title_band[i].y0 = (uint16_t)le16(rec);
        title_band[i].y1 = (uint16_t)row;
        title_band[i].alpha = rec[4];
    }
    if (row != (unsigned)ML_SCREEN_H) {
        fs_close(fd);
        return 0;
    }
    stage = memalign(32, TITLE_TILE_A_W * TITLE_TILE_H * 2u);
    if (!stage) {
        fs_close(fd);
        return 0;
    }
    for (i = 0; i < TITLE_TILES; i++) {
        const size_t bytes = (size_t)tile_w[i] * TITLE_TILE_H * 2u;

        title_tex[i] = pvr_mem_malloc(bytes);
        if (!title_tex[i] || fs_read(fd, stage, bytes) != (ssize_t)bytes) {
            free(stage);
            fs_close(fd);
            return 0;
        }
        pvr_txr_load(stage, title_tex[i], bytes);        /* rows as they are: the texture is not twiddled */
        /* The card's texels over whatever is behind, by the vertex alpha (every texel's own alpha bit is set): ordinary
         * blend, always in front, no fog, unfiltered (one texel to one pixel). */
        pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, (int)tile_w[i], (int)TITLE_TILE_H, title_tex[i], PVR_FILTER_NONE);
        c.gen.alpha = PVR_ALPHA_ENABLE;
        c.gen.culling = PVR_CULLING_NONE;
        c.gen.fog_type = PVR_FOG_DISABLE;
        c.txr.env = PVR_TXRENV_MODULATEALPHA;
        c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        c.depth.write = PVR_DEPTHWRITE_DISABLE;
        c.blend.src = PVR_BLEND_SRCALPHA;
        c.blend.dst = PVR_BLEND_INVSRCALPHA;
        pvr_poly_compile(&hdr_title[i], &c);
    }
    free(stage);
    fs_close(fd);
    title_bands = le16(head + 8);
    return 1;
}

/* Sets the title card up (title_load). When that fails part way, the tiles' video memory is given back and the film has
 * no title card (the clock then skips the card's stretch). Returns 1 when the card can be drawn. */
NOINLINE static int title_setup(void) {
    unsigned i;

    if (title_load()) {
        return 1;
    }
    for (i = 0; i < TITLE_TILES; i++) {
        if (title_tex[i]) {
            pvr_mem_free(title_tex[i]);
            title_tex[i] = NULL;
        }
    }
    title_bands = 0;
    return 0;
}

/* Draws the title card at `s` seconds into it, over the frame already drawn (the translucent list is open): per tile,
 * one screen rectangle a band of rows, with the band's alpha in the vertex colour, one texel to one pixel; then black
 * over everything while the card fades in or out. No guard is needed past the headers': every rectangle is fixed,
 * upright and at least one row high (title_load checked the bands). */
static void title_draw(float s) {
    static const float tile_x[TITLE_TILES + 1u] = { 0.0f, (float)TITLE_TILE_A_W, (float)(TITLE_TILE_A_W + TITLE_TILE_B_W) };
    const float show = ml_title_alpha(s);
    unsigned i, b, k;

    for (i = 0; i < TITLE_TILES && title_bands > 0u; i++) {
        if (!header_send(&hdr_title[i])) {
            continue;
        }
        for (b = 0; b < title_bands; b++) {
            const uint32_t argb = ((uint32_t)title_band[b].alpha << 24) | 0x00ffffffu;

            if (title_band[b].alpha == 0u) {
                continue;                                /* a fully clear band adds nothing to the frame: no quad */
            }
            for (k = 0; k < 4u; k++) {                   /* strip order: left top, left bottom, right top, right bottom */
                pvr_vertex_t v;

                v.flags = k == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                v.x = (k & 2u) ? tile_x[i + 1u] : tile_x[i];
                v.y = (float)((k & 1u) ? title_band[b].y1 : title_band[b].y0);
                v.z = Z_OVERLAY;
                v.u = (k & 2u) ? 1.0f : 0.0f;
                v.v = v.y * (1.0f / (float)TITLE_TILE_H);
                v.argb = argb;
                v.oargb = 0;
                pvr_prim(&v, sizeof(v));
            }
            frame_tris += 2u;
        }
    }
    if (show < 1.0f && header_send(&hdr_fade)) {         /* up from black, and down to black before the first shot */
        screen_rect(0.0f, 0.0f, (float)ML_SCREEN_W, (float)ML_SCREEN_H, Z_HUD, (uint32_t)sh4der_fma_(1.0f - show, 255.0f, 0.5f) << 24, 0);
    }
}

/* ------------------------------------------------------------------------------------------------------------- */
/* End credits. */

/* Reads the credit cards from the disc and checks them, so the frame loop can trust every offset and index: credits.bin
 * ("CRD1": the glyph table, the card records, the ARGB4444 glyph picture; tools/credits.py writes it). Returns 1 when the
 * cards can be drawn, 0 when the file is missing or malformed (the credits are then a black pause). The picture is
 * staged in 128 KB of heap for the upload and freed again. */
NOINLINE static int credits_load(void) {
    uint8_t head[CREDIT_HEAD_BYTES];
    pvr_poly_cxt_t c;
    file_t fd;
    void *stage;
    unsigned i, at, body;

    credit_cards = 0;
    fd = fs_open("/cd/credits.bin", O_RDONLY);
    if (fd == FILEHND_INVALID) {
        return 0;
    }
    if (fs_read(fd, head, sizeof(head)) != (ssize_t)sizeof(head) || memcmp(head, "CRD1", 4) != 0 || le16(head + 4) != ML_CREDIT_CARDS ||
        le16(head + 6) < 1u || le16(head + 6) > CREDIT_GLYPHS_MAX || le16(head + 8) != (uint32_t)CREDIT_FONT_W || le16(head + 10) != (uint32_t)CREDIT_FONT_H ||
        le32(head + 12) < 1u || le32(head + 12) > sizeof(credit_data)) {
        fs_close(fd);
        return 0;
    }
    credit_glyphs = le16(head + 6);
    body = le32(head + 12);
    stage = memalign(32, CREDIT_FONT_BYTES);
    credit_tex = pvr_mem_malloc(CREDIT_FONT_BYTES);
    if (!stage || !credit_tex || fs_read(fd, credit_glyph, credit_glyphs * 4u) != (ssize_t)(credit_glyphs * 4u) ||
        fs_read(fd, credit_data, body) != (ssize_t)body || fs_read(fd, stage, CREDIT_FONT_BYTES) != (ssize_t)CREDIT_FONT_BYTES) {
        free(stage);
        fs_close(fd);
        return 0;
    }
    fs_close(fd);
    pvr_txr_load(stage, credit_tex, CREDIT_FONT_BYTES);  /* rows as they are: the texture is not twiddled */
    free(stage);
    for (i = 0; i < credit_glyphs; i++) {                /* every glyph inside the picture */
        if ((unsigned)credit_glyph[i][0] + credit_glyph[i][2] > (unsigned)CREDIT_FONT_W || (unsigned)credit_glyph[i][1] + credit_glyph[i][3] > (unsigned)CREDIT_FONT_H) {
            return 0;
        }
    }
    at = 0;
    for (i = 0; i < ML_CREDIT_CARDS; i++) {              /* walk the records: each must lie inside the block and name real glyphs */
        unsigned g, count;

        if (at + CREDIT_CARD_BYTES > body) {
            return 0;
        }
        count = le16(credit_data + at);
        if (count > CREDIT_LETTERS_MAX || at + CREDIT_CARD_BYTES + count * CREDIT_LETTER_BYTES > body) {
            return 0;
        }
        for (g = 0; g < count; g++) {
            if (credit_data[at + CREDIT_CARD_BYTES + g * CREDIT_LETTER_BYTES + 4u] >= credit_glyphs) {
                return 0;
            }
        }
        credit_at[i] = (uint16_t)at;
        at += CREDIT_CARD_BYTES + count * CREDIT_LETTER_BYTES;
    }
    /* The letters: the glyph picture's alpha over the vertex colour, ordinary blend, always in front, no fog, unfiltered
     * (one texel to one pixel). */
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB4444 | PVR_TXRFMT_NONTWIDDLED, CREDIT_FONT_W, CREDIT_FONT_H, credit_tex, PVR_FILTER_NONE);
    c.gen.alpha = PVR_ALPHA_ENABLE;
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.txr.env = PVR_TXRENV_MODULATEALPHA;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&hdr_credit, &c);
    credit_cards = ML_CREDIT_CARDS;
    return 1;
}

/* Sets the credits up (credits_load). When that fails part way, the glyph picture's video memory is given back and the
 * credits are a black pause. Returns 1 when the cards can be drawn. */
NOINLINE static int credits_setup(void) {
    if (credits_load()) {
        return 1;
    }
    if (credit_tex) {
        pvr_mem_free(credit_tex);
        credit_tex = NULL;
    }
    credit_cards = 0;
    return 0;
}

/* A colour of 0..255 bytes with a 0..1 alpha, as 0xAARRGGBB. */
static uint32_t credit_argb(float alpha, unsigned r, unsigned g, unsigned b) {
    return ((uint32_t)sh4der_fma_(sh4der_sat_(alpha), 255.0f, 0.5f) << 24) | ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;   /* hand: alpha byte */
}

/* Draws the end credits at `s` seconds into them, as the whole frame (the translucent list is open, nothing else is
 * drawn): the near-black ground over the active picture, the hairline border with a tick across each corner, and every
 * card that shows at this moment (two during a dissolve): its amber rule and its letters, one screen rectangle a letter,
 * one texel to one pixel. The rows outside the active picture stay the black of the background. No guard is needed past
 * the headers': every rectangle is fixed, upright and at least one pixel in each direction. */
static void credits_draw(float s) {
    const float top = 0.5f * (float)(ML_SCREEN_H - ML_BAND_H) + (float)ML_CREDIT_BAR;   /* first row of the active picture */
    const float bottom = (float)ML_SCREEN_H - top;                                      /* the row after its last */
    const float frame = ml_credit_frame_alpha(s);
    const float x0 = (float)ML_CREDIT_INSET, x1 = (float)(ML_SCREEN_W - ML_CREDIT_INSET);
    const float y0 = top + (float)ML_CREDIT_INSET, y1 = bottom - (float)ML_CREDIT_INSET;
    const float tick = (float)ML_CREDIT_TICK;
    unsigned card, g;

    if (!(frame > 0.0f) || !header_send(&hdr_fade)) {
        return;
    }
    screen_rect(0.0f, top, (float)ML_SCREEN_W, bottom, Z_OVERLAY, credit_argb(frame, ML_CREDIT_GROUND_R, ML_CREDIT_GROUND_G, ML_CREDIT_GROUND_B), 0);
    {
        const uint32_t line = credit_argb(frame, ML_CREDIT_LINE_R, ML_CREDIT_LINE_G, ML_CREDIT_LINE_B);

        /* the border: top, bottom, left, right, each one pixel thick and reaching a tick's length past both corners */
        screen_rect(x0 - tick, y0, x1 + tick, y0 + 1.0f, Z_OVERLAY, line, 0);
        screen_rect(x0 - tick, y1 - 1.0f, x1 + tick, y1, Z_OVERLAY, line, 0);
        screen_rect(x0, y0 - tick, x0 + 1.0f, y1 + tick, Z_OVERLAY, line, 0);
        screen_rect(x1 - 1.0f, y0 - tick, x1, y1 + tick, Z_OVERLAY, line, 0);
    }
    for (card = 0; card < credit_cards; card++) {
        const float a = ml_credit_alpha(card, s);
        const uint8_t *rec = credit_data + credit_at[card];
        const unsigned count = le16(rec);
        const float rule_y = (float)le16(rec + 2);

        if (!(a > 0.0f)) {
            continue;
        }
        if (header_send(&hdr_fade)) {                    /* the one accent: a short amber rule under the label */
            screen_rect(0.5f * (float)ML_SCREEN_W - (float)ML_CREDIT_RULE_HALF, rule_y, 0.5f * (float)ML_SCREEN_W + (float)ML_CREDIT_RULE_HALF, rule_y + 1.0f,
                        Z_OVERLAY, credit_argb(a, 255u, (unsigned)(ML_WARM_G * 255.0f + 0.5f), (unsigned)(ML_WARM_B * 255.0f + 0.5f)), 0);
        }
        if (!header_send(&hdr_credit)) {
            continue;
        }
        for (g = 0; g < count; g++) {
            const uint8_t *q = rec + CREDIT_CARD_BYTES + g * CREDIT_LETTER_BYTES;
            const uint8_t *gl = credit_glyph[q[4]];
            const float gx = (float)le16(q), gy = (float)le16(q + 2);
            const uint32_t argb = credit_argb(a * (float)q[5] * (1.0f / 255.0f), ML_CREDIT_INK_R, ML_CREDIT_INK_G, ML_CREDIT_INK_B);
            unsigned k;

            if (gl[2] < 1u || gl[3] < 1u) {
                continue;                                /* an empty glyph */
            }
            for (k = 0; k < 4u; k++) {                   /* strip order: left top, left bottom, right top, right bottom */
                pvr_vertex_t v;

                v.flags = k == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                v.x = gx + ((k & 2u) ? (float)gl[2] : 0.0f);
                v.y = gy + ((k & 1u) ? (float)gl[3] : 0.0f);
                v.z = Z_HUD;
                v.u = (float)(gl[0] + ((k & 2u) ? gl[2] : 0u)) * (1.0f / (float)CREDIT_FONT_W);
                v.v = (float)(gl[1] + ((k & 1u) ? gl[3] : 0u)) * (1.0f / (float)CREDIT_FONT_H);
                v.argb = argb;
                v.oargb = 0;
                pvr_prim(&v, sizeof(v));
            }
            frame_tris += 2u;
        }
    }
}

/* ------------------------------------------------------------------------------------------------------------- */
/* Pad and clock. */

/* Moves the film clock to the start of a shot (wrapping the shot number round the twelve shots). */
static void jump_to_shot(int shot) {
    const int s = (shot % ML_SHOTS + ML_SHOTS) % ML_SHOTS;

    clock_s = ml_shots[s].start_s;                     /* film times are clock times: the title card is the loop's last stretch */
    audio_resync = 1;                                  /* the streams follow the jump */
}

/* Reads the first controller: A pauses / resumes, D-pad right / left jump to the next / previous shot, START shows / hides
 * the readout, X shows / hides the debug caption, B switches the old-film look, Y the lens pass. Edge triggered (a held button acts once). */
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
        audio_resync = 1;                              /* the streams stop, or start again where the clock is */
    }
    if (down & CONT_DPAD_RIGHT) {
        if (cur_shot == ML_SHOTS - 1 && clock_s < ML_FILM_SECONDS) {
            clock_s = ML_FILM_SECONDS;                 /* past the last shot come the end credits */
            audio_resync = 1;
        } else if (clock_s >= ML_TITLE_START_S) {
            jump_to_shot(0);                           /* from the title card: on to the first shot */
        } else {
            jump_to_shot(cur_shot + 1);                /* from the credits: round to the first shot */
        }
    }
    if (down & CONT_DPAD_LEFT) {
        if (clock_s >= ML_TITLE_START_S) {
            clock_s = ML_FILM_SECONDS;                 /* from the title card: back to the start of the credits */
            audio_resync = 1;
        } else {
            jump_to_shot(clock_s >= ML_FILM_SECONDS ? ML_SHOTS - 1 : cur_shot - 1);   /* from the credits: back to the last shot */
        }
    }
    if (down & CONT_START) {
        hud_on ^= 1;
    }
    if (down & CONT_Y) {
        lens_on ^= 1;
    }
    if (down & CONT_X) {
        cap_on ^= 1;
    }
    if (down & CONT_B) {
        film_on ^= 1;
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
    weave_x = 0.0f;
    weave_y = 0.0f;
    if (film_ready && film_on && w->title_s < 0.0f && w->credits_s < 0.0f) {   /* the film weaves in its gate; the title card and credits do not */
        sh4der_film_weave(&film, w->clock, &weave_x, &weave_y);
    }
    {
        float shift[2] = { weave_x, weave_y };           /* the anti-aliasing's sub-pixel offset rides on the same shift */

        lens_step(w, shift);
        weave_x = shift[0];
        weave_y = shift[1];
    }
    spark_screen = sh4der_spark_screen_make(0.5f * (float)ML_SCREEN_W + weave_x, 0.5f * (float)ML_SCREEN_H + weave_y, cam.focal_px);
}

/* Draws the whole picture of one frame: opaque set, snow, then the translucent list. The scene is already begun: the
 * screen's, or with `lens` 1 the lens pass's frame texture (then the picture is put on the screen through lens_resolve and
 * the layers after it are drawn on the screen). */
NOINLINE static void frame_draw(const ml_where_t *w, int lens) {
    float amb[3], flame[3], girl[3], sky[3], overlay_a;
    uint8_t overlay_rgb[3];
    const ml_shot_t *s = &ml_shots[w->shot];
    const float level = ml_match_level(w->shot, w->seconds);
    const float dawn = ml_dawn_blend(w->shot, w->progress);
    unsigned n_snow;

    frame_tris = 0;
    if (w->credits_s >= 0.0f) {                          /* the end credits: the cards are the whole frame, on black */
        pvr_set_bg_color(0.0f, 0.0f, 0.0f);
        pvr_list_begin(PVR_LIST_TR_POLY);
        credits_draw(w->credits_s);
        if (hud_on && header_send(&hdr_hud)) {
            screen_rect(HUD_X, HUD_Y, HUD_X + (float)HUD_W, HUD_Y + (float)HUD_H, Z_HUD, 0xffb0b0b0u, 1);
        }
        ta_meter();
        pvr_list_finish();
        return;
    }
    ml_sky(w->shot, w->progress, sky);
    ml_ambient(w->shot, w->progress, amb);
    ml_girl_pos(w->shot, w->progress, girl);
    ml_flame_pos(w->shot, girl, flame);
    vision_level = level;
    vision_flame[0] = flame[0]; vision_flame[1] = flame[1]; vision_flame[2] = flame[2];
    pvr_set_bg_color(sky[0], sky[1], sky[2]);

    /* Opaque list: the set. */
    pvr_list_begin(PVR_LIST_OP_POLY);
    if (header_send(&hdr_set)) {
        ground_draw(amb, flame, level, s->pool_radius);
        trail_draw(amb, flame, level, s->pool_radius, dawn);
        houses_draw(amb);
        {
            /* her drawn figure: stooped and dipping at every step while she walks (her place itself does not change) */
            const float body[3] = { girl[0], girl[1] - ml_girl_walk_drop(w->shot, w->progress), girl[2] };

            girl_draw(w->shot, body, level);
        }
        props_draw(w->shot, girl);
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

    /* Translucent list, submitted in this order (autosort is off): pool, sprites, hooks, fade, letterbox bars, subtitles, caption, readout. */
    pvr_list_begin(PVR_LIST_TR_POLY);
    if (level > 0.0f && header_send(&hdr_pool)) {
        const float centre[3] = { flame[0], GROUND_Y + POOL_Y, flame[2] };
        const float radius = s->pool_radius;

        pool_fan_draw(centre, radius * POOL_OUTER_SCALE, pack_bytes(s->pool_rgb, 1.0f) & 0x00ffffffu, POOL_ALPHA_OUTER * level);
        pool_fan_draw(centre, radius, pack_bytes(s->pool_rgb, 1.0f) & 0x00ffffffu, POOL_ALPHA_INNER * level);
    }
    spirits_draw(w->shot, w->seconds, girl);
    (void)sprites_draw(w->shot, flame, level, w->clock);
    film_fog_draw(w->shot, w->progress, w->clock);
    film_stars_draw(w->shot, w->progress, w->clock);
    film_vision_draw(w->shot, w->progress, w->clock);
    {
        /* the light that fills the whole picture in M8 and M9: one added rectangle over the 16:9 band */
        const float lift = ml_lift_level(w->shot, w->seconds);

        if (lift > 0.0f && header_send(&hdr_pool)) {
            const uint8_t *c = w->shot == ML_SHOT_M9 ? lift_peak_rgb : s->pool_rgb;
            const float bar = ML_LETTERBOX ? 0.5f * (float)(ML_SCREEN_H - ML_BAND_H) : 0.0f;

            screen_rect(0.0f, bar, (float)ML_SCREEN_W, (float)ML_SCREEN_H - bar, Z_OVERLAY,
                        ((uint32_t)sh4der_fma_(sh4der_sat_(lift), 255.0f, 0.5f) << 24) | ((uint32_t)c[0] << 16) | ((uint32_t)c[1] << 8) | (uint32_t)c[2], 0);
        }
    }
    ml_overlay(w->shot, w->seconds, &overlay_a, overlay_rgb);
    if (w->title_s >= 0.0f) {
        overlay_a = 0.0f;                                /* under the title card the first frame is not faded: the card has its own fade */
    }
    if (overlay_a > 0.0f && header_send(&hdr_fade)) {
        screen_rect(0.0f, 0.0f, (float)ML_SCREEN_W, (float)ML_SCREEN_H, Z_OVERLAY,
                    ((uint32_t)sh4der_fma_(sh4der_sat_(overlay_a), 255.0f, 0.5f) << 24) | ((uint32_t)overlay_rgb[0] << 16) |
                    ((uint32_t)overlay_rgb[1] << 8) | (uint32_t)overlay_rgb[2], 0);
    }
    if (lens) {
        lens_resolve();                                  /* the picture so far goes to the screen through the lens pass */
    }
    if (w->title_s < 0.0f) {
        film_draw(w->clock, 0, w->shot == ML_SHOT_M2B);  /* worn film: over the picture and its fades, under the bars and the subtitles */
    }
    if (ML_LETTERBOX && header_send(&hdr_fade)) {        /* the letterbox: black bars over the rows outside the 16:9 picture */
        const float bar = 0.5f * (float)(ML_SCREEN_H - ML_BAND_H);

        screen_rect(0.0f, 0.0f, (float)ML_SCREEN_W, bar, Z_OVERLAY, 0xff000000u, 0);
        screen_rect(0.0f, (float)ML_SCREEN_H - bar, (float)ML_SCREEN_W, (float)ML_SCREEN_H, Z_OVERLAY, 0xff000000u, 0);
    }
    subs_draw(w->clock);
    if (w->title_s >= 0.0f) {
        title_draw(w->title_s);
        film_draw(w->clock, 1, 0);                       /* the card's own, lighter wear */
    }
    if (cap_on && cap_shot == w->shot && header_send(&hdr_cap)) {
        screen_rect(CAP_X, CAP_Y, CAP_X + (float)CAP_W, CAP_Y + (float)CAP_H, Z_HUD, 0xffe0e0e0u, 1);
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
    PVR_SET(PVR_PT_ALPHA_REF, PT_ALPHA_REF);             /* the falling snow goes down the punch-through list */
    boot_stage(2, 224, 96, 0, "pvr_init");
    pvr_set_bg_color(ML_SKY_NIGHT_R, ML_SKY_NIGHT_G, ML_SKY_NIGHT_B);
    pvr_fog_table_color(1.0f, fog_rgb[0], fog_rgb[1], fog_rgb[2]);   /* fog table 1: night fog; its range is set per shot in the loop */
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
                           sizeof(trail_shade) + sizeof(atlas_px) + sizeof(hud_px) + sizeof(cap_px) + sizeof(snow_draws) + sizeof(mist_noise) + sizeof(mist_px) +
                           sizeof(stream_buf)) / 1024ul,
           (unsigned long)(sizeof(atlas_px) + sizeof(mask_px) + sizeof(hud_px) + sizeof(cap_px) + sizeof(mist_px) + SUB_FONT_BYTES + CREDIT_FONT_BYTES + (TITLE_TILE_A_W + TITLE_TILE_B_W) * TITLE_TILE_H * 2u + SH4DER_LENS_GRAIN_TILE * SH4DER_LENS_GRAIN_TILE * 2u) / 1024ul, (unsigned long)pvr_mem_available() / 1024ul,
           TA_VERTEX_KB, ml_tris_counted(ML_SHOT_M6));
    {
        unsigned n_streams = 0;
        const unsigned n_sfx = sound_setup(&n_streams);

        printf("matchlight: sound: %u of %u effects loaded from /cd/sfx, %u of %u streams (voice.wav, music.wav, fx.wav)\n", n_sfx, (unsigned)ML_SND_COUNT,
               n_streams, STREAMS);
        printf("matchlight: subtitles: %s\n", subs_setup() ? "font.bin and subs.bin loaded" : "none (font.bin or subs.bin missing or malformed)");
        printf("matchlight: credits: %s\n", credits_setup() ? "credits.bin loaded" : "none (credits.bin missing or malformed)");
        printf("matchlight: film look: %s\n", film_setup() ? "ready (B switches it)" : "none (no video memory for the grain tile)");
        printf("matchlight: title card: %s\n", title_setup() ? "title.bin loaded" : "none (title.bin missing or malformed)");
        (void)lens_setup();                              /* last: it takes what video memory the rest left, above a reserve */
        clock_s = title_bands > 0u ? ML_TITLE_START_S : 0.0f;   /* the film opens on its title card when there is one */
    }
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
            const float before = clock_s;

            clock_s = ml_wrap_clock(clock_s + dt);
            if (title_bands == 0u && clock_s >= ML_TITLE_START_S) {
                clock_s = 0.0f;                           /* no title card: its stretch of the loop is skipped */
            }
            snow_time += dt;
            if (clock_s < before) {
                audio_resync = 1;                         /* the film looped: the streams start again */
            }
        }
        ml_locate(clock_s, &w);
        cur_shot = w.shot;
        camera_set(&w);
        footprints_update(w.shot, w.progress);
        sound_update(&w, ml_match_level(w.shot, w.seconds));

        ready = pvr_wait_ready();
        /* KallistiOS times each wait out at 100 ms. Twenty in a row means the PVR never finished a frame: paint white once, log once. */
        if (ready < 0) {
            if (++stuck >= STUCK_WAITS && !stuck_shown) {
                stuck_shown = 1;
                vid_clear(255, 255, 255);
                printf("matchlight: PVR stuck after %lu frames: TA vertex %lu KB (0 on an emulator)\n", total_frames, ta_kb);
            }
            continue;                                     /* the PVR is not ready for a scene: no frame is submitted this time round */
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
        if (fog_shot != w.shot) {                         /* a new shot: its fog range into the fog table (128 entries, once a shot) */
            float fog_start, fog_end;

            ml_fog_range(w.shot, &fog_start, &fog_end);
            pvr_fog_table_linear(fog_start, fog_end);
            fog_shot = w.shot;
        }
        if (cap_on && cap_shot != w.shot) {
            (void)pvr_wait_render_done();                 /* the caption texture is rewritten: the PVR must be done reading it */
            caption_build(w.shot);
            cap_shot = w.shot;
        }
        {
            /* through the lens pass when it is on and its texture's scene begins; else straight to the screen */
            const int lens = lens_active(&w) && sh4der_ca_begin(&lens_gpu, (unsigned)lens_cur) == 0;

            if (!lens) {
                pvr_scene_begin();
            }
            frame_draw(&w, lens);
            pvr_scene_finish();
        }
        total_frames++;
        /* Two serial lines about the TA: after the first frame and after about 10 s. */
        if ((total_frames == 1 && ta_reported == 0) || (total_frames >= 300 && ta_reported == 1)) {
            ta_reported++;
            printf("matchlight: frame %lu at %lu ms: TA vertex %lu of %u KB (0 on an emulator), waits timed out %u, triangles sent %u\n", total_frames,
                   (unsigned long)((timer_us_gettime64() - t_boot) / 1000u), ta_kb, TA_VERTEX_KB, stuck, frame_tris);
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
