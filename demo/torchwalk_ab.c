/* Torch walk: a first-person walk over a big dark floor with the SH4DER-PVR torch, to see the normal map lit by it.
 * The normal map is put on by the highlight power 2 method, with no bump unit: (N.H)^2 from add passes of 8-bit grey textures made from the normal map (three squares,
 * three signed products) onto a flat level kept at half, then the picture multiplied on twice (DESTCOLOR x the source colour = 2 x picture x level): a flat texel stays
 * the picture, a slope facing the half vector goes up to twice it. Power 2 only: a broad highlight. Signed terms (Nx Ny ...) are stored biased (0.5 + 0.5 x value / the
 * map's largest), in a + and a - copy; a cell draws the copy its light's sign asks for and the bias is taken off the flat level, so the sum is exact up to the clamps
 * (the level never below 0, a weight never above 1). The R trigger fires (a point light at the muzzle).
 * Not checked on a real console.
 *
 * Copyright (c) 2026 Jilted Valkyrie. See ../LICENSE.
 *
 * Left stick walks, right stick looks (INVERT Y: stick up looks down). X turns the torch on and off (the prompt fades the first time).
 * D-pad up / down picks a row, left / right / A changes it. Y hides the readout.
 *
 * The floor comes in two forms, to compare them (row FLOOR):
 *   baked  the relief painted into the picture, as the game draws it today; the torch lights it per vertex and the screen
 *          carve + boost shape the beam (the SH4DER torch, nothing else).
 *   clean  the picture without relief, and the relief put back by the normal map (sh4der_bump.h), the light being the torch:
 *          each 0.5 world unit cell gets its own light direction.
 * Row METHOD picks who makes the relief: the normal map's grey passes, TESS VERTEX (the cells in the beam cut small, each vertex lit by the normal map's normal), or
 * TESS EXTRUDE (the same cut, each vertex pushed in by a height map, row EXTRUDE DEPTH, and lit by the slope of the mesh it makes).
 *
 * The textures are made in code (a panel / groove / vent pattern), so the program runs from nothing. If the disc carries TWTEX.BIN
 * (made by a private script from other textures), those are used instead; the file is never part of the source.
 *
 * Costs, per frame, measured by the readout: cpu ms (cells, clipping, light), frame ms, render ms (PVR), VRAM free, texture KB, TA KB.
 * Not checked on a real console: the light's azimuth against the texture's v axis (row AZIMUTH turns it round).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <kos.h>
#include <dc/minifont.h>
#include "sh4der_torch.h"
#include "sh4der_bump.h"
#include "sh4der_guard.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#include "sh4der_lens.h"

/* ---- the screen and the world (world units: 1 wu is about 3 m, as the game) ---- */
#define SCR_W 640.0f
#define SCR_H 480.0f
#define FOCAL 320.0f           /* 90 degrees across the screen */
#define NEAR_Z 0.05f           /* wu */
#define GUARD 1.25f            /* polygons are cut to this share of the screen: the PVR does not like points far outside it */
#define EYE_H 0.55f            /* wu above the floor */
#define CELL 0.5f              /* wu: one floor cell, one bump light */
#define TILE 1.0f              /* wu: one repeat of the floor picture (two cells) */
#define ROOM_HALF 4.0f         /* wu: the room is 8 wu = 24 m square, so a 12 m bump range reaches the middle of the far wall */
#define ROOM_H 1.5f            /* wu: the walls are 4.5 m high, open at the top (the sky) */
#define WALK_RADIUS 0.12f      /* wu: the walking camera stops this far from a wall */
#define MAX_CELLS 512          /* the floor's 16 x 16 and the walls' 4 x 16 x 3 */
#define TESS_VMAX 24000        /* the vertex method's vertices a frame (strips: about 1.1 a triangle; 32 B each = 768 KB of RAM) */
#define CORNERS_MAX 640        /* their corners: 17 x 17 and 4 x 17 x 4 */
#define WALK_SPEED 1.2f        /* wu per second at full stick */
#define TURN_SPEED 2.2f        /* radians per second at full stick */
#define STICK_DEAD 12
#define TESS_KD 1.3f           /* the vertex method: the diffuse change's gain (the torch's own light at the surface is about this) */
#define TESS_KS 2.5f           /* its hot lobe's gain */
#define PITCH_LIMIT 1.25f

/* ---- the look ---- */
static const float AMBIENT[3] = { 0.15f, 0.18f, 0.26f };   /* the faint sky light on the floor (dim, a little blue) */
static const float SKY_ZENITH[3] = { 0.015f, 0.025f, 0.060f };
static const float SKY_HORIZON[3] = { 0.090f, 0.115f, 0.175f };
#define TORCH_NEAR_GAIN 2.0f   /* the library's near add is gentle (0.425 at the eye); the demo takes it twice */
#define TORCH_INNER_RATIO 0.64f     /* the torch's full-strength cone as a share of its edge angle (14 of 22 degrees) */
#define BUMP_SKY_STRENGTH 0.20f     /* the bump with the torch off: the sky light from straight above */
#define WU_M 3.0f                   /* metres in a world unit (the game's: 1 wu is about 3 m); a floor cell is CELL x WU_M = 1.5 m */
#define BUMP_RANGE_FADE 0.75f       /* the bump starts to fade at this share of its range (12 m: from 9 m), to nothing at the range */
#define TORCH_Z 432.0f
#define PROMPT_FADE_S 0.325f

/* ---- the controls ---- */
enum { ROW_BUMP, ROW_TORCH, ROW_FLOOR, ROW_AZIMUTH, ROW_GRAIN, ROW_VIGNETTE, ROW_EDGE, ROW_REACH, ROW_HOT, ROW_BAREA, ROW_BFADE, ROW_BRANGE, ROW_WALLS, ROW_SOFT, ROW_BFILTER, ROW_WIRE, ROW_SCHEME, ROW_INVY, ROW_METHOD, ROW_TLEVELS, ROW_TSPACE, ROW_TCAP, ROW_NSTR, ROW_HSTR, ROW_DEPTH, ROWS };
static const char *const row_name[ROWS] = { "NORMAL MAP", "TORCH", "FLOOR", "AZIMUTH", "GRAIN", "VIGNETTE", "TORCH EDGE", "TORCH REACH", "TORCH CORE", "NORMAL MAP AREA", "NORMAL MAP FADE", "NORMAL MAP RANGE", "WALLS NORMAL MAP", "NORMAL MAP EDGE SOFT", "NORMAL MAP FILTER", "WIREFRAME", "CONTROLS", "INVERT Y", "METHOD", "TESS LEVELS", "MIN SPACING", "TRI CAP", "NORMAL STRENGTH", "HOT STRENGTH", "EXTRUDE DEPTH" };
static const unsigned row_count[ROWS] = { 2, 2, 3, 2, 2, 2, 6, 4, 4, 4, 3, 21, 2, 2, 2, 3, 2, 2, 3, 5, 5, 5, 5, 5, 5 };   /* values a row cycles through */
static const float edge_deg[6] = { 18.0f, 22.0f, 26.0f, 31.0f, 36.0f, 42.0f };   /* the beam's half angle: 22 degrees is a 129 px disc, 31 a 194 px one (1.5 x the radius) */
static const float reach_wu[4] = { 5.0f, 7.0f, 10.0f, 12.0f };                     /* the torch's reach in world units */
static const float hot_scale[4] = { 0.5f, 1.0f, 1.5f, 2.0f };                      /* the hot centre's size */
static const float barea_scale[4] = { 0.75f, 1.0f, 1.25f, 1.5f };                  /* the bump's cone edge as a share of the torch's */
static const float bfade_ratio[3] = { 0.50f, 0.65f, 0.80f };                       /* the bump's full-strength cone as a share of its edge: lower = a longer, softer fade */
static float torch_cos2_outer, torch_cos2_inner, bump_cos2_outer, bump_cos2_inner;  /* set each frame by cones_update */
/* The defaults: FLOOR 1 = clean + normal map (0 baked diffuse, 2 combined), AZIMUTH 1 = the frame flipped (the texel's y runs down the image), torch edge 31 deg (index 3 of edge_deg),
   reach 5 wu, core 200 %, normal map area 150 %, fade 65 %, normal map range 5 m (index 1: 4 + 1 m, in 1 m steps to 24 m), walls on, edge soft on (strength per vertex),
   filter bilinear, METHOD the grey passes, single stick, invert Y on.
        map torch floor azimuth grain vig edge reach core area fade range walls soft filter wire controls invertY method levels spacing cap nstr hstr depth */
static const float tspace_cm[5] = { 4.0f, 5.0f, 9.0f, 15.0f, 20.0f };           /* MIN SPACING: the finest sub-quad edge allowed, in cm */
static const unsigned tcap_v[5] = { 1000u, 2000u, 4000u, 8000u, 12000u };       /* TRI CAP: the most triangles the subdivided cells may hold */
static const float strength_v[5] = { 0.0f, 0.5f, 1.0f, 1.5f, 2.0f };             /* NORMAL STRENGTH and HOT STRENGTH: 0 .. 200 % */
static const float depth_cm[5] = { 0.5f, 1.0f, 2.0f, 3.0f, 5.0f };               /* EXTRUDE DEPTH: from the top of the relief to its lowest point */
static int opt[ROWS] = {  1,   0,    1,       1,     1,   1,  3,   0,    3,   3,   1,    1,    1,   1,     1,   0,   0,   1,   0,   4,   0,   3,   2,   2,   2 };
static int row_pick = ROW_BUMP, hud_show = 0;   /* hud_show: the menu (the rows and the readout) is open; it starts closed */

/* ---- the textures ---- */
#define BASE_SIDE 256
#define HUD_H 128   /* readout texture height: eight 9 px lines of the 5 x 7 font */
#define BUMP_SIDE 64
#define PRIVATE_FILE "/cd/TWTEX.BIN"
typedef struct { pvr_ptr_t mem; uint32_t fmt, side, mips, kb, bytes; } tex_t;   /* bytes: what was allocated */
static tex_t tex_baked, tex_clean, tex_wall;   /* the walls: a base of their own; the normal maps are not textures: only their normals are kept, in RAM */
static uint16_t pic_buf[BASE_SIDE * BASE_SIDE] __attribute__((aligned(32)));
static uint16_t bump_buf[BUMP_SIDE * BUMP_SIDE] __attribute__((aligned(32)));
static uint8_t height_big[BASE_SIDE * BASE_SIDE];
static uint8_t height_small[BUMP_SIDE * BUMP_SIDE];
static unsigned tex_kb_total;
static int private_textures;
/* The normal maps' normals kept in RAM (the grey maps and the vertex method read them): int8 x, y, z (x 127) per texel, row-major, the map's own side (128 x 128 = 48 KB, 64 x 64 = 12 KB). */
static int8_t nrm_floor[3 * 128 * 128], nrm_wall[3 * 128 * 128];
static unsigned nrm_floor_side, nrm_wall_side;
/* The height maps for TESS EXTRUDE: one byte a texel, row-major from the top, 255 = the top of the relief. The placeholder's are the fields its pictures were made
   from; the private file carries its own (kinds 6 and 7), or none (side 0: no push, the surface stays flat). 16 KB each at 128 x 128. */
static uint8_t hgt_floor[128 * 128], hgt_wall[128 * 128];
static unsigned hgt_floor_side, hgt_wall_side;
static float hgt_floor_ref = 1.0f, hgt_wall_ref = 1.0f;   /* the level that stays on the surface (0..1): the map's median, the plate most of it lies on */

static float height_median(const uint8_t *h, unsigned side) {
    unsigned hist[256], i, sum = 0;
    if (!side)
        return 1.0f;
    memset(hist, 0, sizeof(hist));
    for (i = 0; i < side * side; i++)
        hist[h[i]]++;
    for (i = 0; i < 256u; i++) {
        sum += hist[i];
        if (2u * sum >= side * side)
            break;
    }
    return (float)i * (1.0f / 255.0f);
}
/* VRAM by owner, in bytes, counted at allocation: the effect is the grey maps (nine a surface), the torch's (none: vertex colours only) and the lens's grain tile; the base is the floor pictures and the text. */
static unsigned vram_gmap_b, vram_torch_b, vram_lens_b, vram_floor_b, vram_wall_b, vram_text_b;

/* ---- the headers ---- */
static pvr_poly_hdr_t hdr_sky, hdr_op_baked, hdr_op_clean, hdr_op_wall, hdr_torch[2], hdr_hud, hdr_prompt, hdr_fprompt, hdr_wire;   /* hdr_op_*: the opaque base pictures; the walls' own: their texture */
static pvr_ptr_t hud_mem, prompt_mem, fprompt_mem;
static uint16_t hud_pixels[512 * HUD_H] __attribute__((aligned(32)));
static uint16_t prompt_pixels[256 * 8] __attribute__((aligned(32)));   /* drawn 2x on screen: whole texels, no filter */
static uint16_t fprompt_pixels[256 * 8] __attribute__((aligned(32)));   /* the floor-mode prompt: always shown, rebuilt only when the mode changes */
static int fprompt_shown = -1;

/* ---- the camera and the torch ---- */
static float cam[3] = { 0.0f, EYE_H, -3.2f }, cam_yaw, cam_pitch;   /* in the room, near one wall, looking across it */
static float fwd[3], right[3], up[3];
static float torch_power;
static int prompt_state;     /* 0 shown, 1 fading, 2 gone for good */
static float prompt_t;

#define AB_PASS_MAX 6          /* the most grey passes a cell takes: three squares, three products */
/* ---- one floor cell, ready to send ---- */
typedef struct {
    sh4der_bump_vert_t v[9];   /* a quad cut by five planes has up to nine corners */
    uint8_t n;
    uint8_t bump;              /* the normal map is drawn on this cell */
    uint8_t wall;              /* 1: a wall cell (the wall's picture and normal map), 0: the floor's */
    float sv[9];               /* the strength at each corner */
    float ab_w[AB_PASS_MAX];   /* each grey pass's weight at full strength */
    float ab_k;                /* what the flat level loses at full strength: the level is 1 - strength x ab_k */
    uint8_t ab_t[AB_PASS_MAX]; /* the grey map each pass draws (G_*) */
    uint8_t ab_n;              /* the passes */
} cell_t;
static cell_t cells[MAX_CELLS];
static unsigned cell_count;
static unsigned frame_tris;
static unsigned tess_cells_n, tess_prims, tess_tris, tess_lv_used, tess_vb;   /* the vertex method this frame: cells subdivided, pieces, triangles, the level it settled on, vertex bytes */
static float tess_ms, tplan_ms, tshade_ms, temit_ms, tsend_ms;   /* the whole build, and its parts: levels, the vertices (place + light), strips / clip, and the send to the PVR */
/* The vertex method's pieces, ready to send: screen vertices in strips (the last of a strip carries the end flag), one run per subdivided cell (its header). */
typedef struct { uint32_t first, count; uint8_t wall; } tess_run_t;
static pvr_vertex_t tess_vbuf[TESS_VMAX] __attribute__((aligned(32)));
static tess_run_t tess_run[MAX_CELLS];
static unsigned tess_vn, tess_runs, tess_drop;   /* tess_drop: pieces left out because the buffer was full */
static unsigned wire_bytes, wire_kb;   /* the wireframe's TA data: counted apart, never in the effect's numbers */
static uint32_t ta_bytes;          /* what this frame sent to the TA: every header 32 B and every vertex 32 B, counted where it is sent (exact for ours; the lens's from its constants) */
static unsigned ta_counted_kb;     /* the last frame's, in KB */
static int ab_state;               /* START: 0 idle, 1 measuring with the effect off, 2 with it on */
static float ab_t;
static int ab_saved_bump, ab_saved_torch, ab_reset_request;

/* ---- the normal map: the grey maps, the gunfire ---- */
/* The grey maps of one surface (8-bit paletted, the normal map's own side): the squares Nx^2, Ny^2, Nz^2; the products NxNy, NxNz, NyNz biased, a + and a - copy. */
enum { G_XX, G_YY, G_ZZ, G_XYP, G_XYN, G_XZP, G_XZN, G_YZP, G_YZN, G_COUNT };
enum { M_X, M_Y, M_XX, M_YY, M_XY, M_XZ, M_YZ, M_COUNT };   /* what each map's values were divided by (the largest in the map), so a map uses its whole range */
#define AB_PAL_BANK 0u         /* the grey ramp's palette bank (nothing else in the demo is paletted) */
#define AB_SPEC 1.0f           /* the highlight's gain */
#define GUN_RADIUS 2.0f        /* wu: the muzzle light reaches 6 m */
#define GUN_GAIN 1.1f
static const float GUN_TINT[3] = { 1.0f, 0.72f, 0.35f };
static const float GUN_VIEW[3] = { 0.10f, -0.08f, 0.25f };   /* the muzzle in view space (right, up, forward), wu: 30 cm right, 24 cm down, 75 cm ahead */
static int gun_held;
static float gun_power, gun_pos[3];
static pvr_ptr_t g_mem[2][G_COUNT];        /* [0 the floor, 1 the walls] */
static unsigned g_side[2], g_bytes[2];     /* the maps' side (0: none, the surface is drawn flat) and the bytes of one map */
static float g_m[2][M_COUNT];
static uint8_t g_buf[128 * 128] __attribute__((aligned(32)));
static pvr_poly_hdr_t hdr_g_add[2][G_COUNT][2], hdr_apply2[3], hdr_ablabel;   /* [surface][map][0 nearest, 1 bilinear]; apply2: floor clean, floor baked, walls */
static pvr_ptr_t ab_label_mem;
static uint16_t ab_label_px[256 * 64] __attribute__((aligned(32)));
static unsigned ab_lit;                    /* cells that took the normal map this frame */
static uint32_t ab_ta;                     /* what the normal map's translucent passes sent to the TA this frame, bytes */

/* ---- hand-made maths: pack a float colour (SH4ZAM has no packer) ---- */
static uint32_t pack_argb(float r, float g, float b) {
    return 0xff000000u | ((uint32_t)(shz_saturatef(r) * 255.0f + 0.5f) << 16) | ((uint32_t)(shz_saturatef(g) * 255.0f + 0.5f) << 8) |
           (uint32_t)(shz_saturatef(b) * 255.0f + 0.5f);
}

