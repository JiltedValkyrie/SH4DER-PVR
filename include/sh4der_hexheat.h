/* sh4der_hexheat: heat haze as a LATTICE of small hexagons fixed on the screen. Each hexagon carries the picture behind
 * it and is moved a little by a smooth wave field that rises through the lattice: a shimmer, never popping tiles. The
 * design and every formula are the owner's (the lattice, the field, its gradient as the shimmer, the envelope, the
 * lensing; the screen-to-world step: wiki/effects/hexheat.md).
 *
 * RECOMMENDED (no console run: every number an ESTIMATE; wiki/limits/header-recommended.md)
 * Cost:       video memory: a band of SH4DER_HEXHEAT_BAND_W x _BAND_H texels (64 KB at 16 bit) in a target the client
 *             already renders; SH-4 a frame: the far world sent a second time into the band (the client's count); the
 *             lattice's cells in the band walked (about 180: a ray to the ground each, two reciprocals); for a drawn
 *             cell (about 130 at the defaults: test/hexheat_test.c prints the count) two exp, a square root and
 *             SH4DER_HEXHEAT_LAYERS sine-cosine pairs (FSCA); TA: 6 vertices a drawn hexagon as a strip
 * Start with: the defaults (the owner's starting values): s 8 px (a hexagon 13.9 x 16 px), 3 layers, longest wave 7
 *             hexagon widths, rise 1.5 widths a second, wind 0.35 of the rise, haze layer 12.5 % of the screen's
 *             height, D0 30 m, peak offset 0.25 s (2 px), lens 0.1, the camera's travel in the scroll x 0.5
 * Use it for: a hot far view by day: a road's or a runway's far end, a desert horizon, the air over a far fire
 * Status:     ADVISED: host tested (test/hexheat_test.c, against a double-precision transcription of the formulas);
 *             NOT RUN on a console
 * Do:         fill the band in a pass that already runs (a render target's spare region), a texel a screen pixel, and
 *             sample it filtered (bilinear); give the field the time and the camera, never last frame's place
 * Do not:     put near things in the band (cut it to the near distance); move a hexagon's home; roll random numbers
 *             a frame; draw the hexagons nearer than the band's cut (they would cover what the band does not hold)
 * Maturity:   PROTOTYPE v0.1 (the lattice is the second design; no console run, so the label stays: wiki/status/maturity.md)
 *
 * How, on the PVR. The picture behind the hexagons is not read from the screen. A render pass the client already runs
 * (the street demo: its reflection pass) also draws the far end of the world as THIS frame's camera sees it into a band
 * region of its own target: BAND_W x BAND_H texels, a texel a screen pixel, holding only what is farther than the near
 * cut. A hexagon takes its texels from the band at its HOME and is drawn on the screen's translucent list at
 * home + offset (no blend, depth tested at the near cut's depth, not written). With no offset a hexagon is the picture
 * under it, pixel for pixel, so only the field's offsets show.
 *
 * The lattice is the screen's (a post effect lives in the frame buffer): pointy-top hexagons of size s (centre to
 * corner, pixels), homes from the integers (q, r) alone:  x0 = s sqrt(3) (q + r / 2),  y0 = 1.5 s r  (sh4der_hexheat_home).
 * A home never moves. A frame: position = home + offset(t).
 *
 * The screen knows nothing of the world, so every number the physics needs comes from a screen-to-world step, each
 * frame, for each cell: a ray from the camera through the cell's home, met with the ground (sh4der_hexheat_screen_to_ground:
 * the ray-to-ground step a strategy game uses to find where the mouse points). That gives the world point P and its
 * distance D. A ray that lands beyond the haze, or never lands (the homes over the road's far line: the far houses and
 * the sky), takes its point where it crosses an upright sheet across the ground at the haze's distance; there h is the
 * ray's height over the ground (sh4der_hexheat_screen_to_haze). Seen from a car, the ground beyond a 64 m cut is a
 * strip a few pixels high: nearly every cell is on the sheet.
 *
 * The maths (the owner's), in the world, as (X across the view, Y up), metres:
 *   field, N layers:  theta_i = k_i (X - U t) + m_i (Y - W t) + phi_i,   T' = sum a_i sin(theta_i),
 *                     k_(i+1) = 2 k_i, m_(i+1) = 2 m_i, a_(i+1) = a_i / 2, a_1 = 1;
 *                     k_1 = 2 pi / (WAVE_HEX hexagon widths), m_1 = 2 pi / (WAVE_HEX_UP hexagon widths)
 *   shimmer:          dX = -A sum a_i k_i cos(theta_i),   dY = -A sum a_i m_i cos(theta_i)   (the gradient of T')
 *   envelope:         A = A0 exp(-h / H) (1 - exp(-D / D0)), A0 tuned so that the largest offset is PEAK s at the
 *                     sheet's foot; times the gate, the fade above the near cut's line and the taper at the band's edges
 *   lensing:          scale = 1 + c A sum a_i (k_i^2 + m_i^2) sin(theta_i), about the hexagon's middle
 *   phases phi_i:     a fixed table (the only "random" values; never rolled again)
 *   back to pixels:   the offset is a bend of the line of sight: world offset / depth, x the focal length. The lattice's
 *                     own scale is the sheet's foot (a hexagon width in the world = its pixels x depth / focal there), so
 *                     a nearer cell's offset is larger by focal / D, held under OFFSET_MAX s (neighbours never overlap).
 * A hexagon keeps ONE size on the screen: world-sized hexagons (focal / D each) on homes that do not move cannot tile.
 * On the ground short of the sheet Y runs along the road (under 0, in metres short of the sheet), so the field is one
 * piece from the road up the sheet. The scrolls are wrapped to the longest wave (every layer repeats on it), so a long
 * run keeps its precision. A turn's sideways slip of the far picture (sh4der_hexheat_slip) and the camera's travel along
 * the ground (sh4der_hexheat_drive: the pattern streams toward the camera as the car speeds up) are summed into the
 * scroll: the field is a pure function of (home, camera, t, those two sums).
 *
 * What the doubling rule gives. k and m double together, so every layer runs along ONE line, (k_1, m_1): the field is
 * stripes across that line, and every hexagon slides back and forth along it (at the defaults the diagonal: up to the
 * right, down to the left). The stripes cross the view at W + U k_1 / m_1 pixels a second (the rise and the wind's
 * share). A camera that drives forward takes DRIVE x its speed x ppm off that: slower, the stripes climb; faster, they
 * run down; at the one speed where the two are equal they stand still and every offset holds (the street demo's view
 * at the defaults: 8.3 m/s, 30 km/h; test/hexheat_test.c pins it). A layer with a direction of its own would end both
 * the stripes and the standstill. It is not in the owner's formulas, so it is not here: wiki/effects/hexheat.md "Open".
 *
 * Maths only (sh4der_core.h: SH4ZAM's FSCA sine and cosine, its exp, FSRRA reciprocals and roots on the Dreamcast;
 * plain C under SH4DER_CORE_NO_SH4ZAM). The band's pass and the draw are the client's (demo/pl4nar/src/demo.c). */
