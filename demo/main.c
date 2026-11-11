/* SH4DER-PVR demo disc

   demo/main.c
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
   Written with Cortana (Claude, an AI by Anthropic).

   One program that shows the library's effects on a small scene of its own: a dark hall that opens on a sunlit yard, and a few box-built
   models (tools/make_models.py) as actors. Every effect is a row on the read-out, switched and tuned live.
   Build it with `make demo` in the repository's root (see the README's "The demo disc").

   Pad: d-pad left / right picks a row, A switches it on or off, d-pad up / down changes its strength (1.00 = the library's number),
   Y hides the read-out. Left stick: walk and strafe. Second stick: turn and look (up looks down). R / L trigger: look up / down.
   With CAR on, the left stick drives.

   Rows:
     EYE      sh4der_look    the picture adapts like an eye: into light 2 s, into dark 6 s
     BLOOM    sh4der_glow    bright things spill a soft glow
     TORCH    sh4der_torch   a flashlight: near light per vertex, the dark carved round a circle, a hot core
     RAYS     sh4der_glow    god rays from the sun (needs BLOOM)
     PROBE    -              what the frame read saw and the bloom's map, at the bottom of the screen
     CURVE    sh4der_glow    the bloom's ramp: squared or straight, and its threshold
     SH4FTS   sh4der_shafts  dark shapes cast shade away from the sun
     CAR      -              drive the station wagon; its two flood lamps light the yard
     NIGHT    -              the yard at night, no sun
     WEDGE    -              with CAR: the world outside the lamps' fan is dimmed
     TAA      sh4der_taa     the frame before blended in; the number is the old frame's share
     TAAHOLD  -              on: the blend accumulates; off: only the one frame before
     TAAMODE  -              DYNAMIC: less of the old frame while the camera moves; MANUAL: a fixed share
     CA       sh4der_ca      chromatic aberration through the frame texture
     RES      sh4der_ca      the frame texture at 640 x 480 or 320 x 240
     SH4DOW   SH4DOW-DC      soft self-shadow on the four nearest actors, foot cones under the two-legged ones
     DR       -              the world's vertices by KOS direct render (on) or one pvr_prim call each (off)

   The models have no skeleton: SH4DOW-DC's limbs are made at start from the vertices (shadow_models). A game passes its posed bones.
   All maths is SH4ZAM (the view transform: XMTRX, one FTRV a vertex); all drawing is KallistiOS PVR. A serial line every two seconds
   carries the numbers. */
#include <kos.h>
#include <dc/minifont.h>
#include <stdio.h>
#include <string.h>
#include <sh4zam/shz_sh4zam.h>
#include "sh4der_look.h"
#include "sh4der_glow.h"
#include "sh4der_torch.h"
#include "sh4dow_dc.h"       /* SH4DOW-DC: the soft self-shadow and the foot cones */
#include "sh4der_trail.h"
#include "sh4der_ca.h"       /* the frame texture, chromatic aberration */
#include "sh4der_shafts.h"   /* shadow shafts */   /* the front buffer copy and, with it, sh4der_taa.h */
#include "models.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

#define SCR_W 640.0f
#define SCR_H 480.0f
#define FOCAL 457.0f      /* 320 / tan(35 degrees) */
#define NEAR_F 0.08f
#define FAR_F 70.0f
static float cam_z = 0.62f;   /* the eye's height: on foot 0.62, behind the car higher */
#define EYE_Z cam_z
#define LOOK_Z 435.0f
#define TORCH_Z 432.0f
#define BLOOM_Z 447.0f
#define HUD_Z 1000.0f
#define TORCH_TAN 0.5774f /* a 30 degree edge */

typedef struct { float p[3][3], n[3], uv[3][2]; uint8_t c[3], kind; } wtri_t;   /* kind 0: textured, lit; 1: emissive, untextured */
typedef struct { float f, r, u, cr, cg, cb, tu, tv; } vv_t;                     /* a view-space vertex: forward, right, up, colour, uv */
typedef struct { int model; float x, y, z, yaw, scale; } actor_t;

#define WORLD_MAX 1100
static wtri_t world[WORLD_MAX];
static unsigned world_n;

static const float lamp_pos[5][3] = { { -9.5f, 0.0f, 1.45f }, { -5.5f, 0.0f, 1.45f }, { -1.5f, 0.0f, 1.45f }, { -11.6f, 0.0f, 0.8f }, { 4.0f, 4.6f, 0.9f } };
static const float lamp_rgb[5][3] = { { 0.55f, 0.48f, 0.34f }, { 0.55f, 0.48f, 0.34f }, { 0.55f, 0.48f, 0.34f }, { 0.75f, 0.15f, 0.65f }, { 0.2f, 0.6f, 0.8f } };
static const float lamp_r2[5] = { 6.0f, 6.0f, 6.0f, 9.0f, 9.0f };
static const float lamp_inv_r2[5] = { 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 6.0f, 1.0f / 9.0f, 1.0f / 9.0f };   /* a vertex does not divide for a constant */
static const float sun_dir[3] = { 0.775f, 0.382f, 0.503f };   /* 30 degrees up, over the yard's far corner */

static actor_t actors[] = {
    { M_DUDE, -6.5f, 0.9f, 0.0f, 0.0f, 1.0f },      /* turns on the spot */
    { M_DUDE, -2.6f, -1.0f, 0.0f, 2.4f, 1.0f },
    { M_CRATE, -9.0f, -1.4f, 0.0f, 0.3f, 1.0f },
    { M_CRATE, -9.7f, -1.1f, 0.0f, 1.1f, 1.0f },
    { M_LAMP, 4.0f, 4.6f, 0.0f, 0.0f, 0.8f },
    { M_WAGON, 4.0f, -4.3f, 0.0f, 0.0f, 1.0f },      /* slides along the yard's near side wall, away from the sun's corner */
    { M_JET, 10.0f, -4.4f, 2.2f, 0.6f, 0.5f },   /* likewise: the sun's side of the sky stays clear */
};
#define ACTORS (sizeof actors / sizeof actors[0])

static float cam_x = -10.0f, cam_y = 0.0f, cam_yaw = 0.0f, cam_pitch = 0.0f, cam_s, cam_c, cam_ps, cam_pc;
enum { FX_EYE, FX_BLOOM, FX_TORCH, FX_RAYS, FX_PROBE, FX_CURVE, FX_SHAFTS, FX_CAR, FX_NIGHT, FX_WEDGE, FX_TAA, FX_TAAHOLD, FX_TAAMODE, FX_CA, FX_RES, FX_SHADOW, FX_DR, FX_COUNT };
static const char *const fx_name[FX_COUNT] = { "EYE", "BLOOM", "TORCH", "RAYS", "PROBE", "CURVE", "SH4FTS", "CAR", "NIGHT", "WEDGE", "TAA", "TAAHOLD", "TAAMODE", "CA", "RES", "SH4DOW", "DR" };
static const float fx_max[FX_COUNT] = { 2.0f, 4.0f, 1.5f, 4.0f, 1.0f, 0.9f, 3.0f, 3.0f, 1.0f, 1.0f, 1.25f, 1.0f, 1.0f, 4.0f, 1.0f, 1.5f, 1.0f };   /* CAR: the lamps' strength; WEDGE: how dark outside (1 = most) */
static const float fx_min[FX_COUNT] = { 0.25f, 0.25f, 0.25f, 0.25f, 1.0f, 0.2f, 0.25f, 0.25f, 1.0f, 0.25f, 0.05f, 1.0f, 1.0f, 0.25f, 1.0f, 0.25f, 1.0f };
static const float fx_step[FX_COUNT] = { 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.05f, 0.25f, 0.25f, 0.25f, 0.25f, 0.05f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f, 0.25f };
static int hud_show = 1;   /* Y: the read-out on / off */
static int fx_on[FX_COUNT] = { 1, 1, 0, 1, 0, 0, 0, 0, 0, 1, 0, 1, 1, 1, 0, 1, 1 }, fx_pick = FX_DR;   /* at start: the recommended effects on, TAA off, CA on, RES off = 640 x 480 */     /* her defaults 2026-11-11; CURVE off: the straight ramp; TAAMODE on: DYNAMIC */
static float fx_level[FX_COUNT] = { 0.25f, 1.0f, 1.0f, 1.0f, 1.0f, 0.80f, 0.5f, 1.0f, 1.0f, 0.5f, 0.90f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f, 1.0f };   /* the recommended settings (README); x the library's number (CURVE: the threshold) */
#define TAA_MOVING 0.30f   /* DYNAMIC: the old frame's share while the camera moves (recommended: still 68 %, moving 30 %) */
#define TAA_WALK_FULL 1.5f   /* wu / s: from this walking speed the camera counts as moving */
#define fx_eye fx_on[FX_EYE]
#define fx_bloom fx_on[FX_BLOOM]
#define fx_torch fx_on[FX_TORCH]
#define fx_rays fx_on[FX_RAYS]
static float torch_power, torch_k;   /* the ramp; power x the darkness gate */
static sh4der_torch_gate_t torch_gate;

static pvr_poly_hdr_t hdr_world, hdr_flat, hdr_hud, hdr_look[4], hdr_torch[2];
static pvr_ptr_t tex_brick, tex_hud;
static uint16_t hud_buf[512 * 128] __attribute__((aligned(32)));
static char hud_text[6][64];
static sh4der_taa_t taa;
static sh4der_taa_frame_t taa_f;
static sh4der_ca_gpu_t ca_gpu;   /* the library's frame textures (TAA: two in turn, the history; CA or RES alone: one) */
#define taa_tex ca_gpu.tex
static int rt_half = -1;   /* 1: the frame texture is 320 x 240; 0: 640 x 480; -1: not set up yet */
static unsigned rt_n, rt_w = 640, rt_h = 480, rt_tw = 1024, rt_th = 512;   /* how many are allocated; the render size; the texture size its header names */
static float rt_scale = 1.0f, rt_s = 1.0f;   /* the frame texture's size against the screen; what the world pass is drawn at this frame (1 straight to the screen) */
#define CA_SPLIT_PX SH4DER_CA_SPLIT_PX   /* CA 1.00: red and blue 3 screen pixels apart in the corner */
static int taa_cur, taa_have;   /* the one this frame renders into; 1: the other holds the frame before */
static float sun_facing;   /* the cosine between the view axis and the sun this frame; 0: not in view */
static float jit_x, jit_y;   /* this frame's sub-pixel shift of the projection centre (TAA) */
static float car_x = 6.0f, car_y = 0.0f, car_yaw = 0.0f, car_speed, car_fx, car_fy;   /* the car: where, heading, speed, its forward */

