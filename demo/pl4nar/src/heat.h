/* heat: the shimmer of hot air low over the road, far ahead. A window of the frame on screen (the one before this) is
 * copied into a small texture and drawn back over the same place through a mesh whose samples wander a pixel or two.
 * By day on a dry road only (heat_gate); at night or in rain nothing is copied and nothing is drawn.
 *
 * RECOMMENDED (no console run: every number an ESTIMATE; wiki/limits/header-recommended.md)
 * Cost:       video memory 16 KB, RAM 16 KB; SH-4 a frame: 8,192 uncached reads (0.4 to 0.8 ms) and 16 KB by store
 *             queue (about 0.1 ms); TA 6.4 KB; PVR one translucent layer of 384 x 64 px (about 0.25 ms: 8 % of the
 *             screen, scaled from a full-screen layer's 3.1 ms, PVR-002): all ESTIMATE. Off: 0
 * Start with: HEAT_ALPHA = 0.6 at half strength (the demo's LIGHT), HEAT_AMP_X = 2.0, HEAT_AMP_Y = 1.0, HEAT_DIST = 30.0
 * Status:     ADVISED: host tested (host/heat_test.c); the copy's time is not measured. To prove: the map autorun's
 *             last stage (stage 1 with the heat on STRONG) against stage 1, and the page's last copy line
 * Do:         heat_copy before any scene is open; heat_draw after the world in the translucent list; heat_reset
 *             after every pvr_init
 * Do not:     run it at night or in rain (heat_gate gives 0 there); expect a sharp band (3.5 px a texel across)
 * Maturity:   PROTOTYPE v0.1
 *
 * Cost, the screen at 640 x 480 (ESTIMATE, nothing here has run on a console):
 *   video memory  16 KB (HEAT_TW x HEAT_TH, RGB565, not twiddled)
 *   RAM           16 KB (the copy's rows, before the store queue takes them) and under 1 KB of state
 *   SH-4          8,192 reads of video memory a frame (uncached: 0.4 to 0.8 ms ESTIMATE) and 16 KB through the store
 *                 queue (about 0.1 ms ESTIMATE); the mesh is 125 vertices, two FSCA each (under 0.05 ms ESTIMATE)
 *   TA            1 header and 200 vertices: 6.4 KB
 *   PVR           one translucent textured layer of 384 x 64 pixels, no pass, no render target
 *   off           0: a gate of 0 copies nothing and sends nothing
 *
 * Order in a frame:
 *   1. before any scene is open: heat_copy(gate)                       (Dreamcast: reads the screen, loads the texture)
 *   2. in the screen's translucent list, after the world: heat_draw(ax, ay, z, gate, seconds)
 *      (ax, ay, z): a point of the road HEAT_DIST m ahead of the eye, on screen (heat_anchor gives the world point);
 *      z is its 1 / w, so what is nearer covers the layer. The next gfx_tri wants its own gfx_bind.
 *   after the PVR is started again (pvr_init): heat_reset(), the texture went with the video memory pool.
 *
 * What to know:
 *   - the picture in the layer is one or two frames old and 3.5 pixels a texel across: the band is soft. That is the look.
 *   - the copy holds what was drawn there, the layer itself included: the layer's share is HEAT_ALPHA at most, so an
 *     old frame's share falls by that factor a frame.
 *   - a near thing in front of the band (the car) is in the copy: its edge can show a pixel or two beside it. OPEN.
 *   - the PVR's dither is in the copy. OPEN on a console.
 *
 * The maths (the window, the copy, the mesh) is plain C through sh4der_core.h and host tested (host/heat_test.c). */
#ifndef PL4NAR_HEAT_H
#define PL4NAR_HEAT_H
#include <stdint.h>
#include "sh4der_core.h"

#define HEAT_TW        128u        /* the texture, texels */
#define HEAT_TH        64u
#define HEAT_WIN_W     448u        /* the screen window it holds, pixels: the band and a margin */
#define HEAT_WIN_H     96u
#define HEAT_BAND_W    384.0f      /* the band drawn, pixels across */
#define HEAT_BAND_UP   52.0f       /* over the anchor (to the horizon and a little above it) */
#define HEAT_BAND_DOWN 12.0f       /* under it */
#define HEAT_NX        24u         /* the mesh, cells (16 x 16 pixels) */
#define HEAT_NY        4u
#define HEAT_DIST      30.0f       /* the anchor: the road this far ahead of the eye, m */
#define HEAT_AMP_X     2.0f        /* a sample's wander, screen pixels */
#define HEAT_AMP_Y     1.0f
#define HEAT_HZ_X      2.3f        /* its turns a second */
#define HEAT_HZ_Y      1.7f
#define HEAT_CLIMB     0.21f       /* phase a row: the waves climb */
#define HEAT_ALPHA     0.6f        /* the layer's share at its middle */
#define HEAT_NIGHT_MAX 0.25f       /* the night level (0 day .. 1 night) at which the heat is gone */
#define HEAT_VERTS     ((HEAT_NX + 1u) * (HEAT_NY + 1u))