static uint16_t pack565(float r, float g, float b) {
    return (uint16_t)(((uint32_t)(shz_saturatef(r) * 31.0f + 0.5f) << 11) | ((uint32_t)(shz_saturatef(g) * 63.0f + 0.5f) << 5) |
                      (uint32_t)(shz_saturatef(b) * 31.0f + 0.5f));
}

/* ------------------------------------------------------------------------ */
/* The placeholder floor: octagon panels 128 texels across on a 256 texel tile, a groove round each, vent slits in the panel, a rib. Every
   feature is at least 4 texels, so it survives the 64 x 64 bump map. Public-safe: made here, from nothing. */
static uint8_t pattern_height(unsigned x, unsigned y) {
    const int cx = (int)(x & 127u) - 64, cy = (int)(y & 127u) - 64;
    const int ax = cx < 0 ? -cx : cx, ay = cy < 0 ? -cy : cy;
    const int cheb = ax > ay ? ax : ay, diag = (ax + ay) * 7 / 10;
    const int d = cheb > diag ? cheb : diag;
    const int odd = (int)(((x >> 7) ^ (y >> 7)) & 1u);
    int h = 200;
    if (d >= 50 && d < 58)
        h = 90;                                                  /* the groove round the panel */
    else if (d >= 58)
        h = 185;                                                 /* the seam between panels, a little low */
    else if (!odd && ay < 24 && ax < 28 && ((ay + 6) % 12) < 6)
        h = 100;                                                 /* horizontal vent slits */
    else if (odd && ax < 24 && ay < 28 && ((ax + 6) % 12) < 6)
        h = 100;                                                 /* vertical vent slits */
    else if (ax < 4 && ay >= 28 && ay < 50)
        h = 235;                                                 /* a raised rib on the vertical axis */
    return (uint8_t)h;
}

static float hash01(unsigned x, unsigned y) {
    unsigned h = x * 73856093u ^ y * 19349663u;
    h ^= h >> 13;
    h *= 0x5bd1e995u;
    return (float)(h & 255u) * (1.0f / 255.0f);
}

/* The two pictures from one height map: clean (no relief, a little grain) and baked (the relief shaded in from the top left). */
static void make_pictures(uint16_t *clean, uint16_t *baked) {
    unsigned x, y;
    for (y = 0; y < BASE_SIDE; y++)
        for (x = 0; x < BASE_SIDE; x++)
            height_big[y * BASE_SIDE + x] = pattern_height(x, y);
    for (y = 0; y < BASE_SIDE; y++) {
        for (x = 0; x < BASE_SIDE; x++) {
            const float n = 0.90f + 0.10f * hash01(x, y);
            const float r = 0.50f * n, g = 0.54f * n, b = 0.60f * n;
            const unsigned xm = (x - 1u) & (BASE_SIDE - 1u), xp = (x + 1u) & (BASE_SIDE - 1u), ym = (y - 1u) & (BASE_SIDE - 1u), yp = (y + 1u) & (BASE_SIDE - 1u);
            const float emboss = shz_clampf(((float)height_big[ym * BASE_SIDE + xm] - (float)height_big[yp * BASE_SIDE + xp]) * (1.0f / 255.0f) * 2.5f, -0.6f, 0.6f);
            const float ao = height_big[y * BASE_SIDE + x] < 150 ? 0.70f : 1.0f;
            const float k = (1.0f + emboss) * ao;
            const unsigned t = sh4der_bump_twiddle(x, y);
            clean[t] = pack565(r, g, b);
            baked[t] = pack565(r * k, g * k, b * k);
        }
    }
}

static void make_bump(uint16_t *out) {
    unsigned x, y;
    for (y = 0; y < BUMP_SIDE; y++) {
        for (x = 0; x < BUMP_SIDE; x++) {
            unsigned sum = 0, i, j;
            for (j = 0; j < 4; j++)
                for (i = 0; i < 4; i++)
                    sum += height_big[(y * 4 + j) * BASE_SIDE + x * 4 + i];
            height_small[y * BUMP_SIDE + x] = (uint8_t)(sum >> 4);
        }
    }
    sh4der_bump_from_height(out, height_small, BUMP_SIDE, 2.5f);
}

/* ------------------------------------------------------------------------ */
/* The private textures: TWTEX.BIN on the disc, if it is there. Header (all 32-bit little endian words): 'TWTX', version 1, count;
   then count entries of 8 words: kind (0 baked, 1 clean, 2 normal map), side, fmt (0 ARGB1555, 1 RGB565, 2 ARGB4444, 3 bump), flags (1 twiddled,
   2 VQ, 4 mips), offset, size, 0, 0. Data at the offsets, 32-byte aligned. */
/* Decode a bump texture (twiddled KallistiOS texels: S = slope byte xor 0xff, R = direction byte) into normals. Texel x along +u, y up the image, z out of the surface. */
static void nrm_decode(int8_t *out, unsigned *side_out, const uint16_t *tw, unsigned side) {
    sh4der_bump_normal_decode(out, tw, side);   /* the helper's: rounded, row-major, 3 bytes a texel */
    *side_out = side;
}

static int load_private(void) {
    FILE *f = fopen(PRIVATE_FILE, "rb");
    uint32_t head[3];
    uint32_t entry[8];
    unsigned i, got = 0;
    if (!f)
        return 0;
    if (fread(head, sizeof(head), 1, f) != 1 || head[0] != 0x58545754u || head[1] != 1u || head[2] > 8u) {
        fclose(f);
        return 0;
    }
    for (i = 0; i < head[2]; i++) {
        long here;
        void *data;
        tex_t *t;
        uint32_t fmt;
        if (fseek(f, (long)(12u + 32u * i), SEEK_SET) != 0 || fread(entry, sizeof(entry), 1, f) != 1)
            break;
        if (entry[0] == 6u || entry[0] == 7u) {   /* a height map (8 bit, not a texture): the floor's, the walls' */
            uint8_t *dst = entry[0] == 6u ? hgt_floor : hgt_wall;
            const uint32_t px = entry[1] * entry[1];
            if (entry[1] >= 8u && entry[1] <= 128u && (entry[1] & (entry[1] - 1u)) == 0u && entry[5] >= px && fseek(f, (long)entry[4], SEEK_SET) == 0 &&
                fread(dst, 1, px, f) == px)
                *(entry[0] == 6u ? &hgt_floor_side : &hgt_wall_side) = entry[1];
            continue;
        }
        if (entry[0] == 2u || entry[0] == 4u || entry[0] == 5u) {   /* a normal map (the PVR bump texel format): only its normals are kept, in RAM: no video memory. Kind 5, the game's own 64 x 64, is not used */
            if (entry[0] != 5u && entry[1] >= 8u && entry[1] <= 128u && (entry[1] & (entry[1] - 1u)) == 0u && entry[5] >= entry[1] * entry[1] * 2u && entry[5] <= (1u << 20)) {
                void *nd = memalign(32, (entry[5] + 31u) & ~31u);   /* memalign(32): a uint16_t view is aligned */
                if (nd && fseek(f, (long)entry[4], SEEK_SET) == 0 && fread(nd, 1, entry[5], f) == entry[5]) {
                    nrm_decode(entry[0] == 2u ? nrm_floor : nrm_wall, entry[0] == 2u ? &nrm_floor_side : &nrm_wall_side, (const uint16_t *)nd, entry[1]);
                    got++;
                }
                free(nd);
            }
            continue;
        }
        t = entry[0] == 0 ? &tex_baked : entry[0] == 1 ? &tex_clean : entry[0] == 3 ? &tex_wall : NULL;
        if (!t || entry[5] == 0 || entry[5] > (1u << 20) || entry[1] < 8u || entry[1] > 1024u || (entry[1] & (entry[1] - 1u)) != 0u)
            continue;   /* an unknown kind, no data, or a side the PVR cannot take (a bad TWTEX.BIN must not become a bad header) */
        data = memalign(32, (entry[5] + 31u) & ~31u);
        here = (long)entry[4];
        if (!data || fseek(f, here, SEEK_SET) != 0 || fread(data, 1, entry[5], f) != entry[5]) {
            free(data);
            break;
        }
        t->mem = pvr_mem_malloc((entry[5] + 31u) & ~31u);
        if (t->mem) {
            static const uint32_t colour[3] = { PVR_TXRFMT_ARGB1555, PVR_TXRFMT_RGB565, PVR_TXRFMT_ARGB4444 };
            pvr_txr_load(data, t->mem, (entry[5] + 31u) & ~31u);
            fmt = colour[entry[2] > 2 ? 2 : entry[2]];
            fmt |= (entry[3] & 1u) ? PVR_TXRFMT_TWIDDLED : PVR_TXRFMT_NONTWIDDLED;
            if (entry[3] & 2u)
                fmt |= PVR_TXRFMT_VQ_ENABLE;
            t->fmt = fmt;
            t->side = entry[1];
            t->mips = (entry[3] & 4u) ? 1u : 0u;
            t->kb = (entry[5] + 1023u) / 1024u;
            t->bytes = (entry[5] + 31u) & ~31u;
            tex_kb_total += t->kb;
            got++;
        }
        free(data);
    }
    fclose(f);
    (void)got;
    return tex_baked.mem && tex_clean.mem && nrm_floor_side;   /* the floor's two pictures and its normal map; the walls' two are optional */
}

static void upload_placeholder(tex_t *t, const uint16_t *data, unsigned bytes, uint32_t fmt, unsigned side) {
    t->mem = pvr_mem_malloc(bytes);
    if (!t->mem) {
        printf("torchwalk: texture allocation FAIL (%u B)\n", bytes);
        arch_exit();
    }
    pvr_txr_load(data, t->mem, bytes);
    t->fmt = fmt;
    t->side = side;
    t->mips = 0;
    t->kb = bytes / 1024u;
    t->bytes = bytes;
    tex_kb_total += t->kb;
}

/* The placeholder wall: an original procedural sci-fi industrial wall, made in code. The 128 x 128 tile is cut into rectangular panels of varied sizes (the cuts
   run to the tile's edge, so it tiles), each raised or recessed to one of four levels with a 2 texel seam round it; some panels carry vents (slits), some a pipe.
   Hard edged, grey-blue metal with a small tone change per panel. The base and the normal map come from the same height field; the base is 128 x 128 RGB565 = 32 KB, the normal map stays in RAM. */
#define WALL_SIDE 128
static uint8_t wall_h[WALL_SIDE * WALL_SIDE], wall_tone[WALL_SIDE * WALL_SIDE];
static uint16_t wall_bump_buf[WALL_SIDE * WALL_SIDE] __attribute__((aligned(32)));

static unsigned greeble_hash(unsigned a, unsigned b) {
    unsigned h = a * 2654435761u ^ b * 40503u;
    h ^= h >> 15;
    h *= 2246822519u;
    h ^= h >> 13;
    return h;
}

static void greeble_panel(unsigned x0, unsigned y0, unsigned w, unsigned h, unsigned seed) {
    static const uint8_t levels[4] = { 105, 150, 195, 235 };   /* recessed to raised */
    const uint8_t level = levels[seed & 3u];
    const unsigned tone = 110u + ((seed >> 3) & 63u);
    const int vents = ((seed >> 8) & 3u) == 0u && w >= 16u && h >= 14u;
    const int pipe = ((seed >> 10) & 3u) == 1u && h >= 12u;
    unsigned x, y;
    for (y = 0; y < h; y++) {
        for (x = 0; x < w; x++) {
            const int seam = x < 2u || y < 2u || x + 2u >= w || y + 2u >= h;
            const unsigned i = (y0 + y) * WALL_SIDE + x0 + x;
            int hv = seam ? 60 : (int)level;
            if (!seam && vents && x >= 4u && x + 4u < w && y >= 4u && y + 4u < h && ((y - 4u) % 4u) < 2u)
                hv -= 55;                                                           /* a vent slit */
            if (!seam && pipe && y + 3u >= h / 2u && y < h / 2u + 3u) {
                const unsigned d = y > h / 2u ? y - h / 2u : h / 2u - y;           /* a pipe: a round ridge across the panel */
                hv = 245 - (int)d * 28;
            }
            wall_h[i] = (uint8_t)(hv < 0 ? 0 : hv > 255 ? 255 : hv);
            wall_tone[i] = (uint8_t)(seam ? tone / 2u : tone);
        }
    }
}

static void greeble_split(unsigned x0, unsigned y0, unsigned w, unsigned h, unsigned depth, unsigned seed) {
    const unsigned r = greeble_hash(seed, depth * 977u + x0 * 31u + y0 * 7u);
    if (depth < 4u && w >= 24u && h >= 24u && (depth < 2u || (r & 3u) != 0u)) {   /* split the longer side, between 35 % and 65 % */
        if (w >= h) {
            const unsigned w1 = w * (35u + (r >> 8) % 31u) / 100u;
            greeble_split(x0, y0, w1, h, depth + 1u, seed);
            greeble_split(x0 + w1, y0, w - w1, h, depth + 1u, seed);
        } else {
            const unsigned h1 = h * (35u + (r >> 8) % 31u) / 100u;
            greeble_split(x0, y0, w, h1, depth + 1u, seed);
            greeble_split(x0, y0 + h1, w, h - h1, depth + 1u, seed);
        }
    } else {
        greeble_panel(x0, y0, w, h, r);
    }
}

static void make_wall_placeholders(void) {
    unsigned x, y;
    if (tex_wall.mem) pvr_mem_free(tex_wall.mem);   /* a half-loaded private pair is dropped: both or neither */
    memset(&tex_wall, 0, sizeof(tex_wall));
    greeble_split(0, 0, WALL_SIDE, WALL_SIDE, 0, 0x51a7u);
    for (y = 0; y < WALL_SIDE; y++) {
        for (x = 0; x < WALL_SIDE; x++) {
            const float t = (float)wall_tone[y * WALL_SIDE + x] * (1.0f / 255.0f), n = 0.94f + 0.06f * hash01(x, y);   /* grey-blue, a little grain */
            pic_buf[sh4der_bump_twiddle(x, y)] = pack565(0.62f * t * n, 0.70f * t * n, 0.82f * t * n);
        }
    }
    upload_placeholder(&tex_wall, pic_buf, WALL_SIDE * WALL_SIDE * 2u, PVR_TXRFMT_RGB565 | PVR_TXRFMT_TWIDDLED, WALL_SIDE);
    sh4der_bump_from_height(wall_bump_buf, wall_h, WALL_SIDE, 3.0f);
    nrm_decode(nrm_wall, &nrm_wall_side, wall_bump_buf, WALL_SIDE);
    memcpy(hgt_wall, wall_h, sizeof(wall_h));
    hgt_wall_side = WALL_SIDE;
}

static void make_textures(void) {
    static uint16_t clean[BASE_SIDE * BASE_SIDE] __attribute__((aligned(32)));
    if (load_private()) {
        private_textures = 1;
        if (!tex_wall.mem || !nrm_wall_side)
            make_wall_placeholders();   /* the private file has no wall textures: the placeholder walls */
        return;
    }
    /* nothing partly loaded: free what the private path made and use the placeholder */
    if (tex_baked.mem) pvr_mem_free(tex_baked.mem);
    if (tex_clean.mem) pvr_mem_free(tex_clean.mem);
    memset(&tex_baked, 0, sizeof(tex_baked));
    memset(&tex_clean, 0, sizeof(tex_clean));
    tex_kb_total = 0;
    make_pictures(clean, pic_buf);
    upload_placeholder(&tex_baked, pic_buf, sizeof(pic_buf), PVR_TXRFMT_RGB565 | PVR_TXRFMT_TWIDDLED, BASE_SIDE);
    upload_placeholder(&tex_clean, clean, sizeof(clean), PVR_TXRFMT_RGB565 | PVR_TXRFMT_TWIDDLED, BASE_SIDE);
    make_bump(bump_buf);   /* the normal map, made in RAM: its normals are all that is kept */
    nrm_decode(nrm_floor, &nrm_floor_side, bump_buf, BUMP_SIDE);
    memcpy(hgt_floor, height_small, sizeof(height_small));
    hgt_floor_side = BUMP_SIDE;
    make_wall_placeholders();
}

/* ------------------------------------------------------------------------ */
/* The grey maps: made once from the normals the normal maps decode to (nrm_floor, nrm_wall), nine a surface, each side x side bytes of video memory (64 x 64 = 4 KB,
   128 x 128 = 16 KB). Byte work only. */
static float ab_map_value(unsigned g, const float n[3], const float *m) {
    switch (g) {
    case G_XX: return n[0] * n[0] / m[M_XX];
    case G_YY: return n[1] * n[1] / m[M_YY];
    case G_ZZ: return n[2] * n[2];
    case G_XYP: return 0.5f + 0.5f * n[0] * n[1] / m[M_XY];
    case G_XYN: return 0.5f - 0.5f * n[0] * n[1] / m[M_XY];
    case G_XZP: return 0.5f + 0.5f * n[0] * n[2] / m[M_XZ];
    case G_XZN: return 0.5f - 0.5f * n[0] * n[2] / m[M_XZ];
    case G_YZP: return 0.5f + 0.5f * n[1] * n[2] / m[M_YZ];
    default: return 0.5f - 0.5f * n[1] * n[2] / m[M_YZ];
    }
}