static sh4der_look_bins_t look_bins;
static sh4der_look_params_t look_params;
static sh4der_look_eye_t look_eye;
static sh4der_look_layer_t look_layers[SH4DER_LOOK_LAYERS];
static int look_n;
static uint16_t look_grid[SH4DER_LOOK_GRID_X * SH4DER_LOOK_GRID_Y];
static sh4der_glow_t bloom;
static int bloom_ok, bloom_have;
static uint32_t read_us, tris_drawn;
/* Which frame the read saw: every frame carries a 3-bit stamp (a 4 x 4 pixel block at the top left, each channel full or none), and the gain and the glow that frame was
 * drawn with are kept by stamp. The frame on screen is one or two frames old and that changes from frame to frame: taking "last frame's" off the read made the eye and
 * the bloom pulse (the gain swung between 0.50 and 2.00 at a strong eye). */
static unsigned frame_id, read_id;
static float hist_gain_ln[8], hist_glow[8], hist_gain[8] = { 1, 1, 1, 1, 1, 1, 1, 1 };   /* by stamp: the eye's log gain laid over that frame, the bloom strength drawn on it */
static pvr_poly_hdr_t hdr_stamp, hdr_probe, hdr_probe_map, hdr_black;
static pvr_ptr_t tex_probe;
static sh4der_shafts_gpu_t shafts_gpu;   /* the library's shade map texture */
static int shade_dirty = 1;   /* the map holds a shade: clear it once when the light leaves the view */
static uint16_t shade_buf[128 * 64] __attribute__((aligned(32)));   /* SH4FTS: what the dark things cast, grey RGB565 */
static uint16_t probe_buf[128 * 64] __attribute__((aligned(32)));
static uint16_t raw_grid[SH4DER_LOOK_GRID_X * SH4DER_LOOK_GRID_Y];   /* the read's first sample, untouched */
static float read_lum, map_mean;   /* the read's mean luma 0..1, the bloom map's mean 0..1: the read-out's proof that the frame read works */

/* ---------- the world ---------- */

static void add_tri(const float a[3], const float b[3], const float c[3], const float n[3], float ua, float va, float ub, float vb, float uc, float vc, const uint8_t col[3], int kind) {
    if (world_n >= WORLD_MAX) return;
    wtri_t *t = &world[world_n++];
    memcpy(t->p[0], a, 12); memcpy(t->p[1], b, 12); memcpy(t->p[2], c, 12); memcpy(t->n, n, 12);
    t->uv[0][0] = ua; t->uv[0][1] = va; t->uv[1][0] = ub; t->uv[1][1] = vb; t->uv[2][0] = uc; t->uv[2][1] = vc;
    memcpy(t->c, col, 3); t->kind = (uint8_t)kind;
}
/* A quad o, o + e1, o + e1 + e2, o + e2, uv 0..su, 0..sv. */
static void add_quad(float ox, float oy, float oz, float e1x, float e1y, float e1z, float e2x, float e2y, float e2z, float nx, float ny, float nz, float su, float sv, uint8_t r, uint8_t g, uint8_t b, int kind) {
    const float p0[3] = { ox, oy, oz }, p1[3] = { ox + e1x, oy + e1y, oz + e1z }, p2[3] = { ox + e1x + e2x, oy + e1y + e2y, oz + e1z + e2z }, p3[3] = { ox + e2x, oy + e2y, oz + e2z };
    const float n[3] = { nx, ny, nz };
    const uint8_t col[3] = { r, g, b };
    add_tri(p0, p1, p2, n, 0, 0, su, 0, su, sv, col, kind);
    add_tri(p0, p2, p3, n, 0, 0, su, sv, 0, sv, col, kind);
}

static void world_build(void) {
    for (int x = -12; x < 0; x++) {
        for (int y = -2; y < 2; y++) {
            add_quad((float)x, (float)y, 0.0f, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 190, 190, 195, 0);          /* hall floor */
            add_quad((float)x, (float)y, 1.6f, 1, 0, 0, 0, 1, 0, 0, 0, -1, 1, 1, 150, 150, 160, 0);         /* ceiling */
        }
        for (int k = 0; k < 2; k++) {
            add_quad((float)x, -2.0f, 0.8f * (float)k, 1, 0, 0, 0, 0, 0.8f, 0, 1, 0, 1, 1, 205, 180, 160, 0);   /* walls */
            add_quad((float)x, 2.0f, 0.8f * (float)k, 1, 0, 0, 0, 0, 0.8f, 0, -1, 0, 1, 1, 205, 180, 160, 0);
        }
    }
    for (int y = -2; y < 2; y++)
        for (int k = 0; k < 2; k++) add_quad(-12.0f, (float)y, 0.8f * (float)k, 0, 1, 0, 0, 0, 0.8f, 1, 0, 0, 1, 1, 205, 180, 160, 0);   /* the hall's end */
    for (int x = 0; x < 14; x++)
        for (int y = -6; y < 6; y++) add_quad((float)x, (float)y, 0.0f, 1, 0, 0, 0, 1, 0, 0, 0, 1, 1, 1, 170, 185, 140, 0);   /* the yard */
    for (int y = -6; y < 6; y++) {
        if (y < -2 || y >= 2)
            for (int k = 0; k < 2; k++) add_quad(0.0f, (float)y, 0.8f * (float)k, 0, 1, 0, 0, 0, 0.8f, 1, 0, 0, 1, 1, 215, 200, 185, 0);   /* the hall's face */
        add_quad(14.0f, (float)y, 0.0f, 0, 1, 0, 0, 0, 1.0f, -1, 0, 0, 1, 1, 200, 200, 190, 0);
    }
    for (int x = 0; x < 14; x++) {
        add_quad((float)x, -6.0f, 0.0f, 1, 0, 0, 0, 0, 1.0f, 0, 1, 0, 1, 1, 200, 200, 190, 0);
        add_quad((float)x, 6.0f, 0.0f, 1, 0, 0, 0, 0, 1.0f, 0, -1, 0, 1, 1, 200, 200, 190, 0);
    }
    for (int i = 0; i < 3; i++) add_quad(lamp_pos[i][0] - 0.15f, -0.15f, 1.58f, 0.3f, 0, 0, 0, 0.3f, 0, 0, 0, -1, 1, 1, 255, 240, 200, 1);   /* the lamps */
    add_quad(-11.97f, -0.25f, 0.4f, 0, 0.5f, 0, 0, 0, 0.8f, 1, 0, 0, 1, 1, 255, 70, 230, 1);    /* a magenta panel on the end wall */
    add_quad(3.9f, 4.5f, 0.95f, 0.2f, 0, 0, 0, 0.2f, 0, 0, 0, 1, 1, 1, 120, 230, 255, 1);       /* the ground light's cyan top */
    add_quad(13.95f, -1.0f, 0.3f, 0, 2.0f, 0, 0, 0, 0.5f, -1, 0, 0, 1, 1, 255, 255, 255, 1);    /* a white strip on the far wall */
    for (int i = 0; i < 6; i++)   /* a slatted roof toward the sun: the rays' occluder (slats 0.3 wide, 0.7 apart, seen from below and from the side) */
        for (int x = 8; x < 13; x++) {
            add_quad((float)x, 0.6f + 0.7f * (float)i, 2.6f, 1, 0, 0, 0, 0.3f, 0, 0, 0, -1, 1, 0.3f, 170, 150, 130, 0);
            add_quad((float)x, 0.6f + 0.7f * (float)i, 2.6f, 1, 0, 0, 0, 0, 0.12f, 0, -1, 0, 1, 0.12f, 170, 150, 130, 0);
        }
}

/* ---------- lighting and projection ---------- */

static inline float sat(float x) { return shz_clampf(x, 0.0f, 1.0f); }
static inline float outdoor_at(float x) { return sat((x + 1.0f) * (1.0f / 1.5f)); }

/* Per-vertex light at p with normal n: ambient and sun by zone, the lamps, the torch's near add (distance only, no cone). */
static void shade(const float p[3], const float n[3], const uint8_t base[3], float out[3]) {
    const float o = outdoor_at(p[0]) * (fx_on[FX_NIGHT] ? 0.06f : 1.0f);   /* NIGHT: the yard nearly as dark as the hall */
    const float sun = o * 0.7f * shz_fmaxf(shz_vec3_dot(shz_vec3_init(n[0], n[1], n[2]), shz_vec3_init(sun_dir[0], sun_dir[1], sun_dir[2])), 0.0f);
    const float amb = shz_lerpf(0.05f, 0.36f, o);
    float l[3] = { amb + sun, amb + 0.97f * sun, amb + 0.9f * sun + 0.03f * o };
    for (int i = 0; i < 5; i++) {
        const float dx = p[0] - lamp_pos[i][0], dy = p[1] - lamp_pos[i][1], dz = p[2] - lamp_pos[i][2], d2 = dx * dx + dy * dy + dz * dz;
        if (d2 < lamp_r2[i]) {
            float a = 1.0f - d2 * lamp_inv_r2[i];
            a *= a;
            l[0] += a * lamp_rgb[i][0]; l[1] += a * lamp_rgb[i][1]; l[2] += a * lamp_rgb[i][2];
        }
    }
    if (fx_on[FX_CAR]) {   /* two flood lamps at the car's front corners: wide (full inside 40 degrees off the heading, gone at 65), long (18 wu), hot near the lamps */
        float lit = 0.0f, inside = 0.0f;
        for (int k = -1; k <= 1; k += 2) {
            const float lx = car_x + car_fx * 1.0f - car_fy * 0.55f * (float)k, ly = car_y + car_fy * 1.0f + car_fx * 0.55f * (float)k;
            const float dx = p[0] - lx, dy = p[1] - ly, dz = p[2] - 0.45f, d2 = dx * dx + dy * dy + dz * dz;
            if (d2 < 18.0f * 18.0f && d2 > 0.0001f) {
                const float c = (dx * car_fx + dy * car_fy) * shz_inv_sqrtf(d2);   /* cosine off the heading */
                const float cone = shz_smoothstepf(c, 0.4226f, 0.7660f), a = 1.0f - d2 * (1.0f / (18.0f * 18.0f));
                lit += cone * a * a * (0.55f + 0.9f * shz_fmaxf(1.0f - d2 * (1.0f / 9.0f), 0.0f));   /* the hot wash within 3 wu */
                inside = shz_fmaxf(inside, cone);
            }
        }
        lit *= fx_level[FX_CAR];
        if (fx_on[FX_WEDGE]) {   /* outside the wedge: what was there, dimmed (the lamps then read brighter); inside: untouched */
            const float dim = 1.0f - fx_level[FX_WEDGE] * 0.7f * (1.0f - inside);
            l[0] *= dim; l[1] *= dim; l[2] *= dim;
        }
        l[0] += lit; l[1] += lit * 0.93f; l[2] += lit * 0.8f;
    }
    if (torch_k > 0.0f) {
        const float dx = p[0] - cam_x, dy = p[1] - cam_y, dz = p[2] - EYE_Z;
        const float t = sh4der_torch_near(dx * dx + dy * dy + dz * dz, torch_k);
        l[0] += t; l[1] += t * SH4DER_TORCH_TINT_G; l[2] += t * SH4DER_TORCH_TINT_B;
    }
    for (int c = 0; c < 3; c++) out[c] = sat((float)base[c] * (1.0f / 255.0f) * l[c]);
}

