/* SH4DER-PVR hardware probe disc

   demo/probe/probe.c
   Copyright (c) 2026 Jilted Valkyrie
   See ../../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).

   What this is: a small test disc that answers one question per screen: "does this PowerVR2 hardware mode do, on a real
   Dreamcast, what the emulator and the headers say it does?". Effects that depend on a mode are only written after its
   screen passes on a console.

   Screens (one mode each, a label, what to expect, and a verdict line):
     0 BASE         three plain colour bars. Proves the disc draws, and proves the frame read the verdicts depend on.
     1 SHADOW       cheap shadow: an opaque floor darkened inside a modifier volume (opaque modifier list).
     2A..2E         two-volume polygon on the opaque list (a second texture inside the volume), five ways, because the
                    plain form failed on a console in build 2: A as build 2 (closed volume, shadow register off);
                    B the KallistiOS example exactly (one open quad in front, register as pvr_init leaves it);
                    C closed volume, register as pvr_init leaves it; D the polygon's shadow bit cleared;
                    E no texture, two vertex colours (32-byte vertices instead of 64).
     3 TWO-VOL TR   two-volume polygon on the translucent list: invisible outside the volume, a picture inside it.
     4 FOG MODE 2   fog table mode 2: the fog colour replaces the colour and the table value becomes the alpha.
     5 BUMP         the hardware bump texture format, as a polygon (left) and as a sprite (right), light turning.
     6 CLAMP        the colour clamp registers: a grey ramp squeezed between a floor and a ceiling.

   The verdict: every screen parks its moving part for the first second, then reads a few pixels of the finished frame
   back from video memory and compares them (inside against outside, lit against unlit). The result is drawn as the third
   line and printed once on the serial port. It is a helper, not the judge: the eye decides. If screen 0 cannot read its
   own bars back, every later verdict reads "N/A" and only the eye is left.

   Safety on a real console: every header is checked with sh4der_poly_hdr_ok and every volume triangle with
   sh4der_tri_area_ok before it is sent; a volume with a rejected triangle is not sent at all (never an open volume).
   The volume is a truncated pyramid, not a box, so that none of its side walls is edge-on to the screen.

   Pad: A or D-pad right = next screen, D-pad left = previous, Y = automatic cycling on / off (on at start, 6 s a screen).

   Memory: main RAM about 70 KB (three 8 KB textures staged, a 64 KB label image). Video memory 88 KB of textures
   (3 x 8 KB + 64 KB label) on top of the KallistiOS frame and tile buffers. Sound: none, sound RAM 0 KB.
   KallistiOS paths: pvr_init with the two modifier bins sized, pvr_prim for every header and vertex (a probe has a few
   dozen vertices, so the direct-render path buys nothing), pvr_txr_load_ex for the twiddled textures, pvr_txr_load for
   the label, PVR_SET for the two clamp registers KallistiOS has no call for.
   All maths is SH4ZAM (shz_sinf, shz_sincosf); the pixel compare is integer arithmetic on read-back bytes. */
#include <kos.h>
#include <dc/minifont.h>
#include <stdio.h>
#include <string.h>
#include <sh4zam/shz_sh4zam.h>
#include "sh4der_guard.h"

KOS_INIT_FLAGS(INIT_DEFAULT);

/* ---------- the picture's fixed numbers ---------- */
#define SCREEN_W        640             /* frame width in pixels (KallistiOS default video mode) */
#define SCREEN_H        480             /* frame height in pixels */
#define QUAD_X          192.0f          /* left edge of the 256 px test square, pixels */
#define QUAD_Y          112.0f          /* top edge of the test square, pixels */
#define QUAD_SIZE       256.0f          /* side of the test square, pixels: 4 screen pixels a texel of a 64 px texture */
#define CELL_PX         64              /* a texture's grid cell on screen, pixels (16 texels x 4) */
#define TEX_SIZE        64              /* side of every test texture, texels */
#define Z_BACKDROP      0.30f           /* depth (1 / w, larger is nearer) of a backdrop behind the test square */
#define Z_SURFACE       0.50f           /* depth of the test square */
#define Z_VOL_BACK      0.25f           /* far cap of the modifier volume: behind the surface */
#define Z_VOL_FRONT     0.75f           /* near cap of the modifier volume: in front of the surface */
#define Z_HUD           0.90f           /* depth of the label, in front of everything */
#define VOL_INSET       12.0f           /* the far cap is this much smaller on every side, pixels: slanted side walls */
#define PARK_FRAMES     60u             /* frames a screen holds still before it starts to move (the read happens inside) */
#define CHECK_FRAME     40u             /* frame of a screen at which the frame is read back (the frame shown is 1-2 old) */
#define AUTO_FRAMES     360u            /* frames a screen stays up while cycling automatically: 6 s at 60 Hz */
#define STUCK_WAITS     20u             /* KallistiOS waits (100 ms each) in a row before the PVR is called stuck */
#define STAGE_HOLD_MS   300             /* each start-up colour is held this long so a camera can see it, ms */
#define SHADOW_SCALE    0.5f            /* cheap shadow: shadowed pixels are multiplied by this (register, 0..1) */
#define CLAMP_FLOOR     0.25f           /* colour clamp minimum per channel, 0..1 */
#define CLAMP_CEIL      0.75f           /* colour clamp maximum per channel, 0..1 */
#define FOG_NEAR_W      2.0f            /* fog starts at this depth (w, the reciprocal of z) */
#define FOG_FAR_W       50.0f           /* fog is full at this depth (w) */
#define BUMP_HEIGHT     0.8f            /* bump strength, 0 flat .. 1 full */
#define BUMP_ELEVATION  0.7853982f      /* the light's height over the surface, radians (45 degrees) */
#define TWO_PI          6.2831853f      /* a full turn, radians */

/* HUD: a 512 x 64 image of three text lines, drawn as one translucent quad near the bottom of the frame */
#define HUD_W           512
#define HUD_H           64
#define HUD_LINE_PX     20              /* distance between text lines, pixels */
#define HUD_X           64.0f
#define HUD_Y           400.0f
#define HUD_CHARS       42              /* longest line that fits at the widest glyph the font may have (12 px) */

enum { S_BASE, S_SHADOW, S_TV_A, S_TV_B, S_TV_C, S_TV_D, S_TV_E, S_TWOVOL_TR, S_FOG2, S_BUMP, S_CLAMP, S_COUNT };

