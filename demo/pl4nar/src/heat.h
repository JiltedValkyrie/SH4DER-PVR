/* heat: the shimmer of hot air over the road's FAR END. A small window of the frame on screen is copied 1 : 1 into a
 * texture and drawn back over the same place through a fine mesh whose samples drift under a pixel, slowly and
 * smoothly. By day on a dry road only (heat_gate); at night or in rain nothing is copied and nothing is drawn.
 *
 * RECOMMENDED (no console run: every number an ESTIMATE; wiki/limits/header-recommended.md)
 * Cost:       video memory 16 KB, RAM 16 KB; SH-4 a frame: 8,192 uncached reads (0.4 to 0.8 ms) and 16 KB by store
 *             queue (about 0.1 ms); TA 8.5 KB; PVR one translucent layer of 256 x 32 px (about 0.1 to 0.2 ms: under
 *             3 % of the screen, scaled from full-screen translucent bilinear layers, PVR-006): all ESTIMATE. Off: 0
 * Start with: the values USED IN THE DEMO (seen as host pictures, not proven on a console):
 *             amplitude  HEAT_AMP_Y = 0.7 px up and down, HEAT_AMP_X = 0.35 px sideways
 *             share      HEAT_ALPHA = 0.5 at the middle: the demo's STRONG (gate 1); LIGHT is gate 0.5, a share of 0.25
 *             anchor     HEAT_DIST = 70 m ahead of the eye: the road's far end
 *             window     HEAT_TW x HEAT_TH = 256 x 32 px, a texel a pixel, HEAT_BAND_UP = 22 px of it over the anchor
 *             cells      HEAT_NX x HEAT_NY = 32 x 4, 8 x 8 px each
 *             edge fade  HEAT_EDGE = 4 cells each side; top and bottom rows to nothing
 *             waves      HEAT_WAVE_1 = 96 px at HEAT_HZ_1 = 0.55 a second, HEAT_WAVE_2 = 64 px at HEAT_HZ_2 = 0.85,
 *                        HEAT_CLIMB = 0.11 a row
 *             lag        HEAT_LAG = 2 frames, HEAT_SLIP_MAX = 24 px
 *             gate       heat_gate(night, rain): 1 by day, 0 from night level HEAT_NIGHT_MAX = 0.25, 0 in any rain
 * Use it for: a hot dry day: the far end of a long straight road, tarmac or a runway, a desert or salt flat's
 *             horizon, the air over an exhaust or a fire seen from far off (move the anchor there)
 * Status:     ADVISED: host tested (host/heat_test.c) and seen by the owner as host pictures (approved 2026-11-25);
 *             nothing of it has run on a console, the copy's time is not measured. To prove: the autorun's fly-by
 *             stage (the heat on STRONG) on a console, and the page's last copy line; no stage pairs it with OFF yet
 * Do:         heat_copy before any scene is open; heat_view and heat_draw after the world in the translucent list;
 *             heat_reset after every pvr_init; keep the gate: it is what makes the cost 0 when there is no heat
 * Do not:     run it at night or in rain; put the anchor on or in front of near things (what is nearer than the
 *             anchor covers the layer, and its colour is in the copy); use it in a small window or a split screen
 *             under 256 x 32 px; raise the amplitude past a pixel or the share past 0.6 (the copy holds last frame's
 *             layer too)
 * Watch for:  turns: the layer follows the far picture by heat_slip and fades out past HEAT_SLIP_MAX; if it trails
 *             in a turn on a console, HEAT_LAG is wrong (try 1). Copy lag: the picture in the layer is 1 or 2 frames
 *             old, so a fast far thing (traffic crossing the far end) leaves a faint double. Near edges: a far pixel
 *             right beside a near thing's edge can take up to 0.7 px of its colour. The PVR's dither is in the copy
 * Maturity:   PROTOTYPE v0.1
 *
 * Version 2 (2026-11-25). What changed from version 1, which was seen as host pictures (swimming texels, jumping cells):
 *   - 1 : 1. A texel is a pixel (version 1: 3.5 pixels a texel, picked by nearest, so the band was soft and its texels
 *     jumped as the window moved). With no drift the layer is the frame itself, pixel for pixel.
 *   - the drift is a sum of two slow waves, the same for neighbours (version 1: a random phase a vertex, so cells jumped).
 *   - the amplitude is 0.7 px (version 1: 2 px).
 *   - the anchor is HEAT_DIST = 70 m ahead (version 1: 30 m) and the layer has that depth: what is nearer than the
 *     road's far end covers it and is not redrawn.
 *   - the copy is one or two frames old: the layer samples it where the far picture WAS in that frame (heat_slip, from
 *     the camera's two directions), so a turn does not drag the old picture across the new one; and the layer fades
 *     out while the far picture moves fast.
 *   - the edges fade over four cells each side, and to nothing at the top and the bottom.
 *
 * Cost, the screen at 640 x 480 (ESTIMATE, nothing here has run on a console):
 *   video memory  16 KB (HEAT_TW x HEAT_TH, RGB565, not twiddled)
 *   RAM           16 KB (the copy's rows, before the store queue takes them) and under 1 KB of state
 *   SH-4          8,192 reads of video memory a frame (uncached: 0.4 to 0.8 ms ESTIMATE) and 16 KB through the store
 *                 queue (about 0.1 ms ESTIMATE); the mesh is 165 vertices, two FSCA each (under 0.1 ms ESTIMATE)
 *   TA            1 header and 264 vertices: 8.5 KB
 *   PVR           one translucent textured layer of 256 x 32 pixels (a third of version 1's area), no pass, no target
 *   off           0: a gate of 0 copies nothing and sends nothing
 *
 * Order in a frame:
 *   1. before any scene is open: heat_copy(gate)                       (Dreamcast: reads the screen, loads the texture)
 *   2. in the screen's translucent list, after the world:
 *        heat_view(eye, at)                     the camera of this frame: the same eye and at heat_anchor gets
 *        heat_draw(ax, ay, z, gate, seconds)    (ax, ay, z): the road HEAT_DIST m ahead of the eye, on screen
 *      (heat_anchor gives the world point); z is its 1 / w, so what is nearer covers the layer. Without heat_view
 *      nothing is drawn (no direction: no slip). The next gfx_tri wants its own gfx_bind.
 *   after the PVR is started again (pvr_init): heat_reset().
 *
 * What to know:
 *   - what is nearer than the anchor is never redrawn (the depth test). Its colour is in the copy, so the far pixels
 *     right beside its edge can take up to 0.7 px of it at HEAT_ALPHA: a soft edge a pixel wide at most. OPEN.
 *   - which frame the screen shows when the copy is made (HEAT_LAG frames before the one being drawn) is an ESTIMATE:
 *     2. If a console shows the layer trailing in a turn, try 1.
 *   - the PVR's dither is in the copy. OPEN on a console.
 *
 * The maths (the window, the copy, the slip, the mesh) is plain C through sh4der_core.h and host tested (host/heat_test.c). */