/* The world-to-view matrix into XMTRX: rows right, forward, up (the yaw, then the pitch about the right axis), the eye's position folded in. Nothing between this and
 * the last to_view() of the pass may load XMTRX (sh4dow_shade does not; sh4dow_strength_packed_n would). */
static void view_load(void) {
    const shz_vec4_t r = shz_vec4_init(cam_s, -cam_c, 0.0f, -(cam_s * cam_x - cam_c * cam_y));
    const shz_vec4_t f = shz_vec4_init(cam_c * cam_pc, cam_s * cam_pc, cam_ps, -(cam_c * cam_pc * cam_x + cam_s * cam_pc * cam_y + cam_ps * EYE_Z));
    const shz_vec4_t u = shz_vec4_init(-cam_c * cam_ps, -cam_s * cam_ps, cam_pc, -(-cam_c * cam_ps * cam_x - cam_s * cam_ps * cam_y + cam_pc * EYE_Z));
    const shz_vec4_t w = shz_vec4_init(0.0f, 0.0f, 0.0f, 1.0f);
    shz_xmtrx_load_rows_4x4(&r, &f, &u, &w);
}

static inline void to_view(const float p[3], vv_t *v) {   /* one FTRV */
    const shz_vec3_t q = shz_xmtrx_transform_point3(shz_vec3_init(p[0], p[1], p[2]));
    v->r = q.x; v->f = q.y; v->u = q.z;
}

static inline uint32_t pack(float r, float g, float b) {
    return 0xff000000u | (uint32_t)(r * 255.0f) << 16 | (uint32_t)(g * 255.0f) << 8 | (uint32_t)(b * 255.0f);
}

/* Clip a triangle to the near plane and send it (a strip of 3 or 4). */
static void emit(const vv_t in[3]) {
    vv_t out[4];
    int n = 0;
    for (int i = 0; i < 3; i++) {
        const vv_t *a = &in[i], *b = &in[(i + 1) % 3];
        const int ia = a->f >= NEAR_F, ib = b->f >= NEAR_F;
        if (ia) out[n++] = *a;
        if (ia != ib) {
            const float t = (NEAR_F - a->f) * shz_invf(b->f - a->f);
            vv_t *o = &out[n++];
            o->f = NEAR_F;
            o->r = shz_lerpf(a->r, b->r, t); o->u = shz_lerpf(a->u, b->u, t);
            o->cr = shz_lerpf(a->cr, b->cr, t); o->cg = shz_lerpf(a->cg, b->cg, t); o->cb = shz_lerpf(a->cb, b->cb, t);
            o->tu = shz_lerpf(a->tu, b->tu, t); o->tv = shz_lerpf(a->tv, b->tv, t);
        }
    }
    if (n < 3) return;
    static const int order3[3] = { 0, 1, 2 }, order4[4] = { 0, 1, 3, 2 };
    const int *order = n == 3 ? order3 : order4;
    for (int i = 0; i < n; i++) {
        const vv_t *s = &out[order[i]];
        const float inv = shz_invf_fsrra(s->f);   /* s->f >= NEAR_F > 0 (clipped above): the positive-only inverse */
        pvr_vertex_t on_stack, *v = fx_on[FX_DR] ? pvr_dr_target() : &on_stack;   /* DR on: KOS direct render, the vertex is written in the store queue itself */
        v->flags = i == n - 1 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v->x = (SCR_W * 0.5f + jit_x + s->r * inv * FOCAL) * rt_s;
        v->y = (SCR_H * 0.5f + jit_y - s->u * inv * FOCAL) * rt_s;
        v->z = inv;
        v->u = s->tu; v->v = s->tv;
        v->argb = pack(s->cr, s->cg, s->cb);
        v->oargb = 0;
        if (fx_on[FX_DR]) pvr_dr_commit(v);
        else pvr_prim(v, sizeof(*v));   /* DR off: the way before, one call a vertex */
    }
    tris_drawn += (unsigned)n - 2u;
}

static void draw_world(int kind) {
    for (unsigned i = 0; i < world_n; i++) {
        const wtri_t *t = &world[i];
        vv_t v[3];
        if (t->kind != kind) continue;
        for (int k = 0; k < 3; k++) to_view(t->p[k], &v[k]);
        if ((v[0].f < NEAR_F && v[1].f < NEAR_F && v[2].f < NEAR_F) || (v[0].f > FAR_F && v[1].f > FAR_F && v[2].f > FAR_F)) continue;
        if (v[0].r > v[0].f * 1.2f + 1.5f && v[1].r > v[1].f * 1.2f + 1.5f && v[2].r > v[2].f * 1.2f + 1.5f) continue;   /* off the right, then the left */
        if (-v[0].r > v[0].f * 1.2f + 1.5f && -v[1].r > v[1].f * 1.2f + 1.5f && -v[2].r > v[2].f * 1.2f + 1.5f) continue;
        for (int k = 0; k < 3; k++) {
            float c[3];
            if (kind) { c[0] = (float)t->c[0] * (1.0f / 255.0f); c[1] = (float)t->c[1] * (1.0f / 255.0f); c[2] = (float)t->c[2] * (1.0f / 255.0f); }
            else shade(t->p[k], t->n, t->c, c);
            v[k].cr = c[0]; v[k].cg = c[1]; v[k].cb = c[2];
            v[k].tu = t->uv[k][0]; v[k].tv = t->uv[k][1];
        }
        emit(v);
    }
}

/* Texture RAM the demo cannot run without: none left is said on the serial line and the demo stops, not a NULL into a texture load. */
static pvr_ptr_t vram(size_t bytes) {
    pvr_ptr_t t = pvr_mem_malloc(bytes);
    if (!t) { printf("sh4der demo: no texture RAM for %u bytes\n", (unsigned)bytes); arch_exit(); }
    return t;
}

/* ---------- SH4DOW-DC: the soft self-shadow on the nearest four actors, foot cones under every biped ---------- */

#define SHADOW_ACTORS 4
#define BLOB_MAX 12
typedef struct {
    float c[BLOB_MAX][3], r[BLOB_MAX];   /* the limb spheres, model space */
    unsigned n;
    float height, foot[2][3], foot_r;    /* a biped's two foot spheres */
    int feet;
} model_shadow_t;
static model_shadow_t model_shadow[M_COUNT];
static uint8_t *model_bone[M_COUNT];     /* each vertex's limb, or 255 */
static sh4dow_frame_t shadow_frame[SHADOW_ACTORS];
static const sh4dow_frame_t *shadow_of[ACTORS];
static shz_sincos_t actor_sc[ACTORS];   /* each actor's yaw as sine and cosine, once a frame (shadow_step) */
static pvr_poly_hdr_t hdr_foot;
static pvr_ptr_t tex_foot;
static unsigned cones_drawn, shadowed;

static void shadow_models(void) {
    enum { GX = 2, GY = 3, GZ = 4, CELLS = GX * GY * GZ };
    for (int mi = 0; mi < M_COUNT; mi++) {
        const demo_model_t *m = &demo_models[mi];
        model_shadow_t *ms = &model_shadow[mi];
        float lo[3] = { 1e9f, 1e9f, 1e9f }, hi[3] = { -1e9f, -1e9f, -1e9f }, sum[CELLS][3], spread[CELLS];
        unsigned cnt[CELLS];
        uint8_t slot[CELLS];
        memset(sum, 0, sizeof sum); memset(spread, 0, sizeof spread); memset(cnt, 0, sizeof cnt); memset(slot, 255, sizeof slot);
        model_bone[mi] = malloc(m->n);
        if (!model_bone[mi]) continue;
        for (unsigned i = 0; i < m->n; i++)
            for (int k = 0; k < 3; k++) { lo[k] = shz_fminf(lo[k], m->v[i * 6u + (unsigned)k]); hi[k] = shz_fmaxf(hi[k], m->v[i * 6u + (unsigned)k]); }
        const float ix = (float)GX * shz_invf(hi[0] - lo[0] + 1e-5f), iy = (float)GY * shz_invf(hi[1] - lo[1] + 1e-5f), iz = (float)GZ * shz_invf(hi[2] - lo[2] + 1e-5f);
        for (unsigned i = 0; i < m->n; i++) {
            const float *v = &m->v[i * 6u];
            unsigned cx = (unsigned)((v[0] - lo[0]) * ix), cy = (unsigned)((v[1] - lo[1]) * iy), cz = (unsigned)((v[2] - lo[2]) * iz);
            cx = cx < GX ? cx : GX - 1u; cy = cy < GY ? cy : GY - 1u; cz = cz < GZ ? cz : GZ - 1u;   /* a rounding at the box's far face stays in the last cell */
            const unsigned cell = (cz * GY + cy) * GX + cx;
            model_bone[mi][i] = (uint8_t)cell;
            cnt[cell]++;
            for (int k = 0; k < 3; k++) sum[cell][k] += v[k];
        }
        for (unsigned c = 0; c < CELLS; c++) if (cnt[c]) for (int k = 0; k < 3; k++) sum[c][k] *= shz_invf((float)cnt[c]);   /* the centres */
        for (unsigned i = 0; i < m->n; i++) {
            const float *v = &m->v[i * 6u], *c = sum[model_bone[mi][i]];
            spread[model_bone[mi][i]] += (v[0] - c[0]) * (v[0] - c[0]) + (v[1] - c[1]) * (v[1] - c[1]) + (v[2] - c[2]) * (v[2] - c[2]);
        }
        ms->height = hi[2] - lo[2];
        ms->n = 0;
        while (ms->n < BLOB_MAX) {   /* the fullest cells first */
            unsigned best = CELLS;
            for (unsigned c = 0; c < CELLS; c++) if (cnt[c] >= 6u && slot[c] == 255u && (best == CELLS || cnt[c] > cnt[best])) best = c;
            if (best == CELLS) break;
            slot[best] = (uint8_t)ms->n;
            for (int k = 0; k < 3; k++) ms->c[ms->n][k] = sum[best][k];
            ms->r[ms->n] = shz_fmaxf(0.6f * shz_sqrtf(spread[best] * shz_invf((float)cnt[best]) + 1e-12f), 0.03f * ms->height);
            ms->n++;
        }
        for (unsigned i = 0; i < m->n; i++) model_bone[mi][i] = slot[model_bone[mi][i]];
        if (mi == M_DUDE) {   /* the biped: its feet are the vertices in the lowest 12 % of its height, one group each side of their middle */
            float mid = 0.0f, fs[2][3] = { { 0 } };
            unsigned low = 0, fn[2] = { 0, 0 };
            const float cut = lo[2] + 0.12f * ms->height;
            for (unsigned i = 0; i < m->n; i++) if (m->v[i * 6u + 2u] < cut) { mid += m->v[i * 6u + 1u]; low++; }
            if (low) mid *= shz_invf((float)low);
            for (unsigned i = 0; i < m->n; i++) if (m->v[i * 6u + 2u] < cut) {
                const int side = m->v[i * 6u + 1u] < mid;
                for (int k = 0; k < 3; k++) fs[side][k] += m->v[i * 6u + (unsigned)k];
                fn[side]++;
            }
            ms->foot_r = shz_fmaxf(0.04f, 0.09f * ms->height);
            ms->feet = fn[0] && fn[1];
            for (int f = 0; f < 2 && ms->feet; f++) {
                for (int k = 0; k < 2; k++) ms->foot[f][k] = fs[f][k] * shz_invf((float)fn[f]);
                ms->foot[f][2] = lo[2] + ms->foot_r;
            }
        }
        printf("sh4dow: model %d, %u vertices, %u limbs, height x1000 %d, feet %d\n", mi, m->n, ms->n, (int)(ms->height * 1000.0f), ms->feet);
    }
}