/* The cheap shadow register (one value for the whole frame) a screen runs with. */
enum {
    REG_SHADOW_ON,      /* cheap shadow enabled at SHADOW_SCALE: the shadow screen */
    REG_SHADOW_OFF,     /* cheap shadow disabled, scale byte left at SHADOW_SCALE: what build 2 used on every other screen */
    REG_KOS_INIT        /* the value KallistiOS writes at pvr_init and its two-volume example never changes */
};
#define REG_KOS_INIT_VALUE  0x00000001u   /* PVR_CHEAP_SHADOW after pvr_init (KallistiOS pvr_init_shutdown.c): enable bit 8 clear, scale 1 */
#define PCW_SHADOW_BIT      0x00000080u   /* bit 7 of a polygon header's first word: "a modifier volume acts on this polygon" */
enum { V_WAIT, V_PASS, V_FAIL, V_NA };  /* a screen's verdict: not read yet, read and right, read and wrong, cannot be read */

/* A screen's two fixed text lines. */
static const char *const screen_title[S_COUNT] = {
    "0 BASE: THREE COLOUR BARS",
    "1 CHEAP SHADOW (OPAQUE MODIFIER VOLUME)",
    "2A TWO-VOL OPAQUE: CLOSED VOL, REG OFF",
    "2B TWO-VOL OPAQUE: KOS EXAMPLE (OPEN QUAD)",
    "2C TWO-VOL OPAQUE: CLOSED VOL, REG AS INIT",
    "2D TWO-VOL OPAQUE: SHADOW BIT CLEARED",
    "2E TWO-VOL OPAQUE: NO TEXTURE, 2 COLOURS",
    "3 TWO-VOLUME POLYGON, TRANSLUCENT LIST",
    "4 FOG TABLE MODE 2 (DEPTH TO ALPHA)",
    "5 HARDWARE BUMP: POLYGON LEFT, SPRITE RIGHT",
    "6 COLOUR CLAMP REGISTERS",
};
static const char *const screen_expect[S_COUNT] = {
    "SEE: RED, GREEN, BLUE BARS",
    "SEE: A DARKER BLOCK SLIDING ON BLUE",
    "SEE: AN ORANGE BLOCK SLIDING ON BLUE",
    "SEE: AN ORANGE BLOCK SLIDING ON BLUE",
    "SEE: AN ORANGE BLOCK SLIDING ON BLUE",
    "SEE: ORANGE BLOCK (OR NOTHING: A FINDING)",
    "SEE: FLAT ORANGE BLOCK ON FLAT BLUE",
    "SEE: ORANGE BLOCK ONLY, GREY ELSEWHERE",
    "SEE: TOP WHITE>CYAN, BOTTOM NONE>CYAN",
    "SEE: PYRAMIDS, LIGHT TURNING ROUND",
    "SEE: LOWER RAMP GREYER AT BOTH ENDS",
};

/* ---------- state ---------- */
static pvr_ptr_t tex_out, tex_in, tex_bump, tex_hud;                 /* video memory: outside / inside / bump / label */
static uint16_t stage_px[TEX_SIZE * TEX_SIZE] __attribute__((aligned(32)));   /* one texture being built, RGB565 or bump */
static uint16_t hud_px[HUD_W * HUD_H] __attribute__((aligned(32)));  /* the label image, ARGB1555 */
static pvr_poly_hdr_t hdr_flat_op, hdr_flat_tr, hdr_hud;             /* untextured opaque / translucent, and the label */
static pvr_poly_hdr_t hdr_shadow, hdr_twovol_op, hdr_twovol_tr;      /* the three surfaces a volume acts on */
static pvr_poly_hdr_t hdr_twovol_noshadow;                           /* hdr_twovol_op with the shadow bit cleared (screen 2D) */
static pvr_poly_hdr_t hdr_twovol_col;                                /* two-volume with no texture: two vertex colours (screen 2E) */
static pvr_poly_hdr_t hdr_fog1, hdr_fog2, hdr_bump_poly, hdr_clamp_off, hdr_clamp_on;
static pvr_sprite_hdr_t hdr_bump_sprite;
static pvr_mod_hdr_t mod_op_first, mod_op_last, mod_tr_first, mod_tr_last;   /* volume headers: all but the last triangle / the last */
static int verdict[S_COUNT];             /* V_* per screen */
static char verdict_text[S_COUNT][HUD_CHARS + 1];
static int printed[S_COUNT];             /* 1 once a screen's serial line has gone out: no line is ever repeated */
static int readback_ok = -1;             /* -1 not known yet, 0 the base screen could not read its bars, 1 it could */
static unsigned vol_rejected;            /* volumes not sent because a triangle failed the area guard */
static unsigned hdr_rejected;            /* headers not sent because they failed the header guard */
static uint64_t t0;                      /* time at main, microseconds: start-up lines print ms since this */

/* Which cheap shadow register value each screen runs with (REG_*). The two-volume variants differ here on purpose. */
static const uint8_t screen_reg[S_COUNT] = {
    REG_SHADOW_OFF, REG_SHADOW_ON, REG_SHADOW_OFF, REG_KOS_INIT, REG_KOS_INIT, REG_KOS_INIT, REG_KOS_INIT,
    REG_SHADOW_OFF, REG_SHADOW_OFF, REG_SHADOW_OFF, REG_SHADOW_OFF,
};

/* ---------- small helpers ---------- */

/* Prints the eight 32-bit words of a compiled header, once at start-up, so the header a console was sent can be
   compared word for word with the hardware documents. Reads through memcpy: no assumption about the struct's layout. */
static void dump_hdr(const char *name, const pvr_poly_hdr_t *h) {
    uint32_t w[8];

    memcpy(w, h, sizeof(w));
    printf("probe: hdr %-15s %08lx %08lx %08lx %08lx %08lx %08lx %08lx %08lx\n", name,
           (unsigned long)w[0], (unsigned long)w[1], (unsigned long)w[2], (unsigned long)w[3],
           (unsigned long)w[4], (unsigned long)w[5], (unsigned long)w[6], (unsigned long)w[7]);
}

/* Writes the cheap shadow register for screen s. It is one value for the whole frame, so it is written every frame. */
static void set_shadow_reg(int s) {
    switch (screen_reg[s]) {
    case REG_SHADOW_ON:
        pvr_set_shadow_scale(true, SHADOW_SCALE);
        break;
    case REG_KOS_INIT:
        PVR_SET(PVR_CHEAP_SHADOW, REG_KOS_INIT_VALUE);
        break;
    default:
        pvr_set_shadow_scale(false, SHADOW_SCALE);
        break;
    }
}