#ifndef SH4DER_HEXHEAT_H
#define SH4DER_HEXHEAT_H
#include <stdint.h>
#include <string.h>
#include "sh4der_core.h"

#ifndef SH4DER_HEXHEAT_S
#define SH4DER_HEXHEAT_S           8.0f     /* s: a hexagon's centre to corner, screen pixels (pointy top: 13.9 wide, 16 high) */
#endif
#ifndef SH4DER_HEXHEAT_LAYERS
#define SH4DER_HEXHEAT_LAYERS      3u       /* N: the stacked waves, 3 or 4 */
#endif
#ifndef SH4DER_HEXHEAT_WAVE_HEX
#define SH4DER_HEXHEAT_WAVE_HEX    7.0f     /* the longest wave across the view, hexagon widths (6 to 8): k_1 */
#endif
#ifndef SH4DER_HEXHEAT_WAVE_HEX_UP
#define SH4DER_HEXHEAT_WAVE_HEX_UP 7.0f     /* the longest wave upward, hexagon widths (the owner's start: the same as across): m_1 */
#endif
#ifndef SH4DER_HEXHEAT_RISE_HEX
#define SH4DER_HEXHEAT_RISE_HEX    1.5f     /* W: the field's rise, hexagon widths a second (1 to 2) */
#endif
#ifndef SH4DER_HEXHEAT_WIND
#define SH4DER_HEXHEAT_WIND        0.35f    /* U = this x W, sideways (0.2 to 0.5) */
#endif
#ifndef SH4DER_HEXHEAT_LAYER_FRAC
#define SH4DER_HEXHEAT_LAYER_FRAC  0.125f   /* H: the haze layer's thickness, as a share of the screen's height seen at the sheet (0.10 to 0.15) */
#endif
#ifndef SH4DER_HEXHEAT_D0
#define SH4DER_HEXHEAT_D0          30.0f    /* D0: metres from the camera where the haze becomes plain */
#endif
#ifndef SH4DER_HEXHEAT_PEAK
#define SH4DER_HEXHEAT_PEAK        0.25f    /* the largest offset at the sheet's foot (the road's far line), in s */
#endif
#ifndef SH4DER_HEXHEAT_LENS
#define SH4DER_HEXHEAT_LENS        0.1f     /* c: the swell and shrink (0: none) */
#endif
#ifndef SH4DER_HEXHEAT_DRIVE
#define SH4DER_HEXHEAT_DRIVE       0.5f     /* the camera's travel in the scroll: 1 = the field stands in the world, 0 = no streaming */
#endif
#define SH4DER_HEXHEAT_OFFSET_MAX  0.32f    /* the largest offset on the screen, in s: under 1 / 3, so neighbours never overlap */
#define SH4DER_HEXHEAT_NEAR_FADE   0.5f     /* in s: the offset grows from 0 at the near cut's line to whole this far above it */
#define SH4DER_HEXHEAT_EDGE        24.0f    /* pixels: the offset grows from 0 over this from the band's left, right and top */
#define SH4DER_HEXHEAT_CUT         0.25f    /* pixels: a cell whose largest offset is under this is not drawn */
#define SH4DER_HEXHEAT_CELLS_MAX   200u     /* the most hexagons a frame (the band holds about 180 homes at s = 8) */
#define SH4DER_HEXHEAT_BAND_W      256u     /* the band, screen pixels = target texels: 8 x 4 tiles of 32 */
#define SH4DER_HEXHEAT_BAND_H      128u
#define SH4DER_HEXHEAT_FOOT        12.0f    /* rows of the band under the anchor: s, the near cut's line and a row to spare */