typedef struct heat_win {
    int ok;                        /* the window is on the screen */
    int x, y;                      /* its top left, screen pixels */
    uint16_t xs[HEAT_TW];          /* a texel column's pixel in a screen row */
    uint16_t ys[HEAT_TH];          /* a texel row's screen row */
} heat_win_t;

typedef struct heat_vtx { float x, y, u, v; uint32_t argb; } heat_vtx_t;

/* 0 .. 1: full by day on a dry road, gone by HEAT_NIGHT_MAX of night, nothing in any rain. */
static inline float heat_gate(float night, unsigned rain) {
    return rain ? 0.0f : sh4der_sat_(1.0f - night * (1.0f / HEAT_NIGHT_MAX));
}

/* The road HEAT_DIST ahead of the eye, toward `at`, at height road_y. 0: the eye looks straight down (no direction). */
static inline int heat_anchor(const float eye[3], const float at[3], float road_y, float out[3]) {
    const float dx = at[0] - eye[0], dz = at[2] - eye[2], d2 = sh4der_len2_(dx, dz);
    float k;
    if (!(d2 > 1e-6f)) return 0;
    k = sh4der_div_(HEAT_DIST, sh4der_sqrt_(d2));
    out[0] = eye[0] + dx * k; out[1] = road_y; out[2] = eye[2] + dz * k;
    return 1;
}

/* The window round the band of an anchor at (ax, ay), kept on a screen of sw x sh pixels. */
static inline int heat_win_set(heat_win_t *w, float ax, float ay, unsigned sw, unsigned sh) {
    const uint32_t stepx = (HEAT_WIN_W << 16) / HEAT_TW, stepy = (HEAT_WIN_H << 16) / HEAT_TH;   /* constants: folded */
    const float cy = ay + (HEAT_BAND_DOWN - HEAT_BAND_UP) * 0.5f;
    uint32_t p;
    unsigned i;
    int x, y;
    w->ok = 0;
    if (sw < HEAT_WIN_W || sh < HEAT_WIN_H || sw > 65535u || sh > 65535u) return 0;
    if (!(ax > -4096.0f && ax < 8192.0f && ay > -4096.0f && ay < 8192.0f)) return 0;
    x = (int)(ax - (float)(HEAT_WIN_W / 2u)); y = (int)(cy - (float)(HEAT_WIN_H / 2u));
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > (int)(sw - HEAT_WIN_W)) x = (int)(sw - HEAT_WIN_W);
    if (y > (int)(sh - HEAT_WIN_H)) y = (int)(sh - HEAT_WIN_H);
    w->x = x; w->y = y;
    for (i = 0, p = stepx >> 1; i < HEAT_TW; i++, p += stepx) w->xs[i] = (uint16_t)((unsigned)x + (p >> 16));
    for (i = 0, p = stepy >> 1; i < HEAT_TH; i++, p += stepy) w->ys[i] = (uint16_t)((unsigned)y + (p >> 16));
    w->ok = 1;
    return 1;
}

/* The copy: HEAT_TW x HEAT_TH pixels of a 16 bit frame of `stride` pixels a row into dst, rows in order (not twiddled). */
static inline void heat_grab(const heat_win_t *w, const volatile uint16_t *fb, unsigned stride, uint16_t *dst) {
    unsigned i, j;
    for (j = 0; j < HEAT_TH; j++) {
        const volatile uint16_t *row = fb + (unsigned)w->ys[j] * stride;
        for (i = 0; i < HEAT_TW; i++) *dst++ = row[w->xs[i]];
    }
}

/* A vertex's own phase, 0 .. 1 (an integer hash of its place: no table). */
static inline float heat_phase_(unsigned i, unsigned j, unsigned salt) {
    uint32_t h = (i * 0x9E3779B1u) ^ (j * 0x85EBCA77u) ^ (salt * 0xC2B2AE3Du);
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12;
    return (float)(h & 0xffffu) * (1.0f / 65536.0f);
}