static void ab_make_maps(void) {
    unsigned s, g, x, y, k;
    sh4der_bump_grey_palette(AB_PAL_BANK);   /* the grey ramp (RGB565: 32 to 64 levels) */
    for (s = 0; s < 2u; s++) {
        const int8_t *nrm = s ? nrm_wall : nrm_floor;
        const unsigned side = s ? nrm_wall_side : nrm_floor_side;
        float *m = g_m[s];
        if (side < 8u || side > 128u)
            continue;   /* no normals kept for this surface: it is drawn as the plain picture */
        for (k = 0; k < M_COUNT; k++)
            m[k] = 0.02f;   /* a flat map: no division by nothing */
        for (k = 0; k < side * side; k++) {
            const float n[3] = { (float)nrm[3u * k] * (1.0f / 127.0f), (float)nrm[3u * k + 1u] * (1.0f / 127.0f), (float)nrm[3u * k + 2u] * (1.0f / 127.0f) };
            m[M_X] = shz_fmaxf(m[M_X], shz_fabsf(n[0]));
            m[M_Y] = shz_fmaxf(m[M_Y], shz_fabsf(n[1]));
            m[M_XY] = shz_fmaxf(m[M_XY], shz_fabsf(n[0] * n[1]));
            m[M_XZ] = shz_fmaxf(m[M_XZ], shz_fabsf(n[0] * n[2]));
            m[M_YZ] = shz_fmaxf(m[M_YZ], shz_fabsf(n[1] * n[2]));
        }
        m[M_XX] = m[M_X] * m[M_X];
        m[M_YY] = m[M_Y] * m[M_Y];
        for (g = 0; g < G_COUNT; g++) {
            g_mem[s][g] = pvr_mem_malloc(side * side);
            if (!g_mem[s][g])
                break;
            for (y = 0; y < side; y++) {
                for (x = 0; x < side; x++) {
                    const int8_t *p = nrm + 3u * (y * side + x);
                    const float n[3] = { (float)p[0] * (1.0f / 127.0f), (float)p[1] * (1.0f / 127.0f), (float)p[2] * (1.0f / 127.0f) };
                    g_buf[sh4der_bump_twiddle(x, y)] = (uint8_t)sh4der_bump_byte_(ab_map_value(g, n, m) * 255.0f);
                }
            }
            pvr_txr_load(g_buf, g_mem[s][g], side * side);
        }
        if (g < G_COUNT) {
            printf("torchwalk: grey maps: no video memory for surface %u (map %u): it is drawn flat\n", s, g);
            continue;
        }
        g_side[s] = side;
        g_bytes[s] = side * side;
        printf("torchwalk: grey maps surface %u: %u x %u, %u maps x %u B; largest x1000: nx %d ny %d nxny %d nxnz %d nynz %d\n", s, side, side, (unsigned)G_COUNT, side * side,
               (int)(m[M_X] * 1000.0f), (int)(m[M_Y] * 1000.0f), (int)(m[M_XY] * 1000.0f), (int)(m[M_XZ] * 1000.0f), (int)(m[M_YZ] * 1000.0f));
    }
}

/* A grey map's header: translucent list, MODULATE (the vertex colour is the pass's weight), blend src x ONE, lying on the opaque cell under it (no depth write, GEQUAL). */
static void ab_header(pvr_poly_hdr_t *hdr, pvr_ptr_t map, unsigned side, int filter, int src) {
    pvr_poly_cxt_t c;
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_PAL8BPP | PVR_TXRFMT_8BPP_PAL(AB_PAL_BANK) | PVR_TXRFMT_TWIDDLED, side, side, map, filter);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    c.txr.env = PVR_TXRENV_MODULATE;
    c.blend.src = src;
    c.blend.dst = PVR_BLEND_ONE;
    pvr_poly_compile(hdr, &c);
}

/* C's last pass: the picture on the translucent list, DESTCOLOR x (source colour): picture x level + level x picture, twice the product (the level is kept at half). */
static void ab_header_apply2(pvr_poly_hdr_t *hdr, const tex_t *t) {
    pvr_poly_cxt_t c;
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, t->fmt, t->side, t->side, t->mem, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_GEQUAL;
    c.txr.env = PVR_TXRENV_MODULATE;
    c.txr.mipmap = t->mips ? PVR_MIPMAP_ENABLE : PVR_MIPMAP_DISABLE;
    c.blend.src = PVR_BLEND_DESTCOLOR;
    c.blend.dst = PVR_BLEND_DESTCOLOR;   /* the hardware's "other colour": as a destination factor it is the SOURCE colour */
    pvr_poly_compile(hdr, &c);
}

static void ab_make_headers(void) {
    pvr_poly_cxt_t c;
    unsigned s, g, f;
    for (s = 0; s < 2u; s++) {
        if (!g_side[s])
            continue;
        for (f = 0; f < 2u; f++) {
            for (g = 0; g < G_COUNT; g++)
                ab_header(&hdr_g_add[s][g][f], g_mem[s][g], g_side[s], f ? PVR_FILTER_BILINEAR : PVR_FILTER_NEAREST, PVR_BLEND_ONE);
        }
    }
    ab_header_apply2(&hdr_apply2[0], &tex_clean);
    ab_header_apply2(&hdr_apply2[1], &tex_baked);
    ab_header_apply2(&hdr_apply2[2], &tex_wall);
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, 256, 64, ab_label_mem, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&hdr_ablabel, &c);
}

/* ------------------------------------------------------------------------ */
static void make_headers(void) {
    pvr_poly_cxt_t c;
    /* the sky: untextured, opaque, behind everything */
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    pvr_poly_compile(&hdr_sky, &c);
    /* the floor's base pictures, opaque, modulated by the vertex light */
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, tex_baked.fmt, tex_baked.side, tex_baked.side, tex_baked.mem, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.txr.env = PVR_TXRENV_MODULATE;
    c.txr.mipmap = tex_baked.mips ? PVR_MIPMAP_ENABLE : PVR_MIPMAP_DISABLE;
    pvr_poly_compile(&hdr_op_baked, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, tex_clean.fmt, tex_clean.side, tex_clean.side, tex_clean.mem, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.txr.env = PVR_TXRENV_MODULATE;
    c.txr.mipmap = tex_clean.mips ? PVR_MIPMAP_ENABLE : PVR_MIPMAP_DISABLE;
    pvr_poly_compile(&hdr_op_clean, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, tex_wall.fmt, tex_wall.side, tex_wall.side, tex_wall.mem, PVR_FILTER_BILINEAR);   /* the walls' base */
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.txr.env = PVR_TXRENV_MODULATE;
    c.txr.mipmap = tex_wall.mips ? PVR_MIPMAP_ENABLE : PVR_MIPMAP_DISABLE;
    pvr_poly_compile(&hdr_op_wall, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, 256, 8, fprompt_mem, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&hdr_fprompt, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);   /* the wireframe: opaque thin quads, drawn over the scene (no depth: an edge lies in its surface's plane and would fight it) */
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&hdr_wire, &c);
    {   /* the torch makes headers only; measured, not assumed */
        const unsigned long m0 = (unsigned long)pvr_mem_available();
        sh4der_torch_headers(hdr_torch);
        vram_torch_b = (unsigned)(m0 - (unsigned long)pvr_mem_available());
    }
    /* the readout and the prompt: alpha text on the translucent list, over everything */
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, 512, HUD_H, hud_mem, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&hdr_hud, &c);
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, 256, 8, prompt_mem, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.fog_type = PVR_FOG_DISABLE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&hdr_prompt, &c);
    ab_make_headers();
}

/* ------------------------------------------------------------------------ */
/* The camera: yaw round the up axis, pitch up positive. The world is y up, the floor y = 0. */
static void camera_axes(void) {
    float sy, cy, sp, cp;
    sh4der_sincos_(cam_yaw, &sy, &cy);
    sh4der_sincos_(cam_pitch, &sp, &cp);
    fwd[0] = cp * sy; fwd[1] = sp; fwd[2] = cp * cy;
    right[0] = cy; right[1] = 0.0f; right[2] = -sy;
    up[0] = -sp * sy; up[1] = cp; up[2] = -sp * cy;   /* forward x right, written out */
}

typedef struct { float p[3], u, v, r, g, b; uint8_t e; } cv_t;   /* e: which original cell edge runs from this corner to the next (0..3), 255 = an edge the cut made */   /* a corner in view space: position (x right, y up, z forward), picture coordinates, light */
#define CV_MAX 12

/* 1 when two corners are not the same point (to 1e-6 wu). */
static int cv_apart(const cv_t *a, const cv_t *b) {
    return shz_fabsf(a->p[0] - b->p[0]) + shz_fabsf(a->p[1] - b->p[1]) + shz_fabsf(a->p[2] - b->p[2]) > 1.0e-6f;
}

/* Keep the part of the polygon where plane . p - e > 0. A corner exactly on the plane counts as out, and a corner the cut
   would put twice at the same place is kept once: the PVR draws a triangle with no area as a line. */
static unsigned clip_poly(const cv_t *in, unsigned n, cv_t *out, const float plane[3], float e) {
    unsigned i, m = 0;
    for (i = 0; i < n; i++) {
        const cv_t *p = &in[i], *q = &in[(i + 1u) % n];
        const float dp = sh4der_dot3_(plane, p->p) - e, dq = sh4der_dot3_(plane, q->p) - e;
        if (dp > 0.0f && m < CV_MAX)
            out[m++] = *p;
        if ((dp > 0.0f) != (dq > 0.0f) && m < CV_MAX) {
            const float t = sh4der_div_(dp, dp - dq);
            cv_t *o = &out[m];
            unsigned k;
            for (k = 0; k < 3; k++)
                o->p[k] = shz_lerpf(p->p[k], q->p[k], t);
            o->u = shz_lerpf(p->u, q->u, t); o->v = shz_lerpf(p->v, q->v, t);
            o->r = shz_lerpf(p->r, q->r, t); o->g = shz_lerpf(p->g, q->g, t); o->b = shz_lerpf(p->b, q->b, t);
            o->e = dp > 0.0f ? 255u : p->e;   /* leaving the plane: the next edge is the cut's; entering: the rest of the edge it entered on */
            if (m == 0 || cv_apart(o, &out[m - 1]))
                m++;
        }
    }
    if (m > 1 && !cv_apart(&out[m - 1], &out[0]))
        m--;
    return m;
}

/* Take out a corner that lies on the line of its neighbours (sh4der_tri_area_ok: the guard of the library): a strip through three points on a line is a line on the PVR. */
static unsigned prune_flat(cv_t *p, unsigned n) {
    unsigned i = 0;
    while (n >= 3u && i < n) {
        const cv_t *a = &p[(i + n - 1u) % n], *b = &p[i], *c = &p[(i + 1u) % n];
        if (!sh4der_tri_area_ok(a->p, b->p, c->p, NEAR_Z, SH4DER_GUARD_MIN_AREA2)) {
            memmove(&p[i], &p[i + 1u], (n - i - 1u) * sizeof(cv_t));
            n--;
            i = 0;
        } else {
            i++;
        }
    }
    return n;
}

/* The gunfire: a point light at the muzzle, strongest there, nothing at GUN_RADIUS (a squared ease). d2 is the squared distance to the muzzle. */
static float gun_fall(float d2) {
    const float t = shz_saturatef(1.0f - d2 * (1.0f / (GUN_RADIUS * GUN_RADIUS)));
    return gun_power * t * t;
}
static float gun_at(const float p[3]) {        /* p in the world */
    return gun_fall(shz_vec3_magnitude_sqr(shz_vec3_init(p[0] - gun_pos[0], p[1] - gun_pos[1], p[2] - gun_pos[2])));
}
static float gun_at_view(const float p[3]) {   /* p in view space */
    return gun_fall(shz_vec3_magnitude_sqr(shz_vec3_init(p[0] - GUN_VIEW[0], p[1] - GUN_VIEW[1], p[2] - GUN_VIEW[2])));
}
/* Once a frame, after the camera: held, the flash flickers (a new level every third frame); let go, it dies in a twelfth of a second. */
static void gun_step(float dt, unsigned frame) {
    unsigned m;
    if (gun_held)
        gun_power = 0.55f + 0.45f * hash01(frame / 3u, 7u);
    else
        gun_power = shz_fmaxf(0.0f, gun_power - dt * 12.0f);
    for (m = 0; m < 3; m++)
        gun_pos[m] = cam[m] + right[m] * GUN_VIEW[0] + up[m] * GUN_VIEW[1] + fwd[m] * GUN_VIEW[2];
}

/* The light at a point of the room: the faint sky, plus the torch's near add (distance only, the library's), warm. */
static void floor_light(const float p[3], float reach_inv_r2, float out[3]) {
    const float t = sh4der_torch_near_at(shz_vec3_magnitude_sqr(shz_vec3_init(p[0] - cam[0], p[1] - cam[1], p[2] - cam[2])), torch_power,
                                         SH4DER_TORCH_NEAR_K * TORCH_NEAR_GAIN, reach_inv_r2);
    out[0] = AMBIENT[0] + t;
    out[1] = AMBIENT[1] + t * SH4DER_TORCH_TINT_G;
    out[2] = AMBIENT[2] + t * SH4DER_TORCH_TINT_B;
    if (gun_power > 0.0f) {
        const float g = gun_at(p) * GUN_GAIN;
        out[0] += g * GUN_TINT[0];
        out[1] += g * GUN_TINT[1];
        out[2] += g * GUN_TINT[2];
    }
}

/* cos^2 of a cone's half angle in degrees. */
static float cone_cos2(float deg) {
    float sn, cs;
    sh4der_sincos_(deg * (3.14159265f / 180.0f), &sn, &cs);
    return cs * cs;
}

/* The two cones, once a frame: the torch's (its edge, its full-strength core), and the bump's, which follows the torch's by BUMP AREA and fades over BUMP FADE. */
static void cones_update(void) {
    const float edge = edge_deg[opt[ROW_EDGE]], bump_edge = edge * barea_scale[opt[ROW_BAREA]];
    torch_cos2_outer = cone_cos2(edge);
    torch_cos2_inner = cone_cos2(edge * TORCH_INNER_RATIO);
    bump_cos2_outer = cone_cos2(bump_edge);
    bump_cos2_inner = cone_cos2(bump_edge * bfade_ratio[opt[ROW_BFADE]]);
}

/* The room: surface 0 is the floor, 1..4 the walls (north, south, east, west). Corner (a, c) of a surface is origin + t a CELL + b c CELL; picture u grows
   along t, v along b. The walls read upright from inside: u grows to the viewer's right, v grows down. n faces into the room. */
#define SURFS 5
typedef struct { float origin[3], t[3], b[3], n[3]; int nu, nv, flip; unsigned first; } surf_t;
static surf_t surf[SURFS];

static void room_init(void) {
    const float H = ROOM_HALF, UPV = ROOM_H;
    static const float down[3] = { 0.0f, -1.0f, 0.0f };
    const float def[SURFS][3][3] = {   /* origin, t, n */
        { { -H, 0.0f, -H }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 1.0f, 0.0f } },       /* the floor: u along +x, v along +z (b below), up */
        { { -H, UPV, H }, { 1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, -1.0f } },        /* north, z = +H, seen looking +z: right is +x */
        { { H, UPV, -H }, { -1.0f, 0.0f, 0.0f }, { 0.0f, 0.0f, 1.0f } },        /* south, z = -H, seen looking -z: right is -x */
        { { H, UPV, H }, { 0.0f, 0.0f, -1.0f }, { -1.0f, 0.0f, 0.0f } },        /* east, x = +H, seen looking +x: right is -z */
        { { -H, UPV, -H }, { 0.0f, 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f } }         /* west, x = -H, seen looking -x: right is +z */
    };
    unsigned first = 0, k, m;
    for (k = 0; k < SURFS; k++) {
        surf_t *sf = &surf[k];
        for (m = 0; m < 3; m++) {
            sf->origin[m] = def[k][0][m];
            sf->t[m] = def[k][1][m];
            sf->n[m] = def[k][2][m];
            sf->b[m] = k == 0 ? (m == 2 ? 1.0f : 0.0f) : down[m];
        }
        sf->nu = (int)(2.0f * H / CELL);
        sf->nv = k == 0 ? sf->nu : (int)(UPV / CELL);
        sf->flip = sh4der_bump_frame_flip(sf->t, sf->b, sf->n);   /* once per surface: the frame's handedness decides it, not the frame */
        sf->first = first;
        first += (unsigned)((sf->nu + 1) * (sf->nv + 1));
    }
}

static void surf_point(const surf_t *sf, float a, float c, float out[3]) {
    unsigned m;
    for (m = 0; m < 3; m++)
        out[m] = sf->origin[m] + sf->t[m] * a * CELL + sf->b[m] * c * CELL;
}

static cv_t corner[CORNERS_MAX];                /* every surface's corners in view space, made once a frame and shared by the cells round each */
static uint8_t corner_code[CORNERS_MAX];        /* each corner's clip outcode: one bit for every plane it lies outside of */
static uint8_t corner_lit[CORNERS_MAX];         /* 1 once a corner's light is made: only the corners of cells that can be seen are lit */
typedef struct { cv_t v[9]; int16_t a, c; uint8_t n, surf; } poly_t;   /* a cell cut to the screen, before it is projected */
static poly_t polys[MAX_CELLS];
static unsigned poly_count;

/* ---- where the CPU time goes: microseconds summed over a second, by slice. The waits for the PVR are not in any of them.
   The effect's own sections (bump light, bump submit, torch draw) are summed as "effect". ---- */
enum { SL_INPUT, SL_CORNERS, SL_CLIP, SL_PROJECT, SL_BUMPLIGHT, SL_HUD, SL_SUBMIT, SL_BUMPSUB, SL_TORCH, SL_LENS, SL_WIRE, SL_TESS, SL_TPLAN, SL_TSHADE, SL_TEMIT, SL_TSEND, SL_COUNT };   /* PLAN, SHADE and EMIT are inside SL_TESS; SEND is in the submit */
static uint32_t slice_us[SL_COUNT], slice_hud_max;

/* Clip outcode of a corner: bit 0 behind the near plane, 1 left, 2 right, 3 below, 4 above of the guard band. A cell whose four corners share a bit is
   wholly outside that plane (a convex polygon, linear planes: the test holds for points behind the eye too) and is dropped before any light is made. */
static unsigned outcode(const cv_t *c, float gx, float gy) {
    return (c->p[2] <= NEAR_Z ? 1u : 0u) | (c->p[0] + gx * c->p[2] <= 0.0f ? 2u : 0u) | (gx * c->p[2] - c->p[0] <= 0.0f ? 4u : 0u) |
           (c->p[1] + gy * c->p[2] <= 0.0f ? 8u : 0u) | (gy * c->p[2] - c->p[1] <= 0.0f ? 16u : 0u);
}

static void corner_make_lit(const surf_t *sf, int a, int c, float inv_r2) {
    const unsigned idx = sf->first + (unsigned)(c * (sf->nu + 1) + a);
    float world[3], rgb[3];
    if (corner_lit[idx])
        return;
    surf_point(sf, (float)a, (float)c, world);
    floor_light(world, inv_r2, rgb);
    corner[idx].r = rgb[0]; corner[idx].g = rgb[1]; corner[idx].b = rgb[2];
    corner_lit[idx] = 1;
}