_Static_assert(SH4DER_HEXHEAT_LAYERS >= 1u && SH4DER_HEXHEAT_LAYERS <= 4u, "SH4DER_HEXHEAT_LAYERS is 1 to 4");
_Static_assert(SH4DER_HEXHEAT_OFFSET_MAX < 1.0f / 3.0f, "SH4DER_HEXHEAT_OFFSET_MAX must be under a third of s: neighbours would overlap");
_Static_assert(SH4DER_HEXHEAT_PEAK > 0.0f && SH4DER_HEXHEAT_PEAK <= SH4DER_HEXHEAT_OFFSET_MAX, "SH4DER_HEXHEAT_PEAK is over 0 and at most SH4DER_HEXHEAT_OFFSET_MAX");
_Static_assert(SH4DER_HEXHEAT_CUT > 0.0f && SH4DER_HEXHEAT_S >= 2.0f, "SH4DER_HEXHEAT_CUT is over 0 and SH4DER_HEXHEAT_S at least 2");

typedef struct sh4der_hexheat {
    float sx;                      /* the far picture's sideways slip, summed, wrapped to the longest wave, pixels at the sheet */
    float sy;                      /* the camera's travel x DRIVE, summed, wrapped, pixels at the sheet (the pattern's way down) */
} sh4der_hexheat_t;

/* A frame's camera and ground, for the screen-to-world step. Fill it with sh4der_hexheat_view_look, then
   sh4der_hexheat_view_ground; ok is 1 only after both. */
typedef struct sh4der_hexheat_view {
    float eye[3];
    float fwd[3], right[3], down[3];   /* unit, in the world: into the screen, the screen's +x, the screen's +y */
    float focal, inv_focal;            /* pixels at 45 degrees off the middle, and 1 / it */
    float cx, cy, sw, sh;              /* the screen's middle and size, pixels */
    float n[3], g[3], rg[3];           /* the ground's unit normal (the eye's side); forward and right along the ground, unit */
    float e;                           /* the eye's height over the ground, metres */
    float sheet, near_cut;             /* the haze sheet and the near cut: metres ahead along the ground */
    float w0;                          /* the depth of the sheet's foot on the view's centre line */
    float ppm;                         /* pixels a metre there: focal / w0 (the lattice's own scale) */
    float inv_h;                       /* 1 / H, a metre: H = LAYER_FRAC x the screen's height, in metres at the foot */
    float h_cut, inv_fade;             /* the near cut's line as a height at the sheet (under 0), and 1 / the fade above it */
    float grad;                        /* the largest |offset| a unit of A (sh4der_hexheat_grad_) */
    float a0;                          /* A0 for this view: the largest offset at the sheet's foot is PEAK s */
    float h_lim;                       /* h / H past which no offset reaches CUT */
    float nf, nr, nd, gf, gr, gd, rf, rr, rd;   /* n, g and rg, each along fwd, right and down */
    int ok;
} sh4der_hexheat_view_t;

/* Where a screen point's ray meets the haze. */
typedef struct sh4der_hexheat_hit {
    float p[3];                    /* the world point */
    float dist;                    /* D: its distance from the eye, metres */
    float w;                       /* its depth along the view */
    float h;                       /* its height over the ground (0 on the ground) */
    float hs;                      /* the ray's height where it crosses the sheet (under 0: it landed first) */
    float lat, up;                 /* the field's X and Y, metres: across from the view's centre line; up the sheet (under 0: on the ground, that far short of the sheet) */
    int ground;                    /* 1: the ray landed on the ground short of the sheet; 0: the point is on the sheet */
} sh4der_hexheat_hit_t;

typedef struct sh4der_hexheat_vtx { float x, y, u, v; } sh4der_hexheat_vtx_t;   /* x, y: the screen; u, v: band pixels */

/* The phases phi_i: chosen once, never rolled again. And the largest |sum of cos(2^i u + phi_i)| over u for the first N
   of them (test/hexheat_test.c scans it): what A0 is tuned by, so the largest offset is PEAK s and no more. */