/* The mesh of the band of anchor (ax, ay) at `seconds`, sampled from window w (the one the texture was copied through):
   (HEAT_NX + 1) x (HEAT_NY + 1) vertices in rows. A vertex keeps its place; its sample wanders. The share is 0 on the
   mesh's edge, so the band has no outline. Returns the vertices written: 0 when gate <= 0 or the window is not set. */
static inline unsigned heat_mesh(const heat_win_t *w, float ax, float ay, float seconds, float gate, heat_vtx_t out[HEAT_VERTS]) {
    static const float my[HEAT_NY + 1u] = { 0.0f, 0.7f, 1.0f, 0.6f, 0.0f };   /* top .. bottom: the most a little over the road's far end */
    const float tau = 6.2831853f, x0 = ax - HEAT_BAND_W * 0.5f, y0 = ay - HEAT_BAND_UP;
    const float dx = HEAT_BAND_W * (1.0f / (float)HEAT_NX), dy = (HEAT_BAND_UP + HEAT_BAND_DOWN) * (1.0f / (float)HEAT_NY);
    const float iu = 1.0f / (float)HEAT_WIN_W, iv = 1.0f / (float)HEAT_WIN_H;
    const float ulo = 0.5f / (float)HEAT_TW, vlo = 0.5f / (float)HEAT_TH;
    const float g = sh4der_sat_(gate) * HEAT_ALPHA * 255.0f, tx = sh4der_frac_(seconds * HEAT_HZ_X), ty = sh4der_frac_(seconds * HEAT_HZ_Y);
    unsigned i, j;
    if (!(gate > 0.0f) || !w->ok) return 0u;
    for (j = 0; j <= HEAT_NY; j++) {
        const float y = y0 + dy * (float)j, climb = HEAT_CLIMB * (float)j;
        for (i = 0; i <= HEAT_NX; i++) {
            const unsigned e = i < HEAT_NX - i ? i : HEAT_NX - i;
            const float x = x0 + dx * (float)i, mx = e >= 2u ? 1.0f : 0.5f * (float)e;
            float sx, sy, c;
            heat_vtx_t *o = &out[j * (HEAT_NX + 1u) + i];
            sh4der_sincos_(tau * sh4der_frac_(tx + climb + heat_phase_(i, j, 1u)), &sx, &c);
            sh4der_sincos_(tau * sh4der_frac_(ty + climb + heat_phase_(i, j, 2u)), &sy, &c);
            o->x = x; o->y = y;
            o->u = sh4der_clamp_((x + HEAT_AMP_X * sx - (float)w->x) * iu, ulo, 1.0f - ulo);
            o->v = sh4der_clamp_((y + HEAT_AMP_Y * sy - (float)w->y) * iv, vlo, 1.0f - vlo);
            o->argb = ((uint32_t)(g * mx * my[j] + 0.5f) << 24) | 0x00ffffffu;
        }
    }
    return HEAT_VERTS;
}

#ifdef _arch_dreamcast
#include <dc/pvr.h>
#include <dc/video.h>
#include <arch/timer.h>
#include "sh4der_guard.h"

typedef struct heat_state {
    pvr_ptr_t tex;                 /* NULL: not there yet, or no video memory (short) */
    int short_kb;                  /* 1: pvr_mem_malloc gave nothing; not asked again until heat_reset */
    int tex_ok;                    /* the texture holds a copy through `got` */
    pvr_poly_hdr_t hdr;
    heat_win_t next, got;          /* the window of the last heat_draw; the one the texture was copied through */
    uint32_t copy_us, copies, verts;   /* the last copy's time; copies made; vertices sent by the last heat_draw */
} heat_state_t;
static heat_state_t s_heat;
static uint16_t s_heat_rows[HEAT_TW * HEAT_TH] __attribute__((aligned(32)));

/* The texture went with the video memory pool (the PVR was started again): the next heat_copy asks for it anew. */
static inline void heat_reset(void) { s_heat.tex = NULL; s_heat.short_kb = 0; s_heat.tex_ok = 0; s_heat.next.ok = 0; }

/* Before any scene is open (the store queue is the copy's: no list may be open). Reads the frame on screen through the
   scan-out register, as src/sh4der_frame.c does. A gate of 0, a screen that is not RGB565 or no window yet: nothing. */