/* 1 when the normal map is drawn on the cell: it takes the effect and its surface has grey maps. */
static int cell_has_map(const cell_t *cl) {
    return cl->bump && g_side[cl->wall];
}

/* The grey passes of one cell for its light (unit, world), at full strength; the corners' strengths scale them at the send.
   The level is 1 + s k ((N.H)^2 - Hz^2), H the half vector of the light and the eye (the torch sits at the eye: H is the light), kept at half level so it can pass 1:
   (N.H)^2 = sum Ni^2 Hi^2 + sum 2 Hi Hj Ni Nj: three square maps, three products biased (a + and a - copy, the one the sign asks for).
   A pass whose weight would not change a byte is left out, its bias with it. */
static void ab_cell(cell_t *cl, const sh4der_bump_frame_t *f, const float light[3], const float centre[3]) {
    static const uint8_t sq_map[3] = { G_XX, G_YY, G_ZZ }, cr_map[3] = { G_XYP, G_XZP, G_YZP }, cr_a[3] = { 0, 0, 1 }, cr_b[3] = { 1, 2, 2 };
    const float *m = g_m[cl->wall];
    const float S = SH4DER_BUMP_STRENGTH * AB_SPEC;
    const float sq_m[3] = { m[M_XX], m[M_YY], 1.0f }, cr_m[3] = { m[M_XY], m[M_XZ], m[M_YZ] };
    float d[3], h[3], k;
    unsigned n = 0, i;
    sh4der_bump_toward(centre, cam, h);   /* toward the eye */
    h[0] += light[0]; h[1] += light[1]; h[2] += light[2];
    sh4der_norm3_(h);
    d[0] = sh4der_dot3_(h, f->t);
    d[1] = (f->flip ? 1.0f : -1.0f) * sh4der_dot3_(h, f->b);
    d[2] = sh4der_sat_(sh4der_dot3_(h, f->n));
    k = S * d[2] * d[2];   /* the flat texel's own (N.H)^2, taken off so flat stays the picture */
    for (i = 0; i < 3u; i++) {
        const float w = 0.5f * S * d[i] * d[i] * sq_m[i];
        if (w < 0.004f)
            continue;
        cl->ab_t[n] = sq_map[i]; cl->ab_w[n++] = w;
    }
    for (i = 0; i < 3u; i++) {
        const float hh = 2.0f * d[cr_a[i]] * d[cr_b[i]];
        const float w = S * shz_fabsf(hh) * cr_m[i];
        if (w < 0.004f)
            continue;
        cl->ab_t[n] = (uint8_t)(cr_map[i] + (hh < 0.0f ? 1u : 0u)); cl->ab_w[n++] = w;
        k += w;
    }
    cl->ab_k = k;
    cl->ab_n = (uint8_t)n;
}

/* Build every visible cell of the room for this frame in four timed steps: the corners (each transformed once), the cut (outcode reject, light, clip,
   prune), the projection, and the bump light with the distance cut. */
static void tess_cells(const sh4der_bump_frame_t *frame, float inv_r2, float range_m);

static void build_cells(void) {
    sh4der_bump_frame_t frame[SURFS];
    const float reach = reach_wu[opt[ROW_REACH]];
    const float inv_r2 = shz_invf(reach * reach);
    const float range_m = (float)(4 + opt[ROW_BRANGE]);   /* the bump's range in metres */
    const float gx = GUARD * (SCR_W * 0.5f) / FOCAL, gy = GUARD * (SCR_H * 0.5f) / FOCAL;
    const float near_plane[3] = { 0.0f, 0.0f, 1.0f };
    const float side_plane[4][3] = { { 1.0f, 0.0f, gx }, { -1.0f, 0.0f, gx }, { 0.0f, 1.0f, gy }, { 0.0f, -1.0f, gy } };   /* x > -gx z, x < gx z, y > -gy z, y < gy z */
    uint64_t t0 = timer_us_gettime64(), t1, t2, t3;
    unsigned q, k, sidx;
    cell_count = 0;
    poly_count = 0;
    frame_tris = 0;
    memset(corner_lit, 0, sizeof(corner_lit));
    for (sidx = 0; sidx < SURFS; sidx++) {
        const surf_t *sf = &surf[sidx];
        int a, c;
        memcpy(frame[sidx].t, sf->t, sizeof(sf->t));
        memcpy(frame[sidx].b, sf->b, sizeof(sf->b));
        memcpy(frame[sidx].n, sf->n, sizeof(sf->n));
        frame[sidx].flip = sf->flip ^ opt[ROW_AZIMUTH];   /* the row turns it on top, for the console check */
        for (c = 0; c <= sf->nv; c++) {
            for (a = 0; a <= sf->nu; a++) {
                float world[3], d[3];
                cv_t *cr = &corner[sf->first + (unsigned)(c * (sf->nu + 1) + a)];
                surf_point(sf, (float)a, (float)c, world);
                d[0] = world[0] - cam[0]; d[1] = world[1] - cam[1]; d[2] = world[2] - cam[2];
                cr->p[0] = sh4der_dot3_(d, right);
                cr->p[1] = sh4der_dot3_(d, up);
                cr->p[2] = sh4der_dot3_(d, fwd);
                corner_code[sf->first + (unsigned)(c * (sf->nu + 1) + a)] = (uint8_t)outcode(cr, gx, gy);
            }
        }
    }
    t1 = timer_us_gettime64();
    slice_us[SL_CORNERS] += (uint32_t)(t1 - t0);
    for (sidx = 0; sidx < SURFS; sidx++) {
        const surf_t *sf = &surf[sidx];
        int a, c;
        for (c = 0; c < sf->nv; c++) {
            for (a = 0; a < sf->nu; a++) {
                const unsigned stride = (unsigned)(sf->nu + 1), k0 = sf->first + (unsigned)c * stride + (unsigned)a, k1 = k0 + 1u, k2 = k0 + stride + 1u, k3 = k0 + stride;
                const cv_t *src[4] = { &corner[k0], &corner[k1], &corner[k2], &corner[k3] };
                /* the picture repeats every TILE: a cell takes half of it, so u, v are 0..0.5 or 0.5..1, exactly */
                const float ub = (float)(a % 2) * (CELL / TILE), vb = (float)(c % 2) * (CELL / TILE);
                const float us[4] = { ub, ub + CELL / TILE, ub + CELL / TILE, ub }, vs[4] = { vb, vb, vb + CELL / TILE, vb + CELL / TILE };
                cv_t pa[CV_MAX], pb[CV_MAX];
                unsigned n = 4;
                if ((corner_code[k0] & corner_code[k1] & corner_code[k2] & corner_code[k3]) != 0u)
                    continue;   /* wholly outside one plane: no light, no cut */
                corner_make_lit(sf, a, c, inv_r2);
                corner_make_lit(sf, a + 1, c, inv_r2);
                corner_make_lit(sf, a + 1, c + 1, inv_r2);
                corner_make_lit(sf, a, c + 1, inv_r2);
                for (k = 0; k < 4; k++) {
                    pa[k] = *src[k];
                    pa[k].u = us[k];
                    pa[k].v = vs[k];
                    pa[k].e = (uint8_t)k;
                }
                n = clip_poly(pa, n, pb, near_plane, NEAR_Z);
                if (n >= 3) n = clip_poly(pb, n, pa, side_plane[0], 0.0f);
                if (n >= 3) n = clip_poly(pa, n, pb, side_plane[1], 0.0f);
                if (n >= 3) n = clip_poly(pb, n, pa, side_plane[2], 0.0f);
                if (n >= 3) n = clip_poly(pa, n, pb, side_plane[3], 0.0f);
                if (n < 3)
                    continue;
                n = prune_flat(pb, n);
                if (n < 3 || n > 9u || poly_count >= MAX_CELLS)
                    continue;
                memcpy(polys[poly_count].v, pb, n * sizeof(cv_t));
                polys[poly_count].n = (uint8_t)n;
                polys[poly_count].surf = (uint8_t)sidx;
                polys[poly_count].a = (int16_t)a;
                polys[poly_count].c = (int16_t)c;
                poly_count++;
            }
        }
    }
    t2 = timer_us_gettime64();
    slice_us[SL_CLIP] += (uint32_t)(t2 - t1);
    for (q = 0; q < poly_count; q++) {   /* the projection: the screen points and the vertex colours */
        const poly_t *pl = &polys[q];
        const cv_t *pv = pl->v;
        const unsigned n = pl->n;
        cell_t *cl = &cells[cell_count++];
        cl->n = (uint8_t)n;
        cl->bump = 0;
        cl->wall = pl->surf != 0;
        for (k = 0; k < n; k++) {
            const float iz_ = shz_invf(pv[k].p[2]);
            cl->v[k].x = SCR_W * 0.5f + FOCAL * pv[k].p[0] * iz_;
            cl->v[k].y = SCR_H * 0.5f - FOCAL * pv[k].p[1] * iz_;
            cl->v[k].z = iz_;
            cl->v[k].u = pv[k].u;
            cl->v[k].v = pv[k].v;
            cl->v[k].argb = pack_argb(pv[k].r, pv[k].g, pv[k].b);
        }
        frame_tris += n - 2u;
    }
    t3 = timer_us_gettime64();
    slice_us[SL_PROJECT] += (uint32_t)(t3 - t2);
    for (q = 0; q < poly_count; q++) {   /* the normal map's light of each cell: the torch (by the beam's cone) or, torch off, the sky from above; then the distance cut */
        const poly_t *pl = &polys[q];
        const surf_t *sf = &surf[pl->surf];
        cell_t *cl = &cells[q];
        float centre[3], dv[3], light[3] = { 0.0f, 1.0f, 0.0f }, strength = BUMP_SKY_STRENGTH;
        float dist2, dist_m;
        if (opt[ROW_METHOD] || !(opt[ROW_BUMP] && (pl->surf == 0 ? opt[ROW_FLOOR] : opt[ROW_WALLS])))
            continue;   /* this surface's normal map is off, or the vertex method is in use (no grey passes) */
        surf_point(sf, (float)pl->a + 0.5f, (float)pl->c + 0.5f, centre);
        dv[0] = centre[0] - cam[0]; dv[1] = centre[1] - cam[1]; dv[2] = centre[2] - cam[2];
        dist2 = shz_vec3_magnitude_sqr(shz_vec3_init(dv[0], dv[1], dv[2]));
        dist_m = shz_sqrtf(dist2) * WU_M;
        if (dist_m - CELL * 0.7071f * WU_M >= range_m)
            continue;   /* the cell's nearest point is past the range: no grey passes, no TA data */
        /* the sky light for a wall comes from above the room too, not along its normal: straight up */
        if (torch_power > 0.0f) {
            const float cosa = sh4der_dot3_(dv, fwd) * shz_inv_sqrtf(dist2 + 1.0e-12f);
            const float c2 = cosa > 0.0f ? cosa * cosa : 0.0f;
            const float spot = shz_smoothstepf(c2, bump_cos2_outer, bump_cos2_inner);
            sh4der_bump_toward(centre, cam, light);
            strength = shz_fmaxf(BUMP_SKY_STRENGTH * (1.0f - torch_power), torch_power * spot);
        }
        if (gun_power > 0.0f) {   /* the gunfire: the cell's one light direction leans to the muzzle by its share of the light, the strength is the stronger of the two */
            const float g = gun_at(centre);
            if (g > 0.0f) {
                const float a = torch_power > 0.0f ? strength : 0.0f;   /* torch off: the sky gives way to the flash */
                float lg[3];
                sh4der_bump_toward(centre, gun_pos, lg);
                light[0] = a * light[0] + g * lg[0];
                light[1] = a * light[1] + g * lg[1];
                light[2] = a * light[2] + g * lg[2];
                sh4der_norm3_(light);
                strength = shz_fmaxf(strength, g);
            }
        }
        strength *= 1.0f - shz_smoothstepf(dist_m, range_m * BUMP_RANGE_FADE, range_m);   /* an ease to nothing over the last quarter of the range */
        cl->bump = strength > 0.004f;
        for (k = 0; k < pl->n; k++)
            cl->sv[k] = strength;
        if (opt[ROW_SOFT]) {   /* the same strength, at each corner of the cut cell: the pool's edge and the range cut dissolve instead of stepping a cell at a time */
            const cv_t *pv = pl->v;
            float vmax = 0.0f;
            for (k = 0; k < pl->n; k++) {
                const float r2 = shz_vec3_magnitude_sqr(shz_vec3_init(pv[k].p[0], pv[k].p[1], pv[k].p[2]));
                const float dm = shz_sqrtf(r2) * WU_M;
                float sv = BUMP_SKY_STRENGTH;
                if (torch_power > 0.0f) {
                    const float cz = pv[k].p[2];
                    const float c2v = cz > 0.0f ? cz * cz * shz_invf(r2 + 1.0e-12f) : 0.0f;   /* cos^2 of the angle off the view axis */
                    sv = shz_fmaxf(BUMP_SKY_STRENGTH * (1.0f - torch_power), torch_power * shz_smoothstepf(c2v, bump_cos2_outer, bump_cos2_inner));
                }
                if (gun_power > 0.0f)
                    sv = shz_fmaxf(sv, gun_at_view(pv[k].p));
                sv *= 1.0f - shz_smoothstepf(dm, range_m * BUMP_RANGE_FADE, range_m);
                cl->sv[k] = sv;
                vmax = shz_fmaxf(vmax, sv);
            }
            cl->bump = vmax > 0.004f;
        }
        if (cell_has_map(cl))
            ab_cell(cl, &frame[pl->surf], light, centre);
    }
        slice_us[SL_BUMPLIGHT] += (uint32_t)(timer_us_gettime64() - t3);
    tess_cells_n = tess_prims = tess_tris = tess_vb = 0;
    tess_vn = tess_runs = tess_drop = 0;
    if (opt[ROW_METHOD]) {   /* the vertex method: subdivide the cells in the torch's cone and range */
        const uint64_t tt = timer_us_gettime64();
        tess_cells(frame, inv_r2, range_m);
        slice_us[SL_TESS] += (uint32_t)(timer_us_gettime64() - tt);
    }
}

/* ============================ THE VERTEX METHOD: TESS VERTEX ============================
   No PVR bump pass. The cells inside the torch's cone and range are cut into sub-quads (2^L x 2^L of the 1.5 m cell, L = 1..5: 75, 37, 19, 9, 4.7 cm), finest where
   the torch is strongest. Each vertex gets a normal from the bump map kept in RAM, lit by the torch (a diffuse change against the flat surface and a hot lobe), and the
   result goes into the Gouraud vertex colour over the plain textured surface: no extra pass, no extra video memory. Detail can only be as fine as the vertex spacing.
   The grid is anchored to the world (a sub-vertex's place depends on the cell and L only), so nothing swims as the beam moves. Neighbouring cells differ by at most one
   level; where a finer cell meets a coarser one, the finer edge's midpoints are put into the coarser polygon, which is then cut into triangles from its centre: the
   same vertices both sides, no gap, no zero-area triangle. The levels are chosen from the cell centre's strength, and the TRI CAP is kept by trying a lower finest
   level, then a higher strength threshold. Only cells wholly in front of the near plane are cut. */
static uint8_t tess_lvl[MAX_CELLS], tess_ok[MAX_CELLS];
static float tess_env[MAX_CELLS];
static int16_t tess_at[SURFS][256];   /* the poly index of a surface's cell (c * nu + a), -1 where there is none */

static int tess_nb(unsigned q, int da, int dc) {
    const poly_t *pl = &polys[q];
    const surf_t *sf = &surf[pl->surf];
    const int a = pl->a + da, c = pl->c + dc;
    if (a < 0 || c < 0 || a >= sf->nu || c >= sf->nv)
        return -1;
    return tess_at[pl->surf][c * sf->nu + a];
}

/* The guard band's half widths at z = 1: the planes every piece is cut to. */
#define GUARD_X (GUARD * (SCR_W * 0.5f) / FOCAL)
#define GUARD_Y (GUARD * (SCR_H * 0.5f) / FOCAL)

/* The height at picture coordinates (u, v), 0..1 (1 = the top of the relief): bilinear between the four texels round it, wraps like the texture. Byte reads. */
static float height_at(const uint8_t *h, unsigned side, float u, float v) {
    const float fx = sh4der_fma_(u, (float)side, -0.5f), fy = sh4der_fma_(v, (float)side, -0.5f);
    const float ax = sh4der_frac_(fx), ay = sh4der_frac_(fy);
    const unsigned mask = side - 1u;
    const unsigned x0 = (unsigned)(int)(fx - ax) & mask, x1 = (x0 + 1u) & mask, y0 = (unsigned)(int)(fy - ay) & mask, y1 = (y0 + 1u) & mask;
    return sh4der_lerp_(sh4der_lerp_((float)h[y0 * side + x0], (float)h[y0 * side + x1], ax), sh4der_lerp_((float)h[y1 * side + x0], (float)h[y1 * side + x1], ax), ay) *
           (1.0f / 255.0f);
}

/* TESS EXTRUDE: how much of the push a cell corner takes, 1 or 0. 0 on a surface's border (the floor meets the walls there) and on any corner a cell that is
   not subdivided touches (that cell is drawn flat): the pushed mesh meets the flat one with no gap. A cell that is not on screen does not count. */
static float tess_corner_w(const surf_t *sf, unsigned sidx, int ca, int cc) {
    int da, dc;
    if (ca <= 0 || cc <= 0 || ca >= sf->nu || cc >= sf->nv)
        return 0.0f;
    for (dc = -1; dc <= 0; dc++) {
        for (da = -1; da <= 0; da++) {
            const int idx = tess_at[sidx][(cc + dc) * sf->nu + (ca + da)];
            if (idx >= 0 && (!tess_ok[idx] || tess_lvl[idx] == 0u))
                return 0.0f;
        }
    }
    return 1.0f;
}
static float tess_w[4];        /* the cell in work: the push at its corners (a, c), (a + 1, c), (a + 1, c + 1), (a, c + 1) */
static float tess_depth_wu;    /* EXTRUDE DEPTH in world units */

