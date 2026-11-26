/* sh4der_hexheat: heat haze as a few floating HEXAGONS that show the picture behind them a few pixels from where it belongs.
 * Each hexagon is a small flat-topped mesh (one strip of 6 vertices, 4 triangles). They stand in a band over a hot far
 * view, rise slowly, live a second or two and pop on and off; each one's shift is rolled again ten times a second. No
 * smooth wave, no blend, no tint.
 *
 * RECOMMENDED (no console run: every number an ESTIMATE; wiki/limits/header-recommended.md)
 * Cost:       video memory: a band of SH4DER_HEXHEAT_BAND_W x _BAND_H texels (64 KB at 16 bit) in a target the client
 *             already renders; SH-4: the far world sent a second time into the band (the client's count), and
 *             SH4DER_HEXHEAT_N hexagons placed a frame (hashes, no state a hexagon); TA: 6 vertices a drawn hexagon
 * Start with: the defaults: 48 slots (about 17 alive), 14 x 13 px, band 256 x 128 px, top 96 px, rise 2 px/s, shift 1 .. 5 px
 * Use it for: a hot far view by day: a road's or a runway's far end, a desert horizon, the air over a far fire
 * Status:     ADVISED: host tested (test/hexheat_test.c); nothing of it has run on a console
 * Do:         fill the band in a pass that already runs (a render target's spare region), a texel a screen pixel
 * Do not:     put near things in the band (cut it to a near distance); sample outside the band (mesh refuses those)
 * Maturity:   PROTOTYPE v0.1
 *
 * How, on the PVR. The picture behind the hexagons is not read from the screen. A render pass the client already runs
 * (the street demo: its reflection pass) also draws the far end of the world as THIS frame's camera sees it into a band
 * region of its own target: SH4DER_HEXHEAT_BAND_W x _BAND_H texels, a texel a screen pixel, on 32 pixel tile lines,
 * holding only what is farther than a near distance. A hexagon on the screen's translucent list (no blend, point sampled,
 * depth tested at the far view's depth) takes its texels from the band at its own place plus its shift. With no shift
 * a hexagon is the picture under it, pixel for pixel, so only the shift shows. Nothing is a frame late: no copy.
 *
 * The hexagons are fixed to the far picture: the client gives each frame's sideways slip of the far picture in pixels
 * (a turn) and they move with it, wrapping round the band. The reference numbers (size, shift, rise, lean, life) are
 * in the wiki's heat haze reference note.
 *
 * Maths only, plain C through sh4der_core.h: the lives, the shift, the band's place, the mesh. The band's pass and the
 * draw are the client's (demo/pl4nar/src/demo.c shows them). */
#ifndef SH4DER_HEXHEAT_H
#define SH4DER_HEXHEAT_H
#include <stdint.h>
#include <string.h>
#include "sh4der_core.h"

#ifndef SH4DER_HEXHEAT_N
#define SH4DER_HEXHEAT_N         48u      /* hexagon slots; about 0.35 of them are alive at a time */
#endif
#ifndef SH4DER_HEXHEAT_HEX_W
#define SH4DER_HEXHEAT_HEX_W         14.0f    /* a hexagon, pixels: flat top and bottom, points left and right */
#endif
#ifndef SH4DER_HEXHEAT_HEX_H
#define SH4DER_HEXHEAT_HEX_H         13.0f
#endif
#define SH4DER_HEXHEAT_BAND_W    256u     /* the band, screen pixels = target texels: 8 x 4 tiles of 32 */
#define SH4DER_HEXHEAT_BAND_H    128u
#define SH4DER_HEXHEAT_FOOT      6.0f     /* rows of the band under the anchor: room for the lowest hexagon's shift */
#define SH4DER_HEXHEAT_DENSE     72.0f    /* most hexagons start within this many pixels over the anchor */
#define SH4DER_HEXHEAT_TOP       96.0f    /* and none higher than this */
#define SH4DER_HEXHEAT_SPARSE    0.15f    /* the share that starts between DENSE and TOP */
#define SH4DER_HEXHEAT_RISE      2.0f     /* pixels a second upward */
#define SH4DER_HEXHEAT_LEAN      1.4f     /* and up to this many sideways, each its own */
#define SH4DER_HEXHEAT_WOBBLE    0.6f     /* pixels about its line */
#define SH4DER_HEXHEAT_LIFE_MIN  0.7f     /* a life, seconds: MIN + SPAN x h^2 over a hash h in 0 .. 1 (median 1.7) */
#define SH4DER_HEXHEAT_LIFE_SPAN 4.0f
#define SH4DER_HEXHEAT_GAP       0.6f     /* seconds a slot rests between two lives, at most */
#define SH4DER_HEXHEAT_SHIFT_MAX 5        /* a piece's shift, pixels a side: 1 .. 5, median 2 */
#define SH4DER_HEXHEAT_STEP_HZ   10.0f    /* the shift is rolled again this many times a second */