#ifndef PL4NAR_HEAT_H
#define PL4NAR_HEAT_H
#include <stdint.h>
#include <string.h>
#include "sh4der_core.h"

#define HEAT_TW        256u        /* the texture and the window, texels = pixels */
#define HEAT_TH        32u
#define HEAT_BAND_UP   22.0f       /* the window's top over the anchor: to the horizon and a little above it */
#define HEAT_NX        32u         /* the mesh, cells (8 x 8 pixels) */
#define HEAT_NY        4u
#define HEAT_DIST      70.0f       /* the anchor: the road this far ahead of the eye, m: its far end */
#define HEAT_AMP_X     0.35f       /* a sample's drift, screen pixels */
#define HEAT_AMP_Y     0.7f
#define HEAT_WAVE_1    96.0f       /* the two waves' lengths across the band, pixels (12 and 8 cells: smooth over the mesh) */
#define HEAT_WAVE_2    64.0f
#define HEAT_HZ_1      0.55f       /* their turns a second */
#define HEAT_HZ_2      0.85f
#define HEAT_CLIMB     0.11f       /* phase a row: the waves lean as they climb */
#define HEAT_ALPHA     0.5f        /* the layer's share at its middle */
#define HEAT_EDGE      4u          /* cells over which each side fades in */
#define HEAT_LAG       2u          /* frames between the picture the copy reads and the frame being drawn: 1 or 2 (ESTIMATE) */
#define HEAT_SLIP_MAX  24.0f       /* the layer is gone when the far picture moved this many pixels since the copied frame */
#define HEAT_FOCAL     461.0f      /* the client's screen: pixels at 45 degrees off its middle (the street demo: 55 degrees high, 480 lines) */
#define HEAT_SLIP_SIGN_X (-1.0f)   /* the client's screen x against (up x forward): the street demo's (checked against its own projection on the host) */
#define HEAT_NIGHT_MAX 0.25f       /* the night level (0 day .. 1 night) at which the heat is gone */
#define HEAT_VERTS     ((HEAT_NX + 1u) * (HEAT_NY + 1u))