/* One vertex: the surface point (a + fa, c + fc) of cell (a, c): its view-space place, picture coordinates (the cell's own, so the cell's texture is continuous
   across it), and its colour: the plain light plus the torch term.
   TESS VERTEX: the place is the flat surface's, the torch term uses the bump map's normal there.
   TESS EXTRUDE: the place is moved along the surface normal by (height - the map's median) x depth (the plate stays on the surface), eased out with the torch's
   strength; the torch term uses the slope of the height field over one vertex spacing each way: the normal of the mesh the push makes, so the light agrees with
   the shape. The spacing is taken from the torch's strength at the point (not from the cell's level), so two cells give a shared vertex the same colour. */
static void tess_vertex(const surf_t *sf, const sh4der_bump_frame_t *fr, int a, int c, float fa, float fc, float inv_r2, float range_m, cv_t *o) {
    float pw[3], d[3], rgb[3];
    float r2, dist_m, env = 0.0f, pz;
    const int wall = sf != &surf[0], extrude = opt[ROW_METHOD] == 2;
    const float ub = (float)(a % 2) * (CELL / TILE), vb = (float)(c % 2) * (CELL / TILE);
    const float tu = ((float)a + fa) * (CELL / TILE), tv = ((float)c + fc) * (CELL / TILE);
    const uint8_t *hmap = wall ? hgt_wall : hgt_floor;
    const unsigned hside = wall ? hgt_wall_side : hgt_floor_side;
    surf_point(sf, (float)a + fa, (float)c + fc, pw);
    d[0] = pw[0] - cam[0]; d[1] = pw[1] - cam[1]; d[2] = pw[2] - cam[2];
    pz = sh4der_dot3_(d, fwd);
    r2 = shz_vec3_magnitude_sqr(shz_vec3_init(d[0], d[1], d[2]));
    dist_m = shz_sqrtf(r2) * WU_M;
    if (torch_power > 0.0f && pz > 0.0f) {   /* the envelope: the torch's cone (not the bump's) x the range cut x the torch's power */
        const float c2 = pz * pz * shz_invf(r2 + 1.0e-12f);
        env = torch_power * shz_smoothstepf(c2, torch_cos2_outer, torch_cos2_inner) * (1.0f - shz_smoothstepf(dist_m, range_m * BUMP_RANGE_FADE, range_m));
    }
    if (extrude && hside && env > 0.002f) {
        const float wgt = sh4der_lerp_(sh4der_lerp_(tess_w[0], tess_w[1], fa), sh4der_lerp_(tess_w[3], tess_w[2], fa), fc);
        const float push = (height_at(hmap, hside, tu, tv) - (wall ? hgt_wall_ref : hgt_floor_ref)) * tess_depth_wu * wgt * shz_smoothstepf(env, 0.02f, 0.25f);
        pw[0] += sf->n[0] * push; pw[1] += sf->n[1] * push; pw[2] += sf->n[2] * push;
        d[0] = pw[0] - cam[0]; d[1] = pw[1] - cam[1]; d[2] = pw[2] - cam[2];
        pz = sh4der_dot3_(d, fwd);
        r2 = shz_vec3_magnitude_sqr(shz_vec3_init(d[0], d[1], d[2]));
    }
    o->p[0] = sh4der_dot3_(d, right);
    o->p[1] = sh4der_dot3_(d, up);
    o->p[2] = pz;
    o->u = ub + fa * (CELL / TILE);
    o->v = vb + fc * (CELL / TILE);
    o->e = 255u;
    floor_light(pw, inv_r2, rgb);
    if (env > 0.002f) {
        const float inv = shz_inv_sqrtf(r2 + 1.0e-12f);
        const float l[3] = { -d[0] * inv, -d[1] * inv, -d[2] * inv };   /* toward the torch (the eye) */
        float delta = 0.0f, hot = 0.0f, kd = TESS_KD;
        if (extrude) {
            if (hside) {
                const float s_cell = shz_invf(1.0f + env * ((float)(1u << tess_lv_used) - 1.0f));   /* one vertex spacing, as a share of the cell: finest where the torch is strongest */
                const float su = s_cell * (CELL / TILE);
                const float k = tess_depth_wu * strength_v[opt[ROW_NSTR]] * shz_invf(2.0f * s_cell * CELL);   /* height 0..1 over 2 spacings -> the slope */
                const float gu = (height_at(hmap, hside, tu + su, tv) - height_at(hmap, hside, tu - su, tv)) * k;
                const float gv = (height_at(hmap, hside, tu, tv + su) - height_at(hmap, hside, tu, tv - su)) * k;
                const float il = shz_inv_sqrtf(gu * gu + gv * gv + 1.0f);
                const float tl = sh4der_dot3_(l, fr->t), bl = sh4der_dot3_(l, fr->b), nl = sh4der_dot3_(l, fr->n);
                const float dn = (nl - gu * tl - gv * bl) * il;   /* the mesh normal (-gu, -gv, 1) in the surface's own t, b, n: no texture convention in it */
                const float h = sh4der_bump_pow16_(dn) - sh4der_bump_pow16_(nl);
                delta = dn - nl;
                hot = h > 0.0f ? h : 0.0f;
            }
        } else {
            const unsigned side = wall ? nrm_wall_side : nrm_floor_side;
            if (side) {
                float nrm[3];
                sh4der_bump_normal_at(wall ? nrm_wall : nrm_floor, side, tu, tv, nrm);   /* bilinear, wraps, renormalised */
                delta = sh4der_bump_vertex_light(nrm, fr, l, &hot);   /* N.L against the flat surface n.L (0 on flat texels), and the hot lobe (already >= 0) */
                kd *= strength_v[opt[ROW_NSTR]];
            }
        }
        {
            const float gain = env * (kd * delta + strength_v[opt[ROW_HSTR]] * TESS_KS * hot);
            rgb[0] += gain;
            rgb[1] += gain * SH4DER_TORCH_TINT_G;
            rgb[2] += gain * SH4DER_TORCH_TINT_B;
        }
    }
    o->r = rgb[0] > 0.0f ? rgb[0] : 0.0f;
    o->g = rgb[1] > 0.0f ? rgb[1] : 0.0f;
    o->b = rgb[2] > 0.0f ? rgb[2] : 0.0f;
}

/* A view-space corner to a screen vertex (the PVR's own 32 B form, so the send is a plain copy). */
static void tess_project(const cv_t *c, pvr_vertex_t *o) {
    const float iz = shz_invf(c->p[2]);
    o->flags = PVR_CMD_VERTEX;
    o->x = SCR_W * 0.5f + FOCAL * c->p[0] * iz;
    o->y = SCR_H * 0.5f - FOCAL * c->p[1] * iz;
    o->z = iz;
    o->u = c->u;
    o->v = c->v;
    o->argb = pack_argb(c->r, c->g, c->b);
    o->oargb = 0u;
}

/* A convex piece (a triangle or a quad, view space) into the vertex buffer as one strip; 0 when the buffer is full. A piece wholly inside the guard band (nearly
   all of them) is not cut; one that crosses a plane goes through the same five cuts and the flat-corner guard as a whole cell. */
static int tess_emit(const cv_t *in, unsigned n) {
    const float near_plane[3] = { 0.0f, 0.0f, 1.0f };
    const float side_plane[4][3] = { { 1.0f, 0.0f, GUARD_X }, { -1.0f, 0.0f, GUARD_X }, { 0.0f, 1.0f, GUARD_Y }, { 0.0f, -1.0f, GUARD_Y } };
    cv_t pa[CV_MAX], pb[CV_MAX];
    const cv_t *src = in;
    unsigned k, m = n, code = 0, lo = 1, hi;
    for (k = 0; k < n; k++)
        code |= outcode(&in[k], GUARD_X, GUARD_Y);
    if (code) {
        memcpy(pa, in, n * sizeof(cv_t));
        m = clip_poly(pa, m, pb, near_plane, NEAR_Z);
        if (m >= 3) m = clip_poly(pb, m, pa, side_plane[0], 0.0f);
        if (m >= 3) m = clip_poly(pa, m, pb, side_plane[1], 0.0f);
        if (m >= 3) m = clip_poly(pb, m, pa, side_plane[2], 0.0f);
        if (m >= 3) m = clip_poly(pa, m, pb, side_plane[3], 0.0f);
        if (m < 3)
            return 1;
        m = prune_flat(pb, m);
        if (m < 3 || m > 9u)
            return 1;
        src = pb;
    }
    if (tess_vn + m > TESS_VMAX) {
        tess_drop++;
        return 0;
    }
    hi = m - 1u;
    for (k = 0; k < m; k++)   /* round the edge as a strip: 0, 1, m - 1, 2, m - 2, ... */
        tess_project(&src[k == 0u ? 0u : (k & 1u) ? lo++ : hi--], &tess_vbuf[tess_vn++]);
    tess_vbuf[tess_vn - 1u].flags = PVR_CMD_VERTEX_EOL;
    tess_prims++;
    tess_tris += m - 2u;
    tess_vb += m * 32u;
    return 1;
}

/* Levels from the strength at each cell's centre; the finest level lv_eff, nothing below thr. Neighbours kept within one level. Returns the triangles. */
static unsigned tess_plan(unsigned lv_eff, float thr) {
    unsigned q, it, total = 0;
    for (q = 0; q < poly_count; q++) {
        const float e = tess_env[q];
        unsigned l = 0;
        if (tess_ok[q] && e > thr) {
            l = (unsigned)(e * (float)lv_eff + 0.999f);
            l = l < 1u ? 1u : l > lv_eff ? lv_eff : l;
        }
        tess_lvl[q] = (uint8_t)l;
    }
    for (it = 0; it < 5u; it++) {   /* a cell is at most one level coarser than its neighbours */
        int changed = 0;
        for (q = 0; q < poly_count; q++) {
            const unsigned l = tess_lvl[q];
            unsigned d;
            if (l < 2u)
                continue;
            for (d = 0; d < 4u; d++) {
                const int nb = tess_nb(q, d == 0 ? -1 : d == 1 ? 1 : 0, d == 2 ? -1 : d == 3 ? 1 : 0);
                if (nb >= 0 && tess_ok[nb] && tess_lvl[nb] + 1u < l) {
                    tess_lvl[nb] = (uint8_t)(l - 1u);
                    changed = 1;
                }
            }
        }
        if (!changed)
            break;
    }
    for (q = 0; q < poly_count; q++)
        if (tess_lvl[q])
            total += 2u << (2u * tess_lvl[q]);
    return total;
}

/* One cell cut into its sub-quads (level L) with the stitching to finer neighbours. A cell wholly inside the guard band (the usual case) is projected once a
   vertex and sent as one strip a row; a row that stitches to a finer neighbour, and every row of a cell that crosses a plane, goes piece by piece. */
static void tess_patch(unsigned q, const sh4der_bump_frame_t *fr, float inv_r2, float range_m) {
    static cv_t gv[33 * 33];
    static pvr_vertex_t gs[33 * 33] __attribute__((aligned(32)));
    const poly_t *pl = &polys[q];
    const surf_t *sf = &surf[pl->surf];
    const unsigned L = tess_lvl[q], n = 1u << L, w = n + 1u;
    const int a = pl->a, c = pl->c;
    const float inv_n = shz_invf((float)n);
    tess_run_t *run = &tess_run[tess_runs++];
    int fine[4];   /* the neighbour on the -a, +a, -c, +c side is finer */
    unsigned ix, iy, d, code = 0;
    const uint64_t t0 = timer_us_gettime64();
    uint64_t t1;
    run->first = tess_vn;
    run->wall = pl->surf != 0;
    for (d = 0; d < 4u; d++) {
        const int nb = tess_nb(q, d == 0 ? -1 : d == 1 ? 1 : 0, d == 2 ? -1 : d == 3 ? 1 : 0);
        fine[d] = nb >= 0 && tess_ok[nb] && tess_lvl[nb] > L;
    }
    if (opt[ROW_METHOD] == 2) {
        tess_w[0] = tess_corner_w(sf, pl->surf, a, c);
        tess_w[1] = tess_corner_w(sf, pl->surf, a + 1, c);
        tess_w[2] = tess_corner_w(sf, pl->surf, a + 1, c + 1);
        tess_w[3] = tess_corner_w(sf, pl->surf, a, c + 1);
    }
    for (iy = 0; iy <= n; iy++) {
        for (ix = 0; ix <= n; ix++) {
            cv_t *g = &gv[iy * w + ix];
            tess_vertex(sf, fr, a, c, (float)ix * inv_n, (float)iy * inv_n, inv_r2, range_m, g);
            code |= outcode(g, GUARD_X, GUARD_Y);
        }
    }
    t1 = timer_us_gettime64();
    slice_us[SL_TSHADE] += (uint32_t)(t1 - t0);
    if (!code)
        for (ix = 0; ix < w * w; ix++)
            tess_project(&gv[ix], &gs[ix]);
    for (iy = 0; iy < n; iy++) {
        if (!code && !fine[0] && !fine[1] && !(iy == 0u && fine[2]) && !(iy == n - 1u && fine[3])) {   /* a whole row as one strip */
            if (tess_vn + 2u * w > TESS_VMAX) {
                tess_drop++;
                continue;
            }
            for (ix = 0; ix <= n; ix++) {
                tess_vbuf[tess_vn++] = gs[iy * w + ix];
                tess_vbuf[tess_vn++] = gs[(iy + 1u) * w + ix];
            }
            tess_vbuf[tess_vn - 1u].flags = PVR_CMD_VERTEX_EOL;
            tess_prims++;
            tess_tris += 2u * n;
            tess_vb += 2u * w * 32u;
            continue;
        }
        for (ix = 0; ix < n; ix++) {
            cv_t poly[8], mid;
            unsigned m = 0, extras = 0, e;
            const cv_t *A = &gv[iy * w + ix], *B = &gv[iy * w + ix + 1u], *C = &gv[(iy + 1u) * w + ix + 1u], *D = &gv[(iy + 1u) * w + ix];
            const float fx = ((float)ix + 0.5f) * inv_n, fy = ((float)iy + 0.5f) * inv_n;
            poly[m++] = *A;
            if (iy == 0u && fine[2]) { tess_vertex(sf, fr, a, c, fx, 0.0f, inv_r2, range_m, &poly[m++]); extras++; }
            poly[m++] = *B;
            if (ix == n - 1u && fine[1]) { tess_vertex(sf, fr, a, c, 1.0f, fy, inv_r2, range_m, &poly[m++]); extras++; }
            poly[m++] = *C;
            if (iy == n - 1u && fine[3]) { tess_vertex(sf, fr, a, c, fx, 1.0f, inv_r2, range_m, &poly[m++]); extras++; }
            poly[m++] = *D;
            if (ix == 0u && fine[0]) { tess_vertex(sf, fr, a, c, 0.0f, fy, inv_r2, range_m, &poly[m++]); extras++; }
            if (!extras) {
                tess_emit(poly, 4);
                continue;
            }
            tess_vertex(sf, fr, a, c, fx, fy, inv_r2, range_m, &mid);   /* the centre: the pieces fan from it, no collinear triple */
            for (e = 0; e < m; e++) {
                cv_t tri[3];
                tri[0] = mid;
                tri[1] = poly[e];
                tri[2] = poly[e + 1u == m ? 0u : e + 1u];
                tess_emit(tri, 3);
            }
        }
    }
    run->count = tess_vn - run->first;
    slice_us[SL_TEMIT] += (uint32_t)(timer_us_gettime64() - t1);
}

static void tess_cells(const sh4der_bump_frame_t *frame, float inv_r2, float range_m) {
    const unsigned lv_user = (unsigned)opt[ROW_TLEVELS] + 1u;
    const unsigned cap = tcap_v[opt[ROW_TCAP]];
    unsigned lv_max = lv_user, q, lv, total = 0;
    float thr;
    const uint64_t tp0 = timer_us_gettime64();
    tess_depth_wu = depth_cm[opt[ROW_DEPTH]] * (0.01f / WU_M);
    while (lv_max > 1u && 150.0f / (float)(1u << lv_max) < tspace_cm[opt[ROW_TSPACE]])
        lv_max--;   /* the finest level whose sub-quad edge is not under MIN SPACING (a cell is 150 cm) */
    memset(tess_at, 0xff, sizeof(tess_at));
    for (q = 0; q < poly_count; q++) {
        const poly_t *pl = &polys[q];
        const surf_t *sf = &surf[pl->surf];
        const unsigned k0 = sf->first + (unsigned)pl->c * (unsigned)(sf->nu + 1) + (unsigned)pl->a, k1 = k0 + 1u, k3 = k0 + (unsigned)(sf->nu + 1), k2 = k3 + 1u;
        float centre[3], dv[3], dist2, dist_m, c2 = 0.0f;
        tess_at[pl->surf][pl->c * sf->nu + pl->a] = (int16_t)q;
        tess_ok[q] = corner[k0].p[2] > NEAR_Z && corner[k1].p[2] > NEAR_Z && corner[k2].p[2] > NEAR_Z && corner[k3].p[2] > NEAR_Z;
        surf_point(sf, (float)pl->a + 0.5f, (float)pl->c + 0.5f, centre);
        dv[0] = centre[0] - cam[0]; dv[1] = centre[1] - cam[1]; dv[2] = centre[2] - cam[2];
        dist2 = shz_vec3_magnitude_sqr(shz_vec3_init(dv[0], dv[1], dv[2]));
        dist_m = shz_sqrtf(dist2) * WU_M;
        if (torch_power > 0.0f && sh4der_dot3_(dv, fwd) > 0.0f)
            c2 = sh4der_dot3_(dv, fwd) * sh4der_dot3_(dv, fwd) * shz_invf(dist2 + 1.0e-12f);
        tess_env[q] = torch_power * shz_smoothstepf(c2, torch_cos2_outer, torch_cos2_inner) * (1.0f - shz_smoothstepf(dist_m, range_m * BUMP_RANGE_FADE, range_m));
    }
    for (lv = lv_max; lv >= 1u; lv--) {   /* the finest level that fits the cap */
        total = tess_plan(lv, 0.03f);
        if (total <= cap)
            break;
    }
    thr = 0.03f;
    if (total > cap) {   /* even level 1 is over: only the strongest cells */
        for (thr = 0.1f; thr < 1.0f; thr += 0.1f) {
            total = tess_plan(1u, thr);
            if (total <= cap)
                break;
        }
        lv = 1u;
    }
    tess_lv_used = lv < 1u ? 1u : lv;
    slice_us[SL_TPLAN] += (uint32_t)(timer_us_gettime64() - tp0);
    for (q = 0; q < poly_count; q++) {
        const poly_t *pl = &polys[q];
        int patched = tess_lvl[q] > 0u, d;
        if (!tess_ok[q])
            continue;
        for (d = 0; d < 4 && !patched; d++) {   /* a whole cell with a finer neighbour needs the midpoints too */
            const int nb = tess_nb(q, d == 0 ? -1 : d == 1 ? 1 : 0, d == 2 ? -1 : d == 3 ? 1 : 0);
            patched = nb >= 0 && tess_ok[nb] && tess_lvl[nb] > tess_lvl[q];
        }
        if (!patched)
            continue;
        cells[q].n = 0;   /* the whole cell is replaced by its pieces */
        tess_cells_n++;
        tess_patch(q, &frame[pl->surf], inv_r2, range_m);
    }
}