static const float sh4der_hexheat_phi_[4] = { 1.7f, 4.1f, 0.6f, 5.3f };
static const float sh4der_hexheat_sum_[4] = { 1.0f, 1.9512722f, 2.8266031f, 3.1517631f };

static inline float sh4der_hexheat_hexw_(void) { return SH4DER_HEXHEAT_S * 1.7320508f; }                    /* a hexagon's width, px */
static inline float sh4der_hexheat_lx_(void) { return SH4DER_HEXHEAT_WAVE_HEX * sh4der_hexheat_hexw_(); }   /* the longest waves, px at the sheet */
static inline float sh4der_hexheat_ly_(void) { return SH4DER_HEXHEAT_WAVE_HEX_UP * sh4der_hexheat_hexw_(); }
static inline float sh4der_hexheat_k1_(void) { return 6.2831853f / sh4der_hexheat_lx_(); }                  /* hand: constants */
static inline float sh4der_hexheat_m1_(void) { return 6.2831853f / sh4der_hexheat_ly_(); }
static inline float sh4der_hexheat_w_(void) { return SH4DER_HEXHEAT_RISE_HEX * sh4der_hexheat_hexw_(); }    /* W, px a second at the sheet */
static inline float sh4der_hexheat_u_(void) { return SH4DER_HEXHEAT_WIND * sh4der_hexheat_w_(); }           /* U */
/* The largest |offset| a unit of A: |grad T'| at its largest (the phases' own sum, not the bound N). */
static inline float sh4der_hexheat_grad_(void) {
    const float k = sh4der_hexheat_k1_(), m = sh4der_hexheat_m1_();
    return sh4der_sqrt_(k * k + m * m) * sh4der_hexheat_sum_[SH4DER_HEXHEAT_LAYERS - 1u];
}
static inline float sh4der_hexheat_wrap_(float v, float len) { return v - len * sh4der_floor_(sh4der_div_(v, len)); }   /* v into 0 .. len */

static inline void sh4der_hexheat_reset(sh4der_hexheat_t *x) { memset(x, 0, sizeof *x); }

/* The lattice: the home of cell (q, r) on the screen, pixels. From the two integers alone: a home cannot move. */
static inline void sh4der_hexheat_home(int q, int r, float *x, float *y) {
    *x = sh4der_hexheat_hexw_() * ((float)q + 0.5f * (float)r);
    *y = 1.5f * SH4DER_HEXHEAT_S * (float)r;
}

/* ---- the camera ---------------------------------------------------------------------------------------------------- */

/* The camera of a frame: from eye toward target, `up` the world's up, fov the vertical field of view in radians, the
   screen screen_w x screen_h pixels (y down). The same view as sh4der_pl4nar_viewproj_rows gives (the test checks it).
   0: no direction (eye on target, up along the view) or a field of view outside 0 .. pi. */
static inline int sh4der_hexheat_view_look(sh4der_hexheat_view_t *v, const float eye[3], const float target[3], const float up[3],
                                           float fov, float screen_w, float screen_h) {
    float f[3] = { target[0] - eye[0], target[1] - eye[1], target[2] - eye[2] }, r[3], sn, cs;

    v->ok = 0;
    if (!sh4der_finite_(fov) || !(fov > 0.001f) || !(fov < 3.14f) || !(screen_w > 0.0f) || !(screen_h > 0.0f)) return 0;
    if (!(sh4der_len3_(f[0], f[1], f[2]) > 1e-12f)) return 0;
    sh4der_norm3_(f);
    sh4der_cross_(f, up, r);
    if (!(sh4der_len3_(r[0], r[1], r[2]) > 1e-12f)) return 0;
    sh4der_norm3_(r);
    memcpy(v->eye, eye, sizeof v->eye); memcpy(v->fwd, f, sizeof v->fwd); memcpy(v->right, r, sizeof v->right);
    sh4der_cross_(f, r, v->down);
    sh4der_sincos_(fov * 0.5f, &sn, &cs);
    v->focal = screen_h * 0.5f * sh4der_div_(cs, sn);
    v->inv_focal = sh4der_div_(sn, cs * screen_h * 0.5f);
    v->cx = screen_w * 0.5f; v->cy = screen_h * 0.5f; v->sw = screen_w; v->sh = screen_h;
    return 1;
}

/* The ground and the haze's place, after sh4der_hexheat_view_look: the ground is dot(n, p) = d (n unit, up); the haze
   sheet stands across the ground `sheet` metres ahead of the eye, and nothing nearer than near_cut is touched. On
   ground that is not flat, give the plane through the sheet's foot (the road's height there). The eye may then be
   level with that plane or under it (a climb toward a crest): no ray lands, every cell takes its point on the sheet,
   and the haze stays on (it never pops off on a hill). 0 (ok stays 0): the view looks straight down or up, or the two
   distances are not 0 < near_cut < sheet. */