static void shadow_init(void) {
    static uint16_t px[32 * 32] __attribute__((aligned(32)));
    pvr_poly_cxt_t c;
    shadow_models();
    sh4dow_foot_texture565(px, 32);   /* the cone's picture: 2 KB of texture RAM */
    tex_foot = vram(sizeof px);
    pvr_txr_load(px, tex_foot, sizeof px);
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, 32, 32, tex_foot, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.txr.env = PVR_TXRENV_MODULATE;
    c.txr.uv_clamp = PVR_UVCLAMP_UV;
    c.blend.src = PVR_BLEND_ZERO; c.blend.dst = PVR_BLEND_INVDESTCOLOR;   /* the floor keeps 1 - texel x strength (SH4DOW-DC's README: zero / inverse source colour) */
    pvr_poly_compile(&hdr_foot, &c);
}

/* The way the shadow falls at a point: by day in the yard the sun's travel, otherwise away from the nearest lamp. */
static void shadow_dir(const float p[3], float out[3]) {
    if (!fx_on[FX_NIGHT] && outdoor_at(p[0]) > 0.5f) { out[0] = -sun_dir[0]; out[1] = -sun_dir[1]; out[2] = -sun_dir[2]; return; }
    int best = 0;
    float bd = 1e9f;
    for (int l = 0; l < 5; l++) {
        const float d2 = (p[0] - lamp_pos[l][0]) * (p[0] - lamp_pos[l][0]) + (p[1] - lamp_pos[l][1]) * (p[1] - lamp_pos[l][1]) + (p[2] - lamp_pos[l][2]) * (p[2] - lamp_pos[l][2]);
        if (d2 < bd) { bd = d2; best = l; }
    }
    if (bd < 1e-4f) { out[0] = out[1] = 0.0f; out[2] = -1.0f; return; }
    for (int k = 0; k < 3; k++) out[k] = p[k] - lamp_pos[best][k];
}

/* Once a frame: the four actors nearest the camera get a prepared frame (their limb spheres posed, the light's way). */
static void shadow_step(void) {
    int used[ACTORS];
    memset(used, 0, sizeof used);
    for (unsigned i = 0; i < ACTORS; i++) { shadow_of[i] = NULL; actor_sc[i] = shz_sincosf(actors[i].yaw); }
    shadowed = 0;
    if (!fx_on[FX_SHADOW]) return;
    for (int s = 0; s < SHADOW_ACTORS; s++) {
        int idx = -1;
        float bd = 1e9f;
        for (unsigned i = 0; i < ACTORS; i++) {
            const float d2 = (actors[i].x - cam_x) * (actors[i].x - cam_x) + (actors[i].y - cam_y) * (actors[i].y - cam_y);
            if (!used[i] && d2 < bd) { bd = d2; idx = (int)i; }
        }
        if (idx < 0) break;
        used[idx] = 1;
        const actor_t *a = &actors[idx];
        const model_shadow_t *ms = &model_shadow[a->model];
        if (!ms->n || !model_bone[a->model]) continue;
        const shz_sincos_t sc = actor_sc[idx];
        sh4dow_limb_t limbs[BLOB_MAX];
        for (unsigned b = 0; b < ms->n; b++) {
            limbs[b].position[0] = a->x + (ms->c[b][0] * sc.cos - ms->c[b][1] * sc.sin) * a->scale;
            limbs[b].position[1] = a->y + (ms->c[b][0] * sc.sin + ms->c[b][1] * sc.cos) * a->scale;
            limbs[b].position[2] = a->z + ms->c[b][2] * a->scale;
            limbs[b].radius = ms->r[b] * a->scale;
            limbs[b].bone = (uint8_t)b;
            limbs[b].flags = 0;
        }
        const float mid[3] = { a->x, a->y, a->z + 0.5f * ms->height * a->scale };
        float dir[3];
        sh4dow_params_t prm;
        shadow_dir(mid, dir);
        sh4dow_params_default(&prm);
        prm.length = 1.3f * ms->height * a->scale;   /* a column reaches from the head to the feet */
        prm.dark = shz_fminf(SH4DOW_DARK * fx_level[FX_SHADOW], 0.95f);
        if (sh4dow_prepare(&shadow_frame[s], &prm, limbs, ms->n, dir) > 0) { shadow_of[idx] = &shadow_frame[s]; shadowed++; }
    }
}

static void emit(const vv_t in[3]);
static inline void to_view(const float p[3], vv_t *v);

/* The foot cones, in the world pass's translucent list: two quads under every biped, away from its light. */
static void shadow_cones(void) {
    static const float up[3] = { 0.0f, 0.0f, 1.0f };
    int sent = 0;
    cones_drawn = 0;
    if (!fx_on[FX_SHADOW]) return;
    for (unsigned i = 0; i < ACTORS; i++) {
        const actor_t *a = &actors[i];
        const model_shadow_t *ms = &model_shadow[a->model];
        if (!ms->feet) continue;
        const float range = sh4dow_foot_range_fade(NULL, shz_sqrtf((a->x - cam_x) * (a->x - cam_x) + (a->y - cam_y) * (a->y - cam_y) + 1e-9f), 1.0f);
        if (range <= 0.0f) continue;
        const shz_sincos_t sc = actor_sc[i];
        const float forward[3] = { sc.cos, sc.sin, 0.0f }, floor_point[3] = { a->x, a->y, a->z };
        for (int f = 0; f < 2; f++) {
            const float centre[3] = { a->x + (ms->foot[f][0] * sc.cos - ms->foot[f][1] * sc.sin) * a->scale, a->y + (ms->foot[f][0] * sc.sin + ms->foot[f][1] * sc.cos) * a->scale, a->z + ms->foot[f][2] * a->scale };
            sh4dow_foot_t foot;
            float q[4][3];
            sh4dow_foot_place(&foot, NULL, centre, ms->foot_r * a->scale, forward, up);
            shadow_dir(centre, foot.away);
            const float fade = sh4dow_foot_cone(&foot, NULL, floor_point, up, up, forward, q);
            if (fade <= 0.0f) continue;
            const float g = sat(0.6f * fx_level[FX_SHADOW]) * fade * range;
            vv_t v[4];
            for (int k = 0; k < 4; k++) {   /* TL, TR, BL, BR: u across, v along */
                to_view(q[k], &v[k]);
                v[k].cr = v[k].cg = v[k].cb = g;
                v[k].tu = (float)(k & 1); v[k].tv = (float)(k >> 1);
            }
            if (!sent) { pvr_prim(&hdr_foot, sizeof(hdr_foot)); sent = 1; }
            const vv_t t1[3] = { v[0], v[1], v[2] }, t2[3] = { v[1], v[3], v[2] };
            emit(t1); emit(t2);
            cones_drawn++;
        }
    }
}

static void draw_actor(const actor_t *a) {
    const demo_model_t *m = &demo_models[a->model];
    const sh4dow_frame_t *sf = shadow_of[a - actors];   /* NULL: not one of the nearest four, or the setting is off */
    const shz_sincos_t sc = actor_sc[a - actors];
    {   /* the whole actor behind the eye or far off to a side: skip */
        vv_t c;
        const float p[3] = { a->x, a->y, a->z };
        to_view(p, &c);
        if (c.f < -6.0f || c.f > FAR_F || c.r > c.f * 1.2f + 8.0f || -c.r > c.f * 1.2f + 8.0f) return;
    }
    for (unsigned i = 0; i + 2 < m->n; i += 3) {
        float p[3][3], n[3][3];
        vv_t v[3];
        for (int k = 0; k < 3; k++) {
            const float *s = &m->v[(i + (unsigned)k) * 6u];
            p[k][0] = a->x + (s[0] * sc.cos - s[1] * sc.sin) * a->scale;
            p[k][1] = a->y + (s[0] * sc.sin + s[1] * sc.cos) * a->scale;
            p[k][2] = a->z + s[2] * a->scale;
            n[k][0] = s[3] * sc.cos - s[4] * sc.sin;
            n[k][1] = s[3] * sc.sin + s[4] * sc.cos;
            n[k][2] = s[5];
        }
        if ((n[0][0] + n[1][0] + n[2][0]) * (p[0][0] - cam_x) + (n[0][1] + n[1][1] + n[2][1]) * (p[0][1] - cam_y) + (n[0][2] + n[1][2] + n[2][2]) * (p[0][2] - EYE_Z) > 0.0f) continue;   /* faces away */
        for (int k = 0; k < 3; k++) to_view(p[k], &v[k]);
        if (v[0].f < NEAR_F && v[1].f < NEAR_F && v[2].f < NEAR_F) continue;
        {
            float cc[3][3];
            for (int k = 0; k < 3; k++) shade(p[k], n[k], &m->c[(i + (unsigned)k) * 3u], cc[k]);
            if (sf) sh4dow_shade(sf, &p[0][0], sizeof p[0], &model_bone[a->model][i], 1, &cc[0][0], sizeof cc[0], 3);   /* after lighting: rgb x (1 - dark x strength) */
            for (int k = 0; k < 3; k++) { v[k].cr = cc[k][0]; v[k].cg = cc[k][1]; v[k].cb = cc[k][2]; v[k].tu = v[k].tv = 0.0f; }
        }
        emit(v);
    }
}