/* The wireframe: the cut cells' edges. The PVR has no line mode, so an edge is a quad 1 px wide (half a pixel each side of the edge) in screen space. Opaque,
   drawn over the scene. ALL CELLS: each shared edge drawn once (a cell owns its first and last edge of the surface, the next cell's neighbours own the rest),
   cyan, and a cell that takes the bump pass in orange. BUMP CELLS: only those, every one of their edges. Edges the cut made (the screen's or the near plane's)
   are not drawn. The data is counted in wire_bytes, not in the effect's TA figure. */
static void wire_quad(const sh4der_bump_vert_t *p0, const sh4der_bump_vert_t *p1, uint32_t col) {
    const float dx = p1->x - p0->x, dy = p1->y - p0->y, len2 = dx * dx + dy * dy;
    const float nx = -dy * shz_inv_sqrtf(len2 + 1.0e-9f) * 0.5f, ny = dx * shz_inv_sqrtf(len2 + 1.0e-9f) * 0.5f;
    unsigned c;
    if (len2 < 0.25f)
        return;
    for (c = 0; c < 4u; c++) {
        const sh4der_bump_vert_t *src = (c & 2u) ? p1 : p0;
        const float sgn = (c & 1u) ? -1.0f : 1.0f;
        pvr_vertex_t *out = pvr_dr_target();
        out->flags = c == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        out->x = src->x + sgn * nx;
        out->y = src->y + sgn * ny;
        out->z = 1000.0f;
        out->u = 0.0f;
        out->v = 0.0f;
        out->argb = col;
        out->oargb = 0u;
        pvr_dr_commit(out);
    }
    wire_bytes += 4u * 32u;
}

static void wire_draw(void) {
    const int mode = opt[ROW_WIRE];
    unsigned q, k;
    wire_bytes = 0;
    if (!mode || !sh4der_bump_send_header(&hdr_wire))
        return;
    wire_bytes += 32u;
    {   /* the vertex method's strips: green; each triangle draws the two edges its last vertex adds, a strip's first edge once */
        unsigned i0 = 0, i;
        for (i = 0; i < tess_vn; i++) {
            const pvr_vertex_t *t = &tess_vbuf[i];
            const sh4der_bump_vert_t p2 = { t->x, t->y, t->z, 0.0f, 0.0f, 0u };
            if (i > i0) {
                const sh4der_bump_vert_t p1 = { t[-1].x, t[-1].y, t[-1].z, 0.0f, 0.0f, 0u };
                wire_quad(&p1, &p2, 0xff58ff3au);
            }
            if (i > i0 + 1u) {
                const sh4der_bump_vert_t p0 = { t[-2].x, t[-2].y, t[-2].z, 0.0f, 0.0f, 0u };
                wire_quad(&p0, &p2, 0xff58ff3au);
            }
            if (t->flags == PVR_CMD_VERTEX_EOL)
                i0 = i + 1u;
        }
    }
    for (q = 0; q < poly_count; q++) {
        const poly_t *pl = &polys[q];
        const cell_t *cl = &cells[q];
        const surf_t *sf = &surf[pl->surf];
        const uint32_t col = cl->bump ? 0xffff9a1au : 0xff20d8f0u;   /* orange: a bump cell; cyan: a cell */
        if (mode == 2 && !cl->bump)
            continue;
        for (k = 0; k < cl->n; k++) {
            const unsigned e = pl->v[k].e, k1 = k + 1u == cl->n ? 0u : k + 1u;
            const float dx = cl->v[k1].x - cl->v[k].x, dy = cl->v[k1].y - cl->v[k].y, len2 = dx * dx + dy * dy;
            float nx, ny;
            unsigned c;
            if (e > 3u || len2 < 0.25f)
                continue;
            if (mode == 1 && !(e == 0u || e == 3u || (e == 1u && pl->a == sf->nu - 1) || (e == 2u && pl->c == sf->nv - 1)))
                continue;   /* the neighbour draws this edge */
            nx = -dy * shz_inv_sqrtf(len2) * 0.5f;
            ny = dx * shz_inv_sqrtf(len2) * 0.5f;
            for (c = 0; c < 4u; c++) {
                const sh4der_bump_vert_t *src = (c & 2u) ? &cl->v[k1] : &cl->v[k];
                const float sgn = (c & 1u) ? -1.0f : 1.0f;
                pvr_vertex_t *out = pvr_dr_target();
                out->flags = c == 3u ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                out->x = src->x + sgn * nx;
                out->y = src->y + sgn * ny;
                out->z = 1000.0f;
                out->u = 0.0f;
                out->v = 0.0f;
                out->argb = col;
                out->oargb = 0u;
                pvr_dr_commit(out);
            }
            wire_bytes += 4u * 32u;
        }
    }
}

/* ------------------------------------------------------------------------ */
static void sky_draw(void) {
    /* horizon line on screen: pitch up moves it down by focal x tan(pitch) */
    float sp, cp, hy;
    sh4der_sincos_(cam_pitch, &sp, &cp);
    hy = shz_clampf(SCR_H * 0.5f + FOCAL * sh4der_div_(sp, shz_fmaxf(cp, 0.05f)), 1.0f, SCR_H - 1.0f);
    {
        const uint32_t top = pack_argb(SKY_ZENITH[0], SKY_ZENITH[1], SKY_ZENITH[2]), hor = pack_argb(SKY_HORIZON[0], SKY_HORIZON[1], SKY_HORIZON[2]);
        const sh4der_bump_vert_t sky[4] = { { 0.0f, 0.0f, 0.0001f, 0, 0, top }, { SCR_W, 0.0f, 0.0001f, 0, 0, top },
                                            { 0.0f, hy, 0.0001f, 0, 0, hor }, { SCR_W, hy, 0.0001f, 0, 0, hor } };
        const sh4der_bump_vert_t ground[4] = { { 0.0f, hy, 0.0001f, 0, 0, hor }, { SCR_W, hy, 0.0001f, 0, 0, hor },
                                               { 0.0f, SCR_H, 0.0001f, 0, 0, hor }, { SCR_W, SCR_H, 0.0001f, 0, 0, hor } };
        /* four corners as a fan: 0, 1, 3, 2 round the edge */
        const sh4der_bump_vert_t q1[4] = { sky[0], sky[1], sky[3], sky[2] }, q2[4] = { ground[0], ground[1], ground[3], ground[2] };
        ta_bytes += 64u + 32u * (sh4der_bump_submit_fan(&hdr_sky, q1, 4) + sh4der_bump_submit_fan(&hdr_sky, q2, 4));
    }
}

/* A header is sent once for a run of cells that share it (the offset colour rides in each vertex): hdr_use sends it when it changes.
   A header that was never compiled sends nothing, and the cells under it are skipped. Both lists start with no header. */
static const pvr_poly_hdr_t *hdr_cur;
static int hdr_cur_ok;
static int hdr_use(const pvr_poly_hdr_t *h) {
    if (h != hdr_cur) {
        hdr_cur = h;
        hdr_cur_ok = sh4der_bump_send_header(h);
        if (hdr_cur_ok)
            ta_bytes += 32u;
    }
    return hdr_cur_ok;
}

/* The vertex method's strips: one header a run (it changes only between the floor and the walls), every vertex a 32 B copy into the store queue. */
static void tess_send(int clean) {
    const uint64_t t0 = timer_us_gettime64();
    unsigned r, i;
    for (r = 0; r < tess_runs; r++) {
        const tess_run_t *run = &tess_run[r];
        if (!run->count || !hdr_use(run->wall ? &hdr_op_wall : clean ? &hdr_op_clean : &hdr_op_baked))
            continue;
        for (i = run->first; i < run->first + run->count; i++) {
            pvr_vertex_t *out = pvr_dr_target();
            *out = tess_vbuf[i];
            pvr_dr_commit(out);
        }
        ta_bytes += 32u * run->count;
    }
    slice_us[SL_TSEND] += (uint32_t)(timer_us_gettime64() - t0);
}

static void floor_op(void) {
    unsigned i;
    const int clean = opt[ROW_FLOOR] == 1;   /* clean + normal map draws on the clean base; baked and combined on the baked */
    sh4der_bump_vert_t tmp[9];
    hdr_cur = NULL;
    for (i = 0; i < cell_count; i++) {
        const cell_t *cl = &cells[i];
        if (cl->n < 3u)
            continue;   /* a cell the vertex method subdivided: its pieces are drawn instead */
        if (cell_has_map(cl)) {   /* no picture yet: the flat level the grey passes add onto, kept at half, untextured */
            unsigned k;
            for (k = 0; k < cl->n; k++) {
                const float g = 0.5f * shz_saturatef(1.0f - cl->sv[k] * cl->ab_k);
                tmp[k] = cl->v[k];
                tmp[k].argb = pack_argb(g, g, g);
            }
            if (hdr_use(&hdr_sky))
                ta_bytes += 32u * sh4der_bump_send_fan(tmp, cl->n);
        } else if (hdr_use(cl->wall ? &hdr_op_wall : clean ? &hdr_op_clean : &hdr_op_baked)) {
            ta_bytes += 32u * sh4der_bump_send_fan(cl->v, cl->n);
        }
    }
    if (opt[ROW_METHOD])
        tess_send(clean);
}

/* The normal map's translucent passes, after the base (the list draws in the order sent; the cells do not cover each other): map by map (a header a map and a surface),
   then the picture, lit per vertex as the base is, multiplied on twice (DESTCOLOR x the source colour). */
static void floor_tr(void) {
    sh4der_bump_vert_t tmp[9];
    const int filt = opt[ROW_BFILTER] ? 1 : 0;
    unsigned i, k, t, p;
    hdr_cur = NULL;
    ab_lit = 0;
    if (!opt[ROW_BUMP])
        return;
    for (i = 0; i < cell_count; i++)
        ab_lit += cells[i].bump ? 1u : 0u;
    for (t = 0; t < G_COUNT; t++) {
        for (i = 0; i < cell_count; i++) {
            const cell_t *cl = &cells[i];
            if (!cell_has_map(cl))
                continue;
            for (p = 0; p < cl->ab_n && cl->ab_t[p] != t; p++)
                ;
            if (p == cl->ab_n || !hdr_use(&hdr_g_add[cl->wall][t][filt]))
                continue;
            for (k = 0; k < cl->n; k++) {
                const float g = cl->ab_w[p] * cl->sv[k];
                tmp[k] = cl->v[k];
                tmp[k].argb = pack_argb(g, g, g);
            }
            ta_bytes += 32u * sh4der_bump_send_fan(tmp, cl->n);
        }
    }
    for (i = 0; i < cell_count; i++) {
        const cell_t *cl = &cells[i];
        if (cell_has_map(cl) && hdr_use(&hdr_apply2[cl->wall ? 2 : opt[ROW_FLOOR] == 2]))
            ta_bytes += 32u * sh4der_bump_send_fan(cl->v, cl->n);
    }
}

/* ------------------------------------------------------------------------ */
/* Text: KOS minifont (8 x 16) into a texture, white on nothing, alpha one bit. The prompt is made once. */
/* The 5 x 7 font: five columns a glyph, bit 0 the top row, ASCII 32 to 126 (the common public-domain 5 x 7 table). A glyph is 5 texels wide and 7 high on a 6 x 9 cell:
   7 px against the KOS minifont's 16. Drawn into the texture as whole texels (ARGB1555 0xffff); the text textures are sampled without filtering, so it stays crisp. */
static const uint8_t font5x7[95][5] = {
    {0x00,0x00,0x00,0x00,0x00},{0x00,0x00,0x5F,0x00,0x00},{0x00,0x07,0x00,0x07,0x00},{0x14,0x7F,0x14,0x7F,0x14},{0x24,0x2A,0x7F,0x2A,0x12},
    {0x23,0x13,0x08,0x64,0x62},{0x36,0x49,0x55,0x22,0x50},{0x00,0x05,0x03,0x00,0x00},{0x00,0x1C,0x22,0x41,0x00},{0x00,0x41,0x22,0x1C,0x00},
    {0x08,0x2A,0x1C,0x2A,0x08},{0x08,0x08,0x3E,0x08,0x08},{0x00,0x50,0x30,0x00,0x00},{0x08,0x08,0x08,0x08,0x08},{0x00,0x60,0x60,0x00,0x00},
    {0x20,0x10,0x08,0x04,0x02},{0x3E,0x51,0x49,0x45,0x3E},{0x00,0x42,0x7F,0x40,0x00},{0x42,0x61,0x51,0x49,0x46},{0x21,0x41,0x45,0x4B,0x31},
    {0x18,0x14,0x12,0x7F,0x10},{0x27,0x45,0x45,0x45,0x39},{0x3C,0x4A,0x49,0x49,0x30},{0x01,0x71,0x09,0x05,0x03},{0x36,0x49,0x49,0x49,0x36},
    {0x06,0x49,0x49,0x29,0x1E},{0x00,0x36,0x36,0x00,0x00},{0x00,0x56,0x36,0x00,0x00},{0x00,0x08,0x14,0x22,0x41},{0x14,0x14,0x14,0x14,0x14},
    {0x41,0x22,0x14,0x08,0x00},{0x02,0x01,0x51,0x09,0x06},{0x32,0x49,0x79,0x41,0x3E},{0x7E,0x11,0x11,0x11,0x7E},{0x7F,0x49,0x49,0x49,0x36},
    {0x3E,0x41,0x41,0x41,0x22},{0x7F,0x41,0x41,0x22,0x1C},{0x7F,0x49,0x49,0x49,0x41},{0x7F,0x09,0x09,0x09,0x01},{0x3E,0x41,0x49,0x49,0x7A},
    {0x7F,0x08,0x08,0x08,0x7F},{0x00,0x41,0x7F,0x41,0x00},{0x20,0x40,0x41,0x3F,0x01},{0x7F,0x08,0x14,0x22,0x41},{0x7F,0x40,0x40,0x40,0x40},
    {0x7F,0x02,0x0C,0x02,0x7F},{0x7F,0x04,0x08,0x10,0x7F},{0x3E,0x41,0x41,0x41,0x3E},{0x7F,0x09,0x09,0x09,0x06},{0x3E,0x41,0x51,0x21,0x5E},
    {0x7F,0x09,0x19,0x29,0x46},{0x46,0x49,0x49,0x49,0x31},{0x01,0x01,0x7F,0x01,0x01},{0x3F,0x40,0x40,0x40,0x3F},{0x1F,0x20,0x40,0x20,0x1F},
    {0x3F,0x40,0x38,0x40,0x3F},{0x63,0x14,0x08,0x14,0x63},{0x07,0x08,0x70,0x08,0x07},{0x61,0x51,0x49,0x45,0x43},{0x00,0x7F,0x41,0x41,0x00},
    {0x02,0x04,0x08,0x10,0x20},{0x00,0x41,0x41,0x7F,0x00},{0x04,0x02,0x01,0x02,0x04},{0x40,0x40,0x40,0x40,0x40},{0x00,0x01,0x02,0x04,0x00},
    {0x20,0x54,0x54,0x54,0x78},{0x7F,0x48,0x44,0x44,0x38},{0x38,0x44,0x44,0x44,0x20},{0x38,0x44,0x44,0x48,0x7F},{0x38,0x54,0x54,0x54,0x18},
    {0x08,0x7E,0x09,0x01,0x02},{0x0C,0x52,0x52,0x52,0x3E},{0x7F,0x08,0x04,0x04,0x78},{0x00,0x44,0x7D,0x40,0x00},{0x20,0x40,0x44,0x3D,0x00},
    {0x7F,0x10,0x28,0x44,0x00},{0x00,0x41,0x7F,0x40,0x00},{0x7C,0x04,0x18,0x04,0x78},{0x7C,0x08,0x04,0x04,0x78},{0x38,0x44,0x44,0x44,0x38},
    {0x7C,0x14,0x14,0x14,0x08},{0x08,0x14,0x14,0x18,0x7C},{0x7C,0x08,0x04,0x04,0x08},{0x48,0x54,0x54,0x54,0x20},{0x04,0x3F,0x44,0x40,0x20},
    {0x3C,0x40,0x40,0x20,0x7C},{0x1C,0x20,0x40,0x20,0x1C},{0x3C,0x40,0x30,0x40,0x3C},{0x44,0x28,0x10,0x28,0x44},{0x0C,0x50,0x50,0x50,0x3C},
    {0x44,0x64,0x54,0x4C,0x44},{0x00,0x08,0x36,0x41,0x00},{0x00,0x00,0x7F,0x00,0x00},{0x00,0x41,0x36,0x08,0x00},{0x02,0x01,0x02,0x04,0x02}
};

static void font5_draw(uint16_t *px, unsigned width, unsigned x, unsigned y, const char *str) {
    for (; *str; str++, x += 6u) {
        unsigned c = (unsigned char)*str, col, row;
        const uint8_t *g;
        if (c < 32u || c > 126u)
            c = '?';
        if (x + 5u > width)
            break;   /* a line is at most width / 6 characters: longer would run into the next row */
        g = font5x7[c - 32u];
        for (col = 0; col < 5u; col++)
            for (row = 0; row < 7u; row++)
                if (g[col] & (1u << row))
                    px[(y + row) * width + x + col] = 0xffffu;
    }
}