static inline int sh4der_hexheat_view_ground(sh4der_hexheat_view_t *v, const float n[3], float d, float sheet, float near_cut) {
    float nf;
    unsigned k;

    v->ok = 0;
    v->e = sh4der_dot3_(n, v->eye) - d;
    if (!sh4der_finite_(v->e) || !sh4der_finite_(sheet) || !sh4der_finite_(near_cut)) return 0;   /* the bit test: -ffast-math folds a NaN compare away */
    if (!(v->e > -1.0e5f) || !(v->e < 1.0e5f) || !(near_cut > 0.0f) || !(sheet > near_cut) || !(sheet < 1.0e5f)) return 0;
    nf = sh4der_dot3_(n, v->fwd);
    for (k = 0; k < 3u; k++) { v->n[k] = n[k]; v->g[k] = v->fwd[k] - n[k] * nf; }
    if (!(sh4der_len3_(v->g[0], v->g[1], v->g[2]) > 1e-6f)) return 0;
    sh4der_norm3_(v->g);
    sh4der_cross_(v->g, v->n, v->rg);
    v->nf = nf; v->nr = sh4der_dot3_(n, v->right); v->nd = sh4der_dot3_(n, v->down);
    v->gf = sh4der_dot3_(v->g, v->fwd); v->gr = sh4der_dot3_(v->g, v->right); v->gd = sh4der_dot3_(v->g, v->down);
    v->rf = sh4der_dot3_(v->rg, v->fwd); v->rr = sh4der_dot3_(v->rg, v->right); v->rd = sh4der_dot3_(v->rg, v->down);
    v->sheet = sheet; v->near_cut = near_cut;
    v->w0 = sheet * v->gf - v->e * nf;
    if (!(v->w0 > 0.01f)) return 0;
    v->ppm = v->focal * sh4der_recip_pos_(v->w0);
    v->inv_h = v->ppm * sh4der_recip_pos_(SH4DER_HEXHEAT_LAYER_FRAC * v->sh);
    v->h_cut = v->e * (1.0f - sheet * sh4der_recip_pos_(near_cut));
    v->inv_fade = SH4DER_HEXHEAT_NEAR_FADE > 0.0f ? v->ppm * (1.0f / (SH4DER_HEXHEAT_NEAR_FADE * SH4DER_HEXHEAT_S)) : 1.0e6f;
    {   /* A0: tuned so that the largest offset is PEAK s at the sheet's foot, where h is 0 and D is the foot's distance */
        const float at_foot = 1.0f - sh4der_exp_(-sh4der_sqrt_(sheet * sheet + v->e * v->e) * (1.0f / SH4DER_HEXHEAT_D0));
        const float most = sh4der_div_(SH4DER_HEXHEAT_PEAK * SH4DER_HEXHEAT_S, at_foot);    /* the largest offset anywhere, before focal / D */

        v->grad = sh4der_hexheat_grad_();
        v->a0 = sh4der_div_(most, v->grad);
        v->h_lim = sh4der_ln_(most * (SH4DER_HEXHEAT_OFFSET_MAX / SH4DER_HEXHEAT_PEAK) * (1.0f / SH4DER_HEXHEAT_CUT));
        if (!(v->h_lim > 0.0f)) v->h_lim = 0.0f;
    }
    v->ok = 1;
    return 1;
}

/* World to screen: where the world point p is on the screen, pixels. 0: it is not in front of the camera. */
static inline int sh4der_hexheat_world_to_screen(const sh4der_hexheat_view_t *v, const float p[3], float *sx, float *sy) {
    const float rel[3] = { p[0] - v->eye[0], p[1] - v->eye[1], p[2] - v->eye[2] };
    const float w = sh4der_dot3_(rel, v->fwd);
    float k;

    if (!(w > 1.0e-3f)) return 0;
    k = v->focal * sh4der_recip_pos_(w);
    *sx = v->cx + sh4der_dot3_(rel, v->right) * k;
    *sy = v->cy + sh4der_dot3_(rel, v->down) * k;
    return 1;
}

/* Screen to world on the ground plane: the ray from the camera through the screen point (sx, sy), met with the ground.
   The same ray-to-ground step a strategy game uses to find where the mouse points. Gives the world point p and its
   distance from the eye (dist may be NULL). 0: the ray is level or climbs (the point is at or over the horizon), so it
   never lands; nothing is written. */
static inline int sh4der_hexheat_screen_to_ground(const sh4der_hexheat_view_t *v, float sx, float sy, float p[3], float *dist) {
    const float a = (sx - v->cx) * v->inv_focal, b = (sy - v->cy) * v->inv_focal;
    const float fall = -(v->nf + a * v->nr + b * v->nd);       /* metres the ray drops a metre of depth */
    float t;
    unsigned k;

    if (!v->ok || !(v->e > 0.0f) || !(fall > 1.0e-6f)) return 0;   /* an eye level with the plane or under it: no ray lands */
    t = v->e * sh4der_recip_pos_(fall);                         /* the depth where it lands */
    for (k = 0; k < 3u; k++) p[k] = v->eye[k] + t * (v->fwd[k] + a * v->right[k] + b * v->down[k]);
    if (dist) *dist = t * sh4der_sqrt_(1.0f + a * a + b * b);
    return 1;
}