/* The sun: a round disc far along its direction, facing the eye, white core to a warm rim. Returns its screen position (0..1) and whether it is in front. */
static int draw_sun(float *su, float *sv) {
    const float p[3] = { cam_x + sun_dir[0] * 40.0f, cam_y + sun_dir[1] * 40.0f, EYE_Z + sun_dir[2] * 40.0f };
    static shz_sincos_t rim[17];   /* the disc's 16 directions, made once (was 64 sine / cosine pairs a frame) */
    static int rim_ok;
    vv_t c, v[3];
    if (!rim_ok) { for (int t = 0; t < 17; t++) rim[t] = shz_sincosf((float)t * 0.3926991f); rim_ok = 1; }
    to_view(p, &c);
    if (c.f < 1.0f) return 0;
    {
        const float inv = shz_invf_fsrra(c.f);   /* c.f >= 1: once, positive */
        *su = 0.5f + c.r * inv * FOCAL * (1.0f / SCR_W);
        *sv = 0.5f - c.u * inv * FOCAL * (1.0f / SCR_H);
    }
    /* Facing: the cosine between the view axis and the sun; from 0.85 (about 32 degrees off) to dead on it grows to twice the size and its rim goes white. */
    const float facing = c.f * shz_inv_sqrtf(c.f * c.f + c.r * c.r + c.u * c.u);
    sun_facing = facing;
    const float hot0 = sat((facing - 0.85f) * (1.0f / 0.15f)), hot = hot0 * hot0;
    for (int t = 0; t < 16; t++) {   /* the haze */
        const shz_sincos_t a = rim[t], b = rim[t + 1];
        v[0] = v[1] = v[2] = c;
        v[0].f = v[1].f = v[2].f = c.f + 0.5f;
        v[1].r += a.cos * 11.0f * (1.0f + 0.5f * hot); v[1].u += a.sin * 11.0f * (1.0f + 0.5f * hot);
        v[2].r += b.cos * 11.0f * (1.0f + 0.5f * hot); v[2].u += b.sin * 11.0f * (1.0f + 0.5f * hot);
        v[0].cr = 0.95f + 0.05f * hot; v[0].cg = 0.95f + 0.05f * hot; v[0].cb = 0.92f + 0.08f * hot;
        v[1].cr = v[2].cr = 0.20f; v[1].cg = v[2].cg = 0.38f; v[1].cb = v[2].cb = 0.72f;   /* the sky's colour: no edge */
        for (int k = 0; k < 3; k++) v[k].tu = v[k].tv = 0.0f;
        emit(v);
    }
    for (int t = 0; t < 16; t++) {
        const shz_sincos_t a = rim[t], b = rim[t + 1];
        v[0] = v[1] = v[2] = c;
        v[1].r += a.cos * 1.5f * (1.0f + hot); v[1].u += a.sin * 1.5f * (1.0f + hot);
        v[2].r += b.cos * 1.5f * (1.0f + hot); v[2].u += b.sin * 1.5f * (1.0f + hot);
        v[0].cr = v[0].cg = v[0].cb = 1.0f;
        v[1].cr = v[2].cr = 1.0f; v[1].cg = v[2].cg = 0.95f + 0.05f * hot; v[1].cb = v[2].cb = 0.78f + 0.22f * hot;
        for (int k = 0; k < 3; k++) v[k].tu = v[k].tv = 0.0f;
        emit(v);
    }
    return 1;
}

/* ---------- textures, headers ---------- */

static void make_brick(void) {
    static uint16_t px[64 * 64] __attribute__((aligned(32)));
    uint32_t seed = 12345u;
    for (int y = 0; y < 64; y++)
        for (int x = 0; x < 64; x++) {
            const int row = y / 16, bx = (x + (row & 1) * 16) % 32;
            const int mortar = (y % 16) < 2 || bx < 2;
            seed = seed * 1664525u + 1013904223u;
            const int v = mortar ? 150 : 215 + (int)(seed >> 27) + ((x / 32 + row) % 3) * 4;
            px[y * 64 + x] = (uint16_t)((v >> 3) << 11 | (v >> 2) << 5 | (v >> 3));
        }
    tex_brick = vram(sizeof(px));
    pvr_txr_load(px, tex_brick, sizeof(px));
}

static void flat_header(pvr_poly_hdr_t *h, int list, int src, int dst) {
    pvr_poly_cxt_t c;
    pvr_poly_cxt_col(&c, list);
    c.gen.culling = PVR_CULLING_NONE;
    if (list == PVR_LIST_TR_POLY) {
        c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        c.depth.write = PVR_DEPTHWRITE_DISABLE;
        c.blend.src = src; c.blend.dst = dst;
    }
    pvr_poly_compile(h, &c);
}

static void headers(void) {
    static const int src_of[4] = { PVR_BLEND_ZERO, PVR_BLEND_DESTCOLOR, PVR_BLEND_ONE, PVR_BLEND_INVDESTCOLOR };   /* sh4der_look.h's kinds */
    static const int dst_of[4] = { PVR_BLEND_DESTCOLOR, PVR_BLEND_ONE, PVR_BLEND_ONE, PVR_BLEND_ZERO };
    pvr_poly_cxt_t c;
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, 64, 64, tex_brick, PVR_FILTER_BILINEAR);
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&hdr_world, &c);
    flat_header(&hdr_flat, PVR_LIST_OP_POLY, 0, 0);
    flat_header(&hdr_stamp, PVR_LIST_TR_POLY, PVR_BLEND_ONE, PVR_BLEND_ZERO);
    flat_header(&hdr_black, PVR_LIST_TR_POLY, PVR_BLEND_ONE, PVR_BLEND_ZERO);
    tex_probe = vram(sizeof(probe_buf));
    if (sh4der_shafts_init(&shafts_gpu) != 0) { printf("sh4der demo: no texture RAM for the shade map\n"); arch_exit(); }
    sh4der_shafts_clear(shade_buf, 128, 64);
    for (int k = 0; k < 2; k++) {   /* the read's picture, and the bloom map's own texture drawn plain */
        pvr_poly_cxt_t pc;
        pvr_poly_cxt_txr(&pc, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, 128, 64, k ? bloom.big : tex_probe, PVR_FILTER_NONE);
        pc.gen.culling = PVR_CULLING_NONE;
        pc.depth.comparison = PVR_DEPTHCMP_ALWAYS;
        pc.depth.write = PVR_DEPTHWRITE_DISABLE;
        pc.blend.src = PVR_BLEND_ONE; pc.blend.dst = PVR_BLEND_ZERO;
        pvr_poly_compile(k ? &hdr_probe_map : &hdr_probe, &pc);
    }   /* the stamp replaces what is under it */
    for (int k = 0; k < 4; k++) flat_header(&hdr_look[k], PVR_LIST_TR_POLY, src_of[k], dst_of[k]);
    sh4der_torch_headers(hdr_torch);
    tex_hud = vram(sizeof(hud_buf));
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, 512, 128, tex_hud, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    pvr_poly_compile(&hdr_hud, &c);
}

static void hud_set(const char *const l[6]) {   /* KOS minifont, 8 x 16 (the BIOS font before was 12 x 24) */
    int same = 1;
    for (int i = 0; i < 6; i++) same &= !strcmp(l[i], hud_text[i]);
    if (same) return;
    memset(hud_buf, 0, sizeof(hud_buf));
    minifont_set_color(255, 255, 255);
    for (int i = 0; i < 6; i++) {
        snprintf(hud_text[i], sizeof hud_text[i], "%s", l[i]);
        minifont_draw_str(hud_buf + 512 * (2 + 18 * i), 512, hud_text[i]);
    }
    for (unsigned i = 0; i < 512u * 128u; i++) hud_buf[i] = hud_buf[i] ? 0xffffu : 0x0000u;   /* ARGB1555: text opaque white, the rest clear */
    pvr_txr_load(hud_buf, tex_hud, sizeof(hud_buf));
}

static void quad(const pvr_poly_hdr_t *h, float x, float y, float w, float hh, float z, uint32_t argb, float u1, float v1) {
    pvr_prim((void *)h, sizeof(*h));
    for (int q = 0; q < 4; q++) {
        pvr_vertex_t v;
        v.flags = q == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = x + ((q & 1) ? w : 0.0f); v.y = y + ((q & 2) ? hh : 0.0f); v.z = z;
        v.u = (q & 1) ? u1 : 0.0f; v.v = (q & 2) ? v1 : 0.0f;
        v.argb = argb; v.oargb = 0;
        pvr_prim(&v, sizeof(v));
    }
}

/* The frame textures and the chromatic aberration are the library's (sh4der_ca.h). A size or count that does not fit the texture RAM holds nothing, says once on
 * the serial line how many KB were short, and is not tried again until the request changes. */
static void rt_setup(int half, unsigned n) {
    static int bad_half = -1;
    static unsigned bad_n;
    if ((half == rt_half && n == rt_n) || (half == bad_half && n == bad_n)) return;
    taa_have = 0; taa_cur = 0;
    if (!sh4der_ca_setup(&ca_gpu, half, n)) {
        printf("CA: needs %u KB more VRAM (%u frame texture(s) at %s)\n", ca_gpu.short_kb, n, half ? "320x240" : "640x480");
        bad_half = half; bad_n = n;
        rt_half = -1; rt_n = 0;
        return;
    }
    bad_half = -1;
    rt_half = half; rt_n = n;
    rt_w = ca_gpu.size.w; rt_h = ca_gpu.size.h; rt_tw = ca_gpu.size.tex_w; rt_th = ca_gpu.size.tex_h;
    rt_scale = ca_gpu.size.scale;
}

static unsigned rt_kb(void) { return rt_n * sh4der_ca_bytes(&ca_gpu.size) / 1024u; }

/* A frame texture over the screen at (dx, dy), the whole drawn at scale `s`: opaque, or blended at `alpha` in the translucent list. */
static void taa_quad(pvr_ptr_t tex, int blend, float alpha, float dx, float dy, float z, float s) {
    sh4der_ca_quad(&ca_gpu, tex == ca_gpu.tex[1] ? 1u : 0u, blend ? shz_fminf(alpha, 0.99f) : 1.0f, dx, dy, z, s);
}

/* CA: one colour channel of the frame texture over the whole screen (0 red, 1 green, 2 blue). */
static void ca_mesh(pvr_ptr_t tex, int chan, float z) {
    sh4der_ca_channel(&ca_gpu, tex == ca_gpu.tex[1] ? 1u : 0u, chan, fx_level[FX_CA], SCR_W, SCR_H, z);
}

/* ---------- the effects ---------- */