typedef struct heat_win {
    int ok;                        /* the window is on the screen */
    int x, y;                      /* its top left, screen pixels */
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

/* The window of an anchor at (ax, ay): HEAT_TW x HEAT_TH whole pixels round it, kept on a screen of sw x sh pixels. */
static inline int heat_win_set(heat_win_t *w, float ax, float ay, unsigned sw, unsigned sh) {
    int x, y;
    w->ok = 0;
    if (sw < HEAT_TW || sh < HEAT_TH || sw > 65535u || sh > 65535u) return 0;
    if (!(ax > -4096.0f && ax < 8192.0f && ay > -4096.0f && ay < 8192.0f)) return 0;
    x = (int)(ax + 4096.5f) - 4096 - (int)(HEAT_TW / 2u); y = (int)(ay + 4096.5f) - 4096 - (int)HEAT_BAND_UP;
    if (x < 0) x = 0;
    if (y < 0) y = 0;
    if (x > (int)(sw - HEAT_TW)) x = (int)(sw - HEAT_TW);
    if (y > (int)(sh - HEAT_TH)) y = (int)(sh - HEAT_TH);
    w->x = x; w->y = y;
    w->ok = 1;
    return 1;
}

/* The copy: the window's pixels of a 16 bit frame of `stride` pixels a row into dst, rows in order (not twiddled). */
static inline void heat_grab(const heat_win_t *w, const volatile uint16_t *fb, unsigned stride, uint16_t *dst) {
    unsigned i, j;
    for (j = 0; j < HEAT_TH; j++) {
        const volatile uint16_t *row = fb + ((unsigned)w->y + j) * stride + (unsigned)w->x;
        for (i = 0; i < HEAT_TW; i++) *dst++ = row[i];
    }
}

/* ---- which picture the copy holds: the anchors of the last frames ------------------------------------------------- */

/* The anchor is on the camera's own line of sight, so it stays put on the screen when the camera turns; what tells one
   frame's picture from the next is the direction the camera looked. heat_anchor's caller hands that direction on. */
typedef struct heat_trk {
    unsigned n;                    /* draws since the gate opened */
    float pf[3], pax, pay;         /* the draw before this one: where the camera looked (unit), its anchor */
    heat_win_t next; float nf[3];  /* the window the next copy reads, and where the camera looked in the picture it will find there */
    heat_win_t got; float gf[3];   /* the window the texture was copied through, and that picture's direction */
    int tex_ok;                    /* the texture holds a copy this frame */
} heat_trk_t;

static inline void heat_trk_reset(heat_trk_t *t) { memset(t, 0, sizeof *t); }

/* The unit direction from eye to at. 0: none. */
static inline int heat_look(const float eye[3], const float at[3], float f[3]) {
    f[0] = at[0] - eye[0]; f[1] = at[1] - eye[1]; f[2] = at[2] - eye[2];
    if (!(sh4der_len3_(f[0], f[1], f[2]) > 1e-9f)) return 0;
    sh4der_norm3_(f);
    return 1;
}

/* How far, in screen pixels, the far picture moved between a frame that looked along f_old and one that looks along
   f_now (both unit, the world's up is +y): what was at the screen's middle then is at middle + slip now. The far end
   only: a near thing also moves by the eye's own travel, which this does not know. focal: the screen's, in pixels.
   A turn of a right angle or more gives a slip no window holds (1e6). */
static inline void heat_slip(const float f_old[3], const float f_now[3], float focal, float slip[2]) {
    const float up[3] = { 0.0f, 1.0f, 0.0f };
    float r[3], u[3], d = sh4der_dot3_(f_old, f_now);
    sh4der_cross_(up, f_now, r);
    if (!(d > 0.1f) || !(sh4der_len3_(r[0], r[1], r[2]) > 1e-9f)) { slip[0] = slip[1] = 1.0e6f; return; }
    sh4der_norm3_(r);
    sh4der_cross_(f_now, r, u);
    d = sh4der_div_(focal, d);
    slip[0] = HEAT_SLIP_SIGN_X * sh4der_dot3_(f_old, r) * d; slip[1] = -sh4der_dot3_(f_old, u) * d;
}

/* Before the frame's scenes: 1 when the copy is to be made now (through t->got); the caller then calls heat_grab. */
static inline int heat_trk_copy(heat_trk_t *t, float gate) {
    t->tex_ok = 0;
    if (!(gate > 0.0f)) { heat_trk_reset(t); return 0; }
    if (!t->next.ok) return 0;
    t->got = t->next; memcpy(t->gf, t->nf, sizeof t->gf);
    t->tex_ok = 1;
    return 1;
}

/* In the frame's draw, the camera looking along f (unit), the anchor at (ax, ay): sets the next copy's window. The
   picture on screen when the next frame's copy is made is HEAT_LAG - 1 draws back from this one: this draw's own
   (lag 1) or the one before it (lag 2). */
static inline void heat_trk_draw(heat_trk_t *t, const float f[3], float ax, float ay, float gate, unsigned sw, unsigned sh) {
    if (!(gate > 0.0f)) { heat_trk_reset(t); return; }
    t->next.ok = 0;
    if (HEAT_LAG <= 1u) { if (heat_win_set(&t->next, ax, ay, sw, sh)) memcpy(t->nf, f, sizeof t->nf); }
    else if (t->n) { if (heat_win_set(&t->next, t->pax, t->pay, sw, sh)) memcpy(t->nf, t->pf, sizeof t->nf); }
    memcpy(t->pf, f, sizeof t->pf); t->pax = ax; t->pay = ay; t->n++;
}

/* ---- the mesh ------------------------------------------------------------------------------------------------------ */

/* The drift at a place (px, py) of the band, pixels from the anchor, at `seconds`: two slow waves across the band that
   lean as they climb. The same for neighbours: nothing jumps. */
static inline void heat_drift(float px, float py, float seconds, float out[2]) {
    const float tau = 6.2831853f, row = py * (HEAT_CLIMB * (1.0f / 8.0f));
    float s1, s2, c;
    sh4der_sincos_(tau * sh4der_frac_(px * (1.0f / HEAT_WAVE_1) + seconds * HEAT_HZ_1 + row + 64.0f), &s1, &c);
    sh4der_sincos_(tau * sh4der_frac_(-px * (1.0f / HEAT_WAVE_2) + seconds * HEAT_HZ_2 + 2.0f * row + 64.37f), &s2, &c);
    out[0] = HEAT_AMP_X * (0.6f * s2 + 0.4f * s1);
    out[1] = HEAT_AMP_Y * (0.6f * s1 + 0.4f * s2);
}

/* The mesh of the band, sampled from window w, copied from a picture that has since moved by `slip` pixels on the
   screen (heat_slip): (HEAT_NX + 1) x (HEAT_NY + 1) vertices in rows. The band is the window moved with the picture;
   a vertex's sample is its own place of the copied picture plus the drift. The share is 0 all round the edge.
   Returns the vertices written: 0 when gate <= 0, the window is not set, or the slip is HEAT_SLIP_MAX or more. */
static inline unsigned heat_mesh(const heat_win_t *w, const float slip[2], float seconds, float gate, heat_vtx_t out[HEAT_VERTS]) {
    static const float my[HEAT_NY + 1u] = { 0.0f, 0.6f, 1.0f, 0.6f, 0.0f };   /* top .. bottom */
    const float sx = slip[0], sy = slip[1], slip2 = sh4der_len2_(sx, sy);
    const float x0 = (float)w->x + sx, y0 = (float)w->y + sy;             /* the window's place in this frame */
    const float ax = x0 + (float)(HEAT_TW / 2u), ay = y0 + HEAT_BAND_UP;  /* the drift's own origin: it stays on the picture */
    const float dx = (float)HEAT_TW / (float)HEAT_NX, dy = (float)HEAT_TH / (float)HEAT_NY;
    const float iu = 1.0f / (float)HEAT_TW, iv = 1.0f / (float)HEAT_TH, ulo = 0.5f * iu, vlo = 0.5f * iv;
    float g;
    unsigned i, j;
    if (!(gate > 0.0f) || !w->ok || !(slip2 < HEAT_SLIP_MAX * HEAT_SLIP_MAX)) return 0u;
    g = sh4der_sat_(gate) * HEAT_ALPHA * 255.0f * (1.0f - slip2 * (1.0f / (HEAT_SLIP_MAX * HEAT_SLIP_MAX)));
    for (j = 0; j <= HEAT_NY; j++) {
        const float y = y0 + dy * (float)j;
        for (i = 0; i <= HEAT_NX; i++) {
            const unsigned e = i < HEAT_NX - i ? i : HEAT_NX - i;
            const float x = x0 + dx * (float)i, k = e >= HEAT_EDGE ? 1.0f : (float)e * (1.0f / (float)HEAT_EDGE), mx = k * k * (3.0f - 2.0f * k);
            float d[2];
            heat_vtx_t *o = &out[j * (HEAT_NX + 1u) + i];
            heat_drift(x - ax, y - ay, seconds, d);
            o->x = x; o->y = y;
            o->u = sh4der_clamp_((dx * (float)i + d[0] * mx) * iu, ulo, 1.0f - ulo);   /* no drift where there is no share: the edge is the frame */
            o->v = sh4der_clamp_((dy * (float)j + d[1] * mx * my[j]) * iv, vlo, 1.0f - vlo);
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
    pvr_poly_hdr_t hdr;
    heat_trk_t trk;
    uint32_t copy_us, copies, verts;   /* the last copy's time; copies made; vertices sent by the last heat_draw */
} heat_state_t;
static heat_state_t s_heat;
static uint16_t s_heat_rows[HEAT_TW * HEAT_TH] __attribute__((aligned(32)));
static float s_heat_look[3];       /* where the camera looks this frame: heat_view sets it, before heat_draw */

/* The camera of this frame, for heat_draw: call it where heat_anchor is called, with the same eye and at. */
static inline void heat_view(const float eye[3], const float at[3]) { if (!heat_look(eye, at, s_heat_look)) memset(s_heat_look, 0, sizeof s_heat_look); }

/* The texture went with the video memory pool (the PVR was started again): the next heat_copy asks for it anew. */
static inline void heat_reset(void) { s_heat.tex = NULL; s_heat.short_kb = 0; heat_trk_reset(&s_heat.trk); }

/* Before any scene is open (the store queue is the copy's: no list may be open). Reads the frame on screen through the
   scan-out register, as src/sh4der_frame.c does. A gate of 0, a screen that is not RGB565 or no window yet: nothing. */
static inline void heat_copy(float gate) {
    if (s_heat.short_kb || !vid_mode || vid_mode->pm != PM_RGB565 || (unsigned)vid_mode->width < HEAT_TW || (unsigned)vid_mode->height < HEAT_TH) { s_heat.trk.tex_ok = 0; return; }
    if (!heat_trk_copy(&s_heat.trk, gate)) return;
    if (!s_heat.tex) {
        pvr_poly_cxt_t c;
        s_heat.tex = pvr_mem_malloc(HEAT_TW * HEAT_TH * 2u);
        if (!s_heat.tex) { s_heat.short_kb = 1; s_heat.trk.tex_ok = 0; return; }
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
        heat_grab(&s_heat.trk.got, (const volatile uint16_t *)((uintptr_t)PVR_RAM_BASE + frame), (unsigned)vid_mode->width, s_heat_rows);
        pvr_txr_load(s_heat_rows, s_heat.tex, HEAT_TW * HEAT_TH * 2u);
        s_heat.copies++;
        s_heat.copy_us = (uint32_t)(timer_us_gettime64() - t0);
    }
}

/* In the screen's translucent list. (ax, ay): the anchor on screen, z its 1 / w. Draws when this frame's copy was made;
   sets the window of the next copy. Returns the vertices sent. */
static inline unsigned heat_draw(float ax, float ay, float z, float gate, float seconds) {
    static heat_vtx_t m[HEAT_VERTS];
    unsigned i, j, sent = 0u, n = 0u;
    s_heat.verts = 0u;
    if (s_heat.trk.tex_ok && z > 0.0f && sh4der_poly_hdr_ok((const uint32_t *)&s_heat.hdr)) {
        float slip[2];
        heat_slip(s_heat.trk.gf, s_heat_look, HEAT_FOCAL, slip);
        n = heat_mesh(&s_heat.trk.got, slip, seconds, gate, m);
    }
    heat_trk_draw(&s_heat.trk, s_heat_look, ax, ay, gate, (unsigned)vid_mode->width, (unsigned)vid_mode->height);
    if (!n) return 0u;
    {
        volatile uint32_t *d = (volatile uint32_t *)pvr_dr_target(NULL);
        const uint32_t *s = (const uint32_t *)&s_heat.hdr;
        d[0] = s[0]; d[1] = s[1]; d[2] = s[2]; d[3] = s[3]; d[4] = s[4]; d[5] = s[5]; d[6] = s[6]; d[7] = s[7];
        pvr_dr_commit((void *)d);
    }
    for (j = 0; j < HEAT_NY; j++) {                      /* a strip a row of cells: the cells are 8 x 8 pixels, none without area */
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
