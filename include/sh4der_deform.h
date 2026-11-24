/* SH4DER-PVR: ground that keeps what pressed into it (snow, mud, sand)

   sh4der_deform.h
   Copyright (c) 2026 Jilted Valkyrie
   See ../LICENSE. Dreamcast math: SH4ZAM, Copyright (C) 2025, 2026 Falco Girgis (MIT).
*/

/** \file    sh4der_deform.h
    \brief   A depth grid for soft ground: feet and wheels press dents into it, the
             displaced material stands up as a rim, and the ground either keeps the
             dent (snow) or slowly closes it (mud).

    Cost first. RAM: one byte a cell, so 25,600 B (25 KB) for a 160 x 160 window at
    0.25 m a cell (40 m across), plus the state struct (sh4der_deform_t: 52 B with the
    Dreamcast's 4-byte pointers, 56 B on a 64-bit host; the test prints the real size);
    the decal list is 20 B a print (644 B for 32). Video memory: 0 for the grid; a
    footprint decal texture is the caller's. CPU, estimated, not measured on a
    console: a stamp touches only the cells under it (a 0.15 m foot at 0.25 m cells
    is 9 to 16 cells) and never more than the window; a height read is four byte
    loads and three blends, once a ground vertex; a refill pass is one add over every
    cell (25,600 bytes) and runs only when the refill rate is not 0.
    Triangles: none of its own; it bends the ground mesh the caller already draws.

    The grid stores height, not polygons: the caller owns a ground mesh (any regular
    grid of vertices) and asks sh4der_deform_height for the y of every vertex each
    frame. A dent can therefore be no sharper than the mesh that shows it. For a
    crisp print on a coarse mesh, lay a decal over the dent: the print list at the
    end of this file keeps the last few prints and builds their quads.

    How a cell is stored: one unsigned byte. SH4DER_DEFORM_LEVEL (128) is the
    undisturbed surface, `ceil_y`. 0 is the hard bottom, `floor_y`: nothing presses
    deeper. Values above 128 are the raised rim, up to 127 steps above the surface.
    One step is (ceil_y - floor_y) / 128 world units.

    The window follows the camera: sh4der_deform_recentre moves it in whole cells
    and the storage wraps round (no copy), so the cells that leave on one side are
    the ones that enter, fresh, on the other. A dent outside the window is gone.

    Who presses: the caller. Anything the game marks as deforming (a foot bone, a
    capsule's bottom, a wheel) calls sh4der_deform_stamp once a frame with its
    lowest point and a radius. The dent is a bowl (a paraboloid under the point),
    eased in by `amount` so a foot sinks over a few frames, and it only ever
    presses down: a foot lifted out leaves the dent behind.

    Hostile input: every public function checks its floats with the library's own
    finite test (a bit test that still works under -ffast-math). A NaN, an infinity
    or a value outside the documented range is refused (the function changes
    nothing and returns 0), and positions are clamped in float to the window before
    they become integers, so no input can overflow an int or make a loop long.

    No allocation, no globals: the caller owns the cell array (any alignment: it is
    read and written a byte at a time) and the state. The maths is SH4ZAM through
    sh4der_core.h (fused multiply-add, blend, clamp, divide); define
    SH4DER_CORE_NO_SH4ZAM for the plain C path. By hand: cell indices (integer
    arithmetic) and the byte rounding.

    Version: v0.10
    The effect: footprints and trails stay in the ground; a low sun or a lamp fills
      them with shade; with a refill rate the ground closes again behind the walker.
    Typical use: walkers and vehicles leaving tracks in snow.
    Use it for: snow, mud, sand, ash, tall grass pressed flat, a wake in shallow
      water, tyre tracks.

    \author Jilted Valkyrie

    PVR terms (TA, TR/OP list, EOL, twiddled, direct render): see the PVR glossary in README.md.
*/

#ifndef SH4DER_DEFORM_H
#define SH4DER_DEFORM_H

#include <string.h>
#include "sh4der_core.h"