/* The ray's numbers for the haze, without the world point: sh4der_hexheat_screen_to_haze's work, and the mesh's own. */
static inline int sh4der_hexheat_ray_(const sh4der_hexheat_view_t *v, float sx, float sy, sh4der_hexheat_hit_t *o, float ab[2]) {
    const float a = (sx - v->cx) * v->inv_focal, b = (sy - v->cy) * v->inv_focal;
    const float climb = v->nf + a * v->nr + b * v->nd;          /* the ray's rise a metre of depth (under 0: it drops) */
    const float along = v->gf + a * v->gr + b * v->gd;          /* its way along the ground a metre of depth */
    float t;

    if (!(along > 1.0e-4f)) return 0;                           /* it does not go forward over the ground */
    t = v->sheet * sh4der_recip_pos_(along);                    /* the depth where it crosses the sheet */
    o->hs = v->e + t * climb;
    if (o->hs >= 0.0f) { o->ground = 0; o->h = o->hs; o->up = o->hs; }
    else {                                                      /* it lands first: the ground pick */
        if (!(v->e > 0.0f)) return 0;                           /* the eye under the plane: the ray runs under the sheet's foot, no point */
        t = v->e * sh4der_recip_pos_(-climb);
        o->ground = 1; o->h = 0.0f; o->up = t * along - v->sheet;
    }
    o->w = t;
    o->lat = t * (v->rf + a * v->rr + b * v->rd);
    ab[0] = a; ab[1] = b;
    return 1;
}

/* Screen to world for the haze: the point a screen point's haze is judged at. First the ground (the ray-to-ground
   step); a ray that lands beyond the sheet, or never lands, takes its point where it crosses the sheet, and h is its
   height over the ground there. 0: the ray does not go forward over the ground (it points back, or straight up or down). */
static inline int sh4der_hexheat_screen_to_haze(const sh4der_hexheat_view_t *v, float sx, float sy, sh4der_hexheat_hit_t *o) {
    float ab[2];
    unsigned k;

    if (!v->ok || !sh4der_hexheat_ray_(v, sx, sy, o, ab)) return 0;
    for (k = 0; k < 3u; k++) o->p[k] = v->eye[k] + o->w * (v->fwd[k] + ab[0] * v->right[k] + ab[1] * v->down[k]);
    o->dist = o->w * sh4der_sqrt_(1.0f + ab[0] * ab[0] + ab[1] * ab[1]);
    return 1;
}

/* ---- the field ----------------------------------------------------------------------------------------------------- */

/* A frame's start: the far picture moved slip_px sideways on the screen since the frame before (a turn; 0 for none);
   the pattern goes with it. A slip that is not a number or larger than the screen (a cut) is not summed. */
static inline void sh4der_hexheat_slip(sh4der_hexheat_t *x, float slip_px) {
    if (sh4der_finite_(slip_px) && slip_px > -1000.0f && slip_px < 1000.0f) x->sx = sh4der_hexheat_wrap_(x->sx + slip_px, sh4der_hexheat_lx_());
}
/* The camera went `metres` forward along the ground since the frame before (its speed x dt); ppm: the view's pixels a
   metre (sh4der_hexheat_view_t). The pattern streams toward the camera by DRIVE of it. A frame's step is held at a
   quarter of the finest layer's wave, so a large step never reads as a small one the other way (the wagon wheel look;
   the street demo's view at the defaults: from about 108 m/s at 60 frames a second). The hold does not end the standstill at
   the one speed where the stream equals the rise and the wind's share (the head of this file). A jump of 8 m or more
   (a cut) is not summed. */
static inline void sh4der_hexheat_drive(sh4der_hexheat_t *x, float metres, float ppm) {
    const float most = sh4der_hexheat_ly_() * (0.25f / (float)(1u << (SH4DER_HEXHEAT_LAYERS - 1u)));
    float step;

    if (!sh4der_finite_(metres) || !sh4der_finite_(ppm) || !(metres > -8.0f && metres < 8.0f && ppm > 0.0f && ppm < 1.0e4f)) return;
    step = sh4der_clamp_(SH4DER_HEXHEAT_DRIVE * metres * ppm, -most, most);
    x->sy = sh4der_hexheat_wrap_(x->sy + step, sh4der_hexheat_ly_());
}

/* A frame's start, its view made: what the camera did since the frame before, into the scroll. g_before, eye_before:
   that frame's v->g and v->eye (the caller keeps them). A turn: the far picture went sideways on the screen, by the
   sheet's distance x the tangent of the turn; the pattern goes with it (sh4der_hexheat_slip). The way forward along the
   ground: the pattern streams toward the camera (sh4der_hexheat_drive). A turn of most of a right angle, or a jump: a
   cut, not summed. Not on the first frame: there is no frame before. */