static void text_to_texture(uint16_t *px, unsigned width, unsigned y, const char *s) {
    font5_draw(px, width, 0, y, s);
}

static void make_prompt(void) {
    unsigned i;
    memset(prompt_pixels, 0, sizeof(prompt_pixels));
    text_to_texture(prompt_pixels, 256, 0, opt[ROW_SCHEME] ? "Press X to turn on your torch" : "Press Up to turn on your torch");
    for (i = 0; i < 256u * 8u; i++)
        prompt_pixels[i] = prompt_pixels[i] ? 0xffffu : 0u;
    pvr_txr_load(prompt_pixels, prompt_mem, sizeof(prompt_pixels));
}

static const char *row_value(int row, char *buf, size_t cap) {
    switch (row) {
    case ROW_BUMP: return opt[ROW_BUMP] ? "on" : "off";
    case ROW_TORCH: return opt[ROW_TORCH] ? "on" : "off";
    case ROW_FLOOR: return opt[ROW_FLOOR] == 0 ? "baked diffuse (DCE today)" : opt[ROW_FLOOR] == 1 ? "clean + normal map" : "combined: baked + normal map";
    case ROW_AZIMUTH: return opt[ROW_AZIMUTH] ? "y down" : "y up (pvrtex)";
    case ROW_GRAIN: return opt[ROW_GRAIN] ? "on" : "off";
    case ROW_VIGNETTE: return opt[ROW_VIGNETTE] ? "on" : "off";
    case ROW_EDGE: snprintf(buf, cap, "%d deg, disc %d px", (int)edge_deg[opt[ROW_EDGE]], (int)(FOCAL * sh4der_torch_tan_of_cos2(torch_cos2_outer) + 0.5f)); return buf;
    case ROW_REACH: snprintf(buf, cap, "%d wu", (int)reach_wu[opt[ROW_REACH]]); return buf;
    case ROW_HOT: snprintf(buf, cap, "%d %%", (int)(hot_scale[opt[ROW_HOT]] * 100.0f)); return buf;
    case ROW_BAREA: snprintf(buf, cap, "%d %% of the torch edge", (int)(barea_scale[opt[ROW_BAREA]] * 100.0f)); return buf;
    case ROW_BFADE: snprintf(buf, cap, "full strength to %d %% of the edge", (int)(bfade_ratio[opt[ROW_BFADE]] * 100.0f)); return buf;
    case ROW_BRANGE: snprintf(buf, cap, "%d m (fades from %d m)", 4 + opt[ROW_BRANGE], (int)((float)(4 + opt[ROW_BRANGE]) * BUMP_RANGE_FADE)); return buf;
    case ROW_WALLS: return opt[ROW_WALLS] ? "on" : "off";
    case ROW_SOFT: return opt[ROW_SOFT] ? "on: strength per vertex" : "off: one strength per cell";
    case ROW_BFILTER: return opt[ROW_BFILTER] ? "bilinear" : "nearest";
    case ROW_WIRE: return opt[ROW_WIRE] == 0 ? "off" : opt[ROW_WIRE] == 1 ? "all: cyan cells, orange normal map, green tess" : "normal map (orange) / tess (green) only";
    case ROW_SCHEME: return opt[ROW_SCHEME] ? "dual stick: left moves, right looks" : "single stick: stick looks, YAXB move";
    case ROW_INVY: return opt[ROW_INVY] ? "on: stick up looks down" : "off: stick up looks up";
    case ROW_METHOD: return opt[ROW_METHOD] == 0 ? "NORMAL MAP (grey passes)" : opt[ROW_METHOD] == 1 ? "TESS VERTEX (normal map normals)" :
                            hgt_floor_side ? "TESS EXTRUDE (pushed in by height)" : "TESS EXTRUDE (no height map: flat)";
    case ROW_TLEVELS: snprintf(buf, cap, "%d (%d cm sub-quads)", opt[ROW_TLEVELS] + 1, (int)(150.0f / (float)(1 << (opt[ROW_TLEVELS] + 1)))); return buf;
    case ROW_TSPACE: snprintf(buf, cap, "%d cm", (int)tspace_cm[opt[ROW_TSPACE]]); return buf;
    case ROW_TCAP: snprintf(buf, cap, "%u tris", tcap_v[opt[ROW_TCAP]]); return buf;
    case ROW_NSTR: snprintf(buf, cap, "%d %%", (int)(strength_v[opt[ROW_NSTR]] * 100.0f)); return buf;
    case ROW_HSTR: snprintf(buf, cap, "%d %%", (int)(strength_v[opt[ROW_HSTR]] * 100.0f)); return buf;
    default: snprintf(buf, cap, "%d.%d cm (TESS EXTRUDE)", (int)depth_cm[opt[ROW_DEPTH]], ((int)(depth_cm[opt[ROW_DEPTH]] * 10.0f)) % 10); return buf;
    }
}

/* The controls hint: one short line, always on screen, in the same small font, naming the buttons of the scheme in use. It is rebuilt when the scheme changes
   (and the start prompt with it, if it is still showing, since it names the torch button). */
static void floor_prompt_update(void) {
    const char *text = opt[ROW_SCHEME] ? "L stick move R stick look X torch Y menu" : "stick look YAXB move Up torch Dn menu";
    unsigned i;
    if (opt[ROW_SCHEME] == fprompt_shown)
        return;
    fprompt_shown = opt[ROW_SCHEME];
    pvr_wait_render_done();   /* no frame in flight reads the textures while they are rewritten */
    memset(fprompt_pixels, 0, sizeof(fprompt_pixels));
    text_to_texture(fprompt_pixels, 256, 0, text);
    for (i = 0; i < 256u * 8u; i++)
        fprompt_pixels[i] = fprompt_pixels[i] ? 0xffffu : 0u;
    pvr_txr_load(fprompt_pixels, fprompt_mem, sizeof(fprompt_pixels));
    if (prompt_state == 0)
        make_prompt();
}

static float frame_ms, cpu_ms, render_ms;
static unsigned ta_kb, ta_max_kb;
static float eff_own_ms, eff_cpu_delta_ms, eff_rnd_delta_ms;   /* the effect (normal map + torch): its own sections, and the cost against the effect off */
static int eff_delta_valid;
static float eff_ta_delta_kb;   /* the TA vertex data the effect adds per frame: on minus off, a one-second average */
#define KB(b) ((((b) + 1023u) / 1024u) % 10000u)   /* whole KB, four digits at most: the readout lines have a fixed width */
static void hud_update(void) {
    char ln[8][80], v[40];
    static char prev[8][80];
    unsigned i;
    snprintf(ln[0], 80, "%s: %s   %s", row_name[row_pick], row_value(row_pick, v, sizeof(v)), opt[ROW_SCHEME] ? "(up/down row, left/right/A)" : "(D-pad row/change, B close)");
    snprintf(ln[1], 80, "edge %d reach %d core %d%% area %d%% fade %d%% range %dm", (int)edge_deg[opt[ROW_EDGE]], (int)reach_wu[opt[ROW_REACH]],
             (int)(hot_scale[opt[ROW_HOT]] * 100.0f), (int)(barea_scale[opt[ROW_BAREA]] * 100.0f), (int)(bfade_ratio[opt[ROW_BFADE]] * 100.0f), 4 + opt[ROW_BRANGE]);
    snprintf(ln[2], 80, "cpu %d.%d frm %d.%d rnd %d.%d ms  VRAM free %lu KB", (int)cpu_ms, ((int)(cpu_ms * 10.0f)) % 10, (int)frame_ms,
             ((int)(frame_ms * 10.0f)) % 10, (int)render_ms, ((int)(render_ms * 10.0f)) % 10, (unsigned long)(pvr_mem_available() / 1024u));
    if (ab_state)
        snprintf(ln[3], 80, "measuring the effect: %s ...", ab_state == 1 ? "off" : "on");
    else if (eff_delta_valid)
        snprintf(ln[3], 80, "effect cpu %d.%d ms (CPU only) | vs off: cpu %+d.%d rnd %+d.%d ms", (int)eff_own_ms, ((int)(eff_own_ms * 10.0f)) % 10,
                 (int)eff_cpu_delta_ms, abs(((int)(eff_cpu_delta_ms * 10.0f)) % 10), (int)eff_rnd_delta_ms, abs(((int)(eff_rnd_delta_ms * 10.0f)) % 10));
    else
        snprintf(ln[3], 80, "effect cpu %d.%d ms (CPU only); render delta: press START", (int)eff_own_ms, ((int)(eff_own_ms * 10.0f)) % 10);
    {
        char reg[24];
        if (ta_max_kb == 0)
            snprintf(reg, sizeof(reg), "n/a");   /* the register the KOS stat reads gave nothing: unmeasured, not zero */
        else
            snprintf(reg, sizeof(reg), "%u/%u", ta_kb, ta_max_kb);
        if (eff_delta_valid)
            snprintf(ln[4], 80, "tris %u cells %u TA %uKB (%+d) reg %s wire %uKB", frame_tris, cell_count, ta_counted_kb, (int)eff_ta_delta_kb, reg, wire_kb);
        else
            snprintf(ln[4], 80, "tris %u cells %u TA %uKB reg %s wire %uKB", frame_tris, cell_count, ta_counted_kb, reg, wire_kb);
    }
    snprintf(ln[5], 80, "VRAM effect: maps %u torch %u lens %u = %u KB", KB(vram_gmap_b), KB(vram_torch_b), KB(vram_lens_b), KB(vram_gmap_b + vram_torch_b + vram_lens_b));
    snprintf(ln[6], 80, "VRAM base: floor %u wall %u text %u = %u KB (%s)", KB(vram_floor_b), KB(vram_wall_b), KB(vram_text_b),
             KB(vram_floor_b + vram_wall_b + vram_text_b), private_textures ? "priv" : "plhd");
    if (opt[ROW_METHOD]) {   /* the vertex method needs no grey passes and no video memory of its own: only the normals kept in RAM */
        snprintf(ln[5], 80, "tess %u cells %u strips %u tris (cap %u) L%u %dcm TA+%uKB drop %u", tess_cells_n, tess_prims, tess_tris, tcap_v[opt[ROW_TCAP]], tess_lv_used,
                 (int)(150.0f / (float)(1u << tess_lv_used)), (tess_vb + 1023u) / 1024u, tess_drop);
        snprintf(ln[6], 80, "VRAM effect 0 KB; RAM normals %u heights %u KB, verts %u/%u", ((nrm_floor_side * nrm_floor_side + nrm_wall_side * nrm_wall_side) * 3u + 1023u) / 1024u,
                 (hgt_floor_side * hgt_floor_side + hgt_wall_side * hgt_wall_side + 1023u) / 1024u, tess_vn, (unsigned)TESS_VMAX);
#define MS1(x) (int)(x), ((int)((x) * 10.0f)) % 10
        snprintf(ln[7], 80, "tess %d.%d ms = plan %d.%d shade %d.%d emit %d.%d | send %d.%d ms", MS1(tess_ms), MS1(tplan_ms), MS1(tshade_ms), MS1(temit_ms), MS1(tsend_ms));
#undef MS1
    } else {
        snprintf(ln[7], 80, "method: normal map, grey passes (METHOD row: TESS VERTEX)");
    }
    {   /* the texture is rebuilt and uploaded only when a line changed */
        int same = 1;
        for (i = 0; i < 8; i++)
            same &= !memcmp(prev[i], ln[i], 80);
        if (same)
            return;
        memcpy(prev, ln, sizeof(prev));
    }
    memset(hud_pixels, 0, sizeof(hud_pixels));
    for (i = 0; i < 8; i++)
        text_to_texture(hud_pixels, 512, 9u * i, ln[i]);
    for (i = 0; i < 512u * HUD_H; i++)
        hud_pixels[i] = hud_pixels[i] ? 0xffffu : 0u;
    pvr_txr_load(hud_pixels, hud_mem, sizeof(hud_pixels));
}

/* The label, always on screen (bottom right): its name, the draws a lit cell takes at most (the level, six grey passes, the picture), the lit cells, the TA bytes the
   normal map's passes sent last frame, the video memory of the grey maps, and the last second's cpu / render / frame ms. Rebuilt and uploaded only when a line changed. */
static void ab_label_update(void) {
    static char prev[4][44];
    char ln[4][44];
    unsigned i;
    memset(ln, 0, sizeof(ln));
    snprintf(ln[0], 43, "normal map: highlight power 2");
    snprintf(ln[1], 43, "draws/cell <=8 lit %u TA %lu B", ab_lit, (unsigned long)ab_ta);
    snprintf(ln[2], 43, "VRAM %uKB cpu %d.%d rnd %d.%d frm %d.%d ms", KB(vram_gmap_b), (int)cpu_ms, ((int)(cpu_ms * 10.0f)) % 10, (int)render_ms, ((int)(render_ms * 10.0f)) % 10,
             (int)frame_ms, ((int)(frame_ms * 10.0f)) % 10);
    snprintf(ln[3], 43, "R fire  filter %s", opt[ROW_BFILTER] ? "bilinear" : "nearest");
    if (!memcmp(prev, ln, sizeof(prev)))
        return;
    memcpy(prev, ln, sizeof(prev));
    memset(ab_label_px, 0, sizeof(ab_label_px));
    for (i = 0; i < 4u; i++)
        text_to_texture(ab_label_px, 256, 9u * i, ln[i]);
    pvr_txr_load(ab_label_px, ab_label_mem, sizeof(ab_label_px));
}

static float quad_v1 = 1.0f;   /* the bottom edge's v: 1 draws the whole texture */
static void quad_draw(const pvr_poly_hdr_t *hdr, float x, float y, float w, float h, uint32_t argb) {
    unsigned i;
    ta_bytes += 32u + 4u * 32u;
    pvr_prim((void *)hdr, sizeof(*hdr));
    for (i = 0; i < 4; i++) {
        pvr_vertex_t *v = pvr_dr_target();
        v->flags = i == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v->x = x + ((i & 1u) ? w : 0.0f);
        v->y = y + ((i & 2u) ? h : 0.0f);
        v->z = 1000.0f;
        v->u = (i & 1u) ? 1.0f : 0.0f;
        v->v = (i & 2u) ? quad_v1 : 0.0f;
        v->argb = argb;
        v->oargb = 0;
        pvr_dr_commit(v);
    }
}

/* ------------------------------------------------------------------------ */
static void row_change(int dir) {
    int *o = &opt[row_pick];
    const int n = (int)row_count[row_pick];
    *o = (*o + n + dir) % n;   /* every row cycles through its values (two for an on / off row) */
}

/* START measures the effect: the effect off for 3.2 s, then on for 3.5 s (the torch ramps are over inside a one-second window); the one-second windows
   in the main loop then hold both states and the readout shows cpu, render and TA on minus off. The rows are put back after. */
static void ab_tick(float dt) {
    if (ab_state == 0)
        return;
    ab_t -= dt;
    if (ab_t > 0.0f)
        return;
    if (ab_state == 1) {
        const int none = !ab_saved_bump && !ab_saved_torch;   /* it was off to begin with: the on phase turns it on */
        opt[ROW_BUMP] = ab_saved_bump || none;
        opt[ROW_TORCH] = ab_saved_torch || none;
        ab_state = 2;
        ab_t = 3.5f;
    } else {
        opt[ROW_BUMP] = ab_saved_bump;
        opt[ROW_TORCH] = ab_saved_torch;
        ab_state = 0;
    }
}

/* The torch toggle: the first press also starts the start prompt's fade (it then stays gone). */
static void torch_toggle(void) {
    opt[ROW_TORCH] ^= 1;
    if (prompt_state == 0) {
        prompt_state = 1;
        prompt_t = 0.0f;
    }
}

static void measure_start(void) {   /* START: the effect off, then on (ab_tick) */
    if (ab_state != 0)
        return;
    ab_saved_bump = opt[ROW_BUMP];
    ab_saved_torch = opt[ROW_TORCH];
    opt[ROW_BUMP] = 0;
    opt[ROW_TORCH] = 0;
    ab_state = 1;
    ab_t = 3.2f;
    ab_reset_request = 1;
}

/* The controls. SINGLE STICK (the default; one stick on the pad): the stick LOOKS (both axes, a dead zone, invert Y a row); the face buttons MOVE while the menu is
   closed (hold: Y forward, A back, X strafe left, B strafe right); D-pad UP toggles the torch, D-pad DOWN opens the menu, START measures the effect. While the menu is
   OPEN the face buttons do not move: D-pad up / down pick a row, left / right change it, A changes it too, B or START closes it (the stick still looks).
   DUAL STICK (the earlier scheme, row CONTROLS): the left stick moves, the right stick looks, X toggles the torch, Y opens and closes the menu, the D-pad picks and
   changes rows (up / down, left / right, A), START measures. In both, the R trigger fires. */