#ifdef __cplusplus
extern "C" {
#endif

/** \name Default tuning
    @{ */
#define SH4DER_DEFORM_LEVEL      128      /**< \brief Cell value of the undisturbed surface (0 = the hard bottom, 255 = the highest rim). */
#define SH4DER_DEFORM_GRID_W     160      /**< \brief Default window width in cells (40 m at the default cell). */
#define SH4DER_DEFORM_GRID_H     160      /**< \brief Default window depth in cells. */
#define SH4DER_DEFORM_CELL       0.25f    /**< \brief Default cell size, world units (metres). */
#define SH4DER_DEFORM_RIM_REACH  1.6f     /**< \brief The rim ends at this many stamp radii from the stamp's centre. */
#define SH4DER_DEFORM_RIM        0.25f    /**< \brief Default rim height as a share of the dent's depth (0 = no rim). */
#define SH4DER_DEFORM_PRINTS     32       /**< \brief Prints the decal list keeps; the oldest is replaced. */
/** @} */

/** \name Input limits (anything outside them is refused or clamped; see "Hostile input" in the file comment)
    @{ */
#define SH4DER_DEFORM_MAX_DIM     8192       /**< \brief Largest window width or depth, cells (keeps w x h and the cell indices far from int overflow). */
#define SH4DER_DEFORM_MAX_CELLS   1.0e7f     /**< \brief Largest cell index a position may map to, cells (under 2^24, so it is exact in a float). */
#define SH4DER_DEFORM_MAX_HEIGHT  1.0e6f     /**< \brief Largest height magnitude (floor_y, ceil_y, a stamp's bottom_y), world units. */
#define SH4DER_DEFORM_MIN_RADIUS  1.0e-4f    /**< \brief Smallest stamp radius, world units (0.1 mm: below it the bowl's slope overflows). */
#define SH4DER_DEFORM_MAX_RADIUS  1.0e3f     /**< \brief Largest stamp radius, world units (a kilometre: far wider than any window). */
#define SH4DER_DEFORM_MAX_STEPS   1.0e6f     /**< \brief Most refill steps one call applies (far above the 255 that close any byte cell, far below int overflow). */
#define SH4DER_DEFORM_MIN_STEP    1.0e-9f    /**< \brief Smallest world height of one cell step (keeps 1 / step finite). */
/** @} */

/** \brief The grid and its window. Fill it with sh4der_deform_init. */
typedef struct sh4der_deform {
    uint8_t *cells;        /**< \brief w x h bytes, the caller's; row z, column x, both wrapped (see the file comment). */
    int w;                 /**< \brief Window width in cells. */
    int h;                 /**< \brief Window depth in cells. */
    int origin_x;          /**< \brief World cell index of the window's first column. */
    int origin_z;          /**< \brief World cell index of the window's first row. */
    float cell;            /**< \brief Cell size, world units. */
    float inv_cell;        /**< \brief 1 / cell. */
    float floor_y;         /**< \brief World height of the hard bottom (cell value 0). */
    float ceil_y;          /**< \brief World height of the undisturbed surface (cell value 128). */
    float step;            /**< \brief World height of one cell step: (ceil_y - floor_y) / 128. */
    float rim;             /**< \brief Rim height as a share of the dent's depth, 0 .. 1. */
    float refill;          /**< \brief Steps a second every cell moves back toward the surface; 0 keeps dents for ever (snow). */
    float refill_carry;    /**< \brief Fraction of a step the refill has not applied yet. */
} sh4der_deform_t;

/* floor of x as an integer, with x held to +-SH4DER_DEFORM_MAX_CELLS first so the conversion cannot overflow; a
   non-finite x gives 0 (hand: integer conversion; x - frac(x) is the floor for negatives too) */
static inline int sh4der_deform_floor_(float x) {
    float v;

    if (!sh4der_finite_(x)) {
        return 0;
    }
    v = sh4der_clamp_(x, -SH4DER_DEFORM_MAX_CELLS, SH4DER_DEFORM_MAX_CELLS);
    return (int)(v - sh4der_frac_(v));
}

/* i wrapped into 0 .. n - 1, for negative i too (hand: integer arithmetic) */
static inline int sh4der_deform_wrap_(int i, int n) {
    const int m = i % n;

    return m < 0 ? m + n : m;
}

/* the byte of world cell (ix, iz), or NULL outside the window */
static inline uint8_t *sh4der_deform_at_(const sh4der_deform_t *d, int ix, int iz) {
    if (ix < d->origin_x || ix >= d->origin_x + d->w || iz < d->origin_z || iz >= d->origin_z + d->h) {
        return NULL;
    }
    return d->cells + (size_t)sh4der_deform_wrap_(iz, d->h) * (size_t)d->w + (size_t)sh4der_deform_wrap_(ix, d->w);
}

/** \brief Sets the grid up, every cell at the undisturbed surface, the window's first cell at world (0, 0).
    cells: w x h bytes the caller keeps alive. w, h: 2 .. SH4DER_DEFORM_MAX_DIM. cell: world units a cell, > 0 (and not so
    small that 1 / cell overflows). floor_y < ceil_y: the hard bottom and the surface, finite, at most SH4DER_DEFORM_MAX_HEIGHT
    in size, and at least 128 x SH4DER_DEFORM_MIN_STEP apart. The rim takes SH4DER_DEFORM_RIM and the refill 0 (snow); set
    d->rim and d->refill afterwards to change them.
    Returns 1, or 0 for a bad argument (nothing is written). */
static inline int sh4der_deform_init(sh4der_deform_t *d, uint8_t *cells, int w, int h, float cell, float floor_y, float ceil_y) {
    float inv_cell;
    float step;

    if (!d || !cells || w < 2 || h < 2 || w > SH4DER_DEFORM_MAX_DIM || h > SH4DER_DEFORM_MAX_DIM) {
        return 0;
    }
    if (!sh4der_finite_(cell) || !sh4der_finite_(floor_y) || !sh4der_finite_(ceil_y) || !(cell > 0.0f) || !(ceil_y > floor_y)) {
        return 0;
    }
    if (!(floor_y >= -SH4DER_DEFORM_MAX_HEIGHT) || !(ceil_y <= SH4DER_DEFORM_MAX_HEIGHT)) {
        return 0;
    }
    inv_cell = sh4der_div_(1.0f, cell);
    step = (ceil_y - floor_y) * (1.0f / (float)SH4DER_DEFORM_LEVEL);
    if (!sh4der_finite_(inv_cell) || !(step >= SH4DER_DEFORM_MIN_STEP)) {
        return 0;
    }
    d->cells = cells;
    d->w = w;
    d->h = h;
    d->origin_x = 0;
    d->origin_z = 0;
    d->cell = cell;
    d->inv_cell = inv_cell;
    d->floor_y = floor_y;
    d->ceil_y = ceil_y;
    d->step = step;
    d->rim = SH4DER_DEFORM_RIM;
    d->refill = 0.0f;
    d->refill_carry = 0.0f;
    memset(cells, SH4DER_DEFORM_LEVEL, (size_t)w * (size_t)h);
    return 1;
}

/** \brief Moves the window so that world point (x, z) is at its middle, in whole cells. Cells that come into the window
    start at the undisturbed surface; cells that stay keep their dents. Call it once a frame with the camera's (or the
    player's) position. A non-finite x or z moves nothing. A position more than SH4DER_DEFORM_MAX_CELLS cells from the
    origin is held to that distance. Returns the number of cells that were reset (0 when the window did not move). */
static inline unsigned sh4der_deform_recentre(sh4der_deform_t *d, float x, float z) {
    float fx;
    float fz;
    int nx;
    int nz;
    int dx;
    int dz;
    int i;
    int j;
    unsigned reset = 0;

    if (!d || !d->cells || !sh4der_finite_(x) || !sh4der_finite_(z)) {
        return 0;
    }
    fx = x * d->inv_cell;
    fz = z * d->inv_cell;
    if (!sh4der_finite_(fx) || !sh4der_finite_(fz)) {                /* a finite position can still overflow once scaled to cells */
        return 0;
    }
    nx = sh4der_deform_floor_(fx) - d->w / 2;
    nz = sh4der_deform_floor_(fz) - d->h / 2;
    dx = nx - d->origin_x;
    dz = nz - d->origin_z;
    if (dx == 0 && dz == 0) {
        return 0;
    }
    if (dx >= d->w || dx <= -d->w || dz >= d->h || dz <= -d->h) {   /* a jump past the whole window: nothing survives */
        memset(d->cells, SH4DER_DEFORM_LEVEL, (size_t)d->w * (size_t)d->h);
        d->origin_x = nx;
        d->origin_z = nz;
        return (unsigned)(d->w * d->h);
    }
    /* columns: the |dx| world columns that enter reuse the storage columns of the ones that left */
    for (i = 0; i < (dx < 0 ? -dx : dx); i++) {
        const int world = dx > 0 ? d->origin_x + d->w + i : nx + i;
        const int col = sh4der_deform_wrap_(world, d->w);

        for (j = 0; j < d->h; j++) {
            d->cells[(size_t)j * (size_t)d->w + (size_t)col] = SH4DER_DEFORM_LEVEL;
        }
        reset += (unsigned)d->h;
    }
    d->origin_x = nx;
    /* rows, the same way (a corner cell reset twice is counted twice; the count is a measure of work, not of area) */
    for (i = 0; i < (dz < 0 ? -dz : dz); i++) {
        const int world = dz > 0 ? d->origin_z + d->h + i : nz + i;
        const int row = sh4der_deform_wrap_(world, d->h);

        memset(d->cells + (size_t)row * (size_t)d->w, SH4DER_DEFORM_LEVEL, (size_t)d->w);
        reset += (unsigned)d->w;
    }
    d->origin_z = nz;
    return reset;
}

/** \brief Presses one round source into the ground. (x, z): its centre in the world, finite. bottom_y: the world height of
    its lowest point (a foot's sole, the bottom of a capsule), finite, at most SH4DER_DEFORM_MAX_HEIGHT in size.
    radius: its size in the plane, world units, SH4DER_DEFORM_MIN_RADIUS .. SH4DER_DEFORM_MAX_RADIUS.
    amount: 0 .. 1, how far a cell moves toward the pressed height in this call (1 = at once; 0.25 a frame sinks a foot
    in about half a second at 30 frames a second).
    Under the source the ground is pressed to a bowl, bottom_y at the centre rising to bottom_y + radius / 2 at the edge,
    never below the hard bottom and never raising a cell. From the edge out to SH4DER_DEFORM_RIM_REACH radii the rim
    stands up: d->rim of the dent's depth at its highest, never lowering a cell.
    A source whose lowest point is above the surface, or outside the window, changes nothing; so does any argument that
    is not finite or is out of its range. The loop is clamped to the window before it starts, so its cost never exceeds
    one pass over the window, whatever the radius.
    Returns the number of cells changed. */
static inline unsigned sh4der_deform_stamp(sh4der_deform_t *d, float x, float z, float bottom_y, float radius, float amount) {
    float reach;
    float r2;
    float inv_2r;
    float inv_step;
    float depth_steps;
    float rim_steps;
    float inv_ring;
    float rim_share;
    float fx0;
    float fx1;
    float fz0;
    float fz1;
    float win_x0;
    float win_x1;
    float win_z0;
    float win_z1;
    int x0;
    int x1;
    int z0;
    int z1;
    int ix;
    int iz;
    unsigned changed = 0;

    if (!d || !d->cells) {
        return 0;
    }
    if (!sh4der_finite_(x) || !sh4der_finite_(z) || !sh4der_finite_(bottom_y) || !sh4der_finite_(radius) || !sh4der_finite_(amount)) {
        return 0;
    }
    if (!(radius >= SH4DER_DEFORM_MIN_RADIUS) || !(radius <= SH4DER_DEFORM_MAX_RADIUS) || !(amount > 0.0f)) {
        return 0;
    }
    if (!(bottom_y >= -SH4DER_DEFORM_MAX_HEIGHT) || !(bottom_y < d->ceil_y)) {
        return 0;
    }
    amount = sh4der_sat_(amount);
    reach = radius * SH4DER_DEFORM_RIM_REACH;

    /* the cell range the stamp can touch, in float cell units; refuse a position too large to scale, then clamp to the
       window BEFORE any integer conversion (a radius of a kilometre is then still one pass over the window at most) */
    fx0 = (x - reach) * d->inv_cell;
    fx1 = (x + reach) * d->inv_cell;
    fz0 = (z - reach) * d->inv_cell;
    fz1 = (z + reach) * d->inv_cell;
    if (!sh4der_finite_(fx0) || !sh4der_finite_(fx1) || !sh4der_finite_(fz0) || !sh4der_finite_(fz1)) {
        return 0;
    }
    win_x0 = (float)d->origin_x;
    win_x1 = (float)(d->origin_x + d->w - 1);
    win_z0 = (float)d->origin_z;
    win_z1 = (float)(d->origin_z + d->h - 1);
    if (fx0 >= win_x1 + 1.0f || fx1 < win_x0 - 1.0f || fz0 >= win_z1 + 1.0f || fz1 < win_z0 - 1.0f) {
        return 0;                                                   /* wholly outside the window */
    }
    x0 = sh4der_deform_floor_(sh4der_clamp_(fx0, win_x0, win_x1));
    x1 = sh4der_deform_floor_(sh4der_clamp_(fx1 + 1.0f, win_x0, win_x1));   /* floor(f) + 1 is floor(f + 1) */
    z0 = sh4der_deform_floor_(sh4der_clamp_(fz0, win_z0, win_z1));
    z1 = sh4der_deform_floor_(sh4der_clamp_(fz1 + 1.0f, win_z0, win_z1));

    r2 = radius * radius;
    inv_2r = sh4der_div_(0.5f, radius);                         /* the bowl: height over the lowest point = dist^2 / (2 radius) */
    inv_step = sh4der_div_(1.0f, d->step);
    depth_steps = (d->ceil_y - (bottom_y < d->floor_y ? d->floor_y : bottom_y)) * inv_step;   /* the dent's depth at its centre, in steps */
    rim_share = sh4der_finite_(d->rim) ? sh4der_sat_(d->rim) : 0.0f;   /* a corrupt rim share means no rim */
    rim_steps = depth_steps * rim_share;
    inv_ring = sh4der_div_(sh4der_div_(1.0f, reach - radius), reach + radius);   /* 1 / (reach^2 - radius^2): the ring's width in squared distance */
    for (iz = z0; iz <= z1; iz++) {
        for (ix = x0; ix <= x1; ix++) {
            uint8_t *c = sh4der_deform_at_(d, ix, iz);
            float dxw;
            float dzw;
            float dist2;
            float target;
            float now;
            int v;

            if (!c) {
                continue;
            }
            dxw = (float)ix * d->cell - x;
            dzw = (float)iz * d->cell - z;
            dist2 = sh4der_fma_(dxw, dxw, dzw * dzw);
            now = (float)*c;
            if (dist2 <= r2) {                                  /* under the source: press down toward the bowl */
                const float y = sh4der_fma_(dist2, inv_2r, bottom_y);

                target = (float)SH4DER_DEFORM_LEVEL - (d->ceil_y - (y < d->floor_y ? d->floor_y : y)) * inv_step;
                if (target >= now) {                            /* the ground is already lower here */
                    continue;
                }
            } else if (dist2 < reach * reach && rim_steps > 0.0f) {   /* the ring round it: raise the rim */
                /* t runs 0 at the source's edge to 1 at the ring's end; 4 t (1 - t) is a hump, 1 at the middle */
                const float t = sh4der_sat_((dist2 - r2) * inv_ring);

                target = (float)SH4DER_DEFORM_LEVEL + rim_steps * 4.0f * t * (1.0f - t);
                if (target <= now) {                            /* the ground is already higher here, or this is a dent to keep */
                    continue;
                }
                if (now < (float)SH4DER_DEFORM_LEVEL) {         /* never fill an older dent with a new rim */
                    continue;
                }
            } else {
                continue;
            }
            v = (int)(sh4der_lerp_(now, target, amount) + 0.5f);   /* hand: round to the byte (target is in 0 .. 255 by construction) */
            if (v == (int)*c && (int)(target + 0.5f) != v) {       /* rounding ate the move: take one whole step, so a slow */
                v += target < now ? -1 : 1;                        /* ease-in still arrives instead of stalling short */
            }
            v = v < 0 ? 0 : v > 255 ? 255 : v;
            if (v != (int)*c) {
                *c = (uint8_t)v;
                changed++;
            }
        }
    }
    return changed;
}

/** \brief Lets the ground close: every cell moves toward the undisturbed surface by d->refill steps a second (dents rise,
    rims sink). dt: seconds since the last call, finite (a NaN or infinity does nothing). With d->refill 0 (snow) this
    returns at once. Fractions of a step are carried to the next call, so a slow refill is exact over time. A huge dt or
    rate is held to SH4DER_DEFORM_MAX_STEPS steps (about four thousand times what a byte cell can use), so the step count never overflows an int.
    Returns the whole steps applied this call. */
static inline unsigned sh4der_deform_refill(sh4der_deform_t *d, float dt) {
    float carry;
    size_t i;
    size_t n;
    int steps;

    if (!d || !d->cells || !sh4der_finite_(d->refill) || !sh4der_finite_(dt) || !(d->refill > 0.0f) || !(dt > 0.0f)) {
        return 0;
    }
    carry = sh4der_finite_(d->refill_carry) ? d->refill_carry : 0.0f;
    carry = sh4der_fma_(d->refill, dt, carry);
    if (!sh4der_finite_(carry) || carry > SH4DER_DEFORM_MAX_STEPS) {   /* the product overflowed, or it is past the limit */
        carry = SH4DER_DEFORM_MAX_STEPS;
    }
    d->refill_carry = carry;
    steps = (int)d->refill_carry;                               /* hand: the whole steps (0 .. SH4DER_DEFORM_MAX_STEPS here) */
    if (steps < 1) {
        return 0;
    }
    d->refill_carry -= (float)steps;
    n = (size_t)d->w * (size_t)d->h;
    for (i = 0; i < n; i++) {
        const int v = d->cells[i];

        if (v < SH4DER_DEFORM_LEVEL) {
            d->cells[i] = (uint8_t)(v + steps > SH4DER_DEFORM_LEVEL ? SH4DER_DEFORM_LEVEL : v + steps);
        } else if (v > SH4DER_DEFORM_LEVEL) {
            d->cells[i] = (uint8_t)(v - steps < SH4DER_DEFORM_LEVEL ? SH4DER_DEFORM_LEVEL : v - steps);
        }
    }
    return (unsigned)steps;
}

/* the cell value at world cell (ix, iz) as a float; the undisturbed level outside the window */
static inline float sh4der_deform_value_(const sh4der_deform_t *d, int ix, int iz) {
    const uint8_t *c = sh4der_deform_at_(d, ix, iz);

    return c ? (float)*c : (float)SH4DER_DEFORM_LEVEL;
}

/** \brief The ground's world height at (x, z), blended between the four cells round the point. Outside the window it is
    the undisturbed surface, so a mesh larger than the window joins it without a step. A position that is not finite
    reads as the undisturbed surface too. For every ground vertex, every frame. */
static inline float sh4der_deform_height(const sh4der_deform_t *d, float x, float z) {
    float fx;
    float fz;
    float tx;
    float tz;
    float a;
    float b;
    int ix;
    int iz;

    if (!d || !d->cells) {
        return 0.0f;
    }
    fx = x * d->inv_cell;
    fz = z * d->inv_cell;
    if (!sh4der_finite_(fx) || !sh4der_finite_(fz) || fx > SH4DER_DEFORM_MAX_CELLS || fx < -SH4DER_DEFORM_MAX_CELLS ||
        fz > SH4DER_DEFORM_MAX_CELLS || fz < -SH4DER_DEFORM_MAX_CELLS) {
        return d->ceil_y;                                       /* no window reaches there: the undisturbed surface */
    }
    ix = sh4der_deform_floor_(fx);
    iz = sh4der_deform_floor_(fz);
    tx = fx - (float)ix;
    tz = fz - (float)iz;
    a = sh4der_lerp_(sh4der_deform_value_(d, ix, iz), sh4der_deform_value_(d, ix + 1, iz), tx);
    b = sh4der_lerp_(sh4der_deform_value_(d, ix, iz + 1), sh4der_deform_value_(d, ix + 1, iz + 1), tx);
    return sh4der_fma_(sh4der_lerp_(a, b, tz) - (float)SH4DER_DEFORM_LEVEL, d->step, d->ceil_y);
}

/** \brief How deep the ground is pressed at (x, z): 0 at or above the undisturbed surface, 1 at the hard bottom.
    A caller darkens the vertex colour by it, so a dent reads even with no light to shade it. */
static inline float sh4der_deform_depth(const sh4der_deform_t *d, float x, float z) {
    if (!d || !d->cells) {
        return 0.0f;
    }
    return sh4der_sat_(sh4der_div_(d->ceil_y - sh4der_deform_height(d, x, z), d->ceil_y - d->floor_y));
}

/** \brief The ground's slope at (x, z): *sx and *sz get the rise in height per world unit along x and along z, from the
    cells one cell either side. The unnormalised surface normal is (-*sx, 1, -*sz): light a ground vertex with it. */
static inline void sh4der_deform_slope(const sh4der_deform_t *d, float x, float z, float *sx, float *sz) {
    float half;

    if (!sx || !sz) {
        return;
    }
    *sx = 0.0f;
    *sz = 0.0f;
    if (!d || !d->cells) {
        return;
    }
    half = 0.5f * d->inv_cell;                                  /* 1 / (2 cells): a centred difference */
    *sx = (sh4der_deform_height(d, x + d->cell, z) - sh4der_deform_height(d, x - d->cell, z)) * half;
    *sz = (sh4der_deform_height(d, x, z + d->cell) - sh4der_deform_height(d, x, z - d->cell)) * half;
}

/* ------------------------------------------------------------------------ */
/* The print list: decals for a crisp footprint on a coarse mesh, and the whole effect where the grid is not used. */

/** \brief One print: where it is, which way it points, how long it has lain there. */
typedef struct sh4der_deform_print {
    float x;               /**< \brief Centre in the world plane, x. */
    float z;               /**< \brief Centre in the world plane, z. */
    float dir_x;           /**< \brief Unit direction the toe points, in the plane, x. */
    float dir_z;           /**< \brief Unit direction the toe points, in the plane, z. */
    float age;             /**< \brief Seconds since it was made; negative = an empty slot. */
} sh4der_deform_print_t;

/** \brief The last SH4DER_DEFORM_PRINTS prints, oldest replaced first. Zero-initialise, then sh4der_deform_prints_reset. */
typedef struct sh4der_deform_prints {
    sh4der_deform_print_t print[SH4DER_DEFORM_PRINTS];
    unsigned next;         /**< \brief Slot the next print takes. */
} sh4der_deform_prints_t;

/** \brief One corner of a print's quad: world position, texture coordinate, alpha 0 .. 1. */
typedef struct sh4der_deform_corner {
    float x;               /**< \brief World position, x. */
    float y;               /**< \brief World position, y (the height the quad lies at). */
    float z;               /**< \brief World position, z. */
    float u;               /**< \brief Texture coordinate, 0 .. 1. */
    float v;               /**< \brief Texture coordinate, 0 .. 1. */
    float alpha;           /**< \brief Opacity, 0 .. 1. */
} sh4der_deform_corner_t;

/** \brief Empties the list. */
static inline void sh4der_deform_prints_reset(sh4der_deform_prints_t *p) {
    unsigned i;

    if (!p) {
        return;
    }
    for (i = 0; i < SH4DER_DEFORM_PRINTS; i++) {
        p->print[i].age = -1.0f;
    }
    p->next = 0;
}

/** \brief Adds a print at (x, z) pointing along (dir_x, dir_z), which need not be unit length; a zero direction is
    stored as +z. The oldest print is replaced when the list is full. A position or direction that is not finite is
    refused: nothing is stored. A direction so large that its square overflows is scaled down before it is normalised.
    Returns 1 when the print was stored, 0 when it was refused. */
static inline int sh4der_deform_prints_add(sh4der_deform_prints_t *p, float x, float z, float dir_x, float dir_z) {
    sh4der_deform_print_t *s;
    float v[3];
    float biggest;

    if (!p || !sh4der_finite_(x) || !sh4der_finite_(z) || !sh4der_finite_(dir_x) || !sh4der_finite_(dir_z)) {
        return 0;
    }
    v[0] = dir_x;
    v[1] = 0.0f;
    v[2] = dir_z;
    if (!sh4der_finite_(sh4der_len3_(v[0], v[1], v[2]))) {        /* finite parts, but the squared length overflowed: shrink first */
        biggest = sh4der_abs_(dir_x) > sh4der_abs_(dir_z) ? sh4der_abs_(dir_x) : sh4der_abs_(dir_z);
        v[0] = sh4der_div_(dir_x, biggest);
        v[2] = sh4der_div_(dir_z, biggest);
    }
    if (!(sh4der_len3_(v[0], v[1], v[2]) > 1.0e-12f)) {
        v[0] = 0.0f;
        v[2] = 1.0f;
    }
    sh4der_norm3_(v);
    s = &p->print[p->next];
    p->next = (p->next + 1u) % SH4DER_DEFORM_PRINTS;
    s->x = x;
    s->z = z;
    s->dir_x = v[0];
    s->dir_z = v[2];
    s->age = 0.0f;
    return 1;
}

/** \brief Ages every print by dt seconds; a print older than life seconds becomes an empty slot. A dt or life that is
    not finite does nothing. */
static inline void sh4der_deform_prints_tick(sh4der_deform_prints_t *p, float dt, float life) {
    unsigned i;

    if (!p || !sh4der_finite_(dt) || !sh4der_finite_(life) || !(dt > 0.0f)) {
        return;
    }
    for (i = 0; i < SH4DER_DEFORM_PRINTS; i++) {
        if (p->print[i].age < 0.0f) {
            continue;
        }
        p->print[i].age += dt;
        if (!sh4der_finite_(p->print[i].age) || p->print[i].age > life) {   /* too old, or the age itself overflowed */
            p->print[i].age = -1.0f;
        }
    }
}

/** \brief The quad of print i as four corners in strip order (heel-left, heel-right, toe-left, toe-right), lying at
    world height y (lift it 1 to 2 cm off the ground, as a light pool). half_w, half_l: half the print's width and
    length, world units, each finite and > 0. life, fade: the print is full for life - fade seconds, then its alpha falls
    to 0 over the last fade seconds (fade 0 = no fade); both finite.
    Returns 4, or 0 for an empty slot or a bad argument: a NULL pointer, an index past the list, a non-finite number,
    a zero-area quad (half_w or half_l 0, or a product that underflows to 0) or a result that is not finite.
    Nothing is written when it returns 0. */
static inline unsigned sh4der_deform_prints_quad(const sh4der_deform_prints_t *p, unsigned i, float y, float half_w, float half_l,
                                                 float life, float fade, sh4der_deform_corner_t out[4]) {
    const sh4der_deform_print_t *s;
    sh4der_deform_corner_t tmp[4];
    float alpha;
    unsigned k;

    if (!p || !out || i >= SH4DER_DEFORM_PRINTS) {
        return 0;
    }
    if (!sh4der_finite_(y) || !sh4der_finite_(half_w) || !sh4der_finite_(half_l) || !sh4der_finite_(life) || !sh4der_finite_(fade)) {
        return 0;
    }
    if (!(half_w > 0.0f) || !(half_l > 0.0f) || !(half_w * half_l > 0.0f)) {   /* a zero-area quad is dropped, not drawn */
        return 0;
    }
    s = &p->print[i];
    if (s->age < 0.0f) {
        return 0;
    }
    alpha = fade > 0.0f ? sh4der_sat_(sh4der_div_(life - s->age, fade)) : 1.0f;
    for (k = 0; k < 4u; k++) {
        const float side = (k & 1u) ? half_w : -half_w;         /* across the print: its right is (dir_z, -dir_x) */
        const float along = (k & 2u) ? half_l : -half_l;

        tmp[k].x = sh4der_fma_(s->dir_z, side, sh4der_fma_(s->dir_x, along, s->x));
        tmp[k].y = y;
        tmp[k].z = sh4der_fma_(-s->dir_x, side, sh4der_fma_(s->dir_z, along, s->z));
        tmp[k].u = (k & 1u) ? 1.0f : 0.0f;
        tmp[k].v = (k & 2u) ? 0.0f : 1.0f;                      /* the toe at the top of the texture */
        tmp[k].alpha = alpha;
        if (!sh4der_finite_(tmp[k].x) || !sh4der_finite_(tmp[k].z) || !sh4der_finite_(tmp[k].alpha)) {
            return 0;                                           /* an overflowing size: drop the quad, write nothing */
        }
    }
    memcpy(out, tmp, sizeof(tmp));
    return 4u;
}

#ifdef __cplusplus
}
#endif

#endif /* SH4DER_DEFORM_H */