static inline void sh4der_hexheat_follow(sh4der_hexheat_t *x, const sh4der_hexheat_view_t *v, const float g_before[3], const float eye_before[3]) {
    const float step[3] = { v->eye[0] - eye_before[0], v->eye[1] - eye_before[1], v->eye[2] - eye_before[2] };
    const float fore = sh4der_dot3_(g_before, v->g);

    if (!v->ok) return;
    if (fore > 0.1f) sh4der_hexheat_slip(x, v->sheet * sh4der_dot3_(g_before, v->rg) * sh4der_recip_pos_(fore) * v->ppm);
    sh4der_hexheat_drive(x, sh4der_dot3_(step, v->g), v->ppm);
}

/* The envelope's distance part with the view's A0 and the gate (0 .. 1): A0 (1 - exp(-D / D0)) gate. dist: metres. */
static inline float sh4der_hexheat_amp(const sh4der_hexheat_view_t *v, float dist, float gate) {
    if (!v->ok || !sh4der_finite_(dist) || !sh4der_finite_(gate) || !(dist > 0.0f) || !(gate > 0.0f)) return 0.0f;
    return v->a0 * (1.0f - sh4der_exp_(-dist * (1.0f / SH4DER_HEXHEAT_D0))) * sh4der_sat_(gate);
}

/* The field at (fx across, fy up), both in pixels at the sheet (metres x ppm), at `seconds`, amplitude A: the offset
   d[0] across and d[1] UP, in the same pixels, and the lensing scale (NULL: not wanted). A pure function: no state but
   the two sums. */
static inline void sh4der_hexheat_field(const sh4der_hexheat_t *x, float fx, float fy, float seconds, float A, float d[2], float *scale) {
    const float k1 = sh4der_hexheat_k1_(), m1 = sh4der_hexheat_m1_();
    const float px = fx - sh4der_hexheat_wrap_(sh4der_hexheat_u_() * seconds + x->sx, sh4der_hexheat_lx_());   /* X - U t, the slip with it */
    const float py = fy - sh4der_hexheat_wrap_(sh4der_hexheat_w_() * seconds - x->sy, sh4der_hexheat_ly_());   /* Y - W t, the drive against it */
    const float base = k1 * px + m1 * py;
    float mul = 1.0f, cs_sum = 0.0f, sn_sum = 0.0f;
    unsigned i;

    for (i = 0; i < SH4DER_HEXHEAT_LAYERS; i++) {               /* theta_i = 2^i (k_1 X' + m_1 Y') + phi_i; a_i k_i = k_1 */
        float sn, cs;
        sh4der_sincos_(mul * base + sh4der_hexheat_phi_[i], &sn, &cs);
        cs_sum += cs;
        sn_sum += mul * sn;                                     /* a_i (k_i^2 + m_i^2) = 2^i (k_1^2 + m_1^2) */
        mul *= 2.0f;
    }
    d[0] = -A * k1 * cs_sum;
    d[1] = -A * m1 * cs_sum;
    if (scale) *scale = 1.0f + SH4DER_HEXHEAT_LENS * A * (k1 * k1 + m1 * m1) * sn_sum;
}

/* ---- the band and the mesh ----------------------------------------------------------------------------------------- */

/* The band's place on the screen for an anchor at (ax, ay), the far view's point (the sheet's foot): its top left,
   whole pixels. The anchor is the band's middle across and FOOT rows over its bottom. 0: an anchor no screen holds. */
static inline int sh4der_hexheat_band(float ax, float ay, float *x0, float *y0) {
    if (!sh4der_finite_(ax) || !sh4der_finite_(ay) || !(ax > -4096.0f && ax < 8192.0f && ay > -4096.0f && ay < 8192.0f)) return 0;
    *x0 = (float)((int)(ax + 4096.5f) - 4096) - (float)(SH4DER_HEXHEAT_BAND_W / 2u);
    *y0 = (float)((int)(ay + 4096.5f) - 4096) + SH4DER_HEXHEAT_FOOT - (float)SH4DER_HEXHEAT_BAND_H;
    return 1;
}

/* This frame's hexagons: the lattice's cells whose hexagon lies wholly in the band (its top left at (x0, y0),
   sh4der_hexheat_band; a pixel inside its edge, for a filtered sample) and on the screen, each judged at its world
   point (sh4der_hexheat_screen_to_haze) at `seconds`; gate 0 .. 1 scales the offsets. 6 vertices a hexagon in strip
   order: triangle k is vertices k, k + 1, k + 2 (k = 0 .. 3). The screen corners are home + offset + scale x corner;
   the band corners are home + corner less (x0, y0): the picture a cell carries. Returns the hexagons written (at most
   `max`, and SH4DER_HEXHEAT_CELLS_MAX); out holds 6 x that. peak (NULL: not wanted): the largest offset written, pixels. */