static float frame_read(void) {
    const uintptr_t frame = PVR_GET(PVR_FB_ADDR) & (PVR_RAM_SIZE - 1u);   /* the frame on screen */
    const volatile uint16_t *fb = (const volatile uint16_t *)((uintptr_t)PVR_RAM_BASE + frame);
    uint16_t hist[SH4DER_LOOK_BINS] = { 0 };
    uint16_t *out = look_grid;
    unsigned sum = 0;
    {
        const uint16_t st = fb[640u + 1u];   /* the stamp block's pixel (1, 1) */
        read_id = (((st >> 11) & 31u) > 15u ? 4u : 0u) | (((st >> 5) & 63u) > 31u ? 2u : 0u) | ((st & 31u) > 15u ? 1u : 0u);
    }
    for (unsigned y = 0; y < SH4DER_LOOK_GRID_Y; y++) {
        const volatile uint16_t *row = fb + (480u * (2u * y + 1u) / (2u * SH4DER_LOOK_GRID_Y)) * 640u;
        for (unsigned x = 0; x < SH4DER_LOOK_GRID_X; x++) {
            const unsigned px = 640u * (2u * x + 1u) / (2u * SH4DER_LOOK_GRID_X);
            const uint16_t p = row[px], q = row[640u * 3u + px + 3u];   /* the second sample: 3 right, 3 down */
            const unsigned r = (p >> 11) < (q >> 11) ? p >> 11 : q >> 11, g = ((p >> 5) & 63u) < ((q >> 5) & 63u) ? (p >> 5) & 63u : (q >> 5) & 63u, b = (p & 31u) < (q & 31u) ? p & 31u : q & 31u;
            raw_grid[y * SH4DER_LOOK_GRID_X + x] = p;
            const int hud = fx_on[FX_PROBE] && y >= 37u;   /* PROBE on: its two pictures (the bottom of the screen) give the bloom nothing */   /* the read-out's rectangle (24..536, 20..148) in cells */
            *out++ = hud ? 0u : (uint16_t)(r << 11 | g << 5 | b);
            hist[sh4der_look_luma565(p)]++;
            sum += sh4der_look_luma565(p);
        }
    }
    read_lum = (float)sum * (1.0f / (62.0f * (float)(SH4DER_LOOK_GRID_X * SH4DER_LOOK_GRID_Y)));
    return sh4der_look_log_average(&look_bins, hist, SH4DER_LOOK_GRID_X * SH4DER_LOOK_GRID_Y);
}

/* Last frame's glow off the read, so a bright spot does not feed itself (nearest texel: an estimate). */
static void bloom_unfeed(void) {
    float fb, fs;
    fb = hist_glow[read_id] * (1.0f - SH4DER_GLOW_RADIUS); fs = hist_glow[read_id] * SH4DER_GLOW_RADIUS;   /* the strength the frame read was drawn with, all passes together */
    const unsigned wb = (unsigned)(fb * 256.0f), ws = (unsigned)(fs * 256.0f);
    for (unsigned y = 0; y < SH4DER_LOOK_GRID_Y; y++)
        for (unsigned x = 0; x < SH4DER_LOOK_GRID_X; x++) {
            const uint16_t p = look_grid[y * SH4DER_LOOK_GRID_X + x], m = bloom.stage[y * SH4DER_GLOW_TEX_W + x], s = bloom.stage_small[(y >> 2) * SH4DER_GLOW_SMALL_TEX_W + (x >> 2)];
            if (!(m | s)) continue;
            const int r = (int)(p >> 11) - (int)((((m >> 11) & 31u) * wb + ((s >> 11) & 31u) * ws) >> 8);
            const int g = (int)((p >> 5) & 63u) - (int)((((m >> 5) & 63u) * wb + ((s >> 5) & 63u) * ws) >> 8);
            const int b = (int)(p & 31u) - (int)(((m & 31u) * wb + (s & 31u) * ws) >> 8);
            look_grid[y * SH4DER_LOOK_GRID_X + x] = (uint16_t)((r > 0 ? r : 0) << 11 | (g > 0 ? g : 0) << 5 | (b > 0 ? b : 0));
        }
}

static float bloom_now;   /* this frame's bloom strength, every pass together */

static void effects(float dt, int sun_front, float su, float sv) {
    static sh4der_torch_vert_t tv[SH4DER_TORCH_VERTS];
    const int map_on = (fx_bloom || fx_rays) && bloom_ok;   /* RAYS alone still needs the map */
    {   /* b6: the read every frame, so the serial row shows it with every setting off */
        const uint64_t t0 = timer_us_gettime64();
        const float read = frame_read();
        read_us = (uint32_t)(timer_us_gettime64() - t0);
        if (fx_eye && !(torch_k > 0.0f)) {   /* the torch lit (dark places only: its gate): the eye holds */
            const float e = fx_level[FX_EYE];   /* the eye's strength: how far it over-shoots and how far it opens */
            look_params.transient = SH4DER_LOOK_TRANSIENT * e; look_params.open = SH4DER_LOOK_OPEN * e;
            look_params.gain_hi = shz_fminf(1.0f + (SH4DER_LOOK_GAIN_HI - 1.0f) * e, 2.0f); look_params.gain_lo = shz_fmaxf(1.0f - (1.0f - SH4DER_LOOK_GAIN_LO) * e, 0.4f);
            /* The step takes its own last gain off the read; the frame read carried hist_gain_ln[read_id]: hand it the read as if it carried the last one. */
            const sh4der_look_grade_t gr = { { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 0.0f }, 1.0f, sh4der_look_eye_step(&look_eye, &look_params, read - hist_gain_ln[read_id] + look_eye.gain_ln, dt) };
            look_n = sh4der_look_solve(&gr, look_layers);
        }
        if (map_on) {
            if (bloom_have && hist_glow[read_id] > 0.0f) bloom_unfeed();
            if (hist_gain[read_id] < 0.99f || hist_gain[read_id] > 1.01f) {   /* the eye's gain off the bloom's source */
                const unsigned k = (unsigned)(256.0f * shz_invf(hist_gain[read_id]));
                for (unsigned i = 0; i < SH4DER_LOOK_GRID_X * SH4DER_LOOK_GRID_Y; i++) {
                    const unsigned q = look_grid[i], r = ((q >> 11) * k) >> 8, g = (((q >> 5) & 63u) * k) >> 8, b = ((q & 31u) * k) >> 8;
                    look_grid[i] = (uint16_t)((r > 31u ? 31u : r) << 11 | (g > 63u ? 63u : g) << 5 | (b > 31u ? 31u : b));
                }
            }
            map_mean = sh4der_glow_upload(&bloom, look_grid);
            bloom_have = 1;
        }
    }
    if (!fx_eye) { look_n = 0; if (look_eye.ready) sh4der_look_eye_reset(&look_eye); }
    if (!map_on) bloom_have = 0;
    {
        const int at_sun = sun_front && outdoor_at(cam_x) > 0.5f && su > -0.2f && su < 1.2f && sv > -0.2f && sv < 1.2f;
        const float toward = at_sun ? sh4der_shafts_toward(sun_facing) : 0.0f;   /* how far the camera has turned to the light */
        if (fx_on[FX_SHAFTS] && shafts_gpu.tex && toward > 0.01f) {   /* the library's shadow shafts: the read's dark cells cast away from the light */
            sh4der_shafts_map(shade_buf, 128, raw_grid, su, sv, sh4der_shafts_depth(fx_level[FX_SHAFTS], toward));
            sh4der_shafts_upload(&shafts_gpu, shade_buf);
            shade_dirty = 1;
            sh4der_shafts_draw(&shafts_gpu, su, sv, sh4der_shafts_scale(fx_level[FX_SHAFTS]), SCR_W, SCR_H, 425.0f);   /* under the torch, the look and the glow */
        } else if (shade_dirty) {
            sh4der_shafts_clear(shade_buf, 128, 64);   /* no light in view: the map starts clean next time */
            shade_dirty = 0;
        }
    }
    if (fx_on[FX_PROBE]) {   /* bottom right: the read (80 x 60 of the 128 x 64 texture); bottom left: the bloom map; 232 x 174 each, a black edge round them */
        for (unsigned y = 0; y < SH4DER_LOOK_GRID_Y; y++) memcpy(&probe_buf[y * 128u], &raw_grid[y * SH4DER_LOOK_GRID_X], SH4DER_LOOK_GRID_X * 2u);
        pvr_txr_load(probe_buf, tex_probe, sizeof(probe_buf));
        quad(&hdr_black, 396.0f, 294.0f, 240.0f, 182.0f, HUD_Z + 1.0f, 0xff000000u, 0, 0);
        quad(&hdr_probe, 400.0f, 298.0f, 232.0f, 174.0f, HUD_Z + 2.0f, 0xffffffffu, 80.0f / 128.0f, 60.0f / 64.0f);
        if (bloom_ok) {
            quad(&hdr_black, 4.0f, 294.0f, 240.0f, 182.0f, HUD_Z + 1.0f, 0xff000000u, 0, 0);
            quad(&hdr_probe_map, 8.0f, 298.0f, 232.0f, 174.0f, HUD_Z + 2.0f, 0xffffffffu, 80.0f / 128.0f, 60.0f / 64.0f);
        }
    }
    frame_id = (frame_id + 1u) & 7u;   /* this frame's stamp, and what it is drawn with */
    hist_gain_ln[frame_id] = look_n ? look_eye.gain_ln : 0.0f;
    hist_gain[frame_id] = look_n ? look_eye.gain : 1.0f;
    hist_glow[frame_id] = map_on && bloom_have && fx_bloom ? bloom_now : 0.0f;
    quad(&hdr_stamp, 0.0f, 0.0f, 4.0f, 4.0f, HUD_Z + 1000.0f, 0xff000000u | (frame_id & 4u ? 0xff0000u : 0u) | (frame_id & 2u ? 0xff00u : 0u) | (frame_id & 1u ? 0xffu : 0u), 0, 0);
    if (torch_k > 0.0f) {   /* dark places only: in daylight the torch draws nothing */
        const float r = FOCAL * TORCH_TAN;
        unsigned n = sh4der_torch_carve_mesh(tv, SCR_W * 0.5f, SCR_H * 0.5f, r, r, shz_fminf(torch_k, 1.5f));
        sh4der_torch_draw(&hdr_torch[0], tv, n, TORCH_Z);
        n = sh4der_torch_boost_mesh(tv, SCR_W * 0.5f, SCR_H * 0.5f, r, r, torch_k);
        sh4der_torch_draw(&hdr_torch[1], tv, n, TORCH_Z + 0.25f);
    }
    for (int i = 0; i < look_n; i++) quad(&hdr_look[look_layers[i].kind], 0, 0, SCR_W, SCR_H, LOOK_Z + 0.25f * (float)i, sh4der_look_argb(&look_layers[i]), 0, 0);
    if (map_on && bloom_have) {
        /* A draw adds at most the map x 1: a strength past that is more passes. */
        bloom_now = SH4DER_GLOW_STRENGTH * fx_level[FX_BLOOM] * (look_n && look_eye.gain < 1.0f ? shz_invf(look_eye.gain) : 1.0f);   /* the eye closed down: the glare grows */
        float left = fx_bloom ? bloom_now : 0.0f;
        for (int pass = 0; left > 0.001f && pass < 3; pass++, left -= 1.0f)
            sh4der_glow_draw_halo(&bloom, 0, 0, SCR_W, SCR_H, BLOOM_Z + 0.75f * (float)pass, shz_fminf(left, 1.0f), SH4DER_GLOW_RADIUS, 0.0f);
        left = fx_rays && sun_front ? outdoor_at(cam_x) * fx_level[FX_RAYS] : 0.0f;
        for (int pass = 0; left > 0.001f && pass < 4; pass++, left -= 1.0f)
            sh4der_glow_draw_rays(&bloom, 0, 0, SCR_W, SCR_H, BLOOM_Z + 3.0f + 0.5f * (float)pass, su, sv, shz_fminf(left, 1.0f), 6);   /* 6 copies (the most): a longer, smoother streak than the default 4 */
    }
}