static void input(float dt) {
    static uint32_t last;
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
    uint32_t down, held;
    float mv_f = 0.0f, mv_r = 0.0f, look_x = 0.0f, look_y = 0.0f;
    if (!st)
        return;
    held = st->buttons;
    down = held & ~last;
    last = held;
    gun_held = st->rtrig > 160;   /* the right trigger fires while it is held (the wireframe is the WIREFRAME row's only) */
    if (opt[ROW_SCHEME] == 0) {   /* single stick */
        if (hud_show) {           /* the menu is open */
            if (down & CONT_DPAD_UP) row_pick = (row_pick + ROWS - 1) % ROWS;
            if (down & CONT_DPAD_DOWN) row_pick = (row_pick + 1) % ROWS;
            if (down & CONT_DPAD_LEFT) row_change(-1);
            if (down & (CONT_DPAD_RIGHT | CONT_A)) row_change(1);
            if (down & (CONT_B | CONT_START)) hud_show = 0;
        } else {
            if (down & CONT_DPAD_UP) torch_toggle();
            if (down & CONT_DPAD_DOWN) hud_show = 1;
            if (down & CONT_START) measure_start();
            if (held & CONT_Y) mv_f += 1.0f;
            if (held & CONT_A) mv_f -= 1.0f;
            if (held & CONT_X) mv_r -= 1.0f;
            if (held & CONT_B) mv_r += 1.0f;
        }
        if (shz_fabsf((float)st->joyx) > STICK_DEAD) look_x = (float)st->joyx * (1.0f / 128.0f);
        if (shz_fabsf((float)st->joyy) > STICK_DEAD) look_y = (float)st->joyy * (1.0f / 128.0f);
    } else {                      /* dual stick */
        if (down & CONT_DPAD_UP) row_pick = (row_pick + ROWS - 1) % ROWS;
        if (down & CONT_DPAD_DOWN) row_pick = (row_pick + 1) % ROWS;
        if (down & CONT_DPAD_LEFT) row_change(-1);
        if (down & (CONT_DPAD_RIGHT | CONT_A)) row_change(1);
        if (down & CONT_Y) hud_show ^= 1;
        if (down & CONT_START) measure_start();
        if (down & CONT_X) torch_toggle();
        if (shz_fabsf((float)st->joyy) > STICK_DEAD) mv_f = -(float)st->joyy * (1.0f / 128.0f);
        if (shz_fabsf((float)st->joyx) > STICK_DEAD) mv_r = (float)st->joyx * (1.0f / 128.0f);
        if (shz_fabsf((float)st->joy2x) > STICK_DEAD) look_x = (float)st->joy2x * (1.0f / 128.0f);
        if (shz_fabsf((float)st->joy2y) > STICK_DEAD) look_y = (float)st->joy2y * (1.0f / 128.0f);
    }
    cam_yaw += look_x * TURN_SPEED * dt;
    /* INVERT Y on (the default): the stick up (a negative value) looks down; off: up looks up */
    cam_pitch = shz_clampf(cam_pitch + (opt[ROW_INVY] ? 1.0f : -1.0f) * look_y * TURN_SPEED * dt, -PITCH_LIMIT, PITCH_LIMIT);
    camera_axes();
    {   /* walk on the floor plane: the heading's horizontal part, never up or down */
        float sy, cy;
        sh4der_sincos_(cam_yaw, &sy, &cy);
        cam[0] += (sy * mv_f + cy * mv_r) * WALK_SPEED * dt;
        cam[2] += (cy * mv_f - sy * mv_r) * WALK_SPEED * dt;
        cam[0] = shz_clampf(cam[0], -ROOM_HALF + WALK_RADIUS, ROOM_HALF - WALK_RADIUS);   /* the walls: stop at them */
        cam[2] = shz_clampf(cam[2], -ROOM_HALF + WALK_RADIUS, ROOM_HALF - WALK_RADIUS);
    }
}

int main(void) {
    pvr_init_params_t init = pvr_default_params;
    uint64_t previous, cpu_t0, waited = 0;   /* waited: us spent blocked on the PVR this frame, not CPU work */
    unsigned frame = 0, stuck = 0, stuck_shown = 0, hud_tick = 0;
    uint64_t win_cpu_us = 0, win_frame_us = 0, win_rnd_ns = 0, win_ta_b = 0, win_t0;   /* the one-second window: cpu, frame and render sums */
    unsigned win_frames = 0, serial_lines = 0;
    int win_state = -1, have_on = 0, have_off = 0;   /* -1 empty, 0 effect off, 1 on, 2 mixed (not compared) */
    float on_cpu = 0.0f, on_rnd = 0.0f, off_cpu = 0.0f, off_rnd = 0.0f, on_ta = 0.0f, off_ta = 0.0f;
    init.vertex_buf_size = 1536 * 1024;   /* the vertex method at its top TRI CAP needs about 0.8 MB of vertices a frame */
    init.opb_overflow_count = 6;
    init.autosort_disabled = 1;   /* the translucent list is drawn in the order it is sent */
    if (pvr_init(&init) < 0) {
        printf("torchwalk: pvr_init FAIL\n");
        return 1;
    }
    pvr_set_bg_color(0.0f, 0.0f, 0.0f);
    hud_mem = pvr_mem_malloc(sizeof(hud_pixels));
    prompt_mem = pvr_mem_malloc(sizeof(prompt_pixels));
    fprompt_mem = pvr_mem_malloc(sizeof(fprompt_pixels));
    ab_label_mem = pvr_mem_malloc(sizeof(ab_label_px));
    if (!hud_mem || !prompt_mem || !fprompt_mem || !ab_label_mem) {
        printf("torchwalk: text texture allocation FAIL\n");
        return 1;
    }
    make_textures();
    ab_make_maps();
    hgt_floor_ref = height_median(hgt_floor, hgt_floor_side);
    hgt_wall_ref = height_median(hgt_wall, hgt_wall_side);
    make_prompt();
    vram_gmap_b = G_COUNT * (g_bytes[0] + g_bytes[1]);
    vram_floor_b = tex_baked.bytes + tex_clean.bytes;
    vram_wall_b = tex_wall.bytes;
    vram_text_b = (unsigned)(sizeof(hud_pixels) + sizeof(prompt_pixels) + sizeof(fprompt_pixels) + sizeof(ab_label_px));
    make_headers();
    room_init();
    {   /* the lens grain tile (its 64 x 64 texture): the only memory the lens takes, measured at the call */
        const unsigned long m0 = (unsigned long)pvr_mem_available();
        sh4der_lens_init();
        vram_lens_b = (unsigned)(m0 - (unsigned long)pvr_mem_available());
    }
    sh4der_lens_vignette_setup(SCR_W, SCR_H);
    camera_axes();
    cones_update();
    hud_update();   /* the readout once now: its texture holds nothing until the first update */
    ab_label_update();
    printf("torchwalk: VRAM effect maps %u torch %u lens %u = %u KB; base floor %u wall %u text %u = %u KB\n", KB(vram_gmap_b), KB(vram_torch_b), KB(vram_lens_b),
           KB(vram_gmap_b + vram_torch_b + vram_lens_b), KB(vram_floor_b), KB(vram_wall_b), KB(vram_text_b), KB(vram_floor_b + vram_wall_b + vram_text_b));
    printf("torchwalk: %s, textures %u KB, VRAM free %lu KB\n", private_textures ? "private textures" : "placeholder textures", tex_kb_total,
           (unsigned long)(pvr_mem_available() / 1024u));
    previous = win_t0 = timer_us_gettime64();
    for (;;) {
        const uint64_t now = timer_us_gettime64();
        const float dt = shz_fminf((float)(now - previous) * 1.0e-6f, 0.1f);
        uint64_t t, t_scene, t_tr = 0, t_torch = 0, t_lens = 0, t_wire = 0;
        int ready, eff_state;
        pvr_stats_t stats;
        previous = now;
        cpu_t0 = timer_us_gettime64();
        waited = 0;
        ta_bytes = 0;
        t = timer_us_gettime64();
        input(dt);
        gun_step(dt, frame);
        ab_tick(dt);
        if (ab_reset_request) {   /* a new measurement: the old on / off pair is not mixed with it */
            have_on = have_off = 0;
            ab_reset_request = 0;
        }
        torch_power = sh4der_torch_power_step(torch_power, opt[ROW_TORCH], dt);
        if (prompt_state == 1) {
            prompt_t += dt;
            if (prompt_t >= PROMPT_FADE_S)
                prompt_state = 2;
        }
        slice_us[SL_INPUT] += (uint32_t)(timer_us_gettime64() - t);
        cones_update();
        build_cells();
        {   /* the wait for the last frame is the PVR's time, not the CPU's: out of the cpu figure */
            const uint64_t w0 = timer_us_gettime64();
            ready = pvr_wait_ready();
            waited += timer_us_gettime64() - w0;
        }
        if (ready < 0) {   /* KOS times a wait out at 100 ms: twenty in a row means the PVR never finished a frame */
            if (++stuck >= 20u && !stuck_shown) {
                stuck_shown = 1;
                vid_clear(255, 255, 255);
                printf("torchwalk: PVR stuck after %u frames: TA %u KB\n", frame, ta_kb);
            }
        } else {
            stuck = 0;
        }
        if (++hud_tick >= 20u) {   /* the readout text, every 20 frames: after the last render, so the upload does not tear */
            const uint64_t w0 = timer_us_gettime64();
            hud_tick = 0;
            pvr_wait_render_done();
            waited += timer_us_gettime64() - w0;
            ab_label_update();
            if (hud_show) {
                const uint64_t h0 = timer_us_gettime64();
                uint32_t h;
                hud_update();
                h = (uint32_t)(timer_us_gettime64() - h0);
                slice_us[SL_HUD] += h;
                if (h > slice_hud_max)
                    slice_hud_max = h;
            }
        }
        floor_prompt_update();
        t_scene = timer_us_gettime64();
        pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        sky_draw();
        floor_op();
        pvr_list_finish();
        pvr_list_begin(PVR_LIST_TR_POLY);
        t = timer_us_gettime64();
        {
            const uint32_t b0 = ta_bytes;
            floor_tr();   /* the normal map's passes (the effect's submit) */
            ab_ta = ta_bytes - b0;
        }
        t_tr = timer_us_gettime64() - t;
        if (torch_power > 0.0f) {   /* the library's torch: the dark carved outside the beam's circle, then the hot core boost */
            static sh4der_torch_vert_t tv[SH4DER_TORCH_VERTS];
            float rx, ry, hot_u;
            unsigned n;
            t = timer_us_gettime64();
            sh4der_torch_radii(FOCAL, FOCAL, sh4der_torch_tan_of_cos2(torch_cos2_outer), &rx, &ry);
            n = sh4der_torch_carve_mesh_to(tv, SCR_W * 0.5f, SCR_H * 0.5f, rx, ry, torch_power, sh4der_torch_umax(SCR_W * 0.5f, SCR_H * 0.5f, rx, ry, SCR_W, SCR_H));
            sh4der_torch_draw(&hdr_torch[0], tv, n, TORCH_Z);
            ta_bytes += 32u + 32u * n;
            hot_u = SH4DER_TORCH_HOT_U * sh4der_torch_tan_of_cos2(torch_cos2_inner) * shz_invf(sh4der_torch_tan_of_cos2(torch_cos2_outer)) *
                    (1.0f / 0.43185f) * hot_scale[opt[ROW_HOT]];
            n = sh4der_torch_boost_mesh_core(tv, SCR_W * 0.5f, SCR_H * 0.5f, rx, ry, torch_power, SH4DER_TORCH_TINT_G, SH4DER_TORCH_TINT_B, hot_u);
            sh4der_torch_draw(&hdr_torch[1], tv, n, TORCH_Z + 0.25f);
            ta_bytes += 32u + 32u * n;
            t_torch = timer_us_gettime64() - t;
        }
        t = timer_us_gettime64();
        if (opt[ROW_VIGNETTE]) {
            sh4der_lens_vignette_draw();
            ta_bytes += 32u + 32u * ((SH4DER_LENS_VIG_RINGS + 1u) * 2u * (SH4DER_LENS_VIG_SPOKES + 1u));   /* from the lens's constants: 9 bands x 66 */
        }
        if (opt[ROW_GRAIN]) {
            sh4der_lens_grain(frame, SCR_W, SCR_H);
            ta_bytes += 2u * 32u + 8u * 32u;
        }
        t_lens = timer_us_gettime64() - t;
        t = timer_us_gettime64();
        wire_draw();   /* over the scene, under the text; not part of the effect */
        t_wire = timer_us_gettime64() - t;
        wire_kb = (wire_bytes + 1023u) / 1024u;
        if (prompt_state != 2) {   /* the prompt: 2x, centred low; fades by alpha with an ease (smoothstep) over PROMPT_FADE_S once X is pressed */
            const float a = prompt_state == 0 ? 1.0f : 1.0f - shz_smoothstepf(prompt_t, 0.0f, PROMPT_FADE_S);
            quad_draw(&hdr_prompt, (SCR_W - 512.0f) * 0.5f, 392.0f, 512.0f, 16.0f, ((uint32_t)(a * 255.0f + 0.5f) << 24) | 0x00ffffffu);
        }
        if (hud_show)
{
            quad_v1 = 72.0f / (float)HUD_H;
            quad_draw(&hdr_hud, 20.0f, 18.0f, 512.0f, 72.0f, 0xffc0c0c0u);
            quad_v1 = 1.0f;
        }
        quad_draw(&hdr_fprompt, 16.0f, 456.0f, 256.0f, 8.0f, 0xffe0e0e0u);   /* the floor-mode prompt */
        quad_v1 = 36.0f / 64.0f;
        quad_draw(&hdr_ablabel, SCR_W - 256.0f - 16.0f, 426.0f, 256.0f, 36.0f, 0xffe0e0e0u);   /* the A/B label: four lines, whole texels */
        quad_v1 = 1.0f;
        pvr_list_finish();
        pvr_scene_finish();
        {   /* the submit slice is the whole scene minus the effect's and the lens's own parts */
            const uint64_t all = timer_us_gettime64() - t_scene;
            slice_us[SL_SUBMIT] += (uint32_t)(all - t_tr - t_torch - t_lens - t_wire);
            slice_us[SL_WIRE] += (uint32_t)t_wire;
            slice_us[SL_BUMPSUB] += (uint32_t)t_tr;
            slice_us[SL_TORCH] += (uint32_t)t_torch;
            slice_us[SL_LENS] += (uint32_t)t_lens;
        }
        pvr_get_stats(&stats);
        ta_counted_kb = (ta_bytes + 1023u) / 1024u;
        ta_kb = (unsigned)(stats.vtx_buffer_used / 1024u);   /* what KOS read from the TA register at the list-done interrupt: 0 when that register is not there */
        ta_max_kb = (unsigned)(stats.vtx_buffer_used_max / 1024u);
        win_cpu_us += timer_us_gettime64() - cpu_t0 - waited;
        win_frame_us += (uint64_t)(dt * 1.0e6f);
        win_rnd_ns += stats.rnd_last_time;
        win_ta_b += ta_bytes;   /* the on / off comparison uses what we counted */
        win_frames++;
        /* the effect (bump + torch) is on when the floor shows a bump or the torch is on; a window counts for the on / off comparison
           only when that stayed the same, and steady (the torch ramp finished), the whole second */
        eff_state = ((opt[ROW_BUMP] && opt[ROW_FLOOR]) || opt[ROW_TORCH]) ? 1 : 0;
        if (torch_power > 0.0f && torch_power < 1.0f)
            win_state = 2;
        else if (win_state == -1)
            win_state = eff_state;
        else if (win_state != eff_state)
            win_state = 2;
        frame++;
        if (timer_us_gettime64() - win_t0 >= 1000000u) {   /* one second: the readout's ms, the effect, one serial line */
            const float fr = (float)win_frames;
            cpu_ms = (float)win_cpu_us * 1.0e-3f / fr;
            frame_ms = (float)win_frame_us * 1.0e-3f / fr;
            render_ms = (float)win_rnd_ns * 1.0e-6f / fr;
            eff_own_ms = (float)((opt[ROW_METHOD] ? slice_us[SL_TESS] + slice_us[SL_TSEND] : slice_us[SL_BUMPLIGHT] + slice_us[SL_BUMPSUB]) + slice_us[SL_TORCH]) * 1.0e-3f / fr;
            tess_ms = (float)slice_us[SL_TESS] * 1.0e-3f / fr;
            tplan_ms = (float)slice_us[SL_TPLAN] * 1.0e-3f / fr;
            tshade_ms = (float)slice_us[SL_TSHADE] * 1.0e-3f / fr;
            temit_ms = (float)slice_us[SL_TEMIT] * 1.0e-3f / fr;
            tsend_ms = (float)slice_us[SL_TSEND] * 1.0e-3f / fr;
            if (win_state == 1) { on_cpu = cpu_ms; on_rnd = render_ms; on_ta = (float)win_ta_b / fr * (1.0f / 1024.0f); have_on = 1; }
            else if (win_state == 0) { off_cpu = cpu_ms; off_rnd = render_ms; off_ta = (float)win_ta_b / fr * (1.0f / 1024.0f); have_off = 1; }
            eff_delta_valid = have_on && have_off;
            if (eff_delta_valid) {
                eff_cpu_delta_ms = on_cpu - off_cpu;
                eff_rnd_delta_ms = on_rnd - off_rnd;
                eff_ta_delta_kb = on_ta - off_ta;
            }
            if (serial_lines < 60u) {   /* capped: serial is 115200 baud */
                serial_lines++;
                printf("torchwalk: cpu %lu us (in %lu corners %lu clip %lu proj %lu bumplight %lu hud %lu max %lu submit %lu bumpsub %lu torch %lu lens %lu wire %lu) frm %lu rnd %lu us "
                       "TA reg %u/%u counted %u KB cells %u effect %lu us | lit %u normal map TA %lu B\n",
                       (unsigned long)(win_cpu_us / win_frames), (unsigned long)(slice_us[SL_INPUT] / win_frames), (unsigned long)(slice_us[SL_CORNERS] / win_frames),
                       (unsigned long)(slice_us[SL_CLIP] / win_frames), (unsigned long)(slice_us[SL_PROJECT] / win_frames), (unsigned long)(slice_us[SL_BUMPLIGHT] / win_frames),
                       (unsigned long)(slice_us[SL_HUD] / win_frames), (unsigned long)slice_hud_max, (unsigned long)(slice_us[SL_SUBMIT] / win_frames),
                       (unsigned long)(slice_us[SL_BUMPSUB] / win_frames), (unsigned long)(slice_us[SL_TORCH] / win_frames), (unsigned long)(slice_us[SL_LENS] / win_frames), (unsigned long)(slice_us[SL_WIRE] / win_frames),
                       (unsigned long)(win_frame_us / win_frames), (unsigned long)(win_rnd_ns / 1000u / win_frames), ta_kb, ta_max_kb, ta_counted_kb, cell_count,
                       (unsigned long)(((opt[ROW_METHOD] ? slice_us[SL_TESS] + slice_us[SL_TSEND] : slice_us[SL_BUMPLIGHT] + slice_us[SL_BUMPSUB]) + slice_us[SL_TORCH]) / win_frames),
                       ab_lit, (unsigned long)ab_ta);
            }
            memset(slice_us, 0, sizeof(slice_us));
            slice_hud_max = 0;
            win_cpu_us = win_frame_us = win_rnd_ns = win_ta_b = 0;
            win_frames = 0;
            win_state = -1;
            win_t0 = timer_us_gettime64();
        }
    }
}