/* One start-up stage: fills the displayed frame with a colour (no PVR render needed), prints one serial line, holds. */
static void probe_stage(unsigned n, uint8_t r, uint8_t g, uint8_t b, const char *what) {
    vid_clear(r, g, b);
    printf("probe: stage %u %s at %lu ms\n", n, what, (unsigned long)((timer_us_gettime64() - t0) / 1000u));
    thd_sleep(STAGE_HOLD_MS);
}

/* 8-bit channels to an RGB565 texel. */
static uint16_t rgb565(unsigned r, unsigned g, unsigned b) {
    return (uint16_t)(((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3));
}

/* Sends a header only if it is a compiled polygon or sprite header (a zero header would end the list on a console).
   Returns 1 when sent. */
static int send_hdr(const pvr_poly_hdr_t *h) {
    if (!sh4der_poly_hdr_ok(&h->cmd)) {
        hdr_rejected++;
        return 0;
    }
    pvr_prim((void *)h, sizeof(*h));
    return 1;
}

/* One axis-aligned quad of ordinary vertices as a four-vertex strip. c[4] are the corner colours in the order
   top-left, top-right, bottom-left, bottom-right; oargb is the offset colour of all four. u / v run 0..1 over it. */
static void quad(const pvr_poly_hdr_t *h, float x0, float y0, float x1, float y1, float z, const uint32_t c[4], uint32_t oargb) {
    static pvr_vertex_t v __attribute__((aligned(32)));
    int i;

    if (!send_hdr(h)) {
        return;
    }
    for (i = 0; i < 4; i++) {
        v.flags = i == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = (i & 1) ? x1 : x0;
        v.y = (i & 2) ? y1 : y0;
        v.z = z;
        v.u = (i & 1) ? 1.0f : 0.0f;
        v.v = (i & 2) ? 1.0f : 0.0f;
        v.argb = c[i];
        v.oargb = oargb;
        pvr_prim(&v, sizeof(v));
    }
}

/* The same quad with one colour on all corners. */
static void quad1(const pvr_poly_hdr_t *h, float x0, float y0, float x1, float y1, float z, uint32_t argb) {
    const uint32_t c[4] = { argb, argb, argb, argb };

    quad(h, x0, y0, x1, y1, z, c, 0);
}

/* The test square as a two-volume quad: two sets of texture coordinates and colours, the second used inside a volume.
   argb_out / argb_in are the vertex colours outside and inside. */
static void quad_twovol(const pvr_poly_hdr_t *h, uint32_t argb_out, uint32_t argb_in) {
    static pvr_vertex_tpcm_t v __attribute__((aligned(32)));
    int i;

    if (!send_hdr(h)) {
        return;
    }
    memset(&v, 0, sizeof(v));
    for (i = 0; i < 4; i++) {
        v.flags = i == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = QUAD_X + ((i & 1) ? QUAD_SIZE : 0.0f);
        v.y = QUAD_Y + ((i & 2) ? QUAD_SIZE : 0.0f);
        v.z = Z_SURFACE;
        v.u0 = v.u1 = (i & 1) ? 1.0f : 0.0f;
        v.v0 = v.v1 = (i & 2) ? 1.0f : 0.0f;
        v.argb0 = argb_out;
        v.argb1 = argb_in;
        v.oargb0 = v.oargb1 = 0xff000000u;
        pvr_prim(&v, sizeof(v));
    }
}

/* The test square as an untextured two-volume quad: one colour outside a volume, another inside. 32-byte vertices. */
static void quad_twovol_col(const pvr_poly_hdr_t *h, uint32_t argb_out, uint32_t argb_in) {
    static pvr_vertex_pcm_t v __attribute__((aligned(32)));
    int i;

    if (!send_hdr(h)) {
        return;
    }
    memset(&v, 0, sizeof(v));
    for (i = 0; i < 4; i++) {
        v.flags = i == 3 ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
        v.x = QUAD_X + ((i & 1) ? QUAD_SIZE : 0.0f);
        v.y = QUAD_Y + ((i & 2) ? QUAD_SIZE : 0.0f);
        v.z = Z_SURFACE;
        v.argb0 = argb_out;
        v.argb1 = argb_in;
        pvr_prim(&v, sizeof(v));
    }
}

/* The KallistiOS example's volume: ONE flat quad (two triangles) in front of the surface, over the screen rectangle
   x0..x1, y0..y1 at Z_VOL_FRONT. It is not closed: the hardware counts how many volume faces lie between the eye and a
   pixel, so one face in front marks everything behind it. Same guards as the closed volume. Returns 1 when sent. */
static int volume_open(const pvr_mod_hdr_t *first, const pvr_mod_hdr_t *last, float x0, float y0, float x1, float y1) {
    static pvr_modifier_vol_t m __attribute__((aligned(32)));
    const float a[3] = { x0, y1, 1.0f }, b[3] = { x0, y0, 1.0f }, c[3] = { x1, y1, 1.0f }, e[3] = { x1, y0, 1.0f };

    if (!sh4der_tri_area_ok(a, b, c, 0.5f, SH4DER_GUARD_MIN_AREA2) || !sh4der_tri_area_ok(b, c, e, 0.5f, SH4DER_GUARD_MIN_AREA2)) {
        vol_rejected++;
        return 0;
    }
    if (!sh4der_poly_hdr_ok(&first->cmd) || !sh4der_poly_hdr_ok(&last->cmd)) {
        hdr_rejected++;
        return 0;
    }
    memset(&m, 0, sizeof(m));
    m.flags = PVR_CMD_VERTEX_EOL;
    m.az = m.bz = m.cz = Z_VOL_FRONT;
    pvr_prim((void *)first, sizeof(*first));   /* first triangle: bottom-left, top-left, bottom-right (the example's order) */
    m.ax = x0;
    m.ay = y1;
    m.bx = x0;
    m.by = y0;
    m.cx = x1;
    m.cy = y1;
    pvr_prim(&m, sizeof(m));
    pvr_prim((void *)last, sizeof(*last));     /* second and last triangle: top-left, bottom-right, top-right */
    m.ax = x0;
    m.ay = y0;
    m.bx = x1;
    m.by = y1;
    m.cx = x1;
    m.cy = y0;
    pvr_prim(&m, sizeof(m));
    return 1;
}

/* A closed modifier volume round the screen rectangle x0..x1, y0..y1, from Z_VOL_FRONT back to Z_VOL_BACK, where the
   far cap is VOL_INSET smaller on every side. 12 triangles. Every triangle is tested first; if one has no screen area
   the whole volume is left out (a volume missing a face is open, and an open volume marks the wrong pixels).
   first / last are the two compiled modifier headers of the list that is open. Returns 1 when the volume was sent. */
static int volume(const pvr_mod_hdr_t *first, const pvr_mod_hdr_t *last, float x0, float y0, float x1, float y1) {
    /* corner k: bits 0 / 1 pick x / y of the rectangle, bit 2 picks the far cap */
    static const uint8_t tri[12][3] = {
        { 0, 1, 2 }, { 2, 1, 3 },   /* near cap */
        { 4, 6, 5 }, { 5, 6, 7 },   /* far cap */
        { 0, 4, 1 }, { 1, 4, 5 },   /* top wall */
        { 2, 3, 6 }, { 3, 7, 6 },   /* bottom wall */
        { 0, 2, 4 }, { 2, 6, 4 },   /* left wall */
        { 1, 5, 3 }, { 3, 5, 7 },   /* right wall */
    };
    static pvr_modifier_vol_t m __attribute__((aligned(32)));
    float p[8][3];
    int k;

    for (k = 0; k < 8; k++) {
        const float inset = (k & 4) ? VOL_INSET : 0.0f;

        p[k][0] = (k & 1) ? x1 - inset : x0 + inset;
        p[k][1] = (k & 2) ? y1 - inset : y0 + inset;
        p[k][2] = (k & 4) ? Z_VOL_BACK : Z_VOL_FRONT;
    }
    for (k = 0; k < 12; k++) {
        /* the guard wants view space: these points are already on screen, so a view depth of 1 leaves x and y as they are */
        const float a[3] = { p[tri[k][0]][0], p[tri[k][0]][1], 1.0f };
        const float b[3] = { p[tri[k][1]][0], p[tri[k][1]][1], 1.0f };
        const float c[3] = { p[tri[k][2]][0], p[tri[k][2]][1], 1.0f };

        if (!sh4der_tri_area_ok(a, b, c, 0.5f, SH4DER_GUARD_MIN_AREA2)) {
            vol_rejected++;
            return 0;
        }
    }
    /* both headers are tested before anything is sent: once the first triangle is in the list the volume must be closed */
    if (!sh4der_poly_hdr_ok(&first->cmd) || !sh4der_poly_hdr_ok(&last->cmd)) {
        hdr_rejected++;
        return 0;
    }
    pvr_prim((void *)first, sizeof(*first));
    memset(&m, 0, sizeof(m));
    m.flags = PVR_CMD_VERTEX_EOL;
    for (k = 0; k < 12; k++) {
        if (k == 11) {                      /* the last triangle goes under the header that closes the volume */
            pvr_prim((void *)last, sizeof(*last));
        }
        m.ax = p[tri[k][0]][0];
        m.ay = p[tri[k][0]][1];
        m.az = p[tri[k][0]][2];
        m.bx = p[tri[k][1]][0];
        m.by = p[tri[k][1]][1];
        m.bz = p[tri[k][1]][2];
        m.cx = p[tri[k][2]][0];
        m.cy = p[tri[k][2]][1];
        m.cz = p[tri[k][2]][2];
        pvr_prim(&m, sizeof(m));
    }
    return 1;
}

/* Reads the frame the screen shows now (the scan-out address, not the half-drawn render target) at pixel x, y and
   gives the mean of a 3 x 3 block of samples 2 pixels apart as 8-bit channels: the mean takes the 16-bit dither out.
   The frame is RGB565, SCREEN_W pixels a row; every read is one aligned 16-bit load from video memory. */
static void frame_px(int x, int y, int rgb[3]) {
    const uintptr_t frame = PVR_GET(PVR_FB_ADDR) & (PVR_RAM_SIZE - 1u);
    const volatile uint16_t *fb = (const volatile uint16_t *)((uintptr_t)PVR_RAM_BASE + frame);
    int dx, dy;

    rgb[0] = rgb[1] = rgb[2] = 0;
    for (dy = -2; dy <= 2; dy += 2) {
        for (dx = -2; dx <= 2; dx += 2) {
            const int sx = x + dx < 0 ? 0 : x + dx >= SCREEN_W ? SCREEN_W - 1 : x + dx;
            const int sy = y + dy < 0 ? 0 : y + dy >= SCREEN_H ? SCREEN_H - 1 : y + dy;
            const uint16_t px = fb[sy * SCREEN_W + sx];

            rgb[0] += (px >> 11) << 3;
            rgb[1] += ((px >> 5) & 63) << 2;
            rgb[2] += (px & 31) << 3;
        }
    }
    rgb[0] /= 9;
    rgb[1] /= 9;
    rgb[2] /= 9;
}

/* Sum of the three channels: a brightness to compare, 0..765. */
static int lum(const int rgb[3]) {
    return rgb[0] + rgb[1] + rgb[2];
}

/* ---------- textures ---------- */

/* Fills stage_px with a flat colour and a one-texel grid line every 16 texels, then loads it twiddled. The samples the
   verdicts read sit at cell centres, 8 texels from any line. */
static void make_grid(pvr_ptr_t dst, uint16_t base, uint16_t line) {
    int x, y;

    for (y = 0; y < TEX_SIZE; y++) {
        for (x = 0; x < TEX_SIZE; x++) {
            stage_px[y * TEX_SIZE + x] = ((x & 15) == 0 || (y & 15) == 0) ? line : base;
        }
    }
    pvr_txr_load_ex(stage_px, dst, TEX_SIZE, TEX_SIZE, PVR_TXRLOAD_16BPP);
}

/* Fills stage_px with a bump map of four-sided pyramids, 32 texels each, and loads it twiddled. A bump texel is two
   angles of the surface normal: the high byte is its elevation (0 = lying in the surface, 255 = straight up) and the
   low byte its direction round the surface (0..255 = a full turn). Every face here leans 45 degrees (elevation 128)
   toward east, south, west or north (direction 0, 64, 128, 192), so no trigonometry is needed to build it. */
static void make_bump(pvr_ptr_t dst) {
    int x, y;

    for (y = 0; y < TEX_SIZE; y++) {
        for (x = 0; x < TEX_SIZE; x++) {
            const int cx = 2 * (x & 31) - 31, cy = 2 * (y & 31) - 31;   /* twice the offset from the pyramid's centre */
            const int ax = cx < 0 ? -cx : cx, ay = cy < 0 ? -cy : cy;
            const unsigned dir = ax > ay ? (cx > 0 ? 0u : 128u) : (cy > 0 ? 64u : 192u);

            stage_px[y * TEX_SIZE + x] = (uint16_t)((128u << 8) | dir);
        }
    }
    pvr_txr_load_ex(stage_px, dst, TEX_SIZE, TEX_SIZE, PVR_TXRLOAD_16BPP);
}

/* The bump light as the hardware wants it in the offset colour: alpha = the constant part, red and green = the
   light's height split by sine and cosine, blue = its direction round the surface as 0..255. height is the bump
   strength 0..1, elevation and azimuth are radians. Same layout as KallistiOS pvr_pack_bump, with SH4ZAM's sine. */
static uint32_t bump_light(float height, float elevation, float azimuth) {
    const shz_sincos_t sc = shz_sincosf(elevation);
    const uint32_t hp = (uint32_t)(height * 255.0f);
    const uint32_t k1 = 255u - hp;
    const uint32_t k2 = (uint32_t)((float)hp * sc.sin);
    const uint32_t k3 = (uint32_t)((float)hp * sc.cos);
    const uint32_t q = (uint32_t)(azimuth * (255.0f / TWO_PI)) & 255u;

    return (k1 << 24) | (k2 << 16) | (k3 << 8) | q;
}

/* ---------- headers ---------- */

/* Allocates and fills the textures and compiles every header once. Returns 0 if video memory ran out. */
static int probe_setup(void) {
    const int fmt = PVR_TXRFMT_RGB565 | PVR_TXRFMT_TWIDDLED;
    pvr_poly_cxt_t c;
    pvr_sprite_cxt_t s;

    tex_out = pvr_mem_malloc(TEX_SIZE * TEX_SIZE * 2);
    tex_in = pvr_mem_malloc(TEX_SIZE * TEX_SIZE * 2);
    tex_bump = pvr_mem_malloc(TEX_SIZE * TEX_SIZE * 2);
    tex_hud = pvr_mem_malloc(sizeof(hud_px));
    if (!tex_out || !tex_in || !tex_bump || !tex_hud) {
        return 0;
    }
    make_grid(tex_out, rgb565(40, 80, 200), rgb565(255, 255, 255));    /* outside a volume: blue, white lines */
    make_grid(tex_in, rgb565(240, 140, 20), rgb565(0, 0, 0));          /* inside a volume: orange, black lines */
    make_bump(tex_bump);

    /* untextured, opaque and translucent: bars, backdrops, ramps */
    pvr_poly_cxt_col(&c, PVR_LIST_OP_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&hdr_flat_op, &c);
    pvr_poly_compile(&hdr_clamp_off, &c);
    c.gen.color_clamp = true;                                          /* the same ramp through the clamp registers */
    pvr_poly_compile(&hdr_clamp_on, &c);
    pvr_poly_cxt_col(&c, PVR_LIST_TR_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_compile(&hdr_flat_tr, &c);

    /* fog patches: translucent white, once with the ordinary table fog and once with mode 2 */
    c.gen.fog_type = PVR_FOG_TABLE;
    pvr_poly_compile(&hdr_fog1, &c);
    c.gen.fog_type = PVR_FOG_TABLE2;
    pvr_poly_compile(&hdr_fog2, &c);

    /* cheap shadow: an ordinary textured opaque surface with the modifier bit set and the two-volume bit clear */
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, fmt, TEX_SIZE, TEX_SIZE, tex_out, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE;
    c.fmt.modifier = true;
    c.gen.modifier_mode = false;
    pvr_poly_compile(&hdr_shadow, &c);

    /* two-volume, opaque: blue outside, orange inside */
    pvr_poly_cxt_txr_mod(&c, PVR_LIST_OP_POLY, fmt, TEX_SIZE, TEX_SIZE, tex_out, PVR_FILTER_NONE,
                         fmt, TEX_SIZE, TEX_SIZE, tex_in, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_mod_compile(&hdr_twovol_op, &c);
    hdr_twovol_noshadow = hdr_twovol_op;                                /* the same words with the shadow bit cleared */
    hdr_twovol_noshadow.cmd &= ~PCW_SHADOW_BIT;

    /* two-volume with no texture: the vertex carries two colours */
    pvr_poly_cxt_col_mod(&c, PVR_LIST_OP_POLY);
    c.gen.culling = PVR_CULLING_NONE;
    pvr_poly_mod_compile(&hdr_twovol_col, &c);

    /* two-volume, translucent: the vertex alpha is 0 outside and full inside, so only the part in the volume shows */
    pvr_poly_cxt_txr_mod(&c, PVR_LIST_TR_POLY, fmt, TEX_SIZE, TEX_SIZE, tex_out, PVR_FILTER_NONE,
                         fmt, TEX_SIZE, TEX_SIZE, tex_in, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.alpha = c.gen.alpha2 = true;
    c.blend.src = c.blend.src2 = PVR_BLEND_SRCALPHA;
    c.blend.dst = c.blend.dst2 = PVR_BLEND_INVSRCALPHA;
    c.txr.env = c.txr2.env = PVR_TXRENV_MODULATEALPHA;
    pvr_poly_mod_compile(&hdr_twovol_tr, &c);

    /* hardware bump, the KallistiOS example's settings: the bump texture, the light in the offset colour, and the
       texture mode that turns the bump result (it arrives as alpha over white) into a grey over the black vertex colour */
    pvr_poly_cxt_txr(&c, PVR_LIST_OP_POLY, PVR_TXRFMT_BUMP | PVR_TXRFMT_TWIDDLED, TEX_SIZE, TEX_SIZE, tex_bump, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE;
    c.gen.specular = true;
    c.txr.env = PVR_TXRENV_DECAL;
    pvr_poly_compile(&hdr_bump_poly, &c);
    pvr_sprite_cxt_txr(&s, PVR_LIST_OP_POLY, PVR_TXRFMT_BUMP | PVR_TXRFMT_TWIDDLED, TEX_SIZE, TEX_SIZE, tex_bump, PVR_FILTER_NONE);
    s.gen.culling = PVR_CULLING_NONE;                                  /* as the polygons: the winding must not decide */
    s.gen.specular = PVR_SPECULAR_ENABLE;
    s.txr.env = PVR_TXRENV_DECAL;
    pvr_sprite_compile(&hdr_bump_sprite, &s);

    /* the label: a 512 x 64 ARGB1555 image, not twiddled, in front of everything */
    pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_ARGB1555 | PVR_TXRFMT_NONTWIDDLED, HUD_W, HUD_H, tex_hud, PVR_FILTER_NONE);
    c.gen.culling = PVR_CULLING_NONE;
    c.depth.comparison = PVR_DEPTHCMP_ALWAYS;
    c.depth.write = PVR_DEPTHWRITE_DISABLE;
    c.blend.src = PVR_BLEND_SRCALPHA;
    c.blend.dst = PVR_BLEND_INVSRCALPHA;
    pvr_poly_compile(&hdr_hud, &c);

    /* modifier volume headers, one pair a list: every triangle but the last, and the last (which closes the volume) */
    pvr_mod_compile(&mod_op_first, PVR_LIST_OP_MOD, PVR_MODIFIER_OTHER_POLY, PVR_CULLING_NONE);
    pvr_mod_compile(&mod_op_last, PVR_LIST_OP_MOD, PVR_MODIFIER_INCLUDE_LAST_POLY, PVR_CULLING_NONE);
    pvr_mod_compile(&mod_tr_first, PVR_LIST_TR_MOD, PVR_MODIFIER_OTHER_POLY, PVR_CULLING_NONE);
    pvr_mod_compile(&mod_tr_last, PVR_LIST_TR_MOD, PVR_MODIFIER_INCLUDE_LAST_POLY, PVR_CULLING_NONE);
    return 1;
}

/* ---------- the label ---------- */

/* Redraws the three text lines of screen s into hud_px (title, what to expect, verdict). The upload to video memory
   happens in the main loop after pvr_wait_ready, before the next scene is begun. A frame still being drawn may show the
   new text one frame early; for a label that is harmless. */
static void hud_build(int s) {
    static const char *const word[4] = { "CHECK: WAITING", "CHECK: PASS", "CHECK: FAIL", "READBACK UNAVAILABLE: EYE ONLY" };
    char line[HUD_CHARS + 1];
    unsigned i;

    memset(hud_px, 0, sizeof(hud_px));
    minifont_set_color(255, 255, 255);
    minifont_draw_str(hud_px, HUD_W, screen_title[s]);
    minifont_draw_str(hud_px + HUD_W * HUD_LINE_PX, HUD_W, screen_expect[s]);
    snprintf(line, sizeof(line), "%s %s", word[verdict[s]], verdict_text[s]);
    minifont_draw_str(hud_px + HUD_W * 2 * HUD_LINE_PX, HUD_W, line);
    for (i = 0; i < HUD_W * HUD_H; i++) {
        hud_px[i] = hud_px[i] ? 0xffffu : 0u;   /* a drawn pixel: opaque white; the rest: clear */
    }
}

/* ---------- the verdicts ---------- */

/* Reads the frame back for screen s and sets its verdict and its short text (the numbers the verdict was made from).
   Sample points are cell centres of the test square: column / row k is at QUAD + 32 + 64 k pixels. */
static void probe_check(int s) {
    const int qx = (int)QUAD_X, qy = (int)QUAD_Y, half = CELL_PX / 2;
    int a[3], b[3], c[3], d[3];
    int pass = 0;

    verdict_text[s][0] = '\0';
    if (s != S_BASE && readback_ok != 1) {   /* the read itself is not proven: no verdict */
        verdict[s] = V_NA;
        return;
    }
    switch (s) {
    case S_BASE:     /* each bar must read as its own colour */
        frame_px(qx + half, qy + 128, a);
        frame_px(qx + 128, qy + 128, b);
        frame_px(qx + 224, qy + 128, c);
        pass = a[0] > 160 && a[1] < 90 && a[2] < 90 && b[1] > 160 && b[0] < 90 && b[2] < 90 && c[2] > 160 && c[0] < 90 && c[1] < 90;
        readback_ok = pass;
        if (!pass && a[0] + a[1] + a[2] + b[0] + b[1] + b[2] + c[0] + c[1] + c[2] == 0) {
            /* all black: the program cannot see the frame at all (an emulator that keeps the frame off the CPU's side of
               video memory). That is not a failed screen: the bars may be perfectly drawn. */
            verdict[s] = V_NA;
            snprintf(verdict_text[s], sizeof(verdict_text[s]), "(FRAME READ SEES BLACK)");
            return;
        }
        snprintf(verdict_text[s], sizeof(verdict_text[s]), "R%d G%d B%d", a[0], b[1], c[2]);
        break;
    case S_SHADOW:   /* inside the volume: the same texel at about half the brightness */
        frame_px(qx + half, qy + 96, a);
        frame_px(qx + 224, qy + 96, b);
        pass = 4 * lum(a) > lum(b) && 4 * lum(a) < 3 * lum(b);
        snprintf(verdict_text[s], sizeof(verdict_text[s]), "IN %d OUT %d", lum(a), lum(b));
        break;
    case S_TV_A:
    case S_TV_B:
    case S_TV_C:
    case S_TV_D:
    case S_TV_E:        /* inside: orange (red over blue); outside: blue. The numbers say which way round a failure is */
        frame_px(qx + half, qy + 96, a);
        frame_px(qx + 224, qy + 96, b);
        pass = a[0] > a[2] + 60 && b[2] > b[0] + 60;
        snprintf(verdict_text[s], sizeof(verdict_text[s]), "IN R%d B%d OUT R%d B%d", a[0], a[2], b[0], b[2]);
        break;
    case S_TWOVOL_TR:   /* inside: orange; outside: the grey backdrop shows through untouched */
        frame_px(qx + half, qy + 96, a);
        frame_px(qx + 224, qy + 96, b);
        pass = a[0] > a[2] + 60 && b[0] < 110 && b[0] - b[2] < 30 && b[2] - b[0] < 30;
        snprintf(verdict_text[s], sizeof(verdict_text[s]), "IN R%d B%d OUT R%d B%d", a[0], a[2], b[0], b[2]);
        break;
    case S_FOG2:     /* lower row: the near patch gone (dark backdrop), the far patch the fog's cyan */
        frame_px(128 + 48, 256 + 48, a);
        frame_px(416 + 48, 256 + 48, b);
        pass = lum(a) < 240 && b[1] > 150 && b[2] > 150 && b[0] < 100;
        snprintf(verdict_text[s], sizeof(verdict_text[s]), "%s NEAR %d FAR R%d G%d",
                 lum(a) > 600 ? "LIKE MODE 1" : "", lum(a), b[0], b[1]);
        break;
    case S_BUMP:     /* light from the east: east faces bright, west faces dark, on the polygon and on the sprite */
        frame_px(96 + 106, qy + 66, a);     /* polygon, east face of the first pyramid (texel 26, 16) */
        frame_px(96 + 22, qy + 66, b);      /* polygon, west face (texel 5, 16) */
        frame_px(368 + 106, qy + 66, c);    /* sprite, east face */
        frame_px(368 + 22, qy + 66, d);     /* sprite, west face */
        pass = lum(a) > lum(b) + 150 && lum(c) > lum(d) + 150;
        snprintf(verdict_text[s], sizeof(verdict_text[s]), "POLY %d/%d SPR %d/%d", lum(a), lum(b), lum(c), lum(d));
        break;
    case S_CLAMP:    /* the clamped ramp is lifted at the dark end and capped at the bright end */
        frame_px(128 + 16, 150 + 40, a);    /* upper ramp (no clamp), dark end */
        frame_px(128 + 16, 260 + 40, b);    /* lower ramp (clamp), dark end */
        frame_px(512 - 16, 150 + 40, c);    /* upper ramp, bright end */
        frame_px(512 - 16, 260 + 40, d);    /* lower ramp, bright end */
        pass = lum(b) > lum(a) + 75 && lum(d) < lum(c) - 75;
        snprintf(verdict_text[s], sizeof(verdict_text[s]), "LO %d>%d HI %d>%d", lum(a), lum(b), lum(c), lum(d));
        break;
    default:
        break;
    }
    verdict[s] = pass ? V_PASS : V_FAIL;
}

/* ---------- one frame ---------- */

/* Submits screen s. frame counts from the moment the screen came up: the moving part holds still for PARK_FRAMES. */
static void probe_draw(int s, unsigned frame) {
    const float t = frame > PARK_FRAMES ? (float)(frame - PARK_FRAMES) : 0.0f;
    const float slide = 60.0f - 60.0f * shz_sincosf(t * 0.03f).cos;     /* the volume's sideways travel, 0..120 pixels; 0 while parked */
    const float vx = QUAD_X + 8.0f + slide;                             /* parked: over the first column */
    const float vy = QUAD_Y + 72.0f;
    const uint32_t white = 0xffffffffu;
    float azimuth = t * 0.02f;                                          /* the bump light's direction, radians */
    int i;

    while (azimuth >= TWO_PI) {
        azimuth -= TWO_PI;
    }

    pvr_list_begin(PVR_LIST_OP_POLY);
    switch (s) {
    case S_BASE:
        quad1(&hdr_flat_op, QUAD_X, QUAD_Y, QUAD_X + 64.0f, QUAD_Y + QUAD_SIZE, Z_SURFACE, 0xffff0000u);
        quad1(&hdr_flat_op, QUAD_X + 96.0f, QUAD_Y, QUAD_X + 160.0f, QUAD_Y + QUAD_SIZE, Z_SURFACE, 0xff00ff00u);
        quad1(&hdr_flat_op, QUAD_X + 192.0f, QUAD_Y, QUAD_X + QUAD_SIZE, QUAD_Y + QUAD_SIZE, Z_SURFACE, 0xff0000ffu);
        break;
    case S_SHADOW:
        quad1(&hdr_shadow, QUAD_X, QUAD_Y, QUAD_X + QUAD_SIZE, QUAD_Y + QUAD_SIZE, Z_SURFACE, white);
        break;
    case S_TV_A:
    case S_TV_B:
    case S_TV_C:
        quad_twovol(&hdr_twovol_op, white, white);
        break;
    case S_TV_D:
        quad_twovol(&hdr_twovol_noshadow, white, white);
        break;
    case S_TV_E:        /* the grid textures' own base colours, as plain vertex colours */
        quad_twovol_col(&hdr_twovol_col, 0xff2850c8u, 0xfff08c14u);
        break;
    case S_TWOVOL_TR:   /* a grey backdrop for the translucent square to show (or not show) over */
        quad1(&hdr_flat_op, QUAD_X - 32.0f, QUAD_Y - 32.0f, QUAD_X + QUAD_SIZE + 32.0f, QUAD_Y + QUAD_SIZE + 32.0f, Z_BACKDROP, 0xff3c3c3cu);
        break;
    case S_BUMP: {
        static pvr_sprite_txr_t sp __attribute__((aligned(32)));
        const uint32_t light = bump_light(BUMP_HEIGHT, BUMP_ELEVATION, azimuth);
        const uint32_t black[4] = { 0, 0, 0, 0 };

        quad(&hdr_bump_poly, 96.0f, QUAD_Y, 352.0f, QUAD_Y + QUAD_SIZE, Z_SURFACE, black, light);
        hdr_bump_sprite.argb = 0;
        hdr_bump_sprite.oargb = light;
        if (send_hdr(&hdr_bump_sprite)) {   /* a sprite: corners a (bottom left), b (top left), c (top right), d (bottom right) */
            memset(&sp, 0, sizeof(sp));
            sp.flags = PVR_CMD_VERTEX_EOL;
            sp.ax = 368.0f;
            sp.ay = QUAD_Y + QUAD_SIZE;
            sp.az = Z_SURFACE;
            sp.bx = 368.0f;
            sp.by = QUAD_Y;
            sp.bz = Z_SURFACE;
            sp.cx = 624.0f;
            sp.cy = QUAD_Y;
            sp.cz = Z_SURFACE;
            sp.dx = 624.0f;
            sp.dy = QUAD_Y + QUAD_SIZE;
            sp.auv = PVR_PACK_16BIT_UV(0.0f, 1.0f);
            sp.buv = PVR_PACK_16BIT_UV(0.0f, 0.0f);
            sp.cuv = PVR_PACK_16BIT_UV(1.0f, 0.0f);
            pvr_prim(&sp, sizeof(sp));
        }
        break;
    }
    case S_CLAMP: {
        const uint32_t ramp[4] = { 0xff000000u, 0xffffffffu, 0xff000000u, 0xffffffffu };   /* black on the left, white on the right */

        quad(&hdr_clamp_off, 128.0f, 150.0f, 512.0f, 230.0f, Z_SURFACE, ramp, 0);
        quad(&hdr_clamp_on, 128.0f, 260.0f, 512.0f, 340.0f, Z_SURFACE, ramp, 0);
        break;
    }
    default:
        break;
    }
    pvr_list_finish();

    if (s == S_SHADOW || s == S_TV_A || s == S_TV_C || s == S_TV_D || s == S_TV_E) {
        pvr_list_begin(PVR_LIST_OP_MOD);
        volume(&mod_op_first, &mod_op_last, vx, vy, vx + 112.0f, vy + 112.0f);
        pvr_list_finish();
    }
    if (s == S_TV_B) {
        pvr_list_begin(PVR_LIST_OP_MOD);
        volume_open(&mod_op_first, &mod_op_last, vx, vy, vx + 112.0f, vy + 112.0f);
        pvr_list_finish();
    }

    pvr_list_begin(PVR_LIST_TR_POLY);
    if (s == S_TWOVOL_TR) {
        quad_twovol(&hdr_twovol_tr, 0x00ffffffu, white);   /* outside: alpha 0; inside: alpha full */
    }
    if (s == S_FOG2) {   /* three patches at three depths, a row for each fog mode */
        static const float depth_w[3] = { 2.0f, 10.0f, 50.0f };   /* w of each column: at FOG_NEAR_W, between, at FOG_FAR_W */

        for (i = 0; i < 3; i++) {
            const float x = 128.0f + 144.0f * (float)i, z = shz_invf(depth_w[i]);

            quad1(&hdr_fog1, x, 128.0f, x + 96.0f, 224.0f, z, white);
            quad1(&hdr_fog2, x, 256.0f, x + 96.0f, 352.0f, z, white);
        }
    }
    {   /* the label */
        const uint32_t c[4] = { white, white, white, white };

        quad(&hdr_hud, HUD_X, HUD_Y, HUD_X + (float)HUD_W, HUD_Y + (float)HUD_H, Z_HUD, c, 0);
    }
    pvr_list_finish();

    if (s == S_TWOVOL_TR) {
        pvr_list_begin(PVR_LIST_TR_MOD);
        volume(&mod_tr_first, &mod_tr_last, vx, vy, vx + 112.0f, vy + 112.0f);
        pvr_list_finish();
    }
}

static void cont_reset_btn_callback_(uint8_t addr, uint32_t btns) {
    (void)addr;
    (void)btns;
    arch_exit();
}

int main(int argc, char **argv) {
    (void)argc; (void)argv;
    cont_btn_callback(0, CONT_RESET_BUTTONS, cont_reset_btn_callback_);
    /* opaque, opaque modifier, translucent, translucent modifier bins on; punch-through off (nothing here uses it) */
    pvr_init_params_t init = {
        { PVR_BINSIZE_16, PVR_BINSIZE_16, PVR_BINSIZE_16, PVR_BINSIZE_16, PVR_BINSIZE_0 },
        512 * 1024, 0, 0, 0, 3, 0
    };
    int screen = S_BASE, automatic = 1, hud_dirty = 1, stuck_shown = 0, summary = 0;
    unsigned frame = 0, stuck = 0, seen = 0;
    uint32_t held = 0;

    t0 = timer_us_gettime64();
    probe_stage(1, 160, 0, 0, "main");
    if (pvr_init(&init) < 0) {
        printf("probe: pvr_init FAIL\n");
        return 1;
    }
    pvr_set_bg_color(0.05f, 0.05f, 0.05f);
    probe_stage(2, 224, 96, 0, "pvr_init (modifier bins on)");

    /* registers a screen depends on. Fog and clamp are set once: only polygons that ask for them are touched. The cheap
       shadow switch is NOT set here: it is one switch for the whole frame (it would turn the two-volume screens into
       shadow screens), so the main loop sets it for every frame, on for the shadow screen only. */
    pvr_fog_table_color(1.0f, 0.0f, 1.0f, 1.0f);                         /* cyan fog */
    pvr_fog_table_linear(FOG_NEAR_W, FOG_FAR_W);
    PVR_SET(PVR_COLOR_CLAMP_MIN, PVR_PACK_COLOR(0.0f, CLAMP_FLOOR, CLAMP_FLOOR, CLAMP_FLOOR));
    PVR_SET(PVR_COLOR_CLAMP_MAX, PVR_PACK_COLOR(1.0f, CLAMP_CEIL, CLAMP_CEIL, CLAMP_CEIL));
    probe_stage(3, 200, 200, 0, "registers (shadow, fog, clamp)");

    if (!probe_setup()) {
        printf("probe: texture allocation FAIL\n");
        return 1;
    }
    probe_stage(4, 0, 160, 0, "textures + headers");
    dump_hdr("shadow", &hdr_shadow);            /* once: the words a console is sent, to check against the hardware documents */
    dump_hdr("twovol_op", &hdr_twovol_op);
    dump_hdr("twovol_noshadow", &hdr_twovol_noshadow);
    dump_hdr("twovol_col", &hdr_twovol_col);
    dump_hdr("twovol_tr", &hdr_twovol_tr);
    dump_hdr("mod_op_first", &mod_op_first);
    dump_hdr("mod_op_last", &mod_op_last);
    printf("probe: textures 88 KB, VRAM free %lu KB, sound RAM 0 KB\n", (unsigned long)pvr_mem_available() / 1024u);
    probe_stage(5, 200, 0, 200, "first submit");

    for (;;) {
        maple_device_t *pad = maple_enum_type(0, MAPLE_FUNC_CONTROLLER);
        cont_state_t *st = pad ? (cont_state_t *)maple_dev_status(pad) : NULL;
        const uint32_t now = st ? st->buttons : 0u, down = now & ~held;
        int next = screen;

        held = now;
        if (down & (CONT_A | CONT_DPAD_RIGHT)) {
            next = (screen + 1) % S_COUNT;
        }
        if (down & CONT_DPAD_LEFT) {
            next = (screen + S_COUNT - 1) % S_COUNT;
        }
        if (down & CONT_Y) {
            automatic = !automatic;
        }
        if (next != screen) {
            automatic = 0;                    /* a hand on the pad: stop cycling */
        } else if (automatic && frame >= AUTO_FRAMES) {
            next = (screen + 1) % S_COUNT;
        }
        if (next != screen) {
            screen = next;
            frame = 0;
            hud_dirty = 1;
        }

        if (pvr_wait_ready() < 0) {           /* KallistiOS gave up after 100 ms: the PVR did not finish the frame */
            if (++stuck >= STUCK_WAITS && !stuck_shown) {
                stuck_shown = 1;
                vid_clear(255, 255, 255);
                printf("probe: PVR stuck on screen %d (%s)\n", screen, screen_title[screen]);
            }
        } else {
            stuck = 0;
        }

        if (frame == CHECK_FRAME && verdict[screen] == V_WAIT) {
            probe_check(screen);
            hud_dirty = 1;
            if (!printed[screen]) {           /* one serial line a screen, ever */
                static const char *const word[4] = { "WAIT", "PASS", "FAIL", "READBACK UNAVAILABLE (eye only)" };

                printed[screen] = 1;
                seen++;
                printf("probe: %s: %s %s\n", screen_title[screen], word[verdict[screen]], verdict_text[screen]);
            }
        }
        if (seen == S_COUNT && !summary) {    /* every screen read once: one closing line */
            summary = 1;
            printf("probe: all %d screens read; volumes rejected %u, headers rejected %u\n", S_COUNT, vol_rejected, hdr_rejected);
        }
        if (hud_dirty) {
            hud_dirty = 0;
            hud_build(screen);
            pvr_txr_load(hud_px, tex_hud, sizeof(hud_px));
        }

        set_shadow_reg(screen);               /* whole-frame switch: each screen runs with its own value (screen_reg) */
        pvr_scene_begin();
        probe_draw(screen, frame);
        pvr_scene_finish();
        frame++;
    }
    return 0;
}