/* The bloom's weight table from the CURVE setting: luma under the threshold gives 0, white gives 1, between them straight or squared. */
static void curve_set(void) {
    const float th = fx_level[FX_CURVE], inv = shz_invf(1.0f - th);
    for (int i = 0; i < 256; i++) {
        const float t = sat(((float)i * (1.0f / 255.0f) - th) * inv);
        bloom.lut[i] = (uint8_t)(255.0f * (fx_on[FX_CURVE] ? t * t : t) + 0.5f);
    }
}

/* ---------- input ---------- */

static void input(float dt) {
    static uint32_t last;
    maple_device_t *dev = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
    cont_state_t *st = dev ? (cont_state_t *)maple_dev_status(dev) : NULL;
    if (!st) return;
    const uint32_t down = st->buttons & ~last;
    last = st->buttons;
    if (down & CONT_DPAD_LEFT) fx_pick = (fx_pick + FX_COUNT - 1) % FX_COUNT;
    if (down & CONT_DPAD_RIGHT) fx_pick = (fx_pick + 1) % FX_COUNT;
    if (down & CONT_A) fx_on[fx_pick] ^= 1;
    if (down & CONT_Y) hud_show ^= 1;
    if (down & CONT_DPAD_UP) fx_level[fx_pick] = shz_fminf(fx_level[fx_pick] + fx_step[fx_pick], fx_max[fx_pick]);
    if (down & CONT_DPAD_DOWN) fx_level[fx_pick] = shz_fmaxf(fx_level[fx_pick] - fx_step[fx_pick], fx_min[fx_pick]);
    if (fx_pick == FX_CURVE && (down & (CONT_A | CONT_DPAD_UP | CONT_DPAD_DOWN))) curve_set();
    if (fx_on[FX_CAR]) {   /* the left stick drives (up: throttle, sideways: steer); the camera rides behind and above */
        const float steer = shz_fabsf((float)st->joyx) > 12.0f ? (float)st->joyx * (1.0f / 128.0f) : 0.0f, gas = shz_fabsf((float)st->joyy) > 12.0f ? -(float)st->joyy * (1.0f / 128.0f) : 0.0f;
        car_speed += (gas * 5.0f - car_speed) * shz_fminf(dt * 2.0f, 1.0f);
        car_yaw = shz_wrapf(car_yaw - steer * 1.6f * dt * shz_clampf(car_speed * 0.5f, -1.0f, 1.0f), -3.14159265f, 3.14159265f);
        const shz_sincos_t cs = shz_sincosf(car_yaw);
        car_fx = cs.cos; car_fy = cs.sin;
        car_x = shz_clampf(car_x + car_fx * car_speed * dt, 1.5f, 12.5f);
        car_y = shz_clampf(car_y + car_fy * car_speed * dt, -4.5f, 4.5f);
        cam_x = car_x - car_fx * 3.4f; cam_y = car_y - car_fy * 3.4f; cam_z = 1.7f;
        cam_yaw = car_yaw; cam_pitch = -0.22f;
        return;
    }
    cam_z = 0.62f;
    const float strafe = shz_fabsf((float)st->joyx) > 12.0f ? (float)st->joyx * (1.0f / 128.0f) : 0.0f;
    float turn = 0.0f, walk = shz_fabsf((float)st->joyy) > 12.0f ? -(float)st->joyy * (1.0f / 128.0f) : 0.0f;
    float look = shz_fabsf((float)st->joy2y) > 12.0f ? (float)st->joy2y * (1.0f / 128.0f) : 0.0f;   /* inverted: stick up (negative) looks down */
    if (shz_fabsf((float)st->joy2x) > 12.0f) turn += (float)st->joy2x * (1.0f / 128.0f);
    look += ((float)st->rtrig - (float)st->ltrig) * (1.0f / 255.0f);
    cam_pitch = shz_clampf(cam_pitch + look * 1.5f * dt, -1.2f, 1.2f);
    cam_yaw = shz_wrapf(cam_yaw - shz_clampf(turn, -1.0f, 1.0f) * 1.9f * dt, -3.14159265f, 3.14159265f);   /* kept in one turn */
    const shz_sincos_t sc = shz_sincosf(cam_yaw);
    const float ox = cam_x;
    float nx = cam_x + (sc.cos * walk + sc.sin * strafe) * 2.6f * dt, ny = cam_y + (sc.sin * walk - sc.cos * strafe) * 2.6f * dt;
    nx = shz_clampf(nx, -11.6f, 13.6f);
    if (nx < 0.3f && shz_fabsf(ny) > 1.7f) {   /* the hall's face: stay out of the wall */
        if (ox >= 0.3f) nx = 0.3f;
        else ny = shz_clampf(ny, -1.7f, 1.7f);
    }
    ny = shz_clampf(ny, -5.6f, 5.6f);
    cam_x = nx; cam_y = ny;
}