typedef struct sh4der_hexheat {
    float scroll;                  /* the far picture's sideways travel, summed, pixels */
} sh4der_hexheat_t;

typedef struct sh4der_hexheat_vtx { float x, y, u, v; } sh4der_hexheat_vtx_t;   /* x, y: the screen; u, v: band pixels */

static inline uint32_t sh4der_hexheat_hash_(uint32_t a, uint32_t b) {
    uint32_t h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15; h *= 0x2C1B3C6Du; h ^= h >> 12; h *= 0x297A2D39u; h ^= h >> 15;
    return h;
}
static inline float sh4der_hexheat_unit_(uint32_t h) { return (float)(h & 0xffffu) * (1.0f / 65536.0f); }
static inline float sh4der_hexheat_round_(float v) { return (float)((int)(v + 4096.5f) - 4096); }   /* whole pixel, -4096 and up */

static inline void sh4der_hexheat_reset(sh4der_hexheat_t *x) { memset(x, 0, sizeof *x); }

/* Hexagon i at `seconds`: 1 alive, 0 resting; its life's number (> 0); its middle in band coordinates: bx 0 .. BAND_W
   before the scroll, up pixels over the anchor. A pure function: nothing is stored a hexagon. */
static inline int sh4der_hexheat_at(unsigned i, float seconds, uint32_t *life, float *bx, float *up) {
    const float span = SH4DER_HEXHEAT_LIFE_MIN + SH4DER_HEXHEAT_LIFE_SPAN + SH4DER_HEXHEAT_GAP;   /* every life fits one cycle */
    const float t = seconds + span * sh4der_hexheat_unit_(sh4der_hexheat_hash_(i, 11u));
    const float cf = t * (1.0f / span);
    const uint32_t c = (uint32_t)cf;
    const float age = (cf - (float)c) * span;
    const float h = sh4der_hexheat_unit_(sh4der_hexheat_hash_(i, c * 4u + 1u));
    const float lifetime = SH4DER_HEXHEAT_LIFE_MIN + SH4DER_HEXHEAT_LIFE_SPAN * h * h;
    const float u = sh4der_hexheat_unit_(sh4der_hexheat_hash_(i, c * 4u + 2u));
    const float r = sh4der_hexheat_unit_(sh4der_hexheat_hash_(i, c * 4u + 3u));
    const float l = sh4der_hexheat_unit_(sh4der_hexheat_hash_(i, c * 4u + 4u));
    const float keep = 1.0f - SH4DER_HEXHEAT_SPARSE;
    float s, cs, start;

    *life = c + 1u;
    if (age >= lifetime) return 0;
    start = r < keep ? r * (1.0f / keep) * SH4DER_HEXHEAT_DENSE
                     : SH4DER_HEXHEAT_DENSE + (r - keep) * (1.0f / SH4DER_HEXHEAT_SPARSE) * (SH4DER_HEXHEAT_TOP - SH4DER_HEXHEAT_DENSE);
    sh4der_sincos_(6.2831853f * sh4der_frac_(age * 0.9f + u), &s, &cs);
    *bx = u * (float)SH4DER_HEXHEAT_BAND_W + (2.0f * l - 1.0f) * SH4DER_HEXHEAT_LEAN * age + SH4DER_HEXHEAT_WOBBLE * s;
    *up = SH4DER_HEXHEAT_HEX_H * 0.5f + start + SH4DER_HEXHEAT_RISE * age + SH4DER_HEXHEAT_WOBBLE * cs;
    return 1;
}

/* Hexagon i's shift in its life `life` at `seconds`, whole pixels: each side -SHIFT_MAX .. SHIFT_MAX, never both 0,
   small shifts the common ones. New every 1 / STEP_HZ seconds. */
static inline void sh4der_hexheat_shift(unsigned i, uint32_t life, float seconds, int *dx, int *dy) {
    const uint32_t step = (uint32_t)(seconds * SH4DER_HEXHEAT_STEP_HZ);
    const uint32_t roll = sh4der_hexheat_hash_(i * 131u + life * 8191u, step * 7u + 5u);
    const uint32_t k = 2u * SH4DER_HEXHEAT_SHIFT_MAX + 1u;
    int x = (int)(((roll & 0xffu) * k) >> 8) - SH4DER_HEXHEAT_SHIFT_MAX;
    int y = (int)((((roll >> 8) & 0xffu) * k) >> 8) - SH4DER_HEXHEAT_SHIFT_MAX;

    if ((roll >> 16) & 1u) x /= 2;
    if ((roll >> 17) & 1u) y /= 2;
    if (!x && !y) y = (roll >> 18) & 1u ? 1 : -1;
    *dx = x; *dy = y;
}