static inline unsigned sh4der_hexheat_mesh(const sh4der_hexheat_t *x, const sh4der_hexheat_view_t *v, float seconds, float gate,
                                           float x0, float y0, sh4der_hexheat_vtx_t *out, unsigned max, float *peak) {
    const float s = SH4DER_HEXHEAT_S, hexw = sh4der_hexheat_hexw_(), hw = hexw * 0.5f, rowp = 1.5f * s, ihexw = 1.0f / hexw;
    const float cx[6] = { 0.0f, -hw, hw, -hw, hw, 0.0f }, cy[6] = { -s, -0.5f * s, -0.5f * s, 0.5f * s, 0.5f * s, s };
    const float kmax = SH4DER_HEXHEAT_OFFSET_MAX / SH4DER_HEXHEAT_PEAK;
    float left = x0 + 1.0f, right = x0 + (float)SH4DER_HEXHEAT_BAND_W - 1.0f, top = y0 + 1.0f, bottom = y0 + (float)SH4DER_HEXHEAT_BAND_H - 1.0f;
    const float lim = SH4DER_HEXHEAT_OFFSET_MAX * s;
    float a0g, most = 0.0f;
    unsigned n = 0;
    int r, r1;

    if (peak) *peak = 0.0f;
    if (!v->ok || !sh4der_finite_(gate) || !sh4der_finite_(seconds) || !sh4der_finite_(x0) || !sh4der_finite_(y0) || !(gate > 0.0f)) return 0u;
    if (max > SH4DER_HEXHEAT_CELLS_MAX) max = SH4DER_HEXHEAT_CELLS_MAX;
    if (left < 0.0f) left = 0.0f;
    if (top < 0.0f) top = 0.0f;
    if (right > v->sw) right = v->sw;
    if (bottom > v->sh) bottom = v->sh;
    if (!(right - left >= hexw) || !(bottom - top >= 2.0f * s)) return 0u;
    a0g = v->a0 * sh4der_sat_(gate);
    r = -(int)sh4der_floor_(-(top + s) * (1.0f / rowp));
    r1 = (int)sh4der_floor_((bottom - s) * (1.0f / rowp));
    for (; r <= r1 && n < max; r++) {
        const float hy = rowp * (float)r, half = 0.5f * (float)r, ty = hy - s - top;
        int q = -(int)sh4der_floor_(-((left + hw) * ihexw - half));
        const int q1 = (int)sh4der_floor_((right - hw) * ihexw - half);

        for (; q <= q1 && n < max; q++) {
            const float hx = hexw * ((float)q + half), tl = hx - hw - left, tr = right - hx - hw;
            const float edge = tl < tr ? (tl < ty ? tl : ty) : (tr < ty ? tr : ty);
            float taper, fade, A, k, big, d[2], sc, ox, oy;
            sh4der_hexheat_hit_t hit;
            float ab[2];
            unsigned c;

            taper = sh4der_sat_(edge * (1.0f / SH4DER_HEXHEAT_EDGE));
            if (!(taper > 0.0f) || !sh4der_hexheat_ray_(v, hx, hy, &hit, ab)) continue;
            fade = sh4der_sat_((hit.hs - v->h_cut) * v->inv_fade);          /* 0 at the near cut's line and under it */
            if (!(fade > 0.0f) || hit.h * v->inv_h > v->h_lim) continue;
            hit.dist = hit.w * sh4der_sqrt_(1.0f + ab[0] * ab[0] + ab[1] * ab[1]);
            A = a0g * (1.0f - sh4der_exp_(-hit.dist * (1.0f / SH4DER_HEXHEAT_D0))) * sh4der_exp_(-hit.h * v->inv_h) * fade * taper;
            k = v->w0 * sh4der_recip_pos_(hit.w);                           /* focal / D, against the lattice's own scale */
            if (k > kmax) k = kmax;
            big = A * v->grad * k;                                          /* this cell's largest offset, pixels */
            if (big < SH4DER_HEXHEAT_CUT) continue;
            if (big > lim) k *= sh4der_div_(lim, big);                      /* never past OFFSET_MAX s */
            sh4der_hexheat_field(x, hit.lat * v->ppm, hit.up * v->ppm, seconds, A, d, &sc);
            ox = (d[0] * v->rr + d[1] * v->nr) * k;                         /* the world's across and up, as the screen sees them */
            oy = (d[0] * v->rd + d[1] * v->nd) * k;
            if (peak) { const float o2 = ox * ox + oy * oy; if (o2 > most) most = o2; }
            for (c = 0; c < 6u; c++) {
                sh4der_hexheat_vtx_t *o = &out[n * 6u + c];
                o->x = hx + ox + sc * cx[c]; o->y = hy + oy + sc * cy[c];
                o->u = hx - x0 + cx[c];      o->v = hy - y0 + cy[c];
            }
            n++;
        }
    }
    if (peak) *peak = sh4der_sqrt_(most);
    return n;
}

#endif