int main(void) {
    pvr_init_defaults();
    pvr_set_bg_color(0.20f, 0.38f, 0.72f);   /* a deeper sky: under the bloom threshold, so only the sun and the lamps glow */
    make_brick();
    bloom_ok = sh4der_glow_init(&bloom);   /* before the headers: PROBE draws its texture */
    headers();
    shadow_init();
    world_build();
    sh4der_look_bins(&look_bins);
    sh4der_look_params_default(&look_params);
    sh4der_look_eye_reset(&look_eye);
    sh4der_torch_gate_reset(&torch_gate);
    sh4der_taa_init(&taa);
    curve_set();   /* the recommended CURVE (straight 0.80), not the library's table */
    bloom.keep = 128;   /* each upload half way to the new map: no pulse from a read that is one or two frames old */
    printf("sh4der demo: %u world triangles, bloom %s, %lu KB texture RAM free\n", world_n, bloom_ok ? "ready" : "no VRAM", (unsigned long)(pvr_mem_available() / 1024u));

    uint64_t last = timer_us_gettime64(), cpu_sum = 0;
    unsigned frames = 0;
    float clock = 0.0f, cpu_ms = 0.0f, frame_ms = 0.0f, dt_sum = 0.0f, slide = 0.0f;
    for (;;) {
        const uint64_t now = timer_us_gettime64();
        const float dt = shz_fminf((float)(now - last) * 1e-6f, 0.1f);
        last = now;
        clock += dt;
        input(dt);
        {
            const shz_sincos_t sc = shz_sincosf(cam_yaw);
            const shz_sincos_t ps = shz_sincosf(cam_pitch);
            cam_s = sc.sin; cam_c = sc.cos; cam_ps = ps.sin; cam_pc = ps.cos;
        }
        actors[0].yaw = shz_wrapf(actors[0].yaw + 0.8f * dt, -3.14159265f, 3.14159265f);
        slide = shz_wrapf(slide + 0.5f * dt, 0.0f, 6.2831853f);
        if (fx_on[FX_CAR]) { actors[5].x = car_x; actors[5].y = car_y; actors[5].yaw = car_yaw; }
        else { actors[5].x = 4.0f + 1.5f * shz_sinf(slide); actors[5].y = -4.3f; actors[5].yaw = 0.0f; car_fx = 1.0f; car_fy = 0.0f; }
        if (fx_on[FX_NIGHT]) pvr_set_bg_color(0.02f, 0.03f, 0.07f); else pvr_set_bg_color(0.20f, 0.38f, 0.72f);
        if (fx_on[FX_TAA]) {
            sh4der_taa_step(&taa, cam_yaw, cam_pitch, FOCAL, FOCAL, &taa_f);
            {
                static float px, py;
                const float walked = dt > 0.0f ? shz_sqrtf((cam_x - px) * (cam_x - px) + (cam_y - py) * (cam_y - py) + 1e-12f) * shz_invf(dt) : 0.0f;   /* wu / s */
                px = cam_x; py = cam_y;
                if (fx_on[FX_TAAMODE]) {   /* DYNAMIC: still share .. moving share, by the slower of the two measures; no smoothing over time */
                    const float still = shz_clampf(SH4DER_TAA_WEIGHT_HI * fx_level[FX_TAA], 0.0f, 0.9f), moving = shz_fminf(TAA_MOVING, still);
                    const float f = shz_fminf(taa_f.weight * (1.0f / SH4DER_TAA_WEIGHT_HI), 1.0f - sat(walked * (1.0f / TAA_WALK_FULL)));   /* 1 still, 0 moving */
                    taa_f.weight = taa_f.cut ? 0.0f : shz_lerpf(moving, still, f);
                } else taa_f.weight = shz_clampf(taa_f.weight * fx_level[FX_TAA], 0.0f, 0.9f);   /* MANUAL: the method before */
            }
            jit_x = taa_f.jitter[0]; jit_y = taa_f.jitter[1];
        } else { sh4der_taa_reset(&taa); taa_f.weight = 0.0f; jit_x = jit_y = 0.0f; }
        torch_power = sh4der_torch_power_step(torch_power, fx_torch, dt);
        torch_k = torch_power * sh4der_torch_gate_step(&torch_gate, shz_lerpf(0.05f, 0.36f, outdoor_at(cam_x)), dt) * fx_level[FX_TORCH];

        char l0[72], l1[72], l2[72], l3[72], l4[72], l5[72];
        if (fx_pick == FX_TAA) snprintf(l0, sizeof l0, "< TAA >  %s  %d.%02d  old frame %d%%", fx_on[FX_TAA] ? "ON" : "off", (int)fx_level[FX_TAA], (int)(fx_level[FX_TAA] * 100.0f + 0.5f) % 100, (int)(taa_f.weight * 100.0f + 0.5f));   /* the live share */
        else if (fx_pick == FX_TAAMODE) snprintf(l0, sizeof l0, "< TAAMODE >  %s  old frame %d%%", fx_on[FX_TAAMODE] ? "DYNAMIC" : "MANUAL", (int)(taa_f.weight * 100.0f + 0.5f));
        else if (fx_pick == FX_SHADOW) snprintf(l0, sizeof l0, "< SH4DOW >  %s  %d.%02d  %u actors shaded, %u foot cones", fx_on[FX_SHADOW] ? "ON" : "off", (int)fx_level[FX_SHADOW], (int)(fx_level[FX_SHADOW] * 100.0f + 0.5f) % 100, shadowed, cones_drawn);
        else if (fx_pick == FX_DR) snprintf(l0, sizeof l0, "< DR >  world vertices by %s  cpu %d.%d ms", fx_on[FX_DR] ? "KOS direct render" : "pvr_prim calls", (int)cpu_ms, (int)(cpu_ms * 10.0f) % 10);
        else if (fx_pick == FX_CA) snprintf(l0, sizeof l0, "< CA >  %s  %d.%02d  red-blue split %d.%d px in the corner", fx_on[FX_CA] ? "ON" : "off", (int)fx_level[FX_CA], (int)(fx_level[FX_CA] * 100.0f + 0.5f) % 100, (int)(CA_SPLIT_PX * fx_level[FX_CA]), (int)(CA_SPLIT_PX * fx_level[FX_CA] * 10.0f + 0.5f) % 10);
        else if (fx_pick == FX_RES) snprintf(l0, sizeof l0, "< RES >  frame texture %s  (A switches)", fx_on[FX_RES] ? "320x240" : "640x480");
        else snprintf(l0, sizeof l0, "< %s >  %s  %d.%02d", fx_name[fx_pick], fx_pick == FX_CURVE ? (fx_on[fx_pick] ? "squared" : "straight") : fx_on[fx_pick] ? "ON" : "off", (int)fx_level[fx_pick], (int)(fx_level[fx_pick] * 100.0f + 0.5f) % 100);
        {
            int at = 0;
            for (int i = 0; i < 7; i++) at += snprintf(l2 + at, sizeof l2 - (size_t)at, "%.3s%c ", fx_name[i], fx_on[i] ? '+' : '-');   /* + on, - off */
            at = 0;
            for (int i = 7; i < FX_COUNT; i++) at += snprintf(l3 + at, sizeof l3 - (size_t)at, "%s%c ", fx_name[i], fx_on[i] ? '+' : '-');
        }
        snprintf(l1, sizeof l1, "cpu %d.%d gain %d.%02d saw %d.%02d map %d.%03d", (int)cpu_ms, (int)(cpu_ms * 10.0f) % 10, (int)look_eye.gain, (int)(look_eye.gain * 100.0f) % 100,
                 (int)read_lum, (int)(read_lum * 100.0f) % 100, (int)map_mean, (int)(map_mean * 1000.0f) % 1000);   /* saw: the frame read's mean luma (0.00 = the read sees black); map: the bloom map's mean */
        /* The budget: what the settings hold now (allocated), what is left of the texture RAM, the frame's time; then the price list. */
        if (rt_n) snprintf(l4, sizeof l4, "NOW tex %ux%u x%u = %u KB  free %lu KB  frame %d.%d ms", rt_w, rt_h, rt_n, rt_kb(), (unsigned long)(pvr_mem_available() / 1024u), (int)frame_ms, (int)(frame_ms * 10.0f) % 10);
        else snprintf(l4, sizeof l4, "NOW tex none = 0 KB  free %lu KB  frame %d.%d ms", (unsigned long)(pvr_mem_available() / 1024u), (int)frame_ms, (int)(frame_ms * 10.0f) % 10);
        snprintf(l5, sizeof l5, "COST 640x480 640 KB/tex  320x240 160 KB/tex  CA 1 tex  TAA 2 tex");
        { const char *const hl[6] = { l0, l1, l2, l3, l4, l5 }; hud_set(hl); }

        const uint64_t c0 = timer_us_gettime64();
        float su = 0.5f, sv = 0.5f;
        int sun_front;
        tris_drawn = 0;
        shadow_step();
        const int ca_on = fx_on[FX_CA], half = fx_on[FX_RES];
        pvr_wait_ready();
        rt_setup(half, fx_on[FX_TAA] ? 2u : (ca_on || half) ? 1u : 0u);   /* RES at 320 x 240 with nothing else on: the world still goes through the texture, to see it */
        const int taa_on = fx_on[FX_TAA] && taa_tex[0] && taa_tex[1];
        const int rtt_on = taa_on || ((ca_on || half) && taa_tex[0]);
        if (!taa_on) { taa_have = 0; taa_cur = 0; }
        rt_s = rtt_on ? rt_scale : 1.0f;
        if (rtt_on) sh4der_ca_begin(&ca_gpu, (unsigned)taa_cur);
        else pvr_scene_begin();
        pvr_list_begin(PVR_LIST_OP_POLY);
        view_load();
        pvr_prim(&hdr_world, sizeof(hdr_world));
        draw_world(0);
        pvr_prim(&hdr_flat, sizeof(hdr_flat));
        draw_world(1);
        sun_facing = 0.0f;
        sun_front = fx_on[FX_NIGHT] ? 0 : draw_sun(&su, &sv);   /* NIGHT: no sun */
        for (unsigned i = 0; i < ACTORS; i++) draw_actor(&actors[i]);
        pvr_list_finish();
        pvr_list_begin(PVR_LIST_TR_POLY);
        view_load();
        shadow_cones();   /* on the floor, in the world: under CA, TAA and every screen layer */
        if (rtt_on) {   /* TAA: the frame before over this one at the history weight (moved with a pan); then the texture to the screen, plain or as CA's three copies */
            const int blend = taa_on && taa_have && taa_f.weight > 0.0f && !taa_f.cut;
            if (blend && fx_on[FX_TAAHOLD]) taa_quad(taa_tex[taa_cur ^ 1], 1, taa_f.weight, (float)taa_f.copy_px[0], (float)taa_f.copy_px[1], 10.0f, rt_s);   /* into the texture: it accumulates */
            pvr_list_finish();
            pvr_scene_finish();
            pvr_wait_ready();
            rt_s = 1.0f;
            pvr_scene_begin();
            pvr_list_begin(PVR_LIST_OP_POLY);
            if (ca_on) ca_mesh(taa_tex[taa_cur], 1, 1.0f);   /* green: the base */
            else taa_quad(taa_tex[taa_cur], 0, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);
            pvr_list_finish();
            pvr_list_begin(PVR_LIST_TR_POLY);
            if (ca_on) { ca_mesh(taa_tex[taa_cur], 0, 2.0f); ca_mesh(taa_tex[taa_cur], 2, 3.0f); }   /* red and blue, added */
            if (blend && !fx_on[FX_TAAHOLD]) taa_quad(taa_tex[taa_cur ^ 1], 1, taa_f.weight, (float)taa_f.copy_px[0], (float)taa_f.copy_px[1], 10.0f, 1.0f);   /* on screen only: the one frame before, never kept */
            if (taa_on) { taa_cur ^= 1; taa_have = 1; }
        }
        effects(dt, sun_front, su, sv);
        if (hud_show) quad(&hdr_hud, 24.0f, 20.0f, 512.0f, 128.0f, HUD_Z, 0xff909090u, 1.0f, 1.0f);   /* grey: under the bloom's threshold */
        pvr_list_finish();
        pvr_scene_finish();
        cpu_sum += timer_us_gettime64() - c0;
        dt_sum += dt;
        if (++frames == 30u) {   /* the read-out twice a second; one serial row every 10 s */
            static unsigned rows;
            cpu_ms = (float)cpu_sum * (1.0f / 30000.0f);
            frame_ms = dt_sum * (1000.0f / 30.0f); dt_sum = 0.0f;
            cpu_sum = 0; frames = 0;
            if (++rows % 4u == 1u) {   /* every 2 s */
                unsigned lit = 0;
                for (unsigned i = 0; i < SH4DER_LOOK_GRID_X * SH4DER_LOOK_GRID_Y; i++) lit += look_grid[i] != 0u;
                printf("demo: cpu %d us, %lu tris, read %lu us, fb reg %08lx, saw x1000 %d, stamp %u of %u, lit %u/4800, px %04x %04x %04x %04x, map x1000 %d, gain x100 %d, adapted x1000 %d | eye %d %d bloom %d %d torch %d %d rays %d %d probe %d curve %d %d shafts %d %d car %d %d night %d wedge %d %d taa %d %d w%d shift %d %d have %d hold %d dyn %d | dr %d | sh4dow %d %d shaded %u cones %u | ca %d %d half %d frame tex %u KB, VRAM free %lu KB, frame %d us | pos %d %d yaw %d pitch %d out %d\n",
                       (int)(cpu_ms * 1000.0f), (unsigned long)tris_drawn, (unsigned long)read_us, (unsigned long)PVR_GET(PVR_FB_ADDR), (int)(read_lum * 1000.0f), read_id, frame_id, lit,
                       look_grid[0], look_grid[30u * 80u + 40u], look_grid[10u * 80u + 20u], look_grid[4799], (int)(map_mean * 1000.0f), (int)(look_eye.gain * 100.0f), (int)(look_eye.adapted * 1000.0f),
                       fx_on[FX_EYE], (int)(fx_level[FX_EYE] * 100.0f), fx_on[FX_BLOOM], (int)(fx_level[FX_BLOOM] * 100.0f), fx_on[FX_TORCH], (int)(fx_level[FX_TORCH] * 100.0f), fx_on[FX_RAYS], (int)(fx_level[FX_RAYS] * 100.0f), fx_on[FX_PROBE], fx_on[FX_CURVE], (int)(fx_level[FX_CURVE] * 100.0f + 0.5f), fx_on[FX_SHAFTS], (int)(fx_level[FX_SHAFTS] * 100.0f), fx_on[FX_CAR], (int)(fx_level[FX_CAR] * 100.0f), fx_on[FX_NIGHT], fx_on[FX_WEDGE], (int)(fx_level[FX_WEDGE] * 100.0f), fx_on[FX_TAA], (int)(fx_level[FX_TAA] * 100.0f), (int)(taa_f.weight * 100.0f), taa_f.copy_px[0], taa_f.copy_px[1], taa_have, fx_on[FX_TAAHOLD], fx_on[FX_TAAMODE], fx_on[FX_DR], fx_on[FX_SHADOW], (int)(fx_level[FX_SHADOW] * 100.0f), shadowed, cones_drawn, fx_on[FX_CA], (int)(fx_level[FX_CA] * 100.0f), fx_on[FX_RES], rt_kb(), (unsigned long)(pvr_mem_available() / 1024u), (int)(frame_ms * 1000.0f),
                       (int)(cam_x * 10.0f), (int)(cam_y * 10.0f), (int)(cam_yaw * 57.3f), (int)(cam_pitch * 57.3f), (int)(outdoor_at(cam_x) * 100.0f));
            }
        }
    }
    return 0;
}