/* A frame's start: the far picture moved slip_px sideways on the screen since the frame before (a turn; 0 for none).
   A slip that is not a number or larger than the screen (a cut) is not summed. */
static inline void sh4der_hexheat_slip(sh4der_hexheat_t *x, float slip_px) {
    const float bw = (float)SH4DER_HEXHEAT_BAND_W;
    if (slip_px > -1000.0f && slip_px < 1000.0f) x->scroll += slip_px;
    if (x->scroll > 8.0f * bw || x->scroll < -8.0f * bw) x->scroll -= bw * (float)(int)(x->scroll * (1.0f / bw));
}

/* The band's place on the screen for an anchor at (ax, ay), the far view's point: its top left, whole pixels. The anchor
   is the band's middle across and FOOT rows over its bottom. 0: an anchor no screen holds. */
static inline int sh4der_hexheat_band(float ax, float ay, float *x0, float *y0) {
    if (!(ax > -4096.0f && ax < 8192.0f && ay > -4096.0f && ay < 8192.0f)) return 0;
    *x0 = sh4der_hexheat_round_(ax) - (float)(SH4DER_HEXHEAT_BAND_W / 2u);
    *y0 = sh4der_hexheat_round_(ay) + SH4DER_HEXHEAT_FOOT - (float)SH4DER_HEXHEAT_BAND_H;
    return 1;
}

/* This frame's hexagons at `seconds`, the band's top left at (x0, y0) (sh4der_hexheat_band). 6 vertices a hexagon in
   strip order: triangle k is vertices k, k + 1, k + 2 (k = 0 .. 3). A hexagon is drawn when it lives, its slot is in use
   (gate 0 .. 1: the share of the slots) and both it and its piece (it moved by its shift) lie wholly in the band.
   Returns the hexagons written; out holds 6 x SH4DER_HEXHEAT_N. */
static inline unsigned sh4der_hexheat_mesh(const sh4der_hexheat_t *x, float seconds, float gate, float x0, float y0, sh4der_hexheat_vtx_t *out) {
    static const float px[6] = { -0.5f, -0.25f, -0.25f, 0.25f, 0.25f, 0.5f }, py[6] = { 0.0f, -0.5f, 0.5f, -0.5f, 0.5f, 0.0f };
    const float bw = (float)SH4DER_HEXHEAT_BAND_W, bh = (float)SH4DER_HEXHEAT_BAND_H;
    const float hw = SH4DER_HEXHEAT_HEX_W * 0.5f, hh = SH4DER_HEXHEAT_HEX_H * 0.5f, foot = bh - SH4DER_HEXHEAT_FOOT;
    unsigned use, i, k, n = 0;

    if (!(gate > 0.0f)) return 0u;
    use = (unsigned)(sh4der_sat_(gate) * (float)SH4DER_HEXHEAT_N + 0.5f);
    for (i = 0; i < use; i++) {
        uint32_t life;
        float bx, up, cx, cy, sx, sy;
        int dx, dy;

        if (!sh4der_hexheat_at(i, seconds, &life, &bx, &up)) continue;
        cx = sh4der_hexheat_round_(sh4der_frac_((bx + x->scroll) * (1.0f / bw) + 64.0f) * bw);    /* in the band, wrapped */
        cy = sh4der_hexheat_round_(foot - up);
        sh4der_hexheat_shift(i, life, seconds, &dx, &dy);
        sx = cx + (float)dx; sy = cy + (float)dy;
        if (cx - hw < 0.0f || cx + hw > bw || sx - hw < 0.0f || sx + hw > bw) continue;
        if (cy - hh < 0.0f || cy + hh > bh || sy - hh < 0.0f || sy + hh > bh) continue;
        for (k = 0; k < 6u; k++) {
            sh4der_hexheat_vtx_t *o = &out[n * 6u + k];
            o->x = x0 + cx + px[k] * SH4DER_HEXHEAT_HEX_W; o->y = y0 + cy + py[k] * SH4DER_HEXHEAT_HEX_H;
            o->u = sx + px[k] * SH4DER_HEXHEAT_HEX_W;      o->v = sy + py[k] * SH4DER_HEXHEAT_HEX_H;
        }
        n++;
    }
    return n;
}

#endif