static inline void heat_copy(float gate) {
    s_heat.tex_ok = 0;
    if (!(gate > 0.0f) || !s_heat.next.ok || s_heat.short_kb || !vid_mode || vid_mode->pm != PM_RGB565) return;
    if (!s_heat.tex) {
        pvr_poly_cxt_t c;
        s_heat.tex = pvr_mem_malloc(HEAT_TW * HEAT_TH * 2u);
        if (!s_heat.tex) { s_heat.short_kb = 1; return; }
        pvr_poly_cxt_txr(&c, PVR_LIST_TR_POLY, PVR_TXRFMT_RGB565 | PVR_TXRFMT_NONTWIDDLED, (int)HEAT_TW, (int)HEAT_TH, s_heat.tex, PVR_FILTER_BILINEAR);
        c.gen.alpha = PVR_ALPHA_ENABLE;
        c.gen.culling = PVR_CULLING_NONE;
        c.gen.shading = PVR_SHADE_GOURAUD;
        c.txr.alpha = PVR_TXRALPHA_DISABLE;              /* RGB565: the texel's alpha is 1 */
        c.txr.env = PVR_TXRENV_MODULATEALPHA;            /* texel x vertex colour, alpha = vertex alpha */
        c.txr.uv_clamp = PVR_UVCLAMP_UV;
        c.blend.src = PVR_BLEND_SRCALPHA; c.blend.dst = PVR_BLEND_INVSRCALPHA;
        c.depth.comparison = PVR_DEPTHCMP_GEQUAL;
        c.depth.write = PVR_DEPTHWRITE_DISABLE;
        pvr_poly_compile(&s_heat.hdr, &c);
    }
    {
        const uint64_t t0 = timer_us_gettime64();
        const uintptr_t frame = PVR_GET(PVR_FB_ADDR) & (PVR_RAM_SIZE - 1u);
        if ((unsigned)vid_mode->width < HEAT_WIN_W || (unsigned)vid_mode->height < HEAT_WIN_H) return;
        heat_grab(&s_heat.next, (const volatile uint16_t *)((uintptr_t)PVR_RAM_BASE + frame), (unsigned)vid_mode->width, s_heat_rows);
        pvr_txr_load(s_heat_rows, s_heat.tex, HEAT_TW * HEAT_TH * 2u);
        s_heat.got = s_heat.next;
        s_heat.tex_ok = 1; s_heat.copies++;
        s_heat.copy_us = (uint32_t)(timer_us_gettime64() - t0);
    }
}

/* In the screen's translucent list. (ax, ay): the anchor on screen, z its 1 / w. Sets the window of the next copy;
   draws when this frame's copy was made. Returns the vertices sent. */
static inline unsigned heat_draw(float ax, float ay, float z, float gate, float seconds) {
    static heat_vtx_t m[HEAT_VERTS];
    unsigned i, j, sent = 0u;
    s_heat.verts = 0u;
    if (!(gate > 0.0f) || !heat_win_set(&s_heat.next, ax, ay, (unsigned)vid_mode->width, (unsigned)vid_mode->height)) { s_heat.next.ok = 0; return 0u; }
    if (!s_heat.tex_ok || !(z > 0.0f) || !sh4der_poly_hdr_ok((const uint32_t *)&s_heat.hdr)) return 0u;
    if (!heat_mesh(&s_heat.got, ax, ay, seconds, gate, m)) return 0u;
    {
        volatile uint32_t *d = (volatile uint32_t *)pvr_dr_target(NULL);
        const uint32_t *s = (const uint32_t *)&s_heat.hdr;
        d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3]; d[4] = s[4]; d[5] = s[5]; d[6] = s[6]; d[7] = s[7];
        pvr_dr_commit((void *)d);
    }
    for (j = 0; j < HEAT_NY; j++) {                      /* a strip a row of cells: the cells are 16 x 16 pixels, none without area */
        for (i = 0; i <= HEAT_NX; i++) {
            unsigned k;
            for (k = 0; k < 2u; k++) {
                const heat_vtx_t *v = &m[(j + k) * (HEAT_NX + 1u) + i];
                pvr_vertex_t *o = (pvr_vertex_t *)pvr_dr_target(NULL);
                o->flags = (i == HEAT_NX && k == 1u) ? PVR_CMD_VERTEX_EOL : PVR_CMD_VERTEX;
                o->x = v->x; o->y = v->y; o->z = z; o->u = v->u; o->v = v->v; o->argb = v->argb; o->oargb = 0u;
                pvr_dr_commit(o);
                sent++;
            }
        }
    }
    s_heat.verts = sent;
    return sent;
}
#endif /* _arch_dreamcast */
#endif
